# G34 — Remaining performance foundations

G34-I continuation: [production-source integration and acceptance review](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G34_PERFORMANCE_FOUNDATIONS_INTEGRATION.md). Earlier qualification and cleanup below remain historical evidence.

G34-F continuation: [executable native correction, one-pass pacing accounting and final qualification](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G34_NATIVE_REGRESSION_CORRECTION.md). The G34 results below remain historical evidence. Their `stopped` observer excludes the post-sweep policy scan; G34-F records corrected full intervals.

Engineering baseline `9293084098383b12555f42c7d56a2799ca6e5f36`, including accepted G33-A. Production compiler/Runtime source is unchanged. This handoff supplies isolated B5, C2, D3, E2 and an explicitly identified combined source candidate for Golden Wrench/Boss-controlled integration. No Git staging, commit, push, publication or hosted CI was performed.

The accepted G31-I Runtime rebuild has SHA-256 `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`. LLVM/Clang/LLD 23.1.4 and existing accepted providers were reused. G33's verified original archive remains unchanged; exact recovery, source/binary identities, raw observations and replay instructions accompany this handoff. The accepted Go reference was reused without repeating the unchanged campaign.

**Disposition.** HOLD general/default production integration. The four working mechanisms and explicit combination pass native correctness qualification, but allocation-free AOT arithmetic retains approximately 6–10% slowdowns. B–E remain external candidates. Golden Wrench/Boss review must resolve the native boundary/placement constraint or select a separately qualified tradeoff; eligibility was not narrowed to conceal regressions.

| Release identity | Compiler SHA-256 | Runtime archive SHA-256 |
| --- | --- | --- |
| base | `616762bd92db414174ab6c65b77393b3c00558017ec3e90c37f65a74031ab8bf` | `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc` |
| B5 | `ea7c831124f6e3f173ee09496bccdd178b1dab111c0cb82d56aa3de29b905764` | `5a934cbe296183055e96f2905188ed90e8e682b889bfefe49508ad04f4d64312` |
| C2 | `5bf99347f341317838fd80df1b74e8c07c1d68b3fdfd517140262c747e3c3bb4` | `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc` |
| D3 | `bcea86db6ea8b12cf3d9d1e6113cf290009c26b4de3b67920049abcf68dc3193` | `b42cd5c7cc9355e07b262ed606cca4008cda8a87a64ecb74c689c7cb092b1312` |
| E2 | `3511ddd81ea9c9f5411b7df50b3a68714d2bb1686c997a9a517a481ae30d085e` | `546296bab831837fdffc5434bec39a2d694d6a218c98657c6d99a5850ae243e9` |
| combined | `8977635b665d7309a5630c1f792705da019c8f61adbc6069ca84dd5dd70c8752` | `f1db52d3b7758f69baffb5d0e963847369d64954340ab1975b68f9321e3f8796` |

**B — allocation.** B5 reuses dead payloads and detached registry nodes from the single existing Heap registry. Sixteen size classes cover 16–256 bytes, capped at 256 entries/class; large objects and private buffers keep the original path. Existing Runtime locking owns the cache. A private tail resource preserves the original Heap size and existing hot Runtime offsets. There is no second heap, allocator protocol, TLS cache or page framework. Requested object bytes are zeroed before publication. Cached payloads are explicitly poisoned for ASan, unpoisoned for reuse/free, and descriptor leases are released at retirement. Offset-vector capacity remains bounded and contains no managed references.

B5 uses nonthrowing retirement and statically checked nonthrowing map move assignment. This avoids MSVC Debug STL proxy allocation during sweep or after a cached node has been removed. Optional cache metadata uses ordinary `make_unique`/delete with `bad_alloc` fallback. B4's `nothrow new` path produced three fatal sanitizer failures when ASan bypassed host replacement allocation but deletion used that replacement; those failures are retained, and no sanitizer was suppressed. Cold misses still fail without changing published roots. Windows Debug separately exercises fallible metadata preparation and cached-node recovery; the zero-native-request warm-hit promise is qualified for Release.

Linux Heap stays 120 bytes; standalone B Runtime grows 1008→1016 bytes. Cache data is 98,368 bytes on this STL; maximum cached payload is 557,056 bytes, with bounded registry/map metadata additional. Snapshot cached bytes count payload only. B/B64 capacity controls and B2/B3/B4 negative results remain recoverable.

