// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefclient sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "browser_window.h"

#if defined(_WIN32)
// Only for GetCursorPos()/GetKeyState() in the tab drag-reorder poll. The CEF
// Views API (CEF 150) has no mouse-move/drag delegate callbacks, so a true
// pointer drag needs the OS cursor. NOMINMAX/WIN32_LEAN_AND_MEAN keep the
// macro fallout minimal.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "include/cef_color_ids.h"
#include "include/cef_cookie.h"
#include "include/views/cef_display.h"
#include "include/cef_image.h"
#include "app_icon_png.h"
#include "include/cef_parser.h"
#include "include/cef_request_context.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"
#include "include/cef_task.h"


#include "blocklist.h"
#include "opennyx_client.h"
#include "store.h"

namespace {

BrowserWindow* g_browser_window = nullptr;

// TEMPORARY tracer for the DevTools crash hunt (mirrors the one in
// opennyx_client.cc). Remove once fixed.
void DevLog(const std::string& msg) {
  FILE* f = std::fopen("opennyx-devtools.log", "a");
  if (f) {
    std::fprintf(f, "%s\n", msg.c_str());
    std::fclose(f);
  }
}


// ---- View ids ----
enum ViewID {
  ID_WINDOW = 1,
  ID_TAB_STRIP,
  ID_TOOLBAR,
  ID_BACK_BUTTON,
  ID_FORWARD_BUTTON,
  ID_RELOAD_BUTTON,
  ID_HOME_BUTTON,
  ID_NEW_TAB_BUTTON,
  ID_ADDRESS_BAR,
  ID_STAR_BUTTON,
  ID_SHIELD_BUTTON,
  ID_MENU_BUTTON,
  // Frameless window controls (drawn by OpenNyx in the tab strip).
  ID_CAPTION_SPACER,
  ID_MINIMIZE_BUTTON,
  ID_MAXIMIZE_BUTTON,
  ID_CLOSE_WINDOW_BUTTON,
  // Tab views use ID_TAB_FIRST + tab_id * 2 (+1 for the close button).
  ID_TAB_FIRST = 1000,
};

// ---- Accelerator command ids ----
enum CommandID {
  CMD_NEW_TAB = 1,
  CMD_CLOSE_TAB,
  CMD_NEXT_TAB,
  CMD_PREV_TAB,
  CMD_FOCUS_ADDRESS,
  CMD_RELOAD,
  CMD_RELOAD_F5,
  CMD_RELOAD_IGNORE_CACHE,
  CMD_BACK,
  CMD_FORWARD,
  CMD_DEVTOOLS,
  CMD_HISTORY,
  CMD_BOOKMARK,
  CMD_DOWNLOADS,
  CMD_SETTINGS,
  CMD_MOVE_TAB_LEFT,
  CMD_MOVE_TAB_RIGHT,
};

// ---- Windows virtual key codes (avoid pulling in windows.h here) ----
constexpr int kVK_RETURN = 0x0D;
constexpr int kVK_ESCAPE = 0x1B;
constexpr int kVK_TAB = 0x09;
constexpr int kVK_F5 = 0x74;
constexpr int kVK_LEFT = 0x25;
constexpr int kVK_RIGHT = 0x27;
constexpr int kVK_PRIOR = 0x21;  // Page Up.
constexpr int kVK_NEXT = 0x22;   // Page Down.
constexpr int kVK_OEM_COMMA = 0xBC;  // ',' key.

// Drag-to-reorder tuning.
// Horizontal cursor travel (DIP) before a press turns into a drag. Keeps
// plain clicks from jiggling the strip.
constexpr int kDragStartThresholdDip = 8;
// Cursor poll interval while a tab is pressed (~60 fps).
constexpr int kDragPollIntervalMs = 16;

// ---- Dark theme palette ----
// A cohesive, slightly blue-tinted charcoal theme with a purple accent that
// matches the OpenNyx wordmark on the new-tab page.
constexpr cef_color_t kColorWindowBg = CefColorSetARGB(255, 24, 25, 30);
constexpr cef_color_t kColorToolbarBg = CefColorSetARGB(255, 34, 35, 43);
constexpr cef_color_t kColorTabStripBg = CefColorSetARGB(255, 22, 23, 28);
constexpr cef_color_t kColorTabActive = CefColorSetARGB(255, 52, 54, 66);
constexpr cef_color_t kColorTabInactive = CefColorSetARGB(255, 30, 31, 38);
constexpr cef_color_t kColorText = CefColorSetARGB(255, 232, 233, 240);
constexpr cef_color_t kColorTextDim = CefColorSetARGB(255, 150, 152, 162);
constexpr cef_color_t kColorAccent = CefColorSetARGB(255, 122, 92, 255);
constexpr cef_color_t kColorButtonDisabled = CefColorSetARGB(255, 88, 90, 100);
constexpr cef_color_t kColorCloseHover = CefColorSetARGB(255, 232, 76, 76);

// Address-bar (textfield) colors. Readable light text on a lighter-than-window
// input background — fixes the "black on black" bug. These are applied via
// CefWindow::SetThemeColor() using the standard CEF_ColorTextfield* IDs, which
// is the supported path in CEF 150 (the old per-textfield SetTextColor/
// SetBackgroundColor setters are compiled out at the default API version).
constexpr cef_color_t kColorFieldBg = CefColorSetARGB(255, 42, 42, 46);
constexpr cef_color_t kColorFieldText = CefColorSetARGB(255, 240, 240, 240);
constexpr cef_color_t kColorFieldPlaceholder = CefColorSetARGB(255, 148, 150, 160);
constexpr cef_color_t kColorFieldSelectionBg = CefColorSetARGB(255, 122, 92, 255);
constexpr cef_color_t kColorFieldSelectionText = CefColorSetARGB(255, 255, 255, 255);
constexpr cef_color_t kColorFieldOutline = CefColorSetARGB(255, 60, 62, 74);
constexpr cef_color_t kColorFieldOutlineFocused = CefColorSetARGB(255, 122, 92, 255);

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 800;
constexpr int kMinWidth = 480;
constexpr int kMinHeight = 320;

constexpr size_t kMaxTabTitleChars = 24;

const char kFontList[] = "Segoe UI, 13px";
const char kTabFontList[] = "Segoe UI, 12px";

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

std::string TruncateTitle(const std::string& title) {
  if (title.size() <= kMaxTabTitleChars) {
    return title.empty() ? "New Tab" : title;
  }
  return title.substr(0, kMaxTabTitleChars - 1) + "…";
}

// Delegate for popup windows (window.open, DevTools). Simply hosts the
// popup browser view in its own top-level window.
class PopupWindowDelegate : public CefWindowDelegate {
 public:
  explicit PopupWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  PopupWindowDelegate(const PopupWindowDelegate&) = delete;
  PopupWindowDelegate& operator=(const PopupWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    DevLog("D. PopupWindowDelegate::OnWindowCreated");
    window->SetTitle("OpenNyx");
    DevLog("E. adding child view, view_null=" +
           std::string(browser_view_ ? "no" : "yes"));
    window->AddChildView(browser_view_);
    DevLog("F. showing window");
    window->Show();
    DevLog("G. requesting focus");
    browser_view_->RequestFocus();
    DevLog("H. popup window fully created");
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(900, 700);
  }

  // Popups / DevTools also use Alloy style so they match the main window and
  // never fall back to the Chrome runtime UI.
  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
};

// Minimal BrowserView delegate for popup / DevTools browser views. The default
// GetDelegateForPopupBrowserView() returns |this| (the main BrowserWindow),
// which would then be asked to host the DevTools view with all the main
// window's tab/toolbar/draggable-region logic -- that crashes. DevTools (and
// window.open popups) must use their OWN lightweight delegate that only forces
// the Alloy runtime style and draws no Chrome toolbar.
class PopupBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  PopupBrowserViewDelegate() = default;
  PopupBrowserViewDelegate(const PopupBrowserViewDelegate&) = delete;
  PopupBrowserViewDelegate& operator=(const PopupBrowserViewDelegate&) = delete;

