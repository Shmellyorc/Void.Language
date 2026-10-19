# Stage 6 Deliveries 1–2: native async and structured lifecycle

Delivery 1 is owner-confirmed hosted-accepted at
`34efb761965a5e19841e2fcf2816a5b851e902a7`, [CI #73 checks](https://github.com/Shmellyorc/Void.Language/commit/34efb761965a5e19841e2fcf2816a5b851e902a7/checks):
7/7 jobs, 98/98 suites each (686/686). Delivery 2 passed local Debug, Release and strict ASan/UBSan/leak acceptance:
**113/113 each (339/339)**, plus 1 MiB stack and editor/package checks. Hosted
acceptance remains owner-controlled; stop for review before any Delivery 3 work.
The five-delivery roadmap and restricted decisions are in [STAGE6_PROPOSAL.md](STAGE6_PROPOSAL.md).

```typescript
async func increment(value: i32) -> i32:
    return value + 1

async func entry(value: i32) -> i32:
    var first = increment(value)
    var second = increment(value + 1)
    return await first + await second
```

An async call evaluates its receiver and arguments in the existing source order,
snapshots value arguments, allocates a zero-initialized traced heap activation
and enqueues it. Its body never runs inline. Fixed arrays, including nested
fixed children, copy; managed references, lists and dictionaries keep their
existing identities and authority. The inferred task handle has no public type
spelling, payload fields or constructor. `await` is legal inside async functions,
including repeated awaits and already-completed tasks. A returned fixed value is
copied independently at each await. A no-result async function can be awaited as
a statement. Existing result/enum handling and synchronous local closures compose
with suspension; reached defers remain registered until checked exit.

Ordinary async functions and managed receiver methods are supported. The receiver
identity is captured and rooted before enqueue, independent of later receiver
variable rebinding. Async value receivers, virtual/interface methods, lifecycle methods,
accessors and async function values diagnose before LLVM lowering. Value receiver
storage cannot outlive its caller by retaining a local address; async dispatch and
callable lifetime contracts need separate review. These are delivery restrictions,
not permanent language prohibitions. Awaiting an argument while an interface
receiver is prepared is diagnosed; await into a local before selecting that
method, as required by the existing no-borrow-across-suspension boundary. Callable-containing values cannot cross new
async argument/result boundaries, including through managed graphs. Existing
synchronous closure behavior remains supported within an activation.

## Explicit root adapter

The synchronous CLI entry ABI is unchanged. A development host explicitly uses
`Jit::compile_async(analysis, optimize)` for an `async entry(i32) -> i32` and invokes
the returned ordinary `NativeCode`. `emit_async_object(module, path, optimize)`
provides the corresponding native AOT adapter; the normal CLI does not select it
implicitly. Calling an async function requires an active adapter.

One owning thread executes activations in FIFO ready order. It suspends the
current activation when a dependency is incomplete, resumes ready waiters in
creation order, and never blocks an executor thread on a child. Concurrent roots
on one Runtime and nested adapters are checked failures. Separate Runtime owners
retain their existing independence. There is no worker pool, parallel spawn,
detachment, cancellation recovery API or general native binding surface.

## Structured task lifecycle (Delivery 2)

Every activation owns an implicit child scope. An explicit lexical group owns
async calls created within that scope, including calls through synchronous helpers:

```typescript
async func run(value: i32) -> i32:
    with tasks.group() as jobs:
        var pending = increment(value)
        var answer = await pending
    return value
```

The group alias exists only within its lexical body and is not a first-class
value. Declare a result local outside the group to use it after that body; normal
VOID lexical scoping applies. `jobs.spawn`, parallel source execution and explicit
public task type spelling remain deferred. A user binding/import named `tasks`
continues to resolve normally; `tasks.group()` is recognized contextually in
`with` when that name is unbound.

Normal group exit joins its children asynchronously. Normal activation completion
(including a final top-level return) drains its children. An early return from a
nested/conditional region, `break`, `continue` or `?` requests cancellation of
unfinished work in the exited scopes and joins before leaving. Returning from an
explicit group cancels/joins that group. A normal-success root completion retains
Delivery 1's drain rule. Dropping a handle never detaches a task.

`pending.cancel()` requests cooperative cancellation idempotently, including
through an inferred synchronous helper or a nonsuspending cleanup callback. Requests are
observed before an async body starts, at awaits and typed CFG block checkpoints.
A task already completed remains completed. Cancellation observed before start
executes no body and no unreached defer. Awaiting a cancelled task propagates a
distinct cancellation outcome through reached cleanup; it does not synthesize an
`Err` or expose a catching/inspection API. A returned `Err` is ordinary successful
task completion and never automatically cancels siblings. Repeated awaits retain
the same outcome and independent value-copy behavior.

Cancellation or runtime failure cancels/joins descendants before parent cleanup.
Every ordinary managed-resource `with` in an async body also owns a private child
scope: normal resource exit joins before one-shot deinit; early exit cancels/joins
before deinit. This retains resource lifetime without giving general await to
ordinary `with`, `defer` or `deinit`. Indirect enqueue from a defer/deinit callback
is checked and rejected: cleanup cannot publish work after its drain. Synchronous
closures keep their existing behavior outside these cleanup boundaries.

Group and activation draining is cancellation-shielded and never blocks the owning
executor thread. A request arriving after body completion does not interrupt its
shielded cleanup. Existing reached LIFO defers and one-shot deinit remain in use.
Cancellation is kept separately from RuntimeFailure while cleanup runs, so an
actual cleanup failure supersedes cancellation and cannot disappear behind it.

A child runtime failure cancels unfinished siblings in its owning group and wakes
a parent waiting on another task. Existing parent runtime failure remains primary;
otherwise unobserved child failures are selected in creation order across the
activation's groups. Additional failures, including failures found in cancelled
children's cleanup, retain the established 32-frame diagnostic bound. Diagnostics
own their text and caller locations. Observed failures propagate through await.
Self-await, ancestor waits and clear dependency cycles are checked failures;
waiting tasks resume through reached cleanup. Publication, waiting, joining and
single completion remain checked executor transitions.

The adapter drains all started activations before returning, even after cancellation
or failure. All precise roots, Runtime metadata and the enclosing native code
lease remain retained until drain and cleanup finish. Runtime close rejects new
admissions; already active work follows checked failure/cleanup if further task
allocation is rejected. Closing neither force-terminates a body nor invents public
cancellation/shutdown APIs.

## Ownership and validation

The existing typed CFG and LLVM lowerer emit a native start wrapper and native
resume function. Heap homes hold arguments, locals, instruction temporaries,
resume state and cleanup state. Resume stack roots cover existing copy/construction
scratch storage and are removed before suspension. Only validated resume states
can be entered; required frame reads and borrowed values crossing suspension have
an independent IR proof. Malformed task shapes, forged payload access/construction,
sync bypass calls and awaits during deferred cleanup are rejected independently
of source checking. Lifecycle validation additionally proves opaque group-token
use, matching LIFO joins across CFG merges, required structured drains, and no
async work or repeated drain after cleanup begins. Malformed cancellation/group
operands, forged/escaping tokens and exits that bypass a group join diagnose
before LLVM lowering.

Task payload authority/provenance uses the existing logical reference-path model,
as collection handles already do. Physical GC maps trace the task pointer; its
ordinary heap descriptor traces the eventual result. No payload offset is treated
as an inline root in a caller's pointer home. This preserves readonly return
profiles without inventing a second capability or permission system.

All native task records and their task/frame handles live within the drained root
invocation. Its existing NativeCode generation lease covers every suspended
resumption and diagnostic; handles cannot escape that invocation. A module-local,
address-significant generation token rejects enqueue from a different generation
through trusted native reentry. Compatible body reload publishes a new root while
old roots finish with their original code and Runtime metadata. Old generations
retire after the last enclosing lease releases. No native code address is stored
in managed frame descriptors. Root-scope storage currently retains completed task
records and heap frames until drain finishes; long-lived roots can therefore grow
with total created work. More eager reclamation is future measured work.

Normal/O2 JIT and AOT fixtures cover enqueue ordering, arguments, receiver identity,
managed/nullable values, results, generic forwarding, fixed copies, large arrays,
loops and closures. Additional regressions cover deterministic unobserved failure
draining, checked cleanup, allocation failures, GC rendezvous, concurrent executor
rejection, reload/cache leases and malformed IR. Exact acceptance results and
remaining limitations are recorded in [WORK_LOG.md](WORK_LOG.md). Delivery 2 adds
real source JIT/AOT fixtures for cancellation before start/during suspension,
completed/repeated await, normal/early/Err exits, nested/implicit scopes, sibling
failure, cleanup failure ordering and parent precedence. Deterministic semaphore
poll gates cover cancellation with GC rendezvous, compatible reload/retirement
and Runtime close; allocation sweeps and malformed IR are permanent regressions.
