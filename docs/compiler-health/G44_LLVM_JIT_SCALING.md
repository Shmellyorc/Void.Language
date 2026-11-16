# G44 — LLVM optimization and JIT compilation scaling

**Disposition — INTEGRATE for Golden Wrench review.** On the exact nested 2,000-site source, median O2 JIT compilation went from 124.938 to 46.892 seconds. The matched capture retained byte-identical optimized LLVM IR and native JIT objects. The accepted baseline and live remote main are G43 `895ae7b291935f2658fc40731ba89a2aed4ebb55`. The isolated branch is `codex/g44-llvm-jit-scaling`. LLVM, Clang and LLD are 23.1.4. The qualified backend SHA-256 is `88a2e505bd1b70de3ce8fd48970a63ec02a3eb8f9a1edb92c7e14ac4741a43e9`.

## Bottleneck and smallest correction

The first remeasurement of the exact accepted source returned oracle 4000 in 126.036 seconds and 1,323,600 KiB peak RSS. SROA accounted for 84.085 of 87.310 seconds of LLVM O2. There were four SROA invocations and three preserved all analyses. The dominant invocation spent its time on a function with 44,003 blocks, 46,002 edges, 253,028 instructions and 17,006 allocation instructions before optimization. The final function had 19,003 blocks, 32,003 edges, 72,020 instructions, 3,001 PHIs and 1,004 allocation instructions.

The separate installed-SDK replay sampled 714 userspace instruction pointers in the exact G43 nested-500 pre-optimization IR. Iterated dominance frontier calculation accounted for 67.37% of the samples, including the time spent by opt parsing the input. The baseline IR contains 3,750 i32 locals with a zero store, a later store and one load, plus an argument local with the same two stores and multiple loads. The zero definitions were redundant where the second store dominated all reads. LLVM's SROA uses the existing promotion machinery to build SSA. [LLVM SROA source](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/SROA.cpp#L5780-L5797) · [LLVM memory-to-register promotion source](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Utils/PromoteMemoryToRegister.cpp#L721-L801)

The backend now checks private scalar homes in optimized functions with at least 128 LLVM basic blocks. A home is eligible when it is a static single-element integer or floating-point alloca with exactly two same-type simple stores and only same-type simple direct reads. One store must write zero in entry before the remaining store. The existing LLVM dominator tree must prove that the remaining store reaches every read, including instruction order within a block. Escapes, metadata users, assignment-tracked stores, aggregate homes, pointer homes, atomics, volatile accesses, extra stores and non-dominated reads retain original lowering.

The helper removes only the dead entry store. It queries the existing function-analysis manager lazily and preserves the underlying CFG and dominator tree. The default LLVM O2 pipeline, its pass order, original LLVM verifiers, CPU policy, no-SLP setting and runtime-free AOT SIMD boundary are unchanged. Normal lowering, G43 packed validation, Runtime, ABI, precise GC roots, polling, ownership and all escape eligibility remain unchanged. This adds 49 backend lines and an independent memory-semantics test in the existing managed tests. The new test uses a 131-block typed CFG with an early read, a branch-only store and a dominated definition.

## Whole-process compiler measurements

JIT timing is from ordinary processes, with the existing accepted fixtures and independent results. Each shape and mode alternates baseline/candidate order over four matched blocks, except the 750- and 2,000-site nested shapes with two. The exact 2,000-site runs require 7 GiB available, use a 5 GiB address-space cap, preserve 2 GiB live host memory and use a 1 MiB stack with a 480-second timeout. No qualification or build activity overlaps sampling. Shape-specific work and pass diagnostics are separate from these runs.

