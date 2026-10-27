# Compiler stabilization and performance investigation

Current engineering is locally verified and ready for independent review:
[handoff](stabilization/HANDOFF.md), [measurements](stabilization/BENCHMARKS.md)
and [implementation evidence](stabilization/IMPLEMENTATION.md).
The baseline findings below retain their original research scope.

27 October 2026. Audited baseline: **`1d4ad15f5215838d209e48bb47ac7c3f52b05371`**.
Research for Golden Wrench review; no production repair is implemented or
accepted by this report. All new standard-library work remains on hold.

**The severe startup cases are predominantly compilation costs.** This audit
also found a separate exponential frontend traversal, confirmed nested `?`
inference and related hint defects, and measured position-dependent Runtime root
lookup cost. The unexplained historical crashes remain unexplained.

- [Issue inventory and historical reconciliation](ISSUES.md)
- [Reproducible measurements, limitations and evidence](BENCHMARKS.md)
- [Shared inference repair specification](INFERENCE_REPAIR.md)
- [Ordered engineering roadmap and acceptance](ROADMAP.md)

## Evidence and boundaries

The unchanged existing Release `voidc` was used for actual CLI measurements.
Scratch-only helpers called existing compiler APIs and dumped the unchanged
backend's IR; their backend translation unit was compiled at O1 and their core
and Runtime libraries came from the existing Release build. These are explicitly
separate measurements, not an instrumented production compiler. LLVM 19.1.7
`opt`/`llc` analyzed those exact dumps. A Runtime-only helper isolated registered
root width without LLVM or collection. Exact sources, commands, hashes, raw
streams, bounded results and original IR are retained in the evidence archive.

Measurements ran serially on this Linux x86_64 host, with actual 1 MiB child
stacks and ASLR enabled. No production files, Git state or hosted CI were
mutated. No ptrace, debugger, perf attachment or system policy change was used.
No sanitizer or full CTest acceptance run is claimed. This is a broad audit of
the current execution pipeline, not proof that every workload/platform is fast
or that a cost became worse in a particular historical commit. Historical and
current wide DTO fixtures are not identical controlled before/after programs.

## Ranked findings

| Priority | Established finding | Smallest appropriate next action |
|---|---|---|
| 1 | Generic hints incorrectly preserve wrappers/conditions; primitive arithmetic hints can repeat exponentially before LLVM is involved. | Repair the shared hint boundary and its duplicated traversal; reuse ordinary checking/lowering. |
| 2 | Generated JSON decoder control flow creates thousands of function-duration roots and enormous blocks. LLVM IR optimization and especially machine scheduling dominate wide cold compiles. | Factor duplicated decoder field-admission/fetch logic into shared ordinary generated control flow. Keep existing GC/IR/LLVM architecture. |
| 3 | Merely importing an unused source module causes substantial compilation work. | After the decoder correction, evaluate a CLI-entry emission reachability plan that preserves full source validation and existing export/reload contracts. |
| 4 | Root membership validation repeatedly scans root slots; large functions pay at Runtime as well as compile time. | Reuse the existing root-lookup measurement scaffold. Only pursue checked lookup/liveness changes after the smaller generator experiment is measured. |
| 5 | Three typed-IR validations occur on a normal cold source JIT path; O2 object caching is intentionally absent. | Treat as secondary costs. Do not remove validators or silently change optimization/cache policy. |
| Reliability | Multiple historical LLVM/compiler/runtime incidents lack causal closure. | Preserve distinct identities and first-failure evidence. No speculative fix or endless passing retries. |

### 1. Frontend: correctness and exponential arithmetic work

`anchor` is a non-evaluating hint used by generic inference. Its fallback
returns the first child hint ([compiler.cpp:2147–2220](../../src/frontend/compiler.cpp)).
That is wrong for propagation, await and conditional expressions. Fresh
independent non-JSON fixtures reproduce wrapper-versus-payload, task-versus-result
and condition-bool-versus-branch mismatches. Explicit generic arguments or
extracting a local can pass, confirming that the corresponding ordinary checks
and lowering already support the intended expressions.

