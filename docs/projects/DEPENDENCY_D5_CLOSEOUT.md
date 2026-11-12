# D5 relocatable application packaging

Baseline `79e2f51e7353d96846a4bdb7f68ac5fc7c54ee67` on `main`.

## Command and layout

```sh
voidc package --project . --profile release
voidc package --project . --allow-native -o "output/My Application"
voidc package --project . --replace
voidc package --verify "output/My Application"
```

The initial distribution target is Linux x86-64 GNU. The default destination
is `<project>/dist`. Explicit destinations support spaces and UTF-8, including
missing parent directories. Project discovery, `--entry`, application identity,
profiles and source/native lock verification use the existing frontend. Native
packaging requires the managed catalog and `--allow-native`. Manual native
declarations receive a diagnostic from the package command.

```text
app                         native application executable
application.meta            existing encoded ApplicationMetadata revision 1
void-package.txt            target, profile, lock, selected-input digest, file checksums
licenses/                   verified Runtime/third-party notices and optional application LICENSE
native/                     managed provider, adapter, declared runtime libraries and notices
assets/                     explicitly declared application files
```

The existing manifest parser accepts `[application] resources` as a list of
at most 32 exact, flat `assets/<file>` paths. Resource names use the existing
portable ASCII component policy. Their combined byte limit is 16 MiB. Source
files, traversal, globs, aliases and case-folded duplicates are rejected.
Applications locate these files using `os.resource_dir()` and retain their
own resource-use checks. A supplied project-root `LICENSE` is copied to
`licenses/APPLICATION-LICENSE.txt`.

`package --verify` checks the exact file inventory, SHA-256, lengths, metadata
decoding, permissions and executable format. Missing or altered resources,
notices and native payload files fail verification. The inventory is a
content receipt. Publisher identity continues to come from the operator's
approved inputs.

## Reused implementation

D1–D4 provide manifest entry discovery, source packages, lock v3, catalog
selection, metadata, the typed IR, AOT emitter and native link resources.
The package command calls the existing AOT builder with its pinned
LLVM/Clang/LLD 23.1.4 inputs. The managed `native/` directory is published by
the existing D4 builder. Its provider verification, callbacks and ownership
continue through the existing Runtime interfaces.

The existing native resource manifest verifies the Runtime and link archives.
The generated `void-package-notices.txt` similarly hashes the applicable
notice files and maps them to the existing compiler installation's notice
locations. It is installed with the compiler. Missing or altered required
notice files fail packaging. Runtime notices include VOID MIT, Ryu, date,
tzdata, Unicode, LLVM/compiler-rt, libarchive, libsodium, curl, AWS-LC, c-ares,
nghttp2, zlib and libpsl material. The pinned PSL data and its attribution
are included alongside their license. Managed-provider notices retain their
accepted D4 placement.

The generated executable uses the existing verified native link inputs
with the development RUNPATH removed. Its ELF inventory rejects an embedded
RPATH or RUNPATH. Linux supplies the dynamic OS ABI libraries, including
`libidn2.so.0`, `libunistring.so.5`, `libstdc++.so.6`, `libgcc_s.so.1`,
`libm.so.6`, libc and the ELF loader. The raylib application also needs the
host's X11/OpenGL/graphics-driver stack. These OS libraries and their
compatible ABI versions are deployment prerequisites. The package carries
the D4 plan's native runtime files. It carries the selected Runtime notice
set for the linked native application.

The isolated Release ELF probe resolved all six `DT_NEEDED` entries from
the host with the checkout, compiler build and project masked. The test host
reported glibc 2.44. The probe establishes the tested dependency resolution
and leaves minimum supported OS versions unqualified.

