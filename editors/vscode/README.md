# VOID Language for VS Code and VSCodium

Local editor integration for the VOID compiler. Registers `.void` highlighting,
launches your native `voidls`, and displays its diagnostics, definitions and
references. No server download, telemetry, service account or native binary bundle.

Requires VS Code/VSCodium 1.91 or newer, a trusted local filesystem workspace and
a separately built LLVM 19.1.7 VOID language server. This preview supports one
project per window. The entry file's parent is the compiler project root.

Configure workspace settings (example for the compiler repository):

```json
{
  "void.server.path": "build/ci-debug/voidls",
  "void.project.entry": "examples/analysis/main.void",
  "void.trace.server": "off"
}
```

Use `voidls.exe` on Windows. Absolute paths also work. Relative paths resolve from
the single workspace folder. No shell variables, tilde expansion or PATH lookup.
An empty entry selects the first active local `.void` document for a single-file
session; configure an entry explicitly for imports/multi-file work. That selection
stays fixed until an entry setting or restart changes it. Unrelated projects and
multiple workspace folders need separate editor windows.

Open source, view compiler errors in Problems, use Go to Definition (F12) and Find
All References (Shift+F12). Edits synchronize without saving. Use **VOID: Restart
Language Server** to restart or explicitly reread independent disk changes.
View **Output → VOID** for failures. Set trace to `messages` or `verbose` when
investigating; verbose traces may contain source text. Unexpected termination is
reported; use the restart command rather than relying on a crash/restart loop.

Syntax highlighting remains available in Restricted Mode, but no native process
runs until the workspace is trusted. Untitled/virtual documents and network/UNC
projects are unsupported. Unicode comments and physical folders work; compiler
VFS filenames still follow the existing printable-ASCII policy. Only entry-reachable
modules receive semantic validation. There is no completion, hover, rename,
formatting, debugger, file watcher, incremental parser or automatic native reload.

## Build, test and install locally

Use Node.js **24.19.0** with npm. In this directory:

```sh
npm ci
npm test
# Set VOIDLS_PATH to an absolute built server path first:
npm run test:host
npm run package
code --install-extension dist/void-language-0.1.0.vsix
# VSCodium:
codium --install-extension dist/void-language-0.1.0.vsix
```

`test:host` uses the official @vscode/test-electron runner with pinned VS Code
1.141.0, a disposable multi-file project and isolated trusted/restricted profiles.
It downloads that test editor unless `VSCODE_EXECUTABLE_PATH` specifies an existing
native editor executable. This is test tooling only, not a server download feature.
Linux without a display uses `xvfb-run -a npm run test:host`. Extension dependencies,
compiled output, test editors and VSIX files remain ignored. The package contains
bundled language-client JavaScript and language presentation files, never LLVM or
native compiler binaries. No repository license is inferred: package metadata
remains UNLICENSED pending the owner's licensing decision; this is local packaging.

For development, after `npm run build`, launch a separate host from this directory:

```sh
code --new-window --extensionDevelopmentPath="$PWD" /absolute/path/to/project
```

Use an absolute extension path on Windows/PowerShell, e.g.
`code --new-window --extensionDevelopmentPath="$($PWD.Path)" C:\path\to\project`.
Configure that project's settings and open its `.void` entry. VSCodium accepts
the same flags. The repository's docs/EDITOR_INTEGRATION.md gives full compiler
build commands, verification evidence and troubleshooting. No marketplace
publication, accounts or signing are involved.
