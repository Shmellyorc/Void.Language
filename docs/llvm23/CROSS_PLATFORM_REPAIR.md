# LLVM 23.1.4 Windows and macOS configuration repair

2026-10-29. Baseline: `a4ca3e585db945570b22c890b086fa6a53692a5f`.
Implementation is ready for independent Golden Wrench review. **Native Windows
and macOS acceptance remains pending.** This Linux host cannot execute either
platform. No hosted run, staging, commit, push or branch creation was performed.

## Initial failures and inspected inputs

[Compiler foundation run 40811247854](https://github.com/Shmellyorc/Void.Language/actions/runs/40811247854)
passed Linux Debug, Release and Sanitizers. Both Windows configurations stopped
while generating the ORC try-run project; both macOS configurations stopped at
the stop-token compile/link check. Compiler tests did not run on those platforms.

- Windows Debug job `123241817564`, Release `123241817433`: runner image
  `windows-2022`, `20261025.346.1`; MSVC `14.44.35207`, UCRT/Windows SDK
  `10.0.26100.0`. The x64 Visual Studio environment supplies native libraries and
  DIA, but no discoverable zlib, zstd or LibXml2 development package.
- macOS Debug job `123241817521`, Release `123241817637`: image
  `macos-26-arm64`, `20261013.0424.1`; Xcode `26.6`, selected MacOSX SDK `26.5`,
  default deployment `26.0.0`. The archived C++ driver expansion invokes
  `/usr/bin/ld`, `-lc++`, and that SDK's `-syslibroot`; it does not select the
  archive's libc++ dylib. Its other recorded dependencies resolve to SDK zlib
  1.2.12, SDK LibXml2 2.9.13 and `/opt/homebrew/lib/libzstd.a` (the static zstd
  search succeeds; the status line reports the separately found shared library).
- Downloaded Debug configure artifacts `12870104075` (Windows) and `12870970569`
  (macOS), then downloaded and SHA-256 verified the **actual official archives**:
  Windows x64 `854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2`;
  macOS ARM64 `5d3179db39adeca5b17a34d82708fe976bc60e055b05fe557878fb3fb64d39ef`.
  These match the unchanged installer pins. Inspected their CMake exports and
  native COFF/Mach-O library contents on Linux, without executing foreign code.

The [evidence directory](evidence/cross-platform-repair/) retains initial error
excerpts, export excerpts with original line numbers, inspected file hashes,
runtime inspection, local commands/results and negative discovery logs.

## Modified files

- `CMakeLists.txt`, `cmake/LLVMPlatform.cmake`, `cmake/NativeBuild.cmake`:
  platform setup, dependency discovery, CRT/header selection and native manifest.
- `cmake/dependencies/CMakeLists.txt`, `tools/ci/llvm_sdk.py`: pinned upstream
  development-library builds through the existing SDK bootstrap.
- `cmake/probes/StopToken.cpp`, `tests/toolchain/CMakeLists.txt`,
  `tests/toolchain/Dependencies.cpp`: executed toolchain/API probes.
- Only the CRT flag changes in `tests/stage10/{console,conversion,fields,operators,temporal}.py`,
  `tests/stage11/globals.py`, and
  `tests/stage8/{local_libraries,sprint3,sprint4,sprint6}.py`.
- `docs/llvm23/{ACCEPTANCE,COMPATIBILITY,IMPLEMENTATION}.md`, this record and its
  evidence; two active setup descriptions in `docs/SOURCE_COMPILER.md` and
  `docs/STAGE9_APPLICATION_WORKFLOW.md`.

An exact path inventory is in `evidence/cross-platform-repair/modified-files.json`.
No frontend, typed IR, backend/O2, Runtime/GC/leases or protected README source
was edited. Generated channel-test receipts are retained locally beneath
`.toolchains/wrench-inspection/generated-evidence/`, outside the review diff.

## Windows: real dependencies and the actual CRT ABI

`LLVMConfig.cmake` enables zlib, zstd, LibXml2 and DIA, but its dependency searches
are optional. `LLVMSupport` nevertheless exports `ZLIB::ZLIB` and
`zstd::libzstd_static`. The ORC check's imported-target serialization first fails
on absent `ZLIB::ZLIB`. `LLVMWindowsDriver` also exports `LibXml2::LibXml2`;
leaving the other packages absent would leave a predictable broken SDK target.

The SDK's `LLVMSupport.lib` contains actual COFF x86-64 members with ordinary
`compress2`, `uncompress` and `ZSTD_*` undefined symbols, requiring real libraries.
Its **168 inspected members all declare `RuntimeLibrary=MT_StaticRelease`**,
`/DEFAULTLIB:libcmt.lib`, `/DEFAULTLIB:libcpmt.lib`, and iterator level zero.
The baseline's claim that this official archive uses `/MD` was incorrect.
After dependency discovery, that mismatch would cause native link failure.

Corrections:

1. The existing SDK helper builds upstream static development packages using
   the verified SDK's `clang-cl`, Release `/MT`, and ordinary CMake FetchContent.
   It installs beside the immutable SDK, in `.toolchains/llvm-23.1.4-deps`, and
   exports `CMAKE_PREFIX_PATH` for subsequent workflow steps. This runs on both
   fresh SDK installs and SDK cache hits. No new package manager is introduced.
2. Before loading LLVM's exports, CMake requires `FindZLIB`'s `ZLIB::ZLIB`, the
   zstd config's `zstd::libzstd_static`, and upstream LibXml2's config target.
   The upstream LibXml2 target retains its static transitive `bcrypt` dependency.
   LLVM's SDK files are unchanged. Missing packages fail at required discovery.
3. VOID's CMake targets use `MultiThreaded` in both configurations; the native
   manifest and ten existing AOT scripts use `-fms-runtime-lib=static`.
   Debug retains its ordinary debug information/optimization settings. Iterator
   level zero and the SDK's RTTI setting remain in effect.
4. The archive exports `DIASDK::Diaguids`; its own `FindDIASDK.cmake` resolves the
   real consumer SDK. DIA discovery is now required when LLVM enables it. The
   existing fallback for stale absolute DIA references is retained.

| Dependency | Pinned version | Source archive SHA-256 |
|---|---|---|
| zlib | 1.3.2 | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` |
| zstd | 1.5.7 | `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3` |
| LibXml2 | 2.15.4 | `98087fd181d9070724f3fbc65c7377db03038eb92bd882374daff44940138821` |

The zlib static output is named `zlibstatic` so CMake 3.24–3.31 FindZLIB can find
it; upstream 1.3.2 defaults to `zs.lib`. XML parsing remains enabled; optional
iconv, ICU, modules and zlib integration are not needed by LLVM's XML consumer.
The dependency prefix contains static libraries, not new dependency DLLs.
`void-llvm-dependencies-<configuration>.txt` records the resolved target files.
`toolchain_llvm_dependencies` performs actual zlib/zstd round trips and XML
allocation/parsing/free, and checks header/runtime versions.

This changes the erroneous CRT selection to match the official binary SDK; it
does not suppress `/FAILIFMISMATCH` or mix CRTs. Static CRT code is serviced by
rebuilding/redeploying binaries. C++/CRT-owned objects must not cross into an
independently built DLL with a different runtime. VOID's existing native C ABI,
ownership rules, UTF-8 paths, private Windows link workspace, checked copy,
atomic publication and manifest protections are unchanged. The independent
LLVM-free stack-probe project's own CRT setting is outside this repair.

Sources: [zlib release and hash](https://zlib.net/zlib.html),
[zstd 1.5.7](https://github.com/facebook/zstd/releases/tag/v1.5.7),
[LibXml2 2.15.4 checksum](https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.4.sha256sum).

## macOS: pair Apple's headers and runtime

The upstream archive's `include/c++/v1/__config_site` defines
`_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0`. Its LLVM 23 atomic headers
therefore assume the LLVM 22+ synchronization ABI exists. Instantiating real
stop callbacks references the three unresolved symbols reported by the user:
`__atomic_monitor_global`, `__atomic_wait_global_table`, and
`__atomic_notify_all_global_table`, in `std::__1`.
The selected Apple runtime cannot satisfy that link. This is a header/runtime
pairing failure, not a missing language feature in Clang 23 or absent C++20 mode.

The official macOS archive **does** supply all three symbols in
`lib/libc++.1.0.dylib`; `llvm-nm` confirms their external definitions. That dylib
has install name `@rpath/libc++.1.dylib`, requires bundled
`@rpath/libc++abi.1.dylib` and `@rpath/libunwind.1.dylib`, and declares minimum
macOS 14.0 / build SDK 14.5. Simply adding `-L` would not be a complete deployment
solution: a redistributed runtime family would require consistent loader paths,
ABI/interoperation review, packaging, notices, signing and deployment testing.
**This implementation does not select or redistribute that runtime family.**

Inspection of the 66 static LLVM archives in the computed ORC/standard-pass/
AArch64 component dependency closure found no undefined references to these
three new atomic-wait functions. The remaining compatibility check is the actual
native ORC link/run, not a claim inferred from that symbol scan.

Instead, `void_configure_llvm_platform()` selects libc++ vendor headers from the
chosen Apple SDK, falling back to the active Xcode toolchain's header directory.
All project C++ and Objective-C++ compilation uses
`-stdlib=libc++ -nostdinc++ -isystem "<Apple libc++ headers>"` with Clang 23.1.4.
Apple headers select the synchronization ABI available in Apple's runtime and
retain vendor availability checks. The ordinary system `-lc++` link stays in
place. No internal libc++ configuration macro, SDK header or binary export is
rewritten. The native manifest carries the same header flags for independently
compiled C++ hosts, together with the existing Apple SDK/architecture/deployment
arguments. The current deployment target is preserved.

[Apple's Xcode 26 release notes](https://developer.apple.com/documentation/xcode-release-notes/xcode-26-release-notes)
document standard `stop_token`/`jthread` support without the experimental opt-in.
Xcode 26+ is the supported vendor-header setup for this repair; the actual
compile/link/run check remains authoritative. This selects a standard library,
not a different compiler: LLVM, Clang, the version-checked LLD tools and LLVM's
backend remain 23.1.4. The existing Darwin system linker selection is unchanged.

The mandatory stop-token check now compiles, links **and runs** with the selected
flags. Its `-Wl,-t` output records actual linker inputs in CMakeConfigureLog.yaml.
The shared probe exercises `stop_source`, `stop_token`, immediate and late
callbacks, repeated-request behavior, atomic wait/notify, and cancellation of a
worker blocked in `condition_variable_any`. A ten-second watchdog fails a stuck
implementation. The same source is the `toolchain_stop_token` CTest target.
The mandatory ORC try-run is retained and rechecks every configuration.

## Local verification and limits

All execution below is **Linux x86-64**, using the complete official
LLVM/Clang/LLD 23.1.4 SDK, CMake 4.3.4 and Unix Makefiles. The machine has no
Ninja; the first attempted Ninja dependency configuration was corrected to
Unix Makefiles. A first existing-build invocation also needed regeneration and
the correct aggregate target `void_async_tests`; its initial output is retained.
These are local invocation corrections, not native-platform passes.

```sh
sdk="$PWD/.toolchains/llvm-23.1.4"
cmake -S cmake/dependencies -B build/wrench-deps-make -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
  -DCMAKE_INSTALL_PREFIX="$PWD/.toolchains/wrench-deps"
cmake --build build/wrench-deps-make --parallel 3
cmake --install build/wrench-deps-make
cmake -S tests/toolchain -B build/wrench-dependency-probe -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
  -DCMAKE_PREFIX_PATH="$PWD/.toolchains/wrench-deps"
cmake --build build/wrench-dependency-probe --parallel 3
ctest --test-dir build/wrench-dependency-probe -V
```

- Final dependencies build/install and API/link/run probe: **pass**, all three
  reported header/runtime versions match the final pins above. The initial
  investigation also tested LibXml2 2.15.1; the final pin and retained final
  build/probe evidence use the current 2.15.4 maintenance release.
- Three fresh configurations with
  `-DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE`, `..._zstd=TRUE`, or
  `..._LibXml2=TRUE`: **fail as required** at the corresponding required package.
  Each otherwise uses the real built dependency prefix; no dummy targets.
- Fresh root configuration `build/wrench-linux-release`, Release, the SDK's
  explicit C/C++ compiler paths, `LLVM_DIR`, `VOID_CLANG`, `VOID_LINKER`, and
  `CMAKE_EXE_LINKER_FLAGS=--ld-path=<SDK>/bin/ld.lld`: **ORC compile/link/run passes**.
- Fresh `void_stop_token_probe` build and CTest: **1/1 pass**.
- Reconfigured existing accepted Release build; `voidc`, `voidls`,
  `void_stop_token_probe`, `void_async_tests` build. Focused tests **8/8 pass**:
  `toolchain_stop_token`, `async_ir`, `async_lifecycle`, `async_parallel`,
  `source_cli`, `stage11_globals_aot`, `stage10_fields_aot`, `channel_execution`.
- Preservation tests **5/5 pass**: `application_workflow_stage9`,
  `native_output_paths_ci88`, `reload`, `native`, `debug_cache`.
- `python3 -m py_compile tools/ci/llvm_sdk.py` and `git diff --check`: pass.

The dependency test exercises the **same discovery macro and real Linux-built
libraries**, not the Windows CRT/COFF ABI. The cancellation test uses Linux
libstdc++, not Apple libc++. Foreign symbol inspection proves definitions and
imports only. There is no native Windows ORC execution/build/test result and no
native Apple vendor-header probe/cancellation/ORC result yet.

## Exact owner-controlled hosted acceptance

After independent Golden Wrench review, Boss should run the existing
`.github/workflows/ci.yml` against the reviewed source. Preserve all seven jobs.
The unchanged workflow's SDK-helper step now supplies Windows dependencies;
no new dispatch, cache-key change or external setup service is needed.

For **Windows Debug and Release**, in the initialized x64 MSVC environment:

```powershell
# In subsequent Actions steps after tools/ci/llvm_sdk.py exported its environment.
foreach ($configuration in @('Debug', 'Release')) {
  $build = "build/wrench-accept-$configuration"
  cmake -S . -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$configuration" `
    "-DLLVM_DIR=$env:LLVM_DIR" "-DVOID_CLANG=$env:VOID_CLANG"
  if ($LASTEXITCODE) { throw 'Configuration/ORC probe failed' }
  Get-Content "$build/void-llvm-dependencies-$configuration.txt"
  cmake --build $build --parallel 3
  if ($LASTEXITCODE) { throw 'Native build failed' }
  ctest --test-dir $build -R '^(toolchain_.*|async_ir|async_lifecycle|async_parallel|source_cli|stage10_fields_aot|stage11_globals_aot|channel_execution|application_workflow_stage9|native_output_paths_ci88|reload|native|debug_cache)$' -V
  if ($LASTEXITCODE) { throw 'Focused native tests failed' }
  ctest --test-dir $build --output-on-failure
  if ($LASTEXITCODE) { throw 'Full native suite failed' }
}
```

Require `VOID_LLVM_ORC_RUNS=1` and exit status zero in each fresh configure log;
resolved dependency paths must name real `.lib` files from the dependency prefix.
Require matching API/runtime versions, `/MT` (not `/MTd` or `/MD`) directives in
the built project/dependency objects, iterator zero for SDK-facing C++, and the
real DIA target. Inspect `llvm-readobj --coff-directives` on `LLVMSupport.lib`,
`void_runtime.lib` and the installed dependency libraries. Retain the native
manifest's static-runtime flag and the UTF-8/deep-path/security test results.

For **macOS ARM64 Debug and Release**, after the same helper/environment step:

```sh
for configuration in Debug Release; do
  build="build/wrench-accept-$configuration"
  cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$configuration" \
    -DLLVM_DIR="$LLVM_DIR" -DVOID_CLANG="$VOID_CLANG" || exit 1
  cmake --build "$build" --parallel 3 || exit 1
  ctest --test-dir "$build" -R '^(toolchain_stop_token|async_ir|async_lifecycle|async_parallel|source_cli|stage10_fields_aot|stage11_globals_aot|channel_execution|application_workflow_stage9|native_output_paths_ci88|reload|native|debug_cache)$' -V || exit 1
  otool -L "$build/void_stop_token_probe" "$build/voidc" "$build/voidls" || exit 1
  ctest --test-dir "$build" --output-on-failure || exit 1
done
```

Require `VOID_HAS_STANDARD_STOP_TOKEN=1`, an actual zero-status cancellation run,
and `VOID_LLVM_ORC_RUNS=1` in both fresh configure logs. Inspect the stop probe's
`-Wl,-t` trace for the selected Apple SDK/system libc++; `otool -L` must show
`/usr/lib/libc++.1.dylib`, with no new bundled libc++/libc++abi/libunwind family.
Check compile_commands.json for Clang 23.1.4 and Apple vendor include paths in
both C++ and Objective-C++ commands. The compiler, runtime and independently
compiled native hosts must share those header settings. Preserve SDK/deployment
versions and the native manifest, then retain complete CTest/editor results from
the existing workflow. Linux Debug/Release/Sanitizers must remain green.

Stop here for independent review. Neither native platform is accepted by this
local engineering record; Boss owns hosted execution and any release decision.
