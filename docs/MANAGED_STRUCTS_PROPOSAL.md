# Phase 16: managed structs, architecture and source contract

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

Status: **P1–P7 approved by the owner; Phase 16 implemented locally**.
The owner explicitly approved P3 deep readonly views and delegated P4 to the
recommended reusable complete-object publication architecture. The sections below
retain the investigation and approved design rationale; statements describing
pre-Phase-16 gaps are historical baseline findings, not current implementation
status. See [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md) for the actual implemented
surface and WORK_LOG.md for verification. Hosted Phase 16 validation remains pending.
Phase 15 is locked; owner-reported run #27 passed all seven jobs at 31 suites each.

## 1. Recommendation and decisions to approve

Recommend a useful, deliberately bounded first managed-source phase: ordinary
managed structs with numeric/bool/reference-free value fields, direct nullable and
non-nullable managed-reference fields, named-field construction, identity equality,
aliased assignment/parameters/returns and owner-module receiver methods. Include
nullable self-references so real source programs can create and reclaim cycles.
Connect the existing source pipeline to the existing coordinated runtime in JIT
and real linked AOT. Add automatic allocation-triggered collection, without a
source GC API or a new entry-point policy.

Defer custom **managed** initializers and source reference-containing value structs.
Existing custom value initialization remains supported, including reference-free
value fields inside managed objects. Deferral is not a permanent prohibition.
Neither accepted `init()` semantics nor Phase 14 C1 is replaced by a temporary
managed constructor convention.

| Decision | Recommendation requiring approval | Alternative/tradeoff |
| --- | --- | --- |
| P1: phase boundary | Named-field managed construction, direct reference graphs and methods now; managed custom init and reference-containing values next | Include either now, but accept the additional publication/readonly contracts below before implementation |
| P2: nullable surface | `t?` for managed references, contextual `null`, safe `t -> t?`, identity `==`/`!=`, explicit same-nominal checked `as t` unwrap; reject direct nullable dereferences | Implement the bounded `is Ok(binding)` pattern first instead; broader patterns/results still need separate work |
| P3: readonly references | Deep readonly **views**, preserved across reference aliases, casts and returned aliases; existing mutable aliases can still mutate the object | Binding/path-only readonly permits copying a const reference to obtain mutation; global freezing changes all aliases and needs a separate object-state design |
| P4: defaults/publication | Non-nullable references require explicit valid values; complete fields before allocation and atomically publish only a complete object | Implicit `t()` allocation needs constructor/default-cycle policy; allocating a privately rooted incomplete parent needs a weaker interpretation of the requested GC-root boundary |
| P5: automatic GC | Small allocation-count budget since last successful collection, tested with a configurable internal budget; no ordinary source collection controls | Exit-only collection cannot demonstrate ordinary in-program reachability reclamation; production allocator/heuristics are outside this phase |
| P6: execution/reload | Retain `entry(i32) -> i32`; provide runtime bootstrap/link dependency for managed source; reuse one runtime across compatible JIT generations | A new main/top-level/entry discovery model or public managed export ABI is a separate phase |
| P7: receiver/field evaluation | Reference receivers copy a rooted reference; select heap targets once before arguments/RHS; preserve existing inline value receiver semantics | Rebinding caller references through receiver slots would unexpectedly change reference parameter semantics |

P3 refines an explicitly open Bible question. In particular, approval must acknowledge
that a mutable *binding* inferred from a readonly reference remains a readonly object
view: rebinding is possible, but copying the pointer does not restore write permission.
That differs from copying an independent numeric/value object, which remains mutable.
Do not silently adopt this distinction from the proposal. If P3 is not accepted,
resolve the alternative completely before implementing managed readonly behavior.

## 2. Investigation: implemented facilities and concrete gaps

Read LANGUAGE_BIBLE, DEVELOPMENT_STRATEGY, GC_RUNTIME, INITIALIZATION,
VALUE_STRUCTS and the relevant decision/known-issue records. The Bible's reference
identity, safe defaults, automatic GC, cycles, once-only initialization and
storage-independent semantics remain constraints. The approved Phase 14 construction
rules apply to its implemented value subset; managed construction is a new gate.

