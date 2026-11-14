# G35 — Managed escape-analysis extensions

Starting/accepted G34 baseline: `5e840f793d63aef0de2715d2aee1cb083c9d21eb`. **A real compiler candidate is implemented and natively qualified. Production acceptance remains subject to Golden Wrench review and Boss-controlled Git.** Nothing is staged, committed, pushed or sent to hosted CI.

## Eligibility implemented

The existing owned `block_allocation_elision` mechanism now follows single-origin local aliases through ordinary synchronous CFG edges. A must-dataflow check resets alias validity at each construction and rejects any load of a previous construction instance after backing storage is reused. Objects may remain live across branches/loops; each construction site owns separate fixed backing storage. Returning, publishing, capturing, identity inspection, unsupported aliases and rebinding to another origin retain heap allocation. The existing 256-byte object and 16-KiB eligible payload budget per function remain.

Reference-containing owners support numeric/bool fields and ordinary managed child references, including null, nested heap children, aliases and child-field replacement. Initial and replacement children must be proved current-heap values: null, locally constructed heap values/aliases, or bounded private straight-line factories whose successful returns are fresh heap objects. Incoming pointers, field-derived/unproven values and unknown/public factories fall back to heap construction. A reference owner must publish immediately to an existing owning-local home; its function-duration payload roots mirror that home's original retention. Direct reference temporaries require a separate shorter-lifetime proof and remain allocated.

Known same-module private helpers support one non-null managed formal plus scalar formals, scalar/void results, at most 64 straight-line instructions, checked nonretention/return effects and single-origin formal aliases. Readonly authority and scalar field mutation are preserved. The compiler reuses **existing construction-view signatures and origin validation**, generates only selected private variants, and independently revalidates them. Original functions/symbols remain for all other calls. Known concrete generic helpers are covered. Public, unknown, virtual, receiver, native, callback, worker-wrapper and async helper targets remain conservative. Existing construction-view names are not redirected to an unproven namesake.

Async bodies, receiver/initializer bodies and cleanup/release-bearing functions retain the prior conservative fallback. Reference graphs made of multiple stack-resident nodes, arbitrary formal-derived references, multiple-origin rebinding, broad interprocedural summaries and earlier last-use de-rooting are not implemented.

## Source and GC/root correctness

Production implementation changes only `src/backend/llvm/backend.cpp`; CMake and maintained tests add qualification. **Runtime sources and public compiler/Runtime headers are unchanged.** Fresh baseline and candidate Runtime archives both have SHA-256 `286dc4435f62cce434dd08cca9400b28ac10d9783fac5f0be7cc2c8f9e08d7c0`. B5 reuse/poisoning, C2 precise roots, D4 polling/TLS/empty frames, E3 pacing/sweep accounting, explicit host budgets, native interoperability, cancellation and reload remain intact. LLVM/Clang/LLD 23.1.4 is used throughout.

Stack owner addresses are omitted from managed-pointer homes. Every payload child word instead remains in an ordinary precise `Runtime::Slot`, initialized to null before frame entry and updated by the original ordered field stores. Payload fields share the existing target-layout local-root aggregate and its exact offset map. This avoids one additional Slot per object without changing root authority or adding Runtime state, a collector, allocator, cache or optimizer framework. Child objects remain actual heap objects; publishing a would-be stack child into a field rejects that child's elision. Parameter construction views use their existing aggregate-field root contract.

Initializer evaluation/order, nullable checks, numeric errors, readonly checks and all remaining allocations stay active. The accepted OOM rule removes only the eliminated allocation's OOM possibility. The immediate-owning-home condition prevents extending child retention beyond the corresponding baseline local root. The 1-MiB native stack and payload caps are tested; payload allocas are function-entry storage, never dynamically accumulated by loop iterations.

## Qualification and maintained assertions

