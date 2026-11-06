# Automatic SIMD — target-aware O2 qualification

**6 November 2026 status: Historical global trial — rejected/reverted.**

This report describes the rejected global JIT+AOT trial. References to the
"checkout" and "final working source" below describe its completion snapshot,
not the current retained AOT-only candidate. Its JIT/ORC/cache changes remain
reverted. See the [AOT-only qualification](AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md)
and [current Git/CI preparation](AUTOMATIC_SIMD_AOT_STAGING.md).

6 November 2026. Golden Wrench/Boss review. Local engineering qualification of the
approved limited assignment in [the handoff](AUTOMATIC_SIMD_FEASIBILITY.md) and
[the roadmap](../VOIDC_AUTOMATIC_SIMD_ROADMAP.md).

**Recommendation: REVISE — backend candidate reverted.** The candidate produces real target-legal SIMD and passes local correctness qualification, but materially worsens cold execution of the ordinary O2 CLI controls. The requested performance stop condition was applied. The checkout retains the baseline backend; the evaluated implementation, binaries, patch and all evidence remain available for review.

## Implementation and provenance

The starting checkout was exactly `83036facdf74c5a1eee6f475b6cfe110f4752f27`.
There were no tracked edits or subsequent commits to preserve. Existing untracked
experiments/evidence were left in place. No staging, commits, branches, pushes,
hosted jobs or publication were performed.

The evaluated candidate changed `src/backend/llvm/backend.cpp` as follows.
**That production-file change has now been reverted byte-for-byte to the accepted
baseline.** `candidate-backend.patch` and `candidate-backend.cpp` in qualification
evidence retain the exact implementation; `candidate-{release,debug,strict}`
retain its compiled products. The remaining checkout changes are the tests/fixtures
and this report:

- `finish` receives a `TargetMachine&`; its standard per-module O2 PassBuilder
  receives that machine. LLVM verification remains before and after optimization.
  SLP is explicitly disabled; loop vectorization retains LLVM's default policy.
- AOT passes the very same generic deployment machine used for object emission:
  existing native deployment triple, CPU `generic`, empty extra features, PIC,
  unchanged TargetOptions and code-generation level. The private research caller
  of `finish` was updated too; its separate storage-proof pipeline is unchanged.
- JIT retains a private analysis machine made from ORC's **finalized**
  `JITTargetMachineBuilder`, in its compile-function creation callback. ORC's
  default simple/concurrent compiler choice is preserved and its codegen machine
  comes from that same builder. CPU, features, triple, options, relocation model,
  code model and codegen level therefore have one configuration source. A runtime
  check rejects an analysis/LLJIT triple or data-layout mismatch.
- The existing native-object identity now describes the effective relocation and
  code model, and the O2 pipeline field advances from `ir-O2` to
  `ir-O2-target-aware-v1-slp-off`. Build fingerprinting, ABI fields, source identity,
  cache envelope, publication and reload ownership remain in the existing system.
  The more accurate target identity also invalidates earlier no-O2 keys.

