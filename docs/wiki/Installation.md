# Install VOID with VOIDUP

VOIDUP is VOID's native toolchain manager. The intended workflow is to obtain
VOIDUP, install a complete toolchain, [create a project](https://github.com/Shmellyorc/Void.Language/wiki/First-program)
and start coding. It manages the compiler, language server, Runtime, standard
library, native resources and packaged LLVM/Clang/LLD components together.
Installing a published toolchain does not require Python, CMake or a manually
assembled LLVM SDK.

## Check availability first

A public VOIDUP bootstrap and production release catalog are **not yet available
as verified downloads**. The implemented manager currently uses private
engineering trust credentials; those are not public release credentials.
The commands below describe its implemented workflow for a matching supplied
bootstrap and catalog. There is no public download URL to paste today.
The [toolchain guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/voidup/USAGE.md)
records availability and platform limits. If you need to build tools now, use the
separate [advanced source workflow](https://github.com/Shmellyorc/Void.Language/wiki/Installation#build-from-source).

## Install a supplied toolchain

Set `CATALOG_URL` to the actual HTTPS endpoint supplied with your reviewed
bootstrap. Replace `RELEASE_ID` with an identifier from the available list:

```sh
voidup --version
voidup --catalog "$CATALOG_URL" list --available
voidup --catalog "$CATALOG_URL" install RELEASE_ID --activate
voidup list
voidup run default voidc -- --version
voidup doctor
```

Global options come before the command. The manager authenticates the catalog,
checks the full inventory and publishes the complete version before activation.
Keep any diagnostic if a step fails; [Troubleshooting](https://github.com/Shmellyorc/Void.Language/wiki/Troubleshooting)
explains useful checks. A private TLS service can require an explicitly supplied
`--ca-file`; it does not replace release signature verification.

## Choose your installation root

The default is a dedicated `voidup` directory under your OS user-data location:
XDG data on Linux, LocalAppData on Windows, or Application Support on macOS.
To choose a different root, place `--root /absolute/private/voidup` before every
command. Missing parents are created privately; existing ancestry must pass
ownership checks. Do not select an existing manual installation
or a general-purpose prefix. VOIDUP checks private ownership and permissions.

Each version has its own `toolchains/RELEASE_ID/` prefix. Keep `bin/`, `lib/void/`,
`share/void/` and `sdk/` together. Standard-library and native-resource discovery
are relative to the compiler; ordinary use needs no `VOID_STDLIB_ROOT` override.

## Make tools discoverable

You can always launch through `voidup run default voidc -- ...` without changing
PATH. For direct terminal commands, inspect the output of:

```sh
voidup env --format sh
```

Paste the printed export into your POSIX shell. PowerShell users use
`voidup env --format powershell`. The export prepends the selected version's
`bin` to the current session's PATH. VOIDUP only prints it; it never rewrites
profiles, registry settings or unrelated PATH entries. Then check `voidc --version`.
On Unix, `command -v voidc` and `command -v voidls` show what the shell selects;
on Windows use `where.exe voidc` and `where.exe voidls`.

For [editor setup](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup), run
`voidup env --format vscode` and merge its two literal executable paths into your
settings. `voidup which voidc` and `voidup which voidls` print individual paths.
Direct tool launches use that version; `voidup run` verifies the inventory before
each launch. Regenerate session/editor settings after changing versions and
restart the language server.

## Update, switch and roll back

```sh
voidup --catalog "$CATALOG_URL" list --available
voidup --catalog "$CATALOG_URL" install NEW_RELEASE_ID
voidup verify NEW_RELEASE_ID
voidup default NEW_RELEASE_ID
voidup list
voidup rollback
```

Installing without `--activate` leaves the current choice unchanged. `default`
selects a verified installed version; `rollback` returns to the recorded previous
selection. Old versions remain installed. There is no automatic version ordering,
pruning or manager self-update command. Obtain a newer matching bootstrap
separately when needed. After an interrupted download, rerun the installation;
partial transactions remain available for diagnosis.

## Native builds and host libraries

The complete toolchain contains Clang/LLD 23.1.4 and their resource files.
[Native linking](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation) can still need OS development libraries:
Ubuntu x64 C/C++ headers and CRT, or the matching Windows SDK/MSVC environment.
Native packages can require additional device libraries. Those are distinct from
building the compiler yourself. Current VOIDUP execution evidence is Linux x64;
Windows and macOS native installation are not qualified. See
[platform limits](https://github.com/Shmellyorc/Void.Language/wiki/Resources-and-status#platform-limits).

## Build from source

Compiler contributors and advanced users can follow [source installation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/README.md)
for [Linux](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/linux.md),
[Windows](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/windows.md) or
[macOS](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/macos.md).
That workflow requires Python 3.12+, CMake 3.24+ and a complete LLVM 23.1.4
development SDK. It builds the compiler and dependencies locally; it is separate
from installing an authenticated toolchain with VOIDUP.

Next: [Create your first application](https://github.com/Shmellyorc/Void.Language/wiki/First-program).
