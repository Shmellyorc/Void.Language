# Automatic SIMD feasibility — engineering handoff

**6 November 2026 status: Historical research record.**

This feasibility pass preceded both implementation trials. The global target-aware
candidate was later rejected/reverted; Boss accepted the
[AOT-only candidate](AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md) after
[seven green jobs](AUTOMATIC_SIMD_AOT_STAGING.md). JIT target analysis, SLP default
enablement and checked-array SIMD
are not part of the retained change.

6 November 2026. Research only; stop for Golden Wrench architecture review and
Boss implementation approval. Authoritative scope: [automatic SIMD roadmap](../VOIDC_AUTOMATIC_SIMD_ROADMAP.md).
Boss supplied integrated acceptance baseline `83036fac`, hosted run `42176231887`.
This investigation used the current local sources and existing LLVM 23 Release
libraries; it did not independently rerun or verify hosted acceptance.

**Verdict: LIMITED GO for target-aware O2 analysis using the existing JIT/AOT
target policy. DEFER enabling SLP by default and changing checked-array lowering.**
One ordinary, eligible VOID integer loop improved from **1.052 to 0.321 ms**
locally with actual packed instructions. This is a demonstrated opportunity,
not an application-wide speedup or automatic SIMD support for every numeric loop.
No Runtime, public language, array representation or CPU deployment policy change
is justified by these experiments.

## 1. Evidence and experiment boundaries

