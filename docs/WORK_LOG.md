# Work log

## Compiler foundation and architectural proofs, 10 October 2026

### Inputs and scope

Read the complete supplied Language Bible and Development Strategy before
implementation. Moved the original documents into `docs/LANGUAGE_BIBLE.md` and
`docs/DEVELOPMENT_STRATEGY.md`, preserving their snapshots and appending current
implementation notes. Followed the user's larger-phase instruction in place of
the older numbered milestone process. Built fresh code in `Void.Language`;
no earlier compiler implementation was reused or backup modified.

All work is local. No GitHub repository, Git initialization, commit, push,
publication or remote modification was performed. Per user direction, hosted
workflow execution is deferred until the user creates the repository and pushes.

### Implemented

- C++20/CMake targets for core frontend source support, backend-independent proof
  IR, LLVM lowering, managed/native runtime proofs, compiler harness and executable
  tests. No empty lexer/parser/LSP/task systems were added. `stdlib/README.md`
  records the future standard-library boundary without claiming an implementation.
- Exact LLVM 19.1.7 SDK requirement and matching Clang 19.1.7 AOT driver check.
  Verified real ORC headers, `libLLVMCore.a`, and `LLVMConfig.cmake` in the installed
  `/usr/lib/llvm-19` SDK. Missing dependencies were installed by the user; no
  dummy targets, mixed libraries or relaxed requirements were used.
- Source byte offsets, CRLF-aware line/column lookup and a basic source diagnostic.
  Validated proof IR with i32 functions, arguments, literals, arithmetic, Boolean
  comparisons, calls, branches, returns and stable module identities.
- Shared LLVM lowering with IR verification, O2 optimization, native object
  generation and source debug metadata. AOT integration links and executes a real
  native binary with three runtime inputs and verifies its DWARF source table.
- ORC invocation and resource-tracked generations. Leased dispatch preserves active
  old code, retains the execution session and safely removes unused generations.
  Signature/layout/capture/constant incompatibility is rejected without changing
  the active slot. Invalid IR and missing native symbols fail compilation.
- Experimental single-threaded precise non-moving heap, copied type descriptors,
  nested-reference offsets, explicit roots, iterative marking, cycle reclamation
  and validation before sweep. LLVM-generated normal/O2 loops register a stack
  slot and preserve live references across repeated native safepoints.
- Real C ABI fixture: generated native calls, validated borrowed terminated UTF-8,
  opaque resource ownership, stable retained callback, rooted managed capture,
  unregister and deterministic resource release. Callback behavior follows a
  compatible JIT replacement without registering a new native function address.
- Local `.github/workflows/ci.yml` for Linux, Windows and macOS, Debug and Release,
  with Linux ASan/UBSan. Downloads complete official versioned LLVM SDK archives;
  configures Windows SDK/MSVC ABI and Darwin SDK explicitly; installs Linux
  development dependencies. Push, pull-request and manual triggers; read-only
  repository permissions; standard hosted runners only.
- Comprehensive ignore rules for build products, credentials, local environments,
  IDE configuration, agent directories and cloud configuration. `docs/` and
  `.github/` remain included.

### Verified host and toolchain

| Item | Actual value |
|---|---|
| Executed OS | PikaOS 4, Debian-family Linux |
| Architecture | x86_64 |
| LLVM development SDK | 19.1.7, installed distribution package |
| C and C++ compiler | Clang/Clang++ 19.1.7 |
| LLVM CMake directory | `/usr/lib/llvm-19/lib/cmake/llvm` |
| CMake | 4.3.4 |
| Local build generator | Unix Makefiles |
| AOT linker actually used by Clang | GNU ld 2.47 |
| Additional available linker | LLD 19.1.7, verified installed, not used in recorded AOT links |
| C++ standard library | Host GCC 16 libstdc++ headers/runtime |
| Windows / macOS / GitHub Actions | Not executed, unverified |

Linux package reproduction is in the README. Required base packages include
`llvm-19-dev`, `clang-19`, `lld-19`, CMake, a build generator, and the SDK's
exported development dependencies. The user additionally installed `libzstd-dev`
and `libedit-dev` after CMake identified missing zstd targets. LLVM configuration
still notes optional CURL is absent; the selected components do not require it.

A noninteractive administrator install was unavailable. Matching packages were
initially downloaded to `/tmp` during toolchain investigation, but they were not
used for the project builds. All recorded configurations use the complete
system-installed SDK after the user's installation.

### Actual commands and results

Run from the project root. The recorded builds used the default Unix Makefiles
generator; Ninja is also the configured hosted-CI generator, but has not run here.

