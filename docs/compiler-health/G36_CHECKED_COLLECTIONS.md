# G36 — Checked collections and everyday workloads

Accepted G35 baseline: `aa0f48e1998dc76397d238288f0b13669d0f3f2e`. **A real Runtime candidate is implemented and qualified; production acceptance remains subject to Golden Wrench review and Boss-controlled Git.** No staging, commit, push or hosted CI occurred.

## Implementation and ownership

The production changes extend existing Runtime operations. Backend, typed-IR, stdlib, managed-elision eligibility, collector, allocator, native ABI records, host budgets and polling policy are unchanged. All affected C++ compiler/Runtime/test consumers are rebuilt with matching headers and LLVM/Clang/LLD 23.1.4. The accepted baseline Runtime archive is byte-identical to G34/G35: `286dc4435f62cce434dd08cca9400b28ac10d9783fac5f0be7cc2c8f9e08d7c0`.

- `BufferLayout::packed` in `include/void/buffer.hpp` recognizes the already factory-validated immutable schema/capacity/base/stride. Checked list and fixed-scalar-source paths reuse that geometry. The original constructor/equivalence fallback preserves unsupported geometry and error behavior. Current catalog membership, root authority, completeness, live extent and nominal-header checks remain. This does not repeat G32-A's rejected scalar lookup extraction; `list.cpp` is unchanged.
- `BufferLayout::element_offset` retains its index/capacity check. The private constructor already checked base + stride * capacity, rounded allocation extent and address representability; immutable geometry plus index < capacity proves the offset multiplication/addition. The redundant division and its unreachable overflow branch disappear. All construction overflow checks remain.
- List insertion and dictionary write publish their existing temporary frames on the growth branch. In-place commits remain Managed under the original mutex and cross no safepoint/native callback/allocation requiring those temporary references. Growth captures references while Managed, registers them before its first allocation/native scope, and retains leases, revalidation, rollback and publication order. Initial polls and real precise roots remain.
- The immutable `RuntimeMetadata` catalog retains the canonical dictionary-policy index formerly rebuilt as a discarded admission set. Key views borrow strings owned by immutable nominal descriptors in the same catalog; copied catalogs retain those shared owners before copying the index. No caller descriptor or source-image string is borrowed. Admission validates duplicate policy identities and publishes types/schemas/witnesses/index together under the existing mutex. Each dictionary operation checks the **current** catalog index, including the nullable spelling, and retains per-node key rules and complete borrowed-native-descriptor validation. Late forbidden-policy admission still rejects an existing dictionary.
- Direct `make_shared` construction removes the extra const catalog temporary in `snapshot ? *snapshot : TypeCatalog{}`. The compiled probe records two copies before and one after. Private data-only temporary allocations/refcount work disappear; published metadata, descriptor identity and failure atomicity remain.
- Immutable strings were validated before publication by the existing construction paths. Scalar count now folds non-continuation bytes, allowing LLVM's existing vectorizer to generate SSE2 reduction instructions. Nonnegative scalar indexing traverses only its required prefix; negative indexing retains full count/normalization. Runtime object/extent checks, UTF-8 admission, integer-width rules, output staging and error recovery remain. No Unicode cache or second text engine is added.

`RuntimeMetadata` has one additional map in the existing catalog; its keys share descriptor ownership. Runtime/Heap/Frame/Slot layouts and root authority remain unchanged. The C++ metadata header change requires matching consumers. No synchronization is added. The accepted G33/G35 256-byte object / 16-KiB payload caps, G34 bounded reuse/poisoning, reduced roots, cold polling/TLS and one-owner GC pacing/surviving-payload accounting remain intact.

## Correctness and native qualification

Linux accepted G35: 74 selected controls pass, with one stale baseline reload gate repaired and rerun. Final G36 Release and fatal ASan/UBSan/LeakSanitizer: **74/74 each**. Relevant fatal TSan passes Runtime coordination, race/progress controls, live allocator pressure, simultaneous policy publication/forced GC, new geometry/Unicode/input-ownership controls and 26-pattern independently linked normal/O2 AOT. Cached stale storage still produces the required `use-after-poison` diagnostic.

