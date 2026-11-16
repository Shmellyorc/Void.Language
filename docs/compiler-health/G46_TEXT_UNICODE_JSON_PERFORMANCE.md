# G46 — Text, Unicode and JSON materialization

**TARGETED CORRECTION.** The final source passes Linux, sanitizer and native Windows qualification and materially reduces decode cost. Hold integration while investigating the reproducible 17% optimized AOT integer regression. The emitted integer object is identical and the linked Runtime image is different.

The candidate is based on accepted G44 `894e2e8d9930028f86737092d3ac74010676f389` on `codex/g46-text-unicode-json`. Linux and Windows use the installed LLVM, Clang and LLD 23.1.4 and the approved providers. G37-F remains performance-held. The separate G45, G45-U and G45-V LLVM experiments remain on HOLD. The G31–G44 compiler, GC, ownership and SIMD history remains in the cumulative ledger.

## Implemented change and safety

`Runtime::string_decode_into` still obtains an owned snapshot through the existing byte-array schema, precise backing root, read lease and bounded copying. It checks UTF-8 with the existing decoder and takes the existing malformed-input result before touching the output. The old valid path copied the text into a vector containing the length header and validated the text in that vector before publication. It also validated the native view before preparing the vector. The new path gives the shared construction routine a stack length header and the already validated, immutable native snapshot. One validity scan replaces three complete nonempty scans. Copying goes from three passes over text bytes to two, from byte-array backing to snapshot and then to the final managed string. This excludes header stores, zero initialization, and copies in upstream builders.

The length guard uses the original string payload limit. Empty output is the canonical null string. The destination must be a registered precise root. The final allocation checkpoint, poll, descriptor and output checks, existing heap allocator and reuse cache remain on the shared path. The already validated body and header are native locals alive through every checkpoint. The heap allocation, two nonthrowing copies, root publication and accounting run under the existing lock, with the previous output intact until the complete string is ready. The aliased-input test covers this ordering. The ordinary constructor keeps its contiguous payload and reference validation. The managed extent, allocation accounting, heap provenance, C ABI symbols and collector layout remain the same.

Text and JSON both benefit through their existing calls to `string.from_utf8`. Concatenation, Unicode indexing and scalar semantics, JSON escaping rules, number formatting, duplicate-key policy, depth and budget checks keep the established code. No new text or JSON engine, cache, index or root representation is introduced. The production diff adds two private helpers and a copy choice to the shared publication routine. It removes the temporary native allocation, an extra full byte copy and two full UTF-8 passes from successful nonempty decode.

## Measured allocation and copy costs

The following covers 1,000 explicit native decode calls per row after setup, with no collection or reuse during the scope. The native interposer includes allocator requests made while constructing the result map entries. Managed object counts are confirmed from the heap snapshot. The warmed decoded output was already materialized before counting.

| Input bytes | G44 native requests | G46 native requests | G44 requested bytes | G46 requested bytes | Managed strings per side |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 | 0 |
| 6 | 3,006 | 2,006 | 118,976 | 104,976 | 1,000 |
| 12 | 3,006 | 2,006 | 140,976 | 120,976 | 1,000 |
| 64 | 4,006 | 3,006 | 305,976 | 233,976 | 1,000 |
| 1024 | 4,006 | 3,006 | 3,177,976 | 2,145,976 | 1,000 |
| 65536 | 4,006 | 3,006 | 196,713,976 | 131,169,976 | 1,000 |

The native storage saving is one request of exactly `8 + payload size` per nonempty decode across the measured sizes. The full-size snapshot and its copy remain necessary with the current read lease and arbitrary inputs. For 64 KiB, this avoids 65,544,000 requested bytes, one 65,536,000-byte copy pass and two validation passes over the same text across 1,000 calls. The small-string path retains the original inline capacity and checked failure order. Byte-array snapshots use a single resize with known count. The existing text and JSON builder capacity and growth policy remains in place.

