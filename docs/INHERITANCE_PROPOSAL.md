# Phase 20: managed inheritance and explicit virtual dispatch

**Status: I1–I8 approved by the owner for Phase 20 implementation.**
Phase 19 is locked at Actions run #33, attempt #2 (47 suites/all seven jobs).
The recommendations below retain the design-review rationale; their approval is no
longer pending. [INHERITANCE.md](INHERITANCE.md) describes implemented behavior and
WORK_LOG.md records actual verification and remaining hosted gates. Deferred
alternatives still require separate design/approval. No remote work is authorized.

## Approved direction versus new decisions

Already approved in LANGUAGE_BIBLE.md Sections 12/12.1/13:

- Single inheritance of ordinary managed structs; no value-struct or multiple
  inheritance. `extends` expresses subtype identity, `using` stored composition,
  and interfaces required behavior. Composition grants no subtype conversion.
- A derived instance is usable as its base. `virtual func` explicitly permits
  overriding, `override func` explicitly overrides an inherited virtual method,
  and `super.method()` calls a parent implementation directly. Ordinary functions
  do not become virtual automatically.
- P3/RV readonly authority, value-copy/reference identity, C1/S1 initialization,
  complete-object publication P4, precise GC and generation leases remain binding.

Not already settled: exact inherited access/hiding, override signatures/effects,
constructor chaining, virtual calls during construction, dispatch representation
and reload contracts. The following is one recommended coherent slice, not a claim
these details were approved earlier.

| Decision | Recommendation for owner approval |
|---|---|
| I1 — subtype values | Managed-only acyclic base chains. Explicit internal identity-preserving upcasts at assignment/argument/return boundaries, with nullable lifting and no readonly strengthening. Keep nominal equality separate from assignability. Related-type reference equality compares the same complete object. Defer source downcasts and type-pattern expansion. |
| I2 — ownership/access | Retain owner-module methods, current public/internal/file-private access and explicit owner imports. No implicit protected access. Derived visibility must not expose a less-visible base. Virtual declarations are public/internal; overrides retain exactly their slot's visibility. Dynamic dispatch checks the accessible static slot, not imports of the runtime-selected implementation. |
| I3 — lookup | No hiding/redeclaring inherited fields or nonvirtual methods in the initial slice. An inherited virtual name requires override. Real members along the class chain outrank using promotion. Inherited using fields participate, but component promotion still considers only declarations directly on the component type. |
| I4 — override contract | Exact declared parameter/result nominal types, parameter order/nullability and existing const-parameter qualifications; no overloads, variance, covariant returns or method defaults. Receiver type changes only to the overriding descendant. Infer one conservative effect/result-capability union per virtual slot, covering all possible overrides. |
| I5 — constructors | Initializers are not inherited. An init-free hierarchy retains named construction of unique inherited/own fields. Any derived type with an init-bearing ancestor declares its own init. A derived init starts with a mandatory, dedicated `super.init(...)` prelude, even for a defaultable base; this is a proposed lifecycle exception requiring approval. Prepare/initialize base prefix completely before derived own defaults/body, then commit one derived object. |
| I6 — unfinished receivers | Preserve staged, identity-free construction. Permit bounded base-prefix construction views and existing safe nonescaping nonvirtual helpers. Reject virtual-family calls on unfinished self/aliases, including indirect helper uses; completed child objects may dispatch normally. No temporary static virtual semantics. |
| I7 — layout/dispatch | Embed the full base payload at physical offset zero, retaining its footprint/padding. Heap records retain code-independent nominal descriptors. Use generation-owned, validated per-slot dispatch functions with guarded type selection and ordinary direct calls; no code pointers in GC objects. Start with the complete validated project type universe. |
| I8 — reload/tools | Hierarchy/layout/slot/access/signature/effect/default/construction changes require restart. Compatible ordinary bodies, including overrides, can reload in a new whole generation. Virtual constructor dependency closure includes all possible completed-child targets. Existing leases protect old code; GC reachability never does. Shared analysis owns inheritance/override/navigation facts. |

I1–I6 contain the public rules/scope restrictions explicitly approved by the owner.
I7–I8 are the recommended implementation/lifetime architecture. If an implementation
cannot prove these invariants without changing the contract, stop for review.

## What exists and what is missing

Reviewed the current Bible and strategy; COMPOSITION.md/PROPOSAL; INITIALIZATION.md;
VALUE_STRUCTS.md/VALUE_SEMANTICS.md; MANAGED_STRUCTS.md;
MANAGED_INITIALIZATION.md/PROPOSAL; REFERENCE_VALUE_STRUCTS.md; GC_RUNTIME.md;
DECISIONS.md/KNOWN_ISSUES.md/WORK_LOG.md, and actual implementation below. Historical
phase-only deferrals in old guides do not supersede later locked implementations.

