# Stage 6 Delivery 1: native async activation

Delivery 1 passed local Linux Debug, Release and strict ASan/UBSan/leak acceptance:
98/98 suites in each configuration. Stage 5 remains the hosted-locked baseline;
Delivery 1 requires owner review and seven-job hosted CI.
The five-delivery roadmap and remaining decisions are in [STAGE6_PROPOSAL.md](STAGE6_PROPOSAL.md).

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
retain their existing independence. There is no worker pool, cancellation,
detachment, public task group or general native binding surface.

The adapter drains every started task before returning, including unawaited work
and work started by a child after the root completes or fails. It preserves the
root failure as primary. If the root succeeds, the first otherwise unobserved
failure in creation order is primary; additional unobserved failures follow in
that order within the existing 32-frame diagnostic bound. Awaiting a failed task
observes that failure and propagates it through normal reached cleanup. Failures
own copied diagnostics. A cyclic wait is diagnosed and waiting activations resume
through checked cleanup; siblings are not implicitly cancelled.

## Ownership and validation

The existing typed CFG and LLVM lowerer emit a native start wrapper and native
resume function. Heap homes hold arguments, locals, instruction temporaries,
resume state and cleanup state. Resume stack roots cover existing copy/construction
scratch storage and are removed before suspension. Only validated resume states
can be entered; required frame reads and borrowed values crossing suspension have
an independent IR proof. Malformed task shapes, forged payload access/construction,
sync bypass calls and awaits during deferred cleanup are rejected independently
of source checking.

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
remaining limitations are recorded in [WORK_LOG.md](WORK_LOG.md).
