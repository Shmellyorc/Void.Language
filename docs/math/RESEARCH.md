> Implementation authorization, 26 October 2026: Boss approved all three design
> bundles and the complete sprint. Golden Wrench additionally requires exact
> numeric-primitive-only generic specialization, including rejection of nominal
> custom operators. This supersedes the historical proposal/stop wording below.
> Local implementation and verification are complete; no hosted acceptance
> or native Windows/macOS execution is claimed.

## Implementation reconciliation, 26 October 2026

The approved research is now implemented in [math.void](../../stdlib/math.void).
[API.md](API.md) audits all 29 functions and six constants. The source findings
below remain the historical `9eaefbd3` audit, not a statement that Math is absent
from the current working tree. Final local verification is recorded in [HANDOFF.md](HANDOFF.md): 31/31 effective
areas per profile after retained targeted follow-ups, 93/93 combined. Native
Windows/macOS remain unverified; the research snapshot below is historical.

Actual-source corrections made during engineering:

- Ordinary source operator compatibility is insufficient for primitive-only
  admission. Canonical public Math generics now check `numeric::number` or
  `numeric::floating` at specialization before cached/body analysis, attaching
  unsupported-type diagnostics to the caller. No public trait was added.
- Private hooks share 15 closed operation kinds in `math_ops.hpp`; typed IR
  independently validates canonical ownership, exact float precision, fixed
  arity and unused metadata. Const scalar operands may retain their existing
  readonly-copy marker; operation results are clean f32/f64 scalars.
- Worker effect analysis previously failed to recognize the existing generated
  numeric abs/clamp kernels. Only their exact compiler-owned numeric symbols
  are now recognized as scalar operations, alongside the closed Math family.
  Existing native, OS, global and resource worker restrictions remain enforced.
- An incomplete generic call with no inferable arguments previously lost
  signature help. Shared generic tooling now records the source declaration
  before inference and lets a successful specialization refine it. Math has no
  editor-specific declaration catalog.
- LLVM 19.1.7's installed intrinsic definitions and actual lowering support all
  13 required unary/binary intrinsic kinds. atan2 uses the fixed scalar C ABI.
  Existing g++-driver AOT links provide the host math library without new native
  discovery, Runtime providers or build-security changes.

MPFR 4.2.2 generated checked-in exact-input references at 256 and 512 bits,
requiring identical directly rounded target bits. The test bank includes 224
native/root vectors and 44 mathematical interpolation references. Native quality
gates remain 4 ULP f32 / 8 ULP f64, with exact sqrt and explicit class/sign tests.
Interpolation separately checks exact prescribed source operations and records
error against one-round high-precision arithmetic; these are distinct contracts.

# Math research: foundation, numerical choices and native risks

26 October 2026. Audited HEAD: **`9eaefbd339e2456295911cdecb2460d4143f34b0`**.
Boss reports Random Sprint 4 locally accepted after Wrench review. This assignment
authorizes Math research/documentation only, not implementation or hosted acceptance.
[Proposed contract](DESIGN.md); [bounded engineering mission](ROADMAP.md).

## 1. Findings and recommendation

VOID already has the necessary number types, exact conversions, generic source
functions, numeric metadata, precise failure ABI, compiler-owned tooling and
shared JIT/AOT lowering. `abs` and `clamp` are implemented compiler APIs; there
is no public math module, transcendental source intrinsic family or general
function overload mechanism. Random's approved arity adapter is canonical-random
only and must stay that way.

Recommend 29 ordinary generic functions and six typed constants, as enumerated in
DESIGN.md. Fifteen public algorithms need no low-level operation beyond the
existing numeric APIs and shared floor; fourteen correspond to a narrow floating
facility, with source special-value guards around it.
One private scalar type gate completes the float-only generic boundary. There is
no reason for native Math state, a Runtime provider or a general native interface.

Three design bundles need approval: API/type choices, new numerical/error/rounding
policies and interpolation behavior. Existing overflow/cast/abs/clamp semantics
are already decided and are not questions to reopen.

## 2. Actual source evidence

Line numbers below refer to this baseline. Links open repository files; recheck
line numbers if engineering rebases. Historical documents are not substitutes
for these source findings.