Native Windows **10 build 19045**, VS2022 / SDK 10.0.19041.0: final Debug and Release **60/60 each**, maintained source/native controls, normal/O2 JIT and independent AOT oracles, pool/pacing assertions and eleven PE 1-MiB stack-header checks per profile. Protected SDK/provider/toolchain identities pass before/after qualification. The first candidate's Debug/Release 60/60 receipts are retained separately.

The focused maintained test covers all eleven scalar encodings, correct/wrong extents, padded backing, boundary/maximum unsigned offsets, forced GC, descriptor mutation/restoration at one address, later forbidden and nullable policies, conflicting admission rollback, caller policy-key reuse, all integer index widths, NUL and 1/2/3/4-byte UTF-8 scalars, extreme signed/unsigned/invalid indexes, invalid UTF-8 and retry. Existing arrays/dictionaries/sorting/text/JSON controls retain bounds/capacity/version/readonly/ordering, hash/equality, stable snapshots and callback ordering, resource budgets, fault sweeps, cleanup, reload and cancellation assertions.

The broader selection exposed an accepted-G35 test obstruction: an unused `callback_frame` was already eliminated, so an active-generation test waited for a construction that never occurred. The exact stalled source/thread state and interrupted run are retained. The fixture now passes its frame through a public observer, preserving the conservative heap boundary required by the gate. Every existing reload/capture/active-lease assertion remains. Native runner corrections for UNIX-only target selection and task-local Python helper resolution are retained as harness failures, with final passing receipts.

## Work removed and emitted code

Counts are medians of three observer-on invocations, separate from throughput. `search_frames` counts token validation's frame walk; root words count actual collection scans. Fewer temporary frames do not imply that those empty frames were previously scanned at a safepoint.

| Complete source operation | Counter | G35 | G36 |
| --- | --- | --- | --- |
| array construction/traversal 4096 | layout_creations | 12311 | 11 |
| array construction/traversal 4096 | runtime_frames | 4120 | 35 |
| array construction/traversal 4096 | search_frames | 16450 | 12354 |
| array construction/traversal 4096 | policy_queries | 0 | 0 |
| array construction/traversal 4096 | policy_nodes | 0 | 0 |
| array construction/traversal 4096 | managed_constructions | 12 | 12 |
| array construction/traversal 4096 | allocations | 1386 | 1386 |
| array construction/traversal 4096 | collections | 1 | 1 |
| array construction/traversal 4096 | root_words | 98 | 98 |
| prepared array traversal 4096 | layout_creations | 4097 | 0 |
| prepared array traversal 4096 | runtime_frames | 1 | 1 |
| prepared array traversal 4096 | search_frames | 4097 | 4097 |
| prepared array traversal 4096 | policy_queries | 0 | 0 |
| prepared array traversal 4096 | policy_nodes | 0 | 0 |
| prepared array traversal 4096 | managed_constructions | 0 | 0 |
| prepared array traversal 4096 | allocations | 0 | 0 |
| prepared array traversal 4096 | collections | 0 | 0 |
| prepared array traversal 4096 | root_words | 0 | 0 |
| dictionary construction/lookup 1024 | layout_creations | 26 | 26 |
| dictionary construction/lookup 1024 | runtime_frames | 1052 | 37 |
| dictionary construction/lookup 1024 | search_frames | 6204 | 5180 |
| dictionary construction/lookup 1024 | policy_queries | 5121 | 5121 |
| dictionary construction/lookup 1024 | policy_nodes | 1003716 | 0 |
| dictionary construction/lookup 1024 | managed_constructions | 19 | 19 |
| dictionary construction/lookup 1024 | allocations | 2466 | 2466 |
| dictionary construction/lookup 1024 | collections | 1 | 1 |
| dictionary construction/lookup 1024 | root_words | 100 | 100 |
| prepared dictionary lookup 1024 | layout_creations | 0 | 0 |
| prepared dictionary lookup 1024 | runtime_frames | 1 | 1 |
| prepared dictionary lookup 1024 | search_frames | 3073 | 3073 |
| prepared dictionary lookup 1024 | policy_queries | 3073 | 3073 |
| prepared dictionary lookup 1024 | policy_nodes | 602308 | 0 |
| prepared dictionary lookup 1024 | managed_constructions | 0 | 0 |
| prepared dictionary lookup 1024 | allocations | 0 | 0 |
| prepared dictionary lookup 1024 | collections | 0 | 0 |
| prepared dictionary lookup 1024 | root_words | 0 | 0 |
| text 1024 | layout_creations | 131603 | 2128 |
| text 1024 | runtime_frames | 46653 | 19746 |
| text 1024 | search_frames | 170257 | 143281 |
| text 1024 | policy_queries | 0 | 0 |
| text 1024 | policy_nodes | 0 | 0 |
| text 1024 | managed_constructions | 5233 | 5233 |
| text 1024 | allocations | 51869 | 51869 |
| text 1024 | collections | 11 | 11 |
| text 1024 | root_words | 2587 | 2587 |
| JSON 128 | layout_creations | 54433 | 800 |
| JSON 128 | runtime_frames | 44296 | 41742 |
| JSON 128 | search_frames | 69450 | 66739 |
| JSON 128 | policy_queries | 0 | 0 |
| JSON 128 | policy_nodes | 0 | 0 |
| JSON 128 | managed_constructions | 2495 | 2495 |
| JSON 128 | allocations | 54316 | 54316 |
| JSON 128 | collections | 5 | 5 |
| JSON 128 | root_words | 2607 | 2607 |



