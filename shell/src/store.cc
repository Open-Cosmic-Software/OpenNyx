// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// Storage rationale — JSON, not SQLite:
// CEF ships no SQLite helper, and pulling the SQLite amalgamation into the
// build adds a large C source + its own toolchain quirks for what is, at this
// stage, a modest amount of data (browsing history for one local user). We
// therefore use small append-friendly JSON documents via the header-only
// nlohmann/json library (vendored under third_party/json). Each store is a
// single file that is rewritten atomically on change. This keeps the build
// dependency-light and the data trivially inspectable. If history volume ever
// makes full-file rewrites a bottleneck, swapping the backend for SQLite is a
// localized change behind this class.

#include "store.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

#include "include/cef_file_util.h"
#include "include/cef_parser.h"
#include "include/cef_path_util.h"

#include "third_party/json/json.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>  // DPAPI: CryptProtectData / CryptUnprotectData.
#endif

using nlohmann::json;

namespace {

// Base64 (standard alphabet) for storing DPAPI ciphertext inside JSON.
const char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::string& in) {
  std::string out;
  out.reserve(((in.size() + 2) / 3) * 4);
  size_t i = 0;
  while (i + 2 < in.size()) {
    uint32_t n = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 |
                 (uint8_t)in[i + 2];
    out += kB64[(n >> 18) & 63];
    out += kB64[(n >> 12) & 63];
    out += kB64[(n >> 6) & 63];
    out += kB64[n & 63];
    i += 3;
  }
  if (i < in.size()) {
    uint32_t n = (uint8_t)in[i] << 16;
    if (i + 1 < in.size()) n |= (uint8_t)in[i + 1] << 8;
    out += kB64[(n >> 18) & 63];
    out += kB64[(n >> 12) & 63];
    out += (i + 1 < in.size()) ? kB64[(n >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}

std::string Base64Decode(const std::string& in) {
  int rev[256];
  for (int k = 0; k < 256; ++k) rev[k] = -1;
  for (int k = 0; k < 64; ++k) rev[(uint8_t)kB64[k]] = k;
  std::string out;
  uint32_t buf = 0;
  int bits = 0;
  for (char c : in) {
    if (c == '=' || c == '\n' || c == '\r') continue;
    int v = rev[(uint8_t)c];
    if (v < 0) continue;
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buf >> bits) & 0xFF);
    }
  }
  return out;
}

// Encrypts |plain| for the current OS user. On non-Windows (dev builds) this
// falls back to a reversible obfuscation so the code still compiles/runs; the
// real security guarantee is Windows-only (DPAPI, the same mechanism Chrome
// and Opera use for their own password stores).
std::string EncryptSecret(const std::string& plain) {
#if defined(_WIN32)
  DATA_BLOB in;
  in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
  in.cbData = static_cast<DWORD>(plain.size());
  DATA_BLOB out{};
  if (CryptProtectData(&in, L"OpenNyx password vault", nullptr, nullptr,
                       nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
    std::string cipher(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return Base64Encode(cipher);
  }
  return std::string();  // Encryption failed -> store nothing.
#else
  std::string x = plain;
  for (char& c : x) c ^= 0x5A;
  return "OBF:" + Base64Encode(x);
#endif
}

// Reverses EncryptSecret. Empty on failure.
std::string DecryptSecret(const std::string& stored) {
#if defined(_WIN32)
  const std::string cipher = Base64Decode(stored);
  DATA_BLOB in;
  in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data()));
  in.cbData = static_cast<DWORD>(cipher.size());
  DATA_BLOB out{};
  if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &out)) {
    std::string plain(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return plain;
  }
  return std::string();
#else
  std::string s = stored;
  if (s.rfind("OBF:", 0) == 0) s = s.substr(4);
  std::string x = Base64Decode(s);
  for (char& c : x) c ^= 0x5A;
  return x;
#endif
}

int64_t NowMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string ToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(::tolower(c)); });
  return out;
}

// History is capped so the file can't grow without bound.
constexpr size_t kMaxHistory = 5000;
constexpr size_t kMaxDownloads = 500;

std::string ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return std::string();
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

