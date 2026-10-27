> Implementation authorization, 26 October 2026: Boss approved all three design
> bundles and the complete sprint. Golden Wrench additionally requires exact
> numeric-primitive-only generic specialization, including rejection of nominal
> custom operators. This supersedes the historical proposal/stop wording below.
> Local implementation and verification are complete; no hosted acceptance
> or native Windows/macOS execution is claimed.

# Approved `math` contract

26 October 2026. Baseline: `9eaefbd339e2456295911cdecb2460d4143f34b0`.
**Approved for implementation and independent Golden Wrench source review.**
[Source findings and decisions](RESEARCH.md); [implementation mission](ROADMAP.md).
That baseline predates implementation. The current implementation is `stdlib/math.void`;
[API audit](API.md) and [handoff](HANDOFF.md) record its verification.

## 1. Initial scope

Provide one ordinary `use math` module with **29 generic functions and six
typed constants**. Cover practical scalar game calculations completely, using
existing numeric types. No Math/MathF split, general overload system, vectors,
implicit coercions, decimal type or special editor engine.

This proposal excludes integral exponentiation, decimal-place rounding, arbitrary
logarithm bases, hyperbolic/special functions, complex numbers, vector/matrix
operations, noise, angle wrapping, collection reductions, configurable rounding
modes, public fenv controls and approximate-equality defaults. They are useful
separate candidates, not missing parts of the proposed initial contract. In
particular, no arbitrary epsilon is declared a universal game tolerance.

## 2. Exact public signatures and type rules

Every row below denotes one ordinary source generic declaration. `t` is an
existing type parameter, not an overload or a new numeric constraint syntax.

| Declaration | Supported t | Result |
|---|---|---|
| `abs<t>(value: t) -> t` | All ten numeric primitives | Existing absolute-value contract |
| `min<t>(left: t, right: t) -> t` | All ten | Smaller value; NaN propagation and zero ties below |
| `max<t>(left: t, right: t) -> t` | All ten | Larger value; same policies |
| `clamp<t>(value: t, lower: t, upper: t) -> t` | All ten | Existing inclusive clamp contract |
| `sign<t>(value: t) -> t` | All ten | Same-type -1/0/+1; unsigned 0/+1 |
| `is_nan<t>(value: t) -> bool` | f32/f64 | Whether NaN |
| `is_infinite<t>(value: t) -> bool` | f32/f64 | Either infinity |
| `is_finite<t>(value: t) -> bool` | f32/f64 | Neither NaN nor infinity |
| `floor<t>(value: t) -> t` | f32/f64 | Toward negative infinity |
| `ceil<t>(value: t) -> t` | f32/f64 | Toward positive infinity |
| `trunc<t>(value: t) -> t` | f32/f64 | Toward zero |
| `round<t>(value: t) -> t` | f32/f64 | Nearest integral floating value, ties to even |
| `lerp<t>(start: t, end: t, weight: t) -> t` | f32/f64 | Unclamped interpolation/extrapolation |
| `inverse_lerp<t>(start: t, end: t, value: t) -> t` | f32/f64 | Unclamped fraction along a nondegenerate segment |
| `sin<t>(value: t) -> t` | f32/f64 | Sine, radians |
| `cos<t>(value: t) -> t` | f32/f64 | Cosine, radians |
| `tan<t>(value: t) -> t` | f32/f64 | Tangent, radians |
| `asin<t>(value: t) -> t` | f32/f64 | Principal inverse sine, [-pi/2,pi/2] |
| `acos<t>(value: t) -> t` | f32/f64 | Principal inverse cosine, [0,pi] |
| `atan<t>(value: t) -> t` | f32/f64 | Principal inverse tangent, [-pi/2,pi/2] |
| `atan2<t>(y: t, x: t) -> t` | f32/f64 | Quadrant-aware angle, [-pi,pi] |
| `sqrt<t>(value: t) -> t` | f32/f64 | Nonnegative principal square root |
| `pow<t>(base: t, exponent: t) -> t` | f32/f64 | Real floating exponentiation |
| `exp<t>(value: t) -> t` | f32/f64 | e to the value |
| `exp2<t>(value: t) -> t` | f32/f64 | 2 to the value |
| `log<t>(value: t) -> t` | f32/f64 | Natural logarithm |
| `log2<t>(value: t) -> t` | f32/f64 | Base-2 logarithm |
| `radians<t>(value: t) -> t` | f32/f64 | Degrees to radians |
| `degrees<t>(value: t) -> t` | f32/f64 | Radians to degrees |

