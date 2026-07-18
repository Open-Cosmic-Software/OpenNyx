# OpenNyx Search — Feasibility Study

**Status:** Research draft. No code, no crawling done. Numbers below are from public
sources (see footnotes) plus honest back-of-envelope estimates. Read this before
committing engineering time.

**Goal being evaluated:** an independent, EU-hosted search engine with its *own*
index (no Google, no Bing, no US Big Tech dependency) as the default search for the
OpenNyx browser.

**TL;DR verdict:** Start now as a *curated vertical* side project (a few million
high-quality pages) — that is genuinely useful and cheap (~€100–170/month on one
Hetzner box). A general-purpose "web" index (100M+ pages) is a real, multi-year,
funded undertaking. Keep an anonymized external fallback (Mojeek/Brave API) so the
browser never ships a broken default.

---

## 1. Reference points — who has done this, and at what scale

### Marginalia Search (one person, Sweden)
The single most useful precedent: a genuinely independent index built and operated
by **one developer**.

- **Own crawler, own index** — no Bing/Google reuse. Custom engine in **Java**,
  MariaDB for ancillary metadata. AGPL, source public. [1][2]
- **Index size:** ~**300 million documents** at peak (2024), roughly **1 TB** on
  disk. The author is explicit that index size is a *marketing* metric and that
  "a million high-quality documents beats a billion where only a fraction are
  interesting." [2]
- **Hardware:** designed to run on a **single server**. Minimum spec in their docs:
  x86-64, **≥16 GB RAM, ≥4 cores**. Storage for ~100k docs: ~2 TB SSD (index) +
  4 TB HDD (crawl data). Real production is a single beefy machine with enterprise
  NVMe. Ran on *domestic broadband PC hardware* until Oct 2023. [1][3]
- **Design notes:** extremely I/O-intensive; consumer SSDs get destroyed in months;
  `noatime` everywhere; recently re-optimized the index for **small NVMe read sizes**
  to cut query latency. [1][4]
- **Scope discipline:** English-only, on purpose — "half-assed i18n is a bigger
  insult than none." Targets the "small, old and weird web," explicitly *complements*
  Google rather than replacing it. [2]

**Lesson:** an independent index at ~10^8 docs is achievable by one motivated person
on one server — *if* you accept a narrow, opinionated scope and treat quality
filtering (not raw size) as the hard problem.

### Stract (StractOrg, open source, Rust)
- **Fully independent index with own crawler.** Inverted index built on **Tantivy**
  (Rust equivalent of Lucene). [5][6]
- Deliberately limited to lesser-known sites in early stages — same "quality over
  Bing-reuse" pattern as Marginalia. [5]
- Open source, non-profit framing. Good architectural blueprint for us because the
  stack (Rust + Tantivy + own crawler) is exactly what we'd likely pick.

### Mojeek (UK, commercial, own index)
- **Own crawler (MojeekBot), own index.** Independent — not a Bing/Google front-end.
- Scale trajectory: 1B pages (2015) → 6B (Oct 2022) → **5–6B+** today. One of a
  *handful* of engines worldwide with a billion-plus independent index. [7][8]
- Founded 2004; took ~11 years to reach 1B and ~18 to reach ~6B. That timeline is
  the honest cost of a *general* web index built commercially.
- Sells a **Web Search API** (full independent index) — relevant as a fallback (§4).

### SearXNG (contrast: meta-search, NO own index)
- Aggregates results from other engines; stores **no index of its own**. Zero crawl
  cost, trivial to host, but by definition **dependent** on upstream engines
  (including Google/Bing) — which is exactly the dependency OpenNyx wants to avoid.
- Still valuable as a *fallback layer* or for the "everything else" long tail while
  our own index covers curated verticals.

**Takeaway table**

| Engine | Own index? | Scale | Team | Stack | Relevance to us |
|---|---|---|---|---|---|
| Marginalia | yes | ~300M docs / ~1 TB | **1 person** | Java, custom | proof a solo indie index works |
| Stract | yes | small/early | tiny/OSS | Rust + Tantivy | our likely stack blueprint |
| Mojeek | yes | 5–6B+ | company, 20 yrs | custom | shows cost of *general* index |
| SearXNG | **no** | n/a | OSS | Python | fallback / long-tail only |

