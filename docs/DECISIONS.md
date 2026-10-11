# Architectural decisions

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

This establishes a basic reference contract, not a production collector. The heap
is single-threaded. There is no allocator cache, concurrent collection, interior
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
