# Integrated logging and internal performance qualification

> Publication note: this commit keeps selected text evidence. Full local archives,
> binaries, candidate trees and disposable runs remain excluded and preserved;
> see the [artifact scope and original-document hashes](evidence/integrated-performance/LOCAL_ARTIFACTS.md).
> Measurements and conclusions below are unchanged; artifact links are portable.

5 November 2026. Local Linux integration handoff for Golden Wrench/Boss review.

Batch 9 measures a separate frozen application at a later source identity.
See the [G08 compiler and editor baseline](G08_COMPILER_PERFORMANCE.md).

**Recommendation: RETAIN the integrated implementation for Golden Wrench/Boss review.**
All selected tests passed: **85 Release, 85 Debug and 84 strict ASan/UBSan/leak**
on this Linux host. No reproducible integration correctness defect was
found in the reviewed boundaries or this campaign. Performance regressions and
unclassified historical failures remain explicit. This is local qualification,
not independent review or release acceptance. No SIMD implementation or SIMD
benchmark was performed.

## Integrated source and review

The working implementation was already combined at the start of this assignment;
no candidate patch was applied again. HEAD remains
`f3cf240e0b14240d9c7486c8882842bfa7043c63`. All `src`, `include` and `stdlib` files
match the final internal-repair identity, including the new
`src/ir/local_cleanup.inc`. [Starting identity and comparison](../../experiments/integration-qualification/evidence/initial-identity.json)
and [starting tracked patch](../../experiments/integration-qualification/evidence/initial-working-diff.patch)
freeze the actual base. This assignment changes qualification/documentation only.

The source inventory against HEAD is **23 production files and five test/measurement
fixtures**. Overlapping files below are one implementation, not layered duplicate
patches. [Exact inventory](../../experiments/integration-qualification/evidence/source-inventory.json)
contains every path and hash.

| Area | Integrated files and retained behavior |
| --- | --- |
| Compiler cleanup and private operations | `include/void/ir.hpp`, `src/frontend/compiler.cpp`, `src/frontend/capabilities.inc`, `src/ir/module.cpp`, `src/ir/local_cleanup.inc`, `src/ir/reference_capabilities.inc`, `src/backend/llvm/backend.cpp`: eligible synchronous cleanup has a rooted binding and checked activation; unsupported scopes use the existing chain. Private IR shape, effect and lifecycle validation remain. |
| Synchronous I/O bridge | `include/void/filesystem_abi.hpp`, `src/frontend/filesystem.inc`, `src/ir/filesystem.inc`, `src/backend/llvm/filesystem.inc`, `src/runtime/filesystem_abi.cpp`, `stdlib/io.void`: scalar success avoids generic Reply construction; typed failures and RuntimeFailure retain their paths and the same shared Job/resource authority. |
| Runtime/root/metadata | `include/void/runtime.hpp`, `include/void/runtime_abi.hpp`, `src/runtime/abi.cpp`, `src/runtime/coordination.cpp`, `src/runtime/array.cpp`, `src/runtime/buffer.cpp`, `src/runtime/list.cpp`, `src/runtime/dictionary.cpp`: checked tokens, full-search fallback, canonical metadata and owned dense-u8 snapshots. |
| Editor storage | `include/void/tooling.hpp`, `src/frontend/tooling.cpp`: immutable analysis-backed declarations/references with owning views/iterators and owned query results. |
| Qualification fixtures | `tests/advanced/tests.cpp`, `tests/runtime/buffers.cpp`, `tests/runtime/root_lookup_measure.cpp`, `tests/text_encoding/performance.cpp`, `tests/tooling/tests.cpp`. |

Boundary review checked these interactions:

- Frame/token uses validate Managed state, Runtime/thread, linked lifetime identity,
  current Slot/offset and descriptor, private-buffer exclusion and current heap
  membership. Values are read again after safepoints. Frame-address reuse does not
  revive tokens. Newest-first full search remains. There is **no Frame cache**.
- LLVM allocates and aligns Frames using actual `sizeof`/`alignof`; checked-root ABI
  version and Frame size enter the existing native cache identity along with the
  actual compiler/link fingerprints. Frame is 40 bytes versus 32 before the
  internal repair; the discarded cache's 112-byte representation is absent.
- Cleanup activates only after binding publication, clears activation before
  deinit, retains the root through deinit, then clears the binding. Reverse
  unwind, cancellation shielding and primary/secondary failure handling remain.