| Existing implementation | Concrete Phase 20 gap |
|---|---|
| `include/void/ir.hpp`: nominal ValueType/TypeArena, weak field edges, managed_ref, nullable/construction/readonly qualifiers and reference-leaf profiles | No base edge, ancestry/assignability predicate, inherited field ownership, virtual slot or dynamic-type contract. Type equality and call verification are currently exact-nominal. |
| `src/frontend/compiler.cpp`: owner method keys, initializer/default plans, Phase 19 Member selector and typed path elaboration | Parser has no extends/virtual/override/super contract; selector searches real direct members then direct component members, not ancestors. Constructor plans assume one nominal owner. |
| `capabilities.inc`: fixed-point effects and return provenance; `construction.inc`: readiness, aliases, safe helper masks | Effects currently target one known callee. Readiness is indexed by one nominal field set; no base-phase projection/completion proof or staged-self virtual restrictions. |
| `src/ir/module.cpp` and `reference_capabilities.inc`: independent nominal/call/path/capability/initialization checks | No validated upcast, virtual call family, override compatibility, base construction role or field-prefix mapping. Merely accepting unequal receiver types would be unsound. |
| `src/backend/llvm/backend.cpp`: opaque managed pointers, selected receiver/owner roots, module-owned DataLayout, direct calls, checked failures, shared normal/O2 JIT/AOT | Payloads currently follow a flat ordered field vector. Root/debug/field offsets assume that shape. No virtual target lookup or nominal object metadata emission. Managed receivers pass the reference; value/construction receivers use selected storage. |
| `TypeLayout` in `include/void/heap.hpp`: size and precise numeric reference offsets; `Heap::Object` stores a copied layout and mark bit in the existing pointer-keyed object map | No dynamic nominal identity, ancestry or dispatch metadata. Objects are payload allocations, not C++ class instances with vptrs. Dynamic types cannot be guessed from static reference type or allocation size. |
| `Runtime::construct_into` in `coordination.cpp`: validate prepared leaves, collect/poll before locked allocation/copy/root publication | Needs a validated most-derived descriptor at the same complete commit, never an independently published base allocation. Keep its failure/lock/no-sweep contracts. |
| `src/runtime/abi.cpp`: source AOT entry creates Runtime, attaches and enters Managed; JIT NativeCode owns Runtime | No source-class descriptor admission step. Existing internal IR proofs do not supply a nominal dispatch bootstrap automatically. |
| `execution.hpp`, runtime `reload.cpp`, backend Generation/resource trackers: acquired NativeCode retains a whole generation; compatible replacement reuses Runtime | No object-resident stable method pointers, per-method live patching or class migration exists. Adding them is unnecessary and would alter the proven lease model. |
| AnalyzedDeclaration/Reference, `tooling.cpp`, native LSP/session validation | Need base/override/slot/field-owner metadata and dependencies; source positions must navigate original declarations without flattening them into fictitious derived declarations. |

Inheritance must extend these structures once. It is not an LLVM bitcast feature,
a second object system or a new GC implementation.

## Substitutability, storage and tracing

Keep a managed reference as the same complete-object payload pointer. Converting
Derived to any ancestor Base neither allocates, slices, adjusts the pointer nor
reruns initialization. A Base root therefore retains the entire derived object.
The collector uses that object's actual complete descriptor, tracing all inherited
AND derived reference leaves even when only Base references remain. References
inside copied value carriers obey the same rule; the carrier storage copies while
the referenced derived object retains identity.

Use an acyclic weak nominal base edge in the existing type arena. Keep equality
exact-nominal; introduce an explicit subtype/assignability predicate and an IR
upcast operation so source, IR and capability checks can distinguish allowed
conversion from an unrelated reference cast. Do not relax Type::operator== globally.
D -> B, D -> B? and D? -> B? preserve authority and null bits. D? -> B requires the
existing checked nonnull operation before upcasting. A const view copied/cast to
Base cannot become mutable; ordinary mutable Base formals still require mutable
authority. Related ancestor/descendant equality normalizes to an ancestor view;
sibling/unrelated nominal comparisons need explicit common-base views or diagnose.
These recommendations add no numeric promotions or aggregate covariance.

Keep source downcasts, new `is`/match forms, nullable narrowing, reflection and
external managed ABI out of the initial implementation. This is a proposed scope
boundary, not rejection of the Bible's intended future type tests. Internal virtual
selection must not become a user-accessible unchecked downcast.

### Base-prefix payload

Recommend physical `DerivedPayload = { BasePayload, OwnField0, ... }`, recursively,
with the base payload at offset zero. It is a layout prefix, not a source-visible
field, a separately allocated Base object or a value struct the user can copy.
Logical inherited field entries retain original declaration owner/identity and
stable ancestor indexes; one shared mapping translates logical member paths into
actual target physical indexes/offsets. All GEPs, staging accesses, root descriptors,
required-leaf validation and debug metadata must consume that mapping. Preserve
weak descriptor edges rather than copying owning AST/type graphs.

