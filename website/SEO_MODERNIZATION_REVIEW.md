# Website SEO preservation review

4 November 2026. Local source and build checks pass. Ready for Golden Wrench review before Boss publication.

## Final availability wording correction

Golden Wrench clarified that the repository is publicly accessible and the pending milestone is the public VOID Language release. The visible availability paragraph and HTML, Open Graph and Twitter descriptions now use the exact requested release-readiness sentence. GitHub anchors still route through /github/.

Source and built-site checks pass with 50 indexable pages, 50 sitemap URLs and two noindex documents. The build produces 215 entries. All nine SEO regression tests passed in 63.544 seconds. Exact-copy checks verify all four text placements, the canonical, noindex and sitemap exclusion in source and build. The generated per-page SEO inventory has been refreshed.

Files changed in this correction:

- website/public/github/index.html
- website/dist/github/index.html
- website/README.md
- website/MODERNIZATION_REVIEW.md
- website/SEO_MODERNIZATION_REVIEW.md
- website/review/seo-modernization-inventory.json
- website/review/seo-modernization-verification.json

The earlier review patch and screenshots retain their original reviewed snapshot and predate this copy correction. This entry records the current result. Stop for final Golden Wrench approval.

## Inventory and historical identity

Independent enumeration of production-build files found 52 HTML documents, 50 indexable pages, two noindex documents, 43 guides and 27 gallery programs. The sitemap has exactly 50 unique canonical URLs.

The October 20 audit recorded 36 canonical URLs. Later tracked snapshots and the currently deployed sitemap contain 37, including the later /roadmap/ page. All 37 remain valid and indexable locally. The /docs/ directory remains the full index while primary Docs navigation opens /docs/syntax/.

Compatibility was compared against tracked snapshots b29b96ba, 398f2aec, f704d75f and dbe01209, plus archived live HTML. The preserved routes comprise the homepage, Why VOID, Features, Examples, Getting Started, Roadmap, Docs and the 30 original guides. Their union contains 148 addressable section IDs. All remain present, including the homepage roadmap teaser, documentation categories, guide fragments and gallery choices. There are zero removed or renamed historical routes and zero added local redirects.

Thirteen indexable routes were added under /docs/:

- binary-memory/, calendar-time/, channels/, collection-tools/
- compiler-workflow/, console-output/, files-paths/, json-data/
- logging/, math-random/, os-directories/, text-encoding/, url-http/

The availability page /github/ is an additional noindex document. The existing /404.html remains noindex. Downloads and internal evidence are excluded from the sitemap.

The [independent inventory](review/seo-modernization-inventory.json) records every route, title, description, H1, subordinate heading, introduction, social field, link and ID. The [historical compatibility registry](scripts/seo-route-history.json) permanently protects the 37 routes and 148 fragments independently of the current links and sitemap.

## Per-page metadata and discoverability

All 50 indexable pages have unique descriptive titles and descriptions, one meaningful H1, logical heading levels, English language declaration, mobile viewport, exact HTTPS primary-host canonical and matching Open Graph/Twitter identity. The shared social image exists and its dimensions agree with metadata. Introductions identify each page's subject and preserve development and availability limits. The complete per-page findings are in the independent inventory.

One public metadata defect was corrected. The Examples HTML, Open Graph and Twitter descriptions still advertised 18 programs. All three now say 27, matching the actual options and panels.

All 13 new guides now have article-level links to related topics. The existing sidebar, index and footer remain ordinary HTML links. A crawl of the complete build reached all 50 indexable pages and the availability page from the homepage. It found zero orphan indexable pages, broken internal references or missing fragments. The 404 document is intentionally outside normal navigation.

The public identity remains VOID Language. Metadata and introductory content contain no VOID Engine substitution or new public release claim. Logging and HTTP retain their existing verification limits.

## Robots, sitemap and structured data