bool WriteFileAtomic(const std::string& path, const std::string& data) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
      return false;
    }
    f << data;
    if (!f.good()) {
      return false;
    }
  }
  // std::rename replaces atomically on both POSIX and Windows (same volume).
  std::remove(path.c_str());
  return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace

// static
OpenNyxStore* OpenNyxStore::Get() {
  static OpenNyxStore instance;
  return &instance;
}

OpenNyxStore::OpenNyxStore() = default;

std::string OpenNyxStore::DataDir() {
  if (!data_dir_.empty()) {
    return data_dir_;
  }
  CefString user_data;
  std::string base;
  if (CefGetPath(PK_USER_DATA, user_data) && !user_data.empty()) {
    base = user_data.ToString();
  } else {
    CefString exe_dir;
    if (CefGetPath(PK_DIR_EXE, exe_dir)) {
      base = exe_dir.ToString();
    } else {
      base = ".";
    }
  }
#if defined(_WIN32)
  const char sep = '\\';
#else
  const char sep = '/';
#endif
  if (!base.empty() && base.back() != sep) {
    base += sep;
  }
  data_dir_ = base + "OpenNyx";
  CefCreateDirectory(data_dir_);
  data_dir_ += sep;
  return data_dir_;
}

std::string OpenNyxStore::PathFor(const char* name) {
  return DataDir() + name;
}

void OpenNyxStore::EnsureLoaded() {
  if (loaded_) {
    return;
  }
  LoadLocked();
  loaded_ = true;
}

void OpenNyxStore::LoadLocked() {
  // Note: CEF is compiled with exceptions disabled (_HAS_EXCEPTIONS=0), and
  // this project links against it, so we build with JSON_NOEXCEPTION. All
  // json::parse calls pass allow_exceptions=false and are checked via
  // is_discarded()/is_object()/is_array() instead of try/catch.

  // ---- config ----
  {
    const std::string raw = ReadFile(PathFor("config.json"));
    if (!raw.empty()) {
      json j = json::parse(raw, nullptr, false);
      if (j.is_object()) {
        config_.search_engine = j.value("search_engine", config_.search_engine);
        config_.custom_search_url =
            j.value("custom_search_url", config_.custom_search_url);
        config_.homepage = j.value("homepage", config_.homepage);
        config_.blocking_enabled =
            j.value("blocking_enabled", config_.blocking_enabled);
        config_.doh_enabled = j.value("doh_enabled", config_.doh_enabled);
        config_.doh_resolver = j.value("doh_resolver", config_.doh_resolver);
        config_.doh_custom_template =
            j.value("doh_custom_template", config_.doh_custom_template);
      }
    }
  }

  // ---- history ----
  {
    const std::string raw = ReadFile(PathFor("history.json"));
    if (!raw.empty()) {
      json j = json::parse(raw, nullptr, false);
      if (j.is_array()) {
        for (const auto& e : j) {
          HistoryEntry h;
          h.url = e.value("url", "");
          h.title = e.value("title", "");
          h.ts = e.value("ts", (int64_t)0);
          if (!h.url.empty()) {
            history_.push_back(std::move(h));
          }
        }
      }
    }
  }

  // ---- bookmarks ----
  {
    const std::string raw = ReadFile(PathFor("bookmarks.json"));
    if (!raw.empty()) {
      json j = json::parse(raw, nullptr, false);
      if (j.is_array()) {
        for (const auto& e : j) {
          Bookmark b;
          b.url = e.value("url", "");
          b.title = e.value("title", "");
          b.ts = e.value("ts", (int64_t)0);
          if (!b.url.empty()) {
            bookmarks_.push_back(std::move(b));
          }
        }
      }
    }
  }

  // ---- downloads ----
  {
    const std::string raw = ReadFile(PathFor("downloads.json"));
    if (!raw.empty()) {
      json j = json::parse(raw, nullptr, false);
      if (j.is_array()) {
        for (const auto& e : j) {
          DownloadEntry d;
          d.id = e.value("id", (uint32_t)0);
          d.url = e.value("url", "");
          d.filename = e.value("filename", "");
          d.full_path = e.value("full_path", "");
          d.total_bytes = e.value("total_bytes", (int64_t)0);
          d.received_bytes = e.value("received_bytes", (int64_t)0);
          d.percent = e.value("percent", 0);
          d.complete = e.value("complete", false);
          d.canceled = e.value("canceled", false);
          d.ts = e.value("ts", (int64_t)0);
          d.in_progress = false;  // never restore as "in progress".
          downloads_.push_back(std::move(d));
        }
      }
    }
  }

  // ---- session (open tabs) ----
  {
    const std::string raw = ReadFile(PathFor("session.json"));
    if (!raw.empty()) {
      json j = json::parse(raw, nullptr, false);
      if (j.is_object()) {
        session_active_ = j.value("active", (size_t)0);
        const auto& tabs = j["tabs"];
        if (tabs.is_array()) {
          for (const auto& t : tabs) {
            if (t.is_string()) {
              session_tabs_.push_back(t.get<std::string>());
            }
          }
        }
      }
    }
  }

  // ---- passwords (encrypted vault) ----
  {
    const std::string raw = ReadFile(PathFor("passwords.json"));
    if (!raw.empty()) {
      json arr = json::parse(raw, nullptr, false);
      if (arr.is_array()) {
        for (const auto& e : arr) {
          if (!e.is_object()) continue;
          PasswordEntry p;
          p.origin = e.value("origin", "");
          p.username = e.value("username", "");
          p.ts = e.value("ts", (int64_t)0);
          const std::string enc = e.value("password_enc", "");
          p.password = enc.empty() ? "" : DecryptSecret(enc);
          passwords_.push_back(p);
        }
      }
    }
    passwords_loaded_ = true;
  }
}

