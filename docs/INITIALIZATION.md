# Phase 14: custom initialization for value structs

**Stage 8 Sprint 2 update (20 October 2026):**
[The shared calls/numeric contract](STAGE8_CALLS_NUMERIC.md) supersedes older
ordinary named/default-call exclusions below. Existing constructor, readonly,
receiver and specialization protections remain. See
[actual local verification](stage8/SPRINT2_ACCEPTANCE.md) for acceptance status.

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Phase 20 applies owner-approved I5–I6 to managed inheritance: mandatory first super.init preludes, base-to-derived preparation and one most-derived complete commit. C1 value initialization remains unchanged. See [INHERITANCE.md](INHERITANCE.md) for the current boundary and WORK_LOG.md for actual verification.

Phase 16 managed source is described in [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md).
Phase 17 custom managed init follows approved S1 + MI1–MI7; see
[MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md). The value-specific rules
below remain unchanged: required value children complete before the body, and
`var p: point` performs value default construction. Managed nonnullable bindings
still require explicit construction; direct required managed fields use Phase 17
readiness proof instead. Phase 18 reference-containing values preserve C1 and infer
fixed fresh-initializer leaf profiles; see REFERENCE_VALUE_STRUCTS.md.

The owner approved C1 in INITIALIZATION_PROPOSAL.md, including recursive nested
construction, required-child availability, visibility, conservative initialization
reload dependencies and the specific obsolete Phase 13 test updates. This document
describes implemented source behavior; final verification is recorded in WORK_LOG.md.
Fallible constructors and resource lifetimes remain separate future work. LLVM remains pinned to 19.1.7 with the existing SDK-wide RTTI policy.

## Source interface

```python
value struct point:
    x: i32 = 4
    y: i32

func (p point) init(x: i32 = 3, y: i32 = 10):
    p.x += x
    p.y = y

func entry(value: i32) -> i32:
    const a = point(y = 2, x = value)
    var b: point
    var copy = a
    copy.x++
    return a.x + b.x + copy.x
```

One initializer belongs to the type's owning source module. It has explicitly typed
parameters and no result annotation. Empty return or reaching the end completes
construction. Initializers use the same supported numeric, Boolean and nested
nominal value types, statements, methods and diagnostics as ordinary functions.

With custom init, constructor arguments refer **only to initializer parameters**.
Without init, Phase 12 named-field construction remains available, with positional
field arguments rejected. There is no combined parameter/field override namespace,
no overload selection and no automatic fallback when arguments are incorrect.

A positional prefix may precede named arguments in any written order. Positional
after named, unknown labels, duplicate/double binding, excess or missing arguments
and incorrect types diagnose. Required parameters precede defaulted parameters.
Initializer parameter defaults are pure typed compile-time expressions resolved
in the owner module, using the existing evaluator/numeric rules. Receiver/parameter
references, calls and runtime construction are unavailable in defaults. All declared
defaults are checked, including unused ones. Ordinary named/default function and
method calls and inferred parameter types remain deferred.

## Evaluation and nested construction

Supplied arguments evaluate exactly once, left to right in written order. Their
values are saved before binding/reordering parameters or beginning field construction.
Omitted pure parameter defaults add no runtime effects.

Fresh fields initialize in declaration order: copy a declared pure default, otherwise
default-construct the field. Numeric/Boolean defaults remain zero/false. Nested
value construction completes its own children and initializer before the next
parent field. The parent initializer then runs once on the prepared storage. A
readonly destination becomes readonly only after successful construction.

Without a custom initializer, explicit named fields supply completed values;
omitted fields follow those same defaults. An explicitly supplied child is not
first default-constructed and discarded. Assignment, ordinary arguments, returns
and copies never rerun init. Replacing a child in a parent initializer is an
ordinary assignment and does not undo earlier initialization effects.

A custom type is default-constructible only if its initializer's arguments can all
be omitted and its fields have defaults. `var p: point` means the same construction
as `point()`, including visibility, nested effects and failures. No zero-value path
bypasses init.

