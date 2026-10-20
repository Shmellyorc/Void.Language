# Stage 6: concurrency and native libraries

## Delivery 5 local acceptance — 19 October 2026

Delivery 4 is owner-confirmed hosted-accepted at
`619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
7/7 jobs, 131/131 each (917/917). Delivery 5 alone is now authorized: integrated
async/groups/workers/native/GC/reload/cache/tooling acceptance, repairing only
demonstrated defects. Preserve all existing boundaries and 131 suites. Focused
checks precede one consolidated final Debug/Release/strict, 1 MiB and editor
acceptance. Stop after local acceptance for owner review and hosted CI; Stage 6
locks only after seven green hosted jobs. Stage 7 remains planning-only and
unauthorized; its documented goals and research/Work preferences are unchanged.
The earlier Delivery 4 pending-hosted status below is historical.

**Delivery 5 is locally complete:** 139/139 suites each in Debug, Release and
strict ASan/UBSan/leak configurations (417/417); corrected cancellation-fixture
checks passed in all three. All 42 native 1 MiB AOT runs, three semantic-depth
checks, editor unit tests, three real editor profiles and VSIX packaging passed.
No compiler/Runtime production repair was required. Evidence and the initial
fixture-race correction are recorded in [WORK_LOG.md](WORK_LOG.md).
**STOPPED for owner review, commit/push and seven-job hosted verification.**
The final Stage 6 lock is pending; Stage 7 implementation is not authorized.

| Implementation delivery | Current status and dependency |
| --- | --- |
| 1 — Native Async Activation | Hosted accepted, CI #73 |
| 2 — Structured Task Lifecycle | Hosted accepted, CI #74 |
| 3 — Bounded Parallel Spawn | Hosted accepted at `62b17c8c`, CI #75 |
| 4 — Native Libraries | Hosted accepted at `619331cc`, CI #76; current locked baseline |
| 5 — Integrated Stage 6 Acceptance | Locally accepted: 139/139 Debug, Release and strict; owner review/seven-job hosted acceptance pending |

These are the five implementation deliveries, not the six D1–D6 decision groups.
D1 parallel execution now approves only the documented initial value-only scope.
D2 enqueue/await/inferred handles, D3 root/group ownership and D4 lifecycle rules
retain their approved contracts. D5 cross-task callable escapes stay restricted;
D6 now approves the bounded trusted static fixture described in [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md); general FFI remains deferred. No general sharing, task-type spelling,
async-callable or cancellation recovery contract has been silently added.


## Historical Delivery 4 authorization — superseded by Delivery 5 above

Delivery 3 is owner-confirmed hosted-accepted at `62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090`,
[CI #75 checks](https://github.com/Shmellyorc/Void.Language/commit/62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090/checks),
7/7 jobs, 120/120 each (840/840). Delivery 4 alone is authorized: one explicitly
trusted statically compiled C fixture, typed project-local binding descriptors,
ordinary VOID wrappers, owned UTF-8 transfers, opaque managed resource identities
and quiescent rooted callbacks. No public native grammar, dynamic loading, package
management or worker/native effects are approved. The bounded implementation is complete.
**Local Delivery 4 acceptance passed: 131/131 each in Debug, Release and strict ASan/UBSan/leak configurations (393/393).** The 1 MiB semantic/native checks, explicit native host and editor/package checks passed. STOPPED for owner review and seven-job hosted verification; Delivery 5 remains unauthorized. See [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md).
Delivery 5 requires Delivery 4 hosted acceptance and separate owner authorization.
Boss controls Git/hosted CI; Wrench reviews; Keyboard implements this delivery and
stops after complete local acceptance. Earlier pending-hosted statuses are historical.

## Historical Delivery 3 authorization — superseded by Delivery 4 status

**Locked baseline:** Delivery 2 at `5d228804be809465ce2c97b7e0b58b89caa718ac`,
[CI #74 checks](https://github.com/Shmellyorc/Void.Language/commit/5d228804be809465ce2c97b7e0b58b89caa718ac/checks),
owner-confirmed 7/7 jobs, 113/113 each (791/791). Delivery 3 alone is now authorized:
bounded native workers for directly named synchronous noncapturing source functions;
scalar/string and recursively safe fixed value transfers only. No mutable managed
references, task/callable/native-resource transfers, worker task creation or joins.
Preserve structured cancellation/drain, precise GC, generation leases and shared
JIT/AOT lowering. Local acceptance passed **120/120 each** in Debug, Release and strict
ASan/UBSan/leak configurations (**360/360**), plus editor/package and 1 MiB checks.
**STOPPED for owner review and seven-job hosted verification.** Deliveries 4–5
remain unauthorized. Stop after local acceptance for owner review and hosted CI.
D1 parallel sharing is approved only within this initial value-only boundary;
the six design-decision groups remain distinct from the five deliveries.

Earlier Delivery 2 pending-hosted handoffs below are historical, superseded here.
Boss controls Git/CI, Wrench reviews architecture, Keyboard implements authorized work.

## Historical Delivery 2 handoff (superseded by Delivery 3 status above)

**Current locked baseline:** Delivery 1 at `34efb761965a5e19841e2fcf2816a5b851e902a7`,
[CI #73](https://github.com/Shmellyorc/Void.Language/commit/34efb761965a5e19841e2fcf2816a5b851e902a7/checks),
7/7 jobs, 98/98 suites per configuration (686/686), as confirmed by the owner.
Stage 5 remains historically locked at `4627687b3a18349633715e4294bc915cf74ddd41`,
[CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484).

**19 October owner authorization:** implement Delivery 2 only. Cooperative
idempotent `job.cancel()`, distinct success/cancellation/runtime failure (`Err`
is an ordinary result), activation ownership and lexical
`with tasks.group() as jobs:` scopes. Normal exits join; early exits cancel and
join. Cancellation/failure cancels and joins descendants before parent cleanup;
draining is shielded. Preserve parent runtime failure; otherwise select unobserved
child failures in creation order within the bounded diagnostic policy. Child
runtime failure cancels siblings and wakes the waiting parent. Keep one owning
executor, explicit root adapter and synchronous CLI ABI. No implicit detach,
workers, parallel spawn, cancellation recovery API or general cleanup await.
Independent IR and source negatives, JIT/AOT, GC/allocation/reload and complete
Debug/Release/strict sanitizer acceptance are required before owner review.

| Sequential implementation delivery | Scope and dependency | Status |
| --- | --- | --- |
| Delivery 1 — Native Async Activation | Async functions, await, traced frames, owning executor, leased resumptions | Hosted accepted at CI #73 |
| Delivery 2 — Structured Task Lifecycle | Cancellation, groups, failures, structured cleanup/shutdown | Locally accepted: 113/113 Debug, Release and strict sanitizers; owner review/hosted CI pending |
| Delivery 3 — Bounded Parallel Spawn | Workers, reviewed value transfer, parallel source jobs, GC coordination; requires Delivery 2 lock | Not authorized |
| Delivery 4 — Native Libraries | Trusted bridges, typed scalar/string/opaque handles, ownership/callbacks; requires Delivery 3 lock | Not authorized |
| Delivery 5 — Integrated Stage 6 Acceptance | Combined async/parallel/native, reload/cache/lifetime/cross-platform; requires Delivery 4 lock | Not authorized |

**Owner-approved calling boundary (18 October):** ordinary async functions and
managed receiver methods are supported. Root receiver identities and retain code
leases; never retain a pointer to caller-local value storage. Diagnose async value
receivers, virtual/interface async methods and async function values before LLVM
lowering. Their calling, dispatch and lifetime contracts remain future review;
these features are deferred, not permanently prohibited.

These five **implementation deliveries are distinct from D1–D6 design decisions**.
D1: owning-executor async is approved; worker sharing/parallel restrictions remain
future review. D2: enqueue semantics, inferred task handles and await are approved;
public task-type/async-callable syntax is not approved. D3: explicit root adapter
and unchanged sync CLI approved; lexical groups approved for Delivery 2; detach and async CLI rules deferred.
D4: preserve reached cleanup and checked failures; the assigned Delivery 2 cancellation/group policies are approved. D5: per-activation leases approved; new cross-task
callable escapes restricted instead of approving session-wide generation pinning.
D6: native-library scope/grammar/ownership remain Delivery 4 review.

**Workflow for every delivery:** implement selected delivery → focused tests →
complete local acceptance → owner review/commit/push → existing seven-job hosted
CI → lock only when green. STOP after local Delivery 2 review; do not automatically
begin another delivery. No assistant commits, pushes, branch creation, remote
writes or hosted dispatch. Boss owns product decisions and Git/CI; Wrench reviews
architecture/evidence; Keyboard implements only authorized scope.

**Acceptance and recovery:** existing JIT/AOT, GC roots, failure cleanup, reload
compatibility, old-code leases and Debug cache remain mandatory. Record exact local
results, incomplete code, failures and next action in WORK_LOG.md and this handoff.
Main risks are unrooted suspended values, malformed resume states, cleanup loss,
stale generation pointers and callable escapes. Language-facing choices not covered
by authorization return to owner review; do not invent contracts. Owner-approved root policy: drain every started task, including unawaited tasks,
before the adapter returns. Preserve root failure as primary; if root succeeds,
report otherwise unobserved task failures deterministically in creation order.
Retain roots/leases and run all cleanup. Delivery 2 adds the approved cooperative cancellation and lexical groups; detach remains unavailable.
Implementation and exact boundaries are in [ASYNC.md](ASYNC.md). The adapter
drains within its existing NativeCode lease, covering every retained task/frame
and resumption; a generation token rejects cross-generation publication. No task
escapes the drained invocation. Private frame offsets are copied into Runtime
heap layouts, without code pointers or public metadata admission exceptions.
**Delivery 1 historical local acceptance:** Debug, Release and strict sanitizers
passed 98/98 each; subsequent owner-confirmed CI #73 locks Delivery 1 above.
**Delivery 2 current work:** native cancellation outcomes, activation ownership,
lexical groups, shielded nonblocking joins, cleanup ordering and independent IR
proofs are implemented. Final local acceptance passed **113/113** in Debug, Release and strict
ASan/UBSan/leak configurations (**339/339**), plus 1 MiB frontend/native-stack
checks and editor test/package checks. No incomplete Delivery 2 implementation
or failing local test remains. Exact evidence is in WORK_LOG.md; logs are under
`build/stage6-delivery2`. **STOPPED for owner review and hosted acceptance.**
Owner review, commit/push and the seven-job hosted gate must precede a Delivery 2
lock or any authorization to begin Delivery 3. No later delivery is active.


**Original architecture research (18 October 2026):** the recommendations below
are superseded where the current Delivery 1/2 handoff approves or restricts them.
The six decision groups remain distinct from the five implementation deliveries.

## Baseline and recommendation

Stage 5 is owner-locked at `4627687b3a18349633715e4294bc915cf74ddd41`,
[CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484).
Read-only job metadata and each job's CTest log independently confirm seven
successful configurations, **82/82 suites each, 574/574 total**. Windows Debug and
Release include the native-helper resolution repair. Windows persistent caching
still deliberately falls back to compilation; automatic watching is absent.
The lock accepts the delivered Stage 5 subset, not every original roadmap goal.

Recommend **stackless async activations lowered in the existing typed IR, ordinary
precisely traced heap frames, one scheduler associated with the existing Runtime,
and existing generation leases held across suspension**. Start source async on
one owning executor thread. Add a bounded pool for blocking native operations and
a deliberately restricted first parallel `spawn` slice. Do not expose arbitrary
shared mutable graphs to parallel workers before approving their memory model.

Treat native libraries as a separate delivery within Stage 6: a small trusted,
compiled C ABI bridge and normal VOID wrappers first. General header import,
package discovery, arbitrary ABI generation and automatic unloading are not
prerequisites for useful concurrency or one usable native binding.

The largest unresolved decision is **which values may cross into parallel source
jobs**. GC cooperation already exists; safe concurrent mutation does not follow
from it. The recommendations and restrictions below are proposals, not additions
to the approved language contract.

## 1. Existing contracts and implementation evidence

[LANGUAGE_BIBLE.md](LANGUAGE_BIBLE.md) Sections 36–45 agree on async declarations
with ordinary result annotations, implicit task mechanics, `await`, scheduler-based
`spawn`, cooperative cancellation, structured task groups, cleanup, safe native
wrappers and retention of suspended code. They explicitly leave scheduling start,
cancellation representation, group signatures/exit, native ownership metadata and
foreign ABI details open. The examples do not settle these questions.

| Inspected mechanism | Implemented now | Stage 6 consequence |
| --- | --- | --- |
| `src/runtime/coordination.cpp`, `include/void/runtime.hpp` | One Heap; TLS attachments; Managed, NativeSafe and Parked states; cooperative collection; precise Frame/Slot roots and persistent Handle roots | Reuse the coordinator. Suspended tasks need heap storage; stack Frame objects cannot be moved between threads or left linked after return. |
| `src/backend/llvm/backend.cpp`: root emission, `gc_read`/`gc_write`, calls | Fixed stack roots, normal/O2 lowering, direct ordinary field loads/stores, NativeSafe import transitions | Existing collection locks do not serialize application field accesses. Blocking, suspension and user locks require explicit GC-safe boundaries. |
| `include/void/ir.hpp`, `src/frontend/compiler.cpp` | Typed CFG, concrete specialization, closures, dynamic cleanup lists; no task type or suspension terminator | Add task semantics and one lowering transformation here. Do not implement a parallel async parser or backend. |
| `src/runtime/callback.cpp`, `include/void/callback.hpp` | Same/foreign-thread callback admission; provider unregister-and-quiesce barrier; explicit shutdown | Reuse admission for trusted callbacks. A counter alone cannot protect a dispatched callback that has not reached admission. |
| `src/runtime/reload.cpp`, `include/void/execution.hpp` | Leased synchronous invocation; exact compatibility; borrowed failure strings copied while code is leased | A task must retain code between resume calls and own diagnostics after completion. Current `NativeCode::invoke` alone does not do that. |
| `src/backend/llvm/backend.cpp`: generation loading and replacement | Isolated source JITDylibs; explicit main imports then LLVM default links; raw-IR cross-module linking; revision checks | Async resume helpers stay inside their original generation. Preserve the CI #70 link order and serialized compilation contract. |
| `docs/ADVANCED_LANGUAGE.md`, frontend cleanup lowering | Reached LIFO defers; managed one-shot deinit; synchronous events; callable discriminator plus managed environment | Persist cleanup state across suspension. Do not silently turn events, deinit or deferred code into async operations. |
| `src/runtime/native.cpp`, `tests/integration/native_fixture.c`, runtime callback tests | UTF-8 temporary ownership, trusted C calls, retained-copy fixtures, opaque-resource and callback proofs | These are internal bridges and tests, not a source FFI, package resolver or general resource ownership system. |

The coordinator permits several Managed participants and safely waits for them
before tracing. It does not make a sequence such as `object.field += 1` atomic.
Readonly views do not freeze mutable aliases. Compiler output uses direct field
loads/stores, while some collection operations have internal locks and multi-step
validation; neither establishes a source memory model. C++ runtime races also
remain subject to the [C++ memory-model rules](https://eel.is/c++draft/intro.races).

## 2. Async lowering and precise suspended roots

### Prefer a modest typed-IR transformation

After ordinary semantic checking and specialization, split an async function at
its suspension points into a start operation and a resumable CFG with a state
index. Emit ordinary LLVM functions through the existing lowerer. Persist locals,
cross-suspension expression temporaries, captures, cleanup head, pending exit and
awaited-task reference in a compiler-owned managed frame. Begin conservatively;
minimal spill liveness and allocation elision are later measured optimizations.

An expression such as `left() + await right()` must save the already evaluated
left operand and evaluate both calls once. Fixed values retain independent copy
semantics; references retain identity and readonly authority. Construction views,
prepared receivers and native borrows cannot survive suspension. Diagnose these
cases rather than storing a stack address in a task frame.

[LLVM's coroutine documentation](https://releases.llvm.org/19.1.0/docs/Coroutines.html)
and the exact `llvmorg-19.1.7` `Coroutines.rst` were inspected. LLVM supports
coroutine splitting, but its frame construction does not automatically provide
VOID's exact heap reference descriptors or cleanup/lease policy. Using intrinsics
would require integrating the post-split frame with those contracts in normal and
O2 builds. Explicit VOID frame fields reuse the existing descriptor/layout path.
Choose one approach; do not ship both. Fibers, copied stacks and a coroutine VM
add no necessary capability to this first design.

### Ownership while queued, running and suspended

Use one scheduler-owned active task record containing the existing Runtime owner,
an existing generation lease and a `Runtime::Handle` to the managed task/frame
graph. Managed task data holds typed arguments, frame, result and completion data.
Native scheduling state holds synchronization and code ownership; do not place a
`shared_ptr`, mutex or executable pointer inside traced user payloads or nominal
type metadata. This is scheduler bookkeeping, not a second collector.
Keep native parent/waiter links nonowning or explicitly scope-owned, avoiding
reference-count cycles that would root otherwise unreachable completed task graphs.

| State | Required ownership and transition |
| --- | --- |
| Creation | Allocate/initialize through rooted storage; acquire native owner and lease before publication. Failed allocation leaves no runnable task or orphan group child. |
| Ready | Scheduler ownership and persistent handle retain frame and captures. Queue membership alone is not a GC root. |
| Running | Attach to the same Runtime; enter Managed; establish ordinary stack Frame/Slot roots for the resume call. One task has at most one executing resume. |
| Suspended | Save initialized frame fields and continuation state, install the await dependency, then return and unlink stack frames. Persistent roots retain the saved graph. |
| Completion | Run applicable cleanup, publish result/status once, wake waiters, then release active scheduling ownership only after handoff is safe. |
| Retained result | An ordinary managed task handle traces the completed result. Remove scheduler roots when no active/group obligation needs them; completed task cycles must remain collectible. |

No saved `Runtime::Frame`, `RegisteredRootWord`, RuntimeFailure pointer, native
stack address or generation-local diagnostic pointer may survive its invocation.
Frame descriptors use existing reference-shape rules, including nested values,
interfaces and inactive variants. Runtime failure text must be copied while the
lease remains live, using an allocation-failure-safe fallback when necessary.

Use all initialized frame fields as roots initially, clearing references when the
activation ends. Cancellation cannot drop a frame that still owns reached cleanup.
GC does not execute task cleanup, native deinit or application finalizers.

## 3. Scheduler, await and source-visible task policy

### Recommended first scheduling contract, requiring approval

- An async call evaluates its receiver and arguments once, creates a task, and
  enqueues its first execution. It does not run the body inline before returning.
- Async activations run on their Runtime's owning executor thread initially.
  There is no migration or work stealing in the first implementation.
- Awaiting a completed task returns its stored result; awaiting an incomplete
  task registers a continuation and returns the worker to the scheduler. Permit
  repeated await: fixed/value results copy, managed/list results share identity.
- A task belongs to its nearest explicit task group or the current activation's
  implicit child scope. Dropping a handle does not detach it. No detached tasks
  or independent OS thread per task are proposed.
- A synchronous function cannot execute `await`. Source task creation requires
  an active scheduler scope. Add an explicit host/CLI root-task adapter, preserving
  the current synchronous entry ABI; the accepted async entry signature remains
  an owner choice. Bible `async main()` is not the current CLI contract.

The compiler needs an async effect in resolved function/callable signatures and
an internal task-result type. Declaring `async func f() -> T` still annotates the
eventual `T`, not a required user-written `task<T>`. Awaiting a no-result function
is a statement with internal no-result completion; introduce no public `void` or
unit-result syntax. Exact handle type spelling for fields/parameters and async
callable syntax must be approved. Do not silently reuse synchronous callable types.

### One ready queue and a bounded pool

Start with a mutex/condition-variable ready/completion queue and a fixed worker
pool associated with the scheduler. Workers run approved parallel CPU jobs or
trusted blocking native work; completions enqueue notifications to the owner
executor. The [libuv work/completion split](https://docs.libuv.org/en/v1.x/threadpool.html)
is an established example, not a recommendation to add libuv as a dependency.
Use current C++ threading facilities first; avoid an OS I/O reactor until needed.

Waiting threads must be NativeSafe or unattached. Release queue/status locks
before entering Managed, touching frame payloads, calling Runtime handle APIs,
compiling, invoking user code or retiring code. Never wait for a task while marked
Managed, and never make the collector acquire scheduler locks. A blocking call
leaves Managed through the existing Native guard with all references frozen.

Serialize waiter registration and application of worker completion on the owner
executor. Completion-before-registration is checked before suspension. Enqueue,
completion and cancellation need a single winning transition and an operation
identity so a late provider notification cannot resume a reused/finished task.
Never resume recursively from a provider or while a resume call is still active.
Prepare queue/waiter storage transactionally; do not allow allocation failure to
lose a completed job or strand an awaiter. Define overload/admission failures,
worker count and shutdown bounds during implementation, without a language-wide
task-count quota or latency guarantee.

### Parallel spawn needs an approved sharing boundary

Recommend an initially **value-only parallel source boundary**: noncapturing
named functions, scalar/bool/string arguments and results, plus recursively safe
value structs, fixed arrays and enum/result payloads. Copy fixed values with the
existing semantics; strings are immutable. Reject managed/list/dictionary/interface
arguments, shared closure cells, task handles, resource wrappers and readonly
aliases of mutable objects. Do not deep-copy references or relabel const as frozen.

Workers may create private managed data internally under the same Runtime/Heap,
but cannot publish it across that boundary in the first slice. Check the reachable
call effects: no hidden mutable module state or unclassified native/global effects.
If the current effect machinery cannot establish this for a call, reject it with
a source diagnostic. Do not infer safety from the entry's scalar signature alone.
Known worker-safe native bridges can be classified explicitly; unknown ones cannot.

This permits real parallel numeric/value CPU work without inventing a general
Send/Sync system or claiming shared-object safety. It intentionally does not yet
implement the Bible's list-returning pathfinding example. Approving this initial
restriction is a product decision, not an already agreed language rule. If the
owner requires arbitrary reference/closure spawn immediately, first choose and
specify a fuller isolation or synchronized-sharing model. The alternatives include
checked transfer/ownership or shared objects with defined synchronization and
race behavior; none follows from GC or deep readonly. [Rust's Send/Sync model](https://doc.rust-lang.org/book/ch16-04-extensible-concurrency-sync-and-send.html)
illustrates the extra contract required, not a model selected for VOID.

The eventual `sync.mutex`/atomic library needs explicit visibility/order semantics,
safe NativeSafe lock waits and guard authority. Reject ordinary mutex guards across
await. Do not add a global lock around all Managed code or turn unsynchronized
shared access into undocumented undefined behavior. General shared-reference jobs,
dedicated source threads and async locks remain separate approved expansions.

In this first worker slice, reject task creation, await and synchronous task joins
inside the worker's reachable call graph. Otherwise a one-worker pool can deadlock
on its own queued child. Owner-thread async tasks can create and await any number
of admitted independent jobs without blocking workers. Expanding worker reentry
or task-helping is later work, not a hidden scheduler requirement.

## 4. Cancellation, groups and cleanup

Recommend internal completion states **success, cancelled and runtime failure**.
`Err` is an ordinary successful result payload. `job.cancel()` is idempotent and
requests cancellation; it does not promise the task was stopped. Terminal outcomes
are stable. If completion wins before cancellation is observed, keep completion.
Awaiting a cancelled task takes a distinguished cancellation exit through cleanup,
not a fabricated `Err` or string-matched panic. Public inspection/recovery API and
whether user code may catch cancellation need approval.

Observe requests at async start, await and explicit/task-aware loop or call
checkpoints. Recommend loop checkpoints for spawned CPU source functions, including
reachable synchronous helpers. They must use checked propagation through existing
cleanup paths. Today's `vd_gc_poll()` returns void and is GC-only: do not make it
throw across generated code or secretly turn collection into cancellation. Extend
the internal checked status/IR as needed and invalidate the compiler ABI/cache
identity normally. Cancellation of blocking native work is provider-specific;
without a cancel operation, wait NativeSafe for real completion and then clean up.

Each activation owns its children. Explicit `tasks.group()` supplies a nested
scope; recommend one task-creation path for ordinary async calls, `spawn` and
`jobs.spawn`, with group ownership attached before enqueue. Evaluate `spawn f(args)`
arguments in the caller and execute `f` on a worker, rather than evaluating `f`
before scheduling. Distinguish this from the Bible's `jobs.spawn(async_reference)`
example; approve exact overload/signature rules instead of guessing them.

Recommended group exit: normal exit joins children; parent cancellation, runtime
failure or early exit cancels unfinished children and joins them. A child runtime
failure cancels siblings and wakes its parent's cancellation-aware wait, even if
the parent was awaiting a different child. Preserve the primary failure and attach
bounded additional failures using existing diagnostic policy. A normal child
`Err` does not automatically cancel siblings. Define how `return`, `break`, `?`
and an externally cancelled parent select the primary outcome in the approval.

Join by suspension in async code, including group scope exit. Recommend a
compiler-recognized task-group async exit for `with tasks.group()` only; ordinary
`with`/`deinit` remain synchronous. Deferred user code still cannot await. Group
draining is cancellation-shielded so repeated requests cannot skip cleanup. Drain
children before releasing resources their scope may borrow, then finish parent
cleanup. This ordering and the special async `with` behavior require explicit
approval; they are not provided by today's synchronous cleanup list. Python's
[task-group cancellation/join behavior](https://docs.python.org/3/library/asyncio-task.html#task-groups)
is a useful comparison, not VOID's specification.

Reject self-await/self-join and obvious ownership cycles. Scheduler code must
prevent duplicate execution and lost wakeups; it need not promise detection of
every application dependency deadlock or a deadline for uncooperative native code.

## 5. Reload, generation lifetime and persistent cache

Acquire the existing NativeCode/generation lease when a task is created, before
enqueue. Retain it through ready, suspension, cancellation cleanup, result/failure
handoff and pending native completions. Resume with that generation's native
helper and data layout. New invocations after compatible publication use the new
generation; old tasks stay entirely in the old one. Do not rewrite their state
index, discriminator catalog or closure environment during reload.

Version private async frame descriptors by generation content/function identity.
Two body versions can have different internal suspension layouts without changing
their public async signature. Keep existing public layout/constant/effect/capture
compatibility checks; only genuinely private activation layouts are generation
local. This classification must be explicit in analysis and metadata admission,
not achieved by deleting incompatible public metadata from comparisons. Changing
sync/async effect, public task-result/capture layout or native contract requires
restart. Old private frame schemas remain data-only in Runtime-owned metadata.

**Escaping callable values need a separate lifetime answer.** Existing callables
carry a generation-relative discriminator and environment, not an independently
leased native function pointer. Finishing a task is insufficient proof that its
returned closure or a callback stored in another object no longer needs that
generation. Recommend conservative retention of task-used generations by the
scheduler session until shutdown for the first broad async delivery. This reuses
existing leases and avoids GC finalizers or a new ownership registry. It costs
memory across repeated reloads and requires owner approval. The host session must
outlive every use of its task-derived callables; exporting them beyond that lifetime
is unsupported without an explicit owning host lease. Keep the session as an
external owner of Runtime and code leases, with a nonowning scheduler association;
do not put NativeCode owners inside Runtime and create a Runtime/code-owner cycle.
Close rejects further entries only after tasks/providers drain. If that retention is
unacceptable, restrict cross-task callable escape initially or design its exact
owner lifetime before implementation. Do not claim last-task retirement solves it.
Synchronous-only paths retain their existing retirement behavior.

Compile off the scheduler workers through the existing serialized compiler host;
LLJIT concurrency is not permission to race Jit's cache/capture/serial state.
Publish through current revision/compatibility acceptance. Keep source isolation,
explicit native-import precedence, LLVM platform/process helper links and raw-IR
linking. Cached native objects can include resume helpers as ordinary generation
symbols. Never persist tasks, heap frames, scheduler state, handles or executable
addresses. New compiler/runtime bytes change the existing content build identity.
Native bindings without a complete persistent contract continue bypassing cache.
AOT emits the same transformed IR and scheduler Runtime, without an ORC dependency.

Shutdown order: stop task admission, request structured cancellation, continue
running required cleanup/completions, unregister and quiesce providers, join
workers NativeSafe/unattached, release task handles and code/library leases,
detach, then close/destroy Runtime. Do not call today's `Runtime::close()` first:
it rejects new Managed entries and can prevent scheduled cleanup from starting.
No worker termination or destructor waiting under the GC/publication lock.

## 6. Native libraries as a separate bounded delivery

Recommend one project-local, explicitly trusted binding fixture before a package
system. Expose ordinary `.void` wrappers backed by typed, compiled C/C++ bridges.
Use Clang's target ABI for bridge compilation and normal LLVM scalar calls; keep
legacy `i32(i32)` proof imports intact. Neither that import list nor `Jit::bind`
is already a general typed source FFI. No libffi, arbitrary C-header parser or
application-facing pointer syntax is needed for this first delivery.

| Required contract | Recommended initial boundary |
| --- | --- |
| Types/ABI | Exact-width scalar values and explicit conversions; opaque binding-private handles. Reject variadics, C++ exceptions/objects, public managed aggregates and unverified struct-by-value ABI. |
| Strings | One public UTF-8 string. Copy to owned terminated native storage for borrowed calls; reject embedded NUL only when the C API requires it. Retention/returned buffers specify their owner, length and exact release function. No pointer into a suspended caller's temporary storage. |
| Resources | Unforgeable private handle state, transactional result-returning factory and existing managed one-shot deinit. GC does not close native resources. Resource-owning value copies remain unsupported. |
| Calls | Classify blocking, thread affinity, callback/reentry and whether the worker may call the provider concurrently. NativeSafe code cannot dereference managed objects; copy/retain through approved handles first. |
| Callbacks | Stable compiled typed trampoline, existing CallbackRegistration, persistent captured roots, NativeSafe/foreign attachment and binding-defined ABI failure result. Keep providers without unregister-and-quiesce unsupported. |
| Libraries | Static fixture first. For explicit dynamic loading, one owning library lease must outlive functions, resources, callbacks and dispatched operations. Prefer pinning until host shutdown initially, with calls/load/unload outside Runtime/queue locks. |
| Errors/cache | Validate missing symbols, architecture/ABI and library identity before publishing wrappers. Current custom-binding cache bypass remains. Do not assume a DLL filename identifies its ABI or persist a bound address. |

The precise binding declaration/metadata format is still open. Recommend explicit
project-local declarations and reviewed compiled adapters, rather than finalizing
the illustrative Bible `native` grammar or adding package discovery incidentally.
Ordinary numeric fields must not become forgeable native pointers. Any new hidden
handle representation belongs to trusted binding semantics and needs approval.

Existing one-shot `deinit` prevents duplicate release; it does not by itself prevent
use after close or a call racing with native release. The first resource fixture
should allow synchronous operations on its owning executor only, check closed
state on every operation and hold the native owner through each call. Async native
operations that borrow a resource need an explicit drain/close contract before
inclusion. Do not make synchronous deinit secretly await, reorder arbitrary user
defers, or assume task-group ownership proves every nested resource borrow safe.

GUI/SDL-like callbacks may require synchronous results and a specific thread.
Do not silently queue every foreign callback to the owner executor or allow it
to access shared managed state from a foreign thread. Marshal only where the
provider permits it; otherwise retain the current trusted host-only callback
boundary until the source thread-affinity/sharing contract is approved.

Reuse provider barriers for asynchronous native operations: cancellation requests
are not evidence that the provider stopped using its buffers/context. Keep library,
registration and copied-buffer owners until actual completion/quiescence. Native
failure cannot unwind through generated code. Perform Windows DLL work outside
`DllMain`/loader-lock callbacks, following [Microsoft's loader guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).

## 7. Recommended implementation sequence after approval

These are coherent deliveries, not a new sequence of mandatory research certificates.
Resolve the relevant semantic rows below first; implement only the owner's selected
sprint. Focused tests while building, one final consolidated acceptance per delivery.

| Delivery | Implement together | Focused evidence required |
| --- | --- | --- |
| 1. Native async activation | Async signatures/type checking, typed-IR split, traced frame, owning executor, await, explicit root adapter and leased resume | Real source normal/O2 JIT and linked AOT; await before/after completion; expression ordering; nested/value/managed captures; GC during suspension; allocation rollback; no-result task diagnostics. |
| 2. Structured task lifecycle | Implicit/explicit groups, cancel propagation/checkpoints, async group exit, cleanup order, draining shutdown and owned failures | Parent/child failure and cancellation races; Err distinction; multiple/repeated await; child cleanup before resource release; self-join rejection; late completion; pool size one; no worker blocked for async join. |
| 3. Bounded parallel source spawn | Fixed worker pool, approved transferable value shapes and transitive effects, NativeSafe waiting and precise task roots | CPU overlap/barriers for independent jobs; negative shared-reference/capture and worker task-creation cases; nested owner groups and pool-one saturation; cancellation in CPU loops; GC rendezvous with workers; failure cleanup and source diagnostics. |
| 4. Bounded native library | One real trusted project-local binding, typed scalar/string/opaque adapters, ownership/failure paths and callback barrier | Same wrapper behavior in JIT/AOT across targets; NUL/length/encoding; retained and returned buffers; failure acquisition/deinit; callback-before-admission close; thread-affinity rejection; missing symbol/ABI mismatch. |
| 5. Integrated Stage 6 acceptance | Combined async/spawn/native lifecycle, reload and cache compatibility, source tooling snapshots and documented boundaries | Suspended old/new generations and private frame layouts; queued/running/callback leases; repeated reload memory; cold/hit/corrupt fallback; CI #70 helper regression; full unchanged suites plus additive tests, strict sanitizers and existing small-stack/editor checks. |

Deliveries 1–2 must not be reported as complete parallel source concurrency;
delivery 3 is a bounded first spawn contract. General shared-reference jobs,
thread-safe resource ecosystems, package installation, full networking/reactor
support, work stealing, async locks, hard deadlines and new GC tuning are not
implicitly authorized by this plan. Existing regressions remain mandatory. Host
TSan can check scheduler/coordinator/bridge code but does not automatically
instrument generated JIT application instructions; use deterministic observable
handoff/race regressions as well. Hosted acceptance remains owner-controlled.

## 8. D1–D6 design decisions and current approval

| Decision group | Approved/restricted boundary | Still requires owner review |
| --- | --- | --- |
| D1. First parallel sharing contract | One owning executor for Deliveries 1–2 | Worker pool, transfer restrictions and transitive effects in Delivery 3; general shared references remain unapproved |
| D2. Call start and task handles | Enqueue without inline body execution; inferred task handles; completed/repeated await; distinct cancellation outcome | Public task type spelling, async callable values and other deferred calling forms |
| D3. Task ownership and entry | Creating activation/nearest lexical group; no detach; `with tasks.group() as jobs:`; explicit root adapter and sync CLI ABI | Async CLI, detached tasks and public first-class groups |
| D4. Cancellation and group exits | Assigned Delivery 2 rules approved: idempotent request, checkpoints, early-exit cancel/join, shielded drain, parent-primary/creation-order failures, child-before-parent cleanup; Err is ordinary completion | Cancellation inspection/recovery; general await in cleanup remains unsupported |
| D5. Reload lifetime of escaping callables | Drained invocation retains its native generation lease; foreign-generation enqueue rejected; new cross-task callable escapes restricted | Async value receivers, virtual/interface dispatch and async function values; broader escaping-callable ownership. No session-wide generation pinning approved |
| D6. Native scope and surface | Separate bounded Delivery 4; existing trusted host ABI preserved | Native declaration/ABI, opaque authority, ownership, callback affinity/quiescence, dynamic loading |

These six groups are design decisions, not the five implementation deliveries.
Only the owner's assigned Delivery 1/2 boundaries are approved; the remaining
architecture recommendations do not authorize Deliveries 3–5.

## 9. Main risks and acceptance limits

| Risk | Required control |
| --- | --- |
| Unrooted suspended values or stale stack pointers | Heap frame, exact descriptors, active Handle roots and transactional suspension/publication. |
| Shared source memory races | Approved spawn transfer/effect restrictions; no implication that coordinator locks protect field accesses. |
| Queue/GC/callback deadlock | NativeSafe waits; no Runtime/user/provider/ORC calls while scheduler locks are held; explicit shutdown sequencing. |
| Cancellation loses cleanup or strands child | Separate request/outcome, checked cleanup edges and cancellation-shielded structured drain. |
| Reload unloads resumed/captured code | Per-task leases, private versioned frames and explicit escaping-callable retention policy. |
| Session retention grows across reloads | Document and measure retained generations; do not claim prompt reclamation under D5. |
| Native provider outlives context or library | Actual unregister-and-quiesce barrier, owned copies and library leases through late completion. |
| New ABI silently reuses old object | Actual compiler/build identity and unchanged current-source validation before hits. |

No Stage 6 performance, fairness bound, race-safety implementation or cross-platform
execution result is claimed. This investigation changes documentation only.