```sh
cmake -S . -B build/debug \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 4
ctest --test-dir build/release --output-on-failure

cmake -S . -B build/sanitizers \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOID_SANITIZERS=ON
cmake --build build/sanitizers -j 4
ctest --test-dir build/sanitizers --output-on-failure

ctest --test-dir build/release --output-on-failure \
  --repeat until-fail:10 -R 'reload|gc|native'
python3 -m py_compile tools/ci/llvm_sdk.py
```

Final results after code/test changes:

| Configuration/check | Executed result |
|---|---|
| Debug | 6/6 suites passed, 0.59 seconds |
| Release | 6/6 suites passed, 0.53 seconds |
| RelWithDebInfo + ASan/UBSan, including leak detection | 6/6 suites passed, 1.33 seconds |
| Release repeat stress | 10 executions each of reload, GC and native suites passed, 3.54 seconds total |
| Official SDK helper Python syntax | Passed |
| Standalone ORC harness | Executed, printed `42` |
| AOT runtime paths | Executed, printed `7`, `42`, `43` as expected |
| Emitted object debug information | `llvm-dwarfdump --verify` passed; VOID source line table present |

Each suite executes compiled code:

| Suite | Observable checks |
|---|---|
| core | Source positions/diagnostics and rejection of invalid operands, types, duplicate definitions and missing callees |
| jit | Normal/O2 native results and branches; invalid entry; generation retirement; execution after host destruction |
| reload | Active old call blocks on another thread; future calls use new code; old call returns original result; code retires after return; incompatible/invalid edits leave active code intact; four caller threads and 100 compatible publications |
| gc | Embedded reference trace, malformed layouts, invalid-root sweep prevention, mark rollback, normal/O2 generated root loops, live cycle survival and dead cycle reclamation |
| native | Real JIT-to-C call, missing-symbol failure, UTF-8 and NUL diagnostics, opaque resource lifetime, retained/rooted C callback following replacement and unregister cleanup |
| aot | LLVM object emission, Clang native linking, debug metadata verification, executable runtime branch and arithmetic outputs, paths containing spaces |

Each GC suite execution performs 20,000 generated safepoint collections across
40 normal/O2 invocations, allocating 40,000 unreachable cycle nodes plus the
rooted graphs. Each reload suite includes 100 stress publications and four
concurrent native callers. The repeat run adds 1,000 compatible publications and
200,000 generated safepoint collections. These are correctness stress counts,
not throughput or pause-latency performance claims.

### Failures encountered and resolved

- Initial machine had LLVM 23 development tools without usable development SDK
  headers. Selected and installed the pinned complete LLVM 19.1.7 SDK instead.
- LLVM exported zstd dependency could not resolve until the user installed the
  development package. Retried normal CMake configuration successfully.
- Corrected an LLVM 19 header path during compilation; fixed project aggregate
  initialization warnings with explicit default member initialization.
- The first sandboxed sanitizer run failed because LeakSanitizer cannot inspect
  processes under the sandbox tracing restriction. Reran outside that restriction
  with leak detection enabled; all suites passed. Sanitizers were not disabled.
- One third-party warning remains: LLVM 19 includes `<ciso646>`, whose host GCC 16
  header warns under C++20. No global warning suppression was added.

### Decisions, limits and next phase

Architectural choices and their rationale are in `DECISIONS.md`. Restrictions
and unverified work are in `KNOWN_ISSUES.md`. This phase establishes executable
foundation proofs, not the full GC, source compiler or hot reload product.
LLVM statepoint/stack-map integration was investigated through installed APIs
but is not implemented or validated. No performance benchmark, TSan run, foreign
thread callback test or non-Linux execution is claimed.

The next cohesive phase should specify and implement a small frontend with exact
source diagnostics, lower parsed functions into the current IR, and test their
AOT/JIT agreement. Resolve open grammar/numeric choices deliberately. Complete
optimized root lowering and thread-coordination proofs before adding managed
language types. Run hosted platform jobs once the user publishes the repository.

## CI stabilization phase: first hosted run, 10 October 2026

User request: fix actual CI failures before expanding the frontend, keep LLVM
19.1.7 and all architectural tests, work locally without commits or pushes.

### Hosted evidence inspected

