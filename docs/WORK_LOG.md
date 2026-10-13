# Work log

## Phase 15 run #25 CI timeout adjustment, 12 October 2026

Owner-reported hosted evidence: run #25 passed Windows and macOS completely.
Ubuntu passed Debug and Release, 31/31 suites each, but was cancelled during
sanitizer linking when the build job reached its 60-minute timeout. This leaves
that run's Ubuntu sanitizer execution and subsequent steps unverified; it is
not evidence of a sanitizer test failure.

Changed only the build job's `timeout-minutes` from 60 to 90 in
`.github/workflows/ci.yml`. The platform matrix, LLVM 19.1.7 pin, build commands,
tests, assertions, strict ASan/UBSan options and leak detection remain unchanged.
Validated that the workflow differs from HEAD by exactly this one replacement,
checked the parsed job timeout, and ran `git diff --check`. No compiler/runtime
changes or native rebuilds were needed for this CI-only adjustment. The next
hosted run must confirm Ubuntu completes sanitizers and the remaining steps
within the extended timeout. No commits, pushes or remote changes were made.

## Phase 12 run #17 Windows frontend crash correction, 12 October 2026

Read the actual [Windows job 114735091143](https://github.com/Shmellyorc/Void.Language/actions/runs/38226468142/job/114735091143)
at `f9d9f3bf68828c9e46671e157cb165fa46407651` and downloaded its
`tests-Windows-X64` artifact, ID `11736648542` (ZIP SHA256
`d472dfed34e714cc50b46adcde408cb0fcdc5f62a96a79a79be39f58af7eaae5`).
Windows Server 2022 used Clang/clang-cl 19.1.7, the official SDK and matching
RTTI OFF. Debug passed 21/22, including `value_structs_cli`; `frontend` ended
with CTest's generic SegFault classification after 0.86 seconds. Its archived
LastTest.log has **empty frontend output**. The artifact contains only that
test log and CMakeConfigureLog.yaml: no executable, exception code or crash dump.
Consequently that artifact alone cannot establish the failing subgroup or
distinguish Windows stack overflow from an access violation.

Before changing production code, the unchanged local Debug frontend reproduced
SIGSEGV (exit 139) with a 1 MiB process stack. GDB located it at entry to
`Semantics::expression_impl`, compiler.cpp:772 in that commit, recursively
called from the unary handler at line 879. The originating test is
`diagnostic_acceptance::run`, tests/frontend/diagnostics.hpp:141: nine constants,
each with forty unary operators, referencing the next constant. It expects
the combined semantic depth guard and an independent later-function diagnostic.
It is reached **before** `struct_tests`; the new bounded recursive/expanded
value-layout case therefore does not cause this reproduced crash.

Disassembly of the unmodified Clang 19 Debug object shows an 8,416-byte
`expression_impl` stack allocation (`sub rsp, 0x20e0`). Phase 12's construction,
field and expanded Type handling enlarged this single function's collection of
temporaries. Every recursive unary step reserved that whole frame, exhausting
the stack before the existing 256-level guard. This is a production resource
safety defect, not a reason to remove a depth test. The debugger's unreadable
arguments at the fault are at the exhausted function prologue, not evidence
that a null Semantics object was deliberately called.

Extracted literal, name, field, call, conversion, unary and binary checking into
small member handlers behind the same dispatcher, semantic model and guard.
No typing/evaluation order/diagnostic logic changed; a comparison against HEAD
confirmed all six extracted branch bodies match apart from whitespace and local
type aliases. The dispatch frame now reserves 288 bytes; the unary handler
reserves 496 bytes. The formerly crashing complete frontend suite now passes
on a 1 MiB stack with **both** installed and official SDK Debug builds, including
the combined-depth check and bounded expanded-layout check. No stack reserve,
resource limit, LLVM version, RTTI policy, sanitizer flag or assertion changed.

Added flushed stderr begin/pass records for each frontend group and markers for
the combined-depth and expanded-layout cases. Future Windows artifacts will
identify the last active group/case even on abrupt termination. Linux CI now
also repeats its existing Debug frontend suite under `ulimit -s 1024`; all 22
normal suites and platform jobs remain intact.

Verification (all builds completed without warnings):

| LLVM 19.1.7 SDK | Debug | Release | RelWithDebInfo ASan/UBSan |
| --- | --- | --- | --- |
| Installed `/usr/lib/llvm-19`, RTTI ON | 22/22, 24.74s | 22/22, 28.33s | 22/22, 40.72s |
| Complete official Linux archive, RTTI OFF | 22/22, 17.19s | 22/22, 24.64s | 22/22, 34.82s |

These are 132 suite executions, covering native/JIT/AOT normal and optimized
paths, value structs, GC/reload, diagnostics, shared analysis/tooling and LSP.
Both SDKs retain their headers, `libLLVMOrcJIT.a` and `LLVMConfig.cmake`;
llvm-config reports 19.1.7. Fresh configure runs linked and executed the real
ORC probe successfully against both. Installed-SDK sanitizers retain RTTI/vptr
coverage; the official no-RTTI SDK preserves the previously documented ABI
correction and coverage boundary.

Commands (from repository root; each tree was rebuilt with `-j 2`):

```sh
cmake --build build/ci-debug -j 2
ctest --test-dir build/ci-debug --output-on-failure -j 4
# Repeat for ci-release, ci-official-debug and ci-official-release.
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-sanitizers --output-on-failure -j 4
# Same strict environment for ci-official-san, outside the tracing sandbox.
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-debug -R "^frontend$" -V'
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-official-debug -R "^frontend$" -V'
cmake -S . -B build/ci-debug
cmake -S . -B build/ci-official-debug
git diff --check
```

The pre-fix reproduction used `ulimit -s 1024` followed by
`gdb -batch -ex run -ex bt --args build/ci-debug/void_tests frontend "$PWD"`.
Artifacts, original Windows job log/ZIP, full pre-fix backtrace, fresh configure
logs, six build/test logs, SDK checks and verbose small-stack results are saved
under `build/phase12-windows-investigation/` (ignored local evidence).

Windows attribution is strongly supported by the matching small-stack failure,
Debug-specific oversized frame and successful correction, but **is not a
Windows-native backtrace or verified Windows success**. This machine lacks the
Windows SDK/MSVC libraries and the Windows executable was not uploaded. Linux
small-stack execution does not reproduce the MSVC standard-library ABI. Another
hosted run must confirm Windows Debug/Release 22/22 and the remaining matrix
and editor steps. The added progress output will localize any remaining failure.
Ready for the owner's next CI attempt; no commits, branches, pushes or remote
modifications were made. The owner's untracked WRENCH_CLANKER.md was preserved.

## Phase 5 run #7 sanitizer ABI correction, 11 October 2026

Inspected the actual Ubuntu job `114414960372` in run `38120807013`, at commit
`384307dcdb65b476de587c4681e047b9f7dc3acc`, also the local unchanged baseline.
Windows/macOS and Ubuntu Debug/Release passed according to the user and job log;
Ubuntu sanitizers failed eight of ten suites with invalid-vptr reports in ORC
promise/future cleanup and shared code ownership cleanup.

Before editing, configured, built and ran the complete official Linux 19.1.7
archive with its own matching Clang/LLD and CI's RelWithDebInfo/ASan/UBSan settings.
The unchanged full suite reproduced the same eight failures: **2/10 passed in
2.42 seconds**. The host uses GCC 16 libstdc++ headers; CI uses GCC 14. Prior local
sanitizer validation used the installed RTTI-enabled SDK, whereas the official
archive has RTTI disabled. Earlier official-archive Debug validation did not
exercise this sanitizer/RTTI combination.

The archive's actual `LLJIT.cpp.o` bitcode contains a promise-result vtable with
null RTTI metadata. A separate two-file shared_ptr factory/consumer reproducer
fails cleanup with exit 86 when only the factory disables RTTI; consistent RTTI
passes with leak detection. Inspecting the emitted factory vtable confirms the
same absent metadata without relying on a corrupt allocation. CI_SANITIZER_ABI.md
records the mechanism, actual log link, commands and precise coverage limits.

Moved SDK RTTI selection from the backend-only target to shared C++ build options
for all linked targets, in every build type. The SDK configure probe uses the same
flag. The configure output discloses the no-RTTI ABI's unavailable vptr check.
LLVM 19.1.7, complete SDKs, dependencies, exceptions, sanitizer flags, strict
failure settings, entry function-type metadata and all ten tests are preserved.
No frontend, runtime implementation, IR lowering, test assertion or workflow
was changed. Separate installed-SDK validation retains RTTI/vptr coverage; no
equivalence of vptr coverage with the no-RTTI archive is claimed.

Completed local verification:

| SDK / configuration | Result | Full-suite elapsed |
|---|---|---|
| Installed 19.1.7, RTTI ON, Debug | 10/10 | 27.68 s |
| Installed 19.1.7, RTTI ON, Release | 10/10 | 25.35 s |
| Installed 19.1.7, RTTI ON, strict ASan/UBSan | 10/10 | 25.15 s |
| Complete official archive, RTTI OFF, Debug | 10/10 | 10.78 s |
| Complete official archive, RTTI OFF, Release | 10/10 | 9.01 s |
| Complete official archive, RTTI OFF, strict ASan/UBSan | 10/10 | 29.51 s |
| Complete official archive, strict sanitizer repeat, all suites three times | 30/30 executions | 89.26 s |

All 14 C++ translation units in each sanitizer configuration were checked against
their SDK RTTI setting; every C/C++ source command retains ASan/UBSan. The installed
CLI object retains dynamic-type/vptr handlers and the archive-based runtime object
retains indirect-function-type handlers. Seven deliberate fault controls verified
fatal ASan heap overflow, UBSan signed overflow and incorrect function calls in
both modes, plus an invalid downcast with RTTI. Combined runtime options yield
exit 86 for these controls, including the address report; this remains distinct
from expected source-error exit 1. Sanitizers ran outside the tracing sandbox
with leak detection enabled. Build/test/reproducer/control evidence is retained
under ignored `build/ci7-verification/`.

All six configurations completed actual builds and the full ten-suite execution.
The complete-archive builds were serial and produced no new linker backtrace;
positive build/test logs contain no ASan/UBSan/LeakSanitizer report. The existing
third-party `<ciso646>` deprecation warning remains visible where emitted.
A new hosted run is still required for Ubuntu 24.04/GCC 14 and the changed shared
C++ flags on Windows/macOS. A hosted vptr lane would require a complete
RTTI-enabled LLVM 19.1.7 SDK. No commit, push or remote modification was performed.

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


## First real source compiler — 10 October 2026

### Scope and foundation

The user confirmed all three GitHub Actions platforms passed the previous
foundation before authorizing this cohesive compiler phase. Read the current
Language Bible, strategy, decisions and known issues, and preserve those design
sources. The Language Bible was not edited. No commits, pushes, hosted reruns or
other remote changes were made.

Actual files now pass through UTF-8 source handling, indentation tokenization,
AST parsing, signature/binding/type/control-flow analysis and lowering to the
existing backend-independent IR. Shared LLVM lowering generates both ORC code
and native objects. The implementation supports multiple typed parameters,
i32/bool, recursive/forward calls, arithmetic/comparison/short-circuit Boolean
expressions, mutable/default-initialized locals, assignment/compound/postfix
updates, return, nested conditionals, conditional/unbounded for and nearest-loop
break/continue. It diagnoses while with the correct for replacement.

Extend IR signatures, typed slots and jumps without changing the old proof
contracts. Preserve all six architectural suites and their assertions, including
native printf, generated roots under optimization, concurrent replacement and
safe retirement. The new frontend suite shares the existing test runner, avoiding
another expensive static LLVM link in CI. Native-link script setup is shared by
the old AOT proof and new source integration test; required Windows libraries
remain intact. The existing workflow automatically executes all eight suites.

Documented provisional choices include indentation/identifier scope, expression
precedence, flow analysis, left-to-right evaluation and initial wrapping i32
arithmetic. No broader language grammar or numeric policy is silently finalized.
See SOURCE_COMPILER.md and DECISIONS.md for details and remaining questions.

### Actual source acceptance

`examples/architecture.void` is read by the compiler, not reconstructed as proof
IR. Both normal and O2 JIT/AOT execution produced:

| Input | Source ORC JIT | Linked native AOT |
| --- | --- | --- |
| -1 | 7 | 7 |
| 0 | 42 | 42 |
| 1 | 43 | 43 |
| 10 | 52 | 52 |

The source integration suite compiles three real files (`architecture`, `loops`,
`functions`) with and without O2, verifies actual source filenames in valid DWARF,
links native executables and compares 56 JIT/AOT results. It uses source/output
paths with spaces and rejects malformed/out-of-range native CLI arguments.
An edited copy changes 42 to 13 and renames calculate to main; both engines return
23 for input 10, checking real source behavior and native adapter symbol isolation.
Thirteen additional independent programs cover all arithmetic, bool types,
short circuiting around dangerous dynamic division, multi-argument short-circuit
CFGs, recursive/mutual calls, loop nesting/exits and local reinitialization.

Over fifty diagnostic/boundary checks cover indentation/tabs, unexpected tokens,
unresolved names, duplicates/shadowing, argument counts, exact types, invalid and
missing returns, loop misuse, unsupported iteration/numbers, overflow literals,
zero literals, malformed UTF-8 and source locations/carets. BOM, Unicode comments,
scalar columns, CRLF/CR and missing EOF newline receive positive checks. The JIT
host rejects incompatible entry ABIs before invoking generated code.

### Sanitizer finding and correction

The expanded executions exposed a Clang UBSan indirect-call metadata read before
a page-aligned ORC entry. This was an actual JIT host boundary fault under ASan.
The fix obtains the i32(i32) function metadata from the actual host compiler at
configuration and attaches LLVM func_sanitize metadata before ORC code generation.
No sanitizer check or architectural assertion is suppressed. Application-generated
instructions remain outside sanitizer instrumentation, as in the original proofs;
the compiler/runtime, native fixture and host boundary retain instrumentation.

LeakSanitizer requires process inspection unavailable under this desktop's tracing
sandbox. Sanitizer CTest executions ran outside that sandbox with leak detection
and all checks enabled.

### Reproduction and local verification

The installed SDK is `/usr/lib/llvm-19/lib/cmake/llvm`, with Clang and LLD 19.1.7.
The required Linux packages remain documented in README; official-archive CI
installs ninja-build, libffi-dev, libedit-dev, libxml2-dev, libzstd-dev, zlib1g-dev
and libtinfo-dev. SDK-specific development dependencies must remain installed.

```sh
cmake -S . -B build/ci-debug \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure

cmake -S . -B build/ci-release \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld -DCMAKE_BUILD_TYPE=Release
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure

cmake -S . -B build/ci-sanitizers \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOID_SANITIZERS=ON
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure --repeat until-fail:5
```

Debug: 8/8 passed (3.19 s). Release: 8/8 passed (1.91 s).
ASan/UBSan: all eight suites passed five consecutive executions after the final
changes (40 suite executions, 65.56 s while the official SDK linked concurrently).
ASan, UBSan function checks and leak detection remained enabled.

The complete official Linux CI archive is also configured locally:

```sh
cmake -S . -B build/ci-official-debug \
  -DLLVM_DIR="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/lib/cmake/llvm" \
  -DCMAKE_C_COMPILER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/clang" \
  -DCMAKE_CXX_COMPILER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/clang++" \
  -DVOID_LINKER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/ld.lld" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-official-debug -j3
ctest --test-dir build/ci-official-debug --output-on-failure
```

The official SDK configuration links/runs the ORC probe, uses its bitcode archives
through matching LLD, and matches its disabled RTTI ABI. The final rebuild passed
all eight suites (1.70 s), including native source compilation/link/execution and
all preserved architectural regressions. Whitespace checks and local documentation
links passed. The source entry ABI diagnostic was also checked manually at its
function-name location (line 1, column 6).

Native source demonstration (after the integration tests create the executable):

```sh
build/ci-debug/voidc run examples/architecture.void 10
# 52
"build/ci-debug/source programs with spaces/architecture-optimized" 10
# 52
```

### Remaining verification and next phase

The expanded Windows/macOS Debug/Release and hosted Linux sanitizer runs remain
unverified until the user commits/pushes and runs Actions. The prior foundation
passes do not establish these new frontend results on those hosts. Keep LLVM
19.1.7 pinned, the required SDK libraries and every architectural suite intact.

Next, after that hosted gate: module/import resolution with stable identity and
multi-file diagnostics, deliberate numeric and executable/export conventions,
and diagnostic/runtime error improvements. Managed-language types must wait for
root/thread integration; structs, generics, collections and async remain deferred.


## Module system and multi-file compilation — 10 October 2026

### Starting point and delivered architecture

The user confirmed the first real source compiler passed Linux/Windows/macOS
Actions Debug/Release and Linux sanitizers. Started from the clean existing local
project, read the Bible/strategy/source/compiler decision and issue records, and
kept LLVM 19.1.7 and the original eight suites. No Language Bible edits, commits,
pushes, hosted reruns or other remote mutations were made.

The CLI now calls compile_project for check/run/emit. A source graph establishes
its root at the entry file's parent, normalizes VFS imports, retains one source
snapshot per canonical module, parses the reachable graph and registers every
function signature before checking bodies. The in-memory source API shares the
same graph/type/lowering implementation, with an explicit diagnostic for imports
without a filesystem project context. One validated IR compilation unit feeds the
existing shared LLVM JIT/AOT implementation.

Implemented root-relative, importer-relative and aliased use declarations;
canonical deduplication; deterministic simple/qualified call resolution; default
public, explicit public/internal and file-private functions; function-only cycles
and cross-file recursion. Source function ownership carries the VFS module key
and source name into IR; native symbols encode those keys injectively. LLVM debug
names remain friendly, with imported source filenames/locations retained. The
entry adapter selects only the entry file's own function, never an imported entry.
Final review separated the physical compilation-unit debug path from virtual
module identity; the native suite reads DW_AT_comp_dir and verifies the actual
source directory rather than merely matching a filename in line tables.

Documented deliberate initial policies in MODULE_SYSTEM.md and DECISIONS.md:
exact-case portable filenames, space-containing paths, dot/separator normalization,
no root escapes/packages, and rejection of imported symlinks/redirects or hard-link
aliases. Project-scoped identities survive physical relocation and alias changes;
persistent identities for independent projects/external libraries remain deferred.
Explicit aliases expose qualified functions only; direct unaliased imports expose
simple names and optional filename-stem qualification. Ambiguous simple calls
require qualification/aliasing, and implicit qualifier ambiguity is diagnosed only
when used. Repeated identical imports are idempotent, not duplicate declarations.

No namespace hierarchy, folder-based visibility, global initialization or second
compiler/backend path was introduced. Existing arithmetic, Boolean short circuit,
control-flow/for rules and architectural GC/native/reload proofs remain intact.

### Actual multi-file source acceptance

Created the requested real source files:

```text
examples/modules/main.void
examples/modules/math/calculator.void
examples/modules/helpers/numbers.void
```

The unchanged acceptance program calls calculator.double(value) and
increment(value) from their declaring files. Normal and O2 source JIT/native AOT
results matched:

| Input | Multi-file ORC JIT | Linked native AOT |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | 4 | 4 |
| 10 | 31 | 31 |
| -1 | -2 | -2 |

The module native integration suite copies that actual project into paths with
spaces, checks/emits/links/verifies DWARF, and executes it from an unrelated cwd.
It verifies line tables for main.void, calculator.void and numbers.void. Independent
nested and cyclic projects run through the same normal/O2 JIT/AOT sequence.
Changing only calculator.void from multiplication by 2 to multiplication by 3
changes results to 1, 5, 41 and -3 for the same inputs in both engines. This is
actual imported-source compilation, not reconstructed IR or a resolver-only proof.
The four graph variants provide 64 native result comparisons, plus relative-entry
cwd and imported-error diagnostic checks.

### New tests and preserved regressions

`modules` and `modules_cli` bring CTest to ten suites; the original eight names,
assertions and architectural behaviors remain. Both suites are part of every
existing workflow configuration; no workflow or LLVM dependency was weakened.

The module runner executes twelve positive graph scenarios with normal/O2 ORC:
nested/relative/normalized/repeated imports, multiple aliases to one owner,
identical stems with distinct simple functions, same-name functions in different
files, private helpers, public/internal visibility across folders, mutual recursion,
self-imports, local declaration precedence, for mutation, space-containing paths,
multiple typed arguments/short-circuiting and imported entry isolation. Additional
relocation/replacement checks preserve the compatibility key and old native leases.

More than forty negative graph cases check missing files, invalid paths/root
escapes, packages/OS drives, aliases/ambiguity, private access, counts/types,
non-transitive exports, duplicate declarations, imported semantic/syntax/UTF-8
errors, directories, exact filename case, case collisions, symlinks and hard-link
aliases. Diagnostics assert the actual originating filename, line, column and
source caret, without LLVM/internal symbol noise. This Linux filesystem supported
real distinct case-only files, symlinks and hard links, so those negative fixtures
executed. On hosts unable to create those filesystem objects, the runner reports
the capability explicitly; normal source graph and exact-case tests still execute.

### Commands and local results

Retained build caches use Clang/LLD/LLVM 19.1.7. Installed SDK:
`/usr/lib/llvm-19/lib/cmake/llvm`. Fresh configuration commands and Linux development
packages remain in README and the previous phase's reproduction section.
Actual rebuild/test commands for this phase:

```sh
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
# Repeat module suites after the final debug-source-path correction.
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(modules|modules_cli)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j3
ctest --test-dir build/ci-official-debug --output-on-failure
```

Debug: 10/10 passed (3.79 s). Release: 10/10 passed (9.19 s, concurrent SDK builds).
An initial sanitizer run passed 10/10 (17.72 s); modules/module-native/reload/GC/
native then passed five consecutive runs each (25 suite executions, 66.93 s).
After the final debug-source-path correction, ASan/UBSan passed 10/10 (7.16 s);
both module suites passed five consecutive runs each (10 suite executions, 15.97 s).
The final complete official LLVM SDK Debug build passed 10/10 (2.80 s), including
the physical compilation-unit directory check, native graph execution and all
architectural regressions. Whitespace and documentation-link checks passed.
Sanitizer CTest runs outside this desktop's tracing sandbox so LeakSanitizer can
inspect processes. ASan, UBSan function checks and leak detection stay enabled.
The complete official CI SDK build uses its own Clang/LLD 19.1.7 and bitcode
libraries, preserving its RTTI ABI and the executable ORC configuration probe.

For a standalone native demonstration using the built compiler:

```sh
build/ci-debug/voidc run examples/modules/main.void 10
# 31
build/ci-debug/voidc emit examples/modules/main.void -o build/ci-debug/modules-demo.o
clang-19 -fuse-ld=/usr/lib/llvm-19/bin/ld.lld \
  build/ci-debug/modules-demo.o -o build/ci-debug/modules-demo
build/ci-debug/modules-demo 10
# 31
```

### Remaining hosted verification and next phase

Windows/macOS Debug/Release and hosted Linux ASan/UBSan execution of the ten-suite
matrix remain unverified until the user commits/pushes and runs Actions. The prior
eight-suite hosted success does not establish these new changes. Check actual
Windows SDK linking, cross-platform owned native symbols, exact-case handling,
filesystem capability reporting and multi-file DWARF on those runners.

Next, after that gate: const/project-root compile-time constants, precise numeric
policy/conversions and source-first runtime diagnostics, with deliberate constant
dependency/initialization semantics. Project/export configuration and diagnostic
recovery can build on these identities. Packages and external imports need explicit
identity/permission contracts; managed types still require the GC root/thread gate.

## Constants, numeric correctness and runtime diagnostics, 10 October 2026

The user reports the preceding ten-suite module phase passed GitHub Actions on
Linux, Windows and macOS. That is the starting gate for this phase, not hosted
verification of the following local changes. No commits, pushes, publications or
remote mutations were made. LLVM remains exactly 19.1.7; the Bible is unchanged.

### Implementation and architecture

Added i32/bool local readonly const with runtime initializers and module-level
pure compile-time constants, optional exact annotations, safe forward references,
dependency-cycle diagnostics, direct/qualified import lookup, visibility and
project-root constant access without importing the entry. Constants/functions share
file declaration ownership. Root-name collisions and local shadowing diagnose
before LLVM. Assignments, compounds and increment/decrement reject readonly values.

The existing expression type checker annotates constant initializers. A small
pure evaluator shares operation-to-IR definitions, short-circuits execution and
uses unsigned wrapping arithmetic plus widened signed division. Every declaration
and expression operand remains validated; structural dependency cycles and impure
initializers fail even behind lazy operators. No runtime module initialization,
second type checker, wider numeric types or managed readonly system was added.
Unary negation of an already valid negative boundary literal now lowers as an
operation instead of constructing an out-of-range positive literal.

Source IR retains source locations and relevant source-line/caret snapshots.
Shared LLVM lowering supplies a hidden per-invocation failure pointer to source
functions. Division/remainder by zero record a source diagnostic and return
normally; actual callers propagate failure and append their source call sites.
The trace includes at most 32 callers and explicitly reports omitted callers.
The JIT copies generation-owned strings and raises a host exception only after
native return. AOT's native main prints the same diagnostic metadata and exits 1.
There is no C++ unwind through generated code, TLS/global error state or foreign
stack guessing. Const-containing source snapshots form an exact conservative
restart compatibility token, including imported files and local runtime const.

VALUE_SEMANTICS.md documents lookup/cycle rules, current arithmetic, the boundary,
AOT stdout/JIT stderr difference, conservative restart restrictions, unimplemented
exceptions/cleanup and unapproved future literal/conversion recommendations.
SOURCE_COMPILER, MODULE_SYSTEM, DECISIONS, KNOWN_ISSUES, strategy and README were
updated without replacing the source-of-truth design snapshots.

### Actual native acceptance results

Compiled the real examples/constants/main.void and game/scoring.void using forward
root constants and runtime local const. No special-case IR was reconstructed.
Both normal and O2 compilation produced these results in both native engines:

| Input | JIT | Linked AOT executable |
|---:|---:|---:|
| 0 | 50 | 50 |
| 1 | 52 | 52 |
| 10 | 70 | 70 |
| -1 | 48 | 48 |

The standalone demo emitted `build/ci-debug/values-demo-normal.o` and
`values-demo-O2.o`, linked them with matching Clang/LLD 19.1.7, and ran all inputs.
The runtime-error example's JIT and linked `values-demo-error` both exited 1 with:

```text
.../examples/runtime_errors/arithmetic.void:2:16: runtime error: Division by zero
    return 100 / value
               ^
In function /arithmetic.void::calculate
Called from /main.void::bridge
.../examples/runtime_errors/main.void:4:23: source call
    return arithmetic.calculate(value)
                      ^
Called from /main.void::entry
.../examples/runtime_errors/main.void:7:12: source call
    return bridge(value)
           ^
```

### Expanded tests, with all ten suites preserved

Existing core/JIT/reload/GC/native/AOT assertions and frontend/module tests remain.
Extended frontend, modules and modules_cli rather than disabling or replacing any
suite. New tests exercise readonly mutations, initialization/types, forward and
cyclic constants, root/private/internal/direct/aliased lookup, name collisions,
function-only cycles with safe constants and restart rejection for root/imported
local const edits. Concurrent failure tests use four callers, with repeated failing
and successful invocations to verify per-call isolation.

An independent widened-integer oracle covers eight boundary/signed operands per
side and five operators, excluding zero divisors for division/remainder: 304 cases.
Frontend tests make 1,216 comparisons against constant and normal/O2 JIT results.
A generated real `.void` matrix tests all 304 against native runtime arithmetic and
constant values in normal/O2 JIT and linked AOT. Real numeric.void independently
checks compounds, negation, MIN/-1, wraparound, signed division/remainder and lazy
constant arithmetic. Real-source native integration covers division, remainder,
both compounds, bool/no-result propagation, recursive trace truncation and imported
caller positions. AOT tests edit the source after object emission and still require
the original snapshot. Unexecuted branches and and/or expressions remain safe.

### Commands and local verification

Both installed and complete official SDKs were checked for LLJIT.h,
libLLVMOrcJIT.a and LLVMConfig.cmake. Both configuration link/run probes passed.
Installed SDK: `/usr/lib/llvm-19/lib/cmake/llvm`. Complete archive:
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`, using its own matching
Clang/LLD and RTTI settings. Required dependency and platform setup remains intact.

```sh
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(frontend|modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j3
# Final verification uses serial linking after the observation below.
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
```

Final installed-SDK Debug: 10/10 passed (28.07 s while SDK builds ran).
Final Release: 10/10 passed (10.87 s). Final ASan/UBSan: 10/10 passed (25.77 s).
ASan, UBSan including both plain/checked entry ABI function checks, and leak detection
remain enabled. Sanitizer CTest runs outside the desktop tracing sandbox for
LeakSanitizer process inspection; generated application instructions and LLVM
itself are not sanitizer-instrumented. Earlier intermediate runs also passed;
a new module diagnostic assertion initially expected alias column 23 instead of
the actual 18 and was corrected to the source position.

Final repeated ASan/UBSan stress: all six selected suites passed five consecutive
runs each, 30 suite executions (192.58 s, concurrent official LLVM bitcode linking).
An intermediate concurrent official-SDK LLD link emitted a Machine Instruction
Scheduler backtrace in LLVMGlobalISel despite reporting build exit status 0.
A final serial build with the same pinned SDK completed cleanly, without another
backtrace. Its cause remains unconfirmed; compare the next hosted Linux link. No libraries, LLVM version or build requirements were substituted.
After final diagnostic wording corrections, Debug again passed 10/10 (17.67 s),
Release 10/10 (16.41 s), and ASan/UBSan 10/10 (27.01 s), with concurrent SDK linking.
Retained logs contain no AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer
or runtime-error warning lines. Final complete official SDK Debug: 10/10 passed (9.58 s), including all
architectural regressions, the numeric oracle and normal/O2 native diagnostics.
Its final serial build and retained test log contain no linker backtrace or
sanitizer reports. Whitespace and local documentation-link checks passed.

### Hosted verification and next phase

This phase needs a new user-published GitHub Actions run: Windows/macOS Debug and
Release, hosted Linux Debug/Release/ASan/UBSan, especially checked source entry ABI,
CRT diagnostic printing, optimized propagation/call traces and the full numeric
oracle. No current hosted success is claimed. CI still uses the complete pinned
19.1.7 SDK and preserved Linux LLD, Windows SDK/DIA/legacy stdio and macOS settings.

Next improve structured diagnostics/recovery and precise source dependency
compatibility, then approve contextual literal/conversion tables before wider
numeric types. Managed types remain behind the separate GC root/thread gate.

## Phase 5: Structured diagnostics and compiler recovery, 10–11 October 2026

### Baseline and scope

The user reports Phase 4 commit `d78b6b8`, GitHub Actions run #6, passed Linux,
Windows and macOS. Read the current Bible/Strategy and source/module/value,
decision, issue and work-log documentation. The Bible's unified-diagnostic IDs
are explicitly illustrative; Phase 5 now assigns 34 stable compiler categories.
No language grammar, LLVM version, runtime failure ABI, const restart contract,
workflow requirements or existing architectural assertions were relaxed.
All work remains local: no branch, commit, push, publication or remote change.

### Implemented and audited

- One Diagnostic/SourceSpan/RelatedLocation representation with severity, assigned
  codes, description, owned original snapshot, physical path/VFS owner, half-open
  byte ranges, related declarations, expected/received types and hints.
  CompilationError preserves invalid_argument compatibility and exposes structured
  records. SourceFile copies now share immutable bytes/line indexes.
- Numbered compiler snippets, scalar-counted ranges, related source snippets and
  code/type/hint rendering. Runtime excerpts and generated checked-entry ABI remain
  unchanged. DIAGNOSTICS.md documents all assigned codes, byte/column rules and
  display-width/multiline/long-line limits.
- Lexer line recovery and indentation-aware parser statement/declaration recovery,
  permanent EOF sentinel and explicit progress/recursion/size guards. Malformed
  named declarations leave invalid bindings rather than fabricated values.
- Semantic error accumulation across statements/functions/constants/modules,
  AST-only poisoned types, failed constant states and poisoned duplicate names.
  Failed imports continue unrelated discovery; uncertain dependent lookups are
  conservatively suppressed. Stable VFS/span/code ordering, equivalent-record
  deduplication, 100-record budget plus a notice and bounded registry/analysis.
- A complete validation gate before AST-to-IR lowering. Invalid check/run/emit
  returns 1 with no native execution, new object or overwritten existing output.
  Valid source still uses the original IR and shared LLVM implementation.
- Failed edits retain active/leased native generations. Exact conservative const
  tokens are unchanged; changed valid const generations still require restart.
  Tests verify an old native division failure retains its original source line
  after an invalid edit, and compile-time snapshots survive file changes/deletion.

Development checks caught and corrected lexer-line recovery producing a secondary
return mismatch, EOF recovery producing a secondary missing-return error, and a
malformed entry UTF-8 snapshot being confused with an empty file-opening anchor.
Snapshot interning now checks that the bytes agree before substituting an owner.
A recursion audit added one semantic depth budget shared across constant analysis,
preventing nested expressions from multiplying separate per-constant limits.
Duplicate declarations/parameters no longer select an arbitrary type for dependent
uses. Failed local calls and implicit alias conflicts preserve poison state, and
related declaration notes are deduplicated even through repeated imports. A recursion fixture originally contained postfix `--` tokens; it now uses
spaced unary minus to test the actual recursive grammar. No old assertion was removed.

### Acceptance and executable evidence

Checked-in real fixtures under `tests/fixtures/diagnostics/` cover eight independent
primary errors in one file, malformed statements between functions and errors in
multiple imported files, including a cross-file previous declaration. Generated
real `.void` fixtures cover malformed signatures/expressions/if-else/for/use,
indentation/tabs, EOF, UTF-8/BOM/LF/CRLF/CR/no-final-newline, spaces in paths,
long lines, scalar ranges, repeated imports, alias conflicts, duplicate-name
poisoning, constant cycles, 100-diagnostic exhaustion, deeply nested/large
expressions, combined expression/constant depth and a 130-file import chain.
Fifteen deterministic multi-file repeats and three repeats of every malformed
fragment assert stable order, termination and retention of later independent errors.

`source_cli` checks invalid check/run/emit in both normal and O2 modes, verifies
empty success output and failure status, compares existing output SHA-256 before/
after failed emission, and requires that no fresh partial object appears. It then
compiles corrected source through both JIT and linked AOT in both modes. Existing
architecture, numeric oracle, runtime propagation, GC, reload and real native
printf proofs remain in the original ten suites.

Actual final demonstration:

```sh
build/ci-debug/voidc check examples/diagnostics/errors.void
# exit 1; all three in one invocation:
# errors.void:2:23 VOID2001 (bool/i32 mismatch)
# errors.void:4:5  VOID2007 (readonly bonus mutation)
# errors.void:5:12 VOID2003 (unknown missing function)
build/ci-debug/voidc run examples/diagnostics/corrected.void 10 --no-opt
# 52
build/ci-debug/voidc run examples/diagnostics/corrected.void 10
# 52
build/ci-debug/voidc emit examples/diagnostics/corrected.void -o build/ci-debug/diagnostic-demo.o
clang-19 --ld-path=/usr/lib/llvm-19/bin/ld.lld build/ci-debug/diagnostic-demo.o -o build/ci-debug/diagnostic-demo
build/ci-debug/diagnostic-demo 10
# 52
```

The multi-file check reports `/a.void` shadowing/unknown identifier, `/b.void`
return mismatch and `/main.void` missing import, with the original root declaration
as a related location. The failed qualifier adds no downstream unknown-call noise.
DIAGNOSTICS.md includes actual formatted snippets and the correction example.
CLI behavior remains: source/runtime failures 1, usage errors 2; invalid JIT input
retains its existing status 1, while the emitted native adapter rejects bad input
with status 2. Verified these directly; this phase does not change those exits.

An additional deterministic mutation run (seed 50019) wrote 300 malformed real
files, each with a later independent unknown identifier. Every compiler process
terminated within its three-second per-process timeout, returned 1, retained that
later diagnostic and exposed no LLVM errors. This is local robustness evidence,
not a claim of exhaustive fuzz coverage. Its log is retained with final build logs.

### Commands, exact toolchains and measured final results

Verified LLJIT.h, libLLVMOrcJIT.a and LLVMConfig.cmake in both the installed
`/usr/lib/llvm-19` SDK and complete official
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` SDK. Existing CMake configurations
retain exact 19.1.7, matching Clang/LLD and their SDK RTTI settings. No dependencies
or probes were removed. The official archive build links serially, preserving the
previous phase's unexplained concurrent-link observation for reference.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(frontend|modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
```

| Final configuration | Result | CTest wall time |
|---|---|---|
| Installed LLVM 19.1.7 Debug | 10/10 passed | 11.85 s |
| Installed LLVM 19.1.7 Release | 10/10 passed | 14.16 s |
| Installed LLVM 19.1.7 ASan/UBSan | 10/10 passed | 19.41 s |
| Complete official LLVM 19.1.7 SDK Debug, serial build | 10/10 passed | 9.82 s |
| ASan/UBSan repeated stress | Six suites × five runs, all 30 passed | 82.70 s |

Full-build timings vary with concurrent local SDK linking; these are observations,
not performance benchmarks. Checked object timestamps against final source and
acceptance-test timestamps in all four directories so stale intermediate builds
are not counted as final verification. Sanitizer execution runs outside the desktop
tracing restriction for LeakSanitizer inspection; leak detection, UBSan function
checking and assertions remain enabled. Linux CI and final local sanitizer runs
use halt-on-error with distinct ASan/UBSan exit codes 87/86, so an ordinary expected
source-failure exit 1 cannot hide a sanitizer failure in a child compiler process. Generated native instructions and LLVM
itself are not sanitizer-instrumented. Final logs were scanned for sanitizer
reports and LLVM backtraces, in addition to checking CTest status. No such reports
were found. Unique-ID/catalog consistency, local Markdown links and whitespace
checks passed. Final local logs are retained under `build/phase5-verification/`.

### Remaining limitations and hosted verification

DIAGNOSTICS.md records conservative suppression for failed dependencies, recovery
boundaries and resource limits. Lexically unreadable declarations may still cause
later unresolved-name messages. Terminal display width, UTF-16 protocol conversion,
successful-analysis warning/result APIs, incremental parsing/analysis and an LSP
remain future work. Runtime source failure behavior and conservative const restart
compatibility are preserved, not replaced with an inferred dependency hash.

The current workflow automatically runs these expanded ten suites on Linux,
Windows and macOS, including Linux ASan/UBSan. A new user-published Actions run is
still required for Phase 5: hosted Debug/Release on all platforms and hosted Linux
sanitizers, especially filesystem recovery, related ranges, CRT/native output
protection and retained snapshots. No current hosted success is claimed and no
remote operation was performed.

Recommend the next cohesive phase design precise validated source/dependency
compatibility and a reusable analysis-result API before incremental IDE integration.
Keep one parser/type checker. Numeric contextual literals/conversions still require
deliberate policy approval; managed types still require the GC/root/thread gate.

## Phase 6: numeric types and contextual literals, 11 October 2026

The user confirmed Phase 5 commit `4f3688d` passed hosted run #8 across Linux,
Windows and macOS, including the SDK RTTI correction. Read the current Bible
sections 6/30, VALUE_SEMANTICS, DIAGNOSTICS, DECISIONS, KNOWN_ISSUES,
CI_SANITIZER_ABI and DEVELOPMENT_STRATEGY. Presented the requested numeric policy
table and paused before compiler changes. The user explicitly approved the full
ten-type contract, then authorized implementation and complete local verification.

### Implemented contract and shared pipeline

All i8/i16/i32/i64/u8/u16/u32/u64/f32/f64 types now work throughout the existing
source compiler. Unsuffixed literals receive expected context; named/typed values
retain exact types. Integers wrap at their width and preserve the prior i32
MIN/-1 and zero-divisor behavior. Decimal literals construct their selected IEEE
precision directly; integral literal adaptation to float must be exact. Explicit
numeric as permits the approved rounding/truncation with runtime range/non-finite
checks. No bool promotions, as?, managed types or public ABI expansion were added.
NUMERIC_TYPES.md records the full approved contract and remaining syntax limits;
LANGUAGE_BIBLE.md was not edited.

A private LLVM Support APInt/APFloat kernel supplies reliable magnitude parsing
and deterministic declared-precision constant arithmetic/conversion. This is a
narrow new implementation dependency of void_core, with no LLVM types in public
frontend/IR APIs and no second AST evaluator/code generator. IR constants carry
64-bit scalar patterns; conversion/negation are explicit operations. Shared legal
operation helpers serve semantic analysis and IR validation. AOT and ORC still
use one lowering and optional O2 pipeline.

Checked casts guard exact integer ranges and ordered floating bounds before
FPToSI/UI. Float-to-integer lower bounds account for truncation and source
precision, including MIN-1 rounding to MIN at large widths. This removed an
initial trunc/truncf intrinsic approach that would have required native math
library linkage. MIN/-1 division selects a safe divisor before executing the
LLVM operation. Floating negation uses FNeg to preserve signed zero. A boundary
audit added directly signed integer zero in floating context; it now agrees with
floating unary negation, while conversion of an existing integer zero yields +0.
The initial new magnitude parser sign-extension bug was caught by unchanged i32
regressions and corrected before final verification.

### Acceptance coverage and independent calculations

Keep all ten original suite names and architectural assertions. Two negative
frontend fixtures now reject malformed `1.5q` and unsupported string types,
instead of rejecting the newly approved float syntax/types. Add numeric and
numeric_cli: 64 real top-level VOID programs, plus their imported helper.
The numeric suite performs 192 normal/O2 native JIT invocations; numeric_cli
performs 384 normal/O2 JIT/native AOT invocations and 128 actual object/link steps.
Their expressions additionally check compile-time results against explicit
independent expected values. Wider functions are called from source through the
unchanged i32 CLI/host adapter.

Coverage includes 96 signed/unsigned boundary arithmetic cases, all 100 numeric
conversion pairs, 64 valid float-to-integer boundary cases, both invalid boundary
sides for every integer destination in both floating precisions, integer zero
failures, cast failures and caller traces, exact literal adaptation, f32/f64
rounding differences, NaN comparison, signed zero, subnormals, finite overflow,
underflow, non-fused multiply/subtract, per-operation rounding, imports and numeric
constant restart tokens. Source errors preserve snapshots/spans, collect multiple
independent failures, and never return invalid IR. Numeric type context probing
is tested not to hide independent unknown calls in sibling operands. Same-width
incompatible entry signatures are rejected; failed edits preserve the old lease.

The developer-only `tests/numeric/generate_fixtures.py` uses independent Python
unlimited-precision integer formulas and explicit IEEE neighbors, importing no
compiler arithmetic. Regeneration matched every checked-in fixture byte.
The runtime-dependent dynamic_widths program returns 47, -7, 48 for inputs
0, 1, 10; its optimized arithmetic cannot be replaced by a single known result.
Separate handwritten floating fixtures check IEEE edge behavior. No test suite
or sanitizer assertion was disabled.

### Actual source-to-native demonstration

```sh
build/ci-debug/voidc check examples/numeric_types/main.void
build/ci-debug/voidc run examples/numeric_types/main.void 10
# 52
build/ci-debug/voidc emit examples/numeric_types/main.void -o build/phase6-verification/numeric-types.o
clang-19 --ld-path=/usr/lib/llvm-19/bin/ld.lld build/phase6-verification/numeric-types.o -o build/phase6-verification/numeric-types
build/phase6-verification/numeric-types 10
# 52
```

The imported example uses i64 arithmetic, contextual f32/f64 parameters, u64
wrapping and a checked result conversion. A recorded demonstration additionally
verified architecture inputs -1/0/1/10 -> 7/42/43/52, numeric example inputs
0/1/10 -> 42/43/52, and dynamic_widths inputs 0/1/10 -> 47/-7/48 in normal/O2
JIT and linked normal/O2 AOT. Logs and JSON are in build/phase6-verification.
These are parsed files, not reconstructed proof IR.

### Final local commands and measured results

Both SDKs' LLJIT/APFloat headers, OrcJIT library and LLVMConfig.cmake were checked,
and matching Clang 19.1.7 was executed. The installed SDK is `/usr/lib/llvm-19`;
the complete official SDK is `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`.
All six build directories retain their matching Clang/LLD, exact SDK and original
build-type/sanitizer settings. Every C++ compile command was checked for SDK-matched
RTTI; sanitizer commands retain `-fsanitize=address,undefined`. No toolchain
requirements, exported dependencies, test assertions or sanitizer flags were removed.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(numeric|numeric_cli|frontend|reload|gc|native)$' --repeat until-fail:3
```

Official archive members contain ThinLTO summaries. Final local SDK configurations
use LLD's standard content-keyed ThinLTO cache to reuse expensive SDK compilation.
This is only a local link-cache setting, not a replacement SDK, changed ABI,
optimization policy or test result cache. Native programs are emitted, linked and
executed afresh. Existing directories were reconfigured as follows (their explicit
SDK/compiler paths remain the ones recorded in CI_SANITIZER_ABI.md):

```sh
cmake -S . -B build/ci-official-release \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake -S . -B build/ci-official-debug \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake -S . -B build/ci-official-san \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure --repeat until-fail:3
```

| Final configuration | Result | CTest wall time |
|---|---|---|
| Installed LLVM 19.1.7 Debug | 12/12 passed | 39.59 s |
| Installed LLVM 19.1.7 Release | 12/12 passed | 30.07 s |
| Installed LLVM 19.1.7 ASan/UBSan, RTTI enabled | 12/12 passed | 75.04 s |
| Complete official LLVM 19.1.7 Debug | 12/12 passed | 26.69 s |
| Complete official LLVM 19.1.7 Release | 12/12 passed | 24.33 s |
| Complete official LLVM 19.1.7 ASan/UBSan, RTTI disabled | 12/12 passed | 47.89 s |
| Installed sanitizer repeats | Six suites × three, all 18 passed | 162.60 s |
| Official sanitizer repeats | Twelve suites × three, all 36 passed | 144.88 s |

Timings reflect local concurrent work and cache state; they are observations,
not compiler-performance benchmarks. LeakSanitizer runs outside the desktop
tracing restriction. The installed SDK retains vptr coverage; the official no-RTTI
SDK cannot provide that check. Both retain supported UBSan checks, ASan, leak
detection, fatal report handling and the existing UBSan JIT entry type metadata.
LLVM itself and generated application instructions remain uninstrumented, as
explicitly documented before this phase. No sanitizer coverage equivalence is claimed.

Re-executed the seven existing standalone sanitizer fault controls: heap overflow,
signed overflow and wrong indirect-call type in both RTTI modes, plus invalid
vptr in RTTI mode. All emitted the expected report and exited 86 under the strict
combined runtime settings. These reuse the previously compiled control binaries,
not intentionally faulty VOID programs. The shared sanitizer runtime's exit behavior
remains the one documented in CI_SANITIZER_ABI.md.

Additional final recovery stress used seed 60019: 300 malformed real numeric files,
including excessive cast chains, each retained a later independent missing-name
error, exited 1 within a three-second timeout, and exposed no raw LLVM diagnostic.
New as nodes count toward the existing expression-size guard. Unknown callees
still permit independent argument diagnostics; nested constant failures preserve
their original structured code/span rather than being rewrapped at a parent operator.

Final source/fixture hashes and relevant object timestamps were checked in all six
directories, so stale intermediate builds are not counted. Final logs were scanned
for sanitizer reports, LLVM errors/backtraces and project warnings; none were found.
Only the already documented GCC 16/LLVM ciso646 deprecation warning remains.
Local Markdown links and git diff whitespace checks passed. Evidence is retained
under ignored `build/phase6-verification/`: final configuration/build/test/repeat
logs, toolchain information, source manifest, independent oracle/regeneration,
recovery cases, cast failure trace and source/native demonstration.

### Remaining limitations and next phase

NUMERIC_TYPES.md documents deferred exponent/base/separator literal spellings,
float remainder, nullable as?, platform-sized aliases, numeric hashing/formatting,
foreign floating-control-state management and arbitrary public native entry ABIs.
The current CLI remains i32(i32), bool remains separate, and no managed/runtime
systems were expanded. No numeric family members are deferred.

The existing workflow automatically includes both new suites on Linux, Windows
and macOS. Phase 6 still requires a new user-published Actions run for actual
hosted Debug/Release on all platforms and Linux sanitizers. Local Linux results
cannot substitute for Windows/macOS executable verification. No hosted Phase 6
success is claimed. No commit, push, branch change, publication or remote mutation
was performed. Recommended next cohesive phase: validated analysis/dependency
APIs and compatibility metadata before incremental tooling; managed types still
require their separate GC root/thread integration gate.

## Phase 7: validated analysis and dependency compatibility — 11 October 2026

The user reports Phase 6 commit `9b9fdac` passed Actions run #9 on Ubuntu,
Windows and macOS. This phase proceeded locally from that clean tree. No branch,
commit, push, publication or remote mutation was performed. LLVM/Clang/LLD remain
19.1.7; the shared SDK RTTI policy, numerical semantics, twelve existing CTest
suites and architectural assertions remain intact. The Language Bible is unchanged.

### Implementation and safety decisions

Reviewed the current frontend, source graph, semantic/constant evaluator, lowering,
IR, JIT publication contract and project documentation. Split the existing graph
pipeline at its complete validation gate. `analyze_source`/`analyze_project` return
explicit success with an immutable owning ValidatedAnalysis, or diagnostics with
no program. Existing compilation adapters analyze once and lower the retained
validated tree. Analysis itself generates neither VOID IR nor native code.

The result owns canonical VFS modules and immutable source snapshots, typed/visible
function/module/local-constant declarations, source spans, module constant bit
patterns, containing owners and real resolved call/constant references. Module
constants retain their declaring identity when semantics substitutes literal bits.
Local readonly references use typed slot ownership to identify the declaration.
Function-only import cycles require no acyclic assumption or duplicate analysis.

Exact tagged length-delimited comparison data now covers the graph, declarations,
types/visibility and resolved references, plus parser-captured constant tokens.
Trivia, physical paths and source offsets are excluded. Constants include unused,
root, imported, transitive and runtime local readonly declarations. Equal evaluated
values do not hide changed declaration syntax. No finite hash authorizes reload.
Local constant IDs use function/name/lexical same-name occurrence, not raw lines.

Added validated-source JIT compilation and preflight replacement. Compiler-owned
source records override caller-supplied source compatibility fields; host layout,
capture and legacy contracts are additional requirements. Incompatible candidates
return false before native generation. Slot publication rechecks compatibility
under the existing mutex, and old code retains its resource tracker and source
analysis through leases. No automatic restart, watcher, incremental cache, LSP or
unproven multi-slot transaction was introduced. Legacy hand-built IR proofs remain
available; manually constructed IR/native code is a trusted host interface.

An initial focused run correctly exposed a preserved architectural requirement:
unused alias renaming across physical relocation must remain compatible. Canonical
imports now compare actual VFS targets and references compare resolved owners,
while tooling records still retain written spellings. The original module test
then passed unchanged. This was a correction to the new comparison, not a relaxed
architectural assertion. The preliminary failure is not included in final pass claims.

Conservative restrictions remain: declaration order and reference traversal order/
multiplicity participate, so some declaration moves or added calls require restart.
The new convenience replacement helper selects the project entry. Arbitrary host
entries still use the legacy IR interface and retain exact i32(i32) checking.
All ten numeric source types remain distinct in declaration contracts even when
LLVM uses equal storage widths. ANALYSIS_COMPATIBILITY.md documents the full API,
ownership model, classification and tooling boundaries.

### New source tests and observed native behavior

Added `analysis` and `analysis_cli`; all twelve prior suites remain, total fourteen.
The real three-file `examples/analysis/` includes project-root/module/local consts.
Normal and optimized JIT both observed:

```text
input 10, initial source: 53
edit only return stable + 1 -> return stable + 9: compatible, new slot returns 61
old native lease: 53
edit imported const base = 40 -> 41: incompatible, slot remains 61
```

The incompatible preflight does not allocate another native generation. A second
path deliberately compiles the incompatible candidate and proves direct slot
replacement still rejects it even when host metadata attempts to copy old tokens.
Comment/blank-line/constant-spacing edits remain compatible, updating spans without
changing symbol identities. Retained analysis lowers and executes after deletion of
all source files; old runtime failures still show their original source line.

Other focused coverage includes module/local/transitive constants, unchanged-value
constant syntax edits, visibility/signature changes, declaration addition/removal,
module graph changes, imported rename/rebinding, distinct same-named owners, cycles,
relocation, all ten numeric types, host layout/capture guards and multiple structured
errors. Each analysis run executes 480 malformed-source cases across the two JIT
modes, checking recovery retains a following independent diagnostic. Concurrent
callers acquire leases during 24 actual compatible native publications per suite
run; returned results must belong entirely to one generation. Existing deterministic
native-call retirement/GC/C-binding proofs also execute unchanged.

A recorded verbose Debug demonstration observed 229,310 normal-mode calls and
266,992 O2-mode calls during the source publication exercise; these are correctness
stress counts, not performance measurements. `analysis_cli` independently checks
53 -> 61 -> 62 through fresh real-source normal/O2 JIT and linked native AOT.
The final constant edit is legal in a fresh executable; live publication remains
forbidden. Six real object files are linked in each analysis_cli run.

### Actual local validation

Reused six established build directories, triggering the unchanged actual ORC
configure/link/execute SDK probe after CMake changed. SDK headers, APFloat header,
OrcJIT/Support libraries, LLVMConfig.cmake and Clang version were checked for each
configuration; metadata is in `build/phase7-verification/toolchains.json`.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Installed SDK: `/usr/lib/llvm-19/lib/cmake/llvm`, matching installed Clang/LLD19.1.7,
RTTI on. Complete official SDK: `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`,
its matching tools, RTTI off. The official builds retain the existing local ThinLTO
cache under `build/phase6-verification/lto-cache`; no SDK files are changed.
Strict sanitizer tests execute outside the tracing sandbox so LeakSanitizer process
inspection works, retaining leak detection and halt-on-error. The official no-RTTI
ABI cannot check vptr; the separate installed RTTI-enabled SDK supplies that coverage.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 14/14 passed | 39.45 s |
| Installed Release | 14/14 passed | 32.01 s |
| Installed strict ASan/UBSan | 14/14 passed | 60.52 s |
| Official Debug | 14/14 passed | 28.53 s |
| Official Release | 14/14 passed | 25.22 s |
| Official strict ASan/UBSan | 14/14 passed | 49.71 s |

Additional installed strict sanitizer stress ran `analysis`, `reload`, `modules`
with `--repeat until-fail:5`: all fifteen executions passed, 13.09 seconds.
The normal test output's three named suites represents fifteen actual repeats.
The complete official SDK ran the same three suites five times under strict
sanitizers: all fifteen executions passed in 11.77 seconds. Together these are
thirty additional compatibility/reload/module stress executions, including 4,800
malformed analysis inputs in the ten repeated analysis suite executions.

The complete official strict ASan/UBSan run passed all fourteen suites, including
the original native/GC/generation proofs and Phase 5/6 diagnostics/numerics. The
final audit verified all six compiler/test binaries are newer than the frozen
source/fixture inputs, the SHA-256 source manifest is unchanged, and all six
complete test logs contain 14/14 passes. Final build/test logs contain no sanitizer
reports, LLVM backtraces or new project warnings; only the established LLVM/GCC16
third-party `<ciso646>` warning remains. Markdown links and diff whitespace pass.
Both installed and official LLD report 19.1.7. No SDK or workflow requirements
were changed. The manifest is an audit artifact, not reload authorization.
Logs and source manifests are local ignored artifacts under
`build/phase7-verification/`. Windows/macOS Phase 7 hosted execution requires the
user's next push. The workflow already runs every CTest suite; no platform skips,
sanitizer suppressions or weakened requirements were added. Next recommended work:
an in-process tooling consumer and explicit invalidation planning before incremental
compilation/LSP transport; keep the separate managed-runtime integration gate.

## Phase 8: in-process tooling foundations — 11 October 2026

The user reports Phase 7 commit `04768bf` passed hosted Actions run #10 on Linux,
Windows and macOS. Continued directly from that clean local tree. Reviewed the
actual frontend records, validation/lowering boundary, native comparison/publication
API and current analysis, diagnostics, module, strategy, decision and issue records.
The Language Bible and Phase 7 compiler-derived compatibility machinery are unchanged.
No branch, commit, push, publication or remote modification was performed.

### Implementation

Added `include/void/tooling.hpp` and `src/frontend/tooling.cpp` as a small consumer
of the successful immutable analysis, linked into void_core. ToolingIndex retains
shared ownership and indexes stable declaration identities, declarations by module,
incoming resolved references and per-module byte positions. Queries return copied
structured records with owning SourceSpans. Optional/empty results represent absent
or unsupported symbols. No AST, token or declaration pointers escape. No parsing,
semantic checking, LLVM code generation, symbol scanning or global registry occurs.
Canonical declaring ownership survives aliases and root-constant references. Types,
visibility, function signatures, constant values and spans come directly from the
compiler. Only the compiler-collected function/module/local-readonly categories
are supported; mutable locals, parameters, import qualifier tokens, member completion
and arbitrary expression hover are not invented. UTF-8 offsets remain byte offsets.

Added a deterministic invalidation planner over reverse imports and resolved
reference edges, with explicit local-constant-to-containing-function dependencies.
Finite iterative traversal handles cycles. An old graph plus changed VFS hints
requires fresh complete graph discovery and validation. Namespace changes include
reverse importers; unknown module names and unknown root edits conservatively
include the whole known graph. Missing/invalid candidates expose no successful
comparison. New/removed modules are confirmed only between successful graphs.

Two successful snapshots provide actual source-byte differences, added/removed
reachable modules, declaration/constant metadata differences, changed resolved
bindings and exact source-contract classification. Both dependency graphs are
traversed as a union, preserving old dependents after deletion/rebinding. Physical
relocation does not count as a source edit. Changed files still seed every contained
declaration because Phase 7 does not collect function-body extents; no hidden second
parser is used to pretend otherwise. These are potential reanalysis sets, not a
partial compiler or a proof that a body/comment delta was identified exactly.

The planner compares the authoritative Phase 7 compiler contract verbatim. It
cannot publish code, substitute metadata summaries for that comparison, override
host ABI/layout/capture contracts or certify that snapshots match later disk edits.
The public flag is `has_validated_comparison`, deliberately not a claim of current
filesystem freshness. Work-required flags refer to unresolved work between the
supplied snapshots. Unknown hinted identities restore conservative unknown status.

### Real-source tests and executable example

Added `tooling` and `tooling_demo`, retaining all fourteen previous CTest suites and
assertions, total sixteen. Real filesystem projects test direct/aliased definitions,
multiple callers, same-named declarations, root and readonly local constants,
typed/visible records, exact identifier byte ranges, a UTF-8 comment containing
an accented scalar and emoji, changed line positions, forward references, cycles,
transitive invalidation, imported constants, signature/visibility changes, rebinding,
added/removed/renamed/missing/invalid modules and duplicate normalized imports.

Owned records survive original analysis/index destruction, source file deletion,
new analysis creation and native replacement. An independent u64 constant result
outlives both its index and analysis with the full 64-bit value intact. An unused
local const initializer still invalidates its containing function. Repeated queries
and plans compare deterministically, including equivalent physical relocations;
cycles perform 100 traversals per suite and relocation queries 30 comparisons.
The actual source JIT proof remains 53 -> 61, while the old lease stays 53 and
constant/signature changes cannot publish. No architectural regression was replaced
with a mock. Normal/O2 native AOT/JIT and numeric parity remain exercised by all
fourteen existing suites, including the Phase 7 normal/O2 publication proof.

`examples/tooling.cpp` builds as `void_tooling_example`, linked only to void_core.
It analyzes the actual three-file analysis example, looks up a declaration and its
resolved references, edits a disposable copy, plans invalidation, completely
reanalyzes, and distinguishes body/constant source contracts. CTest copies its
fixture before each run, so repeated execution never edits checked-in sources.
The example deliberately edits its argument directory; documentation requires a
copied disposable project. Actual recorded output:

```text
/worker.void::calculate declared in /worker.void; references: 1
edit plan: full validation required; fresh body source contract: exact match
fresh constant source contract: restart required; no native generation modified
```

The focused native/tooling test additionally recorded:

```text
queries: aliased/direct definitions, 2 callers, root/local constants, byte spans and retained snapshots
plan: /leaf.void -> /middle.void::wrapper -> /main.void::entry; validated 53 -> 61, const restart required
```

See `build/phase8-verification/demonstration.log`. TOOLING_FOUNDATIONS.md documents
the API, lifetime contracts, supported categories, conservative outcomes and future
LSP/incremental boundaries. Updated decisions, strategy, known issues, README and
the Phase 7 analysis document. No watcher, JSON-RPC, extension, background compiler,
persistent cache or partial validation pipeline was introduced.

### Actual local verification

Reused installed `/usr/lib/llvm-19/lib/cmake/llvm` and complete official
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` SDK configurations. CMake changes
rerun the established actual ORC configure/link/execute probe. RTTI stays on for
installed SDK targets and off throughout official SDK targets. LLVM, Clang and
LLD remain 19.1.7. Official builds use their existing local ThinLTO linker cache;
no SDK files or dependency/build requirements were modified.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Strict sanitizer execution runs outside the tracing sandbox so LeakSanitizer
process inspection works. Leak detection and halt-on-report remain enabled. The
no-RTTI official ABI still cannot provide vptr checking; the installed RTTI-enabled
lane retains that coverage. All prior numerical, diagnostics, native bindings, GC
root and leased-generation tests execute unchanged.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 16/16 passed | 36.34 s |
| Installed Release | 16/16 passed | 30.91 s |
| Installed strict ASan/UBSan | 16/16 passed | 58.65 s |
| Official Debug | 16/16 passed | 28.03 s |
| Official Release | 16/16 passed | 25.81 s |
| Official strict ASan/UBSan | 16/16 passed | 49.51 s |

Installed strict sanitizer repeats of `tooling`, `tooling_demo`, `analysis`, `reload`
with `--repeat until-fail:5` passed all twenty executions in 10.72 seconds. This
includes actual cycle/relocation queries, source lifetime checks, compiler recovery
and concurrent generation replacement. The same complete-official-SDK strict
sanitizer repeats passed all twenty executions in 9.68 seconds. Together these
are forty additional stress executions, including 1,000 cycle traversals and
4,800 malformed compiler-recovery inputs in the repeated tooling/analysis suites.
All retained source/query/index/native lifetime assertions executed under both
SDK sanitizer ABIs.

Final audit confirms all six compiler/test/example binaries are newer than their
frozen source/fixture inputs, and the SHA-256 manifest is unchanged. Each complete
configuration passed 16/16. Recorded build/test logs contain no sanitizer report,
LLVM backtrace or new project warning. Both LLD tools report 19.1.7; headers,
OrcJIT/Support libraries, LLVMConfig.cmake and Clang versions were checked in all
six configurations. The standalone example's six actual link lines include no
void_llvm, LLVMOrcJIT or void_runtime, confirming its frontend-only boundary.
Markdown links and diff whitespace pass. No SDK, runtime, backend, frontend
validation/compatibility code, sanitizer requirement or workflow was weakened.
The manifest is a verification artifact, never reload authorization. Local logs
and manifests are ignored artifacts in `build/phase8-verification/`. Hosted Phase 8
Windows/Linux/macOS verification requires the user's push; no hosted result for
these changes is claimed. The existing workflow automatically executes all suites.
Next establish explicit document/project revision handling and client invalidation
orchestration before partial compilation or LSP transport. Keep the separate GC/
thread-root integration and external-package identity gates.

## Phase 9 document revisions and tooling sessions, 11 October 2026

The user reports Phase 8 commit `f25b268` passed hosted run #11 on Linux,
Windows and macOS. Read the existing frontend/tooling and relevant documentation;
continued locally without branches, commits, pushes or remote changes.

Added owned ProjectSnapshot/SessionAnalysis and a noncopyable single-threaded
ToolingSession. Logical project identity is a retained unique token, with a numeric
revision for display; per-document nonzero revisions strictly increase even across
close/delete/reopen. Equal bytes do not imply equal revisions. Open, whole-text
replace, explicit unavailable/tombstone, close-to-disk and explicit disk refresh
are distinct transitions. The absolute entry is fixed at session construction.

Selected overlay maps share immutable SourceFile bytes and remain stable through
subsequent edits. Analysis execution and acceptance are separate: delayed, foreign
and duplicate results cannot update current diagnostics, queries, success or plans.
Current queries return absence after edits or failed validation, while explicit
historical handles retain owned records and the last successful baseline.

Extended the existing SourceGraph loader, not the parser/resolver/type checker,
with exact VFS overrides and virtual directory children. Existing path portability,
case, root-escape, symlink/redirect and hard-link restrictions remain. Virtual
file/directory conflicts reject. Tombstones never read old disk bytes. The overlay
analysis overload re-discovers through the same loader and compares exact bytes
and reachable modules before semantic acceptance. Documented its non-atomic disk
boundary, ABA/concurrent mutation limitations and caller refresh responsibility.
Original disk compile_project/CLI/JIT/AOT interfaces remain unchanged.

Session invalidation delegates to Phase 8 plan_invalidation, comparing a successful
candidate against the prior successful baseline before advancing it. Invalid,
stale and unvalidated candidates cannot produce a refined safe classification.
The Phase 7 exact source contract and separate native publication remain intact.

Added sessions and sessions_demo, preserving all sixteen prior suites/assertions.
Tests use real .void filesystem projects and shared analysis: entry/import/multiple
unsaved edits, virtual entry/nested imports, normalized paths/cycles, deletion and
close fallback, exact-case/file-directory collisions, overlay alias restrictions,
strict logical revisions, reordered execution/acceptance, equal bytes/new identity,
foreign session rejection, failed/repair transitions, UTF-8/CRLF byte diagnostics,
retained snapshots/queries, body/constant/signature/binding compatibility and forty
repeated transitions per session suite. Relative entry selection survives working-
directory changes. Historical session analysis lowers through the actual JIT;
existing normal/O2 native source tests retain 53 -> 61 and old lease 53, constant
restart and all architectural numeric/GC/native proofs.

The standalone void_sessions_example links only void_core and consumes the real
examples/analysis project without writing disk. Actual output is retained at
build/phase9-verification/demonstration.log:

```text
revision 1: /worker.void::calculate; references 1
unsaved edit: previous analysis stale; full validation required
fresh query: /worker.void::calculate; body exact match; affected dependencies: /main.void
revision 3 diagnostics:
/worker.void:2:12: error: Unknown identifier 'missing'.
    code: VOID2002
repair: fresh successful analysis
constant edit: restart required; no native publication
```

TOOLING_SESSIONS.md describes public APIs, ownership, revision semantics, freshness,
disk consistency, diagnostic positions, dependency planning and remaining scope.
Updated decisions, strategy, known issues, analysis/module/diagnostic/tooling docs
and README. No LSP, JSON-RPC, watcher, worker, cache, partial AST reuse, incremental
LLVM generation, new language syntax or automatic publication/restart is added.

During verification the existing module regression caught a changed symlink
message; restored the established diagnostic without weakening its assertion.
Review added tests/corrections for fixed relative entry roots and virtual
file/directory conflicts. All final results below use the frozen final sources,
not earlier interim successful binaries.

### Final local validation

Reused the installed `/usr/lib/llvm-19/lib/cmake/llvm` SDK (RTTI on) and complete
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` official archive (RTTI off).
Verified both SDKs' ORC/APFloat headers, Support/OrcJIT libraries, LLVMConfig.cmake,
Clang and LLD 19.1.7. CMake retains its actual ORC configure/link/execute probe.
Official builds are serial and use the existing ThinLTO cache; no SDK components,
requirements, sanitizer settings or RTTI correction were changed.

Actual final commands (existing configured build directories):

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Strict sanitizers run outside the tracing sandbox, retaining leak detection and
halt-on-error. Installed RTTI-on validation retains vptr coverage; the official
archive's RTTI-off ABI still cannot provide that check. All supported ASan/UBSan
instrumentation remains active. The official build's existing third-party
ciso646 deprecation warning remains visible; no project warning or LLVM backtrace
was found in final logs.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 18/18 passed | 38.57 s |
| Installed Release | 18/18 passed | 34.39 s |
| Installed strict ASan/UBSan | 18/18 passed | 57.27 s |
| Official Debug | 18/18 passed | 32.21 s |
| Official Release | 18/18 passed | 26.23 s |
| Official strict ASan/UBSan | 18/18 passed | 49.99 s |


Repeated sessions, sessions_demo, tooling, tooling_demo, analysis and reload ten
times per strict sanitizer SDK (the same leak/halt settings):

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -R '^(sessions|sessions_demo|tooling|tooling_demo|analysis|reload)$' --repeat until-fail:10
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure -R '^(sessions|sessions_demo|tooling|tooling_demo|analysis|reload)$' --repeat until-fail:10
```

Installed: **60/60 executions**, 22.26 s. Official: **60/60 executions**, 19.98 s.
Together the additional 120 executions include 800 repeated session body-edit
transitions, reordered acceptance and snapshot-lifetime cases, and 9,600 malformed
analysis recovery inputs. No sanitizer/leak report occurred.

Final audit verifies all six binaries are newer than the frozen source/fixture
manifest and each full test log is newer than its final binaries. All C++ compile
commands match their SDK RTTI setting; sanitizer compile commands retain ASan/UBSan.
Both standalone examples' six actual link lines exclude void_llvm, void_runtime
and LLVMOrcJIT. Exact SDK/compiler/linker checks, source manifest, build/test/stress
logs, demo output and audit scripts are ignored local artifacts under
build/phase9-verification/. Markdown links and git diff whitespace pass.

Phase 9 is complete locally and ready for hosted CI. Another user-triggered
Linux/Windows/macOS run is required to verify these new filesystem/overlay and
session cases on hosted systems; no new hosted result is claimed. Existing CI
already builds all targets and runs all eighteen suites, with no workflow skips
or requirement changes. The non-atomic disk-read boundary and explicit refresh
obligation remain documented limitations. Next consider protocol position adapters
and a minimal client transport before partial compilation; managed-runtime and
external-package identity gates remain separate. No commit, branch, push,
publication or remote modification was performed.

## Phase 10: minimal language server, 11 October 2026

Baseline: the user reports Phase 9 commit `02a9aa7`, Actions run #12, passed Linux,
Windows and macOS. Implemented this phase locally without branches, commits,
pushes, publication or remote changes. The original eighteen suites and assertions
are unchanged; lsp_positions, lsp_protocol and lsp_demo bring the total to **21**.

### Implementation and evidence

Added a real synchronous, single-threaded `voidls --entry <path>` executable.
It speaks framed stdio JSON-RPC/LSP 3.17: initialize/initialized/shutdown/exit,
full didOpen/didChange/didClose, publishDiagnostics, definition and references.
Only implemented capabilities are advertised. LLVM Support JSON handles parsing
and serialization; framing, lifecycle/errors and resource bounds are explicit.
Numeric/string IDs, negative/zero versions, stale/duplicate rejection, reopening,
multiple full-change events and empty change arrays are exercised through pipes.

The adapter retains exact SourceFile snapshots, reuses the compiler line index
and UTF-8 scalar validator, converts to UTF-16, handles LF/CRLF/CR and rejects split
scalar/surrogate positions. URI conversion handles escaped UTF-8 physical paths,
spaces, `#`, `%`, Windows drive notation and wide argv/binary streams. Another
Windows drive and malformed UTF-8 URI paths reject as parameters. The compiler's
physical-path labels and LLVM debug filenames now use explicit UTF-8 conversion;
portable ASCII VFS module-name restrictions are unchanged. No numeric/syntax,
compatibility, frontend resolver or runtime publication behavior was replaced.

Every accepted transition uses ToolingSession selection/full analysis/acceptance.
Invalid/currently stale graphs have no navigation fallback. Diagnostics preserve
compiler codes, severity, hints, ranges, related locations and negotiated open-file
versions; repair, graph removal and close publish empty arrays where needed.
Navigation consumes existing declarations/resolved references, including imported
functions, root/import/local readonly constants, forward/cyclic calls and multiple
callers. Unsupported mutable locals/parameters/expressions have empty results.

The portable Python harness launches actual subprocesses with binary pipes and
real `.void` fixtures. Tests cover fragmented/coalesced messages, malformed JSON,
invalid IDs, invalid UTF-8, excessive nesting, header/length/truncation errors,
UTF-16 emoji, CRLF/CR, zero-width EOF diagnostics, Unicode physical roots, aliases,
related locations, virtual modules, multi-file unsaved edits, stale/duplicate
versions, invalid/repair, close/reopen, graph changes and clean lifecycle/EOF.
The demo resolves its temporary root (macOS `/var` may alias `/private/var`) so
canonical output URIs compare correctly. No POSIX-only process API is required.

Actual demonstration command/output:

```sh
python3 tools/lsp_demo.py build/ci-debug/voidls
```

```text
initialized: full synchronization, UTF-16, definition and references
open: no diagnostics
definition: worker.void:1:6
references: declaration and one source call
unsaved edit: VOID2002 at worker.void:2:12
invalid revision: navigation returns null
repair: old diagnostics cleared; fresh navigation restored
shutdown and exit: clean protocol streams
```

### Final verification

Reused and regenerated the six existing CMake configurations. Installed SDK:
`/usr/lib/llvm-19/lib/cmake/llvm`, Clang/LLD 19.1.7, RTTI on. Complete official SDK:
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`, its own Clang/LLD 19.1.7,
RTTI off. Verified actual headers (LLJIT/APFloat), static libraries
(LLVMOrcJIT/LLVMSupport), LLVMConfig.cmake and compiler/linker versions. Required
LLVM development dependencies remain intact. Official archive builds ran serially
with the existing ThinLTO cache; the installed builds used two jobs.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

All full suites were rerun after the final demo/client revision. Final measured
results (Linux host, not new hosted-platform claims):

| Configuration | Result | CTest wall time |
|---|---|---|
| Installed Debug | 21/21 passed | 34.45 s |
| Installed Release | 21/21 passed | 31.18 s |
| Installed strict ASan/UBSan | 21/21 passed | 56.58 s |
| Official Debug | 21/21 passed | 28.58 s |
| Official Release | 21/21 passed | 26.10 s |
| Official strict ASan/UBSan | 21/21 passed | 51.23 s |

Repeated six suites ten times per SDK with the same strict settings:

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -R '^(lsp_positions|lsp_protocol|lsp_demo|sessions|analysis|reload)$' --repeat until-fail:10
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure -R '^(lsp_positions|lsp_protocol|lsp_demo|sessions|analysis|reload)$' --repeat until-fail:10
```

Installed **60/60 executions**, 26.10 s; official **60/60**, 24.51 s.
No sanitizer/leak findings occurred. LeakSanitizer ran outside the tracing sandbox;
no checks or tests were disabled. Installed RTTI-on coverage retains vptr;
the official archive's no-RTTI ABI still cannot provide that check. All supported
ASan/UBSan checks and the previous RTTI correction remain active.

The existing analysis tests still assert native 53 -> 61, an old lease returning
53, constant-edit rejection requiring restart, invalid-source rejection and
concurrent native lease safety in normal/O2 modes. All original GC/native/AOT/JIT
architectural regressions passed. The actual voidls link lines for all six builds
exclude void_llvm, LLVMOrcJIT and void_runtime; LLVMSupport remains linked for JSON
and the existing frontend's numeric support. No second compiler was added.

One initial official Clang attempt crashed in Lexer::SkipWhitespace while its
source file was being rewritten during build preparation. That attempt is retained
in `build/phase10-verification/initial-compiler-interruption.log`. The source was
then frozen before validation. The serial retry, all six final builds and three
additional identical official Clang compilations of the frozen server source
succeeded without a backtrace. Concurrent rewriting is consistent with the lexer
failure; a standalone toolchain defect was not reproduced. Do not treat that initial
attempt as clean or modify sources while the compiler reads them. The unrelated
historical LLD scheduler observation remains documented in KNOWN_ISSUES.md.

Final audit checks frozen source/fixture/client hashes (excluding generated pyc),
relevant binary freshness, full test-log freshness, all C++ RTTI compile flags,
sanitizer instrumentation, exact LLD version and the server's link boundary.
Final build/test logs contain no LLVM backtrace, sanitizer finding or new project
warning. The known third-party ciso646 warning remains visible. Markdown links and
git diff whitespace pass. Ignored evidence lives in build/phase10-verification/:
source-manifest.json, toolchains.json, six build/test logs, stress logs/summaries,
demonstration.log, frozen-compiler-check.log and audit scripts/results.

### Delivery and next work

[LANGUAGE_SERVER.md](LANGUAGE_SERVER.md) documents startup, methods, UTF-16/version
contracts, URI/security limits, compiler dependency boundaries and current gaps.
README, decisions, strategy and known issues are updated. Full graph analysis is
synchronous; only entry-reachable source is semantically validated. Disk reads
remain non-atomic and there is no watcher/save handler/refresh extension. Unsupported
symbol categories return empty results. No broad IDE functionality, new language
features, caches, workers or automatic native publication were introduced.

**Phase 10 is complete locally and ready for hosted GitHub CI.** The existing
workflow already installs Python 3.12, builds all targets and runs all tests with
strict Linux sanitizer settings; no workflow/test skip is needed. The user's next
run must verify the new 21-suite Debug/Release matrix on Linux/Windows/macOS and
Linux sanitizers, especially Windows drive/wide-argument/Unicode filesystem handling
and macOS temporary-root behavior. No new hosted outcome is claimed.

Next consider small editor packaging and explicit disk refresh/cancellation
contracts before workers or partial compilation, retaining exact compatibility and
snapshot ownership. Managed-runtime and package identity gates remain separate.
All version-control and remote publication steps remain with the user.


## Phase 11 — VS Code and VSCodium extension (2026-10-11 local)

Baseline: the user reports Phase 10 commit `ccfea79` passed hosted run #13 on
Linux, Windows and macOS. This phase changes only editor integration, documentation
and additional CI steps. No native C++/CMake source, native test, LLVM version,
language semantics, RTTI correction or reload contract was changed. No commits,
branches, pushes, remote changes or publication were performed.

### Implementation and boundaries

`editors/vscode/` is an isolated strict TypeScript project using pinned
vscode-languageclient 10.1.2, Node.js 24.19.0 and a checked-in-ready npm lockfile.
The native build does not depend on Node. One standard language client owns
stdio, document synchronization, diagnostics and navigation; the existing compiler
owns analysis and UTF-16 ranges. It launches the actual executable directly as
`voidls --entry absolute-entry` with argument arrays and `shell: false`.
The executable transport default is intentional: explicit TransportKind.stdio
would add --stdio, which the existing native CLI does not accept.

Configuration is explicit: void.server.path, void.project.entry and
void.trace.server. Literal relative paths resolve against one workspace folder;
ambiguous Windows drive/root-relative paths are rejected. Entry's parent remains
the canonical compiler root. Empty entry selects a fixed single-file fallback;
switching tabs does not guess a new project, while explicit restart can select
the active file. Multi-root and unrelated documents are explained/rejected or
excluded from synchronization. Native launch is gated on actual Workspace Trust.
Serialized lifecycle work handles restart/configuration changes, preflight errors,
actual spawn failure, unexpected termination and asynchronous shutdown. There is
no crash/restart loop, shell, native download or extension telemetry.

Presentation registers .void, # comments, four-space indentation and a small
TextMate grammar for implemented tokens. It is not another semantic parser.
Completion, hover, rename, formatting, code actions, watchers, workers, new language
syntax and native hot reload remain outside this phase.

### Actual extension verification

Local host: Linux PikaOS, installed VS Code **1.141.0**, Node **24.19.0**, npm
11.6.2. Pinned dependencies installed successfully with ordinary lifecycle scripts
(`npm ci --offline --no-audit`, cached after the initial dependency installation).
Initial dependency audit reported zero vulnerabilities. The final `npm test`
strict type-check, TypeScript/esbuild build and **4/4** configuration/presentation
unit tests passed with zero skipped tests.

The final real editor test command was equivalent to:

```sh
cd editors/vscode
npm ci
npm test
VOIDLS_PATH=/absolute/project/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
```

The installed editor ran on the local graphical display, using disposable profiles
and a real multi-file project under a Unicode root containing spaces, # and %.
UTF-8 comments and an imported `worker space.void` were included. All three final
editor profiles passed:

- Trusted: real native initialization and clean initial diagnostics; definition
  in the imported module and two resolved references; unsaved VOID2002 with exact
  error severity/range and unchanged disk content; no stale definition/reference
  results for invalid source; repair clears diagnostics and restores navigation.
- Trusted lifecycle: tab close/reopen plus actual TextDocument close/open events;
  clean restart and configured entry/executable switches; missing executable;
  actual OS spawn failure when a copied real binary disappears after validation;
  unexpected child termination with no restart loop; fixed single-file fallback
  and explicit restart selection; deactivation and final child cleanup. No
  unhandled client rejection occurred. Normal restart/final shutdown exited zero.
- Restricted: actual workspace.isTrusted was false, explanatory reporting and
  zero native children, including an explicit restart attempt.
- Multi-root: two actual workspace roots, explanatory reporting and zero children.

These are VS Code editor API tests against the built real voidls, not a mocked
LSP server. Test instrumentation observes real child creation and captures error
notification calls; it does not replace native execution or compiler analysis.
The official test runner disables Workspace Trust unconditionally, so the
restricted case launches the same editor through its documented test CLI without
that flag. VS Code may retain text models after tabs close; a documented language
reassignment triggers actual TextDocument close/open events for synchronization.

Earlier attempts exposed the unwanted --stdio argument, retained editor models,
and the official runner's trust flag. Those were investigated and corrected;
initial failing logs remain in ignored evidence. A final npm-ci attempt inside
the execution sandbox was blocked by esbuild's native execution validation
(EPERM); the unchanged pinned installation/build/tests succeeded outside that
sandbox with lifecycle scripts intact. GUI tests also require execution outside
that sandbox. These initial attempts are not counted as passing verification.

### Native regression verification

All existing **21 CTest suites** passed in every configuration, with no skips:

| LLVM 19.1.7 SDK | Build | Passed | Test time |
| --- | --- | --- | --- |
| Installed | Debug | 21/21 | 35.03 s |
| Installed | Release | 21/21 | 30.35 s |
| Installed | ASan/UBSan | 21/21 | 57.20 s |
| Official archive | Debug | 21/21 | 29.33 s |
| Official archive | Release | 21/21 | 25.09 s |
| Official archive | ASan/UBSan | 21/21 | 52.00 s |

Builds used the existing build/ci-debug, ci-release, ci-sanitizers,
ci-official-debug, ci-official-release and ci-official-san configurations.
Debug/Release targets were built before ctest; sanitizer binaries retain the
unchanged native source/configuration. Strict sanitizer execution used
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 outside the tracing
sandbox. The existing SDK-dependent RTTI/vptr correction remains intact.
All native AOT/JIT, GC, reload and Phase 5–10 regressions remain present.

### Package and delivery

`npm run package` succeeded using vsce and the normal prepublish build hook.
The resulting ignored `editors/vscode/dist/void-language-0.1.0.vsix` contains eight
files and is **148706 bytes**. SHA-256:
`313d323ba091fe262e90b3cf1b1cb65e0a86b40b56780a56699714d538375680`.
ZIP inspection verified the packaged JavaScript equals the built bundle and
includes full runtime dependency license notices. No LLVM/native binaries, tests,
node_modules, source maps or lockfile are packaged. The 957.84 KB unminified bundle
produces an informational size warning; the compressed VSIX is 145.22 KB. No
license was invented for the repository: metadata remains UNLICENSED and local
packaging explicitly skips the absent owner license/repository metadata checks.

The actual VS Code CLI installed this VSIX successfully in isolated temporary
extension/profile directories. The user's normal editor installation was not
changed. The editor CLI emitted its own url.parse deprecation warning; installation
succeeded, and the warning remains visible in package-install.log. Build/package/test
output and evidence remain ignored under
build/phase11-verification/, including final dependency/unit/editor/package logs,
native logs, package-audit.json and source-manifest.json.

[EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md) and the extension README document
native build, explicit settings, real existing source example, development host,
local installation, logging/troubleshooting and limits. Repository README,
DECISIONS, KNOWN_ISSUES, LANGUAGE_SERVER and DEVELOPMENT_STRATEGY are updated.

**Phase 11 is complete locally and ready for hosted CI.** The workflow preserves
the native three-platform matrix and strict Linux sanitizers, then adds pinned
Node dependency installation, type-check/build/unit tests, real editor tests
(Linux xvfb-run), VSIX packaging and per-platform package artifacts. The next
user-triggered run must validate these added steps on all three hosted platforms;
local Linux success does not establish Windows/macOS GUI or hosted download/display
compatibility. VSCodium is not installed locally, and editor 1.91 API compatibility
was type-checked but not exercised in an actual minimum-version host.

Recommended next phase: expanded hosted/editor compatibility checks, followed by
explicit disk-refresh/cancellation and client usability contracts before workers,
partial compilation or broader IDE features. Keep the compiler analysis/reload
contracts and separate managed-runtime/package design gates intact.


## Phase 11 Windows termination-report correction (2026-10-11 local)

### Hosted evidence and root cause

Read the actual GitHub Actions logs for [run #14, Windows job 114668718372](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718372),
commit `7fa9893d3aadc2cab5d1588f8cafefa21754860f`, using the read-only GitHub
job-log API. Windows Server 2022 native Debug and Release passed 21/21 each;
extension unit tests passed 4/4. The editor test reached stopped after killing
the actual native process, then failed with `Timed out: termination report` at
04:59:04 UTC. The logs did not capture the client output channel/underlying Windows
pipe error. Consequently an exact Windows errno or event timeline cannot be
asserted from this log alone. Focused stream/exit/notification instrumentation now
prints these events in the integration test for the next hosted run.

The pinned vscode-languageclient 10.1.2 implementation establishes the lifecycle
hole: common/client.js handleConnectionError awaits error(), then ErrorAction.Shutdown
calls stop(). shutdown() enters Stopping before asynchronously awaiting shutdown/exit.
handleConnectionClosed skips errorHandler.closed() while Stopping and returns early
when Stopped. VOID's old error() returned Shutdown without reporting, while only
closed() reported unexpected termination. A transport error before close therefore
stopped the client without the promised VOID notification. Windows terminated pipes
can exercise this ordering, unlike the close-first path observed locally. This
ordering explains the hosted symptom; its precise Windows OS event remains to be
confirmed by the added instrumentation on the next run.

Before editing extension lifecycle code, reproduced this exact reporting hole in
an actual Linux VS Code host with the real built voidls: emit a controlled EPIPE
error on the actual child stdin transport, allowing the standard language client's
real error handler to stop the native server. The unmodified extension reached
stopped and failed `Timed out: transport error termination report`. This is a
controlled transport-event regression, not a claimed Windows OS reproduction,
fake server, replacement transport or compiler simulation. The original real
process-kill assertion remains and is strengthened to count reports per failure.

### Exact correction and regression coverage

The final error() policy returns Continue with handled=true and logs the transport
error; an error alone is not proof of native termination. It leaves the standard
client eligible to invoke closed() when the actual connection closes. closed()
reports through a per-client guarded reporter and returns DoNotRestart/handled=true,
so the standard client cleans up without restarting or a duplicate generic popup.
A successfully initialized current session reports once; later independently failed
sessions still notify. Startup failures retain their existing startup-error path.
Intentional shutdown is identified by manager disposal or clearing/replacing the
active client before stop(), suppressing false termination warnings for explicit
restart/configuration/deactivation. The normal asynchronous stop/dispose lifecycle
remains unchanged. No separate process supervisor/transport is introduced.

A first correction reported from error() as well as closed() while retaining
Shutdown. Stronger assertions exposed a generic duplicate popup during an actual
local process kill: handleConnectionClosed uses a default unhandled DoNotRestart
result when Stopping, bypassing our closed() result. Thus reporting from both
callbacks was insufficient. The final Continue policy avoids entering Stopping
from an error and preserves the handled close path. The intermediate attempt is
not counted as passing final verification; its output-channel evidence is retained
in windows-fix-duplicate-warning.log.

The real editor suite additionally injects two controlled stream errors on the
actual native child's stdin, verifies that a transport error alone neither stops
the client nor falsely reports termination, then kills that actual process to
force error-before-close ordering. It requires exactly one notification, real
child cleanup and no automatic restart. After explicit restart, the original
independent native process-kill test remains and must produce the second report.
Exactly one UI notification per failure is asserted, including generic client
popups; normal initialization/restart/entry changes must not warn. Subsequent
single-file selection may legitimately warn about other open project documents,
but no failure notification is permitted during intentional restart/deactivation.
Existing diagnostics, unsaved repair, navigation, startup-failure, trust, multi-root,
cleanup and unhandled rejection assertions remain. Instrumentation logs real stream
errors, native exit status and captured error notifications to stdout/evidence;
the VOID channel logs connection reasons and lifecycle state.

### Local results and remaining hosted check

Using Node 24.19.0, VS Code 1.141.0 and the installed LLVM 19.1.7-built real voidls:

- `npm test`: strict type-check/build and 4/4 unit tests passed.
- `VOIDLS_PATH=.../build/ci-debug/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host`:
  trusted (controlled error-before-close and independent process kill), restricted and multi-root profiles passed.
  Exactly one report per unexpected failure, no termination warning for intentional
  shutdown, no automatic restart, no unhandled rejection and no leaked child.
- `ctest --test-dir build/ci-debug --output-on-failure`: 21/21, 34.85 seconds.
- `ctest --test-dir build/ci-release --output-on-failure`: 21/21, 30.45 seconds.
- Strict installed-SDK ASan/UBSan CTest: 21/21, 54.21 seconds, with leak detection
  and halt-on-error settings unchanged, outside the tracing sandbox.
- `npm run package`: succeeded; eight-file local VSIX includes dependency notices
  and the exact built bundle. Package/source hashes are recorded in
  build/phase11-verification/windows-fix-audit.json. The informational bundle-size
  warning remains visible.

Ignored evidence is in build/phase11-verification/windows-job-excerpt.log and
windows-fix-{before,after,unit,package,native-debug,native-release,native-sanitizers}.log.
The before log is an intentional failing reproduction, not a passing result.
No compiler/native-server/LLVM/configuration/workflow/dependency changes were made.
All existing tests remain intact. Windows is unavailable locally: the actual Windows
process termination/pipe behavior still requires another user-triggered hosted run;
macOS was not rerun locally either. No hosted success is claimed for this correction.
No commits, branches, pushes, remote edits or publication were performed.


## Phase 11 run #14 Linux evidence review (2026-10-11 local)

Reviewed the actual read-only GitHub job logs and completed run metadata for
[run #14](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510),
commit `7fa9893d3aadc2cab5d1588f8cafefa21754860f`:

- [Linux job 114668718282](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718282):
  Debug, Release and strict ASan/UBSan each passed 21/21; extension unit tests
  passed 4/4. The real editor test failed `Timed out: termination report`.
- [Windows job 114668718372](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718372):
  rechecked Debug/Release 21/21 and unit 4/4 passes, with the same editor timeout.
- macOS job 114668718102 completed successfully, including real editor integration
  and VSIX packaging, as confirmed by the run job/step metadata.

Both failing jobs exercised the original committed lifecycle, not the existing
uncommitted correction. Linux restored and initialized the real server at
05:34:48.393–.398 UTC, logged `Error: Unexpected SIGPIPE` at 05:34:48.402,
then remained alive until the termination-report assertion timed out at
05:35:08.454. Its stack is process.<anonymous> in the editor's
resources/app/out/bootstrap-fork.js:4:1710 followed by process.emit, not a native
VOID/LLVM backtrace. The original test awaited the client reaching stopped before
waiting for the notification. This matches the same error-before-close reporting
hole investigated above; neither log captures the exact transport errno/callback
sequence, so that fine-grained Windows/Linux event order remains an inference
supported by the pinned client source and deterministic local reproduction.

Inspected the installed matching VS Code 1.141.0 bootstrap-fork.js. Its SIGPIPE
handler uses process.on('SIGPIPE', ...) and console.error(new Error('Unexpected
SIGPIPE')), once per process. It does not throw that Error or terminate the host.
A write to a killed child's closed pipe is consistent with the test timing; the
log does not identify the individual write. The host continuing for twenty seconds
and failing the notification assertion confirms this message was not itself the
fatal assertion or an extension-host crash. No native sanitizer issue is indicated.

Crucially, the final corrected local real-editor run already recorded the same
`Unexpected SIGPIPE` once in windows-fix-after.log and still passed the trusted,
restricted and multi-root profiles. The real kill, exact notification counts,
no-automatic-restart, intentional-shutdown and child-cleanup assertions all passed
with that signal log present. No signal handler, environment flag, suppression,
weakened assertion or test skip was added. Hashes of extension.ts and host.ts still
match the verified windows-fix-audit.json. The earlier local 4/4 unit, three 21/21
native configurations and package results remain applicable because code is
unchanged. No redundant native or editor rerun was needed for this evidence-only
review. The Linux CI excerpt is retained in ignored
build/phase11-verification/linux-run14-excerpt.log.

**The existing correction covers the lifecycle hole behind both failures and is
ready for the next hosted CI attempt.** ErrorAction.Continue logs a transport error
without prematurely entering Stopping, allowing actual connection closure to run
the guarded reporting handler and DoNotRestart cleanup. macOS close-first behavior
continues to use that same handler. SIGPIPE may still be logged during deliberate
process-kill testing; current evidence does not justify a separate product change.
A new hosted run is still required to verify the corrected code on the actual
Linux/Xvfb, Windows and macOS runners. Exact Windows pipe behavior remains unavailable
locally. This review changes WORK_LOG.md only; the existing lifecycle correction,
all strict tests, native code, LLVM settings and workflow are left intact. No
commits, pushes, branches or remote modifications were performed.


## Phase 11 run #15 Windows entry-path correction (2026-10-11 local)

### Actual hosted evidence

Read [run #15](https://github.com/Shmellyorc/Void.Language/actions/runs/38209988399)
and [Windows job 114681780204](https://github.com/Shmellyorc/Void.Language/actions/runs/38209988399/job/114681780204)
through the read-only GitHub API. Commit is
`6142383e584d5803ea9a20f1de60b3b69204ef5b`. Native Debug/Release each passed 21/21
and extension unit tests passed 4/4. Both real native process-kill scenarios
produced the expected termination notifications (05:58:04.007 and .240 UTC),
confirming progress past the previous termination timeout on Windows.

The new failure at out/test/host.js:256 is exact string inequality in the fallback
entry assertion: actual `C:\Users\runneradmin\...\worker space.void`, expected
`c:\Users\runneradmin\...\worker space.void`. Every other character, including
Unicode root, spaces, escaped punctuation and filename, matches. The native child
was launched successfully with the canonical worker entry and published worker
file diagnostics immediately before this assertion. Evidence is retained in
ignored build/phase11-verification/run15-windows-excerpt.log.

### Contract and minimal correction

This assertion compared two representations of the same entry. The existing
resolveProject contract uses the filesystem-canonical parent (fs.realpath) joined
with the selected basename, retaining a native filesystem path for --entry and
status. VS Code Uri.fsPath lowercases a Windows drive letter. The test fixture
already supplies its filesystem-canonical root independently via VOID_TEST_ROOT;
assertions now compare status against exact path.join(root, 'worker space.void')
and path.join(root, 'main.void'). They still require the precise expected root and
filename, fixed fallback after switching tabs, and new entry after explicit restart.
No generic case-insensitive comparator or whole-path lowercasing was introduced.
No native entry selection, project root, resolver or compiler identity is changed.

Inspection also found a real editor-facing warning defect adjacent to the assertion:
the single-file exclusion check compared doc.uri.fsPath directly against native
project.entry. On Windows it could falsely warn that the selected file itself was
outside its session because of C:/c: spelling. That one comparison now converts
the native entry through vscode.Uri.file(entry).fsPath before comparing editor
paths. This uses the established editor representation without folding directory
or basename case, changing filesystem identity rules or widening project scope.
The native entry/status representation remains unchanged.

Added real-editor API checks, portable across hosts, proving upper/lower Windows
drive URI spellings yield the same fsPath while different drives, directory case
and filenames remain distinct. The real fallback scenario temporarily closes the
main VOID document via language reassignment, leaving only the selected worker
VOID document. Selecting fallback must add no outside-project warning. Reopening
main as VOID restores the existing tab-switch/restart checks. These additions keep
actual source, Unicode/space paths, native children, strict notification counts,
clean deactivation and every previous compiler/lifecycle assertion intact. They
are actual URI API tests, not a claim of running Windows filesystem APIs on Linux.

### Local verification and pending hosted results

- `npm test`: strict type-check/build and all 4 unit tests passed.
- Real installed VS Code 1.141.0 against the LLVM 19.1.7-built native voidls:
  trusted, restricted and multi-root profiles passed, including URI checks,
  selected-file warning regression, both process kills, exact notifications,
  fallback selection, explicit restart and final child cleanup.
- Debug CTest rerun: 21/21 passed, 34.40 seconds. Native C++/tests, sanitizer
  configuration and LLVM/Clang/LLD 19.1.7 were not modified. The preceding local
  Release/sanitizer validation remains applicable to those unchanged sources.
- `npm run package`: succeeded, eight-file VSIX, 145.41 KB. ZIP audit confirms
  the shipped JavaScript equals the tested build; source/package hashes are in
  build/phase11-verification/run15-audit.json. Informational bundle-size warning
  remains visible. git diff whitespace check passed.

Local evidence is in build/phase11-verification/run15-{unit,editor,native-debug,package}.log.
At the latest run #15 job review, macOS job 114681780024 was still building Release;
Linux job 114681780334 was configuring Release. Neither completed result was yet
available, so neither platform is claimed green for run #15. Windows is unavailable
locally; the drive URI behavior is exercised through actual VS Code APIs, while the
full native Windows fallback/warning sequence requires the next hosted run. The
correction is ready for that attempt. No workflow/dependency/compiler/runtime
changes, commits, branches, pushes or remote modifications were performed.
# Phase 12 review — 12 October 2026

The project owner reports Phase 11 Actions run #16 at `1b32151` passed Linux,
Windows and macOS. Phase 12 begins from that completed foundation with a clean
working tree. Reviewed the language/value-semantics contracts and traced the
existing frontend, immutable validated analysis, module resolution, scalar VOID
IR, shared LLVM lowering, tooling and strict SDK/sanitizer configuration.

Value structs require extending the shared type representation, resolving named
types, aggregate IR operations and aggregate debug metadata; primitive-only types
currently flow through all these layers. No parallel type checker/backend is
needed. The existing i32 host adapter can stay intact while source functions
exchange aggregates internally.

Recorded the bounded proposal in VALUE_STRUCTS_PROPOSAL.md. Approval is pending
for pure compile-time declared field defaults versus runtime defaults with an
explicit observable construction sequence. The Bible establishes safe defaults
and named construction, but does not settle runtime default expressions; §16 also
leaves custom initializer interaction open. This is a design gate requested by
the owner, not an implementation failure. No compiler/runtime/editor changes,
Phase 12 builds or tests, commits, branches or remote modifications have been
made during this review.

## Phase 12 implementation and final local validation — 12 October 2026

The owner approved Option A in VALUE_STRUCTS_PROPOSAL.md after the design review.
Implemented pure, typed compile-time declared field defaults, recursive safe
fallbacks and source-order runtime named arguments. Runtime defaults, custom init
and positional constructors remain deferred; none is permanently prohibited.
Phase 11 run #16 at `1b32151` passed all hosted platforms according to the owner.
Phase 12 hosted results are not yet available.

### Implemented surface and architecture

- Numeric/bool and acyclic nested value structs; forward and imported type names;
  public/internal/file-private type declarations; exact nominal module identity.
- Optional named-field construction, compile-time aggregate constants, recursive
  defaults, field reads, nested writes/compounds/increment/decrement, local copies,
  mutable copied parameters and independent aggregate returns. Readonly binding
  roots protect all nested fields. Named arguments evaluate once in written order.
- One shared nominal descriptor model extends ir::Type through frontend, validated
  analysis, independent IR and LLVM. ConstantValue carries recursive values, while
  scalar evaluation retains the original numeric kernel. Layout/default dependency
  gates reject cycles and unsupported types before lowering. Limits bound layout
  depth/default dependency recursion and expanded layout size; expression limits
  reset for each declared field default.
- Aggregate construct/extract/insert IR operations and typed slots implement copy
  semantics with no heap/reference aliasing mechanism. Shared LLVM lowering uses
  target-aligned declaration-order structs and matching private source signatures.
  Target DataLayout is available before aggregate debug member sizes/offsets are
  built. Ordinary runtime failures propagate through aggregate-returning functions
  before callers consume their result. The host adapter remains i32(i32).
- Validated analysis owns type/field declarations and references. Qualified type
  annotation navigation points at the type identifier; annotation dependency edges
  identify their enclosing declaration, preserving tooling invalidation contracts.
  Constructor names/field initializers and field reads/writes navigate through the
  existing LSP index. Layout/type/default changes reject reload before publication;
  compatible function-body changes still reload and preserve old leases.
- Editor grammar/indentation recognizes contextual `value struct`; ordinary `value`
  parameters remain valid. Extension lifecycle code, compiler/runtime boundaries,
  LLVM version/SDK requirements, RTTI correction and native printf proof are intact.

During implementation, existing regressions exposed contextual-keyword and import
lookup/recovery mistakes. These were corrected in the compiler rather than by
changing their assertions. Final runs preserve malformed signature recovery,
failed-import poisoning, ambiguity locations, independent diagnostics, and numeric
context from qualified constants. Added struct tests also check parser recovery,
invalid defaults/fields/types/returns, recursion/resource limits and malformed IR.

### Exact final commands and results

Existing build trees retain their strict LLVM 19.1.7 configurations: installed SDK
at `/usr/lib/llvm-19/lib/cmake/llvm`, matching `/usr/bin/clang++-19`; official SDK
at `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/lib/cmake/llvm`, with its own
matching Clang/LLD. CMake reconfiguration during this phase passed the required
ORC link-and-run probe. SDK headers, libLLVMOrcJIT.a and LLVMConfig.cmake were
checked for both installations; llvm-config reports 19.1.7 for both, RTTI YES
for installed and NO for official. No mismatched components or relaxed SDK
requirements were introduced. See build/phase12-verification/sdk.json.

For each Debug/Release tree below, executed:

```sh
cmake --build build/<tree> -j 2
ctest --test-dir build/<tree> --output-on-failure -j 4
```

For each sanitizer tree, built with the same command, then executed outside the
tracing sandbox so LeakSanitizer can operate:

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/<tree> --output-on-failure -j 4
```

| SDK | Build tree / configuration | Final result | CTest elapsed |
| --- | --- | --- | --- |
| Installed, RTTI on | ci-debug / Debug | 22/22 passed | 26.91 s |
| Installed, RTTI on | ci-release / Release | 22/22 passed | 19.83 s |
| Installed, RTTI on | ci-sanitizers / RelWithDebInfo ASan/UBSan | 22/22 passed | 30.98 s |
| Official, RTTI off | ci-official-debug / Debug | 22/22 passed | 22.71 s |
| Official, RTTI off | ci-official-release / Release | 22/22 passed | 15.97 s |
| Official, RTTI off | ci-official-san / RelWithDebInfo ASan/UBSan | 22/22 passed | 26.81 s |

These are 132 passing suite executions. All original 21 suites remain; the new
value_structs_cli suite adds normal/O2 JIT/AOT execution, actual native linking and
DWARF verification for three source programs. The frontend suite adds struct
semantics, diagnostics, nominal module lookup, readonly, tooling, compatibility,
resource and IR validation cases. Existing numeric/GC/reload/native and Phase 5
recovery assertions remain enabled. Sanitizer instrumentation retains the existing
SDK-compatible RTTI policy; C++ compiler/runtime checks do not imply that generated
LLVM/JIT application code is sanitizer-instrumented (the existing limitation).

Extension verification, with Node 24.19.0 and existing pinned dependencies:

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

Strict TypeScript checking/build passed. All four registered unit tests passed
(the detailed TAP rerun outside the sandbox confirms their individual execution).
Real installed VS Code 1.141.0 passed trusted, restricted-trust and multi-root
profiles against the final native voidls, including actual process kills, strict
notification counts, no automatic restart, fallback paths, explicit restart and
clean child shutdown. The existing intentional-kill SIGPIPE message remains visible
in the passing trusted-host log; no lifecycle change or suppression was added.
The native LSP protocol suite additionally passed type/constructor/field navigation,
references, readonly diagnostics and invalid-to-valid revision recovery.

### Actual source-to-native demonstration

Executed the documented check/run/emit commands on
examples/value_structs/main.void, linked its emitted object with the matching
Clang/LLD 19.1.7, then ran the resulting Linux ELF executable:

```text
Source checked successfully
46                          # normal JIT, input 10
46                          # optimized JIT, input 10
Wrote native source object: build/phase12-verification/value-structs.o
46                          # separately linked native executable, input 10
```

The object/executable and native-demo.log are retained under
build/phase12-verification/. `file` identifies the executable as an x86-64 ELF PIE
with debug_info. This compiles the actual imported .void declarations, fields,
functions and expressions; no manually reconstructed example IR is used.

Final per-tree build/test logs, extension/unit/editor logs, SDK evidence, source
hashes and results.json are retained in build/phase12-verification/. git diff
whitespace validation passed. Build artifacts are local ignored outputs.

### Remaining limitations and hosted readiness

VALUE_STRUCTS.md, SOURCE_COMPILER.md, VALUE_SEMANTICS.md, DECISIONS.md,
KNOWN_ISSUES.md, DEVELOPMENT_STRATEGY.md and README now describe the implemented
surface and limits. The Language Bible was not altered. No public C aggregate ABI,
stable offsets/serialization, managed reference types, runtime field defaults,
custom init, positional constructors, field modifiers or aggregate equality was
introduced. The GC remains experimental. The recommended next phase is receiver
methods and initialization policy, including readonly receiver semantics and the
observable construction sequence, before custom init or managed fields.

Phase 12 is ready for the next hosted CI attempt. All verification above ran on
Linux x86-64; Windows and macOS aggregate calling/layout behavior and their real
editor scenarios still require hosted execution. The unchanged workflow runs the
new 22nd suite automatically in its Debug/Release and Linux sanitizer matrices.
No commits, pushes, branches, publishing or remote modifications were performed.


## Phase 13 — receiver methods and initialization groundwork (2026-10-12)

### Approved scope and implementation

The owner approved **M1 + I1** from RECEIVER_METHODS_PROPOSAL.md. The owner reports
Phase 12 hosted run #18 passed Windows, Linux and macOS at baseline `acf7355`.
This phase implements methods on supported nominal value structs in their owning
source module/file, including calls through direct/aliased imports and project-root
types. Foreign-type extension declarations remain deferred. The implemented
contract is documented in RECEIVER_METHODS.md and the approved additions to Bible
sections 14.2 and 16; M2/I2 alternatives were not silently adopted.

Working syntax is `func (p point) move(dx: i32):` and `p.move(1)`. Ordinary
parameters keep their existing `name: type` syntax and exact contextual argument
checking. Methods support all existing numeric/Boolean/nested value types,
ordinary statements, recursion, aggregate arguments and aggregate returns.

The parser retains receiver expressions until semantic resolution distinguishes
module-qualified calls from value access. Methods share function registration,
argument checking, nominal types, validated bodies and the existing small
expression handlers. Owner/type identity determines lookup; duplicate methods,
field collisions, invalid owners, visibility, receiver/argument/return mistakes
produce structured source-located diagnostics. Unknown methods still analyze
arguments for independent errors. New codes VOID2018–2020 describe invalid
receiver storage, unknown methods and deferred initialization respectively.

Validated receiver-rooted writes seed a finite reverse-call-graph effect analysis.
Indirect nested calls and mutually recursive methods propagate mutability;
independent local and ordinary parameter copies do not alias. Readonly receivers
accept non-mutating calls and reject direct/indirect mutating calls. Non-mutating
calls can materialize temporaries once; mutating calls require actual mutable
local/parameter-rooted storage. Receiver evaluation precedes arguments, which
execute exactly once left to right. Ordinary aggregate arguments remain snapshots.

The one backend-independent IR adds a typed receiver Place (slot plus checked
nested field indexes) and method receiver/effect metadata. The validator checks
signatures, ownership paths and non-mutating receiver writes/calls. Shared LLVM
lowering aliases receiver slot zero to an internal storage pointer; remaining
parameters retain value copies. This avoids delayed copy-back overwriting
argument-side mutations, including replacement of a containing aggregate. JIT
and AOT use the same code and DataLayout, including mixed padded nested values.
No external aggregate C ABI, escaping receiver reference, alias promise, new runtime
system or GC claim was introduced. Existing failure-context checks include method
source locations and caller traces.

Owned analysis/tooling declarations expose receiver kind/type, nominal owner and
inferred effect. Navigation/references, unsaved revisions and LSP diagnostics use
that shared representation. Exact hot-reload compatibility includes method identity,
receiver type/effect and existing signature/visibility/layout/default contracts.
Compatible body edits reload while old leases retain their code/source. Effect
changes require restart and cannot publish. Extension production/lifecycle code
was unchanged; its indentation test now also covers receiver declaration syntax.

### Initialization decisions explicitly deferred

Phase 12 safe defaults, pure declared defaults and runtime named-field construction
remain unchanged. Receiver `init` declarations and `.init()` calls diagnose the
unimplemented lifecycle feature rather than execute as ordinary methods. Custom
`deinit` is also outside this slice. This does not permanently prohibit lifecycle
methods. Compiler checks reject invalid initialization before object output and
preserve an existing output file.

RECEIVER_METHODS.md records the remaining approval gate: named overrides versus
initializer order, initializer parameters versus fields, nested construction/default
rules, typed uninitialized bindings, pure constant/default contexts, lifecycle
visibility, exactly-once publication and initializer compatibility. I2 is an
unapproved proposal only. The recommended next phase is a coherent construction
contract before custom initializer execution; runtime field defaults, managed
reference types and allocation remain future work.

### Final local native validation

All results below are from the final production/test sources, including shared
function/method argument validation and independent unknown-argument recovery.
All **22 original suites remain enabled**, plus receiver_methods_cli, for **23**.
Each complete build succeeded before its full test matrix:

| SDK | Build configuration | Full result | Elapsed test time |
| --- | --- | --- | --- |
| Installed LLVM 19.1.7 | Debug | 23/23 | 18.03 s |
| Installed LLVM 19.1.7 | Release | 23/23 | 34.25 s |
| Installed LLVM 19.1.7 | RelWithDebInfo, ASan/UBSan | 23/23 | 46.50 s |
| Complete official LLVM 19.1.7 archive | Debug | 23/23 | 32.06 s |
| Complete official LLVM 19.1.7 archive | Release | 23/23 | 28.96 s |
| Complete official LLVM 19.1.7 archive | RelWithDebInfo, ASan/UBSan | 23/23 | 38.59 s |

Thus the six complete matrices passed **138 suite executions**. Both SDKs have
verified LLJIT headers, LLVMOrcJIT library and LLVMConfig.cmake, and report version
19.1.7. The installed SDK reports RTTI YES; the complete official archive reports
RTTI NO. The existing consistent SDK-wide RTTI correction was preserved. Fresh
configuration in both Debug trees passed the real `VOID_LLVM_ORC_RUNS` compile/link/
execution probe. Official builds use their matching archive Clang/LLD; installed
builds use installed Clang/LLD 19.1.7. No dependency requirement was weakened.

Build/test commands (the six configured trees retain reproducible CMake caches):

```sh
cmake --build build/ci-debug -j 4
ctest --test-dir build/ci-debug --output-on-failure -j 4
# Repeat build/test for ci-release, ci-official-debug and ci-official-release.
cmake --build build/ci-sanitizers -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j 4
# Repeat the strict sanitizer build/test for ci-official-san.
```

ASan, UBSan and leak checks stayed strict, without new suppressions or exclusions.
Sanitizer processes ran outside the filesystem sandbox so LeakSanitizer can inspect
processes. The installed RTTI-enabled SDK retains vptr instrumentation; the official
no-RTTI SDK retains the established matching configuration, without pretending
vptr checks exist there. Compiler/runtime instrumentation does not imply generated
application machine code is sanitizer-instrumented. Initial LLVM header compilation
emitted its existing `ciso646` deprecation warning with host standard-library headers;
no warning suppression was added.

Both Debug frontend tests ran under the unchanged **1 MiB stack** requirement,
then each passed **five consecutive repetitions** (ten additional suite executions).
This includes the existing Phase 12 recursive layout/resource guards, plus a
180-method effect SCC and a 400-link receiver expression rejected at the nesting
limit with independent later recovery. No resource or stack limits were raised.
Both sanitizer SDKs additionally passed frontend and reload **three consecutive
repetitions each** (twelve additional suite executions). Commands:

```sh
# Run in each of ci-debug and ci-official-debug:
(ulimit -s 1024; ctest --test-dir build/ci-debug -R '^frontend$' --repeat until-fail:5 --output-on-failure)
# Run with the strict sanitizer environment above in both sanitizer trees:
ctest --test-dir build/ci-sanitizers -R '^frontend$|^reload$' --repeat until-fail:3 --output-on-failure
```

The frontend method group checks actual storage, whole/nested replacement, readonly
locals/nested fields/module constants, temporary snapshots, contextual arguments,
parameter/copy independence, evaluation order, owner imports, private/transitive
visibility, same-spelling nominal methods, recursion/effects, parser recovery,
source diagnostics, method navigation, compatible reload/old leases, effect-change
rejection and malformed receiver IR. Existing numeric/diagnostic/GC/native/reload
assertions remain intact.

The new real-source CLI suite reuses the existing native value-source driver.
Five independent projects cover imported nested point/box values, side effects and
snapshots, recursive methods, all ten numeric types plus Boolean/padded aggregates,
and runtime failure traces. It exercises normal/O2 JIT and actual linked AOT,
multiple inputs, emitted-object DWARF validation and rejected custom initialization.
The native LSP suite additionally verifies imported receiver/type definitions,
references, exact readonly ranges, unsaved effect changes and invalid/valid recovery.

### Editor and packaging validation

With Node 24.19.0 and existing pinned dependencies, strict TypeScript checking/build
and **all four extension unit tests passed**. The detailed TAP rerun confirms
individual execution. Local VSIX packaging succeeded:
`editors/vscode/dist/void-language-0.1.0.vsix` (8 files, 145.43 KB).

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
```

Real installed VS Code **1.141.0** passed trusted, restricted-trust and multi-root
profiles using the final native voidls. Actual child-process kills, strict
notification counts, no automatic restart, explicit restart, Unicode/fallback
paths and clean deactivation remain exercised. No fake server or relaxed lifecycle
assertion was substituted.

### Actual source-to-native demonstration and hosted readiness

Executed check, normal JIT, optimized JIT, object emission and matching Clang/LLD
linking against the final compiler, on `examples/receiver_methods/main.void`:

```text
Source checked successfully
301                          # normal JIT, input 10
301                          # optimized JIT, input 10
Wrote native source object: build/phase13-verification/receiver-methods.o
301                          # separately linked native executable, input 10
```

`file` identifies the result as an x86-64 ELF PIE with debug_info. This compiles
real imported .void types, methods, mutation, copies, functions and expressions;
no special-case example IR was reconstructed. Commands are in RECEIVER_METHODS.md.
Final per-tree logs, SDK checks, stress/editor/package evidence, native outputs and
source hashes/results.json are retained locally under build/phase13-verification/.
Git whitespace validation passed; generated evidence/artifacts are ignored outputs.

Phase 13 is **ready for hosted CI**, with approval limited to M1 + I1. All execution
above was Linux x86-64. Windows and macOS receiver pointer/layout/calling conventions,
Debug/Release matrices and real editor scenarios still require hosted execution;
Linux hosted sanitizers must also confirm the official archive environment. The
existing workflow discovers all 23 suites automatically. No commits, pushes,
branches, publishing or remote modifications were performed.

## Phase 14 construction contract proposal — awaiting approval (2026-10-12)

The owner reports Phase 13 `c72e62a` passed hosted run #19 on Windows, Linux and
macOS. The current local HEAD is its subsequent syntax-highlighting correction
`ffa9cdf`; the working tree was clean. Reviewed the current design/technical
documents and the actual parser, parameter/call checker, structural-default
evaluation, typed-local construction, receiver lowering and analysis contracts.

Created INITIALIZATION_PROPOSAL.md, explicitly unapproved. Recommended C1 binds
custom constructor arguments only to initializer parameters, supports typed
positional/named arguments and pure defaults, evaluates supplied expressions
before recursive field construction, and completes child initializers before the
parent body. It describes missing-required-child/default availability, typed local
construction, pure-context rejection, visibility/private factories, once-only
publication/failure propagation and conservative initialization/helper reload
dependencies. Alternatives and limitations are stated for owner selection.

Identified a direct conflict with preserving every historical assertion: one
frontend test and the CLI check/run/emit case expressly reject a valid init
declaration because Phase 13 deferred the feature. The proposal requests explicit
approval for those narrowly scoped expectation updates while retaining direct
lifecycle-call rejection, strict failures and existing-output preservation.

Stopped before compiler behavior changes as explicitly requested for consequential
public construction rules. Only the proposal and this log entry changed. Git
whitespace validation passed; no build/test rerun is claimed for this documentation
gate, and the six Phase 13 matrices are not Phase 14 evidence. Implementation,
INITIALIZATION.md and full Phase 14 validation follow approval. No commits, pushes,
branches or remote modifications were performed.


## Phase 14 — approved custom value initialization and editor correction (2026-10-12)

### Approval, baseline and implemented contract

The owner approved **C1** in INITIALIZATION_PROPOSAL.md, explicitly including
recursive nested construction, required-child default availability, constructor
visibility, conservative initialization/helper reload dependencies and the specific
obsolete Phase 13 rejection-test updates. No I2 alternative was adopted. The owner
reports baseline Phase 13 `c72e62a` passed hosted run #19 on all platforms; this
checkout also includes the owner's subsequent `ffa9cdf` syntax-highlighting change.

INITIALIZATION.md records the working contract. Owner-module custom initializers
accept explicit typed positional/named parameters and pure defaults, with required
parameters before optional ones. Constructor arguments exclusively bind parameters;
no-init types retain named fields. Duplicate/unknown/double binding, order, count,
type, lifecycle result/declaration and access mistakes diagnose before lowering.
Supplied expressions snapshot once in source order before any nested initialization.
Fresh fields construct in declaration order, then init runs once on prepared storage.
Readonly applies on successful completion. Copies/ordinary arguments/returns never
repeat init, and failures do not publish the fresh result or overwrite its destination.

Omitted nested values recursively run available default construction; explicitly
supplied children do not first execute a discarded default initializer. Required
children can be supplied to no-init wrappers. Custom parents must have safe defaults
for all fields before their bodies; invalid declarations identify the field and
required child parameter. Merely declaring/using a required-argument type does not
poison its layout. Typed locals use the same zero-argument construction/access rules.
Pure module/field/parameter defaults cannot execute init, directly or implicitly;
local runtime const construction is valid. Private factories and owner-encapsulated
nested construction work without granting callers private initializer access.

The parser/Function/Parameter/shared nominal Type model owns these rules. One
argument checker performs contextual typing for functions/methods/initializers;
bounded construction plans distinguish pure structural storage from runtime default
availability. Typed slot snapshots, aggregate operations and Phase 13 receiver Places
lower through the unchanged shared LLVM backend. IR initializer signatures and calls
carry explicit invocation metadata and reject ordinary invocation or inappropriate
receiver paths. Existing failure checks guard use of constructed results. No second
frontend, type system, constructor code generator, heap or managed runtime was added.

Shared owned analysis adds initializer kind, parameter/default source metadata,
constructor references and implicit construction dependencies. Implicit edges do
not obscure written navigation/references. Exact compatibility freezes init
presence/signature/labels/defaults/access/effects/body and transitive helper bodies,
using an iterative closure including cycles. Unrelated compatible bodies still
reload, retaining old leases; changes to construction policy require explicit restart.
Existing default/layout/constant/binding rules and source resource guards remain.

### Windows run #20: actual log and isolated correction

Read the completed Windows job **114796125401** through the GitHub API for run
**38244868927**. The whole run was initially still in progress, so run-level logs
were unavailable; the completed job log was retrieved separately and is retained
as ci20-windows.log. At 2026-10-12T11:50:38Z, the presentation unit test failed at
compiled out/test/config.test.js:98 with `keyword.test(word)` false. Three of four
unit tests passed. The first keyword.control.void rule is now the intentionally
approved optional-value/struct rule; `.find()` therefore never examined func/return.
The original negative struct assertion also contradicted intentional highlighting.

Only editors/vscode/src/test/config.test.ts changed for this correction. It filters
all applicable keyword rules, checks func/return and prior working keywords, and
requires exact complete matches for both struct and value struct. Unsupported
while/async/match/defer and misleading prefixes/standalone value remain excluded.
No grammar rules/colors, indentation rules, extension lifecycle or compiler semantics
were changed to resolve the editor regression. The actual native process-kill tests,
notification counts and shutdown/restart behavior remain intact.

### Regression coverage and actual native source programs

All **23 existing suites remain enabled**, plus initialization_cli: **24 total**.
The specifically approved valid-init rejection in frontend/methods.cpp is now
positive source execution. The old CLI deferral fixture now defines a valid init
and attempts an invalid direct p.init() call. Its check/run/emit status/VOID2020 and
protected-output assertions remain, as does direct-call rejection in frontend tests.
No other historical assertion was removed or relaxed.

Seven actual projects under examples/initialization exercise:

- main: imported nested defaults/initializers, named arguments, typed locals,
  readonly/copies, methods, owner-private nested construction and public factories.
- order: source-order side effects, named reordering, once-only argument evaluation
  and omitted pure parameter defaults.
- required: explicit construction of required children in a no-init wrapper.
- numeric: all ten numeric types and Boolean defaults, boundary values and float precision.
- failure: actual initializer-body failure with source/caller trace.
- stages: argument failure before initialization, omitted-child failure and explicit
  child construction that must not execute a redundant failing default.
- copies: aggregate initializer parameters, caller independence, field assignment,
  function parameters and aggregate returns without reinitialization.

initialization_cli runs all seven in normal/O2 JIT and separately linked AOT,
three successful inputs per program (84 success comparisons/configuration), emitted
object DWARF validation and twelve failure/source-order comparisons. Existing native
value/receiver integration cases remain. Frontend checks add precise diagnostics,
required/default/private boundaries, nominal copying, pure contexts, invalid lifecycle
calls/returns, independent recovery, initializer/default navigation metadata, direct
and cyclic helper reload rejection, changed default/access contracts, trivia-safe
and unrelated-body reload and malformed initializer IR rejection.
The actual native LSP protocol suite tests imported initializer definition/references,
exact named-argument diagnostics, stale-query suppression and unsaved repair.

### Final native build/test matrices

Each final matrix below was run only after its own build completed successfully.
These include the final required-child/default/access and aggregate-copy tests.

| SDK | Tree / configuration | Result | CTest elapsed |
| --- | --- | --- | --- |
| Installed, RTTI on | ci-debug / Debug | 24/24 | 34.83 s |
| Installed, RTTI on | ci-release / Release | 24/24 | 31.31 s |
| Installed, RTTI on | ci-sanitizers / RelWithDebInfo, strict ASan/UBSan | 24/24 | 35.72 s |
| Official archive, RTTI off | ci-official-debug / Debug | 24/24 | 29.66 s |
| Official archive, RTTI off | ci-official-release / Release | 24/24 | 25.47 s |
| Official archive, RTTI off | ci-official-san / RelWithDebInfo, strict ASan/UBSan | 24/24 | 31.33 s |

The six final matrices passed **144 suite executions**. Initial concurrent runs
were slower; timings here are verification evidence, not performance benchmarks.
One earlier official Release test invocation overlapped the tail of its build:
three CLI tests encountered a temporarily absent executable during replacement.
That failed harness run is preserved in ci-official-release-overlapped-build.log;
a complete post-build rerun passed, and final build/test orchestration always
awaited a successful build before starting its own matrix. No production workaround
or test weakening was used for that scheduling mistake.

Actual commands, for each installed/official configured tree:

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure -j4
# Also ci-release, ci-official-debug and ci-official-release.
cmake --build build/ci-sanitizers -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j4
# Same strict environment/build/test for ci-official-san.
```

Verified LLJIT headers, LLVMOrcJIT library, LLVMConfig.cmake and llvm-config 19.1.7
for both /usr/lib/llvm-19 and the complete official archive under
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64. Matching Clang/LLD versions and
SDK-wide RTTI policies remain unchanged (installed YES, official NO). Fresh Debug
configuration of both SDKs passed the actual compile/link/execute ORC probe.
No LLVM version, dependency, sanitizer or leak requirement was weakened. Strict
sanitizers ran outside the filesystem sandbox so LeakSanitizer could inspect
processes. The no-RTTI archive retains its established instrumentation policy;
this does not claim unavailable vptr checks. Generated application machine code
is not itself ASan/UBSan-instrumented by the compiler/runtime sanitizer builds.
Earlier full rebuilds emitted the existing SDK ciso646/header deprecation warning;
no suppression was added. Final incremental builds had no new source warnings.

Both final Debug SDKs passed **five consecutive frontend repetitions with a 1 MiB
stack** (ten additional executions). This retains the original Phase 12 layout/
combined-expression regression, Phase 13 deep receiver/SCC guards and the new
initializer checks; limits were not raised. Both strict sanitizer SDKs passed
frontend and reload **three repetitions each** (twelve additional executions).

```sh
(ulimit -s 1024; ctest --test-dir build/ci-debug -R '^frontend$' --repeat until-fail:5 --output-on-failure)
# Same command for ci-official-debug.
# With the strict sanitizer environment above, for both sanitizer trees:
ctest --test-dir build/ci-sanitizers -R '^frontend$|^reload$' --repeat until-fail:3 --output-on-failure
```

### Extension, real editor and native executable evidence

With Node 24.19.0 and existing pinned dependencies, final `npm test` passed strict
TypeScript/build and **4/4 individually executed tests**, including the corrected
keyword assertions. A detailed TAP unit log is also retained. `npm run package`
passed and produced editors/vscode/dist/void-language-0.1.0.vsix (8 files, 145.44 KB).
No publication or native binary bundling was performed.

Real installed VS Code **1.141.0** passed trusted, restricted-trust and multi-root
profiles against native voidls, retaining actual child termination, strict
notification counts, Unicode paths, restart/configuration/fallback behavior,
no automatic restart, no child in restricted/multi-root contexts and clean shutdown.
Compiler-owned constructor navigation/revision checks additionally passed the native
LSP process suite. VSCodium and Windows/macOS editor execution were not available locally.

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
npm run package
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

Actual source-to-native demonstration from examples/initialization/main.void,
linked with matching Clang/LLD 19.1.7:

```text
Source checked successfully
166                          # normal JIT, input 10
166                          # optimized JIT, input 10
Wrote native source object: build/phase14-verification/initialization.o
166                          # separately linked native executable, input 10
```

The result is a Linux x86-64 ELF PIE with debug_info. Commands are in
INITIALIZATION.md. These results compile real imported source, not hand-built IR.
Final build/test/stress/SDK/editor/CI logs, object/executable, source hashes and
results.json are retained under ignored build/phase14-verification/ outputs.
Git whitespace validation passed.

### Remaining limits and hosted readiness

INITIALIZATION.md, the approved proposal, Bible constructor notes, README,
DECISIONS, KNOWN_ISSUES and related analysis/value/compiler docs now distinguish
working C1 from deferred features. Required-child availability and pure default
rules are deliberate approved boundaries. Runtime field defaults, general function/
method named/default parameters, initializer inference/overloads, fallible results,
resource cleanup/deinit, managed reference structs, inheritance/interfaces,
collections/strings and public aggregate ABI remain future work. The experimental
GC has not been presented as a production runtime. A coherent runtime-field-default
or fallible/resource construction design can be considered next, with owner approval;
this phase does not finalize those policies.

Phase 14 and the isolated editor assertion correction are **ready for hosted CI**.
All execution above was Linux x86-64. Windows/macOS native calling/layout behavior,
all hosted matrices and the corrected Windows editor test still require another
Actions run; Linux success is not substituted for those results. The existing
workflow discovers all 24 suites automatically. No commits, pushes, branches,
publishing or remote modifications were performed.

## Phase 14 run #21: macOS ARM64 Debug layout-cache correction (12 October 2026)

### Hosted evidence and investigation before the fix

Read-only GitHub API retrieval inspected run 38252680475 at `424cacb`, macOS job
114822713822 and artifact 11748844018 (`tests-macOS-ARM64`). The job log and
artifact's debug/Testing/Temporary/LastTest.log agree: Debug built; frontend passed
all groups through receiver methods, then aborted after `Frontend begin:
initialization` at DataLayout.h:652, `Idx < NumElements`. The other 23 suites,
including initialization_cli, passed. The run API reports Windows and Ubuntu
success. The artifact contains configuration and CTest logs, **no core/backtrace
or individual initialization-program marker**. It does not identify which of the
six original project examples was active; that hosted source cannot be recovered
from this artifact alone.

Before changing production behavior, temporarily instrumented debug_value_type
with a comparison of native->getNumElements() against
layout.getStructLayout(native)->getMemberOffsets().size(). Linux Debug caught a
real mismatch: /order.void::pair had two current fields but a cached three-field
layout. An isolated run of the original initialization group, including twenty
repetitions, passed locally; the original hosted allocation sequence was not
reproduced by that experiment.

A focused real-source reproducer retained one vd::Jit while compiling and retiring
successive init-bearing cell types through normal/O2 JIT. Field counts follow
1 + (generation * 7) % 17. The first three types have 1, 8 and 15 i32 fields.
Generation 2 constructs cell(value), whose initializer increments f14 and whose
entry returns f14. At input 10 the expected result is 24. The diagnostic caught
**native=15, cached=8** for /initialization.void::cell before execution.

Removed the temporary production guard and captured the original assertion under
GDB, before applying the fix. The unmodified lowering aborted at index 8:

```text
StructLayout::getElementOffset(Idx=8), DataLayout.h:652
StructLayout::getElementOffsetInBits, DataLayout.h:657
lower(...)::debug_value_type, backend.cpp:118
lower(... suffix="__v5"), backend.cpp:133
Jit::compile(... optimize=false), backend.cpp:417
initialization_tests
```

The debugger invocation used the temporarily isolated initialization group:

```sh
gdb -batch -ex run -ex 'bt 22' --args \
  build/ci-debug/void_tests frontend /home/shmellyorc/Documents/Void.Language
```

[LLVM 19.1.7 DataLayout.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/IR/DataLayout.cpp#L720-L739) confirms the mechanism: getStructLayout caches
by raw StructType* and returns a previous entry immediately for an existing key.
The DataLayout copy operation copies target specifications, not the layout cache.
VOID passed the persistent LLJIT DataLayout into lowering, but creates disposable
LLVMContexts per compilation. Once types are destroyed, allocator reuse can give
a new type an old pointer key and retrieve the previous type's field count,
offsets and size. A smaller cached count triggers this exact assertion; a larger
one can silently generate incorrect debug offsets. ASan need not detect this:
the cached StructLayout allocation itself is still live.

This confirms the proposed debug_value_type call site with an actual Linux
assertion/backtrace and specific source/lowering operation. It is a context/cache
lifetime defect, not an ARM64 alignment rule or a collision in VOID nominal names.
All fields in the minimal reproducer are i32; nesting and custom initialization
are not necessary for the cache defect. ARM64's allocation sequence exposes it in
a different original test group. No local macOS execution/backtrace is claimed.

### Small correction and regression coverage

Shared lowering now sets the module's target DataLayout as before, then performs
all layout queries through **module->getDataLayout()**, rather than the borrowed
LLJIT layout. This fresh cache is bounded by the module/context lifetime and
retains the identical target specification. JIT and AOT still share lowering.
No fields, indices, assertions, metadata, compiler semantics, sanitizer settings
or existing tests were removed or bypassed. Temporary production instrumentation
and the isolated-suite shortcut were removed.

The frontend initialization suite adds 64 changing field-count generations and
16 nested mixed-alignment generations, each normal/O2: 160 real source-to-native
compilations/executions on retained JITs. Nested cases alternate i8/i64 beside bool,
i32 and f64, use child/parent initializers and exercise independent copies and
aggregate parameters/returns. Existing AOT integration and DWARF verification are
retained. Program/mode and layout-generation markers now make a future hosted
abort more precise. All 24 suites and the small-stack semantic regression remain.

Evidence is retained under ignored build/phase14-macos-verification/: downloaded
hosted logs, layout-generations-before.log, assertion-backtrace.log, isolated
experiments, focused/full test logs, SDK checks and editor results. The temporary
isolation/instrumentation is not part of the delivered source changes.

### Completed local validation

Focused tests ran first after the cache correction:

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug -R '^(frontend|initialization_cli)$' --output-on-failure
```

Both passed (frontend 4.95s; initialization_cli 2.52s). An initial version of the
new nested fixture correctly failed semantic analysis because its child initializer
required an argument; the fixture now supplies a declared parameter default, as
required by the approved recursive construction contract. No compiler rule was
changed to accommodate the test.

Then reconfigured each existing tree, rerunning the actual LLVM ORC SDK link/run
probe, built it to completion, and ran all 24 suites:

| Build tree | LLVM 19.1.7 SDK / configuration | CTest result | CTest wall time |
| --- | --- | --- | --- |
| ci-debug | installed / Debug | 24/24 | 20.05s |
| ci-release | installed / Release | 24/24 | 17.52s |
| ci-sanitizers | installed / RelWithDebInfo ASan+UBSan | 24/24 | 33.51s |
| ci-official-debug | complete official archive / Debug | 24/24 | 17.99s |
| ci-official-release | complete official archive / Release | 24/24 | 14.16s |
| ci-official-san | complete official archive / RelWithDebInfo ASan+UBSan | 24/24 | 27.20s |

Actual commands per tree were cmake -S . -B build/<tree>,
cmake --build build/<tree> -j2, then
ctest --test-dir build/<tree> --output-on-failure -j4.
Installed SDK root: /usr/lib/llvm-19 (RTTI YES). Official root:
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 (RTTI NO). Matching 19.1.7 Clang/LLD
and complete headers, ORC library and LLVMConfig.cmake were verified; each configure
log records VOID_LLVM_ORC_RUNS success. SDK RTTI matching remains unchanged across
all linked code. Strict runs used the existing sanitizer instrumentation with:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

No sanitizer exclusions or assertion suppression were added. The installed SDK
retains UBSan vptr checks; the official SDK retains its existing no-RTTI ABI policy.
Full suites include normal/O2 separately linked AOT, object debug verification,
source diagnostics, GC/native/hot-reload proofs and LSP/tooling tests.

Both Debug SDK builds passed frontend --repeat until-fail:5 with ulimit -s 1024
(ten executions total; 27.54s installed, 26.23s official). This preserves the
small-stack expanded-layout/resource-limit regression. The command was:

```sh
(ulimit -s 1024; ctest --test-dir build/<debug-tree> -R '^frontend$' --repeat until-fail:5 --output-on-failure)
```

Editor verification used the existing Node runtime and local VS Code:

```sh
cd editors/vscode
npm test
npm run package
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

All four extension unit tests passed. Local VSIX packaging succeeded (8 files,
145.44 KB). Real editor trusted, restricted-trust and multi-root scenarios passed,
including actual native process termination, notification counts, restart and
clean deactivation. No editor production/test code changed for this correction.

Both sanitizer SDK builds additionally passed
ctest --test-dir build/<sanitizer-tree> -R '^(frontend|reload)$'
--repeat until-fail:3 --output-on-failure under the same strict environment:
three repetitions of each of two suites per SDK (twelve suite executions total;
25.41s installed, 24.71s official). No leaks or sanitizer failures were reported.

The correction is ready for the next hosted CI attempt. All new execution evidence
is Linux x86-64. The original macOS artifact cannot identify its exact source
example, and no macOS ARM64 execution is available locally; the identical assertion
was instead reproduced and backtraced on Linux with the focused source sequence.
The next macOS ARM64 Debug frontend run must confirm the repair on the originally
failing platform. Windows/macOS hosted regressions remain necessary; previous
Windows success is baseline evidence, not a test of these local changes. The
existing workflow and all 24 suites remain enabled. No language features, LLVM
version changes, commits, pushes, branches or remote modifications were made.

## Phase 15 investigation and GC runtime proposal, 12 October 2026

**Design only. Implementation is not authorized and has not started.** The owner
states Phase 14 is complete. Inspected current clean baseline `c1b4a66` and the
required Language Bible, Development Strategy, DECISIONS, KNOWN_ISSUES,
INITIALIZATION and prior WORK_LOG evidence. In particular, preserved the approved
non-moving/precise direction, value-copy and C1 initialization rules, separate GC
and code lifetimes, and the open top-level/entry-point discussion. Older pending
hosted statements remain historical; no new hosted result was inferred.

Inspected actual Heap/Root definitions and allocation/tracing, native/reload
runtime, NativeCode/ReloadSlot and compatibility, shared IR types/validation,
LLVM lowering/O2/checked failure paths, root probe, ORC contexts/resource trackers,
AOT adapter/link driver, CLI, real C fixture, concurrent invocation tests and CI.
Relevant implementation is in include/void/{heap,execution,native,backend,ir}.hpp,
src/runtime/, src/backend/llvm/backend.cpp, src/main.cpp, tests/tests.cpp and
tests/integration/. No managed source types or thread registry were found.

### Findings and recommended design

- Heap and explicit Root are single-threaded proofs. The test's thread-local
  probe_heap selector is not collector thread registration. Optimized generated
  root tests cover one stack slot, not general managed source/nested stack roots.
- Normal source IR supports numeric/Boolean and inline value types; it does not
  yet contain managed-reference allocation or safepoint operations. Native imports
  are still trusted i32(i32) proofs. AOT success does not establish GC linkage.
- Graph validation, copied layouts, iterative tracing and rollback before sweep
  are reusable. The current allocation helpers depend on collection occurring
  only at explicit probe polls: their unrooted intermediate allocations must not
  silently become an automatic-GC implementation.
- Existing leases safely retain code through active/native invocations. Preserve
  that scheme; neither GC completion nor replacement publication authorizes
  unloading an active generation. Object tracing descriptors must outlive the
  allocating generation independently; frame maps remain covered by call leases.
- The current C fixture verifies same-thread callbacks only. Its callback fields
  and resource counter do not establish foreign-thread/racing teardown support.

Created [GC_RUNTIME_PROPOSAL.md](GC_RUNTIME_PROPOSAL.md), explicitly awaiting
owner approval. It recommends per-thread explicit root frames, cooperative
Managed/Parked/NativeSafe transitions, an elected requesting collector and one
runtime metadata mutex/CV. Frozen roots and synchronized state transitions permit
tracing only when no mutator is Managed. Old-epoch notifications cannot resume a
thread during a newer request. Native waits/reentry, persistent handles, callback
provider quiescence, scoped code leases, failure cleanup and shutdown have explicit
proposed contracts. No forced suspension, background collector, alternate code
reclamation scheme, managed source syntax or speculative allocator was added.

The proposal compares explicit frames with statepoints and LLVM's built-in shadow
stack. Reviewed versioned LLVM 19 GC/Statepoints/ORCv2/LangRef documentation and
checked exact 19.1.7 installed/official SDK headers for IRBuilder statepoint/result/
relocate APIs, GCStrategy and RewriteStatepointsForGC. API presence is verified;
new statepoint lowering, stack-map consumption and thread protocols are not.
Official documentation links and the six owner decisions G1–G6 are in the proposal.

### Actual verification of the unchanged baseline

Existing build trees were used; no compiler/runtime/editor source, tests, build
configuration, sanitizer settings or workflow was edited or rebuilt for this
investigation. Both llvm-config binaries report 19.1.7. Reran only the following
existing architecture/regression suites to verify the inspected baseline:

```sh
ctest --test-dir build/ci-debug \
  -R '^(core|jit|reload|gc|native|aot|frontend)$' --output-on-failure
ctest --test-dir build/ci-official-debug \
  -R '^(core|jit|reload|gc|native|aot|frontend)$' --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-sanitizers \
    -R '^(gc|reload|native)$' --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-official-san \
    -R '^(gc|reload|native)$' --output-on-failure
```

| Existing configuration | Selected tests | Result | Wall time |
| --- | --- | --- | --- |
| Installed SDK, Debug | core/jit/reload/gc/native/aot/frontend | 7/7 | 5.42s |
| Complete official SDK, Debug | same | 7/7 | 4.98s |
| Installed SDK, strict ASan/UBSan | gc/reload/native | 3/3 | 1.14s |
| Complete official SDK, strict ASan/UBSan | same | 3/3 | 0.98s |

Twenty selected suite executions passed. Sanitizers ran outside the sandbox's
tracing restriction with leak detection intact; installed RTTI/vptr and official
no-RTTI coverage policies remain unchanged. Logs are under ignored
build/phase15-investigation/. These are Linux x86-64 checks of existing behavior,
not evidence for the proposed GC protocol, new foreign-thread callbacks, TSan,
performance, or new Windows/macOS execution. All 24 existing suites remain intact;
no full Phase 15 implementation matrix, small-stack or editor rerun is claimed.

### Stop for owner review

Only GC_RUNTIME_PROPOSAL.md and this log were changed. The proposal includes
acceptance tests, risks and one cohesive implementation sequence, separating the
internal typed JIT/AOT proofs from future managed-language work. Approval is needed
for G1 root representation, G2 coordination/allocation staging, G3 retention,
G4 native/callback contracts, G5 failure policy and G6 bounded implementation scope.
No implementation, commits, pushes, branches or remote modifications were made.


## 2026-10-12 — Phase 15 implementation (owner-approved G1–G6)

Implemented the six-stage sequence in GC_RUNTIME_PROPOSAL.md. This phase adds no
source managed struct, strings, async, finalizers, new CLI entry behavior or native
language feature. No commits, pushes, branches or remote changes were made.

### Runtime and safety invariants

- Added one noncopyable Runtime around the existing noncopyable Heap, one registry,
  coordinator mutex/CV, TLS attachments, Managed/NativeSafe guards, parked epochs,
  persistent handles and explicit closing. Frame publication/removal is allocation-
  free owner-thread work. Runtime allocation requires an already registered output
  root, polls before taking the heap lock, then allocates/publishes without a GC gap.
- Requesting collectors freeze themselves; competing requesters park instead of
  waiting while Managed. Tracing/sweeping holds the coordinator lock only after
  every participant is frozen. CV waits release the lock; collector code calls no
  provider, callback, ORC or application code. A waiter checks the current request
  under the lock and remains frozen if a new epoch starts before it wakes. Joining
  collectors retain the outcome/deadline of their original elected attempt.
- Deadline abort precedes tracing/sweep and releases all waiters. Root enumeration
  or worklist bad_alloc restores idle without sweeping; the existing tracer resets
  partial marks. Deterministic allocator fault injection exercises enumeration and
  partial-mark failures followed by successful recovery. Detectable corrupt roots
  or out-of-order frames terminate; isolated child tests require the terminate
  handler's exit 79 and reject ordinary continuation or sanitizer failures.
- New idle Managed entries are rejected during closing. Existing active scopes,
  NativeSafe continuations and already-admitted callbacks may finish. Cross-runtime
  attachment and Managed-only operations from NativeSafe are rejected before the
  relevant mutation. Destruction with live threads/handles/registrations is fatal.

### Shared compiler, native calls and code lifetime

- Added private nominal managed_ref and validated allocation/field/collection/poll/
  reachability operations to the existing IR. The one IR validator exposes result
  types to the one LLVM lowerer; removed the lowerer's duplicated result-type switch.
  Managed references/operations require coordinated IR; public parsing/analysis does
  not expose them. Internal schemas are bounded, nonempty and acyclic; this does
  not decide future managed source type recursion or empty-object layout.
- Generated root frames describe actual local/receiver storage and fixed parameter/
  expression spill slots. Flattened nested reference maps use the module-owned
  DataLayout, preserving the macOS correction. Slots are initialized before frame
  publication, parameters before polling, and checked exits always unlink frames.
  Entry/block polls preserve numeric-only and managed loop cooperation under O2.
  Allocation and argument/return publication use Managed no-GC handoff windows.
- JIT NativeCode retains an explicit Runtime owner plus its existing generation
  lease. Host dispatch/diagnostic copying runs NativeSafe, including reentry. Exact
  analysis contracts remain intact and are extended with conservative internal
  nominal layouts/runtime identity; changed layout/runtime bindings reject reload.
  Heap descriptors are copied numeric data independent of LLVM contexts/generations.
- Added a same-lowerer runtime object emitter for private AOT proofs; CMake links
  normal/O2 objects with the actual C++ runtime/Threads requirements. ldd on the
  installed Debug AOT executable shows libc, libstdc++, libm and libgcc, no LLVM/ORC.
  The existing CLI adapter/entry behavior is unchanged.
- Added CallbackRegistration with explicit provider unregister-and-quiesce, closing
  admission, in-flight drain, self-drain rejection, same/foreign-thread attachments
  and binding-specific failure translation. Fixture providers own/join all native
  dispatch threads, including dispatch delayed before admission. No arbitrary
  provider's teardown guarantee is assumed. Persistent captures are released only
  after quiescence. Real generated calls block in a native fixture, reenter via an
  actual callback, and retain roots/old leases while collection and reload proceed.

### Full verification — completed stable tree

Both SDKs were rechecked for exact 19.1.7, ORC headers, libLLVMOrcJIT.a and
LLVMConfig.cmake. Every configuration reran the existing real ORC configure probe.
Installed SDK: /usr/lib/llvm-19 (RTTI YES). Official complete archive:
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 (RTTI NO). Project-wide RTTI matching
is preserved. ASan/UBSan remain enabled in both sanitizer trees; RTTI-dependent vptr
coverage is available in the installed configuration, unavailable in the archive ABI.

| SDK | Configuration/tree | Final CTest result | Test time |
| --- | --- | --- | --- |
| Installed | Debug / ci-debug | 31/31 | 20.50 s |
| Installed | Release / ci-release | 31/31 | 17.42 s |
| Installed | RelWithDebInfo + ASan/UBSan / ci-sanitizers | 31/31 | 33.82 s |
| Official archive | Debug / ci-official-debug | 31/31 | 18.21 s |
| Official archive | Release / ci-official-release | 31/31 | 13.31 s |
| Official archive | RelWithDebInfo + ASan/UBSan / ci-official-san | 31/31 | 27.23 s |

The original 24 suites and their assertions remain. Seven added suites are
runtime_coordination, runtime_generated, runtime_callbacks, runtime_failures,
runtime_aot_normal, runtime_aot_optimized and runtime_invariants. Shared normal/O2
JIT and separately linked AOT test nested aggregates, expression temporaries across
allocation, argument/return handoffs, method-created references surviving through
actual nested receiver storage, copy independence, checked failure cleanup and loop
safepoints. AOT also runs four concurrent generated callers. Numeric-only optimized
JIT loops must record actual cooperative parking. The original root/GC, reload,
native, constructor, diagnostics, tooling and frontend assertions all still run.

Commands for each of the six cached SDK/configuration trees:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

The stable-tree final run is build/phase15-verification/validate.py; results.json,
sdk.json and per-tree configure/build/test logs preserve commands/status/timings.
An earlier in-progress build was invalidated while receiver proof sources evolved
(a stale generated AOT object lacked entry_receiver). It was rebuilt; all six final
stable-tree configurations above passed. Earlier logs remain in preliminary/ and
before-native-stress/ rather than being treated as final evidence.

Additional completed checks:

- Both Debug SDKs: ctest -R '^runtime_' --repeat until-fail:10 -j4 — seven suites,
  ten executions each, all passed (20.47 s each).
- Both Debug SDKs: ulimit -s 1024; frontend repeated until-fail:5 — all five passed
  per SDK (24.40 s installed, 22.42 s official). Existing bounded-layout small-stack
  regression remains intact.
- Both strict sanitizer SDKs: ctest -R '^(runtime_.*|frontend|reload|gc|native)$'
  --repeat until-fail:3 -j4 — all 11 suites passed three times, leaks enabled
  (24.28 s installed, 22.88 s official). No sanitizer suppressions or disabled tests.
- Separate runtime-only ThreadSanitizer: clang++-19 -std=c++20 -g -O1
  -fsanitize=thread -fno-omit-frame-pointer -Iinclude tests/runtime/coordination.cpp
  src/runtime/coordination.cpp src/runtime/heap.cpp -pthread; run with
  TSAN_OPTIONS=halt_on_error=1:exitcode=85 — passed, no reported races. This check
  does not instrument the prebuilt LLVM SDK or generated application instructions.
- Release diagnostic baseline, 1000 collections with one already-NativeSafe thread
  holding one frozen object: installed mean 0.201559 us / maximum 2.28 us; official
  mean 0.135120 us / maximum 1.61 us. These fixture measurements are not production
  pause guarantees or a measure of arbitrary Managed rendezvous latency. TSan's
  separate instrumented fixture measured mean 1.62557 us / maximum 34.4 us.
- Extension npm test outside the tracing sandbox: 4/4 individual cases passed.
  npm run package: VSIX produced, eight files / 145.44 KB. A sandbox-only Node run
  reported only a file-level case; the authoritative host run executed all four.
- Real installed VS Code 1.141 Linux integration: trusted, restricted and multi-root
  modes passed; actual native-process termination, notification counts, navigation,
  diagnostics, restart and clean deactivation assertions remain intact. Expected
  injected lifecycle errors appear in its successful test log.
- git diff --check passed. AOT dependency and all extra test outputs are under
  build/phase15-verification; extra.py and extra-results.json record repetitions.

Strict sanitizer and real editor checks ran outside the tracing sandbox so leak
inspection and native editor child processes could execute normally. Requirements
and sanitizer settings were not relaxed. No hosted checks were run or remote state
changed in this phase.

### Remaining limits and next gate

Implementation is complete locally and ready for owner review/hosted CI. Windows
x64 and macOS ARM64 ABI, private root-frame calling conventions, thread scheduling
and native/editor behavior still require the owner's hosted run; Linux results do
not establish those outcomes. VSCodium/minimum-editor-version checks were not run.
Generated application instructions and prebuilt LLVM remain outside C++ sanitizer
instrumentation; architectural observations exercise those roots explicitly.

This is cooperative, nonmoving tracing with serialized allocation, frame-lifetime
retention, no allocation-triggered thresholds and no hard pause bounds. The
coordinator makes tracing safe, not arbitrary simultaneous application field writes.
General source threading/shared-mutation semantics, managed structs/construction,
provider bindings without strong quiescence, strings, async, finalizers, allocator
optimizations, tighter liveness, LTO and statepoints/stack maps remain deferred.
After hosted validation, propose the smallest managed-source type/allocation slice
using these contracts; do not infer approval of new managed language rules.


## Phase 15 run #24 macOS Release numeric-loop correction, 12 October 2026

Inspected the actual [macOS ARM64 job 115048678810](https://github.com/Shmellyorc/Void.Language/actions/runs/38320961702/job/115048678810)
logs through the GitHub read-only connector. The checkout is
`844ce7d201470e83bf6e29a2cc17dc88c41d7e21`, using the cached complete official
LLVM/Clang 19.1.7 macOS ARM64 SDK. Debug passed 31/31 (`runtime_generated`
0.65 s). Release passed 30/31: `runtime_generated` failed after 0.21 s with
`numeric loop never acknowledged a collection`. There was no rendezvous timeout
or LLVM assertion. The subsequent macOS editor steps were skipped. Windows
completed successfully; Ubuntu was still running when job status was retrieved.

### Investigation before tracked edits

The numeric-loop latch only establishes that `signal_running()` ran. The shared
lowerer wraps that import in NativeSafe guards. It establishes neither that the
worker has reentered Managed execution nor that it remains in the numeric loop
when any of the 100 collections starts. All collections can legally complete
while the worker is NativeSafe, or after it finishes. Neither requires a poll to
park. The final cumulative zero-parks assertion therefore has an invalid premise.

A scratch copy of the unchanged test, delaying collection with `worker.wait()`
after the latch, deterministically reproduced the **exact** diagnostic in both
normal and O2 modes, with no production changes. This models a legal preemption
of the collector until worker completion; it does not add a sleep or change the
iteration count. The hosted log does not identify normal versus O2 or capture
the exact interleaving, so those details cannot be claimed for macOS.

Emitted the same numeric IR with `emit_runtime_object` and examined its LLVM
19.1.7 native disassembly. In the Linux x86-64 O2 object, the backedge targets
0x50, containing calls to `vd_gc_poll` at 0x50 and 0x55, followed by the decrement,
comparison and branch back at 0x5f. Entry, initial header and return polls also
remain. Runtime polls are opaque calls with memory effects; the O2 pipeline
retains them. No missing-safepoint compiler/runtime defect was found.
Scratch reproducer, disassemblies and evidence are under `build/phase15-run24/`.
These Linux observations are not macOS ARM64 execution verification.

### Correction and regression strength

Only the test fixture and its build wiring change. The numeric loop has no native
import. A test-local poll wrapper gates the fourth generated poll (the header
reached through the first numeric backedge), while the worker is demonstrably
Managed and before the real poll acquires any coordinator lock. A separate
collector publishes its request; the observer verifies one elected collector is
Parked and both participants are attached before releasing the gate. The real
production poll must then acknowledge the pending rendezvous. The gate is
released on failure too, so joining the worker cannot strand it in test code.

CMake compiles the **existing** `src/runtime/abi.cpp` into this test executable
with only `vd_gc_poll` renamed to `vd_gc_real_poll`. The test wrapper forwards to
that identical implementation. Its ABI definitions satisfy the linker without
extracting the archive ABI object. `llvm-nm` confirms one wrapper, one real poll
and one `Runtime::current` definition. There is no fake collector, alternative
lowerer, production hook or production API change. Other executables, including
both separately linked native AOT tests, retain the uninstrumented ABI.
The test ABI object uses the same `void_options` RTTI/sanitizer requirements.

Both modes retain the original million numeric iterations, result check, 100
successful collections and positive cooperative-parks assertion. Additional
checks require exactly **2,000,003** generated polls, 100 completed epochs,
no pending request and no attached thread leaks. Per-mode evidence makes
future hosted failures distinguishable. A scratch negative control that drops
real polling from the backedge onward fails with the zero-parks diagnostic;
the gate/counting instrumentation cannot substitute for the real GC safety path.
No compiler, runtime algorithm, resource limit, assertion or sanitizer was weakened.

### Local verification after the correction

Fresh configure/build/test runs completed, with all **31/31** existing suites in
all six configurations (186 suite executions):

| LLVM 19.1.7 SDK | Debug | Release | RelWithDebInfo ASan/UBSan |
| --- | --- | --- | --- |
| Installed, RTTI ON | 31/31, 20.48 s | 31/31, 18.26 s | 31/31, 34.71 s |
| Complete official Linux archive, RTTI OFF | 31/31, 19.11 s | 31/31, 13.31 s | 31/31, 27.91 s |

For each of `ci-debug`, `ci-release`, `ci-sanitizers`, `ci-official-debug`,
`ci-official-release`, `ci-official-san`:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

Both SDKs' headers, `libLLVMOrcJIT.a`, `LLVMConfig.cmake`, exact version and RTTI
settings were rechecked; each configure executed the real ORC SDK probe.
Installed sanitizers retain vptr coverage; official sanitizers retain the matching
no-RTTI ABI correction. Leak detection and all strict error settings remain enabled.
`build/phase15-run24/validate.py`, `sdk.json`, `results.json` and the individual
configure/build/test logs preserve these results.

- Focused first: installed Release `runtime_generated` passed, then
  `ctest --test-dir build/ci-release -R '^runtime_generated$' --repeat until-fail:100
  --output-on-failure` passed all 100 (55.07 s). Official Release passed the same
  100 repetitions (51.85 s). Each execution exercises both normal and O2 JIT.
- Both Debug SDKs: all seven `runtime_` suites passed ten repetitions each.
- Both Debug SDKs: `ulimit -s 1024; ctest --test-dir build/<tree> -R '^frontend$'
  --repeat until-fail:5 --output-on-failure` passed all five per SDK.
- Extension `npm test`: 4/4 individual tests passed. Real installed Linux VS Code
  integration passed trusted, restricted and multi-root modes, including native
  child-process lifecycle assertions. `npm run package` produced the eight-file,
  145.44 KB VSIX. No extension files changed. Host/package logs are in the same
  evidence directory. Native editor and strict sanitizer checks ran outside the
  tracing sandbox; their requirements were preserved.

- Both strict sanitizer SDKs: `ctest --test-dir build/<tree> -R
  '^(runtime_.*|frontend|reload|gc|native)$' --repeat until-fail:3
  --output-on-failure -j4` passed all 11 suites three times, including leaks
  (26.48 s installed, 24.75 s official). Both Release coordination/stress fixture
  executables also passed. `extra.py`, `extra-results.json` and per-check logs
  record these additional checks.
- Final `git diff --check` passed. Only CMake test wiring,
  `tests/runtime/generated.cpp`, `docs/GC_RUNTIME.md` and this work log changed.

The correction is ready for the owner's next hosted CI attempt. Local execution
was Linux x86-64; no local macOS ARM64 or Windows execution was available. Their
build/link behavior for the test ABI object, optimized poll counts, deterministic
rendezvous and complete 31-suite matrix still require hosted confirmation. The
actual macOS failure is explained by a confirmed test synchronization defect;
its precise normal/O2 interleaving was not logged. The new test directly checks
that generated loop safepoints execute the real coordinator on those hosts.
No production compiler/runtime behavior, LLVM version, GC contract, source
language feature, commit, branch or remote state changed.
