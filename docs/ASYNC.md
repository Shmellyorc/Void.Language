# Stage 6 Deliveries 1–3: async, structured lifecycle and bounded workers

**Locked baseline:** Delivery 2, `5d228804be809465ce2c97b7e0b58b89caa718ac`,
[CI #74 checks](https://github.com/Shmellyorc/Void.Language/commit/5d228804be809465ce2c97b7e0b58b89caa718ac/checks),
owner-confirmed 7/7 jobs, 113/113 each (791/791). Delivery 3 passed local Debug, Release and strict
ASan/UBSan/leak acceptance: **120/120 each (360/360)**, plus editor/package and
1 MiB checks. STOP for owner review and hosted verification. Deliveries 4–5 remain deferred and
require owner authorization after the prior hosted gate. Git and hosted CI belong
to the owner. The five-delivery roadmap remains in STAGE6_PROPOSAL.md.

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
retain their existing independence. Bounded parallel source jobs use the same scheduler, as described below.
Detachment, cancellation recovery and general native bindings remain deferred.

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
VOID lexical scoping applies. `jobs.spawn` and explicit public task type spelling remain deferred. A user binding/import named `tasks`
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

## Bounded parallel spawn (Delivery 3)

```typescript
func calculate(value: i32) -> i32:
    return value + 1

async func entry(value: i32) -> i32:
    var pending = spawn calculate(value)
    return await pending
```

`spawn` requires an async body and a directly resolved named synchronous,
noncapturing function. Caller arguments evaluate once in ordinary caller order;
the target body executes on a native pool thread. Numeric/bool scalars, immutable
strings and recursively safe value structs, fixed arrays and tagged enum/result
payloads cross this boundary. Fixed data is independently copied before enqueue
and on each await. A worker may create and mutate private managed objects, but
cannot receive or return mutable managed/list/dictionary/interface references,
Task handles, resource wrappers, callable values or nested shapes containing them.
Readonly aliases do not make mutable objects transferable.

Existing capability analysis records the resolved call/reference edges while
inferring effects. Spawn checks those summaries and reachable edges; task creation,
await, cancellation forwarding, indirect calls and unknown/native effects are
rejected within a worker's reachable source graph. The independent typed-IR
validator repeats the transfer, effect and worker-entry proof. Compiler-created
reached defer/deinit callbacks remain supported with their known source targets;
ordinary indirect source invocation is deferred. There is no second effect lattice.
Current module constants are immutable templates; this does not approve future
mutable globals. `jobs.spawn`, nested spawn, synchronous worker task waits and
shared-reference parallelism remain deferred.

The existing Runtime owns all precise roots and coordination. Private typed-IR
worker wrappers use the same heap-frame and native checked-resume lowering as
async activations. A worker writes only its own activation/result payload. A
release/acquire completion handoff publishes that owned result; the executor
alone applies Task outcome/status, waiter wakeup, failure propagation and parent
frame changes. Ready activation work continues while workers execute. When no
activation is runnable, the executor waits for completion notifications in
NativeSafe state. No scheduler lock crosses Runtime admission, Managed entry,
user code, collection or code retirement. Each worker attaches and admits once,
then uses NativeSafe idle waits so Runtime close drains admitted work safely.

The host-only `Runtime::async_worker_limit(count)` configures an idle Runtime:
1–64 workers, default 4. These are finite native resource limits, not a language
task-count ceiling. The pool starts lazily with the first parallel job, drains and
joins before the adapter returns. Its linked admission queue uses existing Task
records and is FIFO; one worker executes admissions in order. With several workers,
start/finish order and fairness are not promised. Queue/storage growth is limited
by available memory and checked allocation failure; no arbitrary source task cap
or queue-full cancellation policy is introduced. Existing root-scope retention of
completed records/frames remains unchanged.

Cancellation is an atomic request. Explicit worker checkpoints in synchronous
CFG loops/helpers unwind through existing reached cleanup. `vd_gc_poll()` remains
a collection safepoint and never throws for cancellation. Reached cleanup is
shielded; a real cleanup failure supersedes cancellation. Scope exit and root
shutdown retain storage, Runtime metadata and the enclosing generation lease
until workers actually finish, even after cancellation/failure. There is no
detachment, forced termination or worker child join. Worker completions are also
received at owner activation checkpoints, so a running owner observes child
failure without first awaiting. Existing parent-primary and creation-order
unobserved-failure rules apply.

Private frame/code addresses remain absent from managed metadata and persistent
Debug cache records. Same-generation enqueue validation covers worker publication;
a compatible reload may publish new code while old queued/running workers retain
their root invocation's lease. Final local/cross-platform evidence belongs in
WORK_LOG.md; no hosted Delivery 3 acceptance is implied here.
