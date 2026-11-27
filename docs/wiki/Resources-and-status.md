# Current capabilities and further reading

Use this Wiki to learn through examples. Use the
[language specification](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md) and individual API guides when
you need the exact behavior of an edge case. VOID is actively developed;
the [project status](https://github.com/Shmellyorc/Void.Language/blob/main/docs/DEVELOPMENT_STRATEGY.md#current-implementation-status)
lists current capabilities and planned work.

## What you can learn here

The language includes typed functions, value and managed structs, constructors and
fallible initialization, tagged/numeric enums and flags, generic interfaces,
collections, nullable values/results, static members, custom iteration,
structured async work, conditional compilation and audited native interoperability.
Explicit pure `compile func` declarations and value methods support bounded
constant evaluation. Start at [Language basics](https://github.com/Shmellyorc/Void.Language/wiki/Language-basics) and follow the next-step links.

The [standard library](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library) provides 29 source modules.
[Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) explains the verified native package
workflow, typed selectors and resource cleanup. A provider or platform can still
impose limits on an operation even when its declaration exists.

## Planned or unavailable behavior

| Topic | Current boundary |
| --- | --- |
| Public VOIDUP distribution | Native manager implemented; no verified public bootstrap or production catalog. Linux private installation evidence only; Windows/macOS installation remains unqualified. |
| Further IntelliSense completion improvements | Planned; current assistance and recovery have documented limits. |
| Compact `if condition: return` | Deferred; write an indented body. |
| Async application main | Available through ordinary JIT and AOT entry with the structured scheduler. See [async application entry](../ASYNC.md#async-application-entry). |
| Detached source tasks | Outside the structured task contract. |
| Async callable values, interface dispatch and destructors | Unavailable under the current async contract. |
| General public package registry | Not implied by the explicit source-package workflow. |

## Platform limits

Linux x64, Windows x64 and macOS ARM64 have distinct installation prerequisites
and native-provider requirements. Use tools, SDKs and native artifacts matching
your OS and architecture. Graphics, audio and keyboard behavior depends on the
native backend and device availability. A declaration's presence does not make
its backend available on every platform.

Native output also depends on the target's system libraries and OS version;
building an executable does not make it universally portable. The language does
not promise a fixed latency or memory ceiling.

Follow [Installation](https://github.com/Shmellyorc/Void.Language/wiki/Installation), [Editor setup](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup) and
[Native compilation](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation) for the relevant workflow and matching
dependencies.

## Go deeper

- [Canonical documentation index](https://github.com/Shmellyorc/Void.Language/blob/main/docs/README.md): feature guides, module index and maintained examples.
- [Language specification](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md): current source contracts, with planned work separated.
- [Numeric types](https://github.com/Shmellyorc/Void.Language/blob/main/docs/NUMERIC_TYPES.md): widths, conversions and arithmetic details.
- [CLI reference](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md): selectors, projects, dependencies and native output.
- [Editor integration](https://github.com/Shmellyorc/Void.Language/blob/main/docs/EDITOR_INTEGRATION.md): server/client configuration and limits.
- [Raylib package](https://github.com/Shmellyorc/Void.Language/blob/main/verified-packages/raylib/README.md): APIs, ownership and native requirements.
- [Official website](https://voidlang.com): another entry point to the project.

Return to [Home](https://github.com/Shmellyorc/Void.Language/wiki/Home) to choose a learning path or use the sidebar to browse.
