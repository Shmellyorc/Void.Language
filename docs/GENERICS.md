# Native generics (Phase 24)

## Conditional generic declarations and bodies (G87)

`compile if` selects source once for the immutable target and owning-project
configuration, before generic declaration capture or specialization. Excluded types,
methods, interface requirements and body statements never enter generic syntax or
semantic lookup. Every specialization sees that same selected template. Predicates
cannot inspect generic parameters, ordinary values or `has`; those retain ordinary
language semantics. Changing conditions requires recompilation and a Runtime restart,
including when selected types retain their written names. See [G87](projects/G87_CONDITIONAL_COMPILATION.md).


## Generic async composition — G86

Async functions and concrete managed generic receiver methods specialize through
the ordinary exact generic/authority pipeline. G86
[async iteration](CUSTOM_ITERATION.md#async-iteration--g86) supports concrete generic
cursors and collection constraints/interface views whose synchronous factory
returns concrete cursor state. The
[sequence module](../examples/async/advanced/sequence.void) demonstrates a reusable
constrained async `collect`; the
[workflow](../examples/async/advanced/workflow.void) combines generic typed results
inside an existing structured task group. Independent jobs start before their
awaits. Typed errors do not automatically cancel siblings.

`task<T>` in tooling describes an inferred opaque handle; it is not new source
type syntax. Independent generic protocol method parameters cannot be inferred
from a loop header, and erased async interface requirements, async function
values, callable task transfers and async value receivers remain unsupported.

The [28 October generic operator amendment](collections/GENERIC_OPERATORS.md)
allows public operators on exact substituted public nominal owners, with verified
IR specialization provenance. Ordinary code keeps the direct-import restriction;
private/internal visibility and exact matching remain unchanged.

**Stage 8 Sprint 2 update (20 October 2026):**
[The shared calls/numeric contract](STAGE8_CALLS_NUMERIC.md) supersedes older
ordinary named/default-call exclusions below. Existing constructor, readonly,
receiver and specialization protections remain. See
[actual local verification](stage8/SPRINT2_ACCEPTANCE.md) for acceptance status.

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

G78 also specializes tagged and numeric/flags enum declarations through the same
machinery, including payload inference, matching and collections. See [enums](ENUMS.md).

G83 derives collection equality inside the existing concrete specialization,
retaining the checked caller context for exact substituted-owner operators and
private access. It does not grant unrelated callers access or add hash callbacks.
See [collection equality](COLLECTION_EQUALITY.md).

G82 extends the same specialization machinery to structural interfaces. See
[generic interfaces](INTERFACES.md#generic-interfaces-g82).

Generic functions, managed structs, value structs, enums and interfaces use lowercase parameters:

```typescript
func identity<t>(value: t) -> t:
    return value

struct box<t>:
    value: t

func (b box<t>) get() -> t:
    return b.value

func entry(value: i32) -> i32:
    var held = box(value = identity(value))
    var numbers: [i32] = [held.get(), 5]
    return identity(numbers)[0]
```

Input 7 returns 7. Explicit arguments (`identity<i32>(value)`,
`box<i32>(value = value)`) and nested applications (`box<box<i32>>`) work.
Constructor inference uses supplied fields or initializer parameters. Function
inference uses ordinary argument types and nested array/type patterns. Typed
arguments anchor contextual numeric literals; conflicting types diagnose rather
than widen. Empty literals, null and return-only parameters require explicit
arguments. Existing argument evaluation and exact conversion rules still apply.

Receiver owner parameters bind directly from `box<t>`; do not repeat `<t>` after
`get`. Methods may introduce distinct parameters, for example
`func (b box<t>) choose<u>(other: u) -> u:`. Parameterless `t()` in a body uses the
concrete struct's ordinary constructor, including its defaults and required-field
checks. It does not invent constructors for numbers, arrays or nullable types.

## Checking and constraints

The owner approved specialization-time body checking. Syntax, declaration names,
parameter structure, type annotations and interface constraint declarations are
validated immediately. Dependent body types and operations are checked when used,
through the ordinary semantic checker. A specialization error includes the
original body location and requesting call/declaration locations. An unused body
can contain an operation unsupported by some potential arguments.

`func read<t: readable>(value: t) -> i32:` requires an accessible non-nullable
interface `readable`. Its concrete argument may be a managed struct, value struct
or existing interface reference. The shared structural member selector and exact
signature/access checks apply before specialization acceptance. Constraints do
not box values, change representations or create interface conversions. Bodies
may use additional operations supported by the concrete type. Existing readonly
and receiver-effect checks still apply to every concrete call; constraints do not
provide mutable authority.

## Specialized interface parameters and constraints

```typescript
interface reader<t>:
    func read() -> t

func inspect<t>(const input: reader<t>) -> t:
    return input.read()

func read<t, s: reader<t>>(seed: t, const input: s) -> t:
    return input.read()
```

`inspect(view)` infers `t` from an already typed `reader<i32>` view. Use
`inspect<i32>(concrete_provider)` when passing a structurally conforming managed
object directly: inference does not search arbitrary concrete methods to guess
interface arguments. `read(0, provider)` infers both arguments from its ordinary
parameters, then checks `provider` against `reader<i32>`. A constraint alone does
not infer an otherwise unbound parameter.

Generic interface and struct parameters may have specialized interface constraints,
including nested arguments and self-referential structural constraints. Generic
struct methods may use owner parameters in
method-specific constraints. Constraints retain the owner's access context and
existing exact signatures; they do not grant erased value boxing or mutable
access. Invalid constraints and unused malformed requirement headers diagnose
through the normal frontend.

## Representation and lifetime

Each original declaration owns its concrete instances within the existing
compilation context. Identical type arguments reuse one instance, including
ordinary recursive calls. Nominal identity, type kind and nullability distinguish
arguments even when layouts match. The same frontend parser, semantic checker,
typed IR and LLVM 23.1.4 lowerer serve JIT and AOT. There are no generic Runtime
objects or new allocation protocols.

Concrete fields derive their layout, tracing and initialization from existing
facilities. Fixed arrays retain independent snapshots; growable arrays and
managed references retain shared identity. Readonly capability propagation,
precise Frame/Slot roots, safepoints, transactional publication and cleanup retain
the existing contracts. Generic imports retain original module access and native
linkage. Host-selected entries and compatible body-only hot reload use existing
exports, generation leases and compatibility checks.

Source navigation publishes original declarations and type parameters once,
with references from concrete uses mapped to those original source spans. Unused
templates expose header/type-annotation navigation; body semantic references
become available when a concrete instance is checked. Native
instance identities remain in the ABI compatibility contract; changing the
concrete instance set can require a restart under the existing conservative
reload policy.

## Bounds and exclusions

There is no fixed type-parameter count or array-byte ceiling. Pathological
expansion uses the existing diagnostic/resource model: dependency depth 128 and
8,388,608 characters of specialization identity work per compilation, plus normal
parser, type-layout and IR limits. Generic interface requirement graphs are
prepared iteratively through the existing declaration lists; expanding graphs
receive the same depth diagnostic without growing the native stack per interface.
Infinite inline value ownership remains
invalid; managed/growable indirection uses the existing rules.

G79 implements general `t?` and nullable concrete arguments through the same
specialization/inference machinery; see [nullable types](NULLABLE_TYPES.md).
Higher-kinded parameters, method-specific generic
virtuals/initializers, variance, operator/comparable contracts and specialization
sharing optimizations remain outside this foundation. Any supported nullable
type can be supplied as `t`. Ordinary named/default method arguments use the Stage 8 Sprint 2 shared binder. Method-specific generic selection currently requires a
direct receiver declaration; owner-parameter methods become ordinary concrete
methods and use the established inheritance/composition paths. Later source
dictionaries and Stage 4 results/function types use these same concrete
specializations. Explicit generic function references and context-inferred generic
method callbacks are covered in ADVANCED_LANGUAGE.md; no parallel generic checker
or polymorphic Runtime representation is introduced.

## Regression coverage

`tests/generics/tests.cpp` covers real normal/O2 programs, distinct/reused nominal
instances, recursion, declaration/specialization diagnostics, original navigation,
selected entries, reload, root inspection, GC pressure and allocation-failure
cleanup/retry. `tests/integration/generics.cmake` executes the same source examples
through normal/O2 JIT and independently linked AOT, including 2 MiB fixed arrays.
See the Phase 24 WORK_LOG entry for exact commands and measured results.

Phase 25 applies insert, clear and independent slicing to concrete generic elements
and generic bodies through the same specialization path; see ARRAY_OPERATIONS.md.


## Stage 8 Sprint 6 — 21 October 2026

[The bounded ergonomics contract](STAGE8_ERGONOMICS.md) supersedes historical
blanket exclusions above for compile-time type `has`, typed if-expressions,
multiline calls and string-only interpolation. Existing nominal storage,
structural conformance, generic specialization, capabilities and checked failure
rules remain authoritative. Both conditional branches type-check. Formatting and
numeric/bool/custom string conversion remain deferred. Approval and implementation
are separate from local acceptance, Wrench review and the final hosted lock.

## Static members of generic owners

G80 gives each concrete generic struct specialization independent static fields.
`storage<i32>.item` and `storage<string>.item` do not share a cell. Static methods
bind the owner's declared parameters without a receiver and may add distinct
method-specific parameters. Generic declarations retain symbolic editor types;
concrete uses retain nominal identities and concrete signatures. See
[Static members](STATIC_MEMBERS.md#generic-owners) for initialization and access rules.

G84 preserves constructor inference and specialization for fallible init. If the
selected initializer declares `result<bool, e>`, the ordinary constructed call is
`result<ConcreteOwner, e>` with the substituted error type. This result participates
in enclosing generic call inference; its owner is not mistaken for an already
unwrapped value. Error-only/return-only parameters still need explicit type
arguments when ordinary argument inference cannot determine them. `?` requires
the exact substituted error type. See [initialization](INITIALIZATION.md#typed-fallible-initialization-g84).

## Custom iteration (G85)

Generic collections can return concrete generic iterator structs from `iterator()`
and `reverse_iterator()`. The selected `next() -> option<T>` infers the loop item,
and `index()` infers the optional second binder. Structural interface constraints
can describe factories with substituted concrete iterator results; normal owner
access, specialization limits and readonly provenance still apply. There are no
new runtime generic objects or iterator-specific inference rules. See the
[iteration guide](CUSTOM_ITERATION.md) and [runnable generic collection](../examples/custom_iteration/main.void).
