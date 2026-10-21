# Known issues and deferred work


## Stage 8 current disposition — 20 October 2026

Stage 8 Sprint 4 is **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending**.
Final consolidated Debug, Release and strict ASan/UBSan/leak runs passed
**148/148 each: 444/444 unique configuration/suite outcomes**. Both complete
1 MiB frontend gates passed. See [the application/static-provider contract](STAGE8_APPLICATION_NATIVE.md)
and [Sprint 4 acceptance](stage8/SPRINT4_ACCEPTANCE.md) for exact evidence,
preliminary failures/corrections, source hashes and limits.

All three Sprint 4 design decisions are approved in [DECISIONS.md](DECISIONS.md);
no Sprint 4 owner decision remains pending. The bounded sink requires explicit
C++ admission, one provider descriptor per graph; CLI/LSP imports grant no trust.
Sprints 1–3 remain preserved, including [CI #82's Windows repair](stage8/CI82_WINDOWS_REPAIR.md),
argument binding, numeric prelude, UTF-8/collections, tooling and reload contracts.
Prior failures and verification records remain historical evidence.

**Hosted CI remains deferred until the end of Stage 8**, unless Boss explicitly
authorizes an earlier run. Windows/macOS, hosted CI #82 Debug/Release execution
and fresh actual editor-client verification remain pending. Linux local tests
cannot establish those results. Stage 8 is not hosted locked.

**Stopped after Sprint 4 for Wrench review. Sprints 5–6 remain future work.**
No commits, pushes, branch/remote changes, hosted CI, publication or website work
occurred. Root README is unchanged; documentation and future sprint prompts belong
under `docs/` by default. Do not begin Sprint 5 without Boss authorization.

VSCodium, new Windows/macOS editor combinations and clean-machine portability
remain unverified. CI #82 Windows execution is deferred, not silently counted as
passing. Sprint 3 supplies the bounded text/recoverable collection APIs in its
contract. Synthetic method callees reached through spawn still reject with VOID1103;
no builtin method values are added. Sprint 4 implements the bounded application
entry and static sink contract, subject to its recorded acceptance status. An
official standard-library catalog, private fields and async retirement remain
outside Sprint 4.

## Stage 7 implementation boundaries — 20 October 2026

The current contract is [STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md);
[acceptance](stage7/ACCEPTANCE.md) distinguishes executed checks from support targets.
Earlier “not implemented/unauthorized” Stage 7 sections below describe research
history. The previously recorded deep valid receiver-chain capability/lowering
stack failure is repaired locally without reducing depth guards; see the acceptance
record for baseline reproduction and 1 MiB positive/rejection results.
Recovery is bounded to identifier/one-line call contexts; existing call-callee
replacement can return no completion. Methods are
conservatively suppressed on readonly recovered receivers. Types/member families,
aliases, generic families and captured bindings are excluded from rename. General
quick fixes and semantic tokens are absent. Native archives require audited host
runtime dependencies and remain private; no redistribution license is selected.
VSCodium, advertised research minimums and new Windows/macOS client combinations
remain unverified unless execution evidence is explicitly added. No Stage 7 lock
or hosted result follows from local acceptance.


## Stage 7 research findings — 20 October 2026

Boss confirms Stage 6 locked at `4139297f` / CI #78, 973/973; earlier pending-lock
statements below are historical. [STAGE7_ROADMAP.md](STAGE7_ROADMAP.md) records the
authorized research result. The main dependencies are compiler-owned scopes and
incomplete-source completion, bounded rename/formatting policies and native
distribution. Measurements demonstrate queued full-analysis work. The inspected
Vim client does not enforce versioned workspace edits, so safe rename must not be
advertised without a tested solution. These are research findings/proposed Stage 7
work, not implemented fixes, new Stage 6 blockers or implementation authorization.

## Stage 7 planning boundary — 19 October 2026

Stage 7's future goals are recorded in the [development strategy](DEVELOPMENT_STRATEGY.md#owner-workflow-and-stage-7-planning--19-october-2026),
[server plan](LANGUAGE_SERVER.md#stage-7-planning-goals-not-implemented) and
[editor plan](EDITOR_INTEGRATION.md#stage-7-editor-goals-planning-only). Completion,
hover, signature help, rename, formatting, richer symbols/project handling and
Neovim/Vim integration are goals, not newly implemented capabilities. Vim client/
version/plugin support, native server discovery/distribution, workspace policy and
exact feature contracts remain unresolved. Compatibility requires real editor
integration evidence. All editors must share compiler-owned `voidls` analysis;
there is no authorization for separate parsers/type checkers or speculative
optimization systems. Stage 7 is not authorized, and Stage 6's contracts, statuses,
five-delivery sequence and owner-controlled hosted acceptance remain unchanged.

## Delivery 5 integrated acceptance — 19 October 2026

Delivery 4 is owner-confirmed hosted-accepted at
`619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
7/7 jobs, 917/917 suites. Delivery 5 local acceptance passed: 139/139 each in Debug,
Release and strict ASan/UBSan/leak configurations (417/417), plus 1 MiB, editor
integration and packaging. A completion race in the new cancellation fixture was
corrected; no known local acceptance failure remains. Windows/macOS and the final
seven-job hosted acceptance remain owner-controlled and pending. Stage 6 is not
yet locked and Stage 7 is not authorized.
Existing bounded native/task/worker contracts remain unchanged.

Combined tests cover explicit root adapters, owning-thread C resources, groups,
workers, callbacks, GC, reload/cache and shutdown. No compiler/Runtime production
change was required by the completed acceptance. Richer callable/native ownership,
resource task transfer, native worker effects, dynamic loading, automatic CLI/LSP
binding admission, packages, async CLI and general shared-memory parallelism remain
deferred. Dropped native owners persist until explicit Runtime shutdown; GC never
runs native cleanup. Root-duration task/frame retention and data-only Runtime
metadata retention remain conservative; zero post-collection managed objects is
not a process-memory bound. Corrupt cache entries remain misses until removed.
See [ASYNC.md](ASYNC.md), [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md) and WORK_LOG.md
for actual integration evidence and the required owner-controlled hosted gate.

## Current boundaries, 18 October 2026

See the [canonical status summary](DEVELOPMENT_STRATEGY.md#current-implementation-status). Phases 1–25 and Stage 3 dictionary/runtime integration are owner-accepted at `cd95e373` / CI #65, all seven jobs successful. Stage 4 is hosted-verified and LOCKED at `483e67e4` / CI #68: 80/80 suites in all seven configurations. Source arrays, interfaces, generics, collection iteration/ranges, named/default arguments and custom initialization are implemented. Older exclusions below are dated findings, not new approval requirements.

- The reproduced deep valid receiver-chain lowering stack issue below remains open; no unrelated repair is claimed.
- Dictionary keys exclude floats, interfaces, arrays/lists/dictionaries and value structs containing unsupported fields. Custom equality/hash, adversarial hash randomization and stable cross-launch hash guarantees are not implemented. Result-based `.get()` remains deferred; checked lookup and contains/remove/clear are available.
- Dictionary template literals/defaults can contain pure entries, but dictionary lookup/count/methods require runtime evaluation. Dictionaries share identity; no independent dictionary clone/view API is promised.
- Stage 4 is hosted-locked at CI #68; see ADVANCED_LANGUAGE.md and WORK_LOG.md for validated behavior. Generic enums, unit/void result payload syntax, module-time patterns and module-time bound receiver allocation are outside the delivered subset. Readonly callback-result authority is conservative. Pop/get still await exact signatures/error types. Missing slice endpoints/views, string indexing/interpolation/formatting, general shared-memory parallel execution and general managed FFI remain future work; bounded value-only source spawn is hosted-accepted at CI #75. Native async activation has the bounded Delivery 1 implementation described in ASYNC.md; Delivery 1 is owner-confirmed hosted-accepted at CI #73; Delivery 2 is hosted-accepted at CI #74.
- Root lifetimes remain conservative to function exit; collection is coordinated, precise and nonmoving, not concurrent collection or a production performance certification. Metadata and executable retirement retain existing contracts.
- LSP completion/hover/rename/formatting and package discovery remain deferred. Stage 5 is hosted-locked at `4627687b` / [CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484), 82/82 suites in all seven configurations. Stage 4 retains its CI #68 historical lock.
- [STAGE6_PROPOSAL.md](STAGE6_PROPOSAL.md) preserves the approved five-delivery roadmap. Delivery 1 implements ordinary async functions, managed receivers, await and an explicit owning root adapter; see [ASYNC.md](ASYNC.md) for restrictions and local acceptance. Delivery 2 cooperative cancellation/groups and Delivery 3 value-only parallel spawn are hosted accepted at CI #74/#75. Delivery 4 adds the bounded static fixture documented in NATIVE_LIBRARIES.md; general parallel sharing/native libraries, public cancellation recovery and an async CLI remain deferred. Completed task records/frames remain rooted until the adapter finishes draining, so long-lived roots can retain substantial memory. Existing GC coordination does not serialize arbitrary application writes.

- Optional persistent Debug caching is hosted-accepted at CI #70; see STAGE5_CACHE_PROPOSAL.md. Windows persistence still bypasses safely because private ACL/path publication is not established; CI verifies fallback, not Windows disk reuse. Builds using unidentified shared LLVM compiler libraries also bypass. macOS cache suites and Windows fallback passed hosted verification. Automatic watching, per-function caching, automatic eviction and remote/shared caches are not implemented. Corrupt entries remain misses until the disposable cache is removed.

## Historical issue records and original scope notes

The following records are retained for traceability. The large-array activation blocker is superseded by Orders 1–5 and Phases 23–25; it is not a current limitation. Current implementation contracts take precedence over dated phase boundaries.

## Historical, resolved Phase 23 activation blocker: large-array storage and analysis limits

The owner's 15 October 2026 decision supersedes the earlier 65,536-byte production
per-function array-storage policy. Neither fixed nor growable arrays may have an
arbitrary 64 KiB source-level ceiling. CI #45 is owner-confirmed green (7/7 jobs,
61 suites each), locking Part 5A research, not enabling production native arrays.

Stage 1 replaces the old extent/multiplicity ceiling for compact reference-free
fixed-array analysis and pure defaults. Repeated defaults no longer allocate one
compiler value per element. Independent IR validation checks these compact shapes
and templates. Actual recipe/graph work and finite/depth/checked-arithmetic guards
remain; target allocation representation is not yet implemented. Reference-bearing
arrays, effectful repeated construction and nonscalar array operations remain
later work. The planner/staged machine auditor retain their unchanged research
checks. See ARRAYS_PROPOSAL.md Section 22 for the current local checkpoint.

Investigate storage for large fixed snapshots, including the existing GC heap,
without another memory manager. No alternative representation, new numeric limit,
production policy reactivation or heap-backed array implementation is approved.
Preserve CFG/ABI/origin/stack safety research, readonly/reference semantics, roots,
publication, failure and reload. Native array lowering remains explicitly blocked.
A successful build must contain every required function; unsupported operations or
failed required functions must diagnose and fail the whole build, never silently
omit code or deliver incomplete success. ARRAYS_PROPOSAL.md Section 21 audits the
specific enforcement locations and future approval/implementation gates. The
research verifier remains operational and unchanged.

## Deep valid receiver chains in small-stack IR lowering

The run #38 investigation separately found that a valid 120-call inline-value
receiver chain can exhaust a 1 MiB Linux Release stack in `Lowering::expression`.
Both parent 5b75d87 and Phase 22 reproduce it. This is distinct from the malformed
400-call semantic-analysis crash corrected for run #38, which must stop before IR
lowering. The lowering case remains unresolved and is not a claim about a captured
Windows stack. Its fixture and GDB trace are retained under build/phase22-crash;
WORK_LOG.md records the evidence. A lowering change requires separately scoped
work; no limit, test assertion or optimization level was reduced to hide it.

## Phase 22 approved diagnostic amendment

The recorded VOID1001 malformed-source output defect is fixed locally. The shared
compile-time renderer escapes rejected bytes as `\xHH`, keeping output valid
UTF-8 and aligning carets with the escaped display. Detection/rejection, original
source bytes and spans, scalar locations, related diagnostics, runtime excerpts
and LSP UTF-16 coordinates are preserved. Strict UTF-8 CLI regression probes,
retained-snapshot/caret checks and disk-error/overlay-repair LSP coverage verify
the correction. See DIAGNOSTICS.md and WORK_LOG.md for the contract and measured
verification. Hosted Windows/macOS acceptance remains with the seven-job gate;
no local result is claimed as hosted verification. S1-S8 string foundation work is
separately approved and described in STRINGS.md; WORK_LOG.md records its validation.

## Phase 22 string boundary

The UTF-8 foundation supports immutable strings, empty defaults, literals/escapes,
content equality, concatenation, constants/default templates, managed/value fields,
parameters/returns, coordinated GC and shared JIT/AOT paths. Retained text contains
copied bytes, never source/LLVM/JIT addresses. The same shape walker distinguishes
traced immutable leaves from mutable-reference authority. See STRINGS.md for the
implemented contracts and STRINGS_PROPOSAL.md for approved S1-S8 alternatives.

Scalar indexing, scalar length, byte_length and nullable strings are approved
future direction, not implemented features. Interpolation remains reserved and
diagnosed. Print, formatting, multiline text, slicing/iteration, grapheme APIs,
ordering, collections, builders, general string FFI and public string ABI remain
deferred. Index errors, slicing and ordering semantics remain open. Flat concat
can be quadratic; scratch duplicates payload bytes transiently; fixed root homes
can retain dead text until return. Compiler constants/pools have approved 16/64 MiB
limits, not a total runtime memory quota or production-GC guarantee.

## Historical Phase 21 status

The following phase-specific pending wording predates the owner's report of
successful runs #36 and #37. Current Phase 22 evidence is in WORK_LOG.md.

Updated for Phase 21. Phase 20 is owner-locked at run #35 (51/51 in all seven jobs).
Phase 21 adds four suites (55 total); INTERFACES.md and WORK_LOG.md record the local
implementation and validation. Its seven-job hosted Windows/macOS/Linux gate is pending.

Historical Phase 20 status follows; the Phase 20 hosted gate is complete. The owner reports Phase 19 locked at run #33, attempt #2,
all seven jobs (47 suites). Phase 20 adds four suites (51 total); INHERITANCE.md
and WORK_LOG.md describe actual local verification. Hosted Windows/macOS and all
seven configurations remain required after owner review/push.

Single managed inheritance, explicit virtual/override/super, complete base prefixes
and staged constructor chaining now use the shared compiler/IR/runtime. Source
casts to descendants, type-pattern expansion, protected/richer field access,
inherited hiding, covariance, abstract/final, richer interface features, open-world type loading,
implicit constructor forwarding/chaining and external managed aggregate ABI remain
deferred. Physical layout depth combines inheritance and inline nesting (128);
expanded fields currently remain capped at 65536 and construction contexts at 1024.
The expanded-layout guard is an implementation limit under the Phase 23 review
above, not authority for a permanent array-size ceiling.
Guarded exact-key dispatch prioritizes correctness over cache/table optimization;
preliminary dispatch timing is not a production benchmark. Objects own no code
pointers and do not pin their allocating generation. The runtime remains a tested
foundation, not a production-ready GC or resource/finalizer system.

Value init retains C1 all-fields-valid entry; required-reference value fields use
explicit no-init construction. Fresh initializer profiles and existing binding
capacities are fixed and conservative. Readonly nominal heap fields, richer value
readiness, flow-sensitive authority upgrades and managed aggregate FFI are deferred.
Fixed aggregate root homes can increase native frame pressure and retain dead values
until return; the 1 MiB semantic checks are not an arbitrary native frame guarantee.

S1 construction views have no precommit identity: whole-view escape, self-links,
casts/nullable injection, rebinding, constructor arguments and returns are rejected.
Helpers require all fields ready and a bounded compatible context; helpers cannot
perform first required-field initialization. Contexts are declaration/parameter
masks, capped at 1024/project. Required value children still complete before the
body. Effects on completed objects are not rolled back on body/commit failure.
These approved limits do not permanently prohibit broader future initialization.

Richer nullable patterns, managed FFI and
production GC tuning remain deferred. Readonly/effect analysis is conservative
across control-flow/rebinding joins and may reject cases requiring path-sensitive
alias reasoning. Managed debug metadata is currently opaque-pointer metadata.
Linux source/JIT/AOT results are not Windows/macOS verification.

The owner reports Phase 13 `c72e62a` passed hosted run #19 on Windows, Linux and
macOS. Phase 14 run #21 (`424cacb`) passed Windows/Ubuntu but macOS ARM64 Debug
aborted in frontend aggregate debug layout generation (23/24 suites passed).
A local reproducer confirmed a context-lifetime bug in the shared LLJIT DataLayout
cache; lowering now queries the module-owned copy. WORK_LOG.md records the
backtrace, regression and validation. The owner subsequently declared Phase 14
complete. The later owner-reported Phase 15/CI successes supersede that historical hosted gate.
Approved C1 initialization is described in INITIALIZATION.md; Phase 13 I2 remains
an unapproved historical alternative. Owner-module methods and custom value
initializers use the existing shared types/IR/LLVM path.

Runtime declared field defaults, inferred/default/named ordinary function/method
parameters, constructor overloads, fallible constructor results,
resource deinit, foreign extensions and external aggregate ABI remain deferred.
An enclosing value initializer requires default-constructible fields; required
value children can be explicitly supplied to no-init wrappers. Managed init uses
required-reference definite initialization as documented for Phase 17. Compile-time contexts
cannot execute init. Initialization body/helper changes require manual restart.
Layout remains compiler-private with current 128-level/65536-expanded-field bounds.
Their array-multiplicity interaction requires the Phase 23 review above; these
implementation guards do not override the large-array language commitment.

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
- Supported value types include the Phase 22 string foundation in STRINGS.md,
  all ten exact-width numeric types, bool and acyclic
  numeric/bool value structs, plus the bounded managed family in MANAGED_STRUCTS.md.
  Collections/ranges, custom initialization, generics and named/default arguments
  were implemented in later phases. Closures, async, multiple returns and the
  remaining future language systems are not implemented. Unsupported
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
  aliases and arbitrary public native numeric ABIs remain deferred. Dictionary
  integer/bool hashing is now implemented.
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
  general source-level native bindings remain deferred; the explicitly trusted static fixture is documented in NATIVE_LIBRARIES.md.

## Managed memory

- The existing experimental non-moving tracer now has a runtime-owned registry,
  cooperative epochs, frozen per-thread frames and persistent handles. It is not
  a production managed runtime. Allocator caches, weak references, async roots,
  finalization and broader managed features remain deferred. Phase 16 source uses
  this foundation; see GC_RUNTIME.md and MANAGED_STRUCTS.md.
- Shared normal/O2 JIT/AOT lowering publishes exact reference storage, including
  nested aggregates and temporaries, and polls coordinated loops/calls. There is
  no conservative stack/register discovery. LLVM statepoints/stack-map decoding,
  tighter liveness and LTO auditing remain separate deferred work.
- Checked generated failure/normal returns unlink frames. Callback bridges catch
  host exceptions and return binding-specific failure status; native exceptions
  must never unwind through generated code. Corrupt precise roots/frame ordering
  are fatal. Uncooperative Managed threads prevent collection; an optional host
  deadline aborts without sweeping rather than forcing suspension.
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
  Phase 15 provides same/foreign-thread callback admission and teardown only for
  bindings with a verified provider-quiescence barrier, including dispatched
  pre-admission calls. Arbitrary providers, native dynamic-library ownership and
  generalized asynchronous native bindings remain unsupported.
- The UTF-8 helper implements borrowing of terminated UTF-8 with a temporary
  owned copy. The static fixture now implements retained string storage and returned buffers; general UTF-16 and native binding
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

## Historical phase priorities and subsequent work

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
[EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md); its dated phase validation is superseded by the owner-accepted current baseline.
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


## Phase 19 composition boundary

Both stored using forms and one-hop field/method promotion are implemented; see
COMPOSITION.md. Automatic transitive promotion, nullable using targets, stateless
method-only reuse and new field access modifiers remain deferred. Embedded methods
retain existing explicit owner-import requirements. Composition grants no subtype
or implicit conversion. Hosted seven-job validation remains necessary for this
phase; Linux local evidence is not Windows/macOS execution.


Phase 21 supports managed-backed interface views, including reference-containing
values and original inline component receivers. Standalone value erasure/boxing,
interface inheritance, defaults/bodies, overloads, Self/generics, width/downcasts,
dynamic discovery/type tests, managed/interface FFI and open-world loading remain
unimplemented. Per-requirement provider-family effects are conservative: an unrelated
mutating provider can reject a readonly reader; empty families are unknown, not
optimistically pure. Consumers need the interface owner's import. Runtime witnesses
are immutable data, not permissions or retained code. Root homes retain dead values
until return as before; no new liveness optimizer or production-GC claim is made.
