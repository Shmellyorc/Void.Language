# Logging engineering record

Boss's attached 28 October 2026 mission supersedes the research-only authorization
status in the preserved RESEARCH, DESIGN and ROADMAP documents. The approved
contract is DESIGN.md, with trusted exclusive-writer namespaces, explicit adoption,
checked results, conservative faults and the three additional native safeguards.
Baseline: f9ac49f1558f326129948bc06c829263448b7714. No Git mutations or hosted CI.

Implementation and focused local acceptance are complete: **93/93** selected
Debug/Release/strict CTests passed. [Acceptance](ACCEPTANCE.md),
[measurements](BENCHMARKS.md) and [handoff](HANDOFF.md) record exact scope and
limits. Engineering stops for independent Golden Wrench review; that review and
native Windows/macOS acceptance are not claimed.

The source module uses ordinary Runtime globals, managed guards, byte arrays and
existing console/io/time/path/convert APIs. Concrete synchronous and asynchronous
I/O methods share admission, framing, receipts and state policies. The development
expansion script in tests/log/expand_io.py retains generation provenance; installed
applications execute only stdlib/log.void.

A source-level check exposed an existing compiler omission: explicit enum/callable
field defaults were treated as missing required initialization in no-init
construction. The narrow frontend correction recognizes the same complete defaults
already recognized by value initializer validation. Missing defaults remain required.
No logging intrinsic, new provider, scheduler or permission bypass was added.

Original unsuccessful engineering checks remain under evidence/crashes with exact
launch-time source snapshots and generator provenance. Initial source rejections
were ordinary diagnostics, not native crashes. They included unsupported enum ==,
local/import naming collisions, private field access outside its owning receiver,
standalone propagation expressions, and readonly diagnostic-cause forwarding.
Each was corrected using existing source contracts; no compiler change was made
for those expected restrictions. H1–H7 remain distinct historical unresolved incidents.

## Cancelled acquisition ownership repair

The preserved native AOT observation in
`evidence/crashes/incident-7rh_igd0/` measured an entered opening followed by
cancellation: descriptor count **4 -> 5**, an empty newly created active file,
and zero explicit native closes before Runtime teardown. The existing io child
task retained its published acquisition, but the logger had not yet observed its
return value. This was a real gap in the logger's deterministic acquisition cleanup.
It was not an unexplained compiler crash or loss of the Runtime provider ledger.

Two internal ordinary-source io helpers now prepare a fresh unopened file identity
and complete its existing async acquisition. Logger and temporary-tail guards retain
that identity before suspension. Native operations, cancellation/drain, publication,
readonly checks and provider ownership remain in the established io subsystem.
No new public logging signature, native provider or Runtime protocol was introduced.
The normal native AOT cancellation rerun measured exactly **one close** for each
entered open, adoption read, write, move and close boundary. Final profile receipts
confirm both JIT modes and both independently linked AOT modes, including forced
GC, completion races and unpublished default replacement.

The post-transfer fault test originally assumed another managed object construction
would occur after a successful native write. Its preserved observation was
`failed=0 state=7 bytes=32`: both complete records succeeded, so no Runtime failure
had actually been injected. It now forces a real bad_alloc at the second native
one-byte chunk, after the first byte reaches the file. Both JIT modes retain that
one byte, fault the logger and reject automatic replay. This correction strengthens
the actual failure boundary; it does not establish a production defect in the
successful original transport.

Emergency close now reserves its final-release fallback before receipt allocation.
It marks the instance closed when the native handle retires, including cancellation,
and preserves ownership when native close is busy. The initial consolidated native
run retained all earlier passes but its old cancellation assertion expected faulted
after a retired close. The source now asserts **closed** specifically for retired
close and **faulted** specifically for uncertain write/rotation. The preserved
failure is `evidence/crashes/incident-piwcm854/`; it showed zero subsequent output,
not an unsafe successful retry.

Reconfiguration also keeps the successfully configured absolute path snapshot
separate from the namespace whose acquisition was most recently attempted.
A failed path replacement cannot redirect recovery of the prior configuration to
the failed new destination. Native tests observe both file identities and sentinel
contents before/after explicit adoption and recovery.

Default replacement now reserves the old default before its first await. It also
retains the unpublished replacement identity in the caller's reached acquisition
guard before awaiting activation. This prevents concurrent reconfiguration from
using a stale old-default reference and closes a cancelled candidate even when an
internal activation task has already completed. Public close and default replacement
share the same reserved-close helper. This uses ordinary source guards and existing
io operations, without another global lock or Runtime service.

