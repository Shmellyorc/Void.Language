# Stage 8 Sprint 2 local acceptance

20 October 2026. **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending.**
Stage 8 is not hosted locked. No hosted CI was run or requested; the seven-job
hosted gate is deferred until Stage 8 end unless Boss separately authorizes it.
Sprint 1 is owner-locally accepted; its Windows CI #82 repair is preserved.
Sprints 3–6 remain future work.

Contract: [ordinary calls and numeric prelude](../STAGE8_CALLS_NUMERIC.md).
Permanent evidence: [results](sprint2-results.json),
[frozen source SHA-256](sprint2-source-hashes.json),
[Debug frame measurements](sprint2-stack-frames.json). Raw local logs are retained
under `build/stage8-sprint2-acceptance/`; result JSON includes final CTest log hashes.
All three builds completed successfully with LLVM **19.1.7**. No source/test changes
occurred between the final frozen builds, verification and recorded hashes.

## Final consolidated local verification

| Configuration | Result | CTest elapsed |
| --- | --- | --- |
| Debug, `build/debug` | 143/143 passed; zero failures | 442.25 s |
| Release, `build/ci-release` | 143/143 passed; zero failures | 179.83 s |
| RelWithDebInfo strict ASan/UBSan/leaks, `build/ci-sanitizers` | 143/143 passed; zero failures | 375.82 s |

**429/429 suites passed.** Both separate **1 MiB frontend gates** passed.
Final acceptance supersedes earlier engineering runs; it does not count retries
as additional suites or infer execution on other operating systems.

Commands from the repository root:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure -j2
cmake --build build/ci-release -j4
ctest --test-dir build/ci-release --output-on-failure -j2
cmake --build build/ci-sanitizers -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
bash -c 'ulimit -s 1024; exec ./build/debug/void_tests frontend .'
bash -c 'ulimit -s 1024; exec ./build/ci-release/void_tests frontend .'
```

Strict sanitizer build/test execution ran outside ptrace after LeakSanitizer
rejected execution under tracing. Leak detection, assertions and halt-on-error
remained enabled. Compiler/Runtime instrumentation does not claim generated LLVM
application code is sanitizer-instrumented.

## Delivered behavior and focused evidence

- One initializer-derived argument mapper now binds ordinary named/default calls
  and generic inference. Tests cover written evaluation order, duplicate/unknown
  names, positional-prefix rules, required/default ordering, inferred annotations,
  declaration-module defaults, generic numeric defaults, methods, readonly effects,
  staged construction, async/spawn mapping and unchanged constructors.
- The ten built-in numeric types admit exactly min_value, max_value, abs and clamp.
  Type and receiver forms share one canonical private typed IR operation function.
  Exact typed extrema, signed minimum failure, inclusive/reversed bounds, NaN bounds
  validated before NaN input propagation, infinities and signed zeros are covered.
  Native f32 bit-pattern and APFloat f32/f64 kernel tests verify sign clearing,
  including NaNs. Shared LLVM lowering serves normal/O2 JIT and linked AOT.
- `numeric_sprint2` performs **71 process checks** across positive programs and
  diagnostic/runtime rejection cases. Compiler-negative checks require exit 1;
  a signal/crash cannot satisfy rejection. Native numeric tests additionally verify
  width-invalid bitcast rejection, canonical implementation deduplication,
  compatible body reload/old leases and rejection of name/default contract edits.
- `lsp_sprint2` covers compiler-owned type/receiver completion, chaining, hover,
  generic-default hover, bound/unbound and named signatures, canonical references,
  builtin readonly rename rejection/null definition and omission of fake symbols.
  Ordinary parameter labels navigate to source and participate in complete rename.
- Semantic-depth tests retain legal nominal/numeric chains through 124 calls and
  reject 400-call chains while recovering the later independent diagnostic. Cold
  numeric helpers reduce recursive Debug frame allocations; limits/reserves remain
  unchanged. Full frontend gates pass with the same 1 MiB stack budget in Debug
  and Release.

Initial engineering verification exposed an obsolete string-default rejection
and a Debug recursive-frame overflow. The string assertion now has positive
normal/O2 default/named materialization and a retained type-error negative;
outlined numeric-only helpers repaired the stack regression. Final consolidated
runs include both repairs and the final generic-default/tooling changes.

## Review boundaries and remaining verification

No unresolved Sprint 2 semantic decision remains: Boss approved the shared binding
rules, numeric prelude admission and exact edge policies in
[DECISIONS.md](../DECISIONS.md). Function-value calls retain positional, fully
supplied arguments; interface requirements retain their required-signature contract;
collection/event intrinsics retain their existing separate APIs. No overload
resolution, user primitive extensions or declared private/readonly fields ship here.

Wrench review remains pending. Linux pipe tests do not establish fresh interactive
VS Code/VSCodium/Neovim/Vim behavior or Windows/macOS execution. Windows Debug and
Release must still verify CI #82 short/long paths and reparse protections at the
deferred hosted gate. Original spelling, containment, Unicode, nested-project,
outside-workspace and alias protections remain intact; the repaired discovery
source and tests exactly match [CI #82 recorded hashes](ci82-repair-results.json).

Root README is unchanged. No commit, push, branch change, remote modification or
publication occurred. Stop here; Sprint 3 requires separate authorization.