Avoid flattening fields without reserving base tail padding: LLVM 19.1.7 target
layout experiments for Linux x86-64, Windows x86-64 and macOS ARM64 show a base
{i64,i8} occupies 16 bytes, while adding an i8 to the flat field list puts it at
byte 9. An embedded BasePayload places it at byte 16. Reserving the full prefix
makes base staging/offset calculations compositional and avoids depending on
which base operations happen not to touch padding. This is an internal layout
choice, not a public C ABI, packed layout or stable binary serialization promise.
Empty types retain distinct object identity and the existing minimum nonzero heap
allocation; an empty LLVM prefix need not invent a source byte/field. Verify actual
alignment for every supported field type on each target; no over-aligned source
feature is added.

A runtime-owned/interned immutable descriptor extends the EXISTING layout metadata
with exact nominal identity, ancestry and the complete physical tracing shape.
Heap records refer to it independently of LLVM contexts or JIT resources. Descriptor
admission validates hierarchy/layout agreement before managed execution; collision-
prone nominal hashes alone must never establish type equality. The existing
runtime/heap owns admission and descriptors, not a second heap or tracer. Preserve
legacy untyped internal allocation proofs; they cannot serve as fabricated typed
receivers for virtual operations.

## Members, visibility, overrides and using

Methods remain owned by the declaring type's original module. Fields retain their
original owner/source/default, even through several inheritance levels. Validate
all base edges, cycles, nominal kind and access before building effective members.
Do not duplicate inherited function bodies or create forwarding source methods.

Recommended lookup order:

1. Resolve a real member on the static class/ancestor chain; the permitted override
   is the only same-name real method replacement. No initial field or nonvirtual
   method hiding, including reserved inherited names, is recommended.
2. Enforce that selected declaration's access/import rules. Inaccessibility does
   not fall back to a using component.
3. Otherwise collect candidates through visible using fields, including fields
   inherited from ancestors. Each component contributes only its directly declared
   ordinary members, preserving U3. Two routes are ambiguous at use, irrespective
   of type/signature/result; qualification remains available.

Thus inherited real members outrank a derived using promotion; a derived real
member can hide an ancestor's promoted candidate. Inherited and newly declared
using fields can create a new ambiguity at access without invalidating the type.
A component's inherited members are reachable through explicit qualification, but
are not automatically promoted through the component in this phase. Do not silently
reinterpret one-hop composition as recursive effective-method-set expansion.

A promoted virtual method dispatches on the COMPONENT object identity. It does not
introduce an outer virtual slot or make the wrapper a subtype of that component.
One can add an ordinary outer method with that name under direct precedence, but
cannot override a component method with an outer receiver. Structural interface
execution remains deferred; this phase does not implement conformance machinery.

I2 recommends existing public/internal/file-private semantics, no protected keyword
or implicit privileged derived access, and no new field visibility modifiers.
Public Derived requires a public base; internal Derived may use a visible public/
internal base; private Derived may use a base accessible in its owning file. This
new inheritance exposure check does NOT change existing composition visibility.
Private base initializers remain file-private; a cross-module derived constructor
cannot bypass them through super. If protected constructors/access are wanted,
that needs a separate approved contract, not an incidental implementation choice.

Virtual declarations are public/internal and instance managed methods with bodies;
no abstract/final/virtual init or value virtual method is proposed. Override must
name an accessible inherited virtual family and match exact explicit parameter/
return/nullability contracts, const-parameter qualifications and visibility. Inferred
result authority/provenance is joined by the family rule below, not mistaken for
new source-level const-result syntax. Receiver spelling is local, but
the receiver nominal type must be the actual overriding descendant. No signature
or result-type overload guessing. Owner-module methods/imports apply as before.

Static lookup controls access to the slot. Calling Base.v through an accessible
Base slot can execute a Derived override without importing the override owner's
module at the call site: requiring every dynamic implementation import would
contradict substitutability. Looking up a directly declared Derived override or an
inherited declaration still uses that declaration's existing owner-import rules.
This distinction must be explicitly validated, not implemented as runtime access
checks or broad transitive exports.

`super.method()` is available only with a receiver and selects the effective real
implementation on its lexical immediate-base chain, directly, including a parent's
inherited grandparent implementation. It is not a value and cannot escape or rebind
the receiver. No super.field syntax is needed with the no-field-hiding rule. Super
lookup does not promote using methods in this slice; qualify the stored component.
A direct super call bypasses dispatch for that call ONLY: virtual calls made inside
the parent body on an already completed self still select the actual derived type.
Direct ordinary `.init()` remains rejected except the dedicated prelude below.

