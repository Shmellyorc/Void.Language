# W1 focused Windows native project qualification

The original W1 workflow was prepared from
`5ab57744e0589cbe34b4bc3ad6b12d8f735e204b`. The repair baseline is
`1e4c91ebdff41a672b9830acefe50880f3133cc5`. Hosted Windows x64 Debug and
Release remain **UNQUALIFIED** pending execution on the reviewed repair.

## Existing support and boundaries

| Area | Integrated Windows implementation and W1 coverage |
| --- | --- |
| Discovery and manifests | The existing compiler selects a manifest application and discovers its entry. The smoke checks a project and output with spaces and Unicode and exercises discovery from the project directory. `project_manifest` checks the shared parser and the Windows DLL/COFF catalog fields. |
| D1 locks | Windows uses wide-path file handles, rejects reparse points and verifies file identities before publication. Lock publication uses the existing Windows writer and replacement path. `project_dependencies` checks the Linux reference lock SHA-256, read-only verification, refresh, concurrent publication, hard links and junction refusal. |
| D2 packages | The verified graph feeds source selection and typed IR. `project_packages` checks locked package compilation, normal and relocated inputs, JIT, independent AOT and compiler-owned stdio project tooling. Its maintained assertions require `voidls`. |
| D3 Git | Local acquisition verifies Git objects and immutable pins before publication. The HTTPS path uses isolated Git configuration, a Windows Job Object, verified TLS and a smart Git server. The file and HTTPS tests cover immutable acquisition, snapshot and lock checks, and offline compilation. The HTTPS test uses a disposable loopback CA and checks that unsolicited packs cause zero requests to the second origin. |
| D4 managed native | The x64 MSVC catalog target checks PE DLLs, COFF link inputs, notices and lock v3 identities. `project_managed_native` builds the existing Windows C FFI fixture, checks catalog and lock behavior before native initialization, and exercises managed JIT, AOT, relocation, missing or changed payload refusal and resource release. AOT uses the existing Windows linker and Runtime admission paths. |

[D1](DEPENDENCY_D1_CLOSEOUT.md) · [D2](DEPENDENCY_D2_CLOSEOUT.md) · [D3 file](DEPENDENCY_D3_CLOSEOUT.md) · [D3 HTTPS](DEPENDENCY_D3_HTTPS_CLOSEOUT.md) · [D4](DEPENDENCY_D4_CLOSEOUT.md) · [Existing Windows C FFI](SLICE7_G4_CONVENTIONAL_FFI.md)

The D1 fixture uses a junction and hard links on Windows. POSIX symlink and
Linux interruption probes keep their existing platform gates. Earlier guest
login failures occurred before the D1–D4 native tests. The historical Windows
leaf-symlink privilege failure was a fixture setup result. W1 uses the
unchanged fixture and does not change privilege or OS policy.
[D1 guest record](DEPENDENCY_D1_CLOSEOUT.md) · [Windows alias controls](SLICE7_G4_WINDOWS_IMAGE_SELECTION.md)

`project_managed_native_resolution` and `project_managed_native_callbacks`
currently register only on Linux. The Windows managed fixture already
covers selection and resource cleanup. W1 leaves managed retained callbacks,
managed editor navigation and graphical raylib outside its Windows
qualification. The W2 implementation record tracks Windows D5 packaging.
[D4](DEPENDENCY_D4_CLOSEOUT.md) · [W2](W2_WINDOWS_D5_IMPLEMENTATION.md)

## Workflow

