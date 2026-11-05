# VOID logging — mandatory performance repair handoff

## Release gate decision — 4 November 2026 (documentation handoff authorized)

**LOG-PERF OPEN / release HOLD.** The [decision memorandum](LOG_PERF_DECISION.md)
consolidates accepted heavy screens (8.603 ms framing / 18.501 ms console) and
latest unchanged-production controls (7.408 / 18.536 ms heavy; file200 20.091 ms).
The original **file200 <10 ms** requirement remains **FAIL**. The approximately
2.06× direct-I/O result does not replace the absolute requirement.

**Allocator experiment: REJECTED.** The discarded allocator delivers only 3.0% file200 improvement with 40.7% more
requested map-storage bytes; its patch, positive component results and failures
remain preserved. CPU work, required `fsync`, measurement variation and unresolved
costs remain distinct. No specific narrow repair is established likely to close
the gate; the roughly 0.85 ms byte-snapshot scope cannot alone do so.

Least-work next action: Golden Wrench obtains the requirement owner's recorded
choice to retain <10 ms and HOLD pending separately scoped work, or explicitly
disposition the requirement. The evidence does not prove the target impossible
or authorize relaxation. The memorandum specifies evidence for an engineering
PASS and separate requirements for an owner exception. Earlier proposed next
actions below are historical and confer no authorization.

Boss authorized committing and pushing only `LOG_PERF_DECISION.md`, this handoff
and `docs/release/PREVIEW_BLOCKERS.md`, with `[skip ci]`. Supporting reports,
receipts, rejected patches and the release qualification handoff remain preserved
locally outside this commit. References to that uncommitted evidence resolve in
the working checkout. Earlier entries retain their historical review status.
No new code, experiment, infrastructure or regression run; HTTP/HTTPS and
installation qualification remain unchanged. **Stop after the documentation handoff.**

## Bounded rule allocation — 4 November 2026 (discarded; review pending)

