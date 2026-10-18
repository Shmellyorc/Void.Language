# Native generics (Phase 24)

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Generic functions, managed structs and value structs use lowercase parameters:

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

## Representation and lifetime

Each original declaration owns its concrete instances within the existing
compilation context. Identical type arguments reuse one instance, including
ordinary recursive calls. Nominal identity, type kind and nullability distinguish
arguments even when layouts match. The same frontend parser, semantic checker,
typed IR and LLVM 19.1.7 lowerer serve JIT and AOT. There are no generic Runtime
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
parser, type-layout and IR limits. Infinite inline value ownership remains
invalid; managed/growable indirection uses the existing rules.

General `t?`, higher-kinded parameters, generic interfaces, method-specific generic
virtuals/initializers, variance, operator/comparable contracts and specialization
sharing optimizations remain outside this foundation. An already nullable managed
type can be supplied as `t`. Named ordinary method arguments retain the existing
language restriction. Method-specific generic selection currently requires a
direct receiver declaration; owner-parameter methods become ordinary concrete
methods and use the established inheritance/composition paths. No future `result`,
`Ok`, dictionary or collection API syntax is activated by illustrative Bible code.

## Regression coverage

`tests/generics/tests.cpp` covers real normal/O2 programs, distinct/reused nominal
instances, recursion, declaration/specialization diagnostics, original navigation,
selected entries, reload, root inspection, GC pressure and allocation-failure
cleanup/retry. `tests/integration/generics.cmake` executes the same source examples
through normal/O2 JIT and independently linked AOT, including 2 MiB fixed arrays.
See the Phase 24 WORK_LOG entry for exact commands and measured results.

Phase 25 applies insert, clear and independent slicing to concrete generic elements
and generic bodies through the same specialization path; see ARRAY_OPERATIONS.md.
