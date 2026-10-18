# Phase 15: GC roots and thread coordination proposal

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

**Status: G1–G6 approved and implemented locally; all six Linux configurations pass. Hosted Windows/macOS verification remains required.**
Inspected baseline: `c1b4a66`, 12 October 2026. The owner declares Phase 14 complete.
The owner subsequently authorized implementation with deterministic coordination,
provider-quiescence, optimized-root and failure-protocol safeguards. This is not a
production GC claim. See `GC_RUNTIME.md` and `WORK_LOG.md` for implementation evidence.

## 1. Recommendation and boundaries

Keep the agreed precise, non-moving, stop-the-world mark/sweep direction. Add one
runtime-owned thread registry, cooperative rendezvous and per-thread explicit root
frames. A requesting thread performs collection after every participating thread
has either parked or entered a strictly defined native-safe state. Use the same
runtime ABI and shared VOID IR/LLVM lowering for JIT and AOT.

Start with one coordinator mutex and condition variable, serialized heap metadata
operations, and no dedicated collector thread. Root frame publication/removal is
allocation-free owner-thread work between safepoints. Prefer visible correctness
over allocator throughput in this foundation. Thread-local allocation caches,
pages/size classes and polling optimizations remain measured follow-up work, not
permanently excluded features.

Use ordinary LLVM IR storage and runtime calls for explicit roots. Defer
statepoint/stack-map integration rather than implementing both root strategies.
The owner approved this choice in G1: it extends the existing successful root experiment,
but does not complete the roadmap's separate statepoint/stack-map experiment.

Phase 15 must not add parsed managed `struct`, reference-containing source value
types, strings, collections, async, finalizers, resource destruction or a new entry
point model. Internal typed IR/runtime proofs can establish the missing contracts
without claiming those language features. Current value construction, readonly
receivers, exact compatibility checks and compiler/tooling representations stay
authoritative. The Bible's proposed top-level execution discussion is a separate
design gate.

## 2. Baseline investigation before Phase 15 implementation

| Actual component | Verified implementation | Missing foundation |
| --- | --- | --- |
| `include/void/heap.hpp`, `src/runtime/heap.cpp` | Single-threaded heap; zeroed payloads; copied reference-offset descriptors; iterative marking; cycles; validate graph and roll back marks before sweep on tracing failure | Thread ownership, rendezvous, automatic collection, concurrent access protection, alignment beyond ordinary allocation |
| `Heap::add_root/remove_root`, `Root` | Explicit addresses of C++ pointer slots; balanced RAII registration in the proofs | Per-thread frames, native-safe transitions, generated failure cleanup; `Root::reset` itself does not validate its value |
| `Jit::compile_root_probe`, `tests/tests.cpp::gc` | One registered generated stack slot survives normal/O2 calls and loops; nested reference offsets are tested in heap payloads | General typed compiler roots, optimized nested stack aggregates, multithreaded collection, GC AOT linkage |
| `ir::Type`, frontend, shared `lower` | Numeric/Boolean values and nested value structs; mutable receiver Places; initialization and checked normal-return failures | No managed-reference type/allocation/safepoint operations in normal source IR; no source-managed objects |
| `NativeCode`, `ReloadSlot`, ORC resource trackers | Leases retain active generations; publication and retirement occur outside execution/publication lock; concurrent callers and blocked native calls tested | Foreign-thread GC attachment, callback teardown races, managed metadata surviving generation retirement |
| Native C fixture and `native_utf8` | Real C call; borrowed owned UTF-8 bridge; opaque resource; same-thread rooted callback following a reload slot | General source FFI, blocking/native state transitions, concurrent/foreign callback support |

`thread_local probe_heap` is a test selector, not a thread registry. The fixture's
resource counter and callback fields are unsynchronized; its current tests do not
establish concurrent callback safety. Heap maps, roots and payload operations have
no synchronization. Putting a mutex only around `collect()` would still race with
mutating stack roots and would not solve the problem.

The collector currently runs only when explicitly called by trusted proof code.
For example, `vd_probe_allocate` allocates a parent and then a child without
temporarily rooting the parent. That is safe under the current explicit collection
schedule, but cannot be reused unchanged if allocation can trigger GC.

