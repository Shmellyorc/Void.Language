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
`list --available` reports the official channel status. Official install and
update paths are disabled until a trusted production catalog and corresponding
release artifacts are approved.

## Format 1

A pack is a directory containing `target-pack.txt`, `link/` and `licenses/`.
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
```

The compiler checks the compiler version, LLVM version, Runtime ABI, platform,
member names, geometry and checksums before preparing output. The platform fixes
the LLVM triple, generic CPU and 64-bit little-endian ABI. Flat inventories
admit a fixed set of link inputs and regular license files. Version 1 has a
256 MiB input budget, 96-member maximum and a 32 KiB manifest bound. It uses no
pack-provided commands, linker scripts, thin archives or executable hooks.
Custom IDs describe a supported ABI closure. A different ABI requires its own
Runtime and linking rules.

Linux uses GNU startup objects, the target dynamic loader, glibc, libstdc++,
GCC support libraries, the existing prebuilt VOID Runtime and the pinned
provider archives. The application uses the destination's installed system
libraries. Check its auditor's SONAMEs and symbol versions for the deployed
distribution. The Ubuntu 24.04 candidate uses glibc 2.39 and the Ubuntu
24.04 system closure. Musl needs its own Runtime build, startup objects, loader
policy and destination qualification.

A private Windows pack is built in the native Visual Studio developer environment
with `--local-msvc-inputs`. Use it only where your Microsoft terms permit use of
those local developer inputs. It links PE32+ with `lld-link`, the existing
static MSVC CRT, the selected Windows SDK's architecture-specific import
libraries and the native Runtime archives. Keep those developer inputs out of
distribution artifacts. Community MinGW libraries use a distinct C++ ABI and
cannot substitute for these archives. [Microsoft redistribution terms](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files)

A pack publish currently requires an ordinary VOID application without a
managed-native provider. Existing native publishing retains exact catalog
authorization and the verified same-platform provider closure. Source package
dependencies continue through the ordinary dependency lock.

## Hosted artifacts and provenance

The [target pack workflow](../.github/workflows/ci-target-packs.yml) builds
Runtime candidates from the same revision with the pinned SDK and provider
recipes. Linux keeps distro library bytes in their original Ubuntu packages.
The candidate carries immutable APT package hashes and exact member hashes.
GitHub build attestations bind candidate bytes to the workflow and source SHA.
The cross-publisher verifies the attestation, then provisions the Ubuntu
link inputs independently. Destination jobs run relocated distributions from
the foreign producer. Windows pack checks use only the local developer SDK
on their destination runners. [GitHub artifact attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)

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

Apple's SDK agreement governs the SDK. The native Runtime archives still need
link-time interfaces for libSystem, libc++ and system frameworks. A prelinked
VOID Runtime and independently authored minimal link definitions are an option
to investigate, with destination symbol, dyld and signature verification.
Godot's already-linked export templates demonstrate portable assembly but do
not supply these interfaces for newly generated native code. No Apple SDK
payload is included in a pack. Universal output requires both independently
qualified slices and their dependency closure. [Apple SDK agreement](https://www.apple.com/legal/sla/docs/xcode.pdf) ·
[Godot macOS exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html)

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
