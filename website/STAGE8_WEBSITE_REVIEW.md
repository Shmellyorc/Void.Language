# Website feature update review

Date: 2026-10-21. Read-only compiler/documentation baseline:
`fbad2b642114a692f93d2f4ce7d3465ea489abf2`.

## Delivered

Updated 20 existing public pages, preserving all 30 guides and all 15 previous
Examples choices. The dropdown now has 18 choices. No new page routes or sitemap
entries were needed; canonical/social metadata remains synchronized.

- Home: named/default calls, string-only interpolation and typed if-expressions
  in feature copy; the generic choice demo now uses a typed if-expression.
- Why VOID / Features: connect ease of use with concrete calls, applications,
  recoverable operations, numeric/text facilities and type relationship queries.
- Examples: three short downloadable programs for named/default multiline calls,
  Unicode scalar indexing/interpolation and main-based application execution.
- Guides: functions, receiver methods, types, strings, collections, dictionaries,
  composition, control flow, syntax, modules, async, task groups, native integration
  and editor tooling. Updated the guide index and relevant SEO descriptions.
- Roadmap: implemented capabilities, ongoing platform/editor validation, explicit
  release boundaries and separate future ecosystem work. No internal workflow
  details are published.

Ten new teaching programs and five revised programs/fragments are registered in
the existing manifest. One additional nested support module is downloaded with the
updated pricing module. Documentation samples remain independently maintained from
Examples downloads. Compiler regression sources remain unchanged.

## Application verification

Application manifest rows use `mode: "app"`, `expected_status` and
`expected_stdout`; numeric adapter inputs/results are forbidden for those rows.
The checker requires separate application result metadata and visible status/stdout
labels. The verifier compares raw stdout, process status and empty stderr, so a
returned seven cannot pass as printed seven. Main-based sources also execute via
directory discovery. Three application programs pass optimized/unoptimized runs,
including the intentionally nonzero status seven. New mutation checks reject
incorrect statuses, fabricated output, adapter inputs, wrong modes and arrow
separators. Existing async adapter verification remains mandatory and unchanged.

## Executed checks

All passed:

- `python3 website/scripts/check.py`
- `python3 website/scripts/test_check.py`: 8 test methods, including deliberate
  invalid SEO, source, download, result, ownership, navigation and app fixtures.
- `python3 website/scripts/build.py`
- `python3 website/scripts/check.py --directory website/dist`
- `python3 website/scripts/verify_examples.py build/ci-release/voidc --async-build build/ci-release`:
  **228 result checks, 12 freshly emitted/linked async AOT builds**. All 71
  registered programs/fragments are verified; the extra recoverable-error case is
  retained. Five fragments run in their explicit manifest contexts.
- HTTP byte parity for all **63 unique source-download URLs**; a browser-triggered
  Unicode download also matched the source bytes.
- `git diff --check`.

Browser checks visited all 38 public pages at desktop/tablet width (1059px) and
mobile width (390px). Wider desktop checks at 1412px covered the updated Examples,
Strings guide and homepage. No document-level horizontal overflow, missing H1,
unnamed controls or non-focusable code windows were found. All 18 selector choices
work at both sizes, show exactly one panel and retain their URLs/downloads.
Light is the fresh-origin default; dark toggling persists across navigation. The
existing 404 remains a static CSS-light page without JavaScript/theme controls.
The mobile menu opens and Escape closes it and restores focus. The application
example's guide link reaches the syntax guide with distinct status/stdout bars.
No browser console errors were observed. Screenshots were visually inspected.

Evidence:

- [Desktop examples](review/stage8-examples-desktop.png)
- [Dark desktop strings](review/stage8-strings-dark-desktop.png)
- [Mobile Unicode](review/stage8-unicode-mobile.png)
- [Mobile application statuses](review/stage8-application-mobile.png)
- [Browser measurements](review/stage8-browser-checks.json)

## Preservation and limitations

Hashes of tracked files outside website and website public CSS/JavaScript/artwork
assets match the pre-edit snapshot. No compiler, Runtime, editor implementation,
root documentation, regression examples, tests or CI were modified. Existing hero
artwork/fade, light default, theme controls, arrowless buttons, diagonal icons and
code-window download/result decoration rules remain intact. Generated `dist`
contains only public files; this review and screenshots do not deploy.

No public builds are available. Platform verification and fresh interactive-client
checks of newest tooling additions remain ongoing, not certified. String
interpolation accepts strings only. Standard-source import spelling does not mean
a shipped module catalog or filesystem/printing API. Application status is not
stdout; async application entry/argv APIs are not implemented. Trusted static
providers require explicit host admission and do not form general FFI. Completed
async frames can release their GC roots sooner, while task/results/group and
registration bookkeeping may still scale within a root execution. No performance
or constant-memory promises were added.

No commits, branches, pushes, publication, Cloudflare changes or hosted CI actions.
Ready for owner and reviewer inspection before publication.
