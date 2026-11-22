# VOIDUP production readiness

Engineering work against accepted G96
`5fdfb3d9bd1c4933223a72023d882f0278877e8f`, 21 November 2026.
LLVM/Clang/LLD **23.1.4** throughout. This report supplements the historical
[G96 evidence](G96_VOIDUP_PUBLIC_INSTALLER.md); it does not rewrite that evidence.

Public distribution remains unavailable. Engineering trust is used exclusively
for qualification. No production credentials were created or enrolled, no hosted
workflow was dispatched, and no website, asset, catalog or announcement was
published. Sacred Wrench acceptance remains a separate independent assessment.

The separately authorized [Windows cleanup and update/rollback continuation](../voidup/WINDOWS_COMPLETION.md)
records subsequent storage recovery and focused native results. The original
disk-full failures below remain part of this evidence.

## Implemented changes

- Unix PATH integration now guards the PATH value when multiple startup files
  are sourced, including an initially empty PATH. Setup migrates only the exact
  old G96 snippet, retaining other profile bytes. The existing publisher continues
  to preserve owner, group, mode and access ACLs and reject substituted files.
- Windows uses creation-time Job Object assignment with kill-on-close for the
  manager and its descendants. Unicode/literal argument quoting and `/MT` remain.
  Per-user PATH comparison accounts for case, separators, quotes, trailing
  separators and expandable registry entries while retaining raw unrelated
  entries and registry value type. Registry reads are bounded and handles close
  on failure.
  Installer-owned file operations use extended-length Windows paths, including
  retained notices, without requiring an administrator registry change. Native
  process cwd capture removes the empty trailing component of `absolute(".")`
  while preserving filesystem roots.
  Stable proxies also preserve `PROGRAMDATA`, which Clang's existing Microsoft
  SDK discovery needs. Native before/after probes demonstrated successful linking
  with that OS context restored; caller compiler/loader overrides remain filtered.
- Bootstrap archives and complete toolchain ZIPs normalize archive timestamps,
  ownership and modes using `SOURCE_DATE_EPOCH` (default 1980-01-01 UTC). This
  provides reproducible archives from identical admitted build inputs; it does
  not claim bit-identical independent compiler rebuilds. Bootstrap publication
  refuses changed, missing or additional runnable files against the build receipt.
- The existing package producer records native runtime resolution for compiler,
  language server, SDK driver/linker and bundled shared libraries. Linux payloads
  cannot depend on producer-local libraries. The public Linux package command and
  production CI preparation require actual Ubuntu 24.04 userspace.
- macOS ARM64 engineering recipes now build the bootstrap, package the existing
  install components, copy Clang/LLD/resources, relocate admitted non-system
  Mach-O dependencies with `install_name_tool`, and check architecture/minimum OS.
  Modified binaries receive ad-hoc signatures for qualification. A small driver
  wrapper asks `xcrun` for the consumer's selected SDK/vendor libc++ headers;
  producer Xcode paths and Apple SDK/header redistribution are avoided. The
  actual native Mac execution, redistribution review, Developer ID/notarization
  and Gatekeeper journey remain **UNQUALIFIED**. Production preparation/assembly
  still exclude macOS until those native requirements are satisfied.
- Production assembly now binds the exact asset set to native preparation
  receipts, the reviewed Git source SHA and approved public-root fingerprint.
  Missing or changed assets fail before output creation; unrelated artifact
  files do not become public assets. Publication verification retains the prior
  authenticated serial/revocations and immutable release identities. Publication
  is serialized and checks that the stable channel is mutable before creating a
  version release.
- The existing getting-started page has a reviewable VOIDUP section with honest
  availability and installer/build dependency distinctions. No dead download
  links, automatic platform detection, new hosting service or frontend framework
  was introduced.

No compiler, language-server, Runtime, standard-library, G95 cache, F5 or native
consent implementation was modified. Disposable matching compiler/server builds
are qualification inputs, not replacements for Boss's development installation.
The unrelated VS Code edits present at task start remain outside this candidate.
Bootstrap compilation/linking now finishes in local producer scratch before
copying completed binaries to the requested prefix, including a writable share.
This avoids depending on compiler-native output publication into UNC paths.

## Qualification and measurements

Evidence root:
`/home/shmellyorc/Documents/Void.Language-archives/voidup-production-2026-11-21/`.
Commands, original failures and source identities are retained there. Large
fixtures/builds are task-owned working material, not public artifacts.