**LOG-PERF OPEN; file200 <10 ms remains FAIL.** One invocation-local fixed-block
allocator was evaluated on the same Release LLVM 23.1.4 host. The production
change is discarded and `text_case.cpp` restored exactly to `11e95f4a`; no second
candidate is launched. All work remains uncommitted for Golden Wrench review.
[Report](SMALL_FILE_INVESTIGATION.md#bounded-rule-allocation-experiment--4-november-2026-discarded)
· [Receipt](evidence/small-file-20261104/results.json), `rule_allocation_experiment`
· [Rejected patch](evidence/small-file-20261104/rule-allocation-candidate.patch).

Uninstrumented original/candidate/candidate/original controls give six samples
per version: file200 **20.091 → 19.485 ms**, only **0.605 ms / 3.0%**. Treat that
end-to-end gain as small under the stop rule, despite quote200 improving 16.0%,
heavy framing 7.408 → 6.916 ms and heavy console 18.536 → 17.030 ms. No heavy
regression is observed. Map allocations fall 16,200 → 200; requested map-storage
bytes rise 1,166,400 → 1,641,600 (40.7%). Coarse preparation/disposal scopes and
allocation counts run separately; their larger deltas are not acceptance gains.

The candidate retains all validation, rooting, leases, quotas, NativeSafe cleanup
and atomic publication with no new shared Runtime contract or formatter change.
Release ownership passes 31,210 checks and native logging 1,688. A four-block
400-rule test passes 18 injected failures and ASan/UBSan/leak checks, including
complete release and aliased-output preservation. Broader strict ownership setup
exits 1 because the older retained strict frontend lacks `_translate_scalars`;
that failure is preserved and no full strict pass is claimed.

All 18 benchmark processes / 270 observations pass their bounded oracles.
54/55 compile/link/check/measurement commands exit 0; the strict setup above is
the sole exit 1. Prior evidence/failures remain intact. Snapshot-reserve and
unpolled-publication pause limits remain; tail latency and full native peak live
memory remain UNKNOWN. No commit/push/CI or installation/HTTP requalification.
**Documented and stopped for Golden Wrench review.**

## Translation attribution — 4 November 2026 (review pending)

**LOG-PERF OPEN; production unchanged at verified `11e95f4a`.**
[Latest report](SMALL_FILE_INVESTIGATION.md#translation-follow-up--golden-wrench-review-pending)
· [Evidence](evidence/small-file-20261104/results.json), `translation_followup`.
The byte-array copy proposal remains on hold and was not implemented.

Actual file200 already returns 200 translation aliases, with no replacement
assembly or transformed-string publication, after validating/snapshotting all
81 rules per call. One current sample spends 5.944 ms in translation, including
4.411 ms in the rule loop and 0.748 ms in disposal. The separate counter run
identifies 16,200 map-node allocations (1,166,400 bytes); short replacement snapshots
allocate no ordinary C++ storage. Input lookup is about 0.12 ms/200 calls.
Root/validation/allocator time within the loop is not independently established.
Coarse timer on/off checks shift medians 0.9–2.7%; sampled inner timings are
perturbed and are not extrapolated or used for acceptance.

Smallest candidate for review: per-invocation native node allocation for the
existing ordered map in text_case.cpp, retaining every validation, root, lease,
safepoint, duplicate-error order and NativeSafe disposal boundary. No persistent
cache, new managed lifecycle or output-builder change. This targets allocation
traffic, not the whole rule-loop cost; time savings remain unmeasured and may be
modest. No <10 ms promise. Current production-control file200 medians remain
21.238–23.755 ms (FAIL). All 28 commands and 13 benchmark processes pass, with
142 observations; prior reports/failures are preserved.

No implementation or new commit. The same five investigation files remain
uncommitted. Snapshot-reserve/unpolled-publication limitations remain; tail latency
and peak native-live memory remain UNKNOWN. Stop for Golden Wrench review.
Installation and HTTP/HTTPS acceptance remain closed.

## Small-file CPU attribution — 4 November 2026 (review pending)

**LOG-PERF OPEN / HOLD. No production change or new commit.** The approved
formatter remains at verified remote commit `11e95f4a6a7d60071bd754008662e88e022fbe16`.
The [CPU follow-up](SMALL_FILE_INVESTIGATION.md#cpu-follow-up--golden-wrench-review-pending)
and [receipt](evidence/small-file-20261104/results.json) retain contemporaneous
controls, detailed/reduced timers and a separate allocation run. All 16 final
benchmark processes pass their bounded oracles (276 rows); three harness setup/
source diagnostic failures and earlier observations remain preserved.

Short frame200 is the largest measured CPU component (control medians 7.187–7.776 ms).
A same-sample reduced-timer write-only breakdown totals 14.026 ms: translation
4.934 ms, GC 0.794 ms, byte snapshot 0.846 ms, checked I/O reply 0.888 ms and
native writes 0.516 ms. Independent component medians are not summed. Detailed
root/lock timers substantially perturb execution; their counts establish repeated
work, not mutex contention. File200 counts 41,435 ordinary C++ allocations,
including 16,600 within translation, and 17 collections.

The smallest separately reviewable repair proposal is a bounded bulk copy in
Runtime::byte_array_bytes, preserving its existing checked roots/lease/snapshot
contract. It targets a secondary cost under roughly 1 ms/200 records, so it
cannot alone close the unchanged <10 ms gate. No formatter, synchronization,
GC-safety or flush/close change is implemented or proposed for weakening.
The five investigation documentation/evidence files remain uncommitted.
Full-input snapshot reserve and unpolled final immutable-string publication
remain pause-time limitations; tail latency/native-live peak remain UNKNOWN.
Stop for Golden Wrench review. Installation and HTTP/HTTPS acceptance stay closed.

## Small-file attribution — 4 November 2026 (review pending)

**LOG-PERF OPEN / release HOLD.** Golden Wrench's reviewed formatter, targeted
checks and evidence are committed/pushed as
`11e95f4a6a7d60071bd754008662e88e022fbe16` with `[skip ci]`; `origin/main`
was independently verified at that exact SHA. No hosted CI was dispatched.
The dated entries below retain their historical state and evidence.

The separately scoped [small-file report](SMALL_FILE_INVESTIGATION.md) and
[receipt](evidence/small-file-20261104/results.json) isolate framing,
write, flush and close on the same Release/LLVM23 JIT O2 host. Original-runtime
file200 repeats at **26.806 ms** (FAIL <10); prepared direct I/O is **12.952 ms**.
Separate phases measure **15.138 ms** for logger writes200, **3.449 ms** for
prepared direct writes200, and about **6–9 ms** for dirty flush. Short frame200
is **7.505 ms**. Instrumented original file200 attributes **0.501–0.518 ms** to
native writes, **5.597–5.906 ms** to flush, and **0.006–0.008 ms** to close scopes.
Dirty logger close is dominated by its specified flush, not native close.
Control/profile differences demonstrate variation; independent medians are not
summed as exclusive attribution. The direct comparator is not a fixed floor.

All nine preparation/measurement commands exit 0; byte/count/checksum and
post-GC resource checks pass. Heavy framing/console PASS remains the retained
candidate result (8.603/18.501 ms), not a new run. Native peak-live memory,
platform scaling and tail latency remain UNKNOWN. Full-input snapshot reserve
and unpolled final immutable-string publication remain pause-time limitations.

Next: Golden Wrench reviews attribution before authorizing any further production
change. The smallest next diagnostic is a bounded per-record CPU split across
logger admission/guard/preparation/delivery and checked I/O bridge work. No
formatter rewrite or flush/sink semantic change is proposed. Only this new
investigation's report, receipt and handoffs remain uncommitted for separate review.
Installation qualification stays ACCEPTED / CLOSED; baseline and historical
crash evidence are preserved.

## Bounded framing implementation — 4 November 2026

**LOG-PERF OPEN / release HOLD. Golden Wrench approved the bounded candidate for commit and push.**
The authorized text.translate_scalars candidate and _quote integration are
implemented in the working tree. The current LLVM 23 check passed 48 observations
on the historical source path. Original LLVM 19 cause/classification evidence
is preserved. Golden Wrench explicitly approved existing frozen-backing
snapshots and atomic string publication.

Final Release/LLVM 23 JIT O2 medians are heavy framing **8.603 ms** (PASS ≤10),
full heavy console **18.501 ms** (PASS ≤20), and file200 **21.390 ms** (FAIL <10).
The current prepared-file control is 12.195 ms. Text JIT/AOT, 31,224 ownership
checks, 15 logging byte/UTC checks, 1,688 logging JIT checks and 827 checks per
AOT mode pass. The initial candidate and failed build diagnostics are retained.
The report includes the source diff identity, measurements, safety boundary and
remaining limitations. [Candidate report](BOUNDED_FRAMING_RESULT.md) · [Evidence](evidence/llvm23-framing-20261104/results.json)

The smallest next measurement is a bounded small-file/direct-I/O comparison
separating framing, writes and flush/close costs after candidate review.
Installation remains closed at d784c049. The reviewed logging candidate is authorized for commit and push with [skip ci].
Further small-file investigation remains separately scoped for Golden Wrench review.


## Retained LLVM23 performance baseline — 4 November 2026

**Performance OPEN; release HOLD. Stop for Golden Wrench review before production
changes.** The [new report](PERFORMANCE_BASELINE_LLVM23.md) and
[machine-readable measurements](evidence/llvm23-baseline-20261104/results.json)
record fresh Release observations at
`ffd8920befe04a67e3eaef6aadcdf0233df13563`, pinned LLVM/Clang/LLD 23.1.4.
The logger source matches the historical measured hash; no implementation changed.

Original normal/O2 JIT and independent AOT yield 216 measured observations;
selected JIT O2 timing/allocation components yield 192 including warmups. Each
cell has warmup plus three samples; measured children retain GC=256 and hard/soft
1 MiB stacks. This is an active Linux developer desktop, not an isolated-host or
native Windows/macOS performance qualification. All measured exits are 0; no full
correctness matrix or historical crash launch was performed.

JIT O2 file200 **24.042 ms** fails <10 ms. Its contemporaneous direct-I/O control
is **11.662 ms**, so the relative aim passes at **2.06×** and the absolute miss
cannot be attributed wholly to logger overhead. Heavy 1-KiB/100 framing
**910.628 ms** fails ≤10 ms; full console **947.525 ms** fails ≤20 ms. Prepared
console bytes take 7.946 ms. The frame repeats 2,509,955 ordinary native
allocations, 77,100 managed constructions and 1,162 collections per 100 records.

Bounded disabled-sink/lazy, per-record metadata and post-GC heap/handle observations
meet their documented criteria. Full scaling/nonregression and platform coverage,
tail latency and peak/native-live memory remain UNKNOWN. The report separates
current component localization from historical exclusive timing attribution and
unproved causes of changed file-I/O timing. No numerical target is relaxed.

Next action: Golden Wrench reviews a separately scoped escape/framing repair plan
against the existing bounded scalar-substitution contract, safety refinements and
unchanged screens, and explicitly dispositions the historical implementation stop
condition before authorizing source changes. No speculative optimization, shared
Runtime primitive or logger integration was implemented.

Raw sources/receipts/binary hashes are retained privately under
`/home/shmellyorc/.local/state/void-log-performance/ffd8920b-20261104-9v8echee/`.
This report does not consume or reopen the exhausted historical LLVM19 capture
budgets below. Installation qualification is separately ACCEPTED 3/3; only that
documentation closure was committed/pushed. Logging changes remain uncommitted.

## Native C supervisor: 12 plus 50 complete — STOP for review

See [C_SUPERVISOR_CAPTURE.md](C_SUPERVISOR_CAPTURE.md). The completed private
C-supervisor campaign retained all twelve native statuses: **12 original VOIDC
launches, all exit 0, no new VOIDC SIGSEGV or core**. Seven controlled fixtures
verified normal/signal/launch-failure/timeout handling and retention of a native
status despite a subsequent report-writing failure. Their deliberate crash cores
are fixture evidence only.

Boss subsequently ran the separate 50-attempt campaign from the delivered ZIP,
after recovery established zero prior launches. It ran **15:10:41–15:26:52 PDT on
28 October 2026: all 50 native exits 0**, independently checked against durable raw
wait records. No SIGSEGV, new VOIDC core, timeout, launch failure, evidence error
or unknown status occurred. All 50 actual soft/hard 1 MiB stacks, filter `0x37`,
argv/environment and private mappings were verified; 2,400 PERF observations are
preserved without performance acceptance. Supervisor, collector and journal read
exited 0; the journal returned no entries. The full saved log and raw record
confirm attempt 50 completed although the user's paste stopped at its START line.

**The 50-launch budget is exhausted. Do not rerun the ZIP, delete its claim or
start another campaign.** No target remained active at review. The earlier 12
attempts and pre-launch ZIP remain unchanged and separately identifiable. Detailed
completion evidence is `c-supervisor-capture-50/post-user-run/verification.json`.

Original binary, all 61 inputs, prior cores and all earlier results remain intact.
There is no new resident-text or causal evidence from VOIDC. **Classification D:
unresolved. Performance OPEN; release HOLD.** No compiler/Runtime/LLVM repair or
logging implementation. The user-run outcomes have been inspected offline; no
new original core exists to analyze. **STOP for independent Golden Wrench review.**

Private handoff:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/c-supervisor-capture-50/EVIDENCE_INDEX.md`.

## Bounded DSE input investigation — cause remains unresolved

See [DSE_INPUT_REVIEW.md](DSE_INPUT_REVIEW.md). One private diagnostic executable
captured the complete logging pre-O2 module, optimizer configuration and pass
events. One source capture and one serialized replay each completed 458 DSE
invocations; one replay of the preserved Stage 11 module completed 35. All passed
pre/post LLVM verification. No unchanged-original launch or new core occurred.

The same-`finish` replays preserve VOIDC's null-target-machine optimizer recipe.
Pinned `opt default<O2>` is not equivalent: its inspected pipeline adds SLP
vectorization and uses a target machine. Ordinary `opt` optimization controls were
therefore left unused. Diagnostic preparation and comparison-tool errors are
preserved and reconciled separately from native crashes.

No invalid input or crash-linked broken invariant was established. Original
executable, all 61 inputs, core and historical evidence remain intact. Shared
production correctness cannot be excluded from impact. **Disposition D; STOP for
Golden Wrench review. Performance OPEN; release HOLD.** No production/test repair,
optimizer workaround or logging implementation.

Private handoff:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/dse-input-investigation/EVIDENCE_INDEX.md`.

## Resident-text follow-up exhausted — no new core

See [RESIDENT_TEXT_CAPTURE.md](RESIDENT_TEXT_CAPTURE.md). The separately authorized
12-launch budget ended with eleven exit-0 observations and one launcher metadata
failure after the original process started; that native exit is unknown and is
not a pass. No new core was captured. Each complete run verified `r-xp` executable
mapping, process-local filter `0x37`, and actual soft/hard 1,048,576-byte stack.
Original executable/source identities and prior evidence remain intact.

There are no new resident bytes, fault registers or failing backtrace to compare.
The earlier LLVM DSE general-protection capture remains the fault evidence, with
cause pending. **Disposition D; STOP for Golden Wrench review. Performance OPEN;
release HOLD.** No target runs beyond the bound, production/test repair or logging
implementation. The report records the launcher defect and the preserved
diagnostic-tool outcomes explicitly.

Private handoff:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/resident-text-capture/EVIDENCE_INDEX.md`.

## Native core captured; cause pending — STOP for Golden Wrench review

Current baseline: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
See [CRASH_CLASSIFICATION.md](CRASH_CLASSIFICATION.md) for the complete bounded
investigation. **Disposition D: unresolved. Performance OPEN; release HOLD.**

The unchanged original executable and all 61 original inputs were verified.
Attempts 1–9 passed; attempt 10 reproduced SIGSEGV and yielded an unmodified-process
Linux core. Further launches stopped immediately. New incident: `incident-w415alhc`.
Original incidents `incident-vs4b96zy` and `incident-tg6xt8tp` remain preserved.
Actual soft/hard execution stack: 1,048,576 bytes. Only process-local core
configuration and evidence-directory privacy permissions changed.

The core and kernel journal identify a general-protection fault in LLVM 19.1.7
`eliminateDeadStores`, through VOIDC's O2 compilation pipeline, before generated
benchmark execution. RIP `0x5565df5f470e`; RSP `0x7ffd8c5ec580`. The stack state
does not support stack exhaustion. On-disk disassembly shows `shr $0x4,%eax` at
the PC, but the core omits resident executable-text pages. No invalid compiler
operation or broken Runtime invariant is proven. Shared production correctness
remains potentially affected; this is component localization, not a repair.

Private evidence:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/native-core-classification`.
Core SHA-256: `2c313fd4d8fba185efdceb35f519a5f6d8bc3d80e47e37b3ed367569405aad24`.
`EVIDENCE_INDEX.md` and `review/void-log-native-core-review.tar.gz` provide the
compact handoff without raw cores or binaries. Complete original environmental
identity cannot be claimed because the original full environment was not retained.

No production/test changes, no secondary instrumentation and no additional
original-command runs after capture. `text.translate_scalars` implementation
remains blocked. The next review concerns cause-specific evidence collection,
including the resident-code gap; no optimizer/Runtime workaround is justified.

## Previous investigation: architecture approved; crash reproduced

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

## Prior decision: stop before a new shared Runtime contract

Continuation baseline: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
**Architecture review required; performance gate OPEN; release HOLD.**
Neither remaining hot path is repaired to acceptance. No production or repository
test source changed. The explicit stop instruction is in Boss's current directive,
section 3; it is not an inferred permission requirement.

Review [HOTPATH_REVIEW.md](HOTPATH_REVIEW.md) for the measured alternatives, exact
root causes, explicit targets and proposed ownership/GC/aliasing/failure contract.
The requested decision is whether to approve or revise a generic bounded Unicode
scalar substitution operation in `text`, with logging escape policy retained in
source. The proposed shared operation has not been implemented.

Ordinary file200 reproduced at 17.323 ms against 2.981 ms prepared direct I/O.
Schema views improve small quote time 8.875 → 7.425 ms but leave full console
13.477 ms. Flat construction plus read/write key views improves expanded heavy
frame 926.213 → 343.877 ms, while occasional-escape 1-KiB framing regresses
138.957 → 181.101 ms. These experiments do not meet acceptance. The expanded
matrix covers ASCII, occasional escapes, heavy controls, ordinary Unicode and
mixed escaped Unicode at 64 B, 1 KiB and near-budget sizes. Fair prepared-I/O,
complete source encoding/framing/I/O and native algorithm controls are distinct.

New raw evidence lives at
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/`.
Its `EVIDENCE_INDEX.md` indexes every dataset, hashes, receipts and the compact
`review/void-log-hotpath-review.tar.gz` package. No new raw evidence/binaries are
staged or added under repository evidence. The previous 263/263 source/test evidence,
all original incidents and root README remain unchanged. No staging, commits,
branches, pushes, hosted CI or ptrace were performed.

### Newly encountered failures and limitations

| Observation | Preserved evidence | Actual disposition |
|---|---|---|
| Combined keys + flat source, JIT O2, exit -11, no measurement stdout/stderr | `crashes/incident-vs4b96zy`, `receipts/run-xr2lodp9`; executable SHA `c284712dd4958faf41547deb49a851306256a9075cf59d968671272bc17e96bb` | Unresolved. Phase/LLVM-signal diagnostic completed; no causal stack captured. Empty stdout does not localize compilation versus setup/first sample. No assertion that prior LLVM incidents explain it. |
| Diagnostic host compile exit 1: incomplete `llvm::StringRef` | `crashes/incident-asxnan14`, `receipts/run-0e_vtqsw` | Test-host header omission corrected by including `llvm/ADT/StringRef.h`; all five subsequent host builds passed. Original source and stderr retained. |
| Launcher used unavailable `python` | `drivers/void-hotpath-diagnostic-build.log` | No Python/build process started. Used installed `python3`; no product change. |
| Async confirmation extractor expected `PERF` rather than `ASYNC` | `baseline/async-confirm.stdout`, original empty `async-confirm.json`, corrected `async-confirm-parsed.json` | Parsing corrected from preserved stdout; 64 samples recovered, no rerun. An exploratory median over the empty derived list also failed before correction. |
| Original async1000 higher than historical median | `baseline/original-repeat.json`, separate `original-confirm.json` | 158.233 then 141.688 ms on the same frozen executable; isolated-matrix async1000 132.280 then 120.285 ms. Specific environmental/cost cause unresolved. No changed scheduler code and no post-change regression-fix claim. |

Instrumented counts and uninstrumented latency are separate. Three-sample medians
are host observations, with an active desktop; compiler time is separate. No new
Debug/Release/strict complete matrix is claimed. Full normal/O2 JIT/AOT, hard-stack,
GC/OOM, array/reference/lease, logger byte/budget/UTC, sink/rotation/partial-write,
async lifecycle, cache/reload/tooling/installed checks remain required after an
approved implementation. Native Windows/macOS remains pending.

### Full modified source/test inventory

Production source: **none**. Repository tests: **none**. Documentation:
`docs/log/HOTPATH_REVIEW.md` (new), `PERFORMANCE_REPAIR.md`, `BENCHMARKS.md`,
`IMPLEMENTATION.md`, `HANDOFF.md`. External experiment source patches, host/oracle
sources, generation/build/run drivers and SHA-256 inventories are included in the
compact review package. They are candidates for review, not production changes.

## Previous sprint handoff (preserved report)

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
