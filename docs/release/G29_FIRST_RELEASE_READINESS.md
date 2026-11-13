# G29-R — first release readiness and installation audit

13 November 2026 · **Research for Golden Wrench/Boss review. No release approval.**

Actual HEAD: `9330e338d3473665dc1b8b9496a75f67d1c13423`, initially clean.
Its only change from `e3e6018430ac68235d2d91fd4b7ce614aa8ce3b6` is the accepted
[G28 report](../compiler-health/G28_REAL_APPLICATION_READINESS.md). The production
baseline therefore remains G27. Compiler/server inputs were reused, with identities
checked against G27/G28 records. No production implementation or hosted execution.

## Decision

**The remaining work is primarily release qualification, payload preparation and
owner-controlled distribution decisions—not another language feature milestone.**
Existing source installation, native application packaging and editor mechanisms
work in the fresh prefixes tested here. Public binary portability and public
VOIDUP deployment are different obligations and remain unestablished.

One small engineering correction is justified before replaying the existing
installation qualification workflow: its dependency phase predates the required
host providers. Regenerating a stale local VSIX, including its missing VOID license, and reconciling contradictory
onboarding/release text are also justified. Neither needs a new installer,
package manager, Runtime, scheduler or distribution framework.

The smallest recommended initial surface is a versioned **source-install release
candidate**, using `tools/install.py`, with a matching generic VSIX if Boss wants
to distribute one. Binary toolchains through VOIDUP remain an additional explicit
scope choice. This recommendation does not silently remove any required platform
or final comprehensive gate. Boss chooses the actual supported platform/feature
claims and publication surface.

## Existing routes and developer journey

| Step | Implemented route and evidence | Remaining boundary |
| --- | --- | --- |
| Obtain compiler | Repository source at an exact revision; current platform recipes | No approved public binary channel or production VOIDUP catalog established |
| Prepare toolchain/install | `tools/install.py --prefix DIR`; exact SDK helper, HTTP and host builders/verifiers, existing CMake components, installed verification | Requires platform prerequisites; wrapper's complete fresh Windows/macOS source-build path is not qualified by this audit |
| Identify compiler | `voidc --version` prints development version, LLVM, target and build identity | Command is `--version`, not `voidc version`; choose the RC identity/channel before publication |
| New/check/run/build | `voidc new DIR`; `check/run/build --project DIR` | Synchronous CLI entry; explicit async host is unchanged |
| Author/use library | `new helper --kind library`; `add helper --path PATH --project APP` | Source identity, locks and native approval remain distinct |
| Immutable Git sources | Existing explicit D3/G26 `add ALIAS --git URL --rev FULL_SHA`, then verified offline consumption | G26/G28 supply evidence; no new fetch or publication here. Raylib's standalone public repository remains unpublished; the rejected monorepo acquisition is not repaired by pretending it is a package |
| Standard library | Installed `share/void/stdlib`, discovered relative to executable | All 29 Linux installed modules match current sources; explicit override still wins |
| Native application distribution | Existing `voidc package`, verification and relocation | D5 is a checked application layout, not a hermetic OS/CRT bundle |
| Editor | Generic VSIX, separate matching `voidls`/`voidc`, trusted local workspace | Regenerate from current source; actual workbench acceptance is reused from G23/G28, not inferred from CLI installation |

[Install guide](../install/README.md), [first program](../FIRST_PROGRAM.md),
[G27](../projects/G27_LIBRARY_AUTHORING.md), [G26](../projects/G26_PACKAGE_MAINTENANCE.md)
and the actual command help supply the current workflow. The Language Bible's
older “Current Slice 1” and external-packages-future passages are historical and
conflict with delivered functionality; they must not override those contracts.

### VOIDUP already supplies the installer architecture

`apps/voidup/{manager,inventory,storage,transport,trust}.void` implement versioned
immutable payloads, complete verification, atomic selection, rollback, held private
roots, bounded HTTPS and authenticated inventories. Existing Phase B packages
relocate the SDK subset **before signing**; VOIDUP does not rewrite signed bytes.
No second installer is warranted.

`apps/voidup/trust_keys.void:1` explicitly identifies engineering trust roots;
`cli.void` describes a private catalog. The retained Linux Phase B acceptance
covers actual toolchains, not a production channel. Public trust/key ownership,
bootstrap delivery and exact production payload qualification are still separate.
Native Windows/macOS VOIDUP application acceptance remains unqualified. Lack of
pruning/self-update is a documented boundary, not an automatically mandatory RC
feature. [Phase B](../voidup/PHASE_B_REPORT.md) · [Integration](../voidup/INTEGRATION_REPORT.md)

