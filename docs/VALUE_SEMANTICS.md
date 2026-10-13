# Constants, numeric correctness and runtime diagnostics

Phase 12 adds numeric/bool value structs with recursive defaults, independent
copies, deep readonly paths and aggregate parameters/returns. See
[VALUE_STRUCTS.md](VALUE_STRUCTS.md) for the approved contract. The scalar rules
below remain applicable.

Scalar subset, updated 11 October 2026 for Phase 6. The Language Bible remains authoritative and
unchanged. The user reports the preceding ten-suite module phase passed Linux,
Windows and macOS Actions. The user reports these Phase 4 changes passed all three platforms in Actions
run #6 (commit `d78b6b8`). The user reports Phase 5 commit `4f3688d` passed all platforms in run #8.
Phase 6 adds the approved numeric family; see NUMERIC_TYPES.md.

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

Both declaration forms also accept supported value struct annotations in Phase 12.
The scalar behavior remains: optional numeric/bool annotations, with
contextual literal adaptation and exact checking of existing typed values. Every const needs an initializer. Local const bindings evaluate
at runtime at their declaration, may call functions, and follow existing lexical
scope rules. Their slots are initialized each time the declaration executes.
Assignment, all arithmetic compounds and postfix increment/decrement reject
readonly bindings. Copying a value into `var` produces an ordinary mutable local.
Parameters remain mutable; readonly parameters and managed readonly access paths
are outside this subset.

Module constants must be pure compile-time expressions. Phase 12 additionally
supports value construction and field reads in these expressions; aggregate
constants still undergo the same checker, purity and dependency gates. Literals, constant
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

All ten exact-width numeric types and bool are implemented. The approved Phase 6
contract is [NUMERIC_TYPES.md](NUMERIC_TYPES.md), including literal construction,
strict implicit conversions, checked numeric `as`, and constant/native parity.

The established i32 behavior is unchanged: integer arithmetic wraps, signed division
truncates toward zero, MIN/-1 wraps to MIN, and MIN%-1 is zero. This now applies at
each integer width. The evaluator uses software APInt/APFloat operations, not host
signed overflow or host floating rounding. LLVM arithmetic has no overflow promises
or fast-math flags. Division selects a safe divisor before the MIN/-1 operation,
avoiding both poison and a need for native 128-bit division support.

Directly known integer literal zero divisors in evaluated function expressions
retain source errors. Statically decisive and/or operands suppress checks on their
unexecuted right side while preserving type checks. Module constant evaluation
checks arithmetic/conversions only when executing the expression. Dynamic failures
use the boundary below. Float division by zero follows IEEE rather than integer
failure rules. No general path-sensitive constant propagation is added.

The previously proposed contextual typing/conversion policy was explicitly approved
for Phase 6. Nullable `as?`, platform-sized aliases and numeric hashing remain
separate future decisions, not simulated features.

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
success. The emitted native adapter's invalid input exits 2; existing JIT CLI input
validation failures exit 1, while usage errors exit 2. No extra runtime library is required
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

Structured recovery is now implemented in [DIAGNOSTICS.md](DIAGNOSTICS.md).
Phase 6 adds numeric and numeric_cli alongside these ten preserved suites.
Recommended next phase: refine validated analysis/dependency APIs and compatibility
metadata before incremental tooling. Wider numbers and checked numeric conversions
are now implemented under the approved contract.
Managed language types still require the separate GC root/thread integration gate.

## Phase 5 preservation audit

Structured compile-time diagnostics/recovery now live in [DIAGNOSTICS.md](DIAGNOSTICS.md).
The arithmetic implementation, runtime failure ABI, original snapshots, bounded
caller traces and exact conservative const restart token are unchanged. Invalid
source edits cannot produce IR or publish a native generation. Tests retain the old
lease and active slot across failures and continue rejecting changed const tokens.
Precise dependency compatibility remains a separate recommended design phase.

## Phase 7 analysis and compatibility update

The earlier whole-file constant-token and analysis-API limitations above describe
the preceding phases. They are superseded by [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md):
one validated, owning analysis result now supplies declarations, snapshots and
resolved dependencies without IR/machine-code generation. Exact declaration tokens
and compiler-derived bindings replace whole const-containing source files in
reload compatibility. Ordinary body/trivia edits beside unchanged constants can
reload; actual constant changes require manual restart. Language rules, numeric
semantics and structured recovery remain unchanged.

## Phase 13 receiver values

The owner approved M1 + I1 in RECEIVER_METHODS_PROPOSAL.md. Receiver methods
operate on validated local/nested storage when mutating; readonly and temporary
values permit only inferred non-mutating methods. Ordinary parameters, assignment
and returns still copy. Receivers select storage once before left-to-right arguments;
aggregate arguments capture their value at their own evaluation point.
RECEIVER_METHODS.md records transitive effect analysis, runtime traces and exact
restart rules. Phase 12 defaults/construction and the numeric kernel are unchanged;
custom init execution remains deferred, not prohibited by the language.


## Phase 14 custom value initialization

Phase 14 approved C1 initializes fresh values recursively, then invokes init once
through the existing receiver-place strategy. Copies, arguments, assignments and returns
never repeat initialization. Supplied values snapshot before default construction;
explicit named child fields avoid redundant initialization. Readonly applies after
successful completion. Failure does not publish a partially completed destination.
INITIALIZATION.md records visibility, required-child and purity boundaries.

## Phase 18 reference-containing values

REFERENCE_VALUE_STRUCTS.md extends these rules to managed references inside values.
Copies retain independent inline storage and embedded object identity. Const-to-var
copies preserve each referent capability; pure all-null templates remain neutral.
C1 defaults/init, S1 complete fields and P4 publication are preserved. Historical
reference-free restrictions above do not exclude the approved RV1–RV7 subset.
