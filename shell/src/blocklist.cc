// Copyright (c) 2026 Open Cosmic Software. MIT licensed.
//
// Bundled starter blocklist. This is a curated subset (~250 domains) of the
// most prevalent advertising / tracking / telemetry hosts, drawn from the
// public-domain and open blocklists:
//   * Peter Lowe's ad/tracking server list (pgl.yoyo.org) — CC-BY 4.0
//   * StevenBlack/hosts (unified hosts) — MIT
//   * EasyList / EasyPrivacy well-known domains — GPLv3 / CC-BY-SA
// We embed a compact curated subset (not the full multi-hundred-thousand-line
// lists) to keep the binary lean; the list is loaded once into a hash set.
// Matching is domain-suffix based (see ShouldBlock).

#include "blocklist.h"

#include <algorithm>

namespace {

// Curated bundled blocklist. Kept intentionally compact and high-signal:
// the highest-volume ad/analytics/tracking domains that appear on the vast
// majority of pages. Grouped by vendor for readability.
const char* const kBundledDomains[] = {
    // --- Google advertising / analytics ---
    "doubleclick.net", "googlesyndication.com", "googleadservices.com",
    "google-analytics.com", "googletagmanager.com", "googletagservices.com",
    "adservice.google.com", "pagead2.googlesyndication.com",
    "partner.googleadservices.com", "www.google-analytics.com",
    "ssl.google-analytics.com", "analytics.google.com", "adsense.google.com",
    "2mdn.net", "app-measurement.com", "crashlytics.com",
    "firebase-analytics.com", "admob.com",
    // --- Facebook / Meta ---
    "connect.facebook.net", "an.facebook.com", "graph.facebook.com",
    "pixel.facebook.com", "analytics.facebook.com", "business.facebook.com",
    "atdmt.com", "fbcdn-photos-a.akamaihd.net",
    // --- Amazon advertising ---
    "amazon-adsystem.com", "aax.amazon-adsystem.com",
    "assoc-amazon.com", "adtago.s3.amazonaws.com",
    // --- Microsoft / Bing / telemetry ---
    "bat.bing.com", "c.bing.com", "clarity.ms", "www.clarity.ms",
    "vortex.data.microsoft.com", "watson.telemetry.microsoft.com",
    "telemetry.microsoft.com", "browser.pipe.aria.microsoft.com",
    // --- Major analytics / tag managers ---
    "scorecardresearch.com", "sb.scorecardresearch.com",
    "quantserve.com", "quantcount.com", "chartbeat.com", "static.chartbeat.com",
    "hotjar.com", "static.hotjar.com", "script.hotjar.com",
    "mixpanel.com", "api.mixpanel.com", "segment.com", "api.segment.io",
    "cdn.segment.com", "amplitude.com", "api.amplitude.com",
    "fullstory.com", "rs.fullstory.com", "heap.io", "cdn.heapanalytics.com",
    "mouseflow.com", "crazyegg.com", "script.crazyegg.com", "kissmetrics.com",
    "loggly.com", "newrelic.com", "js-agent.newrelic.com", "nr-data.net",
    "bam.nr-data.net", "sentry.io", "browser.sentry-cdn.com",
    "matomo.cloud", "stats.wp.com", "pixel.wp.com",
    "cdn.mxpnl.com", "woopra.com", "statcounter.com", "c.statcounter.com",
    "clicktale.net", "cdn.clicktale.net", "inspectlet.com", "cdn.inspectlet.com",
    "logrocket.com", "cdn.logrocket.io", "smartlook.com",
    "yandex.ru/metrika", "mc.yandex.ru", "an.yandex.ru", "metrica.yandex.com",
    // --- Ad exchanges / SSPs / DSPs ---
    "adnxs.com", "ib.adnxs.com", "secure.adnxs.com", "rubiconproject.com",
    "fastlane.rubiconproject.com", "pubmatic.com", "ads.pubmatic.com",
    "openx.net", "us-u.openx.net", "casalemedia.com", "as.casalemedia.com",
    "criteo.com", "static.criteo.net", "bidder.criteo.com", "criteo.net",
    "taboola.com", "cdn.taboola.com", "trc.taboola.com", "outbrain.com",
    "widgets.outbrain.com", "mgid.com", "servicer.mgid.com", "revcontent.com",
    "trends.revcontent.com", "adroll.com", "d.adroll.com", "s.adroll.com",
    "media.net", "contextual.media.net", "sharethrough.com", "smartadserver.com",
    "adform.net", "track.adform.net", "adform.com", "teads.tv", "t.teads.tv",
    "spotxchange.com", "spotx.tv", "indexww.com", "gumgum.com",
    "yieldmo.com", "sonobi.com", "33across.com", "districtm.io",
    "adcolony.com", "applovin.com", "unityads.unity3d.com", "vungle.com",
    "inmobi.com", "chartboost.com", "moatads.com", "z.moatads.com",
    "adsymptotic.com", "bluekai.com", "tags.bluekai.com", "rlcdn.com",
    "agkn.com", "eyeota.net", "demdex.net", "dpm.demdex.net", "everesttech.net",
    "omtrdc.net", "2o7.net", "adobedtm.com", "assets.adobedtm.com",
    "sc.omtrdc.net", "nexac.com", "tapad.com", "crwdcntrl.net",
    "tags.crwdcntrl.net", "mathtag.com", "pixel.mathtag.com", "adsrvr.org",
    "match.adsrvr.org", "turn.com", "rfihub.com", "simpli.fi", "adhigh.net",
    "lijit.com", "ad.lijit.com", "onetag-sys.com", "3lift.com",
    "smadex.com", "loopme.com", "smaato.net", "mopub.com",
    // --- Social / share widgets that track ---
    "addthis.com", "s7.addthis.com", "m.addthis.com", "sharethis.com",
    "platform-api.sharethis.com", "disqus.com/embed", "referrer.disqus.com",
    "widget.intercom.io", "js.intercomcdn.com", "static.ads-twitter.com",
    "analytics.twitter.com", "ads-twitter.com", "t.co/i/adsct",
    "px.ads.linkedin.com", "snap.licdn.com", "ads.linkedin.com",
    "analytics.tiktok.com", "ads-api.tiktok.com", "business-api.tiktok.com",
    "ads.pinterest.com", "log.pinterest.com", "ct.pinterest.com",
    // --- Consent / misc trackers ---
    "cookielaw.org", "cdn.cookielaw.org", "onetrust.com", "cdn.onetrust.com",
    "consensu.org", "quantcast.mgr.consensu.org", "usercentrics.eu",
    "app.usercentrics.eu", "trustarc.com", "consent.trustarc.com",
    "branch.io", "app.link", "api2.branch.io", "adjust.com", "app.adjust.com",
    "appsflyer.com", "t.appsflyer.com", "kochava.com", "control.kochava.com",
    "tenjin.io", "singular.net", "attribution.report",
    "onesignal.com", "cdn.onesignal.com", "pushcrew.com", "pushwoosh.com",
    // --- Video / misc ad networks ---
    "innovid.com", "serving-sys.com", "flashtalking.com", "pixel.adsafeprotected.com",
    "adsafeprotected.com", "static.adsafeprotected.com", "doubleverify.com",
    "cdn.doubleverify.com", "ad-score.com", "iasds01.com", "adsco.re",
    "quantummetric.com", "cdn.quantummetric.com", "clicky.com",
    "static.getclicky.com", "in.getclicky.com", "parsely.com", "cdn.parsely.com",
    "keywee.co", "permutive.com", "cdn.permutive.com", "lockerdome.com",
    "cdn2.lockerdome.com", "zergnet.com", "content.zergnet.com",
    "ad.doubleclick.net", "static.doubleclick.net", "stats.g.doubleclick.net",
    "cm.g.doubleclick.net", "securepubads.g.doubleclick.net",
    "tpc.googlesyndication.com", "cse.google.com/adsense",
    "bounceexchange.com", "api.bounceexchange.com", "yotpo.com",
    "cdn-widgetsrepository.yotpo.com", "trustpilot.com/businessunit",
    // --- Additional prevalent trackers ---
    "nr-data.net", "insightexpressai.com", "adotmob.com", "id5-sync.com",
    "cdn.id5-sync.com", "cadmus.script.ac", "geoedge.be", "adhese.com",
    "browser-intake-datadoghq.com", "rum.browser-intake-datadoghq.com",
    "cdn.mouseflow.com", "cdn.jsdelivr.net/npm/@analytics",
    "wcfbc.net", "d.agkn.com", "sync.mathtag.com", "eb2.3lift.com",
    "cnt.vibabpanel.com", "trackcmp.net", "count.im", "traffic.libsyn.com",
    "cdn.cxense.com", "cxense.com", "cdn.krxd.net", "beacon.krxd.net",
    "krxd.net", "cdn.tapfiliate.com", "log.outbrain.com",
    "px.moatads.com", "geo.moatads.com", "hblg.tapad.com",
    "aa.agkn.com", "us-east-1.online-metrix.net", "online-metrix.net",
    "h.online-metrix.net", "cdn.bizible.com", "bizible.com", "www.bizible.com",
    "munchkin.marketo.net", "marketo.net", "marketo.com", "mktoresp.com",
    "pardot.com", "pi.pardot.com", "cdn.pardot.com", "hsubspot.com",
    "js.hs-analytics.net", "js.hs-scripts.com", "track.hubspot.com",
    "forms.hsforms.com", "js.hscollectedforms.net", "api.hubspot.com/reports",
    "px.owneriq.net", "ad.360yield.com", "match.prod.bidr.io",
    "sync.1rx.io", "u.4dex.io", "ads.yahoo.com", "analytics.yahoo.com",
    "sp.analytics.yahoo.com", "geo.yahoo.com", "log.fc.yahoo.com",
    "beap.gemini.yahoo.com", "udc.yahoo.com", "gemini.yahoo.com",
};

std::string ToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(::tolower(c)); });
  return out;
}

