# Small-file logging attribution — 4 November 2026

## Validated no-match translation experiment — 5 November 2026 (discarded)

**One authorized candidate evaluated and discarded. Production translation is
restored byte-for-byte to HEAD `da2edda0`. LOG-PERF remains OPEN / release HOLD;
the original file200 <10 ms requirement remains FAIL. Stop after this experiment.**
The patch, tests, raw samples and failed harness setup remain uncommitted for
Golden Wrench review. No second implementation or tuning candidate was attempted.

The complete workload improved materially: **21.984 → 16.688 ms**, a reduction
of **5.297 ms / 24.1%**. However, the required absence of a heavy-path regression
was not established. A confirmatory comparison of the same binaries measured
heavy framing **9.4% slower** and console **4.2% slower** for the candidate. One
candidate console block had a **20.731 ms median**, failing its ≤20 ms screen.
Discard conservatively under the approved decision rule. These active-host
observations do not isolate the cause of the heavy-path movement.

[Complete receipt and samples](evidence/no-match-20261105/results.json) ·
[Rejected patch](evidence/no-match-20261105/candidate.patch) ·
[Artifact hashes](evidence/no-match-20261105/SHA256SUMS.json)

### Exact candidate and restoration

Only `src/runtime/text_case.cpp` received a production-code change: **46 added,
five removed lines**. Inside `Runtime::translate_scalars`, after the existing
root/schema/count checks and backing lease acquisition, an invocation-local
probe accepted at most **128 ASCII input bytes**, **256 rules**, and **16 KiB**
of input plus replacement extents. Two 64-bit words recorded byte presence.
Under the existing mutex it checked each current replacement's string identity
and extent, scalar validity, strict key ordering and disjointness from the input.
It did not copy replacement payloads or construct map nodes on proven no-match
calls. Null/empty managed strings retained their existing representation.

A match, invalid or non-increasing key, non-ASCII/long input, large table, or
quota limit fell back to the original loop **inside the same selected backing
leases**. Reference validation preceded the current key check; probing stopped
at the first inconclusive entry, so later entries could not mask an earlier
invalid rule. Unsorted valid tables still used the general path. Invalid rules
were resolved before negative/insufficient budgets. A proven no-match result
used the original checked alias publication, factored into a local lambda and
shared with the general unchanged-result branch. No persistent state or public
API was added. Leases and heap borrows stayed within their existing lifetime and
mutex rules; general safepoints and NativeSafe disposal remained in place.

The [focused VOID fixture](evidence/no-match-20261105/translation_no_match.void)
and [native checks](evidence/no-match-20261105/native-extra.cpp) were experimental
test artifacts. The VOID fixture was removed from `tests/` on rejection; no
CMake or existing test source change remains. `git diff --exit-code` confirms
production translation, logging source, existing ownership tests and CMake are
unchanged. Restored translation SHA-256 is
`a977bdb662633f28ac373a713f6daf4920cc62ebc2b40d3f840b54dd2fd6e24a`, equal to HEAD.
Logging sinks, fsync, HTTP, installation CI, licenses and protected README were
not changed. Earlier rejected allocator evidence below is preserved unchanged.

### Correctness and failure results

- Both the new fixture and existing `text.void` passed **normal/O2 JIT and normal/O2
  linked AOT**. The AOT objects were linked with the same candidate translation
  object before the retained Runtime archive. No stale translation implementation
  was used for the candidate checks.
- Coverage includes 200 distinct and repeated ordinary messages, escapes, Unicode
  with/without matches, empty input, 128/129-byte boundaries, ASCII bit boundaries,
  sorted/unsorted and duplicate tables, invalid scalars, mismatched counts,
  negative/insufficient/exact budgets, same-backing indexed key and replacement
  mutations, input/replacement aliases, oversized replacement payloads and
  257-rule fallback.
- Focused native/JIT checks passed **879** assertions. Additional native checks
  passed **876**, including checked output authority, malformed schemas, original
  alias identity, late corrupt replacement extents, invalid/duplicate-rule
  precedence over later corruption, pending collection and collection after
  failure. The native-only host also passed **876** checks with ASan/UBSan and
  leak detection, using the current candidate TU over the retained strict Runtime.
  Its older frontend was intentionally not invoked; this is not full current
  strict-source qualification.
- No-match allocation sweeps recovered **four injected failures each** for
  0/1/81/256-rule cases and **262** for 257-rule fallback. Failure left output
  untouched and roots collectible; success published exactly the original alias.
  Removed allocations can change injection ordinals. The existing translation
  ABI sweep recovered **11** failures. The existing ownership suite passed
  **31,224** checks across normal/O2 JIT with GC and allocation-failure coverage.
- Existing logging integration passed **1,688** checks, including startup,
  reload, native partial/error boundaries, allocation failures and cancellation.
  Benchmark complete-byte/transfer-count and post-GC payload/handle oracles passed.

One **harness setup failure** is retained: the extra native test initially passed
64-bit all-ones as i32 -1 to `list_write`, which correctly rejected invalid scalar
bits. The test was corrected to `0xffffffffu`; production code was not changed.
The initial source, driver, binary identity and
[failed output/receipt](evidence/no-match-20261105/initial-harness-failure/receipt.json)
remain. This was exit 1, not a signal or sanitizer finding. All **47 child-command
receipts** are preserved: **46 exit 0, one exit 1**. No failed run was erased.

### Complete workload and heavy screens

Same retained Linux host and Release LLVM/Clang/LLD **23.1.4** build, Runtime
GC budget **256**, JIT **O2**, actual checked soft/hard **1 MiB** stacks. The
control and candidate each override only translation in the same retained
libraries. No translation timers or allocation counters are enabled. Compilation
and admission precede timing. File200 retains acquisition, **200 writes, required
fsync and close**, with full byte/count oracles. Prepared direct I/O is a separate
comparator and does not replace the requirement.

Primary order was **control A, candidate A, candidate B, control B**, each cell
with one warmup and three measured samples. Pooled medians below use six measured
samples per version. Brackets retain the actual measured minimum and maximum;
no outlier is removed.

