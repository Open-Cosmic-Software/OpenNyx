// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// The opennyx:// custom scheme. Two responsibilities:
//   1. Serve the privileged UI pages (opennyx://newtab, /history, /bookmarks,
//      /downloads, /settings) as embedded HTML.
//   2. Act as the browser<->page bridge via a small JSON API under
//      opennyx://api/* (GET for reads, POST for mutations). This avoids the
//      complexity of a separate render-process message router: the pages just
//      fetch() these endpoints.
//
// Register with RegisterOpenNyxScheme() (call once after CefInitialize) and
// declare the scheme in CefApp::OnRegisterCustomSchemes().

#ifndef OPENNYX_SHELL_SRC_SCHEME_HANDLER_H_
#define OPENNYX_SHELL_SRC_SCHEME_HANDLER_H_

#include "include/cef_scheme.h"

// The custom scheme name. Pages live at opennyx://<page>, API at
// opennyx://api/<endpoint>.
extern const char kOpenNyxScheme[];

// Called from CefApp::OnRegisterCustomSchemes in every process.
void RegisterOpenNyxCustomScheme(CefRawPtr<CefSchemeRegistrar> registrar);

// Called once in the browser process after CefInitialize succeeds.
void RegisterOpenNyxSchemeHandlerFactory();

#endif  // OPENNYX_SHELL_SRC_SCHEME_HANDLER_H_