All established numeric operands must have the same exact type; contextual
literal rules are unchanged. No bool, enum or nominal struct specialization is
admitted. Numeric return type is t, including rounding and sign; there is no
implicit float-to-integer result conversion. Use the existing checked `as` when
an integer is wanted. Unsigned sign never needs to construct a negative literal.

```void
use math

var health = math.clamp(75, 0, 100)       # i32
var wave = math.sin(0.5)                 # f32
var precise = math.sin<f64>(0.5)         # f64, no initial f32 rounding
var blend = math.lerp(0.0, 10.0, 0.25)  # f32
var heading = math.atan2(1.0, -1.0)      # y first, x second
var grid = math.floor(3.75) as i32       # explicit checked conversion
```

`math.sqrt(9)` infers i32 and rejects; `math.sqrt(9.0)` or
`math.sqrt<f32>(9)` works. An expected result annotation alone does not silently
convert established arguments. Mixed f32/f64 values reject. Named arguments,
written evaluation order, generic specialization and function values use the
ordinary compiler binder. No random-style arity adapter is extended to math.

Math functions are ordinary runtime functions. Module constants and pure field/
parameter defaults cannot call them today, even when a wrapper delegates to a
numeric builtin. Existing `i32.abs(...)`, `value.clamp(...)` and numeric metadata
retain their original constant-evaluation support. Local const bindings inside
functions can hold runtime Math results. Do not expand the constant interpreter
as part of this sprint.

### Constants

Expose `pi_f32`, `pi_f64`, `tau_f32`, `tau_f64`, `e_f32`, `e_f64`, each as an
ordinary typed module const. Suffixes make precision explicit without a new
generic-constant or expected-type lookup mechanism. No ambiguous unsuffixed pi.

| Constant | f32 bits | f64 bits |
|---|---|---|
| pi | 40490fdb | 400921fb54442d18 |
| tau = 2*pi | 40c90fdb | 401921fb54442d18 |
| e | 402df854 | 4005bf0a8b145769 |

These are bit descriptions, not current VOID hexadecimal literal syntax.
Use sufficiently precise decimal pi/e literals for f64, their checked constant
casts for f32, and exact multiplication by two for tau. For these specific
constants those definitions give the listed correctly rounded values. Angle
conversion uses pi rounded to t, then computes `value * (pi / 180)` or
`value * (180 / pi)`, respectively, at t precision. Do not multiply value by 180
first or convert f32 arguments through a double-precision public implementation.

## 3. Errors and numerical environment

Preserve existing numeric semantics: wrapping integer arithmetic, checked casts,
nearest/ties-even floating operations, gradual underflow and no implicit FMA,
reassociation or fast-math. New floating domain failures produce NaN; poles and
range overflow produce the defined infinity/zero. Do not add `math.error`, a
result-returning parallel family, native errno exposure, a panic on sqrt/log
domain errors or a global floating-environment manager.

The exceptions are already settled: signed-minimum abs and invalid clamp bounds
use the existing checked Runtime failure and source trace. They must not change
merely to make every Math function have identical failure behavior. NaN payloads,
NaN signs and signaling/trapping behavior are not new portability promises.
Foreign changes to rounding/FTZ/trap modes remain outside the existing environment
contract. Native errno/exception flags are not a Math API or preservation guarantee.

## 4. Core operations

`abs` and `clamp` delegate directly to `t.abs(value)` and
`t.clamp(value,lower,upper)`. Do not duplicate their algorithms or enlarge the
four-member numeric prelude.

