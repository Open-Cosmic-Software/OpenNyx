# OpenNyx Shell (CEF)

The native OpenNyx browser shell, built on **CEF (Chromium Embedded
Framework)** — a real, full Chromium that we embed and control. No Google API
keys, no metrics, no crash reporting to Google.

## Layout

```
shell/
├── CMakeLists.txt        # downloads the CEF binary distro, builds opennyx.exe
├── cmake/DownloadCEF.cmake
├── src/
│   ├── main_win.cc       # Windows entry point (browser + all sub-processes)
│   ├── opennyx_app.*     # CefApp: privacy switches + main window creation
│   ├── opennyx_client.*  # CefClient: browser lifetime, title/address/load
│   └── browser_window.*  # Views UI: tab strip, toolbar, address bar, tabs
└── win/                  # Windows manifests, icon, version resource
```

## How it works (M2)

- The **CEF Standard binary distribution** (pinned version in
  `CMakeLists.txt`) is downloaded and extracted at CMake configure time into
  `shell/third_party/cef/` — binaries are never committed.
- The shell uses the **CEF Views framework** with a fully **custom OpenNyx
  UI** (`src/browser_window.*`): a dark tab strip and toolbar
  (back/forward/reload + address bar) built from CefPanel / CefLabelButton /
  CefTextfield views, with one CefBrowserView per tab. The M1 approach
  (Chrome runtime toolbar via `CEF_CTT_NORMAL`) was dropped because it
  silently rendered nothing on tester machines — see
  [`../docs/BUILD-STATUS.md`](../docs/BUILD-STATUS.md).
- **Privacy defaults** (see `opennyx_app.cc`):
  - Search: Brave Search (address bar + new-tab page)
  - `metrics-recording-only`, `disable-breakpad`, `disable-crash-reporter`
  - `disable-background-networking`, `disable-component-update`,
    `disable-domain-reliability`, `disable-sync`, `no-pings`
  - Stock Chromium user agent (a custom UA would only add fingerprint
    surface)
  - CEF binary distributions ship **without Google API keys** by default.

## Building (Windows x64)

Requires Visual Studio 2022 (or Build Tools) + CMake 3.21+.

```powershell
cmake -S shell -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target opennyx --parallel
# → build/Release/opennyx.exe (runnable directly; all DLLs/resources copied)
```

CI does exactly this on `windows-latest` and uploads the `OpenNyx-win64`
artifact (see `.github/workflows/windows-build.yml`).

## Known M2 limitations

- Windows sandbox is disabled (`no_sandbox = true`). Current CEF sandbox
  support requires the `bootstrap.exe` + main-DLL split; adopting that is a
  later milestone.
- No favicons / loading spinner in the tab strip yet; no tab drag-reorder.
- Windows only. macOS/Linux targets come later (the CMake file already
  detects the platforms).
