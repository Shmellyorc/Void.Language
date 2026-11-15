# G37 — Source-constant checked operations

Starting source: `1e606f8e5067a3113b1d4d4f850fd3a11e2e57a0` (accepted G36), LLVM/Clang/LLD 23.1.4. **Implemented production-source candidate; keep it separate pending Golden Wrench/Boss acceptance of the measured costs.** No staging, commit, push or hosted CI.

The compiler now uses validated scalar-list encoding information to select constant-width checked reads. The executable path removes 4096 dynamic-width reads from the prepared traversal and emits ordinary constant-width native loads while preserving all dynamic safety checks. Selected native read cells improve about 2.2–5.3%; complete source results are mixed. Full AOT array/4096 regresses about 3.3%, and substantial numeric/context regressions remain. This is a real bounded compiler capability, but the present evidence does not justify default production acceptance.

Corrected source: Linux Release and fatal ASan/UBSan/LeakSanitizer **74/74 each**; native Windows 10 Release and Debug **60/60 each**, with normal/O2 JIT, independent AOT, pool/pacing and all stack controls passing. Relevant fatal **TSan passed** Runtime/race/live-allocator, source-read and concurrent catalog/GC controls plus 26-pattern independent normal/O2 AOT. The final qualification receipt records all stages.

## Bounded implementation and proof

Validated scalar-list `array_get` lowering selects the encoding-specific same-build checked Runtime entry. The existing IR type proves exactly one of the eleven supported scalar encodings and therefore the load width. The encoding is baked into an explicit Runtime template instantiation; no second descriptor, cache, allocator, collector, synchronization mechanism, root authority or optimizer is introduced. Fixed arrays, aggregate/reference elements, native borrowed descriptors and other operations retain their existing checked paths.

The generic and source-selected reads share the complete locked read body in `Runtime::source_list_read_index`. The legacy dynamic entry supplies its runtime encoding to the internal dynamic template instantiation; source instantiations fix the encoding and width. Root/header validation, normalization and the alignment-safe load remain in that one scope. No raw borrowed address is returned. Unsigned raw bits preserve floating-point payloads and booleans; LLVM folds the constant copy size to a native load. The C ABI wrappers share signedness validation, root admission, pending-failure handling and exact error mapping.

The first candidate extracted an address-return proof helper. It removed width interpretation but introduced an extra helper call and by-value root-token copy. Its Release/native qualification, complete measurements, exact patch and source identities remain recoverable. A const-reference/ordinary-inline diagnostic retained the call; sharing the complete checked body in the existing encoding template removes both unnecessary pieces without forcing inlining, changing eligibility or adding another representation. Final matching-header qualification and matched timings were repeated. Numeric/context regressions are investigated separately and are not attributed to that correction.

Only encoding/load width is invariant. Every read still validates current catalog membership, exact schema and nominal header identity, registered-root authority, tracked lifetime, count/capacity, backing geometry, completeness, live prefix and index. Negative indexes normalize against the same checked header used for the read; unsigned magnitude subtraction preserves `INT64_MIN` failure without signed overflow. Element offsets retain their capacity check. Foreach version checks, receiver/index evaluation order, selected writes, cleanup, GC visibility, cancellation and faults remain at their existing program points. No proof is hoisted across a boundary that could invalidate it.

Borrowed native descriptors remain mutable and continue through the existing full validation on each operation. A constant compiler-selected encoding grants no native descriptor or root authority. Current policy/catalog publication remains dynamic. G31 polling, G33/G35 allocation-elimination eligibility and fallbacks, G34 B5/C2/D4/E3, explicit host budgets and G36 foundations remain intact.

## Exact source and identities

The production patch covers five compiler/Runtime files plus one maintained test. No CMake, frontend analysis, collector, allocator or standard-library source change is added. Public Runtime data layouts and explicit budgets are unchanged. The private same-build C ABI adds eleven encoding-specific entries; existing dynamic entries remain available. One existing scalar encoding enum and one checked-read body serve static and dynamic modes; the internal `-1` template marker selects the legacy dynamic mode and is never substituted for its runtime encoding.