- Signed abs at the minimum fails; unsigned abs is identity. Float abs clears
  the sign bit, including negative zero and NaNs, as already specified.
- Clamp checks NaN bounds/reversed bounds before propagating NaN input. Equal
  bounds and infinite ordered bounds are valid. In-range signed zero is retained.
- Min/max propagate a NaN if either argument is NaN. With two NaNs, return a NaN;
  no payload priority is promised. Do not substitute fmin/fmax or LLVM minnum,
  which can ignore a NaN operand.
- Min of zeroes is -0 if either is -0; max is +0 if either is +0. Both-negative
  max is -0 and both-positive min is +0. Equal nonzero values return that value.
- Sign preserves floating NaN and signed zero; infinities map to same-sign one.
  Signed integers return -1,0,1 without abs, including their minimum. Unsigned
  integers return 0 or 1. Return type remains the input type.

Min/max/sign are source-expressible. After NaN handling, zero ties can use
`-((-left)+(-right))` for min and `left+right` for max, **only when both compare
equal to zero**. All-zero integer operations are safe, and existing IEEE addition
gives the required floating signs. Nonzero selection uses comparisons, avoiding
signed overflow and any public bit-representation primitive.

## 5. Classification and rounding

Classification uses self-inequality and comparisons with t's finite min/max.
Infinity is outside those limits; NaN is unordered. A private compile-time type
check restricts these public functions to f32/f64; it is not a public trait.

All four rounding functions return a value of the original floating type.
Already-integral values, infinities and signed zero are unchanged; NaN returns
NaN. No conversion to i64, saturation, decimal-place precision or arbitrary epsilon.

Recommend one low-level floor operation. Derive ceil as `-floor(-value)` and trunc
by selecting floor for nonnegative input and ceil for negative input. Handle zero
and nonfinite input explicitly in source before native work.

For round, on finite input:

1. Let base = floor(value), fraction = value-base, with t operations.
2. Below one half select base; above one half select base+1.
3. At exactly one half, select base when `2*floor(base/2) == base`; otherwise
   base+1. The tested base is small enough for exact integral arithmetic when a
   fractional part exists; huge integral values return unchanged.
4. If the selected result is zero, return `value * zero` to preserve its sign.

Thus 0.5 -> +0, -0.5 -> -0, 1.5 -> 2, 2.5 -> 2, -1.5 -> -2 and -2.5 -> -2.
This avoids incorrect `floor(value+0.5)` behavior and avoids a new platform
roundeven symbol dependency. Existing language/format rounding is not changed.

## 6. Interpolation

These two functions take finite inputs. Return NaN if any input is nonfinite.
This validation precedes endpoint shortcuts. Fractions are **not clamped**.
Callers can explicitly clamp a weight or returned fraction using existing clamp.

### lerp

For finite inputs, weight zero returns start exactly, weight one returns end
exactly, including signed zero. Other weights with equal endpoints return start.
For remaining inputs, use separate t-precision operations:

- Opposite nonzero endpoint signs: `start*(1-weight) + end*weight`.
- Otherwise: `start + (end-start)*weight`.

For 0 < weight < 1, constrain the computed value to the inclusive numeric min/max
of the two endpoints using existing clamp. This corrects rounding overshoot and
guarantees a finite in-range result, including finite opposite extrema. It does
not clamp the weight or suppress NaN invalid inputs. Interior signed-zero sign
follows the algorithm; only endpoint returns promise the original bits.

Outside [0,1], extrapolation uses the same formula without result clamping.
Intermediate overflow may produce infinity or NaN even when rearranged exact
arithmetic would be finite; no extended-precision extrapolation guarantee is made.
Do not introduce hidden f64 widening or fused operations for f32.

### inverse_lerp

Unequal endpoints may be reversed. Equal endpoints, including the two zero signs,
return NaN. A value equal to start returns +0; equal to end returns 1. Otherwise
compute numerator = value-start and denominator = end-start at t precision.
If either difference overflows, recompute **both** using halves:

`(value/2 - start/2) / (end/2 - start/2)`.

