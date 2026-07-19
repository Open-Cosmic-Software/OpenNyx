# OpenNyx — Engine Strategy

_How do we ship a genuinely de-googled engine without a 100 GB self-compile
treadmill?_

**TL;DR — Recommendation:** Ship **Approach D now** (hardened CEF, honestly
labelled) and **budget for Approach A later** (our own ungoogled-CEF binaries,
built on-demand on a rented beefy VM). Do **not** build Approach B (CDP
puppeteering) or C (re-shelling the ungoogled binary) as the product — they are
either a UX dead-end or impossible without recompiling.

---

## Background: what "ungoogled" actually ships

- **ungoogled-chromium** publishes **ready-made browser binaries** for
  Windows / macOS / Linux (winget `eloston.ungoogled-chromium`, plus the
  "Contributor Binaries" site). No compile needed **to run the browser**.
- There are **no prebuilt ungoogled-CEF binaries** anywhere. The official CEF
  binary distributions (cef-builds.spotifycdn.com) are **standard Chromium**,
  not ungoogled.
- ungoogled = two things applied to Chromium **source** before compiling:
  1. a **patch set** (`ungoogled-chromium/patches/…`) that rips out Google
     integration (Safe Browsing, field trials, GCM, hotword, etc.), and
  2. **domain substitution** — a `domain_substitution.list` +
     `domain_regex.list` pass that neutralizes hard-coded Google URLs in the
     source so a binary literally cannot phone home to them.

Because CEF is "Chromium + a thin embedding layer," you **can** apply the
ungoogled patch set to a CEF checkout, but the result **must be compiled** —
there is no shortcut around a full Chromium build. (Confirmed on the CEF forum:
apply ungoogled-chromium patches to `src`, then CEF patches, then build.)

This is the crux: **the browser has ready-made ungoogled binaries; the
embedding framework (CEF) does not.** OpenNyx is a CEF app, so "just download
ungoogled" is not available to us.

---

## The four approaches

### Approach A — Build our own ungoogled-CEF

Apply the ungoogled-chromium patch set + domain substitution to a pinned CEF
checkout and compile our own CEF binary distribution, which our existing shell
consumes unchanged.

**Real numbers for a Chromium/CEF build (from CEF's own docs + build reports):**

| Resource | Requirement |
|---|---|
| Disk | **~120 GB** free (Debug); ~80–100 GB (Release). Source checkout alone is tens of GB. |
| RAM | **16 GB minimum, 32 GB recommended.** Linkers (esp. the final `chrome`/`libcef` link) are memory-hungry. |
| CPU | Scales ~linearly with cores. ~4 h on 16 fast cores/SSD; ~5 h on **2** cores; a beefy 48-core box lands well under 2 h for the compile phase. |
| Network | Full `fetch`/`gclient sync` pulls tens of GB the first time. |

**GitHub-hosted runners are NOT viable for this:**
- Standard runner disk is **~14 GB** — an order of magnitude too small; you
  can't even land the checkout.
- Standard runner is 2–4 vCPU / 7–16 GB RAM, and jobs have a **6-hour hard
  limit**. Chromium does not finish a from-scratch build on a standard runner
  inside 6 h (documented pain point). GitHub's **larger** runners (up to
  64 vCPU) exist but still cap disk and cost money; disk is the killer.
- Conclusion: Approach A needs a **self-hosted runner or a rented big VM**.

**Build-box spec + cost (Hetzner, on-demand):**

Hetzner Cloud **CCX** line = dedicated AMD EPYC vCPUs (no noisy neighbours),
hourly-billable, attach a big block volume for the source tree:

| Plan | vCPU | RAM | Base disk | ~Compile time (full) | Price |
|---|---|---|---|---|---|
| CCX33 | 8 | 32 GB | 240 GB | ~4–6 h | ~€0.10/h · ~€64/mo |
| CCX53 | 32 | 128 GB | 600 GB | ~1.5–2.5 h | ~€0.35/h · ~€230/mo |
| CCX63 | 48 | 192 GB | 960 GB | **~1–2 h** | ~€0.49/h · ~€290/mo |

