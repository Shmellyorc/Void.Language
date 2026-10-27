# Stabilization measurements

All controlled child processes use a verified 1 MiB stack. Times include process creation and child pre-exec stack setup. Values are wall seconds / CPU seconds / peak RSS MiB, unless stated otherwise. Cold compilation means a fresh process with persistent object reuse bypassed or absent; the OS page cache is not flushed. Each controlled measurement runs serially without overlapping compiler builds or acceptance tests. The desktop and normal background services remain active; load averages and scheduling/fault counters are retained. These are local measurements, not universal CI thresholds.

Frozen Release baseline CLI: `3342a2153fc909a98111207db82a8c7b2736983a5311e33a4f68369ec391a952`. Final build/source/tool identities are in [evidence manifest](evidence/manifest.json). Same source bytes and pinned LLVM 19.1.7 CPU/features/Default native codegen are used on each side. Baseline ordinary runs were uncached; final uncached runs explicitly pass `--no-cache`. `--no-opt` disables IR O2 only.

## Cold DTO ladder

| Width | Policy | Baseline replay wall / CPU / RSS | Final wall / CPU / RSS | Wall speedup |
|---|---|---|---|---|
| 8 | No IR O2 | 14.242 / 14.112 / 214.8 | 2.640 / 2.622 / 91.4 | 5.4× |
| 8 | IR O2 | 25.001 / 24.803 / 237.7 | 3.474 / 3.450 / 89.2 | 7.2× |
| 16 | No IR O2 | 20.463 / 20.264 / 317.9 | 2.782 / 2.765 / 93.0 | 7.4× |
| 16 | IR O2 | 36.795 / 36.476 / 365.0 | 3.611 / 3.588 / 92.2 | 10.2× |
| 32 | No IR O2 | 30.911 / 30.624 / 522.1 | 3.225 / 3.192 / 98.1 | 9.6× |
| 32 | IR O2 | 59.687 / 59.096 / 582.6 | 4.072 / 4.045 / 96.0 | 14.7× |
| 64 | No IR O2 | 63.961 / 63.297 / 916.6 | 4.063 / 4.031 / 111.8 | 15.7× |
| 64 | IR O2 | 121.449 / 120.250 / 1034.4 | 5.241 / 5.199 / 106.0 | 23.2× |
| 128 | No IR O2 | 127.656 / 126.247 / 1836.5 | 6.337 / 6.287 / 142.2 | 20.1× |
| 128 | IR O2 | 297.021 / 294.379 / 1839.8 | 8.599 / 8.524 / 134.3 | 34.5× |

DTO128 optimized uncached repeats: 8.598708, 8.609251, 8.148849 seconds; median 8.598708. Original research reported 309.755324 seconds / 1,913,040 KiB RSS. Replay drift and final variation remain visible in raw receipts.

## Application startup and cache workflow

| Workload | Baseline uncached wall / CPU / RSS | Final uncached wall / CPU / RSS | O2 cache miss | O2 cache hit median (3) | AOT process median (3) |
|---|---|---|---|---|---|
| tiny | 0.022 / 0.021 / 44.4 | 0.021 / 0.020 / 43.6 | 0.034 | 0.009 | 0.004 |
| gameplay | 0.073 / 0.071 / 48.9 | 0.062 / 0.060 / 49.4 | 0.083 | 0.016 | 0.004 |
| import-json | 10.307 / 10.209 / 170.0 | 0.227 / 0.224 / 61.7 | 0.246 | 0.083 | 0.004 |
| import-text | 1.689 / 1.676 / 62.0 | 0.066 / 0.064 / 48.4 | 0.078 | 0.025 | 0.004 |
| json-save | 39.750 / 39.384 / 230.8 | 6.255 / 6.200 / 113.1 | 6.255 | 0.167 | 0.013 |
| random-gameplay | 1.444 / 1.434 / 71.5 | 0.443 / 0.438 / 59.8 | 0.456 | 0.066 | 0.005 |
| globals-game | 0.044 / 0.043 / 45.0 | 0.037 / 0.036 / 45.0 | 0.052 | 0.009 | 0.005 |

All application stdout is checked against the baseline oracle. Default optimized and no-IR-O2 cold/warm objects are measured separately; all no-IR-O2 and AOT build/run values are in the raw ledger. Cache hits still perform full source analysis. Native process times include initialization, application execution and teardown.

| Real source edit (added local) | Optimized edit-run miss | Next unchanged run |
|---|---|---|
| tiny | 0.035 | 0.013 |
| dto32 | 4.239 | 0.149 |

