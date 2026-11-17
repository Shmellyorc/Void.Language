# G50 — JSON punctuation without temporary text

Accepted baseline G48 `4790a7a52ecf42732d12e3adc35cac3812246516`. The worktree is isolated at that exact commit. The G49 source stays on its held worktree. Its verified historical report and ledger entry are carried forward. [G49 qualification and hold](G49_DICTIONARY_INSERTION_PROBE_REUSE.md).

## Implementation and scope

The private JSON writer used its general string-to-byte path for six single-byte ASCII tokens. Each bracket, brace, comma or colon first constructed a managed string. The writer checked the token length, encoded it into a temporary managed byte list and pushed its sole byte into the existing output list. G50 routes these six existing call sites through a private punctuation method and the same checked byte writer. The output list, growth strategy, writer frames and serialization order are shared with all other tokens.

Go's shipped array and map encoders emit punctuation directly into the encoder buffer. Released Serde JSON v1.0.145 writes compact delimiters as byte literals. LLVM's pinned JSON stream writes punctuation directly while maintaining its existing output stack. Odin's current marshaler also sends container markers and commas through its byte writer. The applicable technique is writing the fixed ASCII token directly while preserving VOID's metering and builder. [Go 1.25 encoder](https://github.com/golang/go/blob/go1.25.0/src/encoding/json/encode.go#L848) · [Serde JSON compact formatter](https://raw.githubusercontent.com/serde-rs/json/v1.0.145/src/ser.rs) · [LLVM 23.1.4 JSON writer](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Support/JSON.cpp) · [Odin marshaler](https://github.com/odin-lang/Odin/blob/master/core/encoding/json/marshal.odin#L784).

## Metering, failure and GC proof

The old one-byte text path first charged one unit for the source token, then checked the output limit, then charged one unit for the emitted byte and pushed it. The new punctuation method still charges the first unit before calling the existing checked byte method, which checks the output limit, charges the second unit and pushes the byte. A duplicate extent check after the temporary encode had the same input extent because the writer owns the output list and the encode could not mutate it. The admitted byte is identical for every converted site. Once admitted, that byte reaches the same checked push and capacity growth, in the same position relative to all other output and fallible operations.

The removed string and byte-buffer allocations remove their own failure opportunities. The remaining allocation points, recovery paths and all logical errors retain their order. This change operates on producer-known ASCII bytes. Text and key escaping, Unicode input validation, exact number tokens, runtime UTF-8 decode, output UTF-8 validation and text construction outside this writer use the existing source and Runtime paths. No new reference, root slot type or storage lifetime is needed. Existing Runtime calls around traversal and output retain their existing GC and worker contracts.

The maintained metering test adds 513 G48-matched results, including independent exact serialized bytes, all 384 work limits in the chosen range, 64 output limits and 64 joint-boundary checks checking error code, location, path and message. It covers empty nested arrays and objects, nonlexical insertion order, Unicode and escapes. The previous 2,048-line typed-decode oracle remains byte-for-byte intact.


## Operation counts

The observer and ordinary builds link the same Runtime separately. Forty-eight observer executions passed both the independent result checks and an exact structural-token counter. Each case includes setup, one warmup and three sampled actions. The count changes agree in normal and optimized AOT. Each avoided punctuation token saves exactly one managed string construction request, one UTF-8 encode with a temporary byte-list header and backing, two string byte-length queries and one checked scalar read. The checked output push count and general managed struct construction count match. Every observed counter matches for short ASCII, long ASCII, Unicode with escapes, standalone dictionary, sorting, integer and array controls.

| Case | Removed literal constructions and UTF-8 encodes, each | Removed length queries | Removed checked reads | Linked native C++ allocation requests G48 → G50 |
| --- | ---: | ---: | ---: | ---: |
| String array, 256 items | 4,112 | 8,224 | 4,112 | 393,054 → 340,676 |
| Nested object and arrays, 96 records | 23,196 | 46,392 | 23,196 | 2,094,322 → 1,517,894 |
| Typed dictionary construction and serialization, 128 keys | 6,152 | 12,304 | 6,152 | 669,830 → 581,716 |

The allocation observer wraps linked C++ allocation functions across each process. Those counts include allocation effects from changed heap activity as well as the direct temporary requests. The linked package retains both independent output oracles and the individual counts. The emitted mixed image changes in the JSON writer. The dictionary, sort, array and integer fixture emitted under the same physical source paths produces byte-identical standalone objects and linked executables in both AOT modes.

The first comparison found identical isolated-control machine instructions with different embedded source paths. Go and Rust expose path-normalization options for precisely this reproducibility issue. G50 holds the physical source path constant in the replay instead of changing the compiler or removing diagnostics. That corrected the object and executable hash mismatch. Direct Google search returned a browser challenge, and the GitHub sources and official language documentation were accessible. [Go reproducibility](https://go.dev/blog/rebuild) · [Rust source path remapping](https://doc.rust-lang.org/rustc/remap-source-paths.html).

## Qualification

Linux Release passed 40/40. Two existing fixture runs detected that the related metering source was updated during their execution. The original receipts retain those invalidated results. The two tests and the updated metering test passed a separate exact-source rerun. The combined exit receipt references those three replacements. The initial benchmark harness also exposed a reversed sorting expectation. The maintained fixture and production implementation were unaffected by that harness correction.

The same Linux host executable tested both standard-library variants under forced collection and injected allocation failure. The JSON serialization and retry control reported 7,165,944 → 7,086,264 root observations and 11,215 → 11,061 polls in each JIT mode, with 85 general managed constructions in both. The wide readonly control matched 75,211,353 root observations and 119,908 polls in each mode. Each injected failure is followed by successful retry, forced collection and empty temporary-object checks. The failure sweep fell from 1,819 to 1,682 per mode. These are harness-observed failures before the first fully admitted invocation, including effects of changed collector activity.

Fatal TSan passed three Runtime controls. Independently linked normal and optimized AOT each produced 36 checked benchmark samples and matched 513 JSON output and boundary results under the TSan Runtime. All Linux qualification lanes use a hard 1 MiB native stack.

Fatal ASan, UBSan and LeakSanitizer passed 40/40, including the exact final writer fixture. Native Windows 10 x64 build 19045 passed 24/24 each in Debug and Release. The additional application fixture passed 288 independent checked outputs in normal/O2 JIT and independently linked normal/optimized AOT. The maintained metering fixture matched 20,488 lines across both Windows profiles and both JIT/AOT modes. The guest compile packet predates the test-only metering update. The native oracle runner copies the exact final two metering files and its source and executable hashes are retained separately. The optimized and normal application and metering PE headers, CLI and selected native tests reserve 1 MiB. Verification used LLVM/Clang/LLD 23.1.4 revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`.

## Observer-off elapsed time

The Linux throughput runs finished after all compilation and qualification tasks. Eight matched blocks alternate which variant executes first and rotate the four execution modes. Each process uses CPU 3, a hard 1 MiB stack and a warmup followed by three samples per case. JIT uses no disk cache. Python computes independent answers and serialized bytes. The table reports the median paired change in elapsed time from G48 to G50. Negative values are faster. All 2,304 sampled answers passed. Both AOT images are linked independently using the same Runtime archive and C++ toolchain.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| short-16 | -1.52% | +0.03% | -0.70% | -2.40% |
| long-32768 | -1.83% | -0.89% | +0.68% | -1.34% |
| unicode-escapes | -1.17% | -0.28% | +0.55% | -2.27% |
| string-array-256 | -14.13% | -10.02% | -8.64% | -9.39% |
| nested-96 | -24.46% | -26.34% | -24.18% | -24.62% |
| typed-128 | -14.83% | -14.09% | -16.09% | -13.53% |
| dictionary-1024 | -3.92% | -1.26% | -3.37% | -1.50% |
| sort-1024 | -1.55% | -1.67% | +4.59% | +3.36% |
| array-1024 | -0.47% | -0.41% | -2.69% | +2.78% |
| array-4096 | -0.78% | -0.84% | -3.19% | +2.41% |
| integer-1000000 | -0.30% | -0.12% | +0.93% | +0.16% |
| integer-4000000 | +0.85% | -0.72% | +0.61% | +0.43% |

All 32 matched pairs improved for nested serialization, the 256-string array and typed dictionary construction plus serialization. The standalone string workloads exercise the old escape path and remove zero structural tokens. The exact gains above apply to workloads that actually emit the optimized punctuation. Timings cover one physical Ryzen 7 1700X host. Native Windows provides correctness and stack qualification.

### Adverse AOT replays

The mixed normal-AOT sorting case was slower in seven of eight primary pairs. The exact mixed image, run with only sorting selected, repeated at +4.59% with eight of eight pairs slower. Optimized-AOT sorting changed from +3.36% in the primary run to +0.78% in the focused replay. Optimized-AOT array-1,024 went from +2.78% to +4.43% with eight of eight focused pairs slower. Optimized-AOT array-4,096 repeated at +1.52%. Strict guard results for both independently linked AOT modes are retained even when they favor G48.

The standalone control builds produce identical objects and linked images across G48 and G50. Their median sorting costs varied by +1.79% normal and +1.53% optimized. Optimized array-1,024 varied by −0.05%. The mixed optimized object has identical function bytes for sorting, its comparison helper and the array loop. In the normal mixed object, the comparison helper differs by two relative calls to the same callable dispatch symbol. Linked addresses move in the mixed images, including the GC poll and worker checkpoint. The measured association establishes no particular cache or branch mechanism. The qualification keeps both the mixed replay and the standalone control. [Google Benchmark variance controls](https://github.com/google/benchmark/blob/main/docs/reducing_variance.md) · [Go benchstat methodology](https://pkg.go.dev/golang.org/x/perf/cmd/benchstat) · [Rust perf code-generation comparison](https://github.com/rust-lang/rustc-perf/blob/main/collector/README.md).

The bounded source trial removes the extra punctuation-helper call. It charges the same token work against the live writer context already present in `_write`, then calls the existing checked `byte` method. Clearing a source-level call is measurable in the emitted path. The trial uses the direct writer pattern from Go and Serde. It needs no tuning by address, file order or benchmark identity. Its output and metering are checked before timing.

The trial passed 1,458 sampled AOT answers in six balanced three-executable blocks and 1,026 independent output and budget checks. These numbers use the same stock G48 images, original G50 images and independently linked trial images.

| Selected case | Original G50 vs G48 | Direct-write trial vs G48 |
| --- | ---: | ---: |
| Normal AOT nested serialization | -22.48% | -26.72% |
| Optimized AOT nested serialization | -25.68% | -27.34% |
| Normal AOT sorting in mixed run | +3.90% | +3.18% |
| Normal AOT sorting selected in mixed image | +4.87% | +2.55% |
| Optimized AOT array-1,024 selected in mixed image | -1.93% | -4.14% |
| Optimized AOT sorting selected in mixed image | +0.44% | +2.06% |
| Optimized AOT dictionary in mixed run | -1.22% | +2.05% |

Normal-AOT sorting on the original G50 remained slower in all six focused trial blocks. The direct-write variant narrowed that loss. Both its focused and mixed-program normal-AOT sorting medians remained slower than G48. In this run the original optimized-AOT array slowdown changed direction, so these data cannot establish that the trial corrected it. The trial also increased the generated normal writer body from 22,651 to 26,635 bytes while removing a 990-byte helper. The optimized body grew from 19,540 to 23,025 bytes while removing an 854-byte helper. This tradeoff leaves the original G50 implementation as the qualified worktree source. The trial is retained as an independently tested AOT alternative.

## Disposition

**HOLD for default integration.** The optimization removes measured construction and copying work and materially improves JSON across the four execution modes. The normal-AOT sorting loss repeated in the JSON-linked application, and the bounded source trial left a measurable sorting cost. The optimized-AOT array results vary by run. Production stays at G48. The verified archive preserves the exact six-file patch, source hashes, qualification receipts, independent fixtures, emitter and link identities, raw observer counters, raw timings, and the direct-write trial.

The production change is confined to `stdlib/json.void`. The maintained metering source and oracle are the test changes. The G49 historical report is copied byte-for-byte and the cumulative ledger retains the prior entries with their original status. The archive is `g50-json-text-construction-2026-11-16.zip`.

## Final recovery

**HOLD confirmed and recovery closed.** This entry updates the documentation in a separate local worktree. The original six qualified G50 files and the original evidence archive retain their recorded hashes. The recovery introduced zero source, test, compiler or link changes.

The archived normal-AOT mixed application reproduces the sorting cost with only sorting selected. The original instrumented executions count 114,688 checked scalar reads, 16,384 scalar pushes and 10,575 linked native C++ allocation requests on both sides. The sorting entry, merge loop and numeric comparator have identical object-code bytes. The two changed instructions in `_precedes` are relative calls to the same dispatch function at a new address. Its successful path has the same body and calls the same numeric comparator. Link placement moved the merge loop by 0x80 and the GC poll by 0x380. The byte-identical standalone sorting control provides a separate noise check.

### Exact linked-image replay

The retained binaries and input hashes were checked against the original archive. Twelve paired blocks alternated executable order and pinned each process to CPU 3 on the Ryzen 7 1700X. The retirement and fetch counters ran in separate passes with 100% active time and a 1 MiB native stack. Counters cover the complete process. The checked program reports three timed sort samples per launch after warmup. Changes are medians of paired percentages from G48 to G50.

| Normal-AOT sort measurement | Change | Pairs with a higher count or time |
| --- | ---: | ---: |
| Mixed image, sort elapsed time | +4.37% | 12/12 |
| Mixed image, retired instructions | -0.11% | 0/12 |
| Mixed image, cycles | +4.10% | 12/12 |
| Mixed image, retired branches | -1.32% | 0/12 |
| Mixed image, branch misses | +5.53% | 9/12 |
| Mixed image, instruction-cache line fills from L2 | +21.45% | 10/12 |
| Mixed image, instruction-cache line fills from outside L2 | -0.14% | 5/12 |
| Mixed image, instruction-fetch stall cycles | +12.59% | 12/12 |
| Byte-identical standalone image, sort elapsed time | -0.23% | 5/12 |

The measured sorting operations match, and the slower process retires fewer instructions. The higher cycle count and instruction-fetch stalls are consistent with the changed executable layout. They do not establish which addresses, cache sets or branches account for the difference. The optimized-AOT array-1,024 replay was +1.67% elapsed and +1.01% cycles, with essentially unchanged retired instructions.

Six additional observer-off pairs per AOT mode ran the complete mixed fixture. The original normal/O2 JIT columns are carried forward from the verified G50 campaign on the exact same source. All 1,152 new sampled answers matched the retained independent oracles. Negative values are faster.

| Case | Qualified normal JIT | Qualified O2 JIT | Recovery normal AOT | Recovery optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| nested-96 | -24.46% | -26.34% | -24.90% | -23.41% |
| string-array-256 | -14.13% | -10.02% | -10.43% | -8.32% |
| typed-128 | -14.83% | -14.09% | -14.68% | -13.78% |
| dictionary-1024 | -3.92% | -1.26% | -2.66% | -1.78% |
| sort-1024 | -1.55% | -1.67% | +5.54% | +5.50% |
| array-1024 | -0.47% | -0.41% | -1.06% | +1.04% |
| array-4096 | -0.78% | -0.84% | +0.05% | +1.20% |
| integer-1000000 | -0.30% | -0.12% | +0.29% | -2.41% |
| integer-4000000 | +0.85% | -0.72% | +0.33% | -1.01% |

### LLVM and LLD applicability

- The pinned LLD ELF call-graph sort ships cache-directed and call-chain section layout. The archived application object has a single executable input `.text` section. The linker can order input sections, so the option alone cannot reposition the sort, its dispatcher and the writer within that section. The corresponding COFF linker also provides call-graph ordering. Applying it here would require a change to application emission and a representative profile policy with cross-platform qualification. [Pinned LLD ELF implementation](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/lld/ELF/CallGraphSort.cpp) · [Pinned LLD COFF options](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/lld/COFF/Options.td).
- The installed LLVM includes BOLT, which ships profile-guided post-link layout for ELF. The retained executables lack the static text relocations recommended for function rearrangement. A BOLT run would require a different link and a representative profile, and it would leave the Windows PE path outside the result. The LLVM layout-bias RFC describes the effect of changing layout during measurement. Its padding-based proposal is excluded by this task. [Pinned BOLT workflow](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/bolt/README.md#usage) · [LLVM layout-bias RFC](https://discourse.llvm.org/t/rfc-lld-feature-for-controlling-for-code-size-dependent-measurement-bias/83334).

The earlier direct-write source trial already preserved the JSON gains and left mixed normal-AOT sorting slower. The recovery evidence supports no maintainable local source or link correction. The original JSON implementation, 513 added metering checks, allocation-failure and GC receipts, and Linux and Windows qualification all retain their exact identities. No fresh JIT or Windows qualification was needed for this documentation-only disposition. G48 remains production. Recovery evidence is `g50-final-recovery-2026-11-16.zip`.
