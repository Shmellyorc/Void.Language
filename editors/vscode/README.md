# VOID Language for VS Code and VSCodium

Compiler-owned completion, hover, signature help, diagnostics, navigation, source
symbols, guarded rename and conservative formatting through one native `voidls`.
Requires a trusted local filesystem workspace and VS Code API 1.141.0+. VSCodium
execution is a separate verification target. One selected entry graph per window.

Run **VOID: Select Project Entry** or set `void.project.entry` for an explicit
entry. Empty asks the shared compiler to select nearest `main.void` within the
workspace, otherwise a labelled standalone active-file session. Selection stays
fixed until deliberate restart/reselection. The status bar identifies entry and server.
`void.server.path` overrides discovery; empty checks a bundled native server,
then PATH. No shell expansion, runtime download or repository build occurs.

Use the normal editor completion/hover/signature/navigation/rename/format commands.
Unsupported rename categories fail safely. **VOID: Refresh Project** observes disk
changes while retaining dirty buffers; project files are also watched. **VOID:
Restart Language Server** restarts after configuration or process failures.
Output → VOID reports startup/lifecycle failures; verbose trace can include source.

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
UNLICENSED: public publication/redistribution requires the owner's license decision.
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