The software observers count actual decode entries, successful bytes, native requests, precise roots and marks. Copy/scan totals are reconstructed from exact successful byte counts and the verified fixed path multiplicity. The observers wrap application execution separately from ordinary timing. Text/JSON cohorts include one warm and three checked operations plus their application output and setup. Each JSON operation parses and serializes an object with 32 or 128 numeric members. Python's independent input sizes and the suite's expected lengths agree, and the suite also checks the entire serialized document byte for byte. In JSON-128 normal JIT, 1,029 decodes removed 1,029 native requests and 17,871 requested bytes.

| Observer cohort | Decode calls | Decoded bytes | Validation passes | Text bytes copied | Native requests | Native requested bytes |
| --- | --- | --- | --- | --- | --- | --- |
| decode-normal | 1001 | 7175168 | 3,003 → 1,001 | 21,525,504 → 14,350,336 | 5,454 → 4,453 | 21,762,602 → 14,579,426 |
| decode-optimized | 1001 | 7175168 | 3,003 → 1,001 | 21,525,504 → 14,350,336 | 5,454 → 4,453 | 21,762,602 → 14,579,426 |
| json-128-normal | 1029 | 9639 | 3,087 → 1,029 | 28,917 → 19,278 | 211,014 → 209,985 | 12,942,773 → 12,924,902 |
| json-128-optimized | 1029 | 9639 | 3,087 → 1,029 | 28,917 → 19,278 | 211,014 → 209,985 | 12,942,773 → 12,924,902 |
| json-32-normal | 261 | 2283 | 783 → 261 | 6,849 → 4,566 | 46,737 → 46,476 | 2,846,949 → 2,842,578 |
| json-32-optimized | 261 | 2283 | 783 → 261 | 6,849 → 4,566 | 46,737 → 46,476 | 2,846,949 → 2,842,578 |
| suite-normal | 6464 | 236770 | 19,376 → 6,464 | 710,310 → 473,540 | 442,540 → 436,084 | 27,168,161 → 26,879,743 |
| suite-optimized | 6464 | 236770 | 19,376 → 6,464 | 710,310 → 473,540 | 442,540 → 436,084 | 27,168,161 → 26,879,743 |
| text-1024-normal | 4120 | 107872 | 12,352 → 4,120 | 323,616 → 215,744 | 157,382 → 153,266 | 7,838,656 → 7,697,856 |
| text-1024-optimized | 4120 | 107872 | 12,352 → 4,120 | 323,616 → 215,744 | 157,382 → 153,266 | 7,838,656 → 7,697,856 |
| text-256-normal | 1048 | 26976 | 3,136 → 1,048 | 80,928 → 53,952 | 38,818 → 37,774 | 1,971,546 → 1,936,218 |
| text-256-optimized | 1048 | 26976 | 3,136 → 1,048 | 80,928 → 53,952 | 38,818 → 37,774 | 1,971,546 → 1,936,218 |

## Correctness and qualification

- Linux Release 104/104, with a separate focused 5/5 gate. Fatal ASan, UBSan and LeakSanitizer 103/103. The broad selections include JSON grammar and budget errors, configuration/atomic save, exact foreign-heap rejection, worker/async cancellation, callbacks and reload, checked collections, and Unicode and native ABI controls.
- Fatal TSan passed 300 concurrent decodes of 256 KiB against 6,936 completed collector operations, with exact output and no collection failure. Runtime coordination and invariants passed separately. The first prototype's TSan runner omitted the invariant binary target initially. The final runner builds both controls before testing them.
- Native Windows 10 x64 build 19045 Release 71/71 and Debug 71/71. The guest verifies 3,319 source hashes plus the installed provider identities. Normal and optimized AOT and JIT string tests run under the existing LLVM and LLD. PE headers verify 1 MiB reserves for the selected binaries, including the string JIT host and both independently linked string AOT executables.
- The new maintained conversion test covers 1, 2, 3 and 4-byte scalars, NUL, a combining sequence and a truncated final scalar across multiple snapshot chunks. It denies the first allocation for a short malformed input, sweeps cold allocation failures with aliased output, forces collection and checks exact recovery and empty publication. Existing positive and negative scalar-index, overflow, invalid encoding, ownership and reload tests passed.
- Source hashes were checked against the Linux/Windows source packet. The production Runtime C symbol sets match. Retained native IR and disassembly cover the changed decoder and publication path.

