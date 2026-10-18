# Phase 18: reference-containing value structs

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

**Status: RV1–RV7 approved by the owner for Phase 18 implementation.**
Implementation details and current boundaries are in REFERENCE_VALUE_STRUCTS.md;
actual validation results are recorded in WORK_LOG.md.
The owner declares Phase 17 locked and reports Actions run #29 passed all seven
jobs, 39 suites per configuration. That is owner-reported hosted evidence; no
remote run was inspected or modified during this investigation. This proposal
preserves the Bible's value/reference distinction, P3 deep readonly views, P4
complete-object publication, C1 value initialization and S1 managed construction.
The recommendations below are the approved contract, preserved for architectural review.

## 1. Approved recommendation and decisions

Recommend one coherent extension: **ordinary inline value copies carrying precise
managed references and compile-time reference capabilities**. Copy the inline
storage, including reference bits; retain each referenced object's identity.
Extend the existing semantic/capability model and IR verification, then reuse the
existing aggregate lowering, Frame/Slot roots, heap descriptors and collector.
No box allocation, object cloning, reference counting or second GC path is needed.

The value/reference copy rule is already agreed in Bible §§12/36; deep copying
objects or freezing them is not an alternative consistent with that contract.
The new choices concern access capabilities and construction boundaries:

| Decision | Recommended Phase 18 contract | Alternative and consequence |
| --- | --- | --- |
| RV1: readonly value copies | Track readonly authority per managed-reference leaf through finite inline field paths. Copying into var restores independent inline mutation, never mutable authority to copied readonly referents. Mixed mutable/readonly leaves remain distinguishable | One readonly bit for every reference in an aggregate is smaller, but unnecessarily downgrades unrelated fields; treating all aggregate copies as mutable violates P3 |
| RV2: parameters and effects | Plain value parameters require mutable embedded-reference capabilities; const value formals accept deep readonly views. Infer separate inline-storage writes and writes/retention through embedded references. Referent-only methods need appropriate reference authority, not mutable inline storage | Infer permissive plain-parameter borrow contexts or require var storage even for referent-only temporary calls; respectively larger analysis or avoidable restrictions |
| RV3: defaults/readiness | Nullable leaves default null. Nonnullable leaves have no implicit object/default. No-init named construction requires complete supplied values for required fields. Preserve C1: value init starts with all fields valid; required-reference value types therefore need no-init explicit construction in this slice. Preserve S1's direct-required-reference rule only for managed construction views | Extend unavailable-value/leaf definite initialization to value init and enclosing value children now; useful, but requires additional copy/escape/readiness rules and reopens the C1/S1 boundary |
| RV4: fresh value init capabilities | Infer a fixed reference-capability profile for a fresh value initializer's receiver/result, so const reference inputs may populate readonly leaves without creating mutable authority. Check the whole body under that profile; no runtime-dependent result type or context specialization | Reject readonly inputs stored by custom value init and allow them only in no-init construction; simpler but less ergonomic. Flow-sensitive changing receiver capabilities or new ownership annotations are not recommended |
| RV5: pure contexts | Permit pure, init-free all-null reference aggregates. Materialized compile-time null templates are authority-neutral, preserving current null module-constant behavior; runtime const views retain readonly authority. No live object references/allocations in compile-time data | Uniform readonly qualification even for compile-time null templates narrows an existing Phase 17 case; rejecting all reference-containing constants also excludes useful pure defaults |
| RV6: existing storage | Binding capability profiles remain fixed after inference. Writes cannot put a readonly reference into a location promising mutable reference access. Managed fields retain their declared mutable-reference contract, recursively; readonly carriers cannot be stored there | Add readonly-qualified managed field declarations, capability-dependent heap types or whole-object readonly constructor results; each needs a larger, separately approved source contract |
| RV7: implementation/compatibility | One shared logical reference-leaf shape; existing target layout/root ABI and collector. Exact capability/effect/layout/init contracts join validated analysis, LSP invalidation and conservative reload checks | Parallel frontend/roots/constructors, conservative scanning or bypassing readonly in IR are excluded |

RV1–RV6 contain the owner-level language choices. RV7 is the recommended shared
architecture and acceptance boundary. Approval should cover the coherent package,
particularly the C1 restriction in RV3 and fresh-value-only inference in RV4.
Owner approval of RV1–RV7 is recorded in the current implementation work log.
These do not reopen P3/P4 or permanently prohibit richer initialization/field views.

## 2. Evidence from the current implementation

Reviewed the relevant Bible readonly/default/value/init/GC/reload/compiler sections,
Development Strategy and implementation addenda, VALUE_SEMANTICS, VALUE_STRUCTS,
INITIALIZATION, MANAGED_STRUCTS, MANAGED_INITIALIZATION, GC_RUNTIME, shared analysis
compatibility, decisions, known issues and recent work log. Historical phase-only
exclusions are distinguished from the current implementation.

