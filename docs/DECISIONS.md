# Architectural decisions

**Stage 4 continuation decisions (owner approved):** Deferred calls capture their receiver and arguments when registration is reached; deferred blocks read shared captures when cleanup executes. Escaping closures created in value-struct methods capture an independent receiver snapshot, shared by copies of that closure. Ordinary captured locals keep shared storage; managed receivers keep their managed identity.

## Stage 4 Advanced Language, owner-approved 18 October 2026

Stage 3 is hosted-locked at `cd95e37341f756e749ea5f3d6da9ad44716d491f`,
[CI #65](https://github.com/Shmellyorc/Void.Language/actions/runs/39125218619),
seven successful jobs. These are approved Stage 4 contracts, **not an implementation
or hosted-acceptance claim**. Older OPEN labels below are superseded only as stated.

- `result<t,e>` carries `Ok(value)` or `Err(error)`. `?` unwraps success or propagates
  the exact matching error type. Recoverable errors remain distinct from runtime
  failures. `Ok` also matches present nullable values, without converting nullable
  into result; `Ok(null)` can be a successful nullable payload.
- One pattern model serves `is` and statement/expression `match`: type, value,
  range, nullable/result, tagged enum payload, struct field, list and dictionary
  patterns. Dictionaries permit extra keys; list rest copies independently.
  Bare names compare existing bindings; **`var name` explicitly captures**,
  including `Ok(var value)` and `..var rest`. Captures have success-only lexical
  scope and cannot shadow existing bindings. Incomplete matches warn; an unmatched
  statement does nothing, an unmatched expression raises a source-located runtime
  failure. Exhaustive matches do not warn.
- Reached defers register dynamically and run LIFO on function exit, early return
  and `?`. `with` cleans at scope exit. `deinit` releases external resources,
  never GC finalization. Initially it is managed-struct-only, no-result; value
  structs cannot define it. Explicit/deferred `deinit` and `with` share per-object
  one-shot release state so aliases cannot release twice. Ordinary value copies
  are unchanged. Fallible acquisition uses explicit result-returning factories,
  without rewriting `init`. Deferred code permits Ok/Err and locally valid nested
  break/continue, but no direct return/`?`/await. Preserve the primary runtime
  failure, attach cleanup failures and continue applicable cleanup.
- Bible `property`/`get`/`set` syntax executes accessor logic through field access.
  Getters cannot mutate receivers; missing setters are readonly. Recoverable errors
  use explicit methods. Preserve receiver selection, evaluation order, deep readonly
  and value semantics. Accessor body changes can reload; incompatible signatures,
  type/layout changes require restart.
- Function types, function values, anonymous functions and escaping closures share
  captured mutable storage (including value locals), while unrelated ordinary value
  assignments remain independent copies. Assigning closures shares state. Captures
  retain reference identity/readonly and precise GC tracing beyond stack lifetime.
  Reuse stable function identities/dispatch/code leases; incompatible capture layout
  requires restart and compatible bodies can reload.
- Events are synchronous, ordered, strong subscriptions. Snapshot subscribers per
  emission; duplicates are legal; `-=` removes the latest matching identity.
  Only the owner emits; mutation/nested emission must be safe. Callback identity
  survives assignment/compatible reload. Handlers have no result and handle
  recoverable errors internally; runtime panic stops later handlers. No implicit
  async. Single function fields remain distinct from events.
- Compatible `call()` structs are callable/callback values using existing method,
  structural checks, visibility, effects and readonly rules. No arbitrary external
  C function-pointer ABI promise; preserve native callback/quiescence contracts.
- Implement necessary enum/function-type foundations through the existing frontend,
  typed IR, LLVM 19.1.7 lowerer and coordinated Runtime. No second execution engine,
  collector, type checker or ownership registry. Dictionary `.get()` and list
  `.pop()` remain deferred until exact signatures/error types are approved.
- Preserve Stage 3, seven CI jobs, strict sanitizers and 180-minute timeout. Stage 5
  automatic watching/persistent Debug cache remains approved future work, not part
  of this sprint. Stages 6–7 remain outside this scope. Owner controls Git and CI.


## Stage 3 dictionary decisions, owner-approved 18 October 2026

Current status and hosted evidence live in DEVELOPMENT_STRATEGY.md. The owner approved the grouped dictionary decisions recorded in DICTIONARIES.md: expression keys with colon literals, exact string/integer/bool/managed-identity and eligible value-struct keys, checked indexing/upsert, contains/remove/clear/count, shared dictionary identity, deep readonly and fixed-child value copies, deterministic order, captured receiver/key/version before RHS, and next-access structural iterator invalidation. Float/collection/interface/custom-equality keys and result-based get are deferred. No result/pattern system, collector or certification framework is added. CI timeout becomes 180 minutes; the owner retains Git/hosted CI control.

Older phase decisions below remain historical where superseded by delivered Phases 23–25 and this Stage 3 work.

## Phase 23: superseding large-array size decision, 15 October 2026

The owner explicitly withdraws the earlier approval of 65,536 bytes as a production
per-function array-storage policy. Neither `[T, n]` nor `[T]` may have an arbitrary
64 KiB source-level ceiling. General-purpose/game programs must be able to use large
arrays, including megabytes of JSON, text, image and asset data, subject to real
representability/address-space bounds, checked arithmetic and allocation failure.
No replacement numerical ceiling is approved.

The threshold remains valid as an explicitly scoped research parameter for stack,
CFG/ABI certification, diagnostics and verification experiments. Preserve all useful
measurements and safety assertions; do not loosen them to accept large payloads.
The separate 65,536 expanded-layout guard must be reviewed as well: actual analysis
resource protection cannot silently impose an equivalent element/byte ceiling.
Current frontend/IR guards are implementation limitations, not permanent language
semantics, and remain unchanged by this documentation-only task.

Large fixed arrays retain value/snapshot semantics regardless of physical storage.
Investigate safe representations using VOID's existing coordinated GC heap without
another memory-management system. No heap-backed fixed-array design, hybrid cutoff,
copy-on-write rule, changed limit or production policy reactivation is approved.
Existing reference/readonly, GC/root/publication, failure and reload contracts remain.

Successful compilation must include every required function. An unsupported operation
or failed required function fails the whole build with a clear diagnostic; never
silently omit it or deliver partial success. Normal optimization may still eliminate
unneeded code when its correctness is proved; compilation failure is not that proof.

Owner confirms CI #45 green, 7/7 jobs and 61 suites per configuration, locking Part 5A.
Native arrays and public certification activation remain gated. ARRAYS_PROPOSAL.md
introduction, A10 and Section 21 contain the revised authority, code audit and required
review/implementation work. Earlier approvals and measurements remain historical
evidence. Further architecture/implementation requires owner/wrench approval.


## Phase 22: owner-approved UTF-8 string foundation, 14 October 2026

Boss and wrench clanker approved S1-S8 in STRINGS_PROPOSAL.md, including the 16 MiB
decoded constant/result and 64 MiB unique module-pool limits. The subsequent
diagnostic amendment was additional work, not a replacement or new approval gate.

Use one intrinsic immutable string type, a zero-word empty representation and
flat nonempty payloads on the existing coordinated Heap. Extend its private
layout kind/extent checks, reuse complete publication and share scalar validation
without a frontend/LLVM runtime dependency. The existing shape walker includes
immutable leaves for tracing while keeping mutable-reference authority separate.
Roots, staged/aggregate storage, argument/return handoffs, failure cleanup,
generation leases and private JIT/AOT calling conventions remain authoritative.

Quoted literals and approved escapes, empty defaults, pure owned byte templates,
content equality and +/+= use the existing typed AST/IR/validator/shared lowerer.
Keep exact bytes with no normalization. Embedded NUL is valid ordinary content;
terminated native bridging still rejects it. Native calls use owned copies before
NativeSafe entry; retaining providers release references before byte-owner teardown.
No runtime string cache, per-length nominal descriptors, string heap, source cstring
or external aggregate ABI is added. STRINGS.md describes behavior and current costs.

Language Bible Section 8.1 separately records approved future scalar indexing,
scalar length/byte_length, nullable strings in principle and automatic interpolation.
These remain outside Phase 22 implementation. Ordering, slicing/index failures and
broader native bindings retain their design questions. WORK_LOG.md owns measured
verification and the pending seven-job hosted gate; no local execution certifies
Windows/macOS or production GC readiness.

## Foundation phase, 10 October 2026

### C++20 and LLVM 19.1.7

The compiler uses the LLVM C++ API, with an exact CMake SDK version requirement.
ORC `LLJIT`, resource trackers, `IRBuilder`, `DIBuilder`, `PassBuilder` and
`TargetMachine` were compiled and exercised on the installed 19.1.7 SDK.
This settles the compiler implementation choice for this phase. LLVM remains
an implementation dependency, not a language specification.

The original Bible/Strategy design snapshots are preserved. Their discussion of
an open implementation-language choice records the state before this phase.
The user's larger cohesive phases supersede the snapshots' numbered milestone
process; language semantics are unchanged.

### Keep the proof IR small and independent

`void_core` owns source locations and a validated IR without LLVM headers.
The proof represents i32 arguments/constants/arithmetic, comparisons, calls,
branches and returns. Functions currently have one fixed `i32(i32)` signature.
Values are local to a basic block; there are no PHIs in this proof IR. Integer
arithmetic uses bitwise wrapping backend operations for the experiment only;
VOID overflow semantics remain open. No new user syntax is introduced.

AOT and JIT use the same lowering, verification and optional LLVM O2 pipeline.
The CLI is explicitly an architecture harness. AOT generates an object; the
integration driver links it using Clang. The compiler does not shell out with
unescaped user input. Source `.void` parsing is the next frontend phase.

### Generation leases for native replacement

Stable identity is the canonical module identity plus function name, independent
of the native address. Each compiled module has distinct versioned symbols and
one ORC resource tracker. Replacement publishes one entry for a whole dependent
module generation. Internal calls stay within that generation. The compatibility
contract includes signature, object layout, captures and constants.

A `ReloadSlot` uses a short mutex to acquire/publish shared code ownership. It
never holds the mutex while native code runs or a resource tracker is removed.
Active calls keep their generation and execution session alive. Tracker removal
occurs only after the last lease is released. Code can outlive its `Jit` host.
No address is exposed through the callable public API. The bind API is trusted
compiler-host machinery and requires an exact native ABI declaration.

This is a conservative, measured correctness proof. It does not implement file
watching, compiler-derived compatibility fingerprints, unrelated multi-slot
transactions, async suspension or foreign-thread lifetime management. Metadata
is supplied by the host, not derived from parsed source. Compilation and symbol
binding are serialized by the host; callers may invoke leases concurrently.

### Explicit precise roots first

The experimental heap uses non-moving mark/sweep, exact reference-field offsets,
zero-initialized payloads, iterative graph tracing and explicit root slots.
Descriptors are copied into allocation records, so callers need not retain
metadata objects. Embedded value references are flattened into descriptor offsets.
Bad roots abort tracing before sweeping and reset temporary mark bits.

The generated root probe registers the address of a stack slot, stores its live
managed reference there, calls native safepoints in a loop, reloads the slot,
and unregisters it on normal exit. External calls intentionally retain ordinary
memory side-effect semantics. LLVM O2 cannot discard a registered live slot.
No pointer is hidden from LLVM through integer casts or blanket volatile loads.

This original proof established a basic reference contract, not a production collector.
Phase 15 adds a coordinated wrapper around the same single tracer (see below).
There is no allocator cache, concurrent tracing, interior
pointer support, weak references or unwind cleanup. Stack/register discovery,
`gc.statepoint` and stack-map decoding remain deferred. LLVM 19.1.7's
`IR/Statepoint.h`, `IR/GCStrategy.h`, and IRBuilder statepoint API were reviewed;
they require a GC strategy, root/base-pointer liveness, target metadata decoding
and runtime coordination that this explicit-root experiment does not provide.
Do not promote this phase to the strategy's full GC integration gate.

### Trusted C bridge proof

A real C translation unit provides representative functions, an opaque resource
and retained callback registration. Generated native code calls the fixture
through ORC absolute symbols. UTF-8 bridge conversion validates scalar encoding,
rejects embedded NUL for terminated APIs, and owns a temporary terminated copy.
This is a trusted bridge helper, not a second VOID string type.

The callback has a stable C trampoline and an explicit context whose registered
root keeps a captured managed object alive. The context uses the reload slot,
so it follows replacement. The fixture unregisters before context/root teardown;
RAII releases the native resource exactly once. Foreign threads, concurrent
unregister, retained string APIs and exception translation remain future work.

### Cross-platform CI and local-only work

Use standard GitHub-hosted Linux, Windows and macOS runners, read-only repository
permissions, one build system and complete official LLVM 19.1.7 SDK archives.
Avoid Windows compiler-only installers that omit development libraries. SDK
versions are checked at configuration, not silently substituted. CI results
remain unverified until the user creates/pushes the repository and jobs execute.
The user owns repository creation and initial publication.

References: [LLVM ORC design](https://releases.llvm.org/19.1.0/docs/ORCv2.html),
[official 19.1.7 releases](https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7).

## CI stabilization phase, 10 October 2026

The first hosted run exposed SDK integration failures. Keep LLVM 19.1.7 exact
and retain the complete archives and architectural suites.

- Linux explicitly uses the archive's LLD 19.1.7 for compiler and AOT links.
  Its LLVM static archive members are bitcode; the default GNU linker rejected
  them. Match `LLVM_ENABLE_RTTI` in the LLVM backend and configure probe, without
  disabling exceptions or changing the other compiler/runtime targets. This
  backend-only RTTI scope was superseded by the 11 October SDK RTTI consistency
  correction below, after the official-archive sanitizer reproduction.
- Repair only missing exported `diaguids.lib` paths on imported LLVM targets,
  resolving the actual target-architecture DIA library from the active Visual
  Studio SDK. Preserve this required dependency and fail if it is unavailable.
  The pinned Windows release embeds its producer's VS2019 installation path;
  the runner has VS2022. See [upstream issue](https://github.com/llvm/llvm-project/issues/111829).
- Replace unavailable libc++ `jthread`/`stop_token` usage with `std::thread`, an
  atomic stop flag and exception-safe joining. Keep four concurrent callers and
  100 publications, and check the exception cleanup path. C++20 remains required.
- Configuration links and executes an ORC host against the selected SDK to catch
  linker, ABI and exported-dependency errors before the project build.

CI stability precedes frontend expansion. Hosted success must come from a new
run after the user publishes these local changes; no remote mutation is made.

### Windows native AOT library search, second hosted run

Pass `-llegacy_stdio_definitions` to the SDK's GNU-style Clang driver instead of
bare `legacy_stdio_definitions.lib`. The latter is an input file that Clang
requires to exist relative to the working directory; it never reaches native
linker search. The library request is translated to the MSVC `.lib` name and
resolved by the native linker using the developer environment's `LIB` paths.
The failing job already exported the correct x64 MSVC, UCRT and Windows SDK
paths, so no hardcoded Visual Studio version/path or copied library is needed.

Retain the required library, direct generated-code `printf` interoperability,
object debug verification and all three executable output assertions. Missing
libraries still fail the test. LLVM stays pinned to 19.1.7; the workflow already
initializes the developer environment and does not need a discovery workaround.

References: [Clang 19 linker options](https://releases.llvm.org/19.1.0/tools/clang/docs/ClangCommandLineReference.html#linker-options),
[Microsoft CRT compatibility library](https://learn.microsoft.com/en-us/cpp/porting/overview-of-potential-upgrade-issues-visual-cpp?view=msvc-170).


## First real source compiler phase, 10 October 2026

The user confirmed the three-platform foundation gate passed before this phase.
Keep that foundation and its six architectural suites. Add one source pipeline:
UTF-8 source -> indentation tokens -> AST -> typed semantic model -> VOID IR ->
shared LLVM lowering for ORC and native object emission. The frontend has no LLVM
dependency, and normal source mistakes fail before any LLVM compilation.

Extend IR function signatures to typed multiple parameters/results, including no
result, and introduce typed mutable local slots plus general jumps. Preserve
aggregate defaults used by hand-built proofs. Instruction values remain local to
a block; frontend temporaries carry values across short-circuit blocks, keeping
left-to-right evaluation correct even in multi-argument calls. LLVM performs
promotion/optimization. This avoids encoding the old one-parameter proof ABI
into language design and avoids a second code generator or type checker.

SourceFile retains byte offsets but reports one-based Unicode-scalar columns;
validate UTF-8, accept a leading BOM and LF/CRLF/CR. Space indentation must match
enclosing levels; four spaces are recommended, not mandatory. Names are initially
lowercase ASCII/underscore/digits, and expressions are single-line. Neither rule
settles the broader Unicode/multiline grammar. No existing Bible text is replaced.

Use signature-first resolution for recursion, exact i32/bool types without
coercion, lexical mutable bindings and default-zero/false typed declarations.
Reject shadowing visible bindings/function names. Omitted return annotation means
no result. Analyze possible control-flow exits independently of LLVM, conservatively
requiring all reachable ends of value functions to return. Literal `for true:`
and `for:` are non-fallthrough unless a reachable break exits them. Continue and
break target the nearest loop. Reject while with an explicit for replacement hint.

Resolve this slice's Boolean precedence explicitly: `not` binds below comparisons
and above `and`, with `or` weakest. Arithmetic unary operators bind above products.
Arguments and operands evaluate left to right; and/or short-circuit. Increment
and decrement are standalone postfix statements. No collection/range grammar,
chained comparisons or broader grammar is silently accepted.

Make the inherited i32 wrapping arithmetic contract explicit for this provisional
slice, including i64-widened signed division to avoid LLVM MIN/-1 poison. Direct
literal zero divisors are source errors; dynamic zero divisors trap. The final
language policy for checked arithmetic and all numeric types remains unresolved;
this is documented behavior rather than a change to the Bible for convenience.

The CLI checks library files and runs/emits an i32(i32) entry adapter. Internal
functions have arbitrary supported parameter lists. Source AOT symbols are
suffixed before adding the native C main/printf/strtoll adapter, avoiding collisions.
Actual source filenames and instruction locations reach LLVM debug metadata.
Stable export/module mangling and full debugger support remain later work.

Clang UBSan's function check reads a two-word type prefix before an indirect-call
entry. ORC functions previously lacked that prefix, and the new source executions
exposed a page-boundary read failure under ASan. Sanitizer configuration now asks
the actual host Clang to emit i32(i32) type metadata and attaches that metadata as
`func_sanitize` to JIT entry functions before native code generation. The host ABI
is checked first. Keep function checks, ASan, UBSan and leak detection enabled;
do not suppress the boundary check. See [LLVM function-check metadata change](https://reviews.llvm.org/D148785).
The generated LLVM application body is still not ASan/UBSan-instrumented; the
C++ compiler/runtime and JIT host call boundary are instrumented.

See SOURCE_COMPILER.md for the implemented subset, known design questions and
next phase, and WORK_LOG.md for actual commands/results. All changes remain local.


## Module system phase, 10 October 2026

The user confirmed the original source compiler's eight suites passed on all
three hosted platforms before this phase. Keep LLVM 19.1.7, all eight suites and
the existing workflow; add module and module-native suites to the same test runner.
Do not introduce a second parser, semantic model or LLVM lowering path.

`compile_project(entry)` establishes the entry parent's VFS root, loads each
reachable file once, builds imports, and registers every function signature before
checking any body. Both the in-memory source API and filesystem project API call
one graph/type/lowering implementation. Return the entry snapshot with IR so CLI
diagnostics do not reread source after compilation. There is no automatic entry
selection or root override in this phase.

Canonical keys are normalized, exact-case VFS paths plus declared function names,
scoped to the current project. Relative imports use the importing file's parent;
leading / always means virtual root. Aliases, physical relocation, native addresses
and JIT generations do not alter ownership. The IR carries module identity and
friendly source name; its validator checks that the function key matches them.
LLVM alone injectively encodes those keys into portable native symbols. Debug
names remain source names. Keep the physical compilation-unit source path separate
from its virtual identity, preventing /main.void from becoming a bogus debugger path. Root entry selection cannot select an imported entry.
Future independent project/library composition needs persistent project identity;
these keys are not globally unique project IDs.

Function-only import cycles, self-imports and cross-file mutual recursion are safe
because this subset has no top-level initialization. Register signatures across
the full graph, then check bodies. This is not a decision about constant/global
initialization cycles. Repeated normalized imports are idempotent, including
multiple aliases; do not duplicate declaration owners or create false ambiguity.

Unaliased direct imports expose accessible simple function names and a filename
stem qualifier when valid. Own-file declarations take precedence. Explicit aliases
provide qualified access only, keeping conflicting functions out of simple-name
lookup. Identical filename stems need no aliases for unambiguous simple calls;
ambiguous qualified calls require aliases. Explicit alias conflicts diagnose at
use declarations. Imports do not transitively reexport names. These scope/order
choices are provisional grammar detail, not a namespace hierarchy. Use declarations
precede functions. No local variable/parameter may shadow visible functions or
qualifiers. Qualified syntax currently applies to function calls only.

Public is default; explicit public is accepted. Private functions are file-private,
including in cycles and through qualified imports. Internal functions are usable
through imports anywhere in this project, never folder-private. External library
exports are not implemented and must later preserve that project/library boundary.

Filesystem policy is deliberately conservative and portable: printable ASCII VFS
components (internal spaces allowed), exact spelling on all hosts, no reserved
Windows names/characters or trailing dots/spaces. Enumerate referenced directories
to diagnose case mismatches/collisions, rather than inheriting host case sensitivity.
Normalize dot/repeated separator paths before host lookup; reject root escapes.
Canonicalize physical root ancestors once; reject symlinks/redirected components
inside the graph and distinct hard-link aliases for one physical source. This is
not a concurrent-filesystem security sandbox. Unicode filename normalization,
installed packages and explicitly permitted external module identities are deferred.
Do not follow OS absolute paths or silently fall back to package discovery.

No Language Bible semantics were edited. MODULE_SYSTEM.md records the full working
contract and remaining questions; WORK_LOG.md records actual verification. Automatic
file watching/live source replacement remains outside this phase; one graph's ORC
resource tracker and existing leases continue to retire native code safely.

## Constants and runtime diagnostic phase, 10 October 2026

The user reports the preceding ten-suite module matrix passed on all platforms.
Keep one parser, one expression type checker and the shared IR/LLVM pipeline.
Local const means runtime readonly; module const means pure compile-time i32/bool.
Register declarations before dependency analysis, reject structural constant cycles,
and preserve function-only import cycles. Public/internal entry constants are
project-wide, private entry constants stay file-private, and root names cannot
be shadowed. Qualified expressions now also resolve imported constants.

The typed evaluator shares operator definitions with IR lowering, uses unsigned
wrapping arithmetic and widened signed division, and executes and/or lazily.
Type checking still checks all branches. No wider numeric or conversion policy is
silently approved. VALUE_SEMANTICS.md separates current behavior from recommendations.

Generated source functions use a hidden per-invocation diagnostic pointer. Failure
returns normally and source callers propagate a bounded, actual call trace. JIT
host exceptions occur only after native return; AOT main prints the same metadata
and exits 1. Embedded source snapshots and generation leases preserve metadata
lifetime. Preserve UBSan function checking by generating both actual host entry
ABI type hashes with the configured compiler. The legacy proof ABI remains intact.

Exact length-delimited source snapshots of all const-containing files become a
compiler-derived compatibility token. This conservative restart rule rejects any
edit to those files, including imported/local const edits, rather than risking an
undetected constant update or a finite-hash collision. More precise dependency
fingerprints are deferred. See VALUE_SEMANTICS.md for explicit limits and lookup rules.

## Phase 5 structured diagnostics and recovery, 10–11 October 2026

Use one compiler-owned Diagnostic record, stable explicit codes, immutable shared
source snapshots and half-open byte ranges. CompilationError preserves the old
invalid_argument catch boundary while exposing structured records; the CLI renders
them without rebuilding semantic information. Unicode-scalar columns are separate
from future LSP UTF-16 conversion and terminal display width. The Bible is unchanged.

Keep recovery in the existing lexer/parser/type checker. Synchronize by indentation
at statement/declaration boundaries, explicitly poison failed values/declarations,
and retain failed constant states. Continue unrelated imports/functions. Conservative
failed-dependency suppression trades some early spelling diagnostics for fewer
misleading cascades; document that tradeoff rather than guessing an i32 type.
Use per-compilation collection, deterministic VFS/span/code ordering, deduplication,
a 100-record budget and explicit recursion guards. Semantic depth is shared across
constant evaluation so nested expressions cannot multiply a per-constant limit.

Validate the complete graph before lowering any AST. Invalid source returns no IR,
never invokes LLVM or native code and cannot replace an object or reload generation.
Keep runtime checked-entry propagation and its ABI separate; share source utilities
and immutable storage. The exact const restart token is unchanged. No automatic
restart or watcher is added. DIAGNOSTICS.md assigns codes and documents the API,
limits, recovery and acceptance cases. Next design precise dependency compatibility
and analysis-result APIs before incremental IDE work or broader language expansion.

## SDK RTTI consistency, 11 October 2026

Run #7 exposed standard-library template ownership and future cleanup crossing
the backend-only no-RTTI boundary. Match `LLVM_ENABLE_RTTI` across all linked C++
targets through `void_options`, including the CLI and tests, in every build type.
Retain exceptions for diagnostics, exact LLVM 19.1.7, sanitizer instrumentation,
strict report exits and all architectural assertions. C++ RTTI policy is unrelated
to managed VOID runtime type information.

The official Linux archive's no-RTTI ABI cannot supply Clang vptr coverage. Report
that limitation at configure time and retain separate RTTI-enabled installed-SDK
sanitizer validation; do not claim these configurations have identical coverage.
CI_SANITIZER_ABI.md records the actual logs, archive bitcode, isolated reproduction
and coverage evidence. Any hosted vptr lane needs a complete RTTI-enabled 19.1.7
SDK. No frontend changes belong in this CI correction.

## Phase 6 numeric policy and implementation, 11 October 2026

The user reports Phase 5 commit `4f3688d` passed hosted run #8 on all platforms.
Before implementation, the user approved the complete numeric policy table,
including all ten exact-width types, modulo integer arithmetic, IEEE ordinary
floating arithmetic, contextual literal construction, identity-only implicit
conversion of existing values, and explicit checked numeric as. NUMERIC_TYPES.md
records the approved contract; the Language Bible is unchanged.

Extend the existing parser/semantic AST, typed IR and single LLVM lowering.
Public IR constants use 64-bit bit patterns and explicit conversion/negation ops.
A small software numeric kernel privately uses LLVM Support APInt/APFloat for
arbitrary-precision parsing and exact declared-precision constant operations.
This adds a narrow Support dependency to void_core, superseding the earlier
implementation statement that the core has no LLVM dependency. Public frontend
and IR APIs remain LLVM-free; LLVM never defines the language rules. No second
source evaluator or code generator is added. Shared operation/type helpers are
used by semantic analysis and independent IR validation.

Runtime casts guard ranges/NaN/infinity before FPToSI/UI or narrowing integer
conversion. Float-to-integer checks use representable original-value bounds,
accounting for truncation at signed minima; no native trunc/truncf library or new
runtime subsystem is required. Signed MIN/-1 uses a safe divisor selection before
division. Negation uses FNeg for floating signed-zero correctness. Keep the existing
failure context, exact const restart tokens, generation ownership, CLI entry ABI,
SDK RTTI flags, sanitizer checks and all prior suite assertions.

Numeric fixtures use independent unlimited-precision integer calculations and
IEEE neighbor boundaries. Runtime-input checks supplement constant comparisons;
normal/O2 JIT and native AOT use the same files. Existing tests that previously
rejected all floats now reject malformed float suffixes and unsupported string
types, preserving their negative-test purpose while admitting the approved feature.
No unrelated grammar, nullable casts, managed types or public native ABI is added.
See WORK_LOG.md for actual verification and remaining hosted requirements.

## Phase 7 validated analysis and exact dependency compatibility

The user reports Phase 6 commit `9b9fdac` passed hosted run #9 on all platforms.
Expose one immutable successful analysis result from the existing full-graph
validation pipeline, retaining owned typed trees and snapshots. Lower on demand;
compilation adapters never repeat full analysis. Failed results expose diagnostics
only. Public records contain VFS identities, typed/visible declarations, constant
bit patterns and resolved dependency references.

Replace the conservative full-file const token with exact parser-token declarations
and resolved binding/type/visibility/import records. Exclude trivia and physical
locations; compare exact length-delimited data, never hashes alone. Preserve
unused-alias renaming when owners remain unchanged. Retain conservative declaration
and reference ordering restrictions for this phase. All reachable constants,
including runtime local readonly declarations, still require restart after actual
changes. Add a validated-source JIT preflight publication API while preserving
legacy IR proofs and one-slot whole-module leases. Host source metadata cannot
override compiler-derived safety. No LSP, incremental compiler, watcher, automatic
restart or multi-slot transaction is introduced. See ANALYSIS_COMPATIBILITY.md.

## Phase 8 in-process tooling and invalidation planning

The user reports Phase 7 commit `04768bf` passed Actions run #10 on all three
platforms. Consume its successful records through a small owned read-only index.
Return copied declaration/reference records retaining immutable source spans;
index only the supported function and constant categories, using UTF-8 byte
offsets and canonical VFS identities. No mutable-local/member information is
invented. Keep declarations and references deterministically ordered.

Plan invalidation from reverse imports/references and local-constant containment
with finite iterative traversal. Old-only plans require complete discovery and
validation; unknown modules and implicit root namespace edits conservatively
include all known modules. Two successful snapshots permit exact source/module
differences and confirmed declaration/binding summaries; union both dependency
graphs to retain removed/rebound dependents. Source changes still seed every
declaration in a changed file because function-body extents are not collected.

Use the existing exact compiler source contract for the source compatibility
classification. Tooling plans never publish code or replace host ABI contracts.
Snapshot comparisons cannot certify filesystem revisions edited afterward.
No incremental compiler, second frontend, watcher, LSP/JSON-RPC transport or
persistent cache is added. TOOLING_FOUNDATIONS.md documents API and boundaries.


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


## Phase 10 minimal language server

The user reports Phase 9 commit `02a9aa7` passed hosted run #12 on all platforms.
Implement `voidls` as a synchronous stdio adapter around the existing owning
ToolingSession/ToolingIndex/frontend, with full-document synchronization only.
Map signed LSP client versions separately onto increasing positive session
revisions, including close/reopen. Analyze and accept selected snapshots before
publishing; query only current valid analysis. Use compiler declaration/reference
records, including declaration-name queries, rather than a separate symbol scanner.

Compute UTF-16 positions from retained exact source bytes and existing line indexes.
Share the existing UTF-8 scalar validator; never split a scalar/surrogate pair.
Use explicit UTF-8 filesystem conversion for physical paths and Windows wide argv
and binary streams. Keep portable VFS module-name restrictions unchanged. Use LLVM
Support JSON, with bounded framing and nesting, rather than another parser/framework.
The executable links frontend/numeric support, but no LLVM backend, ORC or runtime.

[LANGUAGE_SERVER.md](LANGUAGE_SERVER.md) defines protocol, URI, freshness and
resource contracts. Three new suites bring CTest to twenty-one, preserving all
eighteen old suites/assertions. No language/ABI/reload semantics change. Full graph
analysis and non-atomic disk reads remain; watchers and partial compilation are
separate future decisions. Actual results and pending hosted checks are in WORK_LOG.md.


## Phase 11 editor client

The user reports Phase 10 commit `ccfea79` passed hosted run #13 on all platforms.
The isolated TypeScript extension uses vscode-languageclient and standard editor
APIs over the unchanged native stdio server. Select one explicit entry/root per
window; empty entry supports a fixed active-file fallback, not discovery. Relative
settings resolve literally against one workspace folder. Reject multi-root sessions
and gate every native startup on Workspace Trust. Serialize stop/start/configuration
work, report unexpected termination and require an explicit restart rather than
creating crash loops. The extension contributes presentation only; the compiler
owns diagnostics/resolution and the client owns UTF-16/protocol conversion.

[EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md) documents commands and architecture.
Use the standard executable transport default so the client does not append --stdio.
Dependencies and Node are pinned; the JS bundle includes runtime license notices,
never native binaries/LLVM. Keep package licensing UNLICENSED pending the owner's
choice; local VSIX packaging does not authorize publication. Real editor tests use
actual source/children and include a separate trust-enabled CLI launch because the
official test runner disables trust. No native semantics/CMake/test assertions change.

## Phase 12: value struct foundations, 12 October 2026

The owner approved Option A in VALUE_STRUCTS_PROPOSAL.md: declared field defaults
are pure, typed compile-time expressions; omitted fields use recursive safe
defaults. Explicit named fields evaluate once in source order. Runtime defaults,
custom init and positional constructors remain future features, not permanent
prohibitions. No Language Bible change was required.

Extend the shared ir::Type with owned nominal descriptors used by frontend,
validated analysis, IR and tools. Keep aggregate compile-time values recursive
and delegate scalar computation to the established numeric kernel. Resolve and
validate acyclic layouts before defaults/bodies. Aggregate construct/extract/insert
operations plus typed slots implement copying and nested updates through the same
LLVM lowering in JIT and AOT; no heap or reference aliasing mechanism is added.

Use target-native, declaration-order LLVM aggregate layout and matching private
source-function signatures. Supply DataLayout before lowering debug member
metadata. Layout queries must use the module-owned DataLayout copy: LLVM caches
StructLayout by raw, context-owned StructType pointers, so the longer-lived LLJIT
DataLayout must not cache types from disposable compilation contexts. Recycled
addresses can otherwise return an earlier generation's field offsets/count.
The target layout specification is unchanged; only cache ownership is bounded.
This does not define a public C aggregate ABI or stable packed layout.
The existing i32 host adapter remains enforced. Exact safety contracts include
ordered fields, nominal identities and default syntax; incompatible edits require
manual restart. Shared analysis now owns type/field declarations and references,
so LSP/editor integration does not need a second type model. VALUE_STRUCTS.md
records the working syntax, limits and next recommended design phase.

## Phase 13 receiver methods: approved M1 + I1, 12 October 2026

The owner reports Phase 12 `acf7355` passed hosted run #18 on all three platforms.
Approved owner-module receiver declarations and initializer groundwork, with
extension methods and public custom init execution deferred. I2's constructor
alternatives were not approved and must not be treated as implementation policy.

Use the existing Function/Type/checker/analysis model. Resolve nominal owned method
signatures before bodies, infer receiver mutation from validated writes and a finite
reverse call graph, then check readonly/addressability at every method call. Mutating
methods update actual local/nested storage; non-mutating methods permit readonly
values and temporaries. Ordinary parameters remain copies. Receiver storage is
selected once before arguments, which evaluate once left to right; aggregate
arguments snapshot their value at their evaluation position.

Represent internal storage with a typed IR Place on shared calls. The receiver
aliases slot zero through the existing shared LLVM lowering, rather than delayed
copy-back or another generator. Validate paths/types and non-mutating IR writes.
Preserve native failure contexts and private target layouts; add no external ABI,
heap/runtime system, LLVM alias promises or source references.

Owned method declarations/references and inferred effects feed tools/LSP and exact
restart contracts. Compatible bodies can reload; changed effects/signatures/types/
visibility/layout/defaults/bindings require restart. Reserve lifecycle receiver
syntax with an explicit deferred diagnostic, keeping Phase 12 construction intact.
RECEIVER_METHODS.md documents scope, tests and the remaining constructor gate.


## Phase 14 custom value initialization

Phase 14 approves C1 in INITIALIZATION_PROPOSAL.md. Constructor arguments bind only to
initializer parameters on custom types; positional/named binding and pure typed defaults
share checking. Recursive default construction runs children before the parent;
required-child availability and owner visibility are validated without exposing
uninitialized fields. Fresh receiver storage and explicit IR initialization flags use
shared LLVM lowering and existing failure propagation. Initializer body/default policy
and transitive helper bodies participate in exact restart contracts through an iterative
closure. Shared tooling records initializer/default spans and separates implicit
dependency edges from written navigation. INITIALIZATION.md describes the implemented
contract. The owner authorized only the identified obsolete Phase 13 init rejection
updates; all prior suites and lifecycle/output-protection assertions remain.


## Phase 15: approved GC/runtime foundation (G1–G6)

Use one noncopyable runtime owner, the existing heap/tracer, one metadata mutex/CV,
cooperative Managed/Parked/NativeSafe records and elected requesting collectors.
No sweep occurs after a failed rendezvous or before every participant freezes.
Waiters observe the current epoch under the mutex; losing collectors preserve their
original result. Partial mark failure rolls back before sweep. Detectable corrupt
root/frame invariants are fatal, rather than resuming unsafe Managed execution.

Use per-thread explicit root frames in the existing typed IR/shared lowerer, with
module-owned target layout offsets, real receiver storage, parameter/local/temporary
roots, fixed loop slots and all checked exit cleanup. Publish allocation into an
already registered slot and preserve no-GC argument/return handoffs. Statepoints,
stack maps, minimal liveness and LTO auditing remain deferred, not prohibited.

Require verified native-provider quiescence before freeing callback contexts or
persistent handles. Closing denies new admission; admitted callbacks may finish.
Self-drain is rejected. NativeSafe host boundaries cover blocking, joins, callback
reentry, dispatch and diagnostic copying. Keep GC object descriptors independent of
LLVM contexts; keep existing code-generation leases authoritative for retirement.

These are internal runtime/IR capabilities, not managed source syntax or a new CLI
entry model. GC_RUNTIME.md specifies interfaces, invariants and deferred work;
WORK_LOG.md records local evidence and pending hosted validation.

## Phase 16: managed structs and deep readonly views, 12 October 2026

The owner approved MANAGED_STRUCTS_PROPOSAL P1–P7, explicitly P3 deep readonly
capabilities and P4 reusable complete-object publication. Ordinary managed structs
now have identity/alias semantics, direct nullable/non-nullable references, cyclic
graphs, numeric/bool/reference-free value fields and owner-module methods. Custom
managed init and source reference-containing values remain deferred. Safe nullable
injection and checked same-nominal unwrap preserve capabilities.

One nominal model carries access metadata; machine type equality ignores readonly
but compatibility/tooling contracts compare it explicitly. A monotone capability/
effect pass operates on the validated AST, conservatively tracking returned aliases,
mutation and retention. Existing const formals express readonly parameters. It
does not duplicate nominal checking or freeze objects; independent mutable aliases
remain usable. Existing mutable bindings cannot acquire readonly references by
assignment; inferred readonly bindings remain readonly through rebinding.

One owning immutable type table plus nonowning descriptor edges permits recursive
managed layouts without C++ ownership cycles. Reference graph validation is distinct
from bounded inline-value layout expansion. Target layout/root flattening stays in
the shared LLVM implementation.

The new gc_construct operation stages complete fields and uses the existing Heap
under Runtime::construct_into. Budget collection and polling precede the lock; all
fallible allocation work precedes copying and last-step root publication. No
incomplete object is exposed. Existing private allocate proofs remain intact.
Source runtimes use a 256-allocation budget; tests force budgets 1/8 and allocation
failures. Failed rendezvous never sweeps. Runtime ownership is independent of code
leases; source replacement reuses the current runtime. AOT uses the runtime's
checked bootstrap with the existing entry signature and separately linked runtime.

Selected heap targets are rooted snapshots before arguments/RHS; reference receiver
rebinding is local, inline value receivers retain place semantics. Managed source
does not establish an arbitrary C ABI. See MANAGED_STRUCTS.md for exact semantics,
regressions, conservative limitations and pending hosted gates.

## Phase 17: managed custom initialization, 13 October 2026

The owner approved S1 + MI1–MI7 from MANAGED_INITIALIZATION_PROPOSAL.md, including
replacement of the obsolete managed-init deferral assertion with execution coverage.
Managed init runs on compiler-checked staged construction views; parent reference
identity begins only at successful complete-object commit. No P3/P4 exception,
incomplete managed root, pending-object heap or runtime allocator was introduced.

Reuse C1 parameter binding, pure defaults, source order, visibility and conservative
initialization dependency/reload rules. Types without init keep named fields.
Nonnullable managed bindings require explicit construction. Direct required managed
fields use definite readiness on every successful path; reference-free value fields
still finish their default construction before the body. No implicit managed child
allocation occurs. Effects on completed objects precede commit and are not rolled
back on body/allocation/collection failure; output publication remains last.

The already typed AST receives a bounded construction-use/readiness worklist.
Local aliases share origin/readiness; whole-view rebind/identity/cast/nullable
injection/return/retention/constructor arguments are rejected. All fields must be
ready before whole-view helper calls. Known source helpers/methods have private
passing contexts keyed by declaration and parameter mask, including recursive
calls and multiple aliases; 1024 contexts/project is a diagnosed resource bound.
No new ownership annotations, separate nominal checker or constructor backend.

The shared Type/Place/IR carry private construction capability and storage origins.
Independent CFG validation proves active scopes, reads, compatible calls, once-only
marked init and final commit. LLVM roots actual target-aligned staged field storage,
including helper formals, never the staging pointer. Existing gc_construct,
Runtime::construct_into, failure frames, module DataLayout and code leases remain
authoritative. Shared analyzed declarations expose context masks/required fields;
LSP and reload retain original source identities and conservative contracts.

Whole-self identity/escape, self-links inside init, borrowed returns/rebinding,
helper-assisted first initialization, reference-containing values, fallible results,
resources/deinit, runtime defaults and broader source features remain deferred.
MANAGED_INITIALIZATION.md defines the implemented boundary; WORK_LOG.md records
actual verification and pending seven-job hosted acceptance.

## Phase 18: reference-containing values, approved RV1–RV7

The owner approved per-reference-leaf readonly capabilities, fixed fresh-value-init
profiles, authority-neutral pure all-null templates, fixed existing storage capacity
and the C1/S1 boundary. REFERENCE_VALUE_STRUCTS.md records implemented behavior.
Inline copies retain object identity without cloning. Plain/const value formals and
receiver/return effects use finite inline reference paths and formal-leaf origins;
managed edges stop the shape. Selected heap-owner mutation is distinct from inline
storage mutation and from embedded-reference effects, including reference-free
inline receivers. No runtime permission state is introduced.

Type retains nominal identity independently of leaf qualifications. Shared logical
paths drive frontend facts, independent IR proof and target root offsets. The IR
checker derives effects and call/return capabilities rather than trusting frontend
summaries; a CFG must-proof rejects required-reference slot reads before valid
initialization. Private staged-view readonly places survive separately from managed
leaves. S1 view boxing/escape remains prohibited.

No-init construction shares one complete-field plan for aggregate and managed
results. Custom value init still begins with complete defaults; managed staged init
still commits identity last. Nested nonnullable publication guards precede commit.
The existing Frame/Slot roots, heap descriptors, runtime and generation leases are
reused. Source argument/return snapshots and selected owners remain rooted through
optimized calls, loops, collection and failure. Root homes may retain dead values
until return. Reference profiles, effects and initialization dependencies join exact
analysis/tooling/reload contracts. No public aggregate C ABI is asserted.

The sole obsolete managed-source field-deferral assertion becomes positive coverage.
Four additive source/AOT/CLI suites retain all 39 preceding suites; the seven-job CI
workflow is unchanged. Final commands/results and hosted limitations are recorded
in WORK_LOG.md. Richer unavailable-value initialization, readonly nominal heap fields,
managed FFI and broader lifetime/source features require separate design approval.

Phase 18's final capability audit adds explicit mutable-authority requirements to
finite effect summaries. Plain helper signatures require mutable leaves even when
their current body only reads. Inferred receiver/local alias origins remain
polymorphic; independent mutable capacities do not. Origin/profile closure precedes
requirement closure, avoiding premature assumptions about recursive helper results.
The independent IR proof derives initial capacities from stores/default observations,
preserves explicit output weakening and separately enforces private-stage place
readonly. Null/default leaves retain authority even when they have no origins.
Named leaf paths and field/formal declaration links accompany source diagnostics.


## Phase 19 — stored composition (U1–U7, owner approved)

Both named/unnamed using declarations denote ordinary stored fields on managed/value
owners; shorthand derives its field name from the type. Direct-member precedence
and one-hop accessible promotion use one member namespace. Overlapping names are
legal until ambiguous access; types/signatures/results never disambiguate.
Promotion elaborates to existing typed paths before effects/readiness/lowering,
without forwarding methods, new IR/LLVM representation or runtime changes.
Shared analysis records using markers and implicit dependencies; marker changes
require restart. Exact ownership/imports, C1/S1/P3/P4 and RV1–RV7 are preserved.
See COMPOSITION.md and the approved COMPOSITION_PROPOSAL.md.


## Phase 20 — managed inheritance and guarded virtual dispatch (I1–I8)

The owner approved I1–I8 in INHERITANCE_PROPOSAL.md. One managed base, exact
nominal equality with explicit ancestor assignability, original member ownership,
no inherited hiding/protected access, exact override signatures/visibility and
conservative family-wide reference effects extend the existing semantic model.
Ancestry real members outrank using; direct component promotion remains one-hop.

Physical storage embeds the full base payload at offset zero. Shared logical paths
map to actual nested indexes for fields, roots, stages, tracing and debug metadata.
Cached layout validation bounds combined inheritance/inline depth at 128, including
base tail padding and expanded-field limits. Legacy untyped IR proofs remain;
registered nominal source objects require complete construction, and every virtual
receiver must appear in its generation catalog.

A derived init has a mandatory first super.init prelude even for defaultable bases.
Caller/prelude snapshots precede oldest-base defaults/body, then each child phase.
One identity-free stage commits once as its most-derived type. Base completion uses
may/must dataflow, rejecting repeated backedge initialization and prefix publication
through aliases. Safe bounded prefix helpers are supported; unfinished virtual calls,
identity/escape/explicit casts/rebinding/self-links remain rejected. C1/S1/P3/P4 and
RV1–RV7 are preserved, including all-fields-valid-before-value-init.

The existing runtime interns immutable exact-key descriptors via transactional
NativeSafe admission. Heap records retain only code-independent identity/ancestry/
tracing metadata. Generation-owned guarded switch functions query the heap and call
verified implementations after releasing its short metadata lock. No object vptr,
per-method patch cells, hidden mutable AOT ID table or second allocator/root system
is introduced. AOT admits the same records before Managed entry; user entry behavior
is unchanged. Internal AOT symbol suffixes let the native harness link two versions
and prove new-code dispatch on an old object without inventing a public managed ABI.

Whole-generation leases remain authoritative. Hierarchy/layout/slot/signature/
access/effect/default/construction changes require restart; compatible body-only
override changes can reload. Constructor dependency closure includes all possible
completed-child virtual targets. Shared analysis records bases/slots/owners and
implicit target edges; navigation uses static/original declarations. Deferred public
features and acceptance evidence are in INHERITANCE.md and WORK_LOG.md.


## Phase 21 — structural interfaces (IF1–IF8)

The owner explicitly approved the managed-backed view architecture. Interface
identity is nominal; conformance is implicit, accessible and exact through static
real/inherited/one-hop promoted methods. Conversion captures original ownership and
route, including authorized private factory implementations. Same-interface equality
compares complete object identity. Nullable pairs are canonical; value-only erasure,
boxing and cross-interface discovery/conversions remain deferred.

Per-requirement conservative effects preserve receiver/argument provenance and P3/RV
capabilities without runtime permission tags. Shared member selection, complete
nominal/binding-universe recomputation and independent IR effects reject forged or
omitted evidence. C1/S1/P4 still exclude unfinished erasure/publication and require
valid fields; complete children can be used during initialization.

A private rooted receiver token selects managed snapshots/inline storage before
arguments. Shared Frame/Slot/heap maps trace object words, never witnesses/interior
addresses. Immutable witnesses extend the existing atomic Runtime metadata snapshot,
contain no code/AST pointers or leases, and survive allocating-generation retirement.
Generation plans reuse ordinary/virtual lowering and whole-call leases. Requirement,
access, binding, effect, layout, defaults and construction changes require restart;
constructor closure includes every eligible interface provider. Matching body-only
provider changes can reload. The native aggregate ABI remains compiler private.

Discovery limits are 65536 scope/view/interface combinations and 8388608 member
inventory visits; violations diagnose without truncating candidates. Existing layout,
stack and construction-context limits remain. INTERFACES.md documents the implemented
boundary; WORK_LOG.md distinguishes local results from the pending hosted gate.
