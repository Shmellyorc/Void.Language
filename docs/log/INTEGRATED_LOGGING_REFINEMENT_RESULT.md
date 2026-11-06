# Integrated logging final refinement — 5 November 2026

> Publication note: this commit keeps selected text evidence. Full local archives,
> binaries, candidate trees and disposable runs remain excluded and preserved;
> see the [artifact scope and original-document hashes](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md).
> Measurements and conclusions below are unchanged; artifact links are portable.

**Recommendation: RETAIN the refined candidate for Golden Wrench/Boss integration review, with automatic fusion disabled.** Production V1 remains untouched. The former consistent 1 KiB framing regression is repaired by returning to V1's proven formatting path. Small-console and complete FILE200 performance now improve substantially while the cleanup and native-I/O changes remain intact. Some framing controls still have small or variable regressions; they are reported below, not discarded. This recommendation is not production replacement or release acceptance.

Matched FILE200 medians are **17.685 → 14.650 ms wall (17.16% lower)** and **11.863 → 8.861 ms CPU (25.31% lower)**. Every matched primary pair improves both wall and CPU. Small-console wall improves **16.33%**; the two large-file workloads improve **28.87%** and **52.20%**. The historical strict 10 ms goal is still missed by **4.650 ms** at the candidate median; it remains an optimization goal, not an automatic blocker.

## Exact refinement and preserved architecture

Repository `/home/shmellyorc/Documents/Void.Language`, origin `Shmellyorc/Void.Language`, branch `main`, HEAD `f3cf240e0b14240d9c7486c8882842bfa7043c63`. Final source/header/stdlib digest: `60ca2861c5aa084c978ff738231de65cd00abb0cb3681669a02f0e5a802b3f4f`. [Initial refinement protection](../compiler-health/evidence/integrated-performance/logging-refinement-starting-identity.json), [final candidate source identity](../../experiments/log-integrated/refinement/evidence/candidate-source-identity.json), [source invariants](../../experiments/log-integrated/refinement/evidence/refinement-source-proof.json). The original integrated source is retained in [original-integrated-source.tar.gz](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-21), alongside the unchanged original patch, report, samples, failures and binaries. The editable candidate has continued to evolve as authorized; older report links into that directory show the current copy, while the archive and original patch preserve its exact previous version.

Only **two files differ from the first integrated candidate**:

| File | Final change |
| --- | --- |
| [`src/frontend/compiler.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-8) | Remove automatic join/UTF-8 fusion selection and its operand-snapshot lowering; retain the ordinary V1 join followed by encoding. No runtime length test, logger identity or benchmark special case. |
| [`tests/advanced/tests.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-19) | Replace fixed fault ordinals 5/6 with verified semantic selection during each failing execution, preserving original state/ownership/root/failure/cleanup assertions. |

The [incremental refinement.patch](../../experiments/log-integrated/refinement/refinement.patch) contains exactly these changes. The [full refined-candidate.patch](../../experiments/log-integrated/refinement/refined-candidate.patch) contains the complete **19-file** delta against production V1, including this authorized isolated test repair. The old `experiments/log-integrated/candidate.patch` remains unchanged historical evidence.

**Formatting decision.** The first fused implementation walks operands to size the result, then revisits roots and restarts checked builder work per operand. On small segments, repeated registered-root validation, locking, polling and builder protocol work outweighed removal of the intermediate string. A simpler bounded native-staging implementation was actually built: it used the existing formatting Builder, copied each checked managed borrow into owned native bytes before polling, and used the existing atomic byte-array publisher. It passed **101 native checks and 28 failing allocation ordinals**, including multi-chunk UTF-8/alias/NUL data and unchanged-output failure checks. Focused matched 1 KiB framing changes were **+1.59% ASCII, −2.53% escaped, +2.69% Unicode**. That did not establish a generally profitable class. [All focused samples](../../experiments/log-integrated/refinement/evidence/focus-samples.json), [attempt source/patch/Runtime archive](../../experiments/log-integrated/refinement/evidence/native-staging-attempt/array.patch), [native checks](../../experiments/log-integrated/refinement/evidence/focus-native.stdout).

