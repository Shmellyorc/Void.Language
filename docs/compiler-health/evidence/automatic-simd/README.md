# Automatic SIMD evidence publication

This is a curated, byte-preserving text bundle for the AOT-only staging review.
No experiment, compilation or qualification test was rerun to prepare it.
[Bundle manifest](BUNDLE_MANIFEST.json) lists every copied artifact's original
phase/name, byte count and SHA-256. [Source identity](SOURCE_IDENTITY.json) pins
the approved backend, tests and fixtures. [Preparation receipt](PREPARATION.json)
and [exact staging list](STAGING_MANIFEST.txt) describe the prepared Git scope.

**Current decision: retain native AOT-only target-aware O2 for cross-platform
qualification.** The global JIT+AOT candidate remains rejected/reverted. JIT/ORC
and logical cache policy stay unchanged; whole-compiler build fingerprinting is
preserved. SLP stays off and checked arrays remain checked. No general array SIMD,
application-wide speedup or completed Windows/macOS/native-ARM64 acceptance is claimed.

## Research

Historical feasibility, including the four-way target/SLP comparison:

- [Input identity](research/identity.json), [linked input identity](research/linked-input-identity.json).
- [Matched summary](research/matched-summary.json), [all matched JIT observations](research/matched-jit.json).
- [Baseline object equivalence](research/baseline-equivalence.json), [production recheck](research/production-recheck.json).
- [Native memory-loop control](research/native-controls.csv), [native run receipt](research/native-run.json). These are not VOID checked-array speedups.
- [Original screen](research/screen.json), [initial additional screen](research/additional-screen-initial.json), [corrected screen](research/corrected-screen.json), [extent correction](research/sizing-note.txt).
- [Full original archive manifest](research/manifest.json).

## Global

Rejected/reverted shared JIT+AOT trial; its measurements must not be attributed
to the retained AOT-only implementation:

- [Trial identity](global/qualification-identity.json), [reversion receipt](global/reversion.json).
- [Summary](global/summary.json), [matched execution](global/matched-production-all.json), [broader observations](global/general-perf.json).
- [Cold/warm O2 CLI regressions](global/cli-o2-performance.json), [AOT compile observations](global/matched-aot-compile.json).
- [Original incidents](global/incidents.json), [original Release test log](global/suites-release.log), [corrected numeric/CLI rerun](global/numeric-final-release.log).
- [Full original archive manifest](global/manifest.json).

## AOT

Retained native-AOT-only candidate; local Linux x86-64 qualification:

- [Final source/tool/binary identities](aot/final-identity.json), [JIT source proof](aot/jit-source-proof.json), [normal compiler build identities](aot/compiler-build-identities.json).
- [Summary and min/max](aot/summary.json), [matched production builds](aot/build-timing.json), [first-run validation](aot/build-validation.json), [complete O2 execution](aot/execution-timing.json).
- [LLVM optimization/codegen phase times](aot/phase-timing.json), [paired build and first execution](aot/paired-build-execution.json), [JIT startup](aot/jit-startup.json), [JIT execution](aot/jit-execution.json).
- [JIT cache cross-read proof](aot/cache-proof.json), [cache proof summary](aot/cache-proof-summary.json).
- [Object identity comparisons](aot/codegen-summary.json), [instruction/section inventory](aot/codegen-inventory.json), [executable comparisons](aot/executable-comparison.json), [broader object comparisons](aot/broad-codegen-summary.json).
- Guarded AOT [baseline IR](aot/captures/guarded-range-i32.baseline.True.aot.post.ll) / [candidate IR](aot/captures/guarded-range-i32.candidate.True.aot.post.ll), [baseline disassembly](aot/captures/guarded-range-i32.baseline.True.asm) / [candidate disassembly](aot/captures/guarded-range-i32.candidate.True.asm).
- Guarded JIT [baseline IR](aot/captures/guarded-range-i32.baseline.True.jit.post.ll) / [candidate IR](aot/captures/guarded-range-i32.candidate.True.jit.post.ll); JIT stays scalar.
- [All functional command outputs](aot/functional.json), [range/wrap oracles](aot/ranges.json), [FP bit comparisons](aot/fp-bits.json), [Unicode boundary/error comparisons](aot/json-unicode.json).
- [Release](aot/suites-release.log), [Debug](aot/suites-debug.log), [strict ASan/UBSan/leaks](aot/suites-strict.log), [actual stack limit witness](aot/live-stack-witness.json).
- [ARM64 cross-codegen receipt](aot/arm64-cross.json), [IR](aot/arm64-loop.ll), [assembly](aot/arm64-loop.s). This is not native ARM qualification.
- [JSON IR inspection](aot/json-ir-inspection.json), [typed diff](aot/json_typed-normalized-ir.diff), [validated diff](aot/json_validated-normalized-ir.diff).
- [Incidents](aot/incidents.json), [original emission-scope mismatch](aot/broad-codegen.log), [original symbol-parser error](aot/fp-bit-oracle-initial.log).
- [Workload source mapping/oracles](aot/workloads.json), [FP oracle source](aot/fp-bit-oracle.void) / [independent native oracle](aot/fp-bit-oracle.cpp), [Unicode control source](aot/json-unicode-control.void).
- [Full original archive manifest](aot/manifest.json). The eighteen retained source fixtures are also copied here as evidence; the six approved test fixtures are staged in `tests/numeric`.

## Local archives and exclusions

Full original archives remain locally at:

- Research: `/home/shmellyorc/.local/state/void-log-performance/simd-gbc5btsk`.
- Rejected global trial: `/home/shmellyorc/.local/state/void-log-performance/simd-qualification`.
- Retained AOT-only trial: `/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7`.

These are original producer locations, not Git hyperlinks. Absolute paths inside
raw JSON/log/IR/disassembly are retained provenance, not instructions or links that
a clean checkout must provide. Bare artifact names in the historical reports refer
to their original phase/archive. This bundle commits only the subset enumerated
in its bundle manifest; each full archive manifest also identifies excluded artifacts.

Executable/object/archive binaries, caches, duplicate full command disassembly,
instrumented backend/compiler copies, SDK sources and unrelated experiment trees
remain local. No rejected global implementation is restored or staged as production
source. Original report bytes before publication-link/status updates are preserved
locally; the preparation receipt records their original/published hashes.

The local `.gitattributes` disables newline conversion only, preserving recorded
artifact digests across checkouts. CSV CRLF and literal whitespace in historical
IR-diff context are original data; source, fixtures and prose are checked separately.

Only Boss may commit, push and dispatch the seven-job qualification described in
[the staging review](../../AUTOMATIC_SIMD_AOT_STAGING.md).
