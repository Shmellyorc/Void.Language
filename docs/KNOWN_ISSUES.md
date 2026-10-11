# Known issues and deferred work

Updated 10 October 2026, foundation phase.

## Platform verification

- In [hosted run 38099707063](https://github.com/Shmellyorc/Void.Language/actions/runs/38099707063),
  Linux passed Debug, Release and ASan/UBSan, and macOS passed Debug and Release.
  Windows built Debug and passed core/JIT/reload/GC/native (5/6); AOT failed in
  Clang's input-file validation for bare `legacy_stdio_definitions.lib`.
- The AOT script now requests `-llegacy_stdio_definitions`, allowing the native
  Windows linker to search the active MSVC/Windows SDK environment. The existing
  `printf` call, debug metadata verification and three executable outputs remain.
  This local correction is uncommitted/unpushed and needs hosted Windows execution.
- Windows Release configure/build/tests were skipped after the Debug test failure;
  they must also pass in the next run. Linux/macOS should remain green.
- Local Clang/LLD 19.1.7 checks reproduced the input-file error and verified real
  COFF library discovery through `LIB`, including a path with spaces and failure
  when that search path was removed. They do not replace Windows SDK/runtime tests.
- CI uses complete official LLVM 19.1.7 archives. The Linux archive contains
  bitcode and requires the matching LLD, plus an SDK-compatible RTTI setting.
  CMake now links and runs an ORC host before the normal build. Archive downloads
  use pinned HTTPS URLs; independent checksum/signature verification remains
  future work.

## Compiler scope

- `voidc` is an IR architecture harness, not a source language compiler yet.
  There is no lexer/parser, module resolver, full type checker, import discovery,
  LSP, package manager or source watcher. The example `.void` file is a design
  equivalent of hand-built IR, not compiled input.
- The proof IR supports only one i32 parameter/result, block-local values,
  arithmetic, comparisons, calls, branches and returns. No full language
  overflow, Unicode identifiers or indentation rules have been finalized.
- Source diagnostics use byte columns. Debug metadata is deliberately basic;
  there is no variable debug information, optimized source stack trace or
  backend-error translation suitable for application developers.
- AOT emits a native-host object with a proof-specific `main` wrapper.
  General executable/project compilation, cross-compilation, linker discovery,
  deployment and arbitrary native library loading are not implemented.

## Managed memory

- The heap is a single-threaded experimental non-moving mark/sweep collector
  with explicit roots, copied layouts and iterative tracing. It is not a
  production managed runtime. No thread registry, world-stop protocol, allocator
  caches, weak references, barriers, async roots or finalization exists.
- Generated roots have been exercised through LLVM O2 calls and loops. They are
  explicitly registered stack slots, not discovered stack/register roots.
  LLVM statepoint lowering, stack-map decoding and real multi-thread safepoints
  remain unresolved. Their integration gate is not complete.
- Root registration requires balanced normal-exit unregistering. Generated unwind
  cleanup and native exception translation are absent. Native callbacks used by
  these proofs must not throw across generated code.
- Objects have max-align allocation and whole-object references only. Over-aligned
  types and interior pointers are not supported. Descriptors are trusted compiler
  metadata; the prototype validates offsets but does not infer language layouts.
- ASan/UBSan instrument the C++ compiler, runtime and C fixture. The LLVM dependency
  and LLVM-generated application instructions are not sanitizer-instrumented.
  Generated-code root correctness is verified by explicit observable checks.

## Reload and native boundaries

- Replacement compatibility metadata is supplied by the host. A future frontend
  must compute dependency fingerprints for layouts, constants and captures.
- A single slot publishes an entire module generation. There is no atomic
  transaction across independently published slots, file monitoring, incremental
  recompilation, suspended-task tracking or capture-layout migration.
- A mutex/shared ownership lease proves safe synchronous retirement. Dispatch
  overhead and scalability have not been benchmarked. ORC retirement errors are
  reported to stderr, not recovered by a production development host.
- Trusted symbol addresses assume matching C ABI and host process lifetime.
  Foreign-thread callbacks, callback/unregister races, native dynamic-library
  ownership and asynchronous native calls remain unsupported.
- The UTF-8 helper implements borrowing of terminated UTF-8 with a temporary
  owned copy. Retained string storage, returned buffers, UTF-16 and native binding
  metadata syntax are not implemented. NUL rejection is specific to this bridge,
  not a decision to forbid NUL in VOID strings.

## Toolchain observations

- LLVM 19 headers include `<ciso646>`, and this host's GCC 16 standard-library
  header emits a C++20 deprecation warning. It originates in third-party headers;
  project warnings are not globally suppressed to hide it.
- LLVM configuration reports optional CURL missing. Selected components build and
  run without it. Required exported dependencies, including zstd, are intact.
- The initial sandboxed sanitizer execution failed at LeakSanitizer process
  inspection. Running outside the sandbox tracing restriction passed with leak
  detection enabled. This is a host execution constraint, not a skipped leak check.

## Current phase and subsequent work

Stabilize hosted CI before expanding the language frontend. All three runners
must build and execute Debug and Release, with Linux ASan/UBSan, using LLVM
19.1.7 and the existing architectural tests. See WORK_LOG.md for local evidence.

After that gate, build a precise frontend slice around source management,
indentation, function signatures, primitive expressions, local bindings and
source diagnostics. Formalize open numeric/grammar rules first, and resolve
root lowering/thread coordination before adding managed language types. Keep
the original VOIDC backup untouched.