## Effects and readonly virtual contracts

Exact override parameter/result types alone are insufficient. A base virtual
reader could have a mutating override; checking only the base body would allow a
readonly alias to mutate. Conversely, forcing every override's inferred effects
to be a subset of the base body's effects would make ordinary empty/default base
implementations unexpectedly prohibit useful overrides, absent a new effect syntax.

Recommend a finite fixed point that unions each slot's possible implementations'
receiver mutations, mutable requirements, retention and returned-reference
provenance/capabilities. Remap receiver origins through the declared base/derived
field ownership mapping. Join reference profiles per leaf for value returns;
never replace mixed profiles with a permissive whole-value flag. Use the same
family summary at all virtual call sites, including statically derived receivers;
LLVM may devirtualize later without changing accepted source permissions.

A virtual call through readonly Base is rejected when ANY possible override mutates
or retains through that view. Copying it to var, upcasting or returning it cannot
thaw it. Nonvirtual methods keep their existing individual effect inference.
Independent mutable aliases remain valid. Ordinary aggregate copies and mutable
storage capacities retain RV1–RV7. Adding/changing an override can therefore cause
new source diagnostics and a restart requirement: that conservative result is
explicitly proposed under I4, not silent Liskov/effect behavior.

## Construction: one private stage, one final managed identity

S1 makes it unsafe to simply call existing Base() and insert its reference into
Derived: that creates two identities/allocations and the wrong field ownership.
It is equally unsafe to install a derived vptr before base init or pass incomplete
self to normal virtual dispatch. P4 forbids any incomplete managed parent root.

I5 recommends parameterized constructor support immediately:

- If the entire ancestry has no custom init, Derived retains named-field
  construction over its unique inherited plus own stored fields. Supplied values
  evaluate once in written order; omitted defaults/nested values prepare in
  base-to-derived declaration order. Required references must be supplied. One
  successful complete Derived payload is committed.
- Initializers are not inherited or implicitly forwarded. When any ancestor has
  custom init, every derived declaration in that chain declares its own init.
  An init-free derived class must not bypass an ancestor body through named fields.
  Forcing an explicit wrapper even for a defaultable base is the recommended simple
  rule; automatically synthesizing selected cases is an alternative below.
- A derived init starts with exactly one mandatory `super.init(...)` prelude. The
  compiler treats this as a construction operation, not an ordinary lifecycle call.
  It binds the immediate base's init parameters if present; otherwise it uses that
  init-free base's named stored-field construction contract. A no-init base's
  positional field arguments remain invalid. Existing pure parameter defaults,
  visibility, exact/contextual typing and positional-prefix/named rules apply.
- Prelude expressions can use completed parameters/locals available before the
  receiver phase, but cannot read/capture self, aliases or any inherited/derived
  field. As the prelude is structurally first, no preceding executable local
  declarations are available in this proposed grammar. Evaluate supplied outer
  arguments first, then each prelude's arguments once in written order before its
  base preparation. No runtime field defaults or ownership annotations are added.
- Complete base defaults, nested construction and base init before preparing the
  derived own defaults and executing the remainder of derived init. Repeat through
  a chain: outer supplied/prelude arguments descend first; field defaults/bodies
  complete from oldest base outward. A base with no custom init has no body to run.
- Each initializer in the chain executes once on its intended prefix. None allocates
  or publishes an independent base object. After all phases succeed, associate the
  already admitted most-derived descriptor and commit the complete object once using
  the existing publication path. Descriptor admission occurs before construction work;
  the final commit associates the already admitted descriptor with the allocation.

This proposed super.init grammar is a **new owner-level lifecycle exception** to
existing direct-init rejection. It is invalid outside that exact first prelude,
inside branches/loops/helpers, twice, on ordinary aliases, or in a root type with
no base. Init is never a virtual slot or inherited ordinary method. Do not implement
this exception until approved. Empty return/fallthrough after completed phases
works; an early return cannot skip the mandatory prelude or required initialization.

### S1 base-prefix views and virtual restriction

Allocate one aligned private Derived staging place, zero its registered reference
storage, and project a private typed Base prefix for its construction phase. The
projection is NOT a managed upcast: it has no heap identity/tag and cannot be cast,
compared, stored in objects/aggregates, captured or returned as a reference.

Base init sees only base fields. S1 unavailable required references/readiness and
safe-helper restrictions apply to that visible prefix. Completion establishes all
base-required fields ready for the enclosing derived phase. Derived required
references remain unavailable until explicitly assigned. Existing C1 value children
must already be valid before the phase body; no extension of S1 unavailable rules
into inline value init is proposed. Safe helpers cannot perform first required-field
initialization, just as today. All phases must prove required fields on every
successful exit; no source instruction can publish a prefix as a Base object.

