// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefclient sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "browser_window.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "include/cef_parser.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/wrapper/cef_helpers.h"

#include "opennyx_client.h"

namespace {

BrowserWindow* g_browser_window = nullptr;

// ---- View ids ----
enum ViewID {
  ID_WINDOW = 1,
  ID_TAB_STRIP,
  ID_TOOLBAR,
  ID_BACK_BUTTON,
  ID_FORWARD_BUTTON,
  ID_RELOAD_BUTTON,
  ID_NEW_TAB_BUTTON,
  ID_ADDRESS_BAR,
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
};

// ---- Windows virtual key codes (avoid pulling in windows.h here) ----
constexpr int kVK_RETURN = 0x0D;
constexpr int kVK_ESCAPE = 0x1B;
constexpr int kVK_TAB = 0x09;
constexpr int kVK_F5 = 0x74;
constexpr int kVK_LEFT = 0x25;
constexpr int kVK_RIGHT = 0x27;

// ---- Dark theme palette ----
constexpr cef_color_t kColorWindowBg = CefColorSetARGB(255, 24, 25, 30);
constexpr cef_color_t kColorToolbarBg = CefColorSetARGB(255, 32, 33, 40);
constexpr cef_color_t kColorTabActive = CefColorSetARGB(255, 48, 50, 60);
constexpr cef_color_t kColorTabInactive = CefColorSetARGB(255, 28, 29, 35);
constexpr cef_color_t kColorText = CefColorSetARGB(255, 225, 226, 232);
constexpr cef_color_t kColorTextDim = CefColorSetARGB(255, 150, 152, 160);
constexpr cef_color_t kColorAccent = CefColorSetARGB(255, 122, 92, 255);
constexpr cef_color_t kColorFieldBg = CefColorSetARGB(255, 16, 17, 21);

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
    window->SetTitle("OpenNyx");
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();
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

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
};

}  // namespace

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

