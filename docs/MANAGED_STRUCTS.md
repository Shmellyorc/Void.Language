# Phase 16: managed structs

Implemented locally using the owner-approved P1–P7 contract in
[MANAGED_STRUCTS_PROPOSAL.md](MANAGED_STRUCTS_PROPOSAL.md), including P3 deep
readonly views and P4 complete-object publication. The owner reports hosted run #28
passed all seven jobs (35 suites), locking Phase 16. Phase 17 adds approved
[managed initialization](MANAGED_INITIALIZATION.md); its hosted validation remains
separate. Phase 15's coordinator remains the runtime
foundation; ordinary source programs now use it through the shared compiler path.

## Working source surface

```swift
struct node:
    number: i32
    next: node?

func (n node) increment():
    n.number++

func read(const n: node) -> i32:
    return n.number

func entry(value: i32) -> i32:
    var first = node(number = value)
    var second = node(number = value + 1, next = first)
    first.next = second
    var alias = first
    alias.increment()
    const view = first
    var still_readonly = view
    return read(still_readonly) + (first.next as node).number
```

Ordinary structs have reference identity. Assignment, arguments and returns copy
the reference; `==`/`!=` compare identity within one nominal type. Independent
`value struct` assignments still copy their fields. Numeric/bool fields and nested
reference-free value fields are supported alongside direct managed-reference
fields. Self-references and finite mutually recursive reference graphs are valid;
infinite inline value layouts remain invalid. An empty managed declaration may
have an empty body before the next top-level declaration; no `pass` keyword was
introduced. Every construction has distinct identity, including empty objects.

`t?` denotes a nullable managed reference. `null` is contextual and requires an
expected nullable type. A `t` can be assigned to `t?`; `(reference as t)` checks a
same-nominal nullable reference before use and reports a source runtime failure
for null. Nullable field/method access requires explicit unwrap. There are no
implicit nullable dereferences, unrelated reference casts or nullable primitives.

Types without custom init use named-field construction. Supplied expressions run once in source order. Omitted numeric/bool fields
use established safe/pure declared defaults; nested value fields use the existing
Phase 14 construction plan, including approved value `init` execution. Nullable
reference fields default to null. Non-nullable reference fields must be explicitly
supplied: neither an omitted field nor `var object: node` silently allocates an
object. Value initializers may accept reference arguments without storing references
in their value layout. Phase 17 custom initializers instead bind arguments to init parameters and use
compiler-checked staged construction views before complete commit. Reference-containing
value structs remain deferred, rather than permanently prohibited. Compile-time constants
and pure declared defaults cannot allocate managed objects or execute initializers.

Owner-module methods, imports, nested inline field access, compound assignment,
parameters and reference returns use the existing resolution/lowering path.
Reference receiver rebinding changes its local copy; it does not replace the
caller's binding. A heap receiver or assignment target is selected before
argument/RHS evaluation. Thus changing the containing reference during an argument
does not redirect the selected receiver. Nested inline value mutations preserve
unrelated fields changed by the RHS and update the originally selected object.

## Readonly capabilities

`const object = mutable_object` creates a deep readonly view, without freezing the
object. An independently held mutable alias can still mutate it. `var alias =
object` retains readonly access; `: node` and `as node` cannot remove that capability.
Readonly access propagates through nested references, returned aliases and helper
calls. Copying a reference-free value field produces an independent value copy.

Ordinary reference parameters accept mutable capabilities. Use existing
`const parameter: node` syntax for a readonly formal. A readonly actual cannot be
passed to a mutable formal, stored into mutable reference fields, or used through
a mutating receiver. Effects for receiver mutation, reference retention and
returned aliases are inferred across functions/modules, including disguised
mutation through aliases and helper calls. No ownership annotations or runtime
permission checks are required. An existing mutable-reference binding cannot be
assigned a readonly view; a newly declared binding can infer the view. Rebinding
an inferred readonly view to a mutable reference preserves its readonly capability.

Capability analysis is conservative and monotone over the validated typed AST.
Control-flow joins and rebinding retain possible parameter origins; functions
returning readonly on any path expose a readonly result. This may reject a call
whose safety would require path-sensitive alias reasoning. It does not freeze an
object, duplicate nominal type checking, or claim a borrow checker. Capability
and inferred effect contracts participate in analysis/tooling invalidation and
reload compatibility. There is no arbitrary external managed-reference ABI.

## Shared representation and safe publication

The existing nominal type model distinguishes managed references, inline values,
nullable references and compile-time capabilities. An owning type table with weak
nominal field edges supports cyclic metadata without shared-ownership leaks.
Descriptors queried after analysis destruction retain their table. Inline layout
validation remains separate from cyclic reference-graph validation and preserves
the existing resource bounds.

