# Void.Language

VOID 2.0 is a new language, independent of the earlier C#-compatible VOIDC.
This repository now compiles graphs of actual `.void` files through one frontend, typed
semantic model, backend-independent IR, and shared LLVM 19.1.7 AOT/JIT lowering.
The implemented subset supports typed functions, all ten exact-width numeric types,
contextual literals, checked numeric `as`, bool expressions, local variables,
conditionals, `for` loops, readonly/compile-time constants, numeric/bool/nested value structs carrying managed references with custom initialization, managed structs with receiver methods, custom initialization, single inheritance and explicit virtual dispatch, and source-first
compile/runtime diagnostics. Compile-time errors now include stable codes, ranges,
related declarations and recovery across independent errors. Cooperative GC roots/thread coordination, leased function
replacement, and native C interoperability retain their architectural regression proofs.
The [runtime foundation](docs/GC_RUNTIME.md) uses shared optimized JIT/AOT lowering;
source-level managed structs now use that same runtime, including reference identity,
nullable cyclic graphs and deep readonly views. See [managed source](docs/MANAGED_STRUCTS.md)
for the supported contract and linked-AOT runtime dependency.
See [source compiler scope and decisions](docs/SOURCE_COMPILER.md).

Managed source example: `voidc run examples/managed_structs/main.void 10` returns
`23` after alias mutation, cyclic references and allocation-triggered collection.
The managed CLI tests also emit and separately link this source into native
executables, with and without LLVM optimization; these are distinct from the older
hand-built IR proofs. See the managed-source guide for complete link commands.

Managed inheritance example: `voidc run examples/inheritance/main.void 10` returns
`108` through an identity-preserving Base view and a derived override. Parameterized
base construction and source evaluation order are demonstrated by
`examples/inheritance/initialization.void` and `examples/inheritance/order.void`.
These run through the shared normal/O2 JIT and separately linked AOT, with precise
GC and code-independent object descriptors. See [inheritance scope](docs/INHERITANCE.md)
and [actual validation evidence](docs/WORK_LOG.md). Interfaces and source downcasts
remain deferred; no public managed-reference ABI is promised.

## Build on Linux

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
The optional [private fixed-call experiment](tests/research/call_envelope/README.md)
uses direct LLVM arguments and Runtime copy helpers, without envelope
permissions, store-identity protocols or a linked-byte certificate gate. It does
not enable native source arrays.

Verify your distribution actually provides 19.1.7. CMake rejects different SDK
versions. LLVM's development package depends on other development libraries;
keep them installed. In particular, missing `libzstd-dev` prevents the installed
LLVM CMake targets from linking. `libedit-dev` supplies the optional SDK
line-editing dependency. No alternate library paths or stub targets are used.

For optimized and instrumented builds, use separate directories with
`-DCMAKE_BUILD_TYPE=Release`, or `-DCMAKE_BUILD_TYPE=RelWithDebInfo
-DVOID_SANITIZERS=ON`. Run the same CTest suite in each directory.
LeakSanitizer requires a host that permits process inspection; the Codex sandbox
on the verified host required running sanitizer tests outside its tracing restriction.

CMake matches the selected SDK's RTTI setting across all linked C++ targets.
The complete official Linux archive has RTTI disabled; ASan/UBSan remain enabled,
but Clang's RTTI-dependent vptr check is unavailable in that ABI. A complete
RTTI-enabled LLVM 19.1.7 SDK supplies the separate vptr validation configuration.
See [the CI sanitizer investigation and coverage limits](docs/CI_SANITIZER_ABI.md).

Fixed arrays and [growable lists](docs/GROWABLE_ARRAYS.md) execute through
production JIT/AOT with numeric/bool, string, value-struct, managed/nullable/interface
and nested-array elements. Fixed children copy independently; lists share identity.
Checked nested mutation, count/push/remove, forward/reverse iteration, i32/i64 ranges
and heap-backed megabyte payloads use the existing coordinated Runtime. See
[fixed arrays](docs/FIXED_ARRAYS.md) and [source examples](examples/arrays/iteration.void).

## Compile and execute real source