Extend construction provenance from a plain owning slot to owning-stage PLUS
validated prefix type/phase. Keep helper contexts keyed by the existing function/
construction-parameter mask, not most-derived runtime type. A Base helper accesses
one Base prefix ABI even if the enclosing stage is Derived; no derived-specific
helper specialization is needed. The caller's full stage roots retain derived
leaves while a helper roots actual base storage. Alias/readiness facts use original
field identities and bounded prefix mappings, not fake copied base fields.

I6 recommends rejecting virtual-family calls on staged self/aliases, including
super calls to virtual bodies and transitive helper uses, even when required fields
are ready. The compiler cannot dispatch an identity-free object and must not expose
derived fields before their phase. Nonvirtual nonescaping helpers remain supported
when S1 readiness is satisfied. Completed CHILD objects may freely call virtual
methods during init; their identity and full construction already exist.

Argument/default/base/body/collection/commit failure propagates the existing checked
failure channel, removes all stage frames, skips remaining phases and leaves output/
assignment destinations unchanged. Effects already performed on completed objects
are not rolled back. A failed rendezvous never sweeps. No partial base or derived
allocation needs rollback because allocation/publication happens once at the end.
`var x: nonnullable_managed_type` still requires explicit construction, even if the
chain is defaultable; copies and upcasts never rerun init.

## Dispatch and code lifetime: recommended small implementation

Use the complete entry-reachable validated project's nominal type universe. A slot
is identified by the original virtual declaration (owner + method identity), not
by spelling alone; overriding preserves that slot. Each concrete type maps it to
the nearest overriding body or original inherited implementation. Source/IR must
validate complete maps, ancestry, target signatures and effect families independently.
Adding types/slots changes the conservative compatibility contract and requires
restart. No open plugin/class loading or interface dispatch is introduced.

Recommend generation-owned per-slot dispatch functions, shared by call sites:

1. Root/select the receiver once before arguments, preserving existing snapshot and
   selected-owner semantics. Evaluate argument snapshots in source order.
2. Query the EXISTING runtime/heap for the receiver's immutable exact nominal
   descriptor. The generation provides a readonly sorted catalog of exact nominal
   identities; a checked lookup returns that generation's concrete type index.
   A binary search of identities is sufficient initially. No collision-unsafe hash,
   module-global mutable per-runtime ID binding or new hidden source parameter is
   needed. Null/unregistered/unexpected tags take checked failure/invariant paths
   before any invalid memory access; never use an unchecked index or unreachable
   assumption to suppress a failed check.
3. The generated dispatch function switches over validated compatible descendants
   and directly calls their ordinary LLVM method functions, with the original
   pointer and existing failure context. It may coalesce identical implementations.
   A selected override's narrowed receiver is justified ONLY by that verified type
   case/map, not a source unchecked cast. Its readonly authority remains unchanged.
4. Calls/returns/loop polls and failure-frame cleanup use the current lowering.
   Return handoff is unchanged; dispatch does not park with a collector-required
   lock held or invoke source code under a metadata lock.

The catalog and dispatch functions belong to the active LLVM module/resource
tracker. They are compiler-internal lowering of a virtual IR operation, not source
forwarding methods, another code generator or a runtime trait subsystem. LLVM O2
may inline/devirtualize valid cases, but must retain required polls/root handoffs.
Code/size cost is proportional to slots and their concrete implementations; type
query/identity comparison overhead is a stated initial tradeoff, not a performance
promise. Profile before introducing caches or a more complex table binding scheme.

Heap descriptors own copies/interned runtime metadata, with NO LLVM Type pointers,
module catalog pointers, raw method addresses or code-generation leases. Runtime
metadata therefore survives retirement of allocating code. At a virtual call, the
current generation supplies the dispatch implementation while its existing lease
keeps all selected functions/catalog strings alive. An old in-flight call uses its
old generation's coherent graph; a new acquired entry uses new code even on objects
allocated earlier. No object vptr patching, stop-the-world code patching, object-held
code pinning or per-method reclamation scheme is required.

### Runtime admission and both execution paths

Introduce a small class-descriptor admission extension to the existing Runtime,
validating exact nominal identity, parent/layout agreement and tracing shape before
publishing a generation/starting source execution. A repeated compatible admission
reuses immutable descriptors; conflict requires rejection/restart. Prepare/copy
fallible metadata before the locked publication step; do not register partial
batches or leave unbounded failed-admission debris. Metadata locks follow the
existing coordinator rules; queries are short Managed operations, with no parking
or source invocation while holding the lock. Internal tracing offsets remain the
only GC root/edge representation.

