# LLVM 23.1.4 migration — engineering record

**Current continuation, baseline `5198c7ac9c256fd05f53ff5ba594c38dc0093b7f`:**
[CI 40861888611 native failure inventory and repairs](NATIVE_CI_TRIAGE.md)
supersede the pending native-build status in the historical notes below. All four
native builds now succeeded in that baseline run; Windows had 58 failed tests
per configuration and macOS had 12. The complete inventory is reconciled against
eleven cause families. Feasible repairs are implemented, Linux Release passes
369/369, focused Debug passes 26/26, and focused ASan/UBSan passes 11/11.
Native confirmation of these new changes remains required. Windows persistent
caching remains deliberately unavailable; native macOS filename creation/errno
and stack-limit observations are explicit acceptance gates. Stop for independent
Golden Wrench review after the documented local verification; Boss owns Git/CI.

The [CI 40844952491 native build continuation](NATIVE_BUILD_REPAIR.md), against
`cb54dcc40cf2da712ab480bd7dbc4b49e898df1b`, records successful native configuration
followed by cp1252 fixture-generation and missing Apple `getentropy` declaration
failures. The two focused source corrections and local evidence are complete;
native build/test acceptance of those changes awaits Boss-controlled CI.

2026-10-29 follow-up against `a4ca3e585db945570b22c890b086fa6a53692a5f`:
the [cross-platform configuration repair](CROSS_PLATFORM_REPAIR.md) supersedes
the initial Windows CRT and Apple standard-library assumptions below. It records
inspection of both actual official SDK archives, pinned Windows dependencies,
Apple vendor-header selection, executed Linux checks, native acceptance commands
and remaining platform uncertainty. Repairs stop for independent Golden Wrench
review; hosted native success is not claimed.

28 October 2026. Boss authorizes the complete toolchain migration, local checks
and internal documentation. The migration is locally complete and stops for
independent Golden Wrench review; no hosted/platform release lock is claimed.
Final results and limits are in [acceptance](ACCEPTANCE.md). The sequential
checkpoints below retain their original engineering state and first failures.
No Git mutation, hosted CI, root README edit or unrelated logging work is authorized.

## Preserved starting state

HEAD: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
`evidence/initial-identity.json` records tracked input hashes and the nine pre-existing
dirty logging documents. The original local document diff and LLVM 19 CMake caches
are retained separately. Those dirty documents are not migration edits.
The LLVM 19 SDK and historical crash executables, cores and evidence remain in place.
The DSE incident remains unresolved; an upgrade passing does not establish its cause.

## Execution plan and recovery checkpoint

1. Verify/install the official complete SDK beside LLVM 19; retain SHA-256 provenance.
2. Migrate active pins, ABI manifests, packaging, platform helpers and LLVM APIs.
3. Configure fresh Release/Debug/strict build directories with the same shared
   pre/post verification and standard O2 pipeline, without optimizer workarounds.
4. Repair demonstrated integration failures; run focused checks, then consolidated
   full local suites, hard 1 MiB execution, editor/install/AOT and bounded metrics.
5. Reconcile changed paths, evidence, compatibility risks and acceptance for Wrench.

Historical engineering checkpoint: official release metadata saved in `evidence/official-release.json`;
Linux archive downloading under `.toolchains/downloads/`. Active version pins and
native manifest checks are migrated. SDK installer now verifies official archive
SHA-256 identities, including cache provenance. APIs and native platforms are not
yet validated. Existing LLVM 19-specific libc++ experimental stop-token fallback
is removed in favor of the actual C++20 compile/link probe (libc++ 23 exposes it
without that opt-in). Windows DIA dependency resolution is retained.

## SDK and first integrated checks

