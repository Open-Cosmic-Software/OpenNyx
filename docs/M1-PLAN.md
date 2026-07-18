# OpenNyx — M1 Technical Plan

**Milestone M1:** First clickable browser window with a tab strip and an omnibox,
buildable for **Windows** from our **Linux server** (no Windows machine required).

This document evaluates the realistic engine/shell stacks, picks one, and lays out
the concrete next steps + a CI build skeleton.

---

## 1. The core constraint

We build on a Linux server and have **no Windows machine**. So the real question is
not "can I cross-compile a browser from Linux" (mostly a dead end for anything
Chromium-sized) but **"which stack produces a Windows binary from a GitHub Actions
`windows-latest` runner with the least effort for a small crew?"**

GitHub Actions gives us free Windows runners. That is our Windows build machine.
Every option below is judged on: (a) does it build cleanly on a `windows-latest`
runner, (b) can it manage multiple tabs/webviews, (c) privacy / no phone-home,
(d) bundle size, (e) maintainability for a 1–3 person crew.

---

## 2. Candidate stacks

### A. Tauri v2 (Rust + Wry → WebView2 on Windows)
- **What it is:** Rust backend, Wry renders through the OS webview. On Windows that
  is **WebView2** (Microsoft Edge / Chromium). TAO handles the native window.
- **Windows build from Linux:** Direct Linux→MSVC cross-compile is explicitly
  *"last resort, not tested much"* per Tauri docs. **The blessed path is a
  `windows-latest` GitHub Actions runner** — trivial, first-class, documented.
- **Tabs / multiple webviews:** Supported via multi-webview, currently behind the
  **`unstable`** feature flag (`tauri` crate `features = ["unstable"]`,
  `WebviewBuilder` / `add_child`). Works, but is WIP — expect rough edges
  (known white-on-load bug #10011). Good enough for an M1 prototype.
- **Privacy:** ⚠️ The honest caveat. WebView2 *is* Edge and can phone home to
  Microsoft. We do **not** control the engine → we cannot truly "de-google/
  de-microsoft" it the way the README promises. Mitigations exist (fixed-version
  WebView2 runtime, `--disable-features`, registry/policy toggles) but it is not
  the same as shipping ungoogled-chromium.
- **Bundle size:** Tiny (single-digit MB shell; WebView2 runtime is on the user's
  machine or bundled ~100 MB fixed-version).
- **Maintainability:** ✅ Excellent for a small crew. Web UI (HTML/CSS/JS) for the
  shell chrome, Rust only for OS glue.

### B. CEF — native C++ (or CefSharp / cefpython bindings)
- **What it is:** Chromium Embedded Framework — a real, full Chromium we embed and
  control. This is what the current `docs/ARCHITECTURE.md` picked.
- **Windows build from Linux:** ❌ You do **not** cross-compile CEF from Linux for
  Windows. You consume **prebuilt CEF Windows binaries** (Spotify autobuild
  distributions) and link your host app against them **on a Windows runner**. So:
  still a `windows-latest` GitHub Actions job, but with a much heavier native
  C++/CMake (or .NET for CefSharp) toolchain to maintain.
- **CefSharp** is **.NET / Windows-only** — great DX but locks the shell to Windows,
  contradicting the cross-platform roadmap. **cefpython** is largely unmaintained.
  **Native C++ CEF** is the only genuinely cross-platform CEF route and is the most
  work.
- **Tabs:** ✅ First-class — CEF is *designed* for one browser object per tab
  (`cefclient` demonstrates exactly this).
- **Privacy:** ✅ Best of the bunch. We own the Chromium command line, can apply
  ungoogled-chromium-style switches, strip Google endpoints, control DoH, etc.
- **Bundle size:** ~150–200 MB (full Chromium ships in the app).
- **Maintainability:** ⚠️ Heaviest. Native build, manual Chromium switch curation,
  CEF version bumps every ~4 weeks.

### C. Electron (incl. "ungoogled-electron"?)
- Ships a full Chromium (~150 MB "hello world"), and it is **stock Google Chromium
  with Google services baked in** → directly against our "Google-free" promise.
- There is **no maintained "ungoogled-electron"** distribution. Rebuilding Electron
  against ungoogled patches is a full-Chromium-build project (100 GB+, the exact
  thing ARCHITECTURE.md already ruled out).
- Tabs via `<webview>`/`BrowserView` are easy, DX is great — but the privacy story
  and "it's an app, not a browser" feel disqualify it for OpenNyx. ❌

### D. Raw ungoogled-chromium binaries + our patches
- The "purest" privacy answer and the spiritual goal of the project, but it means
  **maintaining a Chromium fork/build** (multi-GB, multi-hour builds, a dedicated
  release engineer). Not an M1 activity; not viable for the first clickable window.
  Revisit only if the project grows a build team. ❌ for M1.

