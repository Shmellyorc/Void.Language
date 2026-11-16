# G45-V — MachineSinking host-code correction

**Disposition: HOLD.** The sole correction is rejected for adoption. Nested gains survive, but the correction does not establish an acceptable flat-function result. The original production-baseline regression remains partly confounded by different host build artifacts. No LLVM experiment is imported into VOID or the installed SDK, and nothing is submitted upstream.

Accepted production baseline: `894e2e8d9930028f86737092d3ac74010676f389`. Task branch: `codex/g45v-host-code`, in `/home/shmellyorc/Documents/Void.Language-G45V`. LLVM/Clang/LLD remain 23.1.4, pinned to `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. The archived stock source was also checked byte-for-byte against the [pinned upstream file](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/CodeGen/MachineSink.cpp).

## Finding and bounded correction

There are two distinct findings. First, delayed RPO changes the compiled inlining structure: stock and original RPO inline `ProcessBlock` and `SinkInstruction` into `run`; delayed RPO emits `ProcessBlock` separately. Second, the installed SDK's stock object is materially different from a fresh build of pinned stock source. Earlier whole-compiler comparisons used the SDK stock implementation against rebuilt experimental objects. A source-neutral rebuild control is therefore necessary before assigning that entire difference to the traversal patch.

The one correction adds `LLVM_ATTRIBUTE_ALWAYS_INLINE` to the existing private `ProcessBlock` declaration in delayed RPO. It preserves the first two layout sweeps, subsequent RPO traversals, fixed-point loop, edge splitting and every movement-legality check. It adds no cache, metadata, benchmark condition, public API or allocation framework. It does not change padding, alignment or linker ordering as an optimization.

The correction removes the `ProcessBlock` call but causes `SinkInstruction` to be emitted out of line. It executes about **73.5 million more instructions than delayed RPO** on the same flat MIR, approximately +0.81%. Delayed RPO already executes about +3.28% more instructions than rebuilt stock. These observations establish a real host-code cost; they do not isolate which SDK build option or individual instruction sequence accounts for the original production-baseline regression. Cache layout, prediction and profile-guided build effects remain hypotheses, not proven causes. No second tuning patch was tried.

## Host-code and hardware evidence

All four rebuilt variants use the same driver objects, installed static libraries, compiler and `-O3 -DNDEBUG` flags. Line-table diagnostics were verified to leave their host instructions unchanged. Counter observers preserve the key inline/outline decisions. Hardware counters are enabled only around `MachineSinking::run`; the MIR parser and logging are outside the interval, with a small counter enable/disable boundary included. The observer adds entry/exit code, so ordinary sampled binaries and LLVM pass timers are separate controls.

| Variant | Instructions, billions | Branches, billions | Cycles, billions | IPC | Branch misses | Generic L1-I misses |
| --- | --- | --- | --- | --- | --- | --- |
| Rebuilt stock | 8.802856 | 2.043709 | 7.527 | 1.169 | 484,736 | 10,861,298 |
| Original RPO | 8.798643 | 2.044131 | 7.635 | 1.153 | 518,500 | 10,734,578 |
| Delayed RPO | 9.091341 | 2.109780 | 7.357 | 1.236 | 830,414 | 10,647,892 |
| Forced-inline correction | 9.164813 | 2.115802 | 7.052 | 1.300 | 1,000,662 | 10,290,797 |

These are medians of four balanced forward/reverse blocks on identical flat MIR, pinned to logical CPU 3 of an AMD Ryzen 7 1700X. The recorded enabled/running times show no counter multiplexing. IPC is the median of each run's instructions/cycles ratio. Counter wall time, all raw counts, generic instruction-cache accesses, and the earlier three-variant exploratory block remain in the archive. Cycle and miss counts vary substantially; desktop and VM activity remained on the host. CPU pinning was not exclusive CPU reservation. The correction's lower cycle median in this block is retained and does not override the conflicting whole-compiler and embedded-pass results.

Uninstrumented cycle samples identify use/dominance checks, register-set operations, and the run/block processing code as hot. SDK stock and rebuilt stock have different `AllUsesDominatedByBlock` instruction sequences. The SDK object has hot/cold sections that the basic rebuild lacks; this is not proof of its exact optimization recipe. No claim is made that the cache or predictor alone caused the difference.

Compiler-reported static stack usage is 1,608 bytes for stock `run`, 1,688 for original RPO, 1,048 plus 760 for delayed RPO's `run`/`ProcessBlock`, and 1,064 plus 328 for the correction's `run`/`SinkInstruction`. These are individual frames, not dynamic maximum-stack totals. Greedy allocator dumps and `host/spill-summary.json` record newly introduced frame slots and static loads/stores to them; final disassembly and `.su` files are retained. Stack references are not mislabeled as measured dynamic spills. The audit reproduces the measured host instruction streams exactly. Executable controls separately enforce 1 MiB stacks.

## Full compiler and pass results

The ordinary whole-source results below use **rebuilt stock** as their matched build control. Flat has four balanced blocks; each other source/mode has two. All 64 invocations check independent expected results. These compilation measurements are separate from application throughput and observer runs.

| Source / JIT mode | Rebuilt stock s | Original RPO s | Delayed RPO s | Forced-inline correction s |
| --- | --- | --- | --- | --- |
| flat-2000 / normal | 4.4291 | 4.2809 | 4.2737 | 4.2248 |
| mixed / O2 | 0.0639 | 0.0619 | 0.0658 | 0.0638 |
| mixed / normal | 0.0515 | 0.0497 | 0.0506 | 0.0491 |
| nested-2000 / O2 | 60.8502 | 37.7571 | 38.1097 | 36.5869 |
| nested-2000 / normal | 97.5813 | 67.5651 | 64.7638 | 65.8646 |
| small / O2 | 0.0191 | 0.0191 | 0.0194 | 0.0192 |
| small / normal | 0.0202 | 0.0188 | 0.0192 | 0.0180 |

The correction preserves the large exact nested-2,000 gains, but normal nested compilation is approximately 1.7% slower than delayed RPO in this sample, while O2 is approximately 4.0% faster. Small/mixed timings are controls, not proof of generated-code improvement.

A separate four-block flat comparison preserves the original installed-production baseline and also inserts the extracted SDK stock object in the same standard overlay position as every rebuilt object:

| Flat whole normal JIT control | Median seconds |
| --- | --- |
| installed | 4.7881 |
| sdk | 4.4990 |
| stock | 5.0658 |
| adaptive | 5.2268 |
| corrected | 5.1267 |

The correction is approximately **7.1% slower than installed stock**, despite being approximately 1.9% faster than delayed RPO in this block. Rebuilding unchanged stock is itself approximately 5.8% slower than the installed host and 12.6% slower than the extracted SDK object control. The two stock link forms are controls, not a proposed linker-order optimization. Cross-series absolute times must not be combined.

Ordinary standalone MachineSinking pass medians on the same flat MIR are 2.4057 s rebuilt stock, 2.5367 s original RPO, 2.2863 s delayed RPO, 2.1815 s correction, and 1.9487 s SDK stock. This gives the correction a roughly 4.6% pass-time improvement over delayed RPO in this series, while leaving a roughly 11.9% cost versus the SDK object. Parsing, verification and MIR output are excluded from those LLVM pass timers.

Separate embedded observer medians, from 24 checked invocations, retain the adverse flat result and show where the nested improvement occurs:

| Embedded observer phase | Rebuilt stock s | Original RPO s | Delayed RPO s | Forced-inline correction s |
| --- | --- | --- | --- | --- |
| flat-2000-normal / Machine code sinking | 2.6093 | 2.3848 | 2.7511 | 2.8346 |
| flat-2000-normal / Machine Common Subexpression Elimination | 0.0435 | 0.0430 | 0.0427 | 0.0423 |
| flat-2000-normal / jit_native_codegen | 5.3548 | 5.0481 | 5.5666 | 5.5878 |
| nested-2000-normal / Machine code sinking | 30.9545 | 1.3470 | 1.3956 | 1.4643 |
| nested-2000-normal / Machine Common Subexpression Elimination | 45.7420 | 44.8817 | 43.4083 | 44.8448 |
| nested-2000-normal / jit_native_codegen | 95.9023 | 65.6158 | 63.7214 | 65.0558 |
| nested-2000-O2 / Machine code sinking | 24.7780 | 0.7732 | 0.8920 | 0.9475 |
| nested-2000-O2 / Machine Common Subexpression Elimination | 0.0810 | 0.0787 | 0.0827 | 0.0818 |
| nested-2000-O2 / jit_native_codegen | 48.7332 | 22.9579 | 24.1744 | 23.6955 |

Observer totals are not mixed into ordinary compilation medians. MachineCSE remains approximately flat; these results do not identify it as the regression source. No application runtime improvement is inferred from compiler timings.

## Machine correctness and emitted code

The flat graph has 8,003 blocks, 8,002 edges, 95,063 instructions and 23,017 virtual registers. Every variant uses two sweeps and 16,006 visits. Twenty ordinary flat pass runs, including SDK stock, produce identical verified MIR. The reverse 1,000-block standalone chain reproduces stock's 1,001 sweeps / 1,004,003 visits, original RPO's two / 2,006, and delayed RPO and the correction's four / 4,012. Forward chains preserve the ordinary convergence behavior. Count instrumentation is separate from timing binaries.

Sixteen MIR inputs produce identical output across all four variants: chain layouts/sizes, critical edges, PHIs, debug uses, subregisters, flags, irreducible and multiple-predecessor flow, unreachable cycles, volatile/retention and changed-operand controls. That is 64 verified pass outputs. Twenty selected upstream FileCheck runs pass. All four verifiers reject the deliberately wrong register class. Independently linked chain/irreducible/branch oracles pass 550 assertions per variant, 2,200 total, with 1 MiB stacks. The original G45-U fixtures and checks are preserved.

The representative suite passes **2,688 independent samples** across normal/O2 JIT and independently linked normal/optimized AOT. JSON requires the serialized bytes to equal the original input and checks independently expected lengths. Integer, floating-point, collections, dictionaries, sorting, Unicode/text and native I/O controls remain included.

| AOT mode | Variant | Object identical to rebuilt stock | Changed native symbols |
| --- | --- | --- | --- |
| normal | Original RPO | True | none |
| normal | Delayed RPO | True | none |
| normal | Forced-inline correction | True | none |
| optimized | Original RPO | False | /@stdlib/json.void::_write |
| optimized | Delayed RPO | True | none |
| optimized | Forced-inline correction | True | none |

Original RPO changes only the optimized AOT JSON writer, with a 19,978-byte body, a 0x22d8-byte local frame and unchanged 275,440-byte total text size. Delayed RPO and the correction retain byte-identical representative objects in both AOT modes. The original RPO serializer instruction changes remain visible in the retained comparison; JIT identity is never used to infer AOT identity. `aot-comparison/summary.json` records every affected symbol, code size and frame, with selected disassembly. The previous G45 JSON writer findings and G44 sort/text changes remain documented in their unchanged reports.

| Optimized AOT workload | Rebuilt stock ms | Original RPO ms | Delayed RPO ms | Forced-inline correction ms |
| --- | --- | --- | --- | --- |
| json-128 | 36.7448 | 36.8907 | 36.3079 | 36.6541 |
| json-32 | 5.9175 | 5.9511 | 5.7775 | 5.8560 |
| sort-1024 | 28.4380 | 27.3609 | 28.7512 | 27.6263 |
| sort-256 | 5.7020 | 5.5689 | 5.8532 | 5.4854 |
| text-1024 | 53.6273 | 53.0911 | 54.0169 | 53.5147 |
| text-256 | 13.4453 | 13.1934 | 13.3523 | 13.1541 |

These small runtime samples preserve favorable and unfavorable timings. Where code is identical, timing variation is not a generated-code gain. The complete representative timing table is archived; no claim is made that all workloads become faster.

## Limits, preservation and next action

This is a Linux x86-64 proof campaign, not new Windows, macOS or cross-target LLVM qualification. Broader LLVM regression suites and platform qualification remain outstanding. Existing Linux/fatal-sanitizer, TSan, Windows and forced-GC receipts continue to qualify unchanged accepted VOID/SDK source; they are not transferred to these experimental LLVM objects.

The VOID backend, validator, Runtime/public ABI, GC/ownership rules, G38 SIMD and accepted G33–G44 behavior retain their baseline source identities. No G37/G37-F import occurs. The original G45 report, G45-U report, RPO/delayed-RPO patches, separate held MachineCSE experiment and historical ledger entries are preserved. The only repository changes are the cumulative ledger and the preserved G45, G45-U and new G45-V reports. No files are staged or committed, and no push, history rewrite, hosted CI, upstream submission or SDK installation occurs.

**Next concrete action:** before another correction, obtain the exact SDK stock build recipe and any optimization profile, then demonstrate unchanged-source rebuild parity for `MachineSink.cpp` using the existing flat MIR. This is a proposed next investigation, not authorization to start another tuning campaign or maintain a VOID LLVM fork. G45 production remains on HOLD.

## Reproduction and archive

| MachineSink source | SHA-256 |
| --- | --- |
| stock | `139d455613a2ce46c46eeceaa45170eda0fdaf0da821ef43a66399b4b243c06e` |
| original RPO | `3c8e84013c6069eb8d618545c403c9bdcbb9788994b0799dd22e7dca2cedfbea` |
| delayed RPO | `505dda713bcffed238a8c7f635540edb851ed1779deb5613166d51de2e11b374` |
| correction | `cc0fa4041f5ded188457683f294676540172c05bd95150893102d228c25fbf79` |

`g45v-machinesinking-host-code-2026-11-16.zip` is a DEFLATE-9, source-restorable evidence archive. The adjacent verification receipt is authoritative for ZIP SHA-256, size, retained-entry count, CRC, every manifest hash and independent source-patch restoration. `REPLAY.md` documents rebuilding and the finite sampling sequence. Raw pass/counter/whole-source measurements, machine fixtures, independent oracles, host disassembly, selected native objects, failed configuration-path attempt and exact build identities are retained. Both input archives are reverified and remain untouched.

Cleanup occurs only after verification, for this task's generated evidence/build directories and SDK link. The separate cleanup receipt records allocated task bytes and observed host free-space change separately; no guest files are created or removed. Protected original worktrees, archives and SDK installations remain intact. The archive receipt and final Git inventory record the final documentation postimages.