[W1 workflow](../../.github/workflows/ci-windows-dependencies.yml) runs
only on `workflow_dispatch`, using GitHub-hosted `windows-2022` x64 with
separate Debug and Release jobs. At dispatch, the selected ref, checked-out
HEAD and required 40-character `reviewed_sha`
must identify the same commit. The job verifies the Windows x64 runner
and x64 MSVC SDK environment before bootstrap. [GitHub runner labels](https://github.com/actions/runner-images#available-images)

The workflow uses Visual Studio's x64 SDK environment, Python 3.12 x64 and
Ninja. `tools/ci/llvm_sdk.py` verifies the complete LLVM/Clang/LLD 23.1.4
SDK and prepares its pinned static zlib, zstd and LibXml2 development
libraries. It can reuse the existing verified SDK cache. The profile-matched
Schannel HTTP closure and host libarchive/libsodium closure use
`tools/setup_http_deps.py` and `tools/ci/voidup_bootstrap.py`. The latter
verifies both provider inventories and the shared zlib identity. The
compiler uses pinned `clang-cl`, Windows SDK/DIA libraries and the existing
`MultiThreaded` (`/MT`) CRT setting in both profiles. AOT uses the SDK's
`clang.exe` driver with its existing LLD and static CRT settings. The
runner supplies Git for Windows and OpenSSL for the disposable Git HTTPS
fixture. [Windows runner software inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)

The build requests `voidc`, `voidls`, `void_project_manifest`,
`void_project_packages_ir` and `void_windows_native_staging`. Transitive
dependencies include the frontend, LLVM backend, Runtime, native fixture,
pinned providers and `void_native_inventory` for AOT. Bootstrap and build
scratch are separate from the native-link root. After the build, the CI #95
helper receives the absent root path over UTF-8 stdin and creates it
exclusively. The workflow exports TMP/TEMP for smoke and CTest only after
the helper verifies the private DACL, production root admission, effective
owner access and restricted-token mutation denial. [CI #95 private-root evidence](../stage9/CI93_WINDOWS_TEMP_ROOT_FIXTURE.md)

The smoke compiles and runs a manifest project in JIT and as an independently
linked executable. It verifies x64 PE headers and checks direct CRT imports on
both executables.

CTest uses this exact selector in each profile.

```text
^(project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native)$
```

The job checks the generated CTest JSON for all six test names and active
commands before running them. Missing Git registration, disabled tests
and skip properties fail the gate. The six tests still run if the smoke
fails after a successful build and private-root verification. Test failure
output is capped at 16 KiB per test. A failure step prints up to 8 KiB from
each of a bounded set of CTest, CMake and provider logs. Builds and fixtures
live in the disposable runner workspace. The workflow uploads no artifacts.

## Original workflow preflight

- Actionlint 1.7.12 passed. PowerShell 7.6.9 parsed all nine run steps, and
  Python parsed their three embedded blocks.
- A fresh Linux Debug configure with the pinned LLVM 23.1.4 SDK selected
  exactly the six intended CTests. Ninja target queries verified the four
  requested build targets and their dependencies.
- A registration-only CMake probe evaluated the maintained test
  declarations with Windows x64 conditions for Debug and Release. Both
  selected the same six names, and both Git tests disappeared when Git
  was absent. The Windows managed test receives the empty editor argument.
- Local controls confirmed the workflow rejects short and wrong SHAs and
  a Linux runner. Synthetic CTest controls verified exact selection,
  refusal before execution on missing or disabled tests, and propagation
  of a test failure.

Those preflight checks exercised workflow logic and test registration.

```sh
actionlint .github/workflows/ci-windows-dependencies.yml
ctest --test-dir "<configured-build>" --show-only=json-v1 \
  -R '^(project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native)$'
```

## Original hosted failure and corrections

[Run 42950139792](https://github.com/Shmellyorc/Void.Language/actions/runs/42950139792)
built successfully in Debug and Release. In both profiles smoke reached AOT
and failed at root admission. Each profile passed `project_manifest` and
`project_dependencies` (**2/6**). The research record retains the individual
failures and tool boundary details. [W1 prerequisite research](W1_WINDOWS_REPAIR_REVIEW.md)

| Failure | Correction |
| --- | --- |
| Smoke AOT, `project_packages` and `project_managed_native` rejected the ordinary PowerShell-created TMP/TEMP root with `Native temporary root permits untrusted modification`. W1 did not log the rejected root's SDDL. | Use the existing CI #95 private-root helper and keep bootstrap scratch separate. Creation at an existing name fails. Production ACL admission and trusted principals retain their existing requirements. |
| The four-fetch control in `project_git_dependencies` failed with Windows error 32. Reopening the `acquire-*` stage for directory reads with no delete sharing conflicts with DELETE access on its owning handle. | While filling and verifying the owned stage, reuse its live reservation at that component after checking the directory identity at its name. Pin the other components with the existing directory-read access and sharing flags. Use the same authority for recursive snapshot reads and managed native payload verification. Existing destinations are independently admitted on collision. |
| In `project_git_https`, OpenSSL rejected the absolute Unicode config path before CA creation. The guest then exposed a second Unicode boundary when Schannel opened the CA bundle. | Pass ASCII relative config, key, request and certificate names from the same Unicode working directory. Python loads the generated certificate and key at their Unicode paths. On Windows, the isolated Git environment selects `LC_ALL=C.UTF-8` so Schannel can open the original Unicode CA pathname. The TLS, hostname and Git security assertions remain in the existing fixture. |

## Local qualification

10 November 2026, America/Vancouver. Linux x86-64 Debug and Release each
passed all six W1 CTests and a Unicode discovery/check/JIT/AOT smoke using
LLVM/Clang/LLD 23.1.4. The initial Linux runs each passed **5/6**. The AOT
negative link assertion in `project_managed_native` expected LLD's
`undefined symbol: void_d4_link_value` wording, and Clang had invoked GNU
`ld.bfd`. Configuring the existing `VOID_LINKER` setting explicitly for
the pinned LLD gave **6/6** in each profile with the original assertions.
Actionlint 1.7.12 passed, the guest's PowerShell 5.1.19041.6456 parsed all
ten run blocks, and Python parsed all four embedded scripts.

```sh
sdk="$PWD/.toolchains/llvm-23.1.4"
export PATH="$sdk/bin:$PWD/.toolchains/ninja-1.13.2:$PATH"
export TMPDIR="$scratch/test-tmp" PYTHONDONTWRITEBYTECODE=1
mkdir -m 700 "$TMPDIR"
for profile in Debug Release; do
  build="$scratch/linux-$profile"
  cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$profile" \
    -DBUILD_TESTING=ON -DVOID_SANITIZERS=OFF -DVOID_LLVM_RESEARCH=OFF \
    -DCMAKE_C_COMPILER="$sdk/bin/clang" -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
    -DLLVM_DIR="$sdk/lib/cmake/llvm" -DVOID_CLANG="$sdk/bin/clang" \
    -DVOID_LINKER="$sdk/bin/ld.lld" -DVOID_GIT_EXECUTABLE=/usr/bin/git
  cmake --build "$build" --target voidc voidls void_project_manifest void_project_packages_ir
  ctest --test-dir "$build" --no-tests=error --output-on-failure \
    -R '^(project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native)$'
done
```

Set `scratch` to an owned temporary directory outside the checkout.
`VOID_HOST_DEPS_ROOT` and the Linux HTTP prefix used the verified existing
Debug and Release dependency installations.

### Windows guest controls

The x64 Windows 10 guest (build 19045, Python 3.14.8) reproduced the two
Win32 opens on the same directory. The owning `DELETE | FILE_LIST_DIRECTORY |
FILE_READ_ATTRIBUTES | READ_CONTROL` open succeeded. The directory-read
reopen with `FILE_SHARE_READ | FILE_SHARE_WRITE` returned
`ERROR_SHARING_VIOLATION (32)`. The competing rename returned 32 while
the owning handle was held, then moved the same file ID after release.
The Debug baseline replay also failed at the four-fetch assertion
(`project_git_dependencies`, **0/1**, CTest exit 8). A scratch-only
diagnostic identified the failed component as
`project/.void/git-deps/acquire-0cadf9a5fb8374eefa16b3a4aceaf38d`.

The guest selected `C:\Program Files\Git\ucrt64\bin\openssl.exe`,
SHA-256 `77a399eadd9c2eb5fd70012cd1953b86ebca8b9fda1bdf193932dc8830985874`.
Its actual `version -a` reported OpenSSL 3.5.9, 29 September 2026,
platform `mingw64`. The absolute Unicode `tls.cnf` command returned 1,
with the snow character rendered as `?` at `BIO_new_file`/`fopen`.
The relative `tls.cnf` command returned 0 with the same Unicode cwd.
The corrected fixture's three commands and Python's
`SSLContext.load_cert_chain` passed there. The original hosted binary was
`C:\Program Files\OpenSSL\bin\openssl.EXE`. Its actual version output and
binary SHA-256 remain unavailable for the failed run.

Git for Windows 2.56.0.windows.3 with Schannel refused the Unicode CA file
under `LC_ALL=C` whether it received the path through `GIT_SSL_CAINFO` or
`http.sslCAInfo`. An ASCII-path copy of the same CA reached the loopback
server under the C locale. With `LC_ALL=C.UTF-8`, the original Unicode CA
also reached the server and received its deliberate HTTP 404. Both locales
rejected the certificate with the CA omitted (`SEC_E_UNTRUSTED_ROOT`).
The fixture keeps the CA, project and repositories at their Unicode paths
and supplies a hostile caller locale to verify the isolated child setting.
The failed hosted run reported Git for Windows 2.56.0.windows.4 and stopped
earlier at OpenSSL.

With the original Unicode CA opened, the guest's default Git backend
reported `schannel: the revocation status is unknown` for the original
leaf, which had no CRL distribution point. A native loopback control
reached HTTP with a valid signed CRL and rejected the same kind of leaf
when its serial was revoked. The Windows fixture now gives its leaf a
loopback distribution point and serves that CA's signed CRL. Its CRL
listener is separate from the two TLS origins used for the request-count
assertions. Git continues to check the certificate, hostname and revocation.
[Windows CRL retrieval behavior](https://learn.microsoft.com/en-us/windows/win32/seccrypto/certificate-revocation-list-semantics)

The first guest Debug candidate passed **2/6** and reached smoke AOT. The
retained SDK's junction was correctly refused by native artifact admission.
Using the physical SDK path passed smoke and **5/6**. HTTPS first failed
opening the Unicode CA, then failed with the missing revocation status
after the locale correction. The original test limits and assertions
apply to all replays.

The CRL-enabled replay reached the offline AOT phase. The HTTPS fixture
was still overriding TMP/TEMP with its ordinary transport scratch, which
inherited a DACL that production refused. Offline AOT now inherits the
admitted runner root. Git acquisition and transport cleanup checks keep
their separate scratch.

### Final focused results

| Platform | Profile | Exact W1 CTests | Discovery, check, JIT and AOT |
| --- | --- | --- | --- |
| Linux x86-64 | Debug | **6/6** in 60.90 s | Passed |
| Linux x86-64 | Release | **6/6** in 19.37 s | Passed |
| Windows 10 x64 guest | Debug | **6/6** in 118.33 s | Passed with x64 PE and direct CRT import checks |
| Windows 10 x64 guest | Release | **6/6** in 70.02 s | Passed with x64 PE and direct CRT import checks |

The guest used LLVM/Clang/LLD 23.1.4, the verified Windows provider inputs,
Schannel and the existing `/MT` build. In each profile the private-root
helper reported `private_root_verified=1 owner_access=1 restricted_mutation=0`.
A duplicate create returned error 183 and preserved the existing marker.
The separate publication probe read a nested staged file while two
competing rename attempts returned error 32. Authorized publication and
ordinary rename after release preserved the original directory file ID.
The existing collision winner was verified by its identity and payload.
A deliberate verifier failure removed the owned stage and left the
existing winner in place.

The six maintained tests exercised concurrent Git acquisition, immutable
snapshots and locks, offline compilation, and the HTTPS assertions including
zero unsolicited requests to the second TLS origin.

To replay on a native Windows x64 host, configure both profiles with the
[W1 configure and build targets](../../.github/workflows/ci-windows-dependencies.yml),
then run its private-root, smoke and CTest blocks in order. The guest replay
selected the physical path of the retained SDK and reused the two profile
build directories. Its local wrappers select those build directories explicitly.
The publication control adds `w1_publication_probe.cpp` to a disposable source
copy as a target linked against `void_core`. Run it once per profile with an
empty, owned probe directory under the verified workspace.

## Accepted hosted qualification

GitHub Actions run #42986407961 passed on Windows Server 2022 x64 at
`01c465d444060fae1e2806a21673db5ccacfcc8f`.
[Accepted W1 run](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961)

| Profile | Focused CTests | Native discovery, check, JIT and AOT smoke | Private-root verification |
| --- | --- | --- | --- |
| Debug | 6/6 | Passed | Passed |
| Release | 6/6 | Passed | Passed |

[Debug job](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961/job/130408526896) · [Release job](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961/job/130408526334)

## Evidence and cleanup

The original run ZIP and four failure excerpts, selected guest and Linux
receipts, and reproduction sources are in
`../Void.Language-archives/w1-windows-repair-42950139792.zip` relative to the
checkout root. The ZIP is **3,007,742 bytes**, with SHA-256
`bf579d78048dcd9b3867dc22dfafefafd8dd085ff5931c2befe65f9589852ef6`.
The archive passed CRC testing. A disposable extraction matched all
47 member SHA-256 values in `_ARCHIVE_MANIFEST.json`. The original hosted
ZIP is retained verbatim inside it. The complete guest OpenSSL
`version -a` output is in `windows/probe.log`.
[Archive and restoration policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md#restoring-historical-records)

The guest cleanup detached the task-owned junctions and verified that the
shared SDK and provider targets still existed. It then removed the guest
workspace, which held 1,324,695,633 logical bytes. Free space on its C drive
rose from 12,495,503,360 to 13,838,815,232 bytes.

After archive verification, the original untracked log copies, Linux build
and test scratch, and VirtualBox exchange directory were removed. Available
`/tmp` space increased by 850,018,304 bytes and available checkout-filesystem
space increased by 12,156,928 bytes during the removal. The external archive
index records the ZIP checksum and restoration check. Maintained source,
tests, fixtures and pinned dependencies were retained.

The local repair handoff recorded an empty Git index on `main` at the repair
baseline. The original hosted failure and local repair evidence remain above.
