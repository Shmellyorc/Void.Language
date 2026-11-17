# G52 — Reuse scalar list validation results within a checked read

Baseline G51 `a32a5ff3b7ce6c6ea7d56ad26aba578806fa1cac`. G52 makes each ordinary scalar list read do less work inside the Runtime. The improvement applies to indexed traversal of private, locally built lists. It also applies to reads from other scalar lists because the proof covers one locked operation at a time.

## Research and choice

Pinned LLVM 23.1.4 LICM asks MemorySSA and alias analysis which potential writes can invalidate a load. Even where the address is loop invariant, the load needs a memory and fault-order proof. Go 1.25 proves the interval of an induction variable on the dominated loop edge and explicitly checks wrapping. Rust 1.91 derives a slice iterator's endpoint once from an admitted borrowed slice. Its lifetime carries the validity of that extent. In VOID the canonical extent belongs to the live list header, and current scalar list operations establish it while holding the Runtime mutex. The useful local equivalent is consuming the checked layout in the same locked operation. [Pinned LLVM LICM](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/LICM.cpp) · [LLVM MemorySSA](https://llvm.org/docs/MemorySSA.html) · [Go induction analysis](https://github.com/golang/go/blob/go1.25.0/src/cmd/compile/internal/ssa/loopbce.go) · [Rust slice iteration](https://github.com/rust-lang/rust/blob/1.91.0/library/core/src/slice/iter.rs).

Rust's released checked slice indexing also validates the range and then uses the established offsets directly, preserving the failure checks at the index boundary. Odin's merged #7905 removes checks based on previous checks and branch or loop ranges when the contributing values have not been overwritten. Its compiler proof guards Odin's own array addressing. VOID's optimization relies on the mutex for the narrower interval where header and layout facts are reused. [Rust slice indexing](https://github.com/rust-lang/rust/blob/1.91.0/library/core/src/slice/index.rs) · [Merged Odin #7905](https://github.com/odin-lang/Odin/pull/7905).

A loop-level borrow would need both an ownership proof and a footing in the Runtime across poll, cancellation and failure edges. The existing read lease roots the backing and increments its reader count, which can affect checked mutation through an alias. The selected change keeps the proof inside the existing lock. It requires no compiler alias assumption, placement of a new root, or extension of a buffer borrow across a safepoint. The compiler keeps emitting the same checked calls.

## Exact proof and observable effects

1. The scalar checked entry resolves the schema by its canonical key in `types_->schemas` with the Runtime mutex held. Schema admission constructs immutable metadata keyed by its identity and publishes the catalog under that same mutex. A second lookup of the just-resolved schema within that acquisition always yields the same admitted handle. The generic entry still checks schema admission where it receives a schema supplied by the caller. The descriptor, nominal identity, root registration, and backing validation stay in their existing order.
2. The common header check fetches the backing from the same Heap and validates packed geometry or the existing equivalence fallback, completeness and initialized prefix. The result carries the validated layout back to the scalar caller while the mutex is still held. Mutation and collector work take that mutex. Aliased or escaped lists receive the same lock and the full effective checks on each call. A callback, native operation, worker or resize between calls gets a fresh checked read of the resulting state.
3. Signed index normalization still handles the full signed and unsigned domain, including negative indices and INT64_MIN. A successful check proves `index < count <= capacity == layout.capacity()`. The layout constructor checked address and size representability for its full capacity, so the existing address formula is valid without a second compare against capacity. The zero-capacity result has no layout, and index validation rejects every access before it can be used. An empty list with retained capacity also fails the logical bound.
4. Loading copies the same scalar bytes before the mutex releases. The header reference and layout pointer stay on this Runtime call's stack. All generated root scopes and safepoints, current Heap checks, write paths, failure reporting and evaluation order remain intact. The removed table lookups and capacity comparison neither allocate nor reject after their dominating checks pass. The existing checked generalized read for managed elements and fixed arrays is preserved.

The maintained native test checks the exact empty and out-of-range error both before allocation and after removal with retained backing. It checks first and last elements after growth and forced collection, visibility of an alias write, wrong-encoding rejection precedence, and a negative index after refill. Existing tests check each scalar width, logical bounds and descriptor mismatch, receiver/index side effects, managed-reference elements, selected write invalidation, root cleanup, allocation fault sweeps, callbacks, concurrent collection, worker cancellation and async execution.

The exact same Release list-test object linked separately to the G51 and G52 Runtime archives produces identical results and fault-sweep output. This includes 44 failed cold-construction allocation budgets and 17 failed budgets each in normal and O2 JIT before success, with retry and cleanup. The normal and fatal ASan/UBSan/LeakSanitizer runs include the fault probe. The first TSan link attempt for this test encountered duplicate global allocation operators because both the fault probe and TSan define them. The TSan run retains the native allocation interceptors and uses a standalone concurrent list fixture. [Pinned TSan operator definitions](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/compiler-rt/lib/tsan/rtl/tsan_new_delete.cpp).