// Returns the registrable-ish domain: last two labels (best-effort, no PSL).
std::string LastTwoLabels(const std::string& host) {
  size_t last_dot = host.rfind('.');
  if (last_dot == std::string::npos || last_dot == 0) {
    return host;
  }
  size_t prev_dot = host.rfind('.', last_dot - 1);
  if (prev_dot == std::string::npos) {
    return host;
  }
  return host.substr(prev_dot + 1);
}

}  // namespace

// static
OpenNyxBlocklist* OpenNyxBlocklist::Get() {
  static OpenNyxBlocklist instance;
  return &instance;
}

OpenNyxBlocklist::OpenNyxBlocklist() {
  Load();
}

void OpenNyxBlocklist::Load() {
  for (const char* d : kBundledDomains) {
    std::string entry = d;
    // Some curated entries include a path segment (e.g. "t.co/i/adsct");
    // reduce to the host part for the domain set. Path-based rules are only
    // approximated at the host level for this compact list.
    const size_t slash = entry.find('/');
    if (slash != std::string::npos) {
      entry = entry.substr(0, slash);
    }
    if (!entry.empty()) {
      domains_.insert(ToLower(entry));
    }
  }
}

bool OpenNyxBlocklist::ShouldBlock(const std::string& host_in,
                                   const std::string& first_party_host) {
  if (!enabled_.load()) {
    return false;
  }
  if (host_in.empty()) {
    return false;
  }
  const std::string host = ToLower(host_in);

  // First-party exception: never block requests to the same registrable
  // domain as the page the user is on.
  if (!first_party_host.empty()) {
    const std::string fp = ToLower(first_party_host);
    if (LastTwoLabels(host) == LastTwoLabels(fp)) {
      return false;
    }
  }

  // Domain-suffix match: check host and every parent domain.
  std::string h = host;
  while (!h.empty()) {
    if (domains_.find(h) != domains_.end()) {
      return true;
    }
    const size_t dot = h.find('.');
    if (dot == std::string::npos) {
      break;
    }
    h = h.substr(dot + 1);
    // Stop once we're down to a bare TLD (no dot left).
    if (h.find('.') == std::string::npos) {
      // Check the last-two-labels form once (e.g. "doubleclick.net").
      if (domains_.find(h) != domains_.end()) {
        return true;
      }
      break;
    }
  }
  return false;
}

void OpenNyxBlocklist::RecordBlock(const std::string& first_party_host) {
  total_blocked_.fetch_add(1);
  if (first_party_host.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(counts_mutex_);
  per_site_counts_[ToLower(first_party_host)]++;
}

int OpenNyxBlocklist::GetCount(const std::string& first_party_host) {
  std::lock_guard<std::mutex> lock(counts_mutex_);
  auto it = per_site_counts_.find(ToLower(first_party_host));
  return it == per_site_counts_.end() ? 0 : it->second;
}

void OpenNyxBlocklist::ResetCount(const std::string& first_party_host) {
  std::lock_guard<std::mutex> lock(counts_mutex_);
  per_site_counts_[ToLower(first_party_host)] = 0;
}
