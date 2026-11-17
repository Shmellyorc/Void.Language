# G49 — Dictionary insertion probe reuse

Accepted G48 baseline `4790a7a52ecf42732d12e3adc35cac3812246516`. Isolated candidate `codex/g49-dictionary-probe-reuse`.

## Established implementations

Go's shipped `PutSlot` searches for the existing key and retains an insertion location within that traversal. Table replacement signals the caller to select a fresh table. Rust's released hashbrown v0.15.5 exposes a found bucket or an insertion slot and requires the table to stay unchanged until `insert_in_slot`. Odin uses Robin Hood displacement and a separate growth check. VOID keeps its packed entry order and linear bucket sequence. The applicable rule is to carry the search result forward while the bucket array is stable. LLVM's GVN illustrates eliminating repeated proven work when the prior value remains available. [Go PutSlot](https://go.dev/src/internal/runtime/maps/table.go#L260) · [hashbrown v0.15.5](https://raw.githubusercontent.com/rust-lang/hashbrown/v0.15.5/src/raw/mod.rs) · [Odin map implementation](https://github.com/odin-lang/Odin/blob/master/base/runtime/dynamic_map_internal.odin) · [Pinned LLVM GVN](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/GVN.cpp).

## Source change and validity proof

The internal probe returns a matching packed entry index or the address of the first zero bucket. Read and remove continue through the existing private lookup method. The only consumer of a vacant address is the nongrowing insertion branch of `Runtime::dictionary_write`. It already holds the Runtime mutex and validates the selected structural version before hashing.

The lookup and the former insertion scan start at the same masked hash and advance by one modulo twice the capacity. The shared lookup retains the bucket offset checks, rejects corrupt indices and bounds the number of probes. A missing key terminates at the same first zero bucket the second scan would find. The two complete backing objects are distinct because their checked capacities differ. Writing the new packed entry, stored hash, valid prefix and structural version cannot move or overwrite the bucket array. The final one-based bucket store remains inside the same lock and follows those writes. Neither the callback-free equality routine nor the in-place commit reaches a safepoint. The borrowed scalar bucket address is confined to that critical section.

The zero-capacity first insertion still grows. An empty dictionary with retained buffers skips lookup and keeps the single existing insertion scan. Replacement keeps its entry-only write and unchanged version. A full dictionary goes through the original allocation, reindex, version check and publication path. That path takes the same rooted old and new buffers across allocation or polling, and never consumes the old vacant address. Clear and removal continue to rebuild the index. The table bounds, overflow, growth failure ordering, checks against foreign or malformed metadata, and G48's validated descriptor reuse all remain in place.

The added native regression uses u64 keys whose low four FNV-1a bits are all ones on x64. This forces a probe to wrap from the last bucket at capacities four and eight. It verifies growth, repeated replacement, removal and appended reinsertion, final iteration order, exact structural version, value identity, and collection cleanup. The existing 4,096-key managed-value test, selected-write invalidation checks, dictionary CLI checks and allocation-fault sweeps run alongside it.

## Measured operation counts

The separately linked normal and optimized AOT observers passed 92 independently checked processes across G48 and G49. They count actual bucket loads at the lookup, insertion and reindex sites, including the terminating zero read. Totals include workload setup, warmup and three sampled invocations. The independent Python model reproduces the exact G48 insertion scan counts for all four sizes, the 128-key wrapping collision cluster and empty reuse.

| Case | Insertion scan reads G48 → G49 | All bucket reads G48 → G49 | Reuses in G49 |
| --- | ---: | ---: | ---: |
| Insert 64 | 15,104 → 0 | 48,128 → 33,024 | 15,104 |
| Insert 256 | 15,936 → 0 | 48,896 → 32,960 | 15,936 |
| Insert 1,024 | 18,544 → 0 | 53,712 → 35,168 | 16,240 |
| Insert 4,096 | 28,144 → 0 | 76,464 → 48,320 | 16,340 |
| Colliding insert 128 | 260,032 → 0 | 621,728 → 361,696 | 3,904 |
| Clear and reuse retained capacity | 459 → 400 | 987 → 928 | 59 |
| JSON tree to dictionary 128 | 672 → 0 | 1,900 → 1,228 | 488 |
| Mixed dictionary, list and sorting 1,024 | 4,640 → 0 | 22,776 → 18,136 | 4,064 |