  ChromeToolbarType GetChromeToolbarType(
      CefRefPtr<CefBrowserView> browser_view) override {
    return CEF_CTT_NONE;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

  // Nested popups (e.g. a popup opened from DevTools) keep using a fresh
  // lightweight delegate, never the main window.
  CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(
      CefRefPtr<CefBrowserView> browser_view,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      bool is_devtools) override {
    return new PopupBrowserViewDelegate();
  }

  bool OnPopupBrowserViewCreated(
      CefRefPtr<CefBrowserView> browser_view,
      CefRefPtr<CefBrowserView> popup_browser_view,
      bool is_devtools) override {
    CefWindow::CreateTopLevelWindow(
        new PopupWindowDelegate(popup_browser_view));
    return true;
  }

  IMPLEMENT_REFCOUNTING(PopupBrowserViewDelegate);
};

// Decodes the embedded OpenNyx PNG into a CefImage once and caches it.
CefRefPtr<CefImage> GetAppIconImage() {
  static CefRefPtr<CefImage> image;
  if (!image) {
    image = CefImage::CreateImage();
    image->AddPNG(1.0f, kOpenNyxIconPng, kOpenNyxIconPngSize);
  }
  return image;
}

}  // namespace

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

// True if |spec| is a URL we should NOT show in the address bar (the new-tab
// page, blank/initial states). Kept in one place so every code path agrees.
bool ShouldHideUrlInAddressBar(const std::string& spec) {
  if (spec.empty()) {
    return true;
  }
  if (spec == "about:blank" || spec == "about:newtab") {
    return true;
  }
  if (spec.compare(0, 5, "data:") == 0) {
    return true;
  }
  if (spec == "opennyx://newtab" || spec == "opennyx://newtab/") {
    return true;
  }
  return false;
}

std::string GetNewTabURL() {
  // The new-tab / homepage is now served by the privileged opennyx:// scheme
  // (see scheme_handler.cc / pages.cc), which allows rich, interactive pages
  // that talk to the browser via the opennyx://api bridge. The homepage is
  // user-configurable in Settings.
  const AppConfig cfg = OpenNyxStore::Get()->GetConfig();
  if (!cfg.homepage.empty()) {
    return cfg.homepage;
  }
  return "opennyx://newtab";
}

// Legacy self-contained data: new-tab page, retained as a fallback only.
std::string GetLegacyNewTabURL() {
  static std::string url;
  if (url.empty()) {
    const char kHtml[] =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>New Tab</title><style>"
        "*{box-sizing:border-box}"
        "html,body{height:100%;margin:0}"
        "body{background:radial-gradient(1200px 700px at 50% -10%,"
        "#20222c 0%,#17181e 55%,#141419 100%);color:#e8e9f0;display:flex;"
        "align-items:center;justify-content:center;"
        "font-family:'Segoe UI',system-ui,sans-serif;-webkit-font-smoothing:antialiased}"
        ".wrap{text-align:center;width:min(640px,90vw);transform:translateY(-6vh)}"
        "h1{font-size:52px;font-weight:650;letter-spacing:.5px;margin:0 0 8px}"
        "h1 .nyx{color:#7a5cff}"
        ".tag{margin:0 0 30px;font-size:14px;color:#8a8d99;letter-spacing:.3px}"
        "form{display:flex;gap:0;box-shadow:0 10px 40px rgba(0,0,0,.5);"
        "border-radius:14px;overflow:hidden;border:1px solid #34364a;"
        "transition:border-color .15s,box-shadow .15s}"
        "form:focus-within{border-color:#7a5cff;"
        "box-shadow:0 10px 44px rgba(122,92,255,.28)}"
        "input[type=search]{flex:1;padding:16px 20px;font-size:16px;border:0;"
        "outline:none;background:#1c1d25;color:#f0f0f0}"
        "input[type=search]::placeholder{color:#8a8d99}"
        "button{padding:16px 26px;border:0;background:#7a5cff;color:#fff;"
        "font-size:15px;font-weight:600;cursor:pointer;transition:background .15s}"
        "button:hover{background:#8d73ff}"
        ".tiles{display:flex;gap:12px;justify-content:center;margin-top:26px;"
        "flex-wrap:wrap}"
        ".tile{display:flex;flex-direction:column;align-items:center;gap:7px;"
        "width:84px;padding:14px 8px;border-radius:12px;text-decoration:none;"
        "color:#c9cbd6;background:#1c1d25;border:1px solid #2a2c38;"
        "font-size:12.5px;transition:background .15s,border-color .15s,transform .1s}"
        ".tile:hover{background:#24262f;border-color:#3a3d4e;transform:translateY(-2px)}"
        ".tile .ic{font-size:22px}"
        ".hint{margin-top:30px;font-size:12.5px;color:#5c5f6a}"
        "</style></head><body><div class=\"wrap\">"
        "<h1>Open<span class=\"nyx\">Nyx</span></h1>"
        "<p class=\"tag\">Private by default</p>"
        "<form action=\"https://search.brave.com/search\" method=\"get\">"
        "<input type=\"search\" name=\"q\" placeholder=\"Search with Brave "
        "or enter address\" autofocus autocomplete=\"off\">"
        "<button type=\"submit\">Search</button></form>"
        "<div class=\"tiles\">"
        "<a class=\"tile\" href=\"https://search.brave.com/\">"
        "<span class=\"ic\">🔍</span>Brave</a>"
        "<a class=\"tile\" href=\"https://en.wikipedia.org/\">"
        "<span class=\"ic\">📚</span>Wikipedia</a>"
        "<a class=\"tile\" href=\"https://github.com/\">"
        "<span class=\"ic\">🐙</span>GitHub</a>"
        "<a class=\"tile\" href=\"https://news.ycombinator.com/\">"
        "<span class=\"ic\">📰</span>HN</a>"
        "<a class=\"tile\" href=\"https://www.openstreetmap.org/\">"
        "<span class=\"ic\">🗺️</span>Maps</a>"
        "</div>"
        "<div class=\"hint\">powered by Brave Search · no Google services</div>"
        "</div></body></html>";
    url = GetDataURI(kHtml, "text/html");
  }
  return url;
}

std::string ResolveAddressInput(const std::string& raw) {
  // Trim whitespace.
  std::string input = raw;
  const char* ws = " \t\r\n";
  const size_t begin = input.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return std::string();
  }
  input = input.substr(begin, input.find_last_not_of(ws) - begin + 1);

  // Explicit scheme -> use as-is.
  static const char* kSchemes[] = {"http://", "https://", "file://",
                                   "data:",   "about:",   "chrome://",
                                   "view-source:"};
  std::string lower = input;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(::tolower(c)); });
  for (const char* scheme : kSchemes) {
    if (lower.compare(0, strlen(scheme), scheme) == 0) {
      return input;
    }
  }

  // Looks like a host if there is no whitespace AND it contains a dot or a
  // colon-port, or equals "localhost".
  const bool has_space = input.find(' ') != std::string::npos;
  const bool has_dot = input.find('.') != std::string::npos;
  const bool is_localhost = lower.compare(0, 9, "localhost") == 0;
  if (!has_space && (has_dot || is_localhost)) {
    return "https://" + input;
  }

  // Everything else -> the user's configured search engine (Brave by default).
  return OpenNyxStore::Get()->BuildSearchURL(input);
}

// ---------------------------------------------------------------------------
// BrowserWindow
// ---------------------------------------------------------------------------

// static
void BrowserWindow::Create(const std::string& url) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(!g_browser_window);
  CefRefPtr<BrowserWindow> window = new BrowserWindow();
  g_browser_window = window.get();
  window->pending_initial_url_ = url.empty() ? GetNewTabURL() : url;
  CefWindow::CreateTopLevelWindow(window);
}

// static
BrowserWindow* BrowserWindow::Get() {
  return g_browser_window;
}

// ---- CefWindowDelegate ----

void BrowserWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  window_ = window;
  window_->SetID(ID_WINDOW);
  window_->SetTitle("OpenNyx");
  window_->SetBackgroundColor(kColorWindowBg);

  // Set the title-bar + taskbar icon. CEF Views ignores the .rc icon here and
  // wants a CefImage set explicitly on the window (there is no GetWindowIcon
  // delegate method on CefWindowDelegate).
  if (CefRefPtr<CefImage> icon = GetAppIconImage()) {
    window_->SetWindowIcon(icon);
    window_->SetWindowAppIcon(icon);
  }

  // Override theme colors (address-bar text/background etc.) before building
  // the UI so children pick them up on first layout.
  ApplyTheme();

  BuildUI();
  AddAccelerators();

  // Push the theme colors into the freshly-built view hierarchy.
  window_->ThemeChanged();

  // First tab.
  CreateTab(pending_initial_url_, /*select=*/true);
  pending_initial_url_.clear();

  window_->Show();

  // Now that the views are laid out, mark the caption area draggable so the
  // frameless window can be moved.
  UpdateDraggableRegions();

  if (CefRefPtr<CefBrowserView> view = ActiveBrowserView()) {
    view->RequestFocus();
  }
}

void BrowserWindow::OnWindowBoundsChanged(CefRefPtr<CefWindow> window,
                                          const CefRect& new_bounds) {
  CEF_REQUIRE_UI_THREAD();
  // The caption spacer moves/resizes with the window; refresh the draggable
  // region and the maximize-button glyph.
  UpdateDraggableRegions();
  if (maximize_button_ && window_) {
    const bool maxed = window_->IsMaximized();
    if (maxed != is_maximized_) {
      is_maximized_ = maxed;
      // Restore glyph (U+2750) vs maximize glyph (U+25A1).
      maximize_button_->SetText(maxed ? "\xE2\x9D\x90" : "\xE2\x96\xA1");
      maximize_button_->SetTooltipText(maxed ? "Restore" : "Maximize");
    }
  }
}

void BrowserWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  window_ = nullptr;
  tab_strip_ = nullptr;
  toolbar_ = nullptr;
  back_button_ = nullptr;
  forward_button_ = nullptr;
  reload_button_ = nullptr;
  home_button_ = nullptr;
  new_tab_button_ = nullptr;
  address_bar_ = nullptr;
  star_button_ = nullptr;
  shield_button_ = nullptr;
  menu_button_ = nullptr;
  caption_spacer_ = nullptr;
  minimize_button_ = nullptr;
  maximize_button_ = nullptr;
  close_window_button_ = nullptr;
  tabs_.clear();
  g_browser_window = nullptr;
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  // Reached when the WHOLE window is closing: the OS title-bar ×, or
  // MaybeCloseWindow() after the last tab was closed. A single-tab × NEVER
  // routes here (CloseTabAt detaches the view instead of calling
  // CloseBrowser, so CEF never asks the shared window to close).
  //
  // Detach every remaining tab and release our references. Each release
  // triggers ~CefBrowserViewImpl -> WindowDestroyed() -> forced destruction
  // of that browser only (see CloseTabAt for details). OnBeforeClose fires
  // per browser; OpenNyxClient quits the app once browser_list_ is empty.
  // Then let the window close now -- the views were already detached, so the
  // window teardown no longer touches any live browser.
  while (!tabs_.empty()) {
    RemoveTabAt(tabs_.size() - 1);
  }
  return true;
}

CefSize BrowserWindow::GetPreferredSize(CefRefPtr<CefView> view) {
  if (view->GetID() == ID_WINDOW) {
    return CefSize(kDefaultWidth, kDefaultHeight);
  }
  return CefSize();
}

CefSize BrowserWindow::GetMinimumSize(CefRefPtr<CefView> view) {
  if (view->GetID() == ID_WINDOW) {
    return CefSize(kMinWidth, kMinHeight);
  }
  return CefSize();
}

bool BrowserWindow::OnAccelerator(CefRefPtr<CefWindow> window, int command_id) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();

  switch (command_id) {
    case CMD_NEW_TAB:
      CreateTab(GetNewTabURL(), /*select=*/true);
      return true;
    case CMD_CLOSE_TAB:
      CloseActiveTab();
      return true;
    case CMD_NEXT_TAB:
      SelectNextTab(/*forward=*/true);
      return true;
    case CMD_PREV_TAB:
      SelectNextTab(/*forward=*/false);
      return true;
    case CMD_FOCUS_ADDRESS:
      FocusAddressBar();
      return true;
    case CMD_RELOAD:
    case CMD_RELOAD_F5:
      if (browser) {
        browser->Reload();
      }
      return true;
    case CMD_RELOAD_IGNORE_CACHE:
      if (browser) {
        browser->ReloadIgnoreCache();
      }
      return true;
    case CMD_BACK:
      if (browser && browser->CanGoBack()) {
        browser->GoBack();
      }
      return true;
    case CMD_FORWARD:
      if (browser && browser->CanGoForward()) {
        browser->GoForward();
      }
      return true;
    case CMD_DEVTOOLS:
      if (browser) {
        browser->GetHost()->ShowDevTools(CefWindowInfo(), nullptr,
                                         CefBrowserSettings(), CefPoint());
      }
      return true;
    case CMD_HISTORY:
      CreateTab("opennyx://history", /*select=*/true);
      return true;
    case CMD_BOOKMARK:
      ToggleBookmarkActiveTab();
      return true;
    case CMD_DOWNLOADS:
      CreateTab("opennyx://downloads", /*select=*/true);
      return true;
    case CMD_SETTINGS:
      CreateTab("opennyx://settings", /*select=*/true);
      return true;
    case CMD_MOVE_TAB_LEFT:
      if (active_tab_ > 0) {
        MoveTab(active_tab_, active_tab_ - 1);
      }
      return true;
    case CMD_MOVE_TAB_RIGHT:
      if (active_tab_ + 1 < tabs_.size()) {
        MoveTab(active_tab_, active_tab_ + 1);
      }
      return true;
    default:
      return false;
  }
}

// ---- CefBrowserViewDelegate ----

CefBrowserViewDelegate::ChromeToolbarType BrowserWindow::GetChromeToolbarType(
    CefRefPtr<CefBrowserView> browser_view) {
  // OpenNyx draws its own toolbar; no Chrome toolbar inside the browser view.
  return CEF_CTT_NONE;
}

CefRefPtr<CefBrowserViewDelegate>
BrowserWindow::GetDelegateForPopupBrowserView(
    CefRefPtr<CefBrowserView> browser_view,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    bool is_devtools) {
  DevLog(std::string("A. GetDelegateForPopupBrowserView is_devtools=") +
         (is_devtools ? "yes" : "no"));
  // Give popups / DevTools their own lightweight delegate instead of this main
  // window. Returning |this| (the default) would drive the DevTools view
  // through the tab-strip/toolbar logic and crash.
  return new PopupBrowserViewDelegate();
}

bool BrowserWindow::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  DevLog(std::string("B. OnPopupBrowserViewCreated is_devtools=") +
         (is_devtools ? "yes" : "no") +
         " popup_view_null=" + (popup_browser_view ? "no" : "yes"));
  // Host popups (window.open) and DevTools in their own top-level window.
  CefWindow::CreateTopLevelWindow(
      new PopupWindowDelegate(popup_browser_view));
  DevLog("C. CreateTopLevelWindow returned");
  return true;
}

// ---- CefTextfieldDelegate ----

bool BrowserWindow::OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                               const CefKeyEvent& event) {
  CEF_REQUIRE_UI_THREAD();
  if (textfield->GetID() != ID_ADDRESS_BAR ||
      event.type != KEYEVENT_RAWKEYDOWN) {
    return false;
  }

  if (event.windows_key_code == kVK_RETURN) {
    NavigateActiveTab(textfield->GetText().ToString());
    return true;
  }
  if (event.windows_key_code == kVK_ESCAPE) {
    // Restore the current URL and give focus back to the page.
    if (CefRefPtr<CefBrowser> browser = ActiveBrowser()) {
      textfield->SetText(browser->GetMainFrame()->GetURL());
    }
    if (CefRefPtr<CefBrowserView> view = ActiveBrowserView()) {
      view->RequestFocus();
    }
    return true;
  }
  return false;
}