## Measured work and generated code

The observer builds wrap actual Runtime operations and ordinary, array and aligned C++ allocation requests. The count fixture fixes the printed time field, so differing elapsed-time text does not perturb native allocations or root counts. Both independent AOT modes agree exactly. Each row includes one warmup and three invocations of the selected workload. Each invocation constructs its own list and traverses it 32 times, and an independent arithmetic oracle verifies the checksum.

| List size | Reads and locks on each side | Schema lookups G51 → G52 | Backing lookups G51 → G52 | Redundant capacity checks G51 → G52 |
| ---: | ---: | ---: | ---: | ---: |
| 256 | 32,768 | 65,536 → 32,768 | 65,536 → 32,768 | 32,768 → 0 |
| 1,024 | 131,072 | 262,144 → 131,072 | 262,144 → 131,072 | 131,072 → 0 |
| 4,096 | 524,288 | 1,048,576 → 524,288 | 1,048,576 → 524,288 | 524,288 → 0 |

Every source bounds check, checked read call and read lock still executes. In private-4,096, both builds execute 2,212,923 polls and two collections, with 124 frame-slot visits, 464 root-word visits and 300 non-null frame roots over those collections. Both report 87 Heap object-allocation calls, 45 backing adoptions and 9,104 observed C++ allocation requests. The array-write, integer, sort, and nested JSON observer controls preserve allocation and root counts too. The sort and nested JSON controls exercise 114,688 and 86,152 actual scalar reads, respectively, so their Runtime read work also falls by the same amount per call.

G51 was rebuilt from its exact `array.cpp`, `list.cpp`, and private declarations with the G52 Release flags. The matched Runtime archive comparison differs only in those two translation units. Independently emitted normal and optimized application objects match byte for byte between versions. The backend still calls `vd_list_read_index` from the read loop. Runtime LLVM IR and disassembly show that the locked helper returns the validated layout, allowing the caller to use base plus index times stride without a second `Heap::buffer_state` call or a capacity check from `BufferLayout::element_offset`. Bounds failure and lock cleanup code remain present.

## Linux qualification

The focused Linux Release and fatal ASan/UBSan/LeakSanitizer profiles each passed 32/32 tests. Both exercise the affected source in normal and O2 JIT and preserve independent AOT linking controls. Fatal TSan passed all three selected Runtime controls. Its separately linked concurrent test passed 32 forced collections while the list grew and was read, with exact final contents, a cooperative park and zero residual objects. The independently linked normal and optimized AOT fixtures passed 36 additional exact answers under TSan. Every test process has a hard 1 MiB stack.

## Native Windows qualification

Native Windows 10 x64 build 19045 passed 30/30 in both Debug and Release. Each profile also passed 57 exact normal JIT and 57 exact O2 JIT outputs, and 57 outputs each from the independently emitted and linked normal and optimized AOT images. Together these check 456 additional workload answers. The 20 selected PE stack headers in each profile and all four workload images reserve 1 MiB. LLVM/Clang/LLD reports 23.1.4 revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. All 3,319 source inputs were rehashed after the builds. Installed SDK and provider hashes were preserved.

## Matched Linux throughput

Eight paired blocks ran the uninstrumented builds with the same input, CPU affinity to logical CPU 3 on a Ryzen 7 1700X, a hard 1 MiB stack, and the JIT disk cache disabled. Builds and qualification finished before timing. Mode order rotates, and per-mode G51/G52 launch order alternates. An independent oracle checked all 3,648 timed answers. The table uses the median of eight paired percentage changes, each formed from the median of three actions per process. Negative percentages mean lower elapsed time.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| private-256 | -8.89% | -11.80% | -14.31% | -12.39% |
| private-1024 | -7.46% | -10.37% | -12.82% | -11.74% |
| private-4096 | -8.40% | -11.14% | -12.18% | -12.28% |
| array-1024 | -3.19% | -2.84% | -5.83% | -4.71% |
| array-4096 | -1.76% | -3.25% | -5.04% | -3.47% |
| array-writes | -0.42% | -2.80% | -4.73% | -4.25% |
| sort-1024 | -6.79% | -5.91% | -8.04% | -9.59% |
| dictionary-1024 | +1.19% | -0.36% | -0.34% | -0.68% |
| nested-96 | -2.05% | -1.10% | -1.62% | +0.25% |
| typed-128 | -1.45% | -1.06% | -0.82% | -1.66% |
| integer-1000000 | -0.37% | +0.24% | -0.97% | -1.25% |
| integer-4000000 | -0.12% | +0.03% | -0.29% | -0.59% |
| concat-short | +0.61% | +1.44% | +0.02% | -1.24% |
| concat-1024 | +3.08% | +0.21% | -1.09% | -19.21% |
| concat-long | +0.51% | -0.75% | -1.34% | -29.07% |
| concat-unicode | +2.13% | +0.73% | -0.56% | +0.66% |
| concat-chain | +2.05% | +3.20% | +2.92% | +3.25% |
| join-short | -0.53% | -1.67% | -0.72% | +0.67% |
| join-1024 | -3.27% | -4.35% | -3.30% | -2.88% |

