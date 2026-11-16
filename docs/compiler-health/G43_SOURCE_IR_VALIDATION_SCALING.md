# G43 — source IR and validation scalability

**Disposition: INTEGRATE for Golden Wrench review.** G43 reduces the work and resident memory of the existing definite-initialization proof. On the paired 500-site nested workload, median whole-process compilation time fell from 14.053 to 6.396 seconds and peak RSS from 700,872 to 317,948 KiB. The 100-, 500-, 750- and 2,000-site results below show where that improvement applies.

The accepted baseline and live remote `main` are G42 `6022890670333cdd5e38a20c84b8c77d0108f8cd`. Isolated worktree `/home/shmellyorc/Documents/Void.Language-G43` uses branch `codex/g43-source-ir-validation-scaling`. LLVM, Clang and LLD are 23.1.4. The qualified validator SHA-256 is `27e38539120620956142fbdf4f3acf314455beb5cb7abbffe4f166b3fa2b5a86`. G31–G42 Runtime, SIMD, ownership and precise-root foundations remain in place. G37/G37-F remain separate and held for performance. G41 Priority B remains rejected.

## Measured source of the repeated work

The historical 470.7-second, roughly 9.8 GiB nested compile is a G41 measurement reproduced in the G42 archive excerpt. G42's constrained attempt stopped before ownership analysis. The historical source IR timer included `ir::validate`. Its 114.9 seconds cannot be attributed to AST traversal or added to the other timers as independent phase costs.

In a G42 clock-only pilot on the exact 500-site nested source, the frontend built 1,501 blocks and 18,755 instructions in 17.5 ms. Definite initialization took 7.75 s total across the source, JIT admission and LLVM-preparation validation calls inside a 14.25 s process. The separate work observer counted 85,131,879 C++ allocation requests and 3.41 GB in cumulative requested bytes within this helper. The ordinary process and the work observer have different peak RSS. Allocation requests are compiler-side counts, and cumulative bytes are distinct from live or resident memory.

Each validation call checked 4,502 local slots over 1,501 blocks. The old proof filled two set tables with every slot per block, copied sets during every reverse-postorder sweep and intersected them by inspecting individual tree entries. It copied each final input set again before checking loads and receivers. The underlying proof facts are booleans indexed by already validated local slots.

## Production correction and proof

`validate_reference_initialization` in `src/ir/reference_capabilities.inc` now keeps one packed exit-state matrix with 64 slots per word and reuses a scratch vector. The existing reachable reverse-postorder traversal, predecessor lists, block-zero receiver seed and store/capture transfer functions remain. Each predecessor meet is a wordwise intersection. The mask covers only actual slots, including the last partial word. The table size calculation is checked before allocation.

Transfer depends on predecessor exits. After those exits converge, the validator recomputes a block's input from the final exits and walks its original instruction order to check reads. This preserves the greatest fixed point and the order in which legitimate invalid reads report errors. Initializer eligibility, call and receiver authority, observed defaults, array captures and conditional cleanup treatment use the existing rules. A malformed out-of-range read slot now produces the standard `invalid slot` failure before bit indexing. Valid operands and receiver signatures still pass the existing structural checks first.

Nothing is cached across functions or validation calls. Each call starts from the current module and checks mutations independently. The transfer costs per sweep are proportional to the instruction visits plus the number of visited blocks and edges times the number of 64-bit words needed for the locals. State storage uses one word array of `blocks × ceil(locals / 64)` plus scratch. The old set copies and heap nodes disappear from that state. The code also adds the explicit `<span>` include. Production complexity adds 55 lines and removes 22 in the existing proof, plus the explicit include. There is one validator, one type system and one lowering pipeline.

## Matched compiler measurements

Ordinary JIT hosts use the accepted G42 libraries and qualified G43 libraries, with four balanced order blocks per required shape and two for the supplemental 750-site shape. Whole wall and CPU include source analysis, lowering, JIT and small invocations checked against independent oracles. Peak RSS is from `wait4`. The clock-only compiler and allocation-counting compiler are separate builds. No campaign timing overlaps heavy build activity. All matched shapes returned their independent results.