Linux: accepted-baseline Release 40 maintained tests; candidate Release and fatal ASan/UBSan/LeakSanitizer **45/45** each, with all 26 new pattern oracles in normal/O2 JIT and independently linked normal/optimized AOT. Native reuse, pacing and race controls pass, including the required stale cached-storage `use-after-poison` diagnostic. The additional TSan consumer leaves compiler-rt in control of new/delete; its forced-GC/count/root checks are separate from replacement-allocator fault injection, which remains fully qualified in Release and fatal ASan. No duplicate-symbol suppression or sanitizer suppression is used. Fatal TSan passes Runtime coordination, races, live allocator pressure and the full 26-pattern independently linked normal/O2 AOT consumers.

Native Windows **10 build 19045**, VS2022/SDK 10.0.19041.0, LLVM/Clang/LLD 23.1.4: Debug and Release **45/45**, source/native/JIT/AOT oracles, allocation/pacing controls and eleven PE 1-MiB stack-header checks per profile pass. Final source exports contain the exact candidate postimages and no overlays. All affected compiler/test consumers are rebuilt with matching headers. Protected SDK/provider/helper hashes are rechecked.

Original G33-A fixture sources and substantive assertions remain. Two explicitly newly eligible fixtures (`reference`, `across_edge`) change expected construction counts to zero; the published holder boundary now asserts one retained heap child and an elided holder. The hot-reload fixture now passes its object through a public helper and explicitly requires one captured heap object, preserving every existing retention/reload/layout/value assertion. This keeps the control meaningful after reference-bearing locals become eligible.

The new controls cover branch birth/aliases, loop instances, nested/null/replaced references, readonly and generic helpers, initializer order, 256-byte/16-KiB limits, identity/rebinding/escape fallbacks, incoming/unproven reference fallbacks, nullable outlined arrays, remaining null/division errors, forced collection, cold fault injection and recovery. Foreign-child death controls require the exact original current-heap rejection in normal/O2 AOT. All count and root/publication assertions remain enabled.

## Allocation and emitted-code evidence

Counts below are medians from three observation samples per mode, separate from observer-off timing. Every pattern is checked in normal/O2 JIT and independent AOT. `payload_budget` executes 65 constructions per call; `outlined_array` executes two retained buffer constructions and zero managed-object constructor ABI calls. Native requests include real allocation requests; they are distinct from heap constructions/cache hits.

| Pattern, n=20,000 (fixed-size cases noted) | Heap constructions | Native requests | Collections | Root words |
| --- | --- | --- | --- | --- |
| branch_alias | 20000 → 0 | 624 → 0 | 78 → 0 | 546 → 0 |
| branch_birth | 10000 → 0 | 338 → 0 | 39 → 0 | 195 → 0 |
| nullable | 20000 → 0 | 40312 → 0 | 78 → 0 | 390 → 0 |
| references | 40000 → 20000 | 41560 → 780 | 156 → 78 | 2340 → 780 |
| nested | 60000 → 40000 | 82652 → 41560 | 234 → 156 | 3744 → 2340 |
| field_rebind | 60000 → 40000 | 42964 → 1560 | 234 → 156 | 3978 → 1872 |
| alias_rebind | 40000 → 40000 | 1248 → 1248 | 156 → 156 | 1092 → 1092 |
| identity | 40000 → 40000 | 1248 → 1248 | 156 → 156 | 1248 → 1248 |
| loop_carried | 20001 → 20001 | 624 → 624 | 78 → 78 | 390 → 390 |
| helper_return | 20000 → 20000 | 468 → 468 | 78 → 78 | 312 → 312 |
| helper_read | 20000 → 0 | 624 → 0 | 78 → 0 | 390 → 0 |
| helper_write | 20000 → 0 | 624 → 0 | 78 → 0 | 390 → 0 |
| helper_reference | 40000 → 20000 | 41560 → 624 | 156 → 78 | 1872 → 624 |
| public_helper | 20000 → 20000 | 624 → 624 | 78 → 78 | 390 → 390 |
| helper_generic | 20000 → 0 | 624 → 0 | 78 → 0 | 390 → 0 |
| reference_effects | 60000 → 40000 | 43120 → 1872 | 234 → 156 | 4680 → 3432 |
| readonly_helper | 20000 → 0 | 624 → 0 | 78 → 0 | 390 → 0 |
| payload_budget | 65 → 1 | 261 → 4 | 0 → 0 | 0 → 0 |
| context_control | 20000 → 20000 | 624 → 624 | 78 → 78 | 390 → 390 |
| null_failure | 20001 → 20000 | 626 → 468 | 78 → 78 | 702 → 390 |
| numeric_failure | 1 → 0 | 4 → 0 | 0 → 0 | 0 → 0 |
| incoming_reference | 40000 → 40000 | 40936 → 40936 | 156 → 156 | 468 → 468 |
| incoming_rebind | 60000 → 60000 | 42028 → 42028 | 234 → 234 | 3198 → 3198 |
| unknown_factory | 40000 → 40000 | 41248 → 41248 | 156 → 156 | 1248 → 1248 |
| outlined_array | 2 → 2 | 26 → 26 | 0 → 0 | 0 → 0 |
| direct_reference | 40000 → 40000 | 41560 → 41560 | 156 → 156 | 1404 → 1404 |


