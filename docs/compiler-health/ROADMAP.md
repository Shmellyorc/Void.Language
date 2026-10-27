# Compiler stabilization: ordered engineering roadmap

**STATUS: COMPILER STABILIZATION COMPLETED / STANDARD-LIBRARY DEVELOPMENT HOLD LIFTED BY BOSS.**

The [await syntax handoff](../async-await-syntax/HANDOFF.md) records completion
of Boss's local gate and authorization to resume the normal approval workflow.
This does not authorize unspecified modules or claim hosted acceptance or repair
of historical H1–H7 incidents.

Boss authorized the complete stabilization scope on 27 October 2026, including
automatic optimized caching and further measured cold-compile repair. The
[active engineering record](stabilization/IMPLEMENTATION.md) tracks execution.
The [completed local implementation and verification](stabilization/HANDOFF.md)
are ready for independent review. Boss has since lifted the special hold after
the locally verified syntax delivery linked above.
The proposal below remains design history; its 2× experiment target is not
final acceptance. The owner objective is seconds, aiming below 10 s for DTO128.


## Historical research freeze — superseded by Boss decision above

Baseline **`1d4ad15f5215838d209e48bb47ac7c3f52b05371`**, 27 October 2026.
Proposal for independent Golden Wrench architecture review and Boss approval.
**This mission implemented no production repair.** New standard-library work
remains on hold. [Findings](REPORT.md), [inventory](ISSUES.md),
[measurements](BENCHMARKS.md), [inference details](INFERENCE_REPAIR.md).

## One sequential repair effort

| Order | Bounded work | Dependencies / evidence required before proceeding |
|---|---|---|
| 1 | Fix shared propagation/async/await/choice hints and remove duplicate arithmetic hint traversal. | Existing ordinary checker/lowering stays authoritative. Establish independent non-JSON regressions first. |
| 2 | Compact existing generated JSON decoder admission and per-field fetch/descent logic. | Keep exact ordered metering/error behavior. Keep construction closure, Runtime ABI and backend optimization policy unchanged for a controlled comparison. |
| 3 | Measure the two repairs and accept or reject the generator experiment. | Reuse identical fixtures, compiler profiles, target identity and preserved receipts. Structural/phase improvements must accompany total wall-time improvement. |
| 4 | Address demonstrated unused-import emission cost with a compiler-owned CLI entry emission plan. | Full imported source checking stays in place. Retain all reachable initialization/dispatch/cleanup/export edges. Preserve default embedding behavior. |
| 5 | Review residual root/Runtime/backend costs and historical incident evidence. | Only choose a broader root-lookup/liveness correction if the remaining profile justifies it. Do not start a scheduler or GC redesign merely because the first repair is incomplete. |

Each step has a reviewable diff and focused local evidence. No separate
benchmark service, telemetry subsystem, broad validation framework, new parser
or new stdlib module is needed. Keep frontend, typed IR, LLVM 19.1.7, precise
GC, source/VFS identity and native publication architecture.

## Step 1: shared frontend correctness and traversal

Follow [INFERENCE_REPAIR](INFERENCE_REPAIR.md). Add type-only wrapper projections
and correct async-call result hints coherently; do not evaluate expressions in
inference. Choice inference must cover literal fallback and branch scopes.
Compute original arithmetic operand hints once, preserving user-operator
resolution and contextual literals. Avoid persistent memoization.

Acceptance:

- Existing nested `?` reduction and independent direct/nested collection/result
  positives pass; explicit controls still pass. Invalid `?`, error-type mismatch,
  deferred usage, readonly mutation and ambiguity remain source errors.
- Side-effect/Err/cleanup tests establish exactly-once, source order, skipped
  later arguments and one cleanup in JIT/O2/linked AOT.
- Existing-task await, immediate async-call await, direct async call argument,
  qualified/method calls and choice branches all infer their actual types.