JIT generation loading already owns/reuses a Runtime; admit its source catalog in
NativeSafe host work before publication, with no function pointers retained in the
runtime descriptors. AOT currently creates/attaches/enters Managed entirely inside
vd_runtime_entry: extend its PRIVATE bootstrap to admit the emitted readonly catalog
between attachment and Managed execution. Retain the existing legacy proof entry
helper and use one shared bootstrap/publication implementation, not a second runtime.
The source entry remains entry(i32)->i32 and the current CLI/main adaptation; no
new user entry-point or external reference ABI is proposed. Per-runtime admission
must not write one shared mutable AOT module table: two independent entry runtimes
must not race over bindings. Exact-key generation catalogs avoid that issue.

Allocation receives the already admitted most-derived descriptor and reuses
construct_into/Heap allocation, copying the full prepared payload and publishing
last. Type information is associated only with the completed heap allocation, not
a GC root to an unfinished stage. Legacy untyped allocation proofs remain supported
and cannot authorize a typed virtual receiver. Startup/descriptor/admission/OOM
failures use the existing protected-output protocol; no C++ exception crosses
LLVM-generated frames.

## Shared IR, optimizer and tooling requirements

Extend the same IR with nominal ancestry/field-owner/virtual-slot records and
explicit managed upcast, virtual call and base-stage projection/initialization roles.
Keep ordinary direct call for nonvirtual/super calls; avoid an unverified boolean
that silently changes dispatch or construction semantics. Representation details
may reuse Instruction/Place fields, but verification must distinguish each operation.

Independent validation must prove:

- Managed-only acyclic ancestry and valid inherited field mappings; no unrelated
  conversion, nullability strengthening or reference/leaf authority laundering.
- Stable virtual family identity, exact override signatures/access, complete target
  map coverage and inferred effect/result joins across every implementation.
- Actual rooted receiver selection, valid source/IR field paths, original owner
  mapping and justified dynamic receiver refinement for each dispatch target.
- One whole stage with valid base projections/phases, once-only ordered base/derived
  initialization, prefix readiness, no staged virtual/identity use or prefix commit,
  all required leaves valid at final most-derived publication and cleanup on failure.

Update the existing verifier's fixed point for virtual target edges; do not trust
frontend summaries or generate an alternate checker in LLVM. Header/slot/path/depth
bounds must apply before recursive work; keep iterative hierarchy/dispatch/effect
closures and the existing 128 layout depth, 65536 expanded-field and 1024 helper
context protections. Do not specialize helpers by every derived type.

One LLVM physical-layout mapper serves field accesses, prefix staging, precise root
flattening, heap descriptors and debug metadata. Retain the module-owned DataLayout
cache correction; never query a long-lived layout using expired context types.
Managed debug references are currently opaque; preserve correct metadata without
promising a new debugger/object inspector. There is no arbitrary C aggregate ABI.

Shared analyzed declarations record original owners, base edge/source, virtual slot,
override target, visibility, effective fields, family effects and construction phase
contracts. Navigation on inherited names targets the declaring field/method; an
override declaration can relate its base slot, and super navigates the statically
selected ancestor implementation. Virtual call navigation targets the accessible
static declaration, not a guessed runtime body. References/invalidation include
implicit base and all possible target edges without fabricating written spans.
Ambiguity/access/override/base-init diagnostics relate original declarations.
Invalid unsaved edits suppress stale queries and repair restores them through
existing sessions/LSP. No new hover/completion/rename subsystem is needed.

Reload exact contracts include ancestry, field ownership/layout, inherited using
flags/lookup, slots/maps, visibility/import dependencies, signatures, reference
capabilities/effects, construction ordering/defaults and source catalog identity.
Ordinary body-only edits preserving these contracts remain compatible. Constructor
bodies and their transitive helper dependencies retain C1's conservative restart
rule, including all possible virtual completed-child targets reached during init.
No live object migration, automatic restart or reinitialization is proposed.

## Alternatives and tradeoffs requiring explicit review

