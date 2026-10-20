# VOID Language SEO audit

Audit date: October 20, 2026. Scope: `website/` only. Status: implemented and verified locally; owner deployment pending.

## Coverage and findings

Independently enumerated all public HTML files: **37 pages audited and changed**, comprising **36 indexable URLs and one noindex error document**. The documentation index links to 30 individual language guides. Reconciled all files against the sitemap instead of using the sitemap as the inventory. The local and observed deployed sitemap contain the same exact 36 canonical URLs; no error pages, source downloads or assets are included.

Existing canonical URLs, robots policy and sitemap coverage were already correct. Retained those policies and made them enforceable across every page. Replaced vague or generic search descriptions with individually written summaries, made titles subject-specific, and aligned Open Graph and Twitter titles/descriptions with each page's HTML metadata. Shared image branding remains unchanged. Every intended page has a self-referencing HTTPS `voidlang.com` canonical and matching `og:url`.

Reviewed every page's primary heading and heading sequence. Added a visually hidden feature-overview H2 to fix the features page's H1-to-H3 skip. Made the documentation index heading identify VOID guides and their scope. Improved existing introductions for structs, collections, generics, interfaces and async; added relevant async-to-parallelism and native-integration-to-cleanup links. Retained all substantive teaching content and implementation/future-work distinctions. Getting started explicitly states that there are no public builds.

CSS, JavaScript, artwork, fonts, downloadable programs, root compiler examples and internal documentation were not changed. Protected files match their pre-sprint SHA-256 snapshots. The entire examples page body is byte-for-byte identical to the tracked starting version, including the 15-topic selector and all panels. No release, performance or installation claims were added.

## Page-by-page final metadata

Each indexable row below has exactly one meaningful H1, logical heading levels, `lang="en"`, a device-width viewport, a unique title and description, and matching social metadata. The route is appended to `https://voidlang.com` for its canonical and Open Graph URL. All 36 appear exactly once in the sitemap and are reachable from the homepage using ordinary HTML anchors. Subject-specific wording was reviewed against each page's existing explanations and examples.

