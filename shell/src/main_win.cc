// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).
//
// Windows entry point for OpenNyx. This executable is used for the browser
// process and all CEF sub-processes (render, GPU, network, ...).

#include <windows.h>

#include "include/cef_command_line.h"

#include <shlobj.h>

#include <string>

#include "opennyx_app.h"

namespace {

int RunMain(HINSTANCE hInstance, int nCmdShow) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // OpenNyxApp implements application-level callbacks. It MUST be created
  // before CefExecuteProcess and passed to BOTH CefExecuteProcess and
  // CefInitialize so that OnRegisterCustomSchemes() runs in EVERY process
  // type (browser, render, GPU, ...). This is critical: our privileged
  // "opennyx" scheme is declared STANDARD|SECURE, and the RENDER process
  // must know that to load opennyx://newtab. If the app is null for
  // sub-processes, the render process treats opennyx:// as an unknown/opaque
  // scheme and the start page silently fails to load (blank white page).
  CefRefPtr<OpenNyxApp> app(new OpenNyxApp);

  // CEF applications have multiple sub-processes that share the same
  // executable. This function checks the command line and, if this is a
  // sub-process, executes the appropriate logic (running OnRegisterCustomSchemes
  // on the way in).
  int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed, so return here.
    return exit_code;
  }

  // ---- Single-instance guard (browser process only) ----------------------
  // Reached ONLY by the real browser-process launch (sub-processes returned
  // above). A named, per-session mutex lets a second launch detect the first
  // and hand off instead of spawning its own (bare Chrome) window. This does
  // not depend on any CEF-internal process singleton (which the Alloy runtime
  // does not reliably provide).
  HANDLE singleton = CreateMutexW(nullptr, TRUE, L"OpenNyx_SingleInstance_Mutex");
  if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
    // Another OpenNyx is already running. Find its top-level window and bring
    // it to the foreground, then exit before touching CEF.
    HWND existing = FindWindowW(L"CefWindowClass", nullptr);
    if (!existing) {
      existing = FindWindowW(nullptr, L"OpenNyx");
    }
    if (existing) {
      if (IsIconic(existing)) {
        ShowWindow(existing, SW_RESTORE);
      }
      SetForegroundWindow(existing);
    }
    ReleaseMutex(singleton);
    CloseHandle(singleton);
    return 0;
  }

  // Global CEF settings.
  CefSettings settings;

  // M1 runs without the Windows sandbox (a plain .exe, no bootstrap.exe /
  // DLL split). Enabling the sandbox is tracked for M2.
  settings.no_sandbox = true;

  // Persist a single, stable user-data / cache directory so history, cookies,
  // bookmarks and settings survive restarts. (Single-instance is handled by
  // the named mutex above, not by this path.)
  // Compute the cache dir with the native API (CefGetPath is not available
  // until after CefInitialize). Prefer %LOCALAPPDATA%\OpenNyx; fall back to
  // the executable directory.
  std::string data_dir;
  {
    wchar_t* local_appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                       &local_appdata)) &&
        local_appdata) {
      const std::wstring w = std::wstring(local_appdata) + L"\\OpenNyx";
      CoTaskMemFree(local_appdata);
      const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                        nullptr, nullptr);
      if (n > 0) {
        std::string utf8(static_cast<size_t>(n - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, utf8.data(), n, nullptr,
                            nullptr);
        data_dir = utf8;
      }
    }
  }
  if (!data_dir.empty()) {
    CreateDirectoryA(data_dir.c_str(), nullptr);
    CefString(&settings.root_cache_path).FromString(data_dir);
  }

  // Initialize the CEF browser process.
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return CefGetExitCode();
  }

  // Run the CEF message loop. Blocks until CefQuitMessageLoop() is called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  if (singleton) {
    ReleaseMutex(singleton);
    CloseHandle(singleton);
  }

  return 0;
}

}  // namespace

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  return RunMain(hInstance, nCmdShow);
}
