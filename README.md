# VOID.Language

A statically typed language for native applications, with indentation-based syntax,
value and managed types, and automatic memory management. One frontend serves LLVM
JIT execution and native compilation.

VOID is in active development. The working compiler implements a substantial
subset of the language design; proposals are not promises of implemented behavior.

[Website](https://voidlang.com) · [Documentation](docs/README.md) ·
[Wiki](https://github.com/Shmellyorc/Void.Language/wiki) · [Editor setup](docs/EDITOR_INTEGRATION.md)

## What works today

- Typed functions, inference, exact-width numbers, modules and automatic application entry selection.
- Value/managed structs, methods, interfaces, generics, results and pattern matching.
- UTF-8 strings, arrays, lists, dictionaries and a growing source standard library.
- Coordinated garbage collection, bounded async tasks and restricted parallel workers.
- JIT execution, native executables and the `voidls` language server.

See the [implemented feature guides](docs/README.md) and [known limitations](docs/KNOWN_ISSUES.md).
General package management, unrestricted shared mutable concurrency and arbitrary
public FFI are not implemented. The ordinary CLI runs synchronous applications;
async execution currently needs the documented host adapter.

## Install from source

You need **LLVM/Clang/LLD 23.1.4**, **Python 3.12+**, **CMake 3.24+** and the
platform's C/C++ development tools. The source-install helper downloads and checks
the pinned SDK and dependencies, builds `voidc` and `voidls`, installs Runtime,
stdlib and native resources, then checks the installation.

Prepare your platform first: **[Linux x64](docs/install/linux.md)** ·
**[Windows x64](docs/install/windows.md)** · **[macOS ARM64](docs/install/macos.md)**.
From this checkout on Linux:

```sh
python3 tools/install.py --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
voidc --version
voidc --help
```

This is a source installation, not an independently portable binary package.
Keep the SDK and host development libraries available for native builds.
The [installation guide](docs/install/README.md) distinguishes current Linux tests,
prior Windows installation evidence and remaining native-platform qualification.
No release binary, package-manager package or stable installer is advertised.

## Hello, world!

Save as `main.void`:

```void
use console

func main() -> i32:
    print("Hello, world!")
    return 0
```

```sh
voidc check main.void
# Source checked successfully
voidc run main.void
# Hello, world!
voidc build main.void -o hello
./hello
# Hello, world!
```

On Windows, use `-o hello.exe` and run `.\hello.exe`. The compiler chooses the
application entry automatically. `voidc run .` and `voidc build .` select the
current directory's `main.void`; no entry flag is needed. See the
[CLI guide](docs/CLI.md) for object emission, legacy numeric programs and cache options.

## Learn and contribute

Start with [your first program](docs/FIRST_PROGRAM.md), browse [examples](examples/),
or follow the [task-oriented Wiki drafts](docs/wiki/Home.md) while their publication
is reviewed. Repository documentation is canonical. The [Language Bible](docs/LANGUAGE_BIBLE.md)
also includes future design; use implementation guides to establish current support.

Compiler/Runtime CI covers Linux x64, Windows x64 and macOS ARM64. That coverage
does not certify portable packages or every editor/OS combination. Development
procedures belong in the [build guide](docs/SOURCE_COMPILER.md#building-the-compiler).

VOID-original compiler, Runtime, stdlib and tooling code use the [MIT license](LICENSE).
Dependencies retain their own terms. See [licensing scope](docs/release/LICENSING_SCOPE.md)
and the [redistribution audit](docs/release/REDISTRIBUTION_AUDIT.md) for attribution,
source obligations and unresolved distribution questions.
