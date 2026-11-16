# G47 — Sorting bounds-check optimization

Accepted G44 baseline `894e2e8d9930028f86737092d3ac74010676f389`. Candidate `codex/g47-sorting-bounds`. Production source changes only in `stdlib/collections.void`. `tests/collections_stdlib/semantics.void` adds an exact argument-sequence and stable ordering oracle in both directions while the comparator clears the caller's input. LLVM, Clang and LLD remain 23.1.4.

## Established approach

Go's SSA `prove` pass eliminates a checked branch when its result follows from dominating comparisons. LLVM's Inductive Range Check Elimination separates a safe iteration interval from checked iterations. Both require a stable relationship between the index and its bound. VOID lowers a scalar list read through a checked Runtime call that validates the current Heap, schema, backing and signed index. Reusing the already checked scalar at the source avoids the second call and all its validation work. Rust's stable merge checks run and scratch extents before using private cursors and keeps a separate panic-recovery invariant. VOID keeps its existing snapshot, merge algorithm and checked accesses. [Go SSA bounds proof](https://go.dev/src/cmd/compile/internal/ssa/prove.go) · [Pinned LLVM range-check pass](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/InductiveRangeCheckElimination.cpp) · [Rust stable merge](https://doc.rust-lang.org/src/core/slice/sort/stable/merge.rs.html).

## Safety proof

- The snapshot is complete before either scratch list is filled. `order` receives each ordinal in `[0, size)` and `workspace` receives one zero per ordinal. An initialization failure exits before the merge. The two lists are distinct and private to this sort activation. Only nonstructural stores and swaps of the two local handles follow initialization.
- At a merge entry, `0 <= start <= middle <= finish <= size`, `left = start`, `right = middle` and `destination = start`. At each iteration, `destination - start` equals `(left - start) + (right - middle)`. The cursors are at most their respective ends. If `destination < finish`, at least one run remains. If `right == finish`, then `left < middle`. If `left == middle`, then `right < finish`. Those cases read just the remaining head. The comparison case reads both heads in range and the winning branch reuses the ordinal already read from `order`. It writes the same ordinal to the same checked destination before advancing that cursor. At pass completion the output index buffer is a permutation of the input buffer, including for a comparator whose answers are inconsistent.
- A taken cursor increment starts strictly below its end. Bounded additions used to compute `middle` and `finish` retain the existing subtraction guards. Width doubles only after `width < size - width`. Legal list counts fit `i64`, so these increments and doubled widths fit `i64` and a valid buffer offset remains checked by the original Runtime. Sizes zero and one skip the merge. Every actual list read and write follows the ordinary checked path, including unsupported indexing elsewhere.
- The comparator receives snapshot elements in the same right-then-left evaluation order, and `_precedes` still swaps comparator arguments only for descending order. Equal elements still choose the left run. The moved `order[left]` read occurs before the first generic snapshot access. The private list comes from the current activation, with a rooted scalar header, valid packed backing and an in-range index when that read occurs. Its transparent metadata lookups, root validation, lock and scalar load allocate nothing and invoke no user code. The existing generic snapshot checks, element value copies, comparator and selected workspace write retain their relative failure order. If a checked operation or comparator fails, the workspace write and cursor advance are skipped, as before.
- The comparator can clear or otherwise change an alias of the caller's input, mutate any shared state permitted by readonly rules, fail, allocate or re-enter sort. It has no handle to either scratch list. Re-entry owns distinct scratch lists. The retained ordinal is an `i64` and needs no managed root or borrowed address. The existing backend still emits GC polls and worker checkpoint calls for coordinated blocks, including across callback and loop boundaries. No Runtime, layout, ownership, GC, SIMD or ABI implementation changes are required.

## Checked-operation and emission evidence

The independent Python model checks the comparison count and exact sorted checksum. A separately linked observer counts Runtime scalar reads and allocations during the native entry or one warmed JIT invocation. Normal/O2 JIT and independent normal/optimized AOT agree on every checked read total. The count includes final index iteration as well as merge reads.

| Elements | Comparator calls, both | Checked `i64` reads G44 → G47 |
| ---: | ---: | ---: |
| 0 | 0 | 0 → 0 |
| 1 | 0 | 1 → 1 |
| 64 | 296 | 1,040 → 744 |
| 256 | 1,695 | 5,694 → 3,999 |
| 257 | 1,774 | 6,118 → 4,344 |
| 1,024 | 8,808 | 28,880 → 20,072 |
| 1,025 | 8,914 | 30,128 → 21,214 |

