# VOID editor integration (Phase 11)

**Stage 8 Sprint 1 (20 October 2026):** [STAGE8_DISCOVERY.md](STAGE8_DISCOVERY.md)
supersedes the historical import/discovery exclusions below. It specifies separate
standard/project source domains, importer-relative bare quoted imports, bounded
shared `main.void` selection, metadata completion, source docs and readonly standard
navigation. [Local acceptance](stage8/ACCEPTANCE.md) records actual verification;
no hosted lock is implied. Existing snapshot, UTF-16, rename and native safety
contracts remain in force.


## Stage 7 setup — current instructions (20 October 2026)

Production engineering is authorized. This section and
[STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md) supersede the historical
Phase 11 feature exclusions below. See [acceptance](stage7/ACCEPTANCE.md) for exact
executed versions; compatibility targets do not establish a tested version range.

All editors share compiler diagnostics, completion, hover, signatures, navigation,
document/workspace symbols, conservative formatting and refresh. Supported rename
categories are negotiated by client capability; **Vim rename stays disabled**.
Each window selects one entry-reachable project. Unsaved overlays remain authoritative.

### Shared Sprint 1 setup

All adapters call `voidls --discover <workspace> <active> <override>`; empty
arguments represent absent inputs. Explicit entry wins. Neovim accepts
`require('void').setup({server='/absolute/voidls', workspace='/project'})`;
Vim accepts `call void#setup('/absolute/voidls', '', '/project')`.
Their workspace defaults to cwd, so select it deliberately. Neovim restart retains
its selected graph; start a fresh adapter/editor session with the intended setup to reselect. Vim
setup selects before its LSP client starts; reconfigure a fresh editor session to
reselect. `VOID_STDLIB_ROOT` must be an
absolute matching source directory for relocated installations. No real standard
module catalog is supplied. See [the contract](STAGE8_DISCOVERY.md) for import
spelling, source docs, known-root navigation and readonly installed sources.

### VS Code / VSCodium

The extension now declares VS Code **1.141.0+**, raising the old untested 1.91
minimum to the actually exercised API version. VSCodium is an integration target;
execution evidence must be recorded separately. Configure `void.project.entry`
or run **VOID: Select Project Entry**. Empty entry asks `voidls --discover` for nearest exact-case `main.void` within
the workspace, then uses a labelled standalone active-file fallback when absent.
Selection stays fixed until explicit configuration/restart. The status bar
shows this entry and the selected server path.

Server precedence: literal `void.server.path`, matching bundled native `server/voidls`
(`voidls.exe` on Windows), then an absolute executable on PATH. Discovery/spawn
happens only in a trusted filesystem workspace. No repository build, shell expansion,
server download or source execution occurs automatically. Initialization must expose
VOID tooling protocol 1; an incompatible installation fails visibly.

The extension watches project `.void` files and forwards save notifications.
**VOID: Refresh Project** rereads disk without losing dirty buffers. **VOID: Restart
Language Server** restarts the selected service. Output → VOID retains diagnostics
about startup, protocol and lifecycle failures. Verbose trace can contain source.
Multiple workspace folders, UNC/network and virtual documents remain unsupported.

Build an owner-local package with pinned Node 24.19.0 and existing locked dependencies:

```sh
npm --prefix editors/vscode test
npm --prefix editors/vscode run package
# Generic VSIX: configure/install voidls separately.
code --install-extension editors/vscode/dist/void-language-0.1.0.vsix
# VSCodium uses the same offline workflow (execution remains separately verified):
codium --install-extension editors/vscode/dist/void-language-0.1.0.vsix
```

Prepare a shared native archive and a matching private platform VSIX:

```sh
cmake --build build/ci-release --target voidls -j2
python3 tools/package_tooling.py --server build/ci-release/voidls \
  --target linux-x64 --output build/stage7-artifacts --vsix
code --install-extension build/stage7-artifacts/void-language-linux-x64.vsix
```

Packaging audits dependencies, extracts the archive and verifies a real server
handshake without SDK paths on PATH. This is local acceptance on the same host,
not proof of a self-contained bundle or clean-machine portability. Other targets
must be built and audited on their own native hosts. No public redistribution is
authorized while repository licensing remains UNLICENSED.