| Route | Final HTML title | Final meta description | Primary H1 |
| --- | --- | --- | --- |
| / | VOID Language — Native Programming with LLVM | Discover VOID, a general-purpose native programming language built with C++20 and LLVM. Explore its design, working code examples and language guides. | VOID Language |
| /docs/ | VOID Language Documentation — Syntax, Types and Concurrency | Learn VOID with 30 guides covering syntax, functions, data types, structs, generics, async tasks and native integration. Includes complete examples and current limits. | VOID Language guides. From syntax to async. |
| /docs/async/ | Async Functions and Await — VOID Language | Learn VOID async functions, inferred task handles and await with a complete example. Understand cooperative scheduling and the explicit async adapter requirement. | Async & await |
| /docs/callables/ | Callable Structs and the Call Method — VOID Language | Make VOID struct instances callable with a compatible call method. Learn invocation syntax, generic operations and value-versus-reference behavior. | Callable structs |
| /docs/cleanup/ | Defer, With and Scoped Cleanup — VOID Language | Use VOID with and deinit to own an in-memory workspace’s cleanup. Learn how function-exit defer differs from scope cleanup and garbage collection. | Defer & resource cleanup |
| /docs/closures/ | Function Values and Closures — VOID Language | Return functions from VOID functions and capture local state. Follow a small counter to understand closure identity, shared bindings and typed callbacks. | Functions & closures |
| /docs/collections/ | Collections and Growable Lists — VOID Language | Build a growable list in VOID, append a score with push and total its values with a for loop. Learn how list identity differs from fixed-array copying. | Collections |
| /docs/composition/ | Composition with Using — VOID Language | Compose VOID structs from smaller components with using. Learn promoted members and how value copying and managed reference identity remain distinct. | Composition |
| /docs/control-flow/ | Control Flow and Loops — VOID Language | Use VOID’s for keyword for conditional loops, collection iteration and repetition. Follow an odd-number sum and learn where break and continue fit. | Control flow |
| /docs/dictionaries/ | Ordered Dictionaries — VOID Language | Use VOID dictionaries for ordered key/value data. Explore membership, insertion order, iteration, removal, clear and shared identity through a source example. | Dictionaries |
| /docs/editor-tooling/ | Editor Tooling and Source Diagnostics — VOID Language | Explore VOID’s source-first diagnostics and shared language-server direction, including completion, navigation and guarded edits, with current development boundaries. | Editor tooling & diagnostics |
| /docs/errors/ | Results and Error Handling — VOID Language | Handle recoverable errors in VOID with result, Ok, Err and match. Follow a division example and learn how error propagation differs from runtime failure. | Results & errors |
| /docs/events/ | Events and Synchronous Notifications — VOID Language | Connect a sensor and display with a VOID event. Learn event declarations, method subscriptions, owner-controlled emission and synchronous notification order. | Events |
| /docs/functions/ | Functions and Recursion — VOID Language | Write typed functions in VOID and follow a short factorial example. Learn parameters, return values, recursive base cases and the limits of i32 results. | Functions |
| /docs/generics/ | Generics and Reusable Typed Functions — VOID Language | Write reusable VOID generic functions with type parameters and inference. Follow a larger-of-two-values example and understand concrete type requirements. | Generics |
| /docs/inheritance/ | Inheritance and Virtual Dispatch — VOID Language | Extend a managed VOID struct with extends, choose virtual and override behavior explicitly, and follow object identity through a base-type view. | Inheritance & dispatch |
| /docs/initialization/ | Initialization and Field Defaults — VOID Language | Give VOID data a valid starting state with field defaults and init methods. Follow nested initialization and construct values directly with their type names. | Initialization |
| /docs/interfaces/ | Structural Interfaces — VOID Language | Describe required methods with VOID interfaces. Learn structural conformance, interface views, receiver effects and readonly access through a complete example. | Interfaces |
| /docs/live-development/ | Live Development and Compatible Reload — VOID Language | Understand VOID’s compatible function-replacement support, debug-cache scope and live-development goals. Automatic watching and save-triggered reload remain planned. | Live development |
| /docs/methods/ | Receiver Methods — VOID Language | Attach behavior to VOID structs with named receivers. Move a point, calculate its squared distance and understand mutation through value and readonly receivers. | Receiver methods |
| /docs/modules/ | Modules, Imports and Project Paths — VOID Language | Organize VOID across source files with use and import aliases. Understand project-root paths and relative imports with a complete three-file example. | Modules & imports |
| /docs/native-integration/ | Native Integration and C Bindings — VOID Language | Understand VOID’s trusted static C binding subset: typed wrappers, UTF-8 transfers, managed resources and callbacks, with general library loading still planned. | Native integration |
| /docs/nullable/ | Nullable Values and Callbacks — VOID Language | Represent absence with VOID nullable types. Use null and Ok patterns to handle an optional function value before invoking it. | Nullable values |
| /docs/parallelism/ | Parallel Workers and Spawn — VOID Language | Run independent calculations with VOID spawn and await. Learn bounded native workers, supported value transfers and why shared mutable objects stay restricted. | Parallel workers |
| /docs/patterns/ | Enums and Pattern Matching — VOID Language | Match VOID enum variants and typed payloads with is and match. Learn explicit var captures, nested result patterns and value-copy behavior. | Enums & pattern matching |
| /docs/properties/ | Properties and Computed Getters — VOID Language | Define a computed area property for a VOID rectangle. Learn field-style getter access, optional setters and why getters cannot mutate their receiver. | Properties |
| /docs/strings/ | Strings and Unicode — VOID Language | Work with VOID UTF-8 strings, literals, concatenation and independent text values. Understand empty defaults, Unicode representation and the current text API scope. | Strings & Unicode |
| /docs/structs/ | Value Structs, Managed Objects and Memory — VOID Language | Understand VOID value structs and managed objects through a complete example of independent copies, shared identity and automatic memory management. | Structs & memory |
| /docs/syntax/ | Syntax Basics — VOID Language Documentation | Read your first VOID program: indentation, comments, function declarations, typed parameters and the entry function, explained with a complete source example. | Syntax basics |
| /docs/task-groups/ | Task Groups and Cancellation — VOID Language | Group related VOID async work with tasks.group. See normal scope completion wait for child tasks, then learn the boundaries of early exits and cooperative cancellation. | Task groups & cancellation |
| /docs/types/ | Types, Numbers and Conversions — VOID Language | Explore VOID’s exact-width integers, floating-point types, Boolean values and strings, including contextual literals, wrapping arithmetic and checked as conversions. | Types & numbers |
| /docs/variables/ | Variables and Constants — VOID Language | Learn how VOID uses var, const and type inference. Understand mutable bindings, local readonly views and module-level compile-time constants. | Variables & constants |
| /examples/ | VOID Language Examples — Functions, Generics and Async | Browse 15 complete VOID programs covering recursion, lists, properties, closures, events, scoped cleanup, task groups and parallel workers, with source downloads. | Small programs. Real possibilities. |
| /features/ | VOID Language Features — Native Code, Types and Async | Explore VOID’s native LLVM compilation, managed memory, types, collections and concurrency, with clear boundaries between implemented features and future work. | A thoughtful foundation. Real native execution. |
| /getting-started/ | Getting Started with VOID — Development Status | VOID Language is in development. There are no public builds available yet. | VOID is in development. |
| /why-void/ | Why VOID? Language Goals and Design Choices | Why VOID is being built: approachable native programming, automatic memory management, consistent rules, useful diagnostics and a live-development direction. | Programming should feel like making something. |

