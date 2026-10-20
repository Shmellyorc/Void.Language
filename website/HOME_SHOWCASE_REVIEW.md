# Homepage showcase and dedicated roadmap review

Completed October 20, 2026. Website-only work; ready for owner review. Nothing committed, pushed, branched, deployed or changed in Cloudflare, DNS, Search Console or hosted CI.

## Content and structure

The homepage keeps the existing hero shell, mountain artwork, logo, tagline, dark editor window, terminal and closing section. Its primary action is now **Explore VOID**, linked to `/examples/`, without the plain right arrow. **Read the docs** retains its styled diagonal arrow. The new hero program counts qualifying scores using an inferred collection, a loop and a condition rather than the old arithmetic fixture.

Below the hero, six existing feature-card structures introduce native execution, expressive types, collections/patterns, managed memory versus deterministic resource cleanup, cooperative async/task groups, and restricted native workers. Two complete short programs demonstrate generic inference and enum payload matching, with verified input/results, source downloads and links to their respective guides. A developer-experience section explains compiler-owned code intelligence and actual editor differences. A short future-facing teaser replaces the large roadmap grid and preserves the legacy `/#roadmap` target.

The dedicated `/roadmap/` page has three intentionally broad groups:

- Implemented foundation: compiler/language/runtime capabilities and their explicit boundaries.
- Current development: compiler optimization, usability, compatibility/infrastructure and locally implemented tooling awaiting broader validation.
- Future direction: standard library, packages, broader native integration, automatic live-development watching and a more complete developer experience.

It promises no dates, percentages or releases and exposes no internal stage numbers, commit hashes, CI run IDs or development diary. Navigation links directly to `/roadmap/` on every page with the shared navigation. The minimal 404 document has no shared navigation and remains untouched.

## Accuracy and source evidence

Read the current implementation and acceptance documents, rather than relying on older planning-only sections:

- Repository `docs/STAGE7_IMPLEMENTATION.md`: actual completion, hover, signatures, navigation, symbols, guarded rename, conservative formatting and client safety boundaries.
- Repository `docs/stage7/ACCEPTANCE.md`: local Linux editor execution, pending hosted acceptance and unverified client/platform/distribution combinations.
- Repository `docs/EDITOR_INTEGRATION.md`: VS Code/VSCodium, Neovim and Vim setup and their actual capability differences.
- Repository `docs/KNOWN_ISSUES.md`: current limitations distinguished from historical planning statements.
- Existing language guides/features and the actual compiler: language/runtime contracts and executable source.

Public text distinguishes local implementation from public release and platform certification. VS Code, Neovim and Vim have local Linux execution evidence. Vim rename remains disabled; VSCodium execution, new Windows/macOS editor combinations and clean-machine distribution remain unverified. Hosted cross-platform acceptance is pending. No editor tests or hosted compiler acceptance were run by this website sprint; those statements describe the existing recorded implementation evidence.

## Executed showcase programs

Source files live under `website/public/showcase/`, independently of the protected Examples page and documentation samples. All previous manifest rows are unchanged; three showcase rows were appended to the existing `guide-examples.json`.

| Source | Concept | Actually executed cases |
| --- | --- | --- |
| `scores.void` | Inferred collection, iteration, comparison and counting | minimum 18 returns 2; 0 returns 3; 30 returns 0 |
| `choose.void` | A generic conditional choice, concrete type inference and a stock fallback | stock 5 returns 5; 0 returns 0; -2 returns 0 |
| `shipment.void` | Enum alternatives, typed payload and exhaustive matching | items 5 returns 5; 0 returns 0 |

Every case ran through both optimized and unoptimized JIT using the existing `build/ci-release/voidc`. No imagined standard-library APIs, print function or implicit async execution were introduced. The hero terminal shows the actual CLI command/result for its exact source. Source display, terminal command/result and below-hero result/download parity are enforced in the existing standard-library checker.

