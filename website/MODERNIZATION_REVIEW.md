# VOID website modernization review

4 November 2026. The public site now contains 43 documentation guides and 27 gallery programs. Main navigation opens the syntax guide. The full index is available from every guide sidebar and branded footer. The site still uses the existing static HTML, CSS, JavaScript, artwork, themes and responsive components.

All GitHub actions lead to `/github/`. The page explains that VOID Language is not ready for public release yet and that the project repository link will be provided when public access is officially announced, links home, declares `noindex` and is excluded from the sitemap. The stable internal route gives maintainers one landing page to update when public access is officially announced. The checker rejects repository URLs in public text assets and GitHub anchors with other destinations.

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

## Getting Started correction

The landing page now opens with “Start exploring VOID.” and explains compiler availability. Three linked cards lead to Syntax Basics, Examples and the Roadmap. The separate Compiler Workflow guide now identifies a compatible development build as its prerequisite. The landing page uses the existing documentation intro, feature grid and topic cards.

- On the built site, each card was clicked and its destination heading verified. All three routes matched their intended paths. Both source and built HTML contain the three learning destinations and no compiler commands in the landing-page content.
- Light and dark themes were checked at desktop and mobile sizes. At a measured viewport width of 1,239 CSS pixels the three cards shared one row. At 327 pixels they formed one column. Page width equaled viewport width in all four checks. [Desktop preview](review/getting-started-desktop.jpg) and [mobile preview](review/getting-started-mobile.jpg).
- Source and built-site checks passed for 50 indexable pages, 50 sitemap URLs and two noindex pages. The build produced 215 entries. All eight checker regression tests passed in 58.583 seconds. Example sources were unchanged in this correction.
- Correction files are the Getting Started HTML and Compiler Workflow HTML under both `public/` and `dist/`, this review, and the two preview images above.

## Original pages reviewed

Language behavior was checked against the compiler's current entry-selection contract, language and Runtime documentation, the actual standard sources, compiler/LSP implementation and the examples executed by the verifier. The platform claims use the recorded seven-job HTTP results and the logging investigation. Each row identifies the subject reviewed. Useful explanations and programs were retained when accurate.

| Original guide | Review |
| --- | --- |
| Syntax | Automatic entry selection, numeric adapter output, process status, executable statements and file globals |
| Variables | Module constants, local inference, reassignment and readonly views |
| Types | Checked conversions, wrapping arithmetic, scalar bounds and clamp |
| Functions | Pure defaults, named arguments, evaluation order and bounded factorial fragment |
| Control flow | Loop forms, Boolean conditions, break, continue and typed branch expressions |
| Modules | Project paths, standard paths, real io/memory import and directory entry discovery |
| Collections | List aliasing, fixed-array copies, recoverable pop/get and negative indexes |
| Dictionaries | Insertion order, result lookup, mutation and missing-key behavior |
| Strings | Interpolation, scalar versus byte counts and validated UTF-8 conversion |
| Structs | Value-copy and managed-reference examples, GC versus deterministic cleanup |
| Initialization | Required fields, custom initializers and value children |
| Methods | Receiver mutation, named arguments and readonly method access |
| Composition | Promoted components, explicit paths and type relationship predicates |
| Inheritance | Managed base identity, virtual dispatch, override and super |
| Interfaces | Structural method requirements and concrete receiver dispatch |
| Generics | Type inference, concrete specialization and comparison requirements |
| Properties | Computed getter, absent setter and receiver restrictions |
| Callables | Call receivers and independent value copies |
| Closures | Captured bindings, aliasing and shared closure state |
| Events | Synchronous ordered delivery, subscription identity and emission authority |
| Nullable values | Optional managed references and callable presence matching |
| Patterns | Variant payload captures, exhaustive choices and result patterns |
| Errors | Recoverable results, explicit fallback and propagation |
| Cleanup | Scope exit, defer order, one-shot release and GC boundaries |
| Async | Queued task creation, repeated await, retirement after quiescence and await propagation |
| Task groups | Cancellation and joining, lexical ownership and retained results |
| Parallelism | Bounded native workers, eligible functions and restricted value transfers |
| Native integration | Explicit trusted host, static providers, resource and callback ownership |
| Editor tooling | Compiler-owned diagnostics, documentation hover, signature help, scoped rename and actual editor coverage |
| Live development | Compatible body replacement, generation lifetime, debug cache and watcher limits |