Official Linux SDK SHA-256:
`25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
Installed separately at `.toolchains/llvm-23.1.4`; LLVM 19 remains untouched.
LLVM, Clang and LLD report 23.1.4 / commit
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. CMake's real ORC host probe passes.
SDK RTTI/EH are OFF; all VOID C++ consumers retain matching no-RTTI and project
exception handling, using host libstdc++ with CXX11 ABI 1. An inspected
`libLLVMOrcJIT.a(LLJIT.cpp.o)` starts with native ELF bytes, unlike the LLVM 19
bitcode member. No reliance on a system LLVM development package is intended.

Supported API migrations so far: typed `Triple` at target lookup/creation and
module assignment; `Intrinsic::getOrInsertDeclaration`; `IRBuilder::CreateGlobalString`
(the opaque pointer directly addresses the same terminated global data); explicit
instruction iterator insertion. Shared `finish` remains unchanged: verify, normal
O2 with no TargetMachine passed to PassBuilder, then verify. Default native codegen
remains distinct. `evidence/llvm23-standard-O2-pipeline.txt` captures the actual
23.1.4 default recipe (SLP remains off in this direct-builder configuration).
ORC resource trackers and generation leases are retained.

The native manifest producer/consumer and fake Windows test driver require
`VOID native 23.1.4 v1` / matching tool versions. Existing cache identity already
hashes the LLVM version and linked compiler/LLVM libraries; no second cache or
identity authority was introduced. Staging, Windows private link workspace,
validated transfer, same-parent atomic publication and cleanup remain unchanged.

Preserved preparation/engineering failures:

- Initial configure passed ThinLTO cache flags before selecting LLD for CMake's
  early compiler check. GNU ld rejected them. The next local command explicitly
  selected the same pinned LLD for that early check; no project policy changed.
- First backend compile rejected removed `CreateGlobalStringPtr` calls; corrected
  using the SDK's supported opaque-pointer `CreateGlobalString` interface.
- Initial manual native-build check incorrectly expected empty stdout instead of
  the existing `Built native executable: ...` confirmation. The compiler returned
  success. Original JSON and recorder incident `incident-77ivbc3a` remain retained;
  this is an incorrect manual oracle, not a compiler crash. The corrected
  reproducible six-check script passes normal/O2 JIT, native builds and separately
  launched native executables, all on hard 1 MiB stacks.

Historical engineering checkpoint: Release compiler/server build succeeds; Release all-target
build is running. Debug with optional LLVM research enabled is configured.
Complete suites, strict build, editor integration and bounded metrics remain pending.

## Test-envelope correction discovered by full Release validation

The first complete Release selection exposed a pre-existing shared recorder bug:
`run(..., env=None)` passed None into launch-source discovery, which attempted
`environment.get(...)` before any child was launched. JSON's helper explicitly
passes that default. This is a Python harness failure, not a recovered compiler
crash. The original failed test output remains in `release-ctest.txt`.

A single normalization at shared `_sources` now matches subprocess semantics:
None inherits the parent environment; an explicit empty mapping stays empty.
Both `run` and direct `Popen` reuse that path. The existing synthetic preservation
selftest now verifies both environment forms at both process boundaries, alongside
all original crash/timeout/source-snapshot checks. Its focused run passes and adds
no repository incident artifacts. Affected full-suite tests require a focused
rerun; no expectations or negative cases are relaxed.

Optional research source syntax checks also identified and adapted MC register/
subtarget reference returns, merged TargetMachine interfaces, optional stack
alignment, and iterator insertion. The two optional backend files and standalone
ABI target-facts/machine emitter now pass LLVM 23 syntax checks. Their native
acceptance remains pending the Debug research-enabled build and tests.

## Additional inherited acceptance defects (not LLVM regressions)

The original Release workflow assertion requires a source-attributed `index`
diagnostic from invalid scalar-list access. A controlled comparison of the
preserved LLVM 19 compiler and new LLVM 23 compiler produced byte-identical
`Array operation failed` diagnostics in normal/O2 JIT and independently linked
AOT. The prior scalar-list fast path had lost the operation context. The smallest
correction retains Runtime bounds checking and source attribution and identifies
that failing operation as `Array operation failed during index access`.
Existing test assertions and safety checks are unchanged. The comparison receipt
is `evidence/application-diagnostic-comparison.json`.

Math's existing preservation adapter also lacked forwarding for the shared
recorder's modern keyword context (`failure_kind`, launch snapshots, environment).
The adapter now forwards that metadata to the existing recorder. Its unchanged
three-incident synthetic preservation test passes, including exact edited nested
source bytes, original output, compiler provenance and timeout retention.

The initial Release run remains an engineering run, with original failures
retained; a complete post-integration local acceptance remains required. Debug
research-enabled all-target build is running against the corrected source.

## LLVM 23 optional codegen setup and conservative audit boundaries

The optional manual machine emitter initially reported dynamic roots after a
`static-memset-post-expansion` block split. Its existing early-object evidence was
extended with owned block names and static-allocation flags. LLVM 23's stock
`CodeGenTargetMachineImpl::addPassesToGenerateCode` explicitly installs both
TargetLibraryInfoWrapperPass and target/options-aware RuntimeLibraryInfoWrapper.
The default legacy runtime-library wrapper instead uses an unknown triple; the
manual emitter had omitted the new setup. Adding the same stock analysis setup
to both optional manual emitters preserves normal host libc memory lowering.
Ordinary production `addPassesToEmitFile` and ORC `addPassesToEmitMC` already install
it internally, and were not replaced. The shared IR optimization recipe is unchanged.

Pinned source evidence:
[stock codegen setup](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/lib/CodeGen/CodeGenTargetMachineImpl.cpp),
[runtime analysis default](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/lib/Analysis/RuntimeLibcallInfo.cpp),
[memory expansion](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/lib/CodeGen/PreISelIntrinsicLowering.cpp).

LLVM 23 O2 also inlines `node.add` into the managed `main` fixture's `make_cycle`,
creating four homes outside the original inventory. The research certifier still
rejects unproved clone/merge ownership; no lineage was guessed and no subtraction
credit granted. The Linux x64 fixture now requires exactly four identical clone
rejections and unchanged owner bytes, while normal `main` and both `handoffs`
profiles still certify. A virtual-tail fixture's IR-seed attempt likewise now
requires exactly four clone rejections plus one encoded-call mismatch and unchanged
seed bytes. Its existing separate outlined-dispatcher negative remains intact.
These are exact updated negative expectations for experimentally observed LLVM 23
output, not permissive success-or-rejection tests. Ordinary linked execution of
those valid programs remains required. No inliner switch, NoInline production
attribute, certification exemption or new ownership framework was introduced.
The focused research auditor now passes after those adaptations; the full Debug
research selection remains a final acceptance requirement.

## Inherited entry-emission coherence failure

The first complete Release selection ended at 348/368. Besides the recorder and
index-context defects above, console integration exposed pruning of unreachable
custom-equality declarations while retained type metadata still advertised them.
The preserved LLVM 19 and new LLVM 23 CLIs both accept source checking and then
reject entry execution with the same typed-IR metadata error. The comparison is
`evidence/equality-pruning-comparison.json`; this is not attributed to LLVM 23.

The existing entry reachability walk now treats custom-equality declaration
families like its existing virtual dispatch metadata roots. It retains their
ordinary dependencies and keeps unrelated functions pruned. The validator is
unchanged. A focused cache/entry test checks the retained equality declaration,
unrelated-function pruning and normal/O2 execution. Console's original complete
stdout/stderr and cleanup assertions remain unchanged.

Historical engineering checkpoint: all three initial builds complete, including strict
ASan/UBSan/leaks and Debug optional research. Focused corrected-source tests are
running. Incremental rebuilds/source freeze and complete final profile acceptance,
editor/install/native validation and bounded performance comparisons are pending.

## Console cold-cache fixture repair and final source freeze

The corrected Debug compiler passed all 22 console source cases with exact normal/
O2 JIT and independent AOT output/cleanup assertions. The test then reached an old
cold-cache assumption: earlier ordinary `--no-opt` runs had already populated the
same cache after automatic caching was introduced during stabilization. Both
explicit `--cache` calls consequently reported hits. The original failure remains
in `debug-focused-5.txt`.

The test now clears only its own temporary-root cache before each cold/corruption
fixture, keeping exact miss/hit/corruption assertions and adding an explicit first
miss assertion. That also prevents choosing an unrelated existing cache object
for corruption. Production cache behavior is unchanged. The first integration
freeze is preserved; `integration-source-freeze-final.json` adds this single test
setup correction. No later production/test mutation is currently planned.

Initial Release all-target build and initial strict all-target build both complete.
Final corrected Release build completes; final Debug/strict incremental rebuilds
are in progress. Consolidated final profile suites and bounded measurements remain
pending. Research and cache focused regressions pass; original failures remain
recorded rather than replaced with successful retries.

## Integrated builds, editor fixtures and cache boundaries

All corrected-source Release, Debug (including optional LLVM research), and strict
ASan/UBSan/leak all-target builds complete. The final consolidated Release suite
is running with two independent test processes; Debug and strict follow sequentially.
The integrated production source is frozen; acceptance receipts retain both the
initial and subsequent test-fixture freeze identities.

Both existing headless Vim/Neovim editor fixtures copied `console` and `time` into
a private fake standard library but omitted their `error` dependency. The smallest
fixture correction copies that one existing module, preserving exact completion
order and all assertions. It affects `tests/editors/run.py` and the VS Code host
launcher, not production semantics or adapters. Original Neovim failure is retained.
Automatic-main and explicit-entry headless tests now pass on Neovim
0.13.0-dev-1410+g510f61555b and actual Vim 9.2 (`vim.basic`, not the host's `vim`
symlink to Neovim). Real PTY typing also passes all five import completion prefixes
in both editors and documented/undocumented argument popup transitions in Vim.

VS Code 1.143.0 runs the real host suite in isolated trusted, restricted-trust and
multi-root profiles. Both the development extension and locally installed target
VSIX pass; restricted/multi-root cases preserve the no-native-child guarantees.
The existing four extension unit tests and final TypeScript build pass. No user
profile is changed. These provider/host tests do not claim a new visual popup/CDP
observation, VSCodium execution, or a native Windows/macOS editor pass.

The private tooling archive includes the actual LLVM 23.1.4 notice and dependency
audit and starts the extracted server with an ordinary OS PATH, completing the
existing protocol handshake. No artifact is published. Existing cross-version
cache identities are experimentally distinct: 19 cold/hit, 23 cold/hit, then 19
hit again, with original object bytes preserved. A private copy of the new CLI
rejects `VOID native 19.1.7 v1` before replacing an owner output sentinel.
Receipts: `cross-version-cache-native.json`, `tooling-package.txt`,
`extension-{unit,host,installed-host}.txt`, and editor JSON/terminal receipts.

Measurement preparation also rejected an invalid temporary logging variable
that shadowed the imported `logger` type. Both compilers diagnosed it consistently;
only the private benchmark fixture was renamed. Initial and corrected source
checks are retained. This is not a production compiler defect or crash.

## Numeric integration repeated-run fixture discovery

The final Release run found another inherited cache-era test setup issue:
`numeric_cli` globs `*.void`, and CMake includes matching directories by default.
The first suite populated a sibling `.void/cache/v1`; the repeated suite treated
that `.void` directory as an extra program, produced an empty basename, and failed
project discovery before code generation. A diagnostic enumerates 64 actual
source files versus 65 matches including the cache directory. The glob now uses
`LIST_DIRECTORIES false`; every actual program and positive/negative assertion
remains required. No compiler or cache behavior changes. The original failure
is retained in `release-final.txt`; a focused numeric rerun is required. Subsequent
Debug/strict consolidated runs use the corrected source-file discovery.

## Release consolidation and actual stack gates

The integrated Release suite completed at **367/368** in 1068.69 seconds. Its sole
failure was numeric source discovery including the new cache directory. The exact
corrected numeric suite passes **1/1** in 13.00 seconds, giving effective Release
coverage **368/368**; the failed original XML/text are retained separately from
this rerun. All 64 numeric source programs and their normal/O2 JIT/independently
linked AOT outcomes remain required. This is not represented as an initially
clean run.

Both frontend gates pass under actual hard and soft 1 MiB limits: Release
**1/1**, 3.79 seconds; Debug **1/1**, 9.29 seconds. The complete Debug/research
selection of 381 cases is running, followed sequentially by all 358 strict cases
with leak, ASan and UBSan failures fatal. Commands and statuses are recorded in
`remaining-acceptance-status.json`. No production source changed during acceptance.

A final active-documentation audit also updated the editor README and present-tense
probe requirements. Historical LLVM19 measurements and original MIR commands
remain explicitly historical. The current storage-proof reproduction uses the
accepted handoffs subset and explicitly records LLVM23 main O2 clone rejection.
The owner-local VSIX was refreshed for its corrected LLVM23 README; its executable
extension/server payloads are byte-identical to the installed-host-tested archive.
Only README text and ASLR addresses in a fresh dependency audit differ. The initial
comparison's over-strict metadata assertion is recorded in
`package-readme-reconciliation.json`; no native or editor failure occurred.

The actual CTest JSON inventory confirms **358** strict cases, correcting the
earlier anticipated count of 368. The unchanged existing sanitizer configuration
omits ten duplicate stack selections; Debug/Release keep them. It introduces no
new exclusion. Exact differences and all executable commands are retained in
`test-inventory-comparison.json` and the three profile inventory receipts.

## Local build reproduction

Use fresh LLVM23 build directories; do not reconfigure the preserved LLVM19 trees.
The SDK location and all native tools are explicit:

```sh
void_llvm23_sdk="$PWD/.toolchains/llvm-23.1.4"
cmake -S . -B build/llvm23-release -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$void_llvm23_sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$void_llvm23_sdk/bin/clang++" \
  -DLLVM_DIR="$void_llvm23_sdk/lib/cmake/llvm" \
  -DVOID_CLANG="$void_llvm23_sdk/bin/clang" \
  -DVOID_DWARFDUMP="$void_llvm23_sdk/bin/llvm-dwarfdump" \
  -DVOID_LINKER="$void_llvm23_sdk/bin/ld.lld" \
  -DCMAKE_EXE_LINKER_FLAGS="--ld-path=$void_llvm23_sdk/bin/ld.lld -Wl,--thinlto-cache-dir=$PWD/build/llvm23-thinlto-cache"
