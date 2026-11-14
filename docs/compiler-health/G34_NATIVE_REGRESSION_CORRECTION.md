# G34-F — Native regression correction and completed performance foundations

G34-I continuation: [production-source integration and acceptance review](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G34_PERFORMANCE_FOUNDATIONS_INTEGRATION.md). Earlier qualification and cleanup below remain historical evidence.

Baseline: `9293084098383b12555f42c7d56a2799ca6e5f36`. Selected isolated candidate: **combinedF = B5 + C2 + D4 + E3**. This is an executable, qualified review candidate. Production integration remains subject to Golden Wrench review and Boss-controlled Git integration, as required by the G34 authorization. Production C++ source and the Git index remain unchanged.

## Correction and causal evidence

The old combination's arithmetic regression reproduced: integer 123.8→131.2 ms, float 30.5→33.4 ms, dependent arithmetic 31.0→33.7 ms. A controlled relink then held each emitted object and Runtime archive identical while changing only input-section order. Old-combination integer changed **135.284→117.170 ms**; baseline original was 126.264 ms. Float changed 33.925→29.252 ms. Relocation-normalized instructions match across the relinks. This establishes a causal link-placement effect in this native boundary path. It does not identify a particular instruction-cache, decoder, µop-cache or predictor mechanism. Branch-miss counts fall rather than rise in the slow candidate; the counters do not justify a branch-misprediction explanation.

**D4 moves the exact D3 `Runtime::poll` body from coordination.cpp to abi.cpp, beside `Runtime::current` and `vd_gc_poll`.** Ordinary native compilation can inline the no-request path, removing one call/return and register bookkeeping. Attachment, owner and Managed validation, the acquire request load, locked recheck, park loop, every source GC poll and worker check remain. No alignment, padding, section ordering or benchmark change enters the selected candidate. The ordering controls are diagnostics only.

The four-block counter cohort changes old-combination median retired instructions 10.629→10.156 billion, branches 3.320→3.088 billion, and cycles 5.635→5.120 billion. Events are user-mode, with 100% scheduling coverage. Exact binary/object identities, samples, selected disassembly and raw counters are retained.

D1/D2/D3 were compared under the ownership correction: D4hot is D1, D4t is D2, and D4 is D3. D1's large poll body still fails to fix arithmetic. The cold split produces about 11% arithmetic improvements; constant-null TLS adds roughly 2–5% in the matched controls. Both refinements are retained on evidence, with no new TLS state or inliner.

**E3 removes a second live-table traversal.** The corrected observer showed that E2 scanned the heap again while participants remained parked. The existing sweep now returns its surviving payload-byte sum; Runtime uses that result for the same budget formula. CombinedF reuses B's existing private collector overload. Standalone E3 keeps public `Heap::collect` unchanged through one private accounting entry. There is no added persistent state, allocation, lock, collector, root authority or policy parameter. The Linux post-rendezvous buffer witness independently compares the returned sum with the public heap snapshot. Failure and timeout paths preserve policy debt.

## Individual disposition and source ownership

| Mechanism | Review status | Remaining limit |
| --- | --- | --- |
| B5 allocation | Implemented and qualified with D4; payload/node reuse, optional private Runtime-tail resource and ASan poisoning retained | Bounded retention and startup cost; no TLS cache, pages, second heap or registry |
| C2 roots | Implemented and qualified; block-maximum synchronous pointer homes reduce descriptor/search work | Array costs remain in some layouts; independent local/parameter/compound/native/async homes remain |
| D4 frames/boundary | Implemented and qualified; exact empty-frame proof, measured cold/TLS refinements, native call layer removed | Necessary frames, polls and cancellation checks remain |
| E3 pacing | Implemented and qualified; one trigger/accounting owner, live bytes folded into sweep | Soft growth allowance and fewer/larger collection events; no hard RSS or pause bound |

The proposed source policy remains count cap 2×base and live-byte headroom `clamp(live/2, 64 KiB, 1 MiB)`, with source factories opting in. Explicit/default host Runtime budgets remain fixed. Selection rests on the throughput, memory and full-interval pause results below; it is not a claim of universally shorter GC tails. Original E2 and all unfavorable results remain recoverable.

The accepted A helper is byte-identical in every candidate: scalar concrete block-confined objects only, ≤256 bytes/object and ≤16 KiB/function. No alias, escape, native/callback, async, cleanup or cross-edge eligibility expands. Constructor effects, identity, root publication, failures and loop-instance proof remain owned by the accepted helper. Broader managed escape analysis is still active and unimplemented.

