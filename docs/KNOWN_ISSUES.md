# Known issues and deferred work

Updated 10 October 2026, foundation phase.

## Platform verification

- Linux x86_64 on PikaOS 4 is the only executed platform. Windows, macOS and
  GitHub Actions have not run. Workflow configuration is not passing evidence.
- Work is local only. No repository was created, initialized, committed, pushed
  or published. The protected `.git` directory is not a usable Git checkout in
  this session. The user will create the public repository and push it.
- CI uses complete official release archives on all three hosts. Archive layout,
  distribution dependencies, Windows CRT ABI, Darwin SDK and runner availability
  still require actual hosted execution. Downloads use pinned version URLs over
  HTTPS; an independent archive checksum/signature verification step is future work.

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

## Recommended next development phase

Build a precise frontend slice around source management, indentation, function
signatures, primitive expressions, local bindings and source diagnostics. Lower
it into the existing IR and test identical AOT/JIT behavior. Formalize open
numeric and grammar rules before exposing them as language behavior. In parallel
with that phase's normal work, run the pending hosted-platform tests after the
user publishes the repository and resolve root lowering/thread coordination
before introducing managed language types. Keep the original VOIDC backup untouched.
