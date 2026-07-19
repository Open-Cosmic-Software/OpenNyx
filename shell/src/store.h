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
  std::string PathFor(const char* name);

  std::mutex mutex_;
  bool loaded_ = false;
  std::string data_dir_;

  AppConfig config_;
  std::vector<HistoryEntry> history_;    // newest last.
  std::vector<Bookmark> bookmarks_;      // newest last.
  std::vector<DownloadEntry> downloads_;  // newest last.
};

#endif  // OPENNYX_SHELL_SRC_STORE_H_