If a child requires constructor arguments, a no-init wrapper can still be used as
`wrapper(child = child(9))`. Omitting that child or default-constructing the wrapper
fails with a required-parameter diagnostic. An enclosing custom initializer needs
all its fields safely constructed before its body starts; it cannot defer a required
child to a body assignment. That declaration diagnoses at the field, with the child
parameter identified. Layout/type declarations and ordinary parameter use remain
valid even when default construction is unavailable. Runtime field defaults and
definite-initialization analysis are deferred, not permanently prohibited.

## Purity, access and lifecycle boundaries

Module constants, declared field defaults and parameter defaults cannot execute
initializers, including implicit nested initialization. The diagnostic identifies
that runtime requirement; the compiler does not silently substitute zeroes. Pure
construction without initializers retains existing compile-time behavior. Local
runtime const values can execute initialization and are readonly afterward.

Initializer visibility follows existing public/internal/file-private rules in
addition to type access. Private initializers support public owner factories.
An implicit field construction checks access in the field declaring module: public
wrappers can encapsulate private child construction accessible to their owner,
while another module cannot bypass that private constructor through its own field.

Direct `.init(...)` and type-qualified lifecycle calls are rejected even in the
owner/body. An unrelated free function named init retains its ordinary meaning.
Custom deinit, initializer result annotations and foreign extension initializers
remain unsupported and produce source diagnostics.

Construction uses private fresh storage and publishes only after successful
completion. Argument, nested initializer and body failures propagate existing
source-located arithmetic/conversion errors; remaining steps do not execute. A
failed new value does not overwrite an assignment destination. Earlier argument
effects are not rolled back. There is no general exception, destructor, rollback
or fallible result construction system. Those future features need their own
approved contract and must preserve the successful-publication boundary.

## Compiler, tools and reload

The existing Function/Parameter/nominal Type model records initializer identity,
pure defaults and resolved argument/field mappings. Bounded construction plans
separate structural constant storage from runtime default availability. The same
argument checker applies exact contextual types; no alternative frontend exists.

Lowering saves argument snapshots in typed slots, prepares nested aggregates and
uses Phase 13 receiver Places for initializer calls through the shared LLVM backend.
IR signatures/calls mark initialization explicitly and validate it separately from
ordinary receiver invocation. Failure checks precede use/publication of the result.
No source pointers, heap mechanism or public C aggregate ABI was added.

Shared analysis exposes initializer declarations, parameter names/types/source and
default spans, constructor references and implicit dependency edges. Implicit
construction edges contribute to compatibility/invalidation but do not replace
written navigation locations. Native LSP definition/references navigate imported
constructors; invalid unsaved arguments suppress stale queries until repaired.

Exact compatibility includes initializer presence/owner/visibility, parameter
names/order/types/default token syntax, receiver effect and initializer body syntax.
All functions/methods transitively called from any initializer have their body
syntax included too, using an iterative closure that handles cycles. Changing an
initialization helper therefore requires restart just like changing init itself.
Existing field/default/binding/layout contracts remain conservative. Unrelated
compatible function/method bodies can reload and old leases retain their code.
Comments/whitespace do not constitute body policy changes. No automatic restart,
value migration or reinitialization occurs.

## Real source examples and execution

`examples/initialization/` contains imported types/private factories, named/default
order, required children, all ten numeric types/Boolean defaults and distinct
argument/implicit-child/body failures. For main.void, input 10 returns 166:

```sh
build/ci-debug/voidc check examples/initialization/main.void
build/ci-debug/voidc run examples/initialization/main.void 10 --no-opt
build/ci-debug/voidc run examples/initialization/main.void 10
build/ci-debug/voidc emit examples/initialization/main.void -o build/initialization.o
/usr/lib/llvm-19/bin/clang build/initialization.o --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/initialization
build/initialization 10
```

The initialization_cli suite reuses the existing native value-source driver,
covering normal/O2 JIT, separately linked AOT, DWARF and failures. All 23 previous
suites remain; total is 24. Obsolete init-declaration rejection became positive
execution; direct lifecycle rejection and check/run/emit output protection remain.
WORK_LOG.md records full matrices, stack/stress and real editor verification,
including what still needs hosted Windows/macOS execution.
