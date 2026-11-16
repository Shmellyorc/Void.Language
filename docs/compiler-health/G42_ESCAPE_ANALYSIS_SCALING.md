# G42 — scalable escape analysis and compiler throughput

Accepted G41: `f46d53746221ca58da428e5b73188314d3a5b11a`, verified live remote `main`. Worktree `/home/shmellyorc/Documents/Void.Language-G42`, branch `codex/g42-escape-analysis-scaling`. LLVM/Clang/LLD 23.1.4. Qualified backend SHA-256 `203900c25aaddf2fb0f2df5a8d15d42f551413eae1102fc4cffc188dfc7dc268`.

**Recommendation: INTEGRATE for Golden Wrench review.** Adopt the bounded reduction in the existing epoch proof's work. Eligibility and generated programs remain as in G41. No optional eligibility expansion is included; no Runtime/public ABI, collector, allocator, GC protocol, root authority or optimization policy changes. G37-F remains performance-held and separate. The timing evidence does not imply that the whole compiler scales linearly.

## What changed and why it preserves eligibility

G41's indexed use and provenance passes already share function facts. The expensive step came after building a candidate's aliases and events: the epoch solver initialized two `std::set` states for every CFG block, copied and intersected sets during full sweeps, then validated ordered loads. Even candidates with no local aliases ran the empty lattice. The 2,000-site G41 nested stress spent 2.310 s of 470.7 s in the broader index/must-proof phase and used about 9.8 GiB RSS. The proof improvement targets a bounded slice of that cost.

The production change is confined to `block_allocation_elision` in `src/backend/llvm/backend.cpp` (65 added, 19 removed, net 46 lines; review the exact patch). Empty local-alias sets have no alias stores or loads to prove and skip the vacuous solver. From one through 64 locals, the existing alias set assigns a bit per indexed local. Reused per-function scratch holds two 64-bit states per block, one queued bitmap, a block worklist and the local-to-bit lookup. At 64 aliases the all-bits mask uses `~std::uint64_t{0}`; it never shifts by 64. More than 64 aliases use the accepted G41 set solver and its original semantics. There is no persistent cache and no new root or ownership subsystem.

Each compact proof starts from the same top lattice state; block zero and predecessor-free entries have empty facts. The compact meet is the original intersection over every indexed predecessor, including backedges. Candidate birth kills every bit; the same ordered stores generate their alias bits. Initially every block is queued, including disconnected cycles, so the reachable and unreachable fixed-point conventions are preserved. A changed output schedules actual terminator successors; unchanged outputs leave their dependants' transfer inputs unchanged. An input change is still recorded even when the block transfer produces the same output. No block can lose more bits than the alias count, and the pending queue contains at most one item per block. On convergence the original birth/store/load order is replayed to reject a load without its current-epoch fact. The monotone transfer and greatest fixed point are unchanged; only representation and scheduling differ.

No new alias, provenance, budget, nested-region or retirement predicate was added. The set of values, locals, private calls and retirement events stays in the existing ordered output containers. Multiple-origin rebinding, incoming/foreign-Heap references, escapes, callbacks, cleanup and async/worker effects retain their accepted fallbacks. Only an eliminated allocation's own OOM possibility can disappear. The 256-byte/object and 16-KiB/function payload limits remain. G39/F profitability and batching, G40 provenance and G41 physical-root homes are unchanged. The G38 optimizer finish and complete AOT entry suffix are byte-identical; matched Core and Runtime libraries are byte-identical as well.

An evidence-only differential harness compiles the actual extracted G41 and G42 epoch solver bodies against 21,000 fixed-seed CFG/event traces. All 9,685 acceptances and 11,315 rejections match. It exercises backedges, disconnected cycles, duplicate edges, sparse local IDs, ordered loads/stores, repeated scratch use and the 64/65-alias boundary. It supplements the maintained source fixtures rather than replacing them.

Two maintained boundary fixtures exercise 64 aliases and the 65-alias set fallback across loop epochs. Each has one source construction per iteration, independent result 46,995 at n=1,000, and zero heap constructions in either compiler. Both the bit representation and fallback activate; CMake lists both normal/optimized AOT objects. All 64 inherited fixtures remain unchanged.