The [committed research receipts and archive inventory](evidence/automatic-simd/README.md#research)
contain selected unchanged text evidence and identify the complete original local
archive at `/home/shmellyorc/.local/state/void-log-performance/simd-gbc5btsk`.
Binaries, objects and toolchain copies remain local; no measurement was rerun for
publication.
Start with `identity.json`, `linked-input-identity.json`, `build.py`, `build.json`, `manifest.json`,
`matched-summary.json`, `baseline-equivalence.json` and `production-recheck.json`.
The manifest records SHA-256 and size for the retained artifacts. This local
directory must accompany the handoff if another machine needs to reproduce it.

Environment: Linux x86-64, Ryzen 7 1700X, LLVM host CPU `znver1`;
LLVM/Clang/LLD 23.1.4, SDK revision
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. Source execution and native controls
used a real **1 MiB soft and hard stack limit**. No production settings, GC
settings, source files, Git state or CI were changed. The six recorded production
source/header hashes remained identical. This report is the sole repository edit.

The isolated host links the existing frontend and Runtime libraries with a
disposable copy of `src/backend/llvm/backend.cpp`. Only target/pipeline selection
and IR/object capture were added. The host compiler itself used O1; generated VOID
still uses the production O2 pipeline and production machine-code optimization.
One baseline AOT object, for `guarded-range-i32.void`, is **byte-for-byte identical**
to the installed production `voidc emit` output. That establishes fidelity for
that control, not every conceivable program.

Four configurations were screened:

| Label | O2 PassBuilder analysis target | SLP |
|---|---|---|
| Baseline | None, as production | Default off |
| SLP | None | On |
| Target | Host JIT / generic native AOT | Default off |
| Target+SLP | Host JIT / generic native AOT | On |

No fast-math, contraction permission, speculative no-alias metadata, unchecked
array access, changed polling or forced vector width was introduced. The isolated
target variant recreates the local target; production engineering must reuse the
actual configured target instead of copying that experimental convenience.

Primary performance comparisons disable capture. Each fixture/input runs in
balanced order Baseline, SLP, Target, Target+SLP, Target+SLP, Target, SLP, Baseline;
each process compiles once, checks an initial result, warms twice and times five
invocations. Reported medians are ten samples per configuration. Compilation,
linking and IR dumps are outside execution timing. Array construction, filling,
checked arithmetic and output verification are inside it. CPU measurements use
`std::clock`; tiny samples approach timer resolution. There was no CPU pinning or
frequency isolation, so small differences are not evidence of improvements.

Original observations were preserved, including fixture-authoring diagnostics.
The initial fixed-f64 screen inadvertently mixed extents 8193 and 1025 while
reducing compile cost. **That comparison is excluded.** All four were repeated
with extent 1025 under `fixed-f64-matched.*`; see `sizing-note.txt` and
`final-controls.json`. Initial unsupported `while` syntax and exponent notation
were corrected using existing `for` syntax and repeated halving for a subnormal;
the invalid inputs/diagnostics remain. No abnormal compiler/runner exit occurred.

## 2. What production currently does

| Source location | Confirmed behavior and consequence |
|---|---|
| `src/backend/llvm/backend.cpp:81` — `finish` | Verify, standard O2 through `PassBuilder` with no TargetMachine, verify. This registers target-independent analysis despite target-specific later code generation. |
| Same file:101 — `native_machine` | Native AOT uses deployment triple, CPU `generic`, empty extra features and PIC. It does not inherit this workstation's AVX2 capability. |
| Same file:1896 — `Jit::Jit` | ORC detects host CPU/features and uses default codegen optimization. Target identity already includes triple, CPU/features, codegen policy and data layout. |
| Same file:2072 | Cache identity includes compiler build identity, ABI fingerprint, target identity and optimized/unoptimized distinction. CMake also fingerprints built compiler/Runtime inputs. |
| Same file:821 | Coordinated source blocks emit GC polls and synchronous worker checkpoints with failure handling. Pure tested arithmetic functions do not require those coordinated paths. |
| Same file:945–977, 1061 | Fixed/dynamic numeric operations lower through checked array/list ABI helpers, not simple LLVM loads/stores over a raw numeric span. |

The exact SDK's
[PassBuilderPipelines.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Passes/PassBuilderPipelines.cpp#L341)
defaults loop vectorization on and SLP off for direct builder use. Its
[PassRegistry.def](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Passes/PassRegistry.def#L397)
selects TargetIRAnalysis from the machine only when one is provided.
[PassBuilder.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Passes/PassBuilder.cpp#L581)
also registers target callbacks. Supplying a machine can therefore affect more
than vector width. These exact-revision files are saved with the receipts.
The installed SDK's `llvm/Analysis/TargetTransformInfoImpl.h:646` supplies a
32-bit fallback register width, rather than the host's SIMD register width.
Thus enabling the loop pass is not equivalent to supplying usable target costs.

**Observed current source loops:** no packed numeric arithmetic in the sampled
baseline fixed/dynamic array loops, integer reductions or four independent f32
recurrences. Scalar FP instructions still use SIMD registers; that is not packed
execution. Root initialization and memory moves also do not count as numeric
loop vectorization. This is a bounded characterization, not proof that every
production program and C++ Runtime routine lacks SIMD.

## 3. Actual blockers and opportunities

**Target analysis is a demonstrated missing opportunity.**
`guarded-range-i32.void` uses existing syntax:

```text
func entry(n: i32) -> i32:
    if n <= 0:
        return 0
    var total: i32 = 0
    for i in 0..<n:
        total += i % 127
    return total
```

Baseline remains scalar. Target-only generates vector arithmetic and a scalar
remainder. At n=1,000,000 all variants return 62,999,875. The conditional-loop
equivalent (`for i < n:` with explicit increment) also vectorizes with Target.
No new language syntax or altered numeric policy is needed.

**Dynamic range direction independently obstructs the unguarded example.**
`src/frontend/compiler.cpp:4953` snapshots endpoints and direction; :4992 emits
increment/decrement paths. The optimized unguarded `0..<n` loop retains a
selected +1/-1 stride, preserving descending iteration for negative n. Its
induction/trip count is not recognized for vectorization in the tested pipeline;
Target alone does not fix it. This is not permission to assume n positive or
change descending-range behavior. Loop specialization would be separate frontend
work, unsupported by this mission's measured candidate.

**Checked numeric arrays are not currently vectorizer-friendly memory loops.**
`src/runtime/fixed_abi.cpp:35,99,104,141,152` routes reads/writes through registered
roots, layouts, checked indexing and selected mutation versions.
`src/runtime/buffer.cpp:229,234,479,497` validates root authority, frame identity,
live storage and layout, and accesses memory under Runtime coordination.
Even fixed arrays use managed storage with these obligations.

All five types (`u8/i32/i64/f32/f64`), fixed and dynamic, remained scalar in all
four configurations. Optimized array fixtures retain GC polls and checked calls.
Diagnostic-only `opt -passes=loop-vectorize` runs on captured IR report
unvectorizable calls, early-exit and induction/trip-count obstacles. These are
secondary diagnostics, not the production pipeline's original remarks; actual
captured IR and the four-way experiment establish the result.

Removing calls/polls, pinning payloads across safepoints, adding `noalias`, or
prechecking an entire range could change first-error order, prior successful
writes, cancellation, aliasing and ownership. Existing leases do not by themselves
prove equivalence to arbitrary checked source loops. **Do not implement a raw-span
or bulk-operation replacement under this assignment.**

**SLP is technically feasible but not shown profitable here.** It packs four
independent f32 recurrence streams while retaining the order within each stream.
It does not vectorize the genuinely dependent f64 recurrence. Despite fewer
arithmetic instructions, execution did not improve: recurrence latency remains
and the processor can overlap independent scalar instructions. This explanation
is an inference from code and timings, not a measured hardware-counter diagnosis.

LLVM describes loop and SLP vectorization, runtime alias checks and profitability
separately in its [vectorizer documentation](https://llvm.org/docs/Vectorizers.html).
Do not replace target costs with unconditional vectorization. Its
[new-pass-manager documentation](https://llvm.org/docs/NewPassManager.html)
supports supplying the target to pipeline construction; exact defaults above
come from the pinned source, not newer documentation.

## 4. Emitted code and matched execution

| Fixture/configuration | Verified output |
|---|---|
| Guarded integer loop, baseline | Scalar arithmetic; no vector numeric IR |
| Guarded integer loop, host Target JIT | Packed IR, AVX2 `vpmuludq` / `vpaddd` on YMM registers; scalar remainder |
| Same, generic Target AOT | SSE2 `pmuludq` / `paddd` on XMM registers; no host AVX2 requirement |
| Four f32 streams, baseline | Separate `vmulss` / `vaddss` |
| Four f32 streams, SLP | `<4 x float>` and `vmulps` / `vaddps` |
| Four f32 streams, Target+SLP | Two-lane packs selected by host costs; packed arithmetic |
| Dependent f64 / checked array arithmetic | Remains scalar |

No FMA was emitted in the inspected f32 variants. IR inventory found no newly
enabled fast/reassociation/contraction flags. Captures are `out/*.post.ll`,
`out/*.jit.o`, `out/*.jit.asm` and `aot/*.asm`.

**Matched JIT wall time, milliseconds per complete invocation:**

| Workload | Baseline | SLP | Target | Target+SLP |
|---|---:|---:|---:|---:|
| Guarded i32, 1,000,000 iterations | 1.051695 | 1.059235 | 0.320823 | 0.322144 |
| Unguarded bidirectional i32, 1,000,000 | 1.299255 | 1.320475 | 1.315050 | 1.307090 |
| Independent f32 streams, 1,000,000 | 1.725790 | 1.750495 | 1.740855 | 1.747200 |
| Dependent f64, 1,000,000 | 2.025580 | 2.018850 | 2.040750 | 2.025475 |
| Dynamic i32, 8,192 elements | 27.784350 | 27.098150 | 27.950500 | 27.166200 |
| Dynamic f32, 8,192 elements | 26.923450 | 27.646300 | 26.615200 | 27.334900 |

Corresponding Baseline→Target process CPU medians: guarded i32 **1.0475→0.3220**,
unguarded i32 **1.2910→1.3140**, f32 streams **1.7220→1.7360**, dependent f64
**2.0170→2.0345**, dynamic i32 **27.6635→27.8370**, dynamic f32
**26.7995→26.5035 ms**. Raw observations and ranges are retained. Do not attribute
small array differences to SIMD: there is no emitted numeric vector loop there.
SLP's modest unfavorable results are retained rather than advertised as gains.

Small n=17 cases were also recorded. Pure loop wall samples are below a
microsecond and CPU resolution is insufficient for a useful claim. Dynamic
17-element medians ranged roughly 0.074–0.096 ms across configurations.

### Separate native memory-loop control — not VOID speedups

To establish legality/profitability of conventional contiguous loops on the same
toolchain, C++ controls compute `out[i] = a[i]*3 + b[i]`, scalar-vectorization-off
versus normal O2, both with `-ffp-contract=off`. There is no `restrict` assumption.
Integer unsigned-bit arithmetic models modulo-N arithmetic. This omits VOID's
checked Runtime contract and **cannot estimate a safe array-lowering speedup**.

All five types passed bitwise output comparisons (NaN classification instead of
payload), signed-zero/subnormal/infinity controls, integer wrapping, tails,
17/65,536 elements, offset-zero/offset-one inputs and overlapping dependent
destinations. Actual address modulo 32 is recorded. Scalar/vector/vector/scalar
blocks used 200,000 repetitions for small arrays and 200 for large arrays.

| 65,536 elements, offset one | Scalar ms/call | Vector ms/call |
|---|---:|---:|
| u8 | 0.040842 | 0.003690 |
| i32 bits | 0.041892 | 0.011675 |
| i64 bits | 0.042317 | 0.023553 |
| f32 | 0.031569 | 0.011216 |
| f64 | 0.038866 | 0.022483 |

These are aggregate block times divided by repetitions, not separately timed
individual calls. Full wall/CPU observations are in `native-controls.csv`.
Alias-dependent controls matched scalar behavior; they do not authorize marking
VOID array arguments nonaliasing.

## 5. Correctness and portability limits

Local verification includes:

- Original 52-case four-way O2 screen; corrected matched fixed-f64 repeat retained
  separately. Five numeric types, fixed/dynamic arrays, element-offset-one loops,
  independent and dependent scalar arithmetic were exercised.
- 80 matched JIT processes / 400 timed invocations with stable outputs. Integer
  reductions match the independent arithmetic sum; arrays verify every computed
  output. The f32 recurrence checks power-of-two relationships; the dependent
  f64 control checks positivity, so neither is a complete bit-pattern oracle.
- Nine production `--no-opt --no-cache` source executions pass, including five
  dynamic types, fixed i32, edges, shared aliasing and integer reduction.
- 22 independently linked AOT executions: Baseline and Target across 11 fixtures.
  Twenty return expected successful results; two deliberately fail with the
  expected fixed-array bounds diagnostic **before** division-by-zero. All four
  JIT configurations preserve that ordering too.
- VOID controls cover u8/i32/i64 wrapping boundaries, negative zero, generated
  f32 subnormal behavior and NaN classification. Shared-list alias recurrence
  transforms `[1,99,...]` into `[1,2,...,8]` in every configuration.
- Native controls execute all 80 timing blocks without an oracle failure.

Integer wrap and individual floating rounding are established contracts in
`docs/NUMERIC_TYPES.md:69–84`; checked conversions remain checked. Floating
reductions cannot be reassociated merely to get packed arithmetic. Do not add
`nsw`, `nuw`, fast-math or FMA permission to source arithmetic.

ARM64 evidence is **cross-compilation only**: the unchanged pure VOID loop's
captured pre-optimization IR was processed by pinned `opt`/`llc` for generic
`aarch64-unknown-linux-gnu`, producing NEON `add v*.4s`, `umull2` and vector
reduction code. Native C++ memory controls also produce ARM64 assembly.
`codegen-commands.json` records exact commands. This is not native ARM execution,
macOS ABI validation, or a new supported VOID cross-compilation interface.

The native x86-64 AOT objects retain generic-target policy. JIT exploits only
detected host features. Cache identities must continue separating both policies;
do not put host features into portable AOT. Reload generation leases, function
ownership and managed lifetimes remain unchanged in this experiment.

Not tested here: Debug/strict-sanitizer compiler builds, native Windows/macOS,
ARM execution, all cache/reload combinations, exhaustive GC/cancellation injection
or application performance. The local results justify bounded engineering, not
cross-platform acceptance. No ptrace, broad suite or hosted run was needed.

## 6. One bounded Keyboard assignment, after approval

**Implement target-aware standard O2, retain current SLP policy, then qualify it
as one backend repair.** Complexity is modest; regression surface exceeds the
line count because target analysis/callbacks affect the whole O2 pipeline.

1. Change internal `finish` in `src/backend/llvm/backend.cpp` to receive the
   actual configured TargetMachine. Retain verification before/after standard O2.
   AOT must pass the existing `native_machine()` object used to emit its object.
   For JIT, derive/retain the analysis machine from the same configured
   JITTargetMachineBuilder before transferring it to ORC: same deployment triple,
   CPU/features, options, relocation/code model and data layout. Keep ownership
   private to the backend; do not expose a new public target API or detect a second
   potentially different host policy on each compilation.
2. Reuse existing cache/build identity. Advance the existing optimizer/ABI
   fingerprint revision as appropriate so embedders cannot accept old optimized
   objects under the changed pipeline. Test cold/hit/miss behavior and reload
   leases through existing fixtures; do not invent another cache system.
3. Add a few source/codegen fixtures to existing numeric/backend infrastructure:
   the eligible guarded/conditional integer loop, descending and empty ranges,
   independent and dependent FP, checked arrays with aliasing and first-failure
   order. Check vector IR and appropriate packed operations on supported targets,
   plus correct scalar tails; do not demand a particular width/instruction spelling
   across architectures or force vectorization on unsupported hardware.
4. Run focused existing numeric, fixed/growable array, ownership/GC, cancellation,
   cache/reload and source-AOT checks in normal/O2 JIT and independently linked
   AOT; include Debug/Release/strict sanitizers and inherited 1 MiB stacks. Preserve
   exact `_crash.void` and incident evidence on any abnormal exit. Repeat the
   matched complete invocations above, retaining regressions, compile time and
   code size as well as execution time. Native x86-64/ARM64 Windows/macOS coverage
   remains owner-controlled qualification, not an automatic hosted dispatch.

Acceptance: unchanged defined outputs/errors and ownership behavior; actual
target-legal packed arithmetic plus scalar fallback in the eligible control;
portable generic AOT and host-correct JIT; correct cache invalidation; retained
local benefit without a material representative regression. There is **no new
percentage target, performance release gate or claim that arrays now vectorize**.
Stop/narrow the change on a reproducible semantic, feature-selection, cache or
material performance regression. Do not compensate by weakening checks.

SLP default enablement is deferred because packed output alone did not improve
this workload. Checked-array SIMD and range-direction specialization are also
deferred: they require substantially different proofs and have no safe measured
candidate here. No Runtime rewrite, new SIMD types, new source declaration form,
manual ISA dispatch framework or unrelated optimization is part of the handoff.

**Owner decision:** approve or decline this limited target-aware O2 assignment.
The existing language and roadmap already settle the semantic constraints; no
new language decision is needed. Stop for Golden Wrench/Boss review.