## Fresh observations and their limits

### Linux x64

Installed `compiler` and `tooling` from the accepted Release build into a new
private prefix. A copy of the generated CMake install script redirected **only
its two receipt destinations** to scratch; payload paths, resources, notices,
permissions and RPATH rules stayed unchanged. The original build was not
reconfigured or rebuilt. This reuses the established isolated-install technique.

- Both component installations and maintained `tools/verify_install.py` passed.
  Verification covers native-resource hashes, installed console source, optimized
  and unoptimized JIT, independent native build/run and initialize/shutdown/exit.
- Renamed the whole prefix and repeated maintained verification: PASS.
- Created an application and a library; added the local library; check, JIT,
  native build/run, package, package verification and relocated execution passed,
  returning exact `42\n`.
- In a disposable Bubblewrap namespace, hid the **entire checkout**, including
  SDK/build/provider source paths. Installed check/JIT and the packaged app still
  passed. Native build failed explicitly on the missing original Clang path,
  with the existing instruction to keep the SDK and matching install together.
  No real source directory was renamed or altered.
- The compiler component plus tooling contains 139 files, 147,663,285 bytes.
  Full identities and commands are retained in the archive.

This proves an isolated install on the current PikaOS developer host. It is not a
new clean source build or a clean-machine installation of the source wrapper.

### Actual Ubuntu userspace probe: current developer artifacts are unsuitable

Used the existing Ubuntu 24.04 image
`d9b8c34d61a9b9a3c44cab883d7d2f2619d674966ab520186f799cf32e3e0377`
in fresh, automatically removed, network-disabled containers. Mounted only the
candidate prefix or D5 package read-only; no checkout, SDK, host library or home
mount was supplied. The image reports Ubuntu 24.04.5. This is a shared-kernel
userspace probe, not independent hardware or a complete new source installation.

Both commands exited **1 before application entry**:

- Installed `voidc --version`: missing `GLIBCXX_3.4.35` and `GLIBC_2.42`,
  `GLIBC_2.43`, `GLIBC_2.44` symbol versions.
- The generated D5 app: missing `GLIBCXX_3.4.35` and `GLIBC_2.42`.

The binaries were linked on the newer developer system. LLVM being pinned does
not pin host glibc/libstdc++ or startup objects. GCC's [ABI policy](https://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html)
describes the runtime ABI/version compatibility requirement. The smallest
existing solution is to produce release binaries on the selected support-floor
userspace, with the existing provider/toolchain recipes; alternatively distribute
source and build locally. The previous Ubuntu/VOIDUP records already use that
architecture. **No new current-SHA floor build was performed here**, so it still
needs qualification. Do not copy newer libc into the package or advertise these
PikaOS outputs as Ubuntu binaries.