The attempt was therefore reverted completely. Automatic fusion is disabled for all previously selected expressions. Operand evaluation, format hooks, conversion errors, quoting, byte limits, precise roots, safepoints and allocation-failure atomicity use V1's existing path. This retains intermediate joined strings and native encoding staging. It does not guess profitability from a particular message length. The private experimental IR/Runtime facility remains unselected for historical/native fixtures; it contributes **zero** calls in all final measurements. Removing that dormant facility can be considered during integration review, but it is not used to obtain the reported gains.

**Cleanup retained.** Eligible synchronous `with` scopes still use the same compiler-managed rooted local and activation flag, replacing capture cell, closure environment and cleanup-list node. Acquisition activates only after the rooted binding is stored; cleanup clears activation before the existing deinit wrapper, keeps the root through that call, and preserves one-shot release, reverse order, cancellation shielding and primary/secondary failures. Unsupported captures, reassignment, dynamic defers, suspending scopes and unproved destructors retain the original chain. Eligibility, IR lifecycle/effect validation, common LLVM lowering, precise root validation and collector settings are unchanged from the first candidate. The observed **1,218** eliminated source constructions include the required **1,200 per-record synthetic objects**, plus 18 in setup/length/flush/close and surrounding eligible cleanup. Real guards, leases and receipts remain.

**Native I/O retained.** Checked dense-u8 snapshot copying still produces an independent native-owned string under the existing read lease, with bounded polling/reacquisition and prefix/bounds/schema checks. The lease ends before native waiting. Compact synchronous write success still returns the completed scalar without generic success Reply construction; typed domain errors and RuntimeFailure remain separate. The same request preparation, shared Job, native admission/resource authority, platform transfer loop, progress, retirement and async generation/ownership machinery remain. There is no second file lifecycle, borrowed caller buffer, stack-Job rewrite, cursor replay or deferred acknowledgement.

**Durability retained.** Every final FILE200 row has 200 individually acknowledged writes, exact ordered 15,000 output bytes and one real final `fsync`, with open/preparation/flush/close and cleanup inside the original invocation timer. Platform filesystem source is unchanged, including POSIX retry behavior and Windows FlushFileBuffers source. No batching, preallocation, range-writeback or synchronization shortcut was added.

All Runtime/header/stdlib/IR/backend files are byte-identical to the first candidate. In particular, no collector allocation budget, GC check, safepoint contract, output publication rule, partial-write authority or platform write/sync implementation was changed by this refinement.

## Consolidated local correctness

The final source was frozen before one consolidated host validation. All program executions used actual soft **and hard 1,048,576-byte stacks**. The original logging host uses its original GC budget **1**; timing is separate at the documented historical budget **256**.

| Final scope | Actual result |
| --- | --- |
| Original logging correctness host, normal/O2 JIT; Release, Debug Runtime, ASan+UBSan | **1,690 checks each**, three hosts |
| Independently emitted/linked normal/O2 logging AOT; Release, ASan+UBSan | **828 checks each**, four hosts |
| Native rooted publication/snapshot fixture, budget 1; same three profiles | **86 checks each**, 23 failing native allocation ordinals, 100,010-byte input |
| Cleanup/ordered formatting/fallback fixture, normal/O2 JIT; same three profiles | **64 checks each** |
| Independently linked normal/O2 cleanup AOT; Release, ASan+UBSan | **28 checks each**, four hosts |
| Original independent bytes/UTC oracle assertions | **15 execution/emit/link checks**, 30 thresholds, all specified Unicode/control cases, eight budgets, nine invalid configurations, eight fixed instants, poisoned-clock and cap checks |
| Original I/O ownership/documentation suite | **6,475 checks**; 307 sync and 629 async allocation failures per normal/O2 mode; 149 public declarations |
| Semantically repaired advanced suite | All **22 examples** in normal/O2, compatible/incompatible reload and root checks; allocation sweeps **226 / 210 / 388 / 133** per mode |
| IR and cache | `_frame`: **two ordinary joins** (including timestamp prefix), **one ordinary encode**, **zero fused**; 18 local cleanups and one compact write in benchmark module; three malformed private ops rejected; three distinct cache identities reused successfully |

[17-host results with executable hashes](../../experiments/log-integrated/refinement/evidence/final-qualification.json), [validation completion](../../experiments/log-integrated/refinement/evidence/validation-complete.json), [byte/UTC output](../../experiments/log-integrated/refinement/evidence/final-byte-utc-oracles.stdout), [verified IR](../../experiments/log-integrated/refinement/evidence/verified-ir-fallback.stdout), [ownership/advanced/cache results](../../experiments/log-integrated/refinement/evidence/adjacent-results.json), [three-way cache evidence](../../experiments/log-integrated/refinement/evidence/cache.json).

