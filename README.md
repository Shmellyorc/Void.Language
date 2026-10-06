# VOID Language

> [!WARNING]
> **VOID Language is under active development.**
>
> The compiler, runtime, Standard Library, tooling, and language surface are still being built. Not every planned feature is implemented yet, and existing behavior or APIs may change.
>
> This repository is public so development can be followed, tested, and explored. It should not yet be treated as a production-ready language.

VOID is a C#-inspired programming language that compiles to C and then to native code.

The goal is to provide a familiar C#-style language while keeping a native compilation model suitable for game development, native libraries, and low-level interoperability.

VOID does not require the CLR to run compiled applications.

## Example

```csharp
public static class Program
{
    public static void Main()
    {
        Console.WriteLine("Hello from VOID!");

        var value = 40 + 2;
        Console.WriteLine(value);
    }
}
```

VOID source uses the `.void` extension.

## What VOID Is

VOID is an independently implemented language, compiler, runtime, and Standard Library.

It takes familiar ideas from C# while targeting native applications through generated C.

```text
Program.void
    |
    v
VOID Compiler
    |
    v
Generated C
    |
    v
Native C Compiler
    |
    v
Native Executable
```

The generated C is an implementation detail. Normal development is done in VOID.

## Current Development

VOID already includes work across areas such as:

- Classes, structs, interfaces, and inheritance
- Generics
- Delegates and events
- Properties and indexers
- Exceptions
- Iterators and `yield`
- Async/await
- `Task`, `ValueTask`, and threading
- Synchronization primitives
- Managed strings
- Garbage collection
- Arrays, Span, references, and pointers
- Native functions and C interoperability
- Native memory operations
- Collections
- File and stream I/O
- Pattern matching
- LSP tooling
- Source diagnostics
- Linux and Windows support

This is **not** a declaration that the language is complete or that every corresponding C#/.NET feature exists.

Large parts of the language and Standard Library are still being expanded, tested, and refined.

## Building VOID

VOID currently builds with a C11 compiler and GNU Make.

From the repository root:

```bash
make
```

Check the compiler:

```bash
./bin/voidc version
```

On Windows:

```text
.\bin\voidc.exe version
```

## Using the Compiler

General command format:

```text
voidc <command> [target]
```

Current compiler commands include:

```text
check      Resolve and validate a VOID project without building it
build      Resolve and build a VOID project
run        Resolve, build, and run a VOID project
publish    Resolve and publish a VOID project
lex        Tokenize VOID source and print the token stream
parse      Parse VOID source and print the syntax tree
lsp        Run the VOID language server over stdio
version    Print the compiler version
```

A target can be a directory, `.voidproj` file, or `.void` source file.

For example:

```bash
./bin/voidc run Examples/HelloProjectless/Program.void
```

## Projects

VOID supports both project-based and simple projectless programs.

A projectless application can use a `Program.void` entry point directly.

Larger applications can use `.voidproj` projects to describe their build.

Examples are available under:

```text
Examples/
```

## Testing

VOID has a large cumulative regression suite covering the compiler, runtime, Standard Library, native interoperability, threading, async behavior, diagnostics, and other language systems.

Run it with:

```bash
make test
```

The cumulative runner supports multiple isolated workers:

```bash
make TEST_JOBS=6 test
```

Python 3 is required for the test harness.

## Repository Layout

```text
Compiler/         VOID compiler
Runtime/          Native runtime support
StandardLibrary/  VOID Standard Library
Examples/         Example VOID programs
Tests/            Compiler/runtime/library regression tests
```

## Native Interoperability

Native interoperability is a core part of VOID's design.

VOID can interface with native C libraries and provides language/runtime support for native functions, pointers, unmanaged memory, callbacks, and related ABI boundaries.

The intent is to make native libraries usable without requiring a managed runtime between the VOID application and native code.

## Tooling

VOID includes an LSP implementation for editor tooling such as:

- Diagnostics
- Completion
- Hover information
- Signature help
- Definition and navigation support

Tooling is also still under active development.

## Status

VOID is currently a **work in progress**.

Expect:

- Missing features
- Incomplete Standard Library coverage
- Breaking changes
- Compiler bugs
- Tooling changes
- Documentation gaps

The project is being developed in small, tested increments rather than declaring broad compatibility before the underlying systems are complete.

If something is currently missing, that does not necessarily mean it is outside the long-term scope of the language.

## License

See [LICENSE](LICENSE).