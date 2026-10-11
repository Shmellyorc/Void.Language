# Void.Language

VOID 2.0 is a new language, independent of the earlier C#-compatible VOIDC.
This repository currently provides a **compiler architecture foundation**:
validated, backend-independent proof IR; shared LLVM AOT/JIT lowering;
leased function replacement; an experimental precise managed heap; and a C ABI
fixture with UTF-8, owned resources and callbacks.

**There is no `.void` parser or usable application language yet.**
`examples/architecture.void` documents the equivalent of the hand-built IR;
it is not read by the compiler. Full GC and automatic source hot reload are
not implemented. See [known issues](docs/KNOWN_ISSUES.md).

## Build on Linux

Requires CMake 3.24+, a C++20 compiler, and the complete **LLVM 19.1.7** development
SDK. The tests also require matching Clang 19.1.7 and `llvm-dwarfdump`.
On Debian-family distributions that provide these versions:

```sh
sudo apt-get install build-essential cmake ninja-build llvm-19-dev clang-19 lld-19 libffi-dev libedit-dev libxml2-dev libzstd-dev zlib1g-dev libz3-dev
cmake -S . -B build/debug -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
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

## Execute the architecture harness

```sh
build/debug/voidc --jit-proof
# Prints 42 from native ORC-generated code.
build/debug/voidc --emit-proof build/debug/proof.o
clang-19 build/debug/proof.o -o build/debug/proof
build/debug/proof
# Prints 7 for runtime input -1.
build/debug/proof first
# Prints 42 for runtime input 0.
build/debug/proof first second
# Prints 43 for runtime input 1.
```

The AOT harness derives its input from the argument count. LLVM emits the object
through `TargetMachine`; Clang invokes the platform linker. No shell command is
constructed inside the compiler. The integration test uses paths containing spaces
and checks all three outputs and the object's debug information.

## Windows and macOS

The local workflow in `.github/workflows/ci.yml` downloads complete official
LLVM 19.1.7 development archives, including headers and static libraries.
Windows uses the MSVC/Windows SDK environment and `clang-cl`, with the release
MSVC dynamic runtime and iterator ABI matching the prebuilt LLVM libraries in
both build configurations. The native AOT link uses the SDK's `clang` driver.
macOS selects the official archive matching the runner architecture and uses
the Xcode SDK. Linux installs development dependencies explicitly.

All jobs build and execute the same Debug and Release tests; Linux also runs
ASan/UBSan. This workflow has **not run on GitHub**. Windows and macOS are
**unverified**. No repository, commit, push or remote change was made.

## Continue development

Read [the Language Bible](docs/LANGUAGE_BIBLE.md),
[development strategy](docs/DEVELOPMENT_STRATEGY.md),
[architectural decisions](docs/DECISIONS.md), and
[the work log](docs/WORK_LOG.md). The next phase should formalize the small
frontend grammar and diagnostics, then connect source functions to the existing
IR and both execution paths. Resolve optimized root lowering before adding
managed language types.