Failed acquisition cleanup closes without introducing a flush of unchanged data.
The primary acquisition error and its checked close error retain their separate
typed causes. Normal rotation/lifecycle still flush before close; simultaneous
flush/close failures are checked independently by native source assertions.

## Shared async readiness repair

The final forced-GC cancellation matrix exposed `Async await dependency cycle`
after a native acquisition had entered. The diagnostic rerun is preserved in
`evidence/crashes/incident-rs9b9ass/`. A seventh scenario in the existing
`tests/io/native_jobs.cpp` reproduces the failure without importing logging:
an enclosing activation awaits an opening activation that cancels itself after
native publication, requiring asynchronous acquisition abandonment. Its original
failure is `evidence/crashes/incident-ej4p6can/`.

The scheduler scans parents before later-created children. When a child in
`finishing` becomes `finished` during that scan, an earlier waiting/joining parent
can become ready after its readiness check. The old empty-ready-queue path could
diagnose a cycle before rescanning. `src/runtime/async.cpp` now records this actual retirement progress and
rescans before cycle diagnosis. It adds no task state, queue, lock or privilege.
The isolated seven-scenario regression and both JIT cancellation modes pass after
this repair; the final matrix also covers existing async source, IR, parallel,
lifecycle and I/O acquisition checks.

The superseded profile runs are retained. Release reported the cancellation
assertion; Debug/strict selection orchestration was deliberately stopped before
rebuilding. The still-running strict native host was terminated with its original
files retained under `evidence/stopped-native-1466352/`. The action and exact
process identities are recorded in `evidence/superseded-profile-stop.json`.
No spontaneous crash or hang is inferred from this operator stop; its snapshot
shows work had progressed into the second optimization mode. The deterministic
standalone regression establishes the shared readiness defect independently.

## Mandatory performance repair — baseline checkpoint

Boss authorized both investigations on 2026-10-28. Both performance gates remain
OPEN until the measured bottlenecks are repaired and independently reviewed.
The prior correctness acceptance does not constitute performance acceptance.

Before any production edit, `tests/log/performance_baseline.py` froze production
sources, original benchmark sources, the original evidence directory and all three
benchmark executables under `evidence/performance-repair/baseline/`. Its manifest
records SHA-256 identities and checks them again after measurement. No builds or
validation suites ran concurrently with the repeated original workloads. The
desktop remained active; this is recorded, not claimed to be a dedicated machine.

All 216 original-workload samples completed. JIT O2 medians reproduced both
concerns: 200 file records 267.697 ms versus direct I/O 2.922 ms; 200 console
records 257.119 ms versus direct console 1.410 ms; 100 1-KiB records 1952.759 ms;
three near-limit records 3711.910 ms. Async 100/1000 calls measured
64.205/3283.641 ms. Compilation remains outside execution timing. GC allocation
budget remains 256, matching production. Original measurements remain untouched.

The next sequential step builds a separate component harness and test-only native
instrumentation overlays. Production code is unchanged at this checkpoint. The
first overlay source check rejected a misplaced import and a local name shadowing
an imported type; its exact original diagnostics and sources are preserved in
`evidence/crashes/incident-gfpyif5z/`. This was a test-source diagnostic, not a
native crash. The corrected test overlay moves imports to the top and uses a
distinct local name.

## Synchronous repair — measured first correction

The 492 uninstrumented JIT O2 component observations and 492 independently linked
AOT O2 instrumented observations are preserved in
`evidence/performance-repair/framing-before/`. For the 200-record ASCII input,
size scanning alone took 76.764 ms, escaping 153.793 ms and complete framing
240.329 ms. Isolated array traversal took 26.746 ms, byte-copy/push traversal
40.162 ms and existing memory-stream bulk copy 3.684 ms. Admission and individual
clock stages were each below 1 ms. Instrumented framing counted 380,400 list-header
validations, 380,200 root-word lookups, 1,307,217 polls and 410,644 ordinary native
allocations. Collection took 0.88 ms in that instrumented sample; growth-copy time
was 0.31 ms. Thus byte pushes alone and GC are not the established main causes.

The first correction changes ordinary `stdlib/log.void` framing only: use the
existing `io/memory` transactional bulk writer, scan each input once, decode only
possible escaped non-ASCII values, and copy unmodified UTF-8 spans in bulk. The
record remains private until both quoting operations and final byte snapshot
complete. Every escape expansion is checked against the remaining byte budget;
the name reserves separator/end bytes and the message reserves its closing quote
and LF. No transport, scheduler, provider, public API or permission changed.
The prior size helper remains for configuration validation. This is a measured
candidate, not yet performance acceptance.