## Work counts and phase costs

Clock-only builds use the same installed compiler flags and neither the allocation interposer nor edge-counting instrumentation. Separate work builds count actual proof C++ allocation calls/bytes within the solver, including vector/set initialization, and count transfer block visits and inspected incoming edges. These are requested temporary bytes over the whole proof, not peak memory. An additional once-per-sample canonical plan comparison observes exact elided values/locals/calls and pre-physical-plan retirement order; these samples are separate from ordinary wall timing.

| Shape | Sites | Proof entries | Solver runs | Transfer blocks | Incoming edges | C++ allocations | Requested bytes |
|---|---|---|---|---|---|---|---|
| flat | 100 | 50 → 50 | 50 → 0 | 50 → 0 | 0 → 0 | 100 → 0 | 4,800 → 0 |
| flat | 500 | 250 → 250 | 250 → 0 | 250 → 0 | 0 → 0 | 500 → 0 | 24,000 → 0 |
| flat | 2000 | 1,000 → 1,000 | 1,000 → 0 | 1,000 → 0 | 0 → 0 | 2,000 → 0 | 96,000 → 0 |
| nested | 100 | 50 → 50 | 50 → 50 | 30,100 → 16,275 | 40,000 → 22,450 | 68,000 → 5 | 4,160,800 → 9,668 |
| nested | 500 | 250 → 250 | 250 → 250 | 750,500 → 406,375 | 1,000,000 → 562,250 | 1,690,000 → 5 | 103,604,000 → 48,220 |

Boundary work observations: the 64-alias proof uses 2,050 → 5 allocation calls and 82,592 → 456 requested bytes; the 65-alias fallback stays at 2,082 calls and 83,872 bytes. On the initial 500-site nested probe, 250 proofs inspect 750,500 → 406,375 transfer blocks and 1,000,000 → 562,250 predecessor edges. The phase drops from 65.8 to 3.2 ms in that pilot. Its wait4 polling rounded process wall by up to 250 ms, so only its direct clocks, counters and oracles are used. Final wall samples use blocking wait4 and a separate watchdog.

Four matched blocks (two in each order) record ordinary host process wall/CPU and peak RSS. An independent phase-clock cohort records frontend parse/semantic analysis, source IR, use/CFG indexing, provenance, candidate proof work, physical-home planning, LLVM lowering and optimization, and JIT materialization. LLVM lower includes internal analysis/home planning; nested timer percentages must not be summed as independent phases. Candidate time includes the proof; total function analysis also includes retirement planning. Percentages below are elapsed-time changes against G41.

| Shape | Sites | Process wall s | Wall Δ | CPU s | Peak RSS KiB | Parse/sema ms | Lower+JIT ms |
|---|---|---|---|---|---|---|---|
| flat | 100 | 0.116 → 0.118 | +1.2% | 0.113 → 0.115 | 50,330.0 → 50,364.0 | 3.860 → 3.884 | 101.611 → 104.547 |
| flat | 500 | 0.582 → 0.572 | -1.8% | 0.574 → 0.565 | 68,820.0 → 68,824.0 | 18.479 → 17.737 | 550.753 → 542.050 |
| flat | 2000 | 4.620 → 4.746 | +2.7% | 4.585 → 4.711 | 141,212.0 → 141,346.0 | 77.441 → 73.836 | 4519.947 → 4650.655 |
| nested | 100 | 0.679 → 0.647 | -4.7% | 0.670 → 0.640 | 76,930.0 → 77,448.0 | 21.443 → 20.450 | 644.578 → 613.809 |
| nested | 500 | 14.727 → 14.554 | -1.2% | 14.604 → 14.435 | 696,578.0 → 696,578.0 | 115.953 → 115.784 | 14528.441 → 14350.225 |

