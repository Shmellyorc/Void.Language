# Stage 8 Sprint 3 local acceptance

20 October 2026. **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending.**
Stage 8 is not hosted locked. The final seven-job hosted gate remains deferred
until Stage 8 end unless Boss explicitly authorizes it. No hosted CI was run or
requested. Starting commit: `483ef87c3ec62c29c2bf1000529b480f0ba5ef2d`.

Contract: [UTF-8 text and recoverable collections](../STAGE8_TEXT_COLLECTIONS.md).
Evidence: [results](sprint3-results.json), [source/test SHA-256](sprint3-source-hashes.json),
[Debug stack frames](sprint3-stack-frames.json), [traversal samples](sprint3-traversal.json).
Raw logs are retained under `build/stage8-sprint3-acceptance/`.

## Executed local verification

| Configuration | Consolidated run | Repair / final unique outcome | Time |
| --- | --- | --- | --- |
| Debug | 145/145; zero failures | 145/145 | 524.29 s |
| Release | 144/145; one obsolete expected diagnostic | arrays_foundation focused rerun 1/1; final 145/145 | 222.01 s + 0.05 s |
| RelWithDebInfo, strict ASan/UBSan/leaks | 145/145; zero failures | 145/145 | 471.50 s |

**435/435 final unique configuration/suite outcomes pass.** Release did not have
a single all-green full run: its original failure is retained and the repaired
suite is counted once. No production source changed for that assertion repair.
Debug and strict runs used the repaired assertion. Both full **1 MiB frontend
gates** passed. LLVM is **19.1.7**; Windows/macOS execution is not inferred.

The obsolete arrays_foundation negative expected string indexing to be unsupported.
Sprint 3 correctly returns string, so an i32-returning function instead gets
VOID2001 type mismatch. Retained that negative with the precise diagnostic and
added a valid string-returning scalar-index positive. The focused Release rerun
passed 1/1 in 0.05 seconds. All other full-run suites already passed; no unrelated
repeat is counted as new coverage.

Commands from the repository root:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure -j2
cmake --build build/ci-release -j4
ctest --test-dir build/ci-release --output-on-failure -j2
cmake --build build/ci-release --target void_arrays_foundation -j4
ctest --test-dir build/ci-release --output-on-failure -R '^arrays_foundation$'
cmake --build build/ci-sanitizers -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
bash -c 'ulimit -s 1024; exec ./build/debug/void_tests frontend .'
bash -c 'ulimit -s 1024; exec ./build/ci-release/void_tests frontend .'
```

Strict build/tests ran outside ptrace so LeakSanitizer could operate. Assertions,
leak detection and halt-on-error stayed enabled. Compiler/Runtime instrumentation
does not claim generated LLVM application code is sanitizer-instrumented.

## Delivered behavior and focused evidence

- Scalar/i64 length, byte_length and owned scalar indexing support empty/ASCII/
  multibyte/combining/NUL text, every integer width and negative indexing. Bad
  indexes produce checked source failures in both JIT modes and linked AOT.
- Owned UTF-8 encode/decode preserves embedded NUL, readonly inputs and copy
  independence. Malformed encodings return the approved string error; overlong,
  surrogate, out-of-range, stray-continuation and truncated encodings are covered.
- List/fixed-array get, dictionary get and list pop compose existing operations
  and results. Tests cover integer extremes, empty/missing/present/null values,
  generic wrappers, named arguments, receiver/index order, fixed child copies,
  shared referents, deep readonly rejection and iterator invalidation. Captured
  native list headers show empty pop keeps version 0 and push/successful pop gives 2.
- `text_collections_sprint3` passes **106 process checks** across normal/O2 JIT,
  independently linked AOT and diagnostics. Negative compiler/process checks require
  exact exit 1, so a crash cannot count as rejection. `lsp_sprint3` covers compiler
  completion/hover/signatures/references, readonly builtin navigation/rename and
  ordinary source rename with builtin records present. No editor contains a catalog.
- Extended strings_source covers forced GC, exact output/root preservation under
  **31 encode and 14 decode allocation failures**, four independent text-IR flag
  rejections, **70 sampled source allocation failures per optimization mode**,
  cleanup/retry and compatible body reload while retaining the old lease. These
  counts are focused executions inside existing suites, not extra CTest suites.
- Repeated traversal samples retain linear scans without an index cache. Debug
  cold-helper repair reduces recursive frames without changing depth/stack guards;
  nominal/numeric/string chains through 124 calls pass and 400-call chains reject
  while recovering later diagnostics. Both complete frontend small-stack gates pass.

## Preserved failure evidence and remaining limits

The initial Debug depth SIGSEGV and measured frame repair are retained. Early
engineering fixture issues (numeric suffix spelling, missing include, AOT Runtime
link inputs, stdout/stderr expectations and allocation-probe reservation/budget)
were repaired without relaxing product checks. Release's obsolete diagnostic
assertion is preserved separately from the production stack fix.

These are runtime APIs; compile-time text/collection query execution and builtin
method values are not added. The existing conservative worker checker rejects
synthetic method callees through spawn with VOID1103, as the retained focused
probe confirms. This sprint does not expand worker admission, borrowed views,
normalization/graphemes, interpolation, generic enums or collection search/hash APIs.

Wrench review, fresh actual editor-client acceptance and Windows/macOS execution
remain pending. Linux LSP pipe checks do not establish interactive client behavior.
Sprint 1's [CI #82 discovery source/test hashes](ci82-repair-results.json) are
unchanged; Windows Debug and Release short/long paths and reparse checks remain
pending at the deferred hosted gate. Sprint 2 contracts remain covered.

Root README is unchanged. No commits, pushes, branch/remote changes, website work
or publication occurred. Sprints 4–6 remain future work. Stop for Wrench review.
