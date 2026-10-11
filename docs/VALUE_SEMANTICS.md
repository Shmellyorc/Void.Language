# Constants, numeric correctness and runtime diagnostics

Implemented subset, 10 October 2026. The Language Bible remains authoritative and
unchanged. The user reports the preceding ten-suite module phase passed Linux,
Windows and macOS Actions. The changes described here require a new hosted run.

## Readonly and compile-time values

```python
const bonus: i32 = base_score + 5
const base_score = 20
const enabled = true and not false

func calculate(value: i32) -> i32:
    const score = bonus + value
    var copy = score
    copy += 1
    return copy * 2
```

Both declaration forms accept optional `: i32` or `: bool` annotations, with exact
type checking. Every const needs an initializer. Local const bindings evaluate
at runtime at their declaration, may call functions, and follow existing lexical
scope rules. Their slots are initialized each time the declaration executes.
Assignment, all arithmetic compounds and postfix increment/decrement reject
readonly bindings. Copying a value into `var` produces an ordinary mutable local.
Parameters remain mutable; readonly parameters and managed readonly access paths
are outside this subset.

Module constants must be pure compile-time expressions. Literals, constant
references, unary arithmetic/not, arithmetic, comparisons, equality, parentheses,
and short-circuit and/or work. Calls are forbidden even in unevaluated operands.
The ordinary expression type checker validates every operand and resolves its
owner before evaluation. There is no second type checker or runtime global
initializer. The small evaluator consumes that typed expression tree and shares
operator-to-IR definitions with normal lowering. References become typed IR
literals; function code still lowers through the same LLVM implementation.

All declarations are registered before analysis. A depth-first dependency walk
supports forward references and reports cycles with the chain of declaring VFS
owners. Every module constant is checked, including unused declarations. Dependency
cycles are rejected structurally even behind short-circuit operators; execution
short-circuiting does not make a cyclic declaration graph valid. Pure arithmetic
on an unexecuted right operand is not evaluated, so `false and 1 / 0 > 0` and
`true or 1 % 0 > 0` are valid. Type errors and unresolved names there still fail.
Imports must precede constants and functions; constants and functions can otherwise
be interspersed. Runtime module variables remain unsupported.

## Project and import lookup

Public/default and internal constants in the selected entry file are project-wide,
without an import of that entry. Private entry constants stay in that file.
The entry file's parent remains the VFS root. There is no hidden runtime
initialization or extra graph discovery: only the entry and its reachable imports
are compiled. See `examples/constants/main.void` and `game/scoring.void` for the
acceptance project, with forward root declarations and runtime local const.

Other module constants follow the established direct-import rules: explicit
aliases expose qualified values such as `math.amount`; unaliased imports also
expose simple names. Private is file-private, internal is project-accessible,
imports do not reexport declarations, repeated bindings do not duplicate owners,
and ambiguous imported declarations require aliases. Functions and constants
share a file declaration namespace; a const is never callable. Own-file
declarations retain precedence over ordinary imports. Imported functions and
constants competing for an otherwise unresolved simple binding diagnose ambiguity.

Project-wide root names cannot be redeclared by another file's function or constant,
or collide with an import qualifier. Locals and parameters cannot shadow visible
constants, functions, qualifiers or lexical bindings. Private root constants do not
reserve names in other files. These are explicit collision/visibility choices for
this subset; external project export rules remain open.

## Implemented numeric contract

Only i32 and bool are implemented. Decimal literal magnitudes must fit i32, with
unary minus accepting the boundary `-2147483648`. Negation of an already valid
minimum value, including `-(-2147483648)`, wraps to itself. Values do not implicitly
convert between bool and i32.

Addition, subtraction, multiplication and negation wrap modulo 2^32 in compile-time
evaluation, unoptimized native code and O2. The evaluator uses unsigned arithmetic
and bit-preserving conversion; LLVM operations carry no signed-overflow promises.
Signed division truncates toward zero; remainder has the dividend's sign.
Division/remainder widen to i64 before the signed operation and truncate afterward:
minimum / -1 wraps to minimum, and minimum % -1 is zero. Compounds use the same IR
operations. No division instruction executes with a zero divisor.

A directly known literal zero divisor in an ordinarily evaluated function expression
retains its earlier source error. Statically decisive and/or operands suppress
that zero check on their unexecuted right side while preserving type checks.
Module constant evaluation diagnoses zero only when executing the expression.
Other dynamic zero divisors produce the runtime boundary described below.
General path-sensitive constant propagation is not implemented.

