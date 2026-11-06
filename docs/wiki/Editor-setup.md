# Connect your editor

_Draft for Boss approval; repository documentation is canonical._

Install `voidls` with the compiler, then confirm `command -v voidls` (Unix) or
`where.exe voidls` (Windows). Follow the [canonical editor guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/EDITOR_INTEGRATION.md)
for the exact adapter installation and tested versions.

- VS Code: build/install the repository's local VSIX; set `void.server.path` to
  the installed absolute `voidls` path, or use supported PATH discovery.
- Neovim: load the installed adapter and call
  `require('void').setup({server='/absolute/path/to/voidls', workspace='/project'})`.
- Vim: load the installed adapter and call
  `call void#setup('/absolute/path/to/voidls', '', '/project')`.

Choose the workspace deliberately; automatic discovery uses exact `main.void`.
The language server shares compiler analysis and does not run source startup for
checking. Installed standard sources resolve relative to its executable; remove
stale `VOID_STDLIB_ROOT` overrides. Diagnostics, hover, completion and navigation
are supported within documented bounds. Formatting/rename are conservative;
Vim rename stays disabled. VSCodium and other combinations need their own evidence.
For startup failure, inspect the editor's VOID/LSP output log and selected server.