| Shape | JIT | Wall s | CPU s | Peak RSS KiB | Wall change |
|---|---|---|---|---|---|
| small | jit-normal | 0.014 → 0.014 | 0.013 → 0.013 | 40,606 → 41,222 | +2.6% |
| small | jit-O2 | 0.015 → 0.015 | 0.014 → 0.014 | 42,894 → 42,848 | -0.5% |
| mixed | jit-normal | 0.040 → 0.040 | 0.038 → 0.039 | 46,338 → 46,550 | +1.8% |
| mixed | jit-O2 | 0.051 → 0.051 | 0.049 → 0.049 | 48,970 → 48,828 | +1.2% |
| flat-100 | jit-normal | 0.109 → 0.110 | 0.107 → 0.108 | 51,152 → 50,724 | +0.7% |
| flat-100 | jit-O2 | 0.116 → 0.109 | 0.113 → 0.107 | 50,674 → 50,604 | -5.9% |
| flat-500 | jit-normal | 0.585 → 0.590 | 0.576 → 0.581 | 80,890 → 81,182 | +0.8% |
| flat-500 | jit-O2 | 0.564 → 0.500 | 0.557 → 0.495 | 68,638 → 68,640 | -11.3% |
| flat-2000 | jit-normal | 3.836 → 3.763 | 3.803 → 3.734 | 198,798 → 197,952 | -1.9% |
| flat-2000 | jit-O2 | 4.840 → 2.887 | 4.793 → 2.867 | 140,818 → 140,950 | -40.3% |
| nested-100 | jit-normal | 0.440 → 0.441 | 0.434 → 0.433 | 72,662 → 72,176 | +0.3% |
| nested-100 | jit-O2 | 0.412 → 0.362 | 0.407 → 0.356 | 68,678 → 68,780 | -12.3% |
| nested-500 | jit-normal | 4.674 → 4.596 | 4.632 → 4.564 | 185,632 → 186,004 | -1.7% |
| nested-500 | jit-O2 | 6.768 → 4.516 | 6.703 → 4.483 | 318,198 → 318,026 | -33.3% |
| nested-750 | jit-normal | 11.422 → 11.885 | 11.336 → 11.806 | 258,186 → 258,102 | +4.0% |
| nested-750 | jit-O2 | 15.743 → 7.268 | 15.614 → 7.214 | 487,502 → 487,670 | -53.8% |
| nested-2000 | jit-normal | 80.540 → 80.962 | 80.017 → 80.422 | 611,912 → 612,098 | +0.5% |
| nested-2000 | jit-O2 | 124.938 → 46.892 | 124.069 → 46.592 | 1,323,492 → 1,324,122 | -62.5% |

The matched clock-only runs below are milliseconds. The frontend analysis timer includes source and semantic work. The JIT and LLVM-lowering scopes include their existing validation calls. Native code generation is measured from ORC IR transform entry to its compiled notification. Lookup includes native code generation and object linking. These phases and their subscopes overlap.

| Shape, O2 JIT | IR validation calls | First IR validate ms | LLVM lower ms | Proof ms | LLVM O2 ms | Native codegen ms | Lookup ms |
|---|---|---|---|---|---|---|---|
| small | 3 → 3 | 0.104 → 0.088 | 0.254 → 0.323 | 0.000 → 0.001 | 1.130 → 1.183 | 2.875 → 2.929 | 3.072 → 3.145 |
| mixed | 3 → 3 | 1.075 → 1.020 | 2.282 → 2.126 | 0.000 → 0.002 | 13.060 → 13.475 | 20.614 → 20.949 | 21.169 → 21.522 |
| flat-100 | 3 → 3 | 4.364 → 4.594 | 9.063 → 9.472 | 0.000 → 0.088 | 18.616 → 16.348 | 57.779 → 62.919 | 58.593 → 63.719 |
| flat-500 | 3 → 3 | 21.023 → 22.837 | 46.341 → 49.139 | 0.000 → 0.869 | 167.325 → 96.166 | 260.845 → 279.320 | 263.637 → 282.348 |
| flat-2000 | 3 → 3 | 92.743 → 96.271 | 193.662 → 202.254 | 0.000 → 3.282 | 2658.197 → 959.454 | 1361.162 → 1419.669 | 1378.395 → 1438.366 |
| nested-100 | 3 → 3 | 15.317 → 14.903 | 30.759 → 30.317 | 0.000 → 0.369 | 114.650 → 51.386 | 196.020 → 193.268 | 197.806 → 194.965 |
| nested-500 | 3 → 3 | 81.346 → 83.673 | 162.670 → 163.495 | 0.000 → 3.666 | 2699.639 → 584.027 | 3487.446 → 3400.725 | 3497.630 → 3411.454 |
| nested-750 | 3 → 3 | 119.095 → 121.376 | 245.511 → 245.606 | 0.000 → 6.826 | 9262.126 → 1440.009 | 5215.252 → 5521.488 | 5231.476 → 5537.551 |
| nested-2000 | 3 → 3 | 327.767 → 333.721 | 726.270 → 792.633 | 0.000 → 23.187 | 86431.148 → 8218.122 | 35678.013 → 35323.296 | 35732.554 → 35376.730 |

