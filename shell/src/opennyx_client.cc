// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple/cefclient samples (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "opennyx_client.h"

#include <chrono>
#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_download_item.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "blocklist.h"
#include "browser_window.h"
#include "store.h"

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

void OpenNyxClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain()) {
    return;
  }
  // Record the visited page (the store filters internal/data URLs itself).
  const std::string url = frame->GetURL().ToString();
  std::string title;
  if (BrowserWindow* window = BrowserWindow::Get()) {
    title = window->GetBrowserTitle(browser);
  }
  OpenNyxStore::Get()->AddHistory(url, title);
  // Refresh the toolbar shield/star for the (possibly) active tab.
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->RefreshChromeForBrowser(browser);
  }
}

// ---- CefDownloadHandler ----

namespace {

// Extracts the host component of a URL (empty on failure).
std::string HostOf(const std::string& url) {
  CefURLParts parts;
  if (!CefParseURL(url, parts)) {
    return std::string();
  }
  return CefString(&parts.host).ToString();
}

void StoreDownload(CefRefPtr<CefDownloadItem> item, bool in_progress) {
  DownloadEntry e;
  e.id = item->GetId();
  e.url = item->GetURL().ToString();
  e.filename = item->GetSuggestedFileName().ToString();
  if (e.filename.empty() && !item->GetFullPath().empty()) {
    std::string fp = item->GetFullPath().ToString();
    const size_t slash = fp.find_last_of("/\\");
    e.filename = slash == std::string::npos ? fp : fp.substr(slash + 1);
  }
  e.full_path = item->GetFullPath().ToString();
  e.total_bytes = item->GetTotalBytes();
  e.received_bytes = item->GetReceivedBytes();
  e.percent = item->GetPercentComplete();
  e.complete = item->IsComplete();
  e.canceled = item->IsCanceled();
  e.in_progress = in_progress && item->IsInProgress();
  e.ts = std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();
  OpenNyxStore::Get()->UpsertDownload(e);
}

}  // namespace

bool OpenNyxClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  // Show the OS save dialog and record the item.
  callback->Continue(suggested_name, /*show_dialog=*/true);
  StoreDownload(download_item, /*in_progress=*/true);
  return true;
}

void OpenNyxClient::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  StoreDownload(download_item, /*in_progress=*/true);
}

// ---- CefRequestHandler / CefResourceRequestHandler (tracker blocking) ----

CefRefPtr<CefResourceRequestHandler>
OpenNyxClient::GetResourceRequestHandler(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefRequest> request,
                                         bool is_navigation,
                                         bool is_download,
                                         const CefString& request_initiator,
                                         bool& disable_default_handling) {
  return this;
}

cef_return_value_t OpenNyxClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
  OpenNyxBlocklist* bl = OpenNyxBlocklist::Get();
  if (!bl->enabled()) {
    return RV_CONTINUE;
  }
  const std::string url = request->GetURL().ToString();
  // Never block our own scheme.
  if (url.compare(0, 10, "opennyx://") == 0) {
    return RV_CONTINUE;
  }
  // Extract host of the request and of the first-party document.
  const std::string req_host = HostOf(url);
  std::string fp_host;
  if (frame) {
    fp_host = HostOf(frame->GetURL().ToString());
  }
  if (req_host.empty()) {
    return RV_CONTINUE;
  }
  if (bl->ShouldBlock(req_host, fp_host)) {
    bl->RecordBlock(fp_host);
    return RV_CANCEL;
  }
  return RV_CONTINUE;
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