The prepared array creates no replacement layouts in the final checked hot path; dictionary nominal-policy scans disappear while the number of policy queries stays the same. Heap construction counts remain equal across G35/G36 for the maintained escape patterns and checked source operations. Negative root/schema/descriptor/policy/bounds controls remain active.

The complete representative application's LLVM IR and emitted application object are byte-identical between G35 and final G36: IR equality `True`, object equality `True`. All 52 normal/optimized G35 escape-fixture objects are compared independently. Runtime LLVM IR and selected object/final-image disassembly show the removed division, cold geometry fallback, growth-only frame calls, catalog-index lookup and vectorized immutable-string count. Source object identities, exact Runtime archives and consumer hashes accompany the patch. Final executables change because they link the changed Runtime.

## Matched whole-VOID performance

Linux x64, pinned affinity and 1-MiB execution stacks. Four balanced forward/reverse blocks, three warm observer-off samples/cell, independent output oracles. Compilation and performance samples do not overlap; Windows is paused for these measurements. The unchanged G31 corpus is rebuilt with the accepted G35 compiler/Runtime and the final candidate. Source numeric operations/polling are unchanged, so their timing shifts are controls rather than claimed source-algorithm improvements.

| Workload / size | G35 AOT ms | G36 AOT ms | AOT change | Normal JIT change | O2 JIT change |
| --- | --- | --- | --- | --- | --- |
| array/1024 | 1.211 | 1.086 | -10.3% | -8.4% | -13.2% |
| array/4096 | 4.825 | 4.268 | -11.5% | -7.2% | -14.9% |
| dependent/1000000 | 26.780 | 24.518 | -8.4% | -0.6% | +0.6% |
| dict/1024 | 9.801 | 4.027 | -58.9% | -65.6% | -66.9% |
| float/1000000 | 26.557 | 24.229 | -8.8% | +1.3% | +2.0% |
| integer/1000000 | 29.320 | 23.656 | -19.3% | -1.3% | +2.6% |
| integer/4000000 | 117.844 | 94.530 | -19.8% | -1.0% | +0.3% |
| io/200 | 6.907 | 7.021 | +1.6% | +0.0% | -1.0% |
| json/128 | 37.343 | 36.267 | -2.9% | -1.0% | -1.9% |
| json/32 | 6.029 | 5.824 | -3.4% | -0.8% | -3.2% |
| sort/1024 | 28.849 | 27.333 | -5.3% | -6.0% | -1.3% |
| sort/256 | 5.949 | 5.389 | -9.4% | -2.8% | -5.0% |
| text/1024 | 55.793 | 52.149 | -6.5% | -4.3% | -3.8% |
| text/256 | 13.875 | 12.814 | -7.6% | -5.0% | -4.4% |



