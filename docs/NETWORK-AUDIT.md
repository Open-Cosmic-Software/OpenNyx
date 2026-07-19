# OpenNyx Network Audit — the trust proof

_Last updated: 2026-07-19_

OpenNyx claims to be **de-googled at runtime**: it is built on the standard
Chromium Embedded Framework (CEF) binary distribution, but launches Chromium
with a hardened command line that disables Google's telemetry, background
services and update pings, ships without Google API keys, blocks trackers, and
turns on encrypted DNS. This document explains **exactly how to verify that
claim yourself** by capturing every outbound network connection OpenNyx makes
on a clean start with a blank page — and lists the endpoints we *expect* to see
(and, importantly, which ones we do **not**).

> **Wording note.** We deliberately do **not** use the word "ungoogled".
> ungoogled-chromium is a source-patched *build*; OpenNyx ships stock CEF and
> removes Google's runtime phone-home surface via flags and blocking. The
> honest description is **"de-googled at runtime"**. See
> `docs/ENGINE-STRATEGY.md` for the roadmap toward a source-patched engine.

---

## What "clean start" means

A clean start is:

1. A fresh OpenNyx user-data directory (delete the `OpenNyx/` folder next to
   the executable, or the per-user data dir, so no cookies/history exist).
2. Launch OpenNyx.
3. Leave it on the built-in `opennyx://newtab` page. **Do not type anything,
   do not click any tile, do not load any web site.**
4. Wait ~30 seconds and quit.

Everything captured in that window is OpenNyx's *own* behaviour, not a website's.

---

## Method A — mitmproxy (recommended, sees domains + TLS SNI)

`mitmproxy` shows you the actual HTTP(S) requests (via a trusted MITM cert) or,
without installing the cert, at least the TLS SNI (destination hostnames).

### 1. Start mitmproxy

```bash
mitmproxy --listen-host 127.0.0.1 --listen-port 8080 \
  --set block_global=false -w opennyx-clean-start.flows
```

(Or `mitmdump` for a headless capture to the same `-w` file.)

### 2. Launch OpenNyx through the proxy

OpenNyx forwards Chromium's standard proxy flags, so:

```powershell
# Windows (PowerShell), from the unzipped artifact directory:
.\opennyx.exe --proxy-server="http://127.0.0.1:8080"
```

> ⚠️ With DoH in **secure** mode (our default), DNS resolution is tunnelled
> inside HTTPS to the configured resolver and will appear in the capture as a
> connection to that resolver (e.g. `cloudflare-dns.com`). That is expected and
> is one of *our* endpoints — see the table below. To audit the non-DoH
> destination hostnames more directly you can temporarily disable DoH in
> Settings, which makes SNI/hostnames for every other connection visible.

### 3. List every unique domain contacted

```bash
mitmdump -nr opennyx-clean-start.flows \
  | awk '{print $3}' | sed 's#https\?://##; s#/.*##' | sort -u
```

or interactively in `mitmproxy`, then export.

---

## Method B — pcap (Wireshark / tshark, no cert needed)

This sees IPs and TLS SNI without decrypting payloads.

```bash
# Capture (pick the right interface):
sudo tshark -i any -w opennyx-clean-start.pcap

# Launch OpenNyx, idle 30s on opennyx://newtab, quit, stop the capture.

# Extract TLS SNI hostnames actually contacted:
tshark -r opennyx-clean-start.pcap -Y "tls.handshake.extensions_server_name" \
  -T fields -e tls.handshake.extensions_server_name | sort -u

# And the raw destination IPs:
tshark -r opennyx-clean-start.pcap -T fields -e ip.dst | sort -u
```

On Windows use Wireshark (GUI) or `tshark.exe` the same way.

---

## Method C — Windows-native, no extra tools

```powershell
# Show live TCP connections owned by opennyx.exe while it runs:
Get-NetTCPConnection | Where-Object {
  (Get-Process -Id $_.OwningProcess -ErrorAction SilentlyContinue).Name -eq 'opennyx'
} | Select-Object RemoteAddress,RemotePort,State
```

Resolve the `RemoteAddress` values with `Resolve-DnsName` to get hostnames.

---

## Expected endpoints on a clean start

The table classifies every endpoint we expect. Categories:

- **OURS-INTENDED** — a connection OpenNyx makes on purpose, by our design.
- **NONE-EXPECTED** — Chromium subsystems that would normally phone home but
  which our flags disable; you should see **no** traffic here.
- **TO-BE-VERIFIED** — platform/OS behaviour outside OpenNyx's process that a
  careful auditor should attribute correctly (not counted against OpenNyx).

