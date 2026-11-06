# Future optimization: Automatic SIMD vectorization

**Accepted baseline — 6 November 2026:** commit `175c08c8a0cab0b701ba71365ffcc398a0c9bcf3`
passed [all seven jobs in run 42231556437](https://github.com/Shmellyorc/Void.Language/actions/runs/42231556437): Linux Debug, Release and Sanitizers;
Windows Debug/Release; macOS ARM64 Debug/Release. The following preparation/local
measurements are preserved as dated evidence. This compiler CI does not qualify
the new source-install wrapper or independently portable packages.

The [feasibility report](compiler-health/AUTOMATIC_SIMD_FEASIBILITY.md) led to a
[global JIT+AOT candidate](compiler-health/AUTOMATIC_SIMD_QUALIFICATION.md), which
was rejected and reverted after cold O2 CLI startup regressed 28–34%. The
[retained AOT-only candidate](compiler-health/AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md)
passes the existing native AOT TargetMachine into standard O2 analysis. JIT remains
target-independent, ORC and logical JIT cache policy are unchanged, and normal
whole-compiler build fingerprinting remains intact. SLP remains off. Checked arrays
retain their bounds, mutation, ownership, roots and safepoint checks; this is not
general checked-array SIMD.

Local Linux results and platform limits are recorded in that report. The
[staging record](compiler-health/AUTOMATIC_SIMD_AOT_STAGING.md) links the accepted
seven-job qualification.
The original 5 November investigation direction and deferred broader scope follow.

## Goal

VOIDC should automatically use SIMD where the optimizer can prove that vector execution is **semantically equivalent** and beneficial, while retaining ordinary scalar operations and a scalar fallback. Preserve familiar VOID syntax: ordinary numeric variables, arrays and loops. **No new SIMD keywords, mandatory vector types or Odin-style `#simd`/`#soa` syntax** for the first delivery. Explicit vector types/intrinsics are only a separate future design decision if real applications establish a need that auto-vectorization cannot satisfy.

## Historical investigation starting point

The documented LLVM/Clang/LLD toolchain is **23.1.4**. `docs/llvm23/IMPLEMENTATION.md` records standard O2 through a `PassBuilder` without a `TargetMachine`, and its captured pipeline reports SLP disabled in that configuration. This is a **research lead, not proof** that numeric loops fail to vectorize or that enabling SLP will accelerate them. Distinguish LLVM IR-level passes from target machine codegen and exact feature/target policy in JIT versus AOT.

## Preserved research and semantic requirements

1. Establish the current LLVM 23 JIT and independently linked AOT output for ordinary scalar numeric loops and array operations. Inspect optimized LLVM IR and generated machine code; characterize existing loop/SLP vectorization rather than assuming it is absent.
2. Identify the smallest compiler/backend change that improves **automatic, target-aware** vectorization of eligible contiguous numeric operations (e.g., `u8`, `i32`, `i64`, `f32`, `f64`) without changing source syntax or public types. Check target-machine configuration, data layout, CPU feature specification, checked source operation lowering, alias/dependency information and vectorization profitability.
3. Preserve all VOID semantic rules: evaluation and error order; integer overflow/wrapping policy; floating-point result and exceptional behavior without silently enabling fast-math; array bounds and mutation/version checks; precise GC roots/safepoints; ownership, cancellation and native/async lifetimes. Never hoist or suppress an observable failure, retain an unrooted managed pointer across a safepoint, or change aliasing authority.
4. Support portable scalar fallback and legal machine features. Test x86-64 and ARM64 as available; do not emit AVX/AVX2/AVX-512/NEON instructions for unsupported targets. Preserve JIT/AOT, cache/target identity and reload compatibility.
5. Compare matched real execution with unchanged source and equivalent outputs: independent numeric loops, dependent loops that must stay scalar, checked dynamic arrays, contiguous fixed arrays, small/large and unaligned inputs, scalar/float edge cases and large buffers. Record machine code evidence, complete wall/CPU and regressions; preserve all raw runs. Remove or narrow a candidate that worsens representative programs.

## Timing, ownership and release

**Retained bounded AOT-only repair; broader SIMD work remains deferred.** The
JIT-affecting global candidate remains rejected. SLP default enablement,
checked-array lowering changes, range specialization and public SIMD types are
not authorized by this preparation. Review the measured compile-time/code-size
costs as well as execution benefit. No invented percentage target, new release
hold or application-wide speedup claim is added. Git/hosted qualification remains
under Boss control.

**Documentation placement:** incorporate this section into `docs/DEVELOPMENT_STRATEGY.md` under future optimization work and index it from `docs/README.md`; preserve the detailed version as a `docs/`-internal roadmap note if the existing structure benefits. Root `README.md` is protected.