| Original public page | Changes reviewed |
| --- | --- |
| Homepage | Docs entry, current editor coverage, standard-library links and GitHub actions |
| Why VOID | Application entry, source diagnostics and implemented versus future capabilities |
| Features | LLVM 23.1.4, application execution, async retirement and library workflows |
| Examples | Gallery selection, independent teaching sources, downloads, expected results and CLI commands |
| Getting Started | Compiler availability and three learning destinations. Development commands are documented in Compiler Workflow. |
| Roadmap | Separate implemented, hosted-verified, pending acceptance, known limits and future work |
| Documentation index | Thirteen new topic cards, revised metadata and full guide navigation |
| 404 | Existing noindex and homepage recovery preserved |

## New guides and examples

The library and applications group covers math/random, collection helpers, text/encoding, binary/memory streams, files/path operations, OS directories, logging, typed channels, URLs/HTTP, compiler workflow, console output, JSON and calendar time. Examples include a reproducible die draw, a bounded file read with a short-read control, an in-memory binary field, a loopback HTTP request that checks its actual status, an exact console stdout check and invalid leap-day handling.

Twelve former repository-copy registrations were replaced with independently authored gallery programs. The gallery now exposes previously unlisted topics. The small preserved architecture download has an explicit source-parity and execution record. Module dependencies are kept beside their downloadable entry files. Contextual fragments compile in their declared wrappers. The trusted native-integration guide describes its host requirements and makes no claim to execute an arbitrary binding through the CLI.

The verifier uses the matching LLVM 23.1.4 native link manifest, stages the exact sources and support modules outside public assets, and executes synchronous cases in both optimization modes. Async cases use the existing explicit async emitter and newly linked executables. The two fixture-dependent guides use disposable local files or a private loopback server.

## Verification

Run from the repository root:

```sh
python3 website/scripts/check.py
python3 website/scripts/test_check.py
python3 website/scripts/verify_examples.py build/llvm23-release/voidc --async-build build/llvm23-release
python3 website/scripts/build.py
python3 website/scripts/check.py --directory website/dist
```

- Source and built-site checkers pass for 50 indexable pages, 50 sitemap URLs and two noindex pages. Download/display parity, results, links, anchors, heading hierarchy, metadata, social assets and HTML reachability pass.
- Checker regression suite passes all eight test methods, including deliberately broken SEO, source, result, navigation and GitHub privacy fixtures.
- The verifier passed 310 result checks across 84 complete programs and five contextual fragments, with 12 fresh async AOT builds. The [verification receipt](review/modernization-verification.json) preserves the manifest identity and command outputs. The earlier invalid nullable-integer gallery example failed compilation and was corrected to use a nullable managed reference. The compiler also diagnosed a seed parameter shadowing an imported declaration. The corrected example uses a distinct local name.
- Browser QA used the built local site. All 103 rendered GitHub anchors across 51 branded pages opened the availability page. Desktop and mobile page widths matched their viewports on all 102 layout checks. Code panels stayed within the viewport. Mobile menu → Docs, sidebar navigation, desktop table of contents, gallery selection, homepage return and dark-theme persistence through normal link navigation were exercised. [Layout receipt](review/modernization-browser.json), [desktop preview](review/modernization-availability.jpg), [mobile preview](review/modernization-mobile.jpg).
- The [changed-file inventory](review/modernization-files.json) lists exact website source, scripts, review files and evidence. Generated `website/dist` is the local review build.

Execution used Linux x86_64 with the existing Release compiler. Browser checks used the local Chromium-based in-app browser with desktop and mobile viewport overrides. Platform-specific language claims reflect repository acceptance records. This sprint did not perform native Windows/macOS compiler testing, physical-device testing, assistive-technology certification or live hosting validation. Logging performance and release holds and Windows Schannel P12 acceptance remain open.

All changes are under `website/`. No publication, deployment, repository-visibility change or Git write operation was performed. Ready for independent source review and Boss approval.
