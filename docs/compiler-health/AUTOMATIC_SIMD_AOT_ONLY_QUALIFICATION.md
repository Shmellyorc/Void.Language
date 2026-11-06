# Automatic SIMD — AOT-only implementation and qualification

**Accepted baseline — 6 November 2026:** commit `175c08c8a0cab0b701ba71365ffcc398a0c9bcf3`
passed [all seven jobs in run 42231556437](https://github.com/Shmellyorc/Void.Language/actions/runs/42231556437): Linux Debug, Release and Sanitizers;
Windows Debug/Release; macOS ARM64 Debug/Release. The following preparation/local
measurements are preserved as dated evidence. This compiler CI does not qualify
the new source-install wrapper or independently portable packages.

**6 November 2026 status: accepted after seven-job cross-platform qualification.**

Boss approved this AOT-only candidate and its subsequent seven-job qualification.
The measurements below are local Linux qualification. The
[staging review and Boss CI instructions](AUTOMATIC_SIMD_AOT_STAGING.md) record
the exact prepared scope. JIT target analysis remains unchanged; no general
checked-array SIMD is claimed.

6 November 2026. One focused implementation/validation sprint, authorized by Boss.
Inputs: [feasibility](AUTOMATIC_SIMD_FEASIBILITY.md),
[reverted global candidate](AUTOMATIC_SIMD_QUALIFICATION.md),
[roadmap](../VOIDC_AUTOMATIC_SIMD_ROADMAP.md).

**Original local recommendation (subsequently accepted): RETAIN the AOT-only candidate.**
The guarded and conditional integer programs execute **2.69× / 2.72× faster**
under the existing portable generic AOT policy. The guarded complete build plus
first execution improves **254.303→187.310 ms**, including its compilation cost.
The benefit remains specific to eligible integer work; the broader text,
collections, managed and JSON programs do not establish a general speedup.
JIT lowering/optimization, ORC setup and logical persistent-cache identity remain
unchanged. The existing whole-compiler build fingerprint still changes when a
compiler library changes, as explained below; it was not bypassed.

## 1. Exact change and scope

Only `src/backend/llvm/backend.cpp` (private `finish` at line 81; AOT calls at
2349/2368) and this report are new source/document changes
in this sprint. The prior numeric/cache tests and six SIMD fixture files remain
unchanged. The starting production backend is byte-identical to accepted commit
`83036facdf74c5a1eee6f475b6cfe110f4752f27`; its SHA-256 is
`dd67d152fe28f025499f56cced2b0c39187bfdf80a47aa1e87c31e408c53b39d`.
Unrelated local evidence and the protected root README were preserved.

The implementation adds a private, optional `TargetMachine*` parameter to the
existing `finish` helper. Its one standard per-module O2 PassBuilder receives that
pointer; SLP is explicitly false, matching the pinned LLVM 23.1.4 default already
used by JIT. Both AOT callers pass `machine.get()` from their existing native AOT
machine. The same instance supplies lowering data layout and emits the object.
JIT callers retain their original two-argument calls, hence a null analysis target.
No ORC builder, analysis machine, host detection or ownership member was added.

The AOT machine factory is unchanged: native deployment triple, CPU `generic`,
empty additional features, existing TargetOptions, PIC relocation and existing
code-generation level. LLVM verification remains before optimization and after
O2. `--no-opt` still skips the optimization pipeline; no new user flag is required.
The optional private fixed-call research AOT caller follows the same rule. Its
separate storage-proof service is unchanged; optional research certification was
not enabled for this sprint.

There is one optimizer and one target-selection architecture. No frontend,
Runtime, public API, array lowering, GC/root/safepoint, failure-order, cancellation,
range-direction, fast-math or alias-authority change was made. The previous global
candidate was not restored. A new compiler build fingerprint is the existing
build system's consequence of rebuilding the backend archive, not a new JIT
pipeline/cache revision.

## 2. Evidence and reproducible boundaries

All new source snapshots, patch, binaries, exact argv/status/stdout/stderr,
optimized IR, objects, disassembly, measurements, original failures and receipts
were retained in the full local archive; selected text receipts and proofs are in
[this sprint's committed evidence bundle](evidence/automatic-simd/README.md#aot).
`input-identity.json`, `jit-source-proof.json`, `final-identity.json` and the final
`manifest.json` identify inputs and outputs. The existing JSON harness also emitted six per-process repository receipts; copies
are retained under `json-test-receipts`, with originals left in place.

The previous **654** qualification
artifacts and **513** research artifacts were checked for size/SHA-256 before reuse;
all matched their manifests. Neither prior evidence directory was modified.

Host: Linux x86-64, Ryzen 7 1700X, LLVM/Clang/LLD **23.1.4**. Baseline binaries and
backend/cache-identity archives were copied before editing. Candidate products
were built from the current source; frontend and Runtime inputs are shared.
No staging, commits, branches, pushes, hosted CI or release publication occurred
during the implementation/validation sprint. Later authorized staging is recorded
separately in the staging review.

One fixed performance matrix was run after qualification jobs finished, pinned
to logical CPU 2. Baseline/candidate/candidate/baseline order was repeated twice
for the main O2 compile controls and all execution controls. Seven compilation
controls have four O2 and two normal samples per variant; the remaining seven have
one normal/O2 build per variant for coverage. This yields **112 real CLI builds**,
**112 validated first executions**, and **112 separately timed O2 executions**.
Execution medians have four process samples per variant. Every executable has an
exact output oracle, including the existing typed/validated JSON test expectations.

Nine controls also have **36** balanced diagnostic emissions, two per variant,
for LLVM phase timing. A copy of the same backend adds observations only:
`pipeline.run` timing excludes lowering, verification and pipeline setup;
codegen timing includes legacy codegen setup/run and excludes final stream flush.
It uses entry-reachable emission, exactly as production `build` does. Public
`emit` retains all exports; a separate all-exports capture driver verifies that
mode. Capture fidelity holds for **72 numeric objects** and **12 broader objects**
against actual CLI `emit` output. Production build and execution timings use the
unmodified production backend, not the instrumented copy.

All compiler/process controls, independently linked programs and CTest runs use
actual **1,048,576-byte soft AND hard stack limits**. Builds of the compiler itself
retain the ordinary host stack. `live-stack-witness.json` records `/proc` limits
of an actual compiler child. All observations and min/max/CPU times remain in
receipts/`summary.json`; these are local desktop samples, not a dedicated lab or
hardware-counter study. No unfavorable observation was removed or performance
candidate revised during the sprint.

## 3. JIT and cache unchanged proof

- `Jit::Impl`, construction, compile, cache-key construction, reload and root-probe
  source are **byte-identical** to baseline (`jit-source-proof.json`). Every JIT
  `finish` call still selects a null analysis target. ORC configuration and startup
  work are unchanged.
- **36 paired JIT IR comparisons and 36 paired object comparisons** are byte-identical:
  all 18 retained fixtures in normal/O2. The eligible JIT control remains scalar,
  demonstrating that target-aware optimization did not leak into JIT.
- With an identical explicit embedder build identity, the candidate reads the
  baseline's native objects as **hits**, in normal/O2, for both pure arithmetic and
  managed code. Each root retains one identical key/envelope after alternating
  baseline/candidate readers (`cache-proof-summary.json`). No pipeline, target,
  ABI, envelope or cache schema field was changed.
- Normal build fingerprinting remains conservative. `cmake/CacheBuildIdentity.cmake`
  hashes the entire backend archive, so rebuilt CLI compiler identities differ
  (`compiler-build-identities.json`). Thus different compiler builds remain
  separated by that existing build-ID field. This report does **not** claim
  identical default CLI keys across different whole-compiler builds. Preserving
  that invalidation contract avoids weakening cache correctness.
- All three builds passed source/build invalidation, automatic/fresh-process cache,
  corruption fallback and retained-generation/reload tests. The pre-existing O2
  reload check verifies old leases and a replacement cache hit.

Default O2 CLI process wall medians, automatic cache (four samples per cell):

| Input | Cache | Baseline ms | Candidate ms |
|---|---|---|---|
| 17 | cold | 29.842 | 26.113 |
| 17 | warm | 8.511 | 8.529 |
| 1000000 | cold | 27.860 | 26.032 |
| 1000000 | warm | 10.003 | 9.473 |

In-process guarded JIT invocation: **1.043340→1.042785 ms**; cold lowering/O2/codegen
**7.360→7.086 ms**, analyze/JIT/compile **9.252→9.078 ms**. The baseline and candidate
machine code is identical. These timing shifts do not establish a JIT speedup;
they show no recurrence of the global candidate's systematic cold-start penalty.
The original ranges are retained. The explicit CLI `--cache` Debug-mode flag was
not mistaken for default O2.

## 4. AOT instruction, size and semantics evidence

The guarded range and equivalent conditional loop produce vector integer IR,
SSE2 `pmuludq` / `paddd` and scalar remainder paths on this x86-64 generic target.
No width or mnemonic was forced; these are observed instructions. No AVX/AVX2/FMA
was emitted in the inspected numeric objects. Portable AOT does not inherit this
workstation's AVX2 capability. JIT objects remain unchanged/scalar.

All **18 normal AOT object pairs** are identical. **15/18 O2 pairs** are identical:
the two eligible loops change, as does the scalar subnormal control's partial
unroll. Fixed/dynamic u8/i32/i64/f32/f64 controls, shared-alias recurrence,
bidirectional integer and independent/dependent FP controls retain scalar objects.
Inspected checked-array/alias IR retains its polls and checked helper calls.
No raw-buffer or noalias shortcut is present, and sampled FP arithmetic has no
new fast/reassociation/contraction permissions.

Guarded object: **4,312→5,576 bytes** including debug metadata; `.text` including
its CLI adapter **491→1,071 bytes**. Final executable adds **680 bytes**.
The scalar subnormal edge object grows **4,648→4,696 bytes**; ordering and result
remain correct. Code growth is an actual cost, not an instruction-count speedup.

| O2 program | Executable bytes, baseline→candidate | Delta | Byte-identical executable |
|---|---|---|---|
| guarded | 8,928→9,608 | 680 | no |
| conditional | 8,880→9,560 | 680 | no |
| fixed_i32 | 1,899,168→1,899,168 | 0 | yes |
| text | 1,881,928→1,881,928 | 0 | yes |
| managed | 1,938,160→1,938,160 | 0 | yes |
| collections_2m | 1,898,576→1,898,576 | 0 | yes |
| json_typed | 2,945,528→2,945,504 | -24 | no |
| json_validated | 2,583,336→2,583,304 | -32 | no |

All **14 normal executable pairs** are byte-identical. Ten O2 executable pairs
are identical. Text, managed, 2 MiB collections and small collections have identical
entry-emission objects. Typed/validated JSON entry objects shrink **24/32 bytes**;
normalized IR inspection identifies the only changed function as `json::_scalar`.
Target costs retain division/remainder operations rather than decomposing the
remainder into multiplication/subtraction. Checked pushes, branches and roots
remain. The targeted Unicode oracle below covers this affected path. No JSON SIMD
or meaningful JSON execution speedup is claimed.

Generic ARM64 cross-codegen of the current guarded pre-O2 IR produces NEON
arithmetic/reduction with the pinned opt/llc (`arm64-cross.json`). No host feature
list was substituted. This is diagnostic cross-compilation only, not native ARM
execution, macOS/Windows qualification or a new public cross-compilation API.

## 5. Compilation and execution results

Complete production `voidc build` wall medians include frontend, O2/codegen,
matching toolchain-version checks and native linking. Sizes/defined outputs are
matched; normal compilation is retained as a control.

| Program | O2 build ms, baseline→candidate | Change | Normal build ms, baseline→candidate |
|---|---|---|---|
| guarded | 138.784→144.286 | +3.96% | 134.404→132.996 |
| checked_i32 | 162.462→161.825 | -0.39% | 156.354→154.413 |
| text | 169.661→170.005 | +0.20% | 160.718→159.974 |
| managed | 227.009→227.764 | +0.33% | 211.352→210.796 |
| collections_2m | 155.924→154.754 | -0.75% | 153.254→151.588 |
| json_typed | 4249.817→4236.774 | -0.31% | 3337.010→3333.882 |
| json_validated | 2385.659→2388.425 | +0.12% | 1860.227→1882.166 |

Isolated LLVM phase medians, entry emission (diagnostic copy, two samples per variant):

| Program | Optimization pipeline ms, baseline→candidate | Codegen ms, baseline→candidate |
|---|---|---|
| guarded | 4.913→6.718 | 6.769→10.416 |
| f32_streams | 4.972→5.631 | 7.694→7.259 |
| dependent_f64 | 4.350→5.179 | 6.455→5.890 |
| checked_i32 | 8.820→9.642 | 16.128→15.543 |
| text | 12.475→13.120 | 20.545→20.001 |
| managed | 30.398→31.447 | 49.744→49.034 |
| collections_2m | 5.483→6.043 | 14.105→13.626 |
| json_typed | 1225.142→1222.948 | 2096.507→2104.253 |
| json_validated | 696.240→677.383 | 1116.608→1109.249 |

Complete independently linked O2 program execution, including startup, Runtime,
checks, allocations and output:

| Program | Runtime input | Baseline wall ms | Candidate wall ms | Change | CPU ms, baseline→candidate |
|---|---|---|---|---|---|
| guarded | 100000000 | 115.499682 | 42.889134 | -62.87% | 114.2045→41.9260 |
| conditional | 100000000 | 115.880018 | 42.653209 | -63.19% | 114.5890→41.7255 |
| bidirectional | 1000000 | 4.715539 | 4.828085 | +2.39% | 3.9620→4.0910 |
| f32_streams | 1000000 | 5.265805 | 5.421199 | +2.95% | 4.4855→4.5010 |
| dependent_f64 | 1000000 | 5.784515 | 5.632001 | -2.64% | 4.8625→4.7775 |
| checked_i32 | 8192 | 31.289851 | 30.980478 | -0.99% | 30.1190→29.9600 |
| checked_f32 | 8192 | 30.832764 | 31.027246 | +0.63% | 29.8975→29.9920 |
| fixed_i32 | 1024 | 969.561875 | 1000.806125 | +3.22% | 963.9285→995.6485 |
| text | 7 | 4.550458 | 4.407768 | -3.14% | 3.8205→3.7270 |
| managed | 7 | 3.547030 | 3.625026 | +2.20% | 2.9225→2.8745 |
| collections_2m | 7 | 329.370963 | 327.155454 | -0.67% | 327.2430→324.7135 |
| json_typed | application | 8.274867 | 8.371475 | +1.17% | 7.5125→7.5580 |
| json_validated | application | 4.832234 | 4.892057 | +1.24% | 4.1190→4.1870 |
| collections_small | 7 | 3.427019 | 3.312843 | -3.33% | 2.6790→2.6190 |

The guarded build cost rises **138.784→144.286 ms**, but paired build plus first
100-million-iteration execution improves **254.303→187.310 ms**. This is the median
of paired sums of the two observed child-process durations, not a sum of separately
chosen best observations. Four pairs per variant have non-overlapping ranges.
A short program built only once and executed briefly need not repay the extra
optimization/codegen. AOT reuse and compute-heavy eligible loops benefit; the
compiler makes no new runtime-hotness promise.

Beyond that arithmetic example, larger typed JSON builds are **4,249.817→4,236.774 ms**
and validated JSON **2,385.659→2,388.425 ms**. Text/managed/large-collection builds
and code size show no material broad compilation regression. Broader application
execution remains effectively unchanged in this sample; benefits are not generalized
to checked arrays, text, collections or JSON.

Unfavorable samples remain visible: fixed i32 **+3.22%**, independent f32 **+2.95%**,
bidirectional **+2.39%**, managed **+2.20%**, typed/validated JSON **+1.17/+1.24%**.
The first four have byte-identical executables, with local run ranges retained;
those shifts cannot be attributed to different generated code. No follow-up
performance search or selective replacement was used. The slight favorable
array/text/collection observations likewise do not establish SIMD gains.

## 6. Correctness, ownership and platform qualification

- **288** real CLI JIT/emit/independent-clang-link/AOT commands passed for all 18
  retained fixtures, baseline/candidate, normal/O2. All five numeric array types
  retain their checks and outputs. Deliberate bounds errors precede division by
  zero, with identical diagnostics. Independent native links use the same accepted
  Runtime inputs; real CLI `build` programs above are also independently executed.
- **180** independent AOT range oracle executions passed: negative, empty, small,
  tail/vector-boundary ranges and n=40,000,000 with signed i32 wrapping. The ordinary
  descending range remains descending; positivity is not assumed without its guard.
- **64** independent f32/f64 bit comparisons passed in separately linked baseline/
  candidate, normal/O2 AOT. Exported source recurrence functions are checked against
  volatile, separately rounded C++ multiplication/addition, contraction disabled
  in the reference. Cases include descending/empty/short/long input, up to one
  million iterations. Only test symbol names were aliased for the native driver;
  generated instructions were not edited. Existing edge fixtures check integer
  wrapping, NaN, signed zero and subnormal behavior.
- The affected JSON `_scalar` path passes exact Unicode boundary and malformed
  surrogate oracles in baseline Release and candidate Release/Debug/strict, normal/O2:
  **8 independently linked executions**, 12 boundary values plus two error cases each.
- Focused CTest numeric/compiler, fixed/growable arrays, Runtime coordination,
  GC/managed ownership, precise roots, async/lifecycle/cancellation, cache/reload,
  JSON ownership and normal/O2 linked Runtime/managed/reference AOT checks:

| Candidate configuration | Focused result | Wall seconds |
|---|---|---|
| release | 28/28 passed | 35.45 |
| debug | 28/28 passed | 100.44 |
| strict | 28/28 passed | 96.05 |

Strict uses ASan+UBSan/leaks with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS=exitcode=23`.
No suppression or contract relaxation was introduced. No compiler crash,
sanitizer finding, semantic mismatch or target-feature failure occurred in the
completed qualification. Intentional language failures are retained.

Native results are Linux x86-64 only. Native ARM64 and Windows/macOS remain separate
Boss-controlled work; no hosted test was dispatched. This is bounded regression
coverage, not an exhaustive application/GC-interleaving proof. The private optional
research proof configuration was not qualified.

## 7. Original failures, tradeoff and owner stop

Two harness failures are preserved in `incidents.json`:

1. An entry-reachable diagnostic object was initially compared with public all-export
   `emit` output. The failed byte comparison and both objects remain. Correct
   matching-scope captures verify production fidelity; main performance runs and
   phase timing already used production `build`'s entry emission.
2. The floating-point test symbol parser attempted to hex-decode the separate
   `__vd_types__void` metadata symbol. Its original driver/log/receipt remain; the
   parser now skips non-hex metadata names. No backend change or performance rerun
   was made to resolve either issue.

**RETAIN, limited to native AOT O2.** This is worthwhile for the measured eligible
integer class: actual portable packed code, correct scalar fallback and wrapping,
net benefit even including a full build for the long control, and no demonstrated
material broad compile/code-size or execution regression. It does not establish a
benefit for every program. The extra eligible-loop compile time and code growth
are explicit costs for Boss review, not a new percentage target or release hold.
The JIT-startup failure that rejected the global candidate is avoided by leaving
JIT target analysis and cache policy alone.

**Implementation sprint complete. Boss subsequently authorized explicit staging
for cross-platform qualification; no commit, push, branch or CI dispatch is
authorized for the preparation agent. Stop for Boss review.**