| Inspected path | Verified existing behavior | Phase 18 gap |
| --- | --- | --- |
| `include/void/ir.hpp`, `src/ir/module.cpp` | One nominal Type/TypeArena; weak descriptor edges permit managed cycles; inline layouts are separately bounded. Aggregate operations, typed Places and managed fields coexist | Type has one readonly flag; nominal equality deliberately ignores readonly. There is no per-leaf value capability shape/assignability contract |
| `src/frontend/compiler.cpp`, layout registration | Explicitly rejects managed-reference fields in value types; references stop inline layout recursion; 128-level/65536-expanded-field bounds exist | Removing the rejection alone would admit unsafe defaults and lose access capabilities |
| `construction`, `construction_fields`, `field_default`, `initializer_fields` | C1 argument snapshots/default planning and S1 managed stage preparation already share the frontend/lowerer. Physical default metadata may contain zero bits | Required-field omission currently checks direct managed fields only on no-init managed types. Zero metadata is not evidence of a valid nonnullable value; value init currently assumes valid defaults |
| `src/frontend/capabilities.inc` | Monotone P3 origins, mutation/retention/returns and readonly summaries | Explicitly clears reference views for aggregate variables/results. Only managed-ref formals/returns/storage participate. Enabling source fields without extending this pass would lose embedded readonly provenance |
| Method checking and `Lowering::select_place/place/write_place` | Selected-owner snapshots, by-place value receivers, by-value arguments, compound LHS-before-RHS and post-RHS aggregate reload preserve selection/unrelated fields | One receiver_mutates flag conflates storage writes and reachable-object mutation; new mixed paths/capabilities need explicit verification and tests |
| `src/frontend/construction.inc`, `src/ir/construction.inc` | S1 views have finite origins/masks, all-fields-ready helper barrier, once-only init and commit proof | Aggregate copies of completed fields must retain embedded capabilities; whole staging views must still be impossible to hide in a value |
| `src/backend/llvm/backend.cpp` | Recursive reference_offsets already flattens nested aggregates for locals, parameters, instruction temporaries, receiver storage and heap payloads; module-owned DataLayout controls layout/debug metadata | Source coverage is absent. Checked nonnull publication guards are currently direct-field checks, not nested-leaf checks |
| `tests/runtime/proof.hpp`, runtime_generated/runtime_aot tests | Private IR packs nested references, copies/returns aggregates, mutates receiver storage, collects in loops and handles failures through real JIT/AOT | These are hand-built architectural proofs, not accepted Phase 18 source/type/readonly/init programs |
| `src/runtime/coordination.cpp`, `heap.cpp`, `abi.cpp` | Frame/Slot root enumeration and TypeLayout numeric offsets; complete payload commit; graph validation before sweep; no sweep on failed rendezvous | Runtime does not know source readonly or nonnullable field policy; that must not be inferred from pointer bits/offsets |
| `include/void/frontend.hpp`, compiler analysis factory, `src/frontend/tooling.cpp`, `src/lsp/main.cpp` | Owned declarations/references, effect/init contracts, exact metadata comparison, immutable sessions and stale-query rejection | New qualified aggregate/effect facts need shared serialization/comparison; current aggregate identity serialization alone does not capture them |
| Shared LLVM JIT/AOT and callback/runtime integration | Source runtime owner/bootstrap, i32 entry, runtime-only native links, independent generation leases and provider quiescence | No public aggregate/reference FFI or new entry API exists; private aggregate IR is not such an API |

The backend and tracer provide substantial reusable machinery. The missing work is
not merely enabling a field kind: semantic authority, valid defaults, effect
summaries, verifier propagation and real source acceptance remain to be proved.

## 3. Copy meaning, nesting and nominal identity

Allow numeric/bool fields, nested value fields, and direct nullable/nonnullable
managed reference fields. Assignment, ordinary parameters and returns copy all
inline value storage, recursively. Reference leaves copy references to completed
objects; no child constructor runs and no managed object is cloned. Replacing a
copy's reference field changes only that copy's slot. Mutating a shared referent
through a mutable leaf is visible through other aliases to that object.

Approved source contract example:

```swift
struct node:
    value: i32

value struct packet:
    owner: node
    count: i32

func changed(p: packet) -> packet:
    p.count++           # Local copy; caller's count unchanged.
    p.owner.value++     # Same managed target; visible to caller.
    return p

func entry(value: i32) -> i32:
    var n = node(value = value)
    var a = packet(owner = n, count = 10)
    var b = changed(a)
    b.owner = node(value = 90)
    # a.count == 10; b.count == 11
    # a.owner == n; n.value == value + 1; b.owner.value == 90
    return a.count + b.count + n.value
```

The first example is a complete program. Other variable-operation snippets below
are independent function-body fragments using the displayed declarations; they
do not propose executable top-level statements or new entry behavior.

Nullable references remain managed-only `node?`, with contextual null and checked
same-nominal `as node` unwrap. No nullable value-struct type, implicit dereference,
aggregate equality, aggregate cast or deep clone is added. Compare reference fields
for identity explicitly using existing ==/!=. Named/type/module ownership stays nominal.