| Primary workload, ms | Production control median [range] | Candidate median [range] |
| --- | ---: | ---: |
| Complete file200 | **21.984 [20.139–25.132]** | **16.688 [16.295–20.751]** |
| Prepared direct file200 | 9.606 [9.268–9.863] | 9.420 [9.341–12.476] |
| Short ASCII quote200 | 4.801 [4.665–6.253] | 1.701 [1.690–1.775] |
| Short complete frame200 | 7.619 [7.249–9.233] | 4.387 [4.303–4.545] |
| Short full console200 | 11.556 [10.969–12.731] | 8.145 [8.011–8.488] |
| Heavy quote100 | 5.547 [5.305–6.309] | 6.104 [5.769–6.694] |
| Heavy complete frame100, ≤10 ms | 7.817 [7.182–8.370] | 7.985 [7.753–8.471] |
| Heavy full console100, ≤20 ms | 18.293 [17.492–19.203] | 19.462 [18.704–19.845] |

File200 block medians were **21.056 / 16.621 / 17.222 / 22.809 ms** in execution
order. Process CPU medians were **15.691 → 10.865 ms**. Every measured candidate
file200 sample still fails <10 ms. Primary heavy console was **6.4% slower**
pooled, while the control block medians moved **19.045 → 17.874 ms**. This
unresolved heavy-path concern justified one heavy-only confirmation with the
**same binaries**, not a new candidate or implementation change.

Confirmation order was **candidate A, control A, control B, candidate B**, again
one warmup and three samples per cell:

| Heavy confirmation, ms | Production control median [range] | Candidate median [range] |
| --- | ---: | ---: |
| Complete frame100, ≤10 ms | **8.197 [7.626–8.459]** | **8.970 [8.251–9.929]** |
| Full console100, ≤20 ms | **18.804 [18.221–19.206]** | **19.589 [18.873–25.172]** |

Candidate framing block medians were **9.291 / 8.876 ms**; controls were
**8.422 / 7.917 ms**. Console block medians were **20.731 / 18.883 ms** for the
candidate and **19.148 / 18.565 ms** for controls. Pooled candidate framing still
passes 10 ms but regresses **9.4%**; pooled console passes 20 ms but regresses
**4.2%**, with one failing candidate block. Passing pooled thresholds does not
satisfy the separate no-heavy-regression condition. **Discard and stop.**

### Reproduction and remaining limits

The receipt retains **152 timing rows**: **120 measured and 32 component warmup
rows**, plus eight untabulated original-workload warmups. It includes exact argv,
cwd, environment overrides, wall/CPU samples, load observations, source/library/
binary/toolchain hashes and restoration checks. Small source, driver and output
artifacts are copied beside it; large binaries and complete recorder incidents
remain private at
`/home/shmellyorc/.local/state/void-log-performance/no-match-zi9i7gbl/`.
Drivers are `prepare.py`, `check.py`, `extra-check.py`, `measure.py`,
`confirm-heavy.py` and `record.py`. Preparation uses the retained build at
`framing-candidate-hAebdlqN/build` and prior host sources named in the recorded
driver; replay needs those retained artifacts or verified equivalent inputs.
It creates a fresh private directory and never overwrites accepted binaries.

This is an unrandomized active-desktop study without CPU isolation, pinned
frequency or confidence intervals. The file gain is material in these samples;
no causal explanation or universal magnitude is claimed. Heavy no-regression
is not demonstrated. Only JIT O2 was performance-timed; other JIT/AOT modes
received correctness checks. Full current Debug/strict suites, other platforms,
tail latency, worst pauses and peak native-live memory remain unqualified.
The bounded speculative walk can add work before a fallback's existing poll;
exact worst-pause equivalence was not established. Existing full-input reserve
and final immutable-string publication pause limitations remain unchanged.
Production is restored, its earlier accepted heavy-path evidence remains
separate, and **LOG-PERF remains OPEN with the original file200 FAIL**.

## Release gate review — 4 November 2026

**LOG-PERF HOLD / OPEN.** The [decision memorandum](LOG_PERF_DECISION.md)
consolidates the accepted heavy-framing results, latest production file200
controls and rejected allocator trial. It recommends an owner decision before
further work and states exactly what evidence can change blocker status.
The original **file200 <10 ms** requirement remains **FAIL**. This documentation
review adds no measurements or implementation; earlier proposed next actions
below are preserved history, not current authorization.

## Bounded rule-allocation experiment — 4 November 2026 (discarded)

**One candidate evaluated; production source restored exactly to `11e95f4a`.
LOG-PERF remains OPEN. The unchanged file200 <10 ms screen FAILS.** No second
candidate or follow-on optimization is launched. Everything remains uncommitted
for Golden Wrench review. This section supersedes the proposed next action below;
the prior investigations and failures remain intact.

### Decision and contemporaneous measurements

The invocation-local allocator produces real component gains, including 16.0%
faster quote200 and 6.6%/8.1% faster heavy framing/console in these observations.
However, complete file200 improves only **0.605 ms / 3.0%**, from **20.091 to
19.485 ms**. Treat that end-to-end gain as **small** under the approved stop rule:
**discard the production change and stop**. This is a conservative disposition,
not a claim that the component gains are absent or that the allocator is incorrect.
The six-sample file200 ranges overlap, and contemporaneous direct-I/O medians
also shift by 1.8%; no noise correction or statistical confidence is claimed.
Requested map-storage traffic increases 40.7%, despite far fewer allocations.

Same retained Release LLVM/Clang/LLD **23.1.4** host, C++ O3, JIT O2, GC=256,
actual soft/hard **1 MiB** execution stacks. No compilation or correctness checks
overlap measurements. Uninstrumented blocks run original A, candidate A,
candidate B, original B, each with one warmup and three samples per cell.
The pooled column combines the six measured samples per version descriptively;
it is not a confidence interval. Exact ranges and CPU observations are in the
[receipt](evidence/small-file-20261104/results.json), `rule_allocation_experiment`.

| Whole workload, median ms | Original A | Candidate A | Candidate B | Original B | Pooled original → candidate |
| --- | ---: | ---: | ---: | ---: | ---: |
| File200, including acquisition/flush/close | 20.160 | 19.383 | 19.587 | 20.018 | 20.091 → 19.485 |
| Prepared direct file200, identical bytes/flush policy | 9.731 | 9.717 | 9.399 | 9.800 | 9.738 → 9.562 |
| Ordinary quote200 | 4.942 | 4.048 | 4.111 | 4.735 | 4.864 → 4.083 |
| Ordinary frame200 | 7.412 | 6.758 | 6.681 | 7.422 | 7.417 → 6.745 |
| Ordinary full console200 | 11.513 | 10.550 | 10.526 | 11.619 | 11.566 → 10.538 |
| Heavy 1 KiB framing100 | 7.085 | 6.841 | 6.994 | 7.515 | 7.408 → 6.916 |
| Heavy 1 KiB full console100 | 18.684 | 17.132 | 16.863 | 18.434 | 18.536 → 17.030 |

