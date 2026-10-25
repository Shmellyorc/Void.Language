# Stage 10 targeted native stack workflow — 24 October 2026

**Prepared and locally validated; NOT DISPATCHED.** Stop for Golden Wrench review
and Boss-controlled commit/push and hosted authorization. Stage 10 remains
INCOMPLETE. Native results below describe the required future artifacts, not
executed Windows/macOS passes.

[Workflow](../../../../.github/workflows/stage10-native-stack.yml) uses only
`workflow_dispatch`, read-only repository permissions, a 360-minute job timeout,
and exactly four independent entries: Windows 2022 x64 Debug/Release and macOS
ARM64 Debug/Release. It retains failure evidence from every configuration.
There is no Linux job, sanitizer job, editor matrix, full-suite run or warm relink.

The [focused CI helper](../../../../tools/ci/stage10_native_stack.py) reuses the
[Step 42 native commands](../step42-native-commands.md) and the existing SDK/CMake
infrastructure. The helper is CI evidence collection; compiler, Runtime and
existing tests are unchanged. Both existing CI workflows and every pre-existing
Stage 10 document, archive and failure record are preserved byte-for-byte.

## What each job does

1. Verify real native architecture, record checked-out commit/dispatch SHA and
   hashes of all tracked source bytes. A dispatch/source mismatch fails. The
   macOS guard rejects x64/Rosetta rather than inferring ARM64 from the runner label.
2. Reuse the existing x64 MSVC/DIA SDK action on Windows and complete official
   LLVM **19.1.7** installer/cache on both platforms. Record versions, selected
   toolchain environment and compiler/LLVMConfig.cmake file hashes.
3. Configure a fresh Ninja Debug/Release directory with the existing native
   sanitizer-OFF/toolchain policies. Windows executable-link flags are unchanged.
   macOS uses only the proven executable cache option
   `-Wl,-cache_path_lto,<absolute-directory>` via `CMAKE_EXE_LINKER_FLAGS:STRING`.
   Its unique per-job directory starts empty and is shared by configure probes
   and executable links. No persistent ThinLTO cache, `-flto`, new linker or
   optimization/stack-reserve override is introduced. The existing SDK cache is
   separate from the temporary linker cache.
4. Run `cmake --build ... --parallel 3 --target voidc void_tests`, preserving the
   original frontend harness and its native dependencies without building all
   unrelated targets.
5. On Windows, use the pinned SDK's `llvm-readobj.exe --file-headers` on **both**
   actual `voidc.exe` and `void_tests.exe`. Each must have exactly one
   `SizeOfStackReserve` equal to **1,048,576**, independent of StackCommit or CMake
   defaults. Missing, duplicate or wrong reserve values fail before execution;
   raw headers, sizes, hashes and parsed values remain available.
6. Run unchanged `ctest -R '^frontend$' --no-tests=error --output-on-failure` and
   `tests/integration/semantic_depth.py`. On macOS, each wrapper sets **both**
   `RLIMIT_STACK` values to **1,048,576**, reads them back and records them before
   launching its unchanged child. The test and its descendants inherit that
   verified limit. Failure to set/read the exact limits prevents execution.
   On Windows the original probes execute only after the PE gate passes.

Semantic-depth still runs if frontend fails, provided build and Windows PE gates
passed. A test failure remains a job failure. The final collector runs with
`always()` and fails if required evidence is incomplete or tracked sources changed.
The upload step independently uses `always()` so failures do not suppress artifacts.
No source/test limit, timeout, assertion or process-input behavior is relaxed.

## Required hosted evidence

Artifacts are named with OS, architecture, configuration, source SHA and attempt.
They include:

- `identity.json`, complete source-before/after hashes, toolchain environment,
  native compiler/LLVMConfig digests and SDK/version command logs.
- Raw combined stdout/stderr bytes and JSON argv, exit code and duration for
  each executed command; phase errors include exception/traceback diagnostics.
- Windows per-binary PE headers and `pe-stack.json`; macOS independent
  `frontend-stack.json` and `semantic-depth-stack.json` before/observed limits.
- Frontend JUnit, original depth output and `summary.json` with individual exits
  and an explicit stack-only completion flag.
- Actual compiler/frontend binaries and their SHA-256, CMake cache/configure log,
  Ninja build graph/log, compile commands and CTest temporary diagnostics.

An early checkout/setup failure may prevent helper execution; GitHub step logs
remain the source for that failure. A configured/built job preserves whatever
evidence exists even if subsequent gates fail. No partial run certifies acceptance.
Boss must dispatch a reviewed committed ref; local uncommitted files are not
silently included in GitHub Actions. This preparation performs no Git mutation
or hosted execution.

## Local validation and remaining gates

[Validation records](validation.json), [raw local output](local-validation.log.gz),
[exact workflow/helper diff](workflow-helper.diff) and
[this mission's inventory](changed-files.json) retain the evidence.
Actionlint **1.7.7**, YAML/Python parsing, exact matrix/triggers, CMake argument
construction (including a cache path with spaces), build targets/parallelism,
failure logging and incomplete-evidence rejection pass locally.

Linux-generated minimal PE files, read with real LLVM 19 tools, verify the reserve
parser accepts 1 MiB and rejects a 2 MiB reserve. Simulated resource-module cases
verify the macOS guard requests both limits and refuses wrong/failed readback
before scheduling a child. These are local logic/format checks, **not native
Windows compiler binaries, macOS limit enforcement or hosted acceptance**. The
native CMake builds, SDK environment and actual inherited macOS stack still need
the authorized four-job run. No full compiler/test suite was repeated locally.

**Windows console/ConPTY remains UNVERIFIED.** Headless PE/frontend/depth checks
do not prove ReadConsoleW/WriteConsoleW, actual key/modifier/repeat events, before/
after modes or code pages, restoration errors, borrowed synchronous/overlapped
handle cancellation, retained I/O lifetimes or native cleanup behavior. The
[existing native-final handoff](../native-final/HANDOFF.md) retains those genuine
interactive-host requirements and existing-probe limitations. Native Vim/Neovim,
second-volume and ThinLTO performance comparisons keep their separate exclusions.
A future green stack workflow closes only its exact stack evidence gates;
Golden Wrench review and Boss release decisions remain separate.
