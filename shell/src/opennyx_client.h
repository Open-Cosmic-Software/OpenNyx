// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#ifndef OPENNYX_SHELL_SRC_OPENNYX_CLIENT_H_
#define OPENNYX_SHELL_SRC_OPENNYX_CLIENT_H_

#include <list>

#include "include/cef_client.h"

// Browser-level callbacks shared by all OpenNyx browsers (i.e. all tabs and
// windows). With the Chrome runtime style, Chromium itself owns the tab strip
// and toolbar UI; this client tracks browser lifetimes so the application can
// exit cleanly when the last window closes.
class OpenNyxClient : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  OpenNyxClient();
  ~OpenNyxClient() override;

  // Access to the single global instance of this object.
  static OpenNyxClient* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override;
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  bool IsClosing() const { return is_closing_; }

 private:
  // List of existing browsers. Only accessed on the CEF UI thread.
  using BrowserList = std::list<CefRefPtr<CefBrowser>>;
  BrowserList browser_list_;

  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(OpenNyxClient);
};

#endif  // OPENNYX_SHELL_SRC_OPENNYX_CLIENT_H_