**C — roots.** C2 bounds packed synchronous pointer SSA homes by the largest validated block instead of summing every block. IR validation creates a fresh operand type vector per block; values crossing edges use locals. Parameters, locals, compound values, construction/native staging and all async homes retain independent storage. Old words remain registered and live until overwritten, preserving the production baseline's conservative visibility. Current referent, root identity, rebinding and frame-reuse checks remain intact. No unchecked access, root cache or indexed iterator was introduced.

C retains the earlier edge-clearing experiment; C2 removes those stores while preserving the descriptor reduction. Text descriptor words fall 7,226→4,734; JSON 8,889→5,559. Checked root-search comparisons fall 1,776,775→423,999 and 4,082,612→140,412 respectively. Constructor and collection counts in these isolated C controls are unchanged. The larger repeat retained array slowdowns without clear stores, so they are not claimed as the sole cause.

**D — frames and polls.** D1, D2 and D3 remain separate measured sources. Empty frames are omitted only when there are no roots or async/display/collection/deinit scope requirements. Actual frame enters/exits fall 1001→1 while all 5,003 measured polls and the one construction remain, in normal and O2 JIT. Root-bearing frames retain nonwrapping identity and authority. Worker cancellation, effects, cleanup, callbacks, code lifetime and reload checks remain.

D2 factors the rare locked poll body into a noinline lambda while preserving validation, acquire load, lock, recheck and park order. D3 additionally marks the two existing constant-null TLS pointers `constinit` at declaration and definition, eliminating an optional ABI initializer guard without adding Runtime state or synchronization. On this build, `vd_gc_poll` shrinks 163→123 bytes; the common `Runtime::poll` body is 28 bytes versus baseline 366. LLVM already inlines the tiny source helper in the call probe; this change removes frame/ABI overhead and does not add an inliner. D3's standalone floating-point regression remains in the measurements.

**E — pacing.** E2 replaces the two duplicated trigger/accounting paths with one private checkpoint and committed-count/byte owner. Legacy explicitly configured Runtime budgets remain fixed. Source-created Runtimes opt into a count cap of twice the base budget and a growth allowance `clamp(live_payload/2, 64 KiB, 1 MiB)`. Counters saturate and reset/recompute only after successful collection. Failed GC leaves policy/counters intact so the next request retries. Both construction and private buffer adoption retain their original validation, poll, locking, preparation and publication order.

A native regression reproduced and corrected E1/initial-E2's zero-byte exemption bug: an invalid ordinary zero-size request must still reach a due collection before layout rejection. Only typed empty buffers skip the checkpoint. A single large request or concurrent NativeSafe buffer preparation can exceed the soft allowance; it is not a hard RSS or pause limit. Computing live bytes reuses the existing Heap snapshot scan after collection and has a measurable cost outside the collector stopped interval. No replacement collector was introduced.

**Qualification.** B5, C2, D3 and E2 each pass 40 Linux Release and 40 fatal ASan/UBSan tests plus native controls. B5, D3 and E2 pass fatal TSan Runtime, race and concurrent allocator controls. Native Windows 10 Debug and Release each pass 40 tests plus independent JIT/AOT applications, native failures and 1 MiB stack checks. The explicit B5+C2+D3+E2 combination independently passes the same Linux Release/strict/TSan and Windows Debug/Release qualification.

The 40-test selection includes precise buffer/schema roots, generated Runtime boundaries, callbacks, failures/invariants, managed source, allocation-elimination normal/O2 JIT and separately linked normal/optimized AOT, cache processes, workers, async parallel/lifecycle/external controls and maintained normal/optimized AOT for managed values, workers, cancellation, cleanup, closures, large frames and native integration. Supplemental native controls exercise progress, shutdown, failure rollback, cold allocation, cache reuse and expected stale-pointer poisoning. Native stacks are 1 MiB; Windows PE headers verify 1,048,576-byte reserve. All result oracles are checked independently. Qualification includes Linux Release/fatal ASan+UBSan with leaks and relevant fatal TSan, and native Windows 10 build 19045 x64 Debug/Release.

**Activation and combined result.** Accepted A remains at zero constructions and zero native requests for 20,000 eligible objects. Escaping constructions remain 20,000 while native requests fall from 40,624 to 624, with 78 collections unchanged. Combined text/JSON root words per observed invocation fall to 2,587/2,607 and checked searches to 423,999/140,412; collection medians fall from about 20 to 11 and 10 to 5. Call frames fall from 1,001 to 1 while all 5,003 polls and the one construction remain.