All 26 fixture objects, in both emission modes, are **byte-identical between the ordinary emitter and the observation-only IR emitter**. Exact object identities, LLVM IR and AOT disassembly are archived. The calls witness changes baseline one frame/one construction to zero/zero while preserving **5,003 polls** in normal/O2 JIT. Escaping/identity controls retain real heap constructions and every remaining OOM/null/arithmetic outcome.

## Baseline versus candidate performance

Four alternating blocks, three warm samples/cell, observer-off authority. New pattern results use 20,000 iterations except fixed-size controls. Normal/O2 JIT and independent AOT are separate; raw CPU/RSS/GC/live-payload data and unfavorable samples are retained. No Linux build overlaps timing; native Windows is paused only during a compiler build or after its qualification completes.

| Pattern / AOT | G34 ms | G35 ms | Change |
| --- | --- | --- | --- |
| branch_alias | 4.430 | 0.764 | -82.8% |
| branch_birth | 2.929 | 1.019 | -65.2% |
| nullable | 5.906 | 0.760 | -87.1% |
| references | 9.711 | 4.487 | -53.8% |
| nested | 15.961 | 10.202 | -36.1% |
| field_rebind | 13.719 | 7.863 | -42.7% |
| alias_rebind | 7.876 | 7.675 | -2.5% |
| identity | 8.011 | 7.933 | -1.0% |
| loop_carried | 4.193 | 3.999 | -4.6% |
| helper_return | 4.815 | 4.796 | -0.4% |
| helper_read | 5.067 | 0.630 | -87.6% |
| helper_write | 5.075 | 0.650 | -87.2% |
| helper_reference | 10.464 | 4.971 | -52.5% |
| public_helper | 5.245 | 4.793 | -8.6% |
| helper_generic | 5.054 | 0.657 | -87.0% |
| reference_effects | 19.763 | 12.706 | -35.7% |
| readonly_helper | 5.294 | 0.678 | -87.2% |
| payload_budget | 0.044 | 0.002 | -95.8% |
| context_control | 5.716 | 5.284 | -7.5% |
| null_failure | 4.430 | 4.341 | -2.0% |
| numeric_failure | 0.526 | 0.508 | -3.5% |
| incoming_reference | 11.213 | 11.635 | +3.8% |
| incoming_rebind | 15.150 | 15.244 | +0.6% |
| unknown_factory | 10.422 | 10.749 | +3.1% |
| outlined_array | 0.005 | 0.005 | -1.1% |
| direct_reference | 9.315 | 9.553 | +2.6% |


Representative unchanged controls:

| Representative AOT | G34 ms | G35 ms | Change |
| --- | --- | --- | --- |
| array/1024 | 1.232 | 1.239 | +0.6% |
| array/4096 | 5.060 | 4.887 | -3.4% |
| dependent/1000000 | 27.288 | 26.865 | -1.6% |
| dict/1024 | 9.775 | 9.944 | +1.7% |
| float/1000000 | 26.895 | 26.630 | -1.0% |
| integer/1000000 | 29.604 | 29.925 | +1.1% |
| integer/4000000 | 118.950 | 119.259 | +0.3% |
| io/200 | 7.081 | 6.902 | -2.5% |
| json/128 | 37.462 | 37.390 | -0.2% |
| json/32 | 5.943 | 5.901 | -0.7% |
| sort/1024 | 29.228 | 29.283 | +0.2% |
| sort/256 | 6.022 | 6.048 | +0.4% |
| text/1024 | 53.993 | 53.952 | -0.1% |
| text/256 | 13.277 | 13.112 | -1.2% |


The complete representative G31 **AOT executable and optimized LLVM IR are byte-identical** between G34 and G35. Their timing movements are controls, not G35 runtime improvements. Other per-object unchanged controls and exact standalone identities are recorded; no speedup is attributed to unchanged emitted code.

| Managed/control mode | G34 ms | G35 ms | Change |
| --- | --- | --- | --- |
| calls / aot | 0.648 | 0.692 | +6.7% |
| managed-escape / aot | 7.189 | 7.351 | +2.3% |
| managed-local / aot | 0.559 | 0.560 | +0.2% |
| managed-survives-allocation / aot | 43.483 | 42.761 | -1.7% |
| async-tasks / jit-O2 | 1.679 | 1.648 | -1.9% |
| closures / jit-O2 | 1.355 | 1.417 | +4.6% |
| managed-across-edge / jit-O2 | 4.898 | 1.051 | -78.5% |
| managed-reference / jit-O2 | 6.217 | 0.622 | -90.0% |
| workers / jit-O2 | 0.683 | 0.701 | +2.7% |
| async-tasks / jit-normal | 1.709 | 1.632 | -4.5% |
| closures / jit-normal | 1.358 | 1.529 | +12.6% |
| managed-across-edge / jit-normal | 4.730 | 1.111 | -76.5% |
| managed-reference / jit-normal | 5.957 | 0.622 | -89.6% |
| workers / jit-normal | 0.641 | 0.702 | +9.6% |


## Regressions and corrections retained

1. The first broader reference proof omitted Runtime's child-heap ownership rejection. An exact two-Runtime AOT witness reproduced baseline rejection versus incorrect candidate success in both modes. The final proof propagates unproven ownership through the existing alias-use index, requires proved current-heap initial/replacement children and retains heap fallback otherwise. Exact failed source patch and raw failure/recovery evidence are preserved.
2. Generated nullable-array default helpers lacked a formal-effect summary; a new control reproduced `map::at`. Fresh-result qualification now skips helpers without that summary. Baseline/candidate output and emitted-code controls cover the correction.
3. Separate payload Slots added an unnecessary containment test to every remaining allocation. A long-lived holder showed roughly 7–9% regressions despite eliminating one construction. Packing payloads into the existing local Slot reduces its descriptor count **3→2**, preserves the eligibility-helper hash and removes that extra lookup work. Pre-packing measurements, exact source and object remain archived. The final JIT results can still be unfavorable; all modes are reported, not silently narrowed.

The diagnostic relinks below use ordinary flags and natural data layout. Four unchanged standalone controls link to **byte-identical executables** between baseline and candidate, while the combined benchmark bundle has different placement because eligible cases change code. The three-way holder control measures the retained separate-Slot object with the same Runtime and harness. Observation-only Slot-visit counts are also retained. These controls establish specific extra work and layout sensitivity; they do not assign every residual cycle to one CPU cache or predictor mechanism.

| Narrow AOT control | Baseline ms | Packed G35 ms | Separate-Slot ms | Packed change |
| --- | --- | --- | --- | --- |
| alias_rebind | 7.574 | 7.626 | — | +0.7% |
| identity | 7.945 | 8.064 | — | +1.5% |
| helper_return | 4.995 | 4.991 | — | -0.1% |
| public_helper | 4.963 | 5.152 | — | +3.8% |
| null_failure | 4.278 | 4.082 | 4.165 | -4.6% |