| Area and inspected source | What actually exists | Required gap closure |
| --- | --- | --- |
| `include/void/ir.hpp`, `src/frontend/compiler.cpp` | One nominal `Type`/`ValueType` model; parser recognizes `value struct`; fields and methods require aggregates; shared constructor argument/default plans | Parse ordinary struct, nullable annotations/null, distinguish nominal family and access capability; check reference operations in the same semantic checker |
| `src/ir/module.cpp` | Private `managed_ref`, allocation/read/write/collect/poll/alive operations; validator rejects recursive managed descriptor graphs and empty managed layouts | Accept finite reference cycles while rejecting infinite inline layouts; preserve bounded validation; validate source construction, reference equality/unwrap and receiver forms |
| Nominal ownership | `Type.definition` strongly owns descriptors; descriptor fields contain Types | Simply allowing self-references creates C++ shared-ownership cycles. An owning definition table with nonowning nominal edges is needed |
| Receiver/effect analysis | Root local slot plus aggregate field indices; mutation propagation follows receiver-slot method calls | Heap projections, reference receivers and aliases through locals/functions/returns need explicit checking. Current effect analysis is insufficient for managed readonly |
| Phase 14 construction | Saved arguments, private aggregate slots, recursive field plans, checked initializer calls; value publication follows success | Existing private value storage does not prove that a managed initializer cannot publish its receiver |
| `src/backend/llvm/backend.cpp` | Shared normal/O2 lowering; module-owned DataLayout; nested reference-offset flattening; zeroed precise roots for locals/parameters/results; null checks precede managed GEPs | Source-managed module marking, complete-object allocation, nullable/ref operations and heap-backed value receiver places |
| Managed allocation lowering | `gc_allocate` publishes a zeroed allocation directly into a root, then later `gc_write` prepares fields | This private proof sequence does **not** satisfy the stricter source-level complete-publication boundary |
| `src/runtime/heap.cpp`, `coordination.cpp`, `abi.cpp` | Nonmoving tracing, cyclic heap graph traversal, owned copied descriptors, recoverable collection failures, precise runtime registry/roots | Automatic collection policy and fully initialized allocation/publication; no source parser or constructor execution belongs here |
| Field writes | LLVM emits typed stores after managed null checks; runtime `Heap::store` separately validates trusted host reference stores | Source readonly/type/nullability validation must precede codegen. Existing stop-the-world tracing needs no new generational/concurrent write barrier |
| `Jit::compile(ValidatedAnalysis)` and `src/main.cpp` | Source compile/run currently pass no runtime owner; private IR compile can take a shared Runtime | Shared source execution must acquire/reuse its runtime, and replacements must retain that same owner |
| `emit_impl`, `tests/runtime/aot.cpp` | Public source object adapter rejects coordinated modules; private runtime objects use a C++ attachment/Managed harness | Public managed-source AOT bootstrap and documented runtime link inputs, without embedding LLVM/ORC in the executable |
| `frontend.hpp`, `tooling.cpp`, LSP tests | Owning successful analysis, value declarations/fields, references, constructor dependencies and exact invalidation contracts | Managed declaration kinds, cyclic type lifetime, nullable/readonly/effect metadata and current-revision navigation |
| Reload | Whole-generation leases, runtime identity/layout contract for private coordinated IR; conservative source initialization dependencies | Source replacement preflight currently builds a runtime-free contract; extend it to preserve runtime affinity and new effect/type dependencies |

Existing proofs are real but narrower than source support. `tests/tests.cpp` reclaims
an unreachable cyclic Heap graph. `tests/runtime/proof.hpp` exercises nested managed
roots, expression slots, arguments/returns, failures and heap-backed data, but its
nominal descriptors are acyclic and manually constructed. `generated.cpp` tests real
normal/O2 numeric backedge parking. `aot.cpp` links actual runtime objects and drives
the same runtime. Callback tests prove provider quiescence and generation leases;
failure tests inject allocation failures during root enumeration and partial marking.
None demonstrates ordinary source constructors, nullable source fields or automatic
GC in a public source executable.

## 3. Proposed first source surface

The following is **proposed acceptance source**, not executable today:

```python
value struct position:
    x: i32
    y: i32

struct node:
    number: i32
    position: position
    next: node?

func (n node) add(amount: i32):
    n.number += amount

func make_cycle(value: i32) -> i32:
    var a = node(number = value)
    var b = node(number = value + 1)
    a.next = b
    b.next = a
    var alias = a
    alias.add(2)
    return (a.next as node).number + a.number

func entry(value: i32) -> i32:
    var result = make_cycle(value)
    var i = 0
    for i < 1000:
        var temporary = node(number = i)
        i++
    return result
```