Production patch SHA-256: `fc09193612d96a7a734e921c2ed77d8644673519974fe68cdb5156b1702fbc66`. Exact baseline/candidate file hashes and linked consumer identities are retained in the evidence.

| File | Candidate SHA-256 |
| --- | --- |
| `include/void/runtime.hpp` | `b32f8812e0915f57aa5924b722398c4f483c7ff1700b00c0f0db746141688651` |
| `include/void/runtime_abi.hpp` | `c66455d2c4bd7870eba99dd61217f4eaedcfe749db0014268f4eec9bd80ec5ba` |
| `src/backend/llvm/backend.cpp` | `bf27d790e873c1a65898618bad1295d4b6eee093b98fee1965cd460c0a622821` |
| `src/runtime/fixed_abi.cpp` | `711bc19a009a07fa7a8c57d0b38ac9f8e771b57724bcda5ceb0b0753ab93bd19` |
| `src/runtime/list.cpp` | `88b3fe0be11674d14e3898c6888d7a8bac6ab28ef960df3b1c35c3cb3b52480a` |
| `tests/runtime/checked_collections.cpp` | `3584f44d2c029941185469ba9e9fb49ede81caece944288a1334981af98c032f` |

The permanent Git inventory also includes the G32/G33 research reports, the changed implementation ledger and this consolidated report. Unchanged research reports retain their baseline hashes.

## Work removed and checks retained

The complete representative application has 56 dynamic scalar-read sites in G36 and 56 encoding-selected sites in G37, with zero remaining dynamic sites in that scalar-list lowering. Separate normal/O2 IR captures select all eleven encodings. Optimized Runtime IR contains exactly eleven constant-width source reads, no variable payload copy in them and no address-return helper. The legacy dynamic instantiation retains its variable-width payload copy; its existing entry tail-forwards to that body. Selected native disassembly and object/body identities accompany the source patch.

Prepared traversal records 4096→0 dynamic scalar reads and 0→4096 source-selected reads. Actual managed construction, native allocation, layout construction, Runtime frame, root-word/frame-walk and dictionary-policy counts remain equal across the maintained operation cases. One checked source/Runtime ABI boundary per read remains. The 26 G35 fixtures retain their construction, precise-root/publication, forced-GC and fallback assertions; object comparisons and all mode-specific counts are retained.

The specialization adds no production metadata structure, cache, heap allocation, synchronization or root authority. Eleven ABI symbols/registrations and the encoding template instantiations add code and compiler binding cost. The first candidate's extra helper/token copy was removed by sharing the whole checked body; no forced-inlining attribute, padding, alignment tuning, host-budget change or eligibility narrowing is used. Direct loads fold an ordinary alignment-safe constant `memcpy`; no broader SIMD policy is enabled, and source-loop vectorization remains a separate objective.

## Correctness, negative controls and recovery

The maintained source controls cover all scalar encodings and integer index widths, receiver/index evaluation and post-index mutation, live bounds/capacity/version, readonly views, native descriptor mutation, current catalog and policy replacement, callbacks/reload/cancellation, cleanup/fault injection and recovery. New maintained ABI controls compare exact result bits and failure text against the dynamic read for positive/negative/extreme indexes, invalid signedness, pending failure, unregistered roots, wrong schema, forced GC and later catalog publication. Raw floating-point payloads and boolean width are included.

A focused native control reads through the new entries while two peers publish 128 catalog generations and collections are forced repeatedly. Its positive/negative independent oracles, reclamation and collection assertions run in Release, fatal sanitizer and TSan profiles. B5 stale-cache tests still require an actual `use-after-poison` failure; an expected fatal negative is distinguished from a passing invocation.

Native Windows is Windows 10 build 19045 (VM label Windows 11), VS2022 / Windows SDK 10.0.19041.0, existing approved providers and LLVM 23.1.4. Every profile rebuild uses matching headers. The maintained selection, normal/O2 JIT, independent AOT, pool/pacing and PE 1-MiB stack checks remain selected. The additional 33 scalar encoding/live-length cases execute in normal/O2 JIT and independently linked normal/O2 AOT, with two additional 1-MiB PE checks per profile. Native correctness timings are not claimed as matched platform performance measurements.

