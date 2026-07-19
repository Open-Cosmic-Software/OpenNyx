// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
// Portions based on the CEF cefsimple sample (BSD licensed,
// Copyright (c) 2013 The Chromium Embedded Framework Authors).

#include "opennyx_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include "blocklist.h"
#include "browser_window.h"
#include "opennyx_client.h"
#include "scheme_handler.h"
#include "store.h"

namespace {

// Maps a resolver id to its RFC 8484 DoH URI template. Non-US resolvers are
// offered first (Quad9 = Switzerland, Mullvad = Sweden); Cloudflare is the
// default because it is fast and widely reachable.
std::string DohTemplateFor(const AppConfig& cfg) {
  if (cfg.doh_resolver == "quad9") {
    return "https://dns.quad9.net/dns-query";
  }
  if (cfg.doh_resolver == "mullvad") {
    return "https://dns.mullvad.net/dns-query";
  }
  if (cfg.doh_resolver == "custom" && !cfg.doh_custom_template.empty()) {
    return cfg.doh_custom_template;
  }
  // Default: Cloudflare.
  return "https://cloudflare-dns.com/dns-query";
}

}  // namespace

OpenNyxApp::OpenNyxApp() = default;

void OpenNyxApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  // Privacy hardening: applied to the browser process (and inherited by all
  // sub-processes). CEF already ships without Google API keys; these switches
  // remove the remaining phone-home surface.
  if (process_type.empty()) {
    // No metrics/UMA reporting.
    command_line->AppendSwitch("metrics-recording-only");
    // No crash reporting to Google.
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-crash-reporter");
    // No background talk-to-Google services.
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitch("disable-domain-reliability");
    command_line->AppendSwitch("disable-sync");
    command_line->AppendSwitch("disable-client-side-phishing-detection");
    // No hyperlink auditing pings.
    command_line->AppendSwitch("no-pings");
    // Skip first-run and default-browser nagging.
    command_line->AppendSwitch("no-first-run");
    command_line->AppendSwitch("no-default-browser-check");
    // Note: the user agent is deliberately left at the stock Chromium value.
    // A custom UA would only add fingerprinting surface.

    // --- M4: DNS-over-HTTPS (secure mode) ---
    // Encrypt DNS lookups by default. "secure" mode uses ONLY the configured
    // DoH resolver (no plaintext fallback). The resolver is user-selectable
    // in Settings; the choice is applied at startup here.
    const AppConfig cfg = OpenNyxStore::Get()->GetConfig();
    if (cfg.doh_enabled) {
      command_line->AppendSwitchWithValue("dns-over-https-mode", "secure");
      command_line->AppendSwitchWithValue("dns-over-https-templates",
                                          DohTemplateFor(cfg));
    }

    // Keep the blocklist toggle in sync with persisted config at startup.
    // (The actual blocking happens in OpenNyxClient's resource handler.)
  }
}

void OpenNyxApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  RegisterOpenNyxCustomScheme(registrar);
}

void OpenNyxApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  // Register the opennyx:// scheme handler factory (privileged UI pages +
  // the JSON bridge). Must happen after CefInitialize.
  RegisterOpenNyxSchemeHandlerFactory();

  // Sync the runtime blocklist toggle with persisted settings.
  OpenNyxBlocklist::Get()->SetEnabled(
      OpenNyxStore::Get()->GetConfig().blocking_enabled);

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // OpenNyxClient implements browser-level callbacks shared by all tabs.
  // The instance is retrieved via OpenNyxClient::GetInstance() when tabs
  // are created.
  CefRefPtr<OpenNyxClient> client(new OpenNyxClient());

  // Allow overriding the start page with --url=... for testing; the default
  // is the built-in dark new-tab page with a Brave Search box.
  const std::string url = command_line->GetSwitchValue("url");

  // Create the main browser window (tab strip + toolbar + first tab) using
  // the CEF Views framework. It shows itself after creation.
  BrowserWindow::Create(url);
}

CefRefPtr<CefClient> OpenNyxApp::GetDefaultClient() {
  // Called when a new browser window is created via the Chrome runtime style
  // UI (e.g. Ctrl+N, middle-click link -> new window).
  return OpenNyxClient::GetInstance();
}
