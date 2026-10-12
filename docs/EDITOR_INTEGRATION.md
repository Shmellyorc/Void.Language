# VOID editor integration (Phase 11)

The extension in `editors/vscode/` registers `.void` as language ID `void`, supplies
basic TextMate highlighting/indentation, and runs the existing `voidls` through
`vscode-languageclient`. Diagnostics and navigation come from the actual compiler.
There is no editor-side parser, resolver, type checker, LSP transport or position
converter. VS Code and the language client use the server's UTF-16 positions.

## Requirements and native build

VS Code or VSCodium **1.91+** with desktop filesystem access on the extension host.
Local verification uses VS Code 1.141.0; VSCodium and hosted Windows/macOS require
their own verification. Development/packaging uses pinned Node.js **24.19.0** and
locked npm dependencies. End users do not need Node/npm separately to install the
VSIX. The editor extension does not change native CMake requirements or make the
compiler depend on Node.js.

Build `voidls` separately with complete LLVM/Clang/LLD **19.1.7**. For example:

```sh
cmake -S . -B build/editor -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld
cmake --build build/editor --target voidls -j2
```

See README for Linux development packages and the existing Windows developer
SDK/macOS setup. Windows uses `voidls.exe`; macOS uses its pinned SDK CMake path
and compiler without Linux's VOID_LINKER option. No native binaries or LLVM archives
are bundled in the extension, and it never downloads a server.

## Configure one project

Open the repository as a single-folder workspace and set:

```json
{
  "void.server.path": "build/editor/voidls",
  "void.project.entry": "examples/analysis/main.void",
  "void.trace.server": "off"
}
```

Both paths can be absolute or relative to the single workspace folder. They are
literal: no shell, PATH search, environment substitution or tilde expansion.
Windows full drive paths and workspace-relative paths are supported; drive-relative
`C:foo` and drive-less rooted `\foo` paths are rejected as ambiguous. Spaces and
Unicode physical root directories work. The server is launched directly with an
argument array `['--entry', absoluteEntry]`, with stdio as the language client's
executable default. Explicit TransportKind.stdio would append an unwanted `--stdio`;
this adapter preserves the existing server CLI without modifying native code.

The entry file's parent **is** the compiler root, regardless of the editor workspace
root. Imports use that existing VFS policy. Select the entry explicitly for a
multi-file project; do not point entry at an arbitrary imported file.

When entry is empty, the first active local VOID document selects a **single-file**
session (or the first already-open VOID document at activation). It stays selected
when switching tabs. Restart can select the then-active VOID file; configuring an
entry switches to an explicit project. Other VOID documents are warned about and
excluded from that single-file synchronization selector. No filename/import/tab
heuristic tries to discover a multi-file entry.

Only one project/server is managed per window. Multiple workspace roots are rejected
with an explanation and no server. Unrelated projects need separate windows.
Documents outside the selected project root do not synchronize to its server.
Files inside one explicit root are compiler-project files, but only the compiler's
entry-reachable graph receives semantic validation. No .voidproj/discovery/package
format is introduced. Local file documents only: untitled, virtual and UNC/network
projects are unsupported. Remote editor scenarios were not locally verified.

## Build, development host and local installation

From `editors/vscode/`:

```sh
npm ci
npm test
npm run package
code --install-extension dist/void-language-0.1.0.vsix
# Or:
codium --install-extension dist/void-language-0.1.0.vsix
```

`npm test` type-checks strictly, compiles TypeScript, bundles the language client
with esbuild and runs configuration/executable/path tests. Packaging repeats the
build using the normal VS Code prepublish hook. The VSIX includes full runtime
dependency license notices. It excludes tests, source maps, node_modules, lockfiles,
native binaries and LLVM archives. Generated output and packages remain ignored;
package.json/package-lock.json and source remain reviewable. The owner has not
selected a repository license: package metadata remains UNLICENSED rather than
inventing a redistribution policy. No marketplace/account/signing step is used.

Launch a development host after building:

```sh
code --new-window --extensionDevelopmentPath="$PWD" /absolute/path/to/project
```

PowerShell: `code --new-window --extensionDevelopmentPath="$($PWD.Path)" C:\path\to\project`.
VSCodium supports the same development flags. Configure paths in that project's
settings; open its entry `.void` file. The actual extension is activated by VOID
source or its contributed restart command.

## Genuine multi-file example