| Candidate | Case | Baseline ms | Candidate ms | Change |
| --- | --- | ---: | ---: | ---: |
| B5 | aot integer/4000000 | 124.606 | 133.870 | +7.4% |
| B5 | aot float/1000000 | 30.849 | 33.403 | +8.3% |
| B5 | aot array/4096 | 4.726 | 4.709 | -0.4% |
| B5 | aot dict/1024 | 9.889 | 9.900 | +0.1% |
| B5 | aot sort/1024 | 31.511 | 30.414 | -3.5% |
| B5 | aot text/1024 | 57.883 | 57.035 | -1.5% |
| B5 | aot json/128 | 41.886 | 42.647 | +1.8% |
| B5 | jit-O2 calls | 1.402 | 1.413 | +0.7% |
| B5 | jit-O2 managed-escape | 7.322 | 5.997 | -18.1% |
| B5 | jit-O2 workers | 0.860 | 0.844 | -1.9% |
| B5 | jit-O2 async-tasks | 1.644 | 1.676 | +2.0% |
| B4 | aot integer/4000000 | 123.086 | 131.141 | +6.5% |
| B4 | aot float/1000000 | 30.611 | 32.804 | +7.2% |
| B4 | aot array/4096 | 4.698 | 4.622 | -1.6% |
| B4 | aot dict/1024 | 9.760 | 9.822 | +0.6% |
| B4 | aot sort/1024 | 31.647 | 28.580 | -9.7% |
| B4 | aot text/1024 | 57.670 | 54.409 | -5.7% |
| B4 | aot json/128 | 41.098 | 41.422 | +0.8% |
| B4 | jit-O2 calls | 1.343 | 1.319 | -1.8% |
| B4 | jit-O2 managed-escape | 7.224 | 5.555 | -23.1% |
| B4 | jit-O2 workers | 0.856 | 0.838 | -2.1% |
| B4 | jit-O2 async-tasks | 1.615 | 1.638 | +1.4% |
| C | aot integer/4000000 | 124.861 | 124.982 | +0.1% |
| C | aot float/1000000 | 30.775 | 31.060 | +0.9% |
| C | aot array/4096 | 4.771 | 4.988 | +4.6% |
| C | aot dict/1024 | 9.877 | 9.800 | -0.8% |
| C | aot sort/1024 | 32.076 | 28.522 | -11.1% |
| C | aot text/1024 | 59.250 | 58.132 | -1.9% |
| C | aot json/128 | 42.500 | 39.528 | -7.0% |
| C | jit-O2 calls | 1.375 | 1.366 | -0.6% |
| C | jit-O2 managed-escape | 7.345 | 7.132 | -2.9% |
| C | jit-O2 workers | 0.855 | 0.868 | +1.5% |
| C | jit-O2 async-tasks | 1.755 | 1.682 | -4.2% |
| C2 | aot integer/4000000 | 123.305 | 131.447 | +6.6% |
| C2 | aot float/1000000 | 30.487 | 33.061 | +8.4% |
| C2 | aot array/4096 | 4.789 | 4.827 | +0.8% |
| C2 | aot dict/1024 | 9.715 | 9.555 | -1.6% |
| C2 | aot sort/1024 | 31.221 | 28.872 | -7.5% |
| C2 | aot text/1024 | 57.474 | 55.879 | -2.8% |
| C2 | aot json/128 | 41.176 | 37.912 | -7.9% |
| C2 | jit-O2 calls | 1.331 | 1.309 | -1.7% |
| C2 | jit-O2 managed-escape | 7.110 | 7.111 | +0.0% |
| C2 | jit-O2 workers | 0.831 | 0.806 | -3.0% |
| C2 | jit-O2 async-tasks | 1.627 | 1.626 | -0.1% |
| D2 | aot integer/4000000 | 124.937 | 123.646 | -1.0% |
| D2 | aot float/1000000 | 30.876 | 31.429 | +1.8% |
| D2 | aot array/4096 | 4.799 | 4.747 | -1.1% |
| D2 | aot dict/1024 | 9.811 | 10.139 | +3.3% |
| D2 | aot sort/1024 | 31.857 | 31.390 | -1.5% |
| D2 | aot text/1024 | 58.862 | 58.394 | -0.8% |
| D2 | aot json/128 | 42.474 | 41.901 | -1.4% |
| D2 | jit-O2 calls | 1.370 | 0.923 | -32.7% |
| D2 | jit-O2 managed-escape | 7.275 | 7.430 | +2.1% |
| D2 | jit-O2 workers | 0.835 | 0.829 | -0.6% |
| D2 | jit-O2 async-tasks | 1.709 | 1.715 | +0.3% |
| D3 | aot integer/4000000 | 123.148 | 126.647 | +2.8% |
| D3 | aot float/1000000 | 30.525 | 32.295 | +5.8% |
| D3 | aot array/4096 | 4.690 | 4.711 | +0.4% |
| D3 | aot dict/1024 | 9.704 | 9.837 | +1.4% |
| D3 | aot sort/1024 | 31.378 | 31.032 | -1.1% |
| D3 | aot text/1024 | 57.222 | 56.370 | -1.5% |
| D3 | aot json/128 | 40.936 | 41.415 | +1.2% |
| D3 | jit-O2 calls | 1.335 | 0.822 | -38.4% |
| D3 | jit-O2 managed-escape | 7.316 | 6.834 | -6.6% |
| D3 | jit-O2 workers | 0.923 | 0.758 | -17.8% |
| D3 | jit-O2 async-tasks | 1.646 | 1.633 | -0.8% |
| E2 | aot integer/4000000 | 123.321 | 129.416 | +4.9% |
| E2 | aot float/1000000 | 30.631 | 32.282 | +5.4% |
| E2 | aot array/4096 | 4.710 | 4.800 | +1.9% |
| E2 | aot dict/1024 | 9.875 | 9.792 | -0.8% |
| E2 | aot sort/1024 | 31.864 | 32.620 | +2.4% |
| E2 | aot text/1024 | 58.286 | 57.316 | -1.7% |
| E2 | aot json/128 | 42.027 | 41.650 | -0.9% |
| E2 | jit-O2 calls | 1.366 | 1.290 | -5.5% |
| E2 | jit-O2 managed-escape | 7.364 | 7.398 | +0.5% |
| E2 | jit-O2 workers | 0.935 | 0.809 | -13.5% |
| E2 | jit-O2 async-tasks | 1.650 | 1.691 | +2.5% |
| combined | aot integer/4000000 | 123.691 | 131.974 | +6.7% |
| combined | aot float/1000000 | 30.764 | 33.382 | +8.5% |
| combined | aot array/4096 | 4.730 | 4.790 | +1.3% |
| combined | aot dict/1024 | 9.848 | 9.822 | -0.3% |
| combined | aot sort/1024 | 31.363 | 28.421 | -9.4% |
| combined | aot text/1024 | 57.153 | 55.350 | -3.2% |
| combined | aot json/128 | 41.892 | 37.194 | -11.2% |
| combined | jit-O2 calls | 1.323 | 0.824 | -37.7% |
| combined | jit-O2 managed-escape | 7.105 | 5.674 | -20.1% |
| combined | jit-O2 workers | 0.832 | 0.753 | -9.6% |
| combined | jit-O2 async-tasks | 1.636 | 1.673 | +2.3% |

