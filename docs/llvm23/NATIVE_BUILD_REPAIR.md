# CI 40844952491: UTF-8 fixtures and Apple entropy declaration

2026-10-29. Boss-authorized continuation against clean baseline
`cb54dcc40cf2da712ab480bd7dbc4b49e898df1b`. Source changes are complete and stop
for Golden Wrench source review. No commits, staging, pushes, branches or hosted
CI actions were performed. Native acceptance of these new changes is pending.

## Original native failures

[Compiler link experiment run 40844952491](https://github.com/Shmellyorc/Void.Language/actions/runs/40844952491)
used the `thinlto-cache` variant. All four Windows/macOS jobs passed configuration
and their ORC compile/link/run check. Both macOS configurations also passed the
executed C++20 stop-token check. These are actual hosted configuration results
for the baseline; they do not establish completed native builds or test suites.

| Platform/configuration | Job | Build failure |
|---|---|---|
| Windows Debug | `123350061570` | Generating `io-wire.void`: cp1252 cannot encode `雪` (`U+96EA`) |
| Windows Release | `123350061540` | Same fixture generation error |
| macOS ARM64 Debug | `123350061666` | `src/runtime/random.cpp:24`: undeclared `getentropy` |
| macOS ARM64 Release | `123350061685` | Same missing declaration |

Original configure success lines and build errors are retained in
[`evidence/ci40844952491/`](evidence/ci40844952491/).

## Exact corrections and preserved behavior

`tests/io/execution.py`: add `encoding='utf-8'` to every text source write and
the corresponding source read: five write call sites and one read call site.
This covers `--generate-async`, reading `errors.void` and writing
`--generate-errors-async`, the synchronous execution fixture, the async execution
fixture, and all three negative fixtures written through the final loop.
No process-wide UTF-8 environment setting is required by the fix.

All Unicode literals, numeric vectors, endian framing, binary reads, expected
wire bytes, exit-code assertions and failure checks remain unchanged. An AST
comparison against the baseline, excluding only the six encoding arguments,
confirms the rest of the Python file is identical.

`src/runtime/random.cpp`: replace the Apple branch's `<unistd.h>` include with
`<sys/random.h>`. [Apple's public system header](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/random.h)
declares `int getentropy(void*, size_t)` there, with macOS availability from
10.12. The entropy function body is byte-identical to the baseline: the existing
eight-byte request, `errno` handling, Native-safe coordination, seed assembly,
Linux `getrandom` loop and Windows `BCryptGenRandom` path are preserved. No new
entropy provider, fallback, library or framework is introduced.

The protected root README, Windows `/MT` dependency setup, Apple vendor libc++
pairing, native-link manifest generation, stop-token/ORC checks, frontend/IR,
Runtime ownership/GC/leases and LLVM 23.1.4 O2 pipeline remain unchanged.
Exact unchanged-file hashes and source audits are in
[`source-audit.json`](evidence/ci40844952491/source-audit.json).

## Focused local verification

Host: Linux x86-64; existing `build/llvm23-release`, official Clang/LLVM/LLD
23.1.4, Unix Makefiles. No native macOS or Windows execution was available.

```sh
cmake --build build/llvm23-release --target \
  voidc void_io_runner void_random_host void_io_wire_normal \
  void_io_wire_optimized void_io_error_fixture void_stop_token_probe --parallel 3
PYTHONUTF8=0 python3 docs/llvm23/evidence/ci40844952491/encoding_probe.py build/llvm23-release
ctest --test-dir build/llvm23-release \
  -R '^(toolchain_stop_token|io_execution|io_async_execution|io_wire_normal|io_wire_optimized|io_errors_source|io_errors_async|random_host|random_execution|random_errors|random_worker)$' \
  --output-on-failure -j 2
python3 -m py_compile tests/io/execution.py docs/llvm23/evidence/ci40844952491/encoding_probe.py
git diff --check
```

Results:

- **Build passed**, including the changed Runtime source and regenerated async
  wire/error fixtures. Regeneration re-executed the actual ORC check: **pass**.
- **11/11 focused CTests passed**, 52.45 seconds. `random_host` retains its Linux
  entropy fault coverage: EINTR, short reads, zero reads, OS failure, no partial
  seed publication, retry/default initialization and concurrent GC during the
  Native-safe region. These tests exercise Linux's provider, not Apple's API.
- The baseline generator **reproduced `UnicodeEncodeError`** when default VOID
  text opens used cp1252. After the fix, the same default emulation runs both
  generation modes, synchronous normal/O2 JIT and linked AOT, async JIT/O2,
  both byte-order wire oracles, and all three negative source fixtures.
  Generated files equal the expected UTF-8 bytes and retain `雪`. All **eight
  dynamic text accesses** explicitly request UTF-8: seven writes and one read.
  This is file-encoding emulation on Linux, not a native Windows pass.
- The verification helper's first access-count assertion incorrectly expected
  nine operations after successful fixture execution. The correct count is eight
  (the negative loop performs three writes). The helper was corrected and rerun;
  initial and passing outputs are retained. Production test assertions were not
  changed.
- [`entropy_smoke.void`](evidence/ci40844952491/entropy_smoke.void) uses the actual
  OS entropy bridge. Source checking, normal/O2 JIT, and both independently built
  native executables **passed all seven commands** on Linux. Exact commands,
  exit codes and output are in `entropy-smoke.json`.
- Python syntax checks, source-preservation audit and `git diff --check`: **pass**.

Build, CTest details, encoding reproduction and final probe output are retained
as text/JSON beside the small reproducible verification helper and entropy
fixture. No fake Apple header/provider or undefined-symbol suppression was used.

## Remaining owner-controlled native verification

After source review, Boss should run the reviewed source through the existing
Windows x64 and macOS ARM64 Debug/Release jobs, preserving LLVM 23.1.4 and all
previous configuration settings. No CI run was initiated by this repair.

1. Require the existing Windows ORC and macOS stop-token/ORC configuration checks
   to remain successful. Preserve `/MT`, dependency resolution and Apple libc++
   header/runtime pairing.
2. Require the complete native build to finish. On Windows, generation of both
   `io-wire.void` and `io-errors-async.void` must work with Python's ordinary
   default encoding, without relying on `PYTHONUTF8=1`. Decode the generated
   sources as UTF-8 and retain the Unicode fixture. On macOS, compile/link the
   real `src/runtime/random.cpp` against the selected Apple SDK.
3. Run the same focused CTest selection above against each native build directory,
   followed by the complete existing suite. Preserve all I/O byte and failure
   assertions and ordinary random behavior tests.
4. On macOS, run the entropy smoke fixture through normal/O2 JIT and linked AOT
   using the reviewed native `voidc`; this exercises actual `getentropy`. For
   example, for each build directory:

   ```sh
   build/Release/voidc check docs/llvm23/evidence/ci40844952491/entropy_smoke.void
   build/Release/voidc run docs/llvm23/evidence/ci40844952491/entropy_smoke.void --no-opt
   build/Release/voidc run docs/llvm23/evidence/ci40844952491/entropy_smoke.void
   build/Release/voidc build docs/llvm23/evidence/ci40844952491/entropy_smoke.void -o build/Release/entropy-smoke --no-opt
   build/Release/entropy-smoke
   build/Release/voidc build docs/llvm23/evidence/ci40844952491/entropy_smoke.void -o build/Release/entropy-smoke
   build/Release/entropy-smoke
   ```

The source/header correction is supported by Apple's declaration and the
observed CI error. Only future native runs can confirm the repaired Windows
generation path, Apple compilation and completed native suites.