For primitive arithmetic, `operator_anchor` first visits child hints but returns
none when no user operator is selected. `anchor` then revisits the same children.
A left chain has approximately `T(n)=2T(n-1)+O(1)` hint work. A 20-addition generic
argument took **1.514 s** in `voidc check`; the balanced equivalent took
**0.00687 s** and 20 extracted `+=` statements **0.00860 s**. A nongeneric left
chain also took **0.539 s**. This is a demonstrated frontend defect, independent
of LLVM optimization, JIT, JSON or GC. It is distinct from the previously repaired
Debug anchor **stack** regression.

The minimal correction computes arithmetic child hints once and shares them
between exact custom-operator selection and the existing builtin fallback.
Wrapper projections must match the real checker, including reference authority.
No persistent AST hint cache or second expression checker is needed. The
[repair specification](INFERENCE_REPAIR.md) covers async-call/await interactions
that would otherwise make a one-case patch regress an already passing case.

### 2. DTO compilation: generated code meets backend root expansion

The 8–128-field ladder uses the same managed i32 DTO shape at every width,
`json.parse_as`, and stringify equality. Plain managed-struct construction
controls exclude JSON while retaining width. The CLI ladder, phase dumps and
machine-pass receipts are in [BENCHMARKS](BENCHMARKS.md).

At 128 fields, the dump has **301,776 pre-O2 LLVM instructions**. The generated
decoder has **9,603 root slots** and about **413 KiB of summed static allocas**.
After O2, the largest LLVM block contains **101,915 instructions**. These alloca
totals are IR inventory, not a measured final native stack frame or peak live
heap. The wide constructor is already outlined through a closure at 32 fields
([json.inc:178–188](../../src/frontend/json.inc)); the decoder remains the worst
root-frame function. Adding that closure did not eliminate the dominant shape.

The unchanged CLI takes **136.479 s with `--no-opt`** and **309.755 s optimized**
at width 128, while the plain wide-struct control takes **0.058 s** optimized.
All these bounded executions succeeded, including roundtrip assertions; that
does not make the compilation latency acceptable. Peak CLI RSS is about 1.82 GiB
in the optimized width128 run.

In this coordinated synchronous decoder, every reference-containing typed
instruction receives a root home initialized in the entry path; every entry
also gets a slot descriptor and a function-duration
registration ([backend.cpp:708–727](../../src/backend/llvm/backend.cpp)). JSON
duplicates reference-bearing `_same`/`_find` result handling, errors, path updates
and descent for each field ([json.inc:159–161,196–201](../../src/frontend/json.inc)).
That produces a very large memory-heavy prologue even though source execution
visits one field branch at a time. This mechanism is source-supported and the
IR shape is directly measured; a compacted-generator comparison is still needed
to prove the repair's magnitude.

For this 128-field dump, source analysis took **0.264 s**, typed lowering including
its validation **2.994 s**, and LLVM lowering including another validation
**3.147 s**. LLVM verify/O2/verify took **106.593 s**. Running pinned `llc -O2`
on the resulting IR took another **195.309 s**, with about **1.71 GiB peak RSS**.
In that codegen pass report, post-RA scheduling used **69.8%** and machine
instruction scheduling **16.9%** of pass wall time. Register allocation is not
the dominant measured cost. A separate 32-field `opt` run attributes most IR
optimization time to InstCombine, MemCpyOpt and DSE; DSE time here does **not**
explain the historical DSE SIGSEGV.

Pinned LLVM source explicitly recognizes compile-time tradeoffs when building
large scheduling DAGs ([19.1.7 ScheduleDAGInstrs.cpp, lines 65–77](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-19.1.7/llvm/lib/CodeGen/ScheduleDAGInstrs.cpp)).
That supports the mechanism, not a claim that LLVM is defective. Do not start by
changing hidden global scheduler settings or dropping passes for the whole
language. Reduce the pathological input shape first.

#### Recommended first decoder correction

Generate one ordered unknown-field comparison body and one field-fetch/path/
descent body. Use lightweight per-field name/tag selection to feed that shared
body. Non-null descent already differs only by its existing integer work tag;
it needs no new typed dispatcher. Missing nullable fields still need the correct
typed result stack, grouped by **concrete nullable type identity**, not scalar
kind. Keep the current wide construction closure initially.

