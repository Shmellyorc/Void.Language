# G29 — minimal release preparation repair

13 November 2026. Engineering qualification only; no public release approval.
Baseline and final HEAD: `9330e338d3473665dc1b8b9496a75f67d1c13423`.
[G29-R research](G29_FIRST_RELEASE_READINESS.md) was already untracked on entry
and is preserved byte-for-byte. No root README, legal text, compiler, Runtime,
dependency resolver or native trust implementation was changed.

## Corrections and reused machinery

`tools/ci/install_smoke.py` now prepares the matching Release host providers after
HTTP setup/verification and before CMake. It reuses `tools/setup_host_deps.py`
(including its existing native delegate) and `tools/verify_host_deps.py`, exactly
the sequence already used by `tools/install.py`. Both prefixes must start absent;
each failure stops the phase. Platform/architecture/profile arguments are explicit,
the host receipt is retained, CMake receives the same prefix, and source identity
now includes the host recipes/verifier/pins. Existing runner, LLVM version,
provenance, source, native-resource, installation and smoke assertions remain.
The existing workflow's step label/comment now identify both provider families;
no workflow was dispatched. No second provider installer or cache was introduced.

The normal editor build copies the authoritative root `LICENSE` to generated,
ignored `editors/vscode/LICENSE.txt`; `.vscodeignore` includes it. Packaging no
longer skips the license check. There is no independently maintained license copy.
The existing optional `tools/package_tooling.py --vsix` scratch layout now carries
that same root license and repository-relative extension layout, so its build
continues to work. Its obsolete UNLICENSED receipt wording now identifies the
owner-selected MIT license while retaining private-artifact status.

## Focused qualification

All native compiler inputs reused LLVM/Clang/LLD **23.1.4** and the accepted
G27/G28 build environments. No compiler rebuild or unrelated full suite was needed.

| Check | Exact result |
| --- | --- |
| Installation dependency command fixtures | **10/10 PASS**, Linux and native Windows; Linux/Windows/macOS command representations, fresh-prefix refusal, failure propagation and both receipt copies |
| Installed layout fixtures | **15/15 PASS**, including corrupt/missing resources, prefix escapes, link inventory and Windows CRT representations |
| Existing Linux HTTP and host verifiers | **PASS** for Release, Debug and strict, all six verifier invocations |
| Linux provider admission/refusal fixture | **18/18 PASS per profile**: Release, Debug and strict; initial nine plus nine self-consistent forged-receipt controls |
| Real CMake host gates | Missing prefix and invalid receipt each refused with expected **exit 1** and pinned-provider diagnostic |
| Fresh Linux Release provisioning | Existing builder with task-owned `--prefix`/`--build-dir`: **PASS**; **87 upstream sodium targets PASS**, existing policy exceptions unchanged; fresh receipt verifier and **18/18** controls PASS |
| Linux focused CTest | Release **14/14** (24.58 s), Debug **14/14** (95.85 s), strict **14/14** (137.68 s); explicit fatal ASan/UBSan with leaks **14/14 PASS** (124.90 s) |
| Linux installed journey | Maintained `tests/cli/install.py`: **PASS**, both components, relocation, all **29** stdlib sources, optimized/Debug JIT, independent native build/run, resource hashes, LSP, package verification and refusal/output-preservation controls |
| Windows provider verification | HTTP and host: **PASS** in Release and Debug; provider fixture **17/17 per profile**, preserving its existing Windows upstream-test exception |
| Windows installed journey | Maintained `tests/cli/install.py`: **PASS**, including package execution with reads of compiler installation/source denied |
| Windows app/library onboarding | `new`, library `new`, `add ALIAS --path`, check, JIT, native build/run **PASS**; exact native output `42\n` |
| Clean locked npm build | Node **24.19.0**, `npm ci --ignore-scripts`, existing `npm run package`: **PASS**, including prepublish build/typecheck |
| Extension configuration tests | Correctly configured `npm test`: **8/8 PASS** |
| Installed generic VSIX editor host | VS Code **1.141.0**: **38/38 PASS on Linux**, **38/38 PASS on Windows**; isolated user/extension directories |
| Existing optional native tooling package | Linux `tools/package_tooling.py --vsix`: **PASS**, extracted native-server handshake and **13-member** platform VSIX with exact VOID license |
| Documentation | Zero introduced missing local Markdown paths/anchors; current installation/stdlib/package/CLI/performance contracts PASS |

