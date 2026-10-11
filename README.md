# Void.Language

VOID 2.0 is a new language, independent of the earlier C#-compatible VOIDC.
This repository now compiles graphs of actual `.void` files through one frontend, typed
semantic model, backend-independent IR, and shared LLVM 19.1.7 AOT/JIT lowering.
The implemented subset supports typed functions, all ten exact-width numeric types,
contextual literals, checked numeric `as`, bool expressions, local variables,
conditionals, `for` loops, readonly/compile-time constants, and source-first
compile/runtime diagnostics. Compile-time errors now include stable codes, ranges,
related declarations and recovery across independent errors. Experimental GC, leased function
replacement, and native C interoperability remain separate architectural proofs.
See [source compiler scope and decisions](docs/SOURCE_COMPILER.md).

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
multiple typed parameters, any supported numeric/bool result, or no result. `check` accepts library
files without an entry. Append `--no-opt` to `run` or `emit` to disable LLVM O2.
Try [loops](examples/loops.void) and [recursive functions](examples/functions.void).

The older `--jit-proof` and `--emit-proof OBJECT` commands intentionally retain
their hand-built IR architecture harness. Their AOT adapter derives its input
from argument count; it is separate from the working source commands above.
The CTest suites preserve those native, GC and hot-reload proofs and add
frontend diagnostics and real source JIT/AOT integration tests.

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

All jobs build and execute the same Debug and Release tests; Linux also runs
ASan/UBSan. The user reports Phase 9 passed all three platforms at commit `02a9aa7`,
Actions run #12. Phase 10 requires another hosted run after the user commits
and pushes.
See [verified commands and results](docs/WORK_LOG.md).

## Continue development

Read [the Language Bible](docs/LANGUAGE_BIBLE.md),
[development strategy](docs/DEVELOPMENT_STRATEGY.md),
[architectural decisions](docs/DECISIONS.md),
[source compiler scope](docs/SOURCE_COMPILER.md),
[module system](docs/MODULE_SYSTEM.md),
[value semantics](docs/VALUE_SEMANTICS.md), and
[known issues](docs/KNOWN_ISSUES.md).
The minimal language server now connects immutable document sessions to real LSP
clients. Next validate hosted integration, then consider editor packaging and
explicit disk refresh/cancellation before partial compilation.
Resolve runtime/root integration before introducing managed language types.

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
never executes source or publishes native replacements. Phase 10 is awaiting
its own hosted Linux/Windows/macOS verification; see WORK_LOG.md for local results.