| Shape | Whole wall s | CPU s | Peak RSS KiB | Wall change |
|---|---|---|---|---|
| flat 100 | 0.117 → 0.114 | 0.114 → 0.111 | 51,036 → 51,112 | -3.1% |
| flat 500 | 0.553 → 0.563 | 0.548 → 0.556 | 68,738 → 68,726 | +1.9% |
| flat 2000 | 4.654 → 4.623 | 4.622 → 4.588 | 141,380 → 141,028 | -0.7% |
| nested 100 | 0.626 → 0.423 | 0.618 → 0.418 | 77,306 → 68,972 | -32.4% |
| nested 500 | 14.053 → 6.396 | 13.952 → 6.346 | 700,872 → 317,948 | -54.5% |
| nested 750 | 37.289 → 15.740 | 36.970 → 15.614 | 1,530,608 → 487,908 | -57.8% |
| mixed | 0.050 → 0.050 | 0.048 → 0.048 | 48,826 → 49,020 | +0.5% |

The mixed source combines a private generic managed-reference function, nested branches, a list, and a dictionary. The domain oracle for input 4 is 22. The flat and nested fixtures reuse the archived G42 workload. The 750-site case is a prefix of the exact nested workload with its own expected output.

Clock-only medians below are milliseconds. The LLVM lower envelope includes validation in lowering preparation. Source/JIT/lowering validation calls include definite initialization and the other existing checks. The columns are not additive.

| Shape | Semantic graph | IR construction | Source validate | JIT validate | Lower validate | Definite init across calls | LLVM lower | LLVM O2 | JIT materialize |
|---|---|---|---|---|---|---|---|---|---|
| flat 100 | 1.432 → 1.417 | 0.893 → 0.870 | 4.545 → 4.326 | 4.447 → 4.272 | 4.456 → 4.230 | 0.747 → 0.237 | 9.287 → 9.206 | 20.126 → 20.243 | 59.379 → 59.952 |
| flat 500 | 6.660 → 6.611 | 4.843 → 4.505 | 21.376 → 20.306 | 21.030 → 20.357 | 22.128 → 21.018 | 4.149 → 1.235 | 44.682 → 43.550 | 170.173 → 168.397 | 265.311 → 275.194 |
| flat 2000 | 31.605 → 30.838 | 18.234 → 17.679 | 94.072 → 86.909 | 89.960 → 84.428 | 91.546 → 86.091 | 20.325 → 5.438 | 188.273 → 182.638 | 2647.288 → 2664.833 | 1430.899 → 1429.065 |
| nested 100 | 12.008 → 12.289 | 3.004 → 3.195 | 105.521 → 14.828 | 90.574 → 14.534 | 90.399 → 15.105 | 244.685 → 1.174 | 102.551 → 30.392 | 109.392 → 114.803 | 187.865 → 197.218 |
| nested 500 | 71.600 → 71.367 | 16.979 → 17.827 | 2840.031 → 80.178 | 2621.450 → 78.159 | 2621.334 → 79.937 | 7905.929 → 9.242 | 2695.825 → 165.132 | 3083.737 → 2952.028 | 3524.058 → 3745.589 |
| nested 750 | 105.936 → 105.345 | 25.762 → 26.141 | 7464.609 → 119.777 | 6666.215 → 117.054 | 6430.160 → 118.776 | 20232.301 → 18.936 | 6545.811 → 254.691 | 9311.814 → 10279.646 | 5206.834 → 5407.272 |
| mixed | 0.697 → 0.700 | 0.244 → 0.248 | 1.095 → 0.973 | 1.012 → 0.896 | 1.031 → 0.904 | 0.399 → 0.075 | 2.143 → 1.982 | 12.618 → 12.554 | 21.301 → 21.066 |

C++ allocation requests and bytes below come only from the allocation-counting builds. The counters count transfer work in the fixed-point sweeps. Final read validation is counted separately in the raw receipts.