Each standalone and combined primary comparison alternates four blocks with three warm samples per cell. Normal JIT, O2 JIT and independently linked AOT cover integer/float, managed objects, arrays/sorting/dictionaries, Unicode text, JSON, native I/O, closures/calls, workers and async tasks. Native allocator cases have five samples/block. Cold processes and compile/link runs are separate. Builds and native Windows execution are stopped for primary timing; the Windows pause occurs at a compiler-build boundary, not during a timed test. Full samples, ranges, CPU time, RSS and code sizes are retained; observer-off measurements are the throughput authority.

| Operation | Baseline ms | Combined ms | Change |
| --- | ---: | ---: | ---: |
| cold aot | 5.15 | 5.21 | +1.06% |
| cold jit-normal | 3080.73 | 3063.16 | -0.57% |
| cold jit-O2 | 3787.01 | 3777.28 | -0.26% |
| compile compile-link | 3682.54 | 3596.87 | -2.33% |

| Artifact | Baseline bytes | Combined bytes |
| --- | ---: | ---: |
| voidc | 80560792 | 80565784 |
| suite | 5009488 | 4987248 |

**Regressions and limits.** Standalone B5 retains AOT arithmetic regressions around 7–8% despite allocation wins. C2 has repeatable adverse array/numeric controls; D1's arithmetic regression and D3's floating-point cost are retained; E2 has adverse standalone AOT calls/numeric samples. B/E emitted suite objects match baseline, C numeric IR matches after debug-metadata normalization, and D2 array/dictionary IR matches. Several relocation-normalized hot Runtime bodies match while their linked addresses differ. This demonstrates code-placement sensitivity; the precise cache/predictor cause remains unresolved. The archived G33 alignment test is not a repair, and no padding, alignment tuning, eligibility narrowing or replacement framework was added. The combined result above is assessed separately; standalone regressions are not silently erased by it.