Every lookup and reindex load count matches, along with every hash, key comparison, growth, managed allocation, buffer adoption and dictionary-scope C++ allocation request. For colliding insert-128, both variants hash 4,128 keys, compare 32 keys, allocate 32 managed dictionary headers, adopt 384 buffers and issue 9,623 linked C++ allocation requests within dictionary operations. The only savings in the timed lookup and replacement cases' observer totals come from their dictionary construction setup. Both independently emitted AOT application objects match byte for byte across G48 and G49. The matching linked images differ through a single dictionary translation unit, with every other Runtime archive member verified identical.

## JIT and AOT timing

Observer-off Linux timing used a Ryzen 7 1700X with process affinity to CPU 3 and a hard 1 MiB stack. Eight paired blocks reverse candidate order and rotate mode order. Each executable verifies independent workload results and reports three samples per case. The table reports the median of the paired percentage differences in elapsed time. Smaller values are faster. Application object identity holds in both AOT modes. Dictionary operation savings can change the linked Runtime's work. Variations in the integer and array controls do not establish faster emitted application instructions.

The following results are for the qualified original G49 candidate against G48. All 4,416 sampled results matched the independent oracles.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| insert-64 | -1.02% | -3.37% | -0.04% | -0.23% |
| lookup-64 | -1.52% | -2.36% | -0.10% | -2.80% |
| replace-64 | +1.70% | -2.94% | +1.10% | -0.90% |
| insert-256 | +0.43% | -1.96% | +1.53% | -1.75% |
| lookup-256 | +1.04% | -3.43% | +2.58% | -2.51% |
| replace-256 | +0.88% | -3.11% | +0.12% | -0.03% |
| insert-1024 | -1.72% | -2.61% | -1.11% | +0.31% |
| lookup-1024 | -3.40% | -4.44% | -2.31% | -1.58% |
| replace-1024 | -0.49% | -1.63% | -0.89% | -2.61% |
| insert-4096 | -0.32% | -3.02% | -0.96% | -2.70% |
| lookup-4096 | -0.37% | -1.89% | -0.86% | -3.24% |
| replace-4096 | -0.25% | -0.58% | -1.47% | -1.27% |
| empty-reuse-64 | -0.11% | -3.28% | -0.67% | -2.31% |
| collision-insert-128 | -6.88% | -7.46% | -8.48% | -8.83% |
| collision-lookup-128 | -1.71% | -3.69% | -1.55% | -2.55% |
| object-lookup-128 | -2.43% | -0.80% | -0.85% | -0.59% |
| json-dict-build-128 | -0.55% | -2.51% | +0.51% | -2.18% |
| json-parse-dict-128 | -0.80% | -0.54% | +0.31% | -1.48% |
| json-dict-write-128 | -1.37% | +0.31% | +0.02% | -1.72% |
| mixed-1024 | -0.70% | -1.06% | -0.57% | -2.02% |
| array-1024 | +1.25% | +0.31% | -2.06% | -4.07% |
| array-4096 | +3.00% | -0.38% | -4.13% | -1.42% |
| integer-1000000 | +2.98% | +0.08% | -1.64% | -4.05% |

Collision-heavy insertion improved in 31 of 32 matched pairs. In the two-pair optimized-AOT counter replay, it retired 6.06% fewer user-space instructions and 4.90% fewer branches, with 6.27% fewer cycles. Insert-4,096 retired 0.55% fewer instructions. Its cycle result was −2.16%. The integer instruction total differed by +0.003%, which gives no evidence of an added arithmetic operation. Ordinary hash and equality work is unchanged. The timing and hardware counter lanes answer separate questions.

The original candidate's main run recorded +2.98% normal-JIT integer and +3.00% normal-JIT array-4,096. The same executable hashes reproduced +2.03% integer in the mixed fixture, +2.57% in the isolated integer replay, +4.44% for array-1,024 and +5.52% for array-4,096. The eight-block recheck retains individual pairs. Both arrays slowed in a majority of those pairs, including all eight array-1,024 pairs.

