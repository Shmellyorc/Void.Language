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
