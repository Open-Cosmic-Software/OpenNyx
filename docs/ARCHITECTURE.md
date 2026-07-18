# OpenNyx Architecture

## Engine decision

Three candidate approaches were considered:

| Approach | Web compat | Effort | Security updates | Verdict |
|---|---|---|---|---|
| Full Chromium fork (Brave-style) | perfect | extreme (100GB+ builds, team required) | on us | ❌ not viable for a small team |
| Electron | good | low | easy | ❌ ships Google services, heavy, "app" not "browser" |
| **CEF / ungoogled-chromium base** | perfect | medium | upstream handles engine | ✅ **chosen** |

**Chosen path:** Use CEF (Chromium Embedded Framework) built against
ungoogled-chromium patches where possible, with our own browser shell.
The shell owns: window chrome, tab strip, omnibox, shortcuts, settings,
privacy dashboard, dev tooling. The engine renders the web.

## Components

### 1. Shell (`/shell`)
Native UI layer. Tab management, omnibox with search/URL handling
(default: privacy search engines), command palette, themes.

### 2. Privacy layer (`/privacy`)
- Network-level blocker (EasyList/EasyPrivacy compatible filter engine)
- DNS-over-HTTPS resolver (default on)
- Fingerprint surface reduction (canvas noise, UA normalization)
- No phone-home: the browser makes zero requests we don't document

### 3. Dev tools (`/devtools-extra`)
Additions on top of Chromium DevTools:
- JSON pretty-viewer for API responses
- Request inspector / replay
- Built-in REST client
- Command palette integration (Ctrl+K)

### 4. Build & release (`/build`)
- Windows first (NSIS installer + portable zip)
- macOS (dmg, later notarization)
- Linux (AppImage + deb)
- CI: GitHub Actions, artifacts per platform

## Update strategy

Engine updates track ungoogled-chromium releases. Shell updates are ours.
Target: engine bump within 7 days of upstream security release.

## Non-goals (for now)

- Sync service (maybe later, E2E-encrypted only)
- Mobile
- Extension store (Chromium extensions load via developer mode / crx)
