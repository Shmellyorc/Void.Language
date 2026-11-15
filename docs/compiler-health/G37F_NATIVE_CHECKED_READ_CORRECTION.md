# G37-F — Native checked-read correction

Starting commit: `1e606f8e5067a3113b1d4d4f850fd3a11e2e57a0`. Exact G37 archive: `cc39715ce6feb1895ce1e504743d64bdd8330f112e913e451368244a769c4eee`. The eight-file starting worktree is preserved; the production/test correction changes only `fixed_abi.cpp` and its existing checked-collection tests beyond G37. The ledger and this report document qualification. Nothing is staged, committed or pushed; no hosted CI.

## Disposition

HOLD default acceptance of the combined G37/G37-F source pending explicit review of the retained native and GC-tail costs. Keep the corrected bounded specialization implemented; do not restore dynamic-width work as the default.

One small cold-boundary correction removes substantial duplicated exception code, with all eleven source-known reads intact. However, full numeric AOT remains +8.6–10.7% versus G36; the exact-object narrow integer is +11.0% versus G36 and +7.8% versus G37 (cycles +8.7%/+5.8%, effectively identical instructions/branches). Whole-array AOT is +2.1% versus G36; prepared traversal is +2.1% versus G36 and +5.6% versus G37. JSON stopped p99 is 1205.134/1211.293/1357.202 us and maximum 1258.983/1392.642/2036.439 us across G36/G37/F (288 events each). These costs outweigh a general integration recommendation despite the verified code-size reduction and selected-entry savings. Exact cause below the changed ordinary link context is not established.

## Exact mechanism and ownership

One non-template `translate_failure` helper in the existing checked Runtime ABI owns the original exception-type selection. `checked` keeps its missing-context termination, pending-error return, original Work scope and one catch-all that forwards the active exception. A bare rethrow refers to the same active exception: no exception allocation, copy or stored state is added. Catch types, order and messages remain verbatim; the list-specific out-of-range handler retains its original inner scope. Work/lock/lease lifetimes and unknown-exception termination are preserved. The helper is absent from every successful source-read CFG path.

All eleven validated compiler-selected scalar encodings and the complete shared checked-read owner remain. Dynamic/native/aggregate fallback, current root authority, catalog/schema/geometry/header validation, live length, signed/unsigned normalization including INT64_MIN, endian/width behavior, floating payloads, booleans and version checks remain unchanged. No proof moves across allocation, poll, native transition, callback or reload. G33/G35 escape eligibility, 256-byte/16-KiB limits, G34 reuse/poisoning/root/collection ownership and explicit host budgets remain unchanged. No optimizer, allocator, collector, cache, extra state, synchronization, padding, forced alignment or link ordering is added.

Exact production/test patches are `production-source.patch` (G36→G37-F), `g37-source.patch` (archived G36→G37), and `correction-only.patch` (G37→G37-F). Both restoration routes pass actual git apply and all six postimage hashes. Three full Runtime/compiler/header consumer sets use LLVM/Clang/LLD23.1.4; source-identities.json, identities-*.json and commands.jsonl retain exact hashes/commands. One disposable profile build group is reused.

| Production/test path | G37-F SHA-256 |
| --- | --- |
| `include/void/runtime.hpp` | `b32f8812e0915f57aa5924b722398c4f483c7ff1700b00c0f0db746141688651` |
| `include/void/runtime_abi.hpp` | `c66455d2c4bd7870eba99dd61217f4eaedcfe749db0014268f4eec9bd80ec5ba` |
| `src/backend/llvm/backend.cpp` | `bf27d790e873c1a65898618bad1295d4b6eee093b98fee1965cd460c0a622821` |
| `src/runtime/fixed_abi.cpp` | `e1d7b75c73ebae440b98ac3f1ba3624a800d83533e5e41599fe1c083b61b3a6f` |
| `src/runtime/list.cpp` | `88b3fe0be11674d14e3898c6888d7a8bac6ab28ef960df3b1c35c3cb3b52480a` |
| `tests/runtime/checked_collections.cpp` | `91918130fde2e617f3abee6e197d1b58aa9f20aa3c1d7fc5528b2f169c3414de` |

## Native work and code evidence