Preserve observable work accounting and first-error order:

- Unknown-field admission currently compares **every declared name**, even
  after a match. `_same` charges name bytes and comparison work
  ([json.void:1012–1016](../../stdlib/json.void)); early exit changes failures.
- Admit unknown fields in input order before required-field fetching. Fetch
  fields in declaration/effective inherited-field order. Keep `_find` before
  the path push, with the same first-match scan and offsets.
- Preserve missing versus explicit null, nullable-node charges, cursor/awaiting
  transitions, path pops and post-child charges, and exact spelling/escaping.
- Ordinary VOID `const` arrays are Runtime values, not free static LLVM tables
  ([compiler.cpp:3938–3948](../../src/frontend/compiler.cpp)). Do not add global
  metadata initialization or new allocation-before-error behavior by accident.
- Encoder outlining is a later measured option: readonly child authority, fixed
  value copies, helper return paths and O2 reinlining need independent attention.
  The generator currently expects one parsed function at `json.inc:209`.

This is a correction to existing compiler-generated adapters, not new JSON
features, reflection, a native parser, a new Runtime provider or a GC ABI rewrite.

### 3. `--no-opt` still selects optimizing machine-code generation

`--no-opt` skips the explicit LLVM IR O2 pipeline. It does **not** request
`CodeGenOptLevel::None`: JIT uses `detectHost()` without overriding its default
([backend.cpp:1787–1789](../../src/backend/llvm/backend.cpp)); native emission
also omits that override at lines 94–100. Installed pinned SDK headers show
`JITTargetMachineBuilder.h:153` and `TargetRegistry.h:459` default to
`CodeGenOptLevel::Default`; the latter is also visible in the
[19.1.7 source](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-19.1.7/llvm/include/llvm/MC/TargetRegistry.h).
The IR pipeline and backend codegen are separate stages, consistent with
[LLVM 19 documentation](https://releases.llvm.org/19.1.0/docs/NewPassManager.html).

On the **same pre-O2** DTO128 IR and host target, external `llc -O0` took
**5.862 s**, while `llc -O2` took **123.463 s**. The comparison changes a collection
of backend passes; it does not assign the whole difference to one scheduler.
It nevertheless proves that costly codegen persists without the IR O2 pipeline.

A genuine low-codegen mode is a small possible follow-up, requiring explicit
Boss approval for flag behavior, runtime-speed tradeoffs, JIT/AOT agreement and
cache target identity. It is **not** a fix for default optimized cold compilation.
Do not silently downgrade default optimization or apply global hidden flags.

### 4. Unused imports pay for unrelated emitted functions

The tiny fixture prints `7`. Adding only an unused `use json as json` raises O2
fresh-run wall time from about **0.0225 s** to **10.830 s**; unused `text` takes
**1.701 s**. The JSON-import dump has 64 defined functions / 73,505 pre-O2 LLVM
instructions, versus 2 / 190 for tiny. This is measured aggregate import,
analysis, lowering, optimization and materialization cost, not an isolated ORC
or native linker measurement.

The frontend lowers ordinary functions across the flattened import graph
([compiler.cpp:5774–5805](../../src/frontend/compiler.cpp)); backend source-public
functions retain external linkage ([backend.cpp:320–330](../../src/backend/llvm/backend.cpp)).
Entry lookup can trigger materialization of that module. The external `llc`
experiments corroborate machine-pass cost; they are not proof of identical
ORC/AOT pipeline behavior. Merely making every
function private would break exported/raw-IR embedding contracts. A future
CLI-entry-only emission plan must preserve existing analysis/diagnostics
(including intentional deferred generic-template checking), globals
and their initializers, method/interface dispatch, callbacks/closures, metadata
and reload generation lifetime. Existing embedders must retain their exports.
This is a second focused experiment after decoder factoring, not an excuse to
skip source checking or introduce a second compiler.

### 5. Runtime roots and safepoints

`require_output` walks frames, slots and offsets
([coordination.cpp:196–203](../../src/runtime/coordination.cpp)). `root_word`
and `reference_source` repeat registration checks; even a reused valid token
performs slot lookup ([buffer.cpp:201–216](../../src/runtime/buffer.cpp)).
The isolated 5,000-query-per-row sweep confirms this cost: last-slot queries
grow from about **254 ns at one root** to **82,879 ns at 16,384 roots**, while
first-slot controls stay near 200–300 ns. The checksum is constant and no GC or
LLVM is involved. This is synthetic root-width evidence, not a measured
percentage of full JSON runtime.

The same function-duration design also expands entry validation
(`coordination.cpp:364–375`) and collection scans (`165–184`). Backend poll and
cancellation calls occur at typed block entries (`backend.cpp:756–765`);
`Runtime::poll` takes the coordination mutex (`coordination.cpp:134–137`).
Collections additionally validate descriptors/backing storage and lock per
operation. These are meaningful costs with correctness purposes. No check may
be removed on this evidence alone. Root liveness/slot reuse or validated lookup
indexes require tests for frame expiry/address reuse, foreign threads/Runtime,
precise GC and native/async cleanup. Reuse `tests/runtime/root_lookup_measure.cpp`
rather than create a new performance framework.

### 6. Other consequential costs and limits

| Area | Finding / evidence | Treatment |
|---|---|---|
| Repeated validation | Source lowering validates; cold JIT validates its input; LLVM lowering validates its transformed copy. One DTO128 standalone validation costs 2.961 s. | Secondary to 100–200 s backend costs. Validation protects distinct mutable/raw-IR boundaries. Reuse proof only if immutable ownership and transformations make that proof valid; no removed validation gate. |
| Cache | `--cache` explicitly sets `optimize=false` (`main.cpp:42`); optimized API compiles bypass it (`backend.cpp:1947`). A warm DTO8 cache run takes about 0.127 s instead of 14.6 s uncached O0. | Useful existing O0 path, not optimized caching or a cold-compile repair. Source analysis, identity hashing, object admission and Runtime execution remain. Windows persistence deliberately falls back safely. No stale-result/cache correctness defect established here. |
| Runtime/init/teardown | API helper separates first/second invocation from compile. DTO8 first invoke is ~6.6 ms and teardown is milliseconds, versus ~29 s compile. | On these fixtures, execution/shutdown do not explain startup seconds. No conclusion about long-running games or async shutdown incident H2. |
| Async retention/scans | `async.cpp` retains Task/result/Work records until root drain; several task/receive/pending paths linearly scan retained vectors. | Source-supported concern for long-lived task roots, unmeasured here. Preserve the Stage 8 retention contract. Do not redesign scheduler without a bounded workload showing this is consequential. |
| Text/collection throughput | Existing text evidence shows slow multi-operation collection/string workloads; Runtime root lookup is now independently measured. | Attribute collection/copy/lookup/locking time before changing immutable strings, safety checks or GC. Historical text timings had host contention. |
| Editor | `voidls` reanalyzes full graphs; synchronous flushes can expose frontend latency. Save/watch/refresh already exist. | Shared hint repairs benefit tooling. Incremental analysis, remote clients and another semantic engine are not this sprint. No new editor latency benchmark claimed. |
| Building VOID itself | SDK archive ThinLTO and cache/concurrency/link incidents are documented separately. | Scratch helper linking was expensive, but this audit does not establish a new end-user linker regression or replace the prior CI investigation. Do not conflate building voidc with running voidc. |

## Reliability and remaining uncertainty

[ISSUES](ISSUES.md) separates the LLVM DSE, async IO, Stage 9, pinned Clang,
LLD and ThinLTO observations. None is closed by passing retries or by the new
performance findings. No abnormal exit occurred in this audit's bounded probes.
Historical exact IR/backtraces remain linked; missing original IO source/binary
identity is still missing, despite strong reconstructed-generator evidence.

No evidence here proves LLVM UB, a GC use-after-free, incorrect ABI, a cache
poisoning defect, cross-platform timing equivalence or a particular historical
regression commit. LLVM lowering copies/validation and many Runtime checks cost
time, but their removal is not an authorized shortcut. Optimization of generated
shape plus the shared frontend repair is the supported first engineering route.

The [roadmap](ROADMAP.md) proposes measured sequential repairs, exact semantic
gates and a small set of owner decisions. Production work stops for Golden
Wrench architecture review and Boss authorization.
