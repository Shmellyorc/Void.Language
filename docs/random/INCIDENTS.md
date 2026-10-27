# Preserved engineering incidents

No observed random-engine SIGSEGV, native Windows exception or LLVM optimizer
crash is claimed. The following distinct failures are preserved, rather than
silently retried or removed. [Machine index and hash audit](evidence/incidents.json).

- `internal-validation/incident-5tv3becw`: graceful internal IR rejection when
  complex const-input slicing embedded readonly paths in physical array element
  definitions. The exact original sources and generator/output are retained.
  The slice type construction was corrected; the IR validator was kept strict.
- `crashes/incident-b439capv`: the new forged-worker negative test mixed types
  from separate analyzed programs. Its rejection was real, but occurred before
  the worker rule being tested. The test now obtains its entropy-result type from
  the same program and asserts the specific worker-entropy diagnostic.
- Nineteen additional wrapped-process incidents are reserved-status **86** exits
  from LeakSanitizer reporting it cannot inspect threads under the sandbox's
  ptrace restrictions. All nineteen retain exact source bytes, hashes, executable
  hash, command/cwd, streams and available generator/import snapshots. These are
  infrastructure failures, not evidence of a memory leak or a random algorithm
  fault. The initial strict CTest run was **4/31 passing, 27 failing**; its complete
  logs/JUnit also retain diagnostics from legacy direct C++ tests. Those direct
  legacy tests do not use the new Python source-preservation wrapper; their
  in-memory VOID snippets are not retroactively reconstructed or claimed as
  preserved source files.

A minimal strict check of the same sanitized compiler passed outside the sandbox,
with ASan/UBSan/leak detection and fatal exits retained. Only the 27 affected
checks were then rerun outside that sandbox. The original 31-test failed run is
kept separately from its infrastructure-repair receipt. Final results are indexed
in [the handoff](HANDOFF.md).

Signals, native Windows statuses and reserved exits 86/87 remain supported by the
shared wrapper. Timeout handling retains actual native termination status and
complete output before cleanup. Synthetic preservation tests deliberately trigger
and verify faults in disposable directories; those are not engineering incidents.
Historical filesystem/LLVM crash investigations were neither deleted nor changed.

`crashes/incident-s6lvo23q` is a further preserved **180-second harness timeout**
of the Debug optimized vector invocation before stdout appeared. Actual killed
status is **-9 / SIGKILL**, marked `timed_out` in the receipt. Identical vectors
passed other runs, including the 1 MiB invocation. The test's monolithic generated
main was partitioned into small concern functions with identical statements,
oracles and word consumption; the original fixture is retained as
`evidence/before-partition-random-test.void`, and the exact original generator is
also retained in the timeout incident's provenance. No sampler retry limit,
expected value or numerical test was relaxed. Final follow-up receipts distinguish
the partitioned fixture from earlier runs. A timeout is not labeled an LLVM crash.