// ---- CefButtonDelegate ----

void BrowserWindow::OnButtonPressed(CefRefPtr<CefButton> button) {
  CEF_REQUIRE_UI_THREAD();
  const int id = button->GetID();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();

  switch (id) {
    case ID_BACK_BUTTON:
      if (browser) {
        browser->GoBack();
      }
      return;
    case ID_FORWARD_BUTTON:
      if (browser) {
        browser->GoForward();
      }
      return;
    case ID_RELOAD_BUTTON:
      if (browser) {
        browser->Reload();
      }
      return;
    case ID_HOME_BUTTON:
      if (browser) {
        browser->GetMainFrame()->LoadURL(GetNewTabURL());
        FocusAddressBar();
      }
      return;
    case ID_NEW_TAB_BUTTON:
      CreateTab(GetNewTabURL(), /*select=*/true);
      return;
    case ID_STAR_BUTTON:
      ToggleBookmarkActiveTab();
      return;
    case ID_SHIELD_BUTTON:
      // Show settings (privacy dashboard) when the shield is clicked.
      CreateTab("opennyx://settings", /*select=*/true);
      return;
    case ID_MENU_BUTTON:
      CreateTab("opennyx://settings", /*select=*/true);
      return;
    case ID_MINIMIZE_BUTTON:
      if (window_) {
        window_->Minimize();
      }
      return;
    case ID_MAXIMIZE_BUTTON:
      if (window_) {
        if (window_->IsMaximized()) {
          window_->Restore();
        } else {
          window_->Maximize();
        }
      }
      return;
    case ID_CLOSE_WINDOW_BUTTON:
      if (window_) {
        window_->Close();
      }
      return;
    default:
      break;
  }

  // Tab buttons: ID_TAB_FIRST + tab_id * 2 (title) / + 1 (close).
  if (id >= ID_TAB_FIRST) {
    const int tab_id = (id - ID_TAB_FIRST) / 2;
    const bool is_close = ((id - ID_TAB_FIRST) % 2) == 1;
    for (size_t i = 0; i < tabs_.size(); ++i) {
      if (tabs_[i].id == tab_id) {
        if (is_close) {
          CloseTabAt(i);
        } else {
          SelectTab(i);
        }
        return;
      }
    }
  }
}

void BrowserWindow::OnButtonStateChanged(CefRefPtr<CefButton> button) {
  CEF_REQUIRE_UI_THREAD();
  // Drag-to-reorder entry point. The CEF 150 Views API has no mouse-move /
  // drag delegate callbacks, so we detect the press here (a tab title button
  // entering the PRESSED state = mouse-down on the tab) and then track the OS
  // cursor with a short repeating UI-thread poll (DragPoll) until the mouse
  // button is released. While the press is held and the cursor travels
  // horizontally past a small threshold, the tab is live-reordered to follow
  // the cursor -- the same feel as Chrome's tab drag, minus the floating
  // pixel-perfect drag image.
  const int id = button->GetID();
  if (id < ID_TAB_FIRST || ((id - ID_TAB_FIRST) % 2) != 0) {
    return;  // Not a tab title button.
  }
  if (button->GetState() != CEF_BUTTON_STATE_PRESSED) {
    return;  // Only mouse-down starts a potential drag; release is handled
             // by the poll noticing the button is up.
  }
  const int tab_id = (id - ID_TAB_FIRST) / 2;
  if (drag_tab_id_ == tab_id) {
    return;  // Already tracking this press.
  }
#if defined(_WIN32)
  POINT pt;
  if (!GetCursorPos(&pt)) {
    return;
  }
  const CefPoint dip = CefDisplay::ConvertScreenPointFromPixels(
      CefPoint(pt.x, pt.y));
  drag_tab_id_ = tab_id;
  drag_started_ = false;
  drag_press_x_ = dip.x;
  const int seq = ++drag_seq_;
  CefPostDelayedTask(TID_UI,
                     base::BindOnce(&BrowserWindow::DragPoll, this, seq),
                     kDragPollIntervalMs);
#endif
}

void BrowserWindow::DragPoll(int seq) {
  CEF_REQUIRE_UI_THREAD();
#if defined(_WIN32)
  if (seq != drag_seq_ || drag_tab_id_ < 0 || !window_) {
    return;  // Stale poll from a previous press.
  }

  // Logical primary mouse button released -> the press (click or drag) is
  // over. GetKeyState reflects the swapped-buttons setting, matching the
  // button press that started this poll.
  if ((GetKeyState(VK_LBUTTON) & 0x8000) == 0) {
    EndTabDrag();
    return;
  }

  // Resolve the dragged tab's CURRENT index (it moves as we reorder, and
  // other tabs can disappear underneath us, e.g. via JS window.close()).
  size_t from = tabs_.size();
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == drag_tab_id_) {
      from = i;
      break;
    }
  }
  if (from >= tabs_.size()) {
    EndTabDrag();  // Tab vanished mid-press.
    return;
  }

  POINT pt;
  if (GetCursorPos(&pt)) {
    const CefPoint dip = CefDisplay::ConvertScreenPointFromPixels(
        CefPoint(pt.x, pt.y));

    if (!drag_started_ &&
        std::abs(dip.x - drag_press_x_) >= kDragStartThresholdDip) {
      drag_started_ = true;
      // Like mainstream browsers, the dragged tab becomes the active tab.
      if (from != active_tab_) {
        SelectTab(from);
      }
    }

    if (drag_started_) {
      // Find where the cursor is: the tab whose horizontal midpoint the
      // cursor has crossed becomes the drop position. Bounds are in DIP
      // screen coordinates, same space as the converted cursor position.
      size_t target = from;
      for (size_t i = 0; i < tabs_.size(); ++i) {
        if (i == from || !tabs_[i].tab_panel) {
          continue;
        }
        const CefRect b = tabs_[i].tab_panel->GetBoundsInScreen();
        if (b.width <= 0) {
          continue;
        }
        const int mid = b.x + b.width / 2;
        if (i < from && dip.x < mid) {
          target = i;
          break;  // Leftmost crossed midpoint wins when moving left.
        }
        if (i > from && dip.x > mid) {
          target = i;  // Keep scanning: rightmost crossed midpoint wins.
        }
      }
      if (target != from) {
        MoveTab(from, target);
      }
    }
  }

  CefPostDelayedTask(TID_UI,
                     base::BindOnce(&BrowserWindow::DragPoll, this, seq),
                     kDragPollIntervalMs);
#endif
}

void BrowserWindow::EndTabDrag() {
  drag_tab_id_ = -1;
  drag_started_ = false;
  ++drag_seq_;  // Invalidate any queued DragPoll tasks.
}

void BrowserWindow::MoveTab(size_t from, size_t to) {
  CEF_REQUIRE_UI_THREAD();
  if (from >= tabs_.size() || to >= tabs_.size() || from == to) {
    return;
  }

  // Keep the tab vector and the strip's child order in lock-step. Tab-vector
  // index k corresponds to strip child index k (tabs occupy the first
  // GetChildViewCount() slots before the + button / spacer / window
  // controls), and both ReorderChildView and erase+insert place the moved
  // element at final index |to|.
  Tab tab = tabs_[from];
  tabs_.erase(tabs_.begin() + from);
  tabs_.insert(tabs_.begin() + to, tab);
  if (tab_strip_ && tab.tab_panel) {
    tab_strip_->ReorderChildView(tab.tab_panel, static_cast<int>(to));
    tab_strip_->InvalidateLayout();
  }

  // Track the active tab across the move.
  if (active_tab_ == from) {
    active_tab_ = to;
  } else if (from < active_tab_ && to >= active_tab_) {
    --active_tab_;
  } else if (from > active_tab_ && to <= active_tab_) {
    ++active_tab_;
  }

  if (window_) {
    window_->InvalidateLayout();
  }
}

// ---- Client notifications ----

void BrowserWindow::OnBrowserTitleChange(CefRefPtr<CefBrowser> browser,
                                         const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0) {
    return;
  }
  tabs_[index].title = title.ToString();
  tabs_[index].tab_button->SetText(TruncateTitle(tabs_[index].title));
  if (static_cast<size_t>(index) == active_tab_) {
    UpdateWindowTitle();
  }
}