cmake --build build/llvm23-release --parallel 3
```

Debug uses `build/llvm23-debug`, `-DCMAKE_BUILD_TYPE=Debug` and
`-DVOID_LLVM_RESEARCH=ON`. Strict uses `build/llvm23-strict`,
`-DCMAKE_BUILD_TYPE=RelWithDebInfo` and `-DVOID_SANITIZERS=ON`. Both select the same
SDK/tool paths and explicit LLD; neither adds the Release diagnostic ThinLTO-cache
flag. That flag does not request LTO or change LLVM's IR O2 pipeline. Configure
receipts include the actual linked ORC result; final CMake caches/commands remain
in their distinct build directories. Acceptance commands and strict environment
are recorded separately. No installed system compiler or LLVM version is substituted.

## Complete Debug acceptance

The full Debug/research profile passes **381/381** in **3846.39 seconds**. All 13
optional research cases pass (including exact fail-closed LLVM23 clone handling),
alongside ordinary source checking, normal/O2 JIT, independent AOT, Runtime/GC/
ownership/async/callbacks/globals/cache/reload/LSP, real stack and installed-module
checks. The final console/AOT suite passes in 425.92 seconds, including all cold/
hit/corruption checks. Four existing serialized installation selections finish
successfully. No compiler/test source changed during this consolidated run.
Original Release engineering failures and corrected focused receipts remain
separate. Strict **358-case** ASan/UBSan/leak validation has now started against
the same integrated source. Bounded comparable measurements follow when it ends.

## Strict channel AOT link setup correction

Strict validation exposes two inherited test-link errors after **49,708 channel
host checks already pass**. `tests/channel/{host,execution}.py` infer sanitizer
flags from the case-sensitive substring `san` in the executable's build directory.
The valid directory `llvm23-strict` does not contain it (neither does CI's capitalized
`Sanitizers`). Both independent native links consequently omit sanitizer runtimes
while linking instrumented Runtime archives, yielding undefined `__asan_*` and
`__ubsan_*` symbols. This is not an ASan finding, compiler crash or LLVM optimizer
defect. Exact original commands/errors remain in `strict-final.txt` and the
channel invocation receipts.

Both call sites now reuse one helper in the existing channel support module. It
reads the existing compiler-owned native manifest, including sanitizer and native
platform library/framework flags, and resolves installed relative archive paths.
No directory heuristic, renamed build, disabled instrumentation, relaxed assertion
or production change is introduced. This matches the already-working collections
harness approach. Existing strict cases continue, then the two corrected channel
cases require a focused rerun. The final accepted source freeze adds these three
test-only files; earlier freezes and the full Debug result remain preserved.

## Completed strict correctness selection

The complete strict ASan/UBSan/leak profile finishes **356/358** in **3434.55
seconds**, with only the two identified channel harness-link failures. Both
corrected cases pass **2/2** in **92.92 seconds**, including normal/O2 independent
AOT and the full 49,708-check host selection. Effective strict coverage is
**358/358**. Fatal sanitizer/leak settings are unchanged; no sanitizer finding,
compiler SIGSEGV or production fix is associated with these link failures.
The original full failed selection and focused corrected selection remain separate
XML/text records. Release remains effective 368/368; Debug full 381/381; both
frontend hard 1 MiB gates and external editor/package checks pass.

Exactly twenty serial LLVM19/23 representative embedding measurements now run
after consolidated validation is idle, followed by the three bounded retained-input/
source-associated DSE checks. The original clean-suite measurement gate correctly
stopped on the two strict harness failures; the corrected recovery driver requires
exactly those failures and a passing focused correction before any measurement.
No performance execution was duplicated. Historical evidence remains preserved.

## Final local handoff

The bounded measurements finish **20/20**, with all independent checksums correct
and normal/O2 compilation versus execution separated. Every measured compilation
point is 10.5–26.5% lower; execution observations span −16.0% to +5.3% with only
three samples and are not statistical causal claims. Both size increases and
slower execution points remain recorded. Retained Stage11 IR, captured logging
bitcode and source-associated logging all pass the **three-execution DSE bound**
with actual hard 1 MiB limits, original module identifiers and unchanged verified
standard no-TM O2. No new crash occurs; the original LLVM19 DSE cause/repair stays
unproven. No production logging/text optimization is implemented.

Final correctness is effective Release **368/368**, Debug **381/381** clean,
effective strict **358/358**, both frontend hard-stack gates, six focused
normal/O2 JIT/native checks, actual editor/provider/install/package tests and
source/cache/native-manifest identity checks. Original failed selections are
retained separately from focused corrections. The last three source changes are
test-only channel manifest reuse; no compiler/Runtime binary changed during
final acceptance. New module receipts come from existing tests and do not denote
module implementation changes.

The exact changed-path/hash inventory, final compiler/package identities and
protected-input preservation results are linked from ACCEPTANCE.md. All nine
original dirty logging documents, protected root README and original DSE inputs/
executable/core remain byte-identical; the LLVM19 SDK stays in its original
directory. HEAD/index remain unchanged and unstaged. No hosted CI, Git history
action, branch, commit, push, publication or ptrace is used. Native Windows/macOS
and hosted seven-job acceptance remain explicitly pending Boss action.

**Engineering stops for independent Golden Wrench source/architecture review.**