File200 CPU medians are 14.399 → 13.471 ms (6.4% lower). Wall ranges are
19.884–24.931 ms original and 19.266–22.368 ms candidate. Heavy framing stays
below its 10 ms screen and heavy console below 20 ms; no heavy-path regression
is observed in this bounded host/mode sample. Independent component medians
are not additive parts of file200. No platform-wide or tail-latency claim follows.

### Preparation/disposal and allocation accounting

A separate original/candidate/original sequence measures the same coarse scopes
as the retained translation study. These are **diagnostic** instrumented medians,
not the uninstrumented acceptance comparison above. Preparation excludes the
nested lease scope and includes all rule validation and replacement snapshots.
Each phase column is independently summarized and must not be summed into a
claimed same-sample total.

| Scope, file200, ms | Original A | Candidate | Original B |
| --- | ---: | ---: | ---: |
| Rule preparation excluding lease setup | 4.622 | 3.112 | 4.431 |
| Lease/rooted backing setup | 0.307 | 0.335 | 0.287 |
| Rule disposal | 0.777 | 0.102 | 0.648 |
| Complete translation | 6.190 | 4.102 | 5.857 |

For independent quote200, preparation is 3.330 / 3.139 / 3.384 ms and disposal
0.331 / 0.098 / 0.390 ms. Context and instrumentation change the observed phase
costs; the instrumented original file200 reaches 21.491–22.397 ms versus roughly
20 ms uninstrumented. Consequently the much larger instrumented file200 phase
delta is **not** an estimate of the delivered saving. Exact allocator latency
within preparation remains unresolved.

Counts run separately with clocks disabled and allocation sites attributed to
the innermost scope. Short replacement snapshots still allocate no ordinary C++
storage. All 16,200 rule validations remain; ordinary file200 still publishes
200 aliases and no replacement assembly/transformed-string result.

| File200 allocation traffic | Original | Candidate |
| --- | ---: | ---: |
| Rule-map ordinary allocations | 16,200 | 200 |
| Rule-map requested bytes | 1,166,400 | 1,641,600 |
| All ordinary C++ allocations | 41,435 | 25,435 |
| Candidate blocks allocated / released | — | 200 / 200 |

Heavy100 map allocations similarly fall 8,100 → 100, requested bytes
583,200 → 820,800; all 100 blocks are released in every counting sample.
Allocation counts establish the eliminated churn, not time spent in malloc or
whole-operation peak live native memory.

### Concrete candidate and safety boundaries

The [rejected patch](evidence/small-file-20261104/rule-allocation-candidate.patch)
is retained for review. It changes only the ordered map's native allocator in
`text_case.cpp`: `std::pmr::map<i32, std::string>` backed by a private resource.
It adds no persistent cache, shared Runtime contract, API or formatter change.
Native replacement strings retain their existing ordinary allocator.

Growth is lazy at the existing insertion point: fixed **8,192-byte** payload
blocks, **8,208 bytes** including the linked header on this host. There is no
reserve from an unchecked rule count, geometric growth, default-upstream fallback
or cross-call reuse. A 72-byte node fits 113 times per block, so the 81-rule
logger needs one block. Used storage is linear in attempted nodes with at most
one block of tail slack; arbitrary rule counts grow through the same 256-rule
polls, with at most three new blocks per 256 iterations on this build. Invalid
scalars and duplicates still stop in their original order. Single node requests
above 8 KiB or extended alignment are rejected; neither occurs for this host's
map node. Portability of this rejected private allocator is not qualified.

The 16,384-byte replacement/input snapshot quota, rule quota, checked budgets,
all-root/schema validation, frozen backing leases, mutex boundaries and owned
UTF-8 snapshots are unchanged. The map clears its native strings/nodes and then
releases all blocks inside the existing `Runtime::Native` destructor boundary,
including unwinding. Storage outlives the map; the later empty member destructors
have no retained blocks. Ordinary `new Block` routes allocation failures through
the existing `std::bad_alloc` / RuntimeFailure channel; alias and transformed
output publication remain atomic and unchanged.

### Correctness, failed setup and evidence

The candidate passes **31,210 ownership/root checks** across normal/O2 JIT,
including forced GC, aliased input/output, long replacements, failure recovery
and resource restoration. The logger passes **1,688 native checks**, covering
its existing byte/budget/sink/error/lifecycle oracles. A new private 400-rule ABI
test crosses four blocks and sweeps **18 injected failures**: first allocation,
later growth and final publication; acquired block counts 0/1/2/3/4 all occur,
and every acquired block is released. It also checks no-escape/empty aliases,
late duplicate/invalid scalar rejection, budget rejection and a requesting
concurrent collector. That native allocator test also passes ASan/UBSan with
leak detection and the hard 1 MiB stack.

One broader strict-suite setup **fails and is preserved**, exit 1: the older
`build/llvm23-strict` frontend does not recognize `_translate_scalars` while
analyzing current stdlib source. This is a source-analysis incompatibility, not
a signal or sanitizer finding; it is not counted as a strict ownership pass.
Incident `incident-c9av32v5`, receipt `run-lz362woa`, retains the exact diagnostics.
No repair/retry or complete current Debug/strict/AOT/platform qualification is
claimed. The scoped native allocator sanitizer pass does not imply that matrix.

All **18 benchmark processes / 270 observations** (including component warmups)
pass complete file/console byte and write-count oracles, independent quote/frame
length/checksum checks, and post-GC payload/handle restoration. Of **55 total
compile/link/check/measurement commands, 54 exit 0**; the one strict setup exits 1
as recorded above. Earlier failed runs and all historical evidence are retained.

Private sources, binaries, exact build/link recipes, environment, drivers,
stdout/stderr, hashes and receipts are retained at
`/home/shmellyorc/.local/state/void-log-performance/rule-allocation-x81r3_cp/`.
Drivers are `prepare.py`, `check.py`, `strict.py`, `measure.py`, `summarize.py`
and `record.py`. The receipt includes driver text and executable/source hashes;
`manifest.json` indexes the retained artifacts. Replay needs the preserved
candidate source/patch in an isolated checkout, because the working production
file is now restored. Accepted build artifacts were never overwritten.