Prepared G32 inputs/independent oracles are reused with fresh matching consumers. Twenty samples/cell, 16,384 native operations/sample; each row is a complete checked operation, not additive exclusive phase time. Native allocation counts run in separate warmed batches. The raw-memory reference has different contracts and is not equivalent VOID/Go performance.

| Prepared operation / roots | G35 ns/op | G36 ns/op | Change |
| --- | --- | --- | --- |
| dictionary_count_existing_token/128 | 2510.3 | 682.9 | -72.8% |
| dictionary_count_existing_token/3 | 2515.7 | 683.8 | -72.8% |
| dictionary_existing_token/128 | 2612.3 | 769.8 | -70.5% |
| dictionary_existing_token/3 | 2618.6 | 768.4 | -70.7% |
| dictionary_source_abi/128 | 2742.2 | 872.4 | -68.2% |
| dictionary_source_abi/3 | 2660.4 | 813.2 | -69.4% |
| fixed_existing_token_layout/128 | 79.2 | 78.8 | -0.5% |
| fixed_existing_token_layout/3 | 78.0 | 79.1 | +1.5% |
| fixed_source_abi/128 | 331.5 | 316.9 | -4.4% |
| fixed_source_abi/3 | 263.9 | 260.7 | -1.2% |
| list_existing_token/128 | 269.8 | 258.8 | -4.1% |
| list_existing_token/3 | 271.0 | 251.9 | -7.0% |
| list_source_abi/128 | 363.1 | 353.6 | -2.6% |
| list_source_abi/3 | 304.8 | 282.6 | -7.3% |
| native_entry_metadata/128 | 159.2 | 160.1 | +0.5% |
| native_entry_metadata/3 | 160.7 | 159.2 | -0.9% |
| native_key_metadata/128 | 174.4 | 172.5 | -1.1% |
| native_key_metadata/3 | 173.2 | 173.6 | +0.2% |
| raw_scalar_reference_only/128 | 0.5 | 0.5 | -0.1% |
| raw_scalar_reference_only/3 | 0.5 | 0.5 | +0.1% |
| root_admission/128 | 89.0 | 85.2 | -4.3% |
| root_admission/3 | 33.7 | 32.0 | -5.2% |
| source_prepared_array_4096/0 | 1592890.9 | 1499626.5 | -5.9% |
| source_prepared_dictionary_1024/0 | 8212424.5 | 2486854.6 | -69.7% |



Additional source Unicode controls use identical bytes, iteration counts and independent oracles across variants. They include existing source constant materialization and string allocation, rather than claiming a pure decoder-only timing:

| Source operation | Mode | G35 ms | G36 ms | Change |
| --- | --- | --- | --- | --- |
| unicode-count | aot-normal | 20.182 | 15.041 | -25.5% |
| unicode-count | aot-optimized | 19.722 | 14.968 | -24.1% |
| unicode-count | jit-O2 | 19.697 | 15.070 | -23.5% |
| unicode-count | jit-normal | 19.784 | 15.114 | -23.6% |
| unicode-prefix | aot-normal | 21.568 | 13.230 | -38.7% |
| unicode-prefix | aot-optimized | 21.452 | 13.178 | -38.6% |
| unicode-prefix | jit-O2 | 20.507 | 13.418 | -34.6% |
| unicode-prefix | jit-normal | 20.701 | 13.423 | -35.2% |



Managed calls, closures, references, worker and async controls:

| Managed/control | Mode | G35 ms | G36 ms | Change |
| --- | --- | --- | --- | --- |
| calls | aot | 0.692 | 0.659 | -4.8% |
| managed-escape | aot | 7.037 | 7.442 | +5.7% |
| managed-local | aot | 0.558 | 0.510 | -8.7% |
| managed-survives-allocation | aot | 42.587 | 41.893 | -1.6% |
| async-tasks | jit-O2 | 1.674 | 1.606 | -4.1% |
| calls | jit-O2 | 0.774 | 0.823 | +6.4% |
| closures | jit-O2 | 1.290 | 1.412 | +9.4% |
| managed-across-edge | jit-O2 | 1.030 | 1.089 | +5.7% |
| managed-escape | jit-O2 | 6.133 | 5.492 | -10.5% |
| managed-local | jit-O2 | 0.597 | 0.639 | +6.9% |
| managed-reference | jit-O2 | 0.604 | 0.653 | +8.1% |
| managed-survives-allocation | jit-O2 | 38.807 | 38.550 | -0.7% |
| workers | jit-O2 | 0.759 | 0.755 | -0.4% |
| async-tasks | jit-normal | 1.677 | 1.650 | -1.6% |
| calls | jit-normal | 0.777 | 0.828 | +6.6% |
| closures | jit-normal | 1.441 | 1.584 | +9.9% |
| managed-across-edge | jit-normal | 1.051 | 1.144 | +8.9% |
| managed-escape | jit-normal | 5.788 | 5.448 | -5.9% |
| managed-local | jit-normal | 0.592 | 0.640 | +8.3% |
| managed-reference | jit-normal | 0.600 | 0.645 | +7.4% |
| managed-survives-allocation | jit-normal | 39.111 | 38.664 | -1.1% |
| workers | jit-normal | 0.668 | 0.853 | +27.7% |



Accepted G35 escape-analysis AOT controls (normal/O2 JIT and counts are also retained):

| G35 escape pattern / AOT | Heap constructions G35 -> G36 | G35 ms | G36 ms | Change |
| --- | --- | --- | --- | --- |
| branch_alias | 0 -> 0 | 0.765 | 0.791 | +3.5% |
| branch_birth | 0 -> 0 | 0.978 | 1.057 | +8.1% |
| nullable | 0 -> 0 | 0.761 | 0.793 | +4.3% |
| references | 20000 -> 20000 | 4.384 | 4.788 | +9.2% |
| nested | 40000 -> 40000 | 10.126 | 10.505 | +3.7% |
| field_rebind | 40000 -> 40000 | 7.771 | 7.933 | +2.1% |
| alias_rebind | 40000 -> 40000 | 7.733 | 7.852 | +1.5% |
| identity | 40000 -> 40000 | 8.056 | 8.397 | +4.2% |
| loop_carried | 20001 -> 20001 | 4.185 | 4.083 | -2.4% |
| helper_return | 20000 -> 20000 | 4.786 | 4.882 | +2.0% |
| helper_read | 0 -> 0 | 0.634 | 0.678 | +6.9% |
| helper_write | 0 -> 0 | 0.629 | 0.661 | +5.1% |
| helper_reference | 20000 -> 20000 | 4.953 | 5.051 | +2.0% |
| public_helper | 20000 -> 20000 | 4.908 | 5.041 | +2.7% |
| helper_generic | 0 -> 0 | 0.635 | 0.662 | +4.3% |
| reference_effects | 40000 -> 40000 | 12.388 | 12.623 | +1.9% |
| readonly_helper | 0 -> 0 | 0.675 | 0.663 | -1.8% |
| payload_budget | 1 -> 1 | 0.002 | 0.002 | +3.9% |
| context_control | 20000 -> 20000 | 5.371 | 5.320 | -0.9% |
| null_failure | 20000 -> 20000 | 4.130 | 4.354 | +5.4% |
| numeric_failure | 0 -> 0 | 0.499 | 0.522 | +4.7% |
| incoming_reference | 40000 -> 40000 | 10.975 | 11.520 | +5.0% |
| incoming_rebind | 60000 -> 60000 | 15.435 | 15.289 | -0.9% |
| unknown_factory | 40000 -> 40000 | 10.643 | 10.832 | +1.8% |
| outlined_array | 2 -> 2 | 0.005 | 0.005 | -1.5% |
| direct_reference | 40000 -> 40000 | 9.531 | 9.668 | +1.4% |



## Numeric regressions and unfavorable controls

The first owning-key candidate regressed full-suite AOT floating arithmetic by **+6.2%**, with opposite integer movement. Its narrow float/dependent controls also regressed about 9.6%/9.3%, while integer improved about 10.5%. Hardware counters showed roughly 9–10% more cycles for float/dependent with nearly identical instructions/branches and equal poll/checkpoint counts. Its exact source patch, raw timings, metadata costs, native receipts, counters and emitted code remain recoverable. The final candidate changes key ownership and removes a demonstrated catalog copy; no padding, alignment workaround, eligibility change or benchmark input adjustment is used to force timing.

