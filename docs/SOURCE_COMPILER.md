# First real source compiler

The current [Stage 8 application contract](STAGE8_APPLICATION_NATIVE.md) records
explicit `--app` hosting alongside the preserved legacy CLI described below.

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Stage 3 extends this same pipeline with native `{K: V}` dictionaries; see DICTIONARIES.md for the implemented source/runtime contract, supported keys and deferred result-based get.

Phase 22 adds the owner-approved UTF-8 string foundation through this same pipeline.
See [STRINGS.md](STRINGS.md) for immutable content, escapes, empty/default templates,
managed/value storage, exact equality/concatenation, roots, failures, reload and
native copying boundaries. Future indexing/length/nullability/interpolation APIs
remain outside this implementation. WORK_LOG.md records actual local verification
and the pending seven-job hosted gate.

Phase 20 adds managed single inheritance, identity-preserving ancestor conversions, explicit virtual/override/super dispatch and staged base initialization. See [INHERITANCE.md](INHERITANCE.md) for the current boundary and WORK_LOG.md for actual verification.

Implemented locally on 10 October 2026, using LLVM 19.1.7. LANGUAGE_BIBLE.md
remains the language-design source of truth; this document describes the working
subset and explicitly provisional choices. Multi-file compilation is now implemented;
see [MODULE_SYSTEM.md](MODULE_SYSTEM.md) for current import and ownership rules.
Phase 6 updates the numeric subset on 11 October 2026; see NUMERIC_TYPES.md.
Phase 12 adds numeric/bool/nested value structs; see [VALUE_STRUCTS.md](VALUE_STRUCTS.md)
for approved defaults, source syntax, shared aggregate IR and private ABI boundaries.
Earlier foundation-only status in historical documents predates source compilation.

Phase 16 adds ordinary managed source structs, nullable references, deep readonly
views and runtime-backed JIT/linked AOT; see [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md).
Phase 17 adds custom managed initialization with checked construction views,
required-reference readiness, safe helper contexts and complete-object commit; see
[MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md).

## Building the compiler