**Pauses, allocation and memory.** Combined native large-object pressure improves 10.452→3.764 ms with end payload 16,842,752→131,072 bytes, at 1,023 rather than 3 collections. Small/mixed improve 3.721→2.724 ms and 4.697→3.343 ms; threaded CPU 37.835→37.915 ms is flat. Quiet combined text stopped median is 763→858 µs, p95 1,004→1,229, p99 1,167→1,320 and max 1,273→1,353; JSON is 722→746 µs, p95 941→987, p99 1,067→1,109 and max 1,119→1,131. Requested-byte epoch peaks change 236,502→248,250 for text and 177,393→138,182 for JSON; these omit reused/pre-existing storage and are not global physical peaks.

Quiet E2 cohorts contain 816/440 text and 388/192 JSON collection events, enough for nearest-rank p95/p99. Text collections per invocation fall about 20→11, requested-byte epoch peak 236,502→285,663; stopped median 775→852 µs, p95 1,032→1,187, p99 1,451→1,266, max 2,554→1,476. JSON collections fall about 10→5 and requested peak 177,393→217,666; stopped median 702→773 µs, p95 912→1,060, p99 1,006→1,151, max 1,058→1,296. Operation latency includes policy work outside the stopped interval. These are observer results, not uninstrumented throughput or universal latency promises. Native requested-memory counters cover the observed thread/epoch, omit pre-existing/reused storage and are not global physical-heap peaks; RSS and explicit cached payload/metadata are reported separately. Earlier build-load pause observations are retained only as diagnostics.

**Compilation, startup and complexity.** Separate cold/compile samples and artifact sizes are in the raw/summary evidence and CSV. B adds one optional private cache resource, bounded native retention and no lock; it removes repeated payload/registry-node allocation on hits. C adds no Runtime state or lock and removes descriptor/offset work. D adds no state, lock or allocator and removes unnecessary frame/TLS guard work. E adds policy plus three size counters (32 bytes including layout padding), replaces duplicate ownership with two helpers and adds the live-byte snapshot pass. The combined Runtime tail grows by 40 bytes on Linux while preserving pre-existing field offsets. Matching native consumers/libraries are rebuilt together; frozen mixed-library observer setup was rejected before measurement.

**Remaining work.** Accepted A remains narrowly bounded to scalar concrete block-confined objects, at most 256 bytes/object and 16 KiB/function. Broader reference-containing/interprocedural escape, identity/alias, native/callback, async, cleanup and cross-edge proofs remain active and unimplemented. Thread-local allocation caches, reusable pages and concurrent/generational GC are separate future work. No B–E objective is deleted because a prototype failed. Exact intermediate sources, failures, correction attempts and next integration actions remain in the permanent ledger and evidence.

**Handoff and storage.** Exact [candidate patches](/home/shmellyorc/Documents/Void.Language-archives/g34-performance-foundations-2026-11-14-patches), [evidence ZIP](/home/shmellyorc/Documents/Void.Language-archives/g34-performance-foundations-2026-11-14.zip), adjacent SHA-256/verification/cleanup receipts and `REPLAY.md` preserve the implementation and replay contract. The archive uses DEFLATE-9, manifest SHA-256, CRC, extracted hashes and source rehash verification. It retains essential sources, inputs/oracles, identities, raw results/failures and selected IR/disassembly; it excludes disposable objects, complete builds, SDKs and compiler caches. Identical IR is stored once with hash aliases. The final external cleanup receipt supplies retained-file count, ZIP size, reclaimed Linux/Windows space, protected-provider hashes and consumer checks. The archived report is the qualification snapshot; this repository report and external closeout receipt record final cleanup.

The Git inventory includes the previously uncommitted G32/G33 research reports, now tracked at the starting commit and preserved by hash, with the implementation ledger intentionally updated. Production source and the Git index remain unchanged.

**Verified closeout.** The archive retains **1,158 selected files** plus its manifest, **10,035,027 bytes (9.57 MiB)** compressed. SHA-256 is `ed3a59a2129d31e804ca9db103f900fbdf43a5033cdb3c816b04120bd1dc493b`; CRC, extracted hashes and source rehash checks passed. Linux task/transport cleanup recovered **10.69 GiB** observed free space. Windows recorded **3.02 GiB** observed recovery across the combined profile builds and final private root, with **5.96 GiB** of cumulative profile-build logical bytes removed (logical volume is not an additional free-space claim). Both task roots are absent, consumer checks are empty, and protected SDK/provider/helper hashes match. The accepted archives remain preserved. The final Git inventory is one modified ledger and one new G34 report; production source and the Git index remain unchanged.