The error document `/404.html` has title **Page not found — VOID Language**, H1 **Uncharted territory.**, and description “The requested VOID Language page could not be found. Return to the homepage or browse the language documentation.” It explicitly has `noindex`, no canonical, and no sitemap entry. Its navigation remains useful.

## Indexing, links and social assets

- Canonical convention: HTTPS apex domain and trailing-slash directory routes; no competing `.net` canonicals or internal links. Internal links point directly to canonical page paths; local assets, downloadable sources and fragments resolve.
- `robots.txt` permits Googlebot and other crawlers to fetch intended pages and essential assets and declares `https://voidlang.com/sitemap.xml`. The deployment header file has no unintended indexing restriction.
- The sitemap contains 36 unique indexable URLs. No speculative `lastmod`, frequency or priority fields were added.
- The existing `/assets/social.png` was visually inspected and is a readable 1200 × 630 PNG. It presents current VOID Language branding and accurate native-language identity. All indexable pages use its absolute primary-host URL, correct dimensions, meaningful image alt metadata, `og:site_name`, and Twitter large-image metadata. The live image returned HTTP 200 and `image/png` (31,556 bytes).
- Decorative artwork remains decorative; meaningful image/link accessible names are checked. Descriptive guide links and contextual links support navigation without JavaScript.

Metadata follows Google's guidance for [descriptive title links](https://developers.google.com/search/docs/appearance/title-link) and [page-specific snippets](https://developers.google.com/search/docs/appearance/snippet). No arbitrary character-count limits are enforced. Google may choose different title links or snippets. Canonical/sitemap policy follows [canonical consolidation](https://developers.google.com/search/docs/crawling-indexing/consolidate-duplicate-urls) and [sitemap guidance](https://developers.google.com/search/docs/crawling-indexing/sitemaps/build-sitemap). Link discovery follows [crawlable links](https://developers.google.com/search/docs/crawling-indexing/links-crawlable); error exclusion uses [noindex](https://developers.google.com/search/docs/crawling-indexing/block-indexing), rather than relying on a [robots.txt crawl block](https://developers.google.com/search/docs/crawling-indexing/robots/intro).

## Structured data assessment

Added one small homepage `WebSite` JSON-LD block: name `VOID Language`, alternate name `VOID`, canonical URL `https://voidlang.com/`, stable `#website` identifier and language `en`. It describes the visible site identity, following Google's [site-name documentation](https://developers.google.com/search/docs/appearance/site-names). The existing Content Security Policy remains unchanged; normal browser checks recorded no CSP/console errors on the homepage.

Validated JSON syntax and every introduced field/value through the standard-library checker, including homepage location, canonical identity and an explicit reviewed-property allowlist. Negative fixtures reject malformed JSON and fabricated schema types. No Organization, SoftwareApplication, product, price, review, rating, release, date or FAQ claims were introduced. This is local validation, not an external schema-validator certification. Google's Rich Results Test does not support site names, so no Rich Results Test success is claimed. Owner URL Inspection after deployment is the relevant follow-up.