The cycle becomes reclaimable after `make_cycle` exits; later automatic collections
must reclaim it. The scalar result is `2 * value + 3`, under existing wrapping i32
rules. A harness checks actual allocation/collection/object counts; output alone
cannot prove tracing. The loop count is a test workload, not a language GC threshold.
Fixed expression-root slots can retain objects until frame exit, so no prompt local
or statement-lifetime reclamation is promised.

### Types, identity and defaults

- All ten numeric types, bool and existing reference-free nested value structs work
  as inline fields. Public fields only, as in the current value slice. Type/method
  visibility and owning-module/import rules are reused.
- Direct `t` and `t?` fields, parameters, locals and returns refer to managed objects.
  `t?` defaults to null; `t` has no intrinsic default. No nullable numbers/values,
  inheritance conversions or general nullable type algebra in this slice.
- Assignment, arguments and returns copy the reference and preserve identity.
  Passing a reference is not copying the object. Copying an inline position field
  still copies that value independently.
- `t == t` and `t != t` compare identity, never mutable fields. Compare `t` with `t?`
  by safe nullable injection; null comparisons need an identifiable nullable type.
  Different nominal types and reference ordering diagnose. No new value-aggregate
  equality, hashing or user operator system is implied.
- Contextual `null` is legal only where `t?` is expected. `var missing = null`
  requires an annotation. `t -> t?` never drops readonly permission; no implicit
  `t? -> t`. `(reference as t)` checks non-null before returning the same identity,
  preserving readonly capability. Null failure uses the existing source runtime
  failure channel before any dereference/invalid LLVM assumption. Direct `maybe.x`
  and `maybe.method()` diagnose; comparing against null does not secretly enable
  flow narrowing. `as?`, `is Ok`, `match`, `?` propagation and results remain deferred.
- For no-init managed structs, construction takes named fields only. Supplied
  values evaluate once, in written order, before omitted fields are prepared in
  declaration order. Existing pure numeric/bool/value defaults and nested custom
  **value** initialization preserve Phase 14 ordering and exactly-once execution.
- A non-nullable reference field must be explicitly supplied in named construction.
  `struct holder: child: node` is a valid layout; `holder(child = node())` works and
  `holder()` diagnoses the missing field. No recursive implicit heap construction.
  `var object: node` diagnoses and suggests `node()`; `var object: node = node()`
  works. Existing `var position: value_type` default construction is unchanged.
  Non-nullable reference cycles are valid type graphs, but do not supply a finite
  construction seed by themselves. Diagnose unavailable construction separately
  from illegal inline layout; nullable links provide an initial cycle-building seed.
- Declared field/parameter defaults and module constants remain pure. Allocation
  cannot occur in them. A typed nullable null constant is pure; a managed allocation
  is not. Runtime local const construction works. Runtime field defaults remain
  deferred rather than treating `node()` as a hidden compile-time object singleton.
- Empty managed structs can have distinct live identities: use a nonzero private
  allocation size even when the logical payload is empty. Do not expose physical
  size/offset guarantees. Direct `.init()` remains rejected; managed custom init
  declarations produce an explicit feature-deferred diagnostic, not ignored logic.

### Readonly and aliases: recommended P3

A readonly path recursively restricts referenced objects as well as inline fields.
It is a static view, not global freezing. A separately held mutable alias can still
change data observed by the view. Illustrative approved-if-P3 examples:

```python
var mutable = node()
const view = mutable
mutable.number = 4             # Allowed; view observes 4.
# view.number = 5              # Rejected.
var derived = view             # Mutable binding, readonly referent.
# derived.number = 6           # Rejected; pointer copying cannot thaw a view.
var copied = view.position     # Independent reference-free value copy.
copied.x = 7                   # Allowed; original inline position is unchanged.
```

Carry nullable/access qualification in the existing type/semantic records; erase
access permission for native representation, never erase it in source checking.
Readonly qualification persists through local aliases, child reads, casts and
returned aliases. Mutable-to-readonly is safe; readonly-to-mutable is rejected.
References stored into a mutable heap field require mutable capability in this
bounded slice. Readonly reference fields/containers can be designed later; do not
silently discard qualification when retaining a readonly alias in mutable storage.

Reuse the Bible's existing `const parameter: type` spelling for readonly managed
parameters if needed, subject to P3 approval; no new receiver keyword. Ordinary
mutable reference parameters cannot receive readonly views. Receiver methods remain
inferred: only proven nonmutating methods are callable through readonly receivers.

