# Bounded LLVM DSE input investigation

28 October 2026. Baseline `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.

**Disposition D: original crash cause remains unresolved. STOP for Golden Wrench
review. Performance OPEN; release HOLD.** One private diagnostic executable
captured a complete pre-optimization module and actual pass execution. Its source
capture and two serialized replays passed. No unchanged-original-command launch,
production/test repair, optimizer workaround or logging implementation occurred.

**Preserved incident.** The original executable remains SHA-256
`c284712dd4958faf41547deb49a851306256a9075cf59d968671272bc17e96bb`.
All 61 original source entries match their live files and preserved copies.
`incident-vs4b96zy`, `incident-tg6xt8tp`, the native capture
`incident-w415alhc`, and subsequent resident-text results remain preserved.
The original core remains SHA-256
`2c313fd4d8fba185efdceb35f519a5f6d8bc3d80e47e37b3ed367569405aad24`.
The original full environment was not retained; perfect environmental identity
cannot be claimed. [CRASH_CLASSIFICATION.md](CRASH_CLASSIFICATION.md) and
[RESIDENT_TEXT_CAPTURE.md](RESIDENT_TEXT_CAPTURE.md) retain the prior findings.
[HOTPATH_REVIEW.md](HOTPATH_REVIEW.md) and the previous handoff were read before work.

Private evidence root for this investigation, called `D` below:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/dse-input-investigation`.
It is outside Git with mode 0700. Raw IR, binaries, environments, pass traces and
large comparisons stay there. Approximately 530 GB was available before execution.
`D/EVIDENCE_INDEX.md` is the compact handoff; `D/command-index.json` records exact
argv, cwd, statuses and receipt locations, including preparation/tool failures.
Per-command environment files are private and excluded from the review bundle.

**Actual compiler configuration.** Inspection of
`src/backend/llvm/backend.cpp:76` and `:1940` establishes this sequence:

1. `Jit::compile` coordinates the input with its Runtime owner, validates VOID IR,
   checks entry/contract compatibility and lowers into a fresh `LLVMContext`.
2. It sets the module data layout and triple from LLJIT, then calls `finish`.
3. `finish` calls `verifyModule` before optimization. It creates fresh loop,
   function, CGSCC and module analysis managers, registers the standard analyses
   in module/CGSCC/function/loop order and cross-registers their proxies.
4. The production `PassBuilder` uses its default arguments: **null target machine**,
   default pipeline tuning, no PGO option and no instrumentation callbacks.
   It directly builds `buildPerModuleDefaultPipeline(O2)`, runs it, then verifies
   the resulting module again. There is no explicit DSE insertion or AA/TLI override
   in this function. The default function-simplification pipeline inserts DSE
   after MemCpyOpt and before MoveAutoInit.
5. Only after `finish` succeeds does `impl_->load` add the module and perform the
   lookup that triggers emission/linkage. The benchmark has not run at the fault
   location in the preserved core.

`Jit::Jit` separately detects the host target/CPU/features and sets default codegen
optimization for LLJIT. That target machine is **not passed to `finish`**. The
captured module triple is `x86_64-unknown-linux-gnu`; its data layout is
`e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128`.
The captured tuning options include loop vectorization/interleaving/unrolling on,
**SLP vectorization off**, eager analysis invalidation on, LICM caps 100/250,
call-graph profiling on, unified LTO/function merging off and inliner threshold -1.
All options are recorded in each capture's `module.txt`.

**Historical comparison and recovery limit.** The logging core identifies a
general-protection fault inside statically linked LLVM 19.1.7
`eliminateDeadStores`, through DSE/function/CGSCC/inliner/module pass managers and
`vd::finish`. Its RIP was `0x5565df5f470e`, normalized ELF address `0x215070e`;
signal 11, `si_code=128`, kernel trap error 0. Resident executable text was omitted
from that core. The on-disk `shr $0x4,%eax` decode is not proof of the resident
faulting operation. Stack evidence did not support exhaustion.

