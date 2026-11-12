# D4 slices 1–4 implementation report

Baseline `8b4b5e0df7485f59228fb4242794b971fab10e9d` on `main`.
The first two slices cover the manifest and catalog model, artifact selection
and lock verification. Slice 3 reviews the existing integration, records the
two targeted corrections and qualifies managed execution on Linux. Slice 4
qualifies the existing raylib 6.0 gallery through the managed path.
The final local qualification covers managed editor queries and retained
callback lifetime through the same locked project graph.
[Dependency design](PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md)

## Manifest and approved catalog

```toml
[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
```

The application can declare one managed provider. The alias uses a compiler
identifier and the version is an exact SemVer, including build metadata.
ABI revisions 1 through 3 use the existing descriptor revisions. Library
manifests cannot declare native requirements. The parser rejects a managed
requirement combined with manual `[native.linux]` or `[native.windows]`
tables. Those manual tables retain their existing behavior when used alone.

Set `VOID_NATIVE_CATALOG` to an operator-approved absolute directory.
The directory contains one `void.native.toml` and its flat file inventory.
Catalog format 1 has one `[native]` table with provider ID, exact version,
ABI, target triple, build revision, descriptor, provider and adapter names,
and the descriptor identity. `[native.runtime]` records the provider,
adapter and declared bundled shared libraries by their loader filenames.
`[native.link]` records any supplied link inputs and may be empty.
`[native.notices]` contains at least one redistribution notice. File
identities use `<decimal-byte-count>:<lowercase-sha256>`. The model admits
32 KiB of catalog text, 24 claimed files including the descriptor, and
128 MiB of claimed contents in aggregate. It rejects duplicate names
under ASCII case fold, nonportable filenames and incomplete identities.
[Model and parser](../../include/void/project.hpp)

## Selection and lock behavior

Selection matches provider ID, SemVer and ABI independently and requires
the current target lane. The supported catalog targets are
`x86_64-unknown-linux-gnu` and `x86_64-pc-windows-msvc`. It checks the
exact directory inventory, reads each named file through the existing
project input reader and verifies its length and SHA-256. It checks
runtime image headers for the target architecture and format. Linux
accepts x86-64 little-endian ELF `ET_DYN` with System V or GNU OS ABI.
Windows checks x64 PE DLL headers. The existing metadata-only descriptor
adapter checks provider, target and ABI and supplies the validated
descriptor. Selection returns the verified artifact record to the project
resolver. No provider library is loaded during selection.
[Selection](../../src/frontend/project_native.cpp)

The operator's chosen catalog is the approval boundary. Hashes bind
selection to its declared bytes. Catalogs must list the bundled runtime
dependencies and required notices. Header inspection and file verification
supply no evidence about the OS loader's eventual transitive binding.
Link inputs are hashed as catalog files. This slice exercises resolution
without linking an application to them.

```sh
export VOID_NATIVE_CATALOG=/absolute/path/to/approved-catalog
voidc resolve --project .
voidc resolve --project . --locked
# After an intentional change to the approved selection
voidc resolve --project . --refresh
```

With a managed requirement, the existing lock writer emits format and
resolver revision 3. The root fingerprint includes the managed declaration
and the source graph inputs. Source rows retain their separate IDs and
content digests. The `[native]` table and its role tables record the
selected target, provider revision, ABI, descriptor and exact artifact
identities. The lock contains no catalog path. Catalog row order and the
physical location of identical inputs do not change the lock bytes.

A verified repeat leaves the lock in place. `--locked` compares the current
verified graph against the canonical lock and performs read-only
verification. A differing valid lock needs an explicit `--refresh`.
Invalid selections and malformed locks fail before publication. Source-only
and dependency-free projects keep the existing v1/v2 behavior. Explicit Git
acquisition resolves its source graph without consulting the managed catalog.
[Resolver](../../src/frontend/project_resolution.cpp)

## Local qualification

Slice 1 passed the three manifest tests in Linux Release, Debug and fatal
ASan/UBSan with leak detection. The syscall-denial case exercised both
parsers. ASan found a reference through a temporary TOML iterator in the
first pass. Keeping the iterator alive through validation corrected it.