The effect walk must follow receiver-derived aliases, nested references, ordinary
helper arguments and returned aliases, not just the receiver's original slot.
Use finite conservative origin/effect summaries in the existing checker/call-graph
fixed point. Record which parameters/receiver may be mutated or retained into
mutable storage and which origins a result can alias. Unknown effects must not be
assumed nonmutating. Return qualification depends conservatively on aliased inputs;
proven fresh results may retain fresh mutable capability. These summaries are part
of validated analysis and reload compatibility. This is not a borrow checker,
uniqueness system, object freeze bit or another type checker.

### Receivers and writes

A managed receiver is a rooted reference passed by value. Writing fields changes
the shared object; rebinding the receiver variable changes only that local reference,
not the caller's binding. Value receivers retain their existing caller-place behavior.
Mutating a fresh mutable managed temporary can be allowed; the existing rejection
of mutable *value* receivers without addressable storage remains unchanged.

Resolve a heap target/reference once before evaluating method arguments or a write's
RHS. Root the selected owner even if an argument rebinds the original variable or
changes the field that selected it. An inline value receiver inside that object
uses the actual selected owner's storage, so argument effects on that storage remain
visible as in Phase 13. Compound assignment snapshots its old scalar before the RHS,
as today, and preserves unrelated mutations. Never lower heap value-field writes
as a detached aggregate copy whose mutation disappears. Interior addresses are
compiler-private; root the containing object, never an interior pointer.

## 4. Construction, publication and failure safety

The requested GC-root boundary is stronger than merely hiding a constructor result
until it returns. Existing private IR allocation publishes a zero-filled parent to
a root before field writes. **Do not enable that sequence as source construction.**

Recommended no-init construction uses one staged, fully initialized publication:

1. Evaluate supplied field arguments in source order into typed snapshots. Reference
   arguments already denote complete objects and remain in precise roots.
2. Complete omitted defaults/value construction in declaration order. Every source
   field, including every non-nullable reference, now has its final valid value.
   Staging roots contain only null or already complete child objects, not a pointer
   to a partially initialized parent. No user-accessible parent identity exists yet.
3. At the allocation safepoint, automatic/requested collection may run while those
   children are rooted. Validate the typed payload/descriptor before publication.
4. Allocate using the existing heap under the coordinator's allocation lock, copy
   the complete prepared payload, then install the parent reference into its precise
   result root **last**, before releasing the lock. No user code, safepoint or
   fallible operation may occur between successful allocation and publication.
5. Only a successful result becomes an expression value or overwrites a destination.

Add an initialized allocation form/helper to the shared IR/runtime ABI, using the
same descriptor calculation and Heap allocator. The validator requires exactly one
properly typed value per field for source construction. Keep existing raw private
allocation proofs, but prevent raw construction capabilities from being generated
by validated source. This is not a second constructor backend or a new heap.
The precise binary IR encoding is an implementation detail to review against the
same invariant, not a second public constructor syntax.

`Heap::allocate` already cleans up if descriptor/map insertion fails. For the new
path, all potentially throwing descriptor/validation/bookkeeping work must precede
the final nonthrowing payload copy and root store. If anything after allocating
storage can fail, add an explicit private allocation guard; no half-committed object
may remain observable. Collection cannot acquire its tracing lock during this
commit. Never call a constructor or callback while holding the runtime lock.

Argument/default/value-init/allocation/collection failures propagate through the
existing checked failure context and source call trace. Do not publish the parent,
execute later construction steps or overwrite an old destination on failure.
Previously completed child allocations can be collected after their private roots
leave; earlier effects on existing objects are not rolled back. Preserve all frame
unlinking and copy diagnostic strings while the generation lease still exists.
No native exception crosses generated frames, and no result/finalizer/deinit or
resource rollback contract is introduced. Corrupt roots remain fatal invariants.

### Why custom managed init is a separate gate

`func (n node) init(...)` would have a source-visible receiver identity. Its body
could pass that identity to helpers, store it in an existing object, return it
indirectly or expose it to callbacks before success. Phase 14 receiver effects
track mutation, not construction escape. Merely prohibiting `return n` is inadequate.

Two coherent future options need approval before enabling that syntax:

- Allocate a privately rooted object with a construction-only capability; prove
  through all calls/stores that the receiver cannot escape until success. This
  requires defining whether a private GC construction root is permitted despite
  the current literal prohibition on partial objects in GC roots.