The [Stage 11 observation](../stage11/OPTIMIZER_FAILURE.md) instead stopped in
`CallBase::getMemoryEffects`, called from DSE in a Debug console test. Both saved
backtraces contain the LLVM clone suffix `.llvm.6014830315340998214` and the same
general optimizer/adaptor path. Their inputs, host builds and fault locations
differ. The retained Stage 11 backtrace lacks the logging core's signal/trap and
register evidence. Neither observation supplies a common offending IR operation
or broken invariant. A shared cause is unproven.

The evidence search found no saved logging `.ll`, `.ll.gz` or `.bc` input before
this investigation. A mid-optimization core is not a recoverable serialized
pre-optimization snapshot: passes have already mutated the module, and the
available symbols do not reconstruct its former state. The API recipe is
recoverable from matched backend source and pinned LLVM, but an actual pass trace
from the crashing original process is not. Stage 11 does retain a complete module:
`docs/stage11/evidence/console-preopt.ll.gz`, decompressed SHA-256
`402019ad2887f892d65f8effa2aa516342d02013d19e54835187d4b0d2fe59e0`.

**One diagnostic variant.** Private copies of the backend and preserved benchmark
host add complete `.ll`/`.bc` snapshots before verification/O2, module identity,
configuration, printed pipeline and before/after pass events. DSE events identify
the current function. Diagnostic writes use private files and direct writes for
pass events, without the logging library. Original verification remains enabled.
The host returns after compilation/linking; it never invokes benchmark code.
A replay entry parses an existing module into a fresh context, restores its module
identifier, and calls the same instrumented `finish` body and analysis setup.

The build retains the original timing-overlay objects and pinned LLVM libraries;
all link inputs and exact build commands are hashed/recorded. The sole resulting
diagnostic executable has SHA-256
`61f1f39a187c71365f71647d1c2c97aad0d049ac6c749ddb0f53121a2479f238`.
Its source changes are preserved as `D/sources/backend-final.diff` and `host.diff`.
This relinked, instrumented executable is not identical to the original. Snapshot
serialization, callbacks, changed code/heap layout and replay's omitted frontend/
Runtime setup perturb process state; passing it cannot clear the original failure.

The source capture retains the original source path, launch cwd
`/home/shmellyorc/Documents/Void.Language`, execution cwd spelling
`/tmp/VOID-hotpath-combined-kdkc1i_d`, `optimized`/`timing` arguments and explicit
`VOID_STDLIB_ROOT`, shape and stage filters. The executable and diagnostic-output/
recorder environment differ explicitly. Replays use `D/work` and serialized inputs.
All three actual process snapshots confirm soft **and** hard stack limits of
1,048,576 bytes and process-local core filter `0x37`; core limits were unlimited.
No system-wide settings, tracing, debugger attachment or library injection were used.

| Bounded operation | Status | Observation |
|---|---:|---|
| One diagnostic source capture | 0 | Pre/post verifier passed; 458 DSE entries and 458 exits; compile/link completed |
| One logging bitcode replay through the same `finish` | 0 | Pre/post verifier passed; 458 DSE entries and exits |
| One Stage 11 IR replay through the same `finish` | 0 | Pre/post verifier passed; 35 DSE entries and exits |
| Two pinned `opt` pipeline inspections | 0 each | Printed pipelines only; no optimizer execution |
| Pinned `llvm-diff` on the two logging outputs | 1 | Second-file use-list parse error; comparison did not complete |
| One independent pinned `llvm-as` check of that second file | 0 | Parsed and verified successfully; no optimization |

No new SIGSEGV or core occurred. There were **three optimization executions total**,
all in the single variant, and zero unchanged-original launches. The initial plan
allowed at most two ordinary `opt` O2 controls; both were left unused after pipeline
inspection established non-equivalence. No further optimization followed the
comparison-tool error. No retries, additional executable variants or reductions
were run.