**Key cost insight:** we don't need a build box 24/7. Spin up a **CCX53/CCX63
by the hour**, build all three platforms, publish the CEF distribution as a
GitHub Release asset, destroy the box. A monthly engine bump ≈ **a few euros of
compute**, not €290/mo. Budget realistically **€10–30/month** amortized
(occasional rebuilds, wasted spin-ups, a persistent volume to cache the
checkout so we don't re-`fetch` 30 GB every time).

Notes / caveats:
- macOS builds can't run on Hetzner (no Apple hardware). Options: GitHub's
  paid `macos-large` runners, a Mac mini, or MacStadium — defer until M5.
- ccache/a persistent source volume turns the *second* build from ~2 h into
  well under an hour.
- We must track upstream ungoogled patch cadence (they rebase per Chromium
  major); a bump may need a few patch conflicts resolved by hand.

**Verdict:** the honest "real ungoogled" path. Feasible for a small team
*only* because it's on-demand cloud compute, not a permanent build farm.
This is our **Phase 2 target**.

---

### Approach B — CDP-control a ready-made ungoogled binary

Run the official ungoogled-chromium binary headless/app-mode and drive it over
the **Chrome DevTools Protocol** (Puppeteer/Playwright style) while our native
window draws tabs+toolbar.

- **It is a real engine** (real ungoogled Chromium, zero compile) — that part
  is genuinely attractive.
- **But the UX is a puppeteering hack, not a browser.** CDP was built for
  automation/inspection, not for being the primary user-driven UI. To make it
  feel like a browser you must:
  - hide/kill the browser's own window chrome and **embed** its rendering
    surface inside your window (OS-level window reparenting — fragile on
    Windows, worse on Wayland, notoriously flaky), **or**
  - run it headless and paint frames yourself via `Page.screencast` — which
    means **streaming screenshots as your UI**: input latency, no smooth
    scrolling/compositing, broken for video/WebGL/DRM. This is what remote-
    browser products (Browserbase, cloud browsers) do, and it's fine for
    automation but **wrong for a daily-driver browser**.
- Tab/target management over CDP is real (recent work even added foreground-
  tab tracking), but you're fighting the protocol to reproduce what CEF already
  gives us natively.

**Verdict:** great for a scripting/automation feature; **wrong foundation for
the product UI.** CEF already embeds the engine properly — B would be a
regression in UX to gain de-googling we can get other ways. **Reject as the
shell.**

---

### Approach C — Re-shell / re-theme the ungoogled binary

Take the ungoogled binary and replace only the browser chrome (toolbar, tab
strip, new-tab) via resource replacement / themes / policies — no recompile.

What is **actually** achievable on a stock Chromium binary **without** touching
source:

| Want | Possible no-recompile? | How |
|---|---|---|
| Custom **New Tab Page** | ✅ | `NewTabPageLocation` enterprise policy → our URL |
| Custom **homepage / startup** | ✅ | `HomepageLocation`, `RestoreOnStartup` policies |
| **Colours / theme** | ⚠️ partial | Chrome Theme (colors, NTP background) — not layout |
| Default **search engine** (Brave) | ✅ | `DefaultSearchProviderSearchURL` policy |
| Disable Google bits further | ✅ | policies + the ungoogled patches already applied |
| Replace the **toolbar / tab strip / omnibox** | ❌ | These are compiled C++ `views` — **source change + recompile only** |
| Swap `chrome://` UI wholesale | ❌ | resource-pak edits get signature/CRC rejected; real changes = recompile |

So C gets you a **re-skinned ungoogled-chromium** (our NTP, our search, our
colours) but it is **still visibly Chrome** — same toolbar, same tab strip,
same menus. The distinctive OpenNyx shell (custom Views UI we already built on
CEF) **cannot** be bolted onto a prebuilt binary without recompiling. At that
point you're back to Approach A anyway.

**Verdict:** viable as a *quick "privacy build of Chrome with our branding"*
side distribution, but it is **not OpenNyx** — it can't carry our UI. **Reject
as the product**; optionally keep as a "here's a policy-hardened ungoogled
build" community offering later.

---

### Approach D — Hardened CEF, honestly labelled (what we ship today)

Stay on the official prebuilt CEF distribution and neutralize Google at the
binary/runtime level via switches + env + our shell code.

**What standard CEF still contacts by default** (and the exact knobs):

| Google touch-point | Neutralize with |
|---|---|
| API keys (sync, translate, GCM, etc.) | Set `GOOGLE_API_KEY`, `GOOGLE_DEFAULT_CLIENT_ID`, `GOOGLE_DEFAULT_CLIENT_SECRET` to **dummy values** → features silently no-op |
| Component Updater (pulls Google components) | `--disable-component-update` |
| Background networking (variations, etc.) | `--disable-background-networking` |
| Field-trial / "variations" server | Off by default in official CEF (`disable_fieldtrial_testing_config=true`); plus `--disable-background-networking` |
| Sync | `--disable-sync` |
| GCM / push | `--disable-features=GCM`, `--gcm-channel-status=2` |
| Safe Browsing | not wired up in CEF by default; keep it off |
| Optimization Hints / on-device model | `--disable-features=OptimizationHints,OnDeviceModelService` |
| Default-apps, default-browser ping | `--disable-default-apps --no-default-browser-check` |
| Crash reporting / metrics | no Breakpad/crashpad upload configured; `--disable-breakpad` |

With **dummy API keys + these switches, the Google features are inert** — they
either short-circuit on the missing key or are disabled outright. This is
materially "no Google phone-home" in practice.

**The honesty line:** this is **hardened / de-googled *at runtime*, but the
binary still contains** the Google integration code and the hard-coded URLs
(no domain substitution). It is **not** "ungoogled" in the strict
ungoogled-chromium sense (which strips it from source). We should call OpenNyx
today **"de-googled / no-Google-phone-home,"** and reserve the word
**"ungoogled"** for when Approach A lands. Overclaiming here would be the one
thing that damages trust.

**Verdict:** ✅ **Ship now.** Fast security updates (just bump the pinned CEF
version), keeps our existing custom Views shell, zero build infra. Honest,
strong privacy today.

---

## Comparison

| | A: our ungoogled-CEF | B: CDP puppeteer | C: re-shell binary | D: hardened CEF *(now)* |
|---|---|---|---|---|
| Effort | **High** (build pipeline, patch upkeep) | Medium (but wrong UX) | Low–Med | **Low** (already done) |
| Infra cost | On-demand VM, ~€10–30/mo amortized | just the binary | just the binary | **€0** |
| "How real / ungoogled" | **★★★★★** true ungoogled | ★★★★☆ real ungoogled engine, hack UI | ★★★☆☆ ungoogled engine, still-Chrome UI | ★★★☆☆ de-googled at runtime, not stripped |
| Product-quality UX | ✅ our full shell | ❌ screencast/reparent hacks | ⚠️ stuck with Chrome chrome | ✅ our full shell |
| Maintainability | patch rebases each Chromium major | protocol drift | policy-only, fragile skins | **bump one version pin** |
| Update cadence | build per upstream bump (~monthly) | follow ungoogled releases | follow ungoogled releases | **follow CEF releases** |

---

## Recommendation for OpenNyx (small team)

**Two-phase plan:**

1. **Now (M2→M3): Approach D, labelled honestly.** Keep the CEF shell we
   already shipped. Lock in the full de-google switch/env set (list above) in
   one central place, document exactly what the browser can and cannot contact,
   and market it as **"de-googled, no telemetry, no phone-home"** — not
   "ungoogled." Ship the privacy layer (blocker, DoH, fingerprint reduction) on
   top; that's where most user-visible privacy value actually lives.

2. **Later (post-M5, when there's appetite): Approach A.** Stand up an
   **on-demand** Hetzner CCX53/63 build job that applies the ungoogled patch
   set + domain substitution to our pinned CEF, compiles a CEF distribution,
   publishes it as a GitHub Release asset, and self-destructs. Our shell
   consumes it **unchanged**. That's the day we earn the word "ungoogled."

**Explicitly reject B and C as the product** (keep them in mind only as
niche/side offerings).

### Why (one paragraph)

Approach D lets a two-person team ship an honest, strong-privacy browser
**today** with our own beautiful shell and zero build infrastructure, while
Approach A gives us a credible, genuinely-ungoogled upgrade path we can reach
**on-demand for a few euros of cloud compute** instead of a permanent 100 GB
build farm — so we get to be honest now and truly ungoogled later, without ever
compromising the custom UI that makes OpenNyx *OpenNyx*.

### Biggest tradeoff

The one real tension is **truth-in-labelling in the interim**: until Approach A
ships, OpenNyx is "de-googled at runtime" (Google code + URLs still in the
binary, just neutered) — **not** source-stripped "ungoogled." We must resist
calling it "ungoogled" until we actually compile it, because the entire value
of a privacy browser is trust, and one overclaim burns it.

---

## Concrete next steps

**Phase D (immediately):**
1. Create `privacy/degoogle-switches.md` — the canonical list of switches + env
   vars above, with a one-line rationale each.
2. Ensure the shell applies all of them centrally (`OnBeforeCommandLineProcessing`
   + setting dummy `GOOGLE_*` env before CEF init). Verify none are missing.
3. **Verify empirically:** run OpenNyx behind a logging proxy / `--log-net-log`,
   load a few pages, and diff outbound hosts against an allowlist. Publish the
   result as `docs/NETWORK-AUDIT.md` ("here is literally everything it
   contacts").
4. Update README/marketing copy: "**de-googled**, no telemetry, no phone-home"
   — remove any implication of source-level "ungoogled" for now.

**Phase A (spike, timeboxed):**
1. Spin up a **Hetzner CCX53** hourly + a persistent volume; document the exact
   `fetch → apply ungoogled patches → domain_substitution → CEF patches →
   build` recipe as `docs/UNGOOGLED-CEF-BUILD.md`.
2. Produce **one** Linux ungoogled-CEF distribution, drop it in `build/`, and
   confirm the existing shell runs against it unchanged (proof of concept).
3. If green: wrap the recipe in a workflow that a self-hosted/cloud runner
   triggers on demand, publishing the CEF distro as a tagged GitHub Release.
   Only then flip the wording to "**ungoogled**."

---

_Sources: CEF master build quick-start (120 GB / 16 GB RAM / ~4 h), CEF forum
build reports (~80 GB, 5 h on 2 cores) and the "build CEF for ungoogled"
thread, ungoogled-chromium-windows packaging notes, CEF issue #4078 (exact
de-google switch list), Chrome Enterprise NewTabPageLocation & policy docs,
GitHub Actions runner limits (~14 GB disk, 6 h job cap), and Hetzner CCX
dedicated-vCPU pricing._
