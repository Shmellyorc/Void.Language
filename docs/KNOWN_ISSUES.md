# Known issues and deferred work

Updated 10 October 2026, first real source compiler phase.

## Platform verification

- The user confirmed the foundation now passes all three hosted platforms.
  Earlier failed-run descriptions in WORK_LOG.md are historical evidence.
- This frontend phase is locally verified on Linux with exact LLVM 19.1.7,
  including the complete official Linux archive. Windows/macOS execution of the
  expanded eight-suite Debug/Release matrix still requires a hosted run after
  the user commits and pushes. Linux hosted ASan/UBSan also needs that run.
- CI retains complete official LLVM 19.1.7 archives, Linux LLD and required
  development dependencies, Windows SDK/DIA discovery and native printf support,
  and the portable thread implementation. All new suites run automatically in
  the existing workflow; none are skipped by platform.
- Pinned archive URLs lack independent checksum/signature verification, which
  remains future supply-chain work.

## Compiler scope

- Actual single-file `.void` compilation now works. See SOURCE_COMPILER.md for
  supported grammar and deliberately provisional choices. There is no module
  resolver, import discovery, VFS/canonical module identity, LSP, package manager,
  source watcher or incremental compilation yet.
- Supported value types are i32/bool. Strings, floats, const declarations,
  collections/ranges, structs, generics, closures, async, named/default arguments,
  multiple return values and full managed types are not implemented. Unsupported
  syntax is rejected rather than simulated.
- UTF-8 source validation and scalar columns work; identifiers currently use
  lowercase ASCII/underscore/digits. Unicode identifier policy remains open.
  Multiline expressions, visual-width carets and non-ASCII Windows filesystem
  paths have not been established as supported behavior.
- The IR supports typed multiple parameters, no-result functions and typed local
  slots across CFG blocks. Instruction results remain block-local, with temporary
  slots carrying values across blocks; LLVM optimizes those slots. General SSA
  phi construction is not required for this slice.
- Numeric overflow policy is provisional: current i32 operations wrap, signed
  division truncates toward zero, and dynamic zero divisors trap. There are no
  structured runtime arithmetic errors or resource limits for recursive source
  programs/parser nesting. Broader checked arithmetic decisions remain open.
- Diagnostics report the first error with source location/line/caret. Recovery,
  stable error codes, ranges, variable debug information, optimized source stack
  traces and translation of internal/backend failures remain future work.
- AOT emits a host-native object with a simple i32 entry adapter; JIT uses the
  same host ABI. General executable/project compilation, arbitrary entry ABIs,
  cross-compilation, export/mangling policy, linker orchestration, deployment and
  source-level native bindings remain deferred.

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

Run the expanded hosted matrix after the user publishes these local changes.
Then build module/import identity and multi-file source diagnostics around this
frontend; resolve numeric and executable/export conventions deliberately.
Preserve one semantic model and shared IR/LLVM lowering. Resolve managed root
lowering/thread coordination before adding managed language types. Keep the
original VOIDC backup untouched.
