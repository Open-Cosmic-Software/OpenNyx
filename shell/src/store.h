// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// OpenNyxStore — the local, on-disk data layer for history, bookmarks,
// downloads and settings. Storage is plain JSON files in the CEF user-data
// directory (see store.cc for the rationale vs. SQLite). All public methods
// are thread-safe (guarded by an internal mutex) because they are touched
// from both the CEF UI thread (navigation events) and the IO thread (the
// opennyx:// scheme handler serving the settings/history/bookmarks pages).

#ifndef OPENNYX_SHELL_SRC_STORE_H_
#define OPENNYX_SHELL_SRC_STORE_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// A single visited-page record.
struct HistoryEntry {
  std::string url;
  std::string title;
  int64_t ts = 0;  // Unix epoch milliseconds.
};

// A saved bookmark.
struct Bookmark {
  std::string url;
  std::string title;
  int64_t ts = 0;
};

// A download record (kept in memory + persisted so the list survives restart).
struct DownloadEntry {
  uint32_t id = 0;
  std::string url;
  std::string filename;
  std::string full_path;
  int64_t total_bytes = 0;
  int64_t received_bytes = 0;
  int percent = 0;
  bool complete = false;
  bool canceled = false;
  bool in_progress = false;
  int64_t ts = 0;
};

// A saved login credential. In memory `password` is plaintext; on disk it is
// stored encrypted (Windows DPAPI, tied to the OS user account).
struct PasswordEntry {
  std::string origin;    // site URL / origin, e.g. https://example.com
  std::string username;  // login / e-mail
  std::string password;  // plaintext IN MEMORY ONLY (encrypted at rest)
  int64_t ts = 0;        // last saved, Unix epoch millis.
};

// Which search engine the address bar / new-tab box uses.
struct AppConfig {
  std::string search_engine = "brave";  // brave|duckduckgo|mojeek|custom
  std::string custom_search_url;        // used when search_engine == "custom"
  std::string homepage = "opennyx://newtab";
  bool blocking_enabled = true;
  bool doh_enabled = true;
  std::string doh_resolver = "quad9";  // quad9(default)|dns0|mullvad|cloudflare|custom
  std::string doh_custom_template;
};

// Process-wide singleton. Get() lazily loads all files on first use.
class OpenNyxStore {
 public:
  static OpenNyxStore* Get();

  // ---- Config ----
  AppConfig GetConfig();
  void SetConfig(const AppConfig& config);

  // Builds a search URL for |query| using the configured engine.
  std::string BuildSearchURL(const std::string& query);

  // ---- History ----
  void AddHistory(const std::string& url, const std::string& title);
  // Returns matching entries (most-recent first), optional case-insensitive
  // substring filter, capped at |limit|.
  std::vector<HistoryEntry> QueryHistory(const std::string& filter,
                                         size_t limit);
  void ClearHistory();
  // Best-effort autocomplete: the most recent history URL that starts with or
  // contains |prefix| (host-aware). Empty if none.
  std::string AutocompleteHistory(const std::string& prefix);

  // ---- Bookmarks ----
  bool AddBookmark(const std::string& url, const std::string& title);
  bool RemoveBookmark(const std::string& url);
  bool IsBookmarked(const std::string& url);
  std::vector<Bookmark> GetBookmarks();

  // ---- Downloads ----
  // Upserts a download record (matched by id).
  void UpsertDownload(const DownloadEntry& entry);
  std::vector<DownloadEntry> GetDownloads();
  void ClearDownloads();

  // ---- Passwords (encrypted vault) ----
  // Adds or updates a credential (matched by origin+username). Returns false
  // only on hard failure.
  bool AddPassword(const std::string& origin, const std::string& username,
                   const std::string& password);
  bool RemovePassword(const std::string& origin, const std::string& username);
  // Decrypted credentials (most-recent first).
  std::vector<PasswordEntry> GetPasswords();
  void ClearPasswords();
  // Imports many credentials at once (e.g. from a Chrome/Opera CSV export).
  // Returns the number successfully added/updated.
  int ImportPasswords(const std::vector<PasswordEntry>& entries);

  // ---- Session (open tabs, for restore-on-startup) ----
  // Persists the list of currently-open tab URLs (in tab order). Called by the
  // browser window whenever tabs change.
  void SaveSession(const std::vector<std::string>& tab_urls,
                   size_t active_index);
  // Returns the previously-saved open-tab URLs (empty if none).
  std::vector<std::string> GetSessionTabs();
  size_t GetSessionActiveIndex();

  // ---- Bulk ----
  // Clears history + downloads (leaves bookmarks + config intact). Cookies /
  // cache are cleared separately by the browser layer.
  void ClearBrowsingData();

  // Absolute path to the storage directory (created if needed).
  std::string DataDir();

 private:
  OpenNyxStore();
  void EnsureLoaded();
  void LoadLocked();
  void SaveConfigLocked();
  void SaveHistoryLocked();
  void SaveBookmarksLocked();
  void SaveDownloadsLocked();
  void SavePasswordsLocked();
  std::string PathFor(const char* name);

  std::mutex mutex_;
  bool loaded_ = false;
  std::string data_dir_;

  AppConfig config_;
  std::vector<HistoryEntry> history_;    // newest last.
  std::vector<Bookmark> bookmarks_;      // newest last.
  std::vector<DownloadEntry> downloads_;  // newest last.
  std::vector<std::string> session_tabs_;  // open tab URLs (tab order).
  size_t session_active_ = 0;
  std::vector<PasswordEntry> passwords_;   // decrypted in memory, newest last.
  bool passwords_loaded_ = false;
};

#endif  // OPENNYX_SHELL_SRC_STORE_H_