The first source check rejected a multiline array literal (unsupported syntax),
preserved in `evidence/crashes/incident-3udty23f/`. Keeping the literal on one line
passes source checking. Exact byte/budget oracles and repeated timing are next.

The first correction passed all 15 independent byte/UTC oracle executions and
emission/link checks. Its unchanged original workload measured 57.041 ms for 200
small file records and 555.241 ms for three near-limit records. The gate remains
OPEN: this is still far from the screening target. Those 108 JIT observations
are in `evidence/performance-repair/bulk-original-jit.json`; another 984 component
observations are preserved in `framing-bulk/`.

Those observations narrowed the remaining cost to checked scalar access. A plain
`source_bytes[at]` was lowered through selected-write capture: index normalization,
version capture, version recheck, another normalization, and finally the read.
No source work occurs between the first normalization and the value read.
`src/frontend/compiler.cpp` now lowers **plain scalar growable-array value reads**
directly through checked `array_get`, retaining the owner before evaluating the
index. Selected writes/receivers, fixed values and composite elements retain
their existing selection paths. The focused regression in
`tests/growable_arrays/tests.cpp` covers an index that grows/rebinds its owner,
negative indexing, readonly reads, empty-after-index failure, selected-write RHS
invalidation, recovery and forced-GC rooting. Existing extreme-index, numeric,
alias, transaction and allocation-failure cases also pass both JIT modes.
The corresponding original-workload checkpoint is `reads-original-jit.json`:
36.621 ms for 200 file records, still not acceptance.

Two further measured costs are addressed without changing source functionality:

- `array_get` scalar-list lowering now delegates signed-index normalization and
  bounds checking to `vd_list_read_index`, which performs them against the same
  validated list header used for the read. All root, catalog, storage, type and
  bounds validation remains; the redundant preliminary count lookup is eliminated.
  Existing `vd_list_read` remains available. Signed minimum and unsigned maximum
  cannot overflow normalization. Focused native checks cover valid/invalid signed
  and unsigned indices, invalid signedness and an unregistered source root.
- `Runtime::bytes_into` uses the existing dense-u8 `BufferBuilder::write_bytes`
  path instead of invoking `write_scalar` for every byte. Its owned native input,
  private precise root, bounded work quota, failure publication and final list
  identity are unchanged. This targets the measured UTF-8 encoding cost (2.3 ms
  for small inputs and 18.6 ms for the near-limit batch). Existing native private
  buffer, publication, alias and post-poll wrong-type checks pass.

No root cache, new scheduler, logging native intrinsic or raw pointer source API
has been introduced. The latest original-workload timing is still pending at
this checkpoint; final profile validation remains required.

The consolidated checked read plus bulk UTF-8 conversion passed the existing
growable-array source/transaction suite in both JIT modes (including 34 reached
failing allocation budgets per mode) and native GC buffer tests. The unchanged
original workload then measured 30.798 ms for 200 small file records and
155.109 ms for three near-limit records (`fused-original-jit.json`). A 192-row
component subset in `framing-fused/` isolates the remaining work: quoting small
inputs 12.558 ms, full framing 21.383 ms, direct unescaped interpolation 1.557 ms.

Framing now preserves immutable input text when no escape is needed, separates
the scan from construction to keep its precise-root frame small, and uses one
final interpolation/UTF-8 conversion for the record. Escaped spans still use the
existing bounded bulk writer. Exact byte/budget/UTC oracles pass again. The
latest unchanged-workload checkpoint (`immutable-original-jit.json`) measures
19.304 ms for 200 file records versus 3.079 ms direct I/O, and 135.762 ms for three
near-limit records. This is substantial progress but **does not close the
synchronous gate**. No root cache or unchecked byte access was introduced.

## Independent async investigation — measurement checkpoint

`performance_async.void` compares direct synchronous/asynchronous file I/O,
synchronous/asynchronous logging, console/file sinks and timestamps on/off at
100, 250, 500 and 1,000 sequential operations. It also includes empty async
activations to isolate scheduling. Every invocation retains its natural single
async root through all calls and cleanup. There is no shortened root or omitted
await. Console has a synchronous transfer API; its direct async comparator is
explicitly one ordinary activation around that transfer, not a native async
console claim.