---

## 2. Common Crawl bootstrap — how realistic?

Common Crawl (CC) is the obvious "cheat code" to avoid crawling the whole web
ourselves. Reality check with numbers:

- **Corpus scale:** ~250–300B pages across 100+ monthly snapshots. [9][14]
- **One monthly snapshot:** ~**50–100 TB compressed** raw HTML (WARC). Historically
  ~**60,000 WARC files of ~1 GB each** per crawl. [10][11]
- **Formats** (pick the right one — this is the key cost lever):
  - **WARC** = full HTTP response + headers + raw HTML. Biggest.
  - **WAT** = extracted metadata (links, headers) as JSON.
  - **WET** = **plain text only**. This is what we want for indexing — it skips HTML
    parsing entirely and is a fraction of WARC size. [12]

**Realistic pipeline:**
1. Use the **CC columnar URL index** to *filter before download* — select only the
   domains/languages you care about (e.g. a curated allowlist of quality TLDs, docs,
   wikis, EU domains) via SparkSQL, so you never pull the full 60k files. [13]
2. Download only the **WET** ranges for selected records.
3. Run language + quality + spam filtering (domain reputation, boilerplate removal),
   then feed clean text into the index.

**Cost shape (honest):**
- Egress is the trap. Pulling a *full* snapshot's WET out of AWS is many TB → large
  egress bills if you process off-AWS. Two sane options:
  - **Process in-region on AWS** (EMR/EC2 spot) so reads from the CC S3 bucket are
    free/cheap, write only the *filtered* text out. CC's own `cc-pyspark` tooling is
    built for exactly this. [13]
  - **Filter hard first** so you only ever move a curated slice (tens of GB, not TB).
- **Compute:** filtering one monthly snapshot with Spark is a well-trodden path
  ("Analysing Petabytes of Websites" documents doing it on EMR). Expect **hundreds to
  low-thousands of vCPU-hours** for a full-snapshot pass; a *curated-slice* pass is
  far less. On EMR spot / Hetzner this is tens of euros, not thousands — *if* you
  filter first and don't repeatedly move TB around.

**Verdict on CC:** Excellent for **Phase B expansion** and for *seeding* curated
verticals (grab all of Wikipedia, docs sites, etc. without crawling them ourselves).
**Not** a magic "instant Google" — the raw dump is mostly low-quality/spam, so the
value is entirely in the filtering, which is the same hard problem Marginalia
describes. Bootstrapping a *curated* few-million-page index from CC WET is very
realistic and cheap. Bootstrapping a clean *general* index from CC is the same
multi-quarter effort as building the filter that every LLM data team also builds.

---

## 3. Index technology for our scale (1–50M pages)

| Option | Lang | Model | RAM behavior | Best fit |
|---|---|---|---|---|
| **Tantivy** | Rust | library (Lucene-like) | low footprint, mmap; RAM ≈ hot segments, not whole index | **our default** — same as Stract; embed directly in a Rust indexer |
| **Quickwit** | Rust | Tantivy on object storage | decouples compute/storage; index on S3/MinIO/local | great for **Phase B** cheap cold storage of huge index |
| **Meilisearch** | Rust | product | disk-stored, pulls to RAM on demand; easy | good for a *curated* few-M-doc vertical, fast to ship |
| **Vespa** | Java/C++ | platform | RAM-hungry, cluster-oriented | overkill/expensive at our stage |
| **custom** | — | — | — | not now; Tantivy gives us 90% for free |

**Concrete sizing anchors (measured, public):**
- Tantivy/Quickwit **compression ratio ≈ 2.75–3×** vs raw text: 6.1B documents →
  **8.4 TB** index in Quickwit's adversarial benchmark. [15][16]
- Scaling that down linearly (index size is roughly text-volume driven, not doc-count
  driven): assume ~**1–3 KB indexed text per page** after boilerplate stripping.
  - **1M pages** ≈ 1–3 GB text → ~**0.5–1.5 GB** index. Fits in RAM on a laptop.
  - **10M pages** ≈ 10–30 GB text → ~**5–15 GB** index. Comfortable on a 32–64 GB box.
  - **50M pages** ≈ 50–150 GB text → ~**25–75 GB** index. Needs NVMe + mmap; RAM for
    hot set, not the whole thing. Still **one server**.