| Alternative | Assessment |
|---|---|
| Object payload vptr into a JIT generation | Familiar fast dispatch, but an object can outlive allocating code. Dangling pointers or pinning every allocating generation would violate the separation of GC and code lifetime. Do not select without a new proven lifetime design. |
| Stable runtime method cells with per-method patching/leases | Could decouple vptrs from generations, but adds concurrent publication, nested-call lease acquisition and code reclamation machinery beyond current whole-generation reload. Defer. |
| Generation-owned function-pointer tables | Compatible with the same code-independent object metadata/leases and potentially faster than switch dispatch, but requires careful per-runtime type binding, checked indexes and exact indirect-call ABI. A possible later representation optimization, not needed for the first proof. |
| Generation-owned guarded switch dispatch | Recommended: ordinary verified method calls, no object-held code, no new indirect-call ownership problem. Costs catalog query/comparison and possible code size; acceptance must measure bounds. |
| Flat base fields with reused padding | Fewer physical paths, but weakens the simple full-prefix storage invariant. Nested physical base payload with one mapper is recommended. |
| Separate Base object/reference inside Derived | Wrong identity, allocation count, inherited field ownership and base publication; reject. |
| Inherit constructors or silently synthesize only zero-arg wrappers | Convenient, but changes named-field/custom-init boundaries and may become surprising when parameters are added. Recommend explicit derived wrappers whenever ancestry contains init. Owner may instead approve a fully specified automatic scheme. |
| Explicit super.init prelude versus a header constructor clause | Prelude fits approved super spelling but creates a narrow lifecycle exception. A header clause can make ordering more syntactically obvious; exact syntax is OPEN if the owner rejects the recommended prelude. No grammar is approved by this document. |
| Static or most-derived virtual calls during init | Static binding changes ordinary virtual meaning; most-derived access can observe unready fields or requires incomplete identity. Recommend rejection on staged self, retaining ordinary completed-child virtual dispatch. |
| Override effects must be a subset of base-body effects | Stronger local stability, but an empty/default base body would forbid useful mutation/retention overrides without new declarations. Recommend family unions with conservative diagnostics/reload, subject to owner approval. |
| Protected access, covariant returns, method hiding | Additional public semantics; defer instead of silently inventing them. |

## Approved source examples (implemented in Phase 20)

No strings, interfaces or new primitive/entry features are required for acceptance:

```swift
struct base_user:
    health: i32 = 100

virtual func (u base_user) take_damage(amount: i32):
    u.health -= amount

struct player extends base_user:
    armor: i32

override func (p player) take_damage(amount: i32):
    super.take_damage(amount)
    p.armor++

func apply(u: base_user):
    u.take_damage(3)

func entry(value: i32) -> i32:
    var p = player(armor = value)
    var b: base_user = p
    apply(b)
    if b == p:
        return b.health + p.armor
    return 0
```

Under I1–I4, input 10 returns 108: health 97, armor 11, one identity through Base.
The same example should work with a two-level override chain; Base nonvirtual
methods remain static even while their self virtual calls dispatch dynamically.
A const Base cannot invoke this mutating family, nor can copying it to var help.

Approved parameterized construction:

```swift
struct child:
    value: i32

struct base_user:
    owner: child
    health: i32

func (u base_user) init(c: child, health: i32 = 100):
    u.owner = c
    u.health = health

struct player extends base_user:
    armor: i32

func (p player) init(c: child, armor: i32):
    super.init(c, health = 80)
    p.armor = armor

func entry(value: i32) -> i32:
    var p = player(child(value = value), armor = 2)
    var b: base_user = p
    return b.owner.value + b.health + p.armor
```

Expected 92 at input 10. There is one final player identity, never a constructed
base_user reference during the prelude. Rejected cases include primitive/value/nullable
bases, cyclic ancestry, inherited field/nonvirtual-name redeclaration, missing or
spurious override, signature/access mismatch, using-component override, super outside
a method, misplaced/repeated super.init, reads of self in its prelude, unready
required fields, staged-self virtual calls (including through helpers), escaping
base-prefix views and readonly mutations via base aliases.

## Incremental implementation sequence after approval

These are related steps of one cohesive phase, not artificial delivery stops:

1. Shared nominal base/ownership/slot records; grammar/recovery; hierarchy/access/
   duplicate/signature validation; bounded lookup precedence and analysis metadata.
2. Physical base-prefix layout mapper plus explicit upcasts, inherited fields and
   related identity checks; independent IR negatives and exact GC descriptors.
3. Descriptor admission in the existing runtime and JIT/AOT bootstrap; demonstrate
   base-only roots preserving derived-only children after allocating code retires.
4. Base-stage projections and approved prelude sequencing/readiness; once-only full
   commit, failure/output protection, bounded helpers and staged-virtual rejection.
5. Virtual-family fixed-point effects, verified dispatch maps and one shared LLVM
   lowering for normal/O2 JIT and linked AOT; direct super and using interactions.
6. Conservative reload/invalidation and LSP edits/navigation; full regressions,
   stress, cross-platform gates and implemented documentation.

Do not ship a prototype that treats upcasts as unchecked bitcasts or virtual calls
as base-only until later steps. Stop if prefix views require a second allocator,
managed incomplete roots, runtime permission tags, unbounded helper specialization,
object-held code leases or relaxed C1/S1/P3/P4/RV invariants.

## Focused tests and acceptance gates after approval

Preserve ALL **47 existing suites and assertions**, including Windows native CRT
selection and source-runtime separation. Add independent real .void fixtures and
negative IR coverage for:

- Single/multi-level chains, imported bases/owners, access exposure/private-init
  diagnostics, cycles/resource limits, primitive/value/nullable/multiple-base rejection, wrong slots,
  missing/spurious overrides, exact signature/readonly mismatch and all source spans.