The separate host checks **every output byte**, write count and equivalent flush
policy. For exact timestamp-byte equivalence, a test-only native clock overlay
still performs the real UTC sample (including an observable volatile read), then
pins the returned instant to 2021-01-01T00:00:00.123Z in both paths. Formatting
and all per-record calls remain. The unchanged original benchmarks keep the live
production clock. The fixture-clock matrix is not substituted for those timings.

The generated scheduler overlay counts actual task visits independently for
lookup, worker reception, pending-child checks, failure checks, child cleanup and
ready scanning. It times activation, resume, native execution, waiting, root
cleanup and persistent-handle destruction, and observes GC root scans and
retention. Timing-only binaries use the production scheduler. All overlays and
hashes remain in `async-before/`; production scheduler code is unchanged at this
checkpoint. A source naming collision with the imported `file` type was rejected
and corrected to `file_sink`; the original is preserved as `incident-d4b4kyqi`.

The independent matrix completed 272 JIT O2 timing observations and 272 AOT O2
instrumented observations. Direct sync file I/O at 100/250/500/1,000 calls took
1.266/3.237/6.167/12.139 ms. Direct async file I/O took
5.411/13.954/34.619/118.630 ms; async file logging took
22.915/151.645/608.240/3059.067 ms. Empty async activations, with no I/O or log
formatting, took 0.364/1.425/5.161/20.046 ms. This establishes a shared scheduling
problem independently of logging and transport.

For 1,000 async file-log operations, actual scheduler counters recorded 4,029
retained tasks, 30,290,401 linear lookup visits, 101,349,082 worker-reception
visits, 136,136,209 failure-check visits, 10,163,574 pending-child visits and
28,616,315 additional task scans. At 100 calls the corresponding counts are
orders of magnitude smaller. Native execution itself took 7.53 ms and GC
collection 27.18 ms in that instrumented 3,799 ms sample. The counters, not elapsed
time alone, prove repeated scans of completed task history.

Task resume still had substantial exclusive time after accounting for those
scans. Before changing the scheduler, a second test-only overlay instruments
`Heap::validate_output_address` and its actual object visits. This investigates
ordinary async-frame output locations falling within conservative buffer address
bounds. No production alias checks or scheduler behavior have changed at this
checkpoint.

The heap overlay confirmed the second independent async cost: at 1,000 file-log
calls it visited 131,993,278 heap objects, spending 1,509.68 ms in output-alias
validation in the instrumented sample. Most visited objects were unrelated
retained tasks. The existing conservative buffer-address bounds admitted ordinary
async-frame root locations to a slow path that scanned the entire object table.

The scheduler correction retains all task/result handles and creation-order
failure selection, but indexes address lookup, live task scans and owned children.
Group/owner pending and unobserved-failure counters avoid repeated history scans.
The original ready and worker queues remain. Native publication rollback removes
all new indices before destruction; cleanup reactivation uses allocation-free
intrusive links. Five focused Release async tests passed at the initial checkpoint.
The original workload checkpoint (`scheduler-original-jit.json`, 108 observations)
still took 2,380.277 ms for 1,000 async log calls, confirming that scheduler repair
alone is insufficient. Original small sync file records took 18.163 ms in that run.

The next correction links the existing Heap-owned buffer metadata into a scan
list, including private buffers. Adoption links only after successful insertion;
collection/destruction unlinks before releasing storage. Exact alias overlap and
lease checks are unchanged. A focused native regression forces object-table
rehash, abandons a private buffer and removes middle/head/tail/final live buffers,
checking both leased and unleased alias rejection after each collection. Final
validation and performance acceptance remain pending.

Focused post-alias tests passed buffer, growable-array, native-job and four async
suites. The logging host stopped at its injected write-allocation sweep because
its expected diagnostic list omitted the existing interpolation backend's
`Display operation failed` wrapper (`vd_scalar_display` reports the underlying
allocation failure). Added that known checked diagnostic; ownership, fault-state,
partial-effect, rooting and recovery assertions remain intact. Original host and
outer CTest failures are preserved in `incident-v917yx1_` and `incident-du4yxvhw`.
An earlier build command misspelled `void_growable_arrays_tests`; its status-2
build-target failure is preserved in `incident-2psi0ciz`. Corrected target built.

