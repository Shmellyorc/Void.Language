# First real source compiler

Phase 20 adds managed single inheritance, identity-preserving ancestor conversions, explicit virtual/override/super dispatch and staged base initialization. See [INHERITANCE.md](INHERITANCE.md) for the current boundary and WORK_LOG.md for actual verification.

Implemented locally on 10 October 2026, using LLVM 19.1.7. LANGUAGE_BIBLE.md
remains the language-design source of truth; this document describes the working
subset and explicitly provisional choices. Multi-file compilation is now implemented;
see [MODULE_SYSTEM.md](MODULE_SYSTEM.md) for current import and ownership rules.
Phase 6 updates the numeric subset on 11 October 2026; see NUMERIC_TYPES.md.
Phase 12 adds numeric/bool/nested value structs; see [VALUE_STRUCTS.md](VALUE_STRUCTS.md)
for approved defaults, source syntax, shared aggregate IR and private ABI boundaries.
Earlier foundation-only status in historical documents predates source compilation.

Phase 16 adds ordinary managed source structs, nullable references, deep readonly
views and runtime-backed JIT/linked AOT; see [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md).
Phase 17 adds custom managed initialization with checked construction views,
required-reference readiness, safe helper contexts and complete-object commit; see
[MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md).

## Working pipeline

`SourceFile` validates UTF-8 and retains original byte offsets. The indentation
lexer emits tokens, newlines and block boundaries. The recursive-descent parser
builds an AST per file. The module graph resolves all function signatures first, then
semantic analysis checks
lexical bindings and exact types, annotates expressions and determines possible
control-flow exits. Only validated ASTs lower into the shared VOID IR. The IR
validator checks its independent invariants before LLVM lowering.

The LLVM backend compiles that same IR for both ORC JIT and TargetMachine object
emission. There is no architecture-example recognizer or second source code
generator. Changing a source literal or function name changes both native outputs.
Compiler responsibilities stop at code generation; the existing experimental
heap and thread coordinator serve managed source through explicit runtime ownership
and AOT bootstrap; reload leases and the C bridge remain runtime/host responsibilities.

## Syntax and supported semantics

```swift
func sum(left: i32, right: i32) -> i32:
    return left + right

func entry(value: i32) -> i32:
    var total: i32
    var index = 0
    for index < value:
        index++
        if index % 2 == 0:
            continue
        total += sum(index, 1)
    for:
        if total >= 20:
            break
        total += 3
    return total
```

- Functions use `func name(parameter: type, ...) -> type:`. Module-level
  `private`, `public` and `internal` modifiers are supported; see MODULE_SYSTEM.md. Parameters require
  types. Omit the return annotation for no result. Empty `return` or fallthrough
  is valid only for a no-result function. Recursive and forward calls work.
- All ten exact-width numeric types plus bool and the value structs described in
  VALUE_STRUCTS.md work. Contextual literals and
  explicit checked numeric `as` follow [NUMERIC_TYPES.md](NUMERIC_TYPES.md).
  Calls check positional argument counts and exact types after literal adaptation.
- `var x = expression`, `var x: i32 = expression`, and `var x: i32` work.
  Annotated variables without an initializer receive safe defaults (including
  declared/recursive value struct defaults). Parameters
  and var locals are mutable. Local readonly const and module compile-time
  constants are now implemented; see VALUE_SEMANTICS.md. Local bindings are visible
  after declaration and only within their lexical scope. Shadowing visible locals, parameters or function names is
  rejected; sibling scopes can reuse names.
- Assignment `=`, arithmetic compounds `+= -= *= /= %=`, and standalone postfix
  `++`/`--` work. Increment/decrement cannot be used as expression values.
- Arithmetic uses unary `+`/`-`, `+ - * / %`; comparisons use `< <= > >= == !=`.
  Equality accepts matching numeric or bool operands; ordering requires matching numeric types.
- Boolean operators are `not`, `and`, `or`. `and`/`or` short-circuit at runtime.
  Arguments and binary operands evaluate left to right. From weakest to strongest:
  `or`, `and`, `not`, comparisons, addition/subtraction, multiplication/division/
  remainder, numeric `as`, unary arithmetic. Parentheses override precedence. Comparisons do
  not implement chained-comparison semantics; write `a < b and b < c`.
- `if condition:`, `else if condition:`, and `else:` nest normally. Conditions
  require bool. Every reachable end of a value-returning function needs a return.
  Analysis is conservative for conditional branches and conditional loops;
  unbounded `for:` and literal `for true:` need no return after the loop unless a
  reachable break can exit it. Unreachable statements still receive semantic
  checks, and are omitted from lowering.
- All loops use `for`: conditional `for condition:` or unbounded `for:`.
  `continue` reevaluates the nearest loop condition; `break` exits the nearest
  loop. Collection/range iteration is rejected until its types exist.
- Function calls can be statements, including no-result calls. Other standalone
  value expressions are rejected.

## Source and diagnostics

Files must be valid UTF-8; a leading UTF-8 BOM is accepted. LF, CRLF and CR line
endings work, as does a missing final newline. Comments start with `#` (including
`##` documentation-style lines), and may contain Unicode. Indentation uses
spaces with consistent enclosing levels; four spaces are recommended, rather
than imposed. Tabs are diagnosed. Blank/comment lines do not open blocks.
Expressions and declarations are single-line in this subset.

Names currently accept lowercase ASCII letters, digits after the first character,
and underscores. This does not settle future Unicode identifiers or the full
language naming policy. Unsupported characters and types are diagnosed.
Locations are one-based file/line/Unicode-scalar column positions, with the actual
source line and a caret. Scalar columns are not terminal display widths for wide
or combining characters. Diagnostics now collect independent errors with stable codes, ranges, related
declarations and bounded recovery; see [DIAGNOSTICS.md](DIAGNOSTICS.md).

