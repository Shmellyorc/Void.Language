# Build a native executable

_Draft for Boss approval; repository documentation is canonical._

After [Hello World](First-program.md), run:

```sh
voidc build main.void -o "output folder/hello"
"./output folder/hello"
# Hello, world!
```

Use `.exe` and PowerShell's `&` call operator for quoted executable paths on
Windows: `& ".\output folder\hello.exe"`.
Without `-o`, the output is `build/main` (`build/main.exe` on Windows).

`build` uses matching installed Runtime/dependency archives and the source
installation's original Clang/LLD 23.1.4 SDK. It verifies resources and publishes
only a successfully linked executable. Do not overwrite a source with output.
`voidc emit main.void -o main.o` writes an object for manual linking; it does not
produce a complete application. Prefer build for ordinary applications.

The resulting application still has OS/ABI and dependency obligations. Neither
LLVM's presence on the build host nor a successful local run establishes universal
binary portability. Canonical details: [CLI](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md), [native workflow](https://github.com/Shmellyorc/Void.Language/blob/main/docs/STAGE9_APPLICATION_WORKFLOW.md), [redistribution](https://github.com/Shmellyorc/Void.Language/blob/main/docs/release/REDISTRIBUTION_AUDIT.md).