void BrowserWindow::OnBrowserAddressChange(CefRefPtr<CefBrowser> browser,
                                           const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0 || static_cast<size_t>(index) != active_tab_) {
    return;
  }
  // Don't clobber the user's typing.
  if (address_bar_ && !address_bar_->HasFocus()) {
    const std::string spec = url.ToString();
    SetAddressBarText(ShouldHideUrlInAddressBar(spec) ? "" : spec);
  }
  UpdateChrome();
}

void BrowserWindow::OnBrowserLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                                bool is_loading,
                                                bool can_go_back,
                                                bool can_go_forward) {
  CEF_REQUIRE_UI_THREAD();
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0 || static_cast<size_t>(index) != active_tab_) {
    return;
  }
  if (back_button_) {
    back_button_->SetEnabled(can_go_back);
  }
  if (forward_button_) {
    forward_button_->SetEnabled(can_go_forward);
  }
  if (reload_button_) {
    reload_button_->SetText(is_loading ? "✕" : "⟳");
  }
}

bool BrowserWindow::OnBrowserClosed(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  // Runs from CefLifeSpanHandler::OnBeforeClose. With the detach-based close
  // flow the tab was already removed (RemoveTabAt) BEFORE its browser was
  // destroyed, so this is normally a no-op. It only acts as a safety net for
  // a browser that somehow died while its tab was still in the strip (e.g.
  // renderer-initiated teardown we did not see) -- in that case remove the
  // stale tab and, if it was the last one, close the window.
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0) {
    return false;
  }
  RemoveTabAt(static_cast<size_t>(index));
  if (tabs_.empty()) {
    MaybeCloseWindow();
  }
  return true;
}

bool BrowserWindow::HasTabForBrowser(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  return FindTabIndex(CefBrowserView::GetForBrowser(browser)) >= 0;
}

void BrowserWindow::CloseTabForBrowser(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index >= 0) {
    CloseTabAt(static_cast<size_t>(index));
  }
}

// ---- Tab management ----

void BrowserWindow::CreateTab(const std::string& url, bool select) {
  CEF_REQUIRE_UI_THREAD();
  if (!window_) {
    return;
  }

  CefBrowserSettings browser_settings;
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      OpenNyxClient::GetInstance(), url.empty() ? GetNewTabURL() : url,
      browser_settings, nullptr, nullptr, this);

  Tab tab;
  tab.browser_view = browser_view;
  tab.id = next_tab_id_++;
  tab.title = "New Tab";

  // Tab title button.
  tab.tab_button = CefLabelButton::CreateLabelButton(this, "New Tab");
  tab.tab_button->SetID(ID_TAB_FIRST + tab.id * 2);
  tab.tab_button->SetFontList(kTabFontList);
  tab.tab_button->SetInkDropEnabled(true);
  tab.tab_button->SetFocusable(false);
  tab.tab_button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_LEFT);
  tab.tab_button->SetMinimumSize(CefSize(130, 32));
  tab.tab_button->SetMaximumSize(CefSize(220, 32));

  // Tab close button.
  tab.close_button = CefLabelButton::CreateLabelButton(this, "×");
  tab.close_button->SetID(ID_TAB_FIRST + tab.id * 2 + 1);
  tab.close_button->SetFontList("Segoe UI, 14px");
  tab.close_button->SetInkDropEnabled(true);
  tab.close_button->SetFocusable(false);
  tab.close_button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_CENTER);
  tab.close_button->SetMinimumSize(CefSize(26, 32));
  tab.close_button->SetMaximumSize(CefSize(26, 32));
  tab.close_button->SetTooltipText("Close tab (Ctrl+W)");
  tab.close_button->SetTextColor(CEF_BUTTON_STATE_HOVERED, kColorText);

  // Panel grouping title + close button.
  tab.tab_panel = CefPanel::CreatePanel(nullptr);
  CefBoxLayoutSettings panel_layout;
  panel_layout.horizontal = true;
  panel_layout.inside_border_horizontal_spacing = 6;
  panel_layout.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
  tab.tab_panel->SetToBoxLayout(panel_layout);
  tab.tab_panel->AddChildView(tab.tab_button);
  tab.tab_panel->AddChildView(tab.close_button);

  // Insert the new tab directly to the RIGHT of the currently-active tab
  // (like every mainstream browser), not at the far end. The tab strip's
  // child order is [tab_0][tab_1]...[+ button][spacer][min][max][close], so a
  // tab at tab-index k sits at strip-child-index k -- the vector index and the
  // strip child index stay in lock-step as long as we insert into both at the
  // same position.
  size_t insert_index;
  if (select && !tabs_.empty()) {
    insert_index = active_tab_ + 1;
    if (insert_index > tabs_.size()) {
      insert_index = tabs_.size();
    }
  } else {
    insert_index = tabs_.size();
  }
  tabs_.insert(tabs_.begin() + insert_index, tab);
  if (tab_strip_) {
    tab_strip_->AddChildViewAt(tab.tab_panel, static_cast<int>(insert_index));
  }

  // Browser views stack in the content area; only the active one is visible.
  window_->AddChildView(browser_view);
  CefRefPtr<CefBoxLayout> layout =
      window_->GetLayout()->AsBoxLayout();
  if (layout) {
    layout->SetFlexForView(browser_view, 1);
  }

  if (select) {
    SelectTab(insert_index);
    // A brand-new tab always opens the new-tab page. Clear the address bar
    // immediately and unconditionally: at this point the new browser may not
    // be attached yet (GetBrowser() null) or may briefly report about:blank,
    // either of which would otherwise leave the PREVIOUS tab's URL visible.
    // OnBrowserAddressChange will fill in a real URL once the user navigates.
    if (address_bar_ && !address_bar_->HasFocus()) {
      SetAddressBarText("");
    }
  } else {
    browser_view->SetVisible(false);
    UpdateTabStrip();
  }
  window_->InvalidateLayout();
}

void BrowserWindow::CloseActiveTab() {
  if (active_tab_ < tabs_.size()) {
    CloseTabAt(active_tab_);
  }
}

void BrowserWindow::CloseTabAt(size_t index) {
  CEF_REQUIRE_UI_THREAD();
  if (index >= tabs_.size()) {
    return;
  }

  // Multiple CefBrowserViews share ONE CefWindow here. CEF has no supported
  // way to close such a browser via CloseBrowser()/TryCloseBrowser(): with a
  // still-attached view, DoClose() returning false makes CEF call
  // CloseHostWindow() -> widget->Close() on the SHARED top-level window (the
  // whole-window teardown of v2, or the CanClose deadlock of v3). See CEF
  // issue #3376 (open) and libcef/browser/alloy/alloy_browser_host_impl.cc.
  //
  // The one sanctioned path (verified against CEF 150 sources) is Views-
  // hierarchy teardown:
  //   1. Remove the CefBrowserView from the window. While attached, the
  //      views::View held the reference to the CefBrowserView (view_util
  //      PassOwnership); on removal that reference is released, so our Tab
  //      struct now holds the LAST strong reference (the browser-side
  //      platform delegate only keeps a WeakPtr).
  //   2. Release that last reference. ~CefBrowserViewImpl handles exactly
  //      this case ("BrowserView was removed from the Views hierarchy prior
  //      to tear-down and the last BrowserView reference was released") by
  //      calling browser->WindowDestroyed(), which sets window_destroyed_ and
  //      force-closes ONLY this browser -- CloseContents() then skips both
  //      DoClose() and CloseHostWindow() and destroys the browser directly.
  //   3. OnBeforeClose() fires -> OnBrowserClosed() finds no tab (already
  //      removed) -> no-op; OpenNyxClient drops it from browser_list_ and
  //      quits the app when the last browser everywhere is gone.
  //
  // RemoveTabAt() performs step 1 and -- because its local Tab copy goes out
  // of scope -- step 2. The window and all other tabs are never touched.
  //
  // Closing the LAST tab is instead routed through the window close itself:
  // window_->Close() -> CanClose() detaches the remaining tab and allows the
  // close. This keeps the ordering sane (window teardown starts before the
  // last browser is destroyed and CefQuitMessageLoop() fires).
  //
  // CRITICAL: destroying a tab MUST NOT happen re-entrantly inside the button
  // click handler (OnButtonPressed -> CloseTabAt). Tearing a CefBrowserView /
  // browser down while CEF is still on the click's call stack is what made a
  // single-tab close cascade into a full-window teardown. So we capture the
  // tab id now and defer the actual destruction to a fresh UI-thread stack.
  const int tab_id = tabs_[index].id;
  CefPostTask(TID_UI, base::BindOnce(&BrowserWindow::DestroyTabById, this,
                                     tab_id));
}