- Dense byte snapshots hold the existing read lease, poll/revalidate bounded
  ranges, own their native bytes and release the lease before native waiting.
  Compact success does not change native request preparation or acknowledgement.
- Runtime metadata is keyed by exact admitted schema identity, contains immutable
  data only, is bounded by the catalog and survives additive reload. Every
  incoming ABI descriptor is still checked. No managed payload or code pointer
  is cached. Optional indexing allocation failure returns the valid uncached layout.
- Analysis remains alive through views and iterators. Order, duplicates, source
  spans and UTF-16 positions remain; results needing independent lifetimes own
  their records. Refresh/stale rejection and rewrite preflights remain.
- Windows/POSIX native write/sync loops are unchanged. Common bridge changes do
  not introduce a platform-specific Job or alternate ownership path. This is
  source review, not native Windows/macOS execution.

The private experimental join/UTF-8 facility remains dormant. Automatic selection
is disabled; ordinary V1 formatting is retained. Removing its private ABI and
fixtures would expand this qualification's scope; no unused facility was enabled
or new optimization attempted. A fresh IR control finds **zero fused calls**,
18 rooted cleanup operations, one compact write and three malformed private
operations rejected. A fresh syscall-wrapper control retains **400 writes,
30,000 bytes and two real fsync calls** across warmup and measured FILE200.
[IR receipt](../../experiments/integration-qualification/evidence/recheck-logging-ir-retained.stdout),
[durability receipt](../../experiments/integration-qualification/evidence/recheck-durability-proof.stderr).
These supplemental hosts were preserved from the byte-identical accepted source;
the CMake qualification below rebuilds current targets.

## Local correctness qualification

The fixed campaign passed **254 CTest executions**: **85 Release, 85 Debug and
84 strict**, with zero failures or skips. These are bounded relevant selections,
not a claim to run every registered repository test. The strict configuration
does not register the duplicate `text_encoding_stack` wrapper (`CMakeLists.txt`
excludes that wrapper under sanitizers). Its ordinary `text_encoding_text`
execution covers the same text fixture/modes with the inherited hard/soft 1 MiB
limit. This is a configuration difference, not a skipped or failed test.

| Current integrated profile | Common tests | Boundary tests | Stage 7 diagnostic children |
| --- | ---: | ---: | --- |
| Release | [63/63](../../experiments/integration-qualification/evidence/release-junit.xml) | [22/22](../../experiments/integration-qualification/evidence/release-boundary-junit.xml) | 12/12 normal exits |
| Debug | [63/63](../../experiments/integration-qualification/evidence/debug-junit.xml) | [22/22](../../experiments/integration-qualification/evidence/debug-boundary-junit.xml) | 12/12 normal exits |
| Strict | [62/62](../../experiments/integration-qualification/evidence/strict-junit.xml) | [22/22](../../experiments/integration-qualification/evidence/strict-boundary-junit.xml) | 12/12 normal exits |

The separately preserved baseline replay also has 12/12 normal child exits:
**48/48 captured editor processes** across baseline and three current profiles,
empty child stderr, each with actual 1 MiB limits. This does not prove the old
uncaptured failure fixed. The original logging host reports **1,690 checks per
profile**; independently linked normal/O2 logging AOT probes report **828 each**.
The original text ownership host reports **17,834 checks per profile**.
[Machine-readable campaign verification](../../experiments/integration-qualification/evidence/qualification-results.json).