The 14 CTest names are `project_manifest_header`, `project_manifest`,
`project_dependencies`, `project_dependency_explain`, `project_packages`,
`project_managed_native_resolution`, `project_manifest_no_authority`,
`project_manifest_legacy`, `project_manifest_provenance`, `project_creation`,
`project_maintenance`, `project_application`, `cli_contract`, and
`cli_install_resources`. These reuse application/library onboarding and the
existing manifest, source, lock, native-provider and publication-refusal assertions.
The strict tree is RelWithDebInfo with `VOID_SANITIZERS=ON`; the final confirmation
explicitly uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86` with the pinned
symbolizer. The first strict pass did not explicitly set those environment options.

Fresh-prefix installs used copies of the generated CMake install rules, redirecting
only the two install-manifest output destinations. Windows additionally used the
established exact-source shadow technique because its retained guest snapshot is
partial. Source inputs were hashed. Original accepted install scripts were unchanged.
These are native developer-host installation checks, **not** fresh-machine or
end-to-end source-wrapper qualifications. Windows provider checks reused the
established native verifier root, after matching every provider recipe with current
source; its SDK identity and paths were not rewritten for the task shadow.

The VM's historical label is “Windows 11”; the guest actually reports Windows 10
Home x64, build **19045.6466 / 22H2**. The native shell used VS 2022 x64,
MSVC **14.44.35207**, Windows SDK **10.0.19041.0**. No owner editor installation
was modified. The installed editor tests cover real project diagnostics,
navigation, recovery, dirty-source refusal, Check/Build/Run and **Package Project**,
including compiler verification of the generated application package.

## Generic VSIX contents and identity

The regenerated qualification artifact remains at
`editors/vscode/dist/void-language-0.1.0.vsix` (ignored, **160,346 bytes**).
SHA-256: `b77ff85ba81922a3cf223812a173e55c78173332cf90add630fba805d93c8f95`.
The clean scratch candidate used by both native editor tests has identical bytes
for **every member**; ZIP timestamps give it a different outer hash,
`20a910215374436843c1835736546f52c8ca658afc66d60c1625df66372060dd`.

The complete expected member set was verified:

```text
[Content_Types].xml
extension.vsixmanifest
extension/package.json
extension/readme.md
extension/LICENSE.txt
extension/language-configuration.json
extension/syntaxes/void.tmLanguage.json
extension/dist/extension.js
extension/dist/THIRD_PARTY_NOTICES.txt
```

`extension/LICENSE.txt` is byte-exact root MIT text and owner notice: **1,067 bytes**,
SHA-256 `b7c3169df2c29805ee0ca6bc8e935f4cf3daffb0e39c60bde12213940d5f4467`.
VSIX metadata names it as the license asset. Existing runtime third-party notices
are byte-identical to the previous generated notice payload. The manifest and
JavaScript contain `void.packageProject`; bundled preflight timeout is **60,000 ms**
(`6e4` after esbuild). Manifest, README, configuration and grammar match current
source. Generic VSIX contains no native server or SDK; it requires matching
separately installed `voidls`/`voidc`. Its declared minimum VS Code API remains
`^1.141.0`. No new VSCodium or macOS editor qualification is claimed.

## Documentation reconciliation

The extension README now states executable-relative installed stdlib discovery,
explicit absolute override precedence and configured-source fallback. The install
index and Linux instructions include host providers/receipts; the engineering build
guide prepares HTTP/zlib then host providers and describes all native host profiles.
The install index acknowledges Windows x64 as well as Linux D5 packaging.

The Language Bible's current project status, module/dependency descriptions and
CLI references now point to delivered contracts. Historical proposals remain
identified as superseded; current syntax uses `voidc --version`,
`run --project DIR` and `build --project DIR --profile release`. The editor guide
replaces stale UNLICENSED wording with the existing MIT selection and generated
notice behavior. The installed redistribution index retains **20.091 ms** and
**<10 ms** but correctly makes FILE200 advisory under Boss's later directive.
Legal terms and licensing obligations were not changed. Original historical
reports, measurements, incident records and root README remain intact.

## Failures retained and remaining release boundaries

Original failures and corrected receipts are archived. Initial scratch npm tests
omitted `VOIDLS_TEST_SERVER` (6 passed, 2 failed); setting the existing test input
passed 8/8. The first package probe expected literal `60000` instead of emitted
`6e4`. The first Windows provider probe lacked the SDK at its task-shadow path;
the established verifier path passed without relaxing identity. The first Windows
editor run passed all 38 assertions but exited 1 while writing evidence to the
UNC share; writing inside the task root and copying afterward passed. Upstream
editor TextMate/agent/shutdown diagnostics remain in the original logs. An initial
tooling-package notice path and direct guest-Python argv were corrected. Transport
ZIP duplicate-member warnings involved two identical host archives; extracted
input hash checks passed. No native security or product workaround was added.

`npm ci` and its retained audit report **six dependency advisories: four moderate,
two high** in the unchanged lock. They include `vscode-languageclient`, `@vscode/vsce`
and transitive build dependencies. This task did not update dependency versions or
claim npm security clearance. Review the exact candidate before release.

Current developer Linux artifacts retain G29-R's demonstrated newer glibc/libstdc++
requirements and fail Ubuntu 24.04 execution. No ABI requirement, system-library
bundle or compatibility check was changed. Use existing recipes to build any
binary candidate on its declared supported-floor userspace, then qualify **that
exact payload** there. This task supplies no portable Linux binary release claim.

macOS ARM64 retains its dated native evidence, with no fresh G29 Apple host or
later-scope acceptance. A macOS deployment target is not minimum-OS execution
evidence. No Linux ARM64/musl, Intel macOS or Windows ARM64 claim was added.
Public VOIDUP trust, production keys, bootstrap and binary delivery remain outside
this assignment. Boss still chooses the release surface/version/support claims,
requires the selected fresh native source-install/candidate journeys and applicable
macOS evidence, resolves exact-payload rights/ABI/dependency obligations and npm
advisories, and controls the deferred comprehensive project/raylib gate and release.

## Storage, archive and Git disposition

The verified-ZIP mechanism and retention policy were used for selected recipes,
logs, negative controls, hashes, original failures and both exact generic VSIXs.
Archive: `/home/shmellyorc/Documents/Void.Language-archives/g29-minimal-release-preparation-repair-2026-11-13.zip`.
SHA-256: `188d08b39d85f3da4c92f2e9c0393a486e9446ee6941c1b3339c2e1b6710ccee`.
It holds **379 selected files**, **969,714 uncompressed bytes**, **564,481 ZIP bytes**,
DEFLATE level 9. CRC, complete extraction, all restored SHA-256 comparisons and
selected-source rehashes passed before cleanup. Windows also checked the same ZIP
hash/CRC before deleting its task root. `_ARCHIVE_MANIFEST.json` records original
paths and source/member hashes. The existing external archive index and the
adjacent `g29-minimal-release-preparation-repair-2026-11-13-cleanup.json` retain
verification and storage disposition. No earlier archive was replaced.
The regenerated generic VSIX and normal ignored editor build outputs are retained
locally as qualification artifacts. Task provider builds/prefixes, scratch npm
dependencies, native/platform packages and isolated editor profiles are disposable.
Active SDKs, provider prefixes/source inputs and G26–G29 research evidence remain.

| Removed task-owned root | Logical bytes | Allocated bytes |
| --- | ---: | ---: |
| Linux `/tmp/void-g29-repair-p5fhzdn3` | 271,038,325 | 303,276,032 |
| Host transport `/home/shmellyorc/VM/g29-repair-20261113` | 19,610,024 | 19,742,720 |
| Builder diagnostics `docs/voidup/evidence/dependencies-linux-Release-1794613328234796399` | 259,239 | 1,257,472 |
| Windows `C:\Users\a\VoidG29Repair-20261113` | 124,690,913 | Not measured |

Linux free space immediately before/after final cleanup: `/tmp`
**16,206,413,824 / 16,510,111,744 bytes**; host filesystem
**362,240,466,944 / 362,260,660,224 bytes**. Windows C: before/after its cleanup
**8,686,718,976 / 8,812,613,632 bytes**. These are end-of-task measurements,
not peak usage or exact attribution; concurrent filesystem activity is included.
The task roots and scratch pointer are gone. Root license/README and G29 research
hashes matched, as did the indexed G27–G29 evidence archive hashes. No broad
cleanup, active toolchain removal or new cleanup system was introduced.

Exact tracked changed-file inventory:

```text
.github/workflows/ci-install-qualification.yml
docs/EDITOR_INTEGRATION.md
docs/LANGUAGE_BIBLE.md
docs/SOURCE_COMPILER.md
docs/install/README.md
docs/install/linux.md
docs/release/REDISTRIBUTION_NOTICES.md
editors/vscode/.gitignore
editors/vscode/.vscodeignore
editors/vscode/README.md
editors/vscode/build.cjs
editors/vscode/package.json
tests/ci/install_dependencies_test.py
tools/ci/install_smoke.py
tools/package_tooling.py
```

New deliverable: `docs/release/G29_MINIMAL_RELEASE_PREPARATION_REPAIR.md`.
Pre-existing untracked input retained: `docs/release/G29_FIRST_RELEASE_READINESS.md`.
No staging, commit, branch creation, push, publication, public signing or hosted
CI dispatch was performed. Final Git checks are recorded with the storage closeout.

Final `git diff --check`: **PASS**, no output. Staged diff: **empty**. Git status:
**15 modified tracked files** listed above and the **two untracked release documents**
identified above. No generated diagnostic directory remains untracked. HEAD is
unchanged at the baseline. The remaining owner-controlled release requirements
are the candidate/support declarations, fresh native qualification and applicable
payload/trust/rights reviews described above; this repair authorizes no release.