Production `src/runtime/text_case.cpp` exactly matches HEAD again. Logger,
Runtime headers and root README remain unchanged. Only investigation documents,
receipts and the rejected review patch remain uncommitted. No commit, push,
hosted CI, installation/HTTP requalification or second optimization. Existing
full-input reserve/unpolled publication pause limitations remain; full native
peak live memory and tail latency remain UNKNOWN. **Stop for Golden Wrench review.**

## Translation follow-up — Golden Wrench review pending

**LOG-PERF OPEN; production unchanged at `11e95f4a`.** The byte-array bulk-copy
proposal is on hold and was not implemented. The five investigation files remain
uncommitted. This section supersedes earlier proposed next actions while retaining
their evidence below. [Commands, samples and hashes](evidence/small-file-20261104/results.json)
are under `translation_followup`.

### Finding: rule preparation, not missing no-escape publication

The actual file200 workload already reaches the existing no-escape fast path:
**200 translations validate 16,200 rules, return 200 aliases, and perform zero
replacement assembly or transformed-string publications.** Empty logger names
return earlier in `_quote`; the nonempty 64-byte messages still enter translation.
The final frame continues to construct its record outside this operation.

The repeated cost is preparing and disposing of the **81-rule map** for each
message. In one original file200 sample, the end-to-end workload is 21.888101 ms
and translation is 5.943677 ms, broken down below. These are exclusive scopes
from that one invocation; nested timers are subtracted, not independent medians.

| Translation scope, 200 calls | Elapsed ms |
| --- | ---: |
| Root authority and element schemas | 0.197839 |
| Rooted backing/lease setup | 0.309088 |
| Rule validation, replacement snapshots and map insertion | 4.410826 |
| Owned input snapshot | 0.042039 |
| Input scalar lookup and exact-size pass | 0.120938 |
| Replacement assembly | 0.000000 |
| Checked alias publication | 0.038090 |
| Transformed-string publication | 0.000000 |
| Rule disposal | 0.748147 |
| Other translation scope work | 0.076710 |
| **Same-sample translation total** | **5.943677** |

Rule-loop work plus disposal occupies 5.159 ms of that sample. The loop contains
validation, rooted replacement copying, map lookup/insertion and allocation;
this is **not** 5.159 ms of allocator time. Across the three original-workload
samples, translation spans 5.507–6.027 ms. Independent short-quote translation
spans 4.463–4.806 ms. The earlier 4.934 ms observation is not a fixed budget across
contexts or host conditions.

### Allocation, validation and lookup evidence

A separate counting executable disables clocks and attributes native allocations
to the innermost operation. For 200 ordinary translations:

| Source of ordinary C++ allocations inside translation | Count | Requested bytes |
| --- | ---: | ---: |
| Ordered-map rule insertion | **16,200** | **1,166,400** |
| Root/schema setup | 600 | 20,600 |
| Backing/lease setup | 200 | 7,800 |
| Owned 64-byte input snapshots | 200 | 13,000 |
| Short replacement snapshots | **0** | **0** |
| Assembly / transformed-string publication | **0** | **0** |

There is one 72-byte ordinary allocation per rule insertion on this build.
The replacement strings fit this implementation's short-string storage; their
16,200 snapshots still perform checked root/heap accesses and copying despite
allocating no ordinary C++ storage. The original complete file200 workload has
41,435 ordinary allocations, so rule nodes account for about **39.1%**.
Requested bytes are allocation traffic, not peak live memory or allocator latency.

