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
│   ├── opennyx_app.*     # CefApp: privacy switches + first window creation
│   └── opennyx_client.*  # CefClient: browser lifetime, load errors
└── win/                  # Windows manifests
```

## How it works (M1)

- The **CEF Standard binary distribution** (pinned version in
  `CMakeLists.txt`) is downloaded and extracted at CMake configure time into
  `shell/third_party/cef/` — binaries are never committed.
- The shell uses the **CEF Views framework** with the **Chrome runtime
  style**: the tab strip, toolbar (back/forward/reload) and address bar are
  real Chromium UI, not a re-implementation. M2 replaces this chrome with the
  custom OpenNyx UI (dark-first theme, command palette).
- **Privacy defaults** (see `opennyx_app.cc`):
  - Start page / search: DuckDuckGo
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

## Known M1 limitations

- Windows sandbox is disabled (`no_sandbox = true`). Current CEF sandbox
  support requires the `bootstrap.exe` + main-DLL split; adopting that is an
  M2 task.
- Single window is created at startup; new tabs/windows come from the Chrome
  runtime UI (Ctrl+T works because the tab strip is real Chromium).
- Windows only. macOS/Linux targets come later (the CMake file already
  detects the platforms).