Read the actual job logs of
[run 38098620186](https://github.com/Shmellyorc/Void.Language/actions/runs/38098620186),
commit `8ed7e395bd4789f4cb82c61a346794f37a435077`:

| Host/job | Observed failure |
|---|---|
| Ubuntu 24.04 / 114349692845 | Clang invoked GNU ld, which rejected `libLLVMOrcJIT.a` as an unrecognized file format |
| Windows 2022 / 114349692876 | Ninja required the producer's absent VS2019 Professional DIA SDK `diaguids.lib`, despite the runner having VS2022 |
| macOS / 114349692735 | libc++ did not provide `std::jthread` or `std::stop_token` in the selected configuration |

All three configured successfully, then failed Build Debug; none executed the
architectural tests. Earlier sections above describe the earlier local phase,
not successful hosted verification.

### Corrections

- The SDK helper verifies and exports the Linux archive's LLD 19.1.7. All CI
  configurations pass it explicitly to CMake; both host executables and the AOT
  integration link use that exact linker. GNU ld is not given LLVM bitcode.
- CMake resolves only missing imported LLVM DIA dependencies to a real library
  in the active Visual Studio SDK, with the selected target architecture. It
  retains the dependency and fails if it cannot be found. No SDK files are edited.
- The reload test uses standard threads, an atomic stop flag and a scope guard
  that joins successfully created readers even on exceptions. Four concurrent
  callers and 100 publications remain, and exception cleanup is exercised.
- Configuration now builds and executes a minimal ORC host against the selected
  SDK. Testing the exact Linux archive found an additional undefined LLVM RTTI
  symbol: that SDK has `LLVM_ENABLE_RTTI=OFF`. The backend and probe now match
  this setting while retaining exceptions, C++20 and other targets' RTTI.
- CI preserves CMake configure diagnostics as well as CTest logs on failure.

### Local evidence

The installed `/usr/lib/llvm-19` SDK, Clang and LLD are 19.1.7. After the linker,
DIA and thread changes: Debug 6/6 passed (0.55 s), Release 6/6 (0.54 s), and
RelWithDebInfo ASan/UBSan 6/6 (1.41 s), with leak detection enabled outside the
sandbox tracing restriction. Ten repetitions each of reload/GC/native passed
(30 suite executions, 3.56 s). These SDK libraries are native ELF archives.

A synthetic imported LLVM target on Linux reproduced a stale DIA path. The
resolver replaced it with an existing compiled fixture library, and an executable
linked and called that fixture successfully. This checks CMake property repair,
not the Windows SDK, architecture or ABI; Windows hosted execution is pending.

Downloaded and extracted the exact official `LLVM-19.1.7-Linux-X64.tar.xz` used
by CI into the ignored `.toolchains/ci-investigation/` directory. Its static
archive member magic is LLVM bitcode (`BC c0 de`). A configuration using this
archive and the default GNU linker failed with the same unrecognized-format
error (at `libLLVMCore.a` in the SDK probe). The matching archive's LLD passed
that error and exposed the RTTI mismatch; after matching `LLVM_ENABLE_RTTI`,
the configure-time ORC executable linked and ran successfully.

Reproduction for the official archive (substitute its extracted root):

```sh
void_sdk="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64"
cmake -S . -B build/ci-official-debug \
  -DLLVM_DIR="$void_sdk/lib/cmake/llvm" \
  -DCMAKE_C_COMPILER="$void_sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$void_sdk/bin/clang++" \
  -DVOID_CLANG="$void_sdk/bin/clang" \
  -DVOID_DWARFDUMP="$void_sdk/bin/llvm-dwarfdump" \
  -DVOID_LINKER="$void_sdk/bin/ld.lld" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-official-debug --parallel 3
ctest --test-dir build/ci-official-debug --output-on-failure
```

Linux CI installs `ninja-build libffi-dev libedit-dev libxml2-dev libzstd-dev
zlib1g-dev libtinfo-dev`; the complete SDK provides LLVM/Clang/LLD. Distribution
SDK users additionally install the exact `llvm-19-dev clang-19 lld-19` versions
and the SDK's exported dependencies (including `libz3-dev` on the verified host).
No required dependencies or architectural tests are removed.

### Final verification after the SDK ABI correction

| Local configuration/check | Result |
|---|---|
| Exact official Linux CI archive + matching Clang/LLD, Debug | Full build and 6/6 executable suites passed, 0.52 s |
| Exact archive standalone ORC harness | Printed `42` |
| Installed SDK + matching Clang/LLD, Debug, final CMake | Configure probe, build and 6/6 passed, 1.27 s |
| Installed SDK + matching Clang/LLD, Release, final CMake | Configure probe, build and 6/6 passed, 1.82 s |
| Installed SDK ASan/UBSan | 6/6 passed with leak detection, as recorded above |
| Installed SDK repeated Release reload/GC/native | All 30 executions passed, as recorded above |
| SDK helper syntax and whitespace checks | Passed |

The final installed-SDK checks ran while the archive's LTO links were active;
these timings are observations, not performance comparisons. The official SDK
configure probe took 85 seconds and full executable linking took several minutes.
Only Debug was rebuilt locally against the exact archive; the hosted gate must
also confirm that archive's Release and sanitizer configurations. The distribution
SDK's Debug/Release/sanitizer results do not replace that hosted verification.

### Hosted gate still pending

After the user commits/pushes these changes, run all three jobs again. Required
results are Debug and Release builds plus all six executable suites on each
platform, and Linux ASan/UBSan. Specifically verify Windows DIA discovery,
release CRT/iterator ABI, native C/AOT/debug metadata and JIT execution; verify
macOS portable thread cleanup and native ORC/AOT execution with the Xcode SDK.
Linux runner memory, LTO link time and system dependency compatibility also need
hosted confirmation. No hosted green result is claimed from local tests.

## Windows AOT library discovery: second hosted run

Inspected [run 38099707063](https://github.com/Shmellyorc/Void.Language/actions/runs/38099707063),
commit `dc158ccc488436eb08044566ecd71cd68bd67b07`, including the actual logs of
Windows job `114352920880`, Linux job `114352921025` and macOS job `114352921035`.

| Hosted result | Evidence |
|---|---|
| Linux Debug / Release / ASan+UBSan | All 6/6 passed in each configuration (0.54 / 0.39 / 1.16 s) |
| macOS Debug / Release | All 6/6 passed in each configuration (0.85 / 0.78 s) |
| Windows Debug build | Passed, including DIA resolution and the executable SDK probe |
| Windows Debug tests | core/JIT/reload/GC/native passed; AOT failed before invoking the native linker |
| Windows Release | Configure, build and tests skipped after Debug AOT failure |

The Windows test emitted its object, then Clang reported:
`no such file or directory: 'legacy_stdio_definitions.lib'`. The job's `LIB`
already contained MSVC `14.44.35207/lib/x64`, UCRT and Windows SDK x64 library
directories. This was a driver argument error, not evidence of an absent SDK.
The GNU-style Clang driver validates a bare `.lib` as a local input file before
native linker library discovery can run.

Changed only the Windows library argument in `tests/integration/aot.cmake` to
`-llegacy_stdio_definitions`. Clang translates this library request for the MSVC
target, and the native linker searches the active developer environment. No
hardcoded SDK paths, copied libraries, dropped dependencies or skipped tests.
The direct generated-code `printf` call and the executable outputs `7`, `42`,
`43`, paths containing spaces and object debug checks remain intact. LLVM is
still exactly 19.1.7. The existing workflow already establishes the MSVC/Windows
SDK environment; it needs no change for this correction.

### Local verification

- Clang 19.1.7 targeting `x86_64-pc-windows-msvc` reproduced the bare-library
  diagnostic. With `-### -llegacy_stdio_definitions`, it instead produced a
  Windows linker invocation containing `legacy_stdio_definitions.lib`, with no
  input-file error.
- Compiled two small C sources into real x64 COFF objects, archived one as
  `sdk_probe.lib`, and linked a PE executable using Clang/LLD 19.1.7 with
  `-lsdk_probe` and `LIB` pointing to a temporary directory containing spaces.
  Removing that `LIB` path made linking fail for the required library. This
  checks library discovery and missing-dependency behavior, not Windows runtime
  execution. This separate fixture does not replace or modify the AOT proof.
- Linux Debug against the exact official CI archive: 6/6 suites passed, 0.52 s.
- Linux Release against the installed LLVM 19.1.7 SDK: 6/6 passed, 0.50 s.
  Both include actual native AOT linking, debug verification and execution.
- Whitespace/diff checks passed. The change is an integration script argument;
  the local compiled sources are unchanged and needed no rebuild.

Driver-check reproduction, using an existing object:

```sh
clang-19 --target=x86_64-pc-windows-msvc -fuse-ld=lld -### \
  proof.obj legacy_stdio_definitions.lib
# Reproduces the local input-file error when no such file exists in this directory.
clang-19 --target=x86_64-pc-windows-msvc -fuse-ld=lld -### \
  proof.obj -llegacy_stdio_definitions
# Forwards the required .lib to the Windows linker instead.
```

### Remaining hosted verification

A new run after the user commits/pushes must verify Windows Debug AOT discovery
of the actual MSVC compatibility library, native `printf` resolution, debug
metadata and all three executable outputs. Windows Release configure/build and
all six suites must then execute successfully for the first time. Linux/macOS
should continue passing the full matrix. Local cross-target driver/linker tests
cannot establish Windows SDK availability or native execution success.

No commits, pushes, hosted reruns or other remote mutations were performed. The
pre-existing user edit to `docs/LANGUAGE_BIBLE.md` was left untouched.