Considered [breadcrumb markup](https://developers.google.com/search/docs/appearance/structured-data/breadcrumb). The existing site has navigation/guide sidebars but no displayed breadcrumb trail; no synthetic breadcrumb data or visual redesign was added. Kept markup consistent with actual content and Google's [structured-data policies](https://developers.google.com/search/docs/appearance/structured-data/sd-policies). Structured data does not guarantee any search appearance.

## Live deployed baseline

Read-only public HTTP observations made during this sprint, before owner deployment of these changes. Status/redirect/body evidence is saved in `SEO_LIVE_CHECK.json` outside the public output. These observations verify the existing deployed site, not publication of the new local metadata.

| Requested URL | Observed response |
| --- | --- |
| https://voidlang.com/ | 200 |
| https://voidlang.com/docs/ | 200 |
| https://voidlang.com/docs/generics/ | 200 |
| https://voidlang.com/docs/async/ | 200 |
| https://voidlang.com/examples/ | 200 |
| https://voidlang.com/robots.txt | 200 |
| https://voidlang.com/sitemap.xml | 200 |
| https://voidlang.com/assets/social.png | 200 |
| https://voidlang.com/404.html | 308 → 200; https://voidlang.com/404 |
| https://voidlang.com/seo-missing-page-check/ | 404 |
| https://voidlang.net/ | 301 → 200; https://voidlang.com/ |
| https://voidlang.net/docs/generics/ | 301 → 200; https://voidlang.com/docs/generics/ |
| http://voidlang.com/ | 301 → 200; https://voidlang.com/ |
| https://voidlang.com/docs/generics | 308 → 200; https://voidlang.com/docs/generics/ |

The unknown route returned a genuine HTTP 404 with the site's noindex error content. Cloudflare's explicit `/404.html` request redirects to `/404`, which serves the noindex error document with HTTP 200; this reserved error-document route is excluded from indexing and the sitemap. The actual missing-route behavior is correct. The primary HTTPS page requests have no redirect chains. Secondary-domain root and deep paths each use one permanent 301 to the matching primary URL. HTTP-to-HTTPS and the slashless guide redirect each take one hop. No redirect settings were changed; query-string preservation was not independently tested.

Live robots and sitemap returned HTTP 200 with appropriate text/XML content types. All 36 live sitemap URLs reconcile exactly with the local intended URL set. Representative live pages had no `X-Robots-Tag` indexing restriction. This was an ordinary HTTP client check, not a Googlebot crawl or a complete live crawl of all 36 pages.

## Verification completed

| Check | Result |
| --- | --- |
| `python3 website/scripts/check.py` | PASS: all 36 indexable pages plus 404; metadata, headings, assets, canonicals, sitemap, robots, JSON-LD, HTML reachability, links and source/result parity |
| `python3 website/scripts/test_check.py` | PASS: four test methods, including focused negative mutations for SEO and retained example checks; meaningful longer metadata is accepted |
| `python3 website/scripts/build.py` | PASS: static copy build, 115 entries |
| `python3 website/scripts/check.py --directory website/dist` | PASS: same complete checker against generated output |
| `python3 website/scripts/verify_examples.py build/ci-release/voidc --async-build build/ci-release` | PASS: 47 actual result checks and six freshly emitted async AOT builds; all website programs executed |
| Protected-file SHA-256 comparison | PASS: assets, downloadable programs, root examples and internal docs unchanged |
| Examples body comparison to starting tracked version | PASS: byte-identical |
| Representative built browser pages | PASS: homepage, docs index, generics, async, examples and 404 render essential content; no horizontal overflow at observed 941 px width |
| All 15 dropdown choices | PASS: selecting each option shows exactly its matching single panel |
| Theme control | PASS: dark/light toggles work and return to light |
| Site scripts blocked in temporary preview CSP | PASS: all 15 example panels remain visible, selector enhancement stays hidden, ordinary Docs-to-Generics navigation works |
| Mobile static fallback | PASS: docs index and generics essential content/navigation visible, no horizontal overflow at actual observed 287 px width |

The mobile viewport API requested 390 px but the embedded browser reported 287 px; the assertion above records the actual tested width. Temporary preview servers used generated output and deployed headers; the fallback server changed only its temporary response CSP to block site scripts. Browser automation remained available; this was a site-script-blocked fallback test, not a claim that the browser JavaScript engine was globally disabled. No source/deployment CSP was weakened. No permanent server, package or dependency was added.

The extended `check.py` is the single content/SEO validator and uses only Python's standard library. `test_check.py` exercises it against isolated mutated copies; it is not another SEO scoring system. Checks enumerate HTML independently, enforce exact sitemap coverage, detect duplicate/missing/placeholder metadata, verify social assets and dimensions, reject unintended robots restrictions, validate reviewed JSON-LD, resolve links/fragments, and traverse HTML anchors from the homepage. Existing download/source/result checks remain enforced. Human relevance and factual accuracy still require editorial review; metadata scoring is not a substitute.

## Remaining findings and owner-only actions

No unresolved local validation failures. These local changes require owner review and the next owner-controlled Cloudflare deployment. No commits, pushes, branches, deployment, DNS, Cloudflare configuration or Google account changes were performed.

1. Review the page metadata and deploy through the existing owner workflow. Only static `website/dist` contents are public; this audit, scripts and live evidence are excluded by the build.
2. After deployment, confirm the new title/description/social metadata and homepage WebSite JSON-LD on representative live pages; recheck robots, sitemap, image delivery, real missing-route HTTP 404 and secondary-domain redirects.
3. Use Search Console URL Inspection for the homepage and representative guides after publication; request recrawl/indexing only as appropriate. Ownership verification, sitemap acceptance and a queued homepage request were supplied by the owner, not independently inspected here. Sitemap acceptance or a request does not establish indexing. No live Googlebot crawl, Google indexing status, ranking outcome or rich-result eligibility was verified or promised.
