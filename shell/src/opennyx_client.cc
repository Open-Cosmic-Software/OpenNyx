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

void OpenNyxClient::OnFindResult(CefRefPtr<CefBrowser> browser,
                                 int identifier,
                                 int count,
                                 const CefRect& selection_rect,
                                 int active_match_ordinal,
                                 bool final_update) {
  CEF_REQUIRE_UI_THREAD();
  if (BrowserWindow* window = BrowserWindow::Get()) {
    window->OnFindResult(browser, count, active_match_ordinal);
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

// Autofill content-script. The browser process injects this on every page load
// with the credentials for the current site spliced in between HEAD and TAIL as
// a JS array literal `var __onx_creds = [...]`. The script:
//   * fills the first empty username/password pair when exactly one login is
//     stored (or when the user picks from the OpenNyx dropdown),
//   * shows a small OpenNyx account-picker anchored to the focused field when
//     multiple logins match,
//   * offers to save/update a new login on form submit.
const char* const kAutofillScriptHead = R"AF(
(function(){
  var __onx_arr = )AF";

const char* const kAutofillScriptTail = R"AF(;
  // Re-injected on every load: refresh creds, install listeners only once.
  window.__onxCreds = __onx_arr;
  if(window.__onxAutofillInstalled){return;}
  window.__onxAutofillInstalled = true;
  function C_(){return window.__onxCreds||[];}
  function pwFields(){return [].slice.call(document.querySelectorAll('input[type=password]')).filter(function(e){return e.offsetParent!==null;});}
  function userFor(pw){
    // Nearest preceding text/email/tel input in the same form (or document).
    var scope=pw.form||document;var ins=[].slice.call(scope.querySelectorAll('input'));
    var idx=ins.indexOf(pw);
    for(var i=idx-1;i>=0;i--){var t=(ins[i].type||'text').toLowerCase();
      if(t==='text'||t==='email'||t==='tel'||t===''){return ins[i];}}
    // fall back: any text/email input on the page
    for(var j=0;j<ins.length;j++){var t2=(ins[j].type||'text').toLowerCase();
      if(t2==='email'||t2==='text'){return ins[j];}}
    return null;
  }
  function setVal(el,val){
    if(!el)return;
    var d=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value');
    d&&d.set?d.set.call(el,val):(el.value=val);
    el.dispatchEvent(new Event('input',{bubbles:true}));
    el.dispatchEvent(new Event('change',{bubbles:true}));
  }
  function fill(cred){
    var pws=pwFields();if(!pws.length)return;
    var pw=pws[0];var u=userFor(pw);
    if(u)setVal(u,cred.u);setVal(pw,cred.p);
  }
  function removeMenu(){var m=document.getElementById('__onx_menu');if(m)m.remove();}
  function showMenu(anchor){
    removeMenu();
    var C=C_();if(!C||!C.length)return;
    var r=anchor.getBoundingClientRect();
    var box=document.createElement('div');box.id='__onx_menu';
    box.style.cssText='position:absolute;z-index:2147483647;min-width:'+Math.max(220,r.width)+'px;'+
      'background:#1c1d24;border:1px solid #33343f;border-radius:10px;'+
      'box-shadow:0 10px 34px rgba(0,0,0,.5);font:13px system-ui,sans-serif;'+
      'color:#e8e9f0;overflow:hidden;padding:4px;';
    box.style.left=(window.scrollX+r.left)+'px';
    box.style.top=(window.scrollY+r.bottom+4)+'px';
    var hdr=document.createElement('div');
    hdr.textContent='OpenNyx \u2014 choose account';
    hdr.style.cssText='font-size:11px;color:#8b8ca3;padding:6px 8px 4px;';
    box.appendChild(hdr);
    C_().forEach(function(cred){
      var it=document.createElement('div');
      it.textContent=cred.u||'(no username)';
      it.style.cssText='padding:8px 10px;border-radius:7px;cursor:pointer;';
      it.addEventListener('mouseenter',function(){it.style.background='#2a2b36';});
      it.addEventListener('mouseleave',function(){it.style.background='';});
      it.addEventListener('mousedown',function(ev){ev.preventDefault();fill(cred);removeMenu();});
      box.appendChild(it);
    });
    document.body.appendChild(box);
  }
  function onFocus(e){
    var t=e.target;if(!t||t.tagName!=='INPUT')return;
    var ty=(t.type||'').toLowerCase();
    if(ty==='password'||ty==='email'||ty==='text'||ty===''){
      var C=C_();
      if(C&&C.length>1){showMenu(t);}
      else if(C&&C.length===1){/* single: auto-filled below */}
    }
  }
  document.addEventListener('focusin',onFocus,true);
  document.addEventListener('mousedown',function(e){
    if(e.target&&e.target.closest&&e.target.closest('#__onx_menu'))return;removeMenu();},true);
  // Auto-fill immediately if exactly one credential is stored.
  function autofillOnce(){var C=C_();if(C&&C.length===1&&pwFields().length){fill(C[0]);}}
  if(document.readyState!=='loading')setTimeout(autofillOnce,60);
  else document.addEventListener('DOMContentLoaded',function(){setTimeout(autofillOnce,60);});
  // Save-prompt: capture credentials on submit and offer to store them.
  function captureSubmit(){
    var pws=pwFields();if(!pws.length)return;var pw=pws[0];var u=userFor(pw);
    var user=u?u.value:'';var pass=pw.value;if(!pass)return;
    var known=(C_()||[]).some(function(c){return c.u===user&&c.p===pass;});
    if(known)return;
    showSavePrompt(user,pass);
  }
  document.addEventListener('submit',function(){setTimeout(captureSubmit,0);},true);
  function showSavePrompt(user,pass){
    var old=document.getElementById('__onx_save');if(old)old.remove();
    var box=document.createElement('div');box.id='__onx_save';
    box.style.cssText='position:fixed;top:14px;right:14px;z-index:2147483647;'+
      'background:#1c1d24;border:1px solid #33343f;border-radius:12px;'+
      'box-shadow:0 12px 40px rgba(0,0,0,.55);font:13px system-ui,sans-serif;'+
      'color:#e8e9f0;padding:14px 16px;max-width:300px;';
    box.innerHTML='<div style="font-weight:600;margin-bottom:4px">Save password in OpenNyx?</div>'+
      '<div style="font-size:12px;color:#8b8ca3;margin-bottom:10px;word-break:break-all">'+(user||'(no username)')+'</div>';
    var row=document.createElement('div');row.style.cssText='display:flex;gap:8px;justify-content:flex-end';
    var no=document.createElement('button');no.textContent='Not now';
    no.style.cssText='background:#2a2b36;color:#e8e9f0;border:0;border-radius:8px;padding:7px 12px;cursor:pointer;font:inherit;';
    var yes=document.createElement('button');yes.textContent='Save';
    yes.style.cssText='background:#7a5cff;color:#fff;border:0;border-radius:8px;padding:7px 12px;cursor:pointer;font:inherit;';
    no.addEventListener('click',function(){box.remove();});
    yes.addEventListener('click',function(){
      // Persist via the internal api scheme (passwords/add is the only vault
      // endpoint a real site may call, and only to add what the user typed).
      fetch('opennyx://api/passwords/add',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({origin:location.origin,username:user,password:pass})}).catch(function(){});
      box.remove();
    });
    row.appendChild(no);row.appendChild(yes);box.appendChild(row);
    document.body.appendChild(box);
    setTimeout(function(){if(box.parentNode)box.remove();},15000);
  }
})();
)AF";

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
  MaybeInjectAutofill(frame, url);
}