| Shape | Validate calls | Transfer block visits | Predecessor visits | C++ allocations | Requested bytes | Old initial set entries | New exit words |
|---|---|---|---|---|---|---|---|
| flat 100 | 3 → 3 | 6 → 3 | 0 → 0 | 8,766 → 3,333 | 538,932 → 321,564 | 1,812 | 15 |
| flat 500 | 3 → 3 | 6 → 3 | 0 → 0 | 43,566 → 16,533 | 2,679,732 → 1,599,732 | 9,012 | 72 |
| flat 2000 | 3 → 3 | 6 → 3 | 0 → 0 | 174,066 → 66,033 | 10,719,732 → 6,404,772 | 36,012 | 282 |
| nested 100 | 3 → 3 | 1,806 → 1,806 | 2,400 → 2,400 | 3,436,467 → 9,684 | 138,123,336 → 1,074,288 | 1,629,012 | 13,545 |
| nested 500 | 3 → 3 | 9,006 → 9,006 | 12,000 → 12,000 | 85,131,879 → 48,096 | 3,408,577,464 → 7,354,848 | 40,545,012 | 319,713 |
| nested 750 | 3 → 3 | 13,506 → 13,506 | 18,000 → 18,000 | 191,400,885 → 72,102 | 7,661,038,518 → 12,970,422 | 91,192,512 | 715,818 |
| mixed | 3 → 3 | 90 → 87 | 96 → 96 | 5,502 → 795 | 269,448 → 76,992 | 2,064 | 45 |

At 500 nested sites the work counters match at 7,752 lowered expression visits, 1,502 statement visits and 18,755 emitted instructions. The observed semantic helpers match at 2,005 type-resolution calls, 7,255 local lookups and 0 call-resolution lookups. The retained allocation count includes the existing type/required-slot checks and CFG scratch allocations. The reduction comes from representing the same proof state in words.

The exact G42 2,000-site nested workload was deferred at 11.03 GiB available against the 14 GiB launch budget. The earlier 8 GiB attempt had stopped before the ownership analysis. The G43 candidate has a separate 6 GiB address-space cap, 8 GiB launch minimum and 2 GiB live host reserve.

| Exact nested 2,000 candidate | Oracle | Wall s | CPU s | Peak RSS KiB | IR construction ms | Definite init ms | LLVM lower ms | LLVM O2 ms | JIT ms |
|---|---|---|---|---|---|---|---|---|---|
| ordinary | 4000 | 126.271 | 125.342 | 1,325,644 | — | — | — | — | — |
| phase | 4000 | 126.057 | 125.164 | 1,323,752 | 68.347 | 67.588 | 715.208 | 86957.887 | 37055.492 |

There is no same-host whole-compiler before/after percentage for the exact 2,000-site nested workload. Historical timings and the G42 synthetic solver replay remain useful context for the resource limit.

## Correctness and code identity

Three new negative sources exercise non-nullable initialization and nullable and readonly rules. G42 and G43 `voidc check` produce byte-identical exit status, stdout and stderr, including diagnostic codes, spans and order. The maintained mixed fixture executes correctly in normal and O2 JIT and both independently linked AOT modes. Its matched native objects and executables are byte-identical. The direct IR test accepts initialized slots at 63, 64, 127 and 128 and rejects a branch-only write before the join at each slot. The same validator accepts the original program after each rejection.

The evidence-only differential test uses the extracted G42 and G43 helper bodies on 10,000 generated control-flow and event traces with empty, single-word and multiword local domains, backedges, disconnected blocks, duplicate edges, receiver seeds, cleanup, array captures and required/optional reads. The complete default-observation sets and failure text matched in all 10,000 cases, including 7,512 acceptances, 2,488 rejections and 1,241 acceptances with observed defaults. Existing native tests exercise real nominal types, generic specialization, imports, overload resolution, construction, foreign-Heap rejection, allocations, recovery and reload.

All 132 matched normal/optimized LLVM IR and native object pairs are byte-identical. All 264 ordinary/observer emissions match. Both full escape AOT executables and the representative AOT executables match byte for byte. All 18 managed representative object pairs match. The LLVM backend source, backend archive and Runtime archive match the baseline byte for byte. Exact ownership decisions and required roots also match in 264 comparisons of eight GC/allocation fields with three samples each. The raw force-collection witness preserves child visibility and construction counts. G43 makes no eligibility expansion or execution-speed claim for unchanged code.

