# G86 — Advanced async support

Candidate based on accepted G85 `85c6c585d89fc51c8ab4e3b16935d5eb5266d937`.
Prepared for independent Golden Wrench review; this report does not grant its own
acceptance. Linux evidence does not qualify Windows/macOS execution. No staging,
commit, branch, push, installation, publication or hosted CI was performed.

## Implementation decisions and scope

G86 delivers a focused advanced async layer: explicit async sequence consumption,
ordinary managed/generic receiver integration, readable inferred task types, and
runnable generic structured coordination. It extends G85's checked expansion
rather than adding an iterator Runtime, scheduler, stream framework or new task
ownership model. [Canonical async semantics](../ASYNC.md) and the
[iteration contract](../CUSTOM_ITERATION.md#async-iteration--g86) define behavior.

| Area | Delivered behavior and boundary |
| --- | --- |
| Functions and callable integration | Ordinary generic async functions and managed receiver specializations participate in the new protocol. Safe one-hop promotion from concrete value wrappers to managed async receivers works. Synchronous closures inside an activation still compose with body suspension. Async function/method values, callable-containing task transfers and direct async value receivers remain checked rejections. |
| Composition | Keep one existing lexical coordination API: `with tasks.group()`. Start independent jobs before awaiting them; normal scope exit joins children. The generic `coordinate<T>` example combines two typed results, and constrained `collect<T,C>` provides a reusable typed async abstraction. No overlapping `all`/race/detach API was added. |
| Errors and cancellation | `Err` remains successful task completion and does not cancel siblings. `?` preserves checked early exit. Awaited cancellation remains distinct from runtime failure and typed errors. Reached advancement defers and descendants drain before iterator destruction; a cleanup failure supersedes cancellation. |
| Async iteration | `for await item, key in source:` selects synchronous `async_iterator()`; reverse selects `reverse_async_iterator()`. The concrete state selects ordinary parameterless managed `async next() -> option<T>`. Optional synchronous `index()` supplies its exact key type. One advancement is awaited at a time, with no prefetch or fallback to the synchronous protocol. |
| Workers | Existing bounded source pool and value/effect restrictions remain. The example composes a scalar worker with owner async traversal. Managed/native resources and unsynchronized mutable state gain no transfer permission. |
| Native operations | The workflow awaits an existing native timer; external completions, native workers and integrated callback lifetimes retain their existing ownership, ordering, generation and shutdown contracts. No arbitrary blocking native-worker permission was added. |
| Runtime | Ordinary precise activation/task roots and existing resource groups preserve suspended cursor/value lifetimes. Existing completed-frame retirement and quiescent task-record reclamation remain authoritative. No new persistent root registry, internal queue or allocation policy was added. |
| Tooling | The shared frontend provides inferred binders, generic/task hover, parameterless signatures, completion, navigation/references, semantic tokens and recovery. `task<T>`/`task<no result>` is display syntax for opaque inferred handles, not a public type annotation or constructor. |

Acquisition remains synchronous: a loop activates its resource scope immediately
after successful factory completion. Failure during acquisition activates no
cleanup. Managed state with a destructor cleans once on normal completion,
`break`, return, propagation, runtime failure and cancellation. The next task's
reached defers finish before destruction, including when cancellation arrives
during its suspension. Body suspension and nested task groups use existing
shielded drain rules. Normal source/argument order, value-copy semantics,
visibility and readonly authority apply to every generated call.

A concrete value wrapper may forward `next` to a managed component through the
ordinary composition rule. It still has no destructor of its own; resource-owning
cursor state must be managed. Collection interface views and generic constraints
can expose the synchronous factory with a concrete return type. Erased iterator
state cannot hide lifecycle requirements.

Implementation changes are confined to the existing parser/semantic protocol
expansion and common type formatting. Async advancement expands into the ordinary
checked method-call/await/option-test sequence; managed resource state uses the
same `with` scope as synchronous custom iteration. Existing typed IR independently
validates the resulting calls, suspension, capabilities and drains; existing LLVM
lowering handles both JIT and AOT without a new instruction or Runtime ABI.

## Research and deferred capabilities

Research preceded implementation and was bounded by acquisition, advancement,
structured joining and callable lifetime decisions. The official
[C# await-foreach specification](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/statements#13953-await-foreach)
provides a pattern-based async advancement and scoped-disposal precedent. VOID
keeps G85's combined optional-value operation rather than a separate Current
property, and retains synchronous cleanup after task drain.
[Kotlin structured concurrency](https://kotlinlang.org/docs/coroutines-basics.html#structured-concurrency)
supports retaining lexical ownership instead of adding independent task lifetimes.
[Task.WhenAll](https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.whenall?view=net-10.0)
provides a comparison for aggregate completion; VOID's existing task groups already
provide the required lexical join and established failure ordering. These design
references are current documented approaches, not upstream patches. No proposed,
reverted or abandoned upstream implementation was imported.

The [Rust closure reference](https://doc.rust-lang.org/stable/reference/types/closure.html)
and [accepted async-closure RFC 3668](https://github.com/rust-lang/rfcs/blob/main/text/3668-async-closures.md)
show why captured/lending async callables require distinct lifetime reasoning.
G86 does not relax VOID's callable task boundary or borrowed receiver restrictions.
Upstream designs are references, not evidence of VOID correctness or compatibility
with its LLVM/Clang/LLD 23.1.4 implementation.

Deliberate deferrals:

- Async factories: cancellation after acquisition but before loop scope activation
  needs an explicit abandoned-resource-result contract. Current synchronous
  acquisition avoids introducing that unreviewed gap.
- Async destruction/index and direct async value receivers: these require new
  cleanup or receiver-storage lifetime contracts. Managed advancement and
  synchronous indexing/cleanup cover this layer.
- Async interface dispatch and erased cursor state: ordinary interface lifecycle
  erasure cannot represent the needed concrete destructor/activation contract.
- Async function values, closures and arbitrary callbacks: the existing callable
  representation does not express async capture/result and lending lifetimes.
- Public task type syntax, first-class groups, wait-any/racing, detached tasks,
  generator `yield`, prefetch, streams/backpressure and general cancellation
  recovery: unnecessary for explicit pull traversal and structured coordination.
- Broader worker transfer, arbitrary blocking native calls, a second scheduler and
  unsynchronized shared mutable memory: incompatible with retained contracts.

## Verification

Production sources are frozen for all final profiles. Debug and Release each pass
19/19 selected CTest gates plus the separately run `stage6_integrated` gate (20
unique gates per profile). The two awaited-condition regressions are included in
the final 53-record source driver. The three cursor proof gates were rebuilt and
rerun after tightening the cancellation/cleanup precedence assertion; all pass.
That test-only refinement does not change production artifacts. Fatal sanitizer
results below use the same production source state.

| Profile | Selected gates | Additional native integration | Final G86 source records | LSP receipts |
| --- | --- | --- | --- | --- |
| Debug | 19/19 | 1/1 | 53 | 35 |
| Release | 19/19 | 1/1 | 53 | 35 |
| Fatal ASan/UBSan/leaks | 19/19 | 1/1 | 53 | 35 |

The fatal profile runs all 20 gates in a single CTest invocation (135.83 s).
Debug's 19-gate selection takes 146.07 s and its native supplement 7.92 s;
Release takes 37.83 s and 5.46 s respectively. These wall times describe the
recorded local runs, not performance comparisons. All three profiles use the
same final production sources and the strengthened cursor proof.

Final compiler/server SHA-256:

| Profile | `voidc` | `voidls` |
| --- | --- | --- |
| debug | `0097500fcf509300cc96781a433d204e48d95e1c1ff1a1c79cd6c7b2524e97e5` | `0c7af9d0d55536ef1c95365d0441847efe033be9a208d5118947e3bd31eafa79` |
| release | `8c6f8f89763fe4cd1f063641f820ed99034f5c3f6f407d831178d3b3b6f4f107` | `e033cde9d98b4cdc0122764d1430cb65c9294fc05c8c08cf2fe9562389a87016` |
| strict | `c291bf166d0e4a4e9dcc35638cf26655f7edfd52fd4df5823f3898ccf4eb556b` | `9115b9efea94207f8f4cb754d77d54e97cb3010de15bdc4526782c8002613e75` |

The G86 source driver checks 22 invalid protocol/context/authority/worker cases,
13 positive programs plus the generic workflow in fresh JIT O0/O2, and three
imported visibility failures. Positives include empty, reverse, exact string keys,
single evaluation of source/factory, None termination, suspended bodies with a
local closure, generic constraints and interface collection views, typed errors,
repeated-await fixed copies, promoted managed receivers and 24/48 nested async
loops. All children use the original 1,048,576-byte stack. The loop grammar and
existing semantic/depth guards are not weakened.

The native cursor probe exercises 15 exit modes with forced GC both enabled and
disabled. Normal completion, continue, break, return, typed propagation, body/
completion/index/acquisition/destructor failure, escaped managed results,
before-start cancellation, suspended cancellation and cleanup-failure precedence
are observed. It checks precise roots, publication, empty frame chains, one-shot
closed counts, reached advancement defers and zero live managed objects after
observer-root release. JIT O0/O2 supplies 60 observations; two independently linked
Runtime-only AOT executables add 30 each. AOT drivers re-emit on the actual 1 MiB
stack and compare exact object bytes with the linked objects. The workflow also
passes independent Runtime-only AOT O0/O2 with limited-stack object comparisons.

Existing async source/IR/lifecycle/parallel suites cover repeatable results,
borrow/effect/worker restrictions, nested groups and failure ordering, allocation
failure, live-generation replacement, Runtime isolation and shutdown. Existing
retention measurements qualify frame retirement and subsequent native bookkeeping
maintenance; no whole-Runtime RSS or constant-memory claim is made. External
completion, native worker and timer-native regressions cover completion ownership
and drain. `stage6_integrated` adds real same/foreign callback registration,
generation leases and shutdown overlap.

Actual **VS Code 1.144.0** and **VSCodium 1.143.1** each pass all six maintained
editor observations using the final Release server through a 1 MiB stack wrapper:
inferred item/index/generic/option/task hover, visible parameterless signature
widget, imported definitions/references, semantic tokens, actual completion popup
in an unsaved async header, and invalid iterable/next edits followed by repair
with a live server. Protocol checks additionally decode and assert method/variable
semantic classifications and survive incomplete binder/header edits. Editor input
files remain unchanged on disk. `npm run build` passes with the existing Node
24.19.0 environment; no tools or extensions were installed or replaced.

Review found and corrected an early candidate parser regression: consuming
`await` unconditionally after `for` rejected the previously valid awaited-condition
form `for await ready():`. Binder-header lookahead now selects the async iteration
modifier only before `item in` or `item, key in`; ordinary awaited conditions keep
their expression grammar. Call and handle condition regressions execute at O0/O2
under 1 MiB. The original minimal source, diagnostic and compiler identity are
retained. The official [Rust predicate-loop grammar](https://doc.rust-lang.org/reference/expressions/loop-expr.html#predicate-loops)
was a bounded comparison for preserving expression conditions; no upstream patch
was imported. Pre-correction qualification is retained as superseded evidence,
not counted toward final results.

Other development failures were confined to test setup and assertions: an initial
fixture used `?` in an i32-returning entry, a timer example attempted a private
constructor, an observation probe selected the second constructed object instead
of its ledger, a negative assertion expected a more specific built-in diagnostic,
and newly configured CMake targets needed regeneration before target selection.
Their original logs and source/protocol receipts are retained. No compiler crash
or sanitizer finding is discarded or reclassified as successful evidence. The
initial bare `npm` call failed because it was absent from PATH; rerunning with the
existing Node environment completed the build.

## Reproduction and evidence

Evidence directory:
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-work/g86-7K1cZCXR`.
The review archive, SHA-256 receipt and extracted-member verification are stored
under `/home/shmellyorc/Documents/Void.Language-archives/` as
`g86-advanced-async-2026-11-20-7K1cZCXR.zip` and adjacent receipts. It embeds the
candidate files, baseline patch with checked byte-for-byte replay, exact commands,
build identities, selected test/source/LSP/editor receipts and original failure
logs. Rebuildable binaries, compiler objects and browser profiles are excluded.

Use the existing `build/llvm23-debug`, `build/llvm23-release` and
`build/llvm23-strict` configurations with LLVM/Clang/LLD **23.1.4 exclusively**.
Task-local disk-backed `TMPDIR` avoids unrelated tmpfs exhaustion. Build targets:

```text
voidc voidls void_io_runner
void_advanced_async_tests
void_advanced_async_aot_normal void_advanced_async_aot_optimized
void_advanced_async_workflow_normal void_advanced_async_workflow_optimized
void_custom_iteration_tests void_async_tests void_native_workers_tests
void_async_external_tests void_async_timer_tests void_retention_current
void_native_tests
```

Run CTest with this exact 20-gate selection under a 1 MiB soft/hard stack and
core generation disabled; use external `VOID_TEST_DIAGNOSTICS_DIR` for receipts:

```text
^(advanced_async_(runtime|source|aot_normal|aot_optimized|workflow_normal|workflow_optimized)|lsp_advanced_async|custom_iteration_(source|runtime)|async_(source|ir|parallel|lifecycle|external|timer_native|retention_sprint5)|native_workers|lsp_(async_syntax|editor_recovery)|stage6_integrated)$
```

Fatal sanitizer environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS unset
```

The task-owned `qualify.py`, `final-source.py`, `native-check.py`,
`proof-check.py` and `run-editor.py` retain actual profile, launcher, stack and editor commands. Build
processes use their ordinary host stack; source compiler/server/emitter/native
children are restricted to 1 MiB. Electron retains its ordinary host limits.
Native Windows/macOS execution and hosted CI remain **UNQUALIFIED**. This is
risk-proportionate focused Linux qualification, not full-repository acceptance.

## Exact changed-file inventory

The candidate changes 24 files. No public root README or historical milestone
record is rewritten.

```text
CMakeLists.txt
docs/ADVANCED_LANGUAGE.md
docs/ASYNC.md
docs/CUSTOM_ITERATION.md
docs/DEVELOPMENT_STRATEGY.md
docs/GC_RUNTIME.md
docs/GENERICS.md
docs/LANGUAGE_BIBLE.md
docs/README.md
docs/projects/G86_ADVANCED_ASYNC.md
editors/vscode/src/test/advancedAsyncHost.ts
examples/async/advanced/sequence.void
examples/async/advanced/workflow.void
src/frontend/compiler.cpp
src/frontend/iteration.inc
src/ir/module.cpp
tests/async/advanced/aot.cpp
tests/async/advanced/aot.py
tests/async/advanced/editor.void
tests/async/advanced/lsp.py
tests/async/advanced/proof.hpp
tests/async/advanced/roots.void
tests/async/advanced/source.py
tests/async/advanced/tests.cpp
```