## Matched representative throughput

Four alternating forward/reverse blocks, three warm observer-off samples per cell, unchanged independent result oracles and pinned affinities/1-MiB execution stacks. Windows is quiescent for timing. Times are medians; positive deltas are slower. Full samples/ranges, CPU time and RSS are retained.

| Case | G36 AOT ms | G37 AOT ms | AOT delta | Normal JIT delta | O2 JIT delta |
| --- | --- | --- | --- | --- | --- |
| array/1024 | 1.087 | 1.141 | +4.9% | +0.3% | -1.9% |
| array/4096 | 4.344 | 4.486 | +3.3% | -0.6% | -1.5% |
| dependent/1000000 | 24.480 | 27.662 | +13.0% | -8.3% | -6.6% |
| dict/1024 | 4.085 | 4.095 | +0.2% | -0.2% | +1.2% |
| float/1000000 | 24.155 | 26.303 | +8.9% | -10.1% | -8.0% |
| integer/1000000 | 23.694 | 26.163 | +10.4% | -8.5% | -7.7% |
| integer/4000000 | 99.031 | 104.633 | +5.7% | -8.3% | -7.1% |
| io/200 | 6.983 | 6.893 | -1.3% | -0.1% | -1.0% |
| json/128 | 35.663 | 36.034 | +1.0% | +1.1% | +0.3% |
| json/32 | 5.698 | 5.873 | +3.1% | -0.1% | -0.3% |
| sort/1024 | 26.230 | 25.833 | -1.5% | -1.8% | -4.0% |
| sort/256 | 5.284 | 5.188 | -1.8% | +2.3% | -3.3% |
| text/1024 | 52.559 | 52.298 | -0.5% | +1.1% | -2.3% |
| text/256 | 13.059 | 12.873 | -1.4% | +0.2% | -0.9% |

## Managed objects, calls, workers and async

| Source workload | Normal JIT | O2 JIT | AOT |
| --- | --- | --- | --- |
| async-tasks | -0.8% | -8.0% | — |
| calls | +4.6% | +2.7% | -0.3% |
| closures | +1.4% | -2.0% | — |
| managed-across-edge | +6.6% | +4.2% | — |
| managed-escape | -1.0% | -1.3% | +1.0% |
| managed-local | +3.7% | +4.8% | +1.7% |
| managed-reference | +3.7% | +3.2% | — |
| managed-survives-allocation | +0.3% | -0.7% | -0.7% |
| workers | +12.7% | +10.8% | — |

All 26 G35 patterns retain independent result/construction/root assertions in normal/O2 JIT and independently linked AOT. Their per-pattern performance remains explicit:

| G35 pattern | Normal JIT | O2 JIT | AOT |
| --- | --- | --- | --- |
| alias_rebind | -5.7% | -4.5% | +3.5% |
| branch_alias | +3.2% | +4.7% | -4.8% |
| branch_birth | +4.4% | +3.1% | +1.9% |
| context_control | -2.5% | -2.5% | -1.0% |
| direct_reference | -3.6% | -1.2% | +3.5% |
| field_rebind | -2.3% | +0.7% | +2.3% |
| helper_generic | +3.2% | +1.9% | -0.9% |
| helper_read | +3.9% | +6.6% | -1.3% |
| helper_reference | -4.0% | -2.2% | -1.0% |
| helper_return | +0.0% | -3.9% | +3.2% |
| helper_write | +3.4% | +3.6% | -3.5% |
| identity | -1.7% | -3.5% | -0.7% |
| incoming_rebind | -0.1% | -3.2% | +2.3% |
| incoming_reference | -2.5% | -2.7% | +2.1% |
| loop_carried | -3.7% | -3.9% | +2.5% |
| nested | -2.8% | -2.3% | +6.1% |
| null_failure | -3.8% | -2.6% | -1.8% |
| nullable | -4.3% | +3.8% | -0.8% |
| numeric_failure | +5.7% | +2.4% | -0.6% |
| outlined_array | -3.8% | -0.5% | -1.7% |
| payload_budget | -0.9% | +7.5% | -1.1% |
| public_helper | -2.1% | -1.2% | +2.3% |
| readonly_helper | +3.8% | +6.2% | -0.4% |
| reference_effects | -5.6% | -1.5% | +7.6% |
| references | +1.9% | -1.9% | +0.3% |
| unknown_factory | -4.3% | -3.7% | +5.4% |

