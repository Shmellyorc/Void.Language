> Implementation authorization, 26 October 2026: Boss approved all three design
> bundles and the complete sprint. Golden Wrench additionally requires exact
> numeric-primitive-only generic specialization, including rejection of nominal
> custom operators. This supersedes the historical proposal/stop wording below.
> Local implementation and verification are complete; no hosted acceptance
> or native Windows/macOS execution is claimed.

## Current engineering disposition

Steps 1 through 7 are implemented: canonical source, primitive specialization,
closed validated IR and LLVM/JIT integration, all source algorithms and docs,
compiler tooling, relocated installation, cache/reload, worker and managed-GC
checks. Step 8 is complete: the consolidated local Debug/Release/strict selection and
retained affected follow-ups yield 31/31 effective areas per profile (93/93).
Real 1 MiB frontend and four-mode Math execution pass in all three profiles.
The exact outcome, original failures and limits are recorded in
[HANDOFF.md](HANDOFF.md); no hosted acceptance follows.

The ordered plan below is retained as the approved implementation checklist.
Its historical pre-approval stop wording is superseded by the authorization above.
Native Windows/macOS remain unexecuted locally. Root README, Git mutations and
hosted workflows remain outside scope. All engineering evidence stays here.

# Math: complete implementation sprint

26 October 2026. Baseline: `9eaefbd339e2456295911cdecb2460d4143f34b0`.
**Approved complete sprint.** [DESIGN.md](DESIGN.md) is the approved contract.
[RESEARCH.md](RESEARCH.md#7-genuine-decisions-for-wrench-and-boss) records the three
approved owner decision bundles and historical source evidence. Golden Wrench
reviews the completed source/evidence; Boss retains release and Git control.

## 1. Outcome and boundaries

One cohesive sprint delivers the approved complete module: all 29 source
functions, six constants, exact type/edge policies, minimal backend support,
source docs, compiler-owned LSP and installed JIT/AOT support. An intrinsic demo,
f64-only implementation, ordinary-input-only tests or first subset is not completion.

Keep LLVM 19.1.7, frontend/binder/numeric metadata, typed IR, Runtime, GC, native
build staging/publication and editor architecture. Do not add a public overload
or trait system, public primitive extensions, a second numeric kernel, Runtime
provider, alternate libm, native library discovery or approximate math framework.
No unrelated language changes, Random changes, root README edits or routine CI.
Boss alone controls Git staging/branches/commits/pushes and hosted workflows.

## 2. Ordered engineering work

### 1 — Approval, baseline and focused test seam

Record Boss's API/numerical/interpolation choices and reconcile all three Math
documents before production work. Capture the actual starting source/working-tree
state. Preserve existing filesystem/optimizer crash evidence unchanged.

Reuse existing numeric, random, stdlib-install and LSP test patterns. Connect the
current exact-source preservation wrapper before running generated fixtures.
Choose a compact set of fixture programs grouped by concern; avoid a huge
unrolled generic program or dozens of new LLVM-linked executables.

### 2 — Add the closed scalar floating boundary

Use a small outlined frontend handler at the current canonical stdlib-call seam,
with private calls available only to `/@stdlib/math.void`. Reuse numeric::floating,
ordinary argument binding and generic specialization. No runtime calls from
module constants/defaults are newly admitted.

Add one typed IR operation with a closed kind for identity/type check and the 14
native operations in DESIGN.md. Share its fixed arity/type description where
useful; keep LLVM types/IDs in the backend. Validate f32/f64, exact operand/result
agreement, permitted kind, operand count and unused-operand shape independently.
Identity remains in validated IR, then lowers to its input; do not erase the type
boundary before IR validation. No dynamic opcode/string invocation API.

Lower the 13 available math intrinsics using their actual LLVM 19.1.7 IDs and
matching precision. Lower atan2 to the fixed C scalar prototype, y then x.
Use existing Jit::bind for applicable standard float/double helpers and retain
platform/process imports for target-generated helpers. Do not add flags permitting
approximation, NaN assumptions, reassociation, signed-zero loss or contraction.
Do not use roundeven/roundevenf or powi.

Classify this narrow scalar family as pure for source reference/task effects,
with no managed ownership or arbitrary native-call permission. Preserve current
global/resource/OS worker bans. No new Runtime bridge is required.

Verify malformed source/private spoofing and forged IR rejections before native
execution. Run a small real VOID float-call fixture through normal JIT, O2 JIT and
linked AOT early to catch missing symbols in the actual isolated generation path.

### 3 — Complete numeric core, classification and constants

Add `stdlib/math.void` with ordinary generic declarations. Abs/clamp forward to
the existing primitive prelude. Implement min/max/sign in source with NaN checks,
zero-only sign arithmetic and typed constants. Classification uses the private
float gate plus existing finite limits/comparisons; it needs no isnan Runtime call.

Add the six typed constants and their independently pinned bits. Use existing
decimal literals/constant casts; no unsupported hexadecimal/exponent syntax in
VOID fixtures. Verify generic inference, explicit f64 calls, same-type sign and
signed-minimum behavior for every integer width.

### 4 — Complete rounding in source over floor

Expose floor with source zero/nonfinite handling and the shared primitive.
Derive ceil/trunc and ties-even round as specified, restoring negative zero where
round reaches zero. Keep all intermediate source values in t; do not accidentally
introduce a default f32 temporary in a generic f64 body.

Test exact half ties and neighbors, both zero signs, subnormals, large already-
integral floats, extrema, infinities and NaNs. Check output bits and return types,
including invalid integer specializations and explicit checked casts afterwards.
No integer cast inside the rounding implementation or host rounding-mode control.

### 5 — Complete interpolation and angle conversion

Implement the finite-input policy, endpoint shortcuts, sign-aware lerp and
bounded-range correction in source. Implement inverse interpolation with both
differences half-scaled only when either overflows; reversed endpoints remain
legal and degenerate endpoints return NaN. No silent weight/fraction clamp.

Implement radians/degrees using precision-matched pi and fixed source operation
order. Verify finite opposite extrema, nearby large endpoints, extrapolation,
tiny intervals, invalid/nonfinite inputs and exact zero/one endpoint results.
The small source algorithms share private classification/constants rather than
creating interpolation opcodes or a double-only helper.

### 6 — Complete transcendental/root/power facades

Implement all seven trig functions, sqrt, pow, exp/exp2 and log/log2 with the
complete special-value tables in DESIGN.md. Keep ordinary finite calculations in
the standard intrinsic/libm facility; do not create approximation polynomials.

Atan2 must normalize signed origin/axis/infinity quadrants before its regular
native call. Pow must handle identities before NaNs, all zero/infinity cases,
source integrality/parity for arbitrarily large floating exponents and positive-
magnitude native power for a negative integral base. Avoid floating remainder,
signed integer conversion or native negative-base parity assumptions.

Use shared tiny source helpers for typed zero/one/NaN/infinity, negative-zero
tests, finite predicates and parity where helpful. Do not add a public error
hierarchy or duplicate special policy in C++ and VOID.

### 7 — Tooling, installation, cache and workers

Document every public function/constant with the current simple ##/@param/@return
contract: units, exact types, return precision, failure/NaN behavior and signs.
Parameter docs belong in contextual signature help; use existing suppression/
warning rules. Public hovers/definitions should resolve real math.void source.

Use compiler-owned discovery for `use math`, alias/member completion, generic
signatures, references and protected stdlib navigation/rename. No VS Code,
VSCodium, Neovim or Vim math catalog. Keep incomplete-source recovery, immutable
snapshots, UTF-16 spans and stale-result rejection intact.

Recursive installation already includes the new source. Check an actual relocated
compiler and matching stdlib with JIT/O2 and voidc build followed by executable
launch. Preserve source/VFS and native build security/path behavior; no new native
staging or manifest format. Standard C++ linking should already supply required
host math facilities; verify before changing dependencies.

Test a cache-hit Math program and compatible source replacement with no serialized
native addresses, plus a constant change requiring restart under existing rules.
Verify worker-local scalar Math through a direct helper and preserved rejection
of arbitrary native effects. Scalar operations need no special GC roots; include
an ordinary managed caller retaining live objects across Math calls under GC.

### 8 — Consolidated local acceptance and review handoff

Use focused Linux checks after affected steps. Once stable, run one relevant
consolidated Debug, Release and strict ASan/UBSan/leak selection, plus actual
1 MiB requirements. Re-run only affected failures/new changes. Do not require
the complete seven-job matrix for each edit or dispatch any hosted job here.

Produce the actual public API completeness table, commands, configuration/toolchain
hashes, independent vectors, pass/fail counts, original failures and native
platform limits. Golden Wrench reviews source and evidence. Boss controls native
verification authorization, Git and release; unavailable native results stay
explicitly pending rather than becoming Linux-inferred passes.

## 3. Focused acceptance matrix

| Concern | Required cases | Surface |
|---|---|---|
| Source typing | All ten core types; f32/f64 advanced types; bool/enums/nominal/collections rejected; exact mixed-type rejection; named order; explicit generics; runtime/module-const distinction | Existing frontend/binder harness |
| Private boundary | Spoofed math filename/alias, private names/type arguments, wrong arity; forged kinds/int/vector/mismatched precision/result/unused operands | Frontend and independent IR tests |
| Core | Every signed minimum/maximum and unsigned maximum; abs checked minimum; no selector overflow; clamp NaN-bound precedence/infinite bounds/zero preservation | Normal/O2 JIT, linked O0/O2 AOT |
| NaN/zero | Both argument orders for min/max; all four zero pairs; sign NaN/infinity/zero; classification including subnormals | Bit/classification vectors in all modes |
| Rounding | +/-half ties and neighbors; +/-0; smallest subnormal; mantissa integral threshold; largest finite; NaN/infinity; f64 values outside i64 range | Exact reference bits; all modes |
| Interpolation | Both directions; exact endpoints; equal endpoints; opposite extrema; large-close endpoints with overflowed numerator; subnormal/tiny intervals; nonfinite/degenerate NaN; extrapolation; no hidden clamp | Ordered arithmetic oracle plus analytic/reference cases |
| Trig | Radians, principal ranges, both zero signs, endpoints, all atan2 origin/axis/infinity quadrants, NaNs/infinities, very large finite argument reduction | Independent accuracy bank and exact special cases |
| Powers/logs | sqrt negative/zero/infinity; pow identities/NaNs/negative fractional base/odd-even huge exponents/zero and infinity signs; exp/log poles, overflow, gradual underflow | Independent bits/classes and finite reference tolerances |
| Constants/angles | Six exact bit patterns, f32/f64 labels, correct units, single-precision operation order and no silent widening | Source constants/defaults plus runtime checks |
| Cleanup/ownership | Inherited abs/clamp source failures retain trace/defers; source-only Math adds no owned resource; managed caller roots survive; worker scalar results | Existing numeric/cleanup/GC/async fixtures |
| Runtime compatibility | Fresh source/cache hit, actual isolated ORC link, repeated calls, compatible replacement, constants restart | Existing cache/reload integration |
| LSP/install | Use/module/member completion, aliases, signatures/parameter docs/hover, navigation/references, rename guards, malformed source recovery; relocated CLI JIT/AOT | Existing stdio LSP/install harnesses |
| Stack/profiles | Bounded positive and negative frontend depth, representative native program at 1 MiB; Debug/Release/strict affected tests | Existing small-stack/profile mechanisms |

Four execution modes mean real normal/O2 VOID JIT and separately linked
unoptimized/optimized AOT. Force dynamic inputs through at least one caller so
constant-folded examples do not replace actual math calls. Strict host sanitizer
flags do not instrument every generated JIT instruction; describe that limit
honestly and retain linked sanitizer-supported tests.

## 4. Oracles and tolerances

Use exact bits for constants, selectors/zero policy, rounding, specified endpoint
returns and correctly rounded sqrt reference cases. Test NaN classification
rather than payload/sign. Preserve the existing arithmetic per-operation tests
and no-contraction regression.

Prepare a small checked-in high-precision reference bank generated independently
with MPFR or equivalent correctly rounded/high-precision tooling, recording its
version, precision, rounding and source input bits. This is a development oracle,
not a production library dependency or a requirement to install MPFR on end-user
machines. Python math/native libm alone is not an independent oracle for that
same host's native implementation. Existing arbitrary-precision/bit fixture
generation patterns are reusable; no new validation framework is needed.

For finite transcendental vectors in ordinary magnitudes, start with **4 ULP for
f32 and 8 ULP for f64** as proposed engineering quality gates, not a proven
universal public accuracy bound. Compute ULP distance from ordered bit patterns.
Use dedicated absolute-error/range checks near mathematical zero and poles, exact
class/sign checks on underflow and special values, and high-precision references
for large arguments. Do not let a generous absolute tolerance hide flushed
subnormals, a wrong quadrant or a sign error. Very large trig argument tests must
compare to the function of the exact stored input, not a decimal idealization.

For interpolation, separately compare the specified t-operation algorithm and
the mathematical high-precision value: the first checks semantic/codegen parity,
the second reveals conditioning/accuracy limits. Exact invertibility is not an
acceptance criterion. If a native platform misses a quality gate, retain the
case and investigate before changing code or tolerances; Wrench reviews any
function-specific justified adjustment. Do not claim global bit determinism.

## 5. Native checks requiring later authorization

Native Windows **Debug and Release** must demonstrate both precisions, generic-
CPU linked AOT (including floor helper resolution and _fltused), real ORC/default
and isolated-generation math symbols, all normalized atan2/pow special cases,
underflow/zero behavior and relocated install operation. A Linux COFF object is
not evidence of those links or executions.

Native macOS ARM64 must demonstrate the same public policies, libSystem/helper
resolution, actual JIT/AOT/installed operation, signed zero and strict precision.
Cross-emitted Mach-O objects are not execution evidence. Existing platform-specific
1 MiB obligations remain. Request only these focused owner-authorized checks when
ready; do not schedule routine hosted CI or change main CI.

## 6. Crash evidence and completion gate

Reuse the current `tests/text_encoding/crash.py` and `tests/random/harness.py`
patterns. The baseline already preserves timeout evidence as well as signals,
Windows exceptions and reserved sanitizer exits. Do not repeat the obsolete
claim that timeout preservation still needs implementation.

Every abnormal compiler/runner exit must preserve exact generated/imported source
and unsaved editor snapshots as appropriate, named `*_crash.void`, before cleanup,
retry or reduction. Supply original source_inputs even when launching a linked
binary. Retain unique incident directories, matching source/preserved hashes,
compiler/binary hash, command/cwd/status/output, timeout details and generator
provenance. Preserve unexpected internal exits through the existing small harness
boundary too. Expected normal diagnostics remain distinct from crashes.
Never overwrite evidence, reconstruct bytes later as originals, assume LLVM is
at fault or call a successful retry a repair. Stop the affected experiment and
classify a new incident before continuing it.

Completion requires all approved declarations/documentation and tests, unchanged
numeric contracts, strict private/IR checks, passing focused Linux evidence,
honest native-platform disposition and preserved incidents. No hosted acceptance
or production readiness is inferred from this research's scratch checks.

**Return the complete implementation/evidence for Golden Wrench review. Boss
retains all authorization. The implementation is complete and stops for independent source review.**
