# Structural interfaces — Phase 21

The owner approved IF1–IF8 in INTERFACES_PROPOSAL.md. This guide describes the
local implementation. Both LLVM 19.1.7 SDKs passed Debug, Release and strict sanitizer
configurations (55/55 each), repeated stress and 1 MiB checks. WORK_LOG.md records
the exact results and the pending seven-job hosted Windows/macOS/Linux gate.
LLVM remains 19.1.7. The runtime is a tested foundation, not a production GC or a
public managed FFI.

## Source contract

An interface is an owner-module nominal declaration with nonempty, bodyless
`func` requirements. Requirements inherit its visibility. Explicit argument order,
types, nullability and const qualifications, and the nominal result type must
match exactly. Parameter spelling is not significant. No defaults, overloads,
receivers, lifecycle requirements, bodies or virtual modifiers are permitted.

```swift
interface readable:
    func read() -> i32

struct cell:
    value: i32

func (c cell) read() -> i32:
    return c.value

func inspect(const target: readable) -> i32:
    return target.read()

func entry(value: i32) -> i32:
    var c = cell(value = value)
    const view: readable = c
    return inspect(view)
```

No `implements` declaration or generated source forwarding method is needed.
Conversion uses accessible members of the converting static managed view. Real
fields/methods on its ancestry precede one-hop using candidates. Ambiguity fails
before signature checking: arguments/result types never select a component. A
field cannot satisfy a method requirement. Original ownership and direct imports
remain authoritative. Consumers import the interface owner; an authorized factory
may expose a private implementation through that interface.

Completed managed values convert in assignments, arguments, returns and `as`.
Nullable concrete-to-interface lifting preserves null and requires conformance of
the concrete static view. Unwrap before strengthening nullability. Same-interface
nullable lifts/checked unwraps preserve authority. Different interfaces are nominal;
width conversions, downcasts and dynamic discovery are deferred.

A view stores complete-object identity and a stable binding witness. Different
Base/Derived bindings can refer to the same object but call different nonvirtual
members. Same-interface equality compares object identity, not witness identity.
Virtual members retain Phase 20's dynamic dispatch. Interface views expose only
their requirements, not concrete fields, extra methods or lifecycle calls.

Standalone value erasure/boxing is deferred. Managed owners can satisfy requirements
through inline value components: mutating calls borrow original embedded storage.
Value structs may contain interface leaves, including nullable leaves, and copy
the pair while preserving target identity and independent scalar storage.

## Readonly and construction

P3/RV capabilities remain compile-time facts, not runtime permissions or freezing.
Copies, casts and returns cannot recover mutable authority from readonly views.
Plain interface formals require mutable authority; `const` accepts readonly views.
Independent mutable aliases remain valid.

Each requirement has a conservative project-wide inferred effect/provenance summary
across every eligible static binding and virtual implementation. Hidden receiver
origins rebase to interface-owner authority; explicit argument origins retain their
positions and reference paths. Mutating another argument does not itself classify
the receiver as mutating. A mutating requirement does not poison a separate proven
reader. An unrelated mutating provider of that reader can do so. Empty provider
families use an unknown-call summary, never vacuous readonly certification.

Nullable interface defaults and pure compile-time all-null templates are canonical
`{null, null}`. They are distinct from runtime readonly views. Nonnullable interface
fields require valid supplied values. C1 still requires all value fields valid
before value init; it does not borrow S1's unavailable-field rules. S1 managed
initializers can establish required interface fields and call through complete
children. Unfinished self/aliases cannot erase or dispatch through interfaces.
P4 publishes a complete object once, with no incomplete parent identity.

## Shared implementation

The existing nominal arena records interface kinds and declaration headers. Weak
field edges remain weak; recursive interface signatures do not create inline
layouts or metadata ownership cycles. Source member resolution and independent IR
binding use the same selector, using markers and source ownership/import graph.

IR explicitly represents pack, prepare and invoke. Validation recomputes the
complete eligible binding universe and independently infers effects. Omitting a
nominal type or provider map cannot certify an optimistic readonly call. Headers,
scopes, signatures, routes, access, nullability, preparation identity, definite
initialization, construction and capabilities are checked before LLVM lowering.

Receiver preparation occurs before argument expressions, including short-circuit
control flow. Its private token contains the complete owner, a snapshot of a
managed component or a borrowed inline address, and a generation-local selector.
It cannot enter source fields, function formals or returns. Prepared storage must
be initialized and match the invoked requirement. The selected managed component
and complete owner are roots; the interior address and selector are not.

LLVM uses the same aggregate operations, target field mapper and shared lowering
for normal/O2 ORC JIT and linked AOT. Interface layouts are two pointer words;
Frame/Slot and heap descriptors trace only the object word, including nested inline
values. Prepared roots use the existing Frame/Slot system. There is no conservative
scan, alternate allocator, second root registry or interface heap.

Runtime metadata admission atomically publishes one immutable snapshot containing
nominal descriptors and nonallocatable witness data. Admission runs in NativeSafe,
validates domains, and rolls back conflicts/allocation failures. Witnesses own only
strings/data, with no AST/LLVM pointers, JIT addresses, generation indexes, leases
or permission tags. Opaque membership is checked before dereferencing a witness;
complete-object ancestry and current generation binding keys are guarded.

Preparation/invocation plans belong to the leased generation. Invocation reuses
ordinary functions and the existing virtual dispatchers. Runtime metadata survives
code retirement without retaining managed targets or allocating generations. Reload
compatibility freezes requirement/binding/access/effect/layout/default/construction
changes; compatible ordinary provider bodies can replace a whole generation.
Constructor dependency closure includes all eligible interface and virtual providers.
Unknown/domain-incompatible witnesses fail through existing checked failure cleanup.

The pair/preparation layouts and user aggregate calling convention are compiler
private. A two-pointer C++ struct is not an arbitrary external interface ABI. Native
cross-generation tests pass an old typed holder pointer, then read its interface
field through generated code.

## Tooling, limits and examples

The shared analysis distinguishes interface declarations and requirements, records
provider/declaration dependencies, and navigates calls to requirements. Binding
routes and the provider universe participate in compatibility/invalidation. The
extension uses existing keyword colors and indents interface declarations.

Binding discovery is finite: at most 65536 scope/view/interface combinations and
8388608 member-inventory visits per discovery/conversion. Exceeding either produces
a source resource diagnostic; candidates are never truncated. The independent IR
validator applies the same discovery budgets. Existing physical depth (128),
expanded layout (65536) and S1 specialization limits remain unchanged. Effect
inference is a monotone fixed point over these finite bindings and existing formal
reference paths, not runtime instances or permission-mask specialization.

`examples/interfaces/` contains real programs for promotion, nullable carrier copies,
private factories, static binding/identity, selected receivers, interface cycles,
nested aggregate handoffs and GC stress. Use the existing CLI:

```text
voidc run examples/interfaces/main.void 10
voidc run examples/interfaces/static.void 10 --no-opt
voidc emit examples/interfaces/selection.void -o selection.o
```

The native test family links generated objects independently against the runtime,
with the existing Windows CRT selection source. It adds source, normal/optimized
AOT and CLI suites; the 51 earlier suites and seven hosted jobs are retained.

Deferred: standalone value/interface representation, boxing, interface inheritance,
generics/Self, defaults/bodies, overloads, dynamic type tests, downcasts/width casts,
managed/interface FFI, open-world providers, transitive using, finalizers and runtime
permission systems. Root homes can retain dead values until function return, as in
the existing GC foundation; this phase does not add a liveness optimizer.
