# Numeric types and contextual literals

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Phase 6 contract approved by the user on 11 October 2026. This specifies the
implemented numeric slice without changing the agreed Language Bible. Phase 5
commit `4f3688d` passed hosted run #8 according to the user. Phase 6 hosted
verification remains separate from local results in WORK_LOG.md.

## Types and literals

All ten types work in function parameters/results, locals, module/local constants,
operators, calls, assignments and imported modules:

| Types | Representation/range |
|---|---|
| `i8`, `i16`, `i32`, `i64` | N-bit two's complement, -2^(N-1) through 2^(N-1)-1 |
| `u8`, `u16`, `u32`, `u64` | N-bit unsigned, zero through 2^N-1 |
| `f32`, `f64` | IEEE binary32/binary64 |

`bool` is separate, with no numeric conversion. Zero initialization uses integer
zero, positive floating zero or false, according to the declared type.

Initial literal spelling is decimal digits, an optional dot followed by decimal
digits, and optional `f`/`d` floating suffix. The suffix selects f32/f64, including
integral spellings such as `100f`. Exponents, digit separators, other bases and
leading/trailing bare decimal dots are not supported. A leading sign is unary
syntax; a directly signed literal is checked as a complete mathematical value,
so signed minima are accepted. Integer magnitudes are parsed at arbitrary precision
before range checking, never through a possibly overflowing host integer.

Without context, integral literals default to i32 and decimal literals to f32.
A default that cannot represent its literal diagnoses instead of silently widening.
Annotations, assignments, parameters, returns and typed operator operands guide
unsuffixed literals, including literal-only arithmetic subexpressions. Typed
operands take precedence over an outer expected type: assigning an i32 expression
to i64 does not retroactively promote that expression. Named constants, variables,
call results and suffixed literals retain their established type.

An integer literal must fit an integer destination exactly. Implicit adaptation
to a float must preserve the integer exactly. Decimal floating literals instead
construct a correctly rounded value directly in the selected precision. This is
why `var distance: f64 = 0.1` is valid and does not first round through f32.
Overflowing floating literals diagnose; underflow rounds, including to signed zero.
A directly signed `-0` in floating context preserves negative zero, consistently
with floating unary negation. Converting an existing integer zero yields positive zero.
Decimal floating literals do not implicitly adapt to integer types, even when
spelled with a zero fractional part.

```python
var health = 100                  # i32
var speed = 0.5                   # f32
var gravity = 9.81f               # f32
var precision = 9.81d             # f64
var exact_speed: f32 = 100        # exact integer adaptation
var distance: f64 = 0.1           # direct binary64 construction
var wide: i64 = 2147483647 + 1    # expression receives i64 context
# var bad: f32 = 16777217         # not exactly representable
var rounded = 16777217 as f32     # explicit rounding is permitted
```

## Operators

Existing typed values have only identity implicit conversion. Arithmetic and
comparison operands must have the same type after contextual literal adaptation.
There is no automatic widening, signed/unsigned promotion or integer/float
promotion. Two unconstrained integral operands use i32; an unconstrained floating
literal makes a literal-only numeric expression floating. Conflicting typed
operands require explicit conversions.

Integer add/subtract/multiply/negation wrap modulo 2^N, independent of build or
optimization. Signed division truncates toward zero; remainder has the dividend's
sign. MIN/-1 yields MIN; MIN%-1 yields zero. Unsigned division uses ordinary
quotient/remainder. Integer zero divisors preserve existing source/runtime errors.
Float division by zero follows IEEE, producing infinities or NaN as appropriate.
`%` and `%=` remain integer-only. Arithmetic compounds and standalone ++/-- use
the same typed operations; integer negation also wraps for unsigned values.

Floating operations round individually to their declared precision, nearest,
ties-to-even, with gradual underflow. No fast-math, reassociation or implicit
fused multiply/add contraction is enabled. Signed zero and infinities are preserved.
NaN equality is false, inequality true, and all ordered comparisons false. Positive
and negative zero compare equal. NaN payload/sign are unspecified; tests compare
classification and defined numerical behavior rather than payload identity.
There is no bit reinterpretation operator or total float ordering in this phase.

