// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).
//
// Windows entry point for OpenNyx. This executable is used for the browser
// process and all CEF sub-processes (render, GPU, network, ...).

#include <windows.h>

#include "include/cef_command_line.h"

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

  // Global CEF settings.
  CefSettings settings;

  // M1 runs without the Windows sandbox (a plain .exe, no bootstrap.exe /
  // DLL split). Enabling the sandbox is tracked for M2.
  settings.no_sandbox = true;

  // Initialize the CEF browser process.
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return CefGetExitCode();
  }

  // Run the CEF message loop. Blocks until CefQuitMessageLoop() is called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

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