- Compiler-owned LSP uses those concrete types and retains diagnostic spans.
- Real 1 MiB stack tests pass. The preserved arithmetic series loses its
  exponential shape; proposed local 20-operation gate: <50 ms and <5× the
  contemporaneous balanced control. Use manual receipts, not wall-time CI asserts.

## Step 2: compact the decoder without changing JSON semantics

Modify the existing generator in `src/frontend/json.inc`, using the same source
parser/type checker/lowerer and existing work tags. Prefer lightweight per-field
name/tag selection feeding one heavy body over newly allocated metadata tables.

- Unknown-property admission: one ordered `_same` body, including comparisons
  after a name has matched. Preserve original input-order first failure and
  every work charge.
- Field traversal: one `_find`/path/descent body, preserving declared effective
  field order, first-match scan, offsets and `_find`-before-path ordering.
- Non-null descent uses the selected existing integer work tag. Missing-nullable
  cases dispatch by distinct concrete nullable tag to their typed result stack.
- Preserve missing/null differences, node charges, cursor/awaiting transitions,
  cleanup and post-child charges. Do not alter the wide construction closure
  initially or make const source arrays into an assumed static-table facility.

Correctness acceptance:

| Area | Focused required coverage |
|---|---|
| Schema | Width 8/16/32/64/128 repeated fields; mixed/repeated types, nullable fields, inheritance, enum/generic identities and recursive graphs. |
| Output | Exact parse/stringify values; existing deterministic order and numeric/text behavior unchanged. |
| Errors | Both unknown policies; missing vs explicit null; competing errors; original kind/path/offset; tight work limits that distinguish every comparison and transition. |
| Ownership | Readonly managed children, fixed-value copies, fixed/growable arrays, captured storage and allocation/failure cleanup. |
| Execution | Source checking, normal JIT, O2 JIT and independently linked AOT, real 1 MiB stack. Reuse current JSON authority/diagnostic and shared Runtime gates. |

Do not optimize observable ordered quadratic comparisons away without a
separate language/library contract decision. This repair reduces duplicated
compiled code while preserving the currently budgeted execution algorithm.

## Step 3: measured performance gate

Freeze inputs and build identities before measurements. Start with widths
8/32/128 plus one mixed-type fixture. Use one expensive 128-field candidate run
until structural improvement is established, then repeat qualifying candidates.
Use the complete 8/16/32/64/128 ladder for final comparison. Do not repeat full
hosted CI between local steps.

Proposed **experiment acceptance**, on the same host/profile/policy:

1. Repeated-type DTO128 decoder has at least **50% fewer root slots** and a
   **50% smaller maximum LLVM block** than the preserved baseline. Report pre
   and post O2; ensure O2 did not recreate the large shape.
2. DTO128 end-to-end cold O0 and O2 compilation each improve at least **2×**;
   show wall/CPU/RSS and separately IR optimization versus machine scheduling.
   Explain any fixed unused-import floor at smaller widths. No timeout increase
   or optimized-cache substitution counts as a cold-compile improvement.
3. Tiny/gameplay source-check and run medians, and warm-cache/native execution,
   have no material regression: investigate >10% **and** >5 ms compile change
   on tiny cases; for longer cases compare >10% with enough repeats to separate
   host noise. Preserve runtime outputs. Small native timings require more than
   a single few-millisecond sample before claiming a regression.
4. All validation, 1 MiB stack, authority and failure-preservation gates remain.

These are review targets, not predicted results or a claim that a remaining
multi-second compile is satisfactory. If the change misses them, preserve its
receipts and inspect the residual phase/IR shape before expanding scope. If it
passes but developer latency is still poor, continue to the import-emission
step; do not declare the entire performance problem solved by a 2× result.

## Step 4: unused-import emission cost

The preserved unused `json` and `text` imports independently demonstrate general
compiler work unrelated to DTO width. Design entry reachability in the existing
compiler/lowering path, with a conservative keep set. Full source graph checking
and source diagnostics must occur before pruning emitted bodies. Reuse current
symbol/typed-IR metadata; no editor-specific logic or second module system.