The pass costs below use exclusive time from LLVM pass callbacks. Analysis invalidations are observed LLVM callbacks. A repeated analysis run can follow invalidation as indicated in the last column.

| Nested 2,000 LLVM work | Invocations | Exclusive wall ms | Exclusive CPU ms | Invalidations |
|---|---|---|---|---|
| SROAPass | 4 → 4 | 83119.357 → 5184.690 | 82582.250 → 5153.050 | 0 → 0 |
| InstCombinePass | 8 → 8 | 900.611 → 845.742 | 893.154 → 840.186 | 0 → 0 |
| SimplifyCFGPass | 8 → 8 | 759.512 → 682.535 | 751.423 → 678.753 | 0 → 0 |
| EarlyCSEPass | 2 → 2 | 190.053 → 183.550 | 188.515 → 182.390 | 0 → 0 |
| JumpThreadingPass | 2 → 2 | 84.937 → 75.256 | 84.148 → 74.829 | 0 → 0 |
| GVNPass | 1 → 1 | 277.836 → 240.388 | 274.932 → 238.734 | 0 → 0 |
| DominatorTreeAnalysis | 5 → 6 | 36.375 → 41.705 | 36.095 → 41.447 | 5 → 5 |
| LoopAnalysis | 2 → 2 | 13.599 → 12.283 | 13.479 → 12.175 | 2 → 2 |
| AAManager | 4 → 4 | 0.034 → 0.034 | 0.034 → 0.034 | 4 → 4 |
| BasicAA | 4 → 4 | 0.020 → 0.019 | 0.018 → 0.018 | 4 → 4 |
| MemorySSAAnalysis | 2 → 2 | 116.643 → 109.758 | 115.696 → 109.074 | 2 → 2 |

The exact large run recorded 5 → 6 dominator-tree computations. The added proof makes an early tree query. The measured invalidation count stayed at 5, and the SROA pass count stayed at four. The speedup comes from cheaper promotion within the existing pass.

| Nested 2,000 IR | Before the proof | After O2 |
|---|---|---|
| functions | 1 → 1 | 1 → 1 |
| blocks | 44,003 → 44,003 | 19,003 → 19,003 |
| edges | 46,002 → 46,002 | 32,003 → 32,003 |
| instructions | 253,028 → 253,028 | 72,020 → 72,020 |
| phis | 0 → 0 | 3,001 → 3,001 |
| allocas | 17,006 → 17,006 | 1,004 → 1,004 |

The allocator observer counts calls and requested byte volume for malloc, calloc, realloc and aligned_alloc inside the O2 pipeline. Volume includes temporary allocations and is separate from resident memory. Pipeline-interval peak RSS comes from the ordinary profiler's process high-water mark at the end of O2. Whole-process peak includes the downstream native stage.

| Shape | Removed stores | O2 allocation requests | Requested bytes | Through O2 peak RSS KiB |
|---|---|---|---|---|
| nested-500 | 3,751 | 244,359 → 191,881 | 1,206,280,008 → 164,785,272 | 97,644 → 97,620 |
| nested-750 | 5,626 | 374,470 → 285,046 | 3,479,080,331 → 356,560,347 | 128,524 → 128,604 |

The exact nested 2,000-site candidate removed 15,001 dead entry stores. The full LLVM O2 pipeline still ran. Peak RSS through O2 was 287,728 → 288,048 KiB.

## Native code generation, linking and executable memory

The selected ORC linker's callbacks measure graph setup, target/pruning work, memory allocation and address assignment, external symbol resolution, relocation and finalization. External resolution includes content preparation and callback overhead between the applicable JITLink phases. The lookup timer also includes uninstrumented ORC coordination and LLVM context disposal. Materialization is synchronous on the first lookup. Warm and timed invocations check the already-materialized entry, with no observed lazy compile on first call.