## Preserved DTO fixture profile matrix

| Profile | Width | Policy | JIT wall / CPU / RSS | AOT build wall / CPU / RSS | Native process wall |
|---|---|---|---|---|---|
| release | 8 | No IR O2 | 2.640 / 2.622 / 91.4 | 2.447 / 2.444 / 87.5 | 0.011769 |
| release | 8 | IR O2 | 3.474 / 3.450 / 89.2 | 3.393 / 3.374 / 85.9 | 0.008628 |
| release | 16 | No IR O2 | 2.782 / 2.765 / 93.0 | 2.625 / 2.622 / 90.5 | 0.013386 |
| release | 16 | IR O2 | 3.611 / 3.588 / 92.2 | 3.539 / 3.525 / 88.0 | 0.018981 |
| release | 32 | No IR O2 | 3.225 / 3.192 / 98.1 | 2.955 / 2.949 / 95.2 | 0.031104 |
| release | 32 | IR O2 | 4.072 / 4.045 / 96.0 | 3.793 / 3.782 / 92.8 | 0.031110 |
| release | 64 | No IR O2 | 4.063 / 4.031 / 111.8 | 3.625 / 3.614 / 107.6 | 0.075469 |
| release | 64 | IR O2 | 5.241 / 5.199 / 106.0 | 4.725 / 4.706 / 103.1 | 0.069833 |
| release | 128 | No IR O2 | 6.337 / 6.287 / 142.2 | 5.378 / 5.356 / 150.9 | 0.265069 |
| release | 128 | IR O2 | 8.599 / 8.524 / 134.3 | 7.447 / 7.396 / 132.4 | 0.267702 |
| debug | 8 | No IR O2 | 7.229 / 7.184 / 90.8 | 6.021 / 6.006 / 97.3 | 0.035807 |
| debug | 8 | IR O2 | 8.270 / 8.214 / 89.5 | 7.077 / 7.059 / 96.8 | 0.038533 |
| debug | 16 | No IR O2 | 7.535 / 7.488 / 92.4 | 6.333 / 6.333 / 97.3 | 0.075253 |
| debug | 16 | IR O2 | 8.727 / 8.668 / 90.8 | 7.362 / 7.360 / 96.8 | 0.071503 |
| debug | 32 | No IR O2 | 8.414 / 8.362 / 98.9 | 6.848 / 6.833 / 97.7 | 0.162346 |
| debug | 32 | IR O2 | 9.557 / 9.484 / 95.9 | 7.887 / 7.887 / 97.2 | 0.166940 |
| debug | 64 | No IR O2 | 10.183 / 10.118 / 112.5 | 8.132 / 8.117 / 107.4 | 0.434254 |
| debug | 64 | IR O2 | 11.771 / 11.698 / 106.9 | 9.796 / 9.764 / 103.4 | 0.446329 |
| debug | 128 | No IR O2 | 16.275 / 16.111 / 141.3 | 12.056 / 12.017 / 149.5 | 1.470650 |
| debug | 128 | IR O2 | 18.642 / 18.535 / 134.1 | 14.398 / 14.359 / 132.7 | 1.471586 |
| san | 8 | No IR O2 | 7.733 / 7.681 / 878.2 | 6.670 / 6.699 / 957.5 | 0.063468 |
| san | 8 | IR O2 | 9.232 / 9.152 / 967.3 | 8.185 / 8.179 / 1077.4 | 0.056828 |
| san | 16 | No IR O2 | 8.026 / 7.971 / 828.2 | 6.847 / 6.885 / 962.9 | 0.094718 |
| san | 16 | IR O2 | 9.331 / 9.273 / 966.4 | 8.210 / 8.237 / 1099.8 | 0.087091 |
| san | 32 | No IR O2 | 8.735 / 8.679 / 807.9 | 7.757 / 7.776 / 959.1 | 0.202886 |
| san | 32 | IR O2 | 10.503 / 10.427 / 1276.0 | 9.146 / 9.154 / 1080.5 | 0.192466 |
| san | 64 | No IR O2 | 10.924 / 10.855 / 1026.6 | 8.868 / 8.888 / 818.4 | 0.497263 |
| san | 64 | IR O2 | 13.237 / 13.136 / 1353.2 | 11.362 / 11.335 / 1170.4 | 0.495979 |
| san | 128 | No IR O2 | 18.085 / 17.966 / 1022.1 | 13.599 / 13.585 / 1227.0 | 1.637151 |
| san | 128 | IR O2 | 21.920 / 21.775 / 1405.5 | 17.668 / 17.585 / 1528.1 | 1.617071 |

