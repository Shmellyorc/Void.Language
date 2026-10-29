# LLVM SDK RTTI and sanitizer compatibility

## Current LLVM 23 migration — 28 October 2026

The [migration](llvm23/IMPLEMENTATION.md) retains matching SDK RTTI throughout
all C++ consumers, project exception handling, strict ASan/UBSan/leak checks
and host-generated function-type metadata. Actual LLVM 23 SDK settings and local
outcomes, including effective **358/358** strict tests after retained harness
corrections, belong in [compatibility](llvm23/COMPATIBILITY.md) and
[acceptance](llvm23/ACCEPTANCE.md). The LLVM 19 comparison and commands below
remain historical evidence, not active build instructions or LLVM 23 results.


## Run #7 investigation, 11 October 2026

Read the actual [Ubuntu job log](https://github.com/Shmellyorc/Void.Language/actions/runs/38120807013/job/114414960372).
Debug and Release passed 10/10. Sanitizers passed only core and AOT: JIT,
reload, GC, native, frontend, modules, source_cli and modules_cli failed.
The first six failures were in libstdc++ future-result cleanup, reached through
ORC's `ExecutorProcessControl::callWrapper` and EH-frame registration. The CLI
failures were in `shared_ptr<const vd::NativeCode>` control-block cleanup.
These were UBSan invalid-vptr reports, rather than ASan allocation failures.
The local unchanged baseline and hosted run both use commit
`384307dcdb65b476de587c4681e047b9f7dc3acc`.

The complete official Linux archive exports `LLVM_ENABLE_RTTI=OFF`; the installed
19.1.7 SDK used for earlier local sanitizer verification exports `ON`. Both export
`LLVM_ENABLE_EH=OFF`. VOID retains its own exception handling for diagnostics.
Previously only `backend.cpp` received `-fno-rtti`, while the frontend, runtime,
CLI and tests used the compiler's RTTI-enabled default.

Two concrete mechanisms explain the reported failures:

1. `backend.cpp` creates `NativeCode`, generation and JIT ownership control blocks
   with `make_shared`. Their no-RTTI vtables cross into RTTI-enabled callers.
   UBSan's dynamic-type checks cannot validate their missing type information.
2. The archive's ORC implementation creates `std::promise` result objects without
   RTTI. Standard-library template cleanup from the instrumented test executable
   can be selected for those objects at link time. The test executable also uses
   `std::async` for the existing active-call reload proof. Standard-library
   instantiations are shared across translation units, not isolated by the source
   file that includes LLVM headers.

Disassembling the actual archive member `libLLVMOrcJIT.a(LLJIT.cpp.o)` with its
matching `llvm-dis` showed this future-result vtable:

```text
std::__future_base::_Result<llvm::orc::shared::WrapperFunctionResult>
[null offset-to-top, null typeinfo, _M_destroy, destructor, deleting destructor]
```

The null typeinfo is already present in the prebuilt bitcode, before VOID runs.
A two-file standalone reproducer, containing only a `make_shared` factory and
consumer, reports the same invalid-vptr error when the factory uses `-fno-rtti`
and its consumer uses RTTI with UBSan. The live value remains 42 and the vtable's
typeinfo slot is null. Building the factory with RTTI produces non-null typeinfo
and passes strict ASan/UBSan with leak detection. No LLVM or VOID code is needed
to reproduce that mechanism.

The unchanged complete-archive build reproduced all eight failing suites locally
(2/10 passed, 2.42 seconds). Local headers are GCC 16; the hosted reports use GCC
14. The matching Clang and LLD are 19.1.7 in both cases. This reproduces the
metadata incompatibility across two host libstdc++ versions; it does not require
a CI-only allocation corruption explanation. It is not a claim that unrelated
memory bugs cannot exist.

## Build correction and coverage

The shared `void_options` target now applies the selected SDK's RTTI setting to
**all linked C++ targets**, including tests and the CLI. C compilation is excluded
from these C++ options. The existing SDK configure probe uses the same setting.
MSVC uses `/GR` or `/GR-`; other supported compilers use `-frtti` or `-fno-rtti`.
This policy applies to Debug, Release and sanitizer builds, rather than changing
the ABI only to obtain a sanitizer pass. No compiler/runtime/frontend behavior or
test assertion was changed.

ASan/UBSan compile and link flags, leak detection, strict report exits, JIT entry
function-type metadata and all ten tests are retained. There is no sanitizer
ignorelist, runtime suppression or `-fno-sanitize` option.
Both SDKs contain prebuilt, uninstrumented LLVM libraries; these builds instrument
VOID and the C++ templates it emits, rather than rebuilding all LLVM with sanitizers.

**Coverage limit:** Clang 19's vptr check requires RTTI and cannot operate in the
official archive's no-RTTI ABI. The configure log now says so explicitly. Matching
the archive does not provide vptr coverage; the complete RTTI-enabled installed
19.1.7 SDK remains the separate local configuration for that coverage. A hosted
vptr lane would likewise require a complete RTTI-enabled 19.1.7 SDK, not just an
RTTI flag added to consumers of the no-RTTI archive. See the
[versioned Clang documentation](https://releases.llvm.org/19.1.0/tools/clang/docs/UndefinedBehaviorSanitizer.html#available-checks)
for the incompatibility. LLVM's
[CMake package documentation](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project)
defines `LLVM_ENABLE_RTTI` as the selected SDK's build setting.

Deliberately invalid standalone programs verified that heap-buffer-overflow,
signed-overflow and incorrect indirect-function-call reports remain fatal in both
RTTI modes. An invalid downcast additionally verified vptr detection in RTTI mode.
All seven controls produced their expected report and exit 86 with the strict
combined runtime settings. ASan and UBSan share common runtime options here, so
the address control also exits 86; the workflow's requested 87/86 values should
not be interpreted as guaranteed distinct sanitizer-family exit codes. Both
remain distinct from the expected source-error exit 1.

## Reproduction and evidence

Use the same complete official `LLVM-19.1.7-Linux-X64.tar.xz` as CI. Set `sdk` to
its extracted directory, preserving the Clang++ driver filename when invoking it:

```sh
sdk="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64"
cmake -S . -B build/ci-official-san -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOID_SANITIZERS=ON \
  -DCMAKE_C_COMPILER="$sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
  -DLLVM_DIR="$sdk/lib/cmake/llvm" \
  -DVOID_CLANG="$sdk/bin/clang" -DVOID_LINKER="$sdk/bin/ld.lld"
cmake --build build/ci-official-san --parallel 1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure
```

Ninja in CI and Unix Makefiles locally use the same compiler/SDK/ABI settings.
Local sanitizer execution requires leaving the tracing sandbox so LeakSanitizer
can inspect the process; leak checking stays enabled. Local evidence is retained
under ignored `build/ci7-verification/`: downloaded job logs, before/after build
and test logs, SDK settings, the archive vtable excerpt, two-file reproducer and
sanitizer failure controls. WORK_LOG.md records the completed validation results.

Debug and Release were also built and tested against both SDKs (all six
configurations passed 10/10). The official normal-build directories are
`build/ci-official-debug` and `build/ci-official-release`; select Debug or Release
and `VOID_SANITIZERS=OFF` with the same explicit SDK/compiler/linker paths above.
Installed-SDK directories are `build/ci-debug`, `build/ci-release` and
`build/ci-sanitizers`, configured with `/usr/lib/llvm-19/lib/cmake/llvm`,
`/usr/bin/clang-19`, `/usr/bin/clang++-19` and matching installed LLD. Installed
builds used `--parallel 2`. All normal test invocations use the same
`ctest --test-dir DIRECTORY --output-on-failure` command; both sanitizer builds
use the strict environment shown above. Repeated every archive sanitizer suite:

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure --repeat until-fail:3
```

All 30 repeat executions passed in 89.26 seconds. The isolated RTTI reproducer
and seven fault controls also passed their expected-result checks. No new linker
backtrace or sanitizer report occurred in the positive verification runs.

The Ubuntu 24.04/GCC 14 runner and Windows/macOS still require a new hosted run
of this correction after the user commits and pushes. No remote run was started
or remote repository changed during this investigation.

## Hosted confirmation and Phase 6

The user reports commit `4f3688d` passed Actions run #8 across Linux, Windows and
macOS. The SDK RTTI correction above remains intact during Phase 6. Numeric
expansion adds two suites without changing sanitizer flags, report handling or
coverage claims. Both complete SDK configurations are revalidated locally; see
WORK_LOG.md. Phase 6 hosted verification still requires a subsequent user push.