Both variants link the exact same application object for the narrow numeric diagnostic; only the unused application `main` symbol is renamed for a C++ caller. Eight balanced blocks, five timed samples/cell, separate counting images. GC-poll and worker-checkpoint call counts match exactly. `Runtime::poll`, `vd_gc_poll` and `vd_worker_checkpoint` have identical object instruction bytes and symbolic relocations. Exact code identities and the additional four-block hardware counters separate instructions, cycles, branches and branch misses.

| Exact-object narrow control | G35 ms | G36 ms | Change |
| --- | --- | --- | --- |
| integer | 28.946 | 25.941 | -10.4% |
| dependent | 24.465 | 26.771 | +9.4% |
| float | 24.157 | 26.466 | +9.6% |



The final narrow float/dependent controls remain roughly 9–10% slower even when their full-suite cells move in the opposite direction. These unfavorable results are acceptance tradeoffs, not erased by aggregate gains.

These controls investigate linked-context sensitivity, not a new arithmetic optimization. Final raw data retain every adverse block. Similar instruction/branch counts with different cycles do not establish a particular cache/predictor mechanism; no hardware-specific explanation is asserted without its supporting counter. The full-suite, narrow, normal-JIT and O2-JIT results are all available for review.

## GC, memory and retained state

Actual allocation/native-request/root/GC/live-payload/requested-byte/RSS observations are retained for all 26 G35 patterns and the checked operations. Exact-source-corpus admission uses the real 182 nominal records/15,572 canonical-key bytes; the 0/16/196-policy control is separately labeled synthetic. Requested bytes are not a physical/resident-memory bound. Existing reuse-cache payload capacity 557,056 bytes and 98,368-byte Data storage are unchanged. Metadata index storage follows catalog snapshot lifetime; it is not permanent global storage. Both actual and synthetic controls assert zero observed live allocations after Runtime destruction.

| Variant / control | Stage | Retained requested bytes | Peak requested bytes | Allocation requests |
| --- | --- | --- | --- | --- |
| base / metadata-memory | metadata n=0 sizeof_catalog=144 after_first G33 0,0,0 pauses= stopped= | 98368 | 98368 | 1 |
| base / metadata-memory | metadata n=0 after_repeat G33 0,0,0 pauses= stopped= | 98368 | 98368 | 1 |
| base / metadata-memory | metadata n=16 sizeof_catalog=144 after_first G33 0,0,0 pauses= stopped= | 104194 | 107214 | 178 |
| base / metadata-memory | metadata n=16 after_repeat G33 0,0,0 pauses= stopped= | 104194 | 113264 | 387 |
| base / metadata-memory | metadata n=196 sizeof_catalog=144 after_first G33 0,0,0 pauses= stopped= | 168562 | 205974 | 2158 |
| base / metadata-memory | metadata n=196 after_repeat G33 0,0,0 pauses= stopped= | 168562 | 278912 | 4707 |
| base / metadata-source | SOURCE_ADMISSION repeat=0 | 235037 | 395854 | 3425 |
| base / metadata-source | SOURCE_ADMISSION repeat=1 | 235037 | 532523 | 7212 |
| candidate / metadata-memory | metadata n=0 sizeof_catalog=192 after_first G33 0,0,0 pauses= stopped= | 98368 | 98368 | 1 |
| candidate / metadata-memory | metadata n=0 after_repeat G33 0,0,0 pauses= stopped= | 98368 | 98368 | 1 |
| candidate / metadata-memory | metadata n=16 sizeof_catalog=192 after_first G33 0,0,0 pauses= stopped= | 105138 | 106760 | 162 |
| candidate / metadata-memory | metadata n=16 after_repeat G33 0,0,0 pauses= stopped= | 105138 | 113578 | 323 |
| candidate / metadata-memory | metadata n=196 sizeof_catalog=192 after_first G33 0,0,0 pauses= stopped= | 179586 | 199664 | 1962 |
| candidate / metadata-memory | metadata n=196 after_repeat G33 0,0,0 pauses= stopped= | 179586 | 280930 | 3923 |
| candidate / metadata-source | SOURCE_ADMISSION repeat=0 | 245277 | 378874 | 3243 |
| candidate / metadata-source | SOURCE_ADMISSION repeat=1 | 245277 | 525879 | 6485 |



