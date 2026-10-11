# Known issues and deferred work

Updated 11 October 2026, Phase 5 structured diagnostics and recovery.

## Platform verification

- The user confirmed the source compiler and subsequent ten-suite module phase
  passed all three hosted platforms, including Debug/Release and Linux sanitizers. Earlier failed-run descriptions
  in WORK_LOG.md are historical evidence.
- The user reports Phase 4 commit `d78b6b8`, Actions run #6, passed all three
  platforms. Phase 5 has local Linux verification with exact LLVM 19.1.7.
  Windows/macOS execution of this expanded ten-suite Debug/Release matrix still
  requires a hosted run after the user commits and pushes. Linux hosted
  ASan/UBSan also needs that run. See WORK_LOG.md for final local evidence.
- CI retains complete official LLVM 19.1.7 archives, Linux LLD and required
  development dependencies, Windows SDK/DIA discovery and native printf support,
  and the portable thread implementation. All new suites run automatically in
  the existing workflow; none are skipped by platform. Linux sanitizers now halt
  on reports with distinct failure codes, preserving leak detection.
- Pinned archive URLs lack independent checksum/signature verification, which
  remains future supply-chain work.

## Compiler scope

- Actual single- and multi-file `.void` compilation now works. SOURCE_COMPILER.md
  and MODULE_SYSTEM.md describe the implemented grammar, resolver and provisional
  choices. There is no LSP, package manager, source watcher or incremental compiler.
- Canonical VFS/function identities are project-scoped. Persistent identity across
  independent projects/libraries, external-import permissions and installed package
  resolution are deferred. No root override or automatic entry discovery exists;
  the selected entry file's parent is the root.
- Module filenames currently use a portable printable-ASCII subset with exact-case
  checking, excluding Windows-reserved names and trailing dots/spaces. Unicode
  filename/case normalization remains open. Entry/imported symlinks and redirected
  VFS components are rejected. Distinct hard-link filenames to one source in the
  graph are rejected. This resolver is not a sandbox for concurrent filesystem
  mutation. Physical-root aliases are canonicalized before selecting VFS identities.
- Repeated identical imports are idempotent. Explicit aliases expose qualified
  calls only; direct unaliased imports expose simple names. Implicit filename
  qualifiers can be ambiguous, and imports do not transitively reexport names.
  These scope choices are documented rather than implying a complete export model.
- Function-only cycles work through signature-first analysis. Module/project-wide
  constants now use a separate compile-time dependency walk, with structural cycles
  rejected. Runtime module variables/initialization remain absent. Internal
  visibility currently operates inside one compiled project;
  external library export/visibility boundaries remain deferred.
- Supported value types are i32/bool. Strings, floats,
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
  division truncates toward zero, and dynamic zero divisors report source-first
  runtime errors. Broader numeric conversion/overflow decisions remain open.
  Compiler recovery now bounds parser/semantic/dependency/import recursion; see
  DIAGNOSTICS.md. Application execution recursion remains unbounded.
- Diagnostics now collect independent errors with codes, byte ranges, immutable
  snapshots and related declarations. DIAGNOSTICS.md documents recovery boundaries,
  the 100-diagnostic budget and conservative suppression when a direct dependency
  fails. Lexically unreadable declarations can still cause later unresolved-name
  errors. Successful-compilation warning results, variable debug information,
  foreign/native stack traces and translation of internal/backend failures remain
  future work. Arithmetic failures now propagate
  actual source caller locations even under O2, bounded to 32 callers with explicit
  truncation. AOT diagnostics currently print to stdout, while JIT CLI uses stderr.
  This boundary is not general language exceptions or panic/defer cleanup.
- AOT emits a host-native object with a simple i32 entry adapter; JIT uses the
  same host ABI. General executable/project compilation, arbitrary entry ABIs,
  cross-compilation, public library/export ABI, linker orchestration, deployment and
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

- Layout/capture compatibility metadata is supplied by the host. The frontend now
  supplies an exact constant restart token covering complete snapshots of every
  const-containing file. It conservatively rejects unrelated edits in those files
  too. Precise AST/dependency compatibility remains future work.
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
- One concurrent complete-official-SDK LLD link printed a Machine Instruction
  Scheduler backtrace from LLVMGlobalISel while reporting build exit status 0.
  This observation must not be treated as clean merely because the build succeeded.
  A final serial build with the same SDK completed without another backtrace and
  passed all ten suites. Retain the observation when checking the next hosted Linux
  build; its cause is unconfirmed. See WORK_LOG.md.
- LLVM configuration reports optional CURL missing. Selected components build and
  run without it. Required exported dependencies, including zstd, are intact.
- The initial sandboxed sanitizer execution failed at LeakSanitizer process
  inspection. Running outside the sandbox tracing restriction passed with leak
  detection enabled. This is a host execution constraint, not a skipped leak check.

## Current phase and subsequent work

Run the expanded ten-suite hosted matrix after the user publishes these local
changes. See VALUE_SEMANTICS.md for implemented constants, numeric semantics,
runtime diagnostics and explicit limitations. Next refine validated source/dependency compatibility and analysis-result APIs; approve contextual literal/conversion
details before numeric type expansion. Preserve the shared semantic model and
IR/LLVM implementation. Resolve managed root lowering/thread coordination before
managed types. Keep the original VOIDC backup untouched.