The logging preopt text contains 18,835,546 bytes, module ID `/performance.void`,
SHA-256 `9eca9f54e226da000d0d58765c3534201b61354d51c6a66ef820ed125f79e723`.
Its 4,932,540-byte bitcode SHA-256 is
`1c1b046641d143e7c38e2b4168fdab1de028459a3250e05bff2eb5985fbde272`.
The replay reprints **byte-identical preopt text**, including use-list directives.
Its reserialized bitcode bytes differ, so bitcode identity is not asserted.
The pipeline is identical across all three variant executions, SHA-256
`5bf369970c2e66d146f9a0fdeddc6a80ffea4cb8c54e9ead1f5083ff7eeff564`.
`capture-summary.json` contains every capture file's size/hash and event totals.

**Pipeline equivalence matters.** The pinned tools report LLVM 19.1.7, optimized
Release build, assertions off. Ordinary `opt -passes=default<O2>` creates a target
machine and supplies target-aware settings to `PassBuilder`; it also supplies its
own TLI/AA registration. Parsing `default<O2>` turns SLP vectorization on, whereas
VOIDC directly calls the builder with its recorded defaults. The printed `opt`
pipeline adds `slp-vectorizer` and a final verifier pass. VOIDC already verifies
outside the pipeline. The SLP addition is later than the observed DSE site and
does not explain its crash. Matching a pipeline string alone would still not
equate target analyses or memory state. These details are supported by the pinned
headers, captured pipeline comparison and LLVM 19.1.7
[PassBuilder source](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Passes/PassBuilder.cpp),
[opt driver](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/tools/opt/optdriver.cpp)
and [new pass-manager driver](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/tools/opt/NewPMDriver.cpp).
Hashed copies are in `D/references`. The same-`finish` replays match the optimizer
recipe more closely, while still losing the original in-process state.

**Output and tool reconciliation.** Logging optimized-text hashes differ. A saved
comparison finds identical text after consistently renaming local SSA values and
block labels and omitting use-list-order directives. This is an explanatory text
comparison, not a semantic or runtime proof. The first `llvm-diff` attempt failed
to parse the second file's `uselistorder i8 0`: it expected 7,052 entries, while
each independent module has 3,526. Its
[pinned driver](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/tools/llvm-diff/llvm-diff.cpp)
loads both modules into one LLVM context, consistent with the combined use count.
The second file parses and verifies alone with pinned `llvm-as`. Thus the tool
failure is not evidence of an invalid standalone module or the original DSE cause.
Original outputs, raw/normalized comparisons and the failed receipt remain intact.

Two preparation errors also remain explicit: the initializer passed `env=None` to
the existing recorder and failed before starting any child; the first compilation
of the diagnostic source used an unavailable `any_isa` API and exited 1 without
producing an executable. A pinned-header-compatible nullable `any_cast` corrected
that diagnostic-only build error. These are preserved as `incident-c68rljxg` and
`incident-lsb9715_`; the comparator error is `incident-qs8mz8w3`. None is a native
crash. `build-preparation-correction.json` also clarifies that compiler/linker
builds inherited their normal stack/core limits; generic execution-context fields
in those build receipts do not describe build-process limits. The three target
executions have their own actual `/proc/self/limits` captures.

**Smallest defensible conclusion.** The established faulting component remains
LLVM DSE during VOIDC O2 compilation. No invalid preopt IR was found in the new
capture or retained Stage 11 module. This does not establish the integrity of the
original crashing module, its analysis caches, host memory or resident instructions.
The passing same-recipe replays do not distinguish damaged in-process compiler state
from an intermittent LLVM transformation defect or another earlier cause. The
separate comparator limitation supplies no causal link to the SIGSEGV.

Released VOIDC correctness/reliability **cannot be excluded from impact**: the
captured stack traverses the shared production optimizer path. Harness-only
classification A, a specific shared correctness defect B and a specific LLVM/ABI
defect C remain unproven. No production correction or defect-specific regression
test is justified yet. Review now has a complete regenerated module, an exact
recorded pipeline and bounded replay observations, but not a crash-linked bad IR
operation or broken invariant. Further investigation requires a separately
reviewed scope. Logging implementation remains stopped.

`D/preservation-verification.json` records the final checks of original inputs,
executable/core, frozen repository files/executables, historical evidence indexes
and pre-investigation evidence. The root README and production/test sources remain
unchanged; no Git staging, commits, branches, pushes, hosted CI or package work.
