# Examples cleanup review

Only `/website` was edited. Repository `/examples` and `/docs` remain unchanged.
No compiler, runtime, CI, Git history, publication or Cloudflare changes.

## Changed teaching programs

| Example | Focus | Input → result |
| --- | --- | --- |
| Functions & recursion | Factorial and its base case | 5 → 120 |
| Readable loops | Sum odd numbers through a limit | 5 → 9 |
| Growable lists | Append a score, then total the list | 5 → 35 |
| Generics | Return the larger of two typed values | 5 → 10 |
| Properties | Compute a rectangle's area | 5 → 15 |
| Events | Send one sensor reading to a display | 5 → 5 |
| Scoped cleanup | Own an in-memory workspace with `with` | 5 → 10 |
| Task groups | Wait for two scoped progress updates | 5 → 11 |
| Parallel workers | Independently square two inputs | 5 → 61 |

The introductory panel now reuses the existing clear syntax example (10 → 52).
Closures, value/reference behavior, results, basic async and modules were already
small and relevant, and were preserved. The nine changed guides and their source
links/explanations were synchronized. Duplicate factorial/loop downloads were
consolidated into canonical website files. The existing visual design, native
selector, fragment URLs, guide navigation and static architecture were preserved.

## Verification

- Content, links, anchors, source parity, metadata and static build passed.
- Visible input/result pairs are now validated against the manifest as well as
  against the actual downloadable source and result metadata.
- `verify_examples.py build/ci-release/voidc --async-build build/ci-release` passed:
  47 result checks, including focused edge cases, and six fresh async AOT builds.
- Async source is emitted from the actual website downloads through the existing
  explicit adapter in normal and optimized modes, linked against existing matching
  runtime archives and executed. No stale fixture executables are substituted.
- The manifest explicitly distinguishes independent teaching programs from retained
  repository copies. Copy parity remains enforced for the latter; execution remains
  mandatory for both. Missing async tools fail verification instead of skipping it.

- Browser checks: all 15 dropdown choices select one matching panel; the linked
  function guide shows the canonical factorial program and result. Updated guide
  pages were checked for heading/layout overflow. A final 871 px preview also had
  no document overflow. Browser navigation timed out during two batches, so those
  checks were resumed from the loaded pages rather than treated as passes.
- Screenshot: `review/examples-cleanup.jpg`. The existing styles and dropdown
  behavior were retained; no CSS or interaction code was changed for this cleanup.
- A deliberately incorrect displayed result was rejected by the content checker;
  the original page was restored before final validation/build.

## Language boundaries

- Async entry functions need the explicit async adapter; the synchronous CLI entry
  adapter does not run them. The site does not add a new compiler command.
- Factorial uses small non-negative i32 inputs; results beyond 12! wrap.
- Cleanup intentionally models ownership in memory. It invents no file, buffer or
  native-library API. Real external resources need their supported binding release.
- Worker inputs remain within the implemented independent scalar/value subset.

Public release/deployment and broader browser/assistive-technology testing remain
outside this local website cleanup. Stopped for owner review.