## Runtime measurements

Alternating process order supplies four matched blocks per throughput route, with three checked timings per source process. Timing runs have observers off. Linux qualification and the guest build had finished. Windows Debug qualification tests were active on the guest during primary and first repeat measurements. Native decode loop timings use 1,000 calls. AOT emission is independent and linked against each variant's Runtime. Application control throughput variation is retained in full. Same-object and unaffected workloads do not establish a generated code improvement.

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| 0 | 0.293 | 0.279 | -4.7% |
| 1024 | 5.070 | 2.499 | -50.7% |
| 12 | 0.882 | 0.862 | -2.3% |
| 6 | 0.901 | 0.801 | -11.1% |
| 64 | 1.252 | 0.945 | -24.5% |
| 65536 | 255.041 | 108.777 | -57.3% |

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| aot/normal/materialize | 32.359 | 17.018 | -47.4% |
| aot/optimized/materialize | 34.969 | 18.675 | -46.6% |
| jit/O2/async-tasks | 59.762 | 61.398 | +2.7% |
| jit/O2/calls | 0.738 | 0.776 | +5.2% |
| jit/O2/closures | 1.333 | 1.293 | -3.0% |
| jit/O2/materialize | 31.354 | 16.390 | -47.7% |
| jit/O2/workers | 1.236 | 1.323 | +7.0% |
| jit/normal/async-tasks | 59.861 | 59.987 | +0.2% |
| jit/normal/calls | 0.759 | 0.789 | +3.9% |
| jit/normal/closures | 1.396 | 1.332 | -4.6% |
| jit/normal/materialize | 31.803 | 16.688 | -47.5% |
| jit/normal/workers | 1.208 | 1.301 | +7.7% |

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| aot/normal/json-128 | 37.516 | 38.404 | +2.4% |
| aot/normal/json-32 | 5.896 | 6.021 | +2.1% |
| aot/normal/text-1024 | 54.840 | 55.137 | +0.5% |
| aot/normal/text-256 | 13.469 | 13.636 | +1.2% |
| aot/optimized/json-128 | 36.434 | 37.254 | +2.3% |
| aot/optimized/json-32 | 5.765 | 5.828 | +1.1% |
| aot/optimized/text-1024 | 52.959 | 54.266 | +2.5% |
| aot/optimized/text-256 | 13.318 | 13.326 | +0.1% |
| jit/normal/json-128 | 41.409 | 41.348 | -0.1% |
| jit/normal/json-32 | 6.364 | 6.820 | +7.2% |
| jit/normal/text-1024 | 57.413 | 58.181 | +1.3% |
| jit/normal/text-256 | 13.819 | 13.968 | +1.1% |
| jit/optimized/json-128 | 41.964 | 40.780 | -2.8% |
| jit/optimized/json-32 | 6.715 | 6.425 | -4.3% |
| jit/optimized/text-1024 | 58.713 | 57.337 | -2.3% |
| jit/optimized/text-256 | 14.269 | 14.223 | -0.3% |

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| aot/normal/array-1024 | 1.143 | 1.164 | +1.8% |
| aot/normal/array-4096 | 4.533 | 4.682 | +3.3% |
| aot/normal/dependent-1000000 | 27.156 | 31.042 | +14.3% |
| aot/normal/dict-1024 | 4.208 | 4.116 | -2.2% |
| aot/normal/float-1000000 | 27.344 | 30.823 | +12.7% |
| aot/normal/integer-1000000 | 28.072 | 30.733 | +9.5% |
| aot/normal/integer-4000000 | 111.974 | 122.978 | +9.8% |
| aot/normal/io-200 | 6.764 | 6.714 | -0.7% |
| aot/normal/sort-1024 | 28.244 | 29.188 | +3.3% |
| aot/normal/sort-256 | 5.537 | 5.743 | +3.7% |
| aot/optimized/array-1024 | 1.138 | 1.113 | -2.2% |
| aot/optimized/array-4096 | 4.584 | 4.442 | -3.1% |
| aot/optimized/dependent-1000000 | 24.978 | 26.141 | +4.7% |
| aot/optimized/dict-1024 | 4.149 | 4.210 | +1.5% |
| aot/optimized/float-1000000 | 25.029 | 25.781 | +3.0% |
| aot/optimized/integer-1000000 | 25.283 | 29.763 | +17.7% |
| aot/optimized/integer-4000000 | 100.553 | 118.692 | +18.0% |
| aot/optimized/io-200 | 6.598 | 6.750 | +2.3% |
| aot/optimized/sort-1024 | 28.462 | 28.209 | -0.9% |
| aot/optimized/sort-256 | 5.538 | 5.569 | +0.6% |
| jit/normal/array-1024 | 1.097 | 1.087 | -0.9% |
| jit/normal/array-4096 | 4.351 | 4.491 | +3.2% |
| jit/normal/dependent-1000000 | 29.814 | 30.759 | +3.2% |
| jit/normal/dict-1024 | 4.466 | 4.423 | -1.0% |
| jit/normal/float-1000000 | 30.259 | 31.044 | +2.6% |
| jit/normal/integer-1000000 | 30.220 | 31.677 | +4.8% |
| jit/normal/integer-4000000 | 120.399 | 122.794 | +2.0% |
| jit/normal/io-200 | 6.756 | 6.860 | +1.5% |
| jit/normal/sort-1024 | 29.479 | 28.729 | -2.5% |
| jit/normal/sort-256 | 5.992 | 5.889 | -1.7% |
| jit/optimized/array-1024 | 1.123 | 1.164 | +3.6% |
| jit/optimized/array-4096 | 4.507 | 4.624 | +2.6% |
| jit/optimized/dependent-1000000 | 29.356 | 30.410 | +3.6% |
| jit/optimized/dict-1024 | 4.481 | 4.534 | +1.2% |
| jit/optimized/float-1000000 | 29.223 | 30.554 | +4.6% |
| jit/optimized/integer-1000000 | 29.476 | 30.082 | +2.1% |
| jit/optimized/integer-4000000 | 116.916 | 121.604 | +4.0% |
| jit/optimized/io-200 | 6.904 | 6.830 | -1.1% |
| jit/optimized/sort-1024 | 29.572 | 29.030 | -1.8% |
| jit/optimized/sort-256 | 5.958 | 5.938 | -0.3% |

