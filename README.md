# OpenNyx

**A beautiful, Google-free browser for developers and privacy lovers.**

OpenNyx is built on **CEF (Chromium Embedded Framework)** — real, full
Chromium that we embed and control: no Google API keys, no metrics, no crash
reporting, no background phone-home. Full web compatibility, zero Google
services.

> 🚧 **Early development.** Windows first, macOS and Linux to follow.

## What works today (M2)

- Real Chromium (CEF 150) with a custom dark UI: **tab strip, toolbar,
  address bar** — built with the CEF Views framework, no Chrome UI
  dependencies.
- **Multi-tab browsing**: new tab (`Ctrl+T` / `+` button), close tab
  (`Ctrl+W` / `×`), switch (`Ctrl+Tab`, click).
- **Brave Search by default**: type a URL to navigate, anything else
  searches Brave. Dark new-tab page with a centered search box.
- Shortcuts: `Ctrl+L` address bar, `F5`/`Ctrl+R` reload, `Alt+←/→`
  history, `Ctrl+Shift+I` DevTools. Full list in
  [`docs/BUILD-STATUS.md`](docs/BUILD-STATUS.md).
- Privacy defaults: no Google API keys, metrics/crash reporting disabled,
  no background phone-home switches.

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
prebuilt CEF binary distributions and apply ungoogled-chromium-style hardening
via the command line and shell code. That keeps security updates fast (bump
the pinned CEF version) and the project maintainable for a small crew.

## Roadmap

- [x] Project setup
- [x] M1 — real Chromium (CEF) embedded, window, navigation, Windows CI artifact
- [x] M2 — **shipped**: custom dark UI (tab strip, toolbar, address bar), multi-tab, Brave Search default, new-tab page, keyboard shortcuts, app icon
- [ ] M3 — Privacy layer: blocker, fingerprint protection, DoH; command palette, settings
- [ ] M4 — Dev tools: JSON viewer, request inspector, API client
- [ ] M5 — macOS & Linux builds
- [ ] M6 — Auto-updates & release pipeline

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