Must retain:

- Entry/application adapters, file globals and all initialization dependencies.
- Direct calls, function values/closures, bound methods, constructors/deinit,
  cleanup and effect-relevant helper functions.
- Interface/virtual dispatch targets, witness/type metadata, generated default,
  async/copy/JSON helpers and native binding edges.
- Required public/exported entry points for embedders and raw-IR clients, plus
  generation/reload lifetime contracts. A CLI policy must not silently change
  existing `Jit::compile` or native library export semantics.

Acceptance: unused-import emitted function/instruction counts decrease
substantially and its cold run approaches its measured full-analysis floor;
compiler diagnostics for invalid unused ordinary/concrete imported bodies still
appear. Preserve existing deferred checking of unused generic-template bodies;
this is not a new generic validation policy. Validate
global exactly-once ordering/errors, indirect dispatch/closure callbacks,
deinitializers, native binding, hot replacement and existing exported raw-IR
linking. Keep ordinary used-import and DTO outputs identical. Report achieved
latency instead of promising that reachability makes import analysis free.

## Step 5: residual costs and reliability

Measure the existing root lookup scaffold with first/last slots and fresh/reused
tokens after generator factoring. If lookup remains significant in actual
execution, consider one checked registration/index improvement first. Tokens
must reject expired frames, reused addresses, changed slots, foreign Runtime
and foreign threads. No unchecked pointer shortcut or reduced GC visibility.

Root liveness/slot reuse has a wider proof surface: aliases, loops/joins,
captured and mutable roots, safepoints, return/failure paths, async suspend,
cancellation and native calls. It can benefit the whole language but is not
required to begin the narrower decoder repair. Safepoint batching, task record
retirement and per-element collection validation changes each require a new
bounded workload and source proof before engineering. Do not bundle all of them.

Historical H1–H7 remain in [ISSUES](ISSUES.md). Reuse the current incident system
and exact `_crash.void` preservation. On a new abnormal exit, stop that workload,
retain source/dependencies, stdout/stderr/status, binary/tool/input hashes,
command/stack/ASLR and phase/IR if available. Do not erase it after a passing
retry. Boss requires stopping **before ptrace** to authorize access; no debugger
permission is needed for ordinary bounded measurements.

## Local validation and platform limits

Run affected frontend, JSON, GC/cancellation, cache and tooling selections on
Linux first, in Debug, Release and strict-sanitizer configurations as relevant
to each changed component. Use existing test groups and crash capture, not
full seven-job CI after every edit. Confirm immutable source/binary provenance
and serialize expensive measurement jobs. Final selection must exercise normal
and optimized JIT plus linked AOT; stack/authority negatives are mandatory.

Linux x86_64 numbers do not establish Windows/macOS or ARM64 timing, scheduler
choice, ABI or stack behavior. Boss alone controls any later hosted verification.
No native-platform completion claim may be inferred from local Linux acceptance.

## Decisions genuinely requiring review

1. **Wrench/Boss approve the proposed repair sequence and experiment gates.**
   Recommendation: shared frontend fixes, decoder compaction, then import
   emission; use residual profiles to decide broader root work. No new stdlib.
2. **Only if a low-codegen development policy is desired:** should `--no-opt`
   also request `CodeGenOptLevel::None`, or should existing behavior remain?
   Recommendation: preserve current semantics during the primary fixes; later
   compare an explicit policy with runtime/stack costs and correct target/cache
   identity. Do not alter default O2. A Jit instance currently fixes its target
   machine policy, so mixed compile modes require deliberate design.

Nested `?` correctness is already authorized as a repair objective; exactly-once
evaluation, readonly authority, JSON metering, LLVM version, native publication,
source security and crash preservation are settled contracts, not new decisions.
This document authorizes neither engineering nor Git/CI actions by itself.