## Verification

| Check | Actual result |
| --- | --- |
| `python3 website/scripts/check.py` | PASS: 37 indexable pages plus the noindex 404; all existing SEO/link/source/result checks and new homepage/roadmap contracts |
| `python3 website/scripts/test_check.py` | PASS: six methods, including negative cases for hero action decorations, terminal command/results, source parity, roadmap navigation, canonical metadata and sitemap coverage |
| `python3 website/scripts/verify_examples.py build/ci-release/voidc --async-build build/ci-release` | PASS: **191 result checks** and **12 freshly emitted async AOT builds**; includes all prior 175 result checks plus 16 new showcase executions |
| `python3 website/scripts/build.py` | PASS: 156 static entries copied from public sources |
| `python3 website/scripts/check.py --directory website/dist` | PASS against the generated output |
| `git diff --check` | PASS |
| Theme regression probe | PASS: ten scenarios covering absent/invalid/saved preferences, dark/light OS preference, storage failure and toggles; the protected theme source itself is unchanged |
| Desktop preview | Homepage/Roadmap reviewed at actual observed 1412 px width; native hero columns and artwork preserved, no page overflow, all code panels contained |
| Mobile preview | Homepage/Roadmap reviewed at actual observed 390 px width; stacked hero/cards, readable code and contained horizontal code scrolling, no page overflow |
| Theme persistence | Dark selected on the homepage remained dark on navigating to Roadmap; returning to light remained light across navigation |
| Navigation | Mobile menu opened/closed; Roadmap link, page section jumps and homepage teaser link worked |
| Legacy teaser anchor | `/#roadmap` reached the teaser at observed top 112 px after smooth scrolling |
| Examples selector | All 15 options selected, each showing exactly its matching single panel |
| Browser console | No error/warning messages observed on the reviewed homepage/legacy-anchor tab |

Browser viewport overrides are scaled by the embedded browser; the widths above are actual DOM observations, not inferred from requested viewport dimensions. Private screenshots in `website/review/`: `home-showcase-desktop.png`, `home-showcase-mobile.png`, `roadmap-desktop.png`, `roadmap-mobile.png`. These screenshots are not public site assets.

## Preservation and SEO

Pre-sprint comparisons confirm:

- All public artwork, CSS, JavaScript, fonts/icons, original downloads, robots and headers remain byte-identical. No new public images, CSS rules, fonts, effects, framework or dependency were added.
- The existing theme fallback is still light, independent of OS preference; explicitly saved choices and manual toggling remain unchanged.
- All 30 guides, the docs index, Examples page and other existing non-home pages differ only in the authorized Roadmap navigation destination. Their code blocks, content, downloads, metadata and interactions are otherwise unchanged. The 404 is entirely unchanged.
- All existing downloadable `.void` files and all previous manifest entries are unchanged. The menu/dropdown implementation is byte-identical because the entire JavaScript file is unchanged.
- All existing HTML heads, including homepage title/description/social metadata and WebSite JSON-LD, are unchanged.
- Non-website tracked and non-ignored untracked files recorded in the pre-sprint snapshot, including pending compiler/LSP work and repository-root documentation/examples, retain their original hashes.

Roadmap has a unique descriptive title and description, one primary H1, logical heading levels, self-canonical `https://voidlang.com/roadmap/`, matching Open Graph URL/title/description, shared social image/alt and Twitter metadata. The sitemap now covers exactly **37 canonical indexable URLs**; the existing 36 URLs remain and Roadmap is the only additional page. No speculative modification dates were added. Robots and existing structured data are unchanged. Ordinary HTML links reach every intended page without requiring site JavaScript.

Only content, source examples, navigation, the new page, sitemap and directly associated verification/documentation changed. No visual exception requiring owner approval was needed. Temporary localhost preview is stopped at completion. Owner review and publication remain the next steps; tooling hosted acceptance remains an existing separate project boundary.
