# Install and select VOID with VOIDUP

VOIDUP is VOID's native toolchain manager. The intended route is to obtain a
matching VOIDUP bootstrap, install a complete authenticated toolchain, create a
project and start coding. Installation and ordinary compiler use need no Python
or CMake. VOIDUP uses a dedicated per-user root, never a generic source-install
prefix.

## Availability

There is currently **no verified public VOIDUP bootstrap or production catalog**.
The checked-in manager uses private engineering 2-of-3 test trust roots. Its
implemented Linux workflow is available for controlled engineering qualification;
it must not be presented as a production download. Windows and macOS native
VOIDUP installation remain unqualified. See [release production](RELEASE_PRODUCTION.md)
for the exact publication work. Until a reviewed public channel exists, developers
who need tools can use the separate [contributor source build](../install/README.md).
That build's Python/CMake/SDK requirements are build-system requirements.

## Obtain tools and install a version

Use only a bootstrap and HTTPS catalog supplied for the same reviewed trust set.
`CATALOG_URL` below means that real supplied endpoint; no public URL is implied.
`RELEASE_ID` means a host-compatible identifier printed by `list --available`.

```sh
voidup --version
voidup --catalog "$CATALOG_URL" list --available
voidup --catalog "$CATALOG_URL" install RELEASE_ID --activate
voidup list
voidup run default voidc -- --version
voidup doctor
```

Global options precede the command. Private TLS qualification can supply
`--ca-file /absolute/reviewed-ca.pem`; this replaces TLS roots, not release keys.
Normal distribution uses system TLS trust. Installation checks the catalog's
Ed25519 threshold signatures and freshness before inventory and package download.
It verifies the archive hash, every member, permissions and build identity before
publishing a complete version. The inventory includes `voidc`, `voidls`, Runtime
and native libraries, standard sources, native manifests, Clang/LLD, Clang resource
files, package inputs and licenses. A compiler-only archive is not a toolchain.

## Roots and prefixes

Without `--root`, VOIDUP uses `os.data_dir("voidup")`: the XDG data location on
Linux, LocalAppData on Windows, and Application Support on macOS. Existing ancestors must satisfy the filesystem ownership policy; missing parents
are created privately within a bounded depth. To choose another
location, use `--root /absolute/private/voidup` on **every** invocation. Never point it at an existing `.local`,
`Program Files`, checkout or manual compiler prefix. It admits or creates its own
private root and rejects unsafe ownership, links and permissions.

The root keeps control/trust metadata, interrupted transactions and immutable
`toolchains/RELEASE_ID/` prefixes. Keep each prefix's `bin/`, `lib/void/`,
`share/void/` and `sdk/` together. Do not edit installed files. Standard sources
and packaged native resources are found relative to the selected compiler.
No stdlib override or manually assembled LLVM SDK is needed for this layout.

## Use a terminal or editor

The portable, verified launch route needs no PATH integration:

```sh
voidup run default voidc -- new app --name Hello
cd Hello
voidup run default voidc -- check .
voidup run default voidc -- run .
voidup run default voidc -- build . -o hello
./hello
```

On Windows use `-o hello.exe` and `./hello.exe`. The child's arguments are literal;
VOIDUP rechecks the full installed inventory before launch and supplies a bounded
native environment. A source program's normal exit status is preserved.

For direct `voidc`/`voidls` discovery, print and inspect a session export:

```sh
voidup env --format sh
```

Paste the resulting `export PATH=...` line into a POSIX shell. PowerShell users
run `voidup env --format powershell` and paste that output. These commands only
print text. They prepend the verified version's `bin` while preserving the
existing PATH. They never edit a profile, registry or other installation.
Shell quoting handles spaces, Unicode, quotes and shell metacharacters. A root
containing the host PATH separator cannot be exported; use `run` or editor paths.
The default `voidup env` returns JSON with root, release, build identity and tool
paths. `voidup which voidc` and `voidup which voidls` print individual paths.

For VS Code or VSCodium:

```sh
voidup env --format vscode
```

Merge the two printed keys into editor settings, preserving other settings.
Install the matching prepared VSIX separately; there is no claimed Marketplace
release. Explicit paths work when a graphical editor did not inherit terminal
PATH. See [editor setup](../EDITOR_INTEGRATION.md). Direct PATH/editor launches
are ordinary executable launches; use `voidup run` when verification on each
launch is required. Environment exports and editor paths select a **specific
version**. After switching, regenerate them and restart the language server.

## Update, switch and roll back

```sh
voidup --catalog "$CATALOG_URL" list --available
voidup --catalog "$CATALOG_URL" install NEW_RELEASE_ID
voidup verify NEW_RELEASE_ID
voidup default NEW_RELEASE_ID
voidup list
voidup rollback
```

Installing without `--activate` leaves the default unchanged. `default` verifies
and selects an installed version; `rollback` swaps current and previous, and
requires a previous selection. Release IDs have no automatic version ordering.
There is no `update`, `latest`, pruning or manager self-update command. Obtain a
new reviewed bootstrap separately when the manager itself changes. Old versions
are retained, and `run RELEASE_ID voidc -- ...` selects one explicitly.

A catalog's anti-replay floor is committed before artifact transfer. Installed
versions can be verified/selected offline using retained authenticated metadata;
this is not a fresh revocation lookup. An online catalog refresh retains new
revocations and refuses a lower serial, altered same-serial bytes, lost revocations
or a backwards clock. Deleting/restoring the entire root also loses/restores its
local trust floor; it is not a hardware monotonic counter.

If a transfer is interrupted, rerun the same installation command. The current
selection remains intact and a complete published version can be verified and
registered on retry. Partial transactions remain for inspection; `doctor` reports
them. There is no automatic deletion or aggregate cache quota. Preserve the first
diagnostic and transaction state when asking for help.

## Host dependencies

The packaged LLVM tools and their resources are toolchain contents. Native
application linking still requires the declared OS C/C++ headers, CRT and system
libraries: Ubuntu 24.04 x64 `build-essential`, or the corresponding supported
Windows SDK/MSVC environment. Apple SDK/libc++ requirements remain platform work;
there is no qualified macOS payload. Device libraries for a native package may
add their own requirements. Installing a published toolchain itself requires no
host development SDK.

`voidc add raylib` now hosts a reviewed VOID preparation resource with pinned
HTTPS/SHA-256 inputs, existing native binding emission and the selected Clang.
Linux's official asset is tar.gz: first preparation additionally uses native
`/usr/bin/tar` and gzip on the declared Ubuntu host. Windows uses the VOID ZIP
reader. No Python, CMake or source binding generator runs during consumer add.
Later imports/builds reuse the locked provider; native execution still requires
separate consent. See [raylib](../../verified-packages/raylib/README.md).
