# OpenNyx

**A beautiful, Google-free browser for developers and privacy lovers.**

OpenNyx is built on top of [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium) — full Chromium compatibility, zero Google services, zero telemetry.

> 🚧 **Early development.** Windows first, macOS and Linux to follow.

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
│       Engine: ungoogled-chromium      │   ← upstream, de-googled
│     (Blink · V8 · full web compat)    │
└───────────────────────────────────────┘
```

We deliberately do **not** fork Chromium itself. The engine stays upstream
(ungoogled-chromium), we build the shell, features and experience on top.
That keeps security updates fast and the project maintainable.

## Roadmap

- [x] Project setup
- [ ] M1 — Prototype: engine embedded, basic window, tabs, navigation (Windows)
- [ ] M2 — Shell: omnibox, command palette, dark theme, settings
- [ ] M3 — Privacy layer: blocker, fingerprint protection, DoH
- [ ] M4 — Dev tools: JSON viewer, request inspector, API client
- [ ] M5 — macOS & Linux builds
- [ ] M6 — Auto-updates & release pipeline

## Building

Coming with M1. Windows builds will be provided as CI artifacts.

## License

MIT © Open Cosmic Software