---

## 3. Decision

**For M1 we ship a Tauri v2 shell (Rust + Wry, WebView2 on Windows), built on a
GitHub Actions `windows-latest` runner.**

Why, in three sentences: Tauri v2 is the only option a 1–3 person crew can turn into
a clickable, tabbed Windows window in days rather than months, and it builds
first-class on a free GitHub Actions Windows runner so our Linux server never needs
Windows. The entire shell (tab strip, omnibox, command palette, themes) is plain
web UI that is **100% reusable no matter which engine we settle on later**, so this
work is never thrown away. We knowingly accept one debt for M1 — WebView2 is Edge
and cannot be fully de-googled — and we **defer the true privacy engine (native CEF
against ungoogled-chromium switches) to a later milestone**, at which point we swap
the engine *under* the same shell.

### How this reconciles with `docs/ARCHITECTURE.md`
ARCHITECTURE.md correctly picks **CEF / ungoogled-chromium as the long-term
engine**. Nothing here overturns that. M1 is explicitly a **shell / UX prototype
milestone**: prove the window, tabs and navigation UX fast, on real Windows builds,
with throwaway-free UI code. The engine swap (WebView2 → CEF) is an M2/M3 decision
once the shell exists and we can measure whether WebView2's privacy posture is
acceptable or whether CEF is worth its weight.

> **Privacy honesty rule:** until the CEF engine lands, the README/marketing must
> not claim "zero telemetry" for the WebView2-backed builds. We label M1 builds as
> a **preview/prototype**, not the private browser we're promising.

---

## 4. Concrete next steps (M1 backlog)

1. **Scaffold** the Tauri v2 shell under `shell/` (done in this commit — see
   `shell/README.md`). Frontend = a single static `index.html` (no framework yet;
   add Svelte/React only when the UI grows).
2. **Enable multi-webview:** add `features = ["unstable"]` to the `tauri` dependency
   and spike one window hosting N child webviews (one per tab) behind a tab strip.
   Track the known white-on-load issue (#10011); fall back to a single webview that
   swaps `src` per tab if multi-webview is too flaky for M1.
3. **Omnibox:** input in the shell → Rust command → navigate the active webview.
   Default search engine = a privacy engine (DuckDuckGo/Startpage), never Google.
4. **CI:** land `.github/workflows/build-windows.yml` (skeleton below). Produce a
   portable `.exe` + NSIS installer as build artifacts on every push/tag.
5. **Manual test loop:** download the CI artifact, run on a Windows box (Fabian /
   a throwaway VM), confirm: window opens, two tabs, type a URL, page loads, close
   tab. That's M1 "done" (VBR: verify the artifact, don't just trust a green CI).
6. **Privacy spike (parallel):** enumerate every network request a bare WebView2
   window makes on Windows and document mitigations — this feeds the M2 engine
   decision.

---

## 5. GitHub Actions build skeleton

Save as `.github/workflows/build-windows.yml`. Builds on a real Windows runner, so
our Linux server only ever pushes code.

```yaml
name: Build Windows

on:
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Rust
        uses: dtolnay/rust-toolchain@stable
        with:
          targets: x86_64-pc-windows-msvc

      - name: Setup Node
        uses: actions/setup-node@v4
        with:
          node-version: 20

      - name: Rust cache
        uses: swatinem/rust-cache@v2
        with:
          workspaces: shell/src-tauri

      - name: Install frontend deps
        working-directory: shell
        run: npm ci

      # WebView2 runtime is preinstalled on windows-latest images.
      - name: Build Tauri app
        working-directory: shell
        run: npm run tauri build

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: opennyx-windows
          path: |
            shell/src-tauri/target/release/opennyx.exe
            shell/src-tauri/target/release/bundle/nsis/*.exe
          if-no-files-found: warn
```

Notes:
- `windows-latest` ships the WebView2 runtime, so no extra install step for M1.
- For reproducible/offline installs later, switch to a **fixed-version WebView2**
  bundle in `tauri.conf.json` (`bundle.windows.webviewInstallMode`).
- Add a `build-linux` / `build-macos` job in M5 — same pattern, different runner.

---

## 6. Risks & watch-list

| Risk | Impact | Mitigation |
|---|---|---|
| Multi-webview is `unstable` (bug #10011) | Tabs flaky in M1 | Fall back to single webview + per-tab `src` swap |
| WebView2 = Edge telemetry | Undercuts privacy promise | Label M1 as preview; do the privacy spike; plan CEF swap |
| WebView2 lags stable Chrome features | Site breakage | Acceptable for prototype; CEF pins exact Chromium later |
| Engine swap later = rework | Wasted effort | Keep shell UI engine-agnostic; isolate nav behind a thin Rust command layer |

---

*Author: research pass for M1. Decision is reversible at M2 once the shell exists.*