Existing `RuntimeFailure` storage is per invocation. Generated failures return
normally; C++ throws only after native return. GC must preserve this contract.
The AOT adapter currently links plain objects to C library support, not to a
managed-runtime bootstrap. Its success is not evidence of AOT GC support.
`Module::imports` currently describes trusted i32(i32) proof functions only;
it is not an existing typed managed-runtime/FFI signature system.

Local investigation reran existing tests, not the proposed protocol. Results and
commands are in WORK_LOG.md. No new multithreaded-GC, callback-race or performance
result is claimed. The owner's Phase 14 completion is the baseline; older pending
hosted notes in existing documentation remain historical evidence.

## 3. Runtime ownership and thread registration

One non-copyable runtime instance owns a heap, thread records, persistent native
handles and the coordination state. Compiler-owned logical type information stays
in the existing type model; the runtime receives immutable tracing descriptors,
not a parallel language type system. No references may cross independent heaps.

An OS thread attaches before any managed entry or heap/root access. Its stable
thread record contains its runtime identity, state, frame-chain head and nesting
information. TLS identifies the current record. Initially allow only one active
runtime attachment per OS thread; nested entry into that same runtime reuses it.
Multiple independent runtimes on different threads are possible; cross-runtime
nesting is rejected before changing either runtime's state.

An outer attachment guard owns detach. Nested calls/callbacks restore the previous
state and cannot detach the outer invocation. Registration and initial Managed
entry are separate: a new record starts NativeSafe with no managed storage, then
waits for any active collection before becoming Managed. Thread-record allocation
failure leaves the thread unattached and executes no generated code.

| Thread state | Permission and collector guarantee |
| --- | --- |
| Detached | No runtime stack frames or managed access. A native handle may exist independently in the runtime's persistent registry. |
| Managed | May execute generated code, access managed storage and change its own root frames. Collection must wait for this thread. |
| Parked | Acknowledged the current collection request; roots and managed storage are stable. Waits until that epoch completes or aborts. |
| NativeSafe | Frozen managed frames remain valid; may use native-owned data or block. Must not read/write managed storage or mutate frozen roots. Reentry waits for collection. |

The collector is an elected requester, not another kind of application mutator.
Its own record is frozen/acknowledged while it runs runtime-only tracing. This
does not allow arbitrary Managed code to execute while marked Parked.

Detach requires no active managed entry or frame. It is serialized with the
coordinator; a pending epoch must finish before membership is removed. Persistent
handles do not depend on the creator thread remaining attached. Silent TLS
destructor cleanup is not a substitute for balanced generated exits and explicit
thread shutdown. Invalid detach/ownership transitions diagnose before mutation.

## 4. Cooperative collection and deadlock rules

Use a monotonically increasing request epoch and Idle/Requested/Collecting phases,
protected by the coordinator mutex. Initial polls call a small runtime helper that
checks this state under the mutex. An atomic inline fast path is not needed for
the first proof. Condition-variable waits release the mutex and always recheck
their predicates, including epoch completion and abort.

Collection proceeds as follows:

1. A Managed requester finishes publishing its roots, locks the coordinator and
   elects itself if Idle. It opens a new epoch and freezes its own frame chain.
2. Other Managed threads acknowledge at polls or transition to NativeSafe after
   freezing their roots. A second collection requester parks for the existing
   epoch; it must not wait while still recorded as Managed.
3. Attach, NativeSafe reentry and callback Managed entry wait until Idle. Existing
   Managed threads can finish root updates and become safe. The coordinator waits
   with the mutex released until no record remains Managed.
4. Only then enter Collecting. With the mutex held, enumerate frozen thread roots
   and persistent handles, trace and sweep. Invoke no generated code, native user
   callbacks, finalizers, reload operations or application locks.
5. Restore Idle, publish the outcome and wake waiters on every exit path. Parked
   mutators resume Managed only after observing completion of their epoch **and
   current Idle state under the mutex**. If another epoch starts before a waiter
   wakes, it stays frozen and acknowledges/waits for that epoch too; an old
   notification cannot authorize execution during a new collection.