Coverage includes exact UTF-8/escape/ASCII and near-limit output, ordered formatting hooks, early return/break/continue, nested LIFO cleanup, failed acquisition/deinit, simultaneous primary/secondary errors, dynamic cleanup fallback, forced collection/root/commit checks, failed publication atomicity, all 18 partial-write prefixes, progress and no replay, rotation/identity/recovery/mixed sinks, reentrancy, native/queued cancellation races, forgotten resources, flush/close failures, startup and reload.

Release uses LLVM/Clang/LLD 23.1.4 and the retained Release recipes. Changed frontend code was recompiled for Release and strict ASan/UBSan; unchanged IR/backend/Runtime objects retain their previously instrumented versions. Strict execution enables leak detection and abort/halt on error. Debug coverage is **Debug Runtime/host with Release frontend/backend**, not a full Debug compiler build. Third-party LLVM is not fully instrumented. No native Windows/macOS execution or full-repository/hosted release matrix is claimed.

**Original versus repaired advanced test:** the unmodified original still **fails** `with allocation failure/root publication`; [this round's original failure](../../experiments/log-integrated/refinement/evidence/advanced-original-run.stderr) and the earlier failure remain. Its fixed 5/6 targets no longer select the resource lifecycle after synthetic cleanup removal. The repair uses the existing `before_construct` probe during each execution: the resource payload must refer to the captured Tracker for the acquisition target; the first body Tracker payload identifies the post-acquisition target. It sets the current probe fault at that observed event, proves exactly one match, and retains the original expected totals **0 before acquisition commits / 1 after it commits**, plus all raised-failure, retained-root, balanced-frame, commit, empty-heap and cleanup assertions. Both modes observed ordinals **2 and 3**, but those numbers are output, not selection constants. [Full repaired run](../../experiments/log-integrated/refinement/evidence/advanced-semantic-run.stdout).

An added IR fixture initially expected only one join in `_frame`, overlooking its timestamp-prefix interpolation. That supplemental assertion failed after all 17 hosts had passed. Its executable/source and raw failure are preserved in `evidence/ir-assertion-failure` and command logs. The fixture was corrected to the source-backed two joins, then the IR and remaining byte/UTC checks passed. Candidate code was not changed and the passing host matrix was not unnecessarily repeated.

## Matched complete-workload performance

Primary environment remains the same Ryzen 7 1700X, x86-64 Linux, XFS `/dev/sda3`, LLVM 23.1.4, O2 JIT and Runtime budget **256**. Production V1 is the reference, with the current accepted text-case object in both variants. [Prior environment inventory](../../experiments/log-integrated/evidence/measurement-environment.json). Timing hosts reuse the exact previous measurement-host and unchanged Runtime instrumentation objects, relinked against the refined frontend/cache identity. Correctness budget-1 hosts are separate. No build or validation ran during measurements.

Primary order **V1 C C V1 C V1 V1 C**, three observations per workload per block: **12 per variant**. Original `tests/log/benchmark.void` command 11 and timer boundaries are unchanged. Record preparation remains inside the timer, as do all 200 acknowledgements, final durable flush and cleanup. Forced pre/post collection, workspace deletion, JIT setup and independent complete-byte oracle remain outside, as in the reference harness. Phase clocks and managed-construction observers are absent from these primary binaries. The original filesystem operation observer and isolated console endpoint remain in both builds. Every row verifies exact bytes/counts and baseline heap/native-handle restoration.

Medians; negative changes mean faster:

| Complete workload | V1 wall ms | Refined wall ms | Wall change | V1 CPU ms | Refined CPU ms | CPU change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| FILE200, command 11 | 17.685 | 14.650 | -17.16% | 11.863 | 8.861 | -25.31% |
| Small console, command 10 | 7.195 | 6.020 | -16.33% | 7.159 | 5.991 | -16.31% |
| 100 × 1 KiB file, command 12 | 19.968 | 14.203 | -28.87% | 13.509 | 7.871 | -41.74% |
| 3 × 65,500-byte file, command 13 | 20.536 | 9.816 | -52.20% | 13.732 | 3.154 | -77.03% |

FILE200 ranges are **17.275–17.971 → 14.343–15.037 ms wall**, and **11.586–12.067 → 8.697–9.213 ms CPU**. Wall means are **17.688 → 14.692 ms**. All matched pairs improve both metrics:

| Pair, chronological blocks | V1 wall ms | Refined wall ms | Wall reduction | CPU reduction |
| --- | ---: | ---: | ---: | ---: |
| 0/1 | 17.620 | 14.649 | 16.86% | 25.46% |
| 2/3 | 17.769 | 14.603 | 17.82% | 26.69% |
| 4/5 | 17.815 | 14.992 | 15.85% | 23.32% |
| 6/7 | 17.492 | 14.530 | 16.93% | 23.84% |

[All 96 primary rows](../../experiments/log-integrated/refinement/evidence/plain-samples.json), [all aggregate/per-pair statistics](../../experiments/log-integrated/refinement/evidence/summary.json). Every measured FILE200 invocation retains 200 writes, 15,000 bytes, one flush, four metadata operations and zero console/move/delete operations. Every sampled workload restores baseline managed payload and native handles after post-timer collection. RSS fields are process high-water observations including compiler/metadata effects, not a clean live-memory comparison.

## GC, allocation, native copies, replies and real synchronization

Separate attribution order **V1 C C V1**, six observations per variant. Nested phase times exclude children; GC is counted once. Per-sample `wall - actual fsync` is computed before aggregation. Outside-sync medians are **12.992 → 9.883 ms**, profile CPU **12.992 → 9.896 ms**, profile total wall **18.699 → 15.517 ms**. These are instrumented runs, not components to subtract from primary medians.

| Separate instrumented phase | V1 exclusive ms | Refined exclusive ms | Calls V1 / refined |
| --- | ---: | ---: | ---: |
| Ordinary join | 0.144 | 0.143 | 207 / 207 |
| Ordinary UTF-8 encoding | 0.620 | 0.599 | 201 / 201 |
| Experimental fused operation | 0.000 | 0.000 | 0 / 0 |
| Owned native byte snapshot | 0.882 | 0.231 | 200 / 200 |
| Generic Reply construction | 0.644 | 0.066 | 209 / 9 |
| Runtime construct_into | 2.162 | 1.320 | 4311 / 2693 |
| Actual Heap::collect | 0.852 | 0.611 | 17 / 11 |
| Actual native write | 1.680 | 1.517 | 200 / 200 |
| Actual final fsync | 5.640 | 5.665 | 1 / 1 |

Individual phase medians need not sum to the median total. Every raw row verifies that exclusive phases fit within its complete wall time and measured GC calls equal Runtime completed-epoch increments. Unattributed work remains. The ordinary join/encode paths have comparable phase cost in both variants; the formerly expensive fused scope is not executed.

Typed source managed constructions remain **1,839 → 621 (−66.23%)**. Runtime `construct_into` calls are **4,311 → 2,693**. The first fused candidate had 2,493; restoring V1 formatting deliberately restores **200 joined-string constructions**. Actual GC is **17 → 11**, compared with 10 in the earlier fused candidate. This is one additional collection in exchange for a faster complete workload and reliable V1 formatting; the allocation budget is unchanged. Exclusive GC medians are **0.852 → 0.611 ms**. Real guards **203**, leases **203** and receipts **202** remain in both variants. Source-constructor identity misses are zero and identities sum to each sample's construction count.

| FILE200 event or selected copy boundary | V1 | Refined |
| --- | ---: | ---: |
| Ordinary C++ new-family requests | 25,022 | 15,327 |
| Requested new-family bytes | 1,239,656 | 890,280 |
| Owned native snapshot bytes | 15,000 | 15,000 |
| Snapshot scalar byte inspections | 15,000 | 0 |
| Snapshot checked bulk ranges | 0 | 200 |
| Bytes appended by ordinary native join, including helpers | 15,530 | 15,551 |
| Owned staging bytes in ordinary encoding | 15,000 | 15,000 |
| Bytes copied into final dense backing | 15,000 | 15,000 |
| Actual native output bytes | 15,000 | 15,000 |

Copy counters cover selected boundaries, not total physical memory traffic. Ordinary join helper bytes are not the logger output-byte count. Final encoding/backing/snapshot/output bytes remain exactly 15,000. Snapshot work changes from 15,000 scalar inspections to 200 checked bulk ranges. Generic Reply calls remain **209 → 9**: ordinary successful writes avoid generic result materialization, while the other operations and full typed failures retain it. New-family counts describe ordinary C++ allocation requests, not all malloc/system allocations or live/peak memory. [All 12 profile rows and exact identities](../../experiments/log-integrated/refinement/evidence/profile-samples.json), [unchanged instrumentation](../../experiments/log-integrated/perf.hpp).

Actual final sync medians are **5.640 → 5.665 ms**. Each sample records exactly one real `fsync`, 200 native writes and the full 15,000 bytes. The candidate's improvement does not depend on a faster sync median. Native write/sync implementation is unchanged. Profile medians are not a device floor or an end-to-end savings forecast.

## Heavy controls and remaining regressions

Original heavy fixture, O2/budget 256, actual 1 MiB stacks, original preparation/checksum/full console-byte oracle and stage boundaries. Stage 7 is framing, 12 is complete console. Order **V1 C C V1**, warmup plus three measurements per case: **168 measured and 56 warmup rows**. Component input setup follows the original fixture outside its timer; it does not replace complete FILE200's timed preparation.

| Input | Framing V1 / refined ms | Framing change | Console V1 / refined ms | Console change | Refined maximum frame / console ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 KiB ASCII ×100 | 4.937 / 4.778 | -3.23% | 10.865 / 6.109 | -43.77% | 5.570 / 6.876 |
| 1 KiB escaped ×100 | 7.763 / 7.601 | -2.08% | 18.179 / 9.157 | -49.63% | 8.021 / 9.623 |
| 1 KiB Unicode ×100 | 5.336 / 5.070 | -5.00% | 11.141 / 6.412 | -42.45% | 5.714 / 7.145 |
| 65,500-byte ASCII ×3 | 2.518 / 2.519 | +0.04% | 10.262 / 2.597 | -74.69% | 2.558 / 2.680 |
| 16,000-byte escaped ×3 | 1.908 / 1.998 | +4.75% | 5.950 / 2.056 | -65.44% | 2.026 / 2.071 |
| 65,472-byte Unicode ×3 | 2.915 / 3.129 | +7.31% | 10.698 / 3.189 | -70.19% | 3.221 / 3.245 |
| 30,816-byte escaped ×3 | 3.542 / 3.654 | +3.15% | 11.406 / 3.784 | -66.83% | 3.739 / 3.817 |

The original 1 KiB framing medians are now **3.23% / 2.08% / 5.00% lower** for ASCII/escaped/Unicode, instead of the earlier consistent 5.6–11% slowdown. All primary small-console pairs improve, and all seven heavy-console medians improve **42.45–74.69%**. Large-file improvements remain substantial. Native snapshot and compact-result gains were preserved rather than reverting the whole candidate.

**Remaining differences are not hidden.** Primary near-limit framing has **+0.04% ASCII, +4.75% escaped, +7.31% Unicode, +3.15% escaped-near-cap** aggregate wall changes. The largest difference, Unicode, falls from **+14.60% in the first matched pair to +1.23% in the second**. Some of the first candidate's modest large-frame fusion gains are deliberately not retained when returning to V1 formatting. These are distinct from its much larger file/console delivery gains, which remain.

One bounded diagnostic round was declared before execution to investigate block drift: same uninstrumented hosts/workloads and Runtime/stack settings, stage 7 only, **both processes pinned to the same available CPU**, order V1 C C V1. It adds **84 measured and 28 warmup rows**, kept separate and never pooled with the primary round. [Plan including CPU selection](../../experiments/log-integrated/refinement/evidence/framing-control-plan.json), [all control rows](../../experiments/log-integrated/refinement/evidence/framing-control-samples.json), [control statistics](../../experiments/log-integrated/refinement/evidence/framing-control-summary.json).

| Input | Fixed-core framing V1 / refined ms | Wall change | CPU change | Pair 1 / pair 2 wall changes |
| --- | ---: | ---: | ---: | ---: |
| 1 KiB ASCII ×100 | 5.013 / 4.834 | -3.58% | -3.21% | -4.56% / -2.57% |
| 1 KiB escaped ×100 | 7.788 / 7.958 | +2.19% | +1.79% | -3.45% / +4.77% |
| 1 KiB Unicode ×100 | 5.217 / 5.185 | -0.60% | -0.72% | -1.73% / -0.36% |
| 65,500-byte ASCII ×3 | 2.739 / 2.581 | -5.77% | -5.01% | -8.92% / +0.36% |
| 16,000-byte escaped ×3 | 1.993 / 2.030 | +1.85% | +1.86% | +3.87% / -0.44% |
| 65,472-byte Unicode ×3 | 3.186 / 3.173 | -0.41% | -0.88% | +0.19% / -0.85% |
| 30,816-byte escaped ×3 | 3.694 / 3.779 | +2.30% | +2.23% | -0.99% / +0.86% |

The primary +7.31% near-limit Unicode difference is not reproduced on the fixed-core control (−0.41%). Other controls still include small regressions: escaped 1 KiB **+2.19%**, escaped 16,000-byte **+1.85%**, escaped-near-cap **+2.30%**, with opposite pair directions. This supports a smaller, variable residual difference rather than the former consistent medium-frame fusion regression; it does not prove that all remaining differences are noise or establish a causal explanation. The candidate is **not universally faster** on framing alone. All sampled candidate framing maxima in both rounds remain below **10 ms**, and all main heavy-console maxima below **20 ms**. No slow samples were dropped or repeated until passing. [All main heavy rows](../../experiments/log-integrated/refinement/evidence/heavy-samples.json).

## Disposition and integration limits

**RETAIN**, with automatic fusion off. Complete FILE200 CPU and wall gains are consistent across all primary matched pairs; small console and large delivery improve; required cleanup and native-I/O gains remain. The medium framing failure that prompted this refinement no longer shows the previous consistent magnitude. The remaining framing tradeoffs and control variation are acceptable for retaining this isolated candidate for review, not a claim of universal improvement or production acceptance.

Candidate median remains **4.650 ms above 10.000 ms**; its best primary observation **14.343 ms** remains **4.343 ms above**. None of its 12 primary FILE200 samples meets the strict goal. No unsupported prediction is made that another local formatter change will reach it. Any further performance work must demonstrate a general profitable class while preserving V1 formatting equivalence, registered roots, bounded safepoints, independent native ownership and real durability.

Integration still needs independent review of compiler cleanup eligibility/IR validation/backend lifetime interactions, private ABI/cache coordination and supported native platforms. The focused local suites do not substitute for full repository or Windows/macOS qualification. Debug frontend/compiler and fully instrumented third-party LLVM are not claimed. The original advanced test remains a historical failure; the authorized semantic repair is reported separately. The dormant experimental fusion facility remains unselected and should not be re-enabled without new general-case evidence.

No production V1 source, root README, historical evidence, Git index/branch/commit or hosted CI was changed. No public syntax, logger-only native engine, alternate collector or duplicated ownership system was introduced. Stop this authorized refinement for Golden Wrench and Boss review.

## Complete delta and reproduction

- [`src/backend/llvm/backend.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-5)
- [`src/backend/llvm/filesystem.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-6)
- [`src/frontend/capabilities.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-7)
- [`src/frontend/compiler.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-8)
- [`src/frontend/filesystem.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-9)
- [`src/ir/filesystem.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-10)
- [`src/ir/local_cleanup.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-11)
- [`src/ir/module.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-12)
- [`src/ir/reference_capabilities.inc`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-13)
- [`src/runtime/abi.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-14)
- [`src/runtime/array.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-15)
- [`src/runtime/buffer.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-16)
- [`src/runtime/filesystem_abi.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-17)
- [`include/void/filesystem_abi.hpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-1)
- [`include/void/ir.hpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-2)
- [`include/void/runtime.hpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-3)
- [`include/void/runtime_abi.hpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-4)
- [`stdlib/io.void`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-18)
- [`tests/advanced/tests.cpp`](../compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-19)

[Actual commands and statuses](../../experiments/log-integrated/refinement/evidence/commands.jsonl) retain compile/link/test/measurement records and their corresponding stdout/stderr, including failures. [Refinement driver notes](../../experiments/log-integrated/refinement/REPRODUCE.md) identify the actual build/validation/measurement sequence. Source snapshots, patches, discarded attempt, native objects/archives/executables, independently linked AOT objects/adapters, cache inputs, workspaces and every raw timing sample remain under [refinement](../../experiments/log-integrated/refinement/).
