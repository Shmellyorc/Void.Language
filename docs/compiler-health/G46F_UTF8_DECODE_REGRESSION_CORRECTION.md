# G46-F — UTF-8 decode integration regression

**OWNER TRADEOFF.** Preserve the qualified G46 implementation. The optimized AOT integer regression reproduces at +18.0% for 4 million iterations in the exact archived linked images. The source-isolated relinks locate the trigger in the G46 `coordination.cpp` object. A fixed-Runtime placement intervention removes most of the gap in one diagnostic arrangement. A durable source correction is unproven.

The accepted production baseline is `894e2e8d9930028f86737092d3ac74010676f389`. The isolated branch is `codex/g46f-utf8-decode`. All four G46 Runtime and test postimages are identical to the qualified source. LLVM, Clang and LLD remain 23.1.4. G37-F remains performance-held and G45/G45-U/G45-V remain LLVM research HOLD. The G46 report and cumulative G31–G46 history retain their original entries.

## Exact relinks and source attribution

Both modes reuse the archived application objects, whose G44 and G46 SHA-256 hashes match. The original link recipes, provider inputs, archive member order and CPU pinning are reused. G44 and full G46 reproduce every byte of the archived normal and optimized linked suite executables. The two intermediate Runtime archives each replace one object. Coordination-only retains the stock `array.cpp` decoder call. Array-only substitutes the G46 array object with its private decoder helper relocation rebound to the stock `string_into` member of the identical type. That diagnostic produces the same linked executable hash as G44. The changed archive member hashes and actual linker argument vectors are retained.

| Exact executable | SHA-256 |
| --- | --- |
| g44/normal | eae0dd716c61c00da568289775d4f8a3587467d67c4f5ad3ddfe088211fcc642 |
| g44/optimized | 117070c1ad8640a83c71c13cda63696514fb9d5e3b1dd1e68061cc3cfb4d2a1d |
| g46/normal | c6980bd6aa7dcb7986aedfd5e42bbedf4808091be53cd5ddedbd262979151d25 |
| g46/optimized | 19a7546ee7889b7eea1678f24e8780e190b2f4f4b27bd5637859615be542f411 |

Six balanced blocks per ordinary AOT mode each check the same independent arithmetic, array, sorting, dictionary, text, JSON and native-I/O expected values. The optimized result follows `coordination.cpp` even when the UTF-8 decoder still uses the stock publication path.

| AOT mode and workload | G44 ms | Array-only ms | Coordination-only ms | G46 ms | G46 change |
| --- | --- | --- | --- | --- | --- |
| normal integer-1000000 | 27.220 | 27.100 | 30.640 | 30.617 | +12.5% |
| normal integer-4000000 | 108.542 | 108.632 | 122.134 | 121.735 | +12.2% |
| optimized integer-1000000 | 24.571 | 24.727 | 29.126 | 28.966 | +17.9% |
| optimized integer-4000000 | 97.742 | 98.198 | 114.644 | 115.340 | +18.0% |

The archived integer loop contains four GC poll and worker checkpoint pairs per iteration. Its hot loop has no string decode, string construction or shared publication call. The retained disassembly resolves direct calls to `vd_gc_poll` and `vd_worker_checkpoint`. The poll reads TLS, checks the attached managed context and tests the GC request. The worker checkpoint takes the existing no-worker fast return. The loop and relevant Runtime instruction sequences match after accounting for address operands. The validation, request handling and cancellation contracts still apply.

| Ordinary optimized image | Integer entry | First hot-loop instruction | ABI poll | Runtime poll | Worker checkpoint |
| --- | --- | --- | --- | --- | --- |
| g44 | 0x215190 | 0x215220 | 0x2b0480 | 0x2ad900 | 0x2c1db0 |
| array-only | 0x215190 | 0x215220 | 0x2b0480 | 0x2ad900 | 0x2c1db0 |
| coordination-only | 0x215270 | 0x215300 | 0x2b07c0 | 0x2adc40 | 0x2c20f0 |
| g46 | 0x215270 | 0x215300 | 0x2b07c0 | 0x2adc40 | 0x2c20f0 |

