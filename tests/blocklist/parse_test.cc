// Host-only test for OpenNyxBlocklist (no CEF needed).
// g++ -std=c++17 -Ishell/src tests/blocklist/parse_test.cc shell/src/blocklist.cc -o /tmp/bltest
// /tmp/bltest [path/to/easyprivacy.txt]
#include <cstdio>
#include <fstream>
#include <sstream>
#include "blocklist.h"

static int fail = 0;
static void check(const char* name, bool ok) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", name);
  if (!ok) ++fail;
}

int main(int argc, char** argv) {
  std::unordered_set<std::string> b, a;
  OpenNyxBlocklist::ParseAbpHosts(
      "! comment\n||tracker.example^\n||third.example^$third-party\n"
      "||withpath.example/pixel.gif\n||opts.example^$script,domain=x.com\n"
      "||wild*.example^\n@@||allowed.example^$script\n||allowed.example^\n"
      "||UPPER.Example^\r\n##.ad-banner\n|https://x.example^\n||nodot^\n",
      &b, &a);
  check("plain ||host^ is a block rule", b.count("tracker.example") == 1);
  check("||host^$third-party is a block rule", b.count("third.example") == 1);
  check("path rule is skipped", b.count("withpath.example") == 0);
  check("rule with other options is skipped", b.count("opts.example") == 0);
  check("wildcard rule is skipped", b.count("wild*.example") == 0);
  check("@@ exception is collected", a.count("allowed.example") == 1);
  check("upper case + CRLF are normalised", b.count("upper.example") == 1);
  check("host without dot is skipped", b.count("nodot") == 0);

  OpenNyxBlocklist* bl = OpenNyxBlocklist::Get();
  const size_t before = bl->size();
  check("EFF test tracker NOT blocked by bundled list alone",
        !bl->ShouldBlock("trackersimulator.org", "coveryourtracks.eff.org"));
  if (argc > 1) {
    const int added = bl->LoadListFile(argv[1]);
    std::printf("     loaded %d hosts (%zu -> %zu)\n", added, before, bl->size());
    check("list file loads many hosts", added > 10000);
    check("blocks trackersimulator.org",
          bl->ShouldBlock("trackersimulator.org", "coveryourtracks.eff.org"));
    check("blocks eviltracker.net",
          bl->ShouldBlock("eviltracker.net", "coveryourtracks.eff.org"));
    check("blocks subdomain of a listed host",
          bl->ShouldBlock("a.b.trackersimulator.org", "coveryourtracks.eff.org"));
    const char* keep[] = {"cdn.jsdelivr.net", "cdnjs.cloudflare.com",
                          "fonts.googleapis.com", "www.gstatic.com",
                          "www.youtube.com", "github.com", "js.stripe.com",
                          "cdn.shopify.com", "search.brave.com",
                          "coveryourtracks.eff.org", "www.recaptcha.net",
                          "dns.quad9.net", "shellgames.ai", "store.steampowered.com"};
    bool none = true;
    for (const char* h : keep) {
      if (bl->ShouldBlock(h, "example.org")) { std::printf("     over-blocks %s\n", h); none = false; }
    }
    check("common CDNs / sites stay reachable", none);
    check("first party is never blocked",
          !bl->ShouldBlock("trackersimulator.org", "www.trackersimulator.org"));
  }
  std::printf(fail ? "\n%d failed\n" : "\nall ok\n", fail);
  return fail ? 1 : 0;
}