For example, `while value > 0:` produces a VOID diagnostic at that keyword:

```text
example.void:2:5: error: VOID uses 'for' for all loops.
    code: VOID1104

 2 |     while value > 0:
   |     ^^^^^
    Hint: Replace 'while condition:' with 'for condition:'.
```

Invalid indentation/tokens, unresolved names, duplicate declarations, argument
counts, type mismatches, invalid returns and missing return paths are rejected
before LLVM. Malformed UTF-8 is rejected even in comments.

## Approved numeric behavior

Phase 6 extends the existing i32 wrapping/division contract to the exact-width
integer family and adds IEEE f32/f64 arithmetic, contextual literals and checked
numeric casts. See [NUMERIC_TYPES.md](NUMERIC_TYPES.md) for the approved rules,
shared software arithmetic, LLVM guards, diagnostics and precise remaining limits.
Integer division guards zero and MIN/-1 before dangerous LLVM operations;
float division by zero follows IEEE. No Language Bible text was changed.

## CLI and native executable boundary

```sh
voidc check program.void
voidc run program.void [decimal-i32-input] [--no-opt]
voidc emit program.void -o program.o [--no-opt]
```

`check` needs no entry function. `run` and `emit` currently require an
`entry(i32) -> i32` adapter, with any parameter name. This is the CLI host ABI,
not a restriction on language function signatures. Input defaults to zero.
O2 is the default; `--no-opt` selects unoptimized LLVM lowering. `emit` produces a
host-native object; a matching Clang driver links it (see README). Its native
`main` accepts the same optional input, prints the result and exits with status
zero; malformed/out-of-range input or extra arguments exit with status 2.

Source symbols encode their declaring module and function identity before the C
adapter is added, allowing same-named functions in different modules and helpers
named `main`, `printf` or `strtoll` without native adapter collisions. Stable
module identity is implemented; public library/export ABI remains deferred. Generated line
tables refer to the real input file. Variable debug records are not implemented.

## Validation and next phase

The existing six architectural suites remain, with `frontend` and `source_cli`
added to the same test executable/build. Frontend checks run three real files
and thirteen independent programs through normal/O2 ORC, plus over fifty negative
and boundary cases. Integration tests check, emit, link, verify DWARF and execute
three actual files with normal/O2 JIT and AOT, using paths with spaces. A modified
architecture source changes `42` to `13` and renames its helper to `main`; both
engines then return 23 for input 10. Tests retain the native `printf` proof.
See WORK_LOG.md for actual configuration commands and results.

The user confirmed this original eight-suite frontend phase passed Linux,
Windows and macOS Actions. The next module phase adds two suites and is documented
in MODULE_SYSTEM.md and WORK_LOG.md. The user reports that ten-suite phase also
passed all three hosted platforms. The user reports Phase 4 value semantics passed Actions run #6 across all three
platforms. Phase 5 diagnostic recovery requires another hosted run.

Current constants, arithmetic and runtime errors are documented in
[VALUE_SEMANTICS.md](VALUE_SEMANTICS.md). Structured diagnostic recovery is now implemented in [DIAGNOSTICS.md](DIAGNOSTICS.md).
Phases 6–11 subsequently completed numeric types, validated analysis, tooling
sessions, LSP and editor integration. Phase 11 run #16 passed all three hosted
platforms according to the owner. Phase 12 adds VALUE_STRUCTS.md and the
value_structs_cli suite, preserving all 21 previous suites. The subsequent
receiver/initialization and GC phases completed that gate. Current managed source
and the next deferred initialization decisions are documented in MANAGED_STRUCTS.md.

## Phase 13 receiver methods

Supported value structs now have owner-module receiver declarations and ordinary
value.method calls, including imported/nested receivers, inferred readonly effects,
normal/O2 JIT/AOT and owned tooling references. See RECEIVER_METHODS.md. Custom
init execution was deferred under Phase 13 I1; Phase 14 approved C1 now implements
it as documented in INITIALIZATION.md. No-init named construction and the CLI
check/run/emit i32 entry adapter remain available.


## Phase 14 custom value initialization

INITIALIZATION.md documents approved C1 custom value constructors, typed
positional/named arguments, pure defaults, recursive availability/access checks and
lifecycle diagnostics. Fresh construction uses shared receiver Places and explicit IR
invocation metadata; source execution and optimized/unoptimized JIT/AOT share the same
lowering. Required-child wrappers, pure contexts, readonly publication and conservative
initializer/helper reload contracts remain explicit boundaries.

Phase 18 implements owner-approved reference-containing value structs, per-leaf
readonly capability/effect checking, complete construction and precise roots through
normal/O2 JIT and native AOT. See [REFERENCE_VALUE_STRUCTS.md](REFERENCE_VALUE_STRUCTS.md);
earlier phase-only type exclusions in this historical guide are superseded there.


## Phase 19 composition

Owner-approved stored `using name: type` and `using type` composition, one-hop
member promotion and original embedded receiver mutation are implemented through
the existing paths. See [COMPOSITION.md](COMPOSITION.md) for exact lookup, access,
readonly, initialization, tooling and reload behavior. This adds neither subtyping
nor stateless reuse and does not change C1/S1/P3/P4/RV1–RV7. Phase 20 adds managed
inheritance and component virtual dispatch as described in INHERITANCE.md. Interfaces,
transitive promotion and has remain deferred.
