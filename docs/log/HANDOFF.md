# VOID logging — mandatory performance repair handoff

**Ready for independent Golden Wrench review. Release remains on hold.** Both
performance investigations and the demonstrated main repairs are complete.
Fresh local correctness validation passed **263/263 selected CTests**. The
synchronous performance screening target is **not met**, so the performance gate
remains **OPEN**. This is not an independent acceptance lock.

Baseline HEAD: `f9ac49f1558f326129948bc06c829263448b7714`.
No staging, commits, branches, pushes, hosted CI or ptrace. The protected root
README matches HEAD and its original SHA-256. No native Windows/macOS pass is claimed.

## Results and review decision

Final original workload, JIT O2, median execution time:

| Workload | Isolated baseline | Final |
|---|---:|---:|
| Small file records / 200 | 267.697 ms | 17.415 ms |
| Equivalent direct file I/O / 200 | 2.922 ms | 2.973 ms |
| Small console records / 200 | 257.119 ms | 14.608 ms |
| Equivalent direct console / 200 | 1.410 ms | 1.414 ms |
| File 1 KiB / 100 | 1952.759 ms | 74.005 ms |
| File 65,500 B / 3 | 3711.910 ms | 135.523 ms |
| Sequential async / 100 | 64.205 ms | 10.885 ms |
| Sequential async / 1,000 | 3283.641 ms | 125.811 ms |

The original 216 final observations cover normal/O2 JIT and independent normal/O2
AOT. Compilation/emission/linking is separate from execution. GC budget is 256,
with actual soft/hard 1 MiB stacks and no concurrent build or validation.
Complete-byte/count checks occur outside the original workload's timing.

The independent async comparison covers 100/250/500/1,000 operations for direct
sync/async I/O and sync/async logging, console/file, timestamps off/on, plus empty
activations. It contains 816 final observations including independent AOT timing
and instrumentation controls. Scheduler/alias scans now track live tasks or actual
buffers; constant-time Handle removal eliminated the remaining repeated vector
shifts. Completed results retain their original lifetime. GC still scans growing
retained results under unchanged budget 256; its measured cost explains remaining
curvature. No asymptotic linearity claim is made for arbitrarily long async roots.

Synchronous framing now performs a single budgeted escape traversal and uses
existing bulk memory operations, immutable-string reuse and a final join. Ordinary
reads use one checked scalar-array read instead of selected-write machinery.
Large-record work is approximately linear in input/encoded bytes. However, small
file logging is still **5.86× direct I/O**, above the single-digit/2–3× screen.
Checked byte traversal, scalar schema-key materialization/validation, source
admission/guard/receipt work and per-escape stream construction remain measured
costs. Heavy escaping remains expensive. Those costs are not claimed unavoidable
or accepted simply because the baseline was much slower.

[PERFORMANCE_REPAIR.md](PERFORMANCE_REPAIR.md) explains each mechanism and safety
argument. [BENCHMARKS.md](BENCHMARKS.md) retains all historical tables and adds
final before/after results, ranges, CPU/resource counts, per-call costs and timer
overhead. Independent review must decide whether remaining costs are acceptable
or require further work; the engineering result does not close that gate.

## Fresh correctness acceptance

| Profile | Selected CTests | Runtime |
|---|---:|---:|
| Release | 88/88 | 711.41 s |
| Debug | 88/88 | 2063.22 s |
| Strict | 87/87 | 2024.84 s |

The [fresh acceptance index](evidence/performance-repair/acceptance-index.json)
contains all 263 outcomes and executable/library hashes. Full logs and exact
source freezes are in [Release](evidence/performance-repair/acceptance-release/LastTest.log),
[Debug](evidence/performance-repair/acceptance-debug/LastTest.log) and
[strict](evidence/performance-repair/acceptance-strict/LastTest.log).
All 299 source files match across the three profiles and the current tree; the
measured source matched the Release freeze before and after isolated measurements.
The raw Debug/strict manifests additionally include 16/32 generated text-encoding
cache entries. They remain intact and separately recorded in the
[source-freeze comparison](evidence/performance-repair/source-freeze-comparison.json);
only those generated cache entries are excluded from cross-profile source equality.

Per profile, the logging native host performs 1,686 checks across JIT modes;
each independently emitted/linked AOT probe performs 826. Coverage includes exact
UTF-8/escaping, 30 threshold cases, eight budgets, nine invalid configurations,
eight fixed UTC instants, cap-sized records, 244 filesystem observations, ownership,
rotation, partial writes, cache/reload, installed execution, 38 LSP checks, forced
GC and allocation failures, native cancellation/abandonment and provider cleanup.

