# Stage 6 Delivery 4: trusted static native fixture

Delivery 4 is hosted-accepted at `619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
owner-confirmed 7/7 jobs and 917/917 suites. Delivery 5 integrated acceptance is
locally complete: 139/139 each in Debug, Release and strict ASan/UBSan/leak
configurations (417/417), with 1 MiB and editor/package checks passed. The bounded
D6 contract below is unchanged. Only the owner can commit, push, run hosted CI and lock Stage 6
after all seven jobs pass. Stage 7 remains unauthorized.

## Delivery 5 integration boundary

The real `integrated.void`, `integrated_cancel.void` and `integrated_failure.void`
programs compose this binding with owning-executor async, groups and value-only
workers. The native resource stays local to its acquisition thread through await
and cleanup. Use existing explicit `Jit::compile_async` / `emit_async_object` host
APIs for these async entries; ordinary CLI/LSP opt-in rules remain unchanged.
Tests cover normal/O2 JIT and separately linked AOT, GC in foreign callbacks and
workers, cancellation/failure cleanup and callback-before-admission shutdown while
a worker is held. No native resource or callable crosses the worker boundary.

Cached old roots, queued/running jobs and retained registrations keep their code
through compatible replacement. Tests invoke the old callback after its root and
NativeCode release, then unregister and verify retirement; private async frame
changes do not migrate old operations. A callback's old task token is rejected
inside a replacement executor, even though its own native entry remains leased.
Repeated reload checks count managed objects and live generations, not process RSS.

Fresh-process cache regressions use real C bridges and a stack-probed synchronous
worker frame. They prove relocation/rebinding and fresh native resource/callback
creation on cold/hit runs, corrupt-artifact fallback and safe Windows bypass.
Corrupt artifacts remain misses until explicitly removed; this integration does
not introduce automatic cache repair, new granularity or file watching. Neither
live Runtime state nor pointers/handles/callback addresses become cache payloads.

The ordinary compiler-owned tooling session and real LSP process still reject an
unprovided native descriptor, including after other explicitly authorized native
projects have run in the host. Overlay repair restores references without accepting
stale results. No Stage 7 tooling feature or general FFI is introduced.

## Explicit project trust and one compilation path

`src/native/fixture.c` is a real statically linked C library. A host explicitly
opts one project into its compiled catalog using the typed C++ descriptor:

```cpp
auto analysis = vd::analyze_project(entry, vd::native_fixture_project());
// Check analysis.succeeded(), then use the existing Jit or object emitter.
vd::emit_object(analysis.program->lower(), output, optimize, true); // source bootstrap
```

`void_native_example examples/native/main.void` is the runnable embedding example
and prints `19`. Ordinary `.void` wrappers import `/native_bridge.void`. The host
provides that virtual module from its verified descriptor; it cannot shadow a
physical project file. The ordinary CLI/LSP do not automatically authorize this
fixture. No public native grammar, manifest parser, package resolver, dynamic
loader or arbitrary-symbol mechanism is delivered.

The descriptor names the compiled library and exact callable names, symbols,
parameter/result types, affinity, blocking and callback behavior. Unknown symbols,
ABI mismatches and modified behavior fail before source publication. Header
functions are checked declarations, never executable placeholder bodies. Calls
become a verified `native_call` in existing typed IR and use existing LLVM
lowering for normal/O2 JIT and independently linked AOT. IR independently rejects
unsupported signatures, unverified bridge imports, forged resource construction,
payload access, incompatible callback provenance and worker/native effects.

The compiled bridge invokes real C prototypes for i8/i16/i32/i64, u8/u16/u32/u64,
f32/f64 and bool. Private LLVM transport uses u32/u64/pointers; the target host
compiler applies exact C scalar ABI rules. VOID managed aggregates, resource
payloads, varargs, arbitrary C++ objects and source pointers are not public ABIs.

## Strings and deterministic resource ownership

VOID keeps one UTF-8 string type. Terminated APIs receive a safely owned temporary
and reject embedded NUL. Length-aware calls, returned owned buffers and retained
copies preserve embedded NUL and UTF-8 bytes. Returned C allocations are released
even if conversion into a managed string fails. Retained bytes live in the
resource's owned backing string; the C pointer is cleared before that storage is
released or replaced. No pointer into caller storage survives a boundary.

The binding's `resource` is an opaque managed identity. Source cannot construct,
extend or access its payload. The C pointer exists only in the Runtime registry;
it is never a source-visible integer or cache payload. A normal managed VOID
wrapper uses `result<resource, string>` acquisition and existing `deinit`/`with`.
The wrapper is allocated before native acquisition, and registry publication is
transactional. Failed allocation/admission releases the C resource and roots.

Managed assignments share one checked identity. Explicit close rejects duplicate
close and use after close. Existing one-shot `deinit` safely finishes an already
closed wrapper. Resource-owning value `deinit` remains rejected by the existing
managed-receiver requirement; no copyable native pointer ownership is introduced.
Resource handles and callable-containing wrappers cannot cross task/worker
boundaries. Raw handles are binding-private identities, not transferable values.
GC does not invoke native cleanup. Explicit close or Runtime shutdown unregisters,
quiesces, releases retained backing, frees C storage and releases persistent roots.
Dropping an owner without deterministic cleanup retains its resource until shutdown.

## Callbacks, threads and lifetime

The fixture stores real callback/context registrations in C. Dispatch reserves a
provider call and snapshots that registration while locked, then invokes the
stable compiled trampoline without provider locks. Existing CallbackRegistration
admits same-thread or foreign-thread execution into the same Runtime. Callback
captures have precise Handle roots and a lease on their exact native generation.

For this delivery registration accepts directly named functions and freshly
formed literal/bound closures in the registering generation. Forwarded or stored
callable values diagnose: the existing callable ABI carries no generation token.
This avoids pairing an old discriminator/environment with a new dispatcher.
Registration is one-shot; unregister drains and releases its lease. A later
registration requires a new resource. Self-drain fails through checked diagnostics
rather than deadlocking. Callback failures are copied while their code lease is
held, then returned through the existing RuntimeFailure path; C++ exceptions never
unwind through generated source code. Reached cleanup cannot overwrite a primary
native failure's owned message or bounded source frames.

Pure scalar/string operations are thread independent. Resource operations require
the acquisition thread. Blocking operations and synchronous foreign dispatch/join
enter NativeSafe; the collector can rendezvous while the foreign callback is
Managed. Foreign callbacks cannot operate on their caller's thread-affine resource.
Shutdown may drain on another NativeSafe host thread. Provider unregister prevents
new reservations and waits for all reserved dispatches, including callbacks held
before admission; registration drain then waits for admitted callbacks. Roots,
retained bytes and code leases remain until both barriers finish. Concurrent
shutdown calls serialize their drain. Shutdown from inside an active callback is
rejected before changing Runtime admission state.

JIT generations own the existing shared ORC engine, not the compiler's Runtime
owner. This breaks a callback lease → generation → compiler → Runtime ownership
cycle without weakening retirement. Compatible reload keeps a registered callback
on its original code until unregister; replacement calls use replacement code.
Debug cache identity includes the actual compiled C fixture. Cache hits relocate
ordinary code and bind the verified bridge anew; no C pointers, active callbacks,
roots or native resource state are serialized.

## Bounds and acceptance

Permanent source tests cover all scalar widths, bool/floats, UTF-8/NUL transfers,
returned/retained ownership, failed acquisition, cleanup, duplicate/use-after-close,
same/foreign callbacks, pre-admission shutdown, GC, allocation failures, callback
self-drain/affinity, descriptor/IR rejection, cache hits and old-code retirement.
The main and four checked-failure programs have normal/O2 independent AOT coverage.
Acceptance also retains all existing suites, strict sanitizers/leak checks, editor
packaging and 1 MiB stack checks. See WORK_LOG.md for actual results.

General native libraries, public binding syntax, asynchronous native completion,
worker/native calls, native-resource task transfer, UTF-16 policy, arbitrary
callable-generation transfer and dynamic library ownership remain deferred.