## Explicit checked conversions

`as` binds more tightly than binary arithmetic and less tightly than unary
arithmetic. Chained casts associate left to right. Parentheses make grouping
explicit. Only numeric source/destination types are supported here.

| Conversion | Defined behavior |
|---|---|
| Integer to integer | Destination mathematical range check; no wrapping conversion |
| Integer to float | Explicit nearest/ties-even rounding; precision loss permitted |
| Float to integer | Truncate toward zero, then range check; NaN/infinity fail |
| Float to float | Nearest/ties-even rounding; finite-to-infinity overflow fails; underflow allowed; existing infinity/NaN and signed zero preserved |

A direct unsuffixed integer literal can be checked/rounded in an `as` destination
without first fitting i32. Ordinary typed operands keep their source precision.
A decimal operand of an integer cast defaults to f32 unless suffixed or otherwise
typed; use `3.9d as i32` to select binary64 before truncation.

Evaluated invalid compile-time conversions report VOID2015. Invalid runtime
values return the existing source-first RuntimeFailure with source location and
actual caller trace; no exception unwinds through generated code. Direct known
invalid integer literal casts in evaluated function expressions diagnose early.
Short-circuited constant branches are type-checked but do not execute conversions
or arithmetic. Ordinary malformed/out-of-range literal construction still diagnoses.
There is no `as?`, nullable stand-in, implicit Boolean conversion or wrapping cast.

## Shared implementation and guards

`void/numeric.hpp` defines width, signedness, legal operation and software numeric
operations using plain type tags and scalar bit patterns. `src/numeric.cpp` uses
LLVM Support APInt/APFloat privately for reliable magnitude parsing and deterministic
constant arithmetic/conversion. This is a narrow new Support dependency of
void_core; AST/type-checking code and public VOID IR contain no LLVM types or
code-generation APIs. There is one typed AST evaluator and one shared LLVM lowering.
The independent IR validator checks the same legal-operation specification.

IR constants now carry 64 bits; conversion and negation are typed IR operations.
Negation lowers to integer negation or FNeg, preserving negative floating zero.
For signed MIN/-1, lowering selects a safe divisor BEFORE division, then selects
the specified wrapped result. No dangerous signed division is executed speculatively.
Integer conversions compare sign/zero-extended 128-bit values against exact limits
before truncating. Float-to-integer checks ordered lower/upper bounds BEFORE
FPToSI/UI. Upper limits are exclusive powers of two. Lower limits account for
truncation and the source float's precision: MIN-1 is exclusive when representable;
otherwise MIN is inclusive. Unsigned input must be greater than -1. These tests
exclude NaN/infinities and allow fractions whose truncated value fits. This avoids
both LLVM poison and a new dependency on native trunc/truncf library calls.
FPTrunc is defined even on overflow; its result is checked for new infinity before
it can escape. Floating arithmetic itself intentionally permits IEEE infinity/NaN.

Supported hosts use the ordinary nearest/ties-even floating environment without
flush-to-zero. VOID currently exposes no rounding-mode controls. Foreign bindings
that alter host floating control state would need a future boundary contract;
this phase adds no environment-changing runtime subsystem.

## ABI, recovery and verification

The public CLI/JIT host entry remains exactly i32(i32). Wider numeric functions
are exercised internally through real source calls and the same backend. Native
same-width types do not become interchangeable. All source functions still use
the existing hidden failure context, module ownership and generation leases.
Full snapshots of const-containing files remain the conservative restart token.

VOID2014 retains integer literal range errors; VOID2015 adds checked conversion
errors, VOID2016 illegal numeric operators, and VOID2017 floating literal range
or exact-representability errors. VOID2001 continues to report type mismatches.
Snapshots, byte spans, deterministic error collection, poison recovery and the
full-graph validation gate remain intact. Failed source never supplies IR.