void BrowserWindow::DestroyTabById(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  // Resolve the id to a current index (indices may have shifted since the
  // click was posted).
  size_t index = tabs_.size();
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      index = i;
      break;
    }
  }
  if (index >= tabs_.size()) {
    return;
  }
  if (tabs_.size() == 1) {
    // Last tab -> close the whole window (on this fresh stack).
    MaybeCloseWindow();
    return;
  }
  RemoveTabAt(index);
}

void BrowserWindow::MaybeCloseWindow() {
  if (window_close_issued_ || !window_) {
    return;
  }
  window_close_issued_ = true;
  // CanClose() detaches any remaining tabs (destroying their browsers) and
  // then allows the close.
  window_->Close();
}

void BrowserWindow::RemoveTabAt(size_t index) {
  if (index >= tabs_.size() || !window_) {
    return;
  }
  Tab tab = tabs_[index];
  tabs_.erase(tabs_.begin() + index);

  if (tab_strip_) {
    tab_strip_->RemoveChildView(tab.tab_panel);
  }
  window_->RemoveChildView(tab.browser_view);

  if (!tabs_.empty()) {
    if (active_tab_ >= tabs_.size()) {
      active_tab_ = tabs_.size() - 1;
    } else if (index < active_tab_) {
      --active_tab_;
    }
    SelectTab(active_tab_);
  }
  window_->InvalidateLayout();
}

void BrowserWindow::SelectTab(size_t index) {
  if (index >= tabs_.size()) {
    return;
  }
  active_tab_ = index;

  for (size_t i = 0; i < tabs_.size(); ++i) {
    tabs_[i].browser_view->SetVisible(i == index);
  }
  UpdateTabStrip();
  UpdateWindowTitle();

  // Sync toolbar state with the newly active tab.
  CefRefPtr<CefBrowser> browser = tabs_[index].browser_view->GetBrowser();
  if (browser) {
    // Always sync the address bar to the newly-activated tab. (No HasFocus
    // guard here: switching tabs is an explicit user action and must show the
    // new tab's URL, even if the address bar happened to hold focus.)
    if (address_bar_) {
      const std::string url = browser->GetMainFrame()->GetURL().ToString();
      SetAddressBarText(ShouldHideUrlInAddressBar(url) ? "" : url);
    }
    if (back_button_) {
      back_button_->SetEnabled(browser->CanGoBack());
    }
    if (forward_button_) {
      forward_button_->SetEnabled(browser->CanGoForward());
    }
  } else if (address_bar_) {
    // A freshly-created tab's CefBrowser is not attached yet, so GetBrowser()
    // returns null here. Without this branch the address bar would keep the
    // PREVIOUS tab's URL. A brand-new tab always opens the new-tab page, so
    // show an empty, inviting address bar. Once the browser attaches,
    // OnBrowserAddressChange keeps it in sync.
    SetAddressBarText("");
    if (back_button_) {
      back_button_->SetEnabled(false);
    }
    if (forward_button_) {
      forward_button_->SetEnabled(false);
    }
  }

  if (window_) {
    window_->InvalidateLayout();
  }
  UpdateChrome();
  tabs_[index].browser_view->RequestFocus();
}

void BrowserWindow::SelectNextTab(bool forward) {
  if (tabs_.size() < 2) {
    return;
  }
  const size_t count = tabs_.size();
  const size_t next =
      forward ? (active_tab_ + 1) % count : (active_tab_ + count - 1) % count;
  SelectTab(next);
}

// ---- Internal helpers ----

void BrowserWindow::ApplyTheme() {
  if (!window_) {
    return;
  }
  // Address bar / textfield colors. In CEF 150 the per-Textfield color setters
  // (SetTextColor/SetSelectionBackgroundColor/SetPlaceholderTextColor) are
  // gated behind CEF_API_REMOVED(15000) and are compiled out at the default
  // (experimental) API version this project builds against. The supported way
  // to color textfields is to override the standard theme color IDs on the
  // Window; every Textfield in the hierarchy then picks them up on
  // OnThemeChanged. This is what makes the address bar readable (light text on
  // a lighter-than-window input background) instead of black-on-black.
  window_->SetThemeColor(CEF_ColorTextfieldBackground, kColorFieldBg);
  window_->SetThemeColor(CEF_ColorTextfieldForeground, kColorFieldText);
  window_->SetThemeColor(CEF_ColorTextfieldForegroundPlaceholder,
                         kColorFieldPlaceholder);
  window_->SetThemeColor(CEF_ColorTextfieldSelectionBackground,
                         kColorFieldSelectionBg);
  window_->SetThemeColor(CEF_ColorTextfieldSelectionForeground,
                         kColorFieldSelectionText);
  window_->SetThemeColor(CEF_ColorTextfieldOutline, kColorFieldOutline);
  window_->SetThemeColor(CEF_ColorTextfieldFilledUnderline, kColorFieldOutline);
  window_->SetThemeColor(CEF_ColorTextfieldFilledUnderlineFocused,
                         kColorFieldOutlineFocused);

  // Primary background/foreground so any theme-driven surfaces stay dark.
  window_->SetThemeColor(CEF_ColorPrimaryBackground, kColorWindowBg);
  window_->SetThemeColor(CEF_ColorPrimaryForeground, kColorText);

  // Text selection inside the field area.
  window_->SetThemeColor(CEF_ColorTextSelectionBackground,
                         kColorFieldSelectionBg);
  window_->SetThemeColor(CEF_ColorTextSelectionForeground,
                         kColorFieldSelectionText);
}

void BrowserWindow::OnThemeColorsChanged(CefRefPtr<CefWindow> window,
                                         bool chrome_theme) {
  CEF_REQUIRE_UI_THREAD();
  // The OS/Chrome theme changed and reset our overrides — re-apply them.
  // OnThemeChanged notifications are triggered automatically afterwards.
  ApplyTheme();
}