The native Linux environment is Ubuntu **24.04.5 x86-64 userspace**, libc
**2.39-0ubuntu8.12**, on the host's PikaOS Linux kernel through Podman. This is
native Linux execution, not cross-compilation or a claim about an independently
booted Ubuntu kernel. Existing pinned LLVM and floor-built provider inputs were
reverified with the current verifiers at their original receipt-bound paths.
Both tools were rebuilt from the accepted compiler sources in an isolated
snapshot. The initial installer run passed 21 consumer cases; the expanded run
passed **28**, including distinct stable release installation, interrupted
transfer, retry, rollback, reinstall and preservation of developer files.

| Artifact / check | Actual measurement or scope |
| --- | --- |
| Linux final engineering bootstrap | 22,158,735 compressed bytes; 39,438,744 expanded payload bytes |
| Linux current full toolchain | 269,230,148 ZIP bytes; 753,075,215 inventory-expanded bytes |
| Linux compiler/server ABI | GLIBC through 2.38; GLIBCXX through 3.4.32, resolved/executed on the declared userspace floor |
| Windows engineering bootstrap | 11,973,353 ZIP bytes; 21,504,216 expanded payload bytes |
| Windows current full toolchain | 158,247,888 ZIP bytes; 326,715,429 inventory-expanded bytes |
| Windows launcher | Native Windows 10 x64 build 19045 (VM label “Windows 11”); Debug and Release; `/MT`; 1 MiB stack |
| Windows launcher imports | Debug: ADVAPI32, KERNEL32, SHELL32, USER32, ole32; Release: ADVAPI32, KERNEL32, USER32; no shared CRT imports |
| macOS | Producer/manifest/shell logic only; native execution, sizes and dependencies **UNQUALIFIED** |

Linux toolchain system dependencies include the ELF loader, libc, libstdc++,
libgcc_s, libm and libz. libidn2/libunistring aliases resolve inside the package.
The bootstrap has its own measured ELF dependencies and still uses standard
sh/mktemp/tail/tar/gzip/rm extraction facilities. Producer Python, CMake and
cryptography are not consumer prerequisites. Native application linking needs
the platform C/C++ headers/CRT/SDK independently of running the installer.

The final bootstrap additionally passed a fresh Ubuntu 24.04.5 container with
Python, Node.js and CMake physically absent, libc **2.39-0ubuntu8.9**,
libstdc++ **14.2.0-4ubuntu2~24.04.1** and zlib **1.3.dfsg-3.1ubuntu2.3**.
It selected the authenticated latest release without catalog/version arguments,
installed terminal discovery, created a project, ran JIT and passed doctor.
Fixture-only `--ca-file` trusts the loopback service; it is not a public install
requirement. Native application linking was qualified in the separate development
SDK-equipped Ubuntu environment. Two archives from the final bootstrap prefix
were byte-identical (SHA-256
`287531d03267491c68555d571634a546e2246b3b4c276abf5aa2485119c64e36`).

Focused checks cover profile ownership/ACLs and absent ACLs, repeated startup,
legacy snippet migration, Linux cancellation/terminal job control, native Windows
literal/Unicode arguments, registry preservation with `RegOverridePredefKey`, and
termination of both held descendant process handles after launcher loss.
Windows registry qualification does not modify the real user's Environment key.

Seventeen offline production regressions exercise deterministic downloads,
native ZIP directory geometry, missing/modified payload refusal, engineering
root separation, receipt/source binding, exact asset assembly, prior serial and
revocation handling, and macOS manifest/shell preparation. Website source/built
checks, its 11 existing checker regressions and actionlint are included.

The first reproducible payload attempt incorrectly deflated empty directories.
The native curated ZIP verifier rejected their nonzero compressed size. The
producer now emits stored zero-size/CRC directory entries; the original failure
log and a focused regression are retained. No native archive/security check was
weakened.

Native Windows testing also caught the launcher's retained-notice path exceeding
MAX_PATH and a trailing cwd separator rejected by the existing process-path
contract. Extended-length launcher IO and normalized cwd capture address those
installer defects. The original fixture's synthetic leaf lacked revocation data;
Schannel correctly refused it. Qualification now reuses the existing native HTTP
fixtures' directly trusted loopback-certificate pattern. No TLS verification or
revocation option was relaxed, and no root was enrolled in Windows' certificate
store. A private producer-only cryptography installation was needed in the guest.
Original failures and their corrections are retained separately.