```sh
build/debug/voidc check examples/architecture.void
build/debug/voidc run examples/architecture.void -1  # 7, native ORC JIT
build/debug/voidc run examples/architecture.void 0   # 42
build/debug/voidc run examples/architecture.void 1   # 43
build/debug/voidc run examples/architecture.void 10  # 52
build/debug/voidc emit examples/architecture.void -o build/debug/architecture.o
clang-19 build/debug/architecture.o -o build/debug/architecture
build/debug/architecture 10                        # 52, native AOT executable
```

Use matching LLD for the final link when required by your SDK:
`clang-19 -fuse-ld=/usr/lib/llvm-19/bin/ld.lld ...`.
On Windows, from the Visual Studio developer environment, link with
`clang architecture.o -llegacy_stdio_definitions -o architecture.exe`.
On macOS, use the pinned SDK's `clang architecture.o -o architecture`.
The object includes a native `main` adapter and actual source line information.
The executable prints its i32 result and exits successfully; an invalid argument
exits with status 2. The compiler does not launch a shell or linker itself.

`run` and the AOT executable accept one optional decimal i32 input, defaulting to
zero. They require `func entry(value: i32) -> i32`; internal functions can use
multiple typed parameters, any supported numeric/bool/value struct result, or no result. `check` accepts library
files without an entry. Append `--no-opt` to `run` or `emit` to disable LLVM O2.
Try [loops](examples/loops.void) and [recursive functions](examples/functions.void).

The older `--jit-proof` and `--emit-proof OBJECT` commands intentionally retain
their hand-built IR architecture harness. Their AOT adapter derives its input
from argument count; it is separate from the working source commands above.
The CTest suites preserve those native, GC and hot-reload proofs and add
frontend diagnostics and real source JIT/AOT integration tests.

## Value receiver methods

```python
func (p point) move(dx: i32, dy: i32):
    p.x += dx
    p.y += dy
```

Owner-module methods update mutable local/nested receivers; readonly values can
call methods proven non-mutating. Ordinary arguments, assignment and returns still
copy. Imported types retain nominal identity and method visibility. Phase 14 custom
`init` execution follows [C1 construction](docs/INITIALIZATION.md); types without
custom initializers retain safe defaults and named-field construction.

```sh
build/debug/voidc run examples/receiver_methods/main.void 10 --no-opt  # 301
build/debug/voidc run examples/receiver_methods/main.void 10           # 301
build/debug/voidc emit examples/receiver_methods/main.void -o build/debug/methods.o
clang-19 build/debug/methods.o -o build/debug/methods
build/debug/methods 10                                               # 301
```

These compile actual source through shared JIT/AOT lowering. See
[receiver semantics and limitations](docs/RECEIVER_METHODS.md).

## Numeric source compilation

All `i8/i16/i32/i64`, `u8/u16/u32/u64`, `f32/f64` types work through the same source
pipeline. Integer arithmetic wraps; numeric `as` is checked. Existing typed values
require explicit conversion, while compatible literals receive their expected type.
See [the approved numeric contract](docs/NUMERIC_TYPES.md).

```sh
build/debug/voidc run examples/numeric_types/main.void 10  # 52
build/debug/voidc emit examples/numeric_types/main.void -o build/debug/numeric-types.o
clang-19 build/debug/numeric-types.o -o build/debug/numeric-types
build/debug/numeric-types 10                            # 52
```

This example uses imported i64 functions, f32/f64 literals and calls, u64 wrapping,
and a checked i64-to-i32 result. These are parsed `.void` programs, distinct from
the older hand-built IR proofs. The CLI entry adapter remains i32(i32).

## Multi-file programs

```python
use "/math/calculator.void" as calculator
use "/helpers/numbers.void"

func entry(value: i32) -> i32:
    return calculator.double(value) + increment(value)
```

This complete project is in [examples/modules](examples/modules/main.void):

```sh
build/debug/voidc run examples/modules/main.void 10  # 31
build/debug/voidc emit examples/modules/main.void -o build/debug/modules.o
clang-19 build/debug/modules.o -o build/debug/modules
build/debug/modules 10                           # 31
```

