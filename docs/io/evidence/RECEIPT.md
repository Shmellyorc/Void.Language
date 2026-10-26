# Local receipt — io and path

26 October 2026. Linux x86-64, pinned Clang/LLVM/LLD 19.1.7. Baseline
`084fe1ebf26e3f6376ed3e569e1b6c1a74499b17`. Stopped for Golden Wrench review;
this is not a final acceptance lock. No hosted CI or Git mutation.

| Final consolidated run | Result | Elapsed | Evidence |
| --- | --- | --- | --- |
| Debug | 63/63 passed | 1,020.90 s | [summary](debug-final-summary.txt), [full](debug-final.txt), [JUnit](debug-final.xml) |
| Release | 63/63 passed | 688.54 s | [summary](release-final-summary.txt), [full](release-final.txt), [JUnit](release-final.xml) |
| Strict sanitizers | 56/56 passed | 866.81 s | [summary](sanitizers-final-summary.txt), [full](sanitizers-final.txt), [JUnit](sanitizers-final.xml) |

**182/182 selected tests passed; zero skipped/disabled cases.**
[Machine-readable totals](results.json). These are focused local selections, not
the entire CTest or hosted cross-platform matrix.

Each selection includes actual source JIT/O2, independently linked AOT, all ten
numeric primitives and both byte orders, Unicode/NUL/strict UTF-8, files/directories/
paths, actual cross-device rename failures, installed/relocated stdlib examples,
native short/zero/partial transfers and cleanup errors, aliases, pending close,
cancellation, shutdown during opening and whole-file work, LSP docs/recovery and
existing binary/memory/combined/console/convert/time/globals/async/native regressions.

* Ownership: **149 public declarations documented**, zero warnings; **7,213**
  checks per configuration. Allocation sweeps recover **437 sync + 622 async**
  injected failures in each JIT/O2 mode; unreachable roots collect and frames balance.
* Wire oracle: **42 numeric vectors**, both byte orders; independent Python bytes
  include signed boundaries, IEEE zero/subnormal/infinity/NaN and Pascal framing.
* Real-file responsiveness runs JIT/O2 and both linked AOT modes with one worker.
  A second owner activation runs while the actual native read is held.
* New I/O/LSP 1 MiB cases execute in Debug/Release; existing constant recovery and
  global-inference stack guards are included where registered. Strict runs retain
  their registered stack policy; no sanitizer is disabled.
* [API signatures](API.tsv), [per-declaration test mapping](API-AUDIT.tsv),
  [source freeze](source-freeze.json). All **53** changed source/build/test hashes
  and **seven** protected baseline hashes were verified after execution.

Strict environment: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Runs used the
non-sandboxed process permission because LeakSanitizer cannot operate under the
sandbox's ptrace restriction. The existing RTTI-off LLVM ABI makes vptr checking
unavailable. CMake configurations are `build/ci-official-debug`, `release` and
`sanitizers`; the strict build uses `VOID_SANITIZERS=ON` and RelWithDebInfo.

The final CTest selection expression is retained in each full log's test names and
the JUnit cases. It selects `io_*`, `lsp_io*`, binary/memory/combined families,
constant/global recovery and stack/AOT cases, async source/IR/parallel/lifecycle,
console/hooks, temporal/zones, Runtime coordination/callback/failure/invariant and
native/Stage 6 integration tests.

Earlier failures are **not erased or counted as passes**. Allocation budget 580
exposed a close-reservation gap, and an entered-open proof exposed premature
shutdown; both have permanent regressions and recorded repairs. An initial Release
async wire invocation exited with SIGSEGV. Two debugger attempts passed and no
failing stack was captured: its cause remains **unclassified and unresolved**.
See [the full handoff](../HANDOFF.md) and `release-initial-io*`,
`release-gdb-attempt-*`, `debug-allocation-close-*` and
`debug-opening-shutdown-reproducer.txt`.

Windows/macOS native execution is **unverified**. Namespace snapshots are not an
atomic/security containment boundary against concurrent ancestor replacement.
The handoff documents platform prerequisites, resource lifetime, cancellation,
partial mutation and flushing limits. These limits are not passing native tests.
