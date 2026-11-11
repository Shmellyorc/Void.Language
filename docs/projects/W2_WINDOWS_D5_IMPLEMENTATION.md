# W2 native Windows x64 application packaging

Implementation baseline `01c465d444060fae1e2806a21673db5ccacfcc8f` on
`main`. [Research](W2_WINDOWS_D5_RESEARCH.md) · [Linux D5 closeout](DEPENDENCY_D5_CLOSEOUT.md) · [Accepted W1 qualification](WINDOWS_NATIVE_PROJECT_QUALIFICATION.md#accepted-hosted-qualification)

## Windows package contract

```powershell
voidc package --project . --profile release -o "output/My Application"
voidc package --project . --allow-native -o "output/Managed Application"
voidc package --verify "output/My Application"
```

The default destination is `<project>/dist`. Windows emits `app.exe`,
`application.meta`, `void-package.txt`, and `licenses/`. It includes
`assets/` when the manifest declares resources and `native/` when the
selected D4 catalog supplies a managed payload. Inventory revision 1
records `x86_64-pc-windows-msvc`, the profile, the lock identity when
present, the selected-input SHA-256, and sorted relative paths with lengths
and SHA-256. The executable is inventoried independently of the
selected-input digest. Existing file, byte, notice and resource limits
apply.

Windows publishes a completed and verified package exclusively to an
absent destination. An existing destination fails both with and without
`--replace`. With the option present, the diagnostic reads
`Windows application packaging cannot replace an existing destination`.
The existing bytes and directory identity are preserved. The option is
accepted for a fresh Windows destination. Linux retains its same-application
atomic exchange.

## Implementation

- `src/native_build.cpp` shares D5 assembly and verification, applies the
  host-specific executable name and PE/ELF checks, and excludes the selected
  executable row from the input digest. Windows uses the configured Clang
  driver's sibling `lld-link.exe` through an explicit SDK search prefix.
  Both tools must report 23.1.4. The existing verified Runtime, fixture
  and selected link archives feed the native link with the static CRT.
- `src/frontend/project_creation.cpp`, `src/frontend/project_files.hpp` and
  `src/native_windows.hpp` carry the live directory reservation through the
  package callback, checked executable transfer, nested `native/` staging and
  verification. Each borrowed component is checked by volume and file ID
  while its owner holds the original reservation. Ancestor pins retain
  their share masks. The package prepares `native/` and copies the verified
  image directly into the owned stage with exclusive creation. The outer
  handle-based rename publishes both after the nested reservations close.
  A collision or preparation failure cleans the owned stage and preserves
  an existing destination.
- `src/frontend/project_git.cpp` keeps the existing Git snapshot ancestor
  inspections on ordinary read handles. The snapshot itself uses the
  scoped stage reader.
- `cmake/NativeBuild.cmake` generates Windows build-tree and installed
  notice inventories and embeds the expected Windows notice names and
  hashes from those same CMake inputs. Windows includes VOID/Runtime MIT,
  LLVM, Ryu, date, tzdata, Unicode, libarchive, libsodium, curl, c-ares,
  nghttp2, zlib, libpsl and Public Suffix List attribution for the
  Schannel/WinIDN build. The Linux inventory retains its AWS-LC entries.
- `src/main.cpp` makes bare `voidc` and `voidc --help` display the same help
  and return zero. Help links to the public Wiki and documents the
  Windows destination rule. Malformed invocation, including the removed
  `--app` option, still returns status 2 with its existing diagnostic.
- `CMakeLists.txt`, `tests/project/application_packages.py`,
  `tests/project/managed_native.py`, `tests/project/creation.cpp`,
  `tests/ffi/conventional_dependency.cpp`, `tests/cli/contract.py`,
  `tests/cli/install.py` and `tools/test_platform.py` extend the existing
  focused tests. The Windows fixture inspects direct and delay imports
  with the pinned `llvm-readobj` and records the loaded managed DLL
  locations. The isolation control denies reads of fixture-owned roots
  before the child checks access and launches the packaged image.
- `.github/workflows/ci-windows-packaging.yml` defines the manual Debug and
  Release Server 2022 matrix. `docs/README.md`,
  `docs/projects/DEPENDENCY_D5_CLOSEOUT.md`,
  `docs/projects/WINDOWS_NATIVE_PROJECT_QUALIFICATION.md` and this record
  update the engineering navigation, current platform contract and
  historical W1 closeout.

The Windows verifier checks PE32+ AMD64 executable headers, raw section
bounds and executable entry point using LLVM's COFF reader. It hashes the
same bytes it inspects. Package admission checks exact spellings, physical
collisions, regular files, single links and reparse points. Win32 creation
provides the package stage's private DACL. Verification reads the directory
under its present filesystem identity and preserves its ACL. Linux
retains its mode checks.

Managed payload assembly reuses the D4 lock and exact catalog checks,
including DLL and import-library identities. The package carries the
selected runtime DLLs and notices. Import libraries are link inputs. The
existing Runtime loader uses the executable-relative `native/` directory
and `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` and
`LOAD_LIBRARY_SEARCH_SYSTEM32`. The package uses existing NativeSafe
admission, managed lifecycle, resource ownership, scheduler, GC and ABI.

## Scope of the guarantees

Inventory revision 1 describes ordinary files and their contents. Publisher
authentication, NTFS alternate streams and security descriptors fall outside
that inventory. Verification decodes the sidecar metadata. The Runtime admits
the selected native payload on execution and the operator can run
`package --verify` for the full inventory. Cross-location selected-input
digests and package data identities are tested separately from PE byte
reproducibility. Windows filesystem coverage is bounded to the tested
native environment and its local volume.

## Initial failures and corrections

- The first Windows Debug test run passed four of nine cases. A native
  Clang control reproduced the failed link with both slash styles in an
  absolute linker argument. The MSVC driver prefixed that value with the
  target triple. The SDK bin directory passed via -B with -fuse-ld=lld
  selected the SDK sibling and linked a native control successfully.
  [Pinned Clang MSVC driver](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/clang/lib/Driver/ToolChains/MSVC.cpp)
- A Win32 control reproduced error 32 for both an absolute child
  directory rename and MoveFileExW into the owning stage. The tested
  parent-relative Win32 rename returned 87. The package now creates its
  final child entries exclusively inside the private stage, retaining
  the outer and nested reservations through verification.
  [Win32 rename interface](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)
- Guest setup first rejected the Linux-only VOID_LINKER override, then
  reported a missing tracked curl patch and missing corresponding-source
  archives in the disposable source copy. The retained host receipts bind
  the tool path to their original source tree. The guest reused the verified
  SDK and HTTP inputs and rebuilt two host-provider prefixes in its task
  workspace. The first install tests exposed a guest-only host downloads
  junction that CMake could not install over the existing corresponding-source
  directory. Byte-matched archive copies in the guest corrected that setup.
- The second Windows Debug run passed six of nine cases. Windows absolute
  path resolution erased the trailing period from a destination alias.
  D5 now checks the raw path components before absolute-path resolution.
  The two isolation tests exposed inherited deny ACEs left on descendants
  after the root ACL was restored. An attempted isolation replay with
  traversal bypass disabled passed ordinary execution but blocked the
  existing Runtime's canonical path lookup for a managed package. The
  maintained helper instead holds exclusive read reservations on the
  fixture files and temporarily denies listing at each fixture root.
  It restores the root descriptor through a held WRITE_DAC handle. The
  earlier icacls /restore attempt returned 1300 on the guest's standard
  token.
  [Windows traversal privilege](https://learn.microsoft.com/en-us/windows/win32/secauthz/privilege-constants)
- The first expanded Linux runs each passed eight of nine tests. The new
  extra-file fixture inherited mode 0600 and hit the existing mode check
  before the inventory check. The fixture now sets mode 0644 on Linux so
  it reaches the intended extra-entry rejection.
- The local Windows Git acquisition regression caught the strict stage
  reader being used for two ordinary ancestor inspections. Those
  inspections now use the same ordinary directory admission as the
  unstaged Git path. The stage reservation remains scoped to the staged
  snapshot and its descendants.

## Replay

On Windows use the [W2 workflow's configure and build recipe](../../.github/workflows/ci-windows-packaging.yml)
with the pinned LLVM 23.1.4 SDK, the x64 Visual Studio/Windows SDK
environment and verified Schannel and host-provider prefixes. Set
`TMP` and `TEMP` to a fresh private root created by
`void_windows_native_staging.exe --create-private-root` using the UTF-8
stdin path protocol. Run this selector in each single-configuration build.

```powershell
$tests = 'project_application_package|project_package_native|project_managed_native|project_creation|project_creation_process|project_manifest|cli_install|cli_contract|application_workflow_stage9|project_git_dependencies'
ctest --test-dir $build --no-tests=error --output-on-failure -R "^($tests)$"
```

On Linux, use the existing verified Debug and Release provider prefixes,
an owned build directory, and the pinned compiler and linker. Set `TMPDIR`
to an owned directory with mode 0700 before running the tests.

```sh
sdk="$PWD/.toolchains/llvm-23.1.4"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$profile" \
  -DCMAKE_MAKE_PROGRAM="$PWD/.toolchains/ninja-1.13.2/ninja" \
  -DBUILD_TESTING=ON -DVOID_SANITIZERS=OFF -DVOID_LLVM_RESEARCH=OFF \
  -DCMAKE_C_COMPILER="$sdk/bin/clang" -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
  -DLLVM_DIR="$sdk/lib/cmake/llvm" -DVOID_CLANG="$sdk/bin/clang" \
  -DVOID_LINKER="$sdk/bin/ld.lld" -DVOID_GIT_EXECUTABLE=/usr/bin/git
cmake --build "$build" --parallel 3 --target voidc voidls void_project_creation void_project_manifest
tests='project_application_package|project_package_native|project_managed_native|project_creation|project_creation_process|project_manifest|cli_install|cli_contract|application_workflow_stage9|project_git_dependencies'
ctest --test-dir "$build" -j2 --no-tests=error --output-on-failure -R "^($tests)$"
```

## Hosted workflow

The manual Windows Server 2022 x64 workflow is ready for source review.
Its reviewed-SHA gate checks the requested SHA, workflow SHA and actual
checkout. The two jobs verify the pinned SDK and provider closures,
use the W1 private-root helper and require all ten selected tests,
including `project_git_dependencies`, to have active commands and no CTest
skip properties. At the initial local handoff, Actionlint 1.7.12 passed,
the native PowerShell 5.1 parser accepted all eight run blocks, and the
two embedded Python scripts compiled. The initial hosted results and
the correction are recorded in the [hosted failure repair](#hosted-failure-repair).

The final qualification preparation added the Git acquisition regression
to the mandatory hosted selection. Actionlint 1.7.12 passed and both
embedded Python blocks compiled. A temporary CTest fixture exercised the
workflow gate with ten active commands. The gate rejected missing Git
registration, a missing executable, each disabled or skip property,
duplicate names and selections of nine or eleven. The fixture was
removed. The existing native and Linux results below are the execution
record for this handoff.

## Local qualification

11 November 2026, America/Vancouver. The Windows guest ran native x64
Windows 10 build 19045.6466 with MSVC tools 14.44.35207, Windows SDK
10.0.19041.0, CMake 4.4.3 and Python 3.14.8. Linux used CMake 4.3.4 and
Python 3.14.8. Both platforms used LLVM/Clang/LLD 23.1.4 and the
profile-matched pinned providers.

| Platform | Profile | W2 selection | Git acquisition regression |
| --- | --- | --- | --- |
| Windows 10 x64 guest | Debug | 9/9 in 173.82 s | 1/1 in 18.17 s |
| Windows 10 x64 guest | Release | 9/9 in 76.13 s | 1/1 in 26.18 s |
| Linux x86-64 GNU | Debug | 10/10 in 82.76 s including Git | Passed |
| Linux x86-64 GNU | Release | 10/10 in 22.19 s including Git | Passed |

Each Windows profile verified a 30-file Schannel HTTP prefix and the
matching patched host-provider prefix. The private-root helper returned
`private_root_verified=1 owner_access=1 restricted_mutation=0`.
The exact nine-command gate confirmed that all W2 tests had active
commands and no skip properties.

Both Windows profiles built, verified and ran ordinary PE32+ AMD64
packages, including a path longer than 260 UTF-16 code units with spaces,
Unicode and a literal percent sign. The fixture matched metadata, assets,
notices and sorted inventory records. It rejected missing and altered
files, extra members, reparse and hard-link aliases, malformed PE
headers, the wrong machine, an invalid entry point and a DLL posing
as the executable. Two concurrent fresh publishers produced one
verified winner. The maintained native control exercised active stage
rename exclusion, rollback, a late destination collision and Windows
replacement refusal while the prior executable was locked.

The pinned `llvm-readobj` reported ADVAPI32, KERNEL32, SHELL32 and ole32
as the ordinary executable's direct imports. Managed executables imported
ADVAPI32 and KERNEL32, with bcrypt also present in the Debug compiler
profile. The fixture DLLs imported KERNEL32 and their expected edges
`d4_adapter.dll` → `d4_provider.dll` → `d4_dep.dll`. The import check
covered direct and delay imports and found no dynamic CRT in those images.
For both application profiles, the live module paths matched all three
DLLs under the relocated `native/` directory despite CWD and PATH decoys.
The source and original catalog contents were unreadable during launch.
The provider initialized once, released the expected resources, and
refused missing or changed payloads before initialization.

The installed-compiler tests used a relocated CMake installation, verified
its required notice bytes, and refused packaging after a notice was
removed or altered. Each relocated package ran while reads of its actual
compiler installation and original source were denied. Bare and
explicit help matched byte for byte, and malformed CLI cases retained
status 2. Linux additionally exercised the existing atomic exchange and
ran the relocated package with Bubblewrap hiding the checkout, compiler
build and source trees.

A separate native guest replay built and verified each ordinary package
twice. Both profiles produced different PE inventory rows with equal
selected-input digests. The replay then moved the task-owned checkout,
both compiler build trees and the original project out of their original
paths. It verified denied reads of the parked roots and their contents
before launching both relocated executables from an unrelated working
directory. Each returned zero and printed the expected output with
`PATH` limited to System32. The replay restored the guest roots after
both runs.

## Evidence and cleanup

The Linux builds reused the verified SDK and provider prefixes. The guest
used the existing SDK and HTTP prefixes and provisioned the checkout-bound
host receipts inside the task workspace. Generated applications, objects,
builds, install prefixes, test receipts and probe logs stayed in the
disposable Linux scratch, Windows guest workspace or host exchange.

| Storage | Free before work | Free before cleanup | Free after cleanup |
| --- | ---: | ---: | ---: |
| Host checkout volume | 371,419,336,704 bytes | 370,680,184,832 bytes | 371,394,101,248 bytes |
| Guest C: volume | 13,684,310,016 bytes | 12,272,902,144 bytes | 13,563,379,712 bytes |

The Linux build and test workspace used 699,273,216 allocated bytes.
Its test temp root was empty before removal. The host exchange used
15,056,896 allocated bytes. The guest workspace measured 1,270,769,140
logical bytes. Recovery of ACLs left by the failed isolation probe used
the guest's elevated backup and restore privileges within that disposable
tree. Cleanup detached the task-owned junctions, verified the shared SDK
and provider inputs, and removed all three workspaces. Final host
`/tmp` free space was 16,491,376,640 bytes. The Markdown record retains
the evidence. The task created no retained raw evidence archive.

## Git handoff

`main` and local `origin/main` both identify
`01c465d444060fae1e2806a21673db5ccacfcc8f`. The worktree contains 18
modified tracked paths, the new manual workflow, this new implementation
record and the pre-existing untracked research record. The index is empty
and `git diff --check` passes. The change is ready for Golden Wrench's
independent source review.

## Hosted failure repair

The initial hosted W2 run #43004582839 at
`fa8dac51e940c1cd3eaf69cbfadb1d3c80423748` built the compiler and pinned
dependencies in both Windows Server 2022 x64 profiles. Each passed nine of
ten CTests. `application_workflow_stage9` failed after 103.13 s in Debug
and 95.83 s in Release. Both jobs recorded the 300-second CTest timeout,
successful private-root and ACL controls, and the expected directory
rename refusal with Windows error 32. At the long-root control,
`workflow.py:168` waited 90 seconds for `voidc build` and reported
`subprocess.TimeoutExpired`.
[Run](https://github.com/Shmellyorc/Void.Language/actions/runs/43004582839) ·
[Debug job](https://github.com/Shmellyorc/Void.Language/actions/runs/43004582839/job/130468778140) ·
[Release job](https://github.com/Shmellyorc/Void.Language/actions/runs/43004582839/job/130468778561)

The authenticated GitHub Actions log archive recorded a 56-UTF-16-unit
project path under `D:\a\_temp\w2\t`. The fixture adds six 36-unit
components to reach a 272-unit `TMP` and `TEMP`. The preceding local
qualification used 256-unit and 258-unit paths in Debug and Release.
W1 at `01c465d444060fae1e2806a21673db5ccacfcc8f` and the hosted W2
baseline use the same `GetTempPathW` call in `temporary_root()`, and
the Stage 9 fixture is byte-identical. The accepted W1 workflow selected
six project-dependency cases and did not exercise this Stage 9 control.

A bounded Win32 probe on the native Windows guest showed that
`GetTempPathW` returned with a raw 260-unit value and blocked with
261- and 272-unit values. The process remained live until terminated.
At 272 units the call blocked with output capacities of 240, 261 and
32,768. The newer `GetTempPath2W` also blocked in the same probe.
`GetEnvironmentVariableW(name, nullptr, 0)` returned 273 immediately
for the 272-unit input. The probe reproduced the problem when an
oversize `TMP`, `TEMP` or `USERPROFILE` supplied the selected
value. A short `TMP` took precedence over a long `TEMP`. A defined
empty `TMP` reported a one-unit buffer requirement and was selected
by the original API. A short relative value resolved from a deep
working directory returned promptly from the original API.
[Win32 temp selection](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw) ·
[Win32 environment size query](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablew)

The W1 function was also compiled as a standalone native control with
Clang/LLD 23.1.4, `/MT` and the repository's long-path-aware manifest.
It returned at 260 units and remained live at 272 until the probe
terminated it. The corrected function returned status 1 and the
`too long` diagnostic at 272. An initial control without the manifest
returned a 10-unit fallback at 272. The long-path-aware manifest was
needed to reproduce the compiler's behavior.

The correction in `src/native_windows.hpp` checks the first defined
candidate in the existing Windows precedence order before calling
`GetTempPathW`. A value above `MAX_PATH` produces the existing
`Windows temporary root is too long for native linking.` diagnostic.
The original resolver handles in-range input and the system fallback.
The 240-unit linker-path budget, exact-path and reparse checks, ACL
admission, owning handles, rename exclusion and cleanup remain in their
existing paths. The Stage 9 assertion, 90-second subprocess timeout,
300-second CTest timeout and ten-test manual hosted workflow retain
their original settings.

### Native diagnosis and reproduction

The Windows 10 x64 guest (19045.6466) used Clang/LLD 23.1.4, MSVC
14.44.35207 and Windows SDK 10.0.19041.0. The hosted jobs used Windows SDK
10.0.26100.0. The guest reused the verified LLVM and Schannel inputs and
rebuilt the checkout-bound host receipts in the disposable source tree.

The original Debug and Release compilers reproduced the hang with the
hosted component spellings and 272 UTF-16 units. The replay mapped only
task-owned directories to `Q:` and `R:`, preserving the drive-letter
path shape. The private-root helper admitted each long directory. Win32
volume and file IDs matched the corresponding physical paths. Before
termination, both compiler processes were still live with an output-side
`.void-build-*` stage present. Their process snapshots had no child
processes. The long root stayed empty and the previous executable kept
its bytes and identity.

On guest volume 3322181814, the baseline Debug and Release long-root
file IDs were 3659174698206615 and 3659174698209160. The corresponding
output file IDs were 4222124651627925 and 3659174698209162 before and
after the bounded reproductions.

An instrumented Debug copy flushed a marker immediately before
`GetTempPathW` and another after return. The 272-unit replay emitted
only the first marker. This places the block in the Win32 resolver,
before the linker-root path inspection and handle acquisition, LLVM
emission, and Clang or LLD execution.

| Native compiler profile | Original 272-unit command | Corrected 272-unit command |
| --- | --- | --- |
| Debug | Live at 5.091 s, terminated by probe | Status 1 with `too long` in 0.471 s |
| Release | Live at 5.073 s, terminated by probe | Status 1 with `too long` in 0.187 s |

Both corrected profiles also rejected an overlong selected `TMP`,
`TEMP` or `USERPROFILE` and a 256-unit root that reached the existing
linker-path budget. A short `TMP` with a long `TEMP` still linked a
native application that returned 7. All rejected commands preserved the
prior output bytes and file ID and cleaned their owned output stages.
The mapped drives and their disposable fixtures were removed after
the replays.

A disposable launcher initially sent a literal `\n` to the private-root
helper. Fixing its stdin line ending allowed the CTests to start on the
already-built corrected binaries.

### Replay the affected control

Configure the two single-configuration Windows builds with the W2 workflow
settings and build `voidc` and `void_windows_native_staging`. Use the
private-root helper to set `TMP` and `TEMP` for the test runner as in
the workflow. A private parent whose path is 15 or 51 UTF-16 units long
makes the unchanged Stage 9 fixture reach a 272-unit root.

```powershell
ctest --test-dir $build --no-tests=error --verbose -R '^application_workflow_stage9$'
ctest --test-dir $build --no-tests=error --output-on-failure -R '^(project_application_package|project_package_native|project_managed_native)$'
```

For the standalone replay, create the fixture's Unicode, space, hash
and percent path components under the private root. Build `return 7`
to the existing `output # % é.exe` destination with a short root.
Use the helper to create a 272-unit directory, point both `TMP` and
`TEMP` at it, and repeat the same `voidc build <source> -o <output>`.
The corrected command must return 1 with `too long`, preserve the
output bytes and file identity, and leave the long root empty.

### Focused results and review boundary

| Windows 10 x64 profile | Unchanged Stage 9 at 272 units | Adjacent W1/W2 CTests |
| --- | --- | --- |
| Debug | 1/1 in 41.67 s, 125 workflow checks | 3/3 in 184.56 s |
| Release | 1/1 in 21.51 s, 125 workflow checks | 3/3 in 58.11 s |

The Stage 9 logs confirm the same 272-unit root in both profiles.
The adjacent tests were `project_managed_native`,
`project_application_package` and `project_package_native`. The
production edit is Windows-only. The prior Linux evidence remains
the Linux execution record. The corrected hosted Windows Server
2022 x64 matrix awaits a manual replay after independent review.

### Repair cleanup and Git status

The guest workspace contained 853,797,891 logical bytes before removal.
Guest free space rose from 12,679,270,400 to 13,552,365,568 bytes during
cleanup. The task detached the three owned junctions and verified that
the retained SDK and provider inputs were still present.

The host log scratch and guest exchange used 2,908,160 and 14,630,912
allocated bytes respectively. Both were removed. Host free space
measured 371,366,756,352 bytes immediately before host cleanup and
371,366,903,808 bytes afterward. The Markdown record retains the findings,
including the original hosted failure and the bounded reproductions.
The temporary log download, diagnostic source, binaries, objects, test
applications and builds were removed.

The repair changes only `src/native_windows.hpp` and this record.
`main` and local `origin/main` remain at
`fa8dac51e940c1cd3eaf69cbfadb1d3c80423748`. The index is empty and
`git diff --check` passes. The Stage 9 fixture and manual W2 workflow
match the baseline byte for byte. Ready for independent Golden Wrench
source review.
