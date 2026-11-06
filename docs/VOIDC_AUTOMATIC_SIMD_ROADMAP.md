# Future optimization: Automatic SIMD vectorization

**Owner decision: 5 November 2026 — approved for future investigation, not implementation now.**

## Goal

VOIDC should automatically use SIMD where the optimizer can prove that vector execution is **semantically equivalent** and beneficial, while retaining ordinary scalar operations and a scalar fallback. Preserve familiar VOID syntax: ordinary numeric variables, arrays and loops. **No new SIMD keywords, mandatory vector types or Odin-style `#simd`/`#soa` syntax** for the first delivery. Explicit vector types/intrinsics are only a separate future design decision if real applications establish a need that auto-vectorization cannot satisfy.

## Current starting point to verify

The documented LLVM/Clang/LLD toolchain is **23.1.4**. `docs/llvm23/IMPLEMENTATION.md` records standard O2 through a `PassBuilder` without a `TargetMachine`, and its captured pipeline reports SLP disabled in that configuration. This is a **research lead, not proof** that numeric loops fail to vectorize or that enabling SLP will accelerate them. Distinguish LLVM IR-level passes from target machine codegen and exact feature/target policy in JIT versus AOT.

## Future research and bounded prototype

1. Establish the current LLVM 23 JIT and independently linked AOT output for ordinary scalar numeric loops and array operations. Inspect optimized LLVM IR and generated machine code; characterize existing loop/SLP vectorization rather than assuming it is absent.
2. Identify the smallest compiler/backend change that improves **automatic, target-aware** vectorization of eligible contiguous numeric operations (e.g., `u8`, `i32`, `i64`, `f32`, `f64`) without changing source syntax or public types. Check target-machine configuration, data layout, CPU feature specification, checked source operation lowering, alias/dependency information and vectorization profitability.
3. Preserve all VOID semantic rules: evaluation and error order; integer overflow/wrapping policy; floating-point result and exceptional behavior without silently enabling fast-math; array bounds and mutation/version checks; precise GC roots/safepoints; ownership, cancellation and native/async lifetimes. Never hoist or suppress an observable failure, retain an unrooted managed pointer across a safepoint, or change aliasing authority.
4. Support portable scalar fallback and legal machine features. Test x86-64 and ARM64 as available; do not emit AVX/AVX2/AVX-512/NEON instructions for unsupported targets. Preserve JIT/AOT, cache/target identity and reload compatibility.
5. Compare matched real execution with unchanged source and equivalent outputs: independent numeric loops, dependent loops that must stay scalar, checked dynamic arrays, contiguous fixed arrays, small/large and unaligned inputs, scalar/float edge cases and large buffers. Record machine code evidence, complete wall/CPU and regressions; preserve all raw runs. Remove or narrow a candidate that worsens representative programs.

## Timing, ownership and release

**Deferred optional optimization.** Do not interrupt the current integrated performance repair, correctness verification, native cross-platform qualification, or Boss-controlled release work. After that work is settled, Golden Wrench and Boss can authorize a focused Research Clanker feasibility pass and, if supported, one Keyboard Clanker implementation experiment. No invented speedup target, no new release hold, and no claim of SIMD support until compiled machine code and behavioral equivalence are verified.

**Documentation placement:** incorporate this section into `docs/DEVELOPMENT_STRATEGY.md` under future optimization work and index it from `docs/README.md`; preserve the detailed version as a `docs/`-internal roadmap note if the existing structure benefits. Root `README.md` is protected.
