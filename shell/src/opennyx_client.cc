// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple/cefclient samples (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "opennyx_client.h"

#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "browser_window.h"

namespace {

OpenNyxClient* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

OpenNyxClient::OpenNyxClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

OpenNyxClient::~OpenNyxClient() {
  g_instance = nullptr;
}

// static
OpenNyxClient* OpenNyxClient::GetInstance() {
  return g_instance;
}

void OpenNyxClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->OnBrowserTitleChange(browser, title);
  }
}

void OpenNyxClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain()) {
    return;
  }
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->OnBrowserAddressChange(browser, url);
  }
}

void OpenNyxClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->OnBrowserLoadingStateChange(browser, isLoading, canGoBack,
                                        canGoForward);
  }
}

void OpenNyxClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Track the new browser (each tab / popup window is one CefBrowser).
  browser_list_.push_back(browser);
}

bool OpenNyxClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Allow the close to proceed. Cleanup happens in OnBeforeClose().
  return false;
}

void OpenNyxClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // If this browser was a tab in the main window, let the window update its
  // tab strip (and close itself if it was the last tab).
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->OnBrowserClosed(browser);
  }

  // Remove from the list of existing browsers.
  for (auto bit = browser_list_.begin(); bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (browser_list_.empty()) {
    // All browsers have closed; quit the application message loop.
    CefQuitMessageLoop();
  }
}

void OpenNyxClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Chromium renders its own error pages for most cases; keep a minimal
  // dark fallback for completeness.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  std::stringstream ss;
  ss << "<html><body style=\"background:#18191e;color:#e1e2e8;"
        "font-family:'Segoe UI',sans-serif;padding:40px\">"
        "<h2>OpenNyx failed to load "
     << std::string(failedUrl) << "</h2><p>" << std::string(errorText) << " ("
     << errorCode << ")</p></body></html>";
  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void OpenNyxClient::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the CEF UI thread.
    CefPostTask(TID_UI, base::BindOnce(&OpenNyxClient::CloseAllBrowsers, this,
                                       force_close));
    return;
  }

  for (const auto& browser : browser_list_) {
    browser->GetHost()->CloseBrowser(force_close);
  }
}
