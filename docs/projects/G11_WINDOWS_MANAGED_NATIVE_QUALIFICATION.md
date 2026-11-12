# G11 Windows managed-native provider and packaging qualification

Baseline `ceefd3f9a75257756cb8ab476868731e4359dcca`. The guest reported
**Windows 10 Home x64, 10.0.19045.6466**. Both profiles used LLVM,
Clang and LLD **23.1.4**, MSVC 14.44.35207 and Windows SDK 10.0.19041.0.
The baseline export and subsequent source overlay were hash-verified.
The Windows build used the matching approved host and Schannel provider
prefixes, a configure-only delegation to the byte-identical attested host
verifier, and owner-only NTFS temporary roots. [Attested verifier setup](SLICE7_FINAL_CLOSEOUT.md#qualification-scope-and-preserved-platform-boundaries)

## Provider and previous evidence

| Lane | Applicable earlier evidence |
| --- | --- |
| Real conventional raylib | S7 qualified the Windows Win32 software build with normal/O2 JIT, relocated AOT and visible input, drawing and closure. The official GLFW DLL and the WGL 1.1 trial failed to create a window on this guest. [S7 results](SLICE7_RAYLIB_INTEGRATION.md#qualification) |
| D4 managed catalog | Linux qualified real raylib. The native Windows suite qualified catalog selection, JIT and AOT with the independent G4 C fixture. [D4 closeout](DEPENDENCY_D4_CLOSEOUT.md#slice-4-managed-raylib-60) |
| D5 packaging | Linux D5 included real raylib. W2 Windows Server 2022 accepted both profiles with a synthetic managed payload and separate package layout, relocation and refusal tests. [D5 closeout](DEPENDENCY_D5_CLOSEOUT.md) · [W2 acceptance](W2_WINDOWS_D5_IMPLEMENTATION.md#hosted-acceptance) |
| Retained callbacks | G5 previously qualified Windows conventional retained callbacks and Linux managed callbacks. The raylib trace shim uses the provider's global `TraceLogCallback` only within synchronous startup, with an ABI-specific C adapter for `va_list`. [G5 contract and qualification](SLICE7_G5_CALLBACKS.md) |

The exact retained Win32 software artifacts were available in
`%USERPROFILE%\VoidS7-Raylib-Compatibility`. The vendor source is raylib
tag 6.0, at release commit `dbc56a8`. The retained build uses
`PLATFORM_DESKTOP_WIN32` and `GRAPHICS_API_OPENGL_SOFTWARE` with Clang
and LLD 23.1.4. [Upstream release](https://github.com/raysan5/raylib/releases/tag/6.0) · [Upstream Makefile](https://github.com/raysan5/raylib/blob/6.0/src/Makefile) · [Retained native recipe and hashes](SLICE7_RAYLIB_INTEGRATION.md#raylib-60-and-runtime-dependencies)

| Input | Bytes | SHA-256 |
| --- | ---: | --- |
| `raylib-6.0-source.tar.gz` | 52,545,040 | `2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b` |
| `raylib-6.0/src/raylib.h` | 134,582 | `047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b` |
| `raylib-6.0/LICENSE` | 877 | `882a5a819cf562aa3583aae3af3f2211dda15c63de9fc8cc4b399a2f9e78d799` |
| `build-win32-software-linked/raylib.dll` | 1,826,304 | `3bf4db7515f019512026f72baacb8e6c9f98f32786593571e98d28df9dafa824` |
| `build-win32-software-linked/raylib.lib` | 188,462 | `59778ec137ffebe87a6c099944330f20b7eecdf1d49a5a93de7000a8992339df` |

The existing `prepare.py --managed-catalog` generates the checked shim
and descriptor against those inputs. The catalog selects
`org.voidlang.raylib`, exact version `6.0.0`, ABI 3, revision
`raylib-6.0-shim-v1` and target `x86_64-pc-windows-msvc`.
Its provider and adapter are both `void_raylib.dll`. D4 binds their
descriptor and the exact file inventory to the lock. It checks AMD64 PE
DLL format and validates the selected link archive at link time. Catalog
selection and source checking do not load a provider. The independent
link consumes the matching `raylib.lib`. D5 ships the two DLLs and
the raylib notice. [Catalog preparation](../../examples/raylib/prepare.py) · [D4 selection](../../src/frontend/project_native.cpp)

The Runtime binds the checked dispatch to its owning generation and
holds the library while resources and callbacks can use it. Windows
loads from the approved DLL directory and System32 through the existing
loader. The tested image import tables and observed mapped module paths
define the scope of the dependency evidence. Windows retains its
loaded-module and redirection rules. [Runtime loader](../../src/runtime/c_dynamic.cpp) · [Microsoft DLL search rules](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)

## Real Windows application results

Both compiler builds ran the actual gallery with its managed lock,
with normal and O2 source profiles. The Win32 witness observed the
Unicode window title, captured a 640 × 360 client area, and measured
the pointer, animation and keyboard-controlled box moving. The same
window handled `WM_CLOSE`. Each successful window run invoked the
scoped trace callback once, exited zero and emitted one texture and
window release marker. The JIT normal and AOT O2 runs exercised implicit
release. The JIT O2 and AOT normal runs exercised explicit release.

| Windows compiler | Source profile | D4 admission and JIT | Independent relocated AOT | Verified relocated D5 |
| --- | --- | --- | --- | --- |
| Debug | Normal | Pass, 57 / 29 | Pass, 53 / 28 | Pass, 54 / 30 |
| Debug | O2 | Pass, 53 / 27 | Pass, 53 / 28 | Pass, 54 / 28 |
| Release | Normal | Pass, 57 / 28 | Pass, 57 / 30 | Pass, 58 / 29 |
| Release | O2 | Pass, 54 / 28 | Pass, 53 / 27 | Pass, 54 / 29 |

The pairs are rendered frames and keyboard-input frames. Across the
12 windows the pointer moved 334.5–335 pixels, the animation moved
27–30.5 pixels and the keyboard-controlled box moved 105–115 pixels.
Every final capture measured the expected RGB `39, 47, 65` for the
background produced by raylib's by-value `ColorBrightness` call.
The headless checks also refused the embedded-NUL title.

All independent and D5 executables were PE32+ AMD64. Their direct and
delay import audit found `ADVAPI32.dll`, `bcrypt.dll` and `KERNEL32.dll`.
The shim imported `KERNEL32.dll` and the approved `raylib.dll`.
Raylib imported `GDI32.dll`, `KERNEL32.dll`, `OPENGL32.dll`,
`USER32.dll` and `WINMM.dll`. In the visible processes, both actual
module paths pointed to the selected catalog for JIT or the relocated
`native/` directory for AOT.

The D5 outputs had exact 28-file inventories, including the raylib
license, Runtime notices, application metadata and both approved
DLLs. The outputs were verified before and after relocation. The
independent and D5 images each executed the scoped trace in the same
child that demonstrated access denial to the original source,
catalog, installed compiler and CMake build. A separate visible
execution used the relocated paths while the original project,
catalog and compiler installation names were unavailable.

| Windows compiler and source profile | D5 `app.exe` bytes | D5 `app.exe` SHA-256 | Independently linked executable SHA-256 |
| --- | ---: | --- | --- |
| Debug normal | 4,140,544 | `a91a1f988b9af46002899235b24652ae2a9302e37a048621039b18fe6cefb8d3` | `be879a9bf8158d34bf22d6f32f6c6e34d1a3d68c358dc3772bce84a90bad9113` |
| Debug O2 | 4,028,416 | `118af37a2567511bdae1a62703624840ac895549b6747b4809b5dd5024fa1e8d` | `d8e2c30e16bde354bf853b5f67d97180a73878f9ffca6de8d30ca04f2f215d20` |
| Release normal | 1,619,456 | `94ec19590b6a66a3d41effa260c39da03ddf87c814476c390d8f667f84b00602` | `02e4a9b9e8b01860f8623d54bd1f9ffaf1648522eb837397e827dcac3b41e2ce` |
| Release O2 | 1,562,624 | `33f6660b0bcfd26da728bb35f4bc52471d14f8efb13dc10d22521f41a73b47f4` | `2eb8eab85c23d894d4bfb761447dfcc523db3b18144c64deafaf0dd0577ed34d` |

The two D4 catalogs used the same 1,046-byte descriptor, SHA-256
`55fc257ce900ef6708f7d4830ecdf036ad1d2430c5edfb37cc36c473f965cb88`.
The Runtime payload hashes for `raylib.dll` and the raylib license match
the vendor table above in all four packages and independent outputs.
The generated shim is 129,024 bytes. Its SHA-256 is
`bcdb60d48b6170351fe238b3bbb75bcf26291ebb870feb923d1028ca74141a3f` in
the Debug compiler's catalog and
`44debe2f018383b4f7a250dd8c28c38c6ef8975d6beb4a73f9c4c6f33f2eeb75` in
the Release compiler's catalog. The package locks respectively identify
`a39842b8a26ad3f56d40bb70663cecc9335ae7da748b0d5a3462f79a45915cc9`
and `5888de4ec8ce45d731c454b0c6c03dab969147d837d84df69e2c29a85596a5c3`.

### Authorization and integrity controls

| Control | Result |
| --- | --- |
| Catalog ID, exact version, ABI and target | Wrong ID, different exact version, ABI mismatch, unsupported ABI 4 and nonmatching target refused. An x86 machine field in a rehashed PE DLL was refused as the wrong target or image type. |
| DLL and import-library identity | Missing and modified provider or vendor DLL refused. A replaced import library failed identity verification. An intentionally rehashed non-COFF archive passed metadata selection and failed native link input admission before output publication. |
| Catalog and explicit authorization | The absent catalog failed selection. `run`, `emit`, `build` and `package` required `--allow-native`. An extra catalog DLL and a duplicate filename across catalog roles were refused. |
| Provider loading | A disposable copy of the admitted shim referenced a nonexistent import. With the test hash bound in the catalog and lock, loading returned Windows error 126 before the trace callback. |
| Package integrity and authority | Modified or missing native payloads were refused before provider execution. Adjacent, current-directory and PATH DLLs supplied no fallback for a missing selected file. Package verification rejected added, altered and missing files. |
| Publication and aliases | Both variants of publication to an existing Windows destination failed and preserved the complete inventory. Failure with an altered approved DLL left the fresh output absent. Verification refused an NTFS junction. The maintained W2 tests also rejected hard links and nonportable output names. |
| Ownership and outstanding work | The maintained G3 tests checked invalid resource identity and terminal ownership. G5 checked retained shutdown, retirement and protection of the provider mapping while its generation was leased. The real gallery and the separate retained C fixture supply independent evidence. |

## Reproduced package defect and correction

The baseline Windows Debug and Release `project_application_package`,
`project_package_native` and `cli_install` reached package verification
and returned `Invalid application package inventory header.` Both baseline
compilers admitted real raylib and passed normal/O2 JIT trace execution.
Both independently reproduced the same D5 failure on that real catalog
and left the package destination absent. A generated ordinary project
reproduced the failure on Linux. The G03 change
`6ef25f38644b326a398bb334ba0da4954d57ca4c` advanced the effective
profile key to `project-profile-v2:`. The D5 inventory writer used that
key and its verifier still required the earlier prefix. This affected
ordinary packages as well as native ones.

The verifier accepts both recorded profile-key versions in inventory
revision 1. The maintained D5 test checks the current v2 output,
verifies a package with the historical v1 key and rejects a v3 key.
The file rows, hashes, metadata, path checks and publication policy
continue through the existing D5 code. [Verifier](../../src/native_build.cpp) · [Regression](../../tests/project/application_packages.py)

## Windows maintained tests

Both baseline configurations passed **16/20**. The three package failures
are described above. `cli_contract` reached teardown and failed with
`WinError 5` deleting its disposable `broken installation\voidc.exe`.
Both corrected selections passed that unchanged test. The cause of the
earlier delete denial remains unconfirmed.

| Windows 10 compiler profile | Baseline | Corrected |
| --- | --- | --- |
| Debug | 16/20, 155.82 s | **23/23**, 247.77 s |
| Release | 16/20, 66.34 s | **23/23**, 93.58 s |

All 23 final tests had active commands and no skip properties. The two
profile tests and the Windows managed-callback registration account for
the larger selection. Existing timeouts are unchanged.

```powershell
$selection = '^(project_managed_native|project_package_native|project_application_package|ffi_g4_conventional|ffi_g5_callbacks|project_managed_native_callbacks|runtime_callbacks|ffi_g3_jit_ir|ffi_g3_aot_normal|ffi_g3_aot_optimized|runtime_coordination|native_workers|project_manifest|project_dependencies|project_git_dependencies|project_profiles|project_native_debug_info|application_workflow_stage9|native_output_paths_ci88|cli_install|cli_install_resources|cli_contract|target_triple)$'
ctest --test-dir $build -R $selection --show-only=json-v1
ctest --test-dir $build -j 2 --output-on-failure -R $selection
```

### Retained callback lifetime

The separate G5 C provider ran through conventional and managed Windows
admission in both compiler configurations. Each test ran normal/O2 JIT
and relocated normal/O2 AOT. The controls verify registration, dispatch
on the caller and a real Win32 worker, and preservation of the original
generation after compatible replacement. The C fixture exercises a
dispatch reserved before Runtime admission during shutdown and a foreign
callback already admitted before explicit unregister. The Runtime
retains its quiescence, self-drain, GC and failure-ordering checks.

The reload harness observes the exact provider and adapter DLL mappings
without taking a loader reference. It checks those mappings while the
old generation is leased and after an unregister transport refusal. The
live Runtime still owns the binding after its drain. Both DLLs disappear
when the Runtime and code-generation owners go out of scope. The G5
fixture also rejects post-retirement resource use and proves a callback
failure survives refused release and subsequent cleanup. Raylib's
global trace callback is scoped and has no provider unregister/drain
barrier for a retained VOID callback. Retained raylib callbacks stay
on **HOLD**. [Maintained process test](../../tests/ffi/callback_process.py) · [Reload and unload assertions](../../tests/ffi/callback_reload.cpp) · [G5 contract](SLICE7_G5_CALLBACKS.md)

### Qualification harness observations

- A disposable catalog control expected a duplicate-specific diagnostic.
  Cross-role duplication actually returned `VOID5006` with
  `Invalid or ambiguous managed native artifact catalog.` The replay
  asserts that existing diagnostic.
- The first Win32 screen captures returned 34 and zero pink pixels in
  Debug and Release. The Debug bitmap showed the gallery blended with
  console content. The corrected witness brings the owned window to a
  fully visible foreground position and waits for a settled capture
  with the same pixel and input assertions.
- The first independent-link driver treated the manifest version line
  as the Clang path. The next driver passed the Runtime and fixture
  archive paths relative to its unrelated working directory, so Clang
  refused the two missing inputs during `-###`. The corrected driver
  consumes the header and both tool paths and resolves the two relative
  archives against the installed compiler directory.

## Linux shared-source regressions

PikaOS 4 x64 reused the pinned LLVM/Clang/LLD 23.1.4 builds. The
strict profile is RelWithDebInfo with `VOID_SANITIZERS=ON`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
Both managed and conventional G5 and the Runtime callback suite used
the existing test assertions and timeouts. The maintained Linux raylib
`--package` replay exercised the real approved vendor build headlessly,
including isolated relocation with Bubblewrap and missing/modified
payload refusal. [Linux raylib driver](../../tests/project/managed_raylib.py)

| Profile | Package and project CTests | Callback CTests | Real raylib D5 headless |
| --- | --- | --- | --- |
| Debug | **11/11**, 90.86 s | **3/3**, 26.06 s | Pass |
| Release | **11/11**, 22.36 s | **3/3**, 6.83 s | Pass |
| Strict ASan/UBSan with leak detection | **11/11**, 118.21 s | **3/3**, 28.36 s | Pass |

```sh
package='project_application_package|project_package_native|project_managed_native|project_managed_native_resolution|project_dependencies|project_profiles|project_native_debug_info|cli_install|cli_install_resources|cli_contract|application_workflow_stage9'
callbacks='project_managed_native_callbacks|ffi_g5_callbacks|runtime_callbacks'
ctest --test-dir BUILD -j 2 --output-on-failure -R "^($package)$"
ctest --test-dir BUILD -j 2 --output-on-failure -R "^($callbacks)$"
python3 -B tests/project/managed_raylib.py BUILD/voidc PINNED_CLANG VERIFIED_LINUX_RAYLIB OFF --package
```

Use `ON` for the strict replay. The strict run retains leak detection.

## Replay

Use the verified local inputs above from an x64 MSVC developer shell.
Build `voidc`, `voidls`, `void_windows_native_staging`, `void_g5_reload`,
`void_runtime_callbacks`, `void_g3_aot_normal`, `void_g3_aot_optimized`,
`void_runtime_tests`, `void_native_workers_tests`, `void_project_manifest`
and `void_target_triple`. Use the staging probe to create and verify
owner-only `TMP` and `TEMP`. Install the compiler component to a
disposable directory. Copy the gallery project and pass the DLL, header,
license and import library to `prepare.py --managed-catalog`. Replace
the two manual native tables in the copy with the requirement below.
The visible qualification copy changed the title literal to
`VOID Raylib Gallery 雪` so the Win32 window title observes the actual
UTF-8 byte-span path.

```toml
[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
```

Set `VOID_NATIVE_CATALOG` to the approved absolute catalog path and use
the same installed compiler for each command. Repeat the two profile
values with each compiler configuration.

```powershell
& $voidc resolve --project $gallery
& $voidc resolve --project $gallery --locked
& $voidc check --project $gallery
& $voidc run --project $gallery --allow-native --no-cache --profile $profile -- --trace-only
& $voidc emit --project $gallery --allow-native --profile $profile -o $object
& $voidc package --project $gallery --allow-native --profile $profile -o $package
& $voidc package --verify $package
```

For the independent link, invoke the pinned Clang with `--driver-mode=g++`,
`--target=x86_64-pc-windows-msvc`, `-B<physical SDK bin>`, `-fuse-ld=lld`,
the emitted object, the catalog's `raylib.lib` and the Runtime, fixture
and remaining arguments from the installed `void-native.txt`. Expand
`@VOID_NATIVE_DIR@` to the installed compiler's bin directory and resolve
the Runtime and fixture archives relative to that same directory. Verify
the `-###` plan names the pinned `lld-link.exe` before executing it.
Stage the same three catalog payload files beside that independent image
in `native/`. Use the existing Windows isolation helper to test denial
of source, catalog, installed compiler and original build reads in the
same child that launches the relocated image. The visible witness uses
the S7 Win32/GDI capture and process module techniques. [Isolation helper](../../tools/test_platform.py) · [Original visible witness](evidence/slice7-g5/windows/qualify-windows.py)

The disposable in-process raylib witness linked `void_llvm` and
`void_runtime`. It used `analyze_manifest_project` with native
authorization and `Jit::compile` in both modes, passed
`--trace-only` through `Runtime::application_arguments`, then called
`Runtime::close`. It checked both exact mapped DLL paths without
acquiring a loader reference. It observed the mappings while the
Runtime and generation were alive and their absence after scope exit.

## Remaining platform scope

macOS managed-native execution and D5 remain unsupported. A separate
architecture review should establish Mach-O identity and dependent
library resolution, application and bundle layout, signing and loader
authority before implementation. The accepted focused macOS
application/editor run remains at `31608c2dd2ca1ae20e3bdd021d0c36581a01dabc`.
Signed App Sandbox is unverified. The original seven-job integration
gate remains **OPEN / DEFERRED BY BOSS DECISION**. [Reconciliation](PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md#focused-native-macos-qualification-accepted)

The G08 optimization and Windows persistent-cache holds, the G06
Windows symlink privilege limitation and G10's deferred deadline
composition and queues remain recorded. Submit the narrow verifier
repair and Windows evidence to Golden Wrench for independent review.
G11 remains **IN PROGRESS** in the milestone register.
[Milestone register](../roadmap/MILESTONE_REGISTER.md)

## Cleanup and source inventory

The task-owned Windows source, builds, installed copies, packages,
diagnostics and captures were removed. Available space on Windows rose
from 11,195,695,104 to 14,996,393,984 bytes. The Linux scratch and
VirtualBox transfer were removed and the task-local server was stopped.
Linux available space rose from 16,382,230,528 to 16,587,931,648 bytes on
temporary storage and from 367,671,205,888 to 367,671,582,720 bytes on the
share filesystem. The existing Linux builds and approved SDK and vendor
inputs were reused. The Markdown record retains the evidence and replay
instructions. [Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md)

| Role | Exact changed files |
| --- | --- |
| D5 verifier | `src/native_build.cpp` |
| Maintained qualification | `CMakeLists.txt`, `tests/project/application_packages.py`, `tests/ffi/callback_process.py`, `tests/ffi/callback_reload.cpp` |
| Roadmap | `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `docs/roadmap/MILESTONE_REGISTER.md` |
| Native and project records | `docs/NATIVE_LIBRARIES.md`, `docs/projects/DEPENDENCY_D4_CLOSEOUT.md`, `docs/projects/DEPENDENCY_D5_CLOSEOUT.md`, `docs/projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md`, `docs/projects/SLICE7_G5_CALLBACKS.md`, `docs/projects/W2_WINDOWS_D5_IMPLEMENTATION.md` |
| New qualification record | `docs/projects/G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md` |

`main` and local `origin/main` are both
`ceefd3f9a75257756cb8ab476868731e4359dcca`. The index is empty. The
worktree has 13 modified tracked files and the one new Markdown report.
`git diff --check` and changed-document link validation pass.
