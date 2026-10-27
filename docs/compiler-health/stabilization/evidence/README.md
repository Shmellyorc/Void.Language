# Frozen stabilization evidence

The [manifest](manifest.json) records 892 production/test source hashes, original
and current build inventories, the 48 explicitly rebuilt selected targets,
acceptance receipts, pinned tools, 571 measurement records, retained failed
attempts, portable archives and 26 local frozen compiler/failure images.
[SHA256SUMS](SHA256SUMS) verifies the packet files. The original research archives
under `docs/compiler-health/evidence/` remain unchanged.

| Artifact | Contents |
|---|---|
| `baseline-sources.tar.xz` | Full Git source archive at `1d4ad15f5215838d209e48bb47ac7c3f52b05371`; the manifest also records its uncompressed tar hash. |
| `final-sources.tar.xz` | Frozen final source overlay, including new regressions and unchanged protected root README. Apply over the baseline archive for complete build inputs. |
| `measurements.tar.xz` | Individual commands, stdout/stderr, fixture/oracle bytes, stack/environment/hash records, scripts and first-failure captures. |
| `llvm-ir-native.tar.xz` | Baseline/candidate/final LLVM IR, typed/LLVM inventories, target settings, native objects, pass timings and native stack/section evidence. |
| `verification-incidents.tar.xz` | Acceptance logs/JSON/JUnit, setup/failed-experiment evidence, source/static assembly and classification receipts. |
| `helpers-build-provenance.tar.xz` | Manual helper sources/build commands, ORC instrumentation patch, selected-target build recipe and CMake configuration snapshots. |
| `measurements.jsonl`, `acceptance.json` | Directly readable measurement and final profile indexes. |
| `machine-summary.json`, `root-geometry.json`, `helpers.json` | Derived native/root inventories and exact helper identities. |

Archive members are repository-relative and preserve their original locations.
Extract them into a disposable directory; do not overwrite frozen evidence.
The full baseline archive plus the final source overlay retains unchanged
third-party inputs as well as modified compiler files. The bundled profile
CMake caches describe configuration; configure a new build directory rather
than copying a cache with an old source/build root into place.

The frozen local tree is
`/home/shmellyorc/Documents/Void.Language/build/compiler-stabilization/`.
`baseline/` holds the original three CLI images, source/archive hashes, the
preexisting known-issues patch and the untouched research-documents archive.
`accepted/` holds the final three CLI/LSP images and source snapshot. Separate
first-failure directories retain experimental binaries, including failed hint
frame reductions, descriptor proof failures and measurement-helper failures.
The manifest hashes these images; keep this local tree until Golden Wrench
finishes. Frozen AOT compiler images also use the verified profile Runtime
archives/resources in the original build tree.

Individual commands in `measurements.jsonl` are the exact replay units. The
manual `bench.py` wrapper asserts a hard/soft 1 MiB stack in child pre-exec,
records wall/CPU/RSS/fault/scheduling counters and executable/source hashes,
checks status, and preserves signals/reserved failures before raising.
`final.py`, `dto-matrix.py` and `audit.py` organize the serial runs; source and
helper paths in these local scripts are explicit and should be adapted when
extracting elsewhere. Allocation, compiler-lifetime and ORC instrumentation
helpers are separate from the frozen production compiler.

Example direct replay from the original workspace, with no persistent object
reuse (the subshell confines the stack-limit change):

```sh
(
  ulimit -S -s 1024
  ulimit -H -s 1024
  build/compiler-stabilization/accepted/voidc-release run \
    build/compiler-stabilization/research/benchmark/fixtures/dto128/main.void \
    --no-cache
)
```

Expected stdout is `127` followed by `true`, each on its own line. The primary
CLI timing records identify the original/final images and source hashes. Cache
cold/warm, AOT build/run, current Debug/sanitizer matrix and instrumented helper
samples are distinct labels. Original research and superseded candidate samples
remain in the ledger, with their build identities; they are not silently pooled
into the final comparison. Cold means a fresh process and no reusable native
object, with the ordinary OS page cache left active.

The final profile passes are 94/94 Release, 94/94 Debug and 93/93 strict sanitizer.
The one-profile difference is the existing sanitizer exclusion of
`lsp_const_recovery_stack`. See [verification](../VERIFICATION.md) for strict
flags, targeted 1 MiB coverage and additional dynamic/async emission checks.
The [incident ledger](../INCIDENTS.md) explains every retained failing measurement
or product experiment. A passing retry is not used to close H1–H7.
