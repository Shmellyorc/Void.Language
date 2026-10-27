# Math public API and source audit

26 October 2026. Implementation inventory; local acceptance is recorded separately
after execution. This document does not claim hosted or native Windows/macOS
acceptance. The approved numerical contract remains [DESIGN.md](DESIGN.md).

[Canonical source](../../stdlib/math.void) contains **29 ordinary public generic
functions and six typed module constants**. Every public declaration has `##`
documentation, and every written parameter and return value has `@param` /
`@return` documentation. There are no public Math types, errors, globals,
initializers, receiver methods or extra primitive members.

## Exact type contract

Numeric below means i8/u8/i16/u16/i32/u32/i64/u64/f32/f64. Floating means exactly
f32/f64. All established operands have the same exact type; numeric results
retain that type. Classification returns bool. Ordinary contextual literals,
named arguments, written evaluation order, specialization and function values
use the existing binder. `sqrt(9)` rejects its inferred i32; `sqrt<f64>(9)` is
valid. Expected result annotations do not coerce established arguments.

[Generic specialization](../../src/frontend/generics.inc) enforces the canonical
public functions' primitive restrictions independently of their source bodies,
including nominal types that supply custom operators. Every floating public
body also begins with the private `_math_float<t>` identity operation. Bool,
enum, vector and nominal specializations cannot acquire Math access. No public
trait, overload family or Random-style arity adapter was added.

## Complete function inventory

Every declaration below is implemented in [math.void](../../stdlib/math.void).

| Declaration | Supported t | Source policy and execution boundary |
|---|---|---|
| `abs<t>(value: t) -> t` | Numeric | Delegate to existing `t.abs`; signed minimum retains checked Runtime failure, unsigned is identity, floating sign bit clears. |
| `min<t>(left: t, right: t) -> t` | Numeric | Source comparisons propagate either NaN; zero-only sign arithmetic selects -0 if either input is -0. |
| `max<t>(left: t, right: t) -> t` | Numeric | Source comparisons propagate either NaN; zero-only addition selects +0 if either input is +0. |
| `clamp<t>(value: t, lower: t, upper: t) -> t` | Numeric | Delegate to existing `t.clamp`; validate NaN/reversed bounds before NaN input, preserve inclusive bounds and in-range zero sign. |
| `sign<t>(value: t) -> t` | Numeric | Source -1/0/+1 in t, unsigned 0/+1; preserve floating NaN and signed zero, handle signed integer minimum without abs. |
| `is_nan<t>(value: t) -> bool` | Floating | Source self-inequality after the floating identity gate. |
| `is_infinite<t>(value: t) -> bool` | Floating | Compare against t's finite minimum/maximum; NaN returns false. |
| `is_finite<t>(value: t) -> bool` | Floating | Ordered comparisons with t's finite limits; zero and subnormals are finite. |
| `floor<t>(value: t) -> t` | Floating | Preserve zero/nonfinite values in source; native floor rounds toward negative infinity. |
| `ceil<t>(value: t) -> t` | Floating | Preserve zero/nonfinite values; derive `-floor(-value)` in source. |
| `trunc<t>(value: t) -> t` | Floating | Preserve zero/nonfinite values; source selects floor or negated floor toward zero. |
| `round<t>(value: t) -> t` | Floating | Source nearest/ties-even from floor and exact floating parity; restore input sign when the result is zero. |
| `lerp<t>(start: t, end: t, weight: t) -> t` | Floating | Validate all inputs finite before exact endpoint/equal-endpoint returns; source sign-aware formula and interior result clamp, unclamped extrapolation. |
| `inverse_lerp<t>(start: t, end: t, value: t) -> t` | Floating | Validate finite inputs and unequal endpoints; exact +0/1 endpoint fractions, recompute both differences with halves if either overflows; reversed endpoints allowed. |
| `sin<t>(value: t) -> t` | Floating | Radians; preserve either zero, propagate NaN, turn infinities into NaN; native finite sine. |
| `cos<t>(value: t) -> t` | Floating | Radians; either zero yields one, NaN/infinity yields NaN; native finite cosine. |
| `tan<t>(value: t) -> t` | Floating | Radians; preserve either zero, propagate NaN, turn infinities into NaN; native tangent directly. |
| `asin<t>(value: t) -> t` | Floating | Preserve zero; outside [-1,1] yields NaN; endpoints use signed typed pi/2, other values use native inverse sine. |
| `acos<t>(value: t) -> t` | Floating | Outside [-1,1] yields NaN; +1/-1/zero yield +0/pi/pi/2, other values use native inverse cosine. |
| `atan<t>(value: t) -> t` | Floating | Preserve zero/NaN; infinities use signed typed pi/2, other values use native inverse tangent. |
| `atan2<t>(y: t, x: t) -> t` | Floating | y first; source handles NaN, all signed origins/axes and infinite quadrants; finite nonzero pairs use native atan2. |
| `sqrt<t>(value: t) -> t` | Floating | Preserve zero/+infinity/NaN; negative nonzero inputs yield NaN; positive finite inputs use IEEE nearest square root. |
| `pow<t>(base: t, exponent: t) -> t` | Floating | Source identity-before-NaN precedence, zero/infinity magnitude/sign policy and floating integrality/parity; negative integral bases call native positive-magnitude power then restore an odd sign. |
| `exp<t>(value: t) -> t` | Floating | Zero yields one; +/-infinity yields +infinity/+0; NaN propagates, finite values use native exp. |
| `exp2<t>(value: t) -> t` | Floating | Same special-value policy as exp; finite values use native base-two exponential. |
| `log<t>(value: t) -> t` | Floating | Either zero yields -infinity, negative values yield NaN, one yields +0, +infinity/NaN pass through; native positive finite logarithm. |
| `log2<t>(value: t) -> t` | Floating | Same special-value policy as log; native positive finite base-two logarithm. |
| `radians<t>(value: t) -> t` | Floating | Preserve zero/nonfinite values; compute `value * (pi / 180)` with pi rounded to t and separate t operations. |
| `degrees<t>(value: t) -> t` | Floating | Preserve zero/nonfinite values; compute `value * (180 / pi)` with pi rounded to t and separate t operations. |

