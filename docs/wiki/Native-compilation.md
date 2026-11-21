# Build an executable and use native libraries

Use the same source for JIT execution and native compilation. Once you have
[Hello World](https://github.com/Shmellyorc/Void.Language/wiki/First-program), run from its project directory:

```sh
voidc check .
voidc run . --no-cache
voidc build . --profile release -o "output folder/hello"
"./output folder/hello"
```

The final command prints `Hello, world!`. Windows uses an `.exe` output and
PowerShell's call operator for a quoted path, such as
`& ".\output folder\hello.exe"`. Output parent directories are created as needed.

## Pick the right output

`run` invokes the JIT. `build` emits and links an application with the matching
Runtime and native dependencies. `emit` stops at an object file:

```sh
voidc emit src/app.void -o hello.o
```

An object alone is not an executable. Prefer `build` unless you have a deliberate
manual-linking setup. [CLI commands](https://github.com/Shmellyorc/Void.Language/wiki/CLI-commands) explains profiles and options.
Without `-o`, a manifest application builds under its root's `build/` directory,
using the selected entry's stem and the host executable suffix.

## Keep matching resources

A source installation supplies `void-native.txt` and a resource-hash manifest
beside the compiler. Native linking uses those resources and the original pinned
Clang/LLD 23.1.4 SDK. Keep the installation together; do not replace a missing
verified archive with an unrelated ambient system library.
[Installation](https://github.com/Shmellyorc/Void.Language/wiki/Installation#keep-the-installation-together) explains the layout.

A successful build on one host does not establish portability to another OS,
architecture or userspace baseline. See [platform limits](https://github.com/Shmellyorc/Void.Language/wiki/Resources-and-status#platform-limits)
and the [distribution audit](https://github.com/Shmellyorc/Void.Language/blob/main/docs/release/REDISTRIBUTION_AUDIT.md) before shipping output.

## Use a native library

Start with [Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) for an admitted native
provider. A source import alone grants no native execution authority. The lock,
provider identity, ABI, descriptors and artifacts must match before consent can
be used. Source-package maintenance and native permission are separate operations.

At the boundary, preserve the C primitive widths, signedness, layouts and callback
lifetimes. A VOID enum is not presumed ABI-equivalent to a C enum; wrappers extract
`.value` explicitly. Native records can therefore keep integer fields even when a
public selector accepts an [enum or flags type](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags).

Writing a provider requires an audited ownership policy, including cleanup and
callback draining. Follow [Native libraries](https://github.com/Shmellyorc/Void.Language/blob/main/docs/NATIVE_LIBRARIES.md) and the
[compiler architecture](https://github.com/Shmellyorc/Void.Language/blob/main/docs/SOURCE_COMPILER.md); avoid exposing arbitrary raw
addresses as ordinary safe language values.

Next: [Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) or
[debug your setup](https://github.com/Shmellyorc/Void.Language/wiki/Troubleshooting).