### Future numeric policy recommendation, requiring approval

Preserve this slice's i32 compatibility until a deliberate language decision changes
it. Before wider integers/floats, introduce arbitrary-precision literal magnitudes,
expected-type propagation through annotations/calls/operators, and a single typed
operation/conversion specification covering compile-time and native behavior.
Representability must be checked before converting contextual literals. Existing
variables should never silently lose precision. Implement explicit checked `as`
and nullable checked `as?` only after nullable/result representations exist.
Recommend float-to-integer truncation toward zero with range/NaN/infinity rejection,
and explicit tables for signed/unsigned and integer/float conversions. These details,
checked-versus-wrapping overflow for additional types, rounding, float equality/hash,
and platform-sized types remain unapproved. No such syntax or types are implemented
by this phase, and Debug and Release have the same current i32 semantics.

## Runtime boundary and native diagnostics

```text
.../arithmetic.void:2:16: runtime error: Division by zero
    return 100 / value
               ^
In function /arithmetic.void::calculate
Called from /main.void::bridge
.../main.void:4:23: source call
    return arithmetic.calculate(value)
                      ^
Called from /main.void::entry
.../main.void:7:12: source call
    return bridge(value)
           ^
```

Both `/` and `%` carry exact source locations and embedded source-line snapshots.
These remain correct if the files change or disappear after compilation. Generated
source functions receive one hidden pointer to per-invocation failure storage;
ordinary language signatures and backend-independent call typing stay unchanged.
A failing operation records its static diagnostic and returns normally. Each actual
source caller checks the failure, appends its own call location and returns before
executing more source instructions. This also works through bool/no-result calls,
recursion, imported functions and optimized/inlined code. The trace records source
calls actually propagating the failure, without guessing native return addresses.

Failure storage is caller-owned and independent for concurrent invocations. It
contains borrowed generation-owned strings and at most 32 callers; further callers
produce an explicit truncation message. The JIT lease retains these strings through
return and copies them into a C++ runtime_error only after generated code has
returned. No C++ exception unwinds through generated functions. The CLI reports
that error and exits 1. AOT allocates the same storage in its native main, prints
the recorded diagnostics using CRT puts and exits 1. Its diagnostic stream is
currently stdout; JIT CLI diagnostics use stderr. Results are printed only on
success. Invalid CLI input still exits 2. No extra runtime library is required
when linking an emitted object.

This is a small arithmetic-failure boundary, not generalized language exceptions,
result/error propagation, panic/defer cleanup, native-stack unwinding or a managed
runtime. Foreign frames are not reconstructed. Hand-built architectural proofs
retain their historical ABI and behavior. Future source native bindings must define
how their failures enter this boundary. Generated code itself is not sanitizer
instrumented; compiler/runtime and native fixtures remain ASan/UBSan checked.

## Restart compatibility

The frontend emits an exact, length-delimited token containing stable module IDs
and complete source snapshots for every reachable file containing any const,
including local runtime const. JIT compilation copies it into compatibility metadata;
host metadata cannot override it. ReloadSlot rejects a changed token and retains
old code safely. This has no hash-collision risk. It is intentionally conservative:
any edit to a const-containing file requires restart, even an unrelated comment or
function-body change. Files without const retain ordinary compatible-body replacement.
Removing/adding const or changing any imported const-containing file changes the
token. There is no automatic restart, watcher, incremental dependency fingerprint or
live constant update. More precise AST/dependency compatibility is future work.

## Validation and next phase

The existing ten CTest suite names and architectural assertions remain. Frontend,
module and module-native integration suites now also cover const mutation, forward
references, visibility, ambiguity, root collisions, invalid initializers and cycles,
i32 arithmetic boundaries, source errors in imported functions, bounded recursive
traces, no-result propagation, concurrent invocation isolation, unchanged/changed
reload contracts and source snapshots after object emission. Programs run in both
unoptimized and O2 JIT/AOT modes. An independent widened-integer oracle checks
304 operand/operator combinations: 1,216 constant/normal/O2 JIT comparisons, plus
the full 304-case program in normal/O2 JIT and AOT. See WORK_LOG.md for measured local results.

Recommended next phase: improve diagnostic structure and recovery and refine
source/dependency compatibility metadata before broader numeric types and checked
conversions. Define literal contextual typing and conversion tables deliberately.
Managed language types still require the separate GC root/thread integration gate.