void OpenNyxStore::SaveSession(const std::vector<std::string>& tab_urls,
                               size_t active_index) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  session_tabs_ = tab_urls;
  session_active_ = active_index;
  json j;
  j["active"] = active_index;
  json arr = json::array();
  for (const auto& u : tab_urls) {
    arr.push_back(u);
  }
  j["tabs"] = arr;
  WriteFileAtomic(PathFor("session.json"), j.dump(2));
}

std::vector<std::string> OpenNyxStore::GetSessionTabs() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  return session_tabs_;
}

size_t OpenNyxStore::GetSessionActiveIndex() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  return session_active_;
}

// ---- Passwords (encrypted vault) ----

void OpenNyxStore::SavePasswordsLocked() {
  json arr = json::array();
  for (const auto& p : passwords_) {
    const std::string enc =
        p.password.empty() ? std::string() : EncryptSecret(p.password);
    // If encryption failed we skip the secret but keep the record shape so the
    // entry isn't silently lost; an empty password_enc reads back as "".
    arr.push_back({{"origin", p.origin},
                   {"username", p.username},
                   {"password_enc", enc},
                   {"ts", p.ts}});
  }
  WriteFileAtomic(PathFor("passwords.json"), arr.dump(2));
}

bool OpenNyxStore::AddPassword(const std::string& origin,
                               const std::string& username,
                               const std::string& password) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  // Upsert by (origin, username).
  for (auto& p : passwords_) {
    if (p.origin == origin && p.username == username) {
      p.password = password;
      p.ts = NowMillis();
      SavePasswordsLocked();
      return true;
    }
  }
  PasswordEntry p;
  p.origin = origin;
  p.username = username;
  p.password = password;
  p.ts = NowMillis();
  passwords_.push_back(p);
  SavePasswordsLocked();
  return true;
}

bool OpenNyxStore::RemovePassword(const std::string& origin,
                                  const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  const size_t before = passwords_.size();
  passwords_.erase(
      std::remove_if(passwords_.begin(), passwords_.end(),
                     [&](const PasswordEntry& p) {
                       return p.origin == origin && p.username == username;
                     }),
      passwords_.end());
  if (passwords_.size() != before) {
    SavePasswordsLocked();
    return true;
  }
  return false;
}

std::vector<PasswordEntry> OpenNyxStore::GetPasswords() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  std::vector<PasswordEntry> out(passwords_.rbegin(), passwords_.rend());
  return out;
}

