# Intermittent Debug optimizer failure

The initial consolidated Debug run failed `stage10_console_source` with SIGSEGV
inside LLVM 19.1.7 `CallBase::getMemoryEffects`, called by dead-store elimination.
A direct debugger reproduction stopped at the same point during the O2 compile
in `tests/stage10/console.cpp`'s `execution()` group. The generated module has no
runtime globals. Its LLVM verification had succeeded before optimization.

[Original backtrace](evidence/llvm-dse-backtrace.txt).
[Captured pre-optimization IR, gzip](evidence/console-preopt.ll.gz).
The uncompressed IR SHA256 is `402019ad2887f892d65f8effa2aa516342d02013d19e54835187d4b0d2fe59e0`.

The identical Debug executable and source later passed all **4,463** console
checks in an ordinary isolated invocation, without a production-code change.
Serializing the module at the optimizer breakpoint also changed reproducibility:
the previously failing optimization completed. The official pinned `opt` binary
successfully optimized the serialized input with `-passes=default<O2>`.
Release and strict ASan/UBSan console runs passed. An older pre-Stage-11 Debug
executable also passed 4,463 checks using the same current standard sources.

These observations narrow the failure but do not establish its root cause.
In particular, a passing serialized reproduction does not prove the in-process
IR/analysis state was sound. The default LLVM O2 pipeline remains unchanged.
The initial failure is retained as an **unresolved reliability finding for
Golden Wrench/Boss review**, separately from subsequent passing executions.

## Reproduction and inspection

```sh
build/step42-ci96-debug/void_console_source
gdb -q -batch -ex run -ex 'thread apply all bt 22' --args build/step42-ci96-debug/void_console_source
gzip -dc docs/stage11/evidence/console-preopt.ll.gz > /tmp/console-preopt.ll
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/opt '-passes=default<O2>' -disable-output /tmp/console-preopt.ll
```

The source/test file hashes and exact consolidated results are in the
[source freeze](SOURCE_FREEZE.json) and [local record](ACCEPTANCE.md). A retry is
recorded as a retry, not as a clean initial consolidated run.
