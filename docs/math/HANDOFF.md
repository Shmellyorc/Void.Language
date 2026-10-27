# Math implementation handoff

27 October 2026. Authorized 26 October. Baseline `9eaefbd339e2456295911cdecb2460d4143f34b0`.
Boss approved the full contract and Golden Wrench's primitive-specialization
correction. Implementation and local verification are complete; independent source review is next.
This is not a hosted acceptance or release lock.

## Delivered API

One `use math` module, exactly 29 ordinary generic functions and six constants:

- Core: `abs`, `min`, `max`, `clamp`, `sign`.
- Classification: `is_nan`, `is_infinite`, `is_finite`.
- Rounding: `floor`, `ceil`, `trunc`, `round`.
- Interpolation: `lerp`, `inverse_lerp`.
- Trigonometry: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`.
- Roots/exponentials: `sqrt`, `pow`, `exp`, `exp2`, `log`, `log2`.
- Angles: `radians`, `degrees`.
- Constants: `pi_f32`, `pi_f64`, `tau_f32`, `tau_f64`, `e_f32`, `e_f64`.

[API audit](API.md) and [approved contract](DESIGN.md) describe every declaration.
Core accepts all ten exact numeric primitives; the other functions accept only
f32/f64. Classification returns bool; numeric results retain the operand type.
Canonical public specialization uses existing numeric metadata before body/cache
analysis, rejecting nominal custom operators, bool, enums and collections.

All public declarations and their parameters/results have source documentation.
Special-value policy, ties-even rounding, full atan2/pow handling and interpolation
are ordinary VOID source. Existing checked abs/clamp behavior is retained.

## Compiler changes

`math_ops.hpp` defines 15 closed kinds: identity, 13 matching-precision LLVM
19.1.7 intrinsics and fixed-C-ABI atan2. Private frontend calls are restricted to
exact `/@stdlib/math.void`; ordinary shadowing/binding/evaluation order applies.
Typed IR independently checks ownership, scalar f32/f64, exact operand/result
precision, kind/arity and unused metadata. Const scalar operands may retain the
existing readonly-copy marker; results have clean scalar types.

The backend adds no fast-math, reassociation, widening or implicit FMA. Standard
float/double helpers bind in each JIT process; separately linked AOT uses the
existing native driver/manifest. No Runtime provider, resource ledger, arbitrary
FFI, new numeric checker or native-build security change was added.

Worker effects recognize only exact existing numeric abs/clamp kernel symbols
plus the closed Math family. Native, global, OS and resource restrictions remain.
Incomplete generic signature recovery publishes source docs before inference;
shared source-pattern rendering avoids requiring uninstantiated array layouts.
There is no Math-specific editor declaration catalog.

## Local verification

Final effective coverage is **31/31 per profile, 93/93 combined**, after retained
repairs and affected reruns. There were 146 actual CTest executions in the
consolidated/follow-up receipts, including the original unsuccessful executions;
93 is the number of unique final profile/test outcomes, not a first-pass claim.

| Profile | Original consolidated | Affected follow-ups | Final effective |
|---|---:|---:|---:|
| Debug | 29/30 | Tooling/generics 7/7; final numerical/stack 2/2; actual cache 1/1 | 31/31 |
| Release | 29/30 | Tooling/generics 7/7; final numerical/stack 2/2; actual cache 1/1 | 31/31 |
| Strict RelWithDebInfo | 2/30, LSan process-sandbox failure | Same failed 28/28 outside sandbox; tooling/generics 7/7; actual cache 1/1 | 31/31 |

Original consolidated receipts remain in `evidence/acceptance-*.json`, `.xml`
and `.txt`; follow-ups have distinct names. The strict compiler/Runtime use
`-fsanitize=address,undefined -fno-omit-frame-pointer`, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Flags were retained
unchanged outside the sandbox; leak checking was never disabled. Commands,
configuration hashes and effective test dispositions are in
[evidence/FINAL.json](evidence/FINAL.json).

The final numerical fixture has **1,372 checks in each of four real modes**:
normal/O2 JIT and independently linked O0/O2 AOT. Its oracle bank contains
**224 MPFR 4.2.2 native/root vectors** plus **44 mathematical interpolation
references**, evaluated at 256/512 bits with direct ties-even target rounding.
Constants, rounding, signs/classes, endpoints and sqrt use exact checks. Native
quality gates remain 4 ULP f32 / 8 ULP f64; measured final maximum across all profiles/modes is 1 ULP
for cos f64 near a stored pi/2 neighbor. Interpolation matches the mandated
per-operation source oracle exactly; its mathematical one-round comparison has
up to 5 f64 ULP of cancellation error for stored (-0.1, 0.2, 0.3).
See [numerical methods](evidence/numerical-methods.txt) and execution receipts.

Independent IR checks cover 799 accepted/rejected constructions. Frontend checks
cover 356 processes, including all widths, explicit/inferred generics, negative
nominal custom operators, private boundaries and bounded depth. Actual 1 MiB
runs cover frontend, compiler/JIT/build and native executable paths. Math LSP
covers 65 checks; relocated install executes all four modes. Host tests cover
28 cache/reload/generation/GC assertions; worker execution covers both precisions
and source-only helpers in all four modes.

## Failures and evidence disposition

- Initial worker checking rejected existing numeric kernels reached through Math.
  The narrowly scoped effect classification was corrected and worker checks pass.
- Missing incomplete generic Math signature help was corrected. Initial Debug
  integration then exposed a generic-array signature-rendering regression; shared
  source-pattern rendering repairs it, and affected existing LSP tests pass.
- Initial Release Math LSP failure used stale compiled generic tooling after an
  edit overlapped compilation. [Disassembly evidence](evidence/stale-release-lsp-disassembly.txt)
  proves the missing calls. Frozen-source rebuilds replace that artifact.
- A final host-fixture audit found its cache root did not exist, allowing the
  portable unavailable fallback. The fixture now creates the root, requires a
  real Linux native-object hit and prints both cache states. Only that host test
  was rerun in each profile; original receipts remain.
- Harness import precedence, one fixture type annotation and a missing executable
  during concurrent relinking were setup failures; original outputs remain.
- Two older successful Release numerical receipts reread a newer oracle hash at
  exit. Their originals and the explicit limitation remain in
  [bank-freeze-provenance.json](evidence/bank-freeze-provenance.json). A corrected
  loaded-bytes hash and profile-specific snapshots support the final reruns.
- Initial strict execution hit LeakSanitizer's fatal process-inspection failure
  inside the process sandbox. The identical frozen binaries and strict flags
  passed outside it. All 20 incident archives remain intact: 360 source copies,
  536 provenance copies and six editor snapshots. Eighteen contain raw process
  diagnostics; two existing harnesses inherited streams, so their original
  diagnostics are retained upstream in the original CTest XML/text. The
  [complete incident audit](evidence/sanitizers-incident-audit-complete.json)
  states those capture limits without reconstructing bytes as originals.
- One obsolete strict build was deliberately interrupted with status 130 before
  the frozen rebuild. No generated fixture was executing in that build.

Crash preservation is established before fixtures through the unchanged existing
Random/text-encoding harnesses. Math adds recursive controlled imports, immutable
compiler-identity provenance and original overlay transport capture on actual LSP
crashes. Three disposable synthetic incidents validate exact nested sources,
outputs, hashes, sanitizer-reserved exits and timeout preservation. Existing
filesystem, optimizer and Random incident archives remain unchanged. A caught LSP
RPC error is recorded in the original JUnit output, rather than misclassified as
a native crash; no original raw RPC transport is claimed for that normal-status
server error. No unexplained Math-origin signal, timeout, ASan/UBSan defect or
leak remains. Historical filesystem/optimizer/Random observations remain as
recorded; this sprint does not claim to repair them.

## Scope and review boundary

Native Windows Debug/Release and macOS ARM64 execution remain unverified. Linux
results do not establish cross-platform bit identity or a universal ULP bound.
Host ASan/UBSan/leak checks do not instrument every generated JIT instruction;
the existing no-RTTI LLVM ABI also excludes vptr sanitization.

The root README, the pre-existing `docs/README.md` edit, unrelated libraries and
historical incident archives are unchanged from the starting workspace. No Git
staging/history/branch/remote operation or hosted CI was performed.

Exact changed files, source/toolchain/compiler hashes and every retained evidence
file are listed in [evidence/MANIFEST.json](evidence/MANIFEST.json). The final
receipt is [evidence/FINAL.json](evidence/FINAL.json). Math source SHA-256:
`d960ef9015b49f180bd49c13a00ad8006b4cd235e101ad1ca99c66eca4f3e2e4`.

**STOP for independent Golden Wrench source review. No later sprint is started.**
