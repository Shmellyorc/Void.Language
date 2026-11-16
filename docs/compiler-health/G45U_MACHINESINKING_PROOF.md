# G45-U — MachineSinking proof and upstream readiness

**Disposition — TARGETED CORRECTION.** The standalone reproduction confirms the redundant-sweep mechanism, and selected x86 verifier and execution controls pass. The unconditional RPO patch still has a flat-function cost and changes representative optimized AOT code. G45 production remains on HOLD.

Accepted production baseline is 894e2e8d9930028f86737092d3ac74010676f389. G45-U runs on codex/g45u-machinesinking-proof. The installed LLVM/Clang/LLD 23.1.4 and all production compiler sources retain their accepted identities. This milestone preserves the G45 report verbatim, carries forward its ledger history, and retains both earlier LLVM experimental patches in the evidence.

The archive restores pinned LLVM driver sources and the MachineSinking patch, with rebuild commands linking separate stock, RPO and observer executables against the installed static libraries. The upstream source revision is 7a941dfb2877a0b8cf30323fefa85ad019e0b0f5. These measurements establish applicability to that revision. Applicability to other LLVM revisions or targets requires retesting.

## Independent reproduction and invariant

The generated MIR needs three virtual registers and a conditional chain. Reverse block layout makes downstream blocks execute earlier in each sweep. Stock sinking can move the definition only one such boundary per sweep. At 1,000 intermediate blocks, stock performs 1,001 sweeps and 1,004,003 block visits. RPO performs two sweeps and 2,006 visits. Both produce byte-identical verified MIR. The small chain with two intermediate blocks reproduces the ordering issue without the performance scale. Forward-order chains take two sweeps in both versions.

RPO visits an acyclic predecessor before its successor, so an instruction moved downstream can be reconsidered in the same sweep. The original movement legality checks and fixed-point termination still execute. Cycles may still require repeated sweeps. Critical-edge splitting happens after the traversal, and the next traversal recomputes order from the changed CFG. Unreachable machine blocks were already rejected by the original pass. This reasoning establishes progress on the chain and preserves the local safety gates. It does not establish identical optimization choices for every graph or target.

Six chain variants at sizes 2, 10 and 1,000, forward and reverse layout, produced identical outputs across stock, RPO and both counter instruments. The selected pinned upstream critical-edge, debug-value, Issue98477, subregister and EFLAGS FileCheck tests passed for both executables. PHI-use and custom irreducible and multiple-predecessor cases passed machine verification with identical pass output. An unreachable machine cycle passed. Deliberately mismatched register classes were rejected by both verifiers. Volatile memory, memory barriers, multiple successor uses and changed arithmetic/PHI operands retain stock MIR. Independently linked C oracles checked the 1,000-block chain and irreducible/branch functions for 110 integer inputs and both branch values.

These checks cover selected x86 cases. Cross-target regression suites, all debug configurations and arbitrary target hook behavior remain unqualified. The evidence contains a review draft and standalone MIR tests. The patch is held pending the remaining regression and coverage work.

## Flat regression and exact compilation

| Source | Mode | Stock seconds | RPO seconds | Change |
| --- | --- | ---: | ---: | ---: |
| flat-2000 | normal | 3.931 | 4.198 | +6.8% |
| mixed | O2 | 0.051 | 0.050 | -2.5% |
| mixed | normal | 0.042 | 0.039 | -7.6% |
| nested-2000 | O2 | 47.340 | 29.642 | -37.4% |
| nested-2000 | normal | 80.827 | 57.635 | -28.7% |
| small | O2 | 0.015 | 0.014 | -5.9% |
| small | normal | 0.015 | 0.015 | -2.1% |

Flat has four AB/BA pairs. Other source/mode combinations have two. Every source execution checked an independently expected result. All LLVM experiments use the accepted CPU and optimization policies.

The flat native graph has 8,003 blocks, 8,002 edges, 95,063 instructions and 23,017 virtual registers. Stock and RPO both take two sweeps and 16,006 visits. Standalone pass medians were approximately 1.43 → 1.47 s for MachineSinking and 27.5 → 27.8 ms for MachineCSE. This accounts for a small part of the whole-compiler regression. It establishes no MachineCSE work increase. In the embedded JIT, four matched profile pairs localize the increase to MachineSinking at 1.286 → 1.461 s. Native generation goes 3.531 → 3.677 s, while MachineCSE is 33.8 → 33.1 ms and lowering 193.1 → 190.7 ms. The compiled pass cost is established. Its low-level cause remains unresolved. Raw per-pass and embedded JIT samples are retained.

A bounded follow-up keeps the first two layout-order sweeps and switches to RPO only when propagation still needs another sweep. Its flat medians across four pairs are 3.842 → 4.131 s (+7.5%). The standalone chain and difficult-case MIR outputs match stock with machine verification. This follow-up has narrower qualification than the original RPO trial and remains held. Its measured regression persists even while the flat case retains the original two traversal sweeps. Extra DFS work alone therefore cannot explain the result. Generated host pass code, register allocation and instruction/cache effects require controlled investigation.


## AOT and retained limitations

The fresh representative suite passed 1,344 independent samples in normal/O2 JIT and independent normal/optimized AOT. Normal AOT object identity across variants is True. Optimized AOT object identity across variants is False. The fresh optimized AOT comparison must therefore accompany JIT results.

The full G45 report retains the JSON serializer instruction and relocation changes, 19,978-byte writer body, 0x22d8 stack frame, output oracles and previous G34–G44 tradeoffs. Passing JIT object comparisons does not establish AOT identity. The held MachineCSE experiment is separate from this trial and was not linked into G45-U.

## Reproduction, archive and next action

The source-restorable DEFLATE-9 archive contains source patches, pinned LLVM files, standalone MIR and generator, machine-verifier and FileCheck logs, runtime oracles, timing commands and raw results. It records the original G45 archive SHA-256 c7a998a08c6f906fbb7875ca1ecdb32d7746f07cc17402666752c77f58961708. ZIP CRC, every entry hash and restoration against the accepted VOID baseline are checked before cleanup. Original G45 worktrees and archives remain protected.

The smallest next action is to compare the generated host MachineSinking code for stock and delayed-RPO builds, measure the hot-loop instruction/cache behavior on the same flat MIR, and resolve its cost before broader qualification. Upstream submission readiness remains conditional on those results and broader LLVM target coverage. No patch has been submitted.

Broader ownership, checked collections, dictionaries/sorting, Unicode/text/JSON, SIMD, native/JIT costs, GC tails, startup and macOS/release work remain open in the permanent ledger.
