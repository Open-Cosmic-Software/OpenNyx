// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefclient sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#ifndef OPENNYX_SHELL_SRC_BROWSER_WINDOW_H_
#define OPENNYX_SHELL_SRC_BROWSER_WINDOW_H_

#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_menu_model.h"
#include "include/cef_menu_model_delegate.h"
#include "include/internal/cef_types_runtime.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "store.h"  // DownloadEntry

// The main OpenNyx browser window: a CEF Views top-level window that owns
//  * a tab strip (one button per tab + close buttons + new-tab button),
//  * a toolbar (back / forward / reload + address bar),
//  * one CefBrowserView per tab (only the active tab is visible).
//
// All methods must be called on the CEF UI thread.
// NOTE: CefMenuButtonDelegate already derives from CefButtonDelegate, so we
// must NOT list CefButtonDelegate separately (that creates an ambiguous
// duplicate base). CefMenuButtonDelegate covers both button + menu-button
// callbacks.
class BrowserWindow : public CefWindowDelegate,
                      public CefBrowserViewDelegate,
                      public CefTextfieldDelegate,
                      public CefMenuButtonDelegate,
                      public CefMenuModelDelegate {
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

  // Returns the cached page title for |browser| (empty if unknown).
  std::string GetBrowserTitle(CefRefPtr<CefBrowser> browser);

  // Refreshes the toolbar shield count + bookmark star for |browser| if it is
  // the active tab. Safe to call from the UI thread.
  void RefreshChromeForBrowser(CefRefPtr<CefBrowser> browser);

  // Clears cookies + cache for the whole browser (invoked by the settings
  // page "clear browsing data"). Static: posts to the UI thread.
  static void RequestClearBrowsingData();

  // Shell helpers for the downloads page (Windows ShellExecute-based).
  // OpenPath: open a file with its default app. RevealPath: show the file in
  // Explorer with it selected. OpenDownloadsFolder: open the default Downloads
  // directory. Each returns true on success.
  static bool OpenPath(const std::string& path);
  static bool RevealPath(const std::string& path);
  static bool OpenDownloadsFolder();

  // Number of open tabs.
  size_t tab_count() const { return tabs_.size(); }

  // Brings the window to the front and opens a fresh new-tab page. Called when
  // a second app instance is launched (see OpenNyxApp::OnAlreadyRunning).
  void ActivateWithNewTab();

  // ---- Tab management ----
  void CreateTab(const std::string& url, bool select);
  void CloseActiveTab();
  void SelectTab(size_t index);
  void SelectNextTab(bool forward);
  // True if |browser| is hosted by one of this window's tabs.
  bool HasTabForBrowser(CefRefPtr<CefBrowser> browser);
  // Closes the tab hosting |browser| (used for JS window.close()).
  void CloseTabForBrowser(CefRefPtr<CefBrowser> browser);

  // ---- CefWindowDelegate ----
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  // Frameless: OpenNyx draws its own title bar (window controls live in the
  // tab strip). The OS frame/caption is removed.
  bool IsFrameless(CefRefPtr<CefWindow> window) override { return true; }
  bool CanResize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMaximize(CefRefPtr<CefWindow> window) override { return true; }
  bool CanMinimize(CefRefPtr<CefWindow> window) override { return true; }
  void OnWindowBoundsChanged(CefRefPtr<CefWindow> window,
                             const CefRect& new_bounds) override;
  void OnThemeColorsChanged(CefRefPtr<CefWindow> window,
                            bool chrome_theme) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  // Force the Alloy runtime style so CEF renders ONLY the web content inside
  // each BrowserView (no Chrome toolbar / Google NTP). OpenNyx draws its own
  // tab strip, toolbar and start page. Without this, CEF 150 defaults to the
  // Chrome runtime style and shows the full Chrome UI.
  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;
  CefSize GetMinimumSize(CefRefPtr<CefView> view) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;

  // ---- CefBrowserViewDelegate ----
  ChromeToolbarType GetChromeToolbarType(
      CefRefPtr<CefBrowserView> browser_view) override;
  // Force Alloy runtime style for every tab's BrowserView (see the window
  // delegate override above for rationale).
  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }
  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override;
  // Popups and DevTools must NOT reuse this main window as their view delegate
  // (default returns |this|), or the main window's tab/toolbar logic runs for
  // the DevTools view and crashes. Hand them a lightweight popup delegate.
  CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(
      CefRefPtr<CefBrowserView> browser_view,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      bool is_devtools) override;

  // ---- CefViewDelegate ----
  // When the address bar gains focus (e.g. first click), select all its text
  // like every mainstream browser does.
  void OnFocus(CefRefPtr<CefView> view) override;

  // ---- CefTextfieldDelegate ----
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override;
  // Inline address-bar autocomplete from history: after each edit, append the
  // best matching history URL and select the appended part (so the next
  // keystroke overwrites it). Suppressed after Backspace/Delete.
  void OnAfterUserAction(CefRefPtr<CefTextfield> textfield) override;

  // ---- CefButtonDelegate ----
  void OnButtonPressed(CefRefPtr<CefButton> button) override;
  // Drag-to-reorder: a tab title button entering the PRESSED state starts a
  // potential drag (see DragPoll below).
  void OnButtonStateChanged(CefRefPtr<CefButton> button) override;

  // ---- CefMenuButtonDelegate ----
  // The OpenNyx logo (top-left) and the downloads button are menu buttons that
  // pop a CefMenuModel below themselves.
  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override;

  // ---- CefMenuModelDelegate ----
  void ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                      int command_id,
                      cef_event_flags_t event_flags) override;

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
  // Overrides the CEF/Chromium theme colors used by our views (notably the
  // address-bar textfield) so text is readable in our custom dark theme.
  void ApplyTheme();
  void AddAccelerators();

  CefRefPtr<CefBrowserView> ActiveBrowserView();
  CefRefPtr<CefBrowser> ActiveBrowser();
  int FindTabIndex(CefRefPtr<CefBrowserView> view);
  void CloseTabAt(size_t index);
  // Destroys the tab identified by |tab_id| on a FRESH stack (posted to the UI
  // thread) so it never runs re-entrantly inside the button click handler.
  void DestroyTabById(int tab_id);
  void RemoveTabAt(size_t index);
  // Issues the real top-level window close exactly once (idempotent). Called
  // when the last tab's browser has finished closing.
  void MaybeCloseWindow();
  void UpdateTabStrip();
  // Moves the tab at |from| to |to| (both tab-vector indices), keeping the
  // strip child order and active_tab_ in sync. Used by drag-reorder and the
  // Ctrl+Shift+PgUp/PgDn accelerators.
  void MoveTab(size_t from, size_t to);
  // Repeating UI-thread poll that implements pointer drag-to-reorder: while
  // the mouse button is held on a tab it tracks the cursor and reorders the
  // tab under it. |seq| invalidates stale polls after a drag ends.
  void DragPoll(int seq);
  void EndTabDrag();
  void UpdateWindowTitle();
  // Sets the address bar text with a reliable repaint. Clearing an unfocused
  // CefTextfield to empty via SetText("") does not always repaint (the old
  // text visually sticks); routing an empty target through select-all+delete
  // forces a user-triggered edit that repaints.
  void SetAddressBarText(const std::string& text);
  // Marks the empty caption area (drag spacer) as an OS-draggable region so a
  // frameless window can still be moved by dragging the top bar.
  void UpdateDraggableRegions();
  void FocusAddressBar();
  void NavigateActiveTab(const std::string& input);

  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> tab_strip_;
  CefRefPtr<CefPanel> toolbar_;
  CefRefPtr<CefLabelButton> back_button_;
  CefRefPtr<CefLabelButton> forward_button_;
  CefRefPtr<CefLabelButton> reload_button_;
  CefRefPtr<CefLabelButton> home_button_;
  CefRefPtr<CefLabelButton> new_tab_button_;
  CefRefPtr<CefTextfield> address_bar_;
  CefRefPtr<CefLabelButton> star_button_;    // bookmark toggle.
  CefRefPtr<CefLabelButton> shield_button_;  // blocked-request count.
  CefRefPtr<CefMenuButton> app_menu_button_;   // top-left OpenNyx logo menu.
  CefRefPtr<CefMenuButton> downloads_button_;  // toolbar downloads menu.
  // Snapshot of downloads taken when the downloads dropdown is opened, so menu
  // command ids can map back to a specific file (newest-last order).
  std::vector<DownloadEntry> downloads_menu_snapshot_;

  // Applies a zoom step to the active tab. delta = +1 (in), -1 (out), 0 (reset
  // to 100%). Zoom uses CEF's logarithmic level: each step is ~1.2x.
  void ZoomActiveTab(int delta);

  // Persists the current open-tab URLs + active index so they can be restored
  // on next launch. Cheap; called whenever tabs change.
  void SaveSessionState();
  // Frameless window controls.
  CefRefPtr<CefPanel> caption_spacer_;  // flexible drag area
  CefRefPtr<CefLabelButton> minimize_button_;
  CefRefPtr<CefLabelButton> maximize_button_;
  CefRefPtr<CefLabelButton> close_window_button_;
  bool is_maximized_ = false;
  // When true, the next OnAfterUserAction skips inline autocomplete (set after
  // Backspace/Delete so deleting characters doesn't immediately re-suggest).
  bool suppress_autocomplete_ = false;
  // Re-entrancy guard: our own SetText()/SelectRange() inside the autocomplete
  // handler would recursively fire OnAfterUserAction.
  bool in_autocomplete_ = false;

  // Updates the star (filled/hollow) + shield count from the active tab URL.
  void UpdateChrome();
  // Toggles the bookmark state of the active tab.
  void ToggleBookmarkActiveTab();

  std::vector<Tab> tabs_;
  size_t active_tab_ = 0;
  int next_tab_id_ = 1;
  // ---- Drag-to-reorder state ----
  int drag_tab_id_ = -1;    // id of the tab being pressed/dragged, -1 = none.
  int drag_seq_ = 0;        // invalidates queued DragPoll tasks.
  bool drag_started_ = false;  // true once the cursor passed the threshold.
  int drag_press_x_ = 0;    // cursor x (DIP screen) at press time.
  // Set once we have issued the real window_->Close() so it is never issued
  // twice (guards re-entrant / double window teardown).
  bool window_close_issued_ = false;
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
