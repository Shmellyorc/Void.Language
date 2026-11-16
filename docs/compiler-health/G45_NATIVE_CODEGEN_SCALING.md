# G45 — Native code generation and MachineSinking scalability

**Decision — HOLD WITH EVIDENCE.** The Linux LLVM experiment reduced median O2 JIT compilation of the exact G44 2,000-site nested source from 46.535 to 29.255 seconds (37.1%) with byte-identical LLVM IR and native JIT objects. The accepted toolchain remains LLVM/Clang/LLD 23.1.4. The patch changes a native LLVM pass and remains a separate experiment for review.

The starting commit and live remote `main` were `894e2e8d9930028f86737092d3ac74010676f389` (G44). The isolated branch is `codex/g45-native-codegen-scaling`. G45 modifies this report and the cumulative ledger. The qualified VOID backend and validator SHA-256 values remain `88a2e505bd1b70de3ce8fd48970a63ec02a3eb8f9a1edb92c7e14ac4741a43e9` and `27e38539120620956142fbdf4f3acf314455beb5cb7abbffe4f166b3fa2b5a86`. G42 ownership, G43 validation, G44 scalar-store elimination, G38 automatic SIMD, precise GC, Runtime, the public ABI and all existing LLVM passes remain at the accepted baseline. G37/G37-F stays performance-held and the G41 zero-run bridge stays rejected.

## What the compiler spent time doing

The first run replayed the exact archived G44 optimized LLVM IR through installed `llc -O2 -mcpu=native` under a 1 MiB native stack, a 5 GiB address-space limit and a 2 GiB host reserve. It took 35.539 s wall and 35.284 s CPU at 1,181,896 KiB peak RSS. LLVM's native pass timings accounted for 34.886 s wall and 34.640 s CPU. The replay isolates code generation from VOID source analysis, LLVM O2, ORC materialization and program execution. Its target-machine setup and object identity differ from the embedded JIT. The matched VOID JIT measurements below use the actual ORC path.

| Native pass on the exact archived IR | Installed wall (s) | Installed CPU (s) |
| --- | ---: | ---: |
| Machine code sinking | 17.399 | 17.285 |
| Live DEBUG_VALUE analysis | 4.499 | 4.459 |
| Register coalescer | 4.235 | 4.211 |
| Greedy register allocator, second invocation | 4.134 | 4.100 |
| X86 DAG instruction selection | 1.155 | 1.146 |
| Live interval analysis | 0.912 | 0.907 |
| Live variable analysis | 0.522 | 0.519 |
| Post-register-allocation scheduler | 0.238 | 0.237 |
| Machine instruction scheduler | 0.121 | 0.121 |
| X86 assembly/object printer | 0.096 | 0.096 |
| Virtual register rewrite | 0.043 | 0.043 |
| Machine block placement | 0.042 | 0.042 |
| Prologue, epilogue and frame finalization | 0.031 | 0.030 |