A significant audit detail: the pinned LLVM LLJIT builder can set PIC and the small
code model while preparing its platform linker. Creating analysis immediately
before passing the original host builder into LLJIT would miss those final
settings. The final callback observes them without reproducing platform-selection
rules. The default compiler selection and builder finalization were checked in
[pinned LLJIT.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp#L620).
The builder transfers all target configuration into machine construction in
[pinned JITTargetMachineBuilder.cpp](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/ExecutionEngine/Orc/JITTargetMachineBuilder.cpp#L36).
Copies of both sources accompany the evidence.

Retained checkout files:

| File | Purpose |
|---|---|
| `tests/numeric/tests.cpp` | Normal/O2 range oracle, checked alias dependency, first-error order, dependent f32 result-bit oracle using the existing private IR bitcast |
| `tests/cache/tests.cpp` | O2 replacement miss/hit and retained old-generation lease |
| `tests/numeric/automatic_simd/guarded-range-i32.void` | Ordinary eligible source loop, reused by numeric tests and available for codegen inspection |
| `tests/numeric/automatic_simd/alias-dependent.void` | Shared-list recurrence, explicitly exercised with Runtime support |
| `tests/numeric/automatic_simd/failure-order.void` | Bounds error must precede division by zero |
| `tests/numeric/programs/simd_numeric_edges.void` | Wrapping u8/i32/i64, negative zero, subnormal and NaN controls |
| `tests/numeric/programs/simd_independent_f32.void` | Four independent recurrence streams with SLP still off |
| `tests/numeric/programs/simd_dependent_f64.void` | Dependent floating recurrence |
| This report | One implementation/qualification record |

There are no frontend, Runtime, public-header, numeric-policy, checked-array,
root-ownership, GC, safepoint, cancellation or range-direction changes. No new
syntax, SIMD type/API, fast-math, FMA permission, noalias promise, raw-buffer access
or speculative unchecked memory operation was introduced.

## Evidence identities and reproduction

The [committed global-trial receipts and archive inventory](evidence/automatic-simd/README.md#global)
preserve selected exact text records and identify the complete original local
archive at `/home/shmellyorc/.local/state/void-log-performance/simd-qualification`.
Rejected backend copies, binaries and caches remain local. Bare artifact names
below identify that trial's records; the bundle manifest enumerates the committed
subset.
The final `manifest.json` records their sizes and SHA-256 hashes;
`qualification-identity.json` records the evaluated candidate source hashes, host
and toolchain identity; `reversion.json` records final baseline restoration.

Before reuse, all **513** entries in Research Clanker's retained `manifest.json`
were checked for existence, size and SHA-256. All matched. All **seven** linked
compiler/tool/library identities and **six** recorded production-source hashes
also matched. See `research-verification.json`. The source fixtures, including
original invalid/negative controls, were copied without altering the originals.
The original research directory remains
[here](evidence/automatic-simd/README.md#research).

Host: Linux x86-64, Ryzen 7 1700X, LLVM host CPU `znver1`, LLVM/Clang/LLD 23.1.4,
revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. Baseline compiler binaries and
backend/cache-identity archives were copied before modification. The matched JIT
host links those production archives for baseline, and the final production
archives for candidate, against the same frontend and Runtime inputs.

`build-hosts.py` builds the timing hosts and separate capture-only backends.
Capture copies add only IR/object file writes to baseline/final production source;
they implement no alternative target selection or optimization pipeline.
`capture-codegen.py` establishes fidelity: **16 AOT captures are byte-identical to
production CLI objects**, and baseline/candidate capture JIT cache envelopes are
byte-identical to their unmodified production-backend hosts. Measurements use the
unmodified backend hosts and actual CLI-emitted independently linked objects.

After reversion, reproduction drivers select the preserved candidate source and
compiler/archive products, rather than the restored production backend. Use a
fresh private evidence directory for a reproduction; the sealed cache roots
retain the original miss/hit sequence.

Main drivers: `screen-production.py`, `range-oracles.py`, `qualify-suites.py`,
`final-numeric.py`, `cache-invalidation.py`, `measure-production.py`,
`general-perf.py`, `followup-performance.py`, `cli-o2-performance.py`,
`measure-aot-compile.py`, `reverted-checks.py`, `summarize.py`. Receipts include exact argv, results and output.
Source executions, independently linked executables, timing hosts and the focused
CTest runs inherit **1,048,576-byte soft AND hard stack limits** using
`setrlimit(RLIMIT_STACK)`. Builds themselves retain the ordinary host stack. `live-stack-witness.json`
additionally records the actual `/proc` soft/hard limits of running CTest and
compiler children, rather than relying only on driver intent.

## Verified machine code and scalar behavior

| Control | Baseline | Candidate |
|---|---|---|
| Guarded/conditional i32 loop, host JIT | Scalar | Vector integer IR and AVX2 `vpmuludq`/`vpaddd` on this host; vector epilogue and scalar remainder |
| Guarded/conditional loop, generic native AOT | Scalar | SSE2 `pmuludq`/`paddd`; no AVX/AVX2 deployment requirement |
| Unguarded bidirectional range | Scalar | Scalar; selected positive/negative direction preserved |
| Four independent f32 recurrences | Scalar | Scalar with SLP off; separate multiply/add, no FMA |
| Dependent f64 recurrence | Scalar | Scalar and ordered |
| Checked fixed/dynamic arrays and shared alias recurrence | Checked calls and GC polls | Same checked operations/polls; no numeric SIMD claim |

These are observed local instructions, not required ISA spellings or widths.
The guarded JIT IR branches around its vector body for short inputs and retains a
scalar tail; no width was forced. At n=1,000,000 it returns **62,999,875**.
`captures/*.post.ll`, `*.jit.asm`, `*.aot.asm` and `codegen-inventory.json` retain
the inspection evidence. The sampled FP IR has no fast/reassociation/contraction
permission. Vector wrapping additions have no blanket signed-overflow promise.

All **18 normal AOT objects** are byte-identical to baseline. **15/18 O2 objects**
are identical. Two differences are eligible integer loops. The third is the edge
fixture: LLVM partially unrolls its dependent scalar subnormal loop; the retained
IR still performs each ordered multiply separately. `edge-asm.diff` records this
non-SIMD effect. The source edge oracle passes in both engines and modes.

The fixed/dynamic u8/i32/i64/f32/f64 object controls remain scalar. Inspected i32
array/alias IR retains the same checked helper and poll counts, and their O2 AOT
objects are byte-identical. Scalar FP instructions using XMM registers were not
counted as packed arithmetic.

A fresh generic `aarch64-unknown-linux-gnu` opt/llc control on the current guarded
pre-O2 IR produces NEON vector arithmetic and reduction code. Exact commands and
output are in `arm64-cross.json`, `arm64-loop.ll` and `arm64-loop.s`. This is
cross-compilation evidence only; it is not native ARM execution or a VOID public
cross-compilation feature.

## Correctness, roots, cancellation and cache

- **288 production comparison commands passed:** 18 retained fixtures × normal/O2
  × baseline/candidate, using real CLI JIT, object emission, native linking and
  execution. Fixed/dynamic arrays cover all five requested numeric types,
  element-offset-one accesses and output checks; shared-list aliases retain their
  recurrence. Intentional bounds failures occur before division by zero in all
  combinations. Raw records: `screen-production.json`.
- **288 independent range-oracle executions passed:** guarded, conditional and
  unguarded ranges; negative/zero/positive lengths, vector boundary/tail lengths,
  and n=40,000,000, whose reduction crosses signed i32 and wraps. Baseline/candidate,
  normal/O2, JIT/AOT all agree with an independent modular arithmetic oracle.
- Numeric tests add an exact f32 bit-pattern comparison for a dependent recurrence
  at 0/1/17/129/1025 iterations against C++ volatile, separately rounded
  multiply/add intermediates. Existing numeric tests cover conversions, division,
  overflow, NaN payload/sign, signed zero and FP boundaries in normal/O2.
- Existing focused suites exercise precise root publication/retirement, forced GC,
  allocation-failure sweeps, checked bounds/mutation failures, cleanup ordering,
  native and async cancellation, independently linked coordinated AOT, source
  compatibility, generation lifetime and old leases.

| Candidate build | Result under actual 1 MiB stacks | Receipt |
|---|---|---|
| Release (C++ Release) | 86/87 initial; new alias-fixture link placement corrected, then final numeric + numeric_cli 2/2. Every selected test has a passing final result. | suites-release.*, numeric-final-release.* |
| Debug | 87/87 pass, 874.14 s | suites-debug.* |
| Strict (C++ RelWithDebInfo, ASan/UBSan/leaks) | 87/87 pass, 804.98 s | suites-strict.* |

Strict runs use `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, and `LSAN_OPTIONS=exitcode=23`.
No sanitizer suppression or weakened failure policy was added. CTest logs, JUnit
and driver receipts are retained for each build mode. Test-generated tracked
math/random receipts were copied to qualification evidence and restored in the
checkout, keeping this change scoped.

Cache qualification includes cold/miss/hit, fresh-process reuse, source/build and
optimization-mode separation, corruption/truncation/invalid-object fallback,
concurrent publication and filesystem failure, roots on hits and O2 reloads.
`cache-invalidation.json` additionally uses **the same fixed embedder build identity**
on baseline and candidate: baseline miss→hit; candidate miss→hit under a different
key; baseline and candidate then each hit only their own entries. Thus invalidation
does not rely solely on CMake changing the compiler build identity. Existing cache
suites also exercise automatic caching and reload compatibility. The added O2
replacement check verifies the new body, a reload cache hit and unchanged old code.

## Matched execution, compilation and size

The main matched run uses baseline/candidate/candidate/baseline blocks twice, pinned to logical CPU 2, after qualification jobs completed. Each JIT configuration has four cold processes per variant and five timed complete invocations per process, after three warm calls: **80 processes / 400 timed kernel invocations**. Broader programs add **24 processes / 480 timed invocations**. Independent AOT adds **48 complete processes**, in three matched blocks. All defined outputs agree with their controls. Timings include real checks, roots and Runtime work. They are local desktop observations, not hardware-counter or dedicated-machine results. `summary.json` retains min/max, CPU medians and every raw record remains in the source receipts.

Cold compile medians below have four process samples per variant; the total column additionally includes frontend analysis and JIT construction. A later targeted follow-up retains the original unfavorable samples and rechecks the two largest execution increases. A separate **32-process** default-O2 CLI run measures cold and cache-hit one-shot execution (four samples per variant/state/length), including loader/startup and cache costs.

| JIT workload | Baseline wall ms | Candidate wall ms | Change | CPU ms, baseline→candidate |
|---|---|---|---|---|
| Guarded i32, 1,000,000 | 1.043235 | 0.320514 | -69.28% | 1.040000→0.321000 |
| Bidirectional i32, 1,000,000 | 1.288250 | 1.299685 | +0.89% | 1.284000→1.285000 |
| Independent f32, 1,000,000 | 1.723135 | 1.721545 | -0.09% | 1.716500→1.716000 |
| Dependent f64, 1,000,000 | 2.019135 | 2.015495 | -0.18% | 2.012000→2.003000 |
| Dynamic i32, 8,192 | 26.869150 | 27.409850 | +2.01% | 26.750000→27.311000 |
| Dynamic f32, 8,192 | 27.672800 | 26.565300 | -4.00% | 27.567500→26.458500 |

| n=17 control | Baseline wall ms | Candidate wall ms |
|---|---|---|
| guarded-range-i32 | 0.000090 | 0.000055 |
| slp-f32 | 0.000080 | 0.000090 |
| dynamic-i32 | 0.073165 | 0.074595 |
| dynamic-f32 | 0.074030 | 0.073920 |

Sub-microsecond pure-loop timings do not support a percentage-speedup claim.

| Cold JIT compile | Lower/optimize/codegen ms | Analyze + create JIT + compile ms |
|---|---|---|
| Guarded i32 | 7.272→13.655 | 9.329→15.885 |
| Bidirectional i32 | 6.615→7.177 | 8.555→9.314 |
| Independent f32 | 8.734→9.321 | 10.975→11.812 |
| Dependent f64 | 7.379→7.844 | 9.367→10.079 |
| Dynamic i32 | 27.213→27.895 | 29.699→30.595 |
| Dynamic f32 | 28.271→29.106 | 30.883→31.852 |

| Broader JIT program | Invocation wall ms | Change | Cold compile ms |
|---|---|---|---|
| Managed values/cleanup | 0.047160→0.047005 | -0.33% | 97.775→98.496 |
| String stress | 1.030030→1.037140 | +0.69% | 36.606→37.224 |
| 2 MiB collection | 312.861000→316.237000 | +1.08% | 20.384→21.006 |

| Independent AOT complete process | Baseline wall ms | Candidate wall ms | Change |
|---|---|---|---|
| Guarded i32, 100,000,000 | 115.564902 | 43.149618 | -62.66% |
| Dynamic i32, 8,192 | 30.767708 | 30.670234 | -0.32% |
| Independent f32, 1,000,000 | 4.974307 | 5.012792 | +0.77% |
| Dependent f64, 1,000,000 | 5.279247 | 5.290072 | +0.21% |

**Default O2 CLI, one invocation per process, automatic cache:**

| n | Cache state | Baseline median [min,max] ms | Candidate median [min,max] ms | Change |
|---|---|---|---|---|
| 17 | miss | 28.574 [26.777, 29.440] | 36.542 [33.509, 40.014] | +27.9% |
| 17 | hit | 8.997 [8.918, 9.016] | 9.136 [9.009, 9.579] | +1.5% |
| 1000000 | miss | 29.812 [27.202, 33.182] | 39.841 [36.711, 55.546] | +33.6% |
| 1000000 | hit | 10.549 [9.618, 13.508] | 11.401 [9.620, 15.533] | +8.1% |

Matched complete AOT `emit` processes (four cold samples per variant/mode,
including frontend and object writing; `matched-aot-compile.json`):

| Source | Mode | Baseline wall ms | Candidate wall ms |
|---|---|---|---|
| guarded-range-i32 | normal | 17.819 | 18.472 |
| guarded-range-i32 | O2 | 24.244 | 30.761 |
| dynamic-i32 | normal | 34.095 | 34.187 |
| dynamic-i32 | O2 | 40.929 | 40.871 |
| slp-f32 | normal | 20.055 | 20.362 |
| slp-f32 | O2 | 25.113 | 24.815 |

Code growth is real. The guarded object grows from **2,888→4,344 bytes** for JIT,
and **4,312→5,576 bytes** for AOT (including debug/metadata). AOT `.text`, including
its CLI adapter, grows **491→1,071 bytes**. The scalar edge object grows
**4,648→4,696 bytes**, with `.text` **579→620 bytes**. The other 15 O2 controls keep
identical object bytes. See `production-object-comparison.json`; capture object
sizes and IR arithmetic inventories are in `codegen-inventory.json`.

The eligible hot kernel improves **3.25× in JIT** and **2.68× in generic AOT complete-process execution**. Those gains are genuine, but cold JIT lowering/optimization/codegen rises **7.272→13.655 ms**; total analyze/JIT/compile rises **9.329→15.885 ms**. The measured invocation saving would take roughly ten such calls to repay that extra compilation, before other process/cache effects. This is an observation, not a new acceptance target.

The decisive result is actual cold O2 CLI execution: **28.574→36.542 ms** at n=17 and **29.812→39.841 ms** at n=1,000,000. In both cases the slowest baseline cold sample is faster than the fastest candidate cold sample. This is a repeatable startup cost on an ordinary supported execution path, so retaining the candidate would violate this assignment's material-performance-regression stop condition. Cache-hit observations overlap and do not justify a universal hit-speedup or hit-regression claim.

The smaller initial unfavorable execution medians are retained: dynamic i32 **+2.01%**, string stress **+0.69%**, large collection **+1.08%**, and bidirectional arithmetic **+0.89%**. The apparent dynamic-f32 **−4.00%** is not a SIMD result. A targeted follow-up reverses the two largest execution increases: dynamic i32 **27.292→27.254 ms**, large collection **327.426→326.915 ms**. Their machine-code objects are identical. The three broader program objects are also byte-identical (62,848 / 30,696 / 12,744 bytes). These small execution shifts are inconclusive local variability; they do not erase the consistent compile/startup regression.

No additional target detection, CPU dispatch, semantics change or unchecked-array shortcut was attempted to recover the result. An AOT-only policy or a new hot/cold optimization policy would require a separate design decision; neither was silently substituted for the approved shared target-aware O2 repair.

## Original failures and limits

All originals remain in `incidents.json`, logs, XML and retained artifacts:

1. A cache driver raced relinking its candidate host and received
   `FileNotFoundError`. Its baseline cache had already been populated; the first
   retry correctly hit it and failed a cold-cache assertion. The original cache
   directory and retry receipt remain. A new private directory produced the
   required six-step invalidation sequence.
2. The first new FP bit-pattern wrapper omitted the source context required by the
   existing checked-call ABI. `numeric-early.json` records the ordinary exception;
   the wrapper was corrected, retaining the contract.
3. The new alias fixture was initially placed in the Runtime-free numeric CLI AOT
   glob, producing undefined Runtime ABI references. Its original Release failure
   remains in `suites-release.log/xml`. The fixture moved to Runtime-backed numeric
   coverage; final numeric/CLI checks pass. Independent AOT comparisons had already
   passed with the correct Runtime link.

An additional CLI mode audit found that the existing explicit `--cache` flag selects non-O2 Debug caching (`src/main.cpp`). Those original 32 normal-mode cold/hit samples remain in `followup-performance.json`; they are not mislabeled as O2 evidence. The decisive default-O2 run omits that flag, uses fresh private roots, checks outputs and retains its cache envelopes in `cli-auto-o2-*`. `cli-o2-cache-audit.json` records one cache entry per root. No compiler signal/crash, sanitizer failure, target-feature mismatch, semantic mismatch or cache-ownership regression occurred in the completed candidate qualification.

After reversion, the primary compiler, numeric/core test binary and cache test
binary were rebuilt in Release, Debug and strict modes. Numeric, JIT, GC, reload
and cache checks pass **5/5 in each mode (15/15 total)**, again with 1 MiB soft/hard
stacks and strict sanitizer settings. See `reverted-build-*` and
`reverted-checks-*`. The restored backend is byte-identical to `83036fac`, SHA-256
`dd67d152fe28f025499f56cced2b0c39187bfdf80a47aa1e87c31e408c53b39d`.
No production backend improvement is retained. New regression tests and fixtures
remain useful on the accepted scalar baseline. The full candidate qualification
receipts describe the evaluated candidate, not a SIMD feature present in the
final working source.

Native qualification here is **Linux x86-64 only**. Windows/macOS x86-64/ARM64,
other CPU models and native ARM execution were not run or dispatched. No exhaustive
application performance or all possible GC/cancellation schedules are claimed.
Supported deployments still rely on their existing owner-controlled native
qualification; these local results create no new release hold or percentage gate.
There is no general checked-array SIMD, SLP-default enablement, range specialization
or application-wide speedup claim.

**REVISE. Stop for Golden Wrench/Boss review. No SIMD backend candidate is retained in the working source.**