- Stage fields and init on a construction receiver without parent reference identity,
  then atomically allocate/commit. This can enforce the literal root prohibition,
  but passing/comparing/storing self and calling ordinary methods require explicit
  rules. Do not pretend that staging is an ordinary managed reference receiver.

Neither is a routine extension of C1. If P1 instead includes managed init now,
settle these semantics and complete their escape/publication proofs in Phase 16;
do not implement a zero-argument exception or permanently ban self use to get green
results. For the recommended no-init slice, no incomplete parent exists in a root,
field, alias or callback at any safepoint/publication boundary.

## 5. Type graph, layout and shared lowering

Keep one nominal type model, distinguishing inline value and managed-reference
families with nullable/access qualification. Own nominal definitions in one immutable
analysis/module definition table; fields contain nonowning nominal IDs/keys.
References must not make descriptors strongly own themselves or each other.
IR modules, validated snapshots and copied tooling query records retain the table
owner so no nominal handle outlives its definitions. Avoid a second managed symbol
universe, weak-pointer expiry assumptions, global type interning or leaked arenas.

Resolve declarations/signatures first. Validate the entire finite type graph with a
visited worklist, then separately validate inline layout/default dependencies.
Reference edges stop physical layout recursion; they do not skip validation of their
target's fields. Self/mutual reference cycles are valid. Inline value recursion is
still rejected, including cycles reached through inline fields. Preserve existing
128-depth/65536-expanded-inline-field limits and bounded semantic/small-stack behavior.

Reuse shared aggregate layout and reference-leaf flattening, stopping at every
managed pointer rather than traversing its target object. Each object descriptor
contains its payload size and pointer offsets, including references inside supported
inline layouts. Runtime descriptors are owned numeric copies, independent of LLVM
contexts and generation lifetimes. Use the module-owned DataLayout for fields,
debug metadata and frame ABI checks; preserve the Phase 14 ARM64 cache correction.
No public C ABI, fixed offsets, packed layout, serialization or header format is
promised. Nullable/non-nullable reference storage may share pointer representation;
source validity and hidden zeroed GC slots are different questions.

Generalize the existing receiver Place representation to distinguish local inline
storage from a projection into a rooted heap owner. Managed receivers pass the
reference, value receivers pass the selected place address. Use the same semantic
field plan for reads, assignments and calls. Update IR validation and common LLVM
lowering together; no alternate source compiler, unchecked LLVM pointer path or
managed-specific type checker. Null checks must dominate every potentially invalid
address/unwrap. Identity comparisons are typed pointer equality, not integer casts.

Retain precise parameter/local/expression roots, shared failure cleanup and polls in
coordinated functions, including numeric-only callees and loop backedges. Root the
selected owner across RHS/argument allocation; root construction children and return
handoffs. Do not add misleading LLVM memory attributes, lifetime ends, O2 exclusions
or per-statement polling. Existing fixed-slot retention remains documented. Improve
liveness later only with proofs; do not write reclamation tests that assume it now.

## 6. Automatic GC and execution boundaries

### Automatic collection

Phase 15 `allocate_into` polls but does not initiate collection itself. Recommend a
small runtime-owned allocation-count budget, reset on successful collection, with
an internal configurable low threshold for deterministic tests. The exact production
threshold is an implementation tuning choice, not source semantics or a public pause
promise. Track/check it briefly under the existing mutex; release the lock before
requesting/parking for collection. Never hold a collector-required lock while parked.
A failed/timed-out rendezvous never sweeps. On collection failure, abort that source
allocation through the checked failure protocol rather than proceeding with an
invalid publication; preserve recovery and the old destination.

Do not add a collector thread, thread-local allocator, OOM retry loop, source `gc`
module or native GC scheduler API. Existing participating threads can elect the
collector. Source programs in this slice introduce no new threading or native
callback syntax. Existing provider quiescence/state/epoch tests remain authoritative.

### JIT and linked AOT

Validated analysis determines whether the project requires managed runtime support;
checking/lowering does not allocate source objects. For a managed source project,
coordinate its compiled functions consistently and require a runtime owner.

The JIT source entry path must acquire one shared Runtime and retain it with code;
compatible source replacements reuse that owner. Reuse the existing IR compile
runtime parameter and `NativeCode::invoke` attachment/Managed boundary. Do not create
a runtime on every generated function call or silently choose another runtime for
a replacement. An embedding boundary with an already attached different runtime
must reject cleanly before entering generated code, as today.

