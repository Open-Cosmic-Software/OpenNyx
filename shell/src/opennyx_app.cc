// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "opennyx_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include "browser_window.h"
#include "opennyx_client.h"

OpenNyxApp::OpenNyxApp() = default;

void OpenNyxApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  // Privacy hardening: applied to the browser process (and inherited by all
  // sub-processes). CEF already ships without Google API keys; these switches
  // remove the remaining phone-home surface.
  if (process_type.empty()) {
    // No metrics/UMA reporting.
    command_line->AppendSwitch("metrics-recording-only");
    // No crash reporting to Google.
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-crash-reporter");
    // No background talk-to-Google services.
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitch("disable-domain-reliability");
    command_line->AppendSwitch("disable-sync");
    command_line->AppendSwitch("disable-client-side-phishing-detection");
    // No hyperlink auditing pings.
    command_line->AppendSwitch("no-pings");
    // Skip first-run and default-browser nagging.
    command_line->AppendSwitch("no-first-run");
    command_line->AppendSwitch("no-default-browser-check");
    // Note: the user agent is deliberately left at the stock Chromium value.
    // A custom UA would only add fingerprinting surface.
  }
}

void OpenNyxApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // OpenNyxClient implements browser-level callbacks shared by all tabs.
  // The instance is retrieved via OpenNyxClient::GetInstance() when tabs
  // are created.
  CefRefPtr<OpenNyxClient> client(new OpenNyxClient());

  // Allow overriding the start page with --url=... for testing; the default
  // is the built-in dark new-tab page with a Brave Search box.
  const std::string url = command_line->GetSwitchValue("url");

  // Create the main browser window (tab strip + toolbar + first tab) using
  // the CEF Views framework. It shows itself after creation.
  BrowserWindow::Create(url);
}

CefRefPtr<CefClient> OpenNyxApp::GetDefaultClient() {
  // Called when a new browser window is created via the Chrome runtime style
  // UI (e.g. Ctrl+N, middle-click link -> new window).
  return OpenNyxClient::GetInstance();
}
