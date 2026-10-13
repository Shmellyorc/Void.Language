# Phase 19: stored composition and member promotion

The owner approved U1–U7 in [COMPOSITION_PROPOSAL.md](COMPOSITION_PROPOSAL.md),
including named and unnamed embeddings. This guide describes the implementation;
actual validation results and hosted limitations are recorded in WORK_LOG.md.
LLVM remains pinned to 19.1.7. The single frontend, semantic model, shared IR,
LLVM lowering and coordinated runtime remain authoritative.

## Source surface

```swift
value struct movement:
    x: i32

func (m movement) move(amount: i32):
    m.x += amount

struct combat:
    health: i32

func (c combat) hit(amount: i32):
    c.health -= amount

struct player:
    using motion: movement
    using combat
```

`using motion: movement` stores a field named motion. `using combat` stores a field
named combat; an imported `using geometry.movement` likewise derives the final
nominal spelling movement. Both forms accept existing pure declared field defaults
and have exactly the same storage/promotion rules. `using` is contextual in field
syntax; `using: movement` remains an ordinary field named using. Existing `use`
imports are unchanged.

Both managed and value owners can embed nonnullable managed or value struct fields.
Primitive/nullable using targets diagnose; nullable ordinary fields still work with
explicit checked unwrap. Multiple using fields are permitted. Duplicate physical
names, including implicit/explicit collisions, diagnose at declaration. Distinct
names can embed the same type more than once.

Fields and methods can be reached through qualification (`p.motion.x`,
`p.motion.move(2)`) or unique promotion (`p.x`, `p.move(2)`). A direct field or owner
method wins in one member namespace, even if its type/signature does not fit the
requested operation. There is no fallback to a promoted candidate after an error.
Overlapping promoted names do not invalidate the containing declaration. Attempting
ambiguous access diagnoses at its source token, with qualified candidates and
related embedding/member locations. Field types, argument signatures and expected
results never choose a component; i32 x versus f32 x remains ambiguous.

Promotion is **one-hop**: only the component's direct fields/methods participate.
Nested qualified access can perform a fresh one-hop selection at each written dot.
Automatic transitive promotion is deferred, not permanently prohibited. Methods
retain their original nominal/module identity and existing visibility/import rules.
Import an embedded method's owner explicitly where required; wrapper imports do
not re-export private or transitively reachable methods. No extension methods,
forwarding methods, new overloads or stateless mixins are generated.

## Storage, receivers and readonly capabilities

Using adds no object, storage alias or flattened field. Value assignment copies
inline storage; embedded references keep their existing target identity. Managed
owner assignment aliases the owner. Promoted mutating methods select actual embedded
storage; they do not pass an accidental value argument copy or perform delayed
copy-back. Receiver/target selection precedes argument/RHS effects and retains the
original selected managed owner across allocation/collection. Argument snapshots
and callee observations retain the existing qualified-access evaluation rules.

P3 and RV1–RV7 are unchanged: copying const value storage into var permits independent
inline mutation without restoring authority over readonly referents. Mixed reference
leaf profiles remain distinct through promoted access, helpers, constructors and
returns. Existing mutable binding/managed-field capacities still reject readonly
laundering. Independent mutable aliases stay usable. Promoted and qualified paths
undergo the same inferred effects and capability checks.

## Construction and publication

Using fields are ordinary stored fields in declaration order. Supplied constructor
expressions run once in written order. Omitted fields use current safe/pure defaults
and nested value construction. No-init named-field construction uses the physical
name, including the implicit name; promoted names are not constructor labels.
With custom init, labels bind exclusively to initializer parameters.

C1 still requires all value fields valid before value init. S1 direct required
managed fields must be assigned complete references before promoted subfield reads,
writes or method calls. Promotion cannot initialize an unavailable child implicitly
or evade construction-view escape/readiness restrictions. P4 final complete-object
commit and failure output protection are unchanged. Init runs exactly once through
the existing construction path; `.init()`/`.deinit()` remain invalid lifecycle calls.
Pure templates and runtime readonly views retain RV5; using does not enable runtime
work in compile-time defaults.

## Shared implementation, tooling and reload

A source field marker is exposed as AnalyzedDeclaration::using_field; nominal
physical field descriptors and native layout are unchanged. One semantic selector
serves field reads, assignments, calls and contextual type anchors. It returns an
original member and ordered physical field indexes, without recursive promotion.
Reads and receiver calls elaborate to typed ordinary field chains; assignments
carry expanded indexes with separate implicit navigation edges. Existing capability
and construction analysis then consume those same paths.

No IR opcode, verifier permission, LLVM layout/GC path or runtime API was added.
Existing independent IR validation still rejects invalid receiver paths/effects
and readonly/construction violations. Frame/Slot roots and target descriptors see
ordinary physical nesting. The GC does not know that a source field was using.
Debug metadata describes only stored fields; aggregate ABI remains compiler-private.
Managed reachability remains separate from JIT generation leases.

Written promoted tokens navigate the original field/method. Synthetic embedding
edges are dependencies, not fake written references. Shared analysis includes the
using marker in exact declaration/layout/constant contracts; tooling compares it.
Changing composition requires restart even if physical bytes remain identical.
Adding/removing owner members or changing imports/effects is reanalyzed under current
conservative compatibility checks. Compatible ordinary body edits still reload;
initializer dependency/restart policies and retained code leases remain unchanged.
Invalid unsaved promotion suppresses stale navigation until repaired.

## Tests and example execution

Four new suites extend the original 43 to **47**: composition_source,
composition_aot_normal, composition_aot_optimized and composition_cli. Existing
assertions are retained. Real programs in examples/composition cover both forms,
inline copies/shared references, nested qualified access, staged init, imported
methods, selection/evaluation order and cyclic aggregate-root GC stress.

```sh
build/ci-debug/voidc run examples/composition/main.void 10 --no-opt
build/ci-debug/voidc run examples/composition/main.void 10
build/ci-debug/void_composition_cli_main_normal 10
build/ci-debug/void_composition_cli_main_optimized 10
```

All four should print **111**; the last two are separately linked source executables
with runtime-only dependencies. receiver_order.void returns 103 at input 10 while
replacing the containing reference and forcing allocations during argument evaluation.
The selected old embedded receiver must remain alive and mutate correctly.

The stress proof returns 2024 with 1002 constructions at input 10. It observes
precise aggregate-only roots with no unrelated host handles, forces collection,
checks deterministic worker/collector overlap, verifies failed rendezvous never
sweeps, and verifies allocation-failure cleanup/recovery. Linked normal/O2 AOT uses
the same production runtime ABI, with existing test boundary instrumentation.
Independent invalid IR tests forge receiver paths/effects. The real LSP protocol
adds imported promoted read/write/method definitions, ambiguity and unsaved repair.
Editor presentation highlights contextual using with existing keyword colors;
lifecycle behavior is unchanged.

Deferred: automatic transitive promotion, inheritance/virtual dispatch, structural
interface execution, has, method-only mixins, nullable promotion and new field
visibility syntax. No subtype/conversion is conferred by composition. All existing
source resource limits, native-provider safety and C1/S1/P3/P4/RV contracts remain.
The seven hosted jobs must verify this phase after the owner pushes; Linux local
results cannot establish Windows/macOS or unavailable VSCodium execution.
