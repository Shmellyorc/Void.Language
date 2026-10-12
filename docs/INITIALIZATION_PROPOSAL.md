# Phase 14: custom value initialization — owner approval required

Status: **C1 approved by the owner for Phase 14**, including the nested-child
boundary, visibility, reload closure and narrowly scoped obsolete-test updates.
See INITIALIZATION.md for implemented behavior and WORK_LOG.md for verification.
The alternatives below remain unapproved. This document preserves the approved
design proposal and its original rationale.
Phase 13 approved M1 + I1, not the I2 alternative in RECEIVER_METHODS_PROPOSAL.md.
This document proposes a complete parameterized construction contract, not I2's
zero-argument-only implementation. Code examples below are proposed syntax.

The owner reports Phase 13 `c72e62a` passed hosted run #19 on all three platforms.
The current checkout is `ffa9cdf`, a subsequent syntax-highlighting correction;
the working tree was clean before this proposal. Existing local six-configuration
23-suite results are Phase 13 evidence, not Phase 14 verification.

## Current source behavior and constraints

Reviewed LANGUAGE_BIBLE.md (especially sections 7, 14, 16, 39 and 52),
DEVELOPMENT_STRATEGY.md, DECISIONS.md, VALUE_STRUCTS.md, RECEIVER_METHODS.md,
RECEIVER_METHODS_PROPOSAL.md, KNOWN_ISSUES.md and the current WORK_LOG.md evidence.
The implementation, rather than illustrative Bible examples, determines what
currently works:

- `src/frontend/compiler.cpp` parses call labels but accepts them only as named
  value fields. Function parameters require explicit types and have no defaults.
- `Structure::defaults`, `Semantics::field_default` and constant evaluation build
  pure structural values. Typed aggregate locals without expressions use those
  values. They execute no user initialization.
- Construction lowering stores a structural default and evaluates explicit field
  expressions in source order. Phase 13 receiver calls already use typed IR Places,
  actual nested storage, inferred effects and shared JIT/AOT lowering.
- Lifecycle declarations and direct `.init()` calls currently diagnose VOID2020.
  Owned analysis, source references and exact compatibility already describe
  methods, types, fields, pure defaults and receiver effects.

The Bible requires safe fields before initializer logic, one initializer per type,
once-only initialization and no ordinary reinitialization call. It leaves the
binding, nesting, visibility and failure/reload details below open.

## Recommended contract: C1

Approve the following rules together. In particular, nested construction and
default availability are part of this decision, not implementation conveniences.

### 1. One initializer; constructor arguments have one meaning

```python
value struct point:
    x: i32
    y: i32

func (p point) init(x: i32, y: i32 = 10):
    p.x = x
    p.y = y

var a = point(2)
var b = point(y = 4, x = 2)
var c = point(2, y = 4)
```

A type may have one owning-module receiver `init`. Its parameters are explicitly
typed with existing numeric, Boolean or supported nominal value types. Phase 14
initializers have no result annotation; empty `return` or reaching the end means
success. Result annotations, overloads and foreign extension initializers diagnose.
Fallible initializer results remain future work, not permanently forbidden.

When a type has `init`, **all constructor arguments bind exclusively to initializer
parameters**. Field names are not an alternate namespace or fallback, even when a
parameter happens to share a field name. There is no additional post-init field
override step. A missing/unknown argument cannot silently select structural
construction or bypass `init`.

Types without `init` keep named-field construction, with no positional field
arguments. Existing Phase 12 programs keep their behavior. Adding a custom
initializer deliberately changes that type's construction interface and requires
call sites and reload compatibility to be checked again.

### 2. Positional/named arguments and pure parameter defaults

- Accept a positional prefix followed by named arguments in any written order.
  Positional arguments after the first named argument diagnose.
- Bind positional arguments by declaration order and labels by exact parameter
  name. Reject unknown labels, duplicate labels, positional/named double binding,
  excess arguments and missing required parameters. There is no overload search.
