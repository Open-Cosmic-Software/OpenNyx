// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// Embedded HTML for the privileged opennyx:// pages (newtab, history,
// bookmarks, downloads, settings). Each page is a self-contained dark-themed
// document that talks to the opennyx://api/* JSON endpoints via fetch().

#ifndef OPENNYX_SHELL_SRC_PAGES_H_
#define OPENNYX_SHELL_SRC_PAGES_H_

#include <string>

// Returns the HTML body for the given page path ("newtab", "history",
// "bookmarks", "downloads", "settings"). Unknown paths return an index page.
std::string GetOpenNyxPageHTML(const std::string& page);

// Shared CSS + JS helpers injected into every page.
std::string GetSharedChrome();

#endif  // OPENNYX_SHELL_SRC_PAGES_H_