The native Windows full journey reached successful signed first installation,
proxy discovery with Python/CMake/LLVM absent from consumer PATH, project
creation, JIT, native build/execution, LSP and idempotent setup. The update's
controlled 1 MiB interruption and competing lock check passed, but its retry
exhausted the guest disk (`ERROR_DISK_FULL`, native code 112). The selected
1.10.0 toolchain and authenticated serial 101 remained recorded. This interrupted
suite is **not** a full Windows qualification pass. The original error and exact
control/transaction inventory are retained. A focused retry after reclaiming
generated duplicate transaction ZIP caches also exhausted the disk, despite
700,923,904 free bytes before retry. Selected-tool version and doctor passed
before that retry. These results do not establish a successful Windows update or
rollback. Final abandoned staging reclamation and selected-tool health have a
separate receipt; the signed source packages and diagnostic inventories remain.
After that reclamation the selected compiler and doctor passed again, developer
notes and control bytes were unchanged, and 718,548,992 bytes were free. The
task's installed fixture/builds remain retained until the deferred cleanup.
The installer retains abandoned transactions by design, so update qualification
needs space for both toolchains, archives and failed staging, beyond a single
installed toolchain's expanded size.

Windows Server 2022, real-user registry/new-terminal propagation, Windows
raylib/F5, the complete remaining Windows refusal matrix and native macOS
execution remain **UNQUALIFIED**. The native Windows test kept the actual user
PATH unchanged; registry component tests used an isolated volatile hive.
UNC producer output now works through local scratch; an actual UNC project or
installation journey remains unqualified. Windows Release native builds retain
an existing unused `--ld-path` warning; the packaged manifest explicitly selects
LLD with `-fuse-ld=lld`, and native linking succeeds.

## Developer acceptance and reproducibility

Use the existing `tests/voidup/first_run.py` with a native matching `--build`,
`--sdk`, `--licenses` and `--evidence`. In-memory engineering authorities are the
default; no production private input is needed. `--reuse-work` requires a new
caller-owned location and retains the installation for subsequent developer/editor
acceptance. On Windows, `--installation-root` can put disposable installed versions
on local NTFS while large producer fixtures remain on a shared drive. Both options
refuse an existing supplied test root rather than adopting user data.

The expanded journey tests two signed releases (1.10.0 and 1.11.0, different
release/payload identities using the same compiler build), interruption after a
real 1 MiB transfer, competing writer refusal, full retry, retained abandoned
transactions, rollback, reinstall and preservation of a developer sentinel.
That is a manager update test, not evidence of two different compiler versions.
Native-code approval tests exercise the existing protocol with disposable test
projects; they grant no approval to Boss's actual projects or native libraries.

The actual Linux developer application adds bundled raylib 6.0.0, imports it,
calls `get_color`, and prints `raylib 10 20 30 scalar 0.25`. Boss explicitly approved
this disposable project's native code for build/run/F5. The real compiler stdio
approval protocol saved that consent. Native executable and installed-proxy JIT
execution passed; JIT used its existing explicit `--allow-native` invocation flag.
The compiler's first review prompt expired after its existing 120-second bound
before the asynchronous response was forwarded. The same approved identity was
retried successfully; no second authorization was requested.

VS Code **1.144.0** on the native Linux desktop passed F5 with the existing
installed extension, LLVM 23.1.4 LLDB DAP and the floor-built installed proxies.
Both VOID compiler/server settings remained empty: PATH supplied discovery.
The breakpoint stopped at source line 5, the `observed` local was `0.25`, hover
evaluation returned `0.25`, and the raylib application completed. An isolated
editor profile and fixture approval storage were used. This is desktop-host F5
evidence, separately from Ubuntu-container CLI/native-link evidence; it does not
claim an Ubuntu GUI/editor installation. No extension source was changed.

Additional focused commands:

```sh
python3 -B tests/voidup/production.py
python3 -B tests/voidup/release.py
python3 -B tests/voidup/profile.py --sdk /matching/llvm-23.1.4
python3 -B tests/voidup/launcher.py --sdk /matching/llvm-23.1.4
python3 -B tests/voidup/windows_launcher.py --sdk /native/llvm-23.1.4 --evidence native-launcher.json
```

The Windows-only command must execute on Windows; Linux parsing or cross builds
cannot qualify it. Hosted preparation workflows were edited and statically
validated, never dispatched.

## Review inventory and identities

The review candidate contains these **24 files**, excluding the eight unrelated
editor changes present at task start:

```text
.github/workflows/voidup-publish.yml
.github/workflows/voidup-release.yml
apps/voidup/cli.void
apps/voidup/launcher.cpp
apps/voidup/manager.void
docs/README.md
docs/projects/VOIDUP_PRODUCTION_READINESS.md
docs/voidup/RELEASE_PRODUCTION.md
docs/voidup/USAGE.md
tests/voidup/developer_environment.py
tests/voidup/first_run.py
tests/voidup/policy.void
tests/voidup/production.py
tests/voidup/profile.py
tests/voidup/support.py
tests/voidup/windows_launcher.py
tools/build_voidup.py
tools/ci/voidup_release.py
tools/package_voidup_fixtures.py
tools/voidup_dependencies.py
tools/voidup_macos.py
tools/voidup_release.py
website/dist/getting-started/index.html
website/public/getting-started/index.html
```

`candidate.patch` in the evidence root includes tracked edits and new files;
`candidate-inventory.json` records every candidate SHA-256 and baseline blob.
`review-evidence/` contains selected compact native receipts and original failure
records, with `evidence-index.json` identifying their source paths and hashes.
`reproduction.json` identifies scripts, fixtures, build inputs and retained
environments. No signing private key or TLS private key enters that review set.

The Linux compiler/server build identity is
`7519165a8d6f97def00b067da44e4396e94895f975db411becc7a47dd2c37f5c`;
the Windows pair identity is
`a8882ef8e875e768584632081a99d424bba10cbcdca8c4364d900ec57f4e6952`.
Final Linux native build executable SHA-256 values are
`66737590f574c4184aef0d7c4ca7522c87d9b7416567e5b2d8265bf55962ba8d`
(`voidc`) and
`2af14dcfa5d0c0ab2c3d451b7ab3106f4bd5d8bfb9944a1242b99f94ded0c7c8`
(`voidls`). Windows values are
`e1342ae84bd9f41e49a518a9f0f9e4f32c0998861e8ab514362628c2fc8ff509`
and
`bcde23b9c583c42e84f32fafd7a8f2630a472ef937a7e4f46959ab58af8c40ba`.
Bootstrap receipts bind the actual compiler, application source, compiled public
trust overlay and shipped files. Native launcher receipts bind the final C++
source. Earlier full journey receipts precede the final Windows cwd/OS-context
fixes; final focused Linux bootstrap/policy/environment checks cover those exact
sources without claiming the earlier full matrix was rerun unchanged.

`development-after-verification.json` confirms Boss's installed compiler/server,
extension and standard library retained their initial hashes, as did the eight
preexisting editor edits and root README. HEAD is still the accepted baseline
and the index is empty. Independent acceptance is pending; no staging, commit or
push has occurred. Broad cleanup and the consolidated ZIP are deferred until the
reviewed Git push, as Boss requested. Only failed task-owned fixture staging has
been reclaimed where needed to recover exhausted guest storage.

## Distribution, authorization and remaining work

The three pinned authorities, two-of-three Ed25519 threshold, signature domain
`VOIDUP CATALOG v1\n`, exact catalog bytes, freshness/revocations, payload
inventories, locking and installation transactions remain unchanged. No runtime
trust override or weaker hosting-based authentication was introduced.

Boss still supplies/approves three independent production custodians, their IDs
and exact public keys, protected offline custody/backups, the release source and
version, native acceptance, final redistributed materials, and the initial
bootstrap fingerprint announcement. Real key generation/enrollment, OS signing
credentials and actual public publication require separate authorization.

Before publication, create the mutable `voidup-stable` channel under separate
authorization **before** enabling GitHub immutable releases. New immutable
version releases are composed as drafts, filled, then published; the stable
catalog/signature pair is uploaded last. An interrupted/mismatched pair fails
closed and requires restoring a matching signed pair. Independent VOIDUP
authentication remains mandatory regardless of GitHub/Cloudflare hosting.

Website download links should point directly to approved versioned assets at
`https://github.com/Shmellyorc/Void.Language/releases/download/vVERSION/voidup-HOST.EXT`.
Update the single getting-started section with the approved version/platforms,
measured sizes and run instructions. Unsupported hosts retain their availability
message. A later website update changes the bootstrap download version; the
manager's built-in signed stable channel selects the latest compatible toolchain.
No Python, custom Cloudflare Worker, mutable bootstrap URL or manual catalog ID
is needed. Cloudflare deployment remains a separate owner action.

Established platform references used for implementation:
[Microsoft creation-time Job Object assignment](https://devblogs.microsoft.com/oldnewthing/20230209-00/?p=107812),
[Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects),
[environment change notifications](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settingchange),
[Apple relocation/signing guidance](https://developer.apple.com/documentation/xcode/embedding-nonstandard-code-structures-in-a-bundle),
[Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution),
[GitHub immutable release composition](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases),
and [Cloudflare static redirects](https://developers.cloudflare.com/pages/configuration/redirects/).
Native platform results, rather than those references, determine qualification.
