# Phase 15 runtime foundation

## Stage 11 global roots

Runtime globals use complete typed managed cells and this Runtime's existing
persistent Handles, Frame/Slot maps and precise Heap. The Runtime registers null
handles before initialization and publishes complete cells in deterministic order.
No global contains a generation-owned code address. Failed startup drains native
providers before dropping handles; Runtime close preserves that same ordering.
Scalars and reference-bearing globals share the same coordinated path. The
[global contract](RUNTIME_GLOBALS.md) and [evidence](stage11/IMPLEMENTATION.md)
record lifetime, application isolation and acceptance separately.


Stage 8 Sprint 5 adds host memory snapshots of the existing Heap payload, buckets,
Runtime catalog and Handle registries. Completed execution-frame roots now clear
only after native resume returns or acquired worker publication; ordinary precise
GC performs reclamation. Task/result records, active frames, cleanup, callbacks
and native code leases retain their prior safety contracts. See
[the ownership proof and measured limits](STAGE8_ASYNC_RETENTION.md).


Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Delivery 5 combined regressions exercise this same Runtime through suspended
parents, queued/running workers, retained native callback roots, foreign callback
collection and provider shutdown. Existing Frame/Handle/schema ownership and
NativeSafe admission remain unchanged. Deterministic barriers hold both worker
storage and a pre-admission callback during shutdown; explicit drain/retirement
checks and allocation sweeps verify release ordering. Repeated compatible roots
collect back to zero managed objects after explicit native release. This does not
measure RSS, reclaim all Runtime metadata or change root-duration retention.
See ASYNC.md, NATIVE_LIBRARIES.md and WORK_LOG.md for exact integrated evidence.

Stage 6 Delivery 4 adds Runtime-owned native registry entries containing ordinary
Handle roots, owned retained bytes and CallbackRegistration state. Native C pointers
never live in managed payloads; GC never finalizes C resources. Explicit `with`/
`deinit`/close or Runtime shutdown performs provider unregister and quiescence,
including dispatches reserved before admission, before releasing roots/code leases.
No provider/registry lock is held while executing source or retiring code. The
existing precise collector and NativeSafe/Managed admission remain the only memory
coordination mechanism. See [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md).

Phase 20 extends existing heap layouts with runtime-owned immutable nominal descriptors and transactional NativeSafe admission. Generation-owned guarded dispatch uses the same precise roots and collector; objects never retain JIT code pointers. See [INHERITANCE.md](INHERITANCE.md) for the current boundary and WORK_LOG.md for actual verification.

Phase 16 integration: [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md) describes the now
implemented managed-source boundary. Earlier phase-only exclusions below remain
historical. Phase 17 [managed initialization](MANAGED_INITIALIZATION.md) uses
compiler-checked staged field roots and the existing complete-object commit; the
runtime implementation is unchanged. Phase 18 source values now carry managed
references using this same Frame/Slot/descriptor runtime; see REFERENCE_VALUE_STRUCTS.md.

Stage 6 Delivery 1 uses this same Runtime for precisely traced heap activation
frames and task results. Native task records hold ordinary persistent handles
through complete root draining; frame maps contain data offsets, never code
owners. Resume scratch roots leave before suspension. The enclosing NativeCode
lease covers suspended native resumptions and retirement; see [ASYNC.md](ASYNC.md)
and WORK_LOG.md for the exact local acceptance and restrictions. Delivery 2 uses
the same handles while groups join and cancellation drains. Children finish
cleanup before parent resource release; no scheduler wait blocks the owning
executor and no code address enters Runtime metadata.

Delivery 3 reuses this coordinator for bounded parallel source jobs. Workers attach
and enter Managed once, then wait NativeSafe when idle. Their private frames and
results retain the same persistent Handle roots; a release/acquire handoff lets
only the owning executor publish outcome/status and update awaiting parent frames.
The owner runs ready activations and waits NativeSafe for notifications only when
none are runnable. Queue locks never span Runtime/GC admission or user code.
Cancellation checkpoints are explicit checked operations, separate from GC polling;
worker storage and the enclosing code lease survive through actual completion and
shielded cleanup. Transfer restrictions forbid arbitrary mutable shared graphs.
Delivery 3 passed 120/120 local suites in Debug, Release and strict sanitizers;
hosted verification remains owner-controlled.