All execution commands use actual soft **and hard
1,048,576-byte stacks**, not a virtual-stack estimate. Builds have their ordinary
build stack. Fixtures labeling a mode “default” still inherit the same hard/soft
1 MiB limit; explicit small-stack variants retain their original checks.
Clang/LLVM/LLD are **23.1.4**, commit
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`.

Release is `-O3 -DNDEBUG`; Debug uses the actual Debug recipes. Strict is
RelWithDebInfo `-O2 -g` plus `-fsanitize=address,undefined`, frame pointers,
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Third-party LLVM and generated
machine code are not fully sanitizer-instrumented. Passing these tests is not a
whole-repository or whole-platform sanitizer proof.

The common selection covers normal/O2 JIT, independently emitted/linked normal/O2
AOT, GC and root/schema invariants, nested/overlapping/expired/address-reused
Frames, foreign access, changed descriptors, private/interior aliases, forced GC,
allocation-failure sweeps and retry. It also covers catalog/reload identity,
malformed metadata, original text/codec bytes and depth, stable sorting and
mutation semantics, JSON grammar/numbers/schema/bounds/work limits, async
cancellation/retention/quiescence, logging bytes/ownership/failures and LSP
completion/hover/references/rename/formatting/refresh/UTF-16/stale results.
The boundary selection adds logging file/console/oracle/cache controls, process
cache identity, synchronous/async I/O, independently linked I/O AOT, native close
and shutdown/cancellation, HTTP reload/fault/timeouts and private IR checks.
Selections, command environments, JUnit XML, stdout/stderr and CTest transcripts
are retained under [new evidence](../../experiments/integration-qualification/evidence).

## Allocation-fault test reconciliation

The original advanced test assumed construction ordinals 5/6 represented
acquisition and the first body allocation. Compiler-managed cleanup removes
synthetic allocations, so those ordinal meanings no longer hold. The already
approved semantic fixture selects the resource payload that references the
captured Tracker, then the first body Tracker payload, during each execution.
It requires exactly one target, an actual raised allocation failure, valid roots
and publication, no residual Frames, and Tracker totals **0 before acquisition
commits / 1 after acquisition**, plus clean collection and retry.

The original HEAD translation unit was independently compiled against this
current ABI, preserving its bytes and the existing probe/ABI objects. It still
exits **1**, `with allocation failure/root publication`.
[Fresh original failure](../../experiments/integration-qualification/evidence/advanced-original-current-runtime.stderr),
[historical original failure](../../experiments/log-integrated/refinement/evidence/advanced-original-run.stderr),
[historical semantic pass](../../experiments/log-integrated/refinement/evidence/advanced-semantic-run.stdout).
All three current `advanced_source` tests pass. Each reports acquisition ordinal
**2** and first-body ordinal **3** in both normal/O2 execution, selected by meaning
rather than new fixed constants. Those results qualify the semantic test; they do
not retroactively turn the original test into a pass. No production validation
was weakened to satisfy fixed allocation counts.

## Editor startup and HTTP regression examination

The historical final-editor **baseline** invocation reported startup EOF; its
legacy harness did not retain the child's exit code, stderr or partial in-memory
timings. Those facts cannot be reconstructed. Its original failure and later
predeclared replay remain in the [internal repair result](INTERNAL_PERFORMANCE_REPAIR_RESULT.md#3-correctness-and-local-qualification).

This qualification predeclared one unchanged three-repetition Stage 7 replay for
the retained baseline and integrated Release build, followed by Debug/strict
replays. Each replay uses the original small/medium/large graphs, 12 server
processes, CLI checks, initialization barriers, valid/invalid/repair edits, stale
queries and 20 queued edits. Sources, actual process limits, every completed
action and child exit/stderr are preserved immediately. These are correctness
diagnostics under concurrent qualification load; their incidental times are
**not** a new matched performance cohort or a startup speedup claim.

The unchanged HTTP absent/idle/paused one-worker fixture was also rerun against
the retained baseline and current Release host: all nine invocations per host
passed, each asserting 256 acknowledged 64 KiB writes, exact 16 MiB output,
collected managed heap and Runtime shutdown. Its original cold initialization
and source semantics remain. This does not establish the cause of timing
variation or erase a slow cohort. [Baseline diagnostic](../../experiments/integration-qualification/evidence/http-baseline-retained/measurements.json),
[current diagnostic](../../experiments/integration-qualification/evidence/http-integrated/measurements.json).

## Retained matched performance and memory

No new performance redesign or SIMD benchmark was run. The already matched,
uninstrumented **LLVM 23.1.4** measurements remain authoritative for their exact
inputs. The internal baseline already includes refined logging; the earlier
V1-to-refinement cohort is separate and percentages must not be added.
[All 1,356 retained observations / 86 cells](../../experiments/internal-performance/evidence/final-whole-workload-raw.json),
[complete summary](../../experiments/internal-performance/evidence/final-whole-workload-summary.csv),
[earlier refined logging](../log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md).

| Whole workload | Matched baseline → retained | Result |
| --- | --- | --- |
| 64 KiB O2 text/codecs | wall 2237.200 → 1860.165 ms; CPU 2225.060 → 1848.645 ms | −16.85% wall / −16.92% CPU |
| 16,384 mixed stable sort | wall 1024.335 → 584.589 ms; CPU 1020.095 → 582.026 ms | −42.93% wall / −42.94% CPU |
| wide128 JSON, 20 parses / 20 stringifies | 1144.937 → 665.942 / 332.735 → 255.116 ms | −41.84% / −23.33% wall |
| FILE200, additional internal repair | wall 16.265 → 14.015 ms; CPU 8.964 → 8.358 ms | −13.83% wall / −6.76% CPU |
| FILE200, separate V1 → refined logging cohort | wall 17.685 → 14.650 ms; CPU 11.863 → 8.861 ms | −17.16% wall / −25.31% CPU |
| Large graph valid edit / completion | 295.154 → 288.810 / 192.141 → 185.954 ms | −2.15% / −3.22% wall |

Refined logging's separate primary near-limit framing controls retain changes
of **+0.04% ASCII, +4.75% escaped, +7.31% Unicode and +3.15% escaped-near-cap**.
The separate fixed-core diagnostic does not reproduce the Unicode increase
(−0.41%), but retains escaped 1 KiB **+2.19%**, 16,000-byte **+1.85%** and near-cap
**+2.30%**. Those cohorts remain separate; variation is not proof of noise or a
causal repair. Complete file/console gains coexist with framing regressions.
[Full logging controls and all samples](../log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md#heavy-controls-and-remaining-regressions).

Important retained regressions: large rename **+3.61%**; medium rename/formatting
**+5.86% / +6.37%**; medium completion/signature **+3.22% / +3.81%**;
small hover **0.085 → 0.099 ms (+15.20%)** and large definition
**0.057 → 0.061 ms (+8.85%)**. Large edit-cohort idle RSS increases
**124,558 → 129,008 KiB**, despite lower HWM **141,124 → 129,008 KiB**.
After query cohorts idle RSS/HWM fall **134,058 → 126,058 / 147,322 → 128,238 KiB**.
No universal editor latency or RSS improvement is claimed.

Original HTTP idle ABBA regresses **65.093 → 87.332 ms wall (+34.16%)** and
**63.682 → 81.995 ms CPU (+28.76%)**, concentrated in the second candidate block.
A separate fixed-core diagnostic improves idle **2.17% wall / 10.28% CPU**;
cohorts are not pooled. Paused HTTP wall regresses **4.81%** originally and
**8.70%** in that diagnostic; some CPU controls also regress. Cause remains
unestablished. No new scheduler change or causal fix is claimed.

In the separate V1/refined logging attribution cohort, source managed
constructions fall **1,839 → 621**, C++ new requests **25,022 → 15,327**, requested
bytes **1,239,656 → 890,280**, and actual GC calls **17 → 11** at the unchanged
budget. Real guards/leases/receipts remain **203/203/202**. Snapshot work changes
from **15,000 scalar inspections → 200 checked bulk ranges**; generic Replies
**209 → 9**. Ordinary join/encoding and 15,000 output bytes remain. These are not
additional savings measured against the internal repair baseline.

Separate instrumented attribution proves less redundant work, not production
milliseconds. For 64 KiB text C++ new requests fall **6,612,625 → 4,214,910**,
requested bytes **337,794,715 → 262,253,167**, fallback offset comparisons
**168,961,375 → 28,405,972**. Mixed sort requests fall
**1,769,701.5 → 308.5**. Wide128 parse×20 requests fall
**2,139,193.5 → 729,653.5**. Across all 16 matched observed cases, managed
construction, scalar access, selected copy, GC and enumerated root-word counts
remain equal. Half request counts are medians of separate observations.
Observed wide128 parse×20 layout constructions fall **1,393,560 → 787,820**;
stringify×20 **499,280 → 331,840**. Layout counts include stack-only expected
geometry checks, which intentionally remain: mixed sort is nearly unchanged
(**1,769,453 → 1,769,446**) despite the large native-allocation reduction.
Instrumentation substantially perturbs elapsed/CPU time.
[Attribution](../../experiments/internal-performance/evidence/attribution-summary.json).

The large index's **9,835 duplicated immutable records → zero**; retained
index-only requested bytes **10,311,792 → 1,059,178 (−89.73%)**, construction
requests **36,952 → 23,642**. The analysis remains the owner; index maps remain.
Runtime adds 48 inline bytes; a cold canonical zero-layout entry retains 144
requested bytes until Runtime destruction, bounded by admitted schemas.
Frame adds eight bytes, token adds 56. These retention costs are not hidden
inside allocation-traffic reductions. The rejected 112-byte Frame cache and
indexed-view regressions remain documented with all discarded samples.

FILE200 remains above the historical strict 10 ms aim. **Boss explicitly does
not mandate that aim as a release requirement. It is neither claimed passed nor
an automatic release blocker.** No arbitrary replacement timing threshold is
introduced. Original evidence and unfavorable measurements remain intact.

## Open issues and severity

| Item | Actual disposition |
| --- | --- |
| Historical baseline startup EOF | **Unclassified historical failure.** The fixed baseline/Release replays do not reproduce it; missing original child diagnostics prevent causal classification. Do not claim repair. |
| Editor/HTTP timing and idle-RSS regressions | **Observed performance limitations.** Correct output/ownership controls do not explain timing causes. Preserve the unfavorable cohorts; no universal speedup or new timing gate. |
| Combined patch on Windows/macOS | **Qualification gap.** Source review and older hosted acceptance do not establish native acceptance of this delta. Boss controls follow-up. |
| Dormant fusion facility | **Low-priority maintenance.** It remains validated and automatically unselected. Removal is deferred; re-enabling it requires a separately selected investigation. |
| FILE200 above 10 ms | **Unmet optimization aim.** Boss explicitly does not mandate it as a release requirement; no automatic release blocker or claimed pass. |

The initially guessed baseline HTTP host path was unavailable; the actual retained
host was then used once. An evidence-verifier helper initially assumed every
manifest record had a byte count; its special report/test records provide only
hashes. The helper was corrected and full preservation verification passed.
[Correction record](../../experiments/integration-qualification/evidence/verification-helper-correction.txt).
These are qualification-helper corrections, not compiler correctness failures;
all execution receipts and original inputs are preserved.

## Future SIMD, platforms and handoff

The [indexed development-strategy section](../DEVELOPMENT_STRATEGY.md#future-optimization-automatic-simd--boss-direction-5-november-2026)
and [detailed preexisting roadmap](../VOIDC_AUTOMATIC_SIMD_ROADMAP.md) record:
**automatic SIMD wherever safe and profitable; ordinary scalar fallback
otherwise; no new source keywords or mandatory SIMD array types.** The
[documentation index](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/README.md#integrated-performance-qualification-and-aot-only-simd--6-november-2026)
links both. The future work covers actual LLVM/Clang/LLD 23.1.4, current O2
PassBuilder without TargetMachine and documented disabled SLP, loop/SLP and
target codegen, ordinary numeric variables/arrays/loops, all checked semantics,
x86-64/ARM64 features, JIT/AOT/reload/cache, machine code and matched benchmarks.
No SIMD code or SIMD benchmark was implemented. Golden Wrench and Boss select
any next optional investigation after integration settles.

This qualification executes **Linux x86-64 only**. Native Windows x64/macOS ARM64
and hosted CI for this combined patch remain unverified. Existing accepted
LLVM23/HTTP/installation hosted results keep their original source identity and
scope; this handoff neither reopens them nor extends them to the new patch.

**Final recommendation: RETAIN for Golden Wrench/Boss review.** Preserve the
accepted cleanup, snapshots, compact success, checked roots, Runtime metadata
reuse and analysis-backed records. Keep the Frame cache removed and automatic
fusion disabled. No new source repair or experimental cleanup was necessary.
The timing/RSS regressions are accepted engineering tradeoffs for retention,
subject to Boss's disposition; a microbenchmark alone supplies no release gate.

[Final preservation verification](../../experiments/integration-qualification/evidence/preservation-verification.json)
checks the unchanged 180 implementation files, all **2,641 original internal
artifacts**, all **769 protected logging artifacts**, root README, existing SIMD
note and Git HEAD/index. New outputs and documentation are indexed by the
[qualification manifest](../../experiments/integration-qualification/evidence/artifact-manifest.json).
[Reproduction and exact command receipts](../../experiments/integration-qualification/README.md)
identify the fixed campaign and preserved helper corrections.
Git staging/commits/pushes/branches, hosted CI and release publication were not
performed. Root README, historical reports/evidence and unrelated work remain
preserved. Golden Wrench reviews the actual integrated delta and this evidence;
Boss controls subsequent Git, scoped native/platform qualification, final CI and
release decisions. Stop for review; do not start SIMD automatically.
