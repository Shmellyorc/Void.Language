# Void.Language

VOID 2.0 is a new language, independent of the earlier C#-compatible VOIDC.
This repository now compiles actual `.void` files through one frontend, typed
semantic model, backend-independent IR, and shared LLVM 19.1.7 AOT/JIT lowering.
The initial subset supports typed functions, i32/bool expressions, local variables,
conditionals, `for` loops, and source diagnostics. Experimental GC, leased function
replacement, and native C interoperability remain separate architectural proofs.
See [source compiler scope and decisions](docs/SOURCE_COMPILER.md).

## Build on Linux

Requires CMake 3.24+, a C++20 compiler, and the complete **LLVM 19.1.7** development
SDK. The tests also require matching Clang 19.1.7 and `llvm-dwarfdump`.
On Debian-family distributions that provide these versions:

```sh
sudo apt-get install build-essential cmake ninja-build llvm-19-dev clang-19 lld-19 libffi-dev libedit-dev libxml2-dev libzstd-dev zlib1g-dev libz3-dev
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
multiple typed parameters, bool results, or no result. `check` accepts library
files without an entry. Append `--no-opt` to `run` or `emit` to disable LLVM O2.
Try [loops](examples/loops.void) and [recursive functions](examples/functions.void).

The older `--jit-proof` and `--emit-proof OBJECT` commands intentionally retain
their hand-built IR architecture harness. Their AOT adapter derives its input
from argument count; it is separate from the working source commands above.
The eight CTest suites preserve those native, GC and hot-reload proofs and add
frontend diagnostics and real source JIT/AOT integration tests.

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
ASan/UBSan. The user confirmed the previous foundation passed on all three
platforms. This source compiler phase has passed local Linux checks; the expanded
eight-suite matrix still needs a hosted run after the user commits and pushes.
See [verified commands and results](docs/WORK_LOG.md).

## Continue development

Read [the Language Bible](docs/LANGUAGE_BIBLE.md),
[development strategy](docs/DEVELOPMENT_STRATEGY.md),
[architectural decisions](docs/DECISIONS.md),
[source compiler scope](docs/SOURCE_COMPILER.md), and
[known issues](docs/KNOWN_ISSUES.md).
The next phase should establish module/import identity and a broader executable
interface around this frontend, with deliberate numeric and grammar decisions.
Resolve runtime/root integration before introducing managed language types.
