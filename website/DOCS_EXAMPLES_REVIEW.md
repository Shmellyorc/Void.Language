# Documentation examples review

Completed October 20, 2026. Scope: website documentation, owned sources, existing validation tooling and related website documentation. The owner additionally requested a light theme default; that narrowly changes the theme fallback in the shared assets.

## Deliverables

Reviewed and updated **all 30 language guides**. Added **26 complete programs, five explicitly labeled fragments and two supporting module files** under `public/docs/snippets/` (33 source files). Twenty-nine guides have independently maintained code; native integration deliberately remains explanatory because the current binding subset requires an explicitly trusted embedding host. A small native-boundary notice makes that absence explicit.

Complete programs advertise an adapter and a returned integer. Fragments identify their context and use “Download fragment”; they do not advertise standalone CLI results. The module guide displays all three sources and the necessary file layout. Syntax highlighting, keyboard-focusable/selectable code blocks, download attributes, guide/sidebar navigation and existing styles remain in place.

All documentation HTML heads are byte-identical to their pre-sprint snapshots: titles, descriptions, canonical and social metadata are preserved. Existing sitemap, robots, WebSite JSON-LD, non-documentation HTML, artwork and Examples downloads are unchanged. The only CSS/JavaScript edits implement the explicitly requested theme default: light with no saved choice, independent of OS preference, including without site JavaScript. Explicitly saved dark/light choices and the manual toggle still work. The dropdown/menu/navigation code is unchanged.

## Before and after

- **Dictionaries:** removed the regression-style mixture of duplicate keys, order assertions, repeated removal/clear and negative failure codes. The new six-line program updates notebook stock, inserts folders and returns the requested quantity. Advanced operations are explained separately.
- **Strings:** replaced nested carriers, snapshots and a compound assertion with a four-line local UTF-8 greeting fragment. Its resulting string is `Hello, 世界!`, verified inside an explicit compiler context rather than presented as a printing API.
- **Structs:** replaced a single arithmetic combination of value and object observations with two small programs. One returns the unaffected original coordinate; the other returns the updated shared basket count.
- **Functions:** the main program teaches two typed parameters and a subtotal. A separate factorial declaration teaches recursion and its base case without duplicating the Examples page’s complete factorial program.
- **Callable structs / patterns / interfaces:** removed combined fixture features, sentinel failures and redundant checks. Each now centers on one operation or contract.
- **Modules:** replaced assumed supporting files with a visible three-file pricing example and separate downloads for every source.
- **Async / workers:** independently authored ticket and sum-of-squares programs retain the explicit adapter requirement. No invented network/filesystem API is implied.

Standard declarations can naturally resemble other examples (for example a rectangle area getter); their teaching contexts, full programs and explanations are independently maintained. None of the new sources is token-identical to an existing Examples-page program. Editorial review, rather than renaming or an arbitrary line limit, determined the new teaching structure.

## Per-guide review

Inputs/results below refer to complete programs. Fragment validation outputs are private test observations and are not shown as standalone program results on public pages.

| Guide | Owned source(s) | Presentation / primary result | Teaching purpose |
| --- | --- | --- | --- |
| async | `async.void` | program 5 → 63 | A ticket-total task plus a booking fee, with an explicit await and host-adapter explanation. |
| callables | `callables.void` | program 12 → 9 | One coupon value with one call method; no generic helper or unrelated counter. |
| cleanup | `cleanup.void` | program 5 → 5 | An in-memory workspace reservation restores its owner’s available slots on scope exit. |
| closures | `closures.void` | program 5 → 10 | A step counter: capture, escape and shared closure state without summed test observations. |
| collections | `collections.void` | contextual fragment | Create an empty typed list, append three scores and total them in a local fragment. |
| composition | `composition.void` | program 5 → 15 | Promote one position component’s method and field through a cursor. |
| control-flow | `control-flow.void` | program 5 → 9 | Sum a short list of odd numbers up to a limit, using if and break. |
| dictionaries | `dictionaries.void` | program 5 → 7 | Supply quantities: literal creation, known-key lookup, update and insertion. |
| editor-tooling | `editor-tooling.void` | contextual fragment | A typed function declaration as a navigation/parameter target, with existing tooling boundaries preserved. |
| errors | `errors.void` | program 4 → 6 | A recoverable average calculation, with explicit invalid-count handling and documented fallback. |
| events | `events.void` | program 5 → 6 | A sensor sends successive readings to a display; synchronous delivery leaves the latest reading. |
| functions | `functions.void`, `factorial.void` | program 5 → 60; contextual fragment | Two-parameter subtotal program; separately labeled recursive factorial declaration. |
| generics | `generics.void` | program 5 → 18 | Reuse a larger-of-two operation to calculate a temperature peak; explain inference and explicit arguments. |
| inheritance | `inheritance.void` | program 5 → 15 | Express shipping overrides a base cost and adds a surcharge through a base view. |
| initialization | `initialization.void` | program 5 → 6 | Default label initialization nested inside a package; constructor and field defaults. |
| interfaces | `interfaces.void` | program 5 → 5 | One readable contract and one fuel-tank implementation, read through a const interface parameter. |
| live-development | `live-development.void` | contextual fragment | A function-body compatibility illustration; no automatic watcher or hot-reload session claimed. |
| methods | `methods.void` | program 5 → 50 | Move a point from the origin and measure squared distance through a readonly value copy. |
| modules | `modules/main.void`, `modules/pricing.void`, `modules/delivery.void` | program 5 → 23 | A three-file order total: aliased pricing module, relative delivery import and visible file layout. |
| native-integration | — | Host boundary; no standalone source | Explicit trusted-host boundary; no unsupported standalone native grammar or fabricated executable. |
| nullable | `nullable.void` | program 5 → 10 | An optional adjustment callback; present calls double, absent preserves the original number. |
| parallelism | `parallelism.void` | program 5 → 55 | One worker sums squares using private scalar locals; async parent awaits the result. |
| patterns | `patterns.void` | program 5 → 5 | Pickup or shipped fee: one enum, one payload and an exhaustive match. |
| properties | `properties.void` | program 5 → 27 | Extend a rectangle and read its freshly computed area. |
| strings | `strings.void` | contextual fragment | Four-line local greeting: UTF-8 literal, concatenation and rebinding. |
| structs | `value-copy.void`, `shared-object.void` | program 5 → 5; program 5 → 8 | Separate complete programs isolate independent value copying and shared basket identity. |
| syntax | `syntax.void` | program 4 → 12 | Box capacity: comments, indentation, typed input and a returned calculation. |
| task-groups | `task-groups.void` | program 5 → 240 | Chapter and appendix calculations owned by a group; collect their independent results. |
| types | `types.void` | program 5 → 300 | Minutes to seconds using explicit i64/i32 conversions. |
| variables | `variables.void` | program 5 → 17 | An item subtotal, delivery fee and readonly amount due. |