The specialized `sorted<i32>` has five → four static `i64` `vd_list_read_index` call sites and eight → seven total scalar read sites. LLVM IR and native disassembly agree on the read and write call-site changes. The two old checked workspace write sites converge on one site, with one dynamic write per merge iteration in both builds. The emitted function has 968 → 939 disassembled instructions in normal AOT and 792 → 752 in optimized AOT. The native poll/checkpoint sites rise from 36 to 42 in normal AOT and from 32 to 35 in optimized AOT because the control flow changes. The LLVM-print observer emitted objects byte-identical to the ordinary emitter's objects in both modes.

Across each matching mode and size, non-`i64` read counts, native requests and bytes, ordinary allocations/reuse, buffer adoptions, completed collections, nonnull roots, marks and live payload match. For 1,024 elements, the warmed JIT samples have 294 native requests, 66,824 requested bytes, 11 ordinary constructions including reuse, 37 buffer adoptions and one collection each. The collection visits 66 → 63 root words with 26 nonnull words, four marks and 4,140 live bytes on both sides. At 1,025 elements in AOT, native requests are 547 and root words are 65 → 62 with 35 nonnull words, ten marks and 28,816 live bytes on both sides. The observer receipts retain stopped intervals. These few collections establish no pause improvement.

## Failure and concurrent collection controls

The exact comparator trace for five tagged elements is `10,32,31,30,20,41` ascending and `1,23,2,12,13,4,24,14,34` descending, where `10` encodes serials 1 and 0. The first call clears the caller's input. The result still has the expected stable serial order in both directions. A separate comparator prints the two argument pairs `1 3` and `0 2`, clears the input, and fails with `Division by zero` on the second call. Both variants return the same diagnostic and exit status with no third call or result publication. The normal/O2 JIT and both independent AOT paths also reject positive index equal to count and `INT64_MIN` with the exact existing index diagnostic. The valid negative index `-1` returns the last element. Existing large controls cover 16,384 elements and value copies of 2 MiB fixed arrays.

With a fixed GC budget of one, the exact fault sweep reaches first success at 196 → 194 allocation attempts in normal and O2 JIT. Each actual allocation failure retains precise roots, unwinds frames, and permits successful retry. In a separate observer of the same fixture and Runtime budget, all 12 collections, 71 marks, 2,216 summed live bytes, five reused ordinary constructions and eight buffer adoptions match. The tenth collection sees 33 → 30 nonnull root entries. The collector's root vector and Heap marker's pending vector then each stop at capacity 32 instead of growing to 64. Those two avoided scratch requests of 512 bytes each account for the entire 196 → 194 count and 14,872 → 13,848 byte delta. All later collections, marking and source allocations still occur. Sweep counts for the other seven stdlib control operations and partial-map publication failures match.

Fatal TSan passes the two Runtime controls and 32 independently linked generated sort calls per AOT mode with a separately instrumented Runtime on hard 1 MiB stacks. The concurrent collector completes 32 requests in each mode, with 31 normal and 32 optimized cooperative parks, and each sort passes the independent result oracle. The first driver continuously issued collection requests and exceeded the 120-second limit. The final driver bounds the test to one collection request per active sort and asserts actual cooperative overlap. The existing worker effect checker rejects attempts to call the indirect sorting comparator from a worker source body. Separate accepted worker and async regression controls execute in both JIT and AOT modes.

## Qualification

- Linux Release passes 105/105 on a hard 1 MiB stack. Fatal ASan/UBSan/LeakSanitizer passes 104/104 on the same stack limit with `detect_leaks=1`, fatal termination and no suppressions. The Release-only selection is `text_encoding_stack`. TSan uses the separate builds and generated object controls above.
- Native Windows 10 x64 build 19045 passes 71/71 each in Debug and Release with the pinned Clang and LLD and existing native providers. Eight extra stable-sort and comparator-failure pairs exercise JIT and independent AOT and verify 1 MiB reserves in the sort PE images. The runner verifies all 3,319 final source hashes before and after qualification and rehashes the protected providers. The Linux and Windows selections include the existing arrays, dictionaries, text/JSON, Runtime, GC, worker and async controls.
- The first source check rejected a comment written with `//`. The final candidate uses VOID's `#` comments. The archived Linux retry uses the corrected source. Windows reused the task-owned initial build with the final source manifest before collecting either qualifying result.