The mutex also serializes allocation bookkeeping, checked proof heap operations
and persistent-handle changes. An allocating thread releases it before requesting
collection; it never parks while retaining the lock needed by the collector.
Owner-thread frame/slot writes before parking happen-before tracing through the
mutex/condition-variable protocol. The collector never scans a Managed thread.
Generated direct heap writes become visible by the same rendezvous ordering.

Do not hold coordinator, heap, reload-publication or callback-registration locks
while waiting for user code, joining a thread, performing ORC retirement or
calling an external library. Use one runtime metadata lock initially; introduce
no nested heap/registry lock ordering. Brief reload operations stay outside that
lock. Their existing leases remain the code-lifetime authority.

Potentially blocking lock acquisition, I/O, thread joins and native calls must
enter NativeSafe before blocking. This prevents a collector waiting for a thread
which is itself waiting for a parked thread to release an application lock.
The collector never takes application locks. Arbitrary application deadlocks and
incorrect trusted native code cannot be prevented by the GC protocol.

There is no forced OS suspension or stack scanning. A noncooperating Managed
thread prevents collection. Recommend an embedder/test-configurable rendezvous
deadline: expiration aborts the request **before marking/sweeping**, resumes
parked threads and reports failure to collection requesters. It never removes a
thread from the root set or assumes it is safe. No fixed game-frame pause bound
or user-facing GC scheduling API is promised. If allocation cannot proceed after
an aborted collection, it fails through the checked runtime boundary.

## 5. Precise roots in optimized generated code

### Explicit frames and nested values

Each root-bearing invocation uses a fixed-size stack frame linked to its owning
thread record. A frame describes typed storage addresses and immutable reference
offsets. Managed root locations start null before publication. Frame push/pop
helpers are allocation-free, do not collect and cannot call user code.

Trace addressable value storage itself, rather than keeping a stale second copy
of its references. For a nested inline value, flatten reference leaves by adding
the target-native offsets of each field along the path. A mutable receiver Place
then changes the storage the collector actually scans. Ordinary value copying
copies reference values into independent value storage; both copies are traced.
Managed referents retain reference identity when that language feature arrives.

Layout offsets, sizes and alignments come from the shared LLVM lowering's
**module-owned DataLayout**. Preserve the Phase 14 context/cache-lifetime fix.
Emit tracing metadata separately from DWARF; debug metadata is not a GC map.
Runtime-owned object descriptors contain copied numeric offsets and no pointers
to LLVM types, contexts or DI metadata. Bounds/overlap/alignment checks must reject
invalid descriptors before allocation or publication. Start with the existing
ordinary allocation alignment; do not claim support for over-aligned payloads.
Read reference leaves using byte offsets and memcpy, as the heap tracer does,
without aliasing arbitrary aggregate fields through C++ void** casts.

Materialize root storage for managed references in parameters, locals, return
handoffs and expression temporaries, including each reference leaf of aggregate
temporaries. A value may remain in registers for computation only while its root
storage still preserves the required referent. Non-moving collection needs no
pointer relocation. Scalar/numeric-only values need no root entry.

For `h(f(), g())`, root the completed result of f before g can allocate or poll.
Keep receiver bases and argument snapshots rooted throughout calls and partial
construction. Root base objects, never only a derived field pointer; internal
Places are not an authorization for public interior-pointer roots. On-stack
aggregate roots point to real stack storage, not borrowed heap field addresses.

Use fixed slots per IR value/storage, reused on loop iterations, not a growing
root list per allocation. Clear dead expression temporaries and exited lexical
storage where the existing lowering can prove it safe. For the initial proof,
remaining slots can stay rooted until function exit. This is precise by reference
kind but may retain an object longer than its last machine-level use. It is not
conservative scanning of arbitrary words. Tight liveness and escape optimization
are follow-up work; no bounded early reclamation/finalization timing is promised.
Rooted allocas remain live storage until their frame is unlinked. Do not emit an
early lifetime.end or reuse that memory while a published root entry still points
to it, even after clearing a reference to null.

### Safepoints, calls and handoffs

