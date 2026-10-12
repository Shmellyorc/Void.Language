# Phase 13: receiver methods and initialization groundwork

The owner approved M1 + I1 in RECEIVER_METHODS_PROPOSAL.md: methods belong to their
nominal type's owning source module, and public custom initializer execution stays
deferred. This extends Phase 12 value structs without introducing managed types,
source pointers, another type model or another code generator.

## Working syntax

```python
value struct point:
    x: i32
    y: i32

func (p point) move(dx: i32, dy: i32):
    p.x += dx
    p.y += dy

func (p point) distance_squared() -> i32:
    return p.x * p.x + p.y * p.y

func entry(value: i32) -> i32:
    const original = point(x = 2, y = 3)
    var copy = original
    copy.move(value, 1)
    return copy.distance_squared() - original.distance_squared()
```

Receivers use `(name type)` without a colon; ordinary parameters keep `name: type`.
Bodies support the existing statements, functions, receiver calls, typed arguments
and all supported return types, including aggregate returns. There is no `this`.
Method parameters are positional in this slice, with exact types/contextual
literals and no overloading or new implicit conversions.

## Ownership, visibility and lookup

A receiver must resolve to a supported value struct declared in the same source
module/file as the method. Methods can precede their type. The unique method key is
`type_identity::method::method_name`; different nominal types may have the same
method spelling. Duplicate methods or field/method name collisions diagnose.
Ordinary free functions occupy their existing separate callable namespace.

Other modules call through values of imported types:

```python
var p = geometry.point()
p.move(1, 2)
```
Aliased and unaliased direct imports make the owning module's methods available. Public/internal
methods on project-root types are available project-wide; private methods stay
file-private. Mere transitive reachability does not export methods: import their
owner explicitly. Extension declarations on foreign-owned types are deferred,
not permanently forbidden. Unknown, inaccessible and invalid receivers have
structured source diagnostics before lowering.

## Storage, copies and readonly values

A mutating call requires a mutable local/ordinary parameter or a nested field
rooted in one. It operates on that actual storage. Whole receiver assignment,
numeric compounds and field writes update the caller's selected value. Ordinary
parameter passing, local assignment and returns continue to copy; passing the
receiver to an ordinary function does not pass an alias.

A non-mutating method can read readonly locals/module constants and temporaries,
including constructed values and function/method results. Mutating a temporary is
rejected with a suggestion to bind it to `var`. Readonly paths reject mutating
calls, including nested fields and methods whose effects are indirect.

Receiver effects come from validated semantic operations, not a source annotation
or name convention. Direct receiver-rooted writes seed a reverse call graph;
mutating calls on the receiver or its nested fields propagate the effect to
callers. Finite iterative traversal reaches a fixed point, including mutually
recursive methods. Writes to independent local/parameter copies do not count.
This is conservative: even an unreachable receiver write marks the method mutating.
Invalid source never receives a successful analysis or native generation.

Evaluate the receiver once, then arguments once left to right. An addressable
receiver selects its storage before argument effects and the callee observes its
current value after those effects. For example, `p.read(p.change())` observes
`change`'s update. An ordinary aggregate argument is a snapshot taken at its own
evaluation position. Non-addressable receivers materialize one owned temporary.
There are no indexed fields, properties or escaping borrowed receivers in this
slice. Nested inline storage remains selected even if an argument replaces its
containing aggregate. Calls do not perform delayed copy-back that could overwrite
argument-side mutations or unrelated fields.

## Shared representation and lowering

The parser retains receiver expressions until semantic resolution distinguishes
module qualification from value access. Methods use the same Function tree,
nominal ir::Type descriptors, local slots, type checker and expression handlers.
Signature registration precedes every body; effect resolution follows complete
body checking. No recursive call-graph expansion is used.

Backend-independent IR calls optionally carry a typed `Place` consisting of a
local slot and checked ordered field indexes. A method's first logical parameter
is its receiver and aliases local slot zero. Remaining parameters stay values.
The IR validator checks receiver presence, nominal type, field paths, signature
and storage consistency, and rejects writes or mutating receiver calls inside an
IR method marked non-mutating.