## Native qualification

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed the full 86-test group and 5 supplementary diagnostic/lookup tests. Fatal TSan passed 66 fixtures across normal and optimized independent AOT, plus Runtime coordination, race and allocator controls. Native Windows 10 build 19045 Debug and Release each passed 71 tests with all 3,319 input hashes verified. MSVC tools are 14.44.35207 and the Windows SDK is 10.0.19041.0. Linux and Windows stack checks used 1 MiB. All 216,000 raw-address forced-GC checks passed with existing precise roots. Cold allocation failure, recovery, callback, reload, worker, async, G38 SIMD and checked collections are covered by the existing groups.

## Runtime regression controls

The observer-off samples cover arithmetic, arrays, dictionary, sorting, text, Unicode, JSON, native I/O, calls, workers and async. Raw sample files retain every result. Median runtime deltas below are controls for byte-identical AOT executables or matched JIT inputs. Native executable sizes and Runtime/GC measurements are recorded independently. All G34–G42 adverse measurements remain in the historical ledger.

| Representative source | app-normal | app-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| array-1024 | -3.0% | +1.7% | +1.4% | -0.1% |
| array-4096 | -0.9% | +2.2% | +1.1% | -3.5% |
| dependent-1000000 | -1.3% | +0.8% | -0.7% | +0.1% |
| dict-1024 | +0.5% | -0.3% | +0.3% | -0.2% |
| float-1000000 | -1.0% | +0.8% | -1.4% | -1.1% |
| integer-1000000 | -1.1% | -0.3% | -0.4% | -0.7% |
| integer-4000000 | -2.0% | +0.3% | -0.4% | -0.1% |
| io-200 | +1.6% | +6.1% | +0.4% | +3.9% |
| json-128 | -2.1% | -0.4% | -1.4% | +1.5% |
| json-32 | -0.4% | +1.2% | +2.0% | +1.8% |
| sort-1024 | +0.5% | +3.0% | -0.3% | -1.4% |
| sort-256 | +0.4% | +3.0% | -0.7% | -4.5% |
| text-1024 | -0.0% | +2.2% | +0.4% | +1.4% |
| text-256 | +0.5% | +1.0% | +1.0% | +1.1% |

| Managed source | jit-normal | jit-O2 | aot-normal | aot-optimized |
|---|---|---|---|---|
| async-tasks | +3.0% | +2.0% | -3.5% | +0.7% |
| calls | +2.0% | +2.3% | +0.2% | +1.0% |
| closures | +0.1% | +0.8% | +0.7% | -0.5% |
| managed-across-edge | -0.7% | -1.0% | -0.7% | +0.1% |
| managed-escape | -3.1% | -0.8% | +3.6% | -1.2% |
| managed-local | +0.1% | -1.9% | +1.9% | -3.4% |
| managed-reference | -1.2% | +3.5% | +2.2% | +2.2% |
| managed-survives-allocation | -0.8% | +1.8% | -1.2% | -1.9% |
| workers | +0.6% | +3.2% | -0.3% | +2.5% |

Largest adverse elapsed observations in each escape timing mode are retained below.

| Mode | Case | Baseline → G43 median ns | Elapsed delta |
|---|---|---|---|
| jit-normal | direct_null_failure | 415 → 445 | +7.2% |
| jit-normal | root_null_straight | 5,011,900 → 5,278,134 | +5.3% |
| jit-O2 | root_null_backedge | 5,125,884 → 5,315,148 | +3.7% |
| jit-O2 | factory_field | 19,455,803 → 20,021,395 | +2.9% |
| aot-normal | direct_null_failure | 315 → 340 | +7.9% |
| aot-normal | context_control | 5,961,190 → 6,299,918 | +5.7% |
| aot-optimized | root_retirement | 6,585,591 → 7,003,323 | +6.3% |
| aot-optimized | direct_null_failure | 280 → 295 | +5.4% |