| Endpoint / host | Category | Why |
|---|---|---|
| The configured **DoH resolver** — default `cloudflare-dns.com`; or `dns.quad9.net` (CH), `dns.mullvad.net` (SE), or your custom template | **OURS-INTENDED** | Encrypted DNS (`--dns-over-https-mode=secure`). All name resolution is tunnelled here. Selectable/disable-able in Settings. |
| `search.brave.com` (or your chosen engine: `duckduckgo.com`, `mojeek.com`, custom) | **OURS-INTENDED — only after you search** | The default search engine. **Not contacted on a blank new tab**; only when you submit a query. Listed for completeness. |
| Nothing else on an idle `opennyx://newtab` | **OURS-INTENDED** | The new-tab page is served locally by the `opennyx://` scheme; it makes only local `opennyx://api/*` fetches (in-process, no network). |
| `clients2.google.com`, `update.googleapis.com`, `clients*.google.com` (component/CRX/omaha updates) | **NONE-EXPECTED** | Disabled via `--disable-component-update` + `--disable-background-networking`. |
| `www.google.com`, `google.com/complete` (search suggest / omnibox) | **NONE-EXPECTED** | No Google search integration; address bar uses Brave/your engine. No suggest calls on idle. |
| `safebrowsing.googleapis.com`, `sb-ssl.google.com` | **NONE-EXPECTED** | Safe Browsing is not driven (no Google API key; `--disable-client-side-phishing-detection`). |
| `accounts.google.com`, `oauth2.googleapis.com` | **NONE-EXPECTED** | No sign-in / sync (`--disable-sync`). |
| `mtalk.google.com`, `*.gcm.googleapis.com` (GCM/push) | **NONE-EXPECTED** | Disabled with background networking. |
| `clientservices.googleapis.com`, `optimizationguide-pa.googleapis.com`, field-trial / variations (`*-pa.googleapis.com`) | **NONE-EXPECTED** | Variations/field trials not fetched; background networking disabled. No Google API key present. |
| `ssl.gstatic.com`, `www.gstatic.com` (static assets, e.g. some UI) | **NONE-EXPECTED on idle** | Not needed by our local new-tab page. |
| `crashpad`/`clients2.google.com/cr` (crash reports) | **NONE-EXPECTED** | `--disable-breakpad --disable-crash-reporter`. |
| `dns.google` (Google's DoH endpoint) | **NONE-EXPECTED** | We never default to Google's resolver. |
| `*.doubleclick.net`, `google-analytics.com`, `googletagmanager.com`, and the other ~250 domains in the bundled blocklist | **NONE-EXPECTED (and actively blocked)** | Blocked by the request filter when a site tries to load them; on a blank page there is nothing to block. |
| OS-level telemetry (e.g. Windows `*.microsoft.com`, NCSI `www.msftconnecttest.com`) | **TO-BE-VERIFIED (not OpenNyx)** | These come from the operating system, not the `opennyx.exe` process. Attribute by process (Method C) before counting them. |
| Time sync (`time.windows.com`, NTP) | **TO-BE-VERIFIED (not OpenNyx)** | OS behaviour. |

### Bottom line

On a clean idle start with DoH **enabled**, the only connection you should see
from `opennyx.exe` is to the **DoH resolver**. With DoH **disabled**, you
should see plaintext DNS lookups but still **no** connection to any Google
telemetry/update/safe-browsing/variations endpoint. Any Google-owned endpoint
appearing from the `opennyx.exe` process would be a bug — please file an issue
with the capture.

---

## The flags that produce this behaviour

Set in `OpenNyxApp::OnBeforeCommandLineProcessing` (`shell/src/opennyx_app.cc`):

| Flag | Effect |
|---|---|
| `metrics-recording-only` | No UMA metrics upload. |
| `disable-breakpad`, `disable-crash-reporter` | No crash reporting to Google. |
| `disable-background-networking` | No background talk-to-Google services. |
| `disable-component-update` | No CRX/component/omaha updates. |
| `disable-domain-reliability` | No domain-reliability beacons. |
| `disable-sync` | No account sync. |
| `disable-client-side-phishing-detection` | No client-side phishing model calls. |
| `no-pings` | No hyperlink auditing pings. |
| `no-first-run`, `no-default-browser-check` | No first-run/default-browser calls. |
| `dns-over-https-mode=secure` + `dns-over-https-templates=<resolver>` | Encrypted DNS only (when DoH enabled). |

Plus:

- **No Google API keys** are compiled in (the CEF Standard distribution ships
  without `GOOGLE_API_KEY`/`GOOGLE_DEFAULT_CLIENT_ID`), so any API that requires
  a key is inert.
- **Tracker/ad blocking** (`shell/src/blocklist.*`) cancels requests to a
  bundled list of ~250 known ad/tracking domains at the network layer.

---

## Reproducibility checklist

- [ ] Deleted the `OpenNyx/` user-data folder before the run.
- [ ] Captured with mitmproxy **and/or** pcap **and/or** `Get-NetTCPConnection`.
- [ ] Attributed every hostname to a row in the table above.
- [ ] Confirmed **zero** endpoints in the **NONE-EXPECTED** rows from the
      `opennyx.exe` process.
- [ ] (Optional) Re-ran with DoH disabled to inspect per-host SNI directly.

If you find traffic that does not fit the **OURS-INTENDED** rows, open an issue
titled `network-audit: unexpected endpoint <host>` with your capture attached.