## Scalar encoding/extent controls

The new fixture covers all eleven encodings at live lengths 1, 17 and 4096. Its raw `capacity` input label denotes the requested live length; growable backing uses the existing 4/doubling rule (4, 32 and 4096 backing slots). Fixed-layout controls independently cover all widths, exact/wrong extents and padded geometry. Short-cell clock/dispatch noise remains in the raw 132-cell summary. The long traversal cells follow:

| Encoding / length 4096 | Normal JIT | O2 JIT | Normal AOT | O2 AOT |
| --- | --- | --- | --- | --- |
| i8 | +0.1% | -11.4% | +1.7% | +2.2% |
| i16 | -4.5% | +2.0% | -1.7% | +0.5% |
| i32 | -4.1% | -1.5% | +0.8% | +0.2% |
| i64 | -1.8% | +4.2% | -2.6% | +1.4% |
| u8 | -7.9% | -0.9% | +1.7% | +0.0% |
| u16 | -4.4% | -4.9% | +2.5% | -1.4% |
| u32 | -2.1% | -5.1% | -0.7% | -2.5% |
| u64 | -7.5% | -0.7% | +2.3% | -0.1% |
| f32 | +2.4% | -3.2% | +3.5% | -0.1% |
| f64 | -1.6% | -4.7% | +0.7% | +1.3% |
| bool | +4.7% | +0.2% | -0.8% | -1.0% |

Prepared fixed arrays and managed-reference lists use independent sum oracles, warm initialization outside timing and lengths 1/17/4096. Their checked paths remain conservative:

| Prepared element case | Normal JIT | O2 JIT | Normal AOT | O2 AOT |
| --- | --- | --- | --- | --- |
| fixed-i32/1 | +8.9% | +0.5% | -3.9% | -7.3% |
| fixed-i32/17 | +1.3% | +0.1% | +1.3% | +0.2% |
| fixed-i32/4096 | +0.3% | +3.3% | +0.9% | -0.5% |
| managed-reference-list/1 | +0.6% | -5.1% | -6.8% | +2.4% |
| managed-reference-list/17 | -1.3% | -0.3% | +1.2% | +0.8% |
| managed-reference-list/4096 | -2.4% | -0.8% | -0.9% | -0.2% |

## Prepared and native-boundary controls

| Prepared operation / registered roots | G36 ns/operation | G37 ns/operation | Delta |
| --- | --- | --- | --- |
| dictionary_count_existing_token/128 | 679.2 | 664.3 | -2.2% |
| dictionary_count_existing_token/3 | 673.1 | 666.9 | -0.9% |
| dictionary_existing_token/128 | 761.8 | 747.6 | -1.9% |
| dictionary_existing_token/3 | 755.6 | 746.1 | -1.3% |
| dictionary_source_abi/128 | 878.9 | 866.2 | -1.4% |
| dictionary_source_abi/3 | 813.2 | 806.1 | -0.9% |
| fixed_existing_token_layout/128 | 79.9 | 78.7 | -1.6% |
| fixed_existing_token_layout/3 | 79.7 | 78.3 | -1.8% |
| fixed_source_abi/128 | 315.8 | 318.7 | +0.9% |
| fixed_source_abi/3 | 251.3 | 253.1 | +0.7% |
| list_existing_token/128 | 251.4 | 254.2 | +1.1% |
| list_existing_token/3 | 250.7 | 254.5 | +1.5% |
| list_selected_source_abi/128 | 348.4 | 329.9 | -5.3% |
| list_selected_source_abi/3 | 283.6 | 277.3 | -2.2% |
| list_source_abi/128 | 347.3 | 357.0 | +2.8% |
| list_source_abi/3 | 282.5 | 295.5 | +4.6% |
| native_entry_metadata/128 | 157.8 | 159.8 | +1.3% |
| native_entry_metadata/3 | 157.6 | 159.8 | +1.4% |
| native_key_metadata/128 | 172.0 | 173.3 | +0.8% |
| native_key_metadata/3 | 173.1 | 172.5 | -0.3% |
| raw_scalar_reference_only/128 | 0.5 | 0.5 | +0.1% |
| raw_scalar_reference_only/3 | 0.5 | 0.5 | +0.1% |
| root_admission/128 | 84.2 | 92.7 | +10.2% |
| root_admission/3 | 33.0 | 30.6 | -7.4% |
| source_prepared_array_4096/0 | 1467658.2 | 1476738.1 | +0.6% |
| source_prepared_dictionary_1024/0 | 2451092.9 | 2452214.0 | +0.0% |