| Shape | Sites | Source IR ms | Use/CFG ms | Provenance ms | Epoch proof ms | Full indexed analysis ms | Home plan ms | LLVM lower ms | LLVM O2 ms | JIT materialize ms |
|---|---|---|---|---|---|---|---|---|---|---|
| flat | 100 | 5.408 → 5.401 | 0.048 → 0.047 | 0.018 → 0.019 | 0.005 → 0.001 | 0.397 → 0.390 | 0.307 → 0.300 | 9.703 → 9.412 | 20.466 → 20.381 | 61.754 → 60.167 |
| flat | 500 | 26.525 → 26.854 | 0.416 → 0.326 | 0.103 → 0.090 | 0.020 → 0.007 | 2.073 → 1.960 | 1.687 → 1.676 | 47.016 → 46.855 | 179.555 → 178.576 | 274.581 → 269.717 |
| flat | 2000 | 109.519 → 113.218 | 2.042 → 1.921 | 1.288 → 1.064 | 0.070 → 0.026 | 11.408 → 11.166 | 8.250 → 7.648 | 193.666 → 197.437 | 2913.873 → 2874.282 | 1424.945 → 1438.280 |
| nested | 100 | 109.009 → 110.140 | 0.205 → 0.195 | 0.083 → 0.080 | 3.029 → 0.136 | 4.843 → 1.495 | 0.595 → 0.588 | 106.965 → 101.040 | 116.097 → 112.680 | 192.437 → 194.190 |
| nested | 500 | 2951.999 → 2899.372 | 1.533 → 1.492 | 0.835 → 0.971 | 73.383 → 3.197 | 92.289 → 12.515 | 4.236 → 3.776 | 2759.394 → 2553.748 | 2880.902 → 2653.758 | 3535.641 → 3389.962 |

The exact 2,000-site nested source could not safely reach the targeted phase under the 8 GiB address-space budget. The accepted G41 attempt returned `std::bad_alloc` after 26.44 s with peak RSS 8,329,048 KiB. The attempted source hash, output and resource data are retained. There is no phase measurement or executed result for that shape.

Separately, an isolated replay ran the actual extracted solvers over a synthetic 6,001-block, 8,000-edge nested-diamond chain with 1,000 independent owner proofs. Both accept all 1,000. The median proof-loop time is 1106.780→41.092 ms and peak process RSS 12,332→12,330 KiB. This excludes frontend, type validation, native lowering and emitted execution; it is a solver stress test, not a result for the exact 2,000-site source.

Full 2,000-site nested LLVM/JIT was deferred because the shared host had less available memory than the previous compile's peak plus reserve. There was no attempt to stop user processes or grow the guest. The flat 2,000-site cohort fully compiles and executes. Larger nested functions still pay major source-IR/validation/LLVM costs even after shrinking the epoch proof; a compact per-candidate worklist still initially touches the entire CFG. Per-candidate event-vector sizing/sorting and high-alias fallback cost remain. This is a bounded reduction, not an asymptotic redesign.

## Source outputs, generated code and native safety

All 66 patterns preserve independent results, exact construction totals, precise stack homes, and required runtime behavior in normal/O2 JIT and independent normal/optimized AOT. All 132 G41/G42 normal/optimized native object pairs are byte-identical. Pre/post LLVM IR also matches for all 132 pairs, as do all 264 ordinary/observer object comparisons, 64 short/long output-path emission comparisons and the two independently linked full AOT timed images. All 18 representative managed AOT object pairs and both arithmetic/collection/text/JSON/I/O AOT images match. Instruction selection, root/clear ordering and payload bytes are unchanged on these measurements; no change in generated native work is claimed.

Linux Release 86/86; fatal ASan/UBSan/LeakSanitizer 86/86; fatal TSan 66 patterns × two independent AOT modes plus Runtime coordination/races/allocator. Native Windows 10 build 19045, MSVC tools 14.44.35207, SDK 10.0.19041.0 and LLVM 23.1.4: Release 71/71 and Debug 71/71. All 3,315 source inputs hash-match before and after both Windows runs; seven PE stack reserves per profile and Linux stack controls are 1 MiB. Twenty-eight selected Windows COFF/assembly pairs and binary identities are retained. These are correctness results, not Windows throughput or macOS qualification.