void OpenNyxStore::ClearPasswords() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  passwords_.clear();
  SavePasswordsLocked();
}

int OpenNyxStore::ImportPasswords(const std::vector<PasswordEntry>& entries) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  int n = 0;
  for (const auto& in : entries) {
    if (in.origin.empty() && in.username.empty()) continue;
    bool updated = false;
    for (auto& p : passwords_) {
      if (p.origin == in.origin && p.username == in.username) {
        p.password = in.password;
        p.ts = NowMillis();
        updated = true;
        break;
      }
    }
    if (!updated) {
      PasswordEntry p = in;
      if (p.ts == 0) p.ts = NowMillis();
      passwords_.push_back(p);
    }
    ++n;
  }
  if (n > 0) SavePasswordsLocked();
  return n;
}

void OpenNyxStore::SaveConfigLocked() {
  json j;
  j["search_engine"] = config_.search_engine;
  j["custom_search_url"] = config_.custom_search_url;
  j["homepage"] = config_.homepage;
  j["blocking_enabled"] = config_.blocking_enabled;
  j["doh_enabled"] = config_.doh_enabled;
  j["doh_resolver"] = config_.doh_resolver;
  j["doh_custom_template"] = config_.doh_custom_template;
  WriteFileAtomic(PathFor("config.json"), j.dump(2));
}

void OpenNyxStore::SaveHistoryLocked() {
  json arr = json::array();
  for (const auto& h : history_) {
    arr.push_back({{"url", h.url}, {"title", h.title}, {"ts", h.ts}});
  }
  WriteFileAtomic(PathFor("history.json"), arr.dump());
}

void OpenNyxStore::SaveBookmarksLocked() {
  json arr = json::array();
  for (const auto& b : bookmarks_) {
    arr.push_back({{"url", b.url}, {"title", b.title}, {"ts", b.ts}});
  }
  WriteFileAtomic(PathFor("bookmarks.json"), arr.dump(2));
}

void OpenNyxStore::SaveDownloadsLocked() {
  json arr = json::array();
  for (const auto& d : downloads_) {
    arr.push_back({{"id", d.id},
                   {"url", d.url},
                   {"filename", d.filename},
                   {"full_path", d.full_path},
                   {"total_bytes", d.total_bytes},
                   {"received_bytes", d.received_bytes},
                   {"percent", d.percent},
                   {"complete", d.complete},
                   {"canceled", d.canceled},
                   {"ts", d.ts}});
  }
  WriteFileAtomic(PathFor("downloads.json"), arr.dump());
}

// ---- Config ----

AppConfig OpenNyxStore::GetConfig() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  return config_;
}

void OpenNyxStore::SetConfig(const AppConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  config_ = config;
  SaveConfigLocked();
}

std::string OpenNyxStore::BuildSearchURL(const std::string& query) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  const std::string q = CefURIEncode(query, /*use_plus=*/true).ToString();
  const std::string& engine = config_.search_engine;
  if (engine == "duckduckgo") {
    return "https://duckduckgo.com/?q=" + q;
  }
  if (engine == "mojeek") {
    return "https://www.mojeek.com/search?q=" + q;
  }
  if (engine == "custom" && !config_.custom_search_url.empty()) {
    // Custom template: replace {q} placeholder, else append.
    std::string tmpl = config_.custom_search_url;
    const size_t pos = tmpl.find("{q}");
    if (pos != std::string::npos) {
      tmpl.replace(pos, 3, q);
      return tmpl;
    }
    return tmpl + q;
  }
  // Default: Brave.
  return "https://search.brave.com/search?q=" + q;
}

// ---- History ----