Recursion checks follow **inline edges only**. This graph has finite layout:

```swift
value struct links:
    next: node?

struct node:
    edges: links
    value: i32
```

node contains links inline; links contains a pointer, not another inline node.
After construction, cycles `a.edges.next = b; b.edges.next = a` are ordinary tracing
GC graphs. A value containing itself inline, directly or through other value types,
is still invalid. Preserve existing depth/expanded-field limits and the small-stack
regression. Reference graph ownership stays in the existing TypeArena; never build
strong descriptor cycles or recursively expand a managed referent's physical layout.

## 4. Readonly views without losing value-copy ergonomics

Separate two existing concepts explicitly:

1. **Writable inline storage:** a const binding/place cannot be written; a fresh
   var copy has independent writable scalar/value/reference-slot storage.
2. **Authority through a reference leaf:** readonly references remain readonly
   through copying, nesting, checked unwrap, annotation, calls and returns.

A readonly reference capability is not a frozen object or a readonly pointer slot.
An independently held mutable alias can still modify the object. Replacing a slot
in a mutable value copy is possible, but its inferred access profile does not grant
new authority after replacement.

| Access | Inline scalar/value fields | Reachable managed objects |
| --- | --- | --- |
| var copy of fully mutable value | Independently mutable | Mutable through mutable leaves; identity shared |
| const value view | Readonly | Deep readonly |
| var copy of const value | Independently mutable | Copied leaves remain deep readonly |
| var copy of value read through const managed owner | Independently mutable | Embedded leaves remain deep readonly |
| Reference-free var copy of const value | Independently mutable | No reference authority involved; existing behavior unchanged |

For the packet declarations above:

```swift
const view = a
var copy: packet = view
copy.count++                  # Valid: separate inline storage.
# copy.owner.value++          # Reject: copied owner authority is readonly.
var alias = copy.owner
# alias.value++               # Reject: var/annotation/unwrap cannot thaw it.

var fresh = node(value = 30)
copy.owner = fresh            # Valid slot replacement; copy.owner stays readonly.
fresh.value++                 # Valid independent mutable alias.
# copy.owner.value++          # Still reject.
```

Profiles of runtime values are inferred **per inline reference path**, stopping at each managed edge.
Two fields need not share authority:

```swift
value struct pair:
    left: node
    right: node

const left_view = n
var p = pair(left = left_view, right = fresh)
# p.left.value++              # Reject.
p.right.value++               # Valid; do not taint an unrelated mutable leaf.
```

An explicit nominal annotation cannot erase the profile. New bindings infer it;
existing bindings retain their profile. A readonly incoming leaf cannot be written
to a slot/profile that promises mutable access. Mutable-to-readonly weakening is
allowed and does not freeze independent aliases. Joins conservatively retain
readonly authority and possible origins. Do not make runtime authority depend on
whether a nullable pointer happens to be null. Copying an all-null **runtime const
view** still retains its readonly reference profile; a fresh construction can start
with mutable null slots. The compile-time literal/template distinction in RV5 is
explicit, not a runtime null check or permission recovery from an object reference.
Reconstructing from an independently mutable reference is legitimate;
reconstructing from a readonly runtime leaf must preserve readonly.

Ordinary `p: packet` formals require mutable reference leaves, just as ordinary
managed parameters require mutable references today. `const p: packet` accepts a
deep readonly view and protects its local inline binding. To edit independent scalar
storage inside such a helper, copy to var:

```swift
func counted(const p: packet) -> packet:
    var result = p
    result.count++
    return result
# Result storage can be copied/mutated, but its owner remains readonly.
```

Return summaries must preserve readonly/origins per result leaf, including recursive
calls and branch joins. No explicit readonly return syntax, ownership annotation,
runtime permission tag or mandatory borrow declaration is proposed.

Managed fields retain a fixed declared mutable-reference contract recursively.
For `struct holder: data: packet`, constructing/storing holder.data from copy above
must reject: reading it later through a mutable holder would otherwise launder
copy.owner. This applies to nested aggregate stores and managed staging fields too.
Readonly-qualified heap fields or dependent heap-instance capabilities are deferred.

### Receiver effects

Infer distinct effects for writing the receiver's **inline storage** and for
mutating/retaining **objects reachable through reference leaves**. Continue to
infer them from source bodies/call graphs, not method names or annotations.

- count++ or replacing owner writes value storage; it requires a mutable selected
  value place. A var copy with readonly owner may still increment count.
- owner.value++ writes a referent; it requires mutable owner authority. It does
  not modify the value receiver's inline storage.
- Const value access cannot perform either mutation. Mixed profiles check the
  affected leaf, not a global all-or-nothing bit.
- A referent-only method on a fresh temporary with mutable embedded references
  may execute without a writable caller value place; materialize/root the same
  temporary using existing lowering. An inline-mutating temporary call remains
  rejected. This adds no change for existing reference-free value temporaries.

Ordinary by-value parameters changing their own fields do not mutate caller
storage. They may still mutate the caller's referenced objects; dropping their
origins merely because the outer parameter was copied is unsound.