| Finding | Source evidence |
|---|---|
| Exact LLVM 19.1.7 and C++20 are required | [CMakeLists.txt](../../CMakeLists.txt), 1–13; installed SDK `llvm-config.h`, version macros 145–151 |
| Eight fixed-width integers, f32/f64 and existing type predicates | [numeric.hpp](../../include/void/numeric.hpp), 6–30; [numeric.cpp](../../src/numeric.cpp), 19–43 |
| Wrapping integer arithmetic; explicit casts check mathematical range | [numeric.cpp](../../src/numeric.cpp), 80–146; [LLVM backend](../../src/backend/llvm/backend.cpp), 1450 onward; [NUMERIC_TYPES](../NUMERIC_TYPES.md), 64–103 |
| Literal conversion uses target precision and nearest/ties-even APFloat | [numeric.cpp](../../src/numeric.cpp), 45–78; existing source does not accept exponent/hex/separator literals |
| Existing numeric prelude has exactly min_value, max_value, abs, clamp | [numeric.cpp](../../src/numeric.cpp), 148–234; [approved decisions](../DECISIONS.md), 960–979 |
| Abs/clamp have one evaluator and typed IR body implementation | [numeric.cpp](../../src/numeric.cpp), 173–234; [call/numeric contract](../STAGE8_CALLS_NUMERIC.md), 82–119 |
| A generic `t.abs`/`t.clamp` resolves through existing primitive substitution | [compiler.cpp](../../src/frontend/compiler.cpp), 1664–1690; verified by source probes below |
| Functions have one name entry; duplicate signatures do not form overloads | [compiler.cpp](../../src/frontend/compiler.cpp), 5114–5124; [random.inc](../../src/frontend/random.inc), 1–28 limits its exception |
| Shared ordinary binder and concrete specialization already exist | [generic_inference.inc](../../src/frontend/generic_inference.inc); [GENERICS](../GENERICS.md); [STAGE8_CALLS_NUMERIC](../STAGE8_CALLS_NUMERIC.md), 39–79 |
| Source calls in module constants are forbidden; numeric builtin constants are special | [compiler.cpp](../../src/frontend/compiler.cpp), numeric constant evaluation and call paths; actual VOID2011 probe below |
| Floating lowering emits ordinary operations, without fast-math flags | [LLVM backend](../../src/backend/llvm/backend.cpp), 1435–1515; [numeric contract](../NUMERIC_TYPES.md), 79–85, 131–135 |
| Source O2 uses the existing verified default module pipeline | [LLVM backend](../../src/backend/llvm/backend.cpp), 73–90; `native_machine` uses generic CPU for AOT, 92–100 |
| Canonical-only private calls and strict IR validation are established seams | [compiler.cpp](../../src/frontend/compiler.cpp), 3054–3060, 4290–4306; [IR](../../include/void/ir.hpp), 127 onward; [validator](../../src/ir/module.cpp), 1205–1218 |
| Worker safety is transitive; general native calls/globals remain forbidden | [capabilities.inc](../../src/frontend/capabilities.inc), 193–210; [IR validator](../../src/ir/module.cpp), 1371–1394 |
| JIT has explicit symbol binding and platform/process import ordering | [backend.hpp](../../include/void/backend.hpp), 16–20; [LLVM backend](../../src/backend/llvm/backend.cpp), 1718–1727, 1750 onward, 1798 onward |
| User executables use Clang g++ driver mode with existing manifests/staging | [native_build.cpp](../../src/native_build.cpp), 175–192; [NativeBuild.cmake](../../cmake/NativeBuild.cmake), 1–49 |
| Runtime/C++ targets already use matching CRT/threads; stdlib installation is recursive | [CMakeLists.txt](../../CMakeLists.txt), 4–9, 89–94, installation loop; no explicit per-module list needed |
| Source error base and simple documentation are already standard | [error.void](../../stdlib/error.void); [DOCUMENTATION_HOVER](../DOCUMENTATION_HOVER.md); [LANGUAGE_BIBLE](../LANGUAGE_BIBLE.md), section 46 |
| Import discovery and semantic assistance derive from actual sources | [compiler.cpp](../../src/frontend/compiler.cpp), 6276 onward; [tooling.cpp](../../src/frontend/tooling.cpp) |
| Crash preservation already handles timeouts as well as signals/sanitizer exits | [crash.py](../../tests/text_encoding/crash.py), 21–72, 169–210; [random harness](../../tests/random/harness.py), 16–28; [preservation regression](../../tests/random/preservation.py) |