Participating generated functions publish incoming root storage before an entry
poll. Poll at potentially unbounded loop backedges, recursive execution paths and
runtime allocation/transition boundaries. A numeric-only loop reached from managed
code must cooperate too. Use one explicit placement rule in shared lowering,
including continue paths; no poll on every arithmetic instruction or source line.
Initially retain loop polls unless a bounded execution argument is proved. Measure
redundancy before changing placement; LLVM must not remove the only cooperation
point in an optimized infinite loop or recursion cycle.

Every safepoint must see all references needed afterward, including those needed
for failure cleanup. Frame unlink and return form a non-collecting handoff: a
caller stores a returned reference into its prepared root slot before any poll.
The thread remains Managed during this interval, so another collector waits.
Likewise a callee seeds reference parameters before its first poll while caller
argument roots remain live. No forced interruption is permitted between these
steps. Tail-call transformations cannot bypass frame cleanup/handoffs; do not
emit musttail for such functions in the first implementation.

Prefer allocation into an already registered null destination root slot. A slow
allocation can collect before creating the object, then deposits the result
without a further poll. Helpers performing multiple allocations must root each
intermediate object before the next allocating call. Initializer storage and
supplied arguments stay rooted until success or failure cleanup; this proposal
does not decide managed constructor semantics or publication of managed self.

### LLVM 19.1.7 mechanism choice

Use IRBuilder allocas, typed loads/stores/GEPs and calls to the runtime frame and
poll helpers. Keep those helpers opaque to the generated module's O2 pipeline.
Their calls must retain accurate shared-memory effects: no false memory(none),
memory(argmem:...), readonly, nosync or nocapture promises for escaping root frames.
Only mark a runtime C boundary nounwind after it actually catches all exceptions.
Do not use blanket volatile roots, pointer-to-integer hiding, or disable LLVM
optimization. Test the observable reachability contract after O2, not just IR
instruction spelling. New LTO integration requires an explicit re-audit of these
effects; current compilation does not establish an LTO GC contract.

The installed 19.1.7 headers expose CreateGCStatepointCall/Invoke, CreateGCResult,
CreateGCRelocate, GCStrategy and RewriteStatepointsForGC. Their presence does not
establish runtime integration. Statepoints would require a second substantial
piece of work: root/base liveness, safe lowering, target stack maps, native frame
walks and JIT metadata registration/retirement. Defer them until measurements
justify replacing explicit frames. Do not combine two root implementations now.