## 5. Initialization and publication

### Required and default values (RV3)

Use one existing construction plan:

1. Evaluate/save supplied arguments once in written order; bind C1 init parameters
   or no-init named fields, with no mixed namespace.
2. Prepare valid fields in declaration order, preserving nested value init behavior.
3. Execute custom init once, if present, on the existing fresh receiver place.
4. Complete the value result, or commit a managed parent through existing S1/P4.
5. Only after success update the destination binding/place.

Nullable leaves default null, numeric/bool defaults remain zero/false, nested values
recursively use valid defaults/custom init. Nonnullable references have **no intrinsic
safe default and no implicit allocation**. A no-init value's required direct field
needs an explicit completed value; nested required values need a completed supplied
child. `packet(owner = n)` is valid; `packet()` and `var p: packet` are not. A no-init
wrapper can explicitly receive a complete packet. No default is evaluated and
thrown away for an explicitly supplied field.

C1 remains: every field is valid before a **value initializer** body starts. Thus
packet above cannot declare custom init that first assigns owner in its body in
this slice. Use a nullable field for a defaultable custom-initialized carrier, or
no-init named construction for a required nonnullable reference. This is an explicit
scope/approval choice, not silently treating null as a valid nonnullable default.

Similarly, a managed custom initializer's inline value child must still be
successfully default-constructible before the parent body. S1's unavailable direct
managed-reference fields do not recursively authorize partially initialized value
children. Whole-field readiness remains sufficient because those value children
are already complete. An expanded leaf-readiness/borrowed-value construction design
could follow separately; it is not permanently prohibited.

Value copies/ordinary arguments do not rerun init. Preserve existing C1 semantics
for independent copies of a valid defaulted value receiver during its body; do not
retroactively turn all value initializers into noncopyable S1 views. Such copies
contain only completed references and snapshot independent valid value storage.
They do not expose a managed parent's construction identity.

### Readonly inputs to fresh value init (RV4)

Recommend allowing this without mutable-authority laundering:

```swift
value struct optional_packet:
    owner: node?
    count: i32

func (p optional_packet) init(const owner: node, count: i32 = 0):
    p.owner = owner
    p.count = count

const source = n
var p = optional_packet(source, count = 4)
p.count++                     # Independent scalar storage.
var selected = p.owner as node
# selected.value++            # Reject: inferred readonly owner leaf.
```

Infer a **fixed**, conservative per-leaf receiver/result profile for fresh value init
from assignments, const inputs, defaults and compatible helper result summaries.
Check the entire body under that profile before lowering. A const formal supplies
readonly authority even when its actual argument is mutable. Branches join by
retaining readonly; no execution-dependent result type or per-object specialization.
Early receiver copies and helper calls see the same profile, so they cannot leak
mutable authority before a later readonly assignment. If a body requires mutation
through a leaf inferred readonly, diagnose; an independently mutable argument alias
can still be used within its own authority. This conservative rule may reject
flow-sensitive cases and must be documented/tested.

This inference is **fresh value construction only**. It does not weaken an existing
mutable value binding or a managed field's capacity. An ordinary method/helper cannot
write a readonly reference into existing storage promising mutable access. Such APIs
can return a newly constructed qualified value instead. Richer receiver-capability
constraints/readonly field declarations are future design, not implicit new syntax.

### Pure contexts and failure

Allow init-free pure aggregate constants/defaults containing only null reference
leaves; there are no heap references or runtime roots hidden in static data.
Existing typed constant evaluation/numeric/purity rules apply. Custom init, managed
allocation, runtime calls and live object references remain forbidden, including
implicit nested execution. Runtime local const construction is still allowed.

Recommend preserving the existing distinction between a compile-time constant
materialized as a typed literal and a runtime readonly view. A compile-time null
reference conveys no object identity/authority; materializing an all-null constant
value template creates independent null slots, just like pure fresh construction.
The constant itself remains unassignable. Copying a runtime const value/reference,
even if currently null, preserves its readonly profile. Const parameter binding
also applies readonly qualification regardless of the actual argument's bits.
This is a source-category rule, not runtime value-dependent typing.

The current baseline was explicitly checked: a module `const absent: node? = null`,
copied to var and replaced by a newly constructed mutable node, permits mutation
through that variable and returns 11 for input 10. No readonly object identity is
laundered: no object can occur in such a module constant. Extending pure templates
the same way preserves this locked behavior. The alternative uniform const-view
rule would reject this existing program and needs explicit approval of that
narrowing; silently changing it is not recommended. No exception applies to runtime
readonly carriers or to readonly references obtained from actual objects.

Carry the chosen default/argument capability facts alongside constant bits in the
shared construction plan. An evaluator returning zero bits must not independently
invent permissions; neutrality would follow RV5 only if the owner approves it.