Read alongside these findings: [Bible numerical/call/ownership contracts](../LANGUAGE_BIBLE.md),
[NUMERIC_TYPES](../NUMERIC_TYPES.md), and the current
[stdlib handoff, section 7](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md#7-math-one-numeric-module).
The handoff's one-module direction is preserved. Vector length is not magically
added to arbitrary structs. Older pending feature descriptions remain history.

## 3. What belongs in source

**Reuse unchanged:** math.abs and math.clamp delegate to the existing prelude,
including source-located checked errors, signed-minimum handling, NaN-bound
validation order, permitted infinite bounds and in-range zero signs.

**Ordinary source:** min/max/sign, float classification, ceil/trunc/round derived
from floor, interpolation, angle conversion, constants and special-value policy.
This is practical with existing comparisons, typed arithmetic and specialization.
Min/max zero signs require care: comparing alone makes a tie depend on argument
order. The zero-only arithmetic in DESIGN.md gives exact signs without new bit
operators. Sign avoids abs and therefore works at signed minima.

For float-only source functions, the language has no public numeric/floating
generic constraint. A private identity/type-check kind in the closed Math
operation is sufficient; it emits no numerical work. It prevents an accidental
integer interpolation specialization from silently using integer division/wrap.
Do not insert a discarded floor calculation merely to enforce types, add a
public trait system or reuse random's private domain operation from another module.

Keep all wrappers ordinary source, so declaration identity, docs and navigation
are real. The limitation on module-const calls is intentional reuse of current
language behavior; do not mistake `const local = math.sin(...)` inside a function
for a compile-time module constant.

## 4. LLVM 19.1.7 facilities and the key portability finding

The [pinned Intrinsics.td, lines 923–963](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/IR/Intrinsics.td#L923)
contains matching-precision scalar math operations, including inverse trig, tan,
roundeven and log2/exp2. It has no atan2 entry. This was verified against the
installed 19.1.7 headers, rather than assuming newer LLVM behavior.

The [LLVM 19 reference](https://releases.llvm.org/19.1.0/docs/LangRef.html#standard-c-library-intrinsics)
describes math intrinsics and approximation flags. An intrinsic may become an
instruction **or a library call**. Presence in the header is not proof that a
target's C library supplies every lowered symbol. Keep approximation flags absent;
do not select powi, whose multiplication/rounding order is unspecified.

Bounded installed-SDK probes confirmed:

| Target, generic CPU | Codegen O0 and O2 observation |
|---|---|
| Linux x86_64 | Transcendentals/floor emit ordinary libm references; roundeven emits roundeven/roundevenf |
| Windows x86_64 MSVC | Same roundeven references, other expected CRT names and _fltused |
| macOS ARM64 | Rounding/sqrt lower to instructions in the probe; transcendental symbols remain libSystem references |

**Recommendation:** do not make the module depend on external roundeven symbols.
Their availability across the selected CRT/deployment versions is not established.
One floor primitive plus source rounding supplies all four rounding functions
correctly and avoids that dependency without a fallback subsystem.

For the regular finite atan2 case, a direct fixed atan2f/atan2 C call is adequate.
It shares the Math scalar IR operation and existing JIT binding; a new Runtime
call packet/provider is unnecessary. Source normalizes special cases first.
The explicit standard helper bindings and existing link order should be checked
with actual VOID JIT/cache/reload/AOT after implementation. Standalone lli success
does not prove VOID's isolated generation path by itself.

The existing [native binding catalog](../../src/ir/native_binding.cpp), lines
7–71, does support floating transport, but admits the compiled fixture/sink
providers and their bridge contracts. General native calls are worker-restricted.
Turning Math into another provider would add catalog/bridge/effect work despite
LLVM already supplying the operations. An ordinary IR call also cannot name an
arbitrary undeclared LLVM intrinsic. The narrow typed scalar operation is the
necessary connection to those facilities, without extending the trusted native
package system or creating a public escape hatch.

## 5. Numerical policy and mature precedents

[.NET's Round documentation](https://learn.microsoft.com/en-us/dotnet/api/system.math.round)
uses ties-to-even by default, while C round has a different midpoint rule.
Existing arithmetic rounding does not, by itself, settle a new public round API.
Recommend even ties, same-type return and no digits parameter. Source round uses
floor/fraction/parity with sign restoration, rather than integer casts or a
potentially misrounded addition of one half.

[Python's math API](https://docs.python.org/3/library/math.html) provides useful
module/radian/classification naming precedents but normally uses doubles and
raises exceptions for various domain/range errors. VOID should preserve its own
types and existing IEEE arithmetic. Recommend NaN/infinity float results instead
of copying Python's error policy or introducing a parallel managed error family.

The [GNU math manual](https://sourceware.org/glibc/manual/latest/html_node/Exponents-and-Logarithms.html)
and [trig manual](https://sourceware.org/glibc/manual/latest/html_node/Trig-Functions.html)
document native float/double facilities. Good implementations need real argument
reduction and precision handling; a quick source polynomial, sin/cos division
for tan or exp(log(base)*exponent) for pow is not a suitable substitute.

Two concrete CRT concerns justify source normalization:

- [Microsoft atan2 documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/atan-atanf-atanl-atan2-atan2f-atan2l?view=msvc-170)
  summarizes both-zero results as zero, while
  [Apple's documented signed-zero table](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/atan2.3.html)
  distinguishes the signed origin and negative axis. Do not infer uniform current
  native behavior from those descriptions. Define and normalize the table in
  source, then verify each actual platform. Apple's archive is a specification
  precedent, not fresh macOS execution evidence.
- [Microsoft pow documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/pow-powf-powl?view=msvc-170)
  warns about recognition of large integral floating exponents above 2^64.
  Source integrality/parity and positive-magnitude native calls avoid handing
  negative-base sign decisions to that limitation. No integer conversion is needed.

[Microsoft floor/floorf](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/floor-floorf-floorl?view=msvc-170)
is an established matching-precision operation. Extra CMake math-library discovery
is unwarranted: the existing g++ driver supplies libm in the audited Linux dry run;
Windows already links its CRT and macOS its system library. Native acceptance is
still required, particularly generic-CPU AOT and Windows Debug/Release.

### Interpolation choice

Naive `start+(end-start)*weight` overflows the difference for finite opposite
extrema. Naive inverse interpolation can produce inf/inf or infinity even when
the ratio is finite. The sign-aware lerp and conditional half-scaled inverse in
DESIGN.md are source algorithms, not new intrinsics. They retain t precision and
give a useful bounded-range guarantee without claiming extended-precision
extrapolation. Finite-only inputs, no hidden fraction clamp and NaN on degenerate
inverse segments are explicit proposed choices for owner review.

## 6. Research experiments and limitations

No production code, configuration, test files, root README, Git history or hosted
CI was modified. The following bounded scratch probes were run; none implements
or accepts a VOID Math module. No debugger or ptrace was used.

1. **Eight existing-compiler frontend checks: 8/8 expected outcomes.** Generic
   primitive wrappers/zero arithmetic/sign bodies compiled for all ten types;
   typed float expression probes compiled; two positive cases also checked with
   a real 1 MiB stack. Four negative cases diagnosed bool specialization, mixed
   widths, ordinary function calls in module constants and duplicate function
   names. Some unsupported generic primitive calls use the existing unknown-
   qualifier diagnostic; this research does not promise a new constraint syntax.
2. **Six LLVM object-code probes succeeded:** three target triples at codegen O0
   and O2, with undefined symbols inspected using llvm-nm. This is cross-codegen,
   not native Windows/macOS linking/execution or VOID's O2 pipeline.
3. **Four standalone Linux executions passed 28 numerical smoke assertions each:**
   lli ORC at codegen O0/O2 and separately linked Clang++ executables at O0/O2.
   They establish basic local symbol/ABI feasibility, not a Math accuracy budget,
   strict-sanitizer result or real VOID integration.
4. **Independent arithmetic models passed:** 8,022 rounding inputs compared with
   an independent ties-even result; eight zero-pair min/max sign checks; 5,000
   finite in-range lerp cases; 4,998 finite distinct-endpoint inverse cases without
   a spurious NaN. These bounded cases support the reasoning, not exhaustive
   floating proofs or a transcendental quality claim.

Model examples: lerp(-max,max,0.5) = 0 and inverse_lerp(-max,max,0) = 0.5 at both
precisions. With start=max, end=the previous finite value and value=-max, inverse
interpolation remains finite: f32 33554430 and f64 approximately
1.8014398509481982e16, unlike naive subtraction. Round(-0.5) preserved negative
zero. The six constant bit patterns match DESIGN.md.

The first handwritten LLVM execution fixture was rejected for inexact decimal
f32 **LLVM IR** comparison literals. Its construction was corrected to exact
binary-fraction bounds; original input/error remain in scratch. This is a probe
setup correction, not a VOID or LLVM repair. Two standalone Clang++ links emitted
the expected target-triple override warning from that target-unspecified scratch
IR. No SIGSEGV, sanitizer incident or abnormal execution was observed.

Scratch observations are retained at:

- `/tmp/void-math-source-audit-usbnxmo4/` — source/hash/result receipts; compiler
  SHA-256 `dbc5d531016f5e50707d11edc45ed9a5d6f69ca67cafd3e56922abdad216429b`.
  The compiler was not rebuilt in this research; source inspection anchors the
  baseline findings.
- `/tmp/void-math-llvm19-audit-4s24v0dk/` — LLVM input, six object inspections,
  initial setup failure, corrected runs, driver dry run and arithmetic model
  summary. These scratch records are not a permanent implementation acceptance
  bundle; the observed results and limits are recorded here.

Actual Math frontend hooks, IR validation, VOID normal/O2 JIT, cache/reload,
independently linked VOID AOT, Debug/strict profiles and native Windows/macOS
remain future engineering verification. No local CTest suite was run.

## 7. Genuine decisions for Wrench and Boss

1. **API/type bundle.** Recommend the 29 functions and six explicit typed
   constants in DESIGN.md, ordinary single-name generics, same-type sign/rounding,
   float-only advanced functions and classification. Alternatives include an i32
   sign result or differently named constants; neither is dictated by current
   language rules. Do not approve or introduce general overloads.
2. **New numerical/error policy.** Recommend NaN/infinity for new float
   domain/range failures, NaN-propagating min/max with explicit zero signs, and
   ties-even round. No Math error struct/try family. Abs/clamp keep their already
   approved checked failures. If Boss wants checked floating domains or away ties,
   revise the whole affected contract before implementation.
3. **Interpolation policy.** Recommend unclamped fractions, finite inputs, NaN on
   nonfinite inputs/degenerate inverse segments, exact endpoint shortcuts and
   the bounded-range/source-scaling rules. Alternatives are clamped helpers or
   a degenerate-segment return of zero; do not choose those silently.

No separate question is needed for LLVM pinning, integer wrapping, checked casts,
existing abs/clamp, ordinary call binding, worker ownership, simple source docs,
source/VFS identity, root README protection or Boss's Git/CI control.

## 8. Material risks

- Correct intrinsic IDs do not establish CRT symbol availability, default CPU
  support, JIT isolated-link resolution or installed AOT correctness.
- NaN/zero tests can be broken by minnum, fast-math, premature integer conversion,
  negative-base native pow or unnormalized atan2; test actual generated paths.
- A public generic must reject unsupported types even if one branch returns
  before a native operation. Keep the private float gate represented in typed IR.
- Transcendental accuracy is library/target dependent. Avoid a hard unmeasured
  universal ULP promise or cross-platform replay guarantee; pin independent test
  vectors and investigate any tolerance miss instead of widening thresholds.
- Native C math may affect errno/FP flags, which VOID does not expose/preserve.
  Do not falsely annotate a direct atan2 declaration with stronger memory effects.
- Generic body expansion and frontend stack frames must retain the current
  depth/1 MiB safeguards. Use outlined helpers and compact fixtures.
- Historical filesystem/optimizer crashes remain unexplained. Passing this
  research or a later retry cannot establish a repair of either incident.

**Stop for Golden Wrench architecture review and Boss contract approval.**