The checked search loop was instantiated twice by the original lambda-based helper. Clang increased `dictionary_write` from 4,692 to 5,268 bytes. I tested one shared out-of-line probe using the same checks and statically bound member calls in a separate overlay. The trial reduced `dictionary_write` to 4,750 bytes, with a 383-byte shared probe and 27-byte read wrapper. LLVM's MachineOutliner provides precedent for sharing duplicate instructions. Clang's attribute controls source-level inlining. The Go, hashbrown and Odin sources preserve distinct hot lookup and insertion arrangements for their own representations. Their layouts and growth rules do not establish an equivalent result for VOID. Code size and link-address changes are directly observable. The cause of timing shifts in unrelated JIT workloads remains unproven. [Pinned LLVM MachineOutliner](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/CodeGen/MachineOutliner.cpp) · [Clang noinline](https://clang.llvm.org/docs/AttributeReference.html#noinline) · [Go's uint64 path](https://go.dev/src/internal/runtime/maps/runtime_fast64.go) · [hashbrown raw table](https://raw.githubusercontent.com/rust-lang/hashbrown/v0.15.5/src/raw/mod.rs) · [Odin Runtime](https://github.com/odin-lang/Odin/blob/master/base/runtime/dynamic_map_internal.odin).

### Bounded shared-probe trial

The three executables ran in all six permutations. All 5,022 samples matched the independent results. Timing below reports the median paired difference against G48 and includes the isolated integer lane.

| Workload and mode | Original G49 | Shared-probe trial |
| --- | ---: | ---: |
| Normal JIT collision insert 128 | −7.24% | −7.29% |
| O2 JIT collision insert 128 | −3.56% | −5.63% |
| Normal AOT collision insert 128 | −7.16% | −5.91% |
| Optimized AOT collision insert 128 | −6.87% | −7.09% |
| Normal JIT integer 1M, mixed program | +1.16% | −0.62% |
| Normal JIT integer 1M, isolated | −2.58% | −1.57% |
| Normal JIT array 1,024 | +1.12% | +2.42% |
| Normal JIT array 4,096 | −0.77% | −1.58% |
| Normal JIT JSON dictionary construction | +2.62% | +5.93% |
| Normal JIT JSON typed parse | −0.26% | +2.41% |
| Normal AOT integer 1M | −1.82% | +2.22% |
| Optimized AOT integer 1M | −0.36% | +1.33% |

The original candidate's array-4,096 and isolated integer slowdowns did not recur in this three-way run. Normal JIT integer in the mixed program was slower in five of six original-candidate pairs. The trial preserved the collision benefit and showed +2.22% on normal AOT integer with all six pairs slower. Its normal JIT JSON construction took +5.93% longer with four of six pairs slower, and parsing took +2.41% longer with five of six pairs slower. That tradeoff did not justify adopting the shared-probe trial. It remains in the archive alongside the qualified candidate. The independent relinks place the same `vd_gc_poll` and `vd_worker_checkpoint` at different addresses. These observations establish neither a cache mechanism nor a transferable cost across CPUs.

## Qualification

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed 40/40 with a hard 1 MiB stack. The dictionary fault sweeps match accepted G48, including 35 injected growth failures, one nongrowing-insertion failure, 336 source failures for the main fixture and 1,347 for transactions in each JIT mode. The native tests verify the existing error, root cleanup and retry paths.

Fatal TSan passed three Runtime controls and 138 independent normal/optimized AOT assertions with the instrumented Runtime. A separate host fixture checked 16,384 reads and writes each on one shared dictionary, including 64 nongrowing inserts, 100 collection requests, actual cooperative parking and final Heap cleanup. It used the standard TSan allocator to avoid the fault-injection harness's known symbol conflict recorded in G48. The maintained fault-injection harness runs in the ordinary and strict qualification suites.

Native Windows 10 x64 build 19045 passed 31/31 in both Debug and Release with LLVM/Clang/LLD 23.1.4 revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. Both profiles exercised workers, callbacks, async cancellation, checked failures and the generated GC rendezvous. The independent benchmark fixture passed 552 Windows correctness checks across normal/O2 JIT and independently linked normal/optimized AOT. The checked compiler, test and linked fixture PE headers reserve 1 MiB. Source and protected provider hashes were checked before and after qualification.


## Disposition and reproduction

**HOLD for default integration.** The original candidate is correct in the exercised configurations and removes up to 260,032 redundant bucket loads in the matched collision workload. The array and integer controls showed a measurable slowdown in the eight-pair repeat and different results in the three-way run. The one bounded trial moved the tradeoff to JSON and normal AOT arithmetic. Retain both variants and keep G48 as production. Further performance diagnosis needs a stable reproducer for those guards before another code change. Timing here covers one physical x86-64 host. Windows provides correctness qualification on the VM.

The archive `g49-dictionary-insertion-probe-reuse-2026-11-16.zip` preserves the source patch, four postimages, SHA-256 manifest, profile receipts, raw operation counts and timings, disassembly, replay recipes and rejected trial. The qualified implementation is confined to `src/runtime/dictionary.cpp`. The native regression is in `tests/dictionaries/tests.cpp`.