The entry file's directory is the virtual root: `/` in source means that root,
and `./` or `../` resolves from the importing file. Imports use `/` on every OS
and exact filename case. Repeated normalized imports compile once. Simple names
work for unambiguous direct unaliased imports; explicit aliases provide qualified
calls. Functions are public by default, `private func` is file-private, and
`internal func` is available throughout this project through imports. Function-only
cycles and cross-file recursion work. Package imports, external root escapes and
imported symlinks are deferred with clear diagnostics. See
[module rules and provisional filesystem policies](docs/MODULE_SYSTEM.md).

## Constants and runtime diagnostics

The entry file's public/internal compile-time constants are visible throughout the
reachable project without importing the entry. Module constants support typed pure
expressions, forward references and cycle diagnostics. Local `const` can depend on
runtime values and cannot be reassigned; `var` copies remain mutable.

```sh
build/debug/voidc run examples/constants/main.void 0    # 50
build/debug/voidc run examples/constants/main.void 1    # 52
build/debug/voidc run examples/constants/main.void 10   # 70
build/debug/voidc run examples/constants/main.void -1   # 48
build/debug/voidc emit examples/constants/main.void -o build/debug/constants.o
clang-19 build/debug/constants.o -o build/debug/constants
build/debug/constants 10                             # 70
build/debug/voidc run examples/runtime_errors/main.void 0
# arithmetic.void:2:16: runtime error: Division by zero
# Original source line/caret, failing function, and actual source callers; exit 1.
```

The same native link options described above apply. Both JIT and AOT preserve
wrapping i32 arithmetic, widened signed division and safe zero checks. Runtime
errors return normally through native functions, preserve imported source locations,
and include up to 32 actual source callers with explicit truncation. AOT currently
prints diagnostics to stdout, JIT CLI to stderr. Changes to const-containing source
files conservatively require restart; old reload leases remain usable.
See [value semantics, policies and limitations](docs/VALUE_SEMANTICS.md), including
recommendations for future numeric conversions that still require approval.

## Windows and macOS

The local workflow in `.github/workflows/ci.yml` downloads complete official
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

Seven independent runners execute Ubuntu Debug/Release/Sanitizers, Windows
Debug/Release and macOS ARM64 Debug/Release. Each runs the full CTest inventory;
Debug jobs also test/package the extension and run real editor integration. The
owner reports Phase 17 passed all seven jobs in run #29, locking its 39 suites.
Phase 18 adds four reference-value suites (43 total) and requires a new hosted run
after owner review and push.
See [verified commands and results](docs/WORK_LOG.md).

## Continue development

Read [the Language Bible](docs/LANGUAGE_BIBLE.md),
[development strategy](docs/DEVELOPMENT_STRATEGY.md),
[architectural decisions](docs/DECISIONS.md),
[source compiler scope](docs/SOURCE_COMPILER.md),
[module system](docs/MODULE_SYSTEM.md),
[value semantics](docs/VALUE_SEMANTICS.md), and
[known issues](docs/KNOWN_ISSUES.md).
The language server and extension connect immutable document sessions to real LSP
clients. Managed source and reference-containing values use the coordinated runtime.
Review [reference-value semantics](docs/REFERENCE_VALUE_STRUCTS.md) before expanding
initialization or readonly field policies. Custom managed initialization uses the
approved staged construction contract; see
[managed initialization](docs/MANAGED_INITIALIZATION.md) before extending it.

## Structured compiler diagnostics

```sh
build/debug/voidc check examples/diagnostics/errors.void
# One failure reports VOID2001 (type), VOID2007 (readonly), VOID2003 (unknown function).
build/debug/voidc run examples/diagnostics/corrected.void 10
# 52; this is actual parsed-source native execution.
```

The same corrected source can be emitted and linked using the AOT instructions
above; normal and O2 tests execute both native modes. Invalid source never executes
or replaces an existing object. See [diagnostic codes, recovery and snapshots](docs/DIAGNOSTICS.md)
and [verified commands/results](docs/WORK_LOG.md). The user reports the Phase 9 baseline passed all hosted platforms.