[D5's existing contract](../projects/DEPENDENCY_D5_CLOSEOUT.md) removes development
RPATH/RUNPATH and deliberately leaves OS ABI libraries to deployment. Fresh ELF
inspection confirms dependencies including libidn2.so.0, libunistring.so.5,
libstdc++, libgcc, libm and libc. The package ran with the checkout hidden on the
same host; that does not prove those dependencies exist on another installation.
Document and qualify their ABI floor instead of inventing another bundler.

### Native Windows x64

Used the existing VM labelled “Windows 11”; the guest reports **Windows 10 build
19045**, native x64 VS 2022 environment, MSVC **14.44.35207**, SDK **10.0.19041.0**.
The Release compiler/server match accepted G27/G28 hashes. Staging used the
existing private-root verifier on NTFS. No ACL principals, sharing modes,
privileges, SDK binaries or active build/source files were changed.

Both component installs, maintained verification, prefix relocation and repeat
verification passed. New application/library/add, check, JIT, native build/run,
D5 package/verify and relocated application execution passed (`42\n`). Physical
pinned LLVM 23.1.4 and existing profile-matched development inputs remained
available. This is a fresh prefix on the established developer VM, **not** a
fresh Windows machine or a new end-to-end `tools/install.py` source build.

The original install attempt failed because the deliberately partial guest source
snapshot lacked `docs/release/LICENSING_SCOPE.md`. It was not a Windows ACL or
DLL failure. The smallest correction was to supply 107 exact current install
source inputs in a **task-owned shadow** and redirect the disposable install
script's source-input paths there. Every supplied hash was checked afterward;
the active source and original install script remained unchanged. The original
CMake error is retained. A real source install requires a complete source checkout.
No upstream API workaround or production change is justified by this setup error.

No fresh Windows Debug campaign was necessary: G27/G28 already qualify its
application behavior. G28's missing auxiliary async-host fixtures and the older
file-symlink privilege/cleanup incidents retain their original boundaries.

### Setup errors retained separately

The disposable journey initially used the wrong `add` syntax, then accidentally
created a second main, and assumed a `bin/` subdirectory in D5. The compiler
correctly supplied `add ALIAS --path ...` usage and `VOID5010` with both main
locations. Using the generated `src/app.void` and the package's actual root-level
executable corrected the probes. No production behavior was changed. Initial
CMake script probes also used normal variables that their cache declarations
replaced; corrected probes use explicit CACHE entries. These setup failures are
not manufactured product defects or counted as successful qualifications.

## Editor installation and the stale VSIX

The retained generic `editors/vscode/dist/void-language-0.1.0.vsix` installs into a
separate VS Code 1.144.0 user/extension directory, but it is **older than current
source**: its manifest and JavaScript omit `void.packageProject`, and its
preflight timeout is 10 seconds. Current `src/extension.ts:332` registers Package
and `src/config.ts:71` uses 60 seconds. The source package command already rebuilds
before packaging (`package.json:206–207`). This is stale local output, not a
missing implemented command.

Tested the smallest correction: copied current extension inputs to scratch,
reused installed locked dependencies, selected Node **24.19.0**, and ran the
existing `npm run package`. Type checking, compilation and VSCE packaging passed.
The regenerated manifest/JavaScript contain Package and the 60-second bound;
runtime third-party notices are present. Forced installation/listing in the
isolated editor directories passed. No owner extension/profile was changed.
The symlinked dependency location changes generated bundle paths/names; byte
identity with another local build is not claimed. The final public artifact
should use the normal clean locked-input recipe, not this disposable copy.

Generic VSIX contains **no native server**. Minimum declared VS Code API is
1.141.0; VSCodium execution is separately qualified, not inferred. The
[official VS Code documentation](https://code.visualstudio.com/api/working-with-extensions/publishing-extension)
supports VSIX installation independently of Marketplace publication and defines
`engines.vscode` compatibility. Current G23 native UI acceptance is reused;
installation/listing alone does not establish fresh workbench activation.

A separate **confirmed packaging omission** remains even after ordinary regeneration:
the eight-member VSIX has neither VOID's complete MIT text nor its owner copyright
notice. `package.json` saying MIT, README links and other dependencies' MIT texts
do not supply that notice. `.vscodeignore` excludes all unlisted files and the
package command uses `--skip-license`. The [MIT text](https://opensource.org/license/mit)
requires its copyright and permission notice in copies/substantial portions.

A second disposable control copied the authoritative root `LICENSE`, added its
packaging allowlist entry and reran the existing package command: the VSIX now
contains the **byte-exact complete VOID license**. Preserve DRY in engineering by
copying the authoritative root license in the existing packaging/build step and
allowlisting that generated payload; do not maintain another independent license
text. This closes one demonstrated omission only, not the full VSIX/provenance
review. No production file was changed.

A specific stale instruction at `editors/vscode/README.md:111–113` requires
`VOID_STDLIB_ROOT` for relocated installations. Actual `src/frontend/source.cpp:133–149`
prefers the executable-relative installed tree before the configured source
fallback. Fresh relocation and hidden-checkout results demonstrate that the
override is unnecessary for a complete installed layout. Correct this text; do
not add another discovery mechanism.

## Release contents: tools versus generated applications

| Surface | Required material and prerequisites |
| --- | --- |
| Source installation | Complete matching source; Python 3.12+, CMake 3.24+, native C/C++ development tools, complete pinned LLVM 23.1.4 SDK; HTTP then host providers and their source receipts. `tools/install.py:110–133` already prepares/verifies host inputs and installs both components. No PATH/profile or OS package changes are performed automatically. |
| Installed compiler/tooling | Matching voidc/voidls; stdlib; Runtime/native/HTTP/host archives and applicable shared libraries; native manifests/hashes; package-notice manifest; licenses, source inputs, editor adapters. Keep prefix together. Native linking still needs the external SDK and platform CRT/development files. |
| Linux generated D5 application | Root-level executable, metadata, verified assets/provider payload, mapped notices; compatible documented OS dynamic libraries. Compiler/LLVM development SDK is not an end-user application prerequisite. |
| Windows generated application | Qualified PE, D5 metadata/native payload/notices; OS/native-provider requirements. `/MT` and a successful developer-VM run do not certify every independent Windows install or every dynamically loaded provider. |
| Generic VSIX | Current bundled JavaScript, grammar/configuration/manifest and runtime dependency notices; separate matching native server/compiler. npm/Node are build-time inputs, not a newly invented runtime installer. |
| Public VOIDUP package | Authenticated exact inventories and production trust, approved SDK subset/bootstrap, payload-specific dependency/source/notice closure and supported-host qualification; reuse Phase B producer/manager design. |

A source release avoids bundling a compiler SDK, but does not automatically settle
rights to imported source/data. A binary release additionally needs the actual
linked/bundled closure, not just an inventory of all repository files.

## Licensing and redistribution obligations

VOID-original code already carries the owner-selected MIT license. The fresh
installs carry MIT, LLVM and dependency notices, corresponding-source material and
resource receipts through existing CMake rules. All 21 common entries in the
current notice inventory matched installed hashes (the two Windows-only entries
were not part of that Linux check). This is evidence of installed
material, **not a legal-clearance finding**. Preserve the current inventory and
review the exact selected payload rather than restarting an abstract license audit.

The existing `REDIST-GNU` entry remains unresolved for Linux binary distribution:
usable corresponding source/build inputs, effective ABI-compatible replacement or
applicable relink route, applicable terms and any conditional installation duties.
Editable native-resource manifests prove neither compliance nor violation. The
[LGPLv3 text, section 4](https://www.gnu.org/licenses/lgpl-3.0.en.html) provides the
shared-library/relink alternatives; the concrete selected delivery must be reviewed
against them without disabling integrity checks. No new replacement experiment or
legal conclusion is claimed here.

Exact Windows CRT/DIA/SDK, Apple SDK-derived material, generated Unicode provenance,
VSIX/npm and any bundled LLVM subset retain the scoped owner/provenance obligations
in [licensing scope](LICENSING_SCOPE.md) and [redistribution notices](REDISTRIBUTION_NOTICES.md).
A first source-only candidate and a bundled VOIDUP toolchain are different reviews.

The installed `REDISTRIBUTION.txt` currently repeats the obsolete claim that FILE200
20.091 ms failing <10 ms blocks release (`REDISTRIBUTION_NOTICES.md:74–76`). Boss's
later directive and the current preview reconciliation make that target advisory.
Correct the stale policy sentence without altering license terms or suppressing
historical measurements. **This audit creates no performance release blocker.**

## Platform and CI reconciliation

| Platform | Established evidence | Essential qualification boundary |
| --- | --- | --- |
| Linux x64/glibc | G27/G28 Release/Debug/strict and real applications; fresh G29 installed-prefix journey; historical Ubuntu source install and private VOIDUP floor builds | Exact current RC produced on declared floor; fresh install/SDK/provider recipe and actual binary dependency execution. No musl, Linux ARM64 or lower glibc claim |
| Windows x64 | G27/G28 native Windows 10 Debug/Release; G29 Release fresh prefix; accepted W1/W2 Server 2022 runs | Current source wrapper/installer path on the selected fresh host; retained privilege-sensitive controls. Existing MSVC/SDK presence must not be disguised as clean-machine proof |
| macOS ARM64 | Historical source-install acceptance; run 43101701646 at 31608c2d: native app/context and editor Debug/Release | No fresh G29 Apple host. Current wrapper and later G07/G06/G27 scope need selected native evidence. Managed-native/D5 and atomic-save admission remain unsupported/unqualified as documented; macOS14 deployment flag is not macOS14 execution evidence |
| Other targets | None added | Intel macOS, Linux ARM64/musl and Windows ARM64 must not inherit x64/ARM64 claims |

The selected comprehensive workflow is **`ci-link-experiment.yml`**, with seven
native jobs, HTTP bootstrap, host bootstrap, full CTest, strict leak detection and
Debug editor hosts. Its retained “experiment” name is not evidence that another
workflow must be invented. The final integrated project/raylib gate remains
**deferred and Boss-controlled**. G27/G28 local successes do not close it.

Two distinct workflow observations must not be confused:

1. Older `ci.yml` configures without HTTP/host provisioning. It is explicitly
   distinguished from the selected workflow in the integration record. Missing
   providers cause the current CMake gates to refuse configuration. Do not choose
   it as the final gate without reconciliation; fixing this obsolete route is
   not a prerequisite if Boss uses the already prepared selected route.
2. `ci-install-qualification.yml` calls `tools/ci/install_smoke.py`. Its
   `dependencies()` at lines 198–208 prepares HTTP only; `configure()` immediately
   follows. Current `cmake/HostDependencies.cmake:27–34` unconditionally requires
   the matching host receipt. **This installation replay recipe needs updating.**
   Its earlier three-host acceptance remains valid at its recorded SHA.

Focused local probes executed the actual CMake provider gates with explicit absent
prefixes. Missing HTTP produced the setup instruction at `HttpDependencies.cmake:31`;
missing host failed at `HostDependencies.cmake:34`. Both expected exits were 1.
The existing real Release HTTP and host verifiers both passed against retained
prefixes. An initial harness cache-variable mistake is preserved separately.
No hosted failure or full cold configure is claimed from these probes.

**Smallest recipe correction:** reuse the host setup/verification sequence already
in `tools/install.py`, or the existing selected-workflow host helper, after HTTP
and before configure. Preserve fresh-prefix/source verification, profile/platform
identity and the same install smoke assertions. Do not add a new dependency
bootstrap. The exact revised cold three-platform workflow still needs native
execution after review; positive prefix verification alone is not that acceptance.

### Efficient order for the remaining gates

First repair/review that bounded installation recipe and regenerate the chosen
artifacts. Then a **three-platform Release installed journey** using the existing
installation workflow can answer the largest missing clean-provisioning questions
before the final comprehensive run. Reuse the current G27/G28 examples for
new/check/run/build, library/add, stdlib, installed LSP and selected relocation;
no new application framework or fresh broad regression campaign is needed.

Use the existing macOS focused workflow for later platform gaps when its test
selection covers the claimed features; do not mistake its older six tests for all
later functionality. For the final candidate retain the existing seven jobs,
fatal sanitizers, original native/HTTP/stack/editor assertions and receipt capture.
G27/G28 local replay is useful triage but does not replace the final required gate.
Do not duplicate all tests in every installation job or rerun every historical
matrix before that final gate. No CI was dispatched in G29.

Preserve the historical optimizer incidents, external raylib/Gallium leak
classification, Windows symlink prerequisite, executable-cleanup uncertainty and
G28 Windows async-host boundary. No fresh evidence here repairs or worsens them.
Ordinary async CLI hosting, object-model expansion, Windows cache optimization,
new package systems and universal static distribution are not prerequisites
invented by this audit.

## Ranked findings and one coherent handoff

Severity below concerns the stated release surface; it is not an assertion of a
new compiler correctness defect. No P0 failure was reproduced.

| Rank | Classification | Evidence / smallest action | Acceptance and stop condition |
| --- | --- | --- | --- |
| 1 | P1 for a public binary toolchain: payload/ABI and trust qualification | Current developer compiler and D5 app fail Ubuntu loader checks. Public VOIDUP still uses engineering trust. Select surface/floor; use existing floor-build and producer/manager mechanisms | Exact candidate works with declared prerequisites on selected fresh hosts, has reviewed payload/rights and authentic delivery. Do not publish developer binaries or test-trust catalogs; source-only scope does not require inventing public VOIDUP first |
| 2 | P1 for rerunning installation qualification: missing host-provider preparation | Source and actual CMake negative controls above; existing helper already solves provisioning | Add existing host setup/verify step, then pass current reviewed-SHA installation jobs. Stop on identity, provenance or install failure; never skip validation |
| 3 | P2 editor release artifact: stale build and missing VOID license | Missing Package and old timeout; ordinary regeneration still omits VOID license; task-only inclusion passes | Regenerate from current locked inputs, include the authoritative VOID license (task-only control passed), record source/artifact identity and qualify the installed workbench. No TypeScript redesign justified |
| 4 | P2 onboarding/release-policy drift | Installed discovery contradicts README override instruction; Bible/current register retain old implementation status; installed notice wrongly makes FILE200 mandatory | Reconcile only conflicting current-status instructions and release claims to accepted contracts. Preserve dated history. Root README changes require Boss's separate authorization |
| 5 | Conditional qualification/owner obligations | macOS later feature scope, independent installs, GNU/platform closure and selected public channel | Bound supported claims and finish their actual acceptance; do not label every unimplemented convenience a blocker |

**One proposed Keyboard assignment: prepare and qualify the first declared RC
surface through existing installation tooling.** Scope is the host-preparation
recipe correction, narrowly targeted documentation reconciliation, current VSIX
preparation including the tested license inclusion and exact payload/evidence inventory. Reuse existing SDK/provider,
CMake install, VOIDUP and D5 code. Linux/Windows disposable installation and the
regenerated editor artifact already support that approach. Full cold native
qualification and publication remain explicitly outside this research result.

Boss's genuine decisions are the initial source-versus-binary/VOIDUP surface,
supported OS/feature claims, release identity/channel/trust ownership, and the
applicable payload/provenance disposition. Golden Wrench reviews the narrow recipe
and document changes; Boss authorizes hosted gates and release. No new language
engineering milestone is justified by these results.

## Evidence, inventory and storage

The only repository deliverable is this report. Exact commands, original setup
failures, negative portability results, hashes, packaging receipts and source/input
inventories are retained under the existing verified-ZIP policy. No production,
root README, tests, workflow, SDK, active build or owner editor profile was edited.
The original local VSIX remains unchanged. No staging, commits, branches, pushes,
publication or hosted CI occurred. Verified archive/cleanup details follow below.


Archive: `/home/shmellyorc/Documents/Void.Language-archives/g29-release-readiness-2026-11-13.zip`.
SHA-256: `7ea397bdcb5161a1144ac8aa3809753888fa97f93eaea018498914710f06ed4d`.
**114 selected files**, 244,281 uncompressed bytes, **78,447 ZIP bytes**,
DEFLATE level 9. CRC, full extraction and all restored SHA-256 checks passed;
selected originals were rehashed before cleanup. The manifest contains absolute
source paths and member/source hashes. The existing external archive index records
verification and cleanup. Initial verified members were retained when the final
license-inclusion control was added; no original failure receipt was discarded.

Rebuildable installed prefixes, native images, copied inputs and disposable VSIXs
were removed; the exact source revision, recipes, original/corrected receipts and
hashes remain. The existing Node dependency directory was only linked into the
scratch packaging tree; deleting that symlink did not delete its target.

| Removed task-owned root | Logical bytes | Allocated bytes |
| --- | ---: | ---: |
| Linux `/tmp/void-g29r-dyzvdf_g` | 155,905,204 | 156,753,920 |
| Host transport `VM/g29r-20261113` | 9,541,896 | 9,670,656 |
| Windows `C:\Users\a\VoidG29Research-20261113` | 105,065,289 | Not measured |

Free space before/after final Linux scratch cleanup: **16,354,906,112 / 16,511,664,128 bytes**.
Host filesystem: **362,213,265,408 / 362,213,318,656 bytes**.
Windows C: **8,702,709,760 / 8,808,124,416 bytes**.
These are end-of-task inventories, not peak usage. Filesystem-wide changes include
concurrent activity and are not exact attribution of reclaimed bytes. The task
roots and location pointer are gone. Existing container image, toolchains, native
builds, owner editor state and G26–G28 records were preserved.

Final HEAD remains `9330e338d3473665dc1b8b9496a75f67d1c13423`. Tracked and staged
diffs are empty. The sole untracked repository deliverable is
`docs/release/G29_FIRST_RELEASE_READINESS.md`. External changes are the verified
archive and its entry in the existing archive index.

## Smallest remaining set before a practical RC

1. **Boss declares the RC surface, identity and supported claims.** Source-install
   plus matching generic VSIX is the smallest existing route; public binary/VOIDUP
   distribution adds its explicit trust, ABI and payload obligations.
2. **Make the bounded release-preparation corrections:** host-provider setup in
   the install-smoke recipe, exact VOID license inclusion/current VSIX regeneration,
   and the identified contradictory onboarding/release text. Reuse existing tools.
3. **Qualify the exact candidate and prerequisites:** focused current native
   installation journeys, the missing claimed platform scope, installed editor
   behavior, and the existing Boss-controlled final comprehensive gate. Build any
   binary payload on its declared floor; keep all original assertions intact.
4. **Close the applicable payload/rights and delivery decisions.** Do not call
   engineering test trust production trust, or a same-host relocation portable
   deployment. Source-only publication does not require inventing public VOIDUP.

The engineering delta is small. Most remaining work is qualification and reviewed
release preparation. **Stop for Golden Wrench review; Boss retains release, Git
and hosted-CI authority.**