After scheduler and buffer-only alias scans, the independent matrix passed all
272 timing and 272 instrumented observations with exact bytes and transfer/flush
counts. JIT O2 async file logging at 100/250/500/1,000 calls now takes
11.392/27.031/58.861/147.339 ms (before: 22.915/151.645/608.240/3059.067).
Empty activations take 0.201/0.522/0.986/1.987 ms. At 1,000 file-log calls, worker
reception visits fell from 101,349,082 to 192,225; alias visits from 131,993,278 to
590,738. Hash lookup replaced 30,290,401 task-history visits with 15,098 lookups;
successful pending/failure checks no longer walk history. Result lifetimes and
4,029 retained task records are unchanged. Post-root collected payload is 120
bytes at every size, equal to startup, and persistent handles return to baseline.

Remaining curvature is measured: GC scans 398,988 persistent handles (24.05 ms)
at 1,000 operations because the allocation budget remains 256 and completed task
results remain rooted until root exit. Handle destruction adds 7.06 ms versus
0.09 ms at 100 operations. `Runtime::Handle::~Handle` finds and erases each entry
from a vector, repeatedly shifting the remaining entries during bulk async-root
teardown. A constant-time index plus swap-with-last removal is being evaluated
as the final narrow cleanup repair. This does not remove roots earlier or alter
GC policy. The preceding measurements remain immutable in `async-after/`.

The final cleanup correction stores each persistent Handle's current vector index.
Removal swaps the last handle into that slot, updates its index and pops the tail,
under the existing mutex. Registration still allocates before success; destruction
still allocates nothing. All live roots remain enumerated and no root lifetime is
shortened. Enumeration order has no semantic contract. The focused coordinator
regression covers 387 out-of-order removals, collection after each removal,
surviving-root checks, reuse after an empty registry and NativeSafe host-thread
destruction. Final async measurements will separate this repair from the preceding
`async-after/` checkpoint. No GC policy or completed-result retention change.

`framing-after/` completed 544 timing and 544 instrumented observations, including
independent complete-record byte checks. Its added shape 9 has 30,816 escape-heavy
input bytes and 65,484 escaped bytes (65,495 with framing); the original nine
shapes are unchanged. Heavily escaped construction remains expensive despite
linear byte work. This is a limitation to report, not an acceptance pass.

Final-source Release acceptance passed **88/88** selected CTests in 711.41 s.
The fresh `acceptance-release/` package preserves the source freeze, full CTest
log, build/configure output and hashes for all selected executable/library inputs.
It includes 1,686 native logging checks across JIT modes, 826 checks per independent
AOT mode, 244 filesystem observations, all UTF-8/UTC/budget oracles, 38 LSP checks,
forced allocation failures and native cancellation, 34 async AOT cases, and the
long-root retention measurements. Debug and strict profiles remain pending.

Final-source Debug acceptance passed **88/88** selected CTests in 2063.22 s.
Its source-file hashes match Release. Strict ASan/UBSan/leak validation has started
with the same source; the source and performance gate are not represented as
independently accepted. Final benchmark execution and all byte/count oracles
completed before Debug/strict compilation or validation began.

Final-source strict acceptance passed **87/87** selected CTests in 2024.84 s,
with ASan/UBSan/leak settings retained. Existing CMake configuration omits the
duplicate text-encoding stack registration under sanitizers; all selected strict
tests still inherited hard 1 MiB stacks. Together with Release and Debug, the fresh
validation is **263/263**, using identical final source hashes. Original incidents
and superseded checkpoints remain intact.

The original source-freeze manifests remain intact. Debug/strict also captured
16/32 generated text-encoding cache entries; cross-profile source comparison
excludes only those cache entries and confirms all 299 source hashes match. The
review package records the exact generated entries separately.

The isolated final workload contains 216 observations across normal/O2 JIT and
independent normal/O2 AOT. JIT O2 small-file logging improved from 267.697 to
17.415 ms per 200 records, against 2.973 ms direct I/O. Sequential async improved
from 64.205/3283.641 to 10.885/125.811 ms at 100/1,000 calls. The independent
async matrix adds 816 observations; framing adds 1,312, including instrumentation
overhead controls. Every complete-byte/count oracle passed. Remaining checked
byte traversal, per-escape state construction and retained-result GC work are
documented. The synchronous screen remains unmet; there is no performance
acceptance based solely on percentage improvement.

Final documentation is ready for a review package containing all changed/new
source and test files, a tracked patch, SHA-256 inventories, raw measurements,
all three acceptance logs and preserved failures. Packaging verifies source and
validated executable identities, baseline HEAD, an unchanged index and protected
README. Engineering stops for independent Golden Wrench review with the
performance gate **OPEN** and release **HOLD**. Native Windows/macOS is pending;
no staging, commits, branches, pushes, hosted CI or ptrace.
