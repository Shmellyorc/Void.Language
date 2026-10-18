# Managed inheritance and explicit virtual dispatch

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Phase 20 implements the owner-approved I1–I8 contract from
[INHERITANCE_PROPOSAL.md](INHERITANCE_PROPOSAL.md). Actual verification and hosted
limitations are recorded in WORK_LOG.md. This extends the existing compiler and
runtime; it does not introduce an interface system or a production GC claim.

## Source contract

Only managed `struct` can extend one nonnullable managed base. Inline value types
and multiple inheritance are excluded. Ancestor conversions preserve the complete
object pointer and identity, including references embedded in copied value structs.
Nominal type equality remains exact; assignability uses explicit validated upcasts.
D → B, D → B? and D? → B? are supported. Unwrap D? to D before converting to B.
Source downcasts and new `is`/pattern forms remain deferred. Equality between
ancestor/descendant views compares identity; siblings need explicit common-base views.

```swift
struct base:
    health: i32 = 100

virtual func (b base) damage(amount: i32):
    b.health -= amount

struct player extends base:
    armor: i32

override func (p player) damage(amount: i32):
    super.damage(amount)
    p.armor++

func entry(value: i32) -> i32:
    var p = player(armor = value)
    var b: base = p
    b.damage(3)
    return p.health + p.armor
```

Input 10 returns 108. The executable fixture is examples/inheritance/main.void.
Ordinary methods remain static. `super.method()` selects the effective real parent
implementation directly; virtual calls inside that body still dispatch normally
on completed self. Super does not promote a using member or create a first-class
parent value. Declaration modifiers are recognized in their syntactic contexts. Existing lexical
variable/module bindings remain available; the implicit parent qualifier applies
when no such binding occupies super. Broader keyword reservation is not finalized.

Overrides require an accessible inherited virtual method and exact declared
parameter/result types, nullability, const-parameter qualifications and visibility.
There are no overloads, covariant returns, abstract/final methods or method defaults.
Virtual methods require public/internal managed receivers; init is never virtual.
Methods retain original module ownership and direct-owner imports. Dispatch through
an accessible static slot does not require imports of runtime-selected override
owners. No protected access or inherited field/nonvirtual-method hiding is added.
Public derived types require public bases; internal types cannot expose private bases.

Real members along the static ancestry outrank composition. Inherited using fields
participate in promotion. Each component still promotes only its directly declared
members; its inherited members require qualification. Ambiguity remains an access
error, without signature/type guessing. A promoted virtual call uses the component's
identity, and mutating inline component calls use original embedded storage.

## Construction and capabilities

Initializers are not inherited. An entirely init-free ancestry supports named
construction over unique inherited and own fields. All supplied expressions are
snapshotted once in written order; omitted defaults prepare base-to-derived.
If any ancestor declares init, each descendant declares its own initializer.
Every derived initializer begins with an explicit dedicated `super.init(...)`,
even for a defaultable base. With an init-free immediate base, that prelude accepts
named stored fields; otherwise it binds initializer parameters using C1's existing
positional-prefix/named/pure-default rules and exact typing.

```swift
struct base:
    number: i32

func (b base) init(number: i32 = 1):
    b.number = number

struct child extends base:
    extra: i32

func (c child) init(number: i32, extra: i32):
    super.init(number)
    c.extra = extra
```

Caller arguments run first, followed by each derived-to-base prelude argument list.
The oldest base then prepares defaults/runs its body; each child prepares its own
defaults/runs its body afterwards. One private most-derived stage is finally committed
as one complete object. No independently allocated base identity or incomplete
managed root appears. `var b: base` still does not implicitly allocate an object.
Prelude arguments cannot access self, aliases or fields. Direct/misplaced/repeated
init calls are rejected.

Base-prefix views are compiler-checked private places, not managed upcasts. Safe
aliases/helpers can operate on a ready visible prefix without most-derived helper
specialization. Required references must be initialized before reads, helper calls
and successful exits. Helpers cannot perform first required-field initialization.
Virtual-family calls on unfinished self/aliases are rejected, including transitive
helper use and direct super calls to virtual implementations. Completed child objects
may dispatch normally. C1 value children remain fully valid before their value init.
Escape, identity tests, explicit casts, rebinding, self-links and prefix publication
remain prohibited. Failures propagate through existing checked frames, leave output
unchanged and release roots; completed-object side effects are not rolled back.

P3 and RV1–RV7 remain compile-time capabilities. Upcasts, aliases, fields, copied
carriers, parameters and returns cannot thaw readonly referents. Independent mutable
aliases and independent inline value storage retain their existing behavior.
A finite conservative virtual-family fixed point joins mutations, retention, mutable
requirements and per-leaf result provenance across possible implementations. Every
virtual call uses that summary; direct super calls use the selected body's effects,
including its own virtual dependencies. There are no runtime permission tags.

## Shared implementation