These are ELF virtual addresses. Runtime addresses also include the process-specific PIE load bias. Raw function bytes, sizes and SHA-256 hashes are in `native-identities/linked-images.json`. Runtime archive membership confirms `abi.cpp.o` and `async.cpp.o` are identical across the treatments.

## Placement intervention and counters

The diagnostic relink places the unchanged application text in a fixed span and varies its position by 0, 16, 32 or 48 bytes within that span. For each Runtime, the ABI poll, Runtime poll, worker checkpoint and native clock retain both exact addresses and bytes across the four app positions. Address operands and unwind references to relocated application code change as required by the link. The production link recipes remain the ordinary recipes above.

| App shift | G44 integer-1M ms | G46 integer-1M ms | G44 integer-4M ms | G46 integer-4M ms |
| --- | --- | --- | --- | --- |
| 0 | 26.663 | 31.395 | 105.065 | 124.119 |
| 16 | 26.278 | 30.782 | 105.004 | 115.531 |
| 32 | 26.263 | 27.205 | 105.093 | 106.506 |
| 48 | 26.597 | 31.076 | 105.992 | 124.580 |

Moving the app text within the same G46 Runtime reduces integer-4M from 124.119 to 106.506 ms. Stock in the same diagnostic layout remains around 105–106 ms. This intervention establishes placement as a major causal contributor. The wider range on G46 and the differing ordinary versus diagnostic locations preclude a fixed alignment rule. Changing output padding or picking a source layout solely for this address would leave the result vulnerable to other routes and future links.

Four balanced counter blocks record the first six measured integer regions per process on the same Linux Ryzen 7 1700X CPU. The separate observer filters the existing monotonic-clock call by executable symbol address, pairs the first 12 matching clocks, and checks all 84 clock calls and all outputs. Initial probes found no matching calls because the wrapper tail-calls its native helper and the ELF is PIE. The final probe filters the helper and adds the executable load bias. Counter running fractions were 1.0 for all returned intervals.

| Integer scope | Image | Instructions | Cycles | IPC | Branches | Branch misses | L1-I fetches | L1-I misses |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| integer-1000000 | g44 | 204,004,596 | 84,961,748 | 2.401 | 69,001,369 | 854 | 100,291 | 840 |
| integer-1000000 | array-only | 204,004,594 | 84,804,178 | 2.406 | 69,001,366 | 668 | 44,511 | 572 |
| integer-1000000 | coordination-only | 204,004,392 | 100,031,355 | 2.039 | 69,001,164 | 178 | 53,113,572 | 1,681 |
| integer-1000000 | g46 | 204,004,610 | 99,042,695 | 2.060 | 69,001,383 | 688 | 51,748,539 | 2,810 |
| integer-4000000 | g44 | 816,004,464 | 338,177,573 | 2.413 | 276,001,242 | 696 | 332,534 | 2,098 |
| integer-4000000 | array-only | 816,004,475 | 341,941,268 | 2.386 | 276,001,253 | 1,592 | 261,270 | 1,822 |
| integer-4000000 | coordination-only | 816,004,470 | 393,935,025 | 2.071 | 276,001,250 | 448 | 207,684,678 | 6,036 |
| integer-4000000 | g46 | 816,004,488 | 396,672,149 | 2.057 | 276,001,264 | 1,027 | 202,617,442 | 18,112 |