Validated project analysis and native reload APIs are documented in
[ANALYSIS_COMPATIBILITY.md](docs/ANALYSIS_COMPATIBILITY.md). The real three-file
`examples/analysis/main.void` returns 53 at input 10. Its body-edit/reload test
changes that to 61 beside unchanged constants while retaining the old generation;
actual constant edits require manual restart. These source tests supplement the
original hand-built IR architectural proofs. The suite now contains fourteen
CTest suites, including all twelve from Phase 6.

[TOOLING_FOUNDATIONS.md](docs/TOOLING_FOUNDATIONS.md) describes read-only symbol
queries and conservative dependency invalidation over validated source snapshots.
The `tooling_demo` CTest runs the standalone in-process consumer in
`examples/tooling.cpp` against a disposable copied source project. All fourteen
Phase 7 suites remain; tooling/tooling_demo bring the total to sixteen.


Phase 9 adds [document revisions and tooling sessions](docs/TOOLING_SESSIONS.md).
Run `build/ci-debug/void_sessions_example examples/analysis` for an in-process
unsaved-edit, stale-query, failure/repair and compatibility demonstration using real
compiler records. It never writes the example files or publishes native code.
The original sixteen suites remain; sessions/sessions_demo bring the total to
eighteen. This is complete reanalysis with revision tracking; file watching and
partial compilation remain future work.


## Minimal language server

Phase 10 adds `voidls`, a single-threaded stdio LSP 3.17 server with full-document
synchronization, compiler diagnostics, go to definition and find references.

```sh
build/ci-debug/voidls --entry examples/analysis/main.void
# Waits for framed JSON-RPC input from an LSP client.
python3 tools/lsp_demo.py build/ci-debug/voidls
ctest --test-dir build/ci-debug -R '^lsp_' --output-on-failure
```

The demonstration launches the actual executable, opens a two-file source project,
navigates, edits an import without saving, observes an error and unavailable
navigation, repairs it, and verifies cleared diagnostics/fresh navigation before
shutdown. See [LANGUAGE_SERVER.md](docs/LANGUAGE_SERVER.md) for setup, UTF-16
positions, signed document versions, protocol limits and disk-refresh limitations.
All eighteen previous suites remain; lsp_positions/lsp_protocol/lsp_demo bring
the total to twenty-one. The server performs complete compiler analysis and
never executes source or publishes native replacements. The user reports Phase 10 passed hosted run #13 on Linux, Windows and macOS
(commit `ccfea79`); Phase 11 editor integration awaits its own hosted verification.


## VS Code and VSCodium

Phase 11 adds a local extension in [editors/vscode](editors/vscode/README.md).
Build `voidls` separately, then build/install the extension:

```sh
cd editors/vscode
npm ci
npm test
npm run package
code --install-extension dist/void-language-0.1.0.vsix
# Or: codium --install-extension dist/void-language-0.1.0.vsix
```

Development uses pinned Node.js 24.19.0. Configure `void.server.path` and
`void.project.entry` as absolute or single-workspace-relative paths; on Windows
select `voidls.exe`. Open `.void` source for compiler diagnostics, F12 definitions
and Shift+F12 references, including unsaved edits. Use **VOID: Restart Language
Server** and **Output → VOID** for lifecycle/logging. Native execution requires
workspace trust; only one project is managed per window. No server binary is bundled.
See [EDITOR_INTEGRATION.md](docs/EDITOR_INTEGRATION.md) for a complete real multi-file
example, development-host/test commands, restrictions and troubleshooting.

## Value structs (Phase 12)

Real source compilation now supports nested numeric/bool value structs with safe
and compile-time defaults, named-field construction, independent copies, field
mutation, parameters and returns. For example:

```sh
build/debug/voidc run examples/value_structs/main.void 10 --no-opt
build/debug/voidc run examples/value_structs/main.void 10
# Both print 46.
```

These commands compile the actual multi-file source project. The existing GC,
reload and C interoperability proofs remain separate examples. See
[the value struct contract and examples](docs/VALUE_STRUCTS.md) for native AOT
commands, shared lowering, compiler-private aggregate calling conventions and
current limitations.