## Counts and GC intervals

Counts are medians of three observer-on invocations, separate from throughput. Every checked ABI boundary, current root/catalog check and dynamic extent remains.

| Case | Counter | G36 | G37 |
| --- | --- | --- | --- |
| full array/4096 | dynamic_scalar_reads | 4096 | 0 |
| full array/4096 | constant_scalar_reads | 0 | 4096 |
| full array/4096 | managed_constructions | 12 | 12 |
| full array/4096 | allocations | 1386 | 1386 |
| full array/4096 | layout_creations | 11 | 11 |
| full array/4096 | runtime_frames | 35 | 35 |
| full array/4096 | root_words | 98 | 98 |
| full array/4096 | search_frames | 12354 | 12354 |
| full array/4096 | policy_queries | 0 | 0 |
| full array/4096 | policy_nodes | 0 | 0 |
| prepared array/4096 | dynamic_scalar_reads | 4096 | 0 |
| prepared array/4096 | constant_scalar_reads | 0 | 4096 |
| prepared array/4096 | managed_constructions | 0 | 0 |
| prepared array/4096 | allocations | 0 | 0 |
| prepared array/4096 | layout_creations | 0 | 0 |
| prepared array/4096 | runtime_frames | 1 | 1 |
| prepared array/4096 | root_words | 0 | 0 |
| prepared array/4096 | search_frames | 4097 | 4097 |
| prepared array/4096 | policy_queries | 0 | 0 |
| prepared array/4096 | policy_nodes | 0 | 0 |
| dictionary/1024 | dynamic_scalar_reads | 0 | 0 |
| dictionary/1024 | constant_scalar_reads | 0 | 0 |
| dictionary/1024 | managed_constructions | 19 | 19 |
| dictionary/1024 | allocations | 2466 | 2466 |
| dictionary/1024 | layout_creations | 26 | 26 |
| dictionary/1024 | runtime_frames | 37 | 37 |
| dictionary/1024 | root_words | 100 | 100 |
| dictionary/1024 | search_frames | 5180 | 5180 |
| dictionary/1024 | policy_queries | 5121 | 5121 |
| dictionary/1024 | policy_nodes | 0 | 0 |
| prepared dictionary/1024 | dynamic_scalar_reads | 0 | 0 |
| prepared dictionary/1024 | constant_scalar_reads | 0 | 0 |
| prepared dictionary/1024 | managed_constructions | 0 | 0 |
| prepared dictionary/1024 | allocations | 0 | 0 |
| prepared dictionary/1024 | layout_creations | 0 | 0 |
| prepared dictionary/1024 | runtime_frames | 1 | 1 |
| prepared dictionary/1024 | root_words | 0 | 0 |
| prepared dictionary/1024 | search_frames | 3073 | 3073 |
| prepared dictionary/1024 | policy_queries | 3073 | 3073 |
| prepared dictionary/1024 | policy_nodes | 0 | 0 |
| text/1024 | dynamic_scalar_reads | 60768 | 0 |
| text/1024 | constant_scalar_reads | 0 | 60768 |
| text/1024 | managed_constructions | 5233 | 5233 |
| text/1024 | allocations | 51869 | 51869 |
| text/1024 | layout_creations | 2128 | 2128 |
| text/1024 | runtime_frames | 19746 | 19746 |
| text/1024 | root_words | 2587 | 2587 |
| text/1024 | search_frames | 143281 | 143281 |
| text/1024 | policy_queries | 0 | 0 |
| text/1024 | policy_nodes | 0 | 0 |
| JSON/128 | dynamic_scalar_reads | 4153 | 0 |
| JSON/128 | constant_scalar_reads | 0 | 4153 |
| JSON/128 | managed_constructions | 2495 | 2495 |
| JSON/128 | allocations | 54316 | 54316 |
| JSON/128 | layout_creations | 800 | 800 |
| JSON/128 | runtime_frames | 41742 | 41742 |
| JSON/128 | root_words | 2607 | 2607 |
| JSON/128 | search_frames | 66739 | 66739 |
| JSON/128 | policy_queries | 0 | 0 |
| JSON/128 | policy_nodes | 0 | 0 |

