# VOID Language

VOID is a statically typed language for native applications, with indentation-based
syntax, value and managed types, and automatic memory management. Its compiler uses
one frontend and typed intermediate representation for LLVM ahead-of-time (AOT)
compilation and just-in-time (JIT) execution.

VOID is under active development and is independent of the earlier
C#-compatible VOIDC. The compiler implements a working language subset; the language
design documents also contain proposals that are not yet implemented.

Stage 8 Sprint 1 is implemented locally: project/module discovery, import
IntelliSense and source documentation (423/423 consolidated local suites passed).
Wrench review and hosted acceptance remain pending. [The sprint ledger](docs/LANGUAGE_COMPLETENESS_APPROVALS.md)
and [acceptance record](docs/stage8/ACCEPTANCE.md) distinguish approved, locally
verified and hosted-locked work. Stage 7 is owner-confirmed hosted locked at
`f704d75f`, CI #81 attempt 2, seven jobs, 980/980 suites.

## Implemented features

- Typed functions, local type inference, mutable variables, constants, conditionals,
  loops and multi-file programs with explicit import roots.
- Exact-width signed and unsigned integers, `f32`/`f64`, booleans and checked numeric
  conversions.
- Value structs, managed structs, receiver methods, custom initialization, stored
  composition, single inheritance, explicit virtual dispatch and structural interfaces.
- Immutable UTF-8 strings, fixed arrays, growable lists, ordered dictionaries and
  concrete generics.
- Tagged enums, results and pattern matching, deterministic cleanup, closures,
  callbacks, properties and events.
- Bounded async tasks, structured task groups, value-only parallel jobs and a
  trusted static native-binding interface for embedding hosts.
- Shared JIT/AOT lowering, precise coordinated garbage collection, compatible
  function replacement and an optional persistent Debug cache.
- Compiler diagnostics with source ranges, plus a shared `voidls` language server
  for completion, hover, signatures, navigation, symbols and guarded editing.

Each feature has a defined scope. See the [technical guides](#documentation) and
[known limitations](docs/KNOWN_ISSUES.md) before relying on a particular behavior.
General-purpose package management, arbitrary public FFI and unrestricted shared
mutable concurrency are not implemented.

## A small example

Save this as `hello.void`:

```void
func double(value: i32) -> i32:
    return value * 2

func entry(value: i32) -> i32:
    return double(value) + 1
```

After building the compiler:

```sh
build/debug/voidc check hello.void
build/debug/voidc run hello.void 10
# 21
```

The CLI accepts one optional decimal `i32` input, defaulting to zero. Its execution
adapter requires `entry(i32) -> i32`; ordinary language functions support other
signatures. Browse [more examples](examples/) for complete programs.

## Build and get started

You need CMake **3.24+**, a C++20 compiler, Python **3.9+** for tests, and the complete
**LLVM 19.1.7** development SDK with matching Clang. CMake rejects other LLVM versions.
Ninja is used below. On Linux, with the SDK installed under `/usr/lib/llvm-19`:

```sh
cmake -S . -B build/debug -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 4
ctest --test-dir build/debug --output-on-failure
```

Adjust SDK paths for your installation. The [build guide](docs/SOURCE_COMPILER.md#building-the-compiler) covers
dependencies, Windows/macOS setup, sanitizers and native object linking.

To emit and link the example on Linux:

```sh
build/debug/voidc emit hello.void -o build/debug/hello.o
clang-19 -fuse-ld=/usr/lib/llvm-19/bin/ld.lld build/debug/hello.o -o build/debug/hello
build/debug/hello 10
# 21
```

Programs that use managed features also require the VOID Runtime when linking;
see the [managed-source guide](docs/MANAGED_STRUCTS.md).

## Platforms and editor support

The established compiler CI matrix covers Linux x86-64, Windows x64 and macOS ARM64.
The latest accepted compiler baseline passed all seven hosted Debug/Release/
sanitizer jobs. The new tooling implementation has passed local Linux acceptance;
its cross-platform hosted acceptance is still pending.

Actual editor tests cover VS Code **1.141.0**, Neovim **0.12.6** and the installed
**0.13.0 development build**, and Vim **9.2** with a pinned LSP client. VSCodium,
other editor versions and new Windows/macOS editor combinations remain unverified.
Rename is restricted to supported bindings and version-safe clients; Vim rename
is disabled. Formatting is conservative and uses four-space indentation.

See [editor installation and configuration](docs/EDITOR_INTEGRATION.md) for setup,
server discovery and supported combinations. Native tooling packages currently
require matching host libraries; clean-machine portability is not verified.

## Documentation

- [Build guide](docs/SOURCE_COMPILER.md#building-the-compiler) and [source compiler / CLI](docs/SOURCE_COMPILER.md).
- [Modules and imports](docs/MODULE_SYSTEM.md), [numeric types](docs/NUMERIC_TYPES.md)
  and [value semantics](docs/VALUE_SEMANTICS.md).
- [Managed memory](docs/MANAGED_STRUCTS.md), [arrays](docs/FIXED_ARRAYS.md),
  [lists](docs/GROWABLE_ARRAYS.md) and [dictionaries](docs/DICTIONARIES.md).
- [Generics](docs/GENERICS.md), [interfaces](docs/INTERFACES.md) and
  [advanced language features](docs/ADVANCED_LANGUAGE.md).
- [Async tasks and concurrency](docs/ASYNC.md), [native bindings](docs/NATIVE_LIBRARIES.md)
  and [diagnostics](docs/DIAGNOSTICS.md).
- [Language server](docs/LANGUAGE_SERVER.md) and [tooling contract](docs/STAGE7_IMPLEMENTATION.md).
- [Current status and roadmap](docs/DEVELOPMENT_STRATEGY.md#current-implementation-status),
  [local tooling acceptance](docs/stage7/ACCEPTANCE.md) and [known issues](docs/KNOWN_ISSUES.md).

The [Language Bible](docs/LANGUAGE_BIBLE.md) describes the broader language design;
implementation guides and acceptance records determine what works today.

## Licensing

No public redistribution license has been selected. Source visibility and local
acceptance packages do not grant permission to redistribute VOID or its dependencies.