Custom value initialization now supports owner-module `init` parameters, named and
positional arguments, pure defaults and recursive nested construction. Copies do
not rerun initialization. See [INITIALIZATION.md](docs/INITIALIZATION.md).

```sh
build/ci-debug/voidc run examples/initialization/main.void 10 --no-opt
build/ci-debug/voidc run examples/initialization/main.void 10
# Both return 166 from real imported source; native object/link commands are documented.
```


## Managed custom initialization

Actual source in `examples/managed_initialization/main.void` uses initializer
arguments/defaults, a required managed child, nested value initialization and
safe receiver helpers. Both normal and optimized JIT return 26 for input 10:

```sh
build/ci-debug/voidc run examples/managed_initialization/main.void 10 --no-opt
build/ci-debug/voidc run examples/managed_initialization/main.void 10
build/ci-debug/voidc emit examples/managed_initialization/main.void -o build/managed-init.o
clang++-19 build/managed-init.o build/ci-debug/libvoid_runtime.a -pthread \
  --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/managed-init
build/managed-init 10 # 26, separately linked native executable
```

The parent gains identity only after successful initializer execution and complete
commit. Construction views cannot escape or compare identity; readonly views and
ordinary reference aliases retain their established rules. These are parsed source
programs, distinct from the older hand-built IR architectural proofs.
See [the source contract](docs/MANAGED_INITIALIZATION.md) for failure behavior,
required-field/helper restrictions, platform linking and deferred features.


## Reference-containing values (Phase 18)

Value copies duplicate inline storage while retaining embedded managed identities.
Readonly authority follows each reference leaf through copies, calls and nested
fields. A var copy can change its scalar fields without thawing readonly referents.

```sh
build/ci-debug/voidc run examples/reference_values/copies.void 10 --no-opt
build/ci-debug/voidc run examples/reference_values/copies.void 10
build/ci-debug/void_reference_values_cli_copies_normal 10
build/ci-debug/void_reference_values_cli_copies_optimized 10
```

Each prints 32. The latter two are separately linked native executables from actual
source-emitted objects, not hand-built IR proofs. The new source/AOT tests also
exercise nested cycles with aggregate-only precise roots, padding, optimized
handoffs, failure/recovery and selected heap owners. See
[the approved source contract](docs/REFERENCE_VALUE_STRUCTS.md) for C1 defaults,
fixed initializer profiles, all-null templates, readonly limits and verification.


Stored composition supports `using motion: movement` and `using combat`, qualified
access and unique one-hop field/method promotion. Both forms retain real storage
and existing value/reference/readonly semantics. See [COMPOSITION.md](docs/COMPOSITION.md).

```sh
build/ci-debug/voidc run examples/composition/main.void 10 # 111
```

This compiles real source through the shared frontend/IR/LLVM path. Phase 20 adds
managed inheritance and component virtual dispatch. Phase 21 adds managed-backed
structural interface views. Phase 22 adds immutable UTF-8 strings, quoted literals
and escapes, empty defaults, content equality and concatenation, pure templates,
managed/value fields and safe shared JIT/AOT ownership; see [STRINGS.md](docs/STRINGS.md).
String indexing/length/nullability APIs, interpolation evaluation, formatting and
print remain deferred. Transitive promotion, stateless mixins and standalone
value-interface erasure remain deferred.


Structural interfaces use real source compilation, with no `implements` declaration:

```swift
interface readable:
    func read() -> i32
```

Completed managed values satisfying accessible exact method requirements convert
in arguments, assignments, returns and `as`. Interface calls preserve target identity,
readonly capabilities and selected embedded storage. See
[INTERFACES.md](docs/INTERFACES.md) for the implemented scope and deferred features.

```sh
build/ci-debug/voidc run examples/interfaces/main.void 10 # 28
build/ci-debug/voidc run examples/interfaces/static.void 10 # 720
```

These exercise the source frontend and shared native backend, separately from the
older hand-built architectural IR proofs. WORK_LOG.md records local/hosted validation.
