# Known issues and deferred work

Updated 12 October 2026, Phase 12 value struct foundations.

Phase 11 run #16 at `1b32151` passed Linux, Windows and macOS according to the
project owner. Phase 12 local results are recorded in WORK_LOG.md; hosted
Windows/macOS verification remains required.

Numeric/bool/acyclic nested value structs now work; see VALUE_STRUCTS.md. Runtime
field defaults, custom init, positional constructors, field modifiers, aggregate
equality and external aggregate ABI bindings remain deferred. Layout is compiler
private, target-aligned and conservative under hot reload. Resource bounds are
128 layout levels and 65536 expanded fields per type. Managed structs and the full
runtime remain outside this phase.

## Platform verification

- The user confirmed the source compiler and subsequent ten-suite module phase
  passed all three hosted platforms, including Debug/Release and Linux sanitizers. Earlier failed-run descriptions
  in WORK_LOG.md are historical evidence.
- The user reports Phase 4 commit `d78b6b8`, Actions run #6, passed all three
  platforms. Phase 5 run #7 passed Windows/macOS and Ubuntu Debug/Release 10/10,
  but Ubuntu sanitizers failed eight suites. The actual job logs and unchanged
  local official-archive build reproduce mixed RTTI metadata in C++ template
  ownership/future cleanup. The local correction matches SDK RTTI throughout
  the linked program. The user reports commit `4f3688d` passed run #8 on all
  platforms; those earlier failures are resolved. The user reports Phase 6 commit `9b9fdac` passed run #9 on all three platforms. See CI_SANITIZER_ABI.md and
  WORK_LOG.md for evidence and validation.
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
  choices. LSP and a local editor client now exist; package management, source watching and
  incremental compilation remain absent.
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
- Supported value types are all ten exact-width numeric types, bool and acyclic
  numeric/bool value structs. Strings, collections/ranges, managed structs,
  generics, closures, async, named/default function arguments,
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
- Phase 6 implements the explicitly approved numeric contract in NUMERIC_TYPES.md:
  wrapping integers, IEEE floats, contextual literals and checked numeric as.
  Exponent/base/separator syntax, float remainder, nullable as?, platform-sized
  aliases, hashing and arbitrary public native numeric ABIs remain deferred.
  Native bindings that change host rounding/flush-to-zero state need a future
  environment boundary contract; current hosts use the standard IEEE environment.
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
  supplies exact constant declaration tokens and validated dependency contracts.
  Ordinary body/trivia edits beside unchanged constants can reload. Declaration
  and reference ordering remain conservative restart conditions; see
  ANALYSIS_COMPATIBILITY.md.
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

- The official Linux LLVM 19.1.7 archive has RTTI disabled, whereas the installed
  SDK used for earlier local sanitizer tests enables RTTI. Clang vptr checking
  cannot run in the archive's no-RTTI ABI. ASan, supported UBSan checks, leak
  detection and all ten suites remain enabled. The separate RTTI-enabled installed
  SDK retains local vptr coverage; hosted vptr coverage would need a complete
  RTTI-enabled 19.1.7 SDK. CMake reports the selected ABI and coverage limit.
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

The user reports Phase 7 commit `04768bf` passed hosted run #10 on Linux, Windows
and macOS. Phase 8 adds tooling/tooling_demo while preserving all fourteen prior
suites. TOOLING_FOUNDATIONS.md describes source byte-offset query boundaries and
conservative invalidation. Mutable locals, member completion and expression
hover records are absent. Changed files seed all declarations; function-body
extents are not collected. Snapshot comparison cannot certify later edits, and
old/invalid/missing graphs cannot authorize publication. There is no incremental
compiler, watcher, LSP transport or persistent cache.

Actual local validation is in WORK_LOG.md. Hosted Phase 8 verification requires
the user's push; no new hosted result is claimed. Next establish explicit client
revision handling and invalidation orchestration before partial compilation/LSP.
Managed types still need the GC/root/thread gate; external packages need their
identity policy. Keep the original VOIDC backup untouched.


