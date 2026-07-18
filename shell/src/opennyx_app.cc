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

#include "opennyx_client.h"

namespace {

// Delegate for the top-level CefWindow that hosts the browser view.
class OpenNyxWindowDelegate : public CefWindowDelegate {
 public:
  explicit OpenNyxWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  OpenNyxWindowDelegate(const OpenNyxWindowDelegate&) = delete;
  OpenNyxWindowDelegate& operator=(const OpenNyxWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->SetTitle("OpenNyx");
    window->AddChildView(browser_view_);
    window->Show();

    // Give keyboard focus to the browser view.
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(1200, 800);
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(OpenNyxWindowDelegate);
};

// Delegate for the browser view. Enables the Chrome toolbar (back/forward/
// reload buttons + address bar) provided by the Chrome runtime style.
class OpenNyxBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  OpenNyxBrowserViewDelegate() = default;

  OpenNyxBrowserViewDelegate(const OpenNyxBrowserViewDelegate&) = delete;
  OpenNyxBrowserViewDelegate& operator=(const OpenNyxBrowserViewDelegate&) =
      delete;

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    // Create a new top-level window for popups (and DevTools). It will show
    // itself after creation.
    CefWindow::CreateTopLevelWindow(
        new OpenNyxWindowDelegate(popup_browser_view));
    return true;  // We created the window.
  }

  ChromeToolbarType GetChromeToolbarType(
      CefRefPtr<CefBrowserView> browser_view) override {
    // Show the full Chrome toolbar: back/forward/reload + omnibox. This is
    // real Chromium toolbar code, not a re-implementation. M2 will replace
    // this with the custom OpenNyx shell UI (tab strip, command palette).
    return CEF_CTT_NORMAL;
  }

 private:
  IMPLEMENT_REFCOUNTING(OpenNyxBrowserViewDelegate);
};

}  // namespace

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

  // OpenNyxClient implements browser-level callbacks.
  CefRefPtr<OpenNyxClient> client(new OpenNyxClient());

  // Browser settings (defaults are fine for M1).
  CefBrowserSettings browser_settings;

  // Allow overriding the start page with --url=... for testing; default is
  // DuckDuckGo (Google-free).
  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = kOpenNyxStartPage;
  }

  // Create the browser view using the Views framework with the Chrome
  // runtime style (real Chromium toolbar, DevTools, context menus).
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      client, url, browser_settings, nullptr, nullptr,
      new OpenNyxBrowserViewDelegate());

  // Create the top-level window. It shows itself after creation.
  CefWindow::CreateTopLevelWindow(new OpenNyxWindowDelegate(browser_view));
}

CefRefPtr<CefClient> OpenNyxApp::GetDefaultClient() {
  // Called when a new browser window is created via the Chrome runtime style
  // UI (e.g. Ctrl+N, middle-click link -> new window).
  return OpenNyxClient::GetInstance();
}
