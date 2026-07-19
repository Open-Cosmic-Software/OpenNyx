// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// OpenNyxBlocklist — a compact, bundled tracker/ad domain blocklist plus a
// fast host matcher. The list is a curated subset of well-known advertising
// and tracking domains (drawn from the public EasyList / Peter Lowe /
// StevenBlack hosts corpus). Matching is domain-suffix based: a request to
// `foo.doubleclick.net` is blocked because `doubleclick.net` is on the list.
//
// The blocklist is toggle-able at runtime (Settings) and keeps a per-origin
// counter of how many requests it blocked so the toolbar shield can show a
// number for the active site.

#ifndef OPENNYX_SHELL_SRC_BLOCKLIST_H_
#define OPENNYX_SHELL_SRC_BLOCKLIST_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class OpenNyxBlocklist {
 public:
  static OpenNyxBlocklist* Get();

  // Enable/disable at runtime (mirrors the settings toggle).
  void SetEnabled(bool enabled) { enabled_.store(enabled); }
  bool enabled() const { return enabled_.load(); }

  // Returns true if a request to |host| (a bare hostname, no scheme/port)
  // should be blocked. Never blocks first-party requests: |first_party_host|
  // is the host of the top-level document; a request is allowed if its host is
  // the same registrable domain as the first party even if listed (avoids
  // breaking sites that self-host analytics on their own domain — rare, but
  // safer). Pass an empty first party to skip that exception.
  bool ShouldBlock(const std::string& host,
                   const std::string& first_party_host);

  // Always-on Google phone-home guard, independent of the tracker toggle and
  // WITHOUT any first-party exception. This is the network-level backstop for
  // runtime de-googling: even if a command-line switch fails to suppress a
  // Google telemetry/variations/safe-browsing call, the request is cancelled
  // here. Returns true for hosts that are pure Google phone-home endpoints
  // (never user-facing services like google.com search or googlevideo).
  bool ShouldBlockGooglePhoneHome(const std::string& host);

  // Total number of domains in the bundled list.
  size_t size() const { return domains_.size(); }

  // ---- Per-site counters (keyed by the first-party host) ----
  void RecordBlock(const std::string& first_party_host);
  int GetCount(const std::string& first_party_host);
  void ResetCount(const std::string& first_party_host);
  // Grand total across the whole session.
  uint64_t total_blocked() const { return total_blocked_.load(); }

 private:
  OpenNyxBlocklist();
  void Load();

  std::atomic<bool> enabled_{true};
  std::unordered_set<std::string> domains_;

  std::mutex counts_mutex_;
  std::unordered_map<std::string, int> per_site_counts_;
  std::atomic<uint64_t> total_blocked_{0};
};

#endif  // OPENNYX_SHELL_SRC_BLOCKLIST_H_