Atan2 detects negative zero only through a guarded reciprocal. Its special angles
use pi/2, pi/4 and pi-minus-quarter at t precision, then y's sign. Pow never casts
the exponent to an integer or uses floating remainder: `floor(exponent)` tests
integrality and `2*floor(exponent/2)` tests evenness with finite guards, including
representable integers beyond u64. No negative base is delegated to native pow.

## Complete constant inventory

| Declaration | Exact IEEE bits |
|---|---|
| `const pi_f32: f32` | `40490fdb` |
| `const pi_f64: f64` | `400921fb54442d18` |
| `const tau_f32: f32` | `40c90fdb` |
| `const tau_f64: f64` | `401921fb54442d18` |
| `const e_f32: f32` | `402df854` |
| `const e_f64: f64` | `4005bf0a8b145769` |

Pi/e use precise decimal f64 definitions and their checked constant casts to f32.
Tau is twice the corresponding pi. These are ordinary typed module constants;
there is no unsuffixed or generic constant lookup.

## Source/compiler boundary

[math_ops.hpp](../../include/void/math_ops.hpp) describes one closed family:
identity plus 14 fixed native operations. [math.inc](../../src/frontend/math.inc)
admits its private spellings only in `/@stdlib/math.void`, respects ordinary
source shadowing, and uses the existing call binder. The identity stays in typed
IR. [IR validation](../../src/ir/module.cpp) independently checks the canonical
owner, closed tag, fixed arity, f32/f64 agreement and unused/malformed fields.

[LLVM lowering](../../src/backend/llvm/backend.cpp) selects matching LLVM 19.1.7
intrinsics for floor, sqrt, sin, cos, tan, asin, acos, atan, pow, exp, exp2, log and
log2. Atan2 uses only the fixed `atan2f`/`atan2` C scalar ABI. Existing `Jit::bind`
registers the corresponding standard float/double helpers. No Runtime provider,
managed object, public native-call privilege, dynamic opcode or library discovery
was introduced. No fast-math flags, implicit FMA, reassociation, powi or roundeven
dependency was added. Existing installation and native link mechanisms apply.

The [capability analysis](../../src/frontend/capabilities.inc) treats this audited
scalar Math family as pure. Worker composition also required recognizing exactly
the two existing compiler-owned scalar `abs`/`clamp` targets; the check requires a
numeric result and its matching private numeric symbol. Source/native names do
not acquire that identity. Existing global, OS, arbitrary native and resource
restrictions remain in force; these functions introduce no GC roots.

## Tooling and verification boundaries

Public discovery, completion, hover, definitions and references consume the real
source declarations and docs. [Generic inference](../../src/frontend/generic_inference.inc)
now publishes the source generic signature before attempting editor-probe
inference, so incomplete `math.sin(` can retain documented `value: t` help.
Successful inference can replace it with the concrete f32/f64 signature. This
ordinary generic function/method recovery uses no Math editor catalog and retains
the existing suppression of undocumented active parameters.

Math calls remain runtime functions. Module constants and pure field/parameter
defaults cannot call them; local const bindings can hold runtime results. Exact
constants, selectors/signs, zero classes, rounding and specified endpoints have
exact reference contracts. Finite transcendental accuracy is checked against
independent MPFR vectors with explicit local test budgets, rather than a universal
API ULP or cross-platform bitwise promise. Interpolation preserves the source
operation order and precision; no exact rational or inverse-identity promise is
made. NaN payload/sign, errno and floating exception flags are not API guarantees.

[Math tests](../../tests/math) cover source typing, forged IR, source execution,
worker composition, Runtime failures, compiler-owned LSP, installation,
cache/reload and live managed callers. Their [process bootstrap](../../tests/math/bootstrap.py)
reuses the established crash preservation boundary and supplies Math source and
generator provenance. Numerical results, configuration receipts and remaining
platform limits belong to the final implementation/handoff evidence, not this
inventory.
