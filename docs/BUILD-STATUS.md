# Build Status

_Last updated: 2026-07-18 (M2)_

## Current state: M2 — polished first real browser UI

| Item | Status |
|---|---|
| Windows x64 CI build (`OpenNyx-win64` artifact) | see badge / latest run on the Actions tab |
| Custom Views UI (tab strip, toolbar, address bar) | ✅ implemented |
| Brave Search as default search | ✅ |
| Built-in dark new-tab page | ✅ |
| Keyboard shortcuts | ✅ (see below) |
| App icon + version resource | ✅ |
| Privacy defaults (no Google keys, metrics disabled) | ✅ unchanged from M1 |
| Windows sandbox (bootstrap.exe split) | ⏳ deferred |

## What happened to the M1 toolbar?

M1 relied on the Chrome runtime style's built-in toolbar
(`GetChromeToolbarType() == CEF_CTT_NORMAL`). On the tester's machine only
the webview rendered — the Chrome toolbar/tab strip never appeared. The
Chrome toolbar requires Chrome UI resources that are not reliably available
in the plain CEF Standard distribution setup we ship, and when it fails it
fails silently (no toolbar, no error).

**M2 fix:** OpenNyx now draws its **own** UI with the CEF Views framework
(`shell/src/browser_window.*`): a tab strip and toolbar built from
`CefPanel` / `CefLabelButton` / `CefTextfield` views, with one
`CefBrowserView` per tab. Nothing depends on optional Chrome resources, so
the UI cannot silently disappear.

## UI overview (M2)

```
┌──────────────────────────────────────────────┐
│ [Tab 1 ×][Tab 2 ×] [+]                       │  tab strip
├──────────────────────────────────────────────┤
│ ← → ⟳  [ address / Brave Search …          ] │  toolbar
├──────────────────────────────────────────────┤
│                                              │
│              active CefBrowserView           │  content
│                                              │
└──────────────────────────────────────────────┘
```

- Dark-first theme (window `#18191e`, toolbar `#202128`, accent `#7a5cff`).
- Default window 1280×800, minimum 480×320.
- Window title follows the active tab's page title (`<title> — OpenNyx`).
- Address bar: input that looks like a URL navigates (scheme added if
  missing); anything else searches `https://search.brave.com/search?q=…`.
- New-tab page: built-in dark page (data: URL) with the OpenNyx wordmark and
  a centered Brave Search box.
- Popups / `window.open` / DevTools open in their own top-level window.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab (closing the last tab closes the window) |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| `Ctrl+L` | Focus address bar (select all) |
| `F5` / `Ctrl+R` | Reload |
| `Ctrl+Shift+R` | Hard reload (ignore cache) |
| `Alt+Left` / `Alt+Right` | History back / forward |
| `Ctrl+Shift+I` | DevTools (separate window) |
| `Esc` (address bar) | Restore current URL, focus page |

## CI

- Workflow: `.github/workflows/windows-build.yml` — `windows-latest`,
  Ninja + MSVC (`ilammy/msvc-dev-cmd`), CEF binary distro cached.
- Artifact: **OpenNyx-win64** (unzip → run `opennyx.exe`).
- CEF pin: `150.0.11+gb887805+chromium-150.0.7871.115` (Chromium 150).

## Deferred (M3+)

- Windows sandbox (requires the bootstrap.exe/DLL split).
- Favicons in the tab strip (CefImage from `OnFaviconURLChange` download).
- Loading spinner / progress indication beyond the reload/stop glyph swap.
- Tab drag-reorder, middle-click close, tab overflow scrolling.
- `opennyx://` privileged pages (settings, privacy dashboard).
- Command palette (Ctrl+K), themes, blocker — see roadmap in README.