namespace {

// Host component of a URL (empty on failure).
std::string AutofillHostOf(const std::string& url) {
  CefURLParts parts;
  if (!CefParseURL(url, parts)) return std::string();
  return CefString(&parts.host).ToString();
}

// JSON-string-escape for safely embedding values in the injected script.
std::string JsEscape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\': o += "\\\\"; break;
      case '"': o += "\\\""; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '<': o += "\\u003c"; break;
      case '>': o += "\\u003e"; break;
      case '&': o += "\\u0026"; break;
      default: o += c;
    }
  }
  return o;
}

}  // namespace

void OpenNyxClient::MaybeInjectAutofill(CefRefPtr<CefFrame> frame,
                                        const std::string& url) {
  // Only real web pages (http/https), never our internal pages.
  if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
    return;
  }
  const std::string host = AutofillHostOf(url);
  if (host.empty()) return;

  auto creds = OpenNyxStore::Get()->GetPasswordsForHost(host);

  // Build a JS array literal of {u,p} objects for this site (may be empty; the
  // script still installs the save-prompt observer for new logins).
  std::string arr = "[";
  for (size_t i = 0; i < creds.size(); ++i) {
    if (i) arr += ",";
    arr += "{u:\"" + JsEscape(creds[i].username) + "\",p:\"" +
           JsEscape(creds[i].password) + "\"}";
  }
  arr += "]";

  const std::string js = std::string(kAutofillScriptHead) + arr +
                         kAutofillScriptTail;
  frame->ExecuteJavaScript(js, frame->GetURL(), 0);
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
  const std::string url = request->GetURL().ToString();
  // Never block our own scheme.
  if (url.compare(0, 10, "opennyx://") == 0) {
    return RV_CONTINUE;
  }
  const std::string req_host = HostOf(url);
  // Always-on Google phone-home backstop: runs even when the tracker toggle is
  // OFF. This is the network-level guarantee behind runtime de-googling.
  if (!req_host.empty() && bl->ShouldBlockGooglePhoneHome(req_host)) {
    bl->RecordBlock(HostOf(frame ? frame->GetURL().ToString() : std::string()));
    return RV_CANCEL;
  }
  if (!bl->enabled()) {
    return RV_CONTINUE;
  }
  // Extract host of the first-party document.
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