The expansion adds buffer alias/collection/lease tests, scalar-array read effects,
fixed/growable/aggregate arrays, Runtime persistent-root removal, text/encoding,
async syntax/lifecycle/parallel contracts, 34 async AOT cases and long-root
retention tests. Retention tests verify finished frames retire while results remain
usable, and root exit followed by collection returns objects/handles to baseline.
Release and Debug each ran 88 CTests; strict ran 87. Existing CMake configuration
omits the duplicate `text_encoding_stack` registration when sanitizers are enabled.
The strict `text_encoding_text` execution and all other selected tests still
inherited the same actual hard 1 MiB stack from the validation driver. No coverage
was removed for this repair; nested executions are not counted as extra CTests.

Strict settings: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`,
`LSAN_OPTIONS=exitcode=87`. Sanitizers cover compiler, Runtime and C++ hosts;
generated VOID instructions do not independently receive Clang sanitizer passes.

## Implementation to inspect

| Area | Files and change |
|---|---|
| Logging source | `stdlib/log.void`: approved API, guarded state/ownership, bounded single-pass escaping, immutable reuse and final framing. Existing `io/memory` supplies bulk writes. |
| Source I/O support | `stdlib/io.void`: previously implemented internal prepare/complete async-open helpers preserve acquisition ownership across cancellation. |
| Frontend | `src/frontend/compiler.cpp`: default enum/callable field recognition from the original logging implementation; scalar growable-array value reads snapshot the owner before index effects and avoid selected-write machinery. Selected writes retain their checks. |
| Checked array ABI | `src/backend/llvm/backend.cpp`, `src/runtime/list.cpp`, `src/runtime/fixed_abi.cpp`, `include/void/runtime.hpp`, `include/void/runtime_abi.hpp`: fused signed-index normalization/bounds/type/root validation and read; old ABI retained. |
| Byte conversion | `src/runtime/array.cpp`: use existing bounded, private-rooted dense-u8 bulk construction. |
| Scheduler | `src/runtime/async.cpp`: address/live/child scan indices, owner/group counters, rollback, allocation-free cleanup reactivation, plus the original cancellation-readiness rescan repair. Ready/worker queues and result lifetimes remain. |
| Alias validation | `include/void/heap.hpp`, `src/runtime/heap.cpp`: scan existing buffer metadata only; preserve exact overlap/lease checks and private-buffer inclusion. |
| Root cleanup | `include/void/runtime.hpp`, `src/runtime/coordination.cpp`: indexed persistent-Handle removal under the existing mutex, preserving every surviving root. |
| Native test seams | `src/runtime/filesystem.cpp`, `tests/io/native_jobs.cpp`, `tests/io/runner.cpp`: existing boundary hooks, deterministic acquisition-abandonment regression and logging inventory. |
| Regressions and measurement | `tests/log/`, `tests/growable_arrays/tests.cpp`, `tests/runtime/buffers.cpp`, `tests/runtime/coordination.cpp`, `CMakeLists.txt`: behavioral tests, isolated probes, original workload oracles and expanded profile orchestration. |
| Documentation/examples | `docs/log/`, `examples/log/`, `docs/README.md`, `stdlib/README.md`; all changes are included in the inventory. |

There is no new logging worker queue, scheduler, native logging formatter or public
logging API expansion. New Runtime read support is general checked array support.
The complete path-by-path inventory, including pre-existing changes without an
authorship claim, is in [CHANGED_FILES.md](evidence/performance-repair/review/CHANGED_FILES.md)
and [source-inventory.json](evidence/performance-repair/review/source-inventory.json).

## Review package and preserved history

[Review package guide](evidence/performance-repair/review/README.md),
[all changed source/tests/documents](evidence/performance-repair/review/source.tar.gz),
[evidence archive](evidence/performance-repair/review/evidence.tar.gz), and
[archive manifest](evidence/performance-repair/review/manifest.json).
Compiled binaries/objects are not duplicated in the archive; exact identities,
generated sources and commands are included, and frozen binaries remain in the
workspace baseline. The source archive includes every modified source/test file,
new files, the tracked patch and SHA-256 inventory.

The [baseline freeze](evidence/performance-repair/baseline/freeze.json) retains
original sources, exact executables and evidence before any repair. The historical
93/93 acceptance is preserved in the baseline archive and [original acceptance
record](ACCEPTANCE.md); it is not presented as validation of these changes.
Every original failure remains under the shared `tests/text_encoding/crash.py`
workflow, with preserved sources, status, stderr and executable identity. See
[incident index](evidence/performance-repair/incident-index.json),
[diagnoses](INCIDENTS.md), [process receipt index](evidence/performance-repair/receipt-index.json)
and [engineering journal](IMPLEMENTATION.md). A passing retry never replaces a
failed original.

Native Windows/macOS execution remains pending, including platform file-sharing,
path/case and close-before-rename behavior. The approved exclusive-writer/alias
warranty remains; no hostile-namespace, power-loss or cross-process exactly-once
guarantee is added. Engineering stops for independent Golden Wrench review with
the release hold and performance gate OPEN.