## Validation and preservation

The original 27 manifest rows are exactly preserved, including their cases, maintenance policies and async paths. New documentation rows are appended to `scripts/guide-examples.json`, with `guide` and `kind`. Fragment rows contain a small explicit `context_before` / `context_after` wrapper and optional indentation; verification compiles the exact fragment bytes inside it. Module support sources stay alongside their main file, so real import resolution is exercised. No extra validation framework or dependency was introduced.

The existing `check.py` additionally enforces documentation-owned paths, per-guide display/manifest coverage, matching downloads, exact displayed source, full-program-only result bars, explicit fragment labeling/context, support-module inventory and token-based duplicate rejection against Examples programs. Retained SEO, source-copy, result, link and asset checks still run. The regression tests exercise ownership violations, duplicate programs, unlabeled fragments, missing support modules and mismatched downloads in isolated temporary copies.

| Verification | Actual result |
| --- | --- |
| `python3 website/scripts/check.py` | PASS: every public page, SEO/link/source/result parity and documentation ownership checks |
| `python3 website/scripts/test_check.py` | PASS: five methods, including focused documentation and prior SEO regression mutations |
| `python3 website/scripts/verify_examples.py build/ci-release/voidc --async-build build/ci-release` | PASS: **175 result checks**, **12 fresh async AOT builds**; all original and new website programs executed |
| Documentation execution paths | Every synchronous complete program and fragment context ran in optimized and unoptimized JIT modes; all three new async programs emitted, linked and ran in normal and optimized AOT modes |
| Existing execution paths | Original 47 result checks and six async AOT builds remain included without modification |
| `python3 website/scripts/build.py` | PASS: 150 static entries; only public assets/content copied |
| `python3 website/scripts/check.py --directory website/dist` | PASS against actual generated output |
| `git diff --check` | PASS |
| Pre-sprint SHA-256 snapshots | PASS: Examples HTML, all original source downloads, artwork, root examples/docs and other public HTML unchanged; original manifest rows preserved |
| Documentation SEO head snapshots | PASS: all existing documentation heads exactly unchanged |
| Theme checks | PASS: ten isolated JavaScript scenarios covering dark/light OS preference, absent/invalid/saved choices, storage failure and toggling; CSS system-dark fallback removed |
| Browser preview | PASS: strings, dictionaries, structs, modules, async and functions at observed 941 px; modules wide layout at 1412 px shows sidebar and table of contents without page overflow |
| Mobile browser preview | PASS: functions, strings, modules and async at actual observed 390 px; code remains selectable, focusable and contained, with internal scrolling for long lines; menu opens/closes |
| Download | Strings fragment browser download succeeded; all public download/source bytes checked for parity and all source links resolve |
| Existing Examples dropdown | All 15 options selected in the browser, each displaying exactly its matching panel; page HTML and every source download remain byte-identical |
| Manual theme | Browser toggle reached dark and returned to light; default preview loaded light |

Private screenshots: `review/docs-modules-desktop.png` and `review/docs-async-mobile.png`. The browser viewport override reported scaled values, so actual observed widths are recorded above. Temporary localhost preview and validation wrappers are QA aids, not site infrastructure. No existing copy button was removed or invented; code retains native selection/copy behavior and download links.

## Language boundaries and remaining work

No unsupported syntax or unresolved compiler/check failures were found in the new samples. The compiler used was the existing `build/ci-release/voidc`; no compiler/runtime/CI changes or rebuilds were made.

- Async programs require the explicit async host adapter; ordinary synchronous CLI entry execution does not run them. Tasks have no public type spelling, and worker transfers retain their restricted value boundary.
- Fragments require their described enclosing context. Tooling and live-development fragments demonstrate source syntax, not an editor release, automated watcher or an actual runtime replacement session.
- Native integration still requires the trusted embedding host. No arbitrary-library syntax or normal CLI/LSP authorization is claimed.
- Readonly capability, managed/value identity and resource-lifetime restrictions remain explained. Cleanup models an in-memory reservation and assumes capacity at least two; no file/native API is fabricated. GC does not invoke `deinit`.
- Factorial uses non-negative inputs and i32 arithmetic; results beyond 12! wrap. The control-flow list intentionally stops at nine. The coupon example uses prices at least three. Numeric conversions are checked, and strings remain immutable UTF-8 content with the documented API limits.

Owner review/publication remain pending. No commits, pushes, branches, deployment, DNS, Cloudflare, Google account or hosted CI actions were performed. Previously pending repository work was retained.