Nominal descriptors retain weak base edges, effective logical field indexes and
original field owners. Physical payloads are `{complete BasePayload, own fields...}`
at offset zero, including base tail padding. One logical-to-physical mapper serves
field GEPs, heap tracing offsets, private stages, roots and debug metadata. Empty
LLVM prefixes add no artificial source field; heap allocations remain nonzero.
This is a private target layout, not an external C or serialization ABI.

The existing IR now distinguishes upcast, virtual call, base initialization and
private prefix projection. Its independent validator checks hierarchy, ownership,
exact families, capabilities and stages. Base completion is tracked as a may/must
state across control-flow joins, preventing repeated preludes on backedges. Final
commit must match the owning most-derived stage, including through prefix aliases.
Physical inheritance-plus-inline nesting is bounded at 128, expanded fields at
65536, and helper parameter-mask contexts at 1024. Cached layouts cannot bypass
the combined physical-depth bound. Private places have compile-time authority/provenance facts,
including readonly getter results, while reference_paths still excludes unfinished
parents from GC roots. Constructor defaults retain their dedicated mutable-storage
role; ordinary helpers cannot hide receiver mutations. Contextual rewrites and parent-only
method handling stay outside recursively nested semantic frames so the existing
resource-limit diagnostics remain safe under a 1 MiB Debug stack.

Runtime-owned immutable nominal descriptors contain exact identity, ancestry and
complete tracing shape. They contain no LLVM types, catalog pointers, code addresses
or generation leases. Admission prepares/validates complete batches NativeSafe and
publishes atomically; compatible records intern, conflicts/OOM leave the old catalog
unchanged. Objects retain descriptors through the existing Heap/construct_into path.
Legacy untyped internal IR proofs remain supported but cannot fabricate typed
virtual receivers.

A generation owns a sorted exact-key catalog and guarded per-slot switch functions.
The runtime queries the existing heap under a short lock, returns a checked catalog
index, and releases the lock before generated code selects/calls an implementation.
Unknown/null/untyped receivers fail before invalid access. There are no object vptrs,
collision-based type hashes, mutable per-runtime AOT globals or per-method patch cells.
Argument/receiver snapshots, precise Frame/Slot homes, safepoints, return handoffs
and failure cleanup share existing normal/O2 LLVM lowering.

JIT admits metadata before publishing NativeCode. AOT's private bootstrap admits the
same immutable records after attachment and before Managed execution. The legacy
proof bootstrap remains. The user entry/CLI remains `entry(i32) -> i32`.
The internal AOT harness can namespace independently emitted generations using the
lowerer's existing symbol suffix; this is not a new source entry or managed FFI.

## Reload and tooling

Hierarchy, layouts/owners, lookup, slots/maps, signatures/access/imports, capabilities,
defaults, constructors and catalog changes require restart. Compatible ordinary and
override body changes replace the whole generation. Constructor dependency closure
includes every possible completed-child virtual target. Existing leases protect old
in-flight/callback code; object reachability protects data only.

Shared analysis records bases, virtual slots, overrides and original owners. Inherited
field/method navigation uses original declarations; virtual calls navigate the static
slot, and super navigates its selected implementation. Implicit target dependencies
participate in invalidation without invented written spans. Native LSP tests cover
invalid unsaved overrides, stale-query suppression and repair. Editor highlighting
uses existing colors with contextual extends/virtual/override/super patterns.

## Execution and verification

```sh
build/ci-debug/voidc run examples/inheritance/main.void 10 --no-opt
build/ci-debug/voidc run examples/inheritance/main.void 10
build/ci-debug/voidc run examples/inheritance/initialization.void 10
build/ci-debug/voidc run examples/inheritance/order.void 10
build/ci-debug/voidc run examples/inheritance/receiver_order.void 10
```

Expected: 108, 108, 92, 123602 and 113. The inheritance CLI suite also emits and
separately links nine fixtures in normal/O2 modes. Source, normal/O2 AOT and CLI
suites add four tests to the original 47 (51 total). Dedicated native tests trace
derived-only children through sole Base carriers, inject failures, force collection
and hand an old object to a separately generated new override without a masking host
root. JIT tests verify retirement with live objects, exact catalog reuse and old
active calls retaining their own virtual graph during replacement. WORK_LOG.md lists
actual six-configuration results, stress/stack/editor checks and dispatch measurements.

Hosted Windows Debug/Release, macOS ARM64 Debug/Release and Ubuntu Debug/Release/
Sanitizers remain required. Linux success does not establish hosted platform success.
Interfaces, downcasts/type patterns, richer visibility/hiding, open-world loading,
covariance, abstract/final, implicit constructor chaining, value inheritance,
multiple inheritance, transitive promotion, stateless mixins, finalizers and resource
lifetimes remain deferred. After the hosted gate, investigate structural interfaces
as a separate owner-reviewed phase.
