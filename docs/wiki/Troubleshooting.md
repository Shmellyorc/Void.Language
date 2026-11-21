# Find the cause of a failed development step

Start with the first diagnostic and the exact command. A compiler failure,
application error result and nonzero application status have different meanings;
see [CLI status](https://github.com/Shmellyorc/Void.Language/wiki/CLI-commands#interpret-output-and-status) and
[result handling](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values).

## Check the selected installation

For a VOIDUP root, begin with `voidup list`, `voidup doctor`,
`voidup which voidc` and `voidup which voidls`. Include the same `--root` option
you used during installation. `voidup run default voidc -- --version` shows the
manager-selected build. After switching, refresh `voidup env --format sh` (or
`powershell`) and editor paths from `voidup env --format vscode`.

On Unix:

```sh
command -v voidc
command -v voidls
voidc --version
```

On Windows use `where.exe voidc` and `where.exe voidls`. Confirm that the tools
belong to the same installation. When reporting a problem, include the version,
source revision if known, OS/architecture, exact command and first diagnostic.
Avoid including credentials or private source unnecessarily.

## Follow the symptom

| Symptom | Useful next step |
| --- | --- |
| Catalog unavailable or no public bootstrap | No verified public VOIDUP download/catalog is available yet. Use the availability section in [Installation](https://github.com/Shmellyorc/Void.Language/wiki/Installation#check-availability-first); do not substitute test credentials or an invented endpoint. |
| Signature, inventory or replay check fails | Keep the diagnostic and root. Check clock and the supplied matching catalog/bootstrap; do not weaken verification or delete trust metadata to force an update. |
| Download interrupted | Repeat the same VOIDUP install command. `doctor` reports retained transactions; the current version stays selected until successful activation. |
| SDK/dependency setup fails | Read `source-install.json` and adjacent logs in the chosen build directory. Follow the native [installation guide](https://github.com/Shmellyorc/Void.Language/wiki/Installation). |
| Standard imports cannot resolve | Keep the installed prefix together. Remove a stale `VOID_STDLIB_ROOT` override unless you deliberately need matching external sources. |
| No main, ambiguous main or wrong source root | Check the [manifest/source selection](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules); do not rename files blindly. |
| Invalid `--app` or profile option | Remove obsolete `--app`; profiles require manifest project mode. See [CLI commands](https://github.com/Shmellyorc/Void.Language/wiki/CLI-commands). |
| Native linker/resources are missing | Retain the original pinned SDK and matching manifests/archives. See [Native compilation](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation#keep-matching-resources). |
| A dependency or lock is stale | Inspect the changed manifest/source and `voidc deps explain`; review `voidc tidy --check` before choosing an explicit update. |
| Native execution is refused | Review the exact [provider and approval](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib); do not bypass a corrupt lock by weakening checks. |
| An integer is rejected for a selector | Pass the required [nominal enum](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags), or an explicitly documented [raw keyboard-code operation](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib#typed-input-and-raw-key-codes). |
| Async source checks but does not run | Ordinary CLI execution has no implicit [async root host](https://github.com/Shmellyorc/Void.Language/wiki/Async#check-and-execute-the-example). |
| Editor has no diagnostics/completion | Trust the intended folder; inspect **Output → VOID**, actual server path and [project configuration](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup). Repair imports, refresh or restart after fixing configuration. |

## Reduce a source problem

Keep a small copy outside your original project, retain the exact failure and
remove unrelated code until the behavior is clear. Do not replace a legitimate
Runtime diagnostic with a silent default. Check whether a syntax or feature is
implemented in the [language specification](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md), especially
when reading older discussion or future design notes.

Use the guide for your OS and architecture: SDKs, native providers and editor
executable paths are platform-specific. See
[Status and further reading](https://github.com/Shmellyorc/Void.Language/wiki/Resources-and-status) for those boundaries and the
[canonical troubleshooting guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/TROUBLESHOOTING.md) for detailed SDK,
native, source installation and LSP diagnostics.

Next: return to [Home](https://github.com/Shmellyorc/Void.Language/wiki/Home) or [your first application](https://github.com/Shmellyorc/Void.Language/wiki/First-program).