Physical zeroing for root safety is not a source default for required references.
Track default availability separately from ConstantValue zero-shaped metadata.
No-init value lowering should assemble completed final field operands using the
existing aggregate operation, sharing field preparation with the current managed
no-init path, rather than materializing a source-valid aggregate containing required
nulls and patching it later. C1 custom value init already has valid prepared fields;
no partial-value opcode or alternative constructor generator is needed for RV3.

Argument/default/body/child/collection/commit failures keep existing checked source
failure propagation, frame cleanup and protected assignment outputs. Earlier effects
on completed objects are not rolled back. Unretained completed children become
collectible. S1 managed parents acquire identity only at successful final commit.
A construction view cannot be hidden in an aggregate, stored/returned/cast, or passed
as a constructor argument. Copies of completed staged value fields are allowed under
readiness and P3; the whole unfinished parent remains unavailable as a reference.

## 6. Shared representation, checking and bounded analysis

Retain Type::aggregate, Type::managed_ref, weak nominal field edges and TypeArena.
No boxing or new language-level type family. Add one compiler-owned logical
reference-leaf shape for finite inline paths. Reuse it for access profiles, effect
origins, validity queries and LLVM target offset derivation. Managed edges stop
expansion; descendants share that reference's deep readonly authority.

Place writability must also follow the binding/receiver and every crossed managed
edge. A readonly managed owner forbids writes to its inline scalar/value fields,
even when those fields contain no references. Copying a field into a new var drops
the place alias, not its embedded reference qualifications. A leaf mask alone
cannot express permission to mutate reference-free storage inside a readonly object.

Keep nominal/machine equality separate from **capability assignability**. A nominal
annotation, load, field extraction, aggregate construction/insertion, store, return,
checked unwrap and call must not discard qualifications. Logical qualifiers do not
change payload bytes, target layout or object identity. Binding/place writability
remains separate from authority in the copied value. Private construction Places
retain their distinct S1 category; never treat their addresses as managed leaves.

Extend the existing typed-AST capability worklist:

- Per value leaf: readonly fact and finite origins `(parameter/receiver, inline
  reference path)`. Once a managed edge is crossed, a summary may conservatively
  cover everything reachable from that leaf; no infinite heap-path expansion.
- Aggregate build/project/copy/insert maps those paths instead of clearing views.
  Reference-free projections carry no reference authority. Stronger path-sensitive
  alias/control-flow reasoning is not required; conservative joins remain explicit.
- Summaries distinguish receiver-storage writes, reachable-object writes/retention,
  and result-leaf origins/readonly. Source receiver const access and result authority
  are instantiated from the actual receiver and these summaries.
- Fresh value init profiles converge with existing interprocedural effects; do not
  add dynamic-instance or readonly-mask body specialization. Existing S1 contexts
  remain declaration/passing masks with the 1024/project guard.

Finite leaf paths, parameter origins and monotone facts give a bounded fixed point.
Retain 128 inline-depth/65536 expanded-field limits, diagnose any additional justified
analysis resource exhaustion, and exercise it under the existing 1 MiB stack gate.
Do not weaken limits or rely on recursive expansion of call/heap graphs.

IR validation independently derives/checks aggregate qualifications and field/call
operations. In particular:

- Fresh aggregate creation preserves each operand's authority; it is not a store
  granting mutable access. Insertion preserves unaffected leaf qualifications.
- Stores to existing local/field/receiver profiles, returns and ordinary formal
  binding enforce capability assignability, recursively.
- A managed field read through readonly access qualifies every reference leaf of
  an inline result; extracting that leaf must preserve it. Current managed-only
  gc_read qualification and unqualified extract are insufficient.
- Calls validate effects against actual value-place/reference capabilities and
  instantiate result-leaf qualifications from shared origin summaries. Do not
  trust an unqualified nominal callee result to grant more authority at a readonly
  receiver call. No second nominal type checker or constructor backend is added.
- Unavailable required defaults cannot create a successful typed source value.
  Generalize existing nonnull checks to nested required leaves at checked complete
  publication boundaries as necessary, using the same logical shape/layout. The
  runtime's contains check accepts null and cannot enforce source nonnullability.
  Physical zero root storage and checked failure-return sentinels are not successful
  source values. Preserve explicitly internal legacy allocation proofs.
- S1 alias/readiness/once-only commit verification and prohibition on construction
  view operands inside ordinary aggregate/reference fields remain in force.

## 7. LLVM roots, heap descriptors and lifetime boundaries

Use the existing shared lowerer and LLVM **19.1.7**. A value still lowers to its
ordinary native aggregate; copying requires no allocation and invokes no init.
Use existing target DataLayout/StructLayout and ABI alignment to flatten pointer
leaf offsets. Capability shapes do not create different physical LLVM struct bodies.
Keep layout/debug caches module/context-owned, including the ARM64 cache correction.

Every participating function uses the existing Frame/Slot root system:

| Storage/boundary | Required proof |
| --- | --- |
| Locals and by-value parameters | Pointer leaves zeroed before frame publication; incoming copied arguments seeded before the first poll |
| By-place value receiver | Root actual mutable field storage; if it lies in a heap object, retain the selected owning object as well. Interior storage roots do not themselves keep that owner alive |
| Instruction/argument snapshots | Every reference-containing aggregate result has a fixed rooted home before later allocation/poll; later argument effects cannot replace an earlier by-value snapshot |
| Returned aggregate | Callee unlink/return/caller rooted-store is the existing no-GC handoff while the thread stays Managed; failure checks precede use. No new polling/runtime operation in that interval |
| Managed object with inline values | Existing TypeLayout includes all nested leaves; construction validates the complete prepared graph and publishes last |
| S1 staged parent payload | Only null/completed child leaves are roots, including nested values; staging addresses and incomplete parent identities are excluded |
| Loops, recursive helpers and failures | Fixed homes reused, real cooperative polls retained under O2, balanced frame unlink on every exit |

No conservative scan, second registry, statepoint subsystem, blanket volatile,
optimizer disabling, false pure/readonly runtime attributes or early lifetime.end.
Opaque runtime calls see escaped root storage; inspect optimized behavior rather
than assuming an alloca is sufficient. Keep existing allocation-time automatic GC;
user programs do not need collection calls.

A whole aggregate store may become several native stores. Under the current
cooperative stop-the-world contract the executing thread remains Managed throughout
that no-safepoint update; the collector cannot scan it until it acknowledges with
all writes visible. Preserve that ordering. Source concurrent shared-field mutation
is not made safe by GC coordination and remains a separate thread-language problem.

The existing non-moving tracer reads each descriptor offset, follows completed
references and reclaims unreachable cycles. It needs no ownership metadata,
copy counter, reference-array side table or new collector algorithm. Current
stop-the-world marking requires no generational/concurrent write barrier; preserve
shared store/layout boundaries for future approved collector work. Do not insert
per-leaf collection/locking calls into value copies.

Descriptors continue to own size/numeric offsets independently of LLVM contexts and
JIT generations. Objects outliving allocating code remain traceable. Active code and
callback leases, NativeSafe freezing and provider unregister-and-quiesce contracts
remain authoritative; arbitrary native providers gain no safety guarantee here.
No aggregate FFI/callback export or new source threading API is introduced.

## 8. Selection, evaluation and assignments

Preserve established source order and Places, generalized to mixed value/reference
paths. Receiver selection precedes argument evaluation. Arguments snapshot once in
written order. Crossing a managed edge snapshots/roots that object's identity;
inline traversal selects storage, not an independent value copy.

- `value.owner.method(change_owner())` invokes the originally selected owner, even
  if the argument replaces value.owner and triggers GC.
- `object.payload.method(change_payload())` selects that object's payload storage;
  the method observes argument effects on that same storage, including new reference
  leaves. Root the actual place and its owner, not stale field copies.
- An ordinary aggregate argument remains the value snapshot from its own position,
  even if later arguments mutate the source's scalar or reference slots.
- Compound assignment reads its selected old LHS before RHS. Nested value writes
  reload/rebuild the enclosing current aggregate after RHS so unrelated field or
  reference changes made by RHS are not overwritten by stale whole-value copy-back.
- Crossing another embedded reference selects another completed owner; preserve
  checked nullable unwrap and selected-owner liveness at each boundary.

These paths are structurally supported by select_place/read_place/write_place and
shared receiver addressing today. New source tests must prove them, particularly
places containing references whose targets would otherwise become unreachable.

## 9. Analysis, tooling, diagnostics and reload

Expose logical leaf/capability profiles and detailed effect/result summaries through
ValidatedAnalysis and its owned nominal types/declarations, alongside existing init
profiles/defaults/construction contexts. LSP/session/index consume the same successful
analysis. No LSP-only checker or stale fallback. Current LSP provides diagnostics,
definition and references; hover/completion/rename are not implemented by this phase.

Imports/visibility/type ownership remain unchanged. Qualified constructors, nested
fields and methods navigate their actual owner declarations. Diagnostics should
name the failing path, e.g. packet.owner or outer.inner.owner, with the relevant
field/formal/initializer declaration and actionable hints. Cover missing required
reference defaults, forbidden nullable dereference, readonly referent mutation,
readonly carrier store into mutable heap fields, incompatible arguments/returns,
inline recursion and hidden staging-view escape. Suggest const formals or independent
mutable aliases/fresh values where appropriate, never var-copy as a way to thaw refs.

Exact compatibility must serialize/compare capability profiles and detailed effects,
not only aggregate nominal identity or the existing single readonly bit. Include
ordered field types/nullability/defaults, physical layout/leaf shape, initializer
presence/visibility/signature/default/body/helper closure and S1 eligibility. Propagate
changes through owned declaration dependencies/containment to callers and users.
Changed layouts/reference maps or authority contracts require restart before
publication. Compatible unrelated bodies can still reload using the current runtime;
invalid source preserves active old code. No migration, reinitialization or automatic
restart. Test live objects with embedded values across compatible code retirement.

