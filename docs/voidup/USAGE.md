# Install VOID with VOIDUP

VOIDUP installs the latest compatible stable VOID toolchain automatically. The
native manager verifies the signed release catalog and every downloaded file,
installs into a dedicated user directory, and selects the verified toolchain.
Python, CMake and a separately installed LLVM are not installer requirements.

## Availability

**G96 is a review candidate, not a public release.** Production public keys have
not been enrolled and the official catalog/installers have not been published.
Do not distribute the engineering bootstrap: it uses test trust. The Linux
first-install journey has been exercised with authenticated test releases. See
[the G96 report](../projects/G96_VOIDUP_PUBLIC_INSTALLER.md) for the exact native
Windows/macOS qualification limits and remaining publication work.

The following is the prepared end-user workflow, verified with isolated Linux
test releases. There are no public download links to these unpublished assets.

## Download, run, start coding

Obtain the installer for your operating system from the official repository's
published release once it is available.

- Linux x64: run `sh ./voidup-linux-x86_64.run`.
- Windows x64: extract `voidup-windows-x86_64.zip`, then open `Install VOID.cmd`.
  The Windows packaging and per-user PATH route require the native qualification
  recorded in the report before public availability is claimed.
- macOS: bootstrap production fails closed until dependency relocation, complete
  toolchain packaging and native first-install qualification are finished. No
  macOS download is advertised.

The installer downloads, verifies, installs and activates the latest compatible
**stable** release. No catalog URL or release ID is needed. Open a **new terminal**:

```sh
voidc --version
voidc new Hello
cd Hello
voidc run .
```

The installer exposes stable `voidc`, `voidls` and `voidup` launchers. Moving or
removing the downloaded installer does not remove the retained manager. The
launchers use the existing verified native `run` path; selecting another default
or rolling back does not require regenerating shell exports or editor paths.
Full installed-inventory verification on those launches remains an intentional
cost; G96 does not introduce another cache or change verification policy.

Linux integration appends one quoted PATH entry, preserving existing contents,
to `.profile`, `.bashrc` and `.zshenv`, plus existing Bash login profiles. Ordinary
shared directories and writable profiles are supported. Existing profile contents,
owner/group identities, mode bits and access ACLs are preserved; symbolic-link substitution and files the
OS does not permit writing are refused. Bash terminal discovery is exercised; other shells need their own native
qualification. Windows integration prepares an HKCU user-PATH update and environment
notification; it does not change machine PATH. Existing terminals retain their old
environment. `--no-path` on the bootstrap skips profile/registry changes.

## Update or roll back

```sh
voidup update
voidup list
voidup rollback
voidup doctor
```

`update`, `setup`, a bare manager invocation, `install`, and `install latest`
select the highest authenticated numeric stable version for the supported host.
Preview releases, other hosts and revoked releases are excluded. Version order
comes from signed `selection.version`/`selection.channel` fields, never filenames
or release IDs. Older signed catalogs without selection metadata remain usable
with explicit release IDs; automatic selection requires the new metadata.
Ambiguous or malformed stable versions fail closed. An automatic update cannot
silently downgrade an already selected newer stable version.

`rollback` selects the previous verified installation. Existing versions and
interrupted transactions are retained. There is no automatic manager self-update,
key rotation or pruning. Rerunning a newer bootstrap is a separate manager update;
Windows may require closing active launchers before replacing their executables.

## Storage and native requirements

The default root remains `os.data_dir("voidup")`: XDG user data on Linux,
LocalAppData on Windows, and Application Support on macOS. It is a dedicated user
root, never a generic `.local` or manual compiler prefix. Complete immutable
versions live under `toolchains/`; retained bootstraps and stable launchers live
under `bootstraps/` and `bin/`. Failed publication is not permission to delete
arbitrary contents. Retry retains the native trust floor and installed selection.
Storage follows OS access permissions and retains existing directory modes/ACLs;
developers do not need to run chmod or configure special ancestor permissions.
The Linux bootstrap uses a restrictive creation umask for new installer-owned
files. Signing-key confidentiality remains an explicit, separate requirement.

A complete toolchain includes the compiler, language server, Runtime/native
resources, stdlib, packaged Clang/LLD and Clang resources, package inputs and
notices. The bootstrap is **not standalone**: Linux uses the measured system
ELF runtime and standard extraction utilities. Exact sizes/dependencies are in
the report. Production Linux binaries must be rebuilt and exercised on the
supported Ubuntu floor; local developer-host binaries are not those artifacts.

Native application linking still needs the declared OS C/C++ development
headers/CRT/system libraries (`build-essential` on the Ubuntu floor or the matching
Windows SDK/MSVC tools). This is separate from installing VOID or running ordinary
JIT projects. Native package execution still requires the compiler's normal
library consent; installing VOID grants no raylib approval.

The proxies retain CI policy, XDG storage locations and desktop session variables
for the selected tools. Compiler/loader overrides and the caller's PATH remain
excluded. On Unix, redirected editor/LSP cancellation reaches the manager and
its compiler children; interactive terminals keep their foreground job control.

## Advanced and private channels

Global options precede the native manager command:

```sh
voidup --root /absolute/private/voidup --catalog https://your-approved-origin/catalog.json list --available
voidup --root /absolute/private/voidup --catalog https://your-approved-origin/catalog.json install RELEASE --activate
voidup run default voidc -- --version
voidup env --format vscode
```

The URL above is an illustrative private endpoint, not a published service.
Overrides do not change signing keys or bypass the 2-of-3 threshold. `--ca-file`
changes TLS roots only. Private catalogs retain same-origin HTTPS and no arbitrary
redirects. The official GitHub release path admits only the specific HTTPS handoff
to the GitHub release-asset CDN; catalog signatures, exact inventories, byte counts
and hashes remain mandatory after the handoff.

For offline retention, selection, trust-floor recovery and production operations,
see [release production](RELEASE_PRODUCTION.md) and
[production trust enrollment](PRODUCTION_TRUST.md). Until public distribution is
approved, the separate [contributor source build](../install/README.md) remains
available; its producer prerequisites are not end-user installer requirements.