## Phase 9 document revisions and tooling sessions

The user reports Phase 8 commit `f25b268` passed hosted run #11 on all platforms.
[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) defines owned whole-document overlays,
strict per-document logical revisions, project identity tokens, explicit close versus
unavailable transitions, complete analysis selection and stale-result acceptance.
Current queries never fall back to historical success after edits/failures. Dependency
orchestration reuses Phase 8 and the exact Phase 7 compiler contract; native
publication remains separate. Disk fallbacks are reread and checked during analysis,
but are not an atomic filesystem image; clients explicitly refresh for observed
disk changes. All sixteen preceding suites remain, with sessions/sessions_demo
bringing the total to eighteen. Actual local results are recorded in WORK_LOG.md.
Hosted Phase 9 verification remains pending. No LSP, watcher, worker, persistent
cache, partial compiler, new language feature or automatic native reload is added.
Next consider protocol position adaptation and a minimal tooling client/transport
before partial compilation; retain the separate managed-runtime and package gates.


## Phase 10 language server boundaries

The user reports Phase 9 commit `02a9aa7` passed hosted run #12 on all platforms.
Phase 10 local evidence is in WORK_LOG.md; its own hosted Linux/Windows/macOS run
is still required, especially Windows wide argv/drive paths and Unicode roots.

[LANGUAGE_SERVER.md](LANGUAGE_SERVER.md) documents the implemented stdio protocol
and limits. Analysis is synchronous/full graph; only entry-reachable modules are
semantically validated. Open unreachable overlays receive empty diagnostics until
imported. There is no watcher, save handler or refresh extension: restart or an
accepted document transition is needed to observe independent disk edits. Reads
retain inconsistency checks, but do not constitute an atomic filesystem image.

UTF-16 only; local normalized file URIs only; output uses the canonical root URI.
Unicode physical roots work locally; Unicode VFS module components remain outside
the existing portable filename policy. UNC/network/multi-workspace roots are
unsupported. Body/header/depth/open-document bounds do not impose a total project
memory/time quota. Revision history grows with distinct document identities.
Mutable locals/parameters and unsupported expression locations have no symbol data.

Completion, hover, rename, formatting, tokens, actions, packages, watchers, workers,
persistent caches, partial compilation and automatic native publication/restart
remain deferred. LLVM Support/numeric support is still a frontend dependency.
Existing RTTI and managed-runtime/package design gates remain unchanged.


## Phase 11 editor integration

The user reports Phase 10 commit `ccfea79` passed hosted run #13 on all platforms.
Phase 11 provides the local editor extension described in
[EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md); its own hosted run is still needed.
Local actual-editor results and package evidence are recorded in WORK_LOG.md.
VSCodium is API-compatible by design but not installed/tested on this host.
The declared editor API minimum is 1.91; actual local testing uses VS Code 1.141.0.

Single local filesystem project/window only. Explicit entry is required for imports;
fallback selects one local VOID file and excludes other documents. Multi-root and
network/UNC workspaces, untitled/virtual documents and discovery are unsupported.
Use canonical physical project paths when opening filesystem aliases. Entry-reachable
analysis, non-atomic disk reads, portable VFS components and absent mutable-local
symbol records remain existing server limits. Restart explicitly to observe external
disk changes. Editor text models may outlive closed tabs; synchronization follows
actual TextDocument lifetime, not tab visibility. There are no watchers, partial
analysis, completion/hover, rename/formatting, debugger or automatic native reload.

The standard client can send optional $/setTrace notifications; the minimal server
logs them as unsupported. Protocol navigation/diagnostics remain valid. Verbose trace
can include source. Native initialization uses the standard language-client lifecycle;
configure an actual working voidls, not a compiler driver or arbitrary program.
A separate minimum-editor-version run, VSCodium smoke test and hosted platform
matrix remain useful follow-up verification. No repository license is chosen by
this phase; public redistribution/publishing remains with the owner.