namespace {
// User-defined context-menu command IDs must live between MENU_ID_USER_FIRST
// (26500) and MENU_ID_USER_LAST.
const int kMenuInspectElement = MENU_ID_USER_FIRST + 0;
}  // namespace

void OpenNyxClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        CefRefPtr<CefContextMenuParams> params,
                                        CefRefPtr<CefMenuModel> model) {
  CEF_REQUIRE_UI_THREAD();
  // Append "Inspect element" so the dev console is reachable via right-click
  // (in addition to the Ctrl+Shift+I shortcut). DevTools are fully local and
  // Google-free -- part of the Chromium engine, no network calls.
  if (model->GetCount() > 0) {
    model->AddSeparator();
  }
  model->AddItem(kMenuInspectElement, "Inspect element");
}

bool OpenNyxClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefContextMenuParams> params,
                                         int command_id,
                                         EventFlags event_flags) {
  CEF_REQUIRE_UI_THREAD();
  if (command_id == kMenuInspectElement) {
    if (!browser) {
      return true;
    }
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host) {
      return true;
    }
    // Dedicated DevToolsClient (not our main client, whose handlers assume
    // every browser is a tab). CEF owns the DevTools window (see the popup
    // delegate for is_devtools).
    CefPoint inspect_at(params ? params->GetXCoord() : 0,
                        params ? params->GetYCoord() : 0);
    host->ShowDevTools(CefWindowInfo(), new DevToolsClient(),
                       CefBrowserSettings(), inspect_at);
    return true;
  }
  return false;
}

void OpenNyxClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Track the new browser (each tab / popup window is one CefBrowser).
  browser_list_.push_back(browser);
}

bool OpenNyxClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Tab browsers are closed by DETACHING their CefBrowserView from the shared
  // window and releasing the last reference (see BrowserWindow::CloseTabAt).
  // On that path CEF never calls DoClose (window_destroyed_ is already true).
  //
  // DoClose *is* reached when the renderer itself initiates a close, e.g. JS
  // window.close(). Returning false here would make CEF close the browser's
  // host window -- which for a tab is the SHARED top-level window (all tabs
  // would die). Instead take the documented "non-standard close" path: return
  // true and complete the close ourselves by closing just that tab.
  if (BrowserWindow* window = BrowserWindow::Get()) {
    if (window->HasTabForBrowser(browser)) {
      // Post (not inline): DoClose runs inside CloseContents; mutating the
      // view hierarchy and destroying the browser re-entrantly is unsafe.
      CefPostTask(TID_UI,
                  base::BindOnce(
                      +[](CefRefPtr<CefBrowser> b) {
                        if (BrowserWindow* w = BrowserWindow::Get()) {
                          w->CloseTabForBrowser(b);
                        }
                      },
                      browser));
      return true;
    }
  }

  // Popup / DevTools windows: one browser per window, standard close flow.
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