Shared LLVM lowering passes an internal pointer to that storage and uses the
same target DataLayout for nested field addresses, aggregate arguments/returns
and debug types. It does not add alias/overflow promises or source-visible pointer
operations. Non-mutating access remains enforced by validated semantics and IR,
without trusting call-site flags. This compiler-private convention is not an
external C aggregate ABI. Generated callees cannot retain receiver storage.

Normal and O2 ORC JIT and linked AOT use this same lowering. Existing failure
context propagation checks a failing method before using its result; source
snapshots and caller traces include the owned method identity. No rollback,
exceptions, allocation or managed runtime is added. Runtime function recursion
remains the application's concern, as for ordinary functions.

## Analysis, tooling and reload

Owned declarations append the receiver_method kind, receiver type and inferred
mutation effect. Method owner is its nominal type; ordinary parameters exclude
the implicit receiver. Type annotations, method calls and receiver field accesses
retain resolved dependency/reference spans. Tooling and native LSP definition/
reference queries use these same records, including unsaved effect changes and
invalid-to-valid revision recovery. Mutable receiver variable names remain outside
the existing local-variable navigation scope.

Exact compatibility includes receiver kind/type, method identity, visibility,
ordinary parameters/result and inferred effect, in addition to existing constant,
layout, module and binding contracts. A body-only edit preserving those contracts
can reload while old leases retain the old module/source. A changed receiver effect,
including an indirectly changed effect, requires restart. Signatures, ownership,
layout/default and dependency changes retain conservative exact rejection. Tooling
metadata summaries never authorize publication. No automatic restart is introduced.

## Initializer groundwork and explicit deferral

Phase 12 recursive safe defaults and named-field construction remain unchanged.
Receiver `init` declarations and `.init()` calls receive VOID2020 rather than
silently behaving as ordinary methods. Custom `deinit` also remains outside scope.
This is an implementation deferral, not a language prohibition. The receiver
model, internal storage call, mutation analysis, method identity and restart
metadata are foundations for a later lifecycle implementation; no partially working
initializer or temporary construction order is exposed now.

Still require design approval before implementation:

- Named-field overrides versus initializer execution order.
- Initializer parameters versus named fields and positional arguments.
- Recursive nested defaults versus explicit child construction/initialization.
- Typed uninitialized bindings and pure constant/default contexts for init-bearing types.
- Lifecycle visibility, constructor rewriting and exactly-once successful publication.
- Initializer body/signature/default compatibility and any future fallible construction.

I2 in the proposal was **not approved**. Its suggested rules are alternatives only,
not the language contract. Runtime field defaults, custom constructors, managed
allocation, fallible result construction and cleanup remain future work.

## Examples and validation

`examples/receiver_methods/` contains five actual source acceptance programs:

| Program | Input | Result / purpose |
| --- | --- | --- |
| main.void | 10 | 301; imported nested mutation, independent copies, params/returns, loops |
| order.void | 0 | 324; receiver/argument once-only order and temporary snapshot |
| recursive.void | 10 | 5; mutually calling mutating methods and readonly recursive reader |
| numeric.void | 10 | 36; all ten numeric types, bool, padded nested storage, aggregate returns |
| failure.void | 0 | Division failure at line 6, with divide/forward/entry source trace |

```sh
build/ci-debug/voidc check examples/receiver_methods/main.void
build/ci-debug/voidc run examples/receiver_methods/main.void 10 --no-opt
build/ci-debug/voidc run examples/receiver_methods/main.void 10
build/ci-debug/voidc emit examples/receiver_methods/main.void -o build/receiver-methods.o
/usr/lib/llvm-19/bin/clang build/receiver-methods.o --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/receiver-methods
build/receiver-methods 10
```

The new receiver_methods_cli suite reuses the existing value-source native test
driver. It verifies both modes/engines, native linking, DWARF, method failure
traces and rejected initialization preserving an existing output. All 22 old
suites remain, bringing the total to 23. Frontend tests add diagnostics, imports,
readonly and temporary rules, whole/nested replacement, evaluation order, effect
cycles, a 180-method SCC, a 400-link depth failure/recovery, reload and malformed
receiver IR. Both small-stack Debug regressions remain. WORK_LOG.md records actual
results and remaining hosted verification.