The eight additional balanced normal-JIT and optimized-AOT blocks preserve the following repeat measurements. The 1,344 checked samples use the same independent expected outputs.

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| aot-optimized/json-128 | 37.194 | 37.871 | +1.8% |
| aot-optimized/json-32 | 5.817 | 6.109 | +5.0% |
| aot-optimized/text-1024 | 54.755 | 54.750 | -0.0% |
| aot-optimized/text-256 | 13.490 | 13.770 | +2.1% |
| jit-normal/json-128 | 42.599 | 42.652 | +0.1% |
| jit-normal/json-32 | 6.797 | 6.941 | +2.1% |
| jit-normal/text-1024 | 59.855 | 58.883 | -1.6% |
| jit-normal/text-256 | 14.383 | 14.211 | -1.2% |

An additional eight matched optimized-AOT blocks used the same linked images after the guest was idle.

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| integer-1000000 | 24.679 | 29.055 | +17.7% |
| integer-4000000 | 98.491 | 115.520 | +17.3% |
| dependent-1000000 | 24.566 | 25.956 | +5.7% |
| float-1000000 | 24.301 | 25.596 | +5.3% |
| array-1024 | 1.130 | 1.133 | +0.3% |
| array-4096 | 4.416 | 4.500 | +1.9% |
| sort-256 | 5.479 | 5.506 | +0.5% |
| sort-1024 | 27.903 | 27.582 | -1.1% |
| dict-1024 | 4.110 | 4.272 | +4.0% |
| text-256 | 13.098 | 13.060 | -0.3% |
| text-1024 | 53.700 | 52.979 | -1.3% |
| json-32 | 5.770 | 5.877 | +1.9% |
| json-128 | 35.722 | 36.812 | +3.1% |
| io-200 | 6.853 | 6.828 | -0.4% |

