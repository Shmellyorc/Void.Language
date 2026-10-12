# Phase 12: value struct foundations

Current Phase 14 update: approved custom value initialization is documented in
[INITIALIZATION.md](INITIALIZATION.md). It supersedes the historical init deferral
below; Phase 12 construction remains unchanged for types without custom init.

Implemented locally using the Option A contract approved by the project owner on
12 October 2026. The Language Bible remains authoritative. Runtime field defaults,
custom `init` and positional constructors are deferred, not permanently forbidden.
See VALUE_STRUCTS_PROPOSAL.md for the approved bounded contract.

## Working source surface

```python
value struct point:
    x: i32 = 4
    y: i32

value struct shape:
    position: point = point(y = 2)
    enabled: bool = true

func moved(original: shape, amount: i32) -> shape:
    original.position.x += amount
    return original

func entry(value: i32) -> i32:
    var original: shape
    var copy = moved(original, value)
    return copy.position.x - original.position.x
```

Fields require explicit types: any of the ten numeric types, bool or a visible
acyclic value struct. Forward type declarations work. Named construction accepts
zero or more distinct `field = expression` arguments. Unspecified fields retain
their declared defaults, or zero/false/recursive defaults. Declared defaults are
pure typed compile-time expressions using the existing constant rules; nested
construction and field reads of compile-time values also work. Module constants
can hold aggregates. Every declaration/default is validated, including unused
ones. Function calls and self/other-field references cannot be declared defaults.
The software numeric evaluator preserves existing rounding, wrapping and checked
conversion rules at every field type; construction does not add numeric promotions.

Explicit named argument expressions execute once in written order, even when
that order differs from field declaration order. They may call functions and use
short-circuit expressions. Invalid runtime arithmetic/conversions retain ordinary
source context and propagate through aggregate-returning functions before the
caller uses their result. Defaults have no runtime effects in this phase.

Assignment, parameters and returns copy whole values. Nested mutation updates only
the selected mutable binding. Mutable parameters are local copies. Local const
bindings protect all nested paths; copying a const into var permits independent
mutation. Field reads work on variables, constants, constructed values, function
results and parenthesized values. Field writes require a local/parameter binding
root; arithmetic compound assignment and increment/decrement work on numeric
fields. A field cannot be treated as an independently aliased reference.

Type declarations follow existing public/internal/file-private and direct-import
rules. Entry-file public/internal type names are available project-wide like root
constants, and cannot be redeclared by imported files. Functions, constants and
types share a file declaration namespace. Imported same-name types require aliases;
matching fields do not make differently owned types interchangeable. Diagnostics
identify nominal types by their module-qualified identities. Fields in this slice
are public; field modifiers remain unsupported. Phase 13 adds owner-module
receiver methods; see RECEIVER_METHODS.md.
`value` is contextual in `value struct`, so existing `value` parameter names work.

## Shared representation and lowering

`ir::Type` represents primitives or an owned immutable nominal `ValueType`
descriptor. The parser's temporary unresolved references resolve into those same
owned descriptors before validation succeeds. No LLVM types leak into analysis,
tooling or VOID IR. Layout DFS rejects direct/indirect recursion before publishing
cyclic owning descriptors. Existing recovery and poisoned dependency gates remain.
For bounded analysis, layout depth is limited to 128 and an expanded layout to
65536 fields, including nested aggregate nodes; these are compiler resource limits.

The typed expression evaluator carries recursive `ConstantValue` values and uses
the original numeric kernel for scalar operations. Default and module-constant
cycles diagnose rather than recurse indefinitely. The backend-independent IR adds
aggregate construction, field extraction and field insertion. Its validator
checks field indexes, element types, construction arity and descriptor ownership.
Assignments replace immutable aggregate values in existing typed slots. Temporary
slots preserve instruction ownership across short-circuit CFG edges. There is one
source checker and one shared LLVM lowering for ORC JIT and AOT.

LLVM structs retain source field order with the target's native alignment. Debug
member sizes/offsets use the same target DataLayout, supplied before lowering.
Generated callers and callees use matching compiler-private LLVM signatures,
including the existing failure context. Tests exercise both small and large nested
mixed-width aggregates across source-function boundaries without optimization.
The optimized paths exercise LLVM's transformations of the same IR.

This does **not** establish an external C aggregate ABI, packed representation,
stable field offsets, serialization format or cross-version binary ABI. The host
CLI/JIT invocation adapter still requires i32(i32); source functions can exchange
value structs internally. No object allocation or managed-runtime machinery is
added. The GC remains an architectural experiment.

## Analysis, tools and reload

Validated analysis exposes value-type and field declarations using shared type
descriptors. Type annotations, constructor names, named initializers, field reads
and writes contribute source references used by the existing tooling/LSP index.
Aggregate module constants expose their type and exact syntax; the old optional
64-bit `AnalyzedDeclaration::value` remains scalar-only, rather than pretending an
aggregate fits in it. The VS Code grammar highlights the contextual declaration
and indents its body without changing extension lifecycle behavior.

Exact compatibility contracts include nominal type identities, ordered field
names/types and declared default token syntax, including nested dependencies.
Function-body edits with compatible declarations can reload; layout/default/type
changes require manual restart and are rejected before publication. This is
conservative: even unused layout/default changes invalidate compatibility. No
automatic restart or aggregate external ABI migration is introduced.

## Examples and verification

The real source project examples/value_structs/main.void exercises imported nested
types, defaults, parameters, returns, readonly copies and mutation. Inputs -1, 0
and 10 return 28, 36 and 46. numeric.void exercises all ten numeric types plus bool,
large nested parameter/return values, compile-time defaults and local loop mutation;
inputs -10, 0 and 123 return -5, 5 and 128. order.void exercises reverse named-field
order, aggregate returns and source-located failures: input 0 fails division before
conversion; input 1 reports conversion; inputs 2 and 3 return 5 and 4.

```sh
build/ci-debug/voidc check examples/value_structs/main.void
build/ci-debug/voidc run examples/value_structs/main.void 10 --no-opt
build/ci-debug/voidc run examples/value_structs/main.void 10
build/ci-debug/voidc emit examples/value_structs/main.void -o /tmp/value-structs.o
/usr/lib/llvm-19/bin/clang /tmp/value-structs.o -fuse-ld=lld -o /tmp/value-structs
/tmp/value-structs 10
```

CTest's new value_structs_cli suite performs the platform-appropriate native link,
DWARF verification and actual execution for all three programs in both modes and
engines. The frontend suite adds semantic/diagnostic, nominal module identity,
readonly, compatibility, bounded-layout and IR validation checks. The real native
LSP process suite adds struct navigation, references, diagnostics and revision
recovery. Existing 21 suites and extension lifecycle tests remain intact. See
WORK_LOG.md for exact completed build/test commands and SDK/platform evidence.

## Deferred work and next phase

No managed structs, strings, collections, generics, interfaces, inheritance,
custom init, positional construction, runtime field defaults, field visibility
modifiers, aggregate equality/conversions, field references or external aggregate
bindings are implemented. The default/initializer interaction remains open for a
future phase. This phase does not promise persistent identity across projects or
incremental code generation.

Phase 13 implements receiver methods under approved M1 + I1. Custom initialization
remains a separate construction-design gate; see RECEIVER_METHODS.md.