All inputs are finite and endpoints differ. Halving in this overflow branch
keeps the differences finite and the denominator nonzero; tiny adjacent endpoint
differences never need this branch. In other cases use the original differences.
Final division can legitimately overflow or underflow for extrapolated fractions.
Do not divide zero by zero for a degenerate segment or map it silently to zero.
This source policy avoids spurious inf/inf and finite-ratio subtraction overflow;
it does not promise an exactly rounded rational quotient or an inverse bitwise
identity with lerp.

## 7. Trigonometry, roots, powers and logarithms

Normal finite transcendental evaluations use the target's standard math facility.
Radians are the argument/result unit for trig. No approximate pi-multiple snapping,
angle wrapping, home-grown range-reduction polynomial or sin/cos division for tan.

Normalize the following special cases in the source facade before the native
primitive, keeping one public policy in ordinary VOID:

| Function | Required special values |
|---|---|
| sin, tan | Preserve either zero; NaN -> NaN; either infinity -> NaN |
| cos | Either zero -> 1; NaN/infinity -> NaN |
| asin | Preserve zero; outside [-1,1] -> NaN; endpoints -> +/-pi/2 |
| acos | Outside [-1,1] -> NaN; +1 -> +0, -1 -> pi, either zero -> pi/2 |
| atan | Preserve zero; infinities -> +/-pi/2; NaN -> NaN |
| sqrt | Preserve zero; negative nonzero, including -infinity, -> NaN; +infinity -> +infinity; NaN -> NaN |
| exp, exp2 | Either zero -> 1; +infinity -> +infinity; -infinity -> +0; NaN -> NaN |
| log, log2 | Either zero -> -infinity; negative values -> NaN; 1 -> +0; +infinity -> +infinity; NaN -> NaN |
| radians, degrees | Preserve zero sign; infinity retains its sign; NaN -> NaN; finite scaling may overflow/underflow |

Normal positive sqrt uses the conforming IEEE nearest square-root operation.
Its finite result and the exact rounding operations/constants have exact reference
bit tests. Transcendental finite values and interpolation error need appropriate
accuracy tests rather than a cross-platform bit-equality promise.

### atan2: y first, x second

Handle NaNs first. Retain y's sign in the results below:

| Inputs | Result |
|---|---|
| y = +/-0, x positive or +0 | y |
| y = +/-0, x negative or -0 | +/-pi |
| Nonzero y, x either zero | +/-pi/2 |
| Finite nonzero y, x = +infinity | Signed zero with y's sign |
| Finite nonzero y, x = -infinity | +/-pi |
| y infinite, finite x | +/-pi/2 |
| Both infinite, x positive | +/-pi/4 |
| Both infinite, x negative | +/-3*pi/4 |

Then delegate finite nonzero inputs to atan2f/atan2. Never implement the regular
case as atan(y/x), which discards quadrant/scale information. Source can detect
negative zero using a guarded reciprocal (`value == 0 and 1/value < 0`) under
existing floating division semantics. No new public signbit primitive is needed.
Use pi rounded to t, half = pi/2, quarter = pi/4 and three_quarters = pi-quarter,
then apply the selected sign. This fixes the source operation order for the
special branches without a rounded intermediate multiplication by three.

### pow: floating real power

Use this precedence and policy; exponent integrality is determined in source
using floor, and oddness by comparison with `2*floor(exponent/2)`, with finite
guards. No float `%`, integer exponent cast or general overload.

1. Any exponent equal to zero -> 1, including NaN base and 0^0. Base +1 -> 1,
   including NaN exponent. Base -1 with infinite exponent -> 1.
2. Other NaN operands -> NaN. For infinite exponent, compare abs(base) with 1:
   magnitude below 1 tends to +0 for positive infinity and +infinity for negative;
   magnitude above 1 reverses those results.
3. Zero base with finite positive/negative exponent produces zero/infinity;
   it is negative only for negative zero and an odd integral exponent.
4. Infinite base with finite nonzero exponent produces infinity for positive,
   zero for negative, negative only for negative base and an odd integral exponent.
