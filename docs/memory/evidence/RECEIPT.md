# io/memory — concise local test receipt

26 October 2026, Linux x86_64, LLVM/Clang/LLD 19.1.7. Independent Wrench review
and Boss-controlled cross-platform acceptance remain pending. No hosted CI ran.

| Configuration | Selected CTests | Failures/skips | Elapsed |
|---|---:|---:|---:|
| Debug | 21/21 passed | 0/0 | 209.08 s |
| Release | 21/21 passed | 0/0 | 109.66 s |
| RelWithDebInfo, strict ASan/UBSan/leaks | 19/19 passed | 0/0 | 164.88 s |

**61 distinct configuration/suite combinations.** Development repetitions and the
additional checks below are not added to this coverage count.

All configurations report:

* 268 numeric/IEEE vector cases; all ten types and both byte orders.
* 64 deterministic cursor-model transitions; byte/string/seek/EOF/Pascal fixture.
* 14 rejection cases; 35 check/JIT/O2/linked-build/native-execution processes.
* 13,922 ownership assertions; 749 failing allocation budgets per normal/O2 mode
  across 12 operations, with unchanged state, collection and successful retry.
* 156 memory LSP checks and zero module documentation warnings.
* Unchanged binary execution, ownership and LSP suites passed, including binary's
  356 generated cases (346 wire cases plus ten width sweeps).

Debug and Release each also passed seven memory-specific 1 MiB stack processes,
the unchanged binary stack suite and the Stage 11 stack gate. Each independently
passed the complete existing frontend 1 MiB guard/recovery script. An expanded
read-result independence fixture passed seven additional processes per native
configuration after the 21-suite runs; the implementation did not change. The full
sanitizer execution suite includes that expanded fixture. The unchanged Linux
Stage 11 stack gate also passed inside the 19-suite sanitizer selection.

Strict environment: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Tests ran through
approved execution outside the tracing sandbox so LSan could inspect threads.
No leak or undefined-behavior option was weakened. The SDK's RTTI-off ABI makes
vptr checking unavailable.

[Commands](commands.txt), [source hashes](source-sha256.json),
[protected file hashes](protected-sha256.json), [machine-readable results](verification.json),
[Debug details](debug-details.log), [Release details](release-details.log), and
[sanitizer details](sanitizers-details.log) retain exact outputs.