| fixed_abi.cpp.o | G36 | G37 | G37-F |
| --- | ---: | ---: | ---: |
| Text bytes | 17863 | 22791 | 17373 |
| Exception-table bytes | 3504 | 4780 | 2516 |
| Unwind-frame bytes | 3104 | 3936 | 3968 |
| Object bytes | 70648 | 88992 | 64688 |
| Exception type-selection sites | 267 | 355 | 19 |

Native instruction-prefix comparison through the first return passes for all eleven G37/F entries after normalizing addresses and cold branch displacements: internal branch offsets, root-token copies and the same three call relocations are retained. The correction does not remove successful-path instruction/copy/call work. It reduces replicated cold exception code and changes ordinary linked placement; warm timing differences must not be credited to removed hot operations.

G37-F removes actual duplicated cold boundary/exception code, including the eleven specialized entries; it does not claim to remove the required per-read checked ABI call or all G37 code growth. Unwind-frame bytes are reported separately rather than hidden. IR CFG proof asserts one translator and none reachable over normal edges from all eleven entries. Native section/symbol tables, instruction counts and entry disassembly are selected evidence.

Prepared source traversal executes **4096→0→0 dynamic-width reads** and **0→4096→4096 source-constant reads**, in G36/G37/G37-F order. All 56 representative caller sites specialize. Eleven native bodies contain one ordinary constant-width unaligned load and no dynamic memcpy; the legacy dynamic instantiation retains memcpy. Managed constructions, roots, layout/frame/policy work and collection counts match across all three. Allocation observations are separate from timing images. All 33 encoding/length and six fixed/reference controls retain independent oracles.

## Qualification

G37-F: Linux Release **74/74**, fatal ASan/UBSan/LeakSanitizer **74/74**, relevant fatal TSan Runtime/races/live allocator/catalog admission and forced GC/source-read concurrency plus all 26 G35 patterns in independently linked normal/O2 AOT against the instrumented Runtime. Native Windows10 build19045 Debug and Release **60/60 each**, plus pool/pacing, failure/GC and 33 scalar encoding/length cases in normal/O2 JIT and independently linked normal/O2 AOT per profile. PE stack checks and Linux execution retain **1-MiB stacks**. Headers and all consumers match. No suppressions or weakened oracles.

New exact-message controls pass against unchanged G37 and the corrected candidate: missing Runtime state, fixed/list bounds, nonrepresentable geometry, invalid encoding, busy lease, selected-version failure, pending-error priority, recovery and final reclamation. Existing controls preserve descriptor mutation, root authority, concurrent catalog/GC, callbacks, cleanup, numeric/failure injection, hot reload, workers and async/cancellation. The intentional stale-cache ASan use-after-poison remains a required failing negative control. G36/G37 reuse verified historical full suites with fresh focused Runtime/checked controls and all fresh measurement oracles; they are not mislabeled as fresh full qualifications.

## Balanced three-way Linux Release performance

Same inputs/oracles, observer-off timings, all six permutations of G36/G37/G37-F, six blocks and three warm samples (five narrow samples). The Windows VM is paused at a safe build boundary; no Linux build overlaps timings. Independent AOT and normal/O2 JIT cover the complete accepted G36 corpus: numerical/dependent arithmetic, arrays/prepared arrays, dictionaries, sorting, text/Unicode/JSON, managed patterns, calls/closures, workers/async and native I/O. Twenty-six G35 fixtures remain selected. All raw medians/ranges/counts, unfavorable cells and exact images are retained in three-way-results.json and three-way-results.tsv; the Go comparison is the unchanged pinned historical reference, not a new Go measurement.

### Representative complete AOT applications