## Qualification and activation

Fresh Linux Release: 40 selected CTests each for D4, B5D4, C2D4, E2D4, combinedD4, E3 and combinedF. Fatal ASan/UBSan/LeakSanitizer: 40 each for D4, combinedD4, E3 and combinedF, plus pool/pacing/race controls. CombinedD4 and combinedF pass TSan Runtime/race/allocator controls. Expected stale cached storage still reports `use-after-poison`; no sanitizer failure was suppressed.

Native Windows 10 build 19045, LLVM/Clang/LLD 23.1.4, VS2022 and SDK 10.0.19041.0: D4, combinedD4, E3 and combinedF each pass Debug and Release 40-test suites, native controls, normal/O2 JIT, independent AOT and source oracles. Six PE stack headers per cell preserve 1 MiB. Linux executions also use a 1 MiB stack. SDK/provider/helper hashes match; no hosted CI ran.

The corpus covers numeric/dependent loops, eligible/escaping/reference-bearing managed objects, collections/arrays/dictionaries/sorting, text/Unicode, JSON, closures/calls, callbacks, workers/tasks/cancellation, native I/O, failure/cleanup/reload and GC/root controls. Source outputs match independent maintained oracles. Warm throughput uses observer-off binaries; primary comparisons alternate four blocks with three warm samples per cell. Native allocator cases use five samples per block. The component AOT matrix uses six blocks. Builds and native VM execution are stopped during primary timings.

| Case | Accepted constructions → final | Accepted native requests → final | GC epochs → final |
| --- | --- | --- | --- |
| managed-local-True | 0 → 0 | 0 → 0 | 0 → 0 |
| managed-escape-True | 20000 → 20000 | 40624 → 624 | 78 → 78 |
| managed-reference-True | 20000 → 20000 | 80468 → 40468 | 78 → 78 |
| managed-survives-allocation-True | 40000 → 40000 | 201248 → 161248 | 156 → 156 |
| text-1024 | 5233 → 5233 | 74327 → 51869 | 20 → 11 |
| json-128 | 2495 → 2495 | 65481 → 54316 | 10 → 5 |

`managed_constructions` counts actual Heap construction, including cache hits. Native requests are separate. The accepted baseline already eliminates eligible A objects; final still executes zero heap constructions there. Escaping and reference-bearing objects remain real heap constructions. Normal/O2 and independent AOT maintained allocation-elimination tests and IR remain positive/negative controls.

| Case | Accepted root words | C2D4 root words | Final root words | Accepted → final checked offsets |
| --- | --- | --- | --- | --- |
| text-1024 | 7226 | 4734 | 2587 | 1776775 → 423999 |
| json-128 | 7930 | 4983 | 2607 | 4082612 → 140412 |

Final root-word totals include both C and fewer E collections; the C2D4 column isolates representation work. Dynamic normal/O2 frame proof is 1001→1 enter/leave, with **5003 polls and one construction unchanged**, using freshly compiled matching probe ABIs.

The emitted suite object is identical across D4/B5D4/E2D4, and across C2D4/old combined/combinedD4/final combinedF within the corresponding representation. E3 matches E2D4. Runtime changes, not new arithmetic lowering, explain the correction. Exact hashes are in `final-emitted-object-equivalence.json` and `placement-identities.json`.

## Representative performance and retained costs