[Runtime::translate_scalars](../../src/runtime/text_case.cpp#L144) repeats exact
root/schema/lease checks, then reads each key and replacement under the Runtime
mutex, checks the scalar, snapshots the registered replacement root, and inserts
it into the map. Duplicate detection remains in insertion order. After all rules,
it snapshots the input and [translation_spans](../../src/runtime/text_case.cpp#L123)
looks up each original scalar. The ordinary workload's lookup/size pass takes
only 0.113–0.121 ms per 200 calls in the original file samples. Optimizing this
scan or final string construction would miss the demonstrated dominant region.

A 64-byte escaped positive control takes the other branch: **200 assemblies and
200 string publications**, producing 27,200 translated bytes. Its quote-stage
assembly is 0.199–0.208 ms and checked string publication 0.131–0.154 ms. Those
costs are absent from the ordinary no-escape translation. All rule validation
still occurs on both branches, as required by the general text operation.

The exact uncontaminated split within the rule loop between native allocation,
root validation, mutex acquisition, copying and tree insertion remains **UNKNOWN**.
A sampled diagnostic observes one rotating rule per call. Its short inner timers
and changed code layout perturb framing, so its durations are retained but **not
extrapolated** into whole-loop shares. Allocation counts establish the churn;
they do not by themselves establish its time cost.

### Contemporaneous controls and perturbation

Same retained Release/LLVM 23.1.4 build and host, JIT O2, GC=256, hard/soft 1 MiB
stacks. No compilation overlapped measurements. The existing host, framing suffix,
Timer and allocation probe were copied privately; only text_case.cpp receives
phase instrumentation. No Runtime root/lock hot-path timers are enabled.
Each cell has one warmup and three measured samples.

| Whole component median ms | Control A | Coarse phases | Control B | Control C |
| --- | ---: | ---: | ---: | ---: |
| Ordinary quote200 | 4.843 | 5.175 | 4.844 | 4.782 |
| Ordinary frame200 | 7.458 | 7.300 | 7.430 | 7.144 |
| Escaped quote200 | 5.206 | 5.115 | 4.877 | 4.998 |
| Escaped frame200 | 8.022 | 8.131 | 8.252 | 7.505 |

The sampled pass and counting run occur between Controls B and C; their timings
are excluded from acceptance comparisons. In a subsequent same-executable check,
coarse clocks alternate on/off for six consecutive samples per component. Median
on/off shifts are **+2.72%, +0.86%, +1.77%, +1.10%** respectively. This bounds
observed incremental timer disturbance in that check, not all code-layout effects
or desktop variability. No subtraction correction or confidence interval is claimed.
The dominant rule-setup finding is much larger than this observed disturbance.

### Smallest justified candidate — source review only

Investigate a **per-invocation native allocator for the existing ordered rule map**
in `Rules` / `RuleSnapshot` in [text_case.cpp](../../src/runtime/text_case.cpp#L111),
such as scoped PMR arena storage. Consolidate rule-node allocations within one
operation; retain the map's ordering, insertion/duplicate-detection sequence and
lookup behavior. This targets 81 node allocations per ordinary message, rather
than attempting another output-builder repair.

This is a candidate specification, **not an implemented or measured optimization**.
A concrete patch must retain all rule checks, including empty input, and use the
existing roots, frozen backing leases, byte/rule work quotas, safepoints and
NativeSafe disposal. Node storage must outlive the map and be released within
its existing native cleanup boundary, including unwinding. Allocate lazily at the
existing insertion phase, preserve the Runtime allocation-failure channel and
output atomicity, and review fallback/growth and unused-capacity behavior for
arbitrary rule counts. Block growth must remain bounded within the existing
work-quota/NativeSafe guarantees; otherwise stop for contract review. No cross-call
cache, precompiled managed-rule lifecycle,
new public API, bypassed validation or managed pointer shortcut is proposed.
Logger name-first ordering, exact escapes, separate budgets and every sink/error
path stay unchanged. The checked string constructor and alias publication stay.

**Realistic scope:** reduce native rule-node allocation/deallocation traffic.
The candidate cannot remove required validation, replacement copying, input
lookup or the entire measured rule-loop duration. Even disposal retains tree
traversal and string cleanup. Its latency gain is unmeasured and may be modest;
there is no defensible millisecond saving or <10 ms prediction yet. Acceptance
would require a separately reviewed candidate, targeted text alias/GC/failure
checks and logging byte oracles, then contemporaneous component **and file200**
measurements with allocation counting separate. Allocation reduction alone is
not success. A safe shortcut skipping all-rule validation was not established.

### Status and preservation

Small-file remains **FAIL**: the three current production-control medians are
22.418, 23.755 and 21.238 ms against the unchanged <10 ms gate. Existing heavy
framing/console results are retained; neither was rerun. Thirteen benchmark
processes and all 28 preparation/measurement commands exit 0, with 142 recorded
rows including component warmups. Original full-byte/write-count checks,
independent quote/frame length checks, post-GC payload/handle equality, and
allocation-site accounting pass. There are no new failures; all prior failures
and reports remain intact.

Private sources, exact driver text/commands, environment, raw output, hashes and
receipts are retained at
`/home/shmellyorc/.local/state/void-log-performance/translation-nfyczkon/`.
Drivers are `prepare.py`, `measure.py` and `overhead.py`; the receipt records the
executed `/tmp/void-translation-*.py` paths. Replay uses a fresh private directory
and the recorded retained build. No accepted binaries were overwritten.

The full-input snapshot reserve and unpolled final immutable-string publication
remain pause-time limitations. **Tail latency, allocator latency and peak native
live memory remain UNKNOWN.** No production/source optimization, commit, push,
hosted CI, installation or HTTP/HTTPS requalification occurred. **Stop for Golden
Wrench review before implementation.**

## Retained CPU attribution

## CPU follow-up — Golden Wrench review pending

**LOG-PERF OPEN; small-file <10 ms remains FAIL. Measurement only.**
Production remains at `11e95f4a6a7d60071bd754008662e88e022fbe16`; remote SHA was
rechecked. The five investigation documentation/evidence files remain uncommitted.
The accepted heavy-framing implementation, original baselines and failures are
preserved. The earlier investigation below is retained as a dated record.

### Attribution

Short-record framing is the largest measured CPU component. Scalar translation
alone takes approximately **4.9–5.0 ms for 200 records** in the reduced-timer
write-only experiment. Its checked rule snapshots and root operations are
included. GC contributes about **0.77–0.79 ms**, and the native write routine
about **0.49–0.52 ms**. Admission and file lifecycle coordination are smaller.
This does not establish that allocator time, GC, mutex contention or the kernel
write syscall is the principal cause of the CPU hold.

The same Release/LLVM 23.1.4 host/build, GC=256 and 1 MiB stacks were used. Message
size is 64 ASCII bytes; each exact record is 75 bytes. Timestamps and console
output are disabled for the file path. Compilation, open/setup and cleanup are
outside component timing; original end-to-end commands retain acquisition,
flush/close and their full-output oracles. All final work ran sequentially.

The existing component suffix/host was extended only in a private copy. Nine
bounded components cover admission, quote, frame, guard entry/release, preparation,
prepared-record delivery, full writes, direct writes and configuration validation.
Three nearby uninstrumented control blocks bracket the detailed timers and a
separate allocation-counting run. Each cell has one warmup and three samples.

| Component, 200 iterations | Control A median ms | Control B | Control C |
| --- | ---: | ---: | ---: |
| `_admit` (including severity/filter/state checks) | 0.456 | 0.515 | 0.456 |
| `_quote` short message | 4.762 | 4.768 | 4.835 |
| Complete `_frame` | 7.187 | 7.762 | 7.776 |
| `_reserve` / `_enter` / guard release | 1.072 | 1.075 | 1.068 |
| `_prepare` (timestamps disabled) | 6.991 | 7.797 | 7.318 |
| Guard plus `_deliver` of prepared bytes | 6.100 | 6.179 | 5.894 |
| Full logger write loop, no flush/close | 13.835 | 14.379 | 13.981 |
| Direct `io.file.write_bytes` of prepared bytes | 3.547 | 3.552 | 3.451 |
| `_validate` repeated independently | 1.008 | 1.057 | 1.007 |

These are **independent experiments, not additive slices**. In particular, the
frame/preparation medians differ within host/context variation. `_validate` runs
at open/configure, not once per emitted record; multiplying its component cost
into the actual write path would be wrong. Its repeated experiment is diagnostic.
The `_admit` component uses the existing prepared console logger, whose admission
checks are the same; its sink is not exercised.

Detailed timers instrument allocations/construction, GC, root checks, frame
registration, selected lock acquisition and dispatch. They inflate the full
write-loop median from roughly 14 ms to **30.208 ms**, so those timings are not
an acceptance breakdown or an estimate of uncontaminated lock cost.
A bounded second pass retained only translation, GC, byte snapshot, filesystem
bridge/lifecycle/lookup and native-operation timers. The nearby control/reduced-
timer/control write-loop medians were **15.112 / 14.002 / 14.292 ms**; frame
medians were **7.427 / 7.400 / 7.612 ms**. The overlap supports the attribution,
without proving zero instrumentation overhead or a speedup.

The following is an actual disjoint breakdown of **one reduced-timer full-write
sample** (sample 1, stage 17), not a sum of independent medians. Nested timers
are subtracted from parent scopes. No flush/close occurs inside this timed sample.

| Scope | Exclusive elapsed ms |
| --- | ---: |
| Scalar translation, including its uninstrumented root/lease/rule work | 4.933785 |
| Garbage collection | 0.793624 |
| Owned byte snapshot before native write | 0.846268 |
| Filesystem reply construction, excluding nested GC | 0.888366 |
| Native write routine | 0.515969 |
| Other prepare/dispatch/lifecycle/lookup scopes | 0.378058 |
| Remaining generated/Runtime work, timer overhead and scheduling | 5.670320 |
| **Same-sample total** | **14.026390** |

Process CPU time for this sample is 13.835 ms. Scalar translation is 35.2% of
elapsed time; the large remainder is explicitly unresolved. The other two
write-only samples total 13.909 and 14.002 ms; translation takes 4.891 and
4.997 ms. Scope timings include their instrumentation cost. `inclusive_ns` in
the receipt describes overlapping parent scopes and must not be summed.

The corresponding original end-to-end sample totals **19.873088 ms**, CPU
14.426 ms, including **5.481309 ms** native flush and **0.007520 ms** native close
scopes. Its translation takes 5.026664 ms. Across those three end-to-end samples,
flush ranges 5.481–8.615 ms and native writes 0.508–0.528 ms. There is no changed
flush policy: the call remains `fsync` and logger close still flushes then closes.
The same-sample breakdown and all ranges are in the `cpu_followup` section of the
[receipt](evidence/small-file-20261104/results.json).

### Allocation, roots and synchronization

Allocation instrumentation ran separately with elapsed timers disabled. Counts
are ordinary C++ `operator new` traffic, not time inside malloc, live memory or
all process allocations. Source-managed constructions are a different counter
from Runtime construction calls (which also include intrinsic objects).

| Measured path | Ordinary allocations | Requested bytes | Collections |
| --- | ---: | ---: | ---: |
| Original file200 | 41,435 | 2,416,942 | 17 |
| Original prepared direct file200 | 9,704 | 490,220 | 5 |
| Short quote200 | 17,211 | 1,209,480 | 0 |
| Full frame200 | 26,121 | 1,645,992 | 7 |
| Guard entry/release200 | 4,114 | 126,352 | 3 |
| Prepared delivery200 | 14,841 | 637,704 | 10 |
| Full write loop200 | 40,768 | 2,303,464 | 17 |

Original file200 performs **16,600 allocations / 1,185,600 requested bytes** in
the innermost translation scope, excluding separately instrumented child scopes:
about 40% of its ordinary allocation count. The same count occurs in quote200.
The source validates and snapshots **81 rules** for every message, constructs
an owned `std::map`, then traverses the original input. This repeated work is
confirmed; attributing all of its elapsed cost to map allocation rather than
validation, root checks, copying, lookup or disposal remains a hypothesis.
No persistent cache or formatter replacement is proposed.

Detailed file200 counters record 32,524 `require_output` calls, 19,043
`reference_source` calls, 35,209 Runtime polls, 4,311 Runtime object constructions,
and 128,798 acquisitions at instrumented lock sites. They demonstrate frequent
checked operations. The selected acquisition timer includes clock/atomic overhead;
its 3.607 ms in detailed sample 1 is **not** an uncontaminated mutex cost or
proof of contention. No wait distribution or concurrent contention test was run.
Frame registration/root validation and actual allocation time are not independently
resolved by the reduced-timer pass. Safepoints and locking remain required.

In the reduced write-only sample, 800 file claim/release/begin/end scopes total
0.041100 ms and 1,400 native-ledger lookup scopes total 0.048710 ms. Those small
figures include their locks but exclude other Runtime work. Garbage collection
is measurable, yet is much smaller than framing. The native bridge also builds
a generic checked reply with empty string/array fields for each successful write;
its measured 0.888 ms scope includes that work. These results support focused
review, not removing guards, checks or synchronization.

### Smallest proposed repair — not implemented

The strongest evidenced CPU region is short framing, particularly the existing
scalar translation operation. The accepted formatter remains unchanged under
this investigation's boundary. The smallest separate repair candidate is confined
to [Runtime::byte_array_bytes](../../src/runtime/array.cpp#L335): replace its
per-byte checked copy with a **bounded bulk copy of the already rooted, leased
u8 backing**, retaining equivalent checks once per bounded chunk.

The current function invokes [buffer_byte](../../src/runtime/buffer.cpp#L92)
for every byte. For this workload that is 15,000 repeated lock/layout/heap-state
checks across 200 snapshots, after acquiring an exact complete-backing lease.
The reduced-timer snapshot cost is **0.846–0.862 ms** in the full write loop,
and roughly 0.85–0.95 ms in the original/direct samples. This is a demonstrated
secondary cost. Even eliminating the whole observed scope would not close the
<10 ms gate; a real repair must retain allocation and validation work, so savings
would be smaller. No speedup is claimed without a reviewed candidate measurement.

Review boundary for that proposal:

- Keep the existing native-owned `std::string` result and complete-return
  publication contract. No new public API, persistent cache or managed-string
  lifecycle. This shared Runtime helper also serves other byte consumers;
  its existing snapshot contract needs review, not a logger-only assumption.
- Retain the exact schema/layout, complete backing, extent/overflow checks,
  selected-backing roots and `BufferReadLease`. Alias growth must preserve the
  selected snapshot. Do not expose or retain a managed pointer across a poll.
- Copy only verified contiguous u8 spans under the existing mutex, with the
  existing byte work quota (16 KiB) and safepoints between chunks; reacquire and
  revalidate after each safepoint. No unchecked pointer or lifetime shortcut.
- Preserve allocation-failure unwinding, lease release, file progress/error
  reporting, record order, exact bytes, flush/close durability and all logger
  guards. Require targeted snapshot alias/GC/failure tests and the retained
  byte/count oracles before timing. Keep allocation counters separate.

That is a proposal for Golden Wrench source review, not implementation approval.
Further changes to the dominant translation path require separate review and
are not bundled with this small candidate.

### Status, reproduction and limits

Small file200 is **FAIL** in every final control sample. Control-block medians
range 19.771–24.980 ms; the reduced-timer end-to-end median is 19.873 ms.
The relative direct comparator varies with synchronization and host conditions;
no fixed storage floor or changed 10 ms requirement is inferred. Heavy framing
and console retain their prior accepted measurements; neither was rerun.

All **16 final benchmark processes** exited 0, yielding **276 timing/count rows**
including component warmups, plus 16 unrecorded original-workload warmups.
Full record-byte/write-count oracles, independent checksums and post-GC payload/
handle checks pass. Allocation-site totals reconcile to ordinary allocation
counts/bytes. These are bounded measurement checks, not correctness requalification.

Three harness failures are preserved: an ambiguous source marker assertion,
a missing include path in a copied C++ source, and a private VOID variable
shadowing an import. None is a new production correctness failure. The latter
stopped before component execution; six earlier original-control samples remain
separate from the final comparison. Compiler diagnostics and the recorder's
incident directories are retained; none of these failures is a signal/crash.

Private evidence is
`/home/shmellyorc/.local/state/void-log-performance/small-cpu-mm0rbzs_/`, with
reduced-timer files in `coarse/`. The initial preparation failures remain in
`small-cpu-pd9jjxnp/` and `small-cpu-hnxp46ql/` beside it. The receipt records all
65 child-command statuses (63 exit 0; two retained compiler diagnostics exit 1),
the Python assertion, exact argv/cwd/environment, samples, source/binary hashes,
load observations and complete driver text. The executed driver paths were:

```sh
python3 /tmp/void-smallcpu-prepare.py
python3 /tmp/void-smallcpu-measure.py
python3 /tmp/void-smallcpu-coarse.py
python3 /tmp/void-smallcpu-coarse-measure.py
```

Use a fresh private directory for replay and the recorded retained Release build.
`prepare-corrected.py` preserves the final source fixes; original failed sources
and drivers remain alongside it. No accepted artifact was overwritten. Production
sources still match the reviewed candidate hashes. New instrumentation and probe
sources remain private; only the existing five investigation files are updated.

The **full-input snapshot reserve and unpolled final immutable-string publication**
remain pause-time limitations. Tail latency, allocator latency, peak native-live
memory and other-host/platform performance remain **UNKNOWN**. This active-host,
three-sample-per-cell study has no confidence-interval or tail-acceptance claim.
Installation and HTTP/HTTPS acceptance remain closed. **Stop for Golden Wrench
review before any implementation.**

## Retained first small-file investigation

**LOG-PERF remains OPEN / release HOLD. Stop for Golden Wrench review before
another production change.** The approved bounded framing implementation,
targeted tests and evidence were committed and pushed as
`11e95f4a6a7d60071bd754008662e88e022fbe16`, with `[skip ci]`.
`git ls-remote origin refs/heads/main` independently matched that SHA.
This investigation is separate, uncommitted documentation/evidence. No hosted
CI was dispatched, and no further production code was changed.

The remaining failure has both CPU-side per-record cost and file synchronization
cost. The logger's 200-write phase alone takes about 14–15 ms, before flushing.
Dirty-file flush takes about 6–9 ms on this host. Native write routines total
about 0.5 ms and native close is small. These observations do not justify removing
flushes, changing durability/error semantics or rewriting the successful formatter.

[Machine-readable samples, commands and hashes](evidence/small-file-20261104/results.json)
· [Approved candidate](BOUNDED_FRAMING_RESULT.md)
· [Unchanged initial baseline](PERFORMANCE_BASELINE_LLVM23.md)

## Conditions and method

Same developer host: AMD Ryzen 7 1700X, eight cores / 16 logical CPUs, PikaOS 4,
Linux 7.2.11-pikaos, glibc 2.44. The private measurement directory is on XFS,
`/dev/sda3`, on a Samsung SSD 860 EVO 1TB. Storage identity was inspected after
measurement; device health, queueing, firmware and write-cache policy were not
profiled. The desktop remained active. No CPU isolation, fixed frequency,
cache eviction or randomized run ordering was imposed.

Release C++ `-O3 -DNDEBUG -std=c++20 -fno-rtti`, pinned LLVM/Clang/LLD 23.1.4,
JIT O2, GC budget 256, hard/soft 1 MiB execution stacks. The existing reviewed
candidate libraries were reused; production sources match the recorded tested
hashes. All retained component-host hashes were verified, including that its
copied log module is exactly current log.void plus the existing test suffix.
Compilation and startup precede timing. Each cell has one warmup and three
measured samples. There are five benchmark processes, with 114 recorded timing
rows including staged/component warmups, plus six original-workload warmups.

The original [host](../../tests/log/host.cpp#L319) and
[workloads](../../tests/log/benchmark.void#L66) were copied privately and restricted
to commands 11 (file200), 18 (prepared direct file200), and 20 (20 explicit
flushes plus the close flush). Their complete byte and 200-write oracles remain.
The ordinary host retains its existing source-managed construction observer;
native allocation counting is not enabled. This is not a new allocation study.

A private staged source holds an opened logger or direct file in a rooted global
and separately invokes open, 200 writes, flush, and close. A third sequence
omits explicit flush and measures dirty logger close. An untimed full-byte oracle
runs after the write phase. This changes invocation boundaries, object lifetime
and the interval before flush, so it localizes phases without replacing the
original end-to-end acceptance workload.

The control executable links the original Runtime. The profiling executable
substitutes only a private filesystem.cpp copy containing the existing
[exclusive Timer](../../tests/log/performance_metrics.hpp#L31) scopes around
native write, flush, close and the outer dispatcher. Nested durations are removed
from the dispatcher. Native write includes checks, test hooks, the write syscall
and position bookkeeping; native close includes harmless idempotent invocations.
These are elapsed function-scope timings, not pure syscall or device latency.

The existing [component stages](../../tests/log/performance_framing.void#L43)
were reused for short ASCII quote and full framing only (shape 0, stages 6/7).
No heavy workload, allocation-counting run, AOT matrix or broad suite was rerun.

## Results

Original workload wall-clock milliseconds, median [minimum, maximum]:

| Workload | Control, original Runtime | Native-timing executable |
| --- | ---: | ---: |
| Logger file200, including open/close | 26.806 [22.686, 26.822] | 19.961 [19.925, 21.107] |
| Prepared direct file200, including flush/close | 12.952 [10.351, 13.731] | 9.783 [9.552, 10.052] |
| Logger file200 with 20 explicit flushes | 164.285 [161.440, 168.627] | 152.985 [151.560, 158.289] |

Control and profiling runs were sequential. Their substantial difference cannot
measure instrumentation overhead or demonstrate a performance improvement.
The approved candidate's retained 21.390 ms and direct control's 12.195 ms are
preserved; this repeat is additional evidence of the failure and host variability.
Current control file200 delivers about 7,461 records/s, or 134.03 microseconds
per record amortized over acquisition/close. That is not a latency percentile.

Separately invoked phases, original Runtime, wall milliseconds:

| Phase | Logger median [range] | Prepared direct I/O median [range] |
| --- | ---: | ---: |
| Open | 0.336 [0.307, 0.492] | 0.132 [0.110, 0.150] |
| 200 writes, no flush/close | 15.138 [13.768, 15.291] | 3.449 [3.349, 4.023] |
| Explicit dirty flush | 6.348 [6.067, 9.237] | 6.172 [5.858, 8.944] |
| Close after explicit flush | 0.253 [0.246, 2.817] | 0.109 [0.063, 0.127] |

The logger's dirty-close sequence measures 13.988 ms for 200 writes and
6.052 ms [5.921, 9.369] for close, which includes its required flush.
The logger explicit-flush sequence records one flush at flush and another at
close, with one actual close. Direct close records no flush. The 2.817 ms logger
close outlier remains in the samples; this control has no internal timer to
attribute that individual outlier.

Quote200 measures **4.767 ms** [4.744, 4.892]; full frame200 measures
**7.505 ms** [7.498, 7.739]. These are independent component measurements, not
exclusive slices to add to or subtract from the staged/end-to-end medians.

For attribution within the *same* profiled original file200 samples:

| Sample | Total | Native writes (200) | Native flush (1) | Native close scopes (2) | Other dispatcher | Outside timed native scopes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 19.960714 | 0.517568 | 5.751220 | 0.006829 | 0.170380 | 13.514717 |
| 1 | 21.106668 | 0.509128 | 5.906008 | 0.006100 | 0.063810 | 14.621622 |
| 2 | 19.924624 | 0.501006 | 5.597391 | 0.008340 | 0.064370 | 13.753517 |

All values are elapsed milliseconds. The remainder includes generated code,
Runtime/bridge work outside these scopes, instrumentation overhead and scheduling;
it is not a pure logger CPU measurement. Process CPU time was 14.162–15.131 ms
for these samples. Native flush accounts for 28.0–28.8% of their elapsed time.
Prepared direct file200 has 5.837–6.263 ms native flush, 0.366–0.405 ms native
writes and 3.324–3.495 ms outside timed native scopes. In the 21-flush workload,
native flush accounts for 126.430–131.851 ms, about 83% of total elapsed time.

## Demonstrated costs and unresolved causes

- Dirty flush is expensive here. The implementation calls POSIX `fsync` in
  [file_operation](../../src/runtime/filesystem.cpp#L541). The logger's
  [_retire](../../stdlib/log.void#L579) deliberately flushes before closing.
  Instrumented dirty-close time is overwhelmingly inside flush; native close
  itself takes only about 0.008–0.010 ms. Calling this a slow close syscall
  would be incorrect. The storage/filesystem cause of fsync latency is UNKNOWN.
- Per-record execution also exceeds the whole <10 ms gate without any flush.
  Native write routines are a small part of that phase. Full short-record
  framing is a measured substantial cost, but these experiments do not isolate
  individual rule snapshots, validations, allocations, root checks or safepoints.
- The prepared direct write phase takes 3.449 ms while its profiled native writes
  take about 0.375 ms. This locates additional cost outside the native write
  routine, without proving which checked I/O/Runtime operation causes it.
- The profiled direct end-to-end median is below 10 ms while the control median
  is above it. Therefore 12.195 ms is an observed comparator, not a universal
  storage floor or proof the unchanged threshold is impossible. Neither timing
  variability nor allocation reduction establishes acceptance.

## Acceptance, safety and next action

| Criterion | Status | Evidence |
| --- | --- | --- |
| Small file200 <10 ms | **FAIL** | Retained candidate 21.390 ms; repeat control 26.806 ms; every profiled sample also fails |
| Heavy framing ≤10 ms | **PASS, retained** | Approved candidate 8.603 ms; not rerun here |
| Heavy console ≤20 ms | **PASS, retained** | Approved candidate 18.501 ms; not rerun here |
| Bounded output and resource checks | **PASS** | All five benchmark processes exit 0; complete bytes/write counts, framing checksums and post-GC payload/handles verified |
| Tail latency / pause-time acceptance | **UNKNOWN** | No percentile or worst-pause measurements |
| Native peak-live memory / broader platforms and scaling | **UNKNOWN** | Not measured by this investigation |

The **full-input snapshot reserve and unpolled final immutable-string publication
remain documented pause-time limitations**. Post-GC equality is not a peak-live
allocation or tail-latency result. Existing baseline and historical LLVM19 crash
evidence remain unchanged; no new correctness failure appeared in these checks.

The smallest justified next action is Golden Wrench review of this attribution,
then a bounded measurement-only split of the remaining per-record CPU path:
[_admit/_reserve/_enter](../../stdlib/log.void#L234),
[write/_prepare/_deliver](../../stdlib/log.void#L747), and the checked
[io.file.write_bytes](../../stdlib/io.void#L577) bridge. Use the existing host and
independent counters/timers, preserve error/order/ownership behavior, and compare
against the same original workload. Those functions are candidate measurement
sites, not demonstrated defective implementations. No production optimization,
formatter rewrite, buffering, flush suppression or acceptance relaxation is
proposed on the present evidence.

## Reproduction and preservation

Private evidence:
`/home/shmellyorc/.local/state/void-log-performance/small-file-xe9c672y/`.
The JSON retains exact compile/link/benchmark argv, working directories, exit
codes, environment overrides, executable/source hashes, raw-output hashes,
all measured samples, and complete preparation/measurement driver text.
Both driver invocations exited 0, as did all nine child commands (four preparation,
five measurement). Their stderr files are empty. No failed benchmark was discarded.

From the repository at the recorded SHA, the executed drivers were:

```sh
python3 /tmp/void-smallfile-prepare.py
python3 /tmp/void-smallfile-measure.py
```

Their exact copies are `prepare.py` and `measure.py` in the private directory.
Preparation creates a new private directory and records it in
`/tmp/void-smallfile-current`; measurement refuses existing output subdirectories.
Replay needs the retained candidate build and component host paths named in the
driver (or explicitly relocated equivalents verified against the hashes).
It does not rebuild or overwrite accepted artifacts. Native instrumentation and
staged sources exist only in that private directory. The report/receipt and
handoff updates remain uncommitted for separate review.

Installation qualification stays accepted and closed at d784c049. No installation,
HTTP/HTTPS, VM, historical crash, full regression or public-release work occurred.
