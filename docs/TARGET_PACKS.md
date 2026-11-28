# Publishing target packs

A pack supplies destination Runtime and link inputs for the existing VOID LLVM
backend. The host LLVM 23.1.4 SDK supplies one host LLD binary and the stripping tools. VOID selects its
linker flavor from the destination. The compiler snapshots the verified inputs, emits a destination object,
links without running the application, audits the resulting binary and
publishes through the existing exclusive directory transaction. Use the
ordinary native path to publish for the compiler's own platform without a pack.

## Create and use a local pack

From a native Linux GNU Release build with its verified Runtime inputs and
destination GCC development libraries installed:

```sh
python3 tools/target_pack.py /path/to/build /tmp/void-ubuntu-pack --id custom-ubuntu
voidup target add /tmp/void-ubuntu-pack
voidup target inspect custom-ubuntu
voidup target verify custom-ubuntu
voidc publish --target custom-ubuntu --resources embed -o dist/orbit
voidc publish --verify dist/orbit
```

Carry the verified directory to the publishing host if it differs from the
build host. A standalone compiler uses `voidc target` and can select a separate
absolute storage directory with `VOID_TARGET_PACK_ROOT`. The VOIDUP proxy uses
the target store inside its selected VOIDUP root. Publishers need no network
access once the pack and project dependencies are present.

Install a new custom version under a new `custom-*` ID to retain both versions.
Remove a custom registration with `voidup target remove ID`. Re-adding the same
content is idempotent. An occupied ID with different content is refused.

## Acquire an official pack with VOIDUP

An enrolled engineering channel can supply official GNU target IDs. Install and
select its signed compiler release before acquiring a pack.

```sh
voidup --catalog "$TEST_CATALOG" --ca-file "$TEST_CA" target list --available
voidup --catalog "$TEST_CATALOG" --ca-file "$TEST_CA" target install linux-arm64
voidup --catalog "$TEST_CATALOG" --ca-file "$TEST_CA" target update linux-arm64
voidup target verify linux-arm64
voidup run default voidc -- publish --target linux-arm64 -o dist/orbit
```

The usual `voidc` proxy can also publish with the installed pack. The manager
selects the highest compatible signed version for its selected compiler, LLVM
and Runtime ABI. Updates refuse downgrades and conflicting bytes for an
existing release ID or version. The same two-of-three release keys,
freshness checks, HTTPS origin policy and retained revocations apply to pack
catalogs. An archive is bound by its length, SHA-256 and exact member inventory.
The selected verified compiler validates link files and manifest compatibility
before activation. The proxy passes only authenticated selections to the
compiler, which binds its link input snapshot to the signed manifest hash.

A new payload is published to a content-addressed directory before the active
selection changes. An interrupted transfer leaves the previous selection in
place. If the payload was published and activation was interrupted, retry the
same command to verify and reuse the payload. `voidup target remove ID`
unregisters the selection and retains the immutable bytes. After provisioning,
listing installed packs, verification and publishing use local files and
cached signed metadata. The next successful catalog refresh applies newly
learned revocations. The current host's native publish path remains available.

The producer can use `tools/target_pack.py --official-candidate`, then
`tools/voidup_release.py package-target` and the existing engineering catalog
command with `--target-pack`. The catalog carries target packs in an optional
`target_packs.releases` section with minimum manager version 3. Engineering
qualification uses ephemeral signing roots and an isolated HTTPS server.
Public production preparation and promotion still require Boss approval.

## Format 1

A pack is a directory containing `target-pack.txt`, `link/` and `licenses/`.
Linux packs that support HTTP also carry the two pinned GNU library archives
under `source/`.
The manifest begins with the following ordered fields and ends with one
tab-delimited line per member. Text uses LF and each SHA-256 is lowercase hex.

```text
VOID target pack 1
id custom-ubuntu
version 1.0.0
target linux-x64
compiler 0.1.0
llvm 23.1.4
runtime void-aot-64-v1
source <40 lowercase hex characters>
minimum glibc-2.39
<sha256><TAB>link/<file>
<sha256><TAB>licenses/<file>
<sha256><TAB>source/<GNU provider archive>
```

The compiler checks the compiler version, LLVM version, Runtime ABI, platform,
member names, geometry and checksums before preparing output. The platform fixes
the LLVM triple, generic CPU and 64-bit little-endian ABI. The inventory admits a fixed set of link inputs, regular notices and the two
named source archives. Put related notices under up to two directory levels
inside `licenses/`. The publisher copies those notices with the same relative
names and includes their hashes in the publication receipt. Link and source
inputs have exact flat names. Version 1 has a
256 MiB input budget, 96-member maximum and a 32 KiB manifest bound. It uses no
pack-provided commands, linker scripts, thin archives or executable hooks.
Custom IDs describe a supported ABI closure. A different ABI requires its own
Runtime and linking rules.

