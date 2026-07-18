# OpenNyx — M1 Technical Plan (revised: direct CEF)

**Milestone M1:** a real, runnable Windows browser — window titled "OpenNyx",
tabs, navigation (back/forward/reload), address bar — with **real Chromium
(CEF)** as the engine from day one. Built on GitHub Actions `windows-latest`
runners (our dev server is Linux; we never compile Chromium ourselves).

> **Decision update (2026-07-18):** the earlier revision of this document
> proposed a Tauri v2 / WebView2 preview for M1 with a later engine swap to
> CEF. That path is **obsolete**. WebView2 is Edge — it can phone home to
> Microsoft and we don't control the engine, which contradicts the core
> promise of OpenNyx. We skip the preview entirely and build **directly on
> CEF**. The engine is real Chromium, Google-service-free, from the first
> artifact.

---

## 1. Why direct CEF works for M1

The old objection to CEF was toolchain weight. It evaporates once you use the
intended workflow:

- CEF publishes **prebuilt binary distributions** (Spotify automated builds,
  `cef-builds.spotifycdn.com`). Nobody compiles Chromium — you download a
  ~700 MB Standard Distribution and link a small C++ app against it.
- The distribution ships **CMake configs** (`find_package(CEF)`), the
  `libcef_dll_wrapper` static library, and two reference apps (`cefsimple`,
  `cefclient`). A minimal shell is a few hundred lines of C++.
- The **CEF Views framework** + **Chrome runtime style** give us a real
  Chromium tab strip, toolbar and omnibox without writing any UI code. That
  is not a mock — it is Chromium's own toolbar. M2 replaces it with the
  OpenNyx custom shell.
- GitHub Actions `windows-latest` has VS 2022 + CMake preinstalled. The CI
  job is: checkout → cmake configure (downloads CEF, cached) → build →
  upload zip.

## 2. What M1 ships

- `shell/` — C++ CEF app (see `shell/README.md`):
  - `main_win.cc` — entry point, `CefExecuteProcess` / `CefInitialize` /
    message loop. Sandbox off for M1 (bootstrap-DLL split is an M2 task).
  - `opennyx_app.*` — privacy command-line switches, creates the first
    window (Views + Chrome style, `CEF_CTT_NORMAL` toolbar).
  - `opennyx_client.*` — browser lifetime tracking, clean shutdown, load
    error page.
- Privacy defaults: DuckDuckGo start page, no Google API keys (CEF default),
  no metrics/crash reporting/background networking/sync/pings, stock UA.
- `.github/workflows/windows-build.yml` — `windows-latest`, VS 2022
  generator, CEF distro cached, uploads **OpenNyx-win64** artifact
  (unzip → double-click `opennyx.exe`).
- Pinned CEF: `150.0.11+gb887805+chromium-150.0.7871.115` (stable channel,
  Chromium 150).

## 3. Explicitly out of scope for M1

- Custom shell UI (tab strip / omnibox / command palette) — M2. The Chrome
  runtime style already provides working tabs & navigation, so M1 is a real
  multi-tab browser.
- Windows sandbox (bootstrap.exe split) — M2.
- ungoogled-chromium-style deep patches, filter-list blocker, DoH config UI —
  M3 (privacy layer).
- macOS/Linux builds — M5 (CMake already platform-detects).
- Installer/auto-update — M6. M1 is a portable zip.

## 4. M2 outlook

1. Custom window chrome: `CEF_CTT_NONE` + our own Views-based (or HTML-based)
   tab strip and omnibox, dark-first theme.
2. Sandbox on: adopt the `bootstrap.exe` + main-app-DLL layout.
3. Settings persistence (profile dir, search engine choice).
4. CEF version bump automation (renovate-style PR when a new stable lands).