5. Finite negative nonzero base with nonintegral exponent -> NaN. Otherwise call
   the native positive-magnitude power and apply a negative sign for odd exponents.
6. Remaining positive finite base/finite exponent use the native primitive.

Overflow produces signed infinity as above; underflow is gradual and may become
signed zero. Every sufficiently large representable floating integer is even;
source parity must work beyond i64/u64 range. This is particularly relevant to
documented CRT limitations for very large integral exponents. Do not pass a
negative base to native pow and assume every platform resolves parity identically.

## 8. Compiler/native boundary and distribution

All public declarations, constants and special-value policies remain in source.
Reuse numeric metadata and the existing abs/clamp kernels. No Runtime state,
managed resource, native object, GC root, asynchronous operation or math provider.

Use one canonical `/@stdlib/math.void` private operation family, with an
independently validated closed typed-IR operation. It accepts only scalar f32/f64,
has fixed unary/binary arity and returns that same type. Its kinds are:

- Identity/type gate: no machine operation; useful for source-only float functions
  because the language has no public floating-point generic constraint.
- floor, sqrt, sin, cos, tan, asin, acos, atan, pow, exp, exp2, log, log2:
  select the existing LLVM **19.1.7** intrinsic of matching precision.
- atan2: select the fixed standard C symbol atan2f or atan2; LLVM 19.1.7 has no
  atan2 intrinsic in this family.

Use private calls such as `_math_float<t>(value)` and `_math_sin<t>(value)` inside
the canonical source file; these are private compiler hooks, never public extension points. A small shared closed arity/type description
can serve frontend/IR checks; it is not a new numeric type system. No operation
name/arity is selected dynamically. Unused operands and malformed operation tags
must reject in IR validation. Ordinary source shadowing/visibility remains intact.

Lower in the existing backend with no fast-math/approximation flags. Register the
standard float/double helper addresses through existing `Jit::bind` for this
closed set where LLVM may emit libcalls, retaining its existing platform/process
link order for target-generated helpers. Direct atan2 uses only its fixed C scalar
ABI, without invented readnone/errno-preservation attributes. No generic source
native-call privilege is granted. Keep these audited scalar operations worker-safe;
source globals/OS/native resource restrictions remain unchanged.

Linked AOT uses the existing Clang **g++ driver mode** and native flags, which on
the audited Linux toolchain already supplies libm. Windows uses its existing CRT;
macOS uses libSystem. Confirm actual platform links rather than adding a library
discovery subsystem. No new Runtime bridge, staging scheme or LLVM fork is needed.
Do not use llvm.powi for this API, and avoid a roundeven/roundevenf dependency.

Recursive source installation, cache identity, reload leases and compiler-owned
LSP metadata already apply. New apps/imports have no Math initialization side
effects. Constant changes use existing restart rules; compatible function edits
use existing generation behavior. Cached code resolves native symbols for the
current process; it must not serialize a math function address as language state.

## 9. Accuracy and review decisions

Integer selectors/sign, inherited abs/clamp semantics, float zero/NaN classes,
rounding, constants and specified endpoint returns are exact contracts. Source
arithmetic still rounds at each existing t operation. Native transcendental
results may vary with library, CPU and optimization; no bit-identical cross-
platform promise or universal unmeasured ULP bound is made. A fixed Random seed
does not make a simulation using arbitrary transcendental results bit-identical.

The [roadmap](ROADMAP.md) defines independent reference/tolerance gates. Native
Windows/macOS execution remains required before claiming those platform results.
Boss approved the API/types, floating/error/rounding policies and interpolation
behavior listed in RESEARCH.md. Golden Wrench requires primitive-only public
generic specialization before body analysis: all ten numeric primitives for the
five core functions, f32/f64 for the other 24. Nominal custom operators grant no
Math admission. The compiler uses existing numeric metadata for this check;
private helpers remain ordinary source generics. Readonly scalar operands from
const parameters remain valid copies in private IR; results are clean scalar
f32/f64 types. No new public constraints or numeric type system are introduced.
