# Edit VOID in VS Code or VSCodium

Use the VOID extension with matching `voidls` and `voidc` from
[your installation](https://github.com/Shmellyorc/Void.Language/wiki/Installation). The extension handles editor integration;
the language server supplies compiler-owned diagnostics, completion, hover,
signatures, definitions, references and semantic tokens.

## Install the local extension package

Use a local VSIX prepared from the matching source/tooling workflow. The generic
VSIX does not include or download the language server. Extension authors can
prepare it from a source checkout with the pinned Node 24.19.0 and locked npm
dependencies:

```sh
npm --prefix editors/vscode ci
npm --prefix editors/vscode run package
code --install-extension editors/vscode/dist/void-language-0.1.2.vsix
```

For VSCodium, use `codium --install-extension` with that same VSIX path.
End users installing a prepared VSIX do not need a separate npm build. This is a
local package workflow, not a claim that a Marketplace release is available.
Follow the [editor integration guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/EDITOR_INTEGRATION.md) for native
server bundles and other editor adapters.

## Select the tools and project

For a VOIDUP installation, run `voidup env --format vscode` and merge the
printed `void.server.path` and `void.compiler.path` keys into settings. This
selects both tools from the verified active prefix without requiring the editor
to inherit terminal PATH. After switching or rolling back, regenerate those
paths and use **VOID: Restart Language Server**. A PATH export also selects a
specific version; restart the editor after changing its inherited environment.

Open one trusted local folder containing your [project manifest](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules).
If tools are installed outside discovery paths, use workspace settings like:

```json
{
  "void.server.path": "/absolute/path/to/voidls",
  "void.compiler.path": "/absolute/path/to/voidc",
  "void.tasks.profile": "debug",
  "void.trace.server": "off"
}
```

Replace the paths with actual native executables; Windows uses `.exe`. Paths are
literal, with no shell or tilde expansion. Server selection prefers an explicit
path, then a matching bundled server, then executable discovery on PATH.
The server starts only in a trusted filesystem workspace.

For ordinary manifest use, let the compiler discover the entry. Set
`void.project.root` or `void.project.entry` only when you intentionally need an
override. Do not keep an old explicit entry pointing at a different project.

## Try the development loop

1. Open `src/app.void` from [First program](https://github.com/Shmellyorc/Void.Language/wiki/First-program).
2. Save source and manifest edits, then choose **VOID: Check Project**.
3. Use **VOID: Run Project** or **VOID: Build Project** to run saved source.
4. Hover a function, request a signature, or follow **Go to Definition**.
5. Fix a diagnostic and confirm it clears in Problems.

The editor's Debug profile is separate from CLI defaults. Tasks use pipes;
TTY programs and [native provider approval](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib) need an ordinary
interactive terminal. Existing matching approval can be reused by tasks.

## When assistance is incomplete

The server can retain assistance during supported incomplete edits, but unresolved
receivers, broken imports or ambiguous project selection can withhold results.
Formatting and rename require validated analysis and remain conservative.
Themes control rendered enum colors. Completion improvements remain planned;
existing assistance is not a guarantee for every broken source state or editor.

Check **Output → VOID**, the selected server path and protocol/startup messages.
Use **VOID: Refresh Project** to reread disk without discarding dirty buffers or
**VOID: Restart Language Server** after correcting configuration. Trace logs can
contain source; review them before sharing. Multiple workspace folders, virtual
documents and UNC/network workspaces retain their documented restrictions.

Use native tool executables for your OS and architecture. For Neovim and Vim,
use their separate configuration and limits in the
[canonical guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/EDITOR_INTEGRATION.md).

Next: [Troubleshooting](https://github.com/Shmellyorc/Void.Language/wiki/Troubleshooting) or
[Enums and flags](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags) to try member completion.
