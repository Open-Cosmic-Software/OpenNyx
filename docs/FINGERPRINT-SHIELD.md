# Fingerprint shield (M4.1)

**What it does.** A script (`shell/src/fp_shield.cc`) runs in every web page
before the page's own scripts (CEF `OnContextCreated`, render process):

| Surface | What the shield does |
|---|---|
| Canvas (`toDataURL`, `toBlob`, `getImageData`) | flips the lowest bit of ~3 % of colour values: invisible, but every hash changes |
| WebGL | reports a common Windows GPU string instead of yours; `readPixels` gets the same noise |
| Audio (`AudioBuffer`, `AnalyserNode`) | adds noise around 1e-7, far below hearing |
| `hardwareConcurrency`, `deviceMemory` | always 8 |

The noise depends on a **random seed per browser session** and on the **site
(origin)**. A site sees the same values while you browse it (nothing breaks);
another site sees different ones, and after a restart every site sees new ones.
So trackers cannot link you across sites or sessions by these values.

Settings → Privacy → *Fingerprint shield* (on by default, applies to new tabs).

**Honest limits.** This is a JavaScript layer on stock Chromium (CEF):

- It can be detected by a determined script (for example by comparing results
  across iframes or workers). It hides *who* you are, not *that* you use a shield.
- Web Workers and Service Workers are not covered.
- Font list, screen size, time zone, language and the user agent still leak.
  The user agent is stock Chromium on purpose (a custom one would stand out).
- Real engine-level protection (like Brave's) needs Chromium patches, see
  "Approach A" in [`ENGINE-STRATEGY.md`](ENGINE-STRATEGY.md).

**Tests.** `node tests/fp-shield/run.mjs` extracts the real script from
`fp_shield.cc` and runs it in Chromium (Playwright): stable per site+seed,
different across sites and seeds, invisible (no colour value off by more than
2), patched functions look native. `FP_OFF=1` runs the same probe without the
shield; those checks must then fail. WebGL checks only run where Chromium has
WebGL (they are reported as skipped otherwise, never as green).
Manual check in the built browser: <https://coveryourtracks.eff.org> and
<https://abrahamjuliot.github.io/creepjs/>, before and after.
