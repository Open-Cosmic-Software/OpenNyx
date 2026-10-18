// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#ifndef OPENNYX_SHELL_SRC_OPENNYX_APP_H_
#define OPENNYX_SHELL_SRC_OPENNYX_APP_H_

#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"

// Application-level callbacks for the browser process.
//
// OpenNyxApp is responsible for:
//  * Injecting privacy-hardening command-line switches into every process
//    (OnBeforeCommandLineProcessing).
//  * Creating the first browser window once the CEF context is ready
//    (OnContextInitialized).
//  * Fingerprint shield (render process): runs fp_shield.h's script in every
//    http(s) page context before the page's own scripts (OnContextCreated).
class OpenNyxApp : public CefApp,
                   public CefBrowserProcessHandler,
                   public CefRenderProcessHandler {
 public:
  OpenNyxApp();

  OpenNyxApp(const OpenNyxApp&) = delete;
  OpenNyxApp& operator=(const OpenNyxApp&) = delete;

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }
  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override;
  void OnRegisterCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar) override;

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;
  void OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) override;

  // CefRenderProcessHandler methods:
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

 private:
  IMPLEMENT_REFCOUNTING(OpenNyxApp);
};

#endif  // OPENNYX_SHELL_SRC_OPENNYX_APP_H_
