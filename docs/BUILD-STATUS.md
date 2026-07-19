# Build Status

_Last updated: 2026-07-19 (M2.1)_

## M2.1 — polish & the address-bar readability fix

Tester feedback on M2 flagged one blocker and a general "make it prettier"
ask. Both are addressed here.

### Address bar: black-on-black text (BLOCKER) — fixed

**Symptom:** the address bar rendered dark text on a dark background, so
typed URLs were unreadable.

**Root cause / earlier mistake:** an earlier commit assumed the per-Textfield
color setters were *removed* in CEF API 15000 and dropped them, leaving the
field with no explicit colors (hence unreadable defaults). That was wrong.
Inspecting the pinned CEF 150 header
(`include/views/cef_textfield.h`) shows `SetTextColor` /
`SetSelectionBackgroundColor` / `SetPlaceholderTextColor` are still present
but wrapped in `#if CEF_API_REMOVED(15000)` — i.e. they are only compiled in
when `CEF_API_VERSION < 15000`. This project builds at the default
(experimental) API version, where those methods are **compiled out**, so
calling them would not even link.

**Correct fix (the supported CEF 150 path):** color the textfield through the
**window theme** instead of the individual view. `BrowserWindow::ApplyTheme()`
calls `CefWindow::SetThemeColor(color_id, color)` with the standard
`CEF_ColorTextfield*` IDs and then `CefWindow::ThemeChanged()` to push the
colors into the view hierarchy (`cef_color_ids.h`):

| Color ID | Value | Purpose |
|---|---|---|
| `CEF_ColorTextfieldBackground` | `#2a2a2e` | input background (lighter than window) |
| `CEF_ColorTextfieldForeground` | `#f0f0f0` | typed text (now readable) |
| `CEF_ColorTextfieldForegroundPlaceholder` | `#94969a` | placeholder text |
| `CEF_ColorTextfieldSelectionBackground` | `#7a5cff` | selection highlight |
| `CEF_ColorTextfieldSelectionForeground` | `#ffffff` | selected text |
| `CEF_ColorTextfieldOutline` / `…FilledUnderline` | `#3c3e4a` | field border |
| `CEF_ColorTextfieldFilledUnderlineFocused` | `#7a5cff` | focus highlight |

Re-applied automatically on OS/Chrome theme changes via
`CefWindowDelegate::OnThemeColorsChanged`. Verified by compiling
`browser_window.cc` against the pinned CEF 150 headers (`-fsyntax-only`,
clean).

### Visual polish

- **Cohesive dark theme:** refined toolbar (`#22232b`) and a distinct,
  darker tab-strip band (`#16171c`); purple accent `#7a5cff` matching the
  wordmark; roomier paddings/spacing throughout.
- **Toolbar buttons:** larger hit targets, centered glyphs, explicit
  per-state text colors including a **greyed disabled** state (back/forward
  dim automatically when there's no history). Added a **Home** (⌂) button
  that loads the new-tab page.
- **Tab strip:** clearer active-vs-inactive contrast, hover brightening on
  tab titles and the per-tab close (×) button, taller comfortable tabs (32px),
  a brighter-on-hover new-tab (+) button.
- **Address bar:** readable (see above), placeholder "Search with Brave or
  enter address", theme-driven focus outline, accessible name set.
- **New-tab page:** elegant radial-gradient dark background, larger centered
  Open**Nyx** wordmark, focus-glow search box, and five minimal quick-link
  tiles (Brave, Wikipedia, GitHub, HN, Maps). Clean and uncluttered.

Privacy defaults (Brave Search, no Google keys, metrics disabled) and all
keyboard shortcuts are unchanged.

---

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
│ ← → ⟳ ⌂ [ address / Brave Search …         ] │  toolbar
├──────────────────────────────────────────────┤
│                                              │
│              active CefBrowserView           │  content
│                                              │
└──────────────────────────────────────────────┘
```

- Dark-first theme (window `#18191e`, toolbar `#22232b`, tab strip `#16171c`,
  accent `#7a5cff`).
- Address-bar (textfield) colors come from window theme overrides
  (`CefWindow::SetThemeColor` + `CEF_ColorTextfield*`), not per-view setters,
  which are compiled out at this CEF API version.
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
- Rounded textfield corners (CEF Views textfields are square-cornered; would
  need an owner-drawn wrapper panel).
- Favicons in the tab strip (CefImage from `OnFaviconURLChange` download).
- Loading spinner / progress indication beyond the reload/stop glyph swap.
- Tab drag-reorder, middle-click close, tab overflow scrolling.
- `opennyx://` privileged pages (settings, privacy dashboard).
- Command palette (Ctrl+K), themes, blocker — see roadmap in README.