Debug aggregates retain declared field names, nested types, sizes/alignment and
module-owned offsets. Managed references may remain opaque pointer metadata; this
does not promise a rich object inspector. Access capabilities are compiler facts,
not a different C ABI/layout or runtime object freeze.

## 10. Risks and approved implementation sequence

The principal risks are capability loss at aggregate/call boundaries, conflating
place mutation with referent authority, typed-null defaults masquerading as valid
nonnull values, missing selected heap owners, and testing roots while unrelated
host handles mask missing leaves. Fresh init profile inference is conservative:
check the complete body under its fixed profile and diagnose inconsistent use;
never recover permission from runtime pointer contents or an unchecked call.

Fixed aggregate roots/temporaries can increase generated frame size, especially
for large repeated values and recursion. Preserve bounded logical expansion,
measure representative frame/resource use, and retain safe root lifetime behavior.
The 1 MiB semantic/compiler regression does not certify that every possible large
source aggregate executes within a 1 MiB native stack. Any necessary additional
resource policy must be explicit and reviewed; do not weaken existing limits/tests
or drop roots early to conceal stack pressure. Per-leaf origin summaries must remain
bounded/sparse; diagnose justified exhaustion rather than silently omit facts.

Approved implementation sequence:

1. Record approved RV decisions and add shared bounded reference-leaf/default-
   availability facts to the nominal model. Preserve weak graph ownership and
   inline-cycle limits. Establish semantic tests before lifting source rejection.
2. Extend the existing capability/effect worklist and shared analyzed metadata;
   prove mixed leaves, copies/projections, fixed binding profiles, const formals,
   result origins and fresh value-init profiles, including recursive/imported calls.
3. Generalize existing field construction/default checks and completed operand
   assembly. Preserve C1 required-child and S1 readiness/publication; reject hidden
   construction views and readonly stores into mutable managed storage.
4. Extend independent IR capability/call/store validation and audit mixed Place
   selection. Reuse existing aggregate/managed operations and shared lowering;
   close nested nonnull and root-lifetime gaps evidenced by tests, not by disabling
   checks or adding another allocator/root mechanism.
5. Enable the approved source fields, add normal/O2 JIT and real linked AOT examples,
   forced/budget GC observations, failure/timeout/recovery and context-retirement
   tests. Integrate LSP diagnostics/navigation/invalidation and reload contracts.
6. Execute the full acceptance matrix below; update implementation guides, Bible
   only for approved clarifications, decisions, known issues and actual work log.

These are dependent tasks within one cohesive phase, not artificial stop milestones.
Stop for review if correctness requires weakening P3/P4, unbounded field/context
expansion, reinterpreting an unavailable value as initialized, a new public ABI or
changing C1/S1 beyond approved RV decisions.

## 11. Verification gates after implementation

Preserve all **39** suites and applicable assertions. Existing tests rejecting a
reference-containing value solely because it is deferred will become obsolete only
if implementation is approved; explicitly replace those narrow assertions with
positive execution coverage while retaining invalid layout/type/readonly/failure
assertions. No blanket removal or disabling of tests/sanitizers is authorized now.

| Gate | Required new real-source evidence, normal/O2 JIT and separately linked AOT |
| --- | --- |
| Copies/identity | Scalars/nested values independent; embedded targets shared; reference slot replacement independent; ordinary parameters/returns copy; copies never rerun init |
| Layouts | Nested nullable/nonnull leaves, all numeric widths/bool/padding, empty/reference-free siblings, same-named imported nominals, finite managed/value cycles; reject infinite inline recursion/depth/expansion exhaustion |
| Readonly | Const-to-var scalar edits valid; copied refs cannot thaw through annotation/unwrap/nesting/mixed-field reconstruction/calls/returns/loops. Mutable sibling/independent aliases remain valid; heap stores reject readonly carriers |
| Effects | Inline receiver writes versus referent writes/retention, readonly getter/return propagation, plain versus const formals, temporary referent-only calls, recursive/imported helpers and conservative joins |
| Construction | Named field source order, nullable defaults, explicit required values and omitted-default errors, nested C1 init, inferred fresh value-init profile, body exactly once, required-child boundary, pure all-null constants/defaults, no implicit allocation |
| S1/P4 | Managed parent with nested value refs; completed field copies; no hidden stage alias/reference in value constructors/copies/returns. Parent never allocated/rooted before body/final successful commit |
| Precise GC | Force collection with aggregate locals/params/returns/expression and earlier argument snapshots as sole reachability; mutate actual receiver leaves, numeric loops and recursive calls; inspect Frame/Slot leaves and heap descriptors under O2 |
| Selection | Replace source/reference/containing value in later args/RHS with collection; selected object/storage remains correct; preserve unrelated scalar/ref field effects; compound LHS order |
| Failure | Argument/default/value-init/managed-init/arithmetic/unwrap/commit OOM failures; protected destination/output, balanced frames, completed-child effects retained, later recovery. Timed-out rendezvous never sweeps |
| Reachability/lifetime | Cycles through inline values reclaim; complete graphs survive code/analysis retirement and compatible reload, then reclaim when actual roots release. Dead temporary retention may last until frame exit as today |
| IR negatives | Capability-dropping extract/insert/store/call/return, mutable heap-store laundering, wrong receiver effects, invalid nested nonnull publication, staging-view boxing and invalid shape/offset claims reject |
| Tooling | Imported nested field/type/init/method definitions/references; readonly/readiness diagnostics have exact UTF-16 ranges/related locations; unsaved repair restores queries, invalid snapshots never serve stale success |
| Runtime boundaries | Actual CLI bootstrap executable and runtime-only private-source AOT harness; startup/shutdown/failure and callbacks/provider quiescence regressions; no arbitrary managed C ABI claim |