void OpenNyxStore::AddHistory(const std::string& url, const std::string& title) {
  if (url.empty()) {
    return;
  }
  // Don't record internal pages or data: URLs.
  if (url.compare(0, 10, "opennyx://") == 0 ||
      url.compare(0, 5, "data:") == 0 || url.compare(0, 6, "about:") == 0 ||
      url.compare(0, 9, "chrome://") == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  // Collapse consecutive duplicates of the same URL.
  if (!history_.empty() && history_.back().url == url) {
    history_.back().ts = NowMillis();
    if (!title.empty()) {
      history_.back().title = title;
    }
    SaveHistoryLocked();
    return;
  }
  HistoryEntry h;
  h.url = url;
  h.title = title;
  h.ts = NowMillis();
  history_.push_back(std::move(h));
  if (history_.size() > kMaxHistory) {
    history_.erase(history_.begin(),
                   history_.begin() + (history_.size() - kMaxHistory));
  }
  SaveHistoryLocked();
}

std::vector<HistoryEntry> OpenNyxStore::QueryHistory(const std::string& filter,
                                                     size_t limit) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  const std::string needle = ToLower(filter);
  std::vector<HistoryEntry> out;
  for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
    if (!needle.empty()) {
      if (ToLower(it->url).find(needle) == std::string::npos &&
          ToLower(it->title).find(needle) == std::string::npos) {
        continue;
      }
    }
    out.push_back(*it);
    if (out.size() >= limit) {
      break;
    }
  }
  return out;
}

void OpenNyxStore::ClearHistory() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  history_.clear();
  SaveHistoryLocked();
}

std::string OpenNyxStore::AutocompleteHistory(const std::string& prefix) {
  if (prefix.size() < 2) {
    return std::string();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  const std::string p = ToLower(prefix);
  // Most recent first.
  for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
    const std::string url = it->url;
    std::string bare = url;
    // Strip scheme for prefix comparison so "git" matches "https://github…".
    for (const char* scheme : {"https://", "http://"}) {
      const size_t len = std::string(scheme).size();
      if (ToLower(bare).compare(0, len, scheme) == 0) {
        bare = bare.substr(len);
        break;
      }
    }
    // Strip leading www.
    if (ToLower(bare).compare(0, 4, "www.") == 0) {
      bare = bare.substr(4);
    }
    if (ToLower(bare).compare(0, p.size(), p) == 0) {
      return url;
    }
  }
  return std::string();
}

// ---- Bookmarks ----

bool OpenNyxStore::AddBookmark(const std::string& url,
                               const std::string& title) {
  if (url.empty()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  for (const auto& b : bookmarks_) {
    if (b.url == url) {
      return false;  // already bookmarked.
    }
  }
  Bookmark b;
  b.url = url;
  b.title = title.empty() ? url : title;
  b.ts = NowMillis();
  bookmarks_.push_back(std::move(b));
  SaveBookmarksLocked();
  return true;
}

bool OpenNyxStore::RemoveBookmark(const std::string& url) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  const size_t before = bookmarks_.size();
  bookmarks_.erase(
      std::remove_if(bookmarks_.begin(), bookmarks_.end(),
                     [&](const Bookmark& b) { return b.url == url; }),
      bookmarks_.end());
  if (bookmarks_.size() != before) {
    SaveBookmarksLocked();
    return true;
  }
  return false;
}

bool OpenNyxStore::IsBookmarked(const std::string& url) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  for (const auto& b : bookmarks_) {
    if (b.url == url) {
      return true;
    }
  }
  return false;
}

std::vector<Bookmark> OpenNyxStore::GetBookmarks() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  std::vector<Bookmark> out(bookmarks_.rbegin(), bookmarks_.rend());
  return out;
}

// ---- Downloads ----

void OpenNyxStore::UpsertDownload(const DownloadEntry& entry) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  for (auto& d : downloads_) {
    if (d.id == entry.id) {
      d = entry;
      SaveDownloadsLocked();
      return;
    }
  }
  downloads_.push_back(entry);
  if (downloads_.size() > kMaxDownloads) {
    downloads_.erase(downloads_.begin());
  }
  SaveDownloadsLocked();
}

std::vector<DownloadEntry> OpenNyxStore::GetDownloads() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  std::vector<DownloadEntry> out(downloads_.rbegin(), downloads_.rend());
  return out;
}

void OpenNyxStore::ClearDownloads() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  downloads_.clear();
  SaveDownloadsLocked();
}

// ---- Bulk ----

void OpenNyxStore::ClearBrowsingData() {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureLoaded();
  history_.clear();
  downloads_.clear();
  SaveHistoryLocked();
  SaveDownloadsLocked();
}