The frontend stages supplied fields and nested default construction in existing
rooted IR temporaries. `gc_construct` carries the complete field set. Shared LLVM
lowering builds a target-layout payload, then calls `Runtime::construct_into`.
That helper validates the precise output root and referenced objects, performs any
budget-triggered collection and polling before the allocation critical section,
and reuses `Heap::allocate`. Under the existing heap lock it copies all prepared
bytes and publishes the output reference last. All fallible descriptor/map work
precedes publication; there is no user code, safepoint or throwing operation
between successful allocation and complete publication. Failure leaves the output
root and existing aliases unchanged. No incomplete parent becomes a GC root.
Already complete staged children remain rooted while a later parent allocation
fails and are subsequently collectible.

The same reference-offset flattening and registered root frames serve optimized
and unoptimized JIT/AOT. Selected heap receivers, intermediate values, arguments
and returned references survive allocation and loop safepoints. Managed functions
keep Phase 15 failure cleanup, thread state and code leases. Runtime numeric
layout descriptors and external handles outlive JIT generations independently;
managed reachability never substitutes for a code-generation lease.

Source runtimes use an internal allocation-count budget of 256 since the last
successful collection. Tests use budgets 1 and 8 to force collection/coordination.
A failed rendezvous never sweeps; source construction reports failure and unwinds
through the existing checked failure channel. The budget is a foundation heuristic,
not a production memory policy or source-level GC API. Private Phase 15 proofs
retain their explicit runtimes and manual-collection default.

## JIT and linked AOT

`entry(i32) -> i32` and the existing CLI remain unchanged. Source JIT owns a runtime
when needed and compatible replacement reuses it. Managed-source AOT's public
adapter uses the runtime bootstrap, attachment and Managed scope; separately
linked executables require `void_runtime` and platform thread/C++ dependencies.
The runtime library has no LLVM dependency.

```sh
build/ci-debug/voidc check examples/managed_structs/main.void
build/ci-debug/voidc run examples/managed_structs/main.void 10 --no-opt # 23
build/ci-debug/voidc run examples/managed_structs/main.void 10          # 23 (optimized)
build/ci-debug/voidc emit examples/managed_structs/main.void -o build/managed.o
clang++-19 build/managed.o build/ci-debug/libvoid_runtime.a -pthread \
  --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/managed
build/managed 10                                                  # 23
```

On Windows use the configured MSVC/SDK library environment and
`legacy_stdio_definitions.lib`, as in the existing native printf proof. CMake's
managed CLI integration targets link the same required library. There is no new
entry-point discovery rule, public aggregate ABI, thread API or managed export API.

## Verification and remaining boundaries

Four suites extend the previous 31 without removing their assertions:

- `managed_source`: parsed normal/O2 execution, alias/copy/capability diagnostics,
  cyclic metadata, imports, effects, selection/evaluation order, null errors,
  live-object reload, heap ownership and deterministic generated loop rendezvous.
  Budget-driven and forced collections independently test reachability; injected
  allocation/map/collection failures check unchanged output and recovery. Four
  source workers share one runtime under an allocation budget.
- `managed_aot_normal`, `managed_aot_optimized`: real emitted source objects linked
  separately with runtime-only dependencies; complete payloads, roots, automatic
  collection, captured cyclic graphs, injected allocation failure, protected output
  and recovery are checked at the runtime boundary. Runtime bootstrap allocation
  failure and subsequent attachment/shutdown recovery use the real compiled entry.
- `managed_cli`: independent fixtures through normal/O2 JIT and actual linked
  AOT, including nested receivers, construction order, all ten numeric field
  types, imports and source-located null failure/caller output.

The runtime-boundary probe forwards to the real ABI; it neither replaces GC nor
manually reconstructs source IR. It compares committed bytes with prepared bytes,
forces collections, gates actual safepoints and injects allocations deterministically.
The existing LSP protocol suite additionally checks imported managed fields,
methods/self-types, readonly edit diagnostics and repaired/stale navigation.
Editor presentation adds the implemented `null` literal and managed indentation.
Colors and extension lifecycle are unchanged. WORK_LOG.md records actual runs.

Still deferred: source reference-containing value structs; managed compile-time allocations; broader
nullable patterns/flow narrowing; managed FFI; weak refs/finalizers/deinit;
inheritance/interfaces/generics; strings/collections; async/thread APIs; production
collector tuning. Managed debug metadata currently describes opaque reference
pointers, rather than a rich object inspector. Phase 15 callback-provider quiescence
and native-safety contracts remain in force. Windows/macOS hosting and VSCodium
execution cannot be inferred from Linux results.

Phase 17 implements approved custom managed initialization without incomplete
parent identity or roots; see MANAGED_INITIALIZATION.md. Recommended next design
work is reference-containing value capabilities and their copy/readonly/root rules.
