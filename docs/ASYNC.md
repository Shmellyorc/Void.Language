# Stage 6 Deliveries 1–5: async, lifecycle, workers and native integration

## HTTP Runtime foundation ownership amendment, 30 October 2026

The Boss-authorized HTTP mission now implements a working-tree
[foundation amendment](net/http/RUNTIME_FOUNDATION.md): externally completed native
Work with owner publication, shielded abandonment, activation/group lifetime
drain and pre-wait native-provider shutdown notification. These private facilities
do not add public task syntax or a second scheduler. Actual HTTP/source integration now exists;
independent full implementation acceptance remains pending.

After quiescent completion, scheduler result Handles and completed Work may retire.
Ordinary precise source task references preserve reachable results and repeated
awaits; uncommitted acquisitions and the adapter's root result keep their required
roots. Empty scheduler Handles unregister. Quiescent native Task/group records may retire after the existing precise GC
proves their task payload unobservable and no child, Work, failure or waiter still
needs them. Creation sequence IDs remain monotonic; retained source handles keep
both repeatable results and the necessary native ancestry. The existing worker
pool has Runtime lifetime, while source Work remains Scope-owned and drained.
Native-only file producers share that same bounded queue. Runtime close joins
idle workers after scoped source work drains. No whole-Runtime RSS ceiling is claimed.
This supersedes the historical blanket result/Work/null-Handle retention statements
below; the original Stage 6/8 evidence and frame-retirement rules remain history.

## Stage 11 globals and owner execution

Runtime-global access belongs to the initializing application's owner thread.
Worker-reachable reads/writes reject through source effects and independent IR,
including helper/method/property/operator indirection. Safe scalar, immutable-string
and supported value snapshots can still be evaluated on the owner and transferred
to workers. Owner async activations keep ordinary interleaving; globals add no
atomics or scheduler. Startup itself remains synchronous and excludes task creation
and native callback registration. See [runtime globals](RUNTIME_GLOBALS.md).


## Stage 8 Sprint 5 completed-frame lifetime

The [Sprint 5 contract](STAGE8_ASYNC_RETENTION.md) supersedes historical blanket
root-duration execution-frame retention below. After measurements established
material growth, the scheduler now clears a completed execution-frame root only
after its native resume returns, or after the owner acquires worker completion.
The existing GC then reclaims the frame and frame-only referents. Completion
notification alone never triggers retirement. Task/result records and diagnostics
remain available for repeated awaits, identity/copy behavior and ordered failure
handling. Active/suspended/joining frames remain rooted. Native registrations and
the enclosing code lease retain their existing independent lifetimes.

This does not bound total root memory: Task/Work records, result objects, null
Handle registrations and allocator capacity can still grow with total work.
[Acceptance and measurements](stage8/SPRINT5_ACCEPTANCE.md) distinguish actual
verification from approvals and remaining platform/owner gates.


**Historical Deliveries 1–5 context:** the lock/planning statements below describe
that delivery era; [the docs index](README.md) records current Stage 8 status.