LLVM's built-in shadow-stack strategy has a global root chain and is documented
as not thread-safe. The proposed per-thread frames use ordinary IR and our own
runtime protocol; simply setting gc "shadow-stack" would not implement it.
LLVM does not supply the collector or thread rendezvous. See the
[LLVM 19 GC integration documentation](https://releases.llvm.org/19.1.0/docs/GarbageCollection.html),
[statepoint documentation](https://releases.llvm.org/19.1.0/docs/Statepoints.html),
and [LLVM 19 memory-effect attributes](https://releases.llvm.org/19.1.0/docs/LangRef.html#function-attributes).
These are versioned 19.1-series documentation; API availability was checked
locally against both exact 19.1.7 SDKs. No new statepoint execution was tested.

## 6. Native calls, callbacks and blocking

Classify trusted bridge helpers by an internal, checked contract, not user syntax:

| Boundary | Required behavior |
| --- | --- |
| Small runtime leaf | Remains Managed; no collection, blocking, callback or allocation that could collect. Examples: frame push/pop. |
| Allocating/polling runtime helper | Remains Managed, participates in rendezvous; arguments and intermediates are rooted. |
| External or potentially blocking operation | Freeze roots and enter NativeSafe before the call/wait; use native-owned storage/opaque handles; wait for Idle before Managed reentry. |

Treat unknown external calls conservatively as potentially blocking/reentrant.
Initial NativeSafe bridges must not dereference even stable-address managed
payloads. Non-moving addresses alone do not prove reachability, synchronization
or safe mutation. Copy data into native-owned storage or use registered opaque
handles. General direct borrows/pinning and native mutation require a later FFI
contract; this foundation does not permanently forbid them.

A persistent native handle is an off-heap, runtime-owned root slot with a stable
identity and checked runtime ownership. Create/change/release it only through a
Managed runtime operation; a foreign caller first attaches and enters. Frozen
frame roots remain unchanged throughout a native operation. Raw pointers retained
by a C library are not automatically roots. Handles preserve objects independently
of stack frames or the thread which created them.

Stable callback trampolines reuse the existing reload-slot concept. An invocation
first acquires registration admission/lifetime, attaches if foreign, waits for
Managed entry, materializes arguments from handles into roots, and acquires a code
lease outside the coordinator lock. Same-thread reentry from NativeSafe preserves
the frozen outer frames and pushes nested Managed frames. On return/failure it
removes nested frames, restores the previous state and releases its lease and
admission. A newly attached foreign thread detaches after its outermost callback.
No native exception or longjmp may bypass this sequence.

Use a small owned callback registration with a closing flag and in-flight count
for the proof. Closing denies new admission; already admitted invocations retain
permission to complete their entry and cleanup. Ask the native provider
to unregister while NativeSafe, with no runtime/publication lock held, then drain
all in-flight trampoline invocations before releasing capture handles/context.
The provider's quiescence barrier must cover calls already dispatched but not yet
counted by the trampoline, as well as preventing future calls. A guarantee of
"no new dispatch" alone is insufficient. An in-flight counter alone cannot
protect a callback which has not yet entered the trampoline. If the provider
cannot guarantee that no invocation can still access the context at teardown,
do not claim safe teardown.

Self-unregister must not synchronously wait for its own callback. Reject that
blocking teardown attempt and leave the registration alive for explicit teardown
after return. Do not add a deferred scheduler to solve it. These lifetime rules
are proposed trusted-adapter contracts, not a general events/FFI language feature.

Catch C++ exceptions at runtime/trampoline boundaries. Restore state and roots
before returning a binding-specific failure to C; do not invent a universal C
callback error sentinel or silently publish a default VOID success. General
native exception translation and public callback signatures stay deferred.

## 7. Object lifetimes and hot reload remain separate

Keep NativeCode/ReloadSlot ownership and whole-module compatibility rules. An
active call retains its generation through native waits, callbacks, parked GC
epochs, failure-message copying and generated cleanup. Collection completion is
not proof that old machine code can be unloaded. Continue retiring trackers only
after the last existing code lease, outside publication/coordinator locks.

Copy object tracing descriptors into runtime-owned storage at allocation, as the
heap already does. An object must not keep a pointer to a JIT global type map,
scanner function or code generation merely to remain traceable. Stack frame maps
may live in their leased generation because the active invocation keeps that
generation alive until its frame is removed. Persistent handles use runtime-owned
metadata. No GC reachability decision is allowed to drop executable-code leases.

Do not add a second code-retirement epoch scheme. The rendezvous can later support
other runtime coordination, but existing shared ownership already proves the
current synchronous retirement contract. Compilation/binding remain host-serialized
and outside Managed execution or in NativeSafe host work. Layout, signature,
capture, constant and initialization-policy changes still require manual restart;
no migration, object reinitialization or automatic restart is added.

ORC resource trackers identify removable resources; they do not replace VOID's
active-call lifetime proof. See [LLVM 19 ORC resource removal](https://releases.llvm.org/19.1.0/docs/ORCv2.html#how-to-remove-code).

## 8. Failure, cleanup and shutdown

All recoverable runtime operations report a small nonthrowing status to generated
code. Convert it to the existing checked failure path with source context. Catch
allocation/standard-library exceptions inside the C runtime boundary; preserve
the current host exception boundary after generated return. Frame cleanup runs
on every normal return and every existing numeric/call/initialization failure
edge. Pop helpers cannot allocate, collect or replace an earlier failure.

No native unwind through generated code is supported. Host RAII restores thread
attachment/state if NativeCode::invoke reports a failure after return. Leases
remain held while borrowed diagnostic strings are copied, as today.
For a GC-enabled entry, the Managed execution guard must finish after generated
root cleanup and before host diagnostic copying, lease retirement or other host
work. The current invoke path would need that scoped integration; it does not
already perform attachment. Keep the existing i32/checked-entry ABI and GC-free
proof path. TLS supplies the active runtime context, not a new source parameter.

| Failure | Proposed response |
| --- | --- |
| Bad attachment, foreign-runtime handle, duplicate registration or nonempty detach requested through a checked host API | Reject before mutation; preserve the existing valid state. |
| Rendezvous deadline | Abort before tracing; resume parked threads; failed request/allocation reports an error. Ordinary polls do not fabricate successful collection. |
| Worklist allocation failure during marking | Reset temporary marks; no sweep; restore Idle and report failure. |
| Allocation/descriptor validation failure | No published object/result; clear the prepared destination as needed; report checked failure. |
| Untracked root/graph edge, broken frame chain or impossible generated transition, when detectable | No sweep; fatal runtime-invariant diagnostic. Do not resume ordinary managed execution with corrupted roots. Test in a subprocess. |
| Process crash, forced OS-thread termination or native longjmp through managed frames | Unsupported; cannot promise safe recovery or cleanup. |

The fatal-corruption policy needs approval. Retain Heap's existing exception-based
invalid-root tests as tests of its validate-before-sweep algorithm; they do not
establish safe production recovery from corrupted managed state.

Shutdown is explicit host orchestration, not thread killing. Mark the runtime
closing, reject new outer entries/registrations, unregister providers and drain
callbacks, then let existing calls complete and join their threads in NativeSafe
or unattached host code. Allow already admitted executions to reenter for balanced
completion. Clear handles and detach empty records before destroying the heap.
Shutdown/teardown cannot hold a lock needed by those calls or callbacks. Refuse
final destruction while records, frames, handles or callback registrations remain.
This does not supply async cancellation, task groups, deinit or finalization.

GC coordination is not application synchronization. Shared mutable field access
still needs a separately supported synchronization contract; no language data-race
semantics are finalized here. Tests must synchronize their own shared payloads.

## 9. Safe reuse and deferred work

Reuse the existing tracer, zeroed allocation, offset validation, iterative worklist
and failure-before-sweep rollback. Extend its root-enumeration input to accept
frozen frames/handles instead of writing a second collector. Keep the existing
single-threaded Heap/Root proof interface isolated or adapted to that same core;
do not advertise unsynchronized direct Heap methods as thread-safe runtime APIs.
The new runtime's ownership must prevent accidental copying of an owning heap.

Reuse shared typed IR, lowering, O2/module verification, per-module target layout,
checked failures, ORC binding and leases. Reuse native fixture assertions and UTF-8
ownership tests; add a genuinely synchronized provider fixture for foreign/racing
callbacks rather than assuming the existing fixture is safe on multiple threads.
Use portable std::thread/mutex/condition_variable facilities already established
by the project, without reintroducing the unsupported jthread/stop_token path.

Deferred until managed source types or a separately approved phase:

- Managed struct syntax/semantics, nullable references, inherited/object headers,
  safe managed defaults, constructor escape/publication and failure rules.
- Actual source reference fields and their readonly/aliasing rules. Numeric-only
  value struct copying and C1 construction do not change in this phase.
- Runtime module/static roots and initialization semantics; no incidental top-level
  executable statements or change to the current i32 entry adapter.
- Strings, variable-length object maps, collections, closures/events, async frames,
  weak references, finalizers and deterministic native-resource ownership.
- Public FFI grammar, arbitrary managed borrows/interior pointers, dynamic-library
  ownership, callback ABI/error policy and over-aligned managed allocation.
- TLABs, reusable pages/size classes, concurrent/generational marking, moving GC,
  write/read barriers, stack-map decoding, optimized root liveness and escape
  analysis. No barrier is needed for the proposed complete stop-the-world trace;
  preserve semantic reference stores so a later collector can lower them differently.

## 10. Acceptance tests and principal risks

These are **required future tests**, not results of this investigation. Use
deterministic barriers/latches and observable collection/code-retirement outcomes;
timeouts detect hangs but must not replace the intended synchronization schedule.

| Area | Required evidence after implementation |
| --- | --- |
| Registry and epochs | Nested same-runtime attachment; rejected cross-runtime entry; simultaneous collection requests; attach/detach/reentry during request; spurious wakeups; no stale-epoch release; hundreds of repeated thread starts/exits |
| Rendezvous | Multiple allocating mutators; an allocation-free infinite/long loop cooperates under O2; recursive/continue paths; native-safe blocked thread does not prevent collection; deadline abort does not sweep a noncooperating thread |
| Precise roots | Scalars and nested stack/heap values; lookalike integers do not retain objects; live cycles survive and unreachable cycles collect; null/overwritten/exited roots clear; changing nested alignment; many temporary/reference arguments including h(f(), g()) |
| Mutation and handoffs | Rooted base for receiver Places; updated nested receiver fields traced; independent value copies; aggregate parameters/returns; allocation result and return gaps stressed by collection requests; no partially failed result publication |
| Failure cleanup | Numeric/cast/callee failure with nested frames; injected allocation/worklist failure; mark rollback and no partial sweep; balanced frame counts and detach afterward; fatal corruption/misuse subprocess cases |
| Native and callbacks | Blocked I/O/join/lock wrappers; same-thread reentry; foreign OS-thread callbacks; callback-triggered GC; concurrent close/admission/drain; provider quiescence; rejected self-wait; capture alive until quiescence and collectible afterward |
| Reload separation | Old generation parked or blocked while new calls use replacement; live object traceable after allocating code retires; callback follows replacement; frame metadata retained by leases; no tracker removal under coordinator/publication lock; incompatible edits unchanged |
| Shared backend | Same typed proof through shared lowering at normal/O2, ORC and separately linked AOT with real runtime bootstrap; verify LLVM IR/DWARF; no reliance on JIT absolute symbols for AOT |
| Shutdown | Closing against callback admission, collection and thread exit; callbacks drained before context release; no destructor with active roots/threads; no automatic resource finalization |

Preserve all 24 existing suites/assertions, both exact LLVM 19.1.7 SDKs and their
RTTI policies, Debug/Release and strict ASan/UBSan with leak detection, small-stack
semantic tests, LSP/tooling and real editor/package checks. Run actual hosted
Windows x64 and macOS ARM64 tests; Linux success is not a substitute. Generated
application instructions are currently not sanitizer-instrumented, so explicit
reachability and lifetime assertions remain essential.

Add a separate focused runtime ThreadSanitizer experiment where supported. It
supplements, never replaces, strict ASan/UBSan. Record LLVM/prebuilt-library/JIT
coverage limits; no indiscriminate sanitizer suppression. Do not require unsupported
TSan on every platform or claim that an uninstrumented generated store is checked.

Measure request-to-last-ack time separately from tracing/sweep time, plus root
counts, allocation/metadata overhead and worst-case repeated pauses. There are no
throughput or real-time guarantees yet. Main risks are missing root temporaries,
unsafe helper attributes, blocking while Managed, callback provider lifetime
promises, stale code-owned descriptors and long root retention. Passing thread
stress alone cannot prove all schedules; review the state/lock invariants as well.

## 11. Proposed implementation sequence, after approval

This is one cohesive phase with dependency order, not separate approval milestones.

1. Establish checked attachment, states, epochs, NativeSafe guards and explicit
   shutdown over the existing heap. Prove rendezvous, competing requesters and
   failure/deadline cleanup in C++ before allowing collection of generated roots.
2. Extend the single tracer's root enumeration to per-thread frames and persistent
   handles. Prove frozen nested storage and metadata ownership with C++ fixtures.
3. Add only the internal managed-reference/layout/allocation/safepoint IR machinery
   needed for typed architectural proofs. Use the existing type/IR validation and
   shared lowerer, not another frontend or general-purpose runtime type model.
   Validate dedicated runtime operations and their effects; do not route typed
   reference arguments through the legacy i32-only native import contract.
   Integrate frame storage, polls and all failure exits; exercise nested temporary,
   argument and return roots at normal/O2. Preserve the original root-probe assertions.
4. Connect the same runtime contracts to leased JIT invocation and an AOT proof
   bootstrap/link harness. Keep the current public CLI/entry contract. Validate
   separately linked AOT without ORC or LLVM libraries in the runtime executable.
   Use the matching C++ runtime/thread link requirements through CMake; the current
   plain-C link driver alone is insufficient. Check internal runtime ABI/target
   compatibility at bootstrap without defining a public aggregate/object ABI.
5. Add blocking/native and synchronized same-thread/foreign callback fixtures,
   provider quiescence and reload/collection/shutdown stress. Preserve code leases
   rather than introducing another reclamation algorithm.
6. Run the full existing matrix plus new tests, strict sanitizers, small-stack,
   editor checks and real hosted platforms. Record latency baselines and remaining
   limits before claiming this foundation is ready for a managed-struct proposal.

Do not call reference-containing source types supported until a later approved
managed language phase uses these contracts end to end.

## 12. Owner-approved decisions

Non-moving tracing and cooperative participation are already accepted direction.
The owner approved the following concrete implementation choices:

| Decision | Recommended choice | Alternative/tradeoff |
| --- | --- | --- |
| G1: optimized root representation | Per-thread explicit root frames through ordinary IR/runtime calls; defer statepoints | Statepoints now could reduce root-store overhead but require stack walking/maps, target integration and metadata lifetime work before this thread foundation |
| G2: coordination and allocation staging | Elected requesting thread, one runtime metadata mutex/CV, cooperative states, no collector thread; defer allocator caches | Dedicated GC thread/multiple locks/caches add coordination and allocation complexity before correctness evidence; initial design pays serialized allocation/poll overhead |
| G3: root retention | Exact reference locations with safe frame-lifetime retention where tighter clearing is unproved | Full last-use liveness now can reduce retention but expands compiler work; neither choice promises GC destruction timing |
| G4: native/foreign callback contract | Strict NativeSafe access prohibition, persistent handles, automatic same-runtime/foreign attachment, provider quiescence before teardown | Direct managed borrows or providers without quiescence need additional binding-specific safety/lifetime design |
| G5: failures | Checked recoverable failures, pre-trace rendezvous deadline abort, fatal invariant corruption, no forced suspension/unwind | Indefinite wait or recovery from corrupt roots is harder to diagnose/prove; no fixed public deadline value is locked here |
| G6: Phase 15 scope | Internal typed IR proofs plus JIT/AOT runtime boundary; no managed source syntax or new entry semantics | Adding managed language types now would combine unresolved object/constructor policy with runtime coordination |

Approval authorizes this implementation scope, not deferred
language rules or a permanent prohibition on later root/allocator improvements.
The owner approved all six recommendations and the implementation sequence.
Implementation uses the existing tracer and lowerer with seven additional suites;
the exact interfaces, invariants and remaining limits are recorded in `GC_RUNTIME.md`.


## 13. Implementation and verification addendum

All six implementation stages are complete locally. GC_RUNTIME.md records the
interfaces and lock/state/epoch invariants. Runtime admission distinguishes idle
new entries from active scopes and admitted callbacks, so closing rejects new work
while allowing existing calls to finish. Timed-out attempts never enter tracing;
worklist failure rolls back before sweeping and releases waiters. The reused tracer
reads frames and handles; the shared IR validator supplies lowering result types.

Normal/O2 common lowering roots nested storage, expression temporaries, arguments,
returns and actual mutable receivers. Real generated native calls freeze those
frames during blocking and callback reentry. Existing whole-generation leases
remain authoritative across collection/reload. Provider fixtures own and join
all dispatched trampoline users before allowing context destruction.

All 31 suites (the original 24 plus seven runtime suites) passed with both installed
and official LLVM 19.1.7 in Debug, Release and strict ASan/UBSan. Targeted repetitions,
1 MiB stack checks, coordinator-only TSan, real VS Code integration and VSIX packaging
also passed; exact commands, timings and limits are in WORK_LOG.md. Linux results
are not Windows/macOS verification. Managed source types, statepoints, LTO, tighter
liveness, allocation thresholds and production performance claims remain deferred.