std::string GetNewTabURL() {
  static std::string url;
  if (url.empty()) {
    const char kHtml[] =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>New Tab</title><style>"
        "html,body{height:100%;margin:0}"
        "body{background:#18191e;color:#e1e2e8;display:flex;align-items:center;"
        "justify-content:center;font-family:'Segoe UI',system-ui,sans-serif}"
        ".wrap{text-align:center;width:min(620px,90vw);"
        "transform:translateY(-8vh)}"
        "h1{font-size:44px;font-weight:600;letter-spacing:.5px;margin:0 0 28px}"
        "h1 .nyx{color:#7a5cff}"
        "form{display:flex;gap:0;box-shadow:0 6px 30px rgba(0,0,0,.45);"
        "border-radius:12px;overflow:hidden;border:1px solid #303240}"
        "input[type=search]{flex:1;padding:14px 18px;font-size:16px;border:0;"
        "outline:none;background:#101115;color:#e1e2e8}"
        "input[type=search]::placeholder{color:#969ba0}"
        "button{padding:14px 22px;border:0;background:#7a5cff;color:#fff;"
        "font-size:15px;font-weight:600;cursor:pointer}"
        "button:hover{background:#8d73ff}"
        ".hint{margin-top:22px;font-size:12.5px;color:#5c5f6a}"
        "</style></head><body><div class=\"wrap\">"
        "<h1>Open<span class=\"nyx\">Nyx</span></h1>"
        "<form action=\"https://search.brave.com/search\" method=\"get\">"
        "<input type=\"search\" name=\"q\" placeholder=\"Search the web "
        "privately…\" autofocus autocomplete=\"off\">"
        "<button type=\"submit\">Search</button></form>"
        "<div class=\"hint\">Private by default · powered by Brave Search · "
        "no Google services</div>"
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

  // Everything else -> Brave Search.
  return "https://search.brave.com/search?q=" +
         CefURIEncode(input, /*use_plus=*/true).ToString();
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

  BuildUI();
  AddAccelerators();

  // First tab.
  CreateTab(pending_initial_url_, /*select=*/true);
  pending_initial_url_.clear();

  window_->Show();

  if (CefRefPtr<CefBrowserView> view = ActiveBrowserView()) {
    view->RequestFocus();
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
  new_tab_button_ = nullptr;
  address_bar_ = nullptr;
  tabs_.clear();
  g_browser_window = nullptr;
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  if (tabs_.empty()) {
    return true;
  }
  // Ask every remaining tab browser to close. Once the last one is gone,
  // OnBrowserClosed() closes the window for real.
  closing_ = true;
  // Iterate over a copy: TryCloseBrowser may synchronously mutate |tabs_|.
  std::vector<CefRefPtr<CefBrowserView>> views;
  views.reserve(tabs_.size());
  for (const auto& tab : tabs_) {
    views.push_back(tab.browser_view);
  }
  for (const auto& view : views) {
    if (CefRefPtr<CefBrowser> browser = view->GetBrowser()) {
      browser->GetHost()->TryCloseBrowser();
    }
  }
  return tabs_.empty();
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

bool BrowserWindow::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  // Host popups (window.open) and DevTools in their own top-level window.
  CefWindow::CreateTopLevelWindow(
      new PopupWindowDelegate(popup_browser_view));
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
    case ID_NEW_TAB_BUTTON:
      CreateTab(GetNewTabURL(), /*select=*/true);
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
    // Hide the noisy data: URL of the new-tab page.
    address_bar_->SetText(spec.compare(0, 5, "data:") == 0 ? "" : spec);
  }
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
  const int index = FindTabIndex(CefBrowserView::GetForBrowser(browser));
  if (index < 0) {
    return false;
  }
  RemoveTabAt(static_cast<size_t>(index));

  if (tabs_.empty() && window_) {
    // Last tab gone -> close the window.
    closing_ = true;
    window_->Close();
  }
  return true;
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
  tab.tab_button->SetMinimumSize(CefSize(120, 30));
  tab.tab_button->SetMaximumSize(CefSize(220, 30));

  // Tab close button.
  tab.close_button = CefLabelButton::CreateLabelButton(this, "×");
  tab.close_button->SetID(ID_TAB_FIRST + tab.id * 2 + 1);
  tab.close_button->SetFontList(kTabFontList);
  tab.close_button->SetInkDropEnabled(true);
  tab.close_button->SetFocusable(false);
  tab.close_button->SetMinimumSize(CefSize(26, 30));
  tab.close_button->SetMaximumSize(CefSize(26, 30));

  // Panel grouping title + close button.
  tab.tab_panel = CefPanel::CreatePanel(nullptr);
  CefBoxLayoutSettings panel_layout;
  panel_layout.horizontal = true;
  tab.tab_panel->SetToBoxLayout(panel_layout);
  tab.tab_panel->AddChildView(tab.tab_button);
  tab.tab_panel->AddChildView(tab.close_button);

  // Insert before the trailing new-tab button.
  tabs_.push_back(tab);
  if (tab_strip_) {
    tab_strip_->RemoveChildView(new_tab_button_);
    tab_strip_->AddChildView(tab.tab_panel);
    tab_strip_->AddChildView(new_tab_button_);
  }

  // Browser views stack in the content area; only the active one is visible.
  window_->AddChildView(browser_view);
  CefRefPtr<CefBoxLayout> layout =
      window_->GetLayout()->AsBoxLayout();
  if (layout) {
    layout->SetFlexForView(browser_view, 1);
  }

  if (select) {
    SelectTab(tabs_.size() - 1);
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
  if (index >= tabs_.size()) {
    return;
  }
  CefRefPtr<CefBrowser> browser = tabs_[index].browser_view->GetBrowser();
  if (browser) {
    // OnBrowserClosed() removes the tab once the browser is really gone.
    browser->GetHost()->TryCloseBrowser();
  } else {
    RemoveTabAt(index);
  }
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
    if (address_bar_ && !address_bar_->HasFocus()) {
      const std::string url = browser->GetMainFrame()->GetURL().ToString();
      address_bar_->SetText(url.compare(0, 5, "data:") == 0 ? "" : url);
    }
    if (back_button_) {
      back_button_->SetEnabled(browser->CanGoBack());
    }
    if (forward_button_) {
      forward_button_->SetEnabled(browser->CanGoForward());
    }
  }

  if (window_) {
    window_->InvalidateLayout();
  }
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

void BrowserWindow::BuildUI() {
  // Vertical box layout: [tab strip][toolbar][browser views...].
  CefBoxLayoutSettings window_layout;
  window_layout.horizontal = false;
  window_->SetToBoxLayout(window_layout);

  // -- Tab strip --
  tab_strip_ = CefPanel::CreatePanel(nullptr);
  tab_strip_->SetID(ID_TAB_STRIP);
  tab_strip_->SetBackgroundColor(kColorWindowBg);
  CefBoxLayoutSettings strip_layout;
  strip_layout.horizontal = true;
  strip_layout.between_child_spacing = 2;
  strip_layout.inside_border_horizontal_spacing = 4;
  strip_layout.inside_border_vertical_spacing = 3;
  strip_layout.cross_axis_alignment = CEF_AXIS_ALIGNMENT_CENTER;
  tab_strip_->SetToBoxLayout(strip_layout);

  new_tab_button_ = CefLabelButton::CreateLabelButton(this, "+");
  new_tab_button_->SetID(ID_NEW_TAB_BUTTON);
  new_tab_button_->SetFontList(kFontList);
  new_tab_button_->SetInkDropEnabled(true);
  new_tab_button_->SetFocusable(false);
  new_tab_button_->SetMinimumSize(CefSize(30, 30));
  new_tab_button_->SetMaximumSize(CefSize(30, 30));
  new_tab_button_->SetTooltipText("New tab (Ctrl+T)");
  tab_strip_->AddChildView(new_tab_button_);

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
    button->SetFontList(kFontList);
    button->SetInkDropEnabled(true);
    button->SetFocusable(false);
    button->SetMinimumSize(CefSize(32, 30));
    button->SetMaximumSize(CefSize(32, 30));
    button->SetTooltipText(tooltip);
    return button;
  };

  back_button_ = make_nav_button("←", ID_BACK_BUTTON, "Back (Alt+Left)");
  back_button_->SetEnabled(false);
  forward_button_ =
      make_nav_button("→", ID_FORWARD_BUTTON, "Forward (Alt+Right)");
  forward_button_->SetEnabled(false);
  reload_button_ = make_nav_button("⟳", ID_RELOAD_BUTTON, "Reload (Ctrl+R)");

  address_bar_ = CefTextfield::CreateTextfield(this);
  address_bar_->SetID(ID_ADDRESS_BAR);
  address_bar_->SetFontList(kFontList);
  address_bar_->SetBackgroundColor(kColorFieldBg);
  address_bar_->SetTextColor(kColorText);
  address_bar_->SetPlaceholderText("Search with Brave or enter address");
  address_bar_->SetPlaceholderTextColor(kColorTextDim);
  address_bar_->SetSelectionBackgroundColor(kColorAccent);

  toolbar_->AddChildView(back_button_);
  toolbar_->AddChildView(forward_button_);
  toolbar_->AddChildView(reload_button_);
  toolbar_->AddChildView(address_bar_);
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
    tabs_[i].tab_button->SetTextColor(CEF_BUTTON_STATE_NORMAL,
                                      active ? kColorText : kColorTextDim);
    tabs_[i].close_button->SetTextColor(CEF_BUTTON_STATE_NORMAL,
                                        active ? kColorText : kColorTextDim);
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

void BrowserWindow::FocusAddressBar() {
  if (address_bar_) {
    address_bar_->RequestFocus();
    address_bar_->SelectAll(false);
  }
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
