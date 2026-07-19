# Build Status

_Last updated: 2026-07-19 (M3 + M4)_

## Current state: M3 (everyday features) + M4 (privacy layer)

CI is **green**; the `OpenNyx-win64` artifact builds and links with all of the
M3/M4 features below. See the Actions tab for the latest run.

| Item | Status |
|---|---|
| Windows x64 CI build (`OpenNyx-win64` artifact) | ✅ green |
| Custom Views UI (tab strip, toolbar, address bar) | ✅ |
| `opennyx://` privileged scheme (pages + JSON bridge) | ✅ |
| **History** (record + `opennyx://history` list/search/clear) | ✅ |
| **Bookmarks** (star button + `opennyx://bookmarks`) | ✅ |
| **Downloads** (DownloadHandler + `opennyx://downloads`) | ✅ |
| **Settings** (`opennyx://settings`, persisted config) | ✅ |
| **Tracker/ad blocking** (bundled blocklist + shield count) | ✅ |
| **DNS-over-HTTPS** (secure mode, resolver choice) | ✅ |
| **Network audit** doc (`docs/NETWORK-AUDIT.md`) | ✅ |
| Address-bar history autocomplete | ⏳ deferred (helper exists, not wired to the Views textfield) |
| Bookmarks bar under toolbar | ⏳ deferred (nice-to-have) |
| Windows sandbox (bootstrap.exe split) | ⏳ deferred |

## Architecture of the M3/M4 additions

### The `opennyx://` scheme (`scheme_handler.*`, `pages.*`)

Rich UI pages are served by a **privileged custom scheme** rather than native
Views. `OnRegisterCustomSchemes` declares `opennyx` as
STANDARD+SECURE+CORS+FETCH; a `CefSchemeHandlerFactory` returns an in-memory
`CefResourceHandler` for each request:

- `opennyx://newtab|history|bookmarks|downloads|settings` → embedded HTML
  (dark-themed, self-contained; see `pages.cc`).
- `opennyx://api/<endpoint>` → the **browser⇄page bridge** as a small JSON API
  (GET for reads, POST for mutations). Pages just `fetch()` these. This avoids
  a separate render-process `CefMessageRouter` round-trip entirely.

API endpoints: `config` (GET/POST), `resolve` (address-bar resolution),
`history`, `history/clear`, `bookmarks`, `bookmarks/remove`, `downloads`,
`downloads/clear`, `cleardata`, `blockstats`.

### Storage (`store.cc`) — JSON, not SQLite

CEF ships no SQLite helper. Rather than pull the SQLite amalgamation into the
build (a large C TU + its own quirks under CEF's `/GR- /EHsc- /WX` flags), we
use small **JSON documents** via the header-only nlohmann/json library
(vendored under `third_party/json`, built with `JSON_NOEXCEPTION` because CEF
disables exceptions). Files live in an `OpenNyx/` folder in the CEF user-data
dir: `config.json`, `history.json` (capped 5000), `bookmarks.json`,
`downloads.json` (capped 500). Writes are atomic (temp + rename). `OpenNyxStore`
is a thread-safe singleton (touched from the UI thread for navigation events
and the IO thread for the scheme API). The backend is isolated behind this
class, so swapping in SQLite later is a localized change.

### Tracker/ad blocking (`blocklist.*`, `opennyx_client.cc`)

`OpenNyxClient` implements `CefRequestHandler::GetResourceRequestHandler` →
`CefResourceRequestHandler::OnBeforeResourceLoad`, returning `RV_CANCEL` for any
request whose host matches the bundled blocklist (domain-suffix match, with a
first-party exception so a site's own domain is never blocked).

- **Blocklist source:** a curated, compact subset (~250 high-signal domains) of
  the public ad/tracking corpora — Peter Lowe's list (pgl.yoyo.org, CC-BY),
  StevenBlack/hosts (MIT), and EasyList/EasyPrivacy well-known domains. Embedded
  as a C array (`kBundledDomains`) and loaded into a hash set once; kept small
  deliberately to keep the binary lean.
- **Per-site counter + shield:** each block increments a per-first-party-host
  counter; the toolbar **🛡 shield** shows the number for the active tab and
  turns accent-colored when > 0. Toggle-able in Settings (mirrored into the
  runtime `OpenNyxBlocklist` flag).

### DNS-over-HTTPS (`opennyx_app.cc`)

`OnBeforeCommandLineProcessing` appends, when enabled (default on):

```
--dns-over-https-mode=secure
--dns-over-https-templates=<resolver>
```

Resolver is user-selectable in Settings: **Cloudflare** (`cloudflare-dns.com`,
default), **Quad9** (`dns.quad9.net`, CH), **Mullvad** (`dns.mullvad.net`, SE),
or a **custom** RFC 8484 template. "secure" mode means DoH-only (no plaintext
fallback). Applied at process start from persisted config; changes take effect
on restart (documented in the UI).

### Downloads (`opennyx_client.cc`)

`CefDownloadHandler::OnBeforeDownload` shows the OS save dialog and records the
item; `OnDownloadUpdated` upserts progress into the store. The `opennyx://
downloads` page polls `opennyx://api/downloads` for live progress bars.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab (last tab closes the window) |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| `Ctrl+L` | Focus address bar |
| `F5` / `Ctrl+R` | Reload |
| `Ctrl+Shift+R` | Hard reload |
| `Alt+Left` / `Alt+Right` | History back / forward |
| `Ctrl+Shift+I` | DevTools |
| **`Ctrl+H`** | **History page** |
| **`Ctrl+D`** | **Bookmark current page (toggle)** |
| **`Ctrl+J`** | **Downloads page** |
| **`Ctrl+,`** | **Settings page** |
| `Esc` (address bar) | Restore current URL, focus page |

## Build notes / gotchas encountered

- **CEF compiles with `_HAS_EXCEPTIONS=0 /GR- /W4 /WX`.** Third-party code must
  build under the same regime. Fix: `JSON_NOEXCEPTION` for nlohmann/json and
  **no `try/catch`** in our code (`json::parse(..., allow_exceptions=false)` +
  `is_object()/is_array()` checks instead). `/bigobj` added for the large json
  header.
- **`VK_OEM_COMMA` is a Windows macro** — our local constant was renamed to
  `kVK_OEM_COMMA` to avoid the clash (`error C2059`).
- Vendored `third_party/json/` is committed (the `.gitignore` now ignores only
  the downloaded CEF distribution, not vendored headers).

## Deferred (M5+)

- Address-bar autocomplete surfaced in the Views textfield (the
  `OpenNyxStore::AutocompleteHistory` helper exists but isn't wired to a
  dropdown yet).
- Bookmarks bar under the toolbar.
- Dev tools suite (JSON viewer, request inspector, API client), command palette
  (Ctrl+K) — see README roadmap.
- Windows sandbox (bootstrap.exe/DLL split).
- Favicons in the tab strip; tab drag-reorder; loading spinner.

## CI

- Workflow: `.github/workflows/windows-build.yml` — `windows-latest`,
  Ninja + MSVC (`ilammy/msvc-dev-cmd`), CEF binary distro cached.
- Artifact: **OpenNyx-win64** (unzip → run `opennyx.exe`).
- CEF pin: `150.0.11+gb887805+chromium-150.0.7871.115` (Chromium 150).