The optimized AOT integer controls are the integration blocker. In the quiet-guest cohort, 1 million iterations go from 24.679 to 29.055 ms and 4 million from 98.491 to 115.520 ms. Both worsen in all eight balanced pairs and the earlier cohorts show the same direction. The emitted suite object is byte-identical. Disassembly of the linked integer loop and its polling calls preserves the hot instructions and call targets after normalizing addresses. The entry moves from 0x215190 to 0x215270 and the ABI poll moves from 0x2b0480 to 0x2b07c0. The integer loop still calls `vd_gc_poll` and `vd_worker_checkpoint` in its hot path.

Four additional balanced whole-suite hardware-counter pairs record median retired instructions of 9.867 → 9.855 billion and cycles of 4.586 → 4.898 billion. Sampled cycles remain concentrated in polling and the emitted hot code. The profile includes the entire suite, so it cannot allocate the cycle difference to a specific call or microarchitectural cause. Link placement is a plausible contributor. A controlled causal attribution remains open. The reproduced regression is enough to hold integration.

## Compiler, startup and native size

The compiler source is unchanged. The 100, 500 and 2,000-site flat fixtures and mixed fixture use two alternating blocks, with normal/O2 JIT and separate AOT emission. Peak RSS is per process from wait4. Startup exits via the suite's cold argument path and uses six alternating AOT blocks. Representative emission and link are single measurements per side.

| Process | G44 wall ms | G46 wall ms | Change | G44 peak MiB | G46 peak MiB |
| --- | --- | --- | --- | --- | --- |
| materialize-emit-normal | 299.745 | 298.465 | -0.4% | 54.5 | 54.3 |
| materialize-emit-optimized | 289.162 | 295.722 | +2.3% | 56.0 | 56.0 |
| materialize-link-normal | 40.961 | 41.561 | +1.5% | 58.9 | 59.0 |
| materialize-link-optimized | 33.351 | 36.247 | +8.7% | 59.5 | 58.9 |
| scaling-aot-O2-flat-100 | 105.998 | 105.872 | -0.1% | 46.3 | 46.7 |
| scaling-aot-O2-flat-2000 | 2922.648 | 2910.689 | -0.4% | 127.5 | 128.2 |
| scaling-aot-O2-flat-500 | 485.182 | 506.217 | +4.3% | 63.1 | 63.2 |
| scaling-aot-O2-mixed | 51.100 | 51.993 | +1.7% | 44.6 | 44.6 |
| scaling-aot-normal-flat-100 | 99.304 | 101.842 | +2.6% | 47.4 | 47.2 |
| scaling-aot-normal-flat-2000 | 3789.747 | 3687.468 | -2.7% | 182.4 | 182.0 |
| scaling-aot-normal-flat-500 | 605.293 | 570.132 | -5.8% | 75.1 | 74.7 |
| scaling-aot-normal-mixed | 38.044 | 38.497 | +1.2% | 42.5 | 42.8 |
| scaling-jit-O2-flat-100 | 108.802 | 109.789 | +0.9% | 49.2 | 48.9 |
| scaling-jit-O2-flat-2000 | 3027.835 | 3035.224 | +0.2% | 137.6 | 137.7 |
| scaling-jit-O2-flat-500 | 532.917 | 597.688 | +12.2% | 66.6 | 66.9 |
| scaling-jit-O2-mixed | 50.896 | 51.857 | +1.9% | 47.9 | 47.8 |
| scaling-jit-normal-flat-100 | 108.317 | 108.096 | -0.2% | 49.7 | 49.5 |
| scaling-jit-normal-flat-2000 | 4066.682 | 4490.764 | +10.4% | 192.2 | 192.9 |
| scaling-jit-normal-flat-500 | 623.149 | 632.992 | +1.6% | 78.6 | 78.6 |
| scaling-jit-normal-mixed | 40.226 | 39.938 | -0.7% | 45.0 | 45.1 |
| startup-normal | 5.515 | 5.904 | +7.1% | 13.4 | 13.4 |
| startup-optimized | 5.645 | 5.718 | +1.3% | 13.4 | 13.4 |
| suite-emit-normal | 2938.173 | 2750.141 | -6.4% | 118.1 | 118.0 |
| suite-emit-optimized | 3687.641 | 3899.052 | +5.7% | 117.7 | 117.6 |
| suite-link-normal | 86.029 | 50.921 | -40.8% | 63.9 | 64.5 |
| suite-link-optimized | 40.177 | 47.320 | +17.8% | 63.4 | 63.5 |