| ORC phase, ms | Nested 750 normal | Nested 2,000 O2 |
|---|---|---|
| jit_platform_setup | 1.025 → 1.038 | 0.949 → 1.011 |
| jit_register_runtime_symbols | 0.204 → 0.166 | 0.183 → 0.182 |
| jit_generation_and_bind | 0.110 → 0.077 | 0.077 → 0.068 |
| jit_admit_module | 0.780 → 0.766 | 1.099 → 0.991 |
| jit_native_codegen | 10040.050 → 9861.777 | 35678.013 → 35323.296 |
| link_object_graph_parse | 1.573 → 1.409 | 1.759 → 1.778 |
| link_graph_setup | 0.036 → 0.033 | 0.044 → 0.041 |
| link_prune_and_targets | 0.352 → 0.309 | 0.352 → 0.350 |
| link_allocation_and_addresses | 1.211 → 0.961 | 1.155 → 1.148 |
| link_external_resolution | 0.158 → 0.095 | 0.111 → 0.098 |
| link_relocations | 0.256 → 0.210 | 0.318 → 0.300 |
| link_finalize | 0.093 → 0.059 | 0.075 → 0.069 |
| jit_lookup_and_materialize | 10067.960 → 9886.326 | 35732.554 → 35376.730 |
| jit_runtime_metadata | 0.087 → 0.051 | 0.035 → 0.038 |

| Nested 2,000 O2 JIT artifact | G43 → G44 |
|---|---|
| Native object bytes | 2,678,688 → 2,678,688 |
| LinkGraph content bytes | 2,677,190 → 2,677,190 |
| Object graph relocations | 16,033 → 16,033 |
| Entry executable mapping bytes | 528,384 → 528,384 |

The entry mapping was executable without write permission in both captures. Its size is the mapping containing the entry address. Object and graph sizes are separate from the mapped page total.

## Independent AOT and emitted behavior

Both normal and optimized AOT object emission were measured separately from Clang/LLD linking. Nested AOT executables independently check six inputs across all three distinct branch outcomes. The two variants link the same Runtime and use the existing generic deployment target.

| Shape | AOT | Object wall s | Object CPU s | Peak RSS KiB | Link wall s | Object bytes (each) | Executable bytes (each) |
|---|---|---|---|---|---|---|---|
| small | normal | 0.017 → 0.017 | 0.015 → 0.015 | 40,666 → 41,296 | 0.029 → 0.028 | 4,208 | 8,392 |
| small | optimized | 0.016 → 0.016 | 0.015 → 0.015 | 42,378 → 42,604 | 0.028 → 0.028 | 3,456 | 7,896 |
| nested-500 | normal | 4.271 → 4.294 | 4.241 → 4.260 | 175,880 → 175,076 | 0.037 → 0.036 | 1,423,488 | 4,214,736 |
| nested-500 | optimized | 6.647 → 4.294 | 6.584 → 4.266 | 310,882 → 310,944 | 0.035 → 0.038 | 657,288 | 3,586,168 |
| nested-750 | normal | 10.157 → 11.412 | 10.082 → 11.336 | 242,682 → 240,560 | 0.039 → 0.038 | 2,133,896 | 4,782,848 |
| nested-750 | optimized | 15.621 → 7.217 | 15.496 → 7.166 | 477,474 → 477,508 | 0.040 → 0.038 | 992,448 | 3,847,672 |

The first two normal AOT pairs at 750 sites measured 10.157 → 11.412 seconds. Two further pairs, starting in the reverse order, measured 10.884 → 10.571 seconds. The pooled median is 10.368 → 10.831 seconds across four pairs. The objects were byte-identical in every sample and the new proof runs only in optimized mode. These timings are retained as observed compiler wall changes.

All 132 matched escape LLVM IR pairs and 132 native object pairs are byte-identical. All 264 ordinary/observer objects agree. The 18 managed representative objects, the normal representative AOT executable and the mixed-module objects/executables match. The exact nested 500- and 2,000-site O2 JIT captures match before and after LLVM and in the native object. Each paired independent AOT scaling image matches its corresponding object and executable. Four G43 CLI diagnostic cases produced identical exit status, standard output and standard error.

