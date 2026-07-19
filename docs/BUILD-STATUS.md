# Build Status

_Last updated: 2026-07-19 (M3 + M4 + Alloy runtime-style fix + custom-scheme render-process fix + single-tab-close regression fix)_

## Tab close (final flow, 2026-07-19 v4 — detach-based)

This ping-ponged three times. The definitive finding, verified against the
actual CEF 150 sources (`alloy_browser_host_impl.cc`, `browser_view_impl.cc`,
`view_util.cc`) and CEF issue #3376 (still open): **CEF has NO supported way
to close one of several CefBrowserViews sharing a single CefWindow via
`CloseBrowser()`/`TryCloseBrowser()`.**

Why each earlier attempt failed:
- **v1** `TryCloseBrowser()` on an attached browser → `DoClose` false → CEF
  calls `CloseHostWindow()` = `widget->Close()` on the SHARED top-level
  window → our `CanClose()` cancels it → nothing ever closes.
- **v2** detach the view, then `CloseBrowser(true)` → at that point our Tab
  still held a live ref, so `window_destroyed_` was false → CEF again routed
  to `CloseHostWindow()` → whole window died.
- **v3** plain `CloseBrowser(false)` → same `CloseHostWindow()` routing →
  `CanClose()` (tabs non-empty) cancels → tab won't close at all.

**The one sanctioned path is Views-hierarchy teardown** (the case explicitly
handled in `~CefBrowserViewImpl`):
1. `CloseTabAt(i)` (non-last tab): `RemoveTabAt(i)` detaches the
   CefBrowserView from the window. Detaching makes the views::View release
   its ref (`view_util::ResumeOwnership`), so our Tab struct holds the LAST
   strong ref (the browser-side platform delegate only keeps a WeakPtr).
   When RemoveTabAt's local Tab copy goes out of scope, `~CefBrowserViewImpl`
   runs → `browser->WindowDestroyed()` → sets `window_destroyed_` → internal
   `CloseBrowser(true)` → `CloseContents()` now skips BOTH `DoClose()` and
   `CloseHostWindow()` and destroys ONLY this browser → `OnBeforeClose`.
   No `CloseBrowser()` call from us, ever, for a tab.
2. `OnBrowserClosed()` (from `OnBeforeClose`) is a no-op safety net: the tab
   is already gone. `OpenNyxClient` erases the browser from `browser_list_`
   and calls `CefQuitMessageLoop()` when the last browser (incl. popups) is
   gone.
3. Closing the LAST tab: `CloseTabAt` calls `MaybeCloseWindow()` (guarded by
   `window_close_issued_`) → `window_->Close()` → `CanClose()`.
4. `CanClose()` (OS × or last-tab): synchronously detaches ALL remaining tabs
   via `RemoveTabAt` (each release destroys exactly that browser as in step
   1) and returns true — the window teardown then touches no live browser.
5. JS `window.close()`: `DoClose()` returns **true** for tab browsers (the
   documented "non-standard close" path — returning false would close the
   shared window) and posts `CloseTabForBrowser()` which routes into the same
   `CloseTabAt` flow. Popup/DevTools windows keep the standard `DoClose`
   false flow (one browser per window).

Behavior: × on a background tab removes only it; × on the active tab
reselects a neighbor; × on the last tab (or the OS ×) closes the window and
exits the app.

## Current state: M3 (everyday features) + M4 (privacy layer)

CI is **green**; the `OpenNyx-win64` artifact builds and links with all of the
M3/M4 features below. See the Actions tab for the latest run.