The first owning-key index retained 28,724 extra requested bytes for the actual source corpus and made repeated catalog copies more expensive. Final descriptor-owned views remove duplicate canonical strings, and direct construction removes one whole catalog temporary. The retained map-node cost and actual application RSS remain explicit rather than being hidden in managed payload accounting.

Full stopped intervals run from ready rendezvous through epoch release, including policy/accounting work. Collection latency includes rendezvous; both series are in raw results. Nearest-rank p95/p99 and maxima are desktop observations, not hard latency promises:

| Case / variant | N | Median us | p95 us | p99 us | Max us |
| --- | --- | --- | --- | --- | --- |
| text-1024 / base | 440 | 860.9 | 1225.7 | 1332.2 | 1509.2 |
| text-1024 / candidate | 440 | 810.5 | 1174.4 | 1227.1 | 1321.1 |
| json-128 / base | 192 | 815.5 | 1057.4 | 1187.5 | 1639.3 |
| json-128 / candidate | 192 | 794.2 | 1094.4 | 1782.2 | 1804.0 |



In the final JSON cohort, stopped-time p99 rises from about 1.188 ms to 1.782 ms (192 intervals/variant), with max 1.639 -> 1.804 ms. The median improves slightly while the adverse tail remains. Text's sampled tail improves. The paired raw intervals and collection-latency distributions are retained; this finite desktop cohort does not establish a hard pause bound.

Historical G34 AOT-array/async/GC-tail costs and G35 JIT-holder observations remain in the permanent ledger and previous verified evidence. G36 compares against that accepted baseline and does not relabel those historical costs as solved. Final policy/payload/collection controls retain the same host budgets and accounting owner.

## Go, compilation and startup context

Fresh pinned Go 1.27.4 reference from the prior approved SHA-256, unchanged source/oracles, `-trimpath`, `GOAMD64=v1`, `CGO_ENABLED=0`, `GOTOOLCHAIN=local`, `GOMAXPROCS=1`, `GOGC=100`. The same rebuilt comparator is reused across the two candidate campaigns. Text/JSON/I/O implementation and collector differences retain the original G31 comparability qualifications. These ratios do not claim semantic parity or a language-wide ranking.

| Workload | Go 1.27.4 ms | G36 AOT ms | G36 / Go |
| --- | --- | --- | --- |
| array/1024 | 0.004 | 1.086 | 297.9x |
| array/4096 | 0.019 | 4.268 | 222.1x |
| dependent/1000000 | 1.177 | 24.518 | 20.8x |
| dict/1024 | 0.079 | 4.027 | 50.9x |
| float/1000000 | 2.025 | 24.229 | 12.0x |
| integer/1000000 | 1.086 | 23.656 | 21.8x |
| integer/4000000 | 4.066 | 94.530 | 23.2x |
| io/200 | 6.123 | 7.021 | 1.1x |
| json/128 | 0.108 | 36.267 | 335.0x |
| json/32 | 0.030 | 5.824 | 196.3x |
| sort/1024 | 0.042 | 27.333 | 650.7x |
| sort/256 | 0.009 | 5.389 | 595.2x |
| text/1024 | 0.086 | 52.149 | 604.0x |
| text/256 | 0.022 | 12.814 | 584.6x |



| Cost | G35 ms | G36 ms | Change |
| --- | --- | --- | --- |
| cold / aot | 5.349 | 4.983 | -6.8% |
| cold / jit-normal | 3148.353 | 3093.310 | -1.7% |
| cold / jit-O2 | 3791.553 | 3790.855 | -0.0% |
| compile / compile-link | 3708.071 | 3638.108 | -1.9% |



