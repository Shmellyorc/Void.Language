# Bounded logging framing candidate — 4 November 2026

**LOG-PERF remains OPEN / release HOLD. Golden Wrench approved this candidate for commit and push.**

The authorized scalar-substitution implementation passes the selected correctness
checks and the heavy framing and console performance screens on this Linux host.
Small-file logging still fails its absolute screen. This working-tree candidate
is based on d784c0490d13b8abdca754dfecf99578ab498733. File hashes and the
two candidate measurements are retained with the receipts. The original
ffd8920b baseline is preserved byte-for-byte. [Baseline report](PERFORMANCE_BASELINE_LLVM23.md) · [Results](evidence/llvm23-framing-20261104/results.json)

## Historical stop condition

The LLVM 23 migration already passed two retained-IR O2 replays and a
source-associated logging check. That source-associated execution covered the
short ordinary quote path. One additional bounded Release/LLVM 23.1.4 O2 process
used the original flat-builder source and stdlib, shapes 0/4/9 and stages
6/7/11/12, with hard/soft 1 MiB stacks. All 48 observations, including warmups,
passed the independent escaped/framed lengths and complete-output/call-count
oracles, exit 0 and empty stderr.

The host was the retained current LLVM 23 production benchmark host built at
ffd8920b, with GC budget 256. It differs from the original LLVM 19 executable
and schema-key diagnostic overlays. All 181 original incident files, 61 preserved
source copies and 20 original flat-builder input paths were hash-verified.
The later historical capture identified LLVM optimization as the faulting component.
Its original cause remains unresolved. The current evidence was sufficient to
proceed under Golden Wrench's current-toolchain disposition. [Current check and hashes](evidence/llvm23-framing-review-20261104.json) · [Preserved classification](CRASH_CLASSIFICATION.md) · [LLVM 23 prior checks](../llvm23/evidence/metrics/dse-results.json)

## Implementation and safety

