# G48 — Dictionary and collection performance

Accepted G47 baseline `1831364f0ca40fc48b7f706073904f764cecb0b3`. Candidate `codex/g48-dictionary-collections`. The change in `Runtime::dictionary_layout` reuses a validated immutable header descriptor already in the Runtime's collection metadata. A warm call avoids another descriptor geometry check, mutex acquisition, metadata lookup and publication of the same shared pointer. The final candidate moves its two owned layout handles into the result.

## Upstream precedents

- Shipped Go maps use a combined insert search and have hash-based shortcuts for equality. The released hashbrown implementation used by Rust's `HashMap` returns either an existing bucket or a vacant insertion slot and requires the table to stay stable between search and insertion. Abseil's existing implementation has a similar insertion-slot protocol. VOID already stores each full key hash and uses it to filter equality. The redundant insertion scan is a separate optimization opportunity. G48 preserves VOID's linear probing, insertion order, packed entry buffer and checked growth. [Go map table](https://go.dev/src/internal/runtime/maps/table.go) · [hashbrown v0.15.5 raw table](https://raw.githubusercontent.com/rust-lang/hashbrown/v0.15.5/src/raw/mod.rs) · [Rust HashMap implementation](https://github.com/rust-lang/rust/blob/master/library/std/src/collections/hash/map.rs) · [Abseil insertion](https://raw.githubusercontent.com/abseil/abseil-cpp/master/absl/container/internal/raw_hash_set.h).
- LLVM's shipped GVN eliminates repeated loads when the original value remains available. Pinned LLVM LICM uses alias and memory-effect proofs before moving invariant work. VOID expresses a domain-specific proof at the relevant source site. The existing Runtime lock protects the cache lookup and the admitted descriptor is immutable. Compiler inference across lock operations and a mutable metadata table would need this invariant. [LLVM GVN implementation notes](https://blog.llvm.org/2009/12/introduction-to-load-elimination-in-gvn.html) · [Pinned LLVM LICM source](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/LICM.cpp).
- CPython searches for a vacant slot after a potential resize, which illustrates the need to invalidate a probe position on growth. The reviewed paths are shipped implementations. [CPython 3.13.5 dictionary insertion](https://raw.githubusercontent.com/python/cpython/v3.13.5/Objects/dictobject.c).

## Exact change and safety

The warm return occurs after NativeDictionary geometry, key-prefix checks, current-Heap canonical entry and key layouts and the current catalog's dictionary key policy checks. The map key is the canonical entry schema pointer. The only writer of its dictionary-header slot already checks the descriptor's size and both precise reference offsets before publishing it under the same mutex. Copying the cached descriptor's owning shared pointer while the lock is held keeps it valid through the caller's use. Moving the two local layout handles transfers their existing ownership into the result. Misses use the same catalog lookup, descriptor admission, validation and optional cache publication. A missing optional cache entry still uses the cold path.

Read, replace, insertion, removal and clear retain their checked header/backing validation and critical sections. Hashing, equality, collision order, stored hashes, growth and reindexing, root registration, allocation checkpoints and failure ordering are unchanged. The header hit performs no new safepoint, allocation or cross-Heap assumption. There are no new metadata fields, tables, ABI entries or dictionary representations.

The maintained dictionary test fills 4,096 keys, repeatedly replaces alternating values with a distinctly rooted managed object, checks the exact structural version and every key after collection, rejects altered native metadata against a warmed entry, and exercises removal, clear and reinsert. Existing tests cover collisions, growth and allocation-failure rollback, nullable and identity keys, embedded NUL keys, checked invalidation and iteration order, managed aliases, large values and reload. The independent collision benchmark generates 128 distinct i32 keys with identical low eight FNV-1a bits. Its final table and all preceding capacities put these keys in the same probe cluster, including wraparound.

## Operation counts and source attribution

The separately linked observer images ran 80 checked cases in the two native AOT modes for each candidate. These are whole-case process totals including untimed setup and each action's warmup. The host counters observe actual Runtime hash and equality calls, lookup bucket reads, dictionary-scope managed allocations and buffer adoptions, and linked C++ allocation requests within dictionary operations. The C++ allocation counter covers ordinary and aligned scalar and array `operator new` calls. All counted operations other than the final header publication lock match against G47 in every case. Observer timing is excluded.

| Case, optimized AOT | Publish locks G47 → G48 | Key hashes | Equalities | Lookup bucket reads | Managed requests | Buffer adoptions | Dictionary-scope C++ allocation requests |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Insert 64 | 34,048 → 1 | 16,640 | 256 | 16,384 | 256 | 2,560 | 38,432 |
| Insert 4,096 | 32,788 → 1 | 16,388 | 4 | 28,196 | 4 | 88 | 19,848 |
| Lookup 4,096 | 204,801 → 1 | 69,632 | 65,536 | 111,495 | 1 | 22 | 6,811 |
| Replace 4,096 | 335,873 → 1 | 135,168 | 131,072 | 215,943 | 1 | 22 | 72,347 |
| Colliding insert 128 | 8,352 → 1 | 4,128 | 32 | 268,256 | 32 | 384 | 9,565 |
| Colliding lookup 128 | 24,833 → 1 | 8,320 | 8,192 | 536,639 | 1 | 12 | 2,696 |
| Typed JSON dictionary build 128 | 2,064 → 1 | 512 | 0 | 708 | 4 | 48 | 2,901 |
| Mixed dictionary/list/sort 1,024 | 20,500 → 1 | 8,200 | 4,100 | 9,352 | 4 | 72 | 7,208 |

The JSON dynamic object lookup control uses a linear member list and calls no dictionary operation. JSON-from-tree and parse-as-dictionary both exercise dictionary construction. Serializing the typed dictionary exercises iteration, using the same compact JSON output oracle. An early process-wide counter differed by two native requests on one case. That counter included work such as formatting elapsed times. The final observer scopes its allocator and heap hooks to dictionary operations and preserves the early mismatch separately.

In both AOT modes the independently emitted G47 and G48 application objects are byte-identical. The matched libraries preserve archive member order and every byte outside the dictionary translation unit. The production G48 image uses the rebuilt Runtime. For the G47 replay the pinned compiler compiles the exact G47 dictionary postimage with the same options and replaces that one archive member. The JIT compiler executable is relinked with the matching library. Selected host disassembly confirms the warm control flow can return before the republish mutex call. Generated application-code improvement is unclaimed.

## First candidate performance

Eight balanced G47/G48 blocks ran after build and correctness jobs exited, on logical CPU 3 of the Ryzen 7 1700X with a hard 1 MiB stack. JIT disabled the code cache. AOT was independently linked. Both variants executed identical fixture inputs, and every one of the 3,840 timed results matched an independent Python oracle. Values are the median of eight within-block percentage changes in elapsed time. Negative values are faster.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| Insert 64 | -1.37% | +0.06% | -3.36% | -1.41% |
| Lookup 64 | -2.06% | -2.71% | -2.66% | +0.43% |
| Replace 64 | -2.88% | -4.23% | -4.27% | -1.80% |
| Insert 256 | -0.66% | -2.79% | -0.36% | -2.73% |
| Lookup 256 | -1.86% | -3.28% | -0.90% | -0.05% |
| Replace 256 | -3.94% | -2.78% | -3.64% | -4.04% |
| Insert 1,024 | -2.25% | -1.85% | -2.25% | -3.51% |
| Lookup 1,024 | -2.94% | -0.93% | -3.73% | -2.58% |
| Replace 1,024 | -2.08% | -6.59% | -4.21% | -4.36% |
| Insert 4,096 | -1.69% | -5.83% | -2.76% | -2.25% |
| Lookup 4,096 | -1.98% | -5.59% | -3.22% | -1.35% |
| Replace 4,096 | -2.40% | -5.15% | -2.81% | -2.91% |
| Colliding insert 128 | -0.29% | -0.66% | -0.77% | -1.39% |
| Colliding lookup 128 | -0.80% | -2.48% | -0.25% | -2.76% |
| Dynamic JSON object lookup control 128 | -2.19% | -3.09% | -1.33% | -1.57% |
| JSON tree to dictionary 128 | -4.27% | -0.07% | -1.59% | -0.11% |
| JSON parse as dictionary 128 | -1.73% | -3.28% | -1.82% | -0.02% |
| Typed dictionary JSON write 128 | -3.54% | -1.25% | -1.81% | -0.50% |
| Mixed dictionary, list and sorting 1,024 | -1.47% | -1.47% | -1.93% | -0.76% |
| Integer control 1,000,000 | +0.16% | -1.41% | +2.31% | -1.81% |

O2 JIT lookup at 4,096 took 43.851 → 41.577 ms by separate image medians and improved in eight of eight pairs. The 4,096 insert case improved in eight of eight normal JIT and normal AOT pairs. Repeated 1,024-key replacement improved in eight of eight pairs in both AOT modes. Typed JSON construction took 8.737 → 8.341 ms in normal JIT. Optimized AOT typed JSON wall-time changes are near zero. The dynamic JSON control executes zero dictionary operations, so its timing variation carries no claim of JSON lookup improvement.

Focused hardware counters take whole-process optimized AOT measurements for separately selected workloads. Across two balanced pairs, retired user instructions fell 1.53% for insert-1,024, 2.48% for lookup-4,096, 2.54% for replacement-4,096, 0.98% for colliding insertion, 0.08% for JSON dictionary construction and 0.37% for mixed collections. The separate lookup counter lane recorded +1.32% cycles and +33.0% branch misses even as its instructions fell. JSON construction recorded +0.61% cycles in that lane. The integer-only optimized AOT control retired essentially the same instruction count. Counter data covers one host. Microarchitectural attribution and Windows throughput remain open.

## Bounded correction

The first candidate's +2.31% paired median in normal AOT integer-1M repeated at +2.69% in 12 extra mixed-executable pairs and +2.28% in integer-only invocations of that same linked image. This workload calls zero dictionary operations. The application object bytes match, while `vd_gc_poll` and `vd_worker_checkpoint` move 48 bytes in the linked image. These differences leave the cause of the elapsed-time shift unresolved.

The bounded correction transfers `entries` and `key` into the warm result with `std::move`. They are owning locals used for the last time at that return. The cached descriptor still needs its owning copy. Existing C++ shared-pointer move implementations transfer the stored handles directly, while a copy acquires another reference. Rust similarly distinguishes a moved Arc from a clone that increments the count. The hot result now avoids two pairs of reference-count operations. The trial used the original linker and object order. [Pinned libc++ shared-pointer implementation](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/libcxx/include/__memory/shared_ptr.h) · [Rust Arc](https://doc.rust-lang.org/std/sync/struct.Arc.html).

In the six-block three-way trial, corrected normal-AOT integer medians versus G47 were −1.95% in the mixed image and +0.22% in integer-only invocations. The separate 12-pair replay of the corrected image measured −0.10% and −0.03% respectively. Corrected normal AOT retained the 4,096-key dictionary gains and accelerated the colliding lookup by 4.23% in that trial. The correction's optimized AOT trial recorded +0.56% for 4,096-key lookup and +1.60% for parsing a typed JSON dictionary. The initial and corrected sources, every adverse result and normal/optimized AOT identities are retained. The corrected production builds reproduced the trial Runtime archive and both linked executables byte for byte.

## Corrected candidate performance

The final rebuilt source passed another eight balanced blocks under the same Linux settings and all 3,840 independent timing-oracle checks. The values are paired median elapsed-time changes against accepted G47.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| Insert 64 | -2.60% | -2.37% | -1.69% | -3.61% |
| Lookup 64 | -3.50% | -5.16% | -1.95% | -2.46% |
| Replace 64 | -2.06% | -4.63% | -1.58% | -3.90% |
| Insert 256 | -2.57% | -2.75% | -1.92% | -1.81% |
| Lookup 256 | -3.39% | -2.68% | -0.58% | -3.22% |
| Replace 256 | -4.69% | -4.05% | -0.48% | -3.43% |
| Insert 1,024 | -2.31% | -2.72% | -0.62% | -0.09% |
| Lookup 1,024 | -4.02% | -4.87% | -1.84% | -0.87% |
| Replace 1,024 | -3.48% | -4.80% | -0.04% | -3.20% |
| Insert 4,096 | -2.09% | -3.18% | +0.05% | -2.54% |
| Lookup 4,096 | -2.83% | -3.96% | -1.31% | -1.25% |
| Replace 4,096 | -2.59% | -5.25% | -0.39% | -3.04% |
| Colliding insert 128 | -0.64% | -2.33% | -0.83% | -1.35% |
| Colliding lookup 128 | -2.02% | -2.78% | -0.56% | -1.59% |
| Dynamic JSON object lookup control 128 | -0.01% | -0.46% | -1.29% | +0.89% |
| JSON tree to dictionary 128 | -0.07% | -2.73% | -0.57% | +0.52% |
| JSON parse as dictionary 128 | -1.94% | -0.48% | -0.02% | -1.14% |
| Typed dictionary JSON write 128 | -0.23% | +0.31% | +0.12% | +0.85% |
| Mixed dictionary, list and sorting 1,024 | +0.10% | -1.45% | -0.69% | +1.62% |
| Integer control 1,000,000 | +0.19% | -0.28% | -0.16% | +2.24% |

At 4,096 keys, O2 JIT lookup improved in eight of eight pairs with separate medians of 41.493 → 40.173 ms. O2 JIT replacement improved in seven of eight pairs. Optimized AOT 1,024-key replacement improved in all eight pairs with medians of 59.824 → 57.627 ms. JSON tree-to-dictionary conversion was 2.73% faster in O2 JIT and 0.52% slower in optimized AOT. Dynamic object lookup executes no dictionary calls. The optimized-AOT mixed collection control was 1.62% slower in the full run.

The corrected whole-process hardware-counter lane retired 1.94% fewer instructions in insert-1,024, 3.16% fewer in lookup-4,096 and 3.25% fewer in replacement-4,096. The same lane recorded −1.23%, −0.35% and −2.40% cycles respectively. Lookup branch misses rose 46.24%, and the mixed collection control recorded +0.29% cycles. Optimized-AOT integer-only retired essentially the same number of instructions and recorded −1.17% cycles in these two counter pairs. These are whole-process measurements. Raw pair times and adverse counter results are retained.

The optimized-AOT integer control had a +2.24% paired median in the full four-mode run. The 12 additional pairs with the same exact executables measured −0.19% in the mixed program and −0.02% in integer-only invocations. The same mixed-program replay saw +0.13% in the mixed dictionary/list/sort case, +2.33% for JSON tree-to-dictionary construction, +1.45% for parsing typed dictionaries and +0.76% for writing a typed dictionary. Individual optimized integer pairs reached +22.6% in the full run and +21.5% in the isolated replay. The linked controls still show variation. Exact microarchitectural attribution and second-host measurements remain open.

## Qualification and disposition

Linux Release passed 40/40 and fatal ASan/UBSan/LeakSanitizer passed the same 40/40 on hard 1 MiB stacks. The selection covers the maintained dictionary suite and both CLI/AOT modes, arrays and collections, JSON accessors, typed and dynamic execution, limits and adverse inputs, Runtime/GC, callbacks, workers, cancellation and async. Its allocation-failure controls assert exact diagnostics, failure ordering, no partial dictionary publication and successful retry.

Fatal TSan passed the three Runtime coordination, invariant and generated-poll controls. The ordinary dictionary fault-injection executable collides at link time with compiler-rt's C++ allocator definitions. The separate instrumented Runtime control uses the standard TSan allocators and checks 16,384 writes and reads each on one concurrently shared dictionary, with 100 collection requests, observed cooperative parking and final Heap cleanup. The independently linked normal and optimized AOT fixtures also passed all 120 assertions with that instrumented Runtime. The initial link failure remains in the evidence. [Pinned TSan allocator definitions](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/compiler-rt/lib/tsan/rtl/tsan_new_delete.cpp).

Native Windows 10 x64 build 19045 passed 31/31 in both Debug and Release with LLVM/Clang/LLD 23.1.4 revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. The generated poll control completed 100 forced collections in each JIT mode per profile, with exactly 2,000,003 polls and cooperative parks. Across both profiles, the independent dictionary and JSON fixture passed 480 output checks in normal/O2 JIT and independent normal/optimized AOT. The checked PE headers reserve 1 MiB. The build runner verified all 3,319 source inputs and the protected provider hashes before and after qualification.

**Recommendation — integrate the corrected candidate after independent review with its measured tradeoffs.** The dictionary work reduction is exact and the larger lookup and replacement gains survive the correction. The first candidate stays held. The final optimized JSON costs are preserved, and Windows performance remains unmeasured. A bounded next implementation can reuse the first checked vacant bucket on nongrowing insertion while preserving the existing growth, failure and iteration contracts.

The qualification archive `g48-dictionary-collection-performance-2026-11-16.zip` preserves both candidate sources, the exact final patch, versioned input manifest, independent oracles, both qualification rounds, raw timings, counters, selected emitted code and replay recipes. G47 remains the accepted production base. G45 and G46 held artifacts and historical ledger entries remain preserved.