void BrowserWindow::BuildUI() {
  // Vertical box layout: [tab strip][toolbar][browser views...].
  CefBoxLayoutSettings window_layout;
  window_layout.horizontal = false;
  window_->SetToBoxLayout(window_layout);

  // -- Tab strip --
  tab_strip_ = CefPanel::CreatePanel(nullptr);
  tab_strip_->SetID(ID_TAB_STRIP);
  tab_strip_->SetBackgroundColor(kColorTabStripBg);
  CefBoxLayoutSettings strip_layout;
  strip_layout.horizontal = true;
  strip_layout.between_child_spacing = 3;
  strip_layout.inside_border_horizontal_spacing = 6;
  strip_layout.inside_border_vertical_spacing = 4;
  strip_layout.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
  tab_strip_->SetToBoxLayout(strip_layout);

  new_tab_button_ = CefLabelButton::CreateLabelButton(this, "+");
  new_tab_button_->SetID(ID_NEW_TAB_BUTTON);
  new_tab_button_->SetFontList("Segoe UI, 16px");
  new_tab_button_->SetInkDropEnabled(true);
  new_tab_button_->SetFocusable(false);
  new_tab_button_->SetMinimumSize(CefSize(32, 30));
  new_tab_button_->SetMaximumSize(CefSize(32, 30));
  new_tab_button_->SetTextColor(CEF_BUTTON_STATE_NORMAL, kColorTextDim);
  new_tab_button_->SetTextColor(CEF_BUTTON_STATE_HOVERED, kColorText);
  new_tab_button_->SetTooltipText("New tab (Ctrl+T)");
  tab_strip_->AddChildView(new_tab_button_);

  // -- Flexible drag spacer + frameless window controls --
  // A flexible, empty panel eats the remaining horizontal space so the window
  // controls sit at the far right. It doubles as the draggable caption area
  // (see the draggable regions set in OnWindowCreated).
  caption_spacer_ = CefPanel::CreatePanel(nullptr);
  caption_spacer_->SetID(ID_CAPTION_SPACER);
  tab_strip_->AddChildView(caption_spacer_);
  if (CefRefPtr<CefBoxLayout> strip_box =
          tab_strip_->GetLayout()->AsBoxLayout()) {
    strip_box->SetFlexForView(caption_spacer_, 1);
  }

  auto make_caption_button = [this](const char* label, int id,
                                    const char* tooltip, cef_color_t hover) {
    CefRefPtr<CefLabelButton> b = CefLabelButton::CreateLabelButton(this, label);
    b->SetID(id);
    b->SetFontList("Segoe UI, 13px");
    b->SetInkDropEnabled(true);
    b->SetFocusable(false);
    b->SetMinimumSize(CefSize(40, 30));
    b->SetMaximumSize(CefSize(40, 30));
    b->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_CENTER);
    b->SetTextColor(CEF_BUTTON_STATE_NORMAL, kColorTextDim);
    b->SetTextColor(CEF_BUTTON_STATE_HOVERED, hover);
    b->SetTooltipText(tooltip);
    return b;
  };
  // Minimize (U+2013 en dash), Maximize (U+25A1 white square), Close (U+2715).
  minimize_button_ = make_caption_button("\xE2\x80\x93", ID_MINIMIZE_BUTTON,
                                         "Minimize", kColorText);
  maximize_button_ = make_caption_button("\xE2\x96\xA1", ID_MAXIMIZE_BUTTON,
                                         "Maximize", kColorText);
  // Close hover = red, like every OS.
  close_window_button_ = make_caption_button("\xE2\x9C\x95",
                                             ID_CLOSE_WINDOW_BUTTON, "Close",
                                             kColorCloseHover);
  tab_strip_->AddChildView(minimize_button_);
  tab_strip_->AddChildView(maximize_button_);
  tab_strip_->AddChildView(close_window_button_);

  // -- Toolbar --
  toolbar_ = CefPanel::CreatePanel(nullptr);
  toolbar_->SetID(ID_TOOLBAR);
  toolbar_->SetBackgroundColor(kColorToolbarBg);
  CefBoxLayoutSettings toolbar_layout;
  toolbar_layout.horizontal = true;
  toolbar_layout.between_child_spacing = 6;
  toolbar_layout.inside_border_horizontal_spacing = 8;
  toolbar_layout.inside_border_vertical_spacing = 6;
  toolbar_layout.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
  CefRefPtr<CefBoxLayout> tb_layout = toolbar_->SetToBoxLayout(toolbar_layout);

  auto make_nav_button = [this](const char* label, int id,
                                const char* tooltip) {
    CefRefPtr<CefLabelButton> button =
        CefLabelButton::CreateLabelButton(this, label);
    button->SetID(id);
    button->SetFontList("Segoe UI, 15px");
    button->SetInkDropEnabled(true);
    button->SetFocusable(false);
    button->SetMinimumSize(CefSize(34, 30));
    button->SetMaximumSize(CefSize(34, 30));
    button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_CENTER);
    button->SetTextColor(CEF_BUTTON_STATE_NORMAL, kColorText);
    button->SetTextColor(CEF_BUTTON_STATE_HOVERED, kColorText);
    button->SetTextColor(CEF_BUTTON_STATE_PRESSED, kColorAccent);
    button->SetTextColor(CEF_BUTTON_STATE_DISABLED, kColorButtonDisabled);
    button->SetTooltipText(tooltip);
    return button;
  };

  back_button_ = make_nav_button("←", ID_BACK_BUTTON, "Back (Alt+Left)");
  back_button_->SetEnabled(false);
  forward_button_ =
      make_nav_button("→", ID_FORWARD_BUTTON, "Forward (Alt+Right)");
  forward_button_->SetEnabled(false);
  reload_button_ = make_nav_button("⟳", ID_RELOAD_BUTTON, "Reload (Ctrl+R)");
  home_button_ = make_nav_button("⌂", ID_HOME_BUTTON, "New-tab page");

  address_bar_ = CefTextfield::CreateTextfield(this);
  address_bar_->SetID(ID_ADDRESS_BAR);
  address_bar_->SetFontList(kFontList);
  // Text/background/selection/placeholder colors are supplied by the window
  // theme (see ApplyTheme). A comfortable min height gives the field a
  // rounded, roomy feel next to the nav buttons.
  address_bar_->SetPlaceholderText("Search with Brave or enter address");
  address_bar_->SetAccessibleName("Address and search bar");

  // Shield (blocked-request count), star (bookmark), menu (settings).
  shield_button_ = make_nav_button("\xF0\x9F\x9B\xA1", ID_SHIELD_BUTTON,
                                   "Trackers blocked on this site");
  shield_button_->SetTextColor(CEF_BUTTON_STATE_NORMAL, kColorTextDim);
  star_button_ = make_nav_button("\xE2\x98\x86", ID_STAR_BUTTON,
                                 "Bookmark this page (Ctrl+D)");
  menu_button_ = make_nav_button("\xE2\x98\xB0", ID_MENU_BUTTON,
                                 "Menu / Settings (Ctrl+,)");

  toolbar_->AddChildView(back_button_);
  toolbar_->AddChildView(forward_button_);
  toolbar_->AddChildView(reload_button_);
  toolbar_->AddChildView(home_button_);
  toolbar_->AddChildView(address_bar_);
  toolbar_->AddChildView(shield_button_);
  toolbar_->AddChildView(star_button_);
  toolbar_->AddChildView(menu_button_);
  tb_layout->SetFlexForView(address_bar_, 1);

  window_->AddChildView(tab_strip_);
  window_->AddChildView(toolbar_);
}

void BrowserWindow::AddAccelerators() {
  // high_priority=true: handle before web content sees the key event.
  window_->SetAccelerator(CMD_NEW_TAB, 'T', false, true, false, true);
  window_->SetAccelerator(CMD_CLOSE_TAB, 'W', false, true, false, true);
  window_->SetAccelerator(CMD_NEXT_TAB, kVK_TAB, false, true, false, true);
  window_->SetAccelerator(CMD_PREV_TAB, kVK_TAB, true, true, false, true);
  window_->SetAccelerator(CMD_FOCUS_ADDRESS, 'L', false, true, false, true);
  window_->SetAccelerator(CMD_RELOAD, 'R', false, true, false, true);
  window_->SetAccelerator(CMD_RELOAD_F5, kVK_F5, false, false, false, true);
  window_->SetAccelerator(CMD_RELOAD_IGNORE_CACHE, 'R', true, true, false,
                          true);
  window_->SetAccelerator(CMD_BACK, kVK_LEFT, false, false, true, true);
  window_->SetAccelerator(CMD_FORWARD, kVK_RIGHT, false, false, true, true);
  window_->SetAccelerator(CMD_DEVTOOLS, 'I', true, true, false, true);
  // M3 pages: Ctrl+H history, Ctrl+D bookmark, Ctrl+J downloads,
  // Ctrl+, settings.
  window_->SetAccelerator(CMD_HISTORY, 'H', false, true, false, true);
  window_->SetAccelerator(CMD_BOOKMARK, 'D', false, true, false, true);
  window_->SetAccelerator(CMD_DOWNLOADS, 'J', false, true, false, true);
  window_->SetAccelerator(CMD_SETTINGS, kVK_OEM_COMMA, false, true, false, true);
  // Move the active tab left/right (keyboard companion to drag-reorder).
  window_->SetAccelerator(CMD_MOVE_TAB_LEFT, kVK_PRIOR, true, true, false,
                          true);
  window_->SetAccelerator(CMD_MOVE_TAB_RIGHT, kVK_NEXT, true, true, false,
                          true);
}