### Plain documentation and popup policy

[The compiler contract](DOCUMENTATION_HOVER.md) applies equally to automatic and
manual signature requests. Only the active documented parameter supplies prose;
an undocumented argument closes/suppresses help. Hover/completion shows callable
description and applicable return text, with no parameter descriptions or raw tags.

VS Code uses its existing null-result dismissal path. The Neovim Ctrl-K mapping
owns a signature float because the builtin command leaves old floats open on null.
The small wrapper consumes compiler responses and guards buffer, changedtick,
cursor and request generation; it closes only its own window. Neovim's shipped
adapter does not install automatic signature triggers. The pinned Vim client's
existing null/stale-context handling is retained; `void#setup` enables its existing
`showSignatureDocs` display option so parameter prose is visible. This is a client
UI option, with no source parsing. Later explicit client configuration may override
it. Third-party signature plugins and custom float lifecycle settings remain
client-owned. Plaintext may still be styled by a client's preview utilities.

The [implementation handoff](DOCUMENTATION_HOVER_IMPLEMENTATION.md) records exact
executed editor versions, real popup/window checks and remaining platform limits.
The VS Code host harness accepts `VOID_TEST_CDP_PORT` with an isolated test window
to inspect the actual parameter-hint DOM; provider-only assertions do not certify
a visible popup dismissal.

### Neovim

Install/copy the shipped runtime directories, or point `runtimepath` at them.
Example user configuration with an explicit trusted project:

```lua
vim.opt.runtimepath:prepend('/absolute/Void.Language/editors/vim')
vim.opt.runtimepath:prepend('/absolute/Void.Language/editors/neovim')
vim.cmd('filetype plugin indent on')
require('void').setup({
  server = '/absolute/path/voidls', -- omit to discover installed PATH executable
  entry = '/absolute/project/main.void', -- omit for a fixed standalone-file session
})
```

The adapter uses built-in LSP config/enable/completion, with no Mason,
nvim-lspconfig or completion plugin dependency. The research minimum target is
0.11.5; exact executed releases/dev builds are listed in acceptance. `gd`, `gr`,
`K`, Ctrl-K and `<leader>vd` provide definitions, references, hover, signatures and
diagnostics. Built-in completion works with Ctrl-X Ctrl-O and auto-triggering.
`<leader>rn` uses guarded rename; `<leader>vf` / `:VoidFormat` guard formatting by
changedtick; `<leader>vr` / `:VoidRefresh` refresh disk. `:VoidRestart` restarts the
client. No user configuration is edited by integration tests.

### Vim

Use actual Vim **9.1+** as a target, with **yegappan/lsp pinned at
`c741b2f1afb42b9f267824dac4049674908f4a41`**. Only the exact executed version is
claimed tested. Install that maintained client using your existing package
workflow, then add the shipped runtime and choose one entry:

```vim
set runtimepath^=/absolute/Void.Language/editors/vim
" Load the installed yegappan/lsp plugin before setup (for example :packadd lsp).
filetype plugin indent on
syntax enable
call void#setup('/absolute/path/voidls', '/absolute/project/main.void')
```

Use an empty first argument for PATH discovery. Passing a single source file as
entry is the standalone fallback. `gd`, `gr`, `K`, Ctrl-K, Ctrl-X Ctrl-O and the
client's diagnostic/symbol commands use the shared server. `:VoidFormat` checks
changedtick after its RPC before applying the compiler's whole-document edit.
`:VoidRefresh` and `:VoidRestart` control refresh/lifecycle. Use these guarded
formatting commands; third-party async formatting callers own stale-result checks.
No rename mapping/capability is provided to the pinned Vim client.

On this host installing Neovim changed the `vim` alternative to Neovim. Tests use
`/usr/bin/vim.basic` for actual Vim 9.2 and leave alternatives/configuration intact.
Check `vim --version` before selecting the adapter; Neovim cannot run Vim9 scripts.

### Reproduce real adapter tests