Public managed-source AOT emission must stop rejecting the coordinated source flag
and instead generate the existing entry adapter with a small **shared C++ runtime
bootstrap**. It creates/owns Runtime, attaches/enters Managed, invokes the same
checked source entry, exits generated frames before diagnostic handling, and performs
balanced shutdown. No LLVM/ORC dependency belongs in that executable. A library/private
object continues to require an explicit host bootstrap; it must not invent its own
runtime per function. Normal and failed startup/shutdown paths need tests.

Retain `voidc check/run/emit`, explicit entry file, `entry(i32) -> i32`, numeric input,
result output and existing failure exit behavior. No main/top-level discovery or
entry-signature expansion. Managed emitted objects require the matching runtime
archive, C++ support and platform thread libraries. Document this link dependency;
portable integration tests should link through CMake's existing `void_runtime` and
Threads targets. Do not claim the current scalar-only `clang object.o` command is
sufficient for managed programs, add a speculative build command, or hide linker
requirements. The object and runtime must have matching same-build ABI/toolchain.

Acceptance includes ordinary CLI-emitted managed executables with their real adapter,
plus separately linked compiled-source library harnesses that can inspect runtime
statistics. Both consume parsed/validated source IR through the same lowerer; do not
replace source acceptance with hand-built IR. Public managed-reference exports and
foreign ownership remain deferred. Runtime cleanup reclaims ordinary unreachable
memory, not native resources or hot-reload code.

### Reload and tools

Exact compatibility includes nominal family/identity, ordered fields, nullability,
access/effect contracts, reference edges, defaults, initialization dependencies,
coordinated ABI and runtime identity. Serialize cyclic graph edges by identity with
a visited set, not recursive expansion. Changing value versus reference family or
any incompatible field/default/effect contract requires manual restart before any
publication. Compatible non-initialization method/function body changes can reuse
live objects and existing whole-generation leases. Value init and transitive helper
body policy remain as Phase 14; no object migration/reinitialization or auto restart.

A runtime must not retain ORC metadata or code merely because an object survives.
An executing generation lease must not be retired because GC cannot find an object.
Copied object descriptors survive code retirement. Source runtime ownership,
replacement preflight and diagnostic storage must enforce these separate lifetimes.

Add managed declaration/field categories and type/effect metadata to the existing
validated analysis. Share those records with invalidation and LSP; include reference
edges for nullable annotations and cyclic declarations. Constructor and field/method
navigation must resolve nominal ownership across imports. Invalid current revisions
suppress stale queries and native publication as before. Syntax highlighting already
intentionally supports standalone struct; do not change approved colors or invent a
second editor grammar/type interpretation.

## 7. Verification and acceptance gates after approval

Retain every assertion in the 31 existing suites. Add focused managed-source tests
and integration coverage; do not replace the internal Phase 15 proofs. Every new
source fixture must compile through the ordinary frontend, not reconstructed IR.

| Gate | Required observations in normal/O2 JIT and normal/O2 separately linked AOT |
| --- | --- |
| Identity/copy | Alias write visible to original; same versus distinct construction equality; parameter/return aliases; receiver rebinding does not rebind caller; nested inline copies independent; empty identities distinct |
| Construction | Once-only source argument effects, reversed named order, omitted fields in declaration order, existing value init execution once; complete parent at publication; explicit required non-null child; no recursive implicit allocations |
| Nullable | Self/mutual annotations, null defaults/assignment/comparisons, contextual inference errors, safe injection, checked unwrap success/failure before address use, source caller trace, destination protection |
| Readonly | Direct/nested/cast/copied alias mutation rejected; ordinary helper laundering and returned aliases rejected; const-parameter retention into mutable fields rejected; unrelated mutable alias changes remain visible; inline value copies stay mutable |
| Tracing | Live cycles survive; unreachable cycles reclaimed after helper frame exit; same-shaped/differently owned types distinguish; multiple references to one object traced; reference members plus nested reference-free mixed-width values have correct descriptors |
| Handoffs/O2 | References survive later argument allocation, returned references, short-circuit temporaries, nested calls, selected heap receivers and loop safepoints; optimized instructions really call required safepoints |
| Collection/failure | Low-budget automatic collection during nested allocation; pending rendezvous before allocation; no parent reference published before complete fields; injected payload/descriptor/heap/collection allocation failure leaves roots/old destination intact; retry after failure succeeds without half-built survivors |
| Runtime/bootstrap | CLI JIT and actual CLI-AOT executable agree, standalone AOT has no LLVM dependency; separate source library harness observes collections/live-object counts; balanced attach/detach and runtime lifetime on success/failure; wrong-runtime entry rejected |
| Reload | Same-runtime compatible replacement preserves reachable source objects; changed layout/default/nullability/readonly effects/family rejected; old active leases survive replacement; retired code does not invalidate object descriptors |
| Diagnostics/tools | Unsupported init/value-reference feature errors, missing fields, duplicate labels, wrong nominal/nullability/access types, pure allocation rejection, source spans/related declarations/recovery, imported type/field/method navigation, invalid unsaved revisions and repair; never suggest copying a reference to var as an independent-object mutation fix |