| Item | Status |
|---|---|
| Windows x64 CI build (`OpenNyx-win64` artifact) | ✅ green |
| Custom Views UI (tab strip, toolbar, address bar) | ✅ |
| Single-tab close (× / Ctrl+W closes ONE tab, not the window) | ✅ fixed 2026-07-19 (v4 detach-based flow) |
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
- **CEF 150 defaults to the *Chrome* runtime style.** Without an explicit
  override, `CefBrowserView` renders the full Chrome browser UI (Chrome
  toolbar + the Google new-tab page with "Customize Chromium" / AI Mode),
  bypassing our custom Views UI entirely. **Fix:** force the **Alloy** runtime
  style so each BrowserView renders *only* web content, letting our own tab
  strip + toolbar + `opennyx://newtab` start page be the visible UI. We
  override on the delegate instance used for **both** the browser view and the
  top-level window (the single `BrowserWindow` object is delegate for both),
  plus the popup/DevTools window:
  - `BrowserWindow::GetBrowserRuntimeStyle() -> CEF_RUNTIME_STYLE_ALLOY`
    (from `CefBrowserViewDelegate`)
  - `BrowserWindow::GetWindowRuntimeStyle() -> CEF_RUNTIME_STYLE_ALLOY`
    (from `CefWindowDelegate`)
  - `PopupWindowDelegate::GetWindowRuntimeStyle() -> CEF_RUNTIME_STYLE_ALLOY`
  Enum lives in `include/internal/cef_types_runtime.h`. The new-tab URL is our
  own `opennyx://newtab` (see `GetNewTabURL()`), never Google.
- **Blank white `opennyx://newtab` — the custom scheme was invisible to the
  render process.** After the Alloy fix the shell (tab strip/toolbar/address
  bar) rendered correctly, but the web content area stayed blank white: the
  start page never loaded, and no error page appeared (the navigation was
  *dropped*, not errored). **Root cause:** `main_win.cc` called
  `CefExecuteProcess(main_args, nullptr, nullptr)` with a **NULL `CefApp`** for
  sub-processes. `CefApp::OnRegisterCustomSchemes` MUST run in *every* process
  type. Because it never ran in the **render** process, that process did not
  know `opennyx` was a `STANDARD|SECURE` scheme, so it treated
  `opennyx://newtab` as an unknown/opaque origin and silently refused to commit
  the document → blank content (the dark HTML from `pages.cc` was never even
  requested — the scheme handler factory in the browser process was never
  reached). **Fix:** construct the single `OpenNyxApp` *before*
  `CefExecuteProcess` and pass it to **both** `CefExecuteProcess` and
  `CefInitialize`, so scheme registration happens in the browser AND all
  sub-processes. One-line-ish change in `main_win.cc`; the dark new-tab page
  with the OpenNyx wordmark + Brave search box now renders. (General CEF rule:
  a custom STANDARD/SECURE scheme is only fully functional if the SAME `CefApp`
  that implements `OnRegisterCustomSchemes` is handed to `CefExecuteProcess`.)
- Vendored `third_party/json/` is committed (the `.gitignore` now ignores only
  the downloaded CEF distribution, not vendored headers).

## Fixed: tab close (× button + Ctrl+W) did nothing (2026-07-19)

- **Symptom (Windows):** clicking the × on a tab, or pressing Ctrl+W, left the
  tab in place. Nothing closed.
- **Root cause:** OpenNyx hosts every tab as its own `CefBrowserView`, all
  parented to ONE shared top-level `CefWindow`. The old `CloseTabAt()` called
  `browser->GetHost()->TryCloseBrowser()` on a still-parented browser. For a
  windowed browser, `DoClose()` returning false makes CEF route the close to
  the browser's top-level parent window — i.e. `CefWindowDelegate::CanClose()`.
  We intentionally cancel that in `CanClose()` (returns `tabs_.empty()`) to keep
  the window open while other tabs live, so CEF cancelled the browser close and
  the tab never went away. Confirmed against CEF issue #3376 and the
  `cef_life_span_handler.h` DoClose contract.
- **Fix:** In `CloseTabAt()`, detach the tab's `CefBrowserView` from the window
  FIRST (`RemoveTabAt()` calls `window_->RemoveChildView(...)`), THEN call
  `browser->GetHost()->CloseBrowser(/*force_close=*/true)`. A detached view has
  no parent window for CEF to route the close request to, so only that single
  browser is destroyed — the window and other tabs are untouched. `OnBeforeClose
  -> OnBrowserClosed()` still fires but finds no matching tab (already removed),
  so it is a harmless no-op (no double-remove/dangling tab). If the closed tab
  was the last one, `CloseTabAt()` calls `window_->Close()`, and `CanClose()`
  now sees `tabs_` empty and allows the window to close.
- Ctrl+W uses the same path via `CloseActiveTab() -> CloseTabAt(active_tab_)`.

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