**Locked baseline:** Delivery 4, `619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
owner-confirmed 7/7 jobs, 131/131 each (917/917). Delivery 5 integrated acceptance
passed locally: 139/139 each in Debug, Release and strict ASan/UBSan/leak checks
(417/417), plus 1 MiB and editor acceptance. Owner review and seven green hosted
jobs are required before the final
Stage 6 lock. Stage 7 remains planning-only and unauthorized. Existing
[NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md) ownership/ABI boundaries remain intact.
The five implementation deliveries remain distinct from D1–D6 design decisions.

Native resources cannot cross task/worker boundaries, and native calls remain
forbidden in worker-reachable graphs. This delivery does not route blocking native
calls onto workers: the owner enters NativeSafe, foreign callbacks enter through
the existing rooted CallbackRegistration protocol, and the provider drains every
reserved dispatch before releasing roots and generation leases. Static native
binding does not expand the approved value-only parallel memory model.

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

## Delivery 5 integrated evidence

`examples/native/integrated.void` exercises async parents/children, lexical groups,
bounded workers, retained/returned UTF-8 bytes and same/foreign native callbacks in
one application. The native owner is created on the owning executor and remains
local across suspension; it is never transferred as a task/worker argument or
result. Normal/O2 JIT uses the explicit `compile_async` adapter; independent AOT
uses `emit_async_object`. Cancellation and parent-plus-cleanup failure programs
verify draining before deterministic native cleanup. No implicit async CLI entry
or new native/worker contract is added.

Permanent host regressions hold a running worker and another published/queued job
under suspended async parents while GC and compatible reload occur. The old root
keeps its original private suspension frame and native callback code. After root
drain, a retained registration still pins its exact old generation until explicit
unregister on the original owner thread. Repeated compatible reloads retire back
to one current generation; collected post-drain managed object counts return to
zero. Runtime metadata/LLVM allocator memory is not measured by that object count.
An old C callback cannot enqueue its old task code into a replacement's executor.

Native shutdown also overlaps a held worker and a real foreign callback reserved
before admission; provider quiescence protects the callback context, roots and
code while task draining protects worker frames/code. Tests use deterministic
barriers, not sleeps. Allocation sweeps cover combined publication and cleanup.
Resource task transfer, native worker effects, incompatible layout/capture changes
and unauthorized tooling imports remain checked rejections. See WORK_LOG.md for
actual final acceptance and retained-memory observations.

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

## Await and result propagation

`await job?` means `(await job)?`: await the task, then propagate its result.
Both spellings use the same checked await, result inspection and early-exit
cleanup. Calls, member selection and indexing that form the task operand still
bind before await. A postfix chain after `?` acts on the successful payload:

```typescript
async func load_scene() -> result<bool, string>:
    var world_job = load_world()  # Application-defined async result producer.
    var world = await world_job?
    var audio = await load_audio()?
    return Ok(true)
```

This example assumes application loading functions with the matching result
error type. No standard `error` module is added by this syntax change.

| Source | Grouping |
|---|---|
| `await job?` | `(await job)?` |
| `(await job)?` | `(await job)?` |
| `await (job?)` | `await (job?)` |
| `await load()?.name` | `((await load())?).name` |
| `await load()??` | `((await load())?)?` |
| `await job? + 1` | `((await job)?) + 1` |

Explicit parentheses retain their grouping. In `await (job?)`, `job` must be a
result whose successful payload is a task; an ordinary task handle diagnoses at
the inner `?`. Call arguments and index expressions also retain their own
expression boundaries, for example `await identity(envelope(value)?)`.
Propagation still requires exactly matching error types in a result-returning
function. An awaited `Err` is ordinary task completion; the `?` then follows the
existing early-exit cancellation/draining, reached defer and resource cleanup
rules below. Runtime failure and cancellation remain distinct from `Err`.

The [syntax handoff](async-await-syntax/HANDOFF.md) records local verification;
hosted acceptance remains Boss-controlled.

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
or failure. Active-frame/result/callback roots and Runtime metadata remain protected through
their required cleanup, and the enclosing native code lease remains until root
drain. Completed execution-frame roots may retire earlier only at Sprint 5's
proven quiescent points. Runtime close rejects new
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
in managed frame descriptors. Root-scope storage retains task/result records until drain finishes; long-lived
roots can still grow with total created work. Sprint 5 retires completed execution
frames at the proven quiescent points described above; broader record/result
retirement remains deferred.

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
Module constants remain immutable templates. Stage 11 adds runtime globals
under the owner-only access rules above. `jobs.spawn`, nested spawn, synchronous worker task waits and
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
or queue-full cancellation policy is introduced. Root-scope record/result retention remains unchanged. Sprint 5 releases worker
execution-frame roots only after the owning executor acquires completion.

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