- The public text.translate_scalars operation retains the approved signature and
  typed errors. The canonical text wrapper maps private integer statuses to
  text.error_kind. All rules are validated for empty input as well. Unicode
  scalars are replaced once, including deletion, and unmapped UTF-8 bytes retain
  their original representation. [Wrapper](../../stdlib/text.void#L568)
- The frontend binds a canonical-only private operation with typed parameters and
  preserves written argument evaluation order. Independent IR validation checks
  provenance, operands, result shape and coordination. LLVM emits registered
  input/output slots and binds the same ABI for JIT and linked AOT. [Frontend](../../src/frontend/compiler.cpp#L3158) · [IR validation](../../src/ir/module.cpp#L1153) · [Lowering](../../src/backend/llvm/backend.cpp#L1388)
- The Runtime checks root authority, schemas, extents and immutable string
  references. Keys and replacements use ordered, rooted backing leases. String
  snapshots reacquire borrows between 16 KiB work chunks, with additional polling
  every 256 rules. Native-owned rules use balanced lookup. Counting and bulk span
  assembly run NativeSafe. Checked subtraction enforces the exact caller budget.
  Allocation/root/invariant failures use the existing Runtime failure channel.
  RAII releases leases, roots and native storage on every exit. Rule disposal
  enters NativeSafe, including on failure. [Runtime operation](../../src/runtime/text_case.cpp#L144)
- Golden Wrench explicitly selected the existing snapshot and publication
  guarantees. Alias growth may install a new live-list backing while the selected
  backing remains frozen. Publication uses Runtime::string_into, retaining its
  whole-string validation, allocation and final atomic copy. The full-input snapshot reserve and unpolled final publication remain pause-time limitations. Tail-latency acceptance remains UNKNOWN.
- Logger integration changes _quote and its private escape policy tables.
  The tables are ordinary per-application rooted globals. The original early
  budget and empty-string paths remain. _validate, _escaped_size, _frame,
  _prepare and all subsequent sink/lifecycle code were compared byte-for-byte.
  Name-first budgets, formatting, errors and sink ordering are preserved.
  [Quote integration](../../stdlib/log.void#L348) · [Existing checked publication](../../src/runtime/coordination.cpp#L211)

## Correctness evidence

All final checks exited 0 with actual 1 MiB execution stacks where applicable.

| Check | Result |
| --- | --- |
| Text source check, normal/O2 JIT and independently linked normal/O2 AOT | PASS, seven check/build/execute commands |
| Independent private text IR validation | PASS, 16 rejections and user-source authorization rejection |
| Text ownership and failure harness | PASS, 31,224 checks |
| New translation source failure sweep | PASS, 49 injected budgets per normal/O2 mode |
| New direct ABI failure sweep | PASS, 11 injected budgets, aliased output preserved |
| Pending concurrent collector, aliased input/replacement/output, malformed root/schema | PASS |
| Logging byte/UTC oracles | PASS, 15 execution/emit/link checks |
| Logging JIT native host | PASS, 1,688 checks across normal/O2 |
| Logging AOT native hosts | PASS, 827 checks each in normal and O2 |

The text cases cover nonrecursive substitution, Unicode and NUL, deletion,
empty input, duplicate and invalid scalar rules, unequal rule counts, exact-fit
and one-byte-over limits, negative limits and aliases. Logging checks cover all
specified escapes, 30 filtering combinations, eight budgets, nine invalid
configurations, eight fixed instants, lazy evaluation, partial writes, rotation,
allocation failures, cancellation, startup cleanup and reload.
[Source cases](../../tests/text_encoding/text.void) · [Ownership tests](../../tests/text_encoding/ownership.cpp) · [IR tests](../../tests/text_encoding/audit.cpp) · [Logging oracles](../../tests/log/oracles.py) · [Native host](../../tests/log/host.cpp)

## Performance comparison

Same AMD Ryzen 7 1700X desktop, 16 logical CPUs, Linux 7.2.11 PikaOS and glibc 2.44.
Release C++ flags, pinned LLVM/Clang/LLD 23.1.4, GC budget 256 and hard/soft 1 MiB
stacks match the retained baseline. Each cell has one warmup and three measured
samples. Compilation and initial setup precede steady-state timing. Builds and
correctness tests completed before measurements. Allocation counting uses a
separate executable and its timings are excluded.

Each candidate supplies 216 original-workload observations across normal/O2
JIT and AOT, and 192 selected component observations including warmups and the
separate counting runs. Medians below are JIT O2 whole-workload milliseconds.

| Criterion | Retained baseline | Final candidate [range] | Status |
| --- | ---: | ---: | --- |
| File 64 B ×200, <10 ms | 24.042 | 21.390 [20.641, 21.415] | FAIL |
| Heavy complete framing, ≤10 ms | 910.628 | 8.603 [8.603, 8.921] | PASS |
| Heavy full console, ≤20 ms | 947.525 | 18.501 [18.450, 19.030] | PASS |
| Prepared direct file ×200 | 11.662 | 12.195 [9.688, 12.734] | Control |
| Heavy prepared console | 7.946 | 9.133 [9.081, 9.610] | Control |

The file/direct ratio is 1.75×. File200 also exceeds 10 ms in normal JIT
(20.375 ms), normal AOT (18.799 ms) and O2 AOT (21.717 ms). The current direct-I/O
median itself exceeds 10 ms in three modes. This limits attribution of the
absolute miss to logger preparation alone.

Heavy framing processes 102,400 input bytes and 218,700 complete output bytes.
Its median CPU time is 8.552 ms, throughput about 11,623 records/s and amortized
latency 86.03 microseconds/record. Full heavy console is about 5,405 records/s
and 185.01 microseconds/record. File200 is about 9,350 records/s and
106.95 microseconds/record, including acquisition/close. These are workload
averages. Tail latency remains UNKNOWN.

Heavy frame ordinary C++ allocations fell from 2,509,955 to 13,415. Cumulative
requested native allocation bytes fell from 89,737,292 to 3,689,216. Source
managed constructions fell from 77,100 to zero, while three collections still
occurred. Native string construction remains managed allocation. Ordinary
64-byte frame allocations increased from 24,523 to 26,121, while its median time
fell from 11.133 to 8.313 ms. This is a remaining cost worth profiling.

All 408 final observations return managed payload and persistent handles to
their own pre-sample baselines after GC. Whole-workload retained state grows by
2,166 bytes and two handles, consistent with the two per-application rule tables.
JIT whole-process peak RSS was 171,508–187,484 KiB and includes compilation and
earlier workloads. Native-live peaks remain UNKNOWN.

A separate JIT O2 check used three fresh applications per version with an otherwise
empty entry importing log. First-invocation medians increased from **0.081 ms**
(range 0.076–0.085) to **1.130 ms** (1.101–1.203). Second-invocation medians were
0.015 ms and 0.028 ms. These observations include imported global initialization
and Runtime entry work. Compilation precedes each timed invocation. The extra
initialization is excluded from the steady-state table above and retained
separately in the results. Per-phase native profiling remains UNKNOWN.

The first candidate is retained as well. It measured 26.767 ms file200,
10.972 ms framing and 20.694 ms full console. Review identified that it had
dropped _quote's original empty-input and early-budget paths. Restoring those
paths preserves the original behavior and avoids validating the rule table for
empty logger names. Native-safe rule disposal was included in the final revision.
The two revisions therefore change two factors. Their difference alone cannot
isolate each factor's timing contribution.

## Reproduction, failures and remaining review

Private evidence is retained at
/home/shmellyorc/.local/state/void-log-performance/framing-candidate-hAebdlqN/.
The revision2 directory holds candidate.patch, candidate-identity.json,
host-hashes.json, receipts, exact driver copies and captured output identifying
the final inputs and binaries. The initial candidate and original baseline
remain separate. The receipt index and per-cell wall/CPU samples, allocations
and resource observations are available in the machine-readable report. [Results](evidence/llvm23-framing-20261104/results.json)

The results file records each exact Python invocation, including startup-driver.py. Verification uses
revision2/verify.py with text, oracles, void_text_audit, void_text_ownership,
void_log_host, void_log_probe_normal and void_log_probe_optimized.
Measurements use revision2/measure-driver.py with prepare, original and components.
The measurement driver deliberately rejects overwriting existing results.
Replay requires a fresh private output directory and the recorded source patch
on the recorded base commit.

The initial build returned 2 after AOT emission returned 1. Initial source checks
also returned 1. The failures were the new multiline-array syntax, a dynamic
enum-equality expression in a test and a stale frontend object from overlapping
private build preparation. They were corrected and all affected targets rebuilt.
The shared recorder's incident-mzfi2__k is a retained nonzero compiler diagnostic,
with no signal. Its original receipt paths and hashes are retained in
incident-relocation.json. A later private startup diagnostic initially failed compilation with exit 1
because its C++ driver omitted runtime.hpp. The initial source, driver and
compiler output are retained as startup-initial files. Adding the include
completed that diagnostic. Every final correctness and benchmark process returned 0.

The smallest next action is Golden Wrench review of this candidate, followed by
a bounded small-file/direct-I/O comparison that separates framing, writes and
flush/close cost. Current measurements demonstrate the heavy framing repair
under these conditions. They leave the precise remaining split among validation,
rule snapshots, framing and sink work unresolved. Per-phase profiling, broader
shape scaling, heavy AOT component timing, native Windows/macOS performance,
peak-live memory and tail latency remain UNKNOWN.

Installation stays accepted at d784c049. No installation, HTTP/HTTPS or VM
qualification was reopened. Golden Wrench authorized committing and pushing the reviewed implementation,
tests and logging evidence/docs with [skip ci]. The small-file investigation
continues separately with a review stop before further production changes.


## Changed implementation and test files

- include/void/ir.hpp, include/void/runtime.hpp, include/void/runtime_abi.hpp
- src/frontend/compiler.cpp, src/ir/module.cpp, src/backend/llvm/backend.cpp
- src/runtime/text_case.cpp
- stdlib/text.void, stdlib/log.void
- tests/text_encoding/text.void, tests/text_encoding/ownership.void
- tests/text_encoding/ownership.cpp, tests/text_encoding/audit.cpp

Documentation updates are this report, BOUNDED_FRAMING_REVIEW.md, HANDOFF.md,
the release handoff and blocker register, the current-path check receipt and the
candidate results JSON. The original LLVM 23 baseline report/data and historical
crash reports/data retain their recorded hashes. The protected README and CMake,
HTTP and installation workflow/recipe files have no diff.