Every compiler/native child in this matrix passed with exact expected stdout on 1 MiB. Debug/sanitizer matrix samples are current-characterization runs; no unmeasured baseline-profile speedup is claimed.

## Frontend arithmetic traversal

| Compiler | Identical fixture | Check wall median (3) |
|---|---|---|
| baseline | anchor_chain_left20 | 1.518449 |
| baseline | anchor_chain_balanced20 | 0.007915 |
| final | anchor_chain_left20 | 0.008038 |
| final | anchor_chain_balanced20 | 0.009295 |

## Emitted LLVM size and root descriptors

| Workload | Compiler | Pre functions | Pre instructions | Post instructions | Largest post block | Max Slot descriptors | Max pre-IR alloca bytes |
|---|---|---|---|---|---|---|---|
| import-json | baseline | 64 | 73505 | 37635 | 5880 | 536 | 25435 |
| import-json | final | 2 | 138 | 103 | 22 | 2 | 188 |
| dto8 | baseline | 73 | 100124 | 85330 | 9129 | 865 | 38228 |
| dto8 | final | 62 | 52668 | 39096 | 1572 | 173 | 13819 |
| dto16 | baseline | 73 | 113260 | 98338 | 15649 | 1481 | 65404 |
| dto16 | final | 62 | 54604 | 40208 | 1572 | 173 | 13819 |
| dto32 | baseline | 76 | 140880 | 125513 | 27515 | 2595 | 114324 |
| dto32 | final | 65 | 59285 | 42897 | 1572 | 173 | 13819 |
| dto64 | baseline | 76 | 194512 | 178432 | 52315 | 4931 | 217140 |
| dto64 | final | 65 | 67669 | 47644 | 1572 | 173 | 13819 |
| dto128 | baseline | 76 | 301776 | 284226 | 101915 | 9603 | 422772 |
| dto128 | final | 65 | 84437 | 57182 | 1572 | 173 | 13819 |

| Workload | Compiler | Max registered reference words | Total registered reference words |
|---|---|---|---|
| dto8 | baseline | 948 | 6660 |
| dto8 | final | 659 | 4911 |
| dto16 | baseline | 1628 | 7596 |
| dto16 | final | 659 | 5007 |
| dto32 | baseline | 2870 | 9550 |
| dto32 | final | 659 | 5281 |
| dto64 | baseline | 5462 | 13358 |
| dto64 | final | 659 | 5729 |
| dto128 | baseline | 10646 | 20974 |
| dto128 | final | 826 | 6625 |
| import-json | baseline | 659 | 4815 |
| import-json | final | 9 | 9 |

Slot descriptors are not reference-word counts. Pointer homes share exact offset maps; every reference word and original safepoint remains. Compound layouts keep individual descriptors. Pre-IR alloca bytes are an inventory, not the actual native stack size. Native `.stack_sizes`, disassembly and section-size receipts provide the machine evidence. Full checking/typed-IR validation precedes entry emission pruning. Embedding and `emit` defaults retain all exports.

## Phase, native and Runtime evidence

The raw phase/helper receipts separate source analysis, typed lowering plus its validation, explicit validation, LLVM construction, IR verification/O2, JIT creation/materialization and first/second invocation. External `opt -time-passes` and `llc -time-passes` process the preserved IR. Native machine passes use PIC, Default/O2 codegen, the same CPU and feature string. Dumping and inventory are timed separately. Helper backend code is compiled at O1 on both sides and has extra instrumentation, so helper wall totals are not substituted for production CLI latency.

Final API allocation counters cover root-thread C++ `operator new` calls during each invocation, not arbitrary malloc/all-thread allocation. Quiescent collection and metadata snapshots follow both invocations. Synthetic Runtime receipts isolate frame entry/leave, positional root lookup, scalar layouts, buffer reads/copies, heap-size effects and allocation counts. The existing async retention matrix verifies frame retirement, stable settled catalogs and complete root drain; it does not claim Task/Work records retire before root drain. The Runtime archive is unchanged.

The [audit interpretation](AUDIT.md) records phase attribution, runtime behavior, machine code and remaining costs. Raw commands, status, wall/CPU/RSS, hashes, stdout/stderr, stack limits, faults and scheduling counters are in [measurements.jsonl](evidence/measurements.jsonl). The complete preserved IR and binaries remain in the frozen local evidence tree named in the manifest.