The independent 24-pair cold-AOT medians are 4.949 -> 4.646 ms. Compile/JIT scaling includes frontend, validation, LLVM, metadata admission and one checked invocation; the compiler analysis algorithm is unchanged:

| Construction sites | Wall ms G35 -> G36 | CPU ms | Peak RSS KiB | Wall change |
| --- | --- | --- | --- | --- |
| 100 | 52.2 -> 51.9 | 50.6 -> 50.3 | 46900 -> 46936 | -0.5% |
| 500 | 269.8 -> 266.9 | 266.6 -> 263.9 | 60104 -> 60148 | -1.0% |
| 2000 | 2202.0 -> 2135.1 | 2182.6 -> 2117.6 | 117244 -> 117116 | -3.0% |



Exact binary/archive sizes, process CPU/RSS, source input identities and all raw samples accompany these summaries.

## Complexity, remaining work and review recommendation

Removed repeated layout construction, an unreachable per-offset division, common-path temporary frames, full nominal scans per dictionary operation, duplicate policy strings, a whole catalog temporary and redundant immutable-string decoding/full indexing scans. Added one owned catalog index and one small private geometry predicate; no new locks, root authority, allocator, optimizer framework or string/JSON engine. Sorting algorithm, comparator calls, JSON work-budget charging and dictionary hash/equality are unchanged.

**Recommend advancing the final bounded candidate to Golden Wrench review with its documented memory and numeric/context tradeoffs.** Do not mark checked collections or everyday performance complete. Remaining work: qualified source-constant descriptor lowering to reduce repeated native metadata interpretation/ABI boundaries; broader escape coverage beyond G35; dictionary lookup/hash and sorting callback costs; text byte accumulation/temporary materialization; JSON duplicate-name traversal with exact budget/error ordering; and automatic SIMD opportunities in ordinary VOID loops. The next engineering action is a bounded source-constant descriptor path using validated layouts, retaining per-operation current-root, catalog and mutation checks and the general mutable-native-descriptor fallback.

## Evidence, Git and cleanup

[Exact source patch](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14.patch), [verified archive](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14.zip) and [permanent ledger](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md) retain selected reproducible inputs/oracles, exact identities, raw results, critical failures, LLVM IR/disassembly, replay and qualification receipts. The first candidate and negative observations are labeled separately. Build trees, caches, SDK/provider duplicates and routine objects are excluded. DEFLATE-9/manifest/SHA-256 extraction and source restoration precede task-only cleanup. Final retained count, compressed size and observed Linux/Windows reclamation are appended after verified cleanup.

G32/G33 reports remain tracked/preserved, with the ledger intentionally updated. [Final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14-final-Git-inventory.json) covers every changed/untracked file and those research hashes. HEAD/index are unchanged; nothing is staged, committed, pushed or dispatched to hosted CI. The archived report is the qualification snapshot; adjacent closeout receipts cover the final documentation append and cleanup.

## Verified archive and cleanup closeout

The DEFLATE-9 archive retains **420 selected files plus one manifest**, **3,879,722 ZIP bytes**, SHA-256 `c0bdf49c6f34ea9f8c325677b84731630d5b5d9a827dabc88e65e683c23adeda`. CRC, extracted/source hashes and fresh patch restoration and replay preparation passed. The [verification receipt](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14-verification.json) and [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14-cleanup.json) remain adjacent.

All task-owned Linux build groups, frozen consumers, task-local Go SDK/cache and Windows task source/build/transport roots are removed. Recorded cumulative cleanup-phase free-space increases total **8,706,707,456 bytes (8.109 GiB) Linux** and **10,620,878,848 bytes (9.891 GiB) Windows**. These are cumulative observed increases across reused build groups, not a claim of unique retained volume or exclusive filesystem activity; individual logical/allocated sizes and observations are separate in the receipts.

Consumer checks pass. Protected SDK/provider/toolchain hashes and all production source postimages are preserved after cleanup; previous archives remain intact. [Final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g36-checked-collections-2026-11-14-final-Git-inventory.json) includes every changed/untracked file and tracked G32/G33 research reports. HEAD and the empty Git index are unchanged. No commit, push or hosted CI occurred.
