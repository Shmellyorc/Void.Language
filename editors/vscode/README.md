# VOID Language for VS Code and VSCodium

## Manifest projects

Set `void.project.root` to a literal absolute or workspace-relative manifest root.
An optional `void.project.entry` then selects a source inside that root. An entry
setting without a root keeps the legacy entry-parent session. With neither, the
matching voidls performs nearest-manifest discovery within the workspace, then
legacy discovery only if no manifest was selected. No TypeScript source scan is used.

Manifest/error-bearing and library sessions use `voidProjectProtocol: 1` and schema-2
preflight. Missing main and malformed TOML produce diagnostics without preventing
project opening. Dirty source/TOML buffers, file events and root switches retain
revision/client guards. An older project-incapable server produces a visible error;
there is no silent legacy downgrade. Explicit legacy-entry mode remains available.

In a trusted local folder, **VOID: Create Project** reviews and creates a manifest
for existing sources. An eligible first source can also offer project creation.
The offer remembers acceptance, decline or dismissal. **VOID: Reset Project
Creation Prompt** resets that decision.


Compiler-owned semantic highlighting, completion, hover, signature help,
diagnostics, navigation, source symbols, guarded rename and conservative
formatting use the native `voidls`. Open project sources receive editor
analysis before any application source imports them, including unsaved edits.
Requires a trusted local filesystem workspace and VS Code API 1.141.0+. VSCodium
execution is a separate verification target. One pinned project/session per window.

Run **VOID: Select Project Entry** or set `void.project.entry` for an explicit
entry. With no root or entry setting, shared project preflight first selects the
nearest manifest within the workspace; without a manifest it retains nearest
`main.void` and labelled standalone fallback. Selection stays
fixed until deliberate restart/reselection. The status bar identifies entry and server.
`void.server.path` overrides discovery; empty checks a bundled native server,
then PATH. No shell expansion, runtime download or repository build occurs.

Use the normal editor completion/hover/signature/navigation/rename/format commands.
Unsupported rename categories fail safely. **VOID: Refresh Project** observes disk
changes while retaining dirty buffers; project files are also watched. **VOID:
Restart Language Server** restarts after configuration or process failures.
Output → VOID reports startup/lifecycle failures; verbose trace can include source.

### Check, build and run

In a trusted folder with a selected manifest, run **VOID: Check Project**,
**VOID: Build Project**, or **VOID: Run Project** from the Command Palette.
The extension uses the selected root and any explicit entry override. It runs
the matching `voidc` next to `voidls`, or a literal path in `void.compiler.path`.
Set `void.tasks.profile` to `debug` (default) or `release`. Put application
arguments in the `void.tasks.runArguments` string array, one element per
argument. Spaces, empty strings and shell metacharacters are passed literally.

Save changed VOID sources and manifests in the selected project before using
these commands. Tasks use saved files and stop before launch when those buffers
are dirty. The task terminal shows the compiler and application output and exit
status. Compiler errors appear in Problems. Build writes the normal native
output under `build/`. Run uses the JIT. Use a regular terminal for programs
that require a TTY or commands that deliberately authorize a native dependency
with `--allow-native`.

Syntax highlighting works in Restricted Mode; native execution requires trust.
Multi-root, virtual/untitled, network/UNC and unrelated projects need separate
windows or are unsupported. There is no debugger or automatic native reload.

Developer build/package (pinned Node 24.19.0, locked dependencies):

```sh
npm ci
npm test
npm run package
code --install-extension dist/void-language-0.1.0.vsix
# VSCodium offline installation target:
codium --install-extension dist/void-language-0.1.0.vsix
```

A generic VSIX requires a separately built/installed `voidls` from LLVM 23.1.4.
Owner-local platform packages can include the matching server. Host runtime
libraries still apply; no clean-machine portability claim is made. Repository
VOID-original integration source is MIT-licensed under the repository's `LICENSE`.
Third-party dependencies retain their own licenses; see
`docs/release/LICENSING_SCOPE.md` and `docs/release/REDISTRIBUTION_NOTICES.md`.
This source-license declaration does not certify a distributable VSIX or native
server bundle. Public release remains subject to the recorded release holds.
See the repository's `docs/EDITOR_INTEGRATION.md`, `docs/STAGE7_IMPLEMENTATION.md`
and `docs/stage7/ACCEPTANCE.md` for setup, exact executed versions and limitations.

Sprint 1 adds nested standard imports (`use io/files`) and importer-relative quoted
project imports (`use "app/player.void"`), with space/quote/slash completion triggers.
Names here are illustrative fixture modules. Source `##` docs appear in hover,
completion and signatures; `## @param name description` documents parameters.
Set the absolute `VOID_STDLIB_ROOT` environment variable before launching the
editor for relocated installations; otherwise the matching build's configured
source root is used. Installed sources permit navigation without rename/format
or project overlay authority. See `docs/STAGE8_DISCOVERY.md` for the contract.