| Workload / mode | Accepted ms | Final ms | Change |
| --- | --- | --- | --- |
| array/1024 / aot | 1.131 | 1.186 | +4.9% |
| array/4096 / aot | 4.489 | 4.668 | +4.0% |
| dependent/1000000 / aot | 30.985 | 26.591 | -14.2% |
| dict/1024 / aot | 9.740 | 9.799 | +0.6% |
| float/1000000 / aot | 30.434 | 26.032 | -14.5% |
| integer/1000000 / aot | 30.796 | 29.027 | -5.7% |
| integer/4000000 / aot | 123.031 | 116.411 | -5.4% |
| io/200 / aot | 6.869 | 6.757 | -1.6% |
| json/128 / aot | 41.555 | 36.384 | -12.4% |
| json/32 / aot | 6.429 | 5.824 | -9.4% |
| sort/1024 / aot | 31.383 | 28.205 | -10.1% |
| sort/256 / aot | 6.216 | 5.746 | -7.6% |
| text/1024 / aot | 57.645 | 53.305 | -7.5% |
| text/256 / aot | 14.078 | 13.062 | -7.2% |
| calls / aot | 1.210 | 0.649 | -46.4% |
| managed-escape / aot | 9.680 | 7.079 | -26.9% |
| managed-local / aot | 0.666 | 0.561 | -15.7% |
| managed-survives-allocation / aot | 43.826 | 42.114 | -3.9% |
| async-tasks / jit-O2 | 1.618 | 1.632 | +0.9% |
| calls / jit-O2 | 1.331 | 0.736 | -44.7% |
| workers / jit-O2 | 0.821 | 0.639 | -22.1% |
| async-tasks / jit-normal | 1.646 | 1.680 | +2.1% |
| calls / jit-normal | 1.374 | 0.778 | -43.4% |
| workers / jit-normal | 0.804 | 0.692 | -13.9% |
| cold / aot | 4.856 | 4.988 | +2.7% |

Arrays retain an adverse result. The earlier final-D4 full comparison showed about 5%; the six-block combination matrix showed 2.6%, with isolated C2D4 about 5.7%. Array lowering changes stack/register allocation, including an accumulator spill and extra moves. Same-object relinks also change array timings (4.923→4.697 ms for corrected-combination array/4096). This establishes placement sensitivity, without assigning the entire residual cost to one spill or cache effect. Eligibility was not narrowed and the benchmark was not weakened.

| Case | Accepted wall ms | Final wall ms | Change | Accepted → final CPU ms | GC epochs | Accepted → final live payload bytes |
| --- | --- | --- | --- | --- | --- | --- |
| small | 3.718 | 2.788 | -25.0% | 3.699 → 2.756 | 78 → 78 | 528 → 528 |
| mixed | 4.709 | 3.457 | -26.6% | 4.665 → 3.423 | 78 → 79 | 8848 → 23056 |
| large | 10.346 | 3.759 | -63.7% | 10.146 → 3.727 | 3 → 1023 | 16842752 → 131072 |
| retained | 6.885 | 6.111 | -11.2% | 6.816 → 6.050 | 78 → 40 | 8704 → 8704 |
| threads | 17.937 | 12.609 | -29.7% | 42.848 → 23.927 | 77 → 77 | 4000 → 4024 |

The large-object pressure control preserves the workload and checksum; policy reduces retained live payload while increasing collection frequency. Threaded CPU and all unfavorable samples remain in the raw results.

| Operation | Accepted ms | Final ms | Change |
| --- | --- | --- | --- |
| cold / aot | 4.856 | 4.988 | +2.7% |
| cold / jit-normal | 3034.200 | 2998.308 | -1.2% |
| cold / jit-O2 | 3725.646 | 3639.397 | -2.3% |
| compile / compile-link | 3578.588 | 3509.903 | -1.9% |

The separate 24-pair cold-AOT median is **4.641 → 4.681 ms (+0.9%)**. Component cold controls are retained. B adds an upfront optional 98,368-byte cache metadata allocation per Runtime on Linux; startup is a recorded cost, not a universal win. Source compile/link and cold JIT are measured separately. Native compiler rebuild times are not substituted for source compilation times.

| Artifact | Accepted bytes | Final bytes |
| --- | --- | --- |
| voidc | 80560792 | 80565528 |
| suite | 5009680 | 4987216 |


| Process workload | Accepted median peak KiB | Final median peak KiB |
| --- | --- | --- |
| g31 / aot | 13660.0 | 13660.0 |
| g31 / jit-normal | 126744.0 | 128010.0 |
| g31 / jit-O2 | 126686.0 | 126242.0 |
| native / None | 21862.0 | 13706.0 |
| compile / None | 122284.0 | 120840.0 |

Peak RSS is measured per process and includes compiler/JIT state where applicable; it is not interchangeable with managed live payload. Ranges remain in the raw evidence.

## GC pauses, memory and default tradeoff

These matched three-variant samples measure the ready rendezvous **through epoch release**, including policy work, immediately before mutex release. Earlier G34 and initial G34-F `stopped` values end at the collector phase and omit the post-sweep policy scan while participants remain parked. They are preserved as partial intervals and must not be described as whole stopped-world pauses.