The installed Clang revision identifies LLVM `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. `MachineSinking::run` sweeps machine blocks in layout order and repeats as long as a change occurs. Its bottom-up block walk checks target and movement legality, finds destinations, moves instructions and defers critical-edge splitting to the end of a sweep. The exact nested CFG exposes a sequence of dependent moves across the sweep order. The block, edge and instruction counts were identical at pass entry and exit in the instrumented O2 replay. Processing reachable blocks in CFG reverse postorder reduced both the rounds and actual block visits. On the exact 500-site replay the number of successful moves stayed at 248,999 and there were zero edge splits. The stock traversal made 7,696,670 sinking attempts and sent 62,001 moves to destinations it had already visited that round. Reverse postorder made 308,591 attempts and directed zero moves to already visited destinations. [Pinned LLVM MachineSink.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/CodeGen/MachineSink.cpp).

| Exact archived O2 replay | Machine blocks | CFG edges | Instructions | Virtual registers | Sweeps installed → experiment | Visits installed → experiment |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Nested 500 | 4,753 | 8,003 | 34,806 | 14,521 | 251 → 2 | 1,193,003 → 9,506 |
| Nested 2,000 | 19,003 | 32,003 | 139,056 | 58,021 | 1,001 → 2 | 19,022,003 → 38,006 |

The observer counts all six static `clearKillFlags` sites inside MachineSinking. Trivial forward coalescing and the four sink-and-fold repair sites registered zero calls on both archived O2 shapes. The deferred set registered 497 calls and 16,520 use visits at 500 sites, or 1,997 calls and 66,020 use visits at 2,000 sites. At 2,000 sites the one no-register sentinel walk accounted for 64,024 visits, each of 1,996 virtual registers accounted for one visit, and zero operands had a kill flag. The observed clear loop used 4.61 ms thread CPU, compared with 19.92 s for the instrumented sinking pass. The RPO experiment retained those counts. [Pinned LLVM MachineRegisterInfo.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/CodeGen/MachineRegisterInfo.cpp).

## Isolated correction

The experiment changes one LLVM translation unit. It visits the existing `MachineFunction` in reachable CFG reverse postorder on each fixed-point sweep. It uses LLVM's existing traversal utility and recomputes the order after the original edge-splitting phase when another sweep is needed. The existing movement legality, target hooks, pressure checks, alias state, debug handling, deferred kill set, liveness maintenance and convergence rule still execute. The pass traverses without reordering the existing block list. The LLVM machine verifier passed the installed and alternate 500-site replays, which emitted identical objects. Installed LLVM already skips unreachable blocks in `ProcessBlock`.

The traversal adds one DFS and order buffer per sweep. It removes almost all sweeps on the nested shapes. The same LLVM source file built without the traversal change reproduced the installed standalone replay object at 500 sites. The alternate object is linked ahead of the installed static libraries in task-owned Linux test executables. The overlay uses the accepted target configuration, optimization level, native CPU features and LLD. The qualification archive includes the pinned original LLVM file, the experimental patch, independent build recipe, compiler identities and generated code.

The standalone target-machine replay reproduced identical installed, counted and experimental output at 500 sites (651,184 bytes, SHA-256 `b68b172ea271ac0acac54313a81c330e87f387c5f622ffa07499ed0b6ff4a414`). At 2,000 sites the counted installed replay and both experimental replays produced SHA-256 `a98a3434babf636f638e58f801ac6955e4990b580537e3e10f2aaf4fda85079a`. The real embedded O2 JIT emitted the same 2,678,688-byte object in both variants, SHA-256 `49c873f5517f7aa13d6785e9c5bf9cc309ffc4ebb6aa9169be4ced85ca4d485d`. The profiled variants also matched IR before and after LLVM O2.

## Whole-source compilation and native output

The table reports medians of two ordinary AB/BA pairs per shape and mode. Flat 2,000 normal JIT includes four additional alternating pairs. The third matched profile pair per JIT shape captures LLVM IR, objects and phase data. Each graph fixture runs its independent result oracle once before the three checked timings after compilation. AOT measurements cover independent normal and optimized emissions and omit the separately recorded LLD link time. The independently linked nested AOT executables checked inputs 0, 1, 2, 3, 6 and -1. All eight AOT scaling workload/mode pairs matched object and executable bytes, with median independent LLD link times around 29–40 ms. At nested 500 the separate phase run measured normal native emission at 4.029 → 2.889 s and optimized native emission at 3.346 → 2.615 s. The optimized AOT IR pipeline was 0.568 → 0.592 s.

| Pipeline | Source | Mode | Installed (s) | Overlay (s) | Wall change |
| --- | --- | --- | ---: | ---: | ---: |
| JIT | small | normal | 0.015 | 0.014 | -4.2% |
| JIT | mixed | normal | 0.039 | 0.039 | +1.1% |
| JIT | flat-100 | normal | 0.110 | 0.110 | +0.2% |
| JIT | flat-500 | normal | 0.595 | 0.604 | +1.6% |
| JIT | flat-2000 | normal | 3.858 | 4.213 | +9.2% |
| JIT | nested-100 | normal | 0.422 | 0.399 | -5.3% |
| JIT | nested-500 | normal | 4.660 | 3.366 | -27.8% |
| JIT | nested-750 | normal | 11.221 | 8.265 | -26.3% |
| JIT | nested-2000 | normal | 81.579 | 57.530 | -29.5% |
| JIT | small | O2 | 0.015 | 0.014 | -9.4% |
| JIT | mixed | O2 | 0.051 | 0.052 | +1.6% |
| JIT | flat-100 | O2 | 0.114 | 0.112 | -1.0% |
| JIT | flat-500 | O2 | 0.513 | 0.515 | +0.3% |
| JIT | flat-2000 | O2 | 2.948 | 3.027 | +2.7% |
| JIT | nested-100 | O2 | 0.344 | 0.349 | +1.3% |
| JIT | nested-500 | O2 | 4.554 | 3.665 | -19.5% |
| JIT | nested-750 | O2 | 7.773 | 5.009 | -35.6% |
| JIT | nested-2000 | O2 | 46.535 | 29.255 | -37.1% |
| AOT | small | normal | 0.016 | 0.019 | +17.3% |
| AOT | flat-500 | normal | 0.553 | 0.572 | +3.5% |
| AOT | nested-500 | normal | 4.443 | 3.031 | -31.8% |
| AOT | nested-750 | normal | 10.783 | 8.298 | -23.0% |
| AOT | small | optimized | 0.016 | 0.016 | -3.9% |
| AOT | flat-500 | optimized | 0.471 | 0.482 | +2.4% |
| AOT | nested-500 | optimized | 4.528 | 3.528 | -22.1% |
| AOT | nested-750 | optimized | 7.386 | 4.967 | -32.8% |

At exact 2,000-site O2 JIT scale the ordinary process CPU median fell from 46.244 to 29.040 s. Peak RSS was 1,323,692 → 1,323,972 KiB. No meaningful memory reduction was observed. The matched profiler gave the following wall times.

| Phase | Installed | Experiment |
| --- | ---: | ---: |
| Source frontend and project analysis | 451.61 ms | 449.68 ms |
| Three definite-initialization/IR validation calls, total | 999.99 ms | 999.47 ms |
| LLVM lowering, including its validation | 737.49 ms | 736.60 ms |
| LLVM O2 pipeline | 8.444 s | 8.264 s |
| Native code generation | 37.878 s | 17.108 s |
| ORC lookup and materialization, including native code generation | 37.932 s | 17.162 s |
| Object graph parse | 1.764 ms | 1.966 ms |
| ORC address allocation | 1.131 ms | 1.122 ms |
| Relocations | 0.301 ms | 0.311 ms |

The sum of the seven measured link phases stayed below 4 ms in each result. Both JITLink graphs recorded 21 blocks, 5,015 definitions, 16,033 relocations and 2,677,190 graph bytes. The executable entry mapping was 528,384 bytes for both. The byte-identical JIT images preserve code, read-only data and unwind data. Both O2 objects contain 526,097 text bytes, 1,386,542 read-only bytes across the two read-only sections and 104 unwind bytes. The exact O2 entry reserves 0x7d88 bytes after six saved-register pushes. The allocation-observer-only 500-site O2 compiler runs each recorded 191,881 LLVM pipeline allocation requests and 164,785,272 requested bytes. These are compile-time IR pipeline counts. MachineSinking's observed temporary count is the sweep/visit count, and no pass-only allocator metric is inferred from it.

The exact normal JIT object is also identical at 5,723,224 bytes, SHA-256 `32131b6e8d76d7b4410fed4d65e89c1ee6e7a8dfbdb64133345529d950b4f78b`. Normal process CPU medians were 81.018 → 57.191 s and peak RSS was 613,078 → 612,392 KiB. The separate normal profile measured native generation at 79.776 → 55.580 s. The JIT entry occupied a 974,848-byte executable mapping in both. For all 18 JIT shape/mode pairs the captured LLVM IR and object bytes match.

The real O2 JIT observer confirmed 19,022,003 → 38,006 MachineSinking block visits on the exact large function. The live ORC target had 139,048 instructions and 49,014 virtual registers at the pass, with 80,020 use operands visited by 1,997 deferred kill calls and zero flags cleared. The standalone replay retained a different debug and target-machine shape. At nested 750 normal JIT the same whole-pipeline instrumentation counted 5,500,128 → 29,256 visits (376 → 2 sweeps), for 14,628 blocks, 15,377 edges, 138,810 instructions and 36,767 virtual registers. Its MachineSinking kill repair consisted of one no-register walk of 88,161 operands. The actual observer variants retained the matched native object hashes.

In the like-for-like standalone target-machine driver, exact nested 2,000 native pass wall fell from 39.211 to 20.577 s, and MachineSinking from 19.465 to 0.754 s. The three largest remaining native wall costs were register coalescing (5.026 s), Live DEBUG_VALUE analysis (4.485 s) and greedy register allocation (4.293 s). Reported timings include pass instrumentation and remain distinct from compiler medians.

## Independent output and runtime controls

The experiment passed the Release Linux escape-analysis JIT test and independently linked normal and optimized AOT tests, including exact foreign-Heap rejection via five controls in each AOT mode, forced-GC child visibility, allocation failure and recovery. The normal/O2 JIT probe outputs were byte-identical across 132 fixture/mode records, covering 784,376 forced polls, 10,936,212 inspected root visits and 104 remaining-allocation OOM controls. All 132 paired escape-analysis AOT objects and both independently emitted numeric/SIMD objects are byte-identical. Eighteen managed AOT workload/mode objects are also byte-identical. The matched managed controls ran 288 processes with three independently checked times each across calls, closures, workers, async tasks and managed allocation/reference shapes. The representative arithmetic, floating-point, collections, sorting, dictionary, Unicode/text, JSON and native I/O suite checked 1,344 output samples across real JIT and independently linked AOT. Runtime measurements used the observer-off hosts.

Representative normal AOT emitted identical objects and linked executables. Representative optimized AOT changed code and relocation bytes only in the JSON serializer path. `/@stdlib/json.void::_write` still occupied 19,978 bytes, six callee-saved register pushes and a 0x22d8-byte local frame. The disassembly shows branch retargeting and local block reorderings inside that function. The full .text size was 275,440 bytes, the .eh_frame bytes matched, and the overall object fell from 1,260,008 to 1,259,792 bytes. All representative outputs matched. In the changed optimized AOT executable, measured JSON-128 time fell 0.4%, sort-256 rose 1.1% and sort-1024 fell 1.3%. The report retains those small timing changes and the unchanged acceptance evidence for G44's separate sort/text changes. Optimized AOT closures measured +8.3% in the matched blocks with byte-identical closure objects. Managed AOT normal escape measured +4.1% with identical objects. These are timing controls without a demonstrated generated-code improvement. No application runtime win is claimed for G45.

## Unfavorable results and next action

The flat-2,000 normal JIT trial rose 9.4% across the first two pairs and 9.2% pooled across six pairs. Emitted bytes matched. Flat-2,000 O2 JIT rose 2.7%. The small normal AOT median rose 17.3% (about 2.7 ms), and flat-500 normal AOT rose 3.5%. These are elapsed compiler controls with the same emitted objects. The per-sweep CFG order adds traversal and scratch storage even in functions that already converge quickly.

A separate installed-LLVM trial reordered the reachable LLVM IR block list after O2, before native lowering. That moved the exact 500-site replay from 3.891 to 2.629 s and achieved 2 sweeps, with the verifier passing. It grew the replay object from 651,184 to 653,736 bytes. Its allocatable code and data, unwind and text relocation sections matched. Broader replays using stock LLVM changed executable and unwind sections for mixed O2, nested-500 normal and representative O2. Mixed normal matched allocatable sections. None of those replays qualified a VOID-side IR layout policy. The trial remains separate and is held.

### Separate normal-JIT kill-flag cost

Caller-address instrumentation located the dominant nested-750 normal-JIT `clearKillFlags` traffic in MachineCSE's cross-block repair for a retained implicit physical-register definition. On physical register 28, one caller made 1,125 full-list scans, visited 8,016,750 uses and cleared zero flags. That accounted for 2.462 s thread CPU. MachineSinking's separate one-call walk cost 3.35 ms. The nested-500 O2 global observer spent 17.4 ms inside the helper across all callers. [Pinned LLVM MachineCSE.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/CodeGen/MachineCSE.cpp#L707-L712).

A second, independent LLVM trial retains the first repair and a pass-exit repair for each alias-free physical register. Aliased and other registers keep the existing repair sequence. The targeted normal-JIT register went to two scans, 14,252 use visits and 9.61 ms of observed thread CPU in the repair loop. Across two AB/BA pairs, nested-750 normal JIT went 11.523 → 8.000 s (30.6% less time). The nested-750 normal and nested-500 O2 objects and output oracles matched stock. The CSE trial needs proof that target hooks and in-pass transformations between those repairs preserve the required kill information. It is held separately from the traversal patch. Their timings have not been combined.

Reduce the MachineSinking case to standalone LLVM IR/MIR and build regression coverage for ordering-sensitive critical edges, PHIs, debug uses and irreducible CFGs. Submit the reproduction and traversal patch for upstream review through the owner. Prove the normal-JIT MachineCSE repair independently against target hooks and kill-flag consumers before proposing that patch. Other open Language Bible work includes broader escape and precise lifetimes, checked collections, dictionary and sorting, text/Unicode/JSON materialization, wider SIMD, native/JIT and AOT costs, GC tails, startup, and future macOS/release qualification.

## Accepted qualification and reproduction

The prior G44 qualification ZIP has SHA-256 `82b57865ab53d9b4b24ff630cc9ef95218ce09bf6fdf8b7b541398a38d042f97`. G45 verified its CRC and 8,890 entries, the frozen compiler/test identities, 3,319 native Windows source hashes, and the installed tool identities. These receipts apply to the unchanged production source and accepted SDK. They cover Linux Release and fatal ASan/UBSan/LeakSanitizer at 86/86 each plus five diagnostic controls each, native Windows 10 Debug and Release at 71/71 each, fatal TSan on 66 fixtures across both independently linked AOT modes plus three Runtime controls, 216,000 forced-GC checks, 132 matched LLVM/native pairs, 264 allocation/GC comparisons, foreign-Heap rejection, failure recovery, callbacks, reload, workers, async and 1 MiB native stacks. A fresh installed Linux Release smoke run passed four focused tests, including managed, escape and both independent numeric AOT modes. Native Windows and fatal sanitizer receipts cover the installed SDK. The experimental LLVM overlay has separate Linux receipts.

The task evidence archive is `g45-native-codegen-scaling-2026-11-16.zip`. Its receipts record CRC and every SHA-256, restoration of the VOID documentation patch and both experimental LLVM patches, omitted byte-identical artifacts by alias, and exact host and Windows guest cleanup. Workloads use original G44 source where available, independently computed output oracles, sequential AB/BA comparison blocks, observer-free headline timings and separate observer runs. `REPLAY.md` in the archive gives the build and sampling order.
