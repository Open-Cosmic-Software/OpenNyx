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

using nlohmann::json;

namespace {

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
  // ---- config ----
  try {
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
  } catch (...) {
  }

  // ---- history ----
  try {
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
  } catch (...) {
  }

  // ---- bookmarks ----
  try {
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
  } catch (...) {
  }

  // ---- downloads ----
  try {
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
  } catch (...) {
  }
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