- **RAM rule of thumb:** with Tantivy/mmap you do *not* need RAM ≈ index size. You
  need RAM for the OS page cache of hot segments + query working set. Marginalia runs
  ~1 TB of index on a single box with tens of GB RAM by leaning on NVMe. [1]

**Recommendation:** **Rust + Tantivy** for the index (matches Stract, low RAM, fast),
own crawler in Rust/Go, plain-text pipeline. Consider **Quickwit** only when the
index outgrows a single fast NVMe (Phase B), for its object-storage tiering.

---

## 4. Phased plan

### Phase A — Curated vertical index (start now)
**Scope:** a few million *hand-selected, high-quality* pages: developer docs
(MDN, language/framework docs, man pages), wikis (Wikipedia + niche wikis),
reputable tech news/blogs, standards bodies, EU/gov sources. English first (follow
Marginalia's scope discipline), German second.

**How to fill it:** mostly *seeded from Common Crawl WET* (allowlisted domains) +
a light own crawler for freshness. This avoids building a planet-scale crawler on
day one.

**Hardware — one Hetzner dedicated server (EU, Germany/Finland, GDPR, green power):**

| Component | Spec | ~Cost/mo |
|---|---|---|
| **AX52** (Ryzen 7 7700, 64 GB RAM, 2×1 TB NVMe) | good starting box | **~€64** [17] |
| **AX102** (Ryzen 9 7950X3D, 128 GB RAM, 2×1.92 TB NVMe) | headroom for 10–50M pages | **~€100–120** [18] |
| Backups / extra volume / traffic | | ~€10–30 |

**Phase A realistic all-in: ~€100–170/month on a single server.** This is a rounding
error vs. the value of an independent default search for the browser, and it fits the
existing infra style (Hetzner, single boxes).

**What "useful day-to-day" means here:** for a developer, a fast, ad-free, tracker-
free search across docs + wikis + quality tech content that *never* touches Google is
already better than Google for a large fraction of real queries. That's the MVP worth
shipping.

### Phase B — Common Crawl / general expansion
**Scope:** 100M–1B pages, broader web.

**Cost shape:**
- Storage: 100M pages ≈ 50–150 GB index (fits one NVMe); 1B pages ≈ 0.5–1.5 TB index
  (Marginalia territory — still one big box or a small cluster).
- The real cost is **not storage, it's the crawl + filtering pipeline and its
  maintenance**: continuous crawling infra, spam/quality classifiers, dedup, freshness
  re-crawls, abuse handling. This is where Mojeek's 20-year timeline and Marginalia's
  "quality is the hard part" both apply.
- CC re-processing per snapshot: budget a few hundred euros of spot compute per full
  pass if done in-AWS-region; near-zero if you only ever process curated slices.
- **Honest range:** €300–1,000/month infra for a 100M+ general index that's actually
  maintained, **plus real engineering time** — this is the "needs dedication/funding"
  tier, not a weekend.

### Fallback strategy — anonymized proxy to a non-US external index
So the browser's default search is *never* broken while our index is small, and to
cover the long tail:

- **Mojeek Web Search API** — UK, genuinely independent 5–6B-page index, sells API
  access. Best "non-US-ish, own-index" option. [19][20]
- **Brave Search API** — independent index, but US company (already used elsewhere in
  our stack; acceptable as *fallback*, not as the sovereignty story).
- **Qwant** (France/EU) — has historically leaned on Bing for its main index, so it's
  weaker on the "independent" axis; keep as a EU-brand option but verify current
  independence before relying on it.
- **SearXNG self-hosted** — aggregate several of the above; gives resilience and no
  single upstream dependency.

**Privacy-preserving design:** route fallback queries through our own server so the
external API sees only OpenNyx's IP, never the user's; strip identifiers; no query
logging; make the fallback provider visible/togglable in the privacy dashboard.
This keeps the "no phone-home we don't document" promise from ARCHITECTURE.md intact.

**Routing model:** own curated index answers first; if confidence/coverage is low,
transparently blend/fall back to the external index. Over time, as our index grows,
the fallback fires less often.

---

## 5. Honest verdict

**Is this "start now" or "needs funding/hardware"?** — **Both, in sequence.**

1. **Phase A is a start-now side project.** ~€100–170/month, one Hetzner box, Rust +
   Tantivy, seeded from Common Crawl WET (allowlisted quality domains) + light own
   crawler. A solo builder can reach a *useful* few-million-page curated index — this
   is *proven* by Marginalia (300M docs, one person) and Stract (Rust+Tantivy, OSS).

2. **Phase B (general web index) needs dedication and/or funding.** Not because of
   storage (cheap) but because of the *continuous crawl + quality-filtering pipeline*
   and its upkeep. Mojeek took ~18 years; Marginalia is a full-time labor of love.
   Don't promise a "Google replacement" — promise a growing independent index that
   *complements* a privacy-respecting fallback.

3. **Always ship a fallback.** Anonymized proxy to Mojeek (own index, non-US) so the
   browser default is instantly good on day one and degrades gracefully.

**Minimum viable version that's actually useful for a developer, day one:**
> A single Hetzner box running a Rust/Tantivy index of ~1–5M curated pages
> (dev docs + wikis + quality tech content, seeded from Common Crawl WET), fronted by
> a clean OpenNyx search UI, with an anonymized Mojeek fallback for everything the
> curated index doesn't cover. No Google, no Bing in the primary path. Ad-free,
> tracker-free, EU-hosted.

That is genuinely better than Google for a large slice of real developer queries, and
it's buildable now for the price of a nice dinner per month.

**Biggest risk:** not hardware and not cost — it's **index quality / relevance and
crawl-pipeline maintenance**. Raw scale is easy; a clean, spam-free, well-ranked index
that stays fresh is the hard, ongoing problem every indie engine warns about. Scope
discipline (curated verticals first, English first) is the mitigation.

---

### Sources
1. Marginalia docs — Hardware & Configuration: https://docs.marginalia.nu/1_overview/01_hardware/
2. Marginalia FAQ (index size, stack, scope): https://www.marginalia.nu/marginalia-search/faq/
3. Marginalia docs — Crawling and Loading: https://docs.marginalia.nu/2_crawling/
4. "Marginalia optimizes index for NVMe SSDs": https://www.webpronews.com/marginalia-search-optimizes-index-for-nvme-ssds-cuts-query-latencies/
5. Stract on Hacker News / r/rust: https://news.ycombinator.com/item?id=39254172
6. Stract GitHub (Tantivy inverted index): https://github.com/StractOrg/stract
7. Mojeek Wikipedia (scale, Kagi source): https://en.wikipedia.org/wiki/Mojeek
8. Mojeek About / billion-page milestone: https://www.mojeek.com/about/
9. Common Crawl scale (~250–300B pages): https://zeroentropy.dev/concepts/common-crawl/ ; https://registry.opendata.aws/commoncrawl/
10. "Analysing Petabytes of Websites" (WARC ~1 GB × ~60k files/crawl): https://tech.marksblogg.com/petabytes-of-website-data-spark-emr.html
11. Emergent Mind — snapshot 50–100 TB compressed: https://www.emergentmind.com/topics/commoncrawl-corpus
12. Common Crawl formats (WARC/WAT/WET): https://groundingpage.com/facts/common-crawl-web-corpus/
13. Common Crawl cc-pyspark (columnar URL index, in-region processing): https://github.com/commoncrawl/cc-pyspark
14. Common Crawl WARC format blog: https://commoncrawl.org/blog/navigating-the-warc-file-format
15. Quickwit benchmark (6.1B docs → 8.4 TB, ratio 2.75): https://quickwit.io/blog/benchmarking-quickwit-engine-on-an-adversarial-dataset
16. Quickwit 101 (compression ~3×, ~15 GB splits): https://quickwit.io/blog/quickwit-101
17. Hetzner AX52 (Ryzen 7 7700) ~€64/mo: https://www.achromatic.dev/blog/hetzner-server-comparison
18. Hetzner AX102 (Ryzen 9 7950X3D, 128 GB): https://www.hetzner.com/dedicated-rootserver/ax102/
19. Mojeek Web Search API: https://www.mojeek.com/services/search/web-search-api/
20. Mojeek API docs: https://www.mojeek.com/support/api/

*Prices and index sizes are as of mid-2026 from the cited public pages; treat as
order-of-magnitude, verify before procurement.*