The optimized representative AOT executable changed from 4,999,176 to 4,999,168 bytes with identical .text size. The profile and production objects match within each variant. Of 122 matching disassembly symbols, two functions have changes. Earlier scalar promotion changes private spill-home assignment in `collections.sorted`, and the equality comparison still reaches the same conditional branch. In `text.join`, the two independent scalar counter updates exchange order before the same safepoint. Mnemonic counts and relocation targets match in both functions, and the selected control flow, calls and failures are preserved. The standalone normal and optimized AOT oracles exercise empty inputs, duplicate sort keys, a Unicode join with an empty item and a late failure. Their success outputs, failure status and diagnostic bytes agree. The balanced sort and text timings below include this changed optimized image.

All G38 native loop witnesses are identical byte for byte in both AOT modes. Optimized invariant-division and variable-divisor functions retain packed SSE2 arithmetic. Bidirectional, dependent, late-failure and strict f32/f64 controls retain their scalar signatures. The hardware feature baseline has no new AVX or FMA requirement. The C++ AOT tests preserve integer wrap, endpoint and failure oracles and 240 strict floating-bit checks per mode. Existing proofs still cover callbacks, reload, checked numeric/collection behavior, foreign-Heap rejection, precise roots and cancellation.

## Qualification and runtime controls

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed 86/86 plus five diagnostics. Native Windows 10 build 19045 Debug and Release each passed 71/71, with 3,319 verified source inputs. Fatal TSan passed 66 fixtures in each of two independent AOT modes plus Runtime coordination, race and allocator controls. All 216,000 forced-GC checks passed. Linux and Windows 1 MiB stack controls, normal/O2 JIT, independent normal/optimized AOT, allocation-failure recovery, callback, reload, worker, async, G43 validation and G35–G42 ownership controls passed.

All 264 paired allocation/GC comparisons match across eight counters with three samples per case. Observer-on data retains root, marking, allocation and complete pause observations separately from throughput. Runtime source and the Runtime archive are byte-identical. The runtime and peak memory results retain the observed variation of the matched execution images.

Across 32 additional representative pairs, managed construction, root search, live payload and collection counts also match. One normal JIT JSON sample differed by one live native allocation and eight requested bytes at the final observation. The requested allocation counts matched. This difference is retained in the raw receipt.

| Representative | app-normal | app-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| array-1024 | +0.3% | -1.1% | +1.1% | +2.0% |
| array-4096 | -0.5% | -1.7% | +1.5% | +4.5% |
| dependent-1000000 | -6.5% | -6.4% | +0.4% | +1.6% |
| dict-1024 | -1.6% | -6.3% | +0.3% | +1.4% |
| float-1000000 | -6.3% | -6.3% | +0.0% | +0.9% |
| integer-1000000 | -6.9% | -7.5% | -0.9% | +0.7% |
| integer-4000000 | -6.0% | -7.3% | +0.7% | +2.2% |
| io-200 | +0.3% | -1.9% | -1.0% | +1.1% |
| json-128 | +1.0% | -2.8% | +1.0% | -0.2% |
| json-32 | +0.5% | -2.4% | -0.9% | -0.0% |
| sort-1024 | +0.8% | -3.0% | +0.5% | +1.7% |
| sort-256 | +0.4% | -3.7% | -0.2% | +4.2% |
| text-1024 | +0.3% | -2.5% | -0.9% | +2.1% |
| text-256 | -0.8% | -3.0% | -1.8% | +2.2% |

| Managed | jit-normal | jit-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| async-tasks | +3.0% | +5.1% | +0.5% | +2.4% |
| calls | -7.2% | -6.4% | -0.4% | -0.8% |
| closures | -2.2% | -3.0% | +0.7% | -6.2% |
| managed-across-edge | -7.1% | -2.3% | +0.0% | +0.3% |
| managed-escape | +2.7% | +6.5% | +1.0% | -1.5% |
| managed-local | -7.5% | -8.0% | +0.3% | -0.9% |
| managed-reference | -8.1% | -9.3% | +0.7% | -0.7% |
| managed-survives-allocation | -4.1% | -4.0% | -0.0% | +0.0% |
| workers | +0.9% | -6.9% | +0.5% | -3.5% |