A six-block repeat after the guest qualification completed targets the adverse normal flat-2,000, O2 flat-500 and cold startup samples. Its separate receipt includes analysis, JIT, CPU and RSS.

| Workload | G44 ms | G46 ms | Change |
| --- | --- | --- | --- |
| flat500-O2 | 489.763 | 499.074 | +1.9% |
| flat2000-normal | 3875.895 | 3817.408 | -1.5% |
| startup-normal | 5.707 | 5.587 | -2.1% |

| Independent AOT | Object SHA-256 comparison | G44 object bytes | G46 object bytes | G44 executable bytes | G46 executable bytes |
| --- | --- | --- | --- | --- | --- |
| suite/normal | same | 1533648 | 1533648 | 5116480 | 5117648 |
| suite/optimized | same | 1260744 | 1260744 | 4999880 | 5001040 |
| materialize/normal | same | 147616 | 147616 | 3231448 | 3232608 |
| materialize/optimized | same | 124152 | 124152 | 3220424 | 3221616 |

All eight flat/mixed and four representative/focused AOT object pairs have matching SHA-256 hashes. The linked executable size differences are from rebuilding the Runtime. The pure compiler timing controls share the same compiler source and generated AOT objects.

## Collection and marking

The table retains exact counts and p99 observations from the observer runs. Root words count the precise frame slots visited. Mark counts are unique marked objects per collection. Surviving payload sums aggregate per-cycle amounts and are not end-of-run heap sizes. Raw receipts retain fresh managed constructions, reused payloads, buffer adoptions, and each timed interval. Full stopped time spans the observation of all threads ready through collector completion and wake notification under the coordination lock. Request elapsed includes rendezvous. Neither time is just the `Heap::collect` body. These small samples do not establish a GC tail bound.

| Observer cohort | Collections | Root words | Marks | Sum of surviving bytes | G44 stopped p99 ms | G46 stopped p99 ms | G44 request p99 ms | G46 request p99 ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| decode-normal | 112 → 112 | 2,729 → 2,729 | 560 → 560 | 2,403,006 → 2,403,006 | 0.002 | 0.003 | 0.002 | 0.003 |
| decode-optimized | 112 → 112 | 2,729 → 2,729 | 560 → 560 | 2,403,006 → 2,403,006 | 0.044 | 0.003 | 0.045 | 0.003 |
| json-128-normal | 25 → 25 | 16,258 → 16,258 | 10,233 → 10,233 | 448,822 → 448,822 | 0.467 | 0.625 | 0.468 | 0.626 |
| json-128-optimized | 25 → 25 | 16,258 → 16,258 | 10,233 → 10,233 | 448,822 → 448,822 | 0.363 | 1.129 | 0.363 | 1.129 |
| json-32-normal | 10 → 10 | 6,515 → 6,515 | 1,303 → 1,303 | 52,857 → 52,857 | 0.120 | 0.104 | 0.120 | 0.104 |
| json-32-optimized | 10 → 10 | 6,515 → 6,515 | 1,303 → 1,303 | 52,857 → 52,857 | 0.266 | 0.181 | 0.266 | 0.181 |
| suite-normal | 124 → 124 | 68,745 → 68,745 | 52,872 → 52,872 | 2,211,374 → 2,211,374 | 0.645 | 0.670 | 0.645 | 0.670 |
| suite-optimized | 124 → 124 | 68,745 → 68,745 | 52,872 → 52,872 | 2,211,374 → 2,211,374 | 0.573 | 0.777 | 0.573 | 0.777 |
| text-1024-normal | 49 → 49 | 16,002 → 16,002 | 36,014 → 36,014 | 1,295,334 → 1,295,334 | 1.189 | 0.608 | 1.189 | 0.608 |
| text-1024-optimized | 49 → 49 | 16,002 → 16,002 | 36,014 → 36,014 | 1,295,334 → 1,295,334 | 1.041 | 0.757 | 1.042 | 0.757 |
| text-256-normal | 20 → 20 | 6,548 → 6,548 | 4,146 → 4,146 | 134,355 → 134,355 | 0.325 | 0.438 | 0.325 | 0.438 |
| text-256-optimized | 20 → 20 | 6,548 → 6,548 | 4,146 → 4,146 | 134,355 → 134,355 | 0.192 | 0.185 | 0.192 | 0.185 |