Use the existing `examples/analysis/main.void`, `worker.void` and `values.void`.
With the settings above, open main.void: no compiler error is expected. F12 on
`worker.calculate` navigates to worker.void's declaration. Shift+F12 finds resolved
references (the declaration is included by the editor's references context).

In worker.void, change `return stable + 1` to `return missing` without saving.
Problems shows `VOID2002`, error severity, the identifier's original range and the
compiler hint. Definition/references queries are empty while this graph is invalid;
there is no historical-success fallback. Undo the edit: the diagnostic clears and
navigation works again. The extension sends complete source text according to
voidls's full-document synchronization capability; it adds no incremental parser.
Closing and reopening documents follows the language client's standard lifecycle.

`examples/analysis` remains real source compilation: the native tests retain
53 -> 61, old lease 53 and restart-required constant edits. Editor analysis never
executes that code or publishes a native replacement.

## Lifecycle, trust and logs

**VOID: Restart Language Server** stops the existing client and starts a fresh
session. Entry/executable/trace setting changes also restart serially. Missing,
non-file or non-executable server paths are validated and reported; spawn and
startup failures are logged through the client and the VOID channel. Unexpected
termination is reported without an automatic restart loop. Closing the editor or
disabling the extension invokes the same asynchronous disposal lifecycle.

View **Output → VOID** for startup errors/server stderr. Optional trace levels are
`off`, `messages`, `verbose`; verbose traces can contain source text. The standard
client may send optional `$/setTrace` notifications, which this minimal server logs
as unsupported; compiler diagnostics/navigation are unaffected. All server protocol
output remains on its private child stdout pipe.

Restricted Mode permits highlighting but **never launches native analysis**, even
when a workspace configures an executable. Trust is checked before path validation
and spawn. Trusting the workspace enables normal startup; do so only for projects
you trust. No extension telemetry, downloads or external services are implemented.

## Automated real editor tests

Build the extension, then set the real server path:

```sh
# From editors/vscode, Linux/macOS:
export VOIDLS_PATH=/absolute/path/to/build/ci-debug/voidls
npm run test:host
# Windows PowerShell:
# $env:VOIDLS_PATH = 'C:\absolute\path\build\debug\voidls.exe'
# npm run test:host
```

The official `@vscode/test-electron` tooling downloads pinned VS Code **1.141.0**
when needed. Set `VSCODE_EXECUTABLE_PATH` to an installed native editor executable
(e.g. `/usr/share/code/code`) to use it locally. Test-editor downloads belong only
to developer tests. They are not an extension runtime behavior. Linux without a
display uses `xvfb-run -a npm run test:host` with the normal GTK/NSS/GBM/audio packages.

Tests use disposable real source files in Unicode/space/escaped paths, real native
children and editor diagnostics/definition/reference commands. They cover unsaved
repair, exact codes/ranges, stale-query prevention, tab close/reopen and actual
TextDocument close/open events, restarts, config switches, failure reporting,
termination and deactivation cleanup. VS Code can retain text models after tabs
close; language reassignment exercises the documented TextDocument close/open
transition without pretending tab closure immediately releases a model.

Restricted-profile testing launches VS Code's documented test CLI directly because
`test-electron.runTests` unconditionally disables Workspace Trust. It still uses
the same real editor/test entrypoint and asserts actual restricted state and zero
native children. A separate multi-root host verifies rejection. No mocked LSP
substitute or editor emulator is used. Test instrumentation observes real spawns
and captures error UI calls; failures remain assertions. See WORK_LOG.md for actual
results rather than inferring hosted coverage from local success.

## Limits and troubleshooting

If no analysis starts, check Workspace Trust, the single-folder boundary, a configured
native executable with execute permissions, and the selected absolute entry. Build
voidls first; configuring a directory, a compiler driver or a shell command is not
supported. Check Output → VOID before using the restart command.

No watcher/save refresh extension is added. External disk changes alone do not
trigger analysis; restart explicitly rereads disk. In-editor edits use immutable
session overlays. Full graph analysis is synchronous and disk reads remain non-atomic.
UTF-8 comments and physical root names work, but Unicode module filenames and
identifiers retain existing compiler restrictions. Mutable locals/parameters have
no definition/reference records yet. Syntax highlighting covers only implemented
keywords, types, decimal literals/operators, comments and quoted import paths;
it does not promise support for strings, structs, collections, async or other
Language Bible proposals. Completion, hover, rename, formatting, semantic tokens,
code actions, debugger, workers and automatic native reload remain deferred.

Official references: [language client architecture](https://code.visualstudio.com/api/language-extensions/language-server-extension-guide),
[extension-host tests and CLI](https://code.visualstudio.com/api/working-with-extensions/testing-extension),
[Workspace Trust](https://code.visualstudio.com/api/extension-guides/workspace-trust),
[pinned Node LTS release](https://nodejs.org/en/blog/release/v24.19.0).