All 96 private-list timing pairs favor G52. The mixed fixture constructs each list and reads every element 32 times. Sorting also improves in all 32 pairs and exercises the optimized Runtime call. Treat the larger apparent optimized AOT concat-1,024 and concat-long gains as linked-image timing changes. The G51 string path itself is built from the same archive members and emits the same application object.

Separate isolated hardware-counter replay confirms a work reduction for private-4,096. Normal and optimized AOT respectively retire 9.61% and 9.73% fewer user instructions, and 9.27% and 9.37% fewer branches. CPU cycles fall 9.72% and 6.48%, and timed elapsed falls 10.31% and 7.07%. Those isolated results retain a speedup in every pair. The integer 4M optimized-AOT counter control shows effectively equal instructions and branches, +0.49% cycles, and +0.85% elapsed in that replay. The run archives each pair and its range.

## Adverse timing and recommendation

The mixed run measured concat-chain at +2.05%, +3.20%, +2.92%, and +3.25% across normal/O2 JIT and normal/optimized AOT. A bounded follow-up repeated the exact test with just that case selected. It used the same linked images and the same input file, with newly paired runs and independent oracles.

| Execution mode | G51 median ms | G52 median ms | Paired elapsed change | Paired range | G52 slower |
| --- | ---: | ---: | ---: | ---: | ---: |
| normal-jit | 1.707 | 1.814 | +7.51% | -1.56% to +16.09% | 7/8 |
| o2-jit | 1.802 | 1.789 | -0.94% | -4.20% to +16.04% | 3/8 |
| normal-aot | 1.684 | 1.702 | +2.01% | -53.18% to +4.39% | 6/8 |
| optimized-aot | 1.735 | 1.786 | +2.15% | -1.11% to +6.23% | 6/8 |

The normal JIT cost is clearest in the isolated replay. Normal and optimized AOT show smaller costs. The separate AOT hardware-counter run records +4.20% and +2.68% timed elapsed, and +1.82% and +1.52% whole-process CPU cycles, with effectively identical retired instructions and branches. Chained concatenation executes zero scalar reads in the observer. Both builds make 30,978 observed C++ allocation requests, run 47,225 polls and 49 collections, and observe the same 15,043 frame root-word visits and 8,553 non-null roots. O2 JIT does not repeat a slowdown in the isolated replay. The mixed normal JIT concat-1,024 and concat-Unicode medians of +3.08% and +2.13% remain in the record, without a separate isolated attribution.

In both AOT images, the inspected application functions move 128 bytes earlier and retain the same within-line alignment. The linked concat entry and GC poll move 2,592 bytes earlier, changing their offsets within a 64-byte line by 32 bytes. The entire stock concat and poll compilation units match. These observations make layout a plausible contributor and leave the exact mechanism unresolved. The fixed linked image also shows apparent improvements on other concat controls despite equal application bytes and unchanged Runtime concat source. Those figures do not establish source-level text gains. Pinned LLD's call graph sorting and LLVM BOLT use profiles to guide executable layout. Applying either here would involve a new profile and linker pipeline choice beyond these checked list reads. The measured regression did not identify a source edit with a defensible link to the text cost. [Pinned LLD ordering](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/lld/ELF/CallGraphSort.cpp) · [Pinned BOLT guide](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/bolt/docs/OptimizingClang.md).

**Correctness qualified. Keep default integration on HOLD pending review of the normal-JIT chained-text cost.** G52 removes concrete Runtime work, and the matched private-list timings consistently improve on this physical Linux host. Native Windows supplies correctness evidence. The accepted G51 build remains the baseline until the owner accepts the measured tradeoff. There is no compiler hoist, new root, ABI entry, layout cache, or second collection representation. No source correction was justified by the focused replay.

The G52 archive retains the exact six-file patch and postimages, pinned identities, oracles, ordinary and observer samples, focused replay, Runtime IR, native disassembly and full test receipts. The G45/G46/G49/G50 holds and accepted G47/G48/G51 evidence remain in the history.