## Historical Phase 15–23 foundation context

This implemented the owner-approved G1–G6 in `GC_RUNTIME_PROPOSAL.md`. It is an
internal runtime/compiler foundation, not source-level managed `struct` support
or a production managed runtime. Existing source syntax, entry behavior, value
initialization, LLVM 19.1.7 and code-generation leases remain in place.

Phase 23 Stage 2D adds runtime-only reference-bearing private buffers, following
[ARRAYS_PROPOSAL.md Section 27](ARRAYS_PROPOSAL.md#27-stage-2d-reference-bearing-private-buffer-preparation).
The existing Heap streams immutable capacity through admitted schema reference
events. Ordered builder writes use registered Frame words, revalidate after polls
and commit interface pairs atomically. Private null slots remain trace-safe;
required live completeness and zero spare reference positions are distinct checks.
Mark rollback and ordinary/string legacy maps remain in place. Catalog snapshots
retain data-only type/witness ownership; Frame/Slot ABI is unchanged. This does not
activate source native arrays, copying/read leases or another collector. See the
proposal and WORK_LOG.md for actual local validation and performance limits.

Stage 2E is locked at `3cbe250` / owner-confirmed CI #56. It adds rooted RAII
read leases and independent physical-value copying; these freeze buffer value
bytes, not the contents of shared referents. Stage 2F local acceptance, mixed-heap
alias-guard scaling and pending hosted verification are recorded in
[ARRAYS_PROPOSAL.md Section 29](ARRAYS_PROPOSAL.md#29-stage-2f-final-runtime-foundation-acceptance-and-review).
Source arrays, owned-fixed/growable child copying and later stages remain gated.

## Stage 3 collection integration

Real fixed/growable arrays, strings and native ordered dictionaries now use this
same Heap/coordinator/Frame/Slot runtime. Dictionary headers trace their ordered
entry and scalar index buffers; entry schemas trace nested keys/values, including
cycles and nullable/interface references. Growth stages existing BufferBuilder
storage and revalidates before publication; clear/removal vacate traced entries.
Data-only Runtime metadata survives code retirement without executable callbacks.
See DICTIONARIES.md and the latest WORK_LOG entry for actual validation. Earlier
source-nonactivation exclusions above describe historical research only. Stage 3
acceptance does not require concurrent collection or a new certification framework.

## Current ownership and thread states

`Runtime` owns one existing `Heap`, its participant registry, handles and one
coordination mutex/condition variable. Both owners are noncopyable. There is no
collector thread, allocator cache, second tracer or forced thread suspension.

`Attachment` registers a stable stack-owned thread record in TLS. An outer
attachment starts NativeSafe; nested attachments reuse its record. Cross-runtime
nesting is rejected. `Managed` enters after the current request completes;
`Native` freezes the complete frame chain and temporarily leaves Managed state.
Both guards restore their previous state. Outer detach requires balanced scopes
and an empty frame chain, and waits for collection to finish before removing its
record. NativeSafe code may manipulate opaque handles but cannot dereference
managed objects, mutate frozen storage or call Managed-only operations.

Host `NativeCode::invoke` establishes the attachment/Managed scope when explicitly
compiled with a runtime owner. Host dispatch and diagnostic copying run NativeSafe,
including nested callback dispatch. Runtime-free proof/source invocations retain
their existing ABI and default behavior; when invoked within an attached Managed
host, the host boundary temporarily becomes NativeSafe. Numeric-only functions
compiled with an explicit runtime receive the same cooperative polls as managed
internal IR. No public CLI bootstrap or source entry-point rule has changed.

## Rendezvous and lock invariants

The first requesting Managed thread becomes the elected collector, marks itself
Parked and increments the epoch under the coordinator mutex. Other requesters
join that epoch and park; they never wait for collection while recorded Managed.
Polls park only after acquiring the same mutex. Condition-variable waits release
it. The collector traces/sweeps with that mutex held only when every participant
is Parked or NativeSafe. It never calls application code, a native provider,
callback, ORC or reload publication while holding that mutex.

Frame push/pop is allocation-free owner-thread work with no lock or safepoint.
Frame and storage writes become visible before the owner transitions to a frozen
state under the mutex. Allocation validates a registered output root, polls,
then locks only for allocation/publication; no safepoint occurs while it holds
the mutex. Reference stores validate the existing heap's whole-object/field
contract. Handle operations and snapshots use the mutex; none parks while holding
it. A collector cannot hold it for tracing while an existing Managed participant
is trying to acquire it: that participant has not yet acknowledged the rendezvous.

Completion, timeout and allocation failure restore idle and notify all waiters.
A waiter resumes only while the CURRENT request is idle under the mutex. If a
new epoch wins the lock first, it stays frozen through that epoch too. A losing
collection requester retains its original epoch's outcome even across later
epochs. The elected attempt owns the rendezvous deadline; joining requests share
that attempt's deadline and outcome. `completed_epoch` and `cooperative_parks` are diagnostic counters, not
object/code reclamation epochs.

An optional C++ rendezvous deadline aborts before root enumeration, marking or
sweeping. It never excludes an uncooperative Managed participant. Generated
collection currently uses no fixed deadline. Root enumeration/worklist allocation
failure returns a checked failure; the reused heap rolls back partial marks before
any sweep. Invalid precise references and out-of-order frame removal terminate:
these are invariant corruption, not recoverable source errors. Checked host misuse
throws before changing the relevant state. No native unwind crosses generated code.

## Roots and shared LLVM lowering

Private IR adds nominal `managed_ref`, allocation, typed managed field operations,
collection, polling and architectural reachability inspection. The existing IR
validator supplies instruction result types to the existing lowerer; there is no
parallel checker or code generator. Uncoordinated modules reject managed types
and operations. Source parsing/analysis still exposes no managed reference type.

Each coordinated function publishes a fixed stack root frame. Entries describe
actual local/receiver storage, reference-containing parameters and fixed storage
for every reference-containing instruction result. The lowerer recursively flattens
reference offsets in nested inline aggregates using the module-owned target
`DataLayout`. This preserves the Phase 14 layout-cache correction. Root storage
is zeroed before publication; parameters are seeded before the first poll. A
receiver roots the actual mutable place, not an obsolete copy of its fields.

Opaque runtime calls retain escaped storage through optimization. There are no
misleading pure/readonly/argmem attributes, blanket volatile stores, early lifetime
ends, optimizer disabling, stack scanning or parallel statepoint implementation.
Entry/block polls cover loops and continue paths, recursion and numeric-only
coordinated callees. Allocation publishes directly into a registered root. Return
unlink/return/caller root store form a no-GC handoff; arguments stay rooted while
callee parameter roots are seeded. All checked failure returns unlink the frame.
Fixed expression slots are reused on loop iterations and may retain dead values
until frame exit; destruction timing and minimal retention are not promised.

Runtime tracing reads pointer leaves with `memcpy`. Object descriptors copy size
and numeric offsets into the existing heap and outlive allocating LLVM contexts
and code generations. The private same-build frame ABI checks target slot size
and offsets against C++ before lowering. This defines no external aggregate C ABI.

The JIT retains the explicit runtime owner in `NativeCode`, alongside the existing
whole-generation lease. Runtime identity and conservative nominal layout records
participate in internal reload compatibility. GC does not inspect or retire ORC
generations. Active native/callback calls retain their acquired generation lease.

`emit_runtime_object` uses the same lowerer without a CLI adapter. Tests link its
normal/O2 objects into C++ harnesses through CMake with `void_runtime` and Threads;
these executables have no LLVM/ORC link dependency. The public object/CLI adapter
rejects coordinated IR without that bootstrap rather than calling it unattached.

## Native callbacks and shutdown

`CallbackRegistration` requires a binding-owned `CallbackProvider` with an explicit
`unregister_and_quiesce()` barrier. This MUST cover all dispatched trampoline users,
including calls paused before admission. An arbitrary native provider without this
guarantee is unsupported; an in-flight counter cannot make it safe. The provider
and runtime outlive registration. Registration is destroyed only after explicit
successful close. The binding owns its stable trampoline and context.

Admission checks registration/runtime closing under their locks, then increments
the in-flight count before releasing them. Admitted calls may finish attaching
even if runtime closing begins afterward. New admissions cannot. A callback uses
automatic same-runtime/foreign attachment and a Managed scope; same-thread native
reentry preserves the frozen outer frames. Cross-runtime nesting and callback entry
from Managed instead of NativeSafe are rejected. Exceptions become `run()` false
after cleanup; the binding defines the native ABI's actual failure result.

Close rejects self-drain without changing registration, closes admission, calls
the provider outside both locks, then drains all admitted calls before permitting
context/handle destruction. Concurrent close callers wait for the same completion.
The fixture provider owns and joins every dispatched native thread, so its barrier
is demonstrated rather than assumed.

Shutdown is explicit: mark runtime closing, close/quiesce providers, let admitted
callbacks/entries finish, join workers NativeSafe or unattached, release persistent
handles, detach, then destroy the runtime. Existing participants may poll and finish
while closing. Destruction with live threads, registrations, handles or a collection
is fatal. There is no forced cancellation, finalizer, scheduler or arbitrary native
borrow contract.

## Verification and remaining scope

Seven additional CTest suites preserve the original 24: `runtime_coordination`,
`runtime_generated`, `runtime_callbacks`, `runtime_failures`,
`runtime_aot_normal`, `runtime_aot_optimized`, `runtime_invariants`.

Tests cover deterministic state rejection, nested/frozen roots, timeout without
sweep, losing requester recovery, competing epochs, NativeSafe blocking, three
allocating mutators, fault injection during enumeration/partial marking, fatal
invariant child processes, provider dispatch-before-admission, self-drain, shutdown,
foreign/same-thread callbacks, capture handles, reload leases and ordinary checked
failure cleanup. Shared normal/O2 JIT and separately linked AOT proofs cover nested
aggregates, temporaries, argument/return handoffs, repeated loop collections and
concurrent AOT callers. Numeric-only optimized JIT loops must demonstrably park.
Their test executable instruments the existing runtime ABI poll entry, gates a
generated backedge poll in Managed state until a collection request is pending,
then forwards to the real poll. Normal/O2 runs check every loop poll, successful
collection, actual parking and balanced thread cleanup. This avoids assuming a
finite worker still runs after a NativeSafe signal. The gate is outside runtime
locks and is absent from production and separately linked AOT executables.
See `WORK_LOG.md` for actual configuration/test results and commands.

Still deferred: managed source syntax/types and initialization, general native
binding generation, providers without quiescence, public managed entry ABI, LTO
auditing, statepoints/stack maps, tighter liveness, allocator throughput, automatic
allocation thresholds, hard real-time collection bounds and production runtime
claims. Windows/macOS require hosted verification; Linux results do not establish
their ABI/thread scheduling behavior.


Local status: all six LLVM 19.1.7 configurations passed 31/31, including strict
ASan/UBSan with leaks enabled. Coordinator-only TSan, repeated runtime/regression
stress, both small-stack runs, four editor unit cases, real VS Code's three workspace
modes and VSIX packaging passed. See the dated WORK_LOG entry for exact evidence.
The coordinator protects tracing, not arbitrary simultaneous application writes;
future source-level shared-mutation/thread rules remain a separate language decision.
Internal proof schemas currently require nonempty acyclic nominal object descriptions;
this imposes no permanent source managed-object rule. The underlying tracer's cyclic
object-graph proofs remain intact.