| Stopped cohort | Variant | N | Median us | p95 us | p99 us | Max us |
| --- | --- | --- | --- | --- | --- | --- |
| text-1024 | base | 440 | 1032.609 | 1405.112 | 1608.261 | 2182.428 |
| text-1024 | candidate | 440 | 1052.664 | 1363.272 | 1588.321 | 1814.010 |
| json-128 | base | 192 | 966.380 | 1180.533 | 1438.152 | 1507.332 |
| json-128 | candidate | 192 | 989.345 | 1223.873 | 1551.002 | 1638.331 |

Intervals include rendezvous-ready through epoch release; full collection latency and requested-byte/root observations are also retained. These finite desktop cohorts are not hard pause bounds. Existing collector policy, budgets and ownership are unchanged.

## Compiler, fresh-process startup and code size

| Cost | G36 ms | G37 ms | Delta |
| --- | --- | --- | --- |
| cold/aot | 5.297 | 5.332 | +0.7% |
| cold/jit-normal | 3240.310 | 3206.494 | -1.0% |
| cold/jit-O2 | 3942.537 | 3895.249 | -1.2% |
| compile/compile-link | 3779.657 | 3753.179 | -0.7% |

| Image/archive | G36 bytes | G37 bytes | Added bytes | Delta |
| --- | --- | --- | --- | --- |
| libvoid_runtime.a | 4909188 | 4949204 | +40,016 | +0.815% |
| voidc | 80622904 | 80646520 | +23,616 | +0.029% |
| suite | 4990744 | 5006480 | +15,736 | +0.315% |

AOT startup additionally has 24 alternating fresh-process pairs. Compiler scaling uses 100/500/2000 construction sites with wall/CPU/RSS observations. No cache eviction or system-cold guarantee is claimed. The unchanged pinned Go reference is retained from G36 and labelled as reused, not remeasured.

## Numeric diagnostic controls

| Exact reused application object | G36 ms | G37 ms | Delta |
| --- | --- | --- | --- |
| integer | 25.995 | 30.692 | +18.1% |
| dependent | 26.815 | 28.836 | +7.5% |
| float | 26.588 | 26.314 | -1.0% |

The diagnostic links the exact baseline application object to each matching Runtime; only its unused main symbol is renamed. Eight alternating blocks/five samples per cell and separate counted images preserve poll/checkpoint assertions. Three numeric application bodies and polling leaves have independently compared instruction bytes and symbolic relocations. Hardware counters are retained when available; linked-context timing alone does not identify a cache/predictor cause.

## Native allocation pressure and memory

| Pressure case | G36 ms | G37 ms | Delta |
| --- | --- | --- | --- |
| large | 3.760 | 3.799 | +1.0% |
| mixed | 3.401 | 3.424 | +0.7% |
| retained | 6.113 | 6.166 | +0.9% |
| small | 2.779 | 2.818 | +1.4% |
| threads | 12.948 | 11.440 | -11.6% |

