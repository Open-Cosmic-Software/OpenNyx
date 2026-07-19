// Copyright (c) 2026 Open Cosmic Software. MIT licensed.

#include "scheme_handler.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "include/cef_parser.h"
#include "include/cef_request.h"
#include "include/cef_resource_handler.h"
#include "include/cef_response.h"
#include "include/wrapper/cef_helpers.h"

#include "blocklist.h"
#include "browser_window.h"
#include "pages.h"
#include "store.h"

#include "third_party/json/json.hpp"

using nlohmann::json;

const char kOpenNyxScheme[] = "opennyx";

namespace {

// Splits "opennyx://host/path?query" giving us host + path + query. For our
// scheme the "host" is the page name (newtab/history/...) or "api".
struct ParsedURL {
  std::string host;
  std::string path;   // everything after host, no leading slash.
  std::string query;  // raw query string (no '?').
};

ParsedURL ParseURL(const std::string& url) {
  ParsedURL out;
  std::string rest = url;
  const std::string prefix = "opennyx://";
  if (rest.compare(0, prefix.size(), prefix) == 0) {
    rest = rest.substr(prefix.size());
  }
  // query
  const size_t q = rest.find('?');
  if (q != std::string::npos) {
    out.query = rest.substr(q + 1);
    rest = rest.substr(0, q);
  }
  // fragment
  const size_t h = rest.find('#');
  if (h != std::string::npos) {
    rest = rest.substr(0, h);
  }
  // host / path
  const size_t slash = rest.find('/');
  if (slash == std::string::npos) {
    out.host = rest;
  } else {
    out.host = rest.substr(0, slash);
    out.path = rest.substr(slash + 1);
  }
  // strip trailing slash on path
  while (!out.path.empty() && out.path.back() == '/') {
    out.path.pop_back();
  }
  return out;
}

std::string GetQueryParam(const std::string& query, const std::string& key) {
  size_t pos = 0;
  while (pos < query.size()) {
    size_t amp = query.find('&', pos);
    std::string pair =
        query.substr(pos, amp == std::string::npos ? std::string::npos
                                                    : amp - pos);
    const size_t eq = pair.find('=');
    std::string k = eq == std::string::npos ? pair : pair.substr(0, eq);
    if (k == key) {
      std::string v = eq == std::string::npos ? "" : pair.substr(eq + 1);
      return CefURIDecode(v, true,
                          static_cast<cef_uri_unescape_rule_t>(
                              UU_SPACES |
                              UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS))
          .ToString();
    }
    if (amp == std::string::npos) {
      break;
    }
    pos = amp + 1;
  }
  return std::string();
}

// A minimal in-memory resource handler that returns a fixed byte buffer.
class BufferResourceHandler : public CefResourceHandler {
 public:
  BufferResourceHandler(const std::string& data, const std::string& mime)
      : data_(data), mime_(mime) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    response->SetMimeType(mime_);
    response->SetStatus(200);
    response->SetHeaderByName("Cache-Control", "no-store", true);
    response->SetHeaderByName("Access-Control-Allow-Origin", "*", true);
    response_length = static_cast<int64_t>(data_.size());
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    bytes_read = 0;
    if (offset_ >= data_.size()) {
      return false;
    }
    const size_t remaining = data_.size() - offset_;
    const size_t n =
        std::min(static_cast<size_t>(bytes_to_read), remaining);
    memcpy(data_out, data_.data() + offset_, n);
    offset_ += n;
    bytes_read = static_cast<int>(n);
    return true;
  }

  void Cancel() override {}

 private:
  std::string data_;
  std::string mime_;
  size_t offset_ = 0;
  IMPLEMENT_REFCOUNTING(BufferResourceHandler);
};

// Reads the POST body (bytes) from a request, if any.
std::string ReadPostBody(CefRefPtr<CefRequest> request) {
  CefRefPtr<CefPostData> post = request->GetPostData();
  if (!post || post->GetElementCount() == 0) {
    return std::string();
  }
  CefPostData::ElementVector elements;
  post->GetElements(elements);
  std::string body;
  for (const auto& el : elements) {
    if (el->GetType() == PDE_TYPE_BYTES) {
      const size_t count = el->GetBytesCount();
      if (count > 0) {
        std::string chunk(count, '\0');
        el->GetBytes(count, &chunk[0]);
        body += chunk;
      }
    }
  }
  return body;
}

// Builds a JSON resource handler.
CefRefPtr<CefResourceHandler> JsonResponse(const json& j) {
  return new BufferResourceHandler(j.dump(), "application/json");
}

