# Logging performance repair

## Architecture approved; implementation blocked by reproduced crash

Boss approved `text.translate_scalars` with mandatory refinements. That approval
remains valid. Its section 1 also requires stopping if the experimental SIGSEGV
cannot be isolated sufficiently to exclude a relevant production defect.

The original executable and all 61 captured source files were hash-verified.
The crash **reproduced on the third original-command O2 run**, exit -11, with
empty stdout/stderr. The new original-binary incident is `incident-tg6xt8tp`;
`incident-vs4b96zy` remains unchanged. Two phase-instrumented O2 runs, one normal
run, five original-binary signal-preload runs, strict ASan/UBSan/leak normal/O2,
and three preload runs without unwinder prewarm all passed. Across all 16
invocations: **15 passes, one signal**. None of the diagnostic variants captured
a failing stack. Passing controls do not resolve the original defect.

The fault remains unclassified among harness, generated prototype code and
compiler/Runtime; a relevant production correctness defect cannot be excluded.
**Implementation stops under the explicit section 1 condition.** No production
or repository test source changed; the suspect flat builder was only diagnostic
input. The approved text operation and logger integration have not started.
No new full correctness matrix or performance acceptance is claimed. Performance
gate **OPEN**, release **HOLD**.

Detailed escalation and raw evidence:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/approved-crash-classification/ESCALATION.md`.
That directory also contains bounded plans, exact commands/hashes, all receipts,
a separate raw index and `review/void-log-approved-crash-review.tar.gz`.
No ptrace or Git mutations were used. Historical evidence and the previous
263/263 result remain unchanged. The next decision concerns a bounded causal
crash investigation, not another approval of the text architecture.

## Prior checkpoint: architecture review required

The hot-path continuation starts from `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
Both remaining paths were measured, but neither has an accepted repair. **No
production/test source changed.** Scalar key views help ordinary reads but leave
checked traversal dominant; a flat builder plus key views reduces heavy framing
to 343.877 ms while regressing occasional escapes. Expanded byte-checked controls,
explicit heavy targets and a proposed general bounded scalar-substitution operation
are in [HOTPATH_REVIEW.md](HOTPATH_REVIEW.md). Boss explicitly requires architecture
review before implementing a significant shared Runtime contract, so work stops
at that boundary. Performance gate **OPEN**, release **HOLD**. The earlier 263/263
result below is preserved baseline evidence, not validation of a new repair.