| Case | Variant | Completed epochs | End objects | End payload bytes | Cached bytes |
| --- | --- | --- | --- | --- | --- |
| large | base | 1023.0 | 2.0 | 131072.0 | 0.0 |
| large | candidate | 1023.0 | 2.0 | 131072.0 | 0.0 |
| mixed | base | 79.0 | 88.0 | 23056.0 | 15008.0 |
| mixed | candidate | 79.0 | 88.0 | 23056.0 | 15008.0 |
| retained | base | 40.0 | 544.0 | 8704.0 | 3584.0 |
| retained | candidate | 40.0 | 544.0 | 8704.0 | 3584.0 |
| small | base | 78.0 | 33.0 | 528.0 | 3584.0 |
| small | candidate | 78.0 | 33.0 | 528.0 | 3584.0 |
| threads | base | 77.0 | 242.0 | 3872.0 | 248.0 |
| threads | candidate | 78.0 | 33.0 | 528.0 | 3584.0 |

End payload is measured before the final explicit collection. Each case then asserts complete reclamation to zero live objects. Cached payload/node retention and Runtime/Heap/Frame layouts use the accepted B5 bounds and owners; no new production metadata structure, synchronization or heap allocation is introduced by the read specialization. Requested-byte epoch metrics, RSS and cache retention remain distinct observations.

## Regression assessment and next action

The final whole-application AOT numeric cells are slower despite unchanged numeric and polling instruction bodies/relocations. The exact reused-object diagnostic still reports integer about +18.1%, dependent +7.5% and float -1.0%; whole-suite contexts differ. Equal poll/checkpoint invocation counts and hardware instructions/branches separate added work from linked-context cycle costs. They do not establish a precise predictor/cache mechanism or make the regressions disappear. The complete first/final and initial/repeated width cohorts remain available; no favorable subset is substituted for the complete application observations.

Final JSON stopped p99 rises 1438.152→1551.002 us and max 1507.332→1638.331 us (192 intervals per variant); median rises about 966.380→989.345 us. Text stopped p99 changes 1608.261→1588.321 us (440 intervals), while its median rises 1032.609→1052.664 us. Collector source/ownership and operation/root counts are unchanged; these samples do not establish a GC-tail repair. Historical G34/G35/G36 adverse results remain active.

**Recommendation: retain this exact qualified source candidate as a separate reviewable optimization, rather than treating the current whole-compiler image as an accepted performance improvement.** The bounded code-generation capability is correct and real; the current numerical and whole-array costs need an explicit owner decision or further engineering. This recommendation is driven by substantial repeated costs, not rejection over a few percent in one unrelated control.

The next concrete engineering action is to measure and reduce duplicated code/exception-boundary cost from per-encoding specialization within the existing checked owner, using identical application objects and the same independent numeric/poll/cycle controls. Preserve exact error order and all authority/lifetime checks; do not tune padding/alignment or introduce another descriptor cache. In parallel, a future bounded fixed-scalar proof consolidation can target its repeated locked/layout validation, followed by current-catalog compatibility proofs for aggregate descriptors. Those are new qualification steps, not eligibility changes hidden in this candidate.

## Remaining engineering work

Aggregate/reference-element reads still reinterpret borrowed `NativeArrayElement` fields and resolve layouts; exact source keys alone do not prove compatibility with a catalog admitted through native input. The next extension needs a current-catalog compatibility proof for the existing emitted immutable descriptor, preserving admission and failure order, before eliminating that interpretation. No general immutable-descriptor cache is added here.

Dictionary operations still validate entry/key metadata and current policies, then execute existing hash/equality and bucket traversal. Stable sorting still pays source comparator dispatch and immutable snapshot/materialization costs. Text and JSON retain temporary buffers/strings, UTF-8 traversal, duplicate-name work and exact work/error budgets. Their operation counts and prepared/full timings determine the next local change; this scalar load specialization does not close those objectives.

Automatic SIMD beyond approved boundaries, broader escape analysis beyond G35, and future release/native-platform acceptance remain open. Historical G34 array/async/GC-tail, G35 JIT and G36 numeric/worker/JSON-tail results remain authoritative; a new timing sign does not establish a repair.

## Retained first-candidate findings

The first candidate's prepared source traversal improved 4.0% and selected native reads improved 3.2% with three roots, while full AOT array/4096 regressed 2.7%. The address-return helper introduced a redundant root-token argument copy and call; the corrected common-body template removes that concrete overhead. Both source patches, matched observations, native qualification receipts and emitted code remain recoverable.