216,000 raw-address forced-GC checks pass with no extra root authority. For the 96,000 nested-child checks, both G41 and G42 retain the existing one heap child per source owner (1,000 constructions per witness) and exactly 4 live payload bytes after forced GC, including after owner retirement. The exact foreign-Heap rejection, cold injected allocation failure/recovery, callback/reload, worker, cancellation/async, checked collections and SIMD controls pass. Eight separate observer fields (constructions, native requests, root words, Slot visits, nonzero roots, marking, collections and post-call payload) agree over 264 pattern-mode pairs × three samples. Post-call payload is not a live-set measurement; raw forced-collection evidence is separate.

## Observer-off runtime controls and adverse results

All measurement preparation and qualification completes before balanced timing. Matching runtime probes are separate. Artifact equality means AOT timing variation is a control rather than a changed-image or runtime optimization claim. JIT and independent AOT cases include inherited managed fixtures, nested holders, loops, arithmetic, collections/sorting/dictionaries, text/Unicode/JSON, native I/O, calls, closures, workers and async tasks. Every timed invocation checks an independent result. All finite-sample GC pauses and adverse timings are retained; no claim of faster emitted programs. G41's optimized retirement-only +7.1%, null-alias +5.8%, backedge +5.3%, prior failure/control costs and GC-tail changes remain historical results.

| Mode | Largest adverse observation | Median ns G41→G42 | Elapsed Δ |
|---|---|---|---|
| jit-normal | outlined_array | 5600 → 6480 | +15.7% |
| jit-normal | direct_null_failure | 405 → 445 | +9.9% |
| jit-normal | incoming_reference | 12702305 → 13126932 | +3.3% |
| jit-O2 | outlined_array | 5760 → 6385 | +10.9% |
| jit-O2 | direct_null_failure | 385 → 420 | +9.1% |
| jit-O2 | helper_reference | 5442062 → 5778820 | +6.2% |
| aot-normal | root_zero_live_gap | 19129586 → 20339900 | +6.3% |
| aot-normal | mutable_field | 10040048 → 10650175 | +6.1% |
| aot-normal | direct_null_failure | 280 → 295 | +5.4% |
| aot-optimized | root_zero_live_gap | 19648453 → 20654733 | +5.1% |
| aot-optimized | cross_child_survives | 4880424 → 5130068 | +5.1% |
| aot-optimized | direct_effects | 14398936 → 15089062 | +4.8% |

| Whole-source control | app-normal | app-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| array-1024 | -0.4% | +0.8% | -1.2% | +1.7% |
| array-4096 | -1.7% | -0.2% | -0.8% | +1.0% |
| dependent-1000000 | +1.2% | -2.7% | -0.7% | -0.9% |
| dict-1024 | -0.2% | -0.2% | -2.4% | +1.9% |
| float-1000000 | +0.5% | -1.9% | +0.2% | +0.5% |
| integer-1000000 | +2.1% | -2.0% | -0.7% | -1.4% |
| integer-4000000 | +1.5% | -0.4% | -0.6% | -0.9% |
| io-200 | -1.7% | +2.4% | -0.7% | +3.0% |
| json-128 | +1.1% | -2.4% | +3.5% | +1.8% |
| json-32 | -0.6% | -1.4% | +2.1% | +0.2% |
| sort-1024 | -0.9% | -4.2% | -1.8% | +1.4% |
| sort-256 | -1.8% | +2.5% | -1.3% | -3.9% |
| text-1024 | -0.3% | +2.4% | -0.2% | -2.5% |
| text-256 | -0.1% | -0.0% | -0.7% | -0.4% |

| Managed control | jit-normal | jit-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| async-tasks | +2.4% | -4.6% | -4.6% | -3.0% |
| calls | -0.6% | -1.5% | -0.9% | -0.1% |
| closures | +0.6% | +0.1% | +0.7% | -6.6% |
| managed-across-edge | -1.2% | -1.2% | -0.0% | +0.9% |
| managed-escape | +0.4% | +2.8% | +1.5% | +0.0% |
| managed-local | +1.1% | -0.5% | +0.4% | -0.7% |
| managed-reference | -2.0% | -0.5% | +1.3% | -0.0% |
| managed-survives-allocation | +0.1% | +0.4% | +0.1% | -0.1% |
| workers | -2.2% | -0.3% | -0.4% | +0.4% |

