// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
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

void OpenNyxClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Track the new browser (each tab / window is one CefBrowser).
  browser_list_.push_back(browser);
}

bool OpenNyxClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the last browser window requires special handling. See the
  // DoClose() documentation in cef_life_span_handler.h for details.
  if (browser_list_.size() == 1) {
    is_closing_ = true;
  }

  // Allow the close to proceed.
  return false;
}

void OpenNyxClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove from the list of existing browsers.
  for (auto bit = browser_list_.begin(); bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (browser_list_.empty()) {
    // All browser windows have closed; quit the application message loop.
    CefQuitMessageLoop();
  }
}

void OpenNyxClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // With the Chrome runtime style, Chromium renders its own error pages.
  // Keep a minimal fallback for completeness.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  std::stringstream ss;
  ss << "<html><body style=\"background:#111;color:#eee;font-family:sans-serif\">"
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