The ten existing suites remain; numeric and numeric_cli add real-file constant,
unoptimized/O2 JIT and linked unoptimized/O2 AOT checks. Fixtures cover all types,
all 100 conversion pairs, integer limits/wrapping/division, both float-to-integer
boundary families, float rounding/underflow, NaN/signed zero, source failures,
imports, exact entry ABI and retained reload generations. dynamic_widths.void
returns independently calculated checksums 47, -7, 48 for inputs 0, 1, 10;
its arithmetic depends on runtime input, including after optimization.

`tests/numeric/generate_fixtures.py` is a developer-only independent fixture oracle
using Python unlimited-precision integers and explicit IEEE neighbors. Checked-in
VOID fixtures execute without Python. Handwritten floating cases include rounding
per operation and a multiply/subtract whose result would change if fused.
WORK_LOG.md records actual toolchains, commands, results and repeat stress.

Deferred: exponent/base/separator literal syntax, float remainder, nullable casts,
platform-sized aliases, arbitrary public native ABIs, numeric formatting/hashing,
user-defined conversions and foreign floating-environment management. Recommended
next phase: validated analysis/dependency APIs and precise compatibility metadata,
then incremental tooling; managed types still require the separate GC integration gate.

## Phase 7 analysis and compatibility update

The earlier whole-file constant-token and analysis-API limitations above describe
the preceding phases. They are superseded by [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md):
one validated, owning analysis result now supplies declarations, snapshots and
resolved dependencies without IR/machine-code generation. Exact declaration tokens
and compiler-derived bindings replace whole const-containing source files in
reload compatibility. Ordinary body/trivia edits beside unchanged constants can
reload; actual constant changes require manual restart. Language rules, numeric
semantics and structured recovery remain unchanged.

## Stage 8 Sprint 2 numeric APIs

[The implementation contract](STAGE8_CALLS_NUMERIC.md) and
[actual acceptance](stage8/SPRINT2_ACCEPTANCE.md) record the current status.
Boss approved these semantics on 20 October 2026 in [DECISIONS.md](DECISIONS.md).
All ten numeric types expose exactly four members through the compiler-owned
prelude; no import or user-defined primitive extension is required or admitted.

```typescript
var maximum = i32.max_value
var minimum = i32.min_value
var a = i32.abs(-10)
var b = (-10).abs()
var health: i32 = -20
var result = health.abs().clamp(0, 100)
```

| Member | Exact contract |
| --- | --- |
| `T.min_value` | Signed integer minimum, unsigned zero, or most negative finite float |
| `T.max_value` | Integer maximum or most positive finite float |
| `T.abs(value: T) -> T` / `value.abs() -> T` | One canonical implementation; unsigned identity; checked failure at signed minimum; float sign bit cleared, including negative zero and NaNs |
| `T.clamp(value: T, lower: T, upper: T) -> T` / `value.clamp(lower: T, upper: T) -> T` | Inclusive bounds; reject NaN bounds or reversed bounds before propagating NaN input; permit infinite bounds and preserve in-range signed zero |

Properties and pure numeric calls work in constant expressions and parameter
and field defaults. Reached invalid operations diagnose there at compile time;
ordinary runtime calls produce the existing checked source failure and unwind
through cleanup. Unreached constant branches retain the existing short-circuit
rules. Integer unary negation still wraps; checked `abs` does not change it.

Each operation preserves its exact type. Existing contextual numeric literal
adaptation applies, with no new implicit widening or signed/unsigned conversion.
Type and receiver call forms use the same argument binder and typed callable;
receiver expressions evaluate once. Ordinary functions and methods now share the
initializer named/default argument rules documented in
[STAGE8_CALLS_NUMERIC.md](STAGE8_CALLS_NUMERIC.md). Explicit generic specializations
can use their numeric type's properties in pure defaults, such as
`func maximum<t>(value: t = t.max_value) -> t:`. Missing generic type arguments
are inferred from supplied values, not guessed from an omitted default.

Compiler-owned completion, typed hover, documentation and bound/unbound signature
help use the same registry. Builtins have no filesystem definition target;
navigation returns null rather than fabricating a file, while references still
identify actual source calls. `bool`, strings and collections gain no numeric API.
No additional arithmetic operation, provider/import search, boxing, recoverable
numeric result or mutable static field is introduced.