Optimized AOT stopped-world intervals in microseconds (median / descriptive p99 / maximum):

| Case | G41 | G42 |
|---|---|---|
| root_retirement | 7.16 / 22.89 / 27.42 | 7.07 / 20.06 / 24.59 |
| root_null_alias | 7.17 / 9.60 / 14.27 | 7.20 / 9.61 / 19.27 |
| loop_carried | 8.49 / 15.59 / 22.88 | 6.97 / 10.31 / 15.58 |
| cross_nested_diamond | 7.16 / 12.95 / 15.18 | 7.08 / 10.41 / 12.52 |
| cross_nested_child | 7.33 / 10.13 / 14.43 | 7.39 / 15.24 / 17.28 |

| Startup (warm filesystem) | Wall ms G41→G42 | Peak RSS KiB |
|---|---|---|
| aot-normal | 2.481 → 2.424 | 15,228 → 15,228 |
| aot-optimized | 2.150 → 2.159 | 15,228 → 15,228 |
| jit-O2 | 22.456 → 22.475 | 47,096 → 46,884 |
| jit-normal | 20.704 → 20.608 | 44,946 → 44,912 |
| representative-aot | 3.083 → 3.155 | 15,228 → 15,228 |
| representative-aot-normal | 3.159 → 3.196 | 15,228 → 15,228 |


## Rejected alternatives, complexity and next action

A global candidate memo table has no useful shared key here: aliases, ordered stores and birth differ per candidate. Replacing the proof with dominance or ignoring disconnected/backedge facts would require a separate eligibility proof and was not adopted. The worklist uses the existing predecessor table and actual terminators, so it requires neither a persistent successor cache nor a second analysis framework. The bounded word representation eliminates per-block balanced-tree nodes from the common proof. The large-alias fallback intentionally keeps those costs and the same behavior. Full-CFG initialization and alias scans remain; the measured improvement is the execution of the accepted proof. Optional eligibility expansion was deferred to keep the comparison exact.

Initial qualification setup found Ninja was installed outside PATH, then rejected a Release HTTP provider for the fatal profile; pinning the installed Ninja and matching strict providers corrected both. Initial focused selection included unbuilt managed integration targets; narrowed to the 17 built tests, all passed. Raw failures and corrected receipts are retained; these were harness/configuration errors. The first pilot's quantized process wall clock is excluded from final timing.

Next: profile source IR construction/validation on the exact nested stress with a resource-bounded typed-IR snapshot; identify repeated ownership-independent semantic graph copies before changing emitters or expanding eligibility. Within the ownership pass, consider reusing the sparse event staging for candidates while retaining birth/validation order and matched output plans. Remaining Language Bible objectives: broader escape/direct-reference lifetimes, current-Heap provenance and retention effects, checked collection specialization, dictionary/sort, text/Unicode/JSON materialization, SIMD beyond G38, native/JIT overhead, compilation/RSS, GC tails/startup, and future macOS/release qualification. G34–G41 tradeoffs and the rejected G41 Priority B stay in history.

## Evidence, exact inventory and cleanup

The final seven files are:

```text
CMakeLists.txt
docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md
docs/compiler-health/G42_ESCAPE_ANALYSIS_SCALING.md
src/backend/llvm/backend.cpp
tests/escape_analysis/cases.hpp
tests/escape_analysis/fixtures/proof_alias_64.void
tests/escape_analysis/fixtures/proof_alias_65.void
```

The archive `g42-escape-analysis-scaling-2026-11-15.zip` retains exact patches, identity/qualification receipts, raw phase/work/throughput/counter samples and selected IR/native objects. Source restoration, CRC and every SHA-256 manifest hash are checked before removing any task-owned build. Sidecar inventory, verification and cleanup receipts record final source hashes, archive bytes/count, observed host and guest free-space changes separately. Protected worktrees, evidence archives and installed toolchains/providers remain untouched. HEAD remains the accepted G41 base and the seven task files remain unstaged. No commit, push, force-push, history rewrite or hosted CI.