Linux uses GNU startup objects, the target dynamic loader, glibc, libstdc++,
GCC support libraries, the existing prebuilt VOID Runtime and the pinned
provider libraries. The Ubuntu 24.04 candidate uses the installed Ubuntu
system closure and a glibc 2.39 floor. The publisher includes the pinned,
replaceable libidn2 and libunistring pair only if the executable imports it.
That publication carries the corresponding source and notices alongside its
verified relative library directory. Its auditor accepts only the fixed
`$ORIGIN/native` application search path and the exact two-library payload.
Other dependencies use the destination's installed system libraries. Check
the audit's SONAMEs and symbol versions for the deployed distribution.
Musl needs its own Runtime build, startup objects, loader policy and
destination qualification.

A private Windows pack is built in the native Visual Studio developer environment
with `--local-msvc-inputs`. Use it only where your Microsoft terms permit use of
those local developer inputs. It links PE32+ with `lld-link`, the existing
static MSVC CRT, the selected Windows SDK's architecture-specific import
libraries and the native Runtime archives. Keep those developer inputs out of
distribution artifacts. Community MinGW libraries use a distinct C++ ABI. The hosted LLVM-MinGW UCRT
probe links both architectures offline on Linux and executes them on native
Windows. The probe uses a separate C++ closure. A Windows GNU pack will need
the complete VOID Runtime and native providers rebuilt for that closure, plus
a distinct target ABI and destination binary audits. The Microsoft list covers
specific redistributables. Keep the private MSVC link inputs local.
[Microsoft redistribution terms](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files) ·
[Microsoft Visual Studio REDIST list](https://learn.microsoft.com/visualstudio/releases/2022/redistribution) ·
[LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw)

A pack publish currently requires an ordinary VOID application without a
managed-native provider. Existing native publishing retains exact catalog
authorization and the verified same-platform provider closure. A cross-publish
variant requires a destination-specific catalog and descriptor, exact digest
authorization for that variant, architecture and ABI verification of every
link and runtime input, and a way to admit declarations without loading the
destination provider on the publishing host. The established dependency lock
still applies to source packages.

## Hosted artifacts and provenance

The [target pack workflow](../.github/workflows/ci-target-packs.yml) builds
Runtime candidates from the same revision with the pinned SDK and provider
recipes. Linux keeps its system linking inputs in their original Ubuntu
packages. The candidate carries immutable APT package hashes and exact member
hashes, along with the GNU HTTP libraries and their corresponding source.
GitHub build attestations bind candidate bytes to the workflow and source SHA.
The workflow exercises Linux x64 to Linux ARM64, and Windows x64 and macOS
ARM64 to both Linux architectures. Publishers verify the attestation before
provisioning the Ubuntu link inputs independently. Native destination jobs
execute relocated copies. When transferring a Linux distribution from
Windows, use an archive with Unix execute bits on `app` or set those bits after
extraction. Windows pack checks use only the local developer SDK on their
destination runners. [GitHub artifact attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)

Treat a candidate as an engineering artifact. VOIDUP uses its own established
release trust policy for active production channels. Attestation establishes
build provenance and does not select a production release or grant
redistribution rights.

## macOS bundles and signing

Use `voidc publish --format app --icon assets/orbit.icns` on the native
macOS toolchain. The icon is optional. The distribution contains an
`<application-id>.app` with `Contents/Info.plist`, `Contents/MacOS/app` and
`Contents/Resources`. Manifest application identity supplies the display name
and identifier. Apple bundle versions use the numeric SemVer core. File-backed
assets go under `Contents/Resources/assets`. The native bundle gets an ad hoc
signature before final auditing and private diagnostic binding.

`voidc publish --bundle EXISTING-MACOS-DISTRIBUTION -o NEW.zip` can run on a
non-macOS host. It verifies the input and produces a ZIP with the destination
execute bits and UTF-8 names. It can archive an existing bundle or assemble one
from a flat Mach-O publication. An assembled ZIP preserves the input binary's
signature and can be signed as a bundle on macOS. CLI applications can keep
the default flat distribution.

The hosted macOS probe links an executable for each architecture offline on
Linux using repository-authored definitions for a small adapter interface and
the dyld binder. It builds each adapter natively, checks the Mach-O slices and
ad hoc signatures, and runs on macOS ARM64 and Intel. Apple SDK assets stay
on the native runners. Full VOID support still requires a complete prelinked
Runtime, a stable exported C ABI, its signed dylib closure and an audited
packaging layout. LLVM 23.1.4 publishes a macOS ARM64 SDK. The Intel VOID
compiler needs a reproducible Intel SDK build from the pinned LLVM sources.
Universal application qualification depends on both full native VOID slices.
[Apple SDK agreement](https://www.apple.com/legal/sla/docs/xcode.pdf) ·
[LLVM 23.1.4 release assets](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4) ·
[GitHub hosted runner images](https://github.com/actions/runner-images)

For direct public distribution on macOS, use a Developer ID Application
certificate, sign the complete app with hardened runtime and a secure
timestamp, verify with `codesign --verify --strict`, submit to
`xcrun notarytool` and staple the resulting ticket. On Windows, use an
appropriate Authenticode certificate with `signtool sign` and verify with
`signtool verify /pa`. External signing changes the image digest. Preserve
the original publication and its diagnostic archive together, and verify the
signed distribution through its destination signature and a new distribution
checksum. [Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) ·
[Microsoft SignTool](https://learn.microsoft.com/windows/win32/seccrypto/signtool)