All new raw evidence is outside Git at
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/`;
see its `EVIDENCE_INDEX.md` and compact source/review package. The original new
SIGSEGV remains unresolved and preserved. Benchmark tables and limitations appear
in the continuation section of [BENCHMARKS.md](BENCHMARKS.md).

## Previous sprint (preserved report)

Status: **both investigations and main repairs complete; performance gate OPEN,
release HOLD pending independent Golden Wrench review**. Fresh correctness
validation passed 263/263 selected CTests. The synchronous screening target is
unmet. Original 93/93 correctness evidence and all earlier measurements remain
preserved; neither those passes nor the improvements constitute performance acceptance.

## Preserved baseline

`evidence/performance-repair/baseline/` contains an immutable source archive,
original evidence archive, all three exact executable files, CMake/native toolchain
configuration, source/executable SHA-256 manifest, process inventory, and 216 new
isolated raw samples. Every executable hash also matches its original row in
`evidence/benchmarks.json`. Production sources were verified unchanged after the
repeat. The original evidence and old benchmark report are not replaced.

No build or validation suite ran concurrently with these measurements. The user’s
desktop remained active on the same host. GC allocation budget is 256. Each sample
runs on an actual soft and hard 1 MiB stack. JIT compilation and AOT admission are
reported separately from invocation timing; these calls bypass persistent cache.
The original benchmark performs one warm invocation and three timed observations.
Each starts after an explicit collection; that collection is outside timing.
Per-record work, task lifetime and flushing are unchanged from the original.

Isolated JIT O2 median milliseconds (individual observations in the JSON):

| Workload | Original | Isolated repeat |
|---|---:|---:|
| 200 file records, 64-byte input | 265.32 | 267.70 |
| Equivalent direct file output | 3.04 | 2.92 |
| 200 console records | 257.91 | 257.12 |
| Equivalent direct console output | 1.41 | 1.41 |
| 100 records, 1-KiB input | 1935.46 | 1952.76 |
| Three records, 65,500-byte input | 3706.83 | 3711.91 |
| 100 sequential async calls | 68.57 | 64.21 |
| 1,000 sequential async calls | 3318.72 | 3283.64 |

Both concerns reproduced. Desktop noise did not explain either order-of-magnitude
gap. The component investigations below established the mechanisms before the
production corrections were selected.

## Synchronous measurement design

`tests/log/performance_framing.void` is appended only to a copied stdlib module.
It exposes independent component probes for admission, UTC sampling, fixed-instant
timestamp formatting, UTF-8 conversion, escaped-size scanning, escaping, complete
framing, isolated array traversal, isolated byte pushes, existing `io/memory`
bulk writing, direct console transfer, and full console logging. An interpolation
comparator measures a lower bound for framing text that needs no escaping; it is
not presented as a complete replacement for logging.

Input creation and the independent complete-record preparation occur outside each
timed component. ASCII, control/quote/backslash escapes and multi-byte Unicode are
measured separately, with small, 1-KiB and large bounded records. Exact counts and
observable checksums are retained. Each stage records its first invocation and
three subsequent observations. The first invocation is cold for that stage, not
a claim that the OS, compilation or all shared helpers are cold.

`performance_build.py` builds separate uninstrumented and instrumented binaries.
The latter uses generated copies of existing Runtime translation units with named
timers, operation counters, ordinary native allocation counts and explicit
`memcpy`/`memmove` byte counts. These source overlays and their hashes are retained.
Explicit copy counts cover the selected Runtime translation units, including
root/header copies, and are not claimed to count every copy inside the C++ library
or OS. Native allocation counts use the existing managed test allocation shim;
they cover ordinary C++ new allocations, not unrelated process or driver memory.

Native timers report exclusive nested time. Their overhead is assessed against
the separate timing executable; instrumented wall time is never substituted for
production throughput. Heap snapshots, collection epochs, native operation counts,
CPU and elapsed time are recorded independently. No ptrace is used.

## Established synchronous causes and corrections

The original ASCII 64-byte / 200-record component medians were 76.764 ms for
`_escaped_size`, 153.793 ms for `_quote`, 240.329 ms for `_frame`, and 248.660 ms
for full console logging. Independent pushes took 40.162 ms; bulk memory writing
3.684 ms. Thus byte pushes were only one contributor. In the original instrumented
frame, 380,400 list-header validations, 380,200 root lookups, 381,400 reference
validations and 1,307,217 polls dominated small GC (0.88 ms) and growth copying
(0.31 ms). These counts establish redundant checked traversal, rather than a
speculative allocator or transport diagnosis.

Corrections, in the order measured in the immutable checkpoints:

1. `stdlib/log.void`: replace the per-record sizing pass and per-byte output
   assembly with one budgeted escape scan. `_next_escape` has a small precise-root
   frame and skips complete known-safe UTF-8 scalars. `_quote` returns the original
   immutable string when no escaping is necessary. Otherwise it uses the existing
   `io/memory.stream` bulk writer for unchanged spans and escape replacements.
   The source remains strict UTF-8; all original control/bidi/separator escapes
   remain. `_frame` performs subtraction-based byte-budget checks and joins the
   final record once before transfer. Configuration still uses `_escaped_size`.
2. `src/frontend/compiler.cpp`: ordinary scalar growable-array value reads no
   longer acquire a selected-write lifetime, version and repeated normalization.
   The owner is snapshotted before evaluating the index; index side effects run
   before the checked read. Selected assignment/receiver paths and composite or
   fixed-value accesses retain their prior machinery.
3. `src/backend/llvm/backend.cpp`, `src/runtime/list.cpp`,
   `src/runtime/fixed_abi.cpp`, and the corresponding Runtime/ABI declarations:
   the scalar value read normalizes a signed index and loads against one checked
   header under the existing lock. Root provenance, element encoding, bounds and
   readonly authority checks remain. Unsigned subtraction handles INT64_MIN
   without undefined behavior. The new ABI is general array functionality, not a
   logging intrinsic; the old unsigned read ABI remains available.
4. `src/runtime/array.cpp`: UTF-8 byte conversion uses the existing dense-u8
   `BufferBuilder::write_bytes` operation after taking the existing owned input
   copy. Private rooting, bounded polling, quota checks and atomic publication
   remain; the per-byte builder loop is gone.

The growable-array regression checks receiver/index evaluation order, rebinding,
resize/clear effects, negative and unsigned indexes, malformed ABI flags, invalid
roots, forced GC and selected-write invalidation. Logging byte/UTC/budget oracles
passed after the synchronous changes; final expanded profile verification is
recorded separately below.

The `framing-after/` checkpoint (before the final handle-removal correction)
contains 544 timing and 544 instrumented observations. Independent C++ byte
oracles check direct and logged output for all ten inputs. The original nine
inputs remain unchanged; shape 9 adds 30,816 escape-heavy input bytes, 65,484
escaped bytes and 65,495 complete record bytes. Whole-frame medians in milliseconds:

| Input/count | Original | Repaired checkpoint |
|---|---:|---:|
| ASCII 64 B / 200 | 240.329 | 10.545 |
| ASCII 1 KiB / 100 | 1803.814 | 59.937 |
| ASCII 65,500 B / 3 | 3427.094 | 110.610 |
| Escaped 64 B / 200 | 231.895 | 122.878 |
| Escaped 1 KiB / 100 | 1737.150 | 924.164 |
| Escaped 16,000 B / 3 | 810.677 | 415.111 |
| Unicode 64 B / 200 | 191.628 | 8.134 |
| Unicode 1 KiB / 100 | 1421.805 | 39.174 |
| Unicode 65,472 B / 3 | 2695.420 | 72.253 |

Ordinary text now takes one checked traversal plus a bounded number of whole-data
copies. Escaped spans are disjoint, total replacement work is proportional to
encoded bytes, and the existing memory writer grows geometrically. No repeated
full-record concatenation occurs per character. Escape-heavy input still has a
large constant cost: small literal arrays, checked slices, stream-state objects
and Runtime validation for each replacement. At shape 4, `_quote` alone made
76,900 bulk writes, 784,400 list-header validations and 77,100 managed
constructions for 100 records. The raw counters separate those costs from copying
and GC. The 64-byte escape-heavy seed contains 40 escaped scalars: 40 replacement
writes plus nine unchanged spans give 49 bulk writes per record, matching the
observed 9,800 writes for 200 records. Each stream write constructs its next
source state, explaining the 10,200 source construction calls including stream
setup. Repeated small checked operations, rather than quadratic payload copying,
explain this remaining cost. It is not accepted merely because the before/after
ratio improved.

## Established async causes and corrections

The independent matrix compares direct sync/async I/O and sync/async logging at
100/250/500/1,000 operations, console/file, timestamps off/on, plus empty async
activations. Console has no native async API: its direct async comparator wraps
the same synchronous transfer in an ordinary async activation. File creation,
awaits, writes, flush/close and the natural root lifetime are retained. Every
output byte, transfer count and flush count is checked. No records are dropped.

For exact timestamp byte comparison, a **test-only** clock overlay performs the
real UTC sample and then pins its returned instant to
2021-01-01T00:00:00.123Z in both direct and logging paths. All per-record sampling
and formatting remains. The unchanged original benchmark uses the production
clock and is reported separately. Compile/emission/link costs remain outside
execution timing. Timers are exclusive within each thread; worker and main-thread
time can overlap and must not be summed as process CPU time.

`async-before/` contains 272 timing and 272 instrumented samples. JIT O2 file
logging grew from 22.915 to 3059.067 ms over 100 to 1,000 calls. Direct async file
I/O grew from 5.411 to 118.630 ms; empty activations from 0.364 to 20.046 ms. The
independent empty workload establishes a shared Runtime problem.

Two dominant defects and a smaller cleanup defect were measured:

* `src/runtime/async.cpp`: linear task lookup and repeated scans of all retained
  tasks at checkpoints, worker reception, readiness, pending/failure and child
  cleanup checks. At 1,000 log calls this performed 30,290,401 lookup visits,
  101,349,082 reception visits, 136,136,209 failure-check visits, 10,163,574 pending
  visits, 12,202,838 collection visits and 14,221,792 child visits. Completed
  history was revisited throughout subsequent execution.
* `src/runtime/heap.cpp`: conservative buffer address bounds sent ordinary
  async-frame output roots into an alias slow path that walked **every heap
  object**. The separate `async-heap-before/` overlay counted 131,993,278 visits,
  consuming 1509.68 ms at 1,000 calls. Most objects were unrelated retained tasks.
* `src/runtime/coordination.cpp`: persistent Handle destruction searched and
  erased a vector entry, shifting the remaining root history repeatedly. After
  the first two corrections, instrumented destruction still grew from 0.09 ms at
  100 operations to 7.06 ms at 1,000 operations (859 versus 8059 removals).

The scheduler now keeps an address index, an intrusive live-task scan list and
creation-ordered owned-child links. Owner/group pending and unobserved-failure
counters handle the common successful case without scanning history. The original
ready and worker queues remain. All Task/result handles and diagnostic records
remain until root exit; finished frames still retire only after resume returns.
The scan list preserves creation order, including out-of-order start and native
cleanup reactivation. Reactivation allocates nothing. Native publication rollback
removes all indices before destroying its task. Prepared tasks, cancellation,
shielded drains, repeated awaits and creation-order failure selection retain their
contracts. Exceptional failure aggregation still scans the relevant owned children.

The heap now links its existing BufferState metadata into a list of buffers only,
including unpublished private buffers. Insertion links after successful adoption;
collection/destruction unlinks before releasing storage. Metadata stays stable
across object-table rehash. The same conservative bounds, overflow-safe overlap
checks and leased/unleased rejection remain. There is no new allocation for this
index. Tests collect middle/head/tail/final entries, abandon private buffers,
rehash the object table, and verify every survivor's alias rejection.

Persistent Handles now store their vector index. Removal swaps the last handle
into the removed slot and updates its index under the existing mutex, then pops
the tail. Registration's allocation failure remains transactional; removal cannot
allocate. Every live root is still enumerated. Root enumeration order is not a
language contract. Tests repeatedly remove out of order, collect after each
removal, check all surviving roots and destroy a handle from a native host thread.

After the first two repairs, `async-after/` measured file logging at
11.392/27.031/58.861/147.339 ms and empty activations at
0.201/0.522/0.986/1.987 ms. At 1,000 calls reception visits fell to 192,225 and alias
visits to 590,738; 15,098 indexed lookups replaced the historical lookup scans.
Successful pending/failure checks visit no task history. These measurements are
an intermediate checkpoint; final results include the handle correction.

## Remaining costs and limits

Completed result retention is intentional and unchanged. With allocation budget
256, later collections enumerate more retained handles: the intermediate
1,000-call sample had 398,988 cumulative handle visits and 24.05 ms of collection
work. That is a genuine root-lifetime/GC-policy cost, not scheduler scanning.
This sprint does not shorten task lifetime, change the budget, disable collection
or claim asymptotically linear GC for arbitrarily long roots. Final timings and
per-call costs must distinguish this remaining curvature from the repaired scans.

Synchronous small-record performance remains above the single-digit/2–3× direct
I/O screening target in the final measurement. Checked byte traversal dominates
ordinary text; escape-heavy construction remains particularly expensive. The
scalar `list_header` overload in `src/runtime/list.cpp` still materializes its
scalar-schema lookup key on every read. This explains much of the remaining
ordinary native allocation count (16,222 allocations for 12,800 reads in the
small quote probe); catalog/nominal/backing validation and precise-root lookup
also remain per read. These are identified remaining costs, not claimed to be
unavoidable or already optimal. No wider compiler proof/hoisting or new bulk
text primitive has been implemented. The performance gate stays **OPEN** until Golden Wrench reviews the complete final
cost accounting and explicitly accepts the remaining costs or authorizes further
work. There is no throughput acceptance based solely on percentage improvement.

## Final measured result (validated Release source)

The original workload completed 216 final observations in normal/O2 JIT and
independent normal/O2 AOT. In JIT O2, 200 small file records take
**17.415 ms [17.329, 17.529]**, versus direct I/O
**2.973 ms [2.889, 3.076]**. Console takes 14.608 ms; 100 1-KiB records
74.005 ms; three 65,500-byte records 135.523 ms. The original 100/1,000-call
async workloads take **10.885/125.811 ms**, versus isolated baseline
64.205/3283.641 ms. These are execution times; compilation is separate.

The final independent matrix has 272 JIT timing, 272 AOT timing and 272 AOT
instrumented observations. File logging without timestamps takes
10.792/27.525/58.458/138.219 ms at 100/250/500/1,000 calls. Empty async
activations take 0.184/0.409/0.992/1.746 ms. The original and independent
workloads have different source wrapper structure; their numbers are not mixed.
The indexed handle repair reduced instrumented median cleanup to 0.044 ms at
100 calls and 0.388 ms at 1,000 (one representative sample is 0.440 ms), while
GC still costs 0.536/23.682 ms. The natural completed-result retention contract
explains the remaining growth beyond tenfold. No unbounded-root linearity claim.

The final framing suite completed 544 JIT timing and 544 AOT instrumented
observations, plus 224 AOT timing controls. All independent complete-byte and
length oracles passed. [BENCHMARKS.md](BENCHMARKS.md) contains final component
times, all four original modes, CPU time, before/after tables, allocation/copy/GC
counters, timer overhead and per-call async costs. Raw samples and source/executable
hashes remain alongside every checkpoint. Measured sources matched the Release
acceptance freeze before and after the isolated run.

## Verification and handoff

Expanded Release, Debug and strict ASan/UBSan/leak validation passed
88/88, 88/88 and 87/87 selected CTests respectively, **263/263 total**, on identical
source hashes. CTest elapsed
times were 711.41 s, 2063.22 s and 2024.84 s respectively. Fresh profile evidence
lives under `evidence/performance-repair/`;
the pre-repair 93/93 reports remain historical. Actual hard 1 MiB stacks, forced
GC/OOM, native cancellation/abandonment, arrays, buffer aliases, coordinator roots,
async lifecycle/retention, independent normal/O2 AOT, cache, reload, tooling,
installed execution, ownership, rotation and partial writes are included. Existing
CMake configuration omits the duplicate text-encoding stack registration under
sanitizers; strict text execution still inherits the hard stack limit. The
[handoff](HANDOFF.md) explains exact counts and the generated-cache entries in
the original source manifests.

Native Windows/macOS remain pending. No hosted CI, Git staging/commits/branches/
pushes, protected root README edits or ptrace have been performed. Original
incidents remain preserved through the shared text/encoding crash recorder.
