# Owner review

## Scope

Only `/website` was edited. Existing `/docs` and `/examples` were read for
accuracy; they were not modified. Compiler, Runtime, language/editor tests, CMake,
LLVM integration and Git history were not changed. No publication was performed.

## Current deliverables

- Why VOID is now a substantial public explanation of the problems being solved:
  everyday ceremony, readable native development, memory/resource responsibilities,
  native integration boundaries, lost iteration state, source-first feedback,
  portability and consistent rules. It explains the deliberate combination of
  choices and the supplied Python/Go/Lua/PICO-8/GDScript/C# influences.
- Implemented foundations, implemented subsets and future workflow goals are
  distinguished. Automatic on-save reload, broad native loading and package
  management are not represented as delivered features. No unmeasured performance
  comparison or universal claim about other languages is made.
- Documentation now has 30 public guides: eight foundations plus expanded coverage
  of modules, generics, interfaces, receiver methods, properties, callable structs,
  closures, events, cleanup, enum patterns, Unicode strings, nullable values,
  composition, inheritance, initialization, dictionaries, async/await, structured
  groups/cancellation, parallel workers, native integration, tooling and live
  development. Category navigation uses native details disclosures.
- The example browser offers 15 curated programs through a labeled native select,
  with URL fragments, expected results, downloads, explanations and guide links.
  Without JavaScript, all programs remain readable. Async adapter requirements
  and the module example's supporting files are explained in place.
- Working repository examples are copied unchanged where used. The small methods
  guide adapts the existing geometry example into a complete program. Website
  code blocks are checked against their downloadable sources.
- Branding, source-only availability notice, original landscape recreation,
  light/dark themes, static metadata, sitemap and Cloudflare files are retained.
  Public pages contain no internal handoff/review language or Markdown doc links.

## Validation

Passed:

- `python3 website/scripts/check.py`: local and cross-page links/anchors, guide
  source parity, required metadata/headings, branding and content boundaries.
- `python3 website/scripts/verify_examples.py build/ci-release/voidc --async-artifacts build/order1/production`:
  28 executions (25 synchronous website programs including an error-path case,
  and three existing test-suite async AOT examples), three compiler async source
  checks, and byte parity for unchanged repository copies. No compiler rebuild.
- `python3 website/scripts/build.py`: isolated public deployment output.
- In-app Chromium: all 30 guides checked at 390 and 1440 px, 60 layout checks with
  correct headings/sidebar state and no horizontal document overflow.
- All 15 dropdown options tested: exactly one corresponding panel visible and
  the URL fragment updated correctly. A direct async fragment selected its panel.
- Why VOID checked visually on desktop and mobile; async sidebar/current-topic
  visibility verified on mobile. No warning/error logs observed in guide checks.

Review screenshots: `review/why-void-desktop.jpg`, `review/why-void.jpg` (full page),
`review/why-void-mobile.jpg`, `review/guide.jpg`, `review/examples.jpg`.

## Remaining limitations

Guides introduce supported behavior; they are not an exhaustive formal reference.
Native host bindings, async execution and future workflows have their stated
boundaries. There are no public builds or promised release dates.

Safari/Firefox, physical devices and a full assistive-technology audit were not
performed. Publication, Cloudflare deployment, DNS/TLS/redirects and production
header verification remain owner-controlled. Stopped for owner review.
