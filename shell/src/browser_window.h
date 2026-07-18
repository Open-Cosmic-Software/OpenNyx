// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefclient sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#ifndef OPENNYX_SHELL_SRC_BROWSER_WINDOW_H_
#define OPENNYX_SHELL_SRC_BROWSER_WINDOW_H_

#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

// The main OpenNyx browser window: a CEF Views top-level window that owns
//  * a tab strip (one button per tab + close buttons + new-tab button),
//  * a toolbar (back / forward / reload + address bar),
//  * one CefBrowserView per tab (only the active tab is visible).
//
// All methods must be called on the CEF UI thread.
class BrowserWindow : public CefWindowDelegate,
                      public CefBrowserViewDelegate,
                      public CefTextfieldDelegate,
                      public CefButtonDelegate,
                      public CefPanelDelegate {
 public:
  // Creates the singleton main window with an initial tab showing |url|
  // (empty = new-tab page).
  static void Create(const std::string& url);

  // Returns the singleton main window, or nullptr if it was destroyed.
  static BrowserWindow* Get();

  // ---- Notifications forwarded from OpenNyxClient (UI thread) ----
  void OnBrowserTitleChange(CefRefPtr<CefBrowser> browser,
                            const CefString& title);
  void OnBrowserAddressChange(CefRefPtr<CefBrowser> browser,
                              const CefString& url);
  void OnBrowserLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                   bool is_loading,
                                   bool can_go_back,
                                   bool can_go_forward);
  // Returns true if |browser| belonged to this window (i.e. was a tab).
  bool OnBrowserClosed(CefRefPtr<CefBrowser> browser);

  // Number of open tabs.
  size_t tab_count() const { return tabs_.size(); }

  // ---- Tab management ----
  void CreateTab(const std::string& url, bool select);
  void CloseActiveTab();
  void SelectTab(size_t index);
  void SelectNextTab(bool forward);

  // ---- CefWindowDelegate ----
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;
  CefSize GetMinimumSize(CefRefPtr<CefView> view) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;

  // ---- CefBrowserViewDelegate ----
  ChromeToolbarType GetChromeToolbarType(
      CefRefPtr<CefBrowserView> browser_view) override;
  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override;

  // ---- CefTextfieldDelegate ----
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override;

  // ---- CefButtonDelegate ----
  void OnButtonPressed(CefRefPtr<CefButton> button) override;

 private:
  BrowserWindow() = default;

  struct Tab {
    CefRefPtr<CefBrowserView> browser_view;
    CefRefPtr<CefPanel> tab_panel;        // Container in the tab strip.
    CefRefPtr<CefLabelButton> tab_button; // Shows the page title.
    CefRefPtr<CefLabelButton> close_button;
    std::string title;
    int id = 0;  // Unique tab id (basis for view ids).
  };

  // Builds the tab strip + toolbar and attaches everything to |window_|.
  void BuildUI();
  void AddAccelerators();
  void SetWindowIconFromDisk();

  CefRefPtr<CefBrowserView> ActiveBrowserView();
  CefRefPtr<CefBrowser> ActiveBrowser();
  int FindTabIndex(CefRefPtr<CefBrowserView> view);
  void CloseTabAt(size_t index);
  void RemoveTabAt(size_t index);
  void UpdateTabStrip();
  void UpdateWindowTitle();
  void FocusAddressBar();
  void NavigateActiveTab(const std::string& input);

  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> tab_strip_;
  CefRefPtr<CefPanel> toolbar_;
  CefRefPtr<CefLabelButton> back_button_;
  CefRefPtr<CefLabelButton> forward_button_;
  CefRefPtr<CefLabelButton> reload_button_;
  CefRefPtr<CefLabelButton> new_tab_button_;
  CefRefPtr<CefTextfield> address_bar_;

  std::vector<Tab> tabs_;
  size_t active_tab_ = 0;
  int next_tab_id_ = 1;
  bool closing_ = false;
  std::string pending_initial_url_;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
};

// URL of the built-in dark new-tab page (a data: URL with an OpenNyx
// wordmark and a Brave Search box).
std::string GetNewTabURL();

// Turns raw address-bar input into a URL: URL-ish input is normalized
// (scheme added if missing), everything else becomes a Brave Search query.
std::string ResolveAddressInput(const std::string& input);

#endif  // OPENNYX_SHELL_SRC_BROWSER_WINDOW_H_
