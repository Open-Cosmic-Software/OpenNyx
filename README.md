# OpenNyx

**A beautiful, Google-free browser for developers and privacy lovers.**

OpenNyx is built on **CEF (Chromium Embedded Framework)** — real, full
Chromium that we embed and control: no Google API keys, no metrics, no crash
reporting, no background phone-home. Full web compatibility, zero Google
services.

> 🚧 **Early development.** Windows first, macOS and Linux to follow.

## What works today (M3 + M4)

- Real Chromium (CEF 150) with a custom dark UI: **tab strip, toolbar,
  address bar** — built with the CEF Views framework, no Chrome UI
  dependencies.
- **Multi-tab browsing**: new tab (`Ctrl+T` / `+` button), close tab
  (`Ctrl+W` / `×`), switch (`Ctrl+Tab`, click).
- **Everyday features (M3):**
  - **History** — every visited page is recorded locally; browse/search/clear
    it at `opennyx://history` (`Ctrl+H`).
  - **Bookmarks** — star the current page (toolbar ☆ / `Ctrl+D`); manage them
    at `opennyx://bookmarks`.
  - **Downloads** — files download with a save dialog and appear with live
    progress at `opennyx://downloads` (`Ctrl+J`).
  - **Settings** — `opennyx://settings` (`Ctrl+,`): default search engine
    (Brave/DuckDuckGo/Mojeek/custom), homepage, privacy toggles, clear
    browsing data — all persisted to a local config file.
- **Privacy layer (M4):**
  - **Tracker & ad blocking** — a bundled ~250-domain blocklist cancels known
    ad/tracking requests at the network layer; a **toolbar shield** shows the
    per-site blocked count. Toggle-able in Settings.
  - **DNS-over-HTTPS** — on by default in *secure* mode; resolver selectable
    (Cloudflare / Quad9 🇨🇭 / Mullvad 🇸🇪 / custom).
  - **Network audit** — a documented, reproducible procedure to capture every
    outbound connection and verify the *de-googled at runtime* claim:
    [`docs/NETWORK-AUDIT.md`](docs/NETWORK-AUDIT.md).
- **Privileged `opennyx://` pages** served locally by a custom scheme handler
  with a small JSON bridge (`opennyx://api/*`) — no render-process plumbing.
- Shortcuts: `Ctrl+L` address bar, `F5`/`Ctrl+R` reload, `Alt+←/→` history,
  `Ctrl+Shift+I` DevTools, `Ctrl+H`/`Ctrl+D`/`Ctrl+J`/`Ctrl+,`. Full list in
  [`docs/BUILD-STATUS.md`](docs/BUILD-STATUS.md).
- Privacy defaults: no Google API keys, metrics/crash reporting disabled,
  no background phone-home switches. **De-googled at runtime.**

## Why OpenNyx?

- 🔒 **Privacy by default** — no Google pings, no telemetry, tracker & ad blocking built in, DNS-over-HTTPS out of the box
- 🛠️ **Made for developers** — command palette (Ctrl+K), built-in JSON viewer, request inspector, API client — no extension zoo needed
- 🎨 **Actually beautiful** — a clean, dark-first UI that gets out of your way
- 🌍 **Cross-platform by design** — one codebase, Windows / macOS / Linux targets
- 📖 **Fully open source** — MIT licensed, no strings attached

## Architecture

```
┌───────────────────────────────────────┐
│           OpenNyx Shell (UI)          │   ← our code: tabs, omnibox,
│   command palette · themes · tools    │     dev tools, privacy dashboard
├───────────────────────────────────────┤
│      Engine: Chromium via CEF         │   ← prebuilt CEF binary distro,
│     (Blink · V8 · full web compat)    │     privacy-hardened switches
└───────────────────────────────────────┘
```

We deliberately do **not** fork Chromium itself. We consume the official
prebuilt CEF binary distributions and **de-google them at runtime** via the
command line and shell code (telemetry/updates/crash-reporting/sync disabled,
no Google API keys, encrypted DNS, tracker blocking). That keeps security
updates fast (bump the pinned CEF version) and the project maintainable for a
small crew. See [`docs/ENGINE-STRATEGY.md`](docs/ENGINE-STRATEGY.md) for the
roadmap toward a source-patched engine, and
[`docs/NETWORK-AUDIT.md`](docs/NETWORK-AUDIT.md) for how to verify the claim.

> **Wording:** we say **"de-googled at runtime"**, not "ungoogled" —
> ungoogled-chromium is a source-patched build; OpenNyx ships stock CEF and
> removes Google's runtime phone-home surface via flags + blocking.

### Local data & storage

History, bookmarks, downloads and settings are stored as plain **JSON files**
in an `OpenNyx/` folder in the CEF user-data directory (chosen over bundling
the SQLite amalgamation to keep the build dependency-light; the storage backend
is isolated behind `OpenNyxStore` and can be swapped later). Everything stays
on your device.

## Roadmap

- [x] Project setup
- [x] M1 — real Chromium (CEF) embedded, window, navigation, Windows CI artifact
- [x] M2 — **shipped**: custom dark UI (tab strip, toolbar, address bar), multi-tab, Brave Search default, new-tab page, keyboard shortcuts, app icon
- [x] M3 — **shipped**: history, bookmarks, downloads, settings (`opennyx://` pages) with local persistence
- [x] M4 — **shipped**: privacy layer — tracker/ad blocking + shield, DNS-over-HTTPS, network-audit trust proof
- [ ] M5 — Dev tools: JSON viewer, request inspector, API client; command palette (Ctrl+K)
- [ ] M6 — macOS & Linux builds
- [ ] M7 — Auto-updates & release pipeline

## Building

See [`shell/README.md`](shell/README.md). Short version (Windows x64,
VS 2022 + CMake):

```powershell
cmake -S shell -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target opennyx --parallel
```

Every push to `main` also builds on GitHub Actions (`windows-latest`) and
uploads a ready-to-run **OpenNyx-win64** artifact (unzip → `opennyx.exe`).

## License

MIT © Open Cosmic Software