| Case / variant | N | Median µs | p95 µs | p99 µs | Max µs |
| --- | --- | --- | --- | --- | --- |
| text-1024 / base | 816 | 764.2 | 1025.2 | 1210.6 | 1306.9 |
| text-1024 / combinedD4 | 440 | 869.9 | 1200.8 | 1288.5 | 1471.0 |
| text-1024 / combinedF | 440 | 822.0 | 1190.1 | 1275.6 | 1376.1 |
| json-128 / base | 388 | 704.2 | 921.4 | 1068.0 | 1135.3 |
| json-128 / combinedD4 | 192 | 718.3 | 992.5 | 1155.2 | 1165.2 |
| json-128 / combinedF | 192 | 688.0 | 1021.2 | 1084.6 | 1110.8 |

Nearest-rank p95/p99 and maximum are reported with N; these desktop measurements are not latency guarantees. Full collection latency, including rendezvous, is also retained. E3 changes accounting placement, not the live/count budget or its fixed-host behavior. Text/JSON constructions and policy collection counts remain checked. Requested-byte epoch peaks omit pre-existing/reused storage; they are not global physical peaks. RSS, live payload and cache retention are separate measurements.

B's bound is 16 classes through 256 bytes, 256 cached blocks/class: at most 557,056 cached payload bytes plus 98,368-byte Data and bounded detached-node/reference-map metadata on Linux. The existing live registry stays authoritative. Heap remains 120 bytes; Runtime 1008→1048, Frame 40, Slot 32. Existing hot-field offsets stay fixed. D4 adds no state; E3 adds only a sweep-local sum and removes an O(n) traversal/aggregate snapshot. Existing E policy state is 32 bytes and B's tail pointer 8 bytes. Native consumers and libraries are rebuilt together.

## Exact handoff, storage and Git

Full and incremental [review patches](/home/shmellyorc/Documents/Void.Language-archives/g34f-native-regression-correction-2026-11-14-patches) and the [verified evidence archive](/home/shmellyorc/Documents/Void.Language-archives/g34f-native-regression-correction-2026-11-14.zip) retain exact sources, inputs/oracles, binary identities, significant failures, raw balanced results, selected IR/disassembly and replay instructions. G34's original archive remains unchanged at SHA-256 `ed3a59a2129d31e804ca9db103f900fbdf43a5033cdb3c816b04120bd1dc493b`; B/C/D1/D2/D3/E2 and earlier failures remain recoverable. No complete build tree, object collection, SDK, provider installation or compiler cache is retained in this archive. The adjacent verification/cleanup receipts record counts, compressed bytes, observed reclamation and protected hashes after DEFLATE-9/manifest/CRC/extracted/source rehash verification.

The working inventory is one modified `G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, the pre-existing untracked `G34_PERFORMANCE_FOUNDATIONS.md` with a continuation note, and new `G34_NATIVE_REGRESSION_CORRECTION.md`.

The Git inventory includes the G32 attribution reports and G33 experiments/integration reports already tracked at the baseline, preserved by hash; the G33 implementation ledger is intentionally updated. The previously uncommitted G34 report remains in the inventory, with this G34-F report added. Production source and index are unchanged. No staging, commit, push or hosted CI dispatch occurred. Production review remains Boss-controlled; all authorized experimental implementation and qualification is concrete and replayable.

**Verified closeout.** The DEFLATE-9 ZIP retains **519 selected files plus its manifest**, **5,029,098 bytes (4.80 MiB)**, SHA-256 `10b26a81708b7a6d2a958184e35fe94adf2c1aa91d13924c67b83ec7cecb35d3`. CRC, extracted hashes, source rehash and fresh patch/source restoration passed. Fourteen exact review patches are also available beside the ZIP. Linux final cleanup recovered 6.32 GiB observed, plus 3.44 GiB at prior profile retirements: **9.76 GiB observed total**. Native Windows recorded **2.32 GiB observed recovery** across the monitored Debug deletion and final private root. Its 5.97 GiB cumulative profile-build logical volume is separate, not an additional free-space claim.

Both Linux task/transport roots and the Windows private root are absent. Open-consumer checks are empty; protected SDK/provider/helper hashes match. Only this campaign's perf cache copies were removed; installed system libraries remain unchanged. The archive preserves the qualification snapshot; the [external closeout receipt](/home/shmellyorc/Documents/Void.Language-archives/g34f-native-regression-correction-2026-11-14-closeout.json) records final cleanup and Git inventory. No production source, staging, commit, push or hosted CI integration occurred.