| Case (ms; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| g31/aot/array/1024 | 1.0831 | 1.0859 | 1.1062 | +2.1% | +1.9% |
| g31/aot/array/4096 | 4.2808 | 4.2953 | 4.3710 | +2.1% | +1.8% |
| g31/aot/dependent/1000000 | 24.3846 | 27.4546 | 26.4733 | +8.6% | -3.6% |
| g31/aot/dict/1024 | 3.9565 | 4.0919 | 4.0496 | +2.4% | -1.0% |
| g31/aot/float/1000000 | 24.1126 | 27.2126 | 26.4452 | +9.7% | -2.8% |
| g31/aot/integer/1000000 | 23.5741 | 26.5513 | 26.0999 | +10.7% | -1.7% |
| g31/aot/integer/4000000 | 94.7066 | 105.9468 | 103.5847 | +9.4% | -2.2% |
| g31/aot/io/200 | 6.8749 | 6.8475 | 6.8264 | -0.7% | -0.3% |
| g31/aot/json/128 | 35.7059 | 35.9951 | 35.8827 | +0.5% | -0.3% |
| g31/aot/json/32 | 5.7228 | 5.7500 | 5.7748 | +0.9% | +0.4% |
| g31/aot/sort/1024 | 27.5816 | 26.3888 | 26.6439 | -3.4% | +1.0% |
| g31/aot/sort/256 | 5.3981 | 5.3056 | 5.3713 | -0.5% | +1.2% |
| g31/aot/text/1024 | 51.3636 | 51.6849 | 52.3137 | +1.8% | +1.2% |
| g31/aot/text/256 | 12.7263 | 12.7843 | 12.8748 | +1.2% | +0.7% |

### Prepared checked native reads

| Case (ns/iteration; prepared-source rows are whole traversals; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| boundary/native/dictionary_count_existing_token/128 | 653.3231 | 676.1611 | 671.8487 | +2.8% | -0.6% |
| boundary/native/dictionary_count_existing_token/3 | 654.7113 | 687.1724 | 669.7411 | +2.3% | -2.5% |
| boundary/native/dictionary_existing_token/128 | 735.8641 | 766.1096 | 760.6092 | +3.4% | -0.7% |
| boundary/native/dictionary_existing_token/3 | 736.1788 | 772.3996 | 760.5237 | +3.3% | -1.5% |
| boundary/native/dictionary_source_abi/128 | 850.3438 | 877.4958 | 889.0103 | +4.5% | +1.3% |
| boundary/native/dictionary_source_abi/3 | 798.4199 | 819.0146 | 812.1870 | +1.7% | -0.8% |
| boundary/native/fixed_existing_token_layout/128 | 78.8338 | 78.3418 | 78.2045 | -0.8% | -0.2% |
| boundary/native/fixed_existing_token_layout/3 | 78.2860 | 78.0193 | 78.2308 | -0.1% | +0.3% |
| boundary/native/fixed_source_abi/128 | 312.8688 | 320.0620 | 317.5550 | +1.5% | -0.8% |
| boundary/native/fixed_source_abi/3 | 251.5477 | 253.8700 | 251.8641 | +0.1% | -0.8% |
| boundary/native/list_existing_token/128 | 248.9567 | 248.6116 | 253.3516 | +1.8% | +1.9% |
| boundary/native/list_existing_token/3 | 250.3016 | 247.6586 | 254.5979 | +1.7% | +2.8% |
| boundary/native/list_selected_source_abi/128 | 348.9928 | 331.3577 | 340.1785 | -2.5% | +2.7% |
| boundary/native/list_selected_source_abi/3 | 287.6571 | 273.3054 | 279.5672 | -2.8% | +2.3% |
| boundary/native/list_source_abi/128 | 348.8180 | 342.8558 | 347.4715 | -0.4% | +1.3% |
| boundary/native/list_source_abi/3 | 287.5845 | 283.8864 | 287.1166 | -0.2% | +1.1% |
| boundary/native/native_entry_metadata/128 | 157.7582 | 158.6448 | 153.8483 | -2.5% | -3.0% |
| boundary/native/native_entry_metadata/3 | 158.1730 | 157.9920 | 153.2514 | -3.1% | -3.0% |
| boundary/native/native_key_metadata/128 | 172.3196 | 171.5487 | 174.0496 | +1.0% | +1.5% |
| boundary/native/native_key_metadata/3 | 171.1319 | 173.4839 | 178.5461 | +4.3% | +2.9% |
| boundary/native/raw_scalar_reference_only/128 | 0.4913 | 0.4919 | 0.4913 | +0.0% | -0.1% |
| boundary/native/raw_scalar_reference_only/3 | 0.4922 | 0.4922 | 0.4926 | +0.1% | +0.1% |
| boundary/native/root_admission/128 | 84.8817 | 92.9258 | 84.8860 | +0.0% | -8.7% |
| boundary/native/root_admission/3 | 32.1702 | 31.1451 | 29.8616 | -7.2% | -4.1% |
| boundary/native/source_prepared_array_4096/0 | 1484256.6250 | 1435562.0000 | 1515397.7500 | +2.1% | +5.6% |
| boundary/native/source_prepared_dictionary_1024/0 | 2421647.7500 | 2475943.7500 | 2482587.5000 | +2.5% | +0.3% |

The fresh selected-source native entry measures about 279.6/340.2 ns at 3/128 roots, versus dynamic 287.1/347.5 ns, existing-token checked reads 254.6/253.4 ns and root admission 29.9/84.9 ns. This locates the dominant measured cost in the existing checked body and root-admission boundary; constant-width work is a small fraction. The raw scalar reference-only control is about 0.49 ns and supplies no equivalent Runtime guarantees. Required checks stay in place. Prepared-source traversal is +2.1% versus G36 and +5.6% versus G37 despite the selected-entry advantage over G36. The per-entry improvement is not a whole-application gain.

### Exact-object narrow native controls

| Case (ms; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| dependent | 26.7319 | 28.7650 | 24.4674 | -8.5% | -14.9% |
| float | 26.4393 | 26.1815 | 24.1349 | -8.7% | -7.8% |
| integer | 25.8777 | 26.6417 | 28.7255 | +11.0% | +7.8% |

The narrow controls relink one exact baseline application object against each Runtime. Numeric arithmetic and poll/checkpoint leaf bytes and symbolic relocations are unchanged across variants; invocation assertions retain integer 4,000,005 and dependent/float 4,000,003 calls to each gc_poll and worker_checkpoint. G37 and G37-F whole suite objects and optimized IR are byte-identical, while linked executable identities and addresses differ. These controls distinguish linked-context timing from compiler arithmetic work: unchanged bodies do not imply identical performance. Cold-EH reduction is established; a cache/decoder/predictor cause is not established. No timing movement in these unchanged bodies is claimed as a new optimizer gain.

Hardware user cycles/instructions/branches/branch misses were captured for six balanced blocks; raw perf output and commands are retained. Per-event per-case medians follow; process totals include startup/warm-up and five samples, so these are not isolated source-loop retired counts.

| Case/event | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| integer/cycles:u | 543794018 | 558999842 | 591298956 | +8.7% | +5.8% |
| integer/instructions:u | 1227982204 | 1227982211 | 1227982221 | +0.0% | +0.0% |
| integer/branches:u | 414829078 | 414829086 | 414829092 | +0.0% | +0.0% |
| integer/branch-misses:u | 22914 | 23059 | 23026 | +0.5% | -0.1% |
| dependent/cycles:u | 556347520 | 598172922 | 511130183 | -8.1% | -14.6% |
| dependent/instructions:u | 1179981622 | 1179981636 | 1179981614 | -0.0% | -0.0% |
| dependent/branches:u | 414828879 | 414828892 | 414828868 | -0.0% | -0.0% |
| dependent/branch-misses:u | 22816 | 23037 | 22896 | +0.4% | -0.6% |
| float/cycles:u | 553614888 | 550238812 | 504229763 | -8.9% | -8.4% |
| float/instructions:u | 1191981744 | 1191981742 | 1191981734 | -0.0% | -0.0% |
| float/branches:u | 414828914 | 414828911 | 414828900 | -0.0% | -0.0% |
| float/branch-misses:u | 22900 | 22964 | 22927 | +0.1% | -0.2% |

### Unfavorable managed AOT controls

| Case (ms; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| aot/direct_reference | 9.5458 | 9.9029 | 10.1910 | +6.8% | +2.9% |
| aot/helper_write | 0.6223 | 0.6273 | 0.6626 | +6.5% | +5.6% |
| aot/identity | 7.9184 | 7.9764 | 8.4765 | +7.0% | +6.3% |
| aot/nested | 9.7463 | 10.5285 | 10.8845 | +11.7% | +3.4% |
| aot/references | 4.4110 | 4.5040 | 4.6537 | +5.5% | +3.3% |
| aot/unknown_factory | 10.5534 | 10.9175 | 11.3570 | +7.6% | +4.0% |

All 52 normal/optimized G37/G37-F escape source objects are byte-identical; these adverse linked timings accompany preserved construction/root/failure semantics and are not evidence of broader or reduced escape eligibility.

### Compilation, startup and cold process cost

| Case (ms; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| cold/aot | 5.0362 | 5.0653 | 5.1057 | +1.4% | +0.8% |
| cold/jit-O2 | 3714.3248 | 3705.5962 | 3723.1974 | +0.2% | +0.5% |
| cold/jit-normal | 3068.6190 | 3063.5784 | 3051.5495 | -0.6% | -0.4% |
| compile/compile-link | 3578.1212 | 3567.8876 | 3578.9993 | +0.0% | +0.3% |
| independent-AOT-process | 4.9065 | 4.7309 | 4.8974 | -0.2% | +3.5% |

Binary and compiler identities/sizes and per-process RSS are in the three-way result. Scaling controls at 100/500/2000 construction sites retain six matched compile/JIT wall, CPU and RSS samples per variant. This is source-constant-read/cold-boundary work, not a change to the existing escape proof.

## GC tails, memory and unfavorable results

| Workload / variant | Stopped n | Median us | p95 us | p99 us | Max us |
| --- | ---: | ---: | ---: | ---: | ---: |
| text-1024 / base | 660 | 814.176 | 1190.233 | 1271.143 | 1799.810 |
| text-1024 / g37 | 660 | 821.961 | 1187.404 | 1245.984 | 1418.752 |
| text-1024 / candidate | 660 | 839.105 | 1213.394 | 1265.953 | 1672.651 |
| json-128 / base | 288 | 810.121 | 1062.364 | 1205.134 | 1258.983 |
| json-128 / g37 | 288 | 832.515 | 1061.324 | 1211.293 | 1392.642 |
| json-128 / candidate | 288 | 748.501 | 1079.294 | 1357.202 | 2036.439 |

All request-to-completion latency statistics accompany full stopped intervals, using nearest-rank quantiles. These finite cohorts retain observed tails without establishing a collector algorithm cause. Text/JSON cohorts, pressure repetitions, surviving objects/payload, retained-cache bytes, epochs and exact checksums remain raw evidence. No new collector/accounting owner or altered budget is introduced. RSS and retained memory changes must be read with their process/link context; equal semantic counts do not assert equal wall-time tails.

### Memory-pressure controls

| Case (ms; lower is better) | G36 | G37 | G37-F | F/G36 | F/G37 |
| --- | ---: | ---: | ---: | ---: | ---: |
| large | 3.8161 | 3.8340 | 3.7891 | -0.7% | -1.2% |
| mixed | 3.3837 | 3.3761 | 3.3904 | +0.2% | +0.4% |
| retained | 6.0813 | 6.1321 | 6.1357 | +0.9% | +0.1% |
| small | 2.7829 | 2.7493 | 2.7928 | +0.4% | +1.6% |
| threads | 12.0282 | 15.5243 | 16.3073 | +35.6% | +5.0% |

| Pressure / variant | Objects range | Payload bytes range | Cache bytes range | Completed epochs range |
| --- | ---: | ---: | ---: | ---: |
| large/base | 2–2 | 131072–131072 | 0–0 | 1023–1023 |
| large/g37 | 2–2 | 131072–131072 | 0–0 | 1023–1023 |
| large/candidate | 2–2 | 131072–131072 | 0–0 | 1023–1023 |
| mixed/base | 88–88 | 23056–23056 | 15008–15008 | 79–79 |
| mixed/g37 | 88–88 | 23056–23056 | 15008–15008 | 79–79 |
| mixed/candidate | 88–88 | 23056–23056 | 15008–15008 | 79–79 |
| retained/base | 544–544 | 8704–8704 | 3584–3584 | 40–40 |
| retained/g37 | 544–544 | 8704–8704 | 3584–3584 | 40–40 |
| retained/candidate | 544–544 | 8704–8704 | 3584–3584 | 40–40 |
| small/base | 33–33 | 528–528 | 3584–3584 | 78–78 |
| small/g37 | 33–33 | 528–528 | 3584–3584 | 78–78 |
| small/candidate | 33–33 | 528–528 | 3584–3584 | 78–78 |
| threads/base | 2–257 | 32–4112 | 0–4080 | 77–78 |
| threads/g37 | 2–257 | 32–4112 | 0–4080 | 77–78 |
| threads/candidate | 3–256 | 48–4096 | 16–4064 | 77–78 |

The historical G37 narrow integer +18.1%, dependent +7.5%, whole-array AOT approximately +3.3%, and JSON stopped p99 1438.152→1551.002 us (192 events each) remain recoverable. This fresh three-way cohort supersedes neither their identities nor the observed costs. G34 AOT-array +4.9/+4.0%, async +3.2–3.5%; G35 holder JIT +4.5–6.4%; G36 floating/dependent +9.6/+9.4%, workers +27.7%, JSON p99 1187.543→1782.240 us (192 events) remain explicit historical tradeoffs. Favorable new signs do not erase them.

The pressure executables are byte-identical across all three variants (SHA-256 `45a07c11e99bef81a22d24f93bcecc79344c1a5107d4a41844860789e199726b`). Their threaded median varies +35.6% versus G36, with paired signs from -31.5% to +67.0%; this is retained as process/scheduling variation and is not attributed to G37-F. The compiler Core archive is identical across all three; the LLVM backend archive is identical for G37/G37-F. Numeric linked entry/leaf addresses and selected disassembly are retained in numeric-linked-context.json and numeric-linked-leaves.asm.

Full unfavorable fresh results across JIT/AOT width/element/escape/application cells are present in the complete TSV and raw JSON with sample ranges, not omitted through a favorable aggregate. No eligibility narrowing or benchmark/alignment workaround was used.

## Complexity and remaining objectives

G37-F moves eight repeated typed handlers into one existing-boundary helper and adds one small exact-message test. It introduces one failure-only forwarding call and active rethrow, with no successful-path translator call, stored metadata or new abstraction layer. Its measured code-size reduction is real; native performance disposition above remains workload-aware.

Aggregate/fixed descriptor compatibility and broader checked-operation specialization, escape analysis beyond G35, dictionary/hash/equality/sorting dispatch, text/Unicode/JSON materialization/traversal with exact budgets, automatic SIMD and future native/release acceptance remain active in the permanent ledger. The eleven bounded source-selected reads remain implemented and qualified.

## Evidence and cleanup

Archive prefix: `g37f-native-checked-read-correction-2026-11-14` in `/home/shmellyorc/Documents/Void.Language-archives`. Selected source patches, independent inputs/oracles, identities, raw measurements, essential fatal/negative results, selected IR/native disassembly and replay instructions are retained with DEFLATE-9, CRC and SHA-256 manifest verification. No compiled build trees, caches, full consumer binaries or duplicated SDKs are retained. Post-verification cleanup and final inventory receipts are adjacent to the immutable archive.

## Verified archive and cleanup closeout

The DEFLATE-9 archive retains **503 selected files plus one manifest**, **4,985,896 ZIP bytes**, SHA-256 `0cd0d21f695e2535033610a89aec54c4a402df6d9faa3b159d69831869781e9c`. CRC, extracted/source hashes and fresh patch restoration and replay preparation passed. The [verification receipt](/home/shmellyorc/Documents/Void.Language-archives/g37f-native-checked-read-correction-2026-11-14-verification.json) and [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g37f-native-checked-read-correction-2026-11-14-cleanup.json) remain adjacent.

All task-owned Linux build groups, frozen consumers, Windows task source/build/transport roots are removed. Recorded cumulative cleanup-phase free-space increases total **9,203,879,936 bytes (8.572 GiB) Linux** and **5,336,342,528 bytes (4.970 GiB) Windows**. These are cumulative observed increases across reused build groups, not a claim of unique retained volume or exclusive filesystem activity; individual logical/allocated sizes and observations are separate in the receipts.

Consumer checks pass. Protected SDK/provider/toolchain hashes and all production source postimages are preserved after cleanup; previous archives remain intact. [Final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g37f-native-checked-read-correction-2026-11-14-final-Git-inventory.json) includes every changed/untracked file and tracked G32/G33 research reports. The [exact final nine-file worktree diff](/home/shmellyorc/Documents/Void.Language-archives/g37f-native-checked-read-correction-2026-11-14-full-worktree.patch) includes both untracked reports; its actual restoration receipt is adjacent. HEAD and the empty Git index are unchanged. No commit, push or hosted CI occurred.