The redistribution audit describes a dated source revision. The current
VOID MIT license and licensing scope accompany the existing install.
This assignment assembles local application outputs under the current
owner-authorized engineering scope.
[D4](DEPENDENCY_D4_CLOSEOUT.md) · [CLI](../CLI.md#application-distributions) · [Licensing scope](../release/LICENSING_SCOPE.md) · [Redistribution audit](../release/REDISTRIBUTION_AUDIT.md)

## Reproducibility and publication

The receipt records the compiler build identity, effective profile, verified
native-link resource digests, logical module identities and bytes, and
selected packaged data in an input digest. Its file records use sorted
relative names and the actual output lengths and SHA-256. Repeated builds
from the same inputs check identical receipts and file hashes. A copied
equivalent project checks the same file membership, selected-input digest
and non-executable data identities. The executable's actual checksum is
recorded for each build. Byte-identical executables across build locations
remain a separate determinism qualification.

Publication reuses the private project staging and filesystem admission
implementation. A complete package is assembled and verified beneath the
private stage, then published by exclusive directory rename. Existing output
requires `--replace`, a valid inventory and the same immutable application
ID. Linux `RENAME_EXCHANGE` replaces the complete directory in one operation.
A parent directory lock serializes package publication. A competing writer
can receive a collision or busy diagnostic. Previously published output is
preserved on earlier input, compile, link or validation failure. An unexpected
displaced directory is retained with a diagnostic. Directory and file modes
are normalized to 0755 and 0644, with 0755 for `app`.

Power-loss durability remains untested. The tests cover process-level
failure, repeated output, changed-resource replacement, competing creation,
alias rejection and preservation of unrelated existing directories.

## Maintained controls

`project_application_package` covers ordinary applications with an arbitrary
entry filename, metadata, declared assets, applicable notices, deterministic
repeat inventories, copied equivalent inputs, missing and stale source
locks, invalid resource declarations, resource limits, missing/changed
notice inputs, safe publication, replacement and independent execution.
It also runs the locked-source application with the checkout, source
project, dependency source tree and development compiler path unavailable.

`project_package_native` reuses the maintained D4 process fixture and adds
package publication and verification. It checks approved link inputs,
native rejection before initialization, resource release and relocated
execution. `project_package_raylib` reuses the real managed raylib 6.0
fixture headlessly. The same fixture's `--package --graphics` mode uses
the S7 window, input and pixel controls for normal/O2 JIT and independently
linked, relocated AOT. Bubblewrap masks the checkout and compiler tree.
The installed-compiler regression also packages and runs an application
from a relocated compiler installation.

## Qualification notes

| Linux x86-64 GNU compiler profile | Focused CTest | Visible normal JIT | Visible O2 JIT | Visible relocated normal package | Visible relocated O2 package |
| --- | --- | --- | --- | --- | --- |
| Release | 14/14, 26.37 s | Pass, 49/22 | Pass, 47/25 | Pass, 50/24 | Pass, 51/24 |
| Debug | 14/14, 93.75 s | Pass, 46/24 | Pass, 52/25 | Pass, 52/25 | Pass, 52/24 |
| RelWithDebInfo with fatal ASan/UBSan and leak detection | 14/14, 115.79 s | Pass¹, 50/24 | Pass¹, 46/22 | Pass¹, 47/24 | Pass¹, 42/22 |

The display counts are rendered frames and input frames. The real XWayland
desktop runs verified both mapped raylib libraries, pointer motion of 335 px,
animation motion of 22–25 px and keyboard motion of 100–105 px. Each window
reported one callback and one texture/window release before successful exit.
The package runs use both explicit close and Runtime-owned implicit cleanup.

¹ Strict visible runs used the existing Mesa graphics-only leak detection
exclusion. All focused CTests and the trace-only subprocesses ran with leak
detection enabled. ASan and UBSan stayed fatal throughout.

The application package and installed-compiler tests also passed 2/2 in
each profile after the fixtures' host gates were aligned with Linux x86-64
GNU support.

The first ordinary-package fixture expected a generic inventory error for
an empty asset directory. It now checks the specific missing-directory
diagnostic. Its restored resource also needed the original 0644 mode.
The raylib fixture initially retained the old `gallery` executable name
after packaging produced `app`. The fixture now carries the selected
executable name through relocation.

The initial strict compiler build crashed inside Clang 23.1.4 SROA while
optimizing `Semantics::json_call` in `compiler.cpp`. A serial retry with a
64 MiB process stack completed that translation unit using the same
compiler and optimization/sanitizer settings. Both scheduling and stack
limit changed on the retry, so the triggering condition remains unisolated.

## Platform and retained limitations

This closeout records Linux x86-64 GNU. W2 adds Windows x64 MSVC
packaging and passed all ten focused CTests in both hosted Windows
Server 2022 profiles, Debug and Release.
[W2 hosted acceptance](W2_WINDOWS_D5_IMPLEMENTATION.md#hosted-acceptance)

Windows uses `app.exe` in the same directory layout and requires an
absent destination even when `--replace` is supplied. macOS ARM64 packaging
and managed native execution remain unsupported.

G11 covers the real Windows raylib payload and the profile-key compatibility
repair for D5 inventory revision 1. [G11 Windows qualification](G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md)

The known 256-byte Mesa graphics allocation remains the accepted display
limitation. Leak detection stays enabled for focused headless sanitizer
tests. Any strict visible replay using the established Mesa exclusion
records that exclusion explicitly. The upstream raylib binary remains the
approved, uninstrumented 6.0 release. The provider shim uses matching
instrumentation for the strict Runtime.
[Raylib and Mesa attribution](SLICE7_RAYLIB_INTEGRATION.md) · [D4 graphics results](DEPENDENCY_D4_CLOSEOUT.md#final-linux-results)

## Replay

Use the pinned Linux configure recipe with the same LLVM/Clang/LLD SDK
and profile-matched host and HTTP prefixes as D4. Keep build and temporary
outputs in a task-owned directory outside the checkout. For strict, use
`RelWithDebInfo`, `VOID_SANITIZERS=ON`, fatal ASan/UBSan and leak detection.
[Pinned Linux configure recipe](DEPENDENCY_D2_CLOSEOUT.md#linux-reproduction)

```sh
vendor="$PWD/build/slice7-raylib/upstream/raylib-6.0_linux_amd64"
mkdir -p "$scratch/tmp"
export TMPDIR="$scratch/tmp" PYTHONDONTWRITEBYTECODE=1 VOID_TEST_STDLIB="$PWD/stdlib"
# Strict test environment
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake -S . -B "$build" -DVOID_TEST_RAYLIB_ROOT="$vendor"
cmake --build "$build" --parallel 4 --target voidc voidls \
  void_project_manifest void_project_header void_project_creation
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_(application_package|package_native|package_raylib|manifest(_header|_no_authority)?|creation(_process)?|git_dependencies|git_https|managed_native|discovery_cli)|cli_(contract|install))$'

PYTHONDONTWRITEBYTECODE=1 python3 -B tests/project/managed_raylib.py \
  "$build/voidc" "$PWD/.toolchains/llvm-23.1.4/bin/clang" "$vendor" "$sanitizers" --package --graphics
```

The strict display replay can append `--exclude-confirmed-mesa-lsan` for
the established host-driver limitation. The headless fixture keeps leak
detection enabled within that replay.

The strict compiler build retry used `ulimit -s 65536` and
`cmake --build "$build" --parallel 1` with the same target list and flags.
Relocation tests require Bubblewrap. Visible runs use the existing S7
Pillow, X11, `xdotool`, `wmctrl`, ImageMagick and `/dev/uinput` controls.

## Source inventory and Git status

```text
 M CMakeLists.txt
 M cmake/NativeBuild.cmake
 M docs/CLI.md
 M docs/install/README.md
 M include/void/native_build.hpp
 M include/void/project.hpp
 M src/frontend/project.cpp
 M src/frontend/project_creation.cpp
 M src/frontend/project_files.hpp
 M src/main.cpp
 M src/native_build.cpp
 M tests/cli/install.py
 M tests/project/managed_native.py
 M tests/project/managed_raylib.py
?? docs/projects/DEPENDENCY_D5_CLOSEOUT.md
?? tests/project/application_packages.py
```

At qualification closeout, the Git index was empty. Local `main` and the
local `origin/main` ref pointed to the D4 baseline.
The disposable D5 build and package workspace was removed after verification.
Available space under `/var/tmp` was 371,633,119,232 bytes before qualification
and increased from 369,933,639,680 to 371,591,385,088 bytes during cleanup.
[Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md)

## Controlled local integration

The 16 paths above are the integration allowlist, with 14 modifications and
two additions. Each is a regular nonexecutable file with Git mode `100644`.
The review checked exact package inventory, same-application replacement,
bounded asset paths, verified notice inputs and isolated relocation against
the implementation and maintained fixtures. The existing `build` command
uses the shared linker with packaging disabled. D1–D4 lock, acquisition and
native ownership code carry forward from the baseline.

Integration reuses the accepted Linux qualification above. The integration
checks include `git diff --check` and a byte comparison of the staged and
committed blobs against the reviewed working tree. The local commit title is
`Implement relocatable VOID application packaging [skip ci]`.
