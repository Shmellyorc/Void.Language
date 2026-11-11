# W1 focused Windows native project qualification

Baseline `5ab57744e0589cbe34b4bc3ad6b12d8f735e204b`. The workflow is prepared
for native execution. W1 Windows x64 Debug and Release remain **UNQUALIFIED**
until the hosted jobs run the reviewed commit.

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
qualification. D5 `package` is implemented for Linux x86-64 GNU. Windows
packaging remains **UNQUALIFIED**. [D4](DEPENDENCY_D4_CLOSEOUT.md) · [D5](DEPENDENCY_D5_CLOSEOUT.md)

## Workflow

[W1 workflow](../../.github/workflows/ci-windows-dependencies.yml) runs
only on `workflow_dispatch`, using GitHub-hosted `windows-2022` x64 with
separate Debug and Release jobs. At the later authorized dispatch, the
selected ref, checked-out HEAD and required 40-character `reviewed_sha`
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

The build requests exactly `voidc`, `voidls`, `void_project_manifest` and
`void_project_packages_ir`. The latter two support the parser and D2 typed
IR tests. Transitive dependencies include the frontend, LLVM backend,
Runtime, native fixture, pinned providers and `void_native_inventory` for
AOT. The smoke compiles and runs a manifest project in JIT and as an
independently linked executable. It verifies x64 PE headers and checks
direct CRT imports on both executables.

CTest uses this exact selector in each profile.

```text
^(project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native)$
```

The job checks the generated CTest JSON for all six test names and active
commands before running them. Missing Git registration, disabled tests
and skip properties fail the gate. The six tests still run if the smoke
fails after a successful build. Test failure output is capped at 16 KiB
per test. A failure step prints up to 8 KiB from each of a bounded set of
CTest, CMake and provider logs. Builds and fixtures live in the disposable
runner workspace. The workflow uploads no artifacts.

## Local preflight and remaining qualification

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

Local checks exercised workflow logic and registration. Native Windows
compiler builds, PE smoke checks, JIT/AOT execution and the six maintained
tests still need to pass in both hosted profiles. In particular, the first
run must confirm Git-for-Windows TLS acquisition and zero unsolicited
pack requests, the D1 reference lock digest and managed Windows DLL
execution and release. The running historical VirtualBox guest was unused.

```sh
actionlint .github/workflows/ci-windows-dependencies.yml
ctest --test-dir "<configured-build>" --show-only=json-v1 \
  -R '^(project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native)$'
```

## Changed files

- `.github/workflows/ci-windows-dependencies.yml`
- `docs/projects/WINDOWS_NATIVE_PROJECT_QUALIFICATION.md`

Both paths are untracked at handoff. The index is empty and HEAD is the D5
baseline. Production source, maintained fixtures and existing workflows
retain their baseline contents.