CefRefPtr<CefBrowserView> BrowserWindow::ActiveBrowserView() {
  if (active_tab_ < tabs_.size()) {
    return tabs_[active_tab_].browser_view;
  }
  return nullptr;
}

CefRefPtr<CefBrowser> BrowserWindow::ActiveBrowser() {
  if (CefRefPtr<CefBrowserView> view = ActiveBrowserView()) {
    return view->GetBrowser();
  }
  return nullptr;
}

int BrowserWindow::FindTabIndex(CefRefPtr<CefBrowserView> view) {
  if (!view) {
    return -1;
  }
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].browser_view->IsSame(view)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void BrowserWindow::UpdateTabStrip() {
  for (size_t i = 0; i < tabs_.size(); ++i) {
    const bool active = (i == active_tab_);
    const cef_color_t bg = active ? kColorTabActive : kColorTabInactive;
    tabs_[i].tab_panel->SetBackgroundColor(bg);
    // Active tab: bright text; inactive: dimmed but brightens on hover.
    tabs_[i].tab_button->SetTextColor(CEF_BUTTON_STATE_NORMAL,
                                      active ? kColorText : kColorTextDim);
    tabs_[i].tab_button->SetTextColor(CEF_BUTTON_STATE_HOVERED, kColorText);
    tabs_[i].close_button->SetTextColor(CEF_BUTTON_STATE_NORMAL,
                                        active ? kColorTextDim : kColorTextDim);
    tabs_[i].close_button->SetTextColor(CEF_BUTTON_STATE_HOVERED, kColorText);
  }
}

void BrowserWindow::UpdateWindowTitle() {
  if (!window_) {
    return;
  }
  std::string title = "OpenNyx";
  if (active_tab_ < tabs_.size() && !tabs_[active_tab_].title.empty() &&
      tabs_[active_tab_].title != "New Tab") {
    title = tabs_[active_tab_].title + " — OpenNyx";
  }
  window_->SetTitle(title);
}

void BrowserWindow::SetAddressBarText(const std::string& text) {
  if (!address_bar_) {
    return;
  }
  if (text.empty()) {
    // SetText("") on an unfocused, already-populated textfield does not
    // reliably repaint in CEF's views (the old URL visually sticks even though
    // the model is cleared -- confirmed via tracing). Force a real edit:
    // select everything, then Delete. This goes through the user-triggered
    // change path which schedules a paint.
    if (!address_bar_->GetText().empty()) {
      address_bar_->SelectAll(false);
      address_bar_->ExecuteCommand(CEF_TFC_DELETE);
    }
    // Belt and suspenders: also set empty so state is unambiguous.
    address_bar_->SetText("");
  } else {
    address_bar_->SetText(text);
  }
}

void BrowserWindow::UpdateDraggableRegions() {
  if (!window_ || !caption_spacer_) {
    return;
  }
  // Only the empty caption spacer is draggable. Interactive controls (tabs,
  // buttons, address bar) keep their own hit-testing because they are NOT
  // included as draggable regions.
  std::vector<CefDraggableRegion> regions;
  const CefRect b = caption_spacer_->GetBoundsInScreen();
  const CefRect win = window_->GetBoundsInScreen();
  if (b.width > 0 && b.height > 0) {
    // Convert screen coords to window-relative coords.
    CefRect r(b.x - win.x, b.y - win.y, b.width, b.height);
    regions.push_back(CefDraggableRegion(r, /*draggable=*/true));
  }
  window_->SetDraggableRegions(regions);
}

void BrowserWindow::ActivateWithNewTab() {
  CEF_REQUIRE_UI_THREAD();
  if (window_) {
    window_->Show();
    window_->BringToTop();
  }
  CreateTab(GetNewTabURL(), /*select=*/true);
}

void BrowserWindow::FocusAddressBar() {
  if (address_bar_) {
    address_bar_->RequestFocus();
    address_bar_->SelectAll(false);
  }
}

void BrowserWindow::OnFocus(CefRefPtr<CefView> view) {
  CEF_REQUIRE_UI_THREAD();
  if (!view || view->GetID() != ID_ADDRESS_BAR) {
    return;
  }
  CefRefPtr<CefTextfield> field = view->AsTextfield();
  if (!field) {
    return;
  }
  // Select the whole URL when the address bar first gains focus, matching
  // every mainstream browser (one click -> full selection). The selection is
  // posted to a fresh UI-thread stack: a mouse click that grants focus also
  // sets the caret on mouse-up, which would clear an immediate SelectAll, so
  // we run it just after the click settles.
  CefPostTask(TID_UI, base::BindOnce(
                          [](CefRefPtr<CefTextfield> f) {
                            if (f && f->HasFocus() && !f->GetText().empty()) {
                              f->SelectAll(false);
                            }
                          },
                          field));
}

void BrowserWindow::NavigateActiveTab(const std::string& input) {
  const std::string url = ResolveAddressInput(input);
  if (url.empty()) {
    return;
  }
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return;
  }
  browser->GetMainFrame()->LoadURL(url);
  if (CefRefPtr<CefBrowserView> view = ActiveBrowserView()) {
    view->RequestFocus();
  }
}

// ---- M3/M4: bookmarks star + tracker shield ----

std::string BrowserWindow::GetBrowserTitle(CefRefPtr<CefBrowser> browser) {
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0) {
    return std::string();
  }
  return tabs_[index].title;
}

void BrowserWindow::RefreshChromeForBrowser(CefRefPtr<CefBrowser> browser) {
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0 || static_cast<size_t>(index) != active_tab_) {
    return;
  }
  UpdateChrome();
}

void BrowserWindow::UpdateChrome() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return;
  }
  const std::string url = browser->GetMainFrame()->GetURL().ToString();

  // Star: filled if bookmarked.
  if (star_button_) {
    const bool marked = OpenNyxStore::Get()->IsBookmarked(url);
    star_button_->SetText(marked ? "\xE2\x98\x85" : "\xE2\x98\x86");  // ★ / ☆
    star_button_->SetTextColor(CEF_BUTTON_STATE_NORMAL,
                               marked ? kColorAccent : kColorText);
  }

  // Shield: show per-site blocked count.
  if (shield_button_) {
    CefURLParts parts;
    std::string host;
    if (CefParseURL(url, parts)) {
      host = CefString(&parts.host).ToString();
    }
    const int count = OpenNyxBlocklist::Get()->GetCount(host);
    std::string label = "\xF0\x9F\x9B\xA1";  // 🛡
    if (count > 0) {
      label += " " + std::to_string(count);
    }
    shield_button_->SetText(label);
    shield_button_->SetTextColor(
        CEF_BUTTON_STATE_NORMAL,
        count > 0 ? kColorAccent : kColorTextDim);
    shield_button_->SetTooltipText(
        OpenNyxBlocklist::Get()->enabled()
            ? (count > 0
                   ? (std::to_string(count) + " trackers blocked on this site")
                   : std::string("Tracker blocking on"))
            : std::string("Tracker blocking off"));
  }
}

void BrowserWindow::ToggleBookmarkActiveTab() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return;
  }
  const std::string url = browser->GetMainFrame()->GetURL().ToString();
  if (url.empty() || url.compare(0, 5, "data:") == 0) {
    return;
  }
  OpenNyxStore* store = OpenNyxStore::Get();
  if (store->IsBookmarked(url)) {
    store->RemoveBookmark(url);
  } else {
    const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
    const std::string title =
        index >= 0 ? tabs_[index].title : std::string();
    store->AddBookmark(url, title);
  }
  UpdateChrome();
}

// static
void BrowserWindow::RequestClearBrowsingData() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&BrowserWindow::RequestClearBrowsingData));
    return;
  }
  // Clear cookies + cache via the global request context.
  CefRefPtr<CefRequestContext> ctx = CefRequestContext::GetGlobalContext();
  if (ctx) {
    CefRefPtr<CefCookieManager> cookies = ctx->GetCookieManager(nullptr);
    if (cookies) {
      cookies->DeleteCookies(CefString(), CefString(), nullptr);
    }
  }
}