- Required parameters precede defaulted parameters. Defaults use `name: type =
  expression`; no inferred parameter types in this phase.
- Defaults are pure typed compile-time expressions, using the same evaluator and
  numeric rules as field defaults. They resolve in the initializer's owning module,
  never the caller's scope. They cannot reference the receiver, another parameter,
  a function/method call or a runtime constructor. Validate unused defaults too.
- Existing exact typing and contextual literal rules apply. No implicit numeric
  promotion or structural conversion is added. Aggregate parameters remain copies.
- Evaluate supplied expressions **once, left to right in source order**, before
  any default construction or initializer body. Save their values before reordering
  them into parameter slots. An omitted pure default has no runtime effects.

Named/default ordinary function and method calls remain outside Phase 14's public
surface. Their eventual rules should reuse this contract and the shared parameter
binding implementation; there must be no second constructor type checker.

### 3. Construction order, storage and exactly-once execution

For an init-bearing type:

1. Evaluate and snapshot supplied arguments in written order. If one fails, stop;
   do not run field construction or the initializer.
2. Initialize fields in declaration order: copy a declared pure field default,
   otherwise perform the field type's default construction as defined below.
3. Run the selected initializer **once** on the fresh value's actual storage, using
   the completed arguments/defaults. All fields are safe, fully constructed values
   before the body can read or call methods on them.
4. Expose the value only after the body succeeds. Apply a destination binding's
   readonly restriction after construction; `const p = point(2)` may run a mutating
   initializer on its private fresh storage.

Pure declared defaults are checked/evaluated at compile time, not reevaluated as
runtime expressions. The order above defines their installation and all observable
nested initialization effects. Source programs cannot access the fresh receiver
during argument evaluation. Assignment/parameter passing/return of an existing
value copies it and **never reruns `init`**. Storage placement/elision is an
optimization and may not add, remove or reorder observable initialization.

For a type without `init`, evaluate/snapshot supplied named fields first in written
order, then assemble fields in declaration order. Use a supplied value as-is;
otherwise copy a pure declared default or default-construct the omitted field.
There is **no redundant default construction of an explicitly supplied field**.
With only Phase 12 types this is observably identical to current behavior, since
their defaults are pure. It also avoids initializing a nested value merely to
discard it when a named field overrides it.

### 4. Nested values and availability of default construction

Recommend recursive construction, rather than exposing a child value whose custom
initializer was silently skipped:

- Numeric/Boolean default construction remains zero/false.
- A type with `init` is default-constructible only when `init()` can be called with
  all arguments omitted and every field has an available default.
- A type without `init` is default-constructible when every omitted field can be
  initialized. An explicit named field supplies an already completed value and
  needs no default for that field.
- Default-construct an omitted nested value by the same rules, completing its
  children and then its initializer before proceeding to the next parent field.
  No init is called again when that completed child is copied into its parent.
- A declared pure field default supplies a completed constant value; copying it
  does not run initialization. It cannot hide a runtime initializer call.

Example: if `child.init(seed: i32 = 3)` sets `child.x`, an omitted `child` field runs
that initializer once before its parent initializer. An explicitly supplied
`child(9)` runs once at that argument's evaluation position, not again in the
parent. Replacing a default-constructed child inside a parent initializer is an
ordinary assignment: the earlier child's initialization effects are not undone.

**Important boundary requiring approval:** if `child.init(seed: i32)` requires an
argument, `child()` and an omitted child field cannot manufacture it. A no-init
wrapper with a `child` field remains usable as `wrapper(child = child(9))`, but
`wrapper()` and `var w: wrapper` diagnose. An enclosing custom initializer requires
defaults for all its fields before its body starts; it cannot leave a required
child uninitialized until a body assignment. Reject that enclosing initializer
with a field-located explanation and a note on the child's required parameter.
Merely declaring the no-init wrapper or using it as a parameter type stays valid.