The Slice 2 process fixture exercises valid selection, missing and
mismatched provider metadata, ambiguous records, missing files, bad sizes
and digests, wrong ELF machine and OS ABI, and mismatched descriptors.
Its fixture images trap if initialized. It checks repeat resolution,
read-only verification, refresh, relocated catalog determinism and
v1/v2/v3 source and managed lock combinations. It also checks that manual
declarations and source-only resolution ignore the catalog setting.
[Selection fixture](../../tests/project/managed_native_resolution.py)

| Linux x86-64 profile | Slice 2 focused selection |
| --- | --- |
| Release | 8/8 passed |
| Debug | 8/8 passed |
| RelWithDebInfo with fatal ASan/UBSan and leak detection | 8/8 passed |

The original sanitizer run completed with 8/8. Both operator replays also
passed 8/8. This selection includes `project_managed_native_resolution` and
the D1–D3 project controls. Managed application execution is qualified separately.

An early Slice 2 build used a private LLVM ELF accessor. The selection
check now reads the public ELF header.

Use the existing disposable Linux configure recipe with LLVM/Clang/LLD
23.1.4 and profile-matched host and HTTP prefixes. Use
`RelWithDebInfo` and `VOID_SANITIZERS=ON` for strict.
[Linux configure recipe](DEPENDENCY_D2_CLOSEOUT.md#linux-reproduction)

```sh
mkdir -p "$scratch/tmp"
export TMPDIR="$scratch/tmp"
export PYTHONDONTWRITEBYTECODE=1 VOID_TEST_STDLIB="$PWD/stdlib"
# Strict profile
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake --build "$build" --parallel 3 --target voidc voidls void_project_packages_ir \
  void_project_header void_project_manifest
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_manifest(_header|_no_authority)?|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_managed_native_resolution)$'
```

## Slice 3 integration review

| Area | Implementation present |
| --- | --- |
| Project frontend | The existing locked resolver supplies `CPrimitiveAdmission` to project analysis. `check` and editor analysis verify the approved catalog and lock and use the generated binding module without loading native code. `run`, `build` and `emit` require `--allow-native`. The lock identity participates in project and build provenance. Manual declarations retain their separate path. |
| Typed IR and Runtime | The existing descriptor and FFI contract carry a managed payload list with filenames, lengths and SHA-256. Runtime checks every listed runtime file and notice before opening the provider and adapter. The existing LLVM JIT uses the approved catalog root. Existing AOT startup reads the embedded contract and uses the executable-relative `native/` directory. The typed ABI, ownership and generation rules still use the existing FFI and Runtime. |
| Native build | `build` uses the pinned compiler and Runtime inputs, rechecks the payload, and publishes a complete sibling `native/` directory before publishing the executable. An existing directory is reusable only if its contents match. `emit` writes the object with the contract and leaves payload staging to `build`. |
| Raylib | The existing preparation helper can produce an approved catalog from the raylib 6.0 inputs, the generated checked shim and the license. The checked-in sample manifest still uses manual native declarations. Managed qualification needs a disposable copy with the managed declaration and its own lock. |

The integration uses the accepted lock and existing frontend, typed IR, LLVM
and Runtime interfaces. Conventional loading still delegates actual transitive
binding to the OS after verifying the declared files. The checks do not bind
the verified bytes to an open loader handle. Linux G4-L remains a separate
path.
[Admission](../../src/ir/c_primitive.cpp) · [Runtime](../../src/runtime/c_dynamic.cpp) · [Native build](../../src/native_build.cpp)

## Slice 3 targeted completion

The source review found that a fresh `-o <dir>/native` passed the initial
output check, then collided with publication of the sibling payload. Managed
builds now reserve the `native` leaf and its case variants before creating
the output directory or staging files. Ordinary builds can still use that
name. The payload and executable retain separate publication steps, so a
later executable publication failure can still leave a complete payload.
[Output checks and publication](../../src/native_build.cpp)

The review also found that the application linker skipped the verified
`[native.link]` entries. The verified lock plan now carries those identities
to `build`. It rereads the files against their locked lengths and SHA-256,
then stages the verified bytes in the private link workspace. The pinned
linker receives those files in filename order alongside its existing inputs.
The AOT inputs support x86-64 relocatable objects, self-contained archives
and Windows x64 COFF import libraries. Thin archives, shared images and
objects with ELF linker option or dependent-library sections or COFF
`.drectve` are rejected at build time. The lock schema, metadata selection
and JIT path are unchanged. Link inputs stay
in the link workspace. Shared libraries continue through the existing FFI
loader from the executable's relative `native/` directory, alongside notices.
[Verified lock](../../src/frontend/project_resolution.cpp) · [Link inputs](../../src/frontend/project_native.cpp) · [Native build](../../src/native_build.cpp)

The maintained execution fixture verifies the reserved name on fresh and
populated output paths, ordinary output with that name, and successful
managed output and replacement. On Linux it links a catalog object that
requires a symbol from a catalog archive, verifies the symbol in the final
executable and checks the link fails when an intentionally refreshed lock
omits that archive. A missing file and a hash-matched archive for the wrong
architecture are rejected without publishing an executable or payload.
The existing initializer and release markers still check authorization,
Debug/O2 JIT, AOT relocation and cleanup, and refusal of missing or altered
bundled runtime files before initialization.
[Managed fixture](../../tests/project/managed_native.py)

| Capability | Qualification status |
| --- | --- |
| Metadata selection and locks | Linux Release, Debug and strict 8/8 each, including the source dependency controls. |
| Synthetic managed execution | Linux Release, Debug and strict all passed the extended execution fixture with LLVM/Clang/LLD 23.1.4. The earlier source review also passed a direct Linux Debug replay before the targeted corrections. |
| Real managed raylib | Outside Slice 3 qualification. Slice 4 records separate headless and display-backed results through the managed path. |
| Windows and macOS managed execution | UNQUALIFIED. The Windows x64 path and fixture are present. The manual macOS ARM64 Debug/Release workflow selects only `project_managed_native` plus the shared compiler smoke. The fixture exercises manifest errors and unsupported-host refusal on macOS. The workflow was inspected locally and was not dispatched. |

The synthetic fixture covers relocation with the source project and catalog
moved away. Slice 4 independently exercises raylib relocation with the
compiler and checkout unavailable. The final local qualification below
covers managed editor analysis and retained callbacks.
[Managed fixture](../../tests/project/managed_native.py) · [macOS workflow](../../.github/workflows/ci-macos-managed-native.yml) · [Raylib sample](SLICE7_RAYLIB_INTEGRATION.md)

### Final focused Linux results

| Linux x86-64 profile | `project_managed_native` | `project_managed_native_resolution` | Total |
| --- | --- | --- | --- |
| Release | Passed, 7.29 s | Passed, 0.66 s | 2/2 |
| Debug | Passed, 32.19 s | Passed, 0.79 s | 2/2 |
| RelWithDebInfo with fatal ASan/UBSan and leak detection | Passed, 34.52 s | Passed, 2.81 s | 2/2 |

The final selection was limited to these two tests. The earlier Slice 2
8/8 results for all three profiles remain in the local qualification record.

From the repository root, use the Linux configure recipe above for each
profile. Set the fatal sanitizer options above for the strict run.

```sh
cmake --build "$build" --parallel 4 --target voidc voidls
ctest --test-dir "$build" --output-on-failure --no-tests=error \
  -R '^project_managed_native(_resolution)?$'
```

## Slice 4 managed raylib 6.0

The fixture copies the accepted S7 sample, replaces the manual tables in
the copy with a managed requirement for `org.voidlang.raylib`, exact
version `6.0.0`, ABI 3, and calls the existing `prepare.py --managed-catalog`.
The official Linux x86-64 release archive in the retained S7 workspace
matches SHA-256 `b64ba618a19e7da9e9c0e09bb398ecfd477a77d2d7231901bafc8739d27c08d2`.
The opt-in fixture independently checks the approved extracted inputs.
[S7 artifact approval and display controls](SLICE7_RAYLIB_INTEGRATION.md)

| Vendor file | SHA-256 |
| --- | --- |
| `include/raylib.h` | `047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b` |
| `lib/libraylib.so.6.0.0` | `1041653dd5c1cb8c67494fa398a296520d3ffec20cf1bf71b1eaa4b96ac61aee` |
| `LICENSE` | `882a5a819cf562aa3583aae3af3f2211dda15c63de9fc8cc4b399a2f9e78d799` |

The tests check the selected Linux target, provider and version, unchanged
lock bytes and timestamp on `--locked` and `check`, and explicit authorization
for JIT. The normal and O2 JIT and AOT processes run `--trace-only` with the
display variables removed. They verify one scoped callback with the expected
arguments and reject an invalid title. Missing and altered copies of both
the provider and upstream library are rejected in the catalog and the
relocated payload before the trace callback.

The existing AOT linker produces both executable profiles. The fixture
checks the ELF dynamic entries and the hashes of the three files published
automatically in the sibling `native/` directory. It relocates the packages
to paths containing spaces and `雪` and moves the source project and catalog
away. Bubblewrap masks the actual checkout and compiler build directory,
uses an unrelated working directory, and confirms the source and compiler
paths are inaccessible inside the same mount configuration used for AOT.
The approved libraries load dynamically from the catalog for JIT and from
the relocated `native/` directory for AOT.

The visible checks run separately on the real XWayland desktop using the S7
window, pointer, keyboard and pixel controls. The session reports AMD Radeon
RX 6600, Mesa 26.2.6 and direct GLX rendering. The harness finds only windows
owned by its application process group, verifies both mapped library paths,
captures the 640 × 360 application window, moves the pointer and sends a
Right-key event through the compositor. It checks motion of the pink pointer,
green animation and orange keyboard-controlled box, then sends the normal
window-manager close message. Each passing window run reports one trace
callback and one texture/window release. Normal JIT and O2 AOT exercise
implicit cleanup. O2 JIT and normal AOT exercise explicit cleanup.
[Managed fixture](../../tests/project/managed_raylib.py) · [Graphics controls](../../tests/project/raylib_graphics.py)

The first Debug and Release focused runs passed the two synthetic tests and
failed the new raylib case because the fixture appended `--project` after the
application argument delimiter. Moving it before the delimiter corrected the
fixture. No compiler, Runtime, C FFI or sample source change was required.

### Final Linux results

The three selected CTests were `project_managed_native`,
`project_managed_native_resolution` and `project_managed_raylib`. The raylib
CTest runs headlessly. The visible runs were separate direct fixture calls.
All profiles used LLVM/Clang/LLD 23.1.4.

| Linux x86-64 compiler profile | Focused CTest | Visible normal JIT | Visible O2 JIT | Visible relocated normal AOT | Visible relocated O2 AOT |
| --- | --- | --- | --- | --- | --- |
| Release | 3/3, 11.47 s | Pass, 48/24 | Pass, 47/24 | Pass, 49/22 | Pass, 52/24 |
| Debug | 3/3, 48.94 s | Pass, 46/24 | Pass, 47/24 | Pass, 50/23 | Pass, 47/25 |
| RelWithDebInfo with fatal ASan/UBSan | 3/3 with leak detection, 51.63 s | Pass¹, 47/22 | Pass¹, 47/22 | Pass¹, 43/22 | Pass¹, 43/22 |

The visible counts are rendered frames and keyboard-input frames. Across
the twelve passing windows, the pointer centroid moved 335 pixels, the
animated object moved 22–25 pixels, and the keyboard-controlled box moved
100–105 pixels. Every process exited zero with one callback and one owner
release marker.

¹ The first leak-enabled strict visible run drew 48 frames, received input
on 24 frames, released the texture and closed the window once, then aborted
on a 256-byte LeakSanitizer report in one allocation. Both unresolved stack
frames map to file offsets `0x9188aa` and `0x918f92` in `libgallium-26.2.6`,
matching both S7 native C controls. The leak-enabled graphics result remains
failed. The four passing strict windows used the explicit graphics-only Mesa
exclusion. ASan and UBSan remained fatal and all headless subprocesses kept
leak detection enabled. The checked shim was instrumented. The upstream
raylib binary was the uninstrumented release artifact.
[S7 Mesa frame attribution](evidence/slice7-raylib/failures/linux-mesa-address-attribution.txt)

### Platform status

Windows managed raylib remains **UNQUALIFIED**. The Windows guest was running,
but Guest Control rejected the login and the guest screenshot API failed.
No native Windows execution result was obtained. macOS managed execution
remains **UNQUALIFIED**. The prepared manual ARM64 Debug/Release workflow
still checks the reviewed full SHA and uses LLVM/Clang/LLD 23.1.4 with a
single `project_managed_native` selector and the existing compiler smoke.
On macOS the fixture checks metadata and the unsupported-host diagnostic.
Local actionlint 1.7.12 passed for the wrapper and shared workflow. Local
CTest selector inspection returned exactly the single intended test. The
opt-in test registered only when the local vendor path was supplied. The
workflow was not dispatched.
[macOS wrapper](../../.github/workflows/ci-macos-managed-native.yml) · [Shared workflow](../../.github/workflows/ci-macos-package-qualification.yml)

The later G11 record covers the Windows raylib software-provider lane on
Windows 10 Home x64. The results above retain their original source and
platform scope. [G11 Windows qualification](G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md)

### Replay

Configure each profile using the existing pinned Linux recipe. Keep `build`
and `TMPDIR` outside the checkout. The raylib test is opt-in and uses only
the approved local vendor directory. Bubblewrap with working mount isolation
is required for the relocation assertions. Visible runs additionally use
Pillow, X11, `xdotool`, `wmctrl`, ImageMagick `import` and writable
`/dev/uinput` on the test desktop.

```sh
vendor="$PWD/build/slice7-raylib/upstream/raylib-6.0_linux_amd64"
cmake -S . -B "$build" -DVOID_TEST_RAYLIB_ROOT="$vendor"
cmake --build "$build" --parallel 4 --target voidc voidls
ctest --test-dir "$build" --output-on-failure --no-tests=error \
  -R '^project_managed_(native(_resolution)?|raylib)$'

# Use ON for the strict build and OFF for Release or Debug.
PYTHONDONTWRITEBYTECODE=1 python3 -B tests/project/managed_raylib.py \
  "$build/voidc" "$PWD/.toolchains/llvm-23.1.4/bin/clang" "$vendor" "$sanitizers" --graphics
```

For the independently attributed Mesa leak on this desktop, the strict
graphics replay appends `--exclude-confirmed-mesa-lsan`. With `ON` on this
host, the default command retained leak detection and reproduced the failure.

## Final local qualification

The existing managed project fixture opens a versioned source overlay in
`voidls` with a verified format 3 lock that contains both a native provider
and a source package. It checks the generated native function's typed
hover, definition range and reverse reference to the application call. It
also checks definition navigation from the same application into the locked
source package. A different approved catalog revision with a stale lock
produces a diagnostic and no definition result. Both editor sessions shut
down cleanly, preserve the lock bytes and timestamp, and leave the native
initializer and release markers absent. Execution elsewhere in the fixture
requires those markers as positive controls. The generated definition is a
compiler-owned location with a synthetic file URI. The server does not
materialize an on-disk document for an editor to open.
[Managed project and editor fixture](../../tests/project/managed_native.py)

The Linux managed G5 test reuses the existing independently compiled C
provider, generated adapter, callback program and reload probe. The normal
and O2 JIT and relocated AOT cases exercise foreign-thread callbacks,
explicit and implicit unregister/drain, retired-handle rejection, and
callback fault propagation. Managed AOT checks the packaged provider,
adapter and notice before running with the original catalog moved away.
The reload probe obtains its admission through the verified manifest and
lock. It keeps the same Runtime and dynamic binding across a compatible
replacement, retains the old ORC generation across collection, and
dispatches the old captured body until unregister retires it. A second
approved lock and catalog change the notice payload identity while retaining
the project and native images. The replacement is rejected, and the old
registration still works. Transport refusal, retry, self-drain rejection
and fault cleanup use the same assertions as the existing manual G5 test.
[G5 process controls](../../tests/ffi/callback_process.py) · [G5 reload probe](../../tests/ffi/callback_reload.cpp)

| Linux x86-64 compiler profile | Managed project and editor | Manual G5 | Managed G5 | Total |
| --- | --- | --- | --- | --- |
| Release | 7.82 s | 5.20 s | 5.49 s | 3/3, 18.52 s |
| Debug | 36.07 s | 21.42 s | 22.78 s | 3/3, 80.28 s |
| RelWithDebInfo with fatal ASan/UBSan and leak detection | 39.67 s | 23.57 s | 23.94 s | 3/3, 87.19 s |

All three profiles used LLVM/Clang/LLD 23.1.4. This run exercised the
directly affected fixtures. The Slice 4 graphical results and the known
256-byte Mesa leak limitation remain the graphical qualification record.
Windows and macOS managed execution remain **UNQUALIFIED**. The manual macOS
workflow retains its single `project_managed_native` selector. Actionlint
1.7.12 passed for it and its reusable workflow.

Use the pinned Linux configure recipe above for each profile, with a
disposable build directory and profile-matched dependencies. The strict
profile uses `RelWithDebInfo` and `VOID_SANITIZERS=ON`.

```sh
mkdir -p "$scratch/tmp"
unset VOID_G5_EVIDENCE_ROOT
export TMPDIR="$scratch/tmp" PYTHONDONTWRITEBYTECODE=1 VOID_TEST_STDLIB="$PWD/stdlib"
# Strict profile
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake --build "$build" --parallel 4 --target voidc voidls void_g5_reload
ctest --test-dir "$build" --output-on-failure --no-tests=error \
  -R '^(project_managed_native(_callbacks)?|ffi_g5_callbacks)$'
```

## Source inventory and handoff

The starting inventory contained 18 modified tracked paths and eight
additions. The final pass modifies `CMakeLists.txt`, the two G5 controls,
the existing managed project fixture and this report. The reviewed
inventory contains 20 modified tracked paths and eight untracked paths.
The shared publication changes in `project_creation.cpp` and
`project_files.hpp` serve managed AOT payloads and the existing Git snapshot
publisher. Every changed path belongs to D4 or its maintained controls. The
editor and managed callback checks now have maintained CTest coverage.

```text
 M CMakeLists.txt
 M examples/raylib/prepare.py
 M include/void/c_dynamic.hpp
 M include/void/c_primitive.hpp
 M include/void/project.hpp
 M include/void/project_resolution.hpp
 M src/dynamic_closure.cpp
 M src/frontend/project.cpp
 M src/frontend/project_creation.cpp
 M src/frontend/project_files.hpp
 M src/frontend/project_resolution.cpp
 M src/frontend/project_selection.inc
 M src/ir/c_primitive.cpp
 M src/main.cpp
 M src/native_build.cpp
 M src/runtime/c_dynamic.cpp
 M tests/ffi/callback_process.py
 M tests/ffi/callback_reload.cpp
 M tests/project/header.cpp
 M tests/project/manifest.cpp
?? .github/workflows/ci-macos-managed-native.yml
?? docs/projects/DEPENDENCY_D4_CLOSEOUT.md
?? src/frontend/project_native.cpp
?? src/frontend/project_native.hpp
?? tests/project/managed_native.py
?? tests/project/managed_native_resolution.py
?? tests/project/managed_raylib.py
?? tests/project/raylib_graphics.py
```

At qualification closeout, `main` and the local `origin/main` ref pointed
to the baseline and the Git index was empty. Slices 3 and 4 removed their
disposable outputs at closeout. Slice 4
cleanup increased available space under `/var/tmp` from 368,493,584,384 to
369,501,716,480 bytes. The final pass reused the retained Slice 2 workspace
at `/var/tmp/void-d4-lock.4NMw47Kd` after verifying its original 8/8 logs
for each Linux profile. The workspace was removed after the final results
were recorded. Available space under `/var/tmp` rose from 369,254,817,792
to 371,472,523,264 bytes during cleanup.
[Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md)

## Controlled local integration

The 28 paths above are the integration allowlist. The review matched 20
modifications and eight additions to D4 implementation, documentation and
maintained tests. All 28 are regular nonexecutable files with Git mode
`100644`. The source and fixtures carry forward the qualified working tree.
The closeout records the integration checks and the accepted Linux results.

Actionlint 1.7.12 and `bash -n` passed for the manual macOS wrapper and
the shared workflow's shell steps. A local CMake registration probe with
the Darwin platform setting verified that `^(project_managed_native)$`
selects exactly one test and that `voidc` is its only compiler build target.
The macOS fixture checks the manifest
and the unsupported-host diagnostic. Windows and macOS managed execution
remain **UNQUALIFIED**.

The local commit title is
`Integrate D4 managed native dependencies and raylib qualification [skip ci]`.
The commit check compares the staged and committed path sets, modes, and
blob bytes with the pre-stage snapshot and checks the parent against the
baseline at the top of this report.