// ---- API dispatch (runs on the IO thread; store is thread-safe) ----
CefRefPtr<CefResourceHandler> HandleApi(const std::string& endpoint,
                                        const std::string& query,
                                        const std::string& method,
                                        const std::string& body) {
  OpenNyxStore* store = OpenNyxStore::Get();

  if (endpoint == "config") {
    if (method == "POST") {
      json j = json::parse(body, nullptr, false);
      AppConfig c = store->GetConfig();
      if (j.is_object()) {
        c.search_engine = j.value("search_engine", c.search_engine);
        c.custom_search_url = j.value("custom_search_url", c.custom_search_url);
        c.homepage = j.value("homepage", c.homepage);
        c.blocking_enabled = j.value("blocking_enabled", c.blocking_enabled);
        c.doh_enabled = j.value("doh_enabled", c.doh_enabled);
        c.doh_resolver = j.value("doh_resolver", c.doh_resolver);
        c.doh_custom_template =
            j.value("doh_custom_template", c.doh_custom_template);
        store->SetConfig(c);
        OpenNyxBlocklist::Get()->SetEnabled(c.blocking_enabled);
      }
    }
    AppConfig c = store->GetConfig();
    json j;
    j["search_engine"] = c.search_engine;
    j["custom_search_url"] = c.custom_search_url;
    j["homepage"] = c.homepage;
    j["blocking_enabled"] = c.blocking_enabled;
    j["doh_enabled"] = c.doh_enabled;
    j["doh_resolver"] = c.doh_resolver;
    j["doh_custom_template"] = c.doh_custom_template;
    return JsonResponse(j);
  }

  if (endpoint == "resolve") {
    const std::string q = GetQueryParam(query, "q");
    json j;
    j["url"] = ResolveAddressInput(q);
    return JsonResponse(j);
  }

  if (endpoint == "history") {
    const std::string q = GetQueryParam(query, "q");
    size_t limit = 300;
    const std::string ls = GetQueryParam(query, "limit");
    if (!ls.empty()) {
      limit = static_cast<size_t>(std::max(0, atoi(ls.c_str())));
      if (limit == 0) limit = 300;
    }
    auto items = store->QueryHistory(q, limit);
    json arr = json::array();
    for (const auto& h : items) {
      arr.push_back({{"url", h.url}, {"title", h.title}, {"ts", h.ts}});
    }
    json j;
    j["items"] = arr;
    return JsonResponse(j);
  }

  if (endpoint == "history/clear" && method == "POST") {
    store->ClearHistory();
    return JsonResponse(json{{"ok", true}});
  }

  if (endpoint == "bookmarks") {
    auto items = store->GetBookmarks();
    json arr = json::array();
    for (const auto& b : items) {
      arr.push_back({{"url", b.url}, {"title", b.title}, {"ts", b.ts}});
    }
    return JsonResponse(json{{"items", arr}});
  }

  if (endpoint == "bookmarks/remove" && method == "POST") {
    json j = json::parse(body, nullptr, false);
    if (j.is_object()) {
      const std::string url = j.value("url", "");
      store->RemoveBookmark(url);
    }
    return JsonResponse(json{{"ok", true}});
  }

  if (endpoint == "downloads") {
    auto items = store->GetDownloads();
    json arr = json::array();
    for (const auto& d : items) {
      arr.push_back({{"id", d.id},
                     {"url", d.url},
                     {"filename", d.filename},
                     {"full_path", d.full_path},
                     {"total_bytes", d.total_bytes},
                     {"received_bytes", d.received_bytes},
                     {"percent", d.percent},
                     {"complete", d.complete},
                     {"canceled", d.canceled},
                     {"ts", d.ts}});
    }
    return JsonResponse(json{{"items", arr}});
  }

  if (endpoint == "downloads/clear" && method == "POST") {
    store->ClearDownloads();
    return JsonResponse(json{{"ok", true}});
  }

  if (endpoint == "cleardata" && method == "POST") {
    store->ClearBrowsingData();
    // Also ask the browser layer to clear cookies/cache.
    BrowserWindow::RequestClearBrowsingData();
    return JsonResponse(json{{"ok", true}});
  }

  if (endpoint == "blockstats") {
    OpenNyxBlocklist* bl = OpenNyxBlocklist::Get();
    json j;
    j["list_size"] = static_cast<uint64_t>(bl->size());
    j["total_blocked"] = bl->total_blocked();
    j["enabled"] = bl->enabled();
    return JsonResponse(j);
  }

  return JsonResponse(json{{"error", "unknown endpoint"}});
}

// The scheme handler factory: creates a resource handler per opennyx:// request.
class OpenNyxSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    const std::string url = request->GetURL().ToString();
    const std::string method = request->GetMethod().ToString();
    ParsedURL p = ParseURL(url);

    if (p.host == "api") {
      const std::string body =
          (method == "POST") ? ReadPostBody(request) : std::string();
      return HandleApi(p.path, p.query, method, body);
    }

    // Otherwise serve a page (default newtab).
    std::string page = p.host.empty() ? "newtab" : p.host;
    static const char* kPages[] = {"newtab", "history", "bookmarks",
                                   "downloads", "settings"};
    bool known = false;
    for (const char* k : kPages) {
      if (page == k) {
        known = true;
        break;
      }
    }
    if (!known) {
      page = "newtab";
    }
    return new BufferResourceHandler(GetOpenNyxPageHTML(page), "text/html");
  }

 private:
  IMPLEMENT_REFCOUNTING(OpenNyxSchemeHandlerFactory);
};

}  // namespace

void RegisterOpenNyxCustomScheme(CefRawPtr<CefSchemeRegistrar> registrar) {
  // Standard + secure + local + CORS so pages can fetch() the api endpoints
  // and are treated as a trustworthy origin (needed for a privileged UI).
  registrar->AddCustomScheme(
      kOpenNyxScheme,
      CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
          CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED);
}

void RegisterOpenNyxSchemeHandlerFactory() {
  CefRegisterSchemeHandlerFactory(kOpenNyxScheme, /*domain_name=*/CefString(),
                                  new OpenNyxSchemeHandlerFactory());
}