This deliberately avoids definite-initialization analysis or runtime field-default
expressions in Phase 14. A later approved runtime field default such as
`child: child = child(9)` could support that enclosing initializer without reversing
the order or exposing an uninitialized child. It is not implemented now.

`var p: point` means default construction, equivalent to `point()` for access,
required/default arguments, nested effects and failures. If construction is not
available, require an explicit initializer expression. No implicit zero-value
escape hatch bypasses a custom initializer. Existing `const` declarations still
require an expression.

### 5. Compile-time contexts stay pure

Module constants, declared field defaults and initializer parameter defaults may
not execute custom initialization, even when its body appears pure/non-mutating.
No compile-time interpretation of arbitrary initializer bodies is added.

Reject a construction requiring an initializer anywhere in its actual construction
plan, including an omitted nested field, with the nested cause and a runtime-local
alternative. Do not silently substitute structural zeroes. Non-init types whose
construction uses only pure defaults/expressions retain all current constant
capabilities. Primitive expressions merely mentioning a type in a signature are
not rejected. A local runtime `const p = point(2)` is allowed and becomes readonly
after successful construction.

### 6. Ownership, visibility and direct lifecycle calls

Initializers belong to the nominal type's owning module, as under M1. Construction
resolves through the visible type, not competing imported methods or foreign
extensions. Retain public-by-default, internal/project and private/file visibility
for initializers. The effective permission is the intersection of type and
initializer access. A public type may have a private initializer and an owner
factory; copying a returned value needs no constructor access.

Explicit `type(...)` and typed-local default construction check access at the use
site. An omitted field's implicit construction is checked in the **field declaring
module** as part of that type's default plan. A public enclosing type may therefore
encapsulate a private nested initializer available to its owner. Importers do not
gain permission to call that nested initializer directly. A field declaration
cannot grant its owner access to another module's private initializer.

Always reject `.init(...)`, type-qualified `type.init(...)` and treating an
initializer as an ordinary callable method, inside and outside the owner. This
includes the initializer body itself: constructing a separate value is distinct
from reinitializing the current receiver. Existing field/method collisions and
duplicate declarations still diagnose. An unrelated free function named `init`
retains its existing meaning; the lifecycle rule applies to receiver declarations.

### 7. Failures and publication

Use existing VOID arithmetic/conversion failure propagation, with accurate source
locations for arguments, nested construction, initializer bodies and call sites.
If an argument, child construction or initializer fails, do not run remaining
steps or expose a successful constructor result. A failed assignment's fresh
temporary must not overwrite its destination before success. Previously completed
external argument effects are not rolled back.

Initializers can use ordinary methods on the safely prepared receiver. Ordinary
arguments remain copies and there are no source references or global mutable
storage through which the fresh receiver can escape in this subset. Safe fields
do not imply that the initializer's application-level postcondition already holds
inside its own body. The completion boundary, not a second user-visible state/type,
controls publication.

Phase 14 adds no `result`, exceptions, fallible-constructor rewriting, destructors,
rollback or cleanup-on-failure promise. A future approved fallible constructor
must expose only a completed value on success and can reuse this boundary; its
result spelling, resource ownership and cleanup policy remain open.

### 8. Exact hot-reload policy

Recommend conservative restart for changes to initialization policy:

- Adding/removing an initializer; its identity, ownership, visibility, parameter
  names/order/types/default syntax, receiver effect or eventual result contract.
- Initializer body token changes (excluding comments/whitespace), pure field/default
  changes, nested construction plans and relevant resolved binding changes.
- Body changes in functions/methods transitively reachable from an initializer,
  because they can alter initialization just as a direct body edit can. Compute
  this finite dependency closure iteratively, including recursive calls.

This intentionally freezes initialization helpers as well as direct initializer
bodies. Otherwise moving code into a helper would accidentally bypass the policy.
Ordinary compatible body changes outside that closure retain Phase 13 reload.
Existing values are never reinitialized or migrated; rejected edits keep the old
generation active and require explicit restart. Tooling exposes initializer kind,
owner, parameters/default spans, constructor references and these dependencies
through the shared analysis model, without authorizing native publication itself.