- Upcast identity in locals/fields/parameters/returns, nullable propagation/checks,
  const/base alias/copy/cast/return laundering, unrelated casts, and reference-
  containing value carriers whose Base leaves retain derived-only managed children.
- Inherited numeric/bool/nested-value/reference fields with padding/empty classes,
  precise target offsets and complete descriptors; actual selected old owners and
  argument/return snapshots across allocations, loops and failure cleanup.
- Source-order supplied/prelude arguments, base/derived defaults/body order,
  required-reference readiness, inherited init-free named construction, explicit
  parameterized base init, inaccessible constructors, once-only chain execution,
  no prefix allocation/publication and no stale destination after any failure.
- Staged alias/helper misuse, indirect virtual self calls, direct super virtual
  rejection during construction, safe completed-child dispatch, no view escape,
  phase-skipping/repeated base init/invalid commit and helper-context bounds.
- Dynamic calls through Base and Derived references, inherited original fallback,
  several overrides, parent body calls on self, scalar/value/reference results,
  family effect/retention/provenance unions, mutual recursion and readonly diagnostics.
- Real/inherited member precedence over using, inherited using field routes,
  inaccessible direct members, ambiguous routes without type/signature guessing,
  component virtual dispatch on component identity and no automatic transitive
  promotion or composition subtyping.
- Independently forged IR ancestry/upcasts, field mappings, override families,
  missing/incorrect dispatch targets/signatures/effects, readonly authority upgrades,
  unsafe dynamic refinement, unready base reads and partial/duplicate publication.
- Collection with ONLY Base/aggregate roots retaining derived references/cycles,
  no unrelated host root masking; forced budget-one GC, deterministic rendezvous,
  no-sweep timeout, bootstrap/descriptor/commit OOM, cleanup and successful recovery.
- Old object/new code dispatch, old active/callback generation leases during reload,
  allocating-generation retirement with live objects, rejection of hierarchy/slot/
  effect/default changes, constructor virtual dependency changes and metadata
  lifetime after code retirement. Native provider quiescence remains mandatory.
- Imported inherited/override/super navigation and references, precise related
  diagnostics, invalid unsaved snapshot suppression, repair and shared invalidation.

Final implementation gates remain both complete LLVM 19.1.7 SDKs across Debug,
Release and strict ASan/UBSan with leak detection (six Linux configurations), all
old/new suites, 1 MiB semantic checks, repeated GC/dispatch/reload stress, normal/O2
ORC JIT and separately linked native AOT without LLVM runtime linkage, SDK/RTTI
validation, native LSP, real editor lifecycle/integration and VSIX packaging.
Preserve the seven-job workflow and its Windows Debug/Release, macOS ARM64 Debug/
Release and Ubuntu Debug/Release/Sanitizers gates. Linux and cross-target layout
results cannot establish hosted Windows/macOS execution or VSCodium availability.

## Actual investigation evidence and remaining questions

Both unchanged existing Debug SDK builds passed five useful baseline suites:
managed_initialization_source, reference_values_source, composition_source,
lsp_protocol and reload. Installed: 5/5 in 12.00 s; official: 5/5 in 11.58 s.
No rebuild, full matrix or implementation feature test was run for this design task.
Evidence: build/phase20-investigation/{installed,official}-baseline.log.
LLVM 19.1.7 llc cross-target layout constants confirm the padding example for Linux
x86-64, Windows x86-64 and macOS ARM64; layout.ll, target assembly files and
layout-results.json are in that directory. These are layout experiments, NOT
execution of VOID inheritance or target-platform test runs. Inventory remains 47.

The owner approved I1–I8 before implementation, including exact
visibility/override/effect rules; member hiding and imports; mandatory explicit
super.init with init-bearing ancestry; base/default/body sequencing; and rejecting
staged-self virtual dispatch. Alternatives are recorded above rather than treated
as approved rules. Protected access, richer casts/patterns, automatic constructor
inheritance, open-world loading, covariance, abstract/final, interfaces, inheritance
for values, multiple inheritance, stateless mixins and resource/finalizer systems
remain outside this slice. Implementation was authorized by the subsequent Phase 20
owner request; remote work remains unauthorized. See INHERITANCE.md for the actual
implementation and WORK_LOG.md for acceptance evidence.


## Implementation trace

I1–I8 are implemented through the existing shared pipeline; see INHERITANCE.md.
Four inheritance suites add to the preserved 47 (51 total). Actual verification is
in WORK_LOG.md. Independent checks include prefix publication/backedges, combined
physical-depth limits, catalog coverage and private-place readonly provenance; such
place authority does not create an unfinished managed GC root. The native harness
links independently emitted generations to exercise new-code dispatch on an old
object, without changing source entry behavior or promising a managed FFI. All
deferred alternatives above still require separate review; the experimental runtime
is not presented as production-ready.