| Escape AOT mode | Bytes in each binary | Shared SHA-256 |
|---|---|---|
| normal | 3,653,512 | `afaed10dc4754f98a402bcf13777584e72333b4060b870cf0fd736ef472a4c91` |
| optimized | 3,479,816 | `f831d56d95155b275f84a48e18921719f35f420676ef9c6f03b01a49d4902494` |

| Representative AOT mode | Bytes in each binary | Shared SHA-256 |
|---|---|---|
| normal | 5,115,704 | 9673ee46d978b5a9a1bea2cce104c74b5173911bea9cf1175c36656354304f64 |
| optimized | 4,999,176 | fe774b659c8807fd627fa62f9e4dc3009e39e3572aad3ea865acf16f69fbd19f |

The observer-only raw data retains each collection latency and stopped-world interval alongside the exact matching managed construction, allocation, root-search, marking and live-payload counts. Timing variation in those samples does not show a change in collector work.

Warm-filesystem process startup medians use 20 balanced blocks. JIT startup includes frontend, lowering and execution checked against an independent oracle. AOT startup includes process launch and an independent output check.

| Startup mode | Whole wall ms | CPU ms | Peak RSS KiB |
|---|---|---|---|
| aot-normal | 2.359 → 2.336 | 2.059 → 2.004 | 15,188 → 15,188 |
| aot-optimized | 2.235 → 2.216 | 1.950 → 1.903 | 15,188 → 15,188 |
| jit-O2 | 21.713 → 21.430 | 20.954 → 20.643 | 46,726 → 46,904 |
| jit-normal | 19.651 → 19.596 | 18.868 → 18.916 | 44,670 → 45,076 |
| representative-aot | 3.054 → 2.981 | 2.693 → 2.643 | 15,188 → 15,188 |
| representative-aot-normal | 3.034 → 3.170 | 2.687 → 2.639 | 15,188 → 15,188 |

## Deferred alternatives and next implementation

The first copy of the differential harness lacked the production type aliases and pointer interface used by the extracted helper. The corrected harness passed all traces. The first startup run stopped before sampling because its measurement module was missing from task scratch. Restoring the exact module from the G42 archive let the campaign resume at startup with the completed runtime blocks preserved. Both first-attempt logs are archived. The qualified compiler and tests stayed identical.

The pilot found enough avoidable work in one existing validator to meet the milestone. A persistent module-validation cache would need to account for mutable IR and lowering transformations. AST caching would target a much smaller measured phase. Neither was implemented. The whole compiler still spends substantial time in LLVM optimization and JIT materialization after validation is cheaper. See the per-phase table and original 2,000-site resource receipt. A wall-time or RSS observation that gets worse remains in the tables, regardless of the helper improvement.

Next, profile the LLVM O2 pass pipeline and JIT materialization on the exact nested 2,000-site input. The candidate spent 86.96 seconds in LLVM O2 and 37.06 seconds in JIT materialization in its instrumented run. The pass-level evidence needed to justify a bounded pipeline change is still missing. For the remaining source validation cost, instrument the duplicate per-block `Type` vector materialization in `ir::validate`. Check whether `body_types` and `qualified_types` can share one owning vector without shifting terminator failures or the later construction/ownership checks. Remaining Language Bible work includes broader ownership/retention, collections and dictionary/sort, text/Unicode/JSON, SIMD beyond G38, native/JIT overhead, GC tails and startup, and macOS/release qualification.

## Evidence and source inventory

The evidence archive retains exact patches, history, source and toolchain hashes, phase records, allocation counts, independent oracles, diagnostics, native/sanitizer receipts, selected IR/objects and raw regression controls. Source restoration and every archive CRC/SHA-256 record are required before task-owned cleanup. The sibling receipts record separate observed host and guest reclamation and the exact Git status.

```text
docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md
docs/compiler-health/G43_SOURCE_IR_VALIDATION_SCALING.md
src/ir/module.cpp
src/ir/reference_capabilities.inc
tests/managed/tests.cpp
tests/managed/validation/invalid_initialization.void
tests/managed/validation/invalid_nullable.void
tests/managed/validation/invalid_readonly.void
tests/managed/validation/mixed.void
```

HEAD remains the accepted G42 parent. The index remains empty. No commit, push, force-push, history rewrite or hosted CI.