The ordinary G46 integer-4M scope retires essentially the same instructions and branches as G44. Its median cycles rise from 338.2 to 396.7 million and IPC falls from 2.413 to 2.057. The rise in generic L1-I fetches occurs on the two ordinary images containing the changed coordination object. Branch-miss counts vary without explaining the cycle gap. These Linux generic events map to AMD instruction fetch and miss events. They do not isolate a microarchitectural cause. [Linux AMD PMU mapping](https://github.com/torvalds/linux/blob/master/arch/x86/events/amd/core.c)

The clock observer changes the ranking of some diagnostic placements. For example, the uninstrumented G46 app shift 32 is the fastest tested G46 placement, while the scoped observer ranks shift 16 ahead of it. All raw counter and ordinary timing results are retained separately. The exact role of instruction delivery, predictor state, cache or decoded-op caching is unresolved. The evidence supports a link-placement sensitivity with the existing instructions and boundary contracts.

## Repeat measurements and decode proof

The reduced work survives. The two direct native cases execute 1,000 successful nonempty decodes per side. Each adds 1,000 managed strings with no collections in the scope. Counts record one eliminated staging request of payload plus eight header bytes per decode.

| 1,000 decodes | G44 native requests | G46 native requests | G44 requested bytes | G46 requested bytes |
| --- | --- | --- | --- | --- |
| 1024 | 4006 | 3006 | 3177976 | 2145976 |
| 65536 | 4006 | 3006 | 196713976 | 131169976 |

The fresh focused Unicode source contains ASCII, NUL, multibyte and supplementary scalars and a combining mark. The independent checksum uses 7,168 bytes and 3,584 scalars per decoded value. Results reproduce across normal and O2 JIT and separately linked normal and optimized AOT. Native direct and source timings exclude the counter and GC observers.

| Focused replay | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| native release 1024 | 4.903 | 2.519 | -48.6% |
| native release 65536 | 252.356 | 109.530 | -56.6% |
| jit normal materialize | 31.548 | 16.409 | -48.0% |
| jit optimized materialize | 30.830 | 16.518 | -46.4% |
| aot normal materialize | 32.822 | 16.601 | -49.4% |
| aot optimized materialize | 34.475 | 18.973 | -45.0% |
| JIT normal calls | 0.789 | 0.795 | +0.7% |
| JIT optimized calls | 0.761 | 0.782 | +2.7% |
| JIT normal closures | 1.479 | 1.411 | -4.6% |
| JIT optimized closures | 1.521 | 1.328 | -12.7% |
| JIT normal workers | 1.252 | 1.330 | +6.2% |
| JIT optimized workers | 1.241 | 1.312 | +5.7% |
| JIT normal async-tasks | 62.023 | 61.497 | -0.8% |
| JIT optimized async-tasks | 64.042 | 61.093 | -4.6% |

The representative medians below retain favorable and unfavorable measurements. Timing changes in unaffected integer and other controls are observations of the linked Runtime and executable environment. They do not establish a compiler code-generation change. AOT uses six balanced blocks and JIT uses four.

| Representative workload | Normal AOT | Optimized AOT | Normal JIT | O2 JIT |
| --- | --- | --- | --- | --- |
| integer-1000000 | +12.5% | +17.9% | +2.6% | +4.2% |
| integer-4000000 | +12.2% | +18.0% | +1.0% | +5.6% |
| dependent-1000000 | +12.6% | +6.0% | +1.6% | +3.7% |
| float-1000000 | +14.2% | +6.3% | +4.8% | +3.3% |
| array-1024 | +5.1% | +2.8% | -1.3% | -0.5% |
| array-4096 | +5.3% | +0.6% | -0.8% | -0.5% |
| sort-256 | +3.5% | +1.5% | -2.1% | +0.9% |
| sort-1024 | +4.1% | -0.2% | -2.8% | +6.2% |
| dict-1024 | +1.5% | +4.3% | +2.9% | +2.2% |
| text-256 | +4.2% | +2.2% | +1.3% | +1.0% |
| text-1024 | +5.2% | +2.9% | -2.5% | +2.8% |
| json-32 | +1.7% | +2.1% | -0.6% | +4.9% |
| json-128 | +2.6% | +2.7% | -0.8% | +5.4% |
| io-200 | -2.0% | -0.2% | -0.1% | +2.6% |

The allocation/GC replay independently checks eight pairs. A nonempty decode changes three payload copies to two and three full UTF-8 validations to one. The observer counts decode entries and actual bytes, then derives fixed path copies and validation passes. Managed construction count, reuse, buffer adoption, root words, marking, surviving payload and collections match for every pair. The stopped interval runs from observed rendezvous completion through collector completion and wake notification. The request interval includes the rendezvous. Individual intervals and p99 differences remain in the archive.

| Observer cohort | Decodes | G44 copied bytes | G46 copied bytes | G44 scans | G46 scans | Cycles of GC | Root words | Marks | Stopped p99 ms | Request-to-resume p99 ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| normal/json-128 | 1029 | 28917 | 19278 | 3087 | 1029 | 25 | 16258 | 10233 | 0.422 → 0.666 | 0.422 → 0.666 |
| normal/json-32 | 261 | 6849 | 4566 | 783 | 261 | 10 | 6515 | 1303 | 0.306 → 0.155 | 0.306 → 0.155 |
| normal/text-1024 | 4120 | 323616 | 215744 | 12352 | 4120 | 49 | 16002 | 36014 | 0.543 → 0.816 | 0.543 → 0.816 |
| normal/text-256 | 1048 | 80928 | 53952 | 3136 | 1048 | 20 | 6548 | 4146 | 0.144 → 0.118 | 0.144 → 0.119 |
| optimized/json-128 | 1029 | 28917 | 19278 | 3087 | 1029 | 25 | 16258 | 10233 | 0.664 → 0.279 | 0.665 → 0.279 |
| optimized/json-32 | 261 | 6849 | 4566 | 783 | 261 | 10 | 6515 | 1303 | 0.143 → 0.125 | 0.143 → 0.126 |
| optimized/text-1024 | 4120 | 323616 | 215744 | 12352 | 4120 | 49 | 16002 | 36014 | 0.775 → 0.606 | 0.776 → 0.606 |
| optimized/text-256 | 1048 | 80928 | 53952 | 3136 | 1048 | 20 | 6548 | 4146 | 0.282 → 0.131 | 0.282 → 0.131 |

## Qualification and disposition

All 3,319 native qualification inputs match their archived G46 SHA-256 hashes, including the four changed Runtime/test files. This reuses the exact selected Linux Release 104/104, fatal ASan/UBSan/LeakSanitizer 103/103, fatal TSan concurrent 300-decode control with 6,936 collector completions, and native Windows 10 Debug and Release 71/71 each. Preserved suites cover 1 MiB stacks, Unicode indexing, malformed UTF-8, NUL, alias output, cold OOM, foreign Heap rejection, forced collection, resource roots, callbacks, reload, workers and async. The final G46 cold failure receipt rejects malformed short input even with the first allocation denied and preserves the prior output. The earlier prefixed snapshot prototype and its failure-order regression remain archived.

| Qualified Runtime/test postimage | SHA-256 |
| --- | --- |
| include/void/runtime.hpp | a9fe2993f109a2619cb6be103e08f5afbcd3f93a752bed064b05f469a295e387 |
| src/runtime/array.cpp | 878a4cc755f35bdb8467ec123a4f2d18dc05946e7fc0b127e4a0d6e5728134d1 |
| src/runtime/coordination.cpp | 41085ba1ea566fa5da1bfbae8ebdabd578f2cc90313c586401ac788b58408595 |
| tests/stage8/sprint3.hpp | e079ffb801d2e0844e5279ad00872cb27d073cfab36639086944b73645f94e36 |

The two changed Runtime source operations in the hot-loop investigation are the private publication split and byte-array decode caller. Removing either safety check or changing the number of existing checkpoint calls would require an independent proof. This campaign establishes no unnecessary new hot-loop boundary and proposes no placement-dependent production change. Keep the byte-exact G46 candidate for Golden Wrench and Boss to evaluate against the reproduced integer cost. The follow-up with the least new scope is to run these exact G44 and G46 linked images and the matched counter regions on a second x86 host. A subsequent implementation would require a separate proof about the repeated poll/checkpoint pairs and their response guarantees.

The DEFLATE-9 evidence archive is `g46f-utf8-decode-regression-2026-11-16.zip`. It restores the exact retained G46 source and current documentation from accepted G44. The verification receipt records CRC, manifest checks and independent source restoration. Raw measurements, checked oracles, native function bytes, replay commands and the exact qualification receipts are included. The original G46 archive remains a read-only replay input. Relative to qualified G46, the ledger and this report are the two documentation changes. Relative to production G44, the inventory includes the four retained G46 Runtime/test files and six documents.