The output-lookup observer counts 40,002 Slot containment visits in baseline, 60,000 with separate payload Slots, and 40,000 when packed, at 20,001 / 20,000 / 20,000 heap constructions. This directly identifies the added 20,000 failed containment tests. The final broad holder JIT measurements still regress about 4.5–6.4%; the narrow packed AOT control improves about 4.6%. Even the byte-identical public-helper control varies by about +3.8%, so residual timing variation is not erased or assigned entirely to code changes.

No alignment, padding, extra Runtime state or eligibility adjustment was used to force favorable timings. The first and pre-packing campaigns remain clearly labeled non-final evidence.

## Existing G34 array/async/GC-tail tradeoffs

The baseline already includes the accepted G34 tradeoffs. Historical AOT-array +4–5%, prior async costs and adverse text/JSON full-GC tails remain in the permanent ledger and original verified archives. G35's unchanged array code does not repair those costs. Current stopped intervals run from ready rendezvous through epoch release and include policy work:

| Case / variant | N | Median µs | p95 µs | p99 µs | Max µs |
| --- | --- | --- | --- | --- | --- |
| text-1024 / base | 440 | 864.0 | 1411.3 | 1631.6 | 2065.6 |
| text-1024 / candidate | 440 | 839.7 | 1261.0 | 1459.8 | 1716.5 |
| json-128 / base | 192 | 758.6 | 1016.3 | 1325.0 | 1447.5 |
| json-128 / candidate | 192 | 794.3 | 1134.9 | 1368.1 | 1500.7 |


Nearest-rank p95/p99, maxima and counts are desktop observations, not latency guarantees. Runtime/collector/pacing code is byte-identical. Full rendezvous latency and allocation/root/search work accompany the stopped-time data. No hard resident-memory or GC-pause bound is claimed.

## Competitive reference, compilation, startup and memory

The original Go 1.27.4 reference was freshly rebuilt from the pinned official SDK/archive SHA-256 and unchanged Go source with `-trimpath`, `GOAMD64=v1`, `CGO_ENABLED=0`, `GOTOOLCHAIN=local`, `GOMAXPROCS=1`, `GOGC=100`. Four matched reference blocks preserve independent oracles. Original numeric/collection comparability and informative text/JSON/I/O qualifications still apply; native stack conventions and collectors differ.

| Case | Go 1.27.4 ms | G35 AOT ms | G35 / Go |
| --- | --- | --- | --- |
| array/1024 | 0.003 | 1.239 | 365.9× |
| array/4096 | 0.019 | 4.887 | 261.1× |
| dependent/1000000 | 1.171 | 26.865 | 22.9× |
| dict/1024 | 0.078 | 9.944 | 127.0× |
| float/1000000 | 2.051 | 26.630 | 13.0× |
| integer/1000000 | 1.076 | 29.925 | 27.8× |
| integer/4000000 | 4.189 | 119.259 | 28.5× |
| io/200 | 6.015 | 6.902 | 1.1× |
| json/128 | 0.113 | 37.390 | 331.8× |
| json/32 | 0.030 | 5.901 | 195.1× |
| sort/1024 | 0.044 | 29.283 | 668.3× |
| sort/256 | 0.009 | 6.048 | 670.2× |
| text/1024 | 0.086 | 53.952 | 630.1× |
| text/256 | 0.022 | 13.112 | 597.1× |


| Operation | G34 ms | G35 ms | Change |
| --- | --- | --- | --- |
| cold / aot | 5.269 | 5.320 | +1.0% |
| cold / jit-normal | 3175.931 | 3192.948 | +0.5% |
| cold / jit-O2 | 3888.432 | 3911.574 | +0.6% |
| compile / compile-link | 3737.576 | 3746.430 | +0.2% |


The separate 24-pair cold-AOT median is 4.895→4.940 ms. Synthetic compile/JIT process costs include frontend, validation, LLVM and a checked invocation; their emitted scalar code is unchanged, so they measure analysis scaling, not a runtime speedup:

| Construction sites | Wall ms G34 → G35 | CPU ms | Peak RSS KiB | Wall change |
| --- | --- | --- | --- | --- |
| 100 | 53.4 → 52.8 | 50.8 → 49.7 | 47080 → 48032 | -1.1% |
| 500 | 300.6 → 273.4 | 286.4 → 262.0 | 60316 → 61260 | -9.0% |
| 2000 | 2821.0 → 2268.8 | 2685.3 → 2164.2 | 117640 → 118884 | -19.6% |


One function-wide use index replaces the old repeated whole-function instruction scans. The same indexed edges propagate unproven ownership; candidate alias events drive the must-dataflow check. Small ordered sets remain for owned results/lifetime facts. The private helper/factory checks are bounded and use verified effect information. No public IR schema or persistent Runtime state is added. Exact line counts/diff are archived rather than treating file size as a runtime metric.

Per-process RSS, live payload, requested-byte peaks and collection counts are separate in `g35-facts.json` and raw results. Requested-byte observation excludes pre-existing/reused storage and is not a physical heap bound. G34's bounded cache remains 557,056 payload bytes plus the existing Data/node/reference metadata; it is outside the live registry. Heap/Runtime/Frame/Slot layouts remain unchanged. Reference payload lifetimes mirror their owning local roots; no moving/concurrent/generational collector or new root authority is introduced.

## Inventory, review and next implementation action

The [exact source diff](/home/shmellyorc/Documents/Void.Language-archives/g35-managed-escape-analysis-2026-11-14.patch), identities, [ledger](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md), inputs/oracles, essential raw results, failures, IR/disassembly and replay are retained in the [verified evidence archive](/home/shmellyorc/Documents/Void.Language-archives/g35-managed-escape-analysis-2026-11-14.zip). Source/postimage and manifest SHA-256, DEFLATE-9 verification, fresh replay and task-only cleanup receipts accompany it. Build trees, SDKs, providers, caches and thousands of routine intermediates are excluded.

G32/G33 research reports remain tracked and preserved, with the ledger intentionally updated. Final inventory covers every changed and untracked path. No staging, commit, push or hosted CI occurred. The archived report is the qualification snapshot; adjacent closeout records final retained count, compressed size, source inventory, measured reclamation and protected-installation checks after cleanup.

**Recommendation:** submit the bounded G35 candidate for Golden Wrench review, retaining documented performance tradeoffs and all conservative fallbacks. Do not claim complete escape analysis. Next bounded implementation action: qualify a shorter precise payload lifetime for direct reference temporaries/earlier final uses, then broaden field-derived/current-heap provenance and multi-block private-helper summaries within the same owned mechanism. Interprocedural retention, arbitrary incoming references, multiple-origin rebinding, native/callback/async/cleanup escapes and stack graphs remain explicitly unqualified.

## Verified archive and cleanup closeout

The DEFLATE-9 archive retains **540 selected files plus one manifest**, **4,365,818 ZIP bytes**, SHA-256 `832123cd10ea1bc8ff96201eb1d73dcbe13b44e5f6d537856832fbe5d93f59af`. CRC, extracted/source hashes and a fresh source replay passed. The [verification receipt](/home/shmellyorc/Documents/Void.Language-archives/g35-managed-escape-analysis-2026-11-14-verification.json) and [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g35-managed-escape-analysis-2026-11-14-cleanup.json) remain adjacent.

All task-owned Linux build groups, frozen consumers, temporary Go SDK/cache and Windows task source/build/transport roots are removed. Recorded cumulative cleanup-phase free-space increases total **7,137,722,368 bytes (6.648 GiB) Linux** and **12,368,044,032 bytes (11.519 GiB) Windows**. These are cumulative observed increases across reused build groups, not a claim of unique retained volume or exclusive filesystem activity; individual logical/allocated sizes and observations are separate in the receipts.

Consumer checks pass. Protected SDK/provider/toolchain hashes and all production source postimages are preserved after cleanup; previous archives remain intact. [Final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g35-managed-escape-analysis-2026-11-14-final-Git-inventory.json) includes every changed/untracked file and tracked G32/G33 research reports. HEAD and the empty Git index are unchanged. No commit, push or hosted CI occurred.
