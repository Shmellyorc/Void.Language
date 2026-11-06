# Integrated performance staging preparation

Boss-authorized staging only. Branch **`main`**, HEAD
`f3cf240e0b14240d9c7486c8882842bfa7043c63`. Suggested commit title:
**Retain integrated logging and shared internal performance repairs**.

| Scope | File count |
| --- | ---: |
| Initially classified changed/untracked files | 11,863 |
| Initial tracked modifications / untracked | 35 / 11,828 |
| Current classified files, including preparation records | 11,894 |
| Exact approved staging set | 126 |
| Excluded files, preserved locally and left untracked | 11,768 |
| Pre-existing staged changes | 0 |

The [explicit staging manifest](evidence/integrated-performance/STAGING_MANIFEST.txt)
lists individual literal paths, including this receipt and the manifest itself.
No directory, wildcard or blanket add is used. Categories: 17 documentation, 5 fixture, 5 historical patch evidence, 23 production, 10 qualification/instrumentation helpers, 66 text receipts/data/manifest.

Production and fixture hashes exactly match the approved
[source inventory](../../experiments/integration-qualification/evidence/source-inventory.json).
No performance patch was reapplied and no production/test/benchmark input changed.
The preparation changes documentation links and publishes selected evidence only;
no optimization research, compilation, correctness test or benchmark was run.

The [consolidated qualification](INTEGRATED_PERFORMANCE_QUALIFICATION.md) preserves
85 Release / 85 Debug / 84 strict test passes, the original advanced-test failure
and semantic replacement, editor startup uncertainty, performance regressions and
Linux-only scope. The original FILE200 10 ms aim remains unmet and is not a
Boss-mandated release requirement. [Automatic SIMD](../DEVELOPMENT_STRATEGY.md#future-optimization-automatic-simd--boss-direction-5-november-2026)
is documented/indexed as future work only; no SIMD implementation is included.

The [artifact scope and publication mapping](evidence/integrated-performance/LOCAL_ARTIFACTS.md)
identify local-only binaries, archives, candidate copies and disposable runs.
Original documents affected by link updates are preserved byte-for-byte locally,
with original/published hashes recorded. Selected raw JSON/CSV, JUnit, qualification
receipts and three durable historical CTest-log copies keep report links usable
without committing the experiment trees. Unrelated investigations remain excluded.

Static preparation checks cover the complete explicit set: prospective-checkout
Markdown targets/anchors, exact approved source hashes, excluded-file presence and
sizes, Git index preservation, whitespace, text/binary classification and common
private-key/token/credential patterns. The actual staged blobs and index entries
are checked after adding the manifest. This is a bounded secret-pattern scan,
not a claim that all possible sensitive information can be detected automatically.
The default whitespace checker reports 106 notices confined to five preserved
historical evidence/helper files: literal patch-context spaces, CRLF CSV rows
and one helper's final blank line. Their bytes are intentionally unchanged;
production, fixtures and documentation pass the whitespace check. These notices
are not a source-formatting change or a staging blocker.
Root README is unchanged. No unrelated staged changes were present.

Final index verification: **126 files** (35 modified, 91 added), approximately
**4.84 MB** of staged contents; largest blob **563,187 bytes**. All **554** checked
local Markdown links/anchors resolve in the staged/tracked checkout. Eleven
credential/private-key pattern rules found no matches; no binary files are
staged. All **66,233** unselected existing index entries are unchanged, and there
are no unstaged tracked changes. **No staging blocker was found.**

## Boss CI instructions

After Boss reviews, commits and pushes, use
[**Compiler link experiment**](../../.github/workflows/ci-link-experiment.yml)
(`ci-link-experiment.yml`), mode **`thinlto-cache`**, all **seven** jobs:
Linux Debug/Release/Sanitizers, Windows Debug/Release and macOS Debug/Release.
Do not use the slower standard workflow. No CI was triggered here.

No commit, push, branch creation, history rewrite, deletion, reset or release
publication is part of this preparation. Native Windows/macOS qualification of
the integrated patch remains for the Boss-controlled matrix. Stop for Boss review.
