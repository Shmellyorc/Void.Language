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

TCP, UDP and DNS submit bounded native requests to one lazily acquired
socket provider per Runtime. The provider holds native descriptors, c-ares
channels, byte snapshots and external completion tickets. Socket readiness
uses the provider thread, leaving the source worker pool available for
file operations and source workers. Synchronous calls wait in NativeSafe
state. Async calls complete on the owning executor, which publishes managed
results and ledger entries. Provisional sockets and reserved buffers remain
owned until publication, commit or abandonment.

A source cancellation signals the provider and drains its native access.
An admitted TCP data cancellation closes the stream. An unpublished connect
or accept result closes its acquired socket. UDP cancellation can leave one
datagram effect, and DNS cancellation destroys the individual channel.
Runtime shutdown signals the service before waiting for admitted native
work, then joins it before releasing the shared HTTP provider lease.
[Socket architecture](net/TCP_UDP_HANDOFF.md#batch-4-contract-review)

On supported hosts, `process.capture_async` submits one native worker job.
It receives copied launch inputs and owns the process and pipe readers until
native cleanup finishes. The owner can resume other activations during launch,
drain and reap. Scope and Runtime shutdown request cancellation and drain the
worker before releasing its completion state. The worker holds no managed
output references. The owner materializes and roots the two byte lists only
when publishing the completion. Task cancellation discards the native reply
and propagates the existing cancellation outcome.
[Process capture contract](process/API.md)

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

## G10 monotonic timers

Batch 11 starts from `e83c1070e5d2f6b49a2b84c52e21fbf2e6e319b1`.
The existing Runtime owns one async executor per active root. Before G10,
`time` could read a monotonic clock and measure elapsed time. G07 socket
deadlines belonged to the socket provider, and G04 process capture used
the bounded native-worker pool to drain and reap a child. Those facilities
retain their existing contracts.

`time.sleep(delay: time.duration) -> result<bool, time.error>` is async.
Use `time.make_duration(0, 100000000)` for 100 ms. The duration has signed
whole seconds and normalized fractional nanoseconds. Sleep accepts zero
through 24 hours inclusive. Zero completes at the first timer checkpoint
without a timed wait or queue slot. The async call itself follows ordinary
task scheduling. Success yields `Ok(true)`. Invalid durations, full timer
capacity and unrepresentable deadlines yield `Err` with operation `sleep`.
The Runtime checks the bounds before nanosecond conversion and checks the
addition to the monotonic sample. If the native clock has coarser ticks, the
deadline rounds up. A ready task may resume later under cooperative scheduling.
Allocation and Runtime admission failures use the checked Runtime failure
path. [Temporal API](../stdlib/time.void) · [Steady clock contract](https://eel.is/c++draft/time.clock.steady)

### Ownership and cancellation

The compiler admits the private timer operation only in the standard-library
`time` module. Typed IR validates the async effect, operands and scalar task
payload. LLVM supplies the current generation and a rooted output to the
same-build Runtime ABI. The owning executor uses the existing external native
completion path for each timer and accounts for the job until retirement.

The Runtime ledger owns one bounded queue of up to 256 pending positive
deadlines. Entries hold a monotonic time point and a native completion ticket.
The owner expires due entries at its normal receive checkpoints. When it has
no runnable activation, it waits for native work or the earliest deadline on
the existing condition variable. There is no timer thread or timer work in
the worker pool. Equal deadlines and deadlines observed in the same receive
pass have no source ordering guarantee. Publication still occurs on the
owner. [Runtime design](net/http/RUNTIME_FOUNDATION.md#g10-timer-participant)

Cancellation before entry runs no sleep body. Cancellation after admission
removes the pending entry and settles its ticket. If expiry has already
settled the ticket, source cancellation can still win before owner
publication. A completed task preserves its outcome on later cancellation and
repeated await. A cancelled sleep propagates the existing task cancellation
outcome. Group exit, early return and parent cancellation use the existing
join, drain and reached defer rules. Runtime close rejects new admission and
stops the queue before waiting on native job accounting. Stop cancels and
wakes pending timers. The owner drains their tickets and frames through the
existing scheduler. Timer deadlines have no authority to cancel arbitrary
external effects.

### Application and coordination scope

The [turn example](../examples/async/turn_timer.void) starts a shot windup and
an idle timer. A bounded worker calculates an aim adjustment while the turn
is active. The application awaits the shot and cancels the remaining idle
timer before its lexical group drains. Run it through the explicit async
adapter with `void_io_runner --async-one-worker examples/async/turn_timer.void`.
The normal CLI entry contract is unchanged. The focused tests also link
normal and optimized standalone executables using `emit_async_object` and the
pinned Clang and LLD.

General deadline composition is deferred. The application can explicitly
cancel a pending child and drain its scope. Selecting a winner between
arbitrary task results would require an agreed observation order, result type,
loser cancellation policy and external-effect contract. The turn uses
bounded work and two owned timers. A general async queue with backpressure
is deferred until an application needs its send, receive and closure semantics.

### Focused G10 qualification

The Linux host is PikaOS 4 x64 with kernel `7.2.11-pikaos`. The native
Windows guest reported Windows 10 Home x64, build `10.0.19045.6466`.
Both used the pinned LLVM, Clang and LLD 23.1.4. The Windows baseline
export and source overlay were checked by SHA-256. Debug and Release
used their matching provider prefixes and owner-only NTFS temporary
roots. The configure-only shim delegated to the byte-identical host
verifier at its attested root. [Attested verifier setup](projects/SLICE7_FINAL_CLOSEOUT.md#qualification-scope-and-preserved-platform-boundaries)

| Control | Evidence |
| --- | --- |
| Native duration and queue | Zero, signed/fractional limits, exact 24-hour boundary, checked deadline overflow, the 256-slot limit and slot reuse. Injected monotonic samples prove separated and simultaneous deadlines, no early expiry and cancellation just before expiry. Retained tickets reject duplicate completion. |
| Owner and shutdown | Cancellation immediately after admission and after native expiry but before publication. A published result survives later cancellation. An ordinary native job completes with one worker while a long timer remains pending. After 1,024 immediate completions, collection leaves just the root and its frame until return. Runtime close from another thread wakes an admitted timer and the root observes cancellation. |
| Public source | Real elapsed time for positive waits, pre-start and parent cancellation, normal join, early return, reached defers, repeated await, checked capacity and post-drain reuse. The turn example runs useful worker work, awaits expiration and cancels the idle timer. Exact source diagnostics cover synchronous await, deferred await, wrong duration type and attempts to call the private primitive. |
| Interoperability | Real loopback TCP accept/read/write and UDP receive/send while a long timer is pending. A short timer expires while the TCP and UDP requests are outstanding. The unchanged G07 and G04 suites exercise their cancellation, provider retirement and native compatibility. |
| Code generation | Each public timer fixture and turn example runs through one-worker normal and O2 JIT, and normal and O2 standalone AOT. The harness emits the async-root object, then invokes the pinned Clang driver independently with the configured Runtime link manifest. On Windows it checks the actual driver plan selects the pinned `lld-link.exe`. |

[Native controls](../tests/async/timer.cpp) · [Public controls](../tests/async/timers.void) · [TCP and UDP fixture](../tests/async/timer_network.void) · [JIT and link driver](../tests/async/timer.py)

Configure the corresponding pinned build before invoking the new Make targets.
Use a private temporary root for the tests. On Windows use the x64 developer
environment, physical pinned Clang path and the staging probe's
`--create-private-root` verification before setting `TMP` and `TEMP`.
Use `RelWithDebInfo` and `VOID_SANITIZERS=ON` for the strict Linux profile.

```sh
cmake --build BUILD --parallel 3 --target voidc void_io_runner void_async_timer_tests \
  void_async_tests void_async_external_tests void_native_workers_tests void_runtime_tests \
  void_network_async_aot void_network_native void_process_capture_async_aot \
  void_process_capture_probe void_host_portable void_temporal void_temporal_ir
selector='^(async_timer_native|async_timer_source|async_timer_network|async_source|async_ir|async_parallel|async_lifecycle|async_external|native_workers|runtime_coordination|process_capture|net_sockets|format_temporal|stage10_temporal_ir|documentation_execution)$'
ctest --test-dir BUILD -R "$selector" --show-only=json-v1
ctest --test-dir BUILD -j 2 --output-on-failure -R "$selector"
```

For strict Linux, export
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86` before
building and running the selection. The new CTest timeouts are 60 seconds
for native controls and 180 seconds each for source and network controls.
The Python harness bounds each subprocess at 45 seconds.

All five profiles had the same 15 active CTests with no skip properties.

| Native host and profile | Final result | Execution record |
| --- | --- | --- |
| PikaOS Debug | **15/15** | The first selection passed 14/15 in 122.77 s. The three final timer tests passed in 20.49 s. |
| PikaOS Release | **15/15** | The selection passed in 32.86 s. The three final timer tests passed in 9.44 s after the portable path correction. |
| PikaOS strict ASan/UBSan with leak detection | **15/15** | The final selection passed in 100.05 s. |
| Windows 10 Debug | **15/15** | All 12 adjacent tests passed in the original 184.14 s selection. The three corrected timer tests passed in 44.47 s. |
| Windows 10 Release | **15/15** | The rebuilt selection passed in 50.55 s. |

### Qualification corrections and remaining scope

- Initial source probes found the existing synchronous CLI boundary.
  An async `main` produced `VOID4001`. Running an async `entry` with the
  normal CLI returned `Async entry requires the explicit scheduler root adapter`.
  The maintained fixture uses the explicit adapter. Source locals that
  collided with the imported names `elapsed`, `listener` and `datagram` were
  renamed within the fixtures.
- The first Linux builds with new Make targets reported
  `No rule to make target 'void_async_timer_tests'` during regeneration.
  The build succeeded with the regenerated Makefiles.
- The first Linux and Windows Debug native witness returned
  `Async scheduler invariant failed`. The test repeated its pending-timer
  assertion when the root resumed from its own drain. The witness now
  checks progress before cancellation and resumes the drain at its saved
  stage.
- Windows Debug's two new source checks initially failed object emission
  with `cannot write object: invalid argument`. The baseline Release
  and candidate Debug development adapters both emitted
  `ascii control.obj` successfully and returned that error for
  `unicode 雪.obj`, using the unchanged async activation example.
  The existing adapter takes narrow Windows arguments. The timer harness
  now uses an ASCII path with spaces. Unicode paths through that
  development adapter remain outside this qualification.
- The first Windows Release incremental build reused a baseline frontend
  object because the overlay's archived timestamps predated that object.
  `stage10_temporal_ir` and the new source checks then reported
  `VOID2003` for the private `_sleep` call. The corrected run verifies
  the source hashes, refreshes the overlay timestamps and rebuilds the
  selected targets.

G10 native macOS execution is unqualified. The G08 optimization and Windows
persistent-cache holds remain in force. The existing G06 Windows symlink
privilege limitation (error 1314) remains recorded. G10 stays **IN PROGRESS**
for independent review. [Milestone register](roadmap/MILESTONE_REGISTER.md)

The Windows scratch source and builds, host transfer archives, temporary
applications and raw logs were removed. PikaOS retained its three existing
configured builds. Available bytes immediately before and after cleanup were
16,369,459,200 and 16,588,279,808 on Linux temporary storage, 367,688,339,456
and 367,688,343,552 on the Linux share filesystem, and 12,311,003,136 and
15,008,583,680 on Windows. The Markdown record retains the failures and replay
commands. [Retention policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)

### G10 changed-file inventory

| Role | Paths |
| --- | --- |
| Build registration | `CMakeLists.txt` |
| Checked source and lowering | `include/void/ir.hpp`, `src/frontend/compiler.cpp`, `src/ir/module.cpp`, `src/ir/async_validation.inc`, `src/backend/llvm/backend.cpp`, `stdlib/time.void` |
| Runtime | `include/void/async.hpp`, `src/runtime/async.cpp`, `src/runtime/timer.hpp`, `src/runtime/timer.cpp` |
| Runnable example and qualification | `examples/async/turn_timer.void`, `tests/async/timer.cpp`, `tests/async/timer.py`, `tests/async/timers.void`, `tests/async/timer_network.void` |
| Documentation | `docs/ASYNC.md`, `docs/net/http/RUNTIME_FOUNDATION.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `docs/roadmap/MILESTONE_REGISTER.md`, `docs/stage10/DISPLAY_FORMATTING.md`, `stdlib/README.md` |

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