Use the existing test-only ABI observer/fault injection forwarding to the real
runtime. Output numbers or descriptor inspection alone are insufficient. Force
collection while only the intended aggregate carriers keep a child alive (without
independent host/direct-reference roots masking an omitted leaf); prove real
poll overlap/parking with gates, not sleeps or inflated loop counts. Test both
budget-driven automatic collection and independently forced collection. Retain
failed-rendezvous/no-sweep and allocation/map/marking-failure assertions.

Local required matrix after implementation: installed LLVM 19.1.7 (RTTI ON) and
complete official Linux SDK (RTTI OFF), each Debug, Release and strict ASan/UBSan
configuration. Reverify SDK headers/libraries/LLVMConfig.cmake/matching tools and
existing RTTI correction. Preserve detect_leaks=1, halt_on_error and existing
failure exit codes; no suppression. Generated code is not automatically sanitizer-
instrumented, so real optimized root/semantic assertions remain essential.

Run full CTest inventory in all six, repeated GC/source/runtime stress, explicit
1 MiB-stack frontend and new semantic/resource suites on both Debug SDKs, repeated
Release normal/O2 execution, applicable runtime-only TSan, LSP/editor unit tests,
real native editor integration and VSIX packaging. Preserve all prior architectural
proofs and Phase 17 constructor/context/timeout checks.

Hosted gate remains seven unchanged independently scheduled jobs: Ubuntu 24.04
Debug/Release/Sanitizers, Windows 2022 Debug/Release, macOS ARM64 Debug/Release.
Every job runs the full inventory; each platform Debug retains real editor/package
checks; Linux Debug retains the small-stack regression. Preserve official LLVM
19.1.7, dependency/SDK configuration, fail-fast false and 90-minute timeout. Verify
ABI alignment, target offsets, aggregate calls/returns, debug metadata and O2 roots
on the actual Windows/macOS hosts; Linux cannot certify them.

## 12. Evidence collected for this design phase

No compiler/runtime/test/workflow change, configure or rebuild was made. Focused
checks used existing Phase 17 Debug binaries against both SDKs:

```sh
ctest --test-dir build/ci-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli|analysis)$' --output-on-failure -j2
ctest --test-dir build/ci-official-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli|analysis)$' --output-on-failure -j2
ctest --test-dir build/ci-debug -R '^runtime_aot_.*$' --output-on-failure
ctest --test-dir build/ci-official-debug -R '^runtime_aot_.*$' --output-on-failure
```

Both broad selections passed **15/15**, 12.43 s installed and 12.14 s official.
Both private aggregate AOT selections passed **2/2**, 0.12 s each. These include
normal/O2 existing managed source, S1 roots/timeout/commit, value frontend/init,
analysis/reload/LSP and private nested aggregate-root JIT/AOT proofs. They are
baseline checks, **not Phase 18 source or readonly implementation proof**.

Both llvm-config report 19.1.7, RTTI YES/NO respectively. LLJIT.h,
libLLVMOrcJIT.a and LLVMConfig.cmake were present in each complete installation.
An unchanged voidc check of a temporary source with a value packet containing
`owner: node?` confirmed the then-current source-located feature-deferred diagnostic.
A separate existing-source null module-constant probe returned 11, exposing the
RV5 literal-versus-runtime-view distinction that must be preserved or deliberately
approved for change; this is not a Phase 18 source test.
Evidence files: `/tmp/void-phase18-investigation/{sdk.json,installed-baseline.log,
official-baseline.log,installed-aot-baseline.log,official-aot-baseline.log,
source-boundary.log,null-module-constant.json}`. These temporary logs are local, not durable CI artifacts;
WORK_LOG.md records the results. No new full Release/sanitizer/stress/editor or
hosted run is claimed during design-only work.

**The design review gate was satisfied by explicit owner approval of RV1–RV7.**
The preceding evidence is historical design-only baseline evidence; see
REFERENCE_VALUE_STRUCTS.md and WORK_LOG.md for the implementation and final tests.
Deferred beyond the recommended package: unavailable/borrowed value initialization,
readonly-qualified heap fields, managed object cloning, aggregate equality/casts,
collections/strings/generics, source threads/async, managed aggregate FFI/closures,
weak references/finalizers/deinit/resources, moving/concurrent/generational GC,
statepoints/stack maps, precise last-use lifetime shortening and production GC tuning.