Deterministically observe publication with a private test seam outside runtime locks
and/or a collector request at the construction safepoint. Inspect the actual result
root and payload at commit boundaries; an output-only test cannot prove absence of
partial publication. Never gate a Managed thread while holding a lock the collector
needs, rely on sleeps/iteration speed for overlap, or use allocation counts alone as
proof of reachability. Fault-injection checks must cover cleanup after real allocations.

Run all old/new suites in six local LLVM 19.1.7 configurations: installed RTTI ON and
complete official RTTI OFF SDKs, each Debug/Release/strict ASan+UBSan. Preserve leak
detection, ABI matching, small-stack regressions, repeated coordination/callback/GC
stress and applicable TSan checks. Generated code and prebuilt LLVM are not made
sanitizer-instrumented merely by instrumenting the C++ runtime; root/layout/failure
observations remain essential. Preserve real editor processes, extension unit tests,
VSIX packaging and LSP revisions. New recursive type graphs must pass bounded-resource
and 1 MiB stack cases and leave no owning-descriptor cycles under leak detection.

All seven hosted jobs must run the full suite inventory: Ubuntu Debug/Release/
Sanitizers, Windows Debug/Release, macOS ARM64 Debug/Release, with the three Debug
editor/package runs. Preserve SDK 19.1.7 and 90-minute timeout. Linux evidence is not
Windows/macOS execution evidence; require real hosted constructor/root/frame/layout,
link/bootstrap and shutdown results. No phase lock before all applicable gates pass.

## 8. Implementation order if the owner approves

This is one cohesive phase, not authorization to stop at artificial milestones.

1. Lock P1-P7 and their explicit diagnostics. Refactor shared nominal ownership and
   cyclic validation without changing existing value behavior; retain all proofs.
2. Extend parsing/shared semantic analysis for managed/nullable/capability types and
   conservative receiver/function alias effects. Update analysis/tooling together.
3. Add complete-object construction, generalized places and reference operations to
   shared IR validation/lowering. Reuse descriptors, roots, checked failures and O2.
4. Implement initialized allocation plus automatic trigger outside all required locks;
   prove atomic publication, OOM cleanup and coordinator invariants deterministically.
5. Connect the source JIT owner/reload path and real AOT bootstrap/link requirements;
   run independent real source programs with observable reachability evidence.
6. Complete source/LSP/editor/reload tests and all local/hosted acceptance matrices;
   document actual commands/results and remaining limits in an implementation document.

## 9. Deferred work and stop conditions

Deferred: managed custom init/self construction and fallible constructors;
reference-containing source value structs (including nullable leaves); runtime field
defaults, readonly heap fields, mutable module globals, inheritance/interfaces,
strings/collections/generics, weak references, finalizers/deinit/resource ownership,
closures/events, source threads/async, generalized native bindings, public managed
exports, object migration, LTO, tighter root liveness, allocator optimization and a
new executable-entry policy. Existing value init and private mixed-reference IR
proofs remain supported and tested. This proposal creates no permanent prohibition
on any deferred language feature.

Stop implementation if the chosen readonly contract can be bypassed, complete
publication requires exposing an incomplete managed identity, a runtime/link/bootstrap
path cannot preserve failure cleanup, or cyclic type metadata cannot be owned without
leaks/dangling records. Bring the contradiction to the owner rather than using
unchecked pointers, hiding GC roots, suppressing sanitizers or weakening tests.

The owner has approved the recommended P1–P7. The acceptance gates above still
apply; local implementation does not establish hosted platform verification.