## Alternatives and why C1 is recommended

| Decision | Recommended C1 | Alternative and consequence |
| --- | --- | --- |
| Constructor binding | Init parameters exclusively; fields only without init | Mixed parameter/field namespace creates collisions and an observable override policy |
| Arguments/defaults | Typed, pure defaults; source-order supplied expressions first | Runtime defaults/earlier-parameter references add a new execution/scope contract; defer |
| Nested omitted fields | Complete recursive default construction | Structural defaults that skip child init are simpler, but expose values that bypass child invariants |
| Required nested arguments | Explicit value for no-init wrappers; custom parents need defaultable fields | Permit incomplete fields until assigned by init, requiring definite-initialization/escape analysis |
| Typed locals | Equivalent to zero-argument construction | Structural zeroing creates a second way to bypass init |
| Pure contexts | Reject actual runtime initialization | Interpreting init or skipping it changes current purity/invariant guarantees |
| Visibility | Existing modifiers restrict construction | Always-public intrinsic init makes private factories impossible; rejecting modifiers only delays this choice |
| Reload | Freeze init plus transitive helpers | Allow compatible initializer/helper body reload for future constructions only, with old values unchanged |

The reload alternative is coherent if explicitly chosen; it accepts different
initialization behavior for old and future values. C1 favors the existing
conservative field-default/state compatibility policy. The structural-nesting
alternative is also a real language choice, not an optimization. Neither is
silently inherited from Phase 13 I2.

## Implementation and verification after approval

Extend the existing Function/Parameter/Structure metadata, semantic checking and
owned analysis with initializer identity, parameter binding/defaults and a bounded
construction plan. Keep one nominal type model and one argument checker. Separate
pure constant defaults from runtime default-construction availability; do not
poison a usable required-argument type merely because `type()` is unavailable.

Lower evaluated arguments and fresh construction storage with the existing typed
slots/receiver Places, aggregate operations and shared failure-aware calls. Keep
initializer invocation distinguishable and validated; no second LLVM generator,
runtime object system or external aggregate ABI. Preserve small expression handlers,
acyclic layout checks, dependency/depth budgets and iterative effects/closure walks.

Add real multi-file examples and normal/O2 JIT/linked AOT assertions for positional/
named/default arguments, source order, nested/default construction, required-child
diagnostics, private factories, readonly completion, copies/returns, method calls,
failure order/publication and imports. Cover invalid lifecycle declarations/calls,
argument/return/default types, constant contexts, independent recovery, tooling/LSP
revisions and exact reload rejection including helper changes.

Keep all 23 suites, native/GC/reload/diagnostic assertions and the 1 MiB semantic
regression. Run installed and official LLVM 19.1.7 Debug/Release/strict ASan+UBSan
matrices, leak detection/RTTI validation, repeated stress, extension unit tests,
real editor lifecycle tests and VSIX packaging. Document Linux evidence separately
from pending Windows/macOS hosted results. Write INITIALIZATION.md only once an
approved contract is implemented; do not present this proposal as working syntax.

**Existing assertion conflict requiring explicit approval:**
`tests/frontend/methods.cpp:62` asserts that a valid receiver init declaration is
rejected, and `tests/integration/structs.cmake:108–127` checks the same Phase 13
deferral for check/run/emit. Accepting initializers necessarily supersedes those
specific expectations. Request approval to replace the declaration rejection with
positive constructor execution and use a genuinely invalid lifecycle call for the
CLI rejection case, retaining all action/failure/output-preservation checks and
the direct `.init()` rejection. No other existing assertion needs weakening.

## Approval record

The owner approved **C1 for Phase 14**, including recursive nested construction,
the required-argument child boundary, constructor visibility, conservative
initialization dependency/reload policy and the narrowly scoped obsolete Phase 13
test updates. Implementation follows INITIALIZATION.md. Alternatives remain
unapproved. No commits, pushes, branches or remote changes are authorized.