Largest adverse elapsed observations among ordinary escape controls remain in the table. Byte-identical code permits no claim of reduced generated work.

| Mode | Case | Elapsed delta |
|---|---|---|
| jit-normal | context_control | +7.7% |
| jit-normal | public_helper | +6.8% |
| jit-O2 | branch_alias | +4.3% |
| jit-O2 | alias_rebind | +3.6% |
| aot-normal | direct_null_failure | +21.2% |
| aot-normal | context_control | +6.0% |
| aot-optimized | direct_null_failure | +3.3% |
| aot-optimized | direct_factory | +3.1% |

| Warm-filesystem startup mode | Wall ms | CPU ms | Peak RSS KiB |
|---|---|---|---|
| aot-normal | 2.428 → 2.323 | 2.107 → 2.001 | 15,224 → 15,224 |
| aot-optimized | 2.117 → 2.163 | 1.814 → 1.825 | 15,224 → 15,224 |
| jit-O2 | 21.436 → 21.016 | 20.648 → 20.267 | 46,952 → 47,218 |
| jit-normal | 19.544 → 19.073 | 18.737 → 18.377 | 44,836 → 45,164 |
| representative-aot | 2.939 → 2.968 | 2.619 → 2.666 | 15,224 → 15,224 |
| representative-aot-normal | 3.050 → 3.075 | 2.719 → 2.708 | 15,224 → 15,224 |

## Rejected attempts and next objective

The first implementation excluded all functions with a debug subprogram. That excluded historical JIT line/type metadata and removed zero stores on the workload. Slot metadata and assignment-tracking guards now protect actual debug homes while preserving the source line tables. The first evidence-only symbol parser used the wrong names for the two strict FP entries and assumed the varying-condition witness lacked existing packed instructions. The corrected comparison checks the unchanged baseline object and the selected scalar witnesses. The evidence setup also needed the archived managed emitter and a missing Python import before completing the representative builds. The successful Release, strict and Windows qualification runs continued independently. The first small-source timing exposed a null Runtime owner dereference in the profiling host for a runtime-free program. The evidence host now uses empty snapshots on that path. The failed sample is retained and scaling resumed with the completed runtime and startup blocks preserved.

Hardware PMU and DWARF call-stack profiling were unavailable under the host's profiling permissions. The supported direct-child software IP sample succeeded. The first replay also called a timing executable absent from the host. Replaying through wait4 corrected that setup. The default vs early-mem2reg replay took 3.786 vs 4.879 seconds. An early mem2reg pass leaves the duplicate definitions in place and was excluded. These replay results only explain the diagnosis. The matched JIT and independent AOT measurements above are whole-compiler results.

Native code generation is still the primary JIT cost at the exact scale. In the separate nested-750 normal JIT IP sample, `MachineRegisterInfo::clearKillFlags` and `MachineSinking::run` accounted for 31.89% and 24.02% of self samples respectively. The next concrete action is to time MachineSinking on the exact 2,000-site O2 JIT function and count kill-flag walks and visited register uses per virtual register against machine IR size. Call-chain or pass instrumentation is needed to attribute the flag walks to their callers before changing any policy. Remaining Language Bible work includes checked collections and dictionaries/sorting, text/Unicode/JSON, broader ownership and GC tails, SIMD beyond G38, native/JIT and AOT overhead, startup, and macOS/release qualification. G37-F remains performance-held and the G41 zero-run bridge remains rejected.

## Evidence and source inventory

The archive includes exact source patches, preimages, postimages, toolchain identities, pass counters, timing and allocation data, selected LLVM IR/native objects and disassembly, independent oracles, negative and failure receipts, native qualification and replay instructions. Source restoration and every CRC/SHA-256 record are checked before task-only cleanup. Separate closeout files capture host and guest observed reclamation, source identities and final git status.

- `src/backend/llvm/backend.cpp`
- `tests/managed/tests.cpp`
- `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`
- `docs/compiler-health/G44_LLVM_JIT_SCALING.md`

HEAD remains the accepted G43 parent. The index remains empty. No commit, push, force-push, history rewrite or hosted CI.