VOID requires the complete LLVM **19.1.7** SDK, including headers and development
libraries. For a first program, start with the
[root README](../README.md#a-small-example).

### Linux dependencies and build

Requires CMake 3.24+, a C++20 compiler, and the complete **LLVM 19.1.7** development
SDK. The tests also require Python 3.9+, matching Clang 19.1.7 and `llvm-dwarfdump`.
On Debian-family distributions that provide these versions:

```sh
sudo apt-get install build-essential cmake ninja-build python3 llvm-19-dev clang-19 lld-19 libffi-dev libedit-dev libxml2-dev libzstd-dev zlib1g-dev libz3-dev
cmake -S . -B build/debug -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 4
ctest --test-dir build/debug --output-on-failure
```

Machine-code ownership certification is optional research. Ordinary JIT/AOT
compilation uses typed IR validation, LLVM verification and the shared lowerer;
it does not require allocation lineage or final-byte certificates. To build the
existing experiments, configure with `-DVOID_LLVM_RESEARCH=ON`, build
`void_arrays_storage_audit`, and run
`ctest --test-dir build/debug -L research --output-on-failure`. Research checker rejections are not LLVM correctness
failures or production compilation blockers. The research-only 64 KiB storage
budget is not a production limit.
The optional [private fixed-call experiment](../tests/research/call_envelope/README.md)
uses direct LLVM arguments and Runtime copy helpers, without envelope
permissions, store-identity protocols or a linked-byte certificate gate. It is
separate from already implemented production source arrays.

Verify your distribution actually provides 19.1.7. CMake rejects different SDK
versions. LLVM's development package depends on other development libraries;
keep them installed. In particular, missing `libzstd-dev` prevents the installed
LLVM CMake targets from linking. `libedit-dev` supplies the optional SDK
line-editing dependency. No alternate library paths or stub targets are used.

For optimized and instrumented builds, use separate directories with
`-DCMAKE_BUILD_TYPE=Release`, or `-DCMAKE_BUILD_TYPE=RelWithDebInfo
-DVOID_SANITIZERS=ON`. Run the same CTest suite in each directory.
LeakSanitizer requires a host that permits process inspection and cannot run
under ptrace; use an environment without that tracing restriction.

CMake matches the selected SDK's RTTI setting across all linked C++ targets.
The complete official Linux archive has RTTI disabled; ASan/UBSan remain enabled,
but Clang's RTTI-dependent vptr check is unavailable in that ABI. A complete
RTTI-enabled LLVM 19.1.7 SDK supplies the separate vptr validation configuration.
See [the CI sanitizer investigation and coverage limits](CI_SANITIZER_ABI.md).

### Native execution and linking

```sh
build/debug/voidc check examples/architecture.void
build/debug/voidc run examples/architecture.void 10
# 52
build/debug/voidc emit examples/architecture.void -o build/debug/architecture.o
clang-19 -fuse-ld=/usr/lib/llvm-19/bin/ld.lld build/debug/architecture.o -o build/debug/architecture
build/debug/architecture 10
# 52
```

`run` uses the native ORC JIT. `emit` produces a host-native object with a `main`
adapter and source line information; the compiler does not launch a shell or
linker. Both execution modes accept one optional decimal i32 input, defaulting to
zero. The adapter requires `func entry(value: i32) -> i32` (any parameter name).
The executable prints its i32 result and exits successfully; invalid input exits
with status 2. `check` also accepts libraries without an entry function. Append
`--no-opt` to `run` or `emit` to disable LLVM O2.

For managed programs, link the VOID Runtime and its dependencies as described in
[managed structs](MANAGED_STRUCTS.md). Feature-specific native examples belong in
[receiver methods](RECEIVER_METHODS.md), [initialization](INITIALIZATION.md),
[managed initialization](MANAGED_INITIALIZATION.md),
[reference-containing values](REFERENCE_VALUE_STRUCTS.md),
[inheritance](INHERITANCE.md) and [interfaces](INTERFACES.md).

The older `--jit-proof` and `--emit-proof OBJECT` options are hand-built IR
architecture harnesses, separate from source compilation. Their AOT adapter uses
argument count. They are not substitutes for the source commands above.

### Windows and macOS

The [CI workflow](../.github/workflows/ci.yml) downloads complete official
LLVM 19.1.7 development archives, including headers and static libraries.
Windows uses the MSVC/Windows SDK environment and `clang-cl`, with the release
MSVC dynamic runtime and iterator ABI matching the prebuilt LLVM libraries in
both build configurations. The native AOT link uses the SDK's `clang` driver,
with `-llegacy_stdio_definitions` to resolve the direct native `printf` symbol
through the active MSVC library search paths. Use a Visual Studio developer
environment (CI initializes it before configuration and testing).
macOS selects the official archive matching the runner architecture and uses
the Xcode SDK. Linux installs development dependencies explicitly and selects
the same archive's LLD 19.1.7 because its static LLVM libraries contain bitcode.
CMake matches the LLVM backend's RTTI setting to the selected SDK, retaining
exception support. Configuration links and executes an ORC host as an SDK check.
On Windows, stale exported DIA library paths are resolved against the active
Visual Studio DIA SDK (`VSINSTALLDIR`); an explicit real
`-DVOID_DIA_GUIDS_LIBRARY=.../diaguids.lib` can also be supplied.

On Windows, link an unmanaged source object from the Visual Studio developer
environment with `clang architecture.o -llegacy_stdio_definitions -o architecture.exe`.
On macOS, use the pinned SDK's `clang architecture.o -o architecture`.

The established seven-job gate covers Ubuntu Debug/Release/Sanitizers, Windows
Debug/Release and macOS ARM64 Debug/Release. Stage 6 is hosted locked at CI #78;
Stage 7's Linux local acceptance does not certify its new Windows/macOS tooling.
See [current status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and
[local evidence](stage7/ACCEPTANCE.md) for the distinction.

## Working pipeline

`SourceFile` validates UTF-8 and retains original byte offsets. The indentation
lexer emits tokens, newlines and block boundaries. The recursive-descent parser
builds an AST per file. The module graph resolves all function signatures first, then
semantic analysis checks
lexical bindings and exact types, annotates expressions and determines possible
control-flow exits. Only validated ASTs lower into the shared VOID IR. The IR
validator checks its independent invariants before LLVM lowering.

The LLVM backend compiles that same IR for both ORC JIT and TargetMachine object
emission. There is no architecture-example recognizer or second source code
generator. Changing a source literal or function name changes both native outputs.
Compiler responsibilities stop at code generation; the existing experimental
heap and thread coordinator serve managed source through explicit runtime ownership
and AOT bootstrap; reload leases and the C bridge remain runtime/host responsibilities.

Source `private` function definitions now use LLVM internal linkage, except when
explicitly selected as the host/JIT or AOT entry. All source files in an import
graph lower into one LLVM module, so their private implementation calls resolve
locally. Public/project-visible source functions, legacy native definitions,
trusted C imports, Runtime ABI declarations and externally consumed metadata
retain their existing linkage. Virtual/interface helpers were already internal.
Names, generation suffixes, calling conventions and source visibility are
unchanged. Debug subprogram metadata reflects local linkage. This is a function
construction rule, not an internalization pass or a new native FFI contract.

`pass` is supported as a standalone no-op in every ordinary statement block,
including methods and initializers. Semantic analysis preserves fallthrough and
required-return/field-readiness checks; lowering emits no IR operation or runtime
call. Empty blocks remain invalid, and interface requirements remain bodyless.
The canonical syntax and examples are in LANGUAGE_BIBLE.md Section 27.2;
`examples/pass.void` exercises nested control flow and subsequent execution through
normal/O2 JIT and separately linked AOT in the existing frontend/source_cli suites.
The editor grammar assigns `pass` the same control-keyword scope as `return`.

## Syntax and supported semantics

```swift
func sum(left: i32, right: i32) -> i32:
    return left + right

func entry(value: i32) -> i32:
    var total: i32
    var index = 0
    for index < value:
        index++
        if index % 2 == 0:
            continue
        total += sum(index, 1)
    for:
        if total >= 20:
            break
        total += 3
    return total
```

- Functions use `func name(parameter: type, ...) -> type:`. Module-level
  `private`, `public` and `internal` modifiers are supported; see MODULE_SYSTEM.md. Parameters require
  types. Omit the return annotation for no result. Empty `return` or fallthrough
  is valid only for a no-result function. Recursive and forward calls work.
- All ten exact-width numeric types plus bool and the value structs described in
  VALUE_STRUCTS.md work. Contextual literals and
  explicit checked numeric `as` follow [NUMERIC_TYPES.md](NUMERIC_TYPES.md).
  Calls check positional argument counts and exact types after literal adaptation.
- `var x = expression`, `var x: i32 = expression`, and `var x: i32` work.
  Annotated variables without an initializer receive safe defaults (including
  declared/recursive value struct defaults). Parameters
  and var locals are mutable. Local readonly const and module compile-time
  constants are now implemented; see VALUE_SEMANTICS.md. Local bindings are visible
  after declaration and only within their lexical scope. Shadowing visible locals, parameters or function names is
  rejected; sibling scopes can reuse names.
- Assignment `=`, arithmetic compounds `+= -= *= /= %=`, and standalone postfix
  `++`/`--` work. Increment/decrement cannot be used as expression values.
- Arithmetic uses unary `+`/`-`, `+ - * / %`; comparisons use `< <= > >= == !=`.
  Equality accepts matching numeric or bool operands; ordering requires matching numeric types.
- Boolean operators are `not`, `and`, `or`. `and`/`or` short-circuit at runtime.
  Arguments and binary operands evaluate left to right. From weakest to strongest:
  `or`, `and`, `not`, comparisons, addition/subtraction, multiplication/division/
  remainder, numeric `as`, unary arithmetic. Parentheses override precedence. Comparisons do
  not implement chained-comparison semantics; write `a < b and b < c`.
- `if condition:`, `else if condition:`, and `else:` nest normally. Conditions
  require bool. Every reachable end of a value-returning function needs a return.
  Analysis is conservative for conditional branches and conditional loops;
  unbounded `for:` and literal `for true:` need no return after the loop unless a
  reachable break can exit it. Unreachable statements still receive semantic
  checks, and are omitted from lowering.
- All loops use `for`: conditional `for condition:` or unbounded `for:`.
  `continue` reevaluates the nearest loop condition; `break` exits the nearest
  loop. Collection/range iteration is rejected until its types exist.
- Function calls can be statements, including no-result calls. Other standalone
  value expressions are rejected.

## Source and diagnostics

Files must be valid UTF-8; a leading UTF-8 BOM is accepted. LF, CRLF and CR line
endings work, as does a missing final newline. Comments start with `#` (including
`##` documentation-style lines), and may contain Unicode. Indentation uses
spaces with consistent enclosing levels; four spaces are recommended, rather
than imposed. Tabs are diagnosed. Blank/comment lines do not open blocks.
Expressions and declarations are single-line in this subset.

Names currently accept lowercase ASCII letters, digits after the first character,
and underscores. This does not settle future Unicode identifiers or the full
language naming policy. Unsupported characters and types are diagnosed.
Locations are one-based file/line/Unicode-scalar column positions, with the actual
source line and a caret. Scalar columns are not terminal display widths for wide
or combining characters. Diagnostics now collect independent errors with stable codes, ranges, related
declarations and bounded recovery; see [DIAGNOSTICS.md](DIAGNOSTICS.md).

For example, `while value > 0:` produces a VOID diagnostic at that keyword:

```text
example.void:2:5: error: VOID uses 'for' for all loops.
    code: VOID1104

 2 |     while value > 0:
   |     ^^^^^
    Hint: Replace 'while condition:' with 'for condition:'.
```

Invalid indentation/tokens, unresolved names, duplicate declarations, argument
counts, type mismatches, invalid returns and missing return paths are rejected
before LLVM. Malformed UTF-8 is rejected even in comments.

## Approved numeric behavior

Phase 6 extends the existing i32 wrapping/division contract to the exact-width
integer family and adds IEEE f32/f64 arithmetic, contextual literals and checked
numeric casts. See [NUMERIC_TYPES.md](NUMERIC_TYPES.md) for the approved rules,
shared software arithmetic, LLVM guards, diagnostics and precise remaining limits.
Integer division guards zero and MIN/-1 before dangerous LLVM operations;
float division by zero follows IEEE. No Language Bible text was changed.

## CLI and native executable boundary

```sh
voidc check program.void
voidc run program.void [decimal-i32-input] [--no-opt]
voidc emit program.void -o program.o [--no-opt]
```

`check` needs no entry function. `run` and `emit` currently require an
`entry(i32) -> i32` adapter, with any parameter name. This is the CLI host ABI,
not a restriction on language function signatures. Input defaults to zero.
O2 is the default; `--no-opt` selects unoptimized LLVM lowering. `emit` produces a
host-native object; a matching Clang driver links it (see README). Its native
`main` accepts the same optional input, prints the result and exits with status
zero; malformed/out-of-range input or extra arguments exit with status 2.

Source symbols encode their declaring module and function identity before the C
adapter is added, allowing same-named functions in different modules and helpers
named `main`, `printf` or `strtoll` without native adapter collisions. Stable
module identity is implemented; public library/export ABI remains deferred. Generated line
tables refer to the real input file. Variable debug records are not implemented.

## Validation and next phase

The existing six architectural suites remain, with `frontend` and `source_cli`
added to the same test executable/build. Frontend checks run three real files
and thirteen independent programs through normal/O2 ORC, plus over fifty negative
and boundary cases. Integration tests check, emit, link, verify DWARF and execute
three actual files with normal/O2 JIT and AOT, using paths with spaces. A modified
architecture source changes `42` to `13` and renames its helper to `main`; both
engines then return 23 for input 10. Tests retain the native `printf` proof.
See WORK_LOG.md for actual configuration commands and results.

The user confirmed this original eight-suite frontend phase passed Linux,
Windows and macOS Actions. The next module phase adds two suites and is documented
in MODULE_SYSTEM.md and WORK_LOG.md. The user reports that ten-suite phase also
passed all three hosted platforms. The user reports Phase 4 value semantics passed Actions run #6 across all three
platforms. Phase 5 diagnostic recovery requires another hosted run.

Current constants, arithmetic and runtime errors are documented in
[VALUE_SEMANTICS.md](VALUE_SEMANTICS.md). Structured diagnostic recovery is now implemented in [DIAGNOSTICS.md](DIAGNOSTICS.md).
Phases 6–11 subsequently completed numeric types, validated analysis, tooling
sessions, LSP and editor integration. Phase 11 run #16 passed all three hosted
platforms according to the owner. Phase 12 adds VALUE_STRUCTS.md and the
value_structs_cli suite, preserving all 21 previous suites. The subsequent
receiver/initialization and GC phases completed that gate. Current managed source
and the next deferred initialization decisions are documented in MANAGED_STRUCTS.md.

## Phase 13 receiver methods

Supported value structs now have owner-module receiver declarations and ordinary
value.method calls, including imported/nested receivers, inferred readonly effects,
normal/O2 JIT/AOT and owned tooling references. See RECEIVER_METHODS.md. Custom
init execution was deferred under Phase 13 I1; Phase 14 approved C1 now implements
it as documented in INITIALIZATION.md. No-init named construction and the CLI
check/run/emit i32 entry adapter remain available.


## Phase 14 custom value initialization

INITIALIZATION.md documents approved C1 custom value constructors, typed
positional/named arguments, pure defaults, recursive availability/access checks and
lifecycle diagnostics. Fresh construction uses shared receiver Places and explicit IR
invocation metadata; source execution and optimized/unoptimized JIT/AOT share the same
lowering. Required-child wrappers, pure contexts, readonly publication and conservative
initializer/helper reload contracts remain explicit boundaries.

Phase 18 implements owner-approved reference-containing value structs, per-leaf
readonly capability/effect checking, complete construction and precise roots through
normal/O2 JIT and native AOT. See [REFERENCE_VALUE_STRUCTS.md](REFERENCE_VALUE_STRUCTS.md);
earlier phase-only type exclusions in this historical guide are superseded there.


## Phase 19 composition

Owner-approved stored `using name: type` and `using type` composition, one-hop
member promotion and original embedded receiver mutation are implemented through
the existing paths. See [COMPOSITION.md](COMPOSITION.md) for exact lookup, access,
readonly, initialization, tooling and reload behavior. This adds neither subtyping
nor stateless reuse and does not change C1/S1/P3/P4/RV1–RV7. Phase 20 adds managed
inheritance and component virtual dispatch as described in INHERITANCE.md. Phase 21 adds managed-backed structural interfaces as documented in INTERFACES.md.
Transitive promotion, stateless reuse and has remain deferred.


## Phase 21 structural interfaces

Owner-approved IF1–IF8 implement bodyless interface requirements, implicit exact
structural conformance, managed-backed identity views, nullable values, readonly
capabilities and original-storage receiver dispatch. Locals, fields, copied values,
arguments and returns share the same frontend/IR/LLVM pipeline and precise roots.
Private factory exposure retains conversion-scope access proof; calls navigate to
requirements. Independent IR validation recomputes complete nominal/binding universes
and effects. See [INTERFACES.md](INTERFACES.md) for the source contract, budgets,
publication, reload policy, deferred value erasure/boxing and private ABI boundaries.

## Stage 8 application hosting

Sprint 4's [application contract](STAGE8_APPLICATION_NATIVE.md) adds explicit
`voidc run <file-or-directory> --app` and
`voidc emit <file-or-directory> -o <object> --app`, retaining `--no-opt` and Debug
run caching. Applications accept no numeric operand and return their status
without printing it. The linked executable likewise accepts no arguments.
Invalid invocation exits 2; compile/runtime failure exits 1. Platform exit-code
representation applies to explicit i32 results.

The selected source may contain executable top-level statements or synchronous
zero-argument main, with no result or i32. These competing entry forms reject
when combined. Top-level fallthrough/bare return yields 0; an i32 return supplies
status. Explicitly typed main requires a return on every reachable path. Imported
executable statements reject. Top-level variables remain body locals. Declaration-
only legacy entry programs retain their printed numeric adapter and precedence
over an ordinary main helper. Use existing legacy commands without `--app`.

Implementation and local acceptance status are maintained in the linked contract;
this does not grant editor execution, native import trust or hosted acceptance.