## G44 comparison

The same pinned compiler and Runtime compile either the accepted standard library or the G47 version. AOT objects are emitted and linked independently. Ordinary observer-free sampling starts after the build and qualification processes exit. Six balanced blocks run on logical CPU 3 of the AMD Ryzen 7 1700X on Linux with the performance governor and boost enabled. Each mixed suite invocation checks the archived independent oracles before returning three elapsed samples per workload. The 2,160 timing rows include JIT worker and async controls. AOT worker and async runs have byte-identical objects and linked images and only serve as output controls. A pilot taken during Windows Debug qualification is retained separately from the table.

| Sorting input | Route | G44 ms | G47 ms | Median time change |
| ---: | --- | ---: | ---: | ---: |
| 256 | Normal JIT | 5.754 | 5.567 | -3.26% |
| 256 | O2 JIT | 5.785 | 5.493 | -5.04% |
| 256 | Normal AOT | 5.431 | 5.107 | -5.96% |
| 256 | Optimized AOT | 5.507 | 4.904 | -10.96% |
| 1,024 | Normal JIT | 29.557 | 26.999 | -8.66% |
| 1,024 | O2 JIT | 28.628 | 26.760 | -6.52% |
| 1,024 | Normal AOT | 27.346 | 25.578 | -6.47% |
| 1,024 | Optimized AOT | 28.062 | 24.742 | -11.83% |


The table uses the ratio of the six block medians. The archive also retains each paired change. The bounds-call reductions and the observed sorting improvement concern the changed sort implementation. Timing changes in other workloads describe their execution in the linked suite.

| Control | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| integer-1000000 | +0.37% | -0.59% | +9.25% | -1.63% |
| integer-4000000 | +0.98% | -0.55% | +10.55% | -0.37% |
| dependent-1000000 | -0.11% | -1.80% | +8.32% | -0.57% |
| float-1000000 | +2.28% | -0.46% | +5.29% | +0.43% |
| array-1024 | -4.46% | -2.73% | -6.79% | +2.51% |
| array-4096 | -2.55% | -0.26% | -7.39% | +2.03% |
| dict-1024 | +1.04% | +1.00% | -0.24% | -2.67% |
| text-256 | -2.92% | +3.76% | +2.08% | -0.16% |
| text-1024 | -0.91% | +2.37% | +0.74% | -0.09% |
| json-32 | +2.94% | +2.02% | -0.21% | -1.26% |
| json-128 | -0.94% | +3.26% | +1.31% | -2.99% |
| io-200 | +1.93% | +14.70% | +0.74% | -0.52% |
| workers | -2.82% | -1.32% | Byte-identical | Byte-identical |
| async-tasks | +1.47% | -0.01% | Byte-identical | Byte-identical |

## Integration disposition

**HOLD for default integration.** The sorting gain and qualification are established. The normal AOT mixed-image arithmetic cost is substantial and reproducible. In six fresh balanced blocks, integer-1M rises from 25.391 to 27.533 ms (+8.43%), integer-4M from 98.567 to 110.350 ms (+11.95%), and dependent arithmetic from 25.019 to 27.163 ms (+8.57%). All six paired medians rise in each case. Sorting still improves in the same normal AOT retest at both sizes. O2 JIT I/O goes from an initial +14.70% with large outliers to +0.86% in the retest, so the initial I/O loss is retained as an unconfirmed regression. The optimized AOT integer lanes stay close to baseline in the main run.

Independent normal and optimized AOT objects and executables for the numeric-only source are byte-identical and pass the integer-1M and 4M oracles. The mixed normal AOT integer function is 544 bytes in both builds and its instruction bytes and relative offsets match in the input objects. One failure-path relocation targets a differently numbered local constant. The linked integer function, GC poll and worker checkpoint move in the larger ELF. The evidence localizes the timing loss to the mixed image and leaves its exact mechanism unresolved. No special alignment, linker orders or placement workarounds were applied.

The source patch carries ten files. Two are code and tests. The remaining eight are the G47 report, updated cumulative ledger and six byte-identical G45–G46-P reports. The original locally modified Wrench rule remains in the canonical checkout. The separate verified evidence ZIP stores exact path and SHA-256 inventory, raw paired timing and observer data, qualification receipts, LLVM IR, selected executables' object inputs and disassembly, and the replay procedures. Source restoration starts at accepted G44. The G45 and G46 experiments and their original archives retain their previous dispositions.