Robots permits crawling and declares the primary-domain sitemap. Checks now include noindex documents and every asset, so crawlers can observe the availability and error-page directives. HTML policies and applicable _headers selectors were reviewed together. Current _headers provides security policy without an indexing override.

Both noindex documents are excluded from the sitemap. The availability page has a self-canonical. The error document has no canonical. Intended public pages remain indexable.

The namespaced sitemap XML has no stale URLs, downloads, evidence or modification dates. Extra sitemap metadata requires verified provenance before acceptance.

The homepage retains one valid WebSite JSON-LD object with VOID Language, VOID alternate name, English language, the primary URL and website identifier. Additional identity, breadcrumb or software-distribution markup was unnecessary for this correction. No ratings, pricing or public availability claims were added.

## Validator gaps demonstrated and corrected

Five isolated mutations passed the previous checker. Original and corrected outcomes are retained in the [verification receipt](review/seo-modernization-verification.json).

| Mutation | Correction |
| --- | --- |
| Rename a historical fragment and its current links | Historical registry rejects removal even when the local graph stays consistent |
| Block /github/ in robots | Crawl checks include noindex documents and all assets |
| Add index alongside HTML noindex | Conflicting HTML directives are rejected |
| Apply an index header to /github/ | Header selectors are checked against HTML indexing policy |
| Add an unsupported future lastmod | Extra sitemap metadata requires provenance |

Regression controls additionally cover historical guide deletion and duplicate canonicals. Existing metadata, sitemap coverage, link/fragment, accidental noindex, JSON-LD, public-content and source/result controls are retained. The implementation extends the existing standard-library Python validator.

## Live and archived comparison

Read-only GET probes covered 61 requests, including all local canonical destinations, archived probe URLs, assets, redirects and a missing route. The [live receipt](review/seo-modernization-live.json) includes the exact UTC time, response chains, status codes, compression, selected headers, canonicals and robots/sitemap content. Original SEO_AUDIT.md and SEO_LIVE_CHECK.json are preserved.

The web retrieval tool could not open the site. Direct curl GET requests succeeded. All 37 existing canonical routes returned HTTP 200 with their primary-domain canonicals. The deployed sitemap still lists those 37 URLs. The 13 new guide routes and /github/ return 404 on the deployed site because the modernized local build has not been published.

HTTP and secondary-domain probes redirect to the primary HTTPS domain. The generics no-slash URL redirects to its slash canonical. The /404.html path redirects to /404 and serves the noindex error document with status 200. A genuinely missing route returns 404. HTML, robots, sitemap and stylesheet probes show Brotli compression. Observed security headers include the expected content-security policy and frame protection. No X-Robots-Tag conflict was observed.

Archived observations already show the same canonical-domain redirect behavior. Live metadata reflects the older deployed content. Local build acceptance and current live hosting describe different versions. Python preview behavior was not used as evidence for Cloudflare headers or redirects.

Search Console, Googlebot crawling, Google indexing, rankings and rich-result eligibility were not inspected. Publication and subsequent indexing outcomes remain owner-controlled follow-up.

## Verification and changed files

Commands run from the repository root:

    python3 website/scripts/check.py
    python3 website/scripts/test_check.py
    python3 website/scripts/build.py
    python3 website/scripts/check.py --directory website/dist
    git diff --check -- website

Source and built-site checkers pass for 50 indexable pages and two noindex pages. The build produces 215 entries. All nine regression test methods pass in 66.591 seconds. The independent build inventory, HTML graph crawl and historical-anchor comparison pass. The five original gap controls now fail with the expected diagnostics.

This sprint changed Examples metadata, related-topic links on the 13 new guides, the checker and regression tests, the historical registry, corresponding generated output, this report, a README pointer and review receipts. The [exact sprint inventory](review/seo-modernization-files.json) lists the files. Existing downloadable example programs are preserved.

All modifications are under website/. No staging, commits, pushes, branches, CI dispatch, publication, Cloudflare or Search Console changes were performed. Stop for Golden Wrench review.