First-candidate full-suite AOT integer/1M regressed 17.6%, dependent 9.4% and float 6.7%. The exact reused-object diagnostic measured integer +3.3%, dependent +8.3% and float -0.4%; dependent hardware cycles rose 9.14% while instructions and branches were effectively equal. Numeric and polling instruction bodies/relocations and call counts match. These controls establish a context-sensitive timing cost, not an added arithmetic operation or a proven cache/predictor cause. The source-read correction is not described as a numeric repair, and unfavorable first observations are retained.

Historical G34 AOT arrays +4.9%/1024 and +4.0%/4096, G34-I async +3.2–3.5%, G35 broad holder JIT +4.5–6.4%, and G36 narrow dependent/float +9.4%/+9.6% remain open acceptance evidence. G36 normal-JIT workers rose 27.7%; its JSON stopped p99 rose 1187.543→1782.240 us (192 intervals/variant), with max 1639.261→1804.040 us. New signs do not erase those historical costs. Original archives/reports remain protected.

## Harness corrections

The additional AOT fixture initially exposed its embedding `entry` instead of benchmark `main`; separating the application function name preserved all output assertions and passed the independent link/execution controls. An early IR parser greedily captured exception-personality names and produced empty maps; the rejected result is retained. The corrected parser asserts one baseline dynamic read, exactly eleven static reads and all four widths before accepting evidence. These corrections did not change the production candidate or weaken a safety assertion.

The final Windows Debug controller watchdog was handed off after JSON alone took 1091.80 seconds. CTest, its input/binary identities and all 60 substantive tests continued uninterrupted under a new bounded wait; real CTest JUnit and exit status are retained. Future replay uses a 3600-second outer selection watchdog. This changes controller scheduling, not source assertions, Runtime budgets or failure semantics.

## Qualification closeout

Corrected source qualification is complete: exact G36 baseline and corrected Linux Release 74/74; fatal ASan/UBSan/LeakSanitizer 74/74; relevant fatal TSan including source-read/catalog/GC races and independently linked normal/O2 AOT; native Windows 10 Release and Debug 60/60 each. The corrected concurrent source-read control passed 10,070 reads/314 collections under fatal sanitizers and 5,494 reads/171 collections under TSan. Expected stale storage still fails with `use-after-poison`. All build consumers match headers and the existing LLVM/Clang/LLD 23.1.4 SDK/providers.

The source patch was actually applied to six fresh G36 preimages and every restored postimage matched. The final qualification receipt is `linux/evidence/final-qualification.json`; the pending acceptance recommendation remains **KEEP SEPARATE** for the documented performance costs. Qualified correctness does not imply default performance acceptance. No source safety assertion or Runtime budget was weakened.

## Verified archive and cleanup closeout

The DEFLATE-9 archive retains **500 selected files plus one manifest**, **5,250,977 ZIP bytes**, SHA-256 `cc39715ce6feb1895ce1e504743d64bdd8330f112e913e451368244a769c4eee`. CRC, extracted/source hashes and fresh patch restoration and replay preparation passed. The [verification receipt](/home/shmellyorc/Documents/Void.Language-archives/g37-source-constant-checked-operations-2026-11-14-verification.json) and [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g37-source-constant-checked-operations-2026-11-14-cleanup.json) remain adjacent.

All task-owned Linux build groups, frozen consumers, Windows task source/build/transport roots are removed. Recorded cumulative cleanup-phase free-space increases total **8,462,454,784 bytes (7.881 GiB) Linux** and **10,621,214,720 bytes (9.892 GiB) Windows**. These are cumulative observed increases across reused build groups, not a claim of unique retained volume or exclusive filesystem activity; individual logical/allocated sizes and observations are separate in the receipts.

Consumer checks pass. Protected SDK/provider/toolchain hashes and all production source postimages are preserved after cleanup; previous archives remain intact. [Final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g37-source-constant-checked-operations-2026-11-14-final-Git-inventory.json) includes every changed/untracked file and tracked G32/G33 research reports. HEAD and the empty Git index are unchanged. No commit, push or hosted CI occurred.