```sh
python3 tests/editors/run.py --server build/ci-release/voidls \
  --nvim /usr/bin/nvim --vim /usr/bin/vim.basic \
  --vim-client /absolute/pinned-yegappan-lsp \
  --output build/stage7-editors.json
VOIDLS_PATH="$PWD/build/ci-release/voidls" \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code \
npm --prefix editors/vscode run test:host
# Set VOID_TEST_VSIX to the platform VSIX to install and test the actual artifact
# in disposable profiles; otherwise the existing extension-development mode runs.
```

Tests exercise real clients with temporary projects, Unicode physical roots,
unsaved edits and shutdown. Vim/Neovim runtime files are not installed into the
user's configuration. The server itself is not published or installed system-wide.

---


**20 October 2026:** See the [Stage 7 research roadmap](STAGE7_ROADMAP.md) for
verified current editor behavior, real Linux Neovim/Vim baseline probes, measured
latency and the proposed shared-server sprint. Boss confirms Stage 6 locked at
`4139297f` / CI #78. Research is complete; no Stage 7 production integration or
new capability is implemented by that research.

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

The extension in `editors/vscode/` registers `.void` as language ID `void`, supplies
basic TextMate highlighting/indentation, and runs the existing `voidls` through
`vscode-languageclient`. Diagnostics and navigation come from the actual compiler.
There is no editor-side parser, resolver, type checker, LSP transport or position
converter. VS Code and the language client use the server's UTF-16 positions.

## Stage 7 editor goals (planning only)

**Future goals, not delivered functionality or Stage 7 authorization.** The existing
VS Code adapter described below remains the implementation baseline. Stage 6
continues unchanged through Delivery 5 and owner-controlled hosted acceptance.
Stage 7 is planned as one cohesive implementation sprint by default, splitting
only if investigation exposes genuine architectural dependencies, high-risk
contracts or acceptance problems. Its aim is a practical, developer-ready editing
experience; it need not implement every optional LSP extension.

| Editor | Stage 7 goals |
| --- | --- |
| VS Code / VSCodium | Full-featured extension experience: reliable installation/configuration, native `voidls` discovery and startup; IntelliSense/completion, hover, signature help, navigation, rename, formatting and diagnostics; appropriate semantic highlighting, code actions and workspace features where supported; reliable project selection, document synchronization/refresh and lifecycle; cross-platform testing and packaging. |
| Neovim | Native LSP integration with shared `voidls`; filetype recognition, syntax highlighting and indentation; working completion, navigation, hover, rename and diagnostics; reproducible configuration examples, documentation and integration tests. |
| Vim | Integration through an appropriate maintained Vim LSP client; filetype recognition, syntax and indentation; compatible diagnostics, completion and navigation; documented supported Vim versions and required plugins, validated through realistic integration tests. |

Neovim/Vim adapters and their supported versions/plugins are not delivered or
selected by this update. Choose the Vim client and compatibility matrix after
checking official documentation and real behavior. Do not assume that passing
VS Code tests establishes VSCodium, Neovim or Vim compatibility. Verify actual
client/server attachment, unsaved edit/repair, completion/navigation and other
advertised features, startup failures, refresh and shutdown/restart on the declared
supported combinations. Include installable packaging and reproducible setup.

Every editor shares **one authoritative `voidls`** and compiler-owned analysis.
Filetype/syntax/indentation rules and client configuration are editor integration;
no editor-specific parser or type checker is allowed. Preserve the server's
immutable snapshots, diagnostic accuracy, UTF-16 positions, source identities and
stale-result checks. Better project handling, executable discovery/distribution,
workspace scope, rename/formatting behavior and optional semantic/code-action
support need investigation; this plan does not silently change current settings,
trust, explicit-entry/root or synchronization contracts.

Measure useful compiler/server/editor performance and setup friction, then repair
demonstrated bottlenecks. Preserve KISS/DRY and LLVM 19.1.7; do not invent speculative
optimization frameworks, native packages, public FFI, new syntax or automatic
native hot reload. Research real source and official editor/client documentation
for architectural or compatibility questions, then use focused development checks
and consolidated acceptance. See the [canonical workflow and open decisions](DEVELOPMENT_STRATEGY.md#owner-workflow-and-stage-7-planning--19-october-2026).
The current instructions and limitations below describe the existing extension,
not a claim that these future goals have been completed.

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