Precise roots, marking, object construction, reuse, buffer adoptions and per-cycle surviving payload matched in all 12 cohort pairs. The first optimized JSON-128 observer run measured 0.363 → 1.129 ms stopped p99. The quiet-guest repeat makes four matched observations in each mode and retains 100 collection intervals per side.

| JSON-128 mode | Collections per side | G44 stopped p99 ms | G46 stopped p99 ms |
| --- | --- | --- | --- |
| normal | 100 | 0.377 | 0.367 |
| optimized | 100 | 0.353 | 0.347 |

The G36 adverse JSON-tail result of 1.188 → 1.782 ms and the G44 sorting and text timing variations remain in the historical entries. Performance control regressions in the tables are retained as measured. The extra copy and validating scan were a measurable cost even where the end-to-end improvement was within timing variation.

## Rejected prototype and remaining work

The first prototype reserved eight prefix bytes in the native snapshot. At 12 bytes of text it crossed inline-string capacity and increased the requested storage. More seriously, a short malformed string attempted allocation before UTF-8 rejection. With the first allocation denied, the original Runtime and final candidate both report malformed UTF-8 and preserve the output. The prototype instead raised allocation failure. Its patch, measurements, partial qualification and the independent three-way fault probe are archived. The final campaign replaced the prototype's Linux and guest builds. The old sanitizer selection overlapped the start of the final campaign and wrote to the same log path. The final source hashes, final JUnit report and final CTest log identify the qualified run. The earlier permission failure and overlapping receipts are retained separately. An initial guest resume script had a quoting error and was corrected before the final source packet was used.

The existing general text join/split accumulators, encode-to-byte-array snapshots, Unicode traversal outside decode, JSON escape and key processing, and checked collection costs remain open. Other ledger objectives include broader escape and ownership proofs, sorting/dictionary performance, SIMD beyond G38, compilation/JIT costs, GC tails, startup and future macOS/release qualification. The next action is to isolate the integer loop with its exact poll and worker-checkpoint calls, compare the stock Runtime and new publication path in a matched link, and explain the cycle gap before changing another text or JSON path. Once that control is resolved, count calls to `to_utf8` in the JSON writer and parser before changing copy or budget behavior.

## Artifacts and review

| Qualified Runtime or test postimage | SHA-256 |
| --- | --- |
| include/void/runtime.hpp | `a9fe2993f109a2619cb6be103e08f5afbcd3f93a752bed064b05f469a295e387` |
| src/runtime/array.cpp | `878a4cc755f35bdb8467ec123a4f2d18dc05946e7fc0b127e4a0d6e5728134d1` |
| src/runtime/coordination.cpp | `41085ba1ea566fa5da1bfbae8ebdabd578f2cc90313c586401ac788b58408595` |
| tests/stage8/sprint3.hpp | `e079ffb801d2e0844e5279ad00872cb27d073cfab36639086944b73645f94e36` |

The DEFLATE-9 archive is `g46-text-unicode-json-performance-2026-11-16.zip`. Its adjacent receipt records every manifest hash, CRC and restoration of the exact Git source patch. Raw data include build and provider identities, source oracles, qualifier receipts, timing/RSS observations, native IR/disassembly and the rejected prototype. The adjacent cleanup receipt records task-owned allocation bytes, observed host free-space change and observed guest free-space change. The four historical/report files and the updated ledger are retained alongside the four Runtime/test changes. No staging or commit is part of the handoff. Live remote main remains at accepted G44.
