# Stage 11 final acceptance and local verification record

## STAGE 11: OFFICIALLY LOCKED, October 25, 2026

**Boss officially authorizes the Stage 11 runtime globals lock. Golden Wrench
has reviewed and accepted the implementation.** The exact tested commit is
`e8633b246fe6139d39d830afd4cb249f7dab4d8e`.
[Final CI run 40248573233](https://github.com/Shmellyorc/Void.Language/actions/runs/40248573233)
used `ci-link-experiment.yml`, `mode=thinlto-cache`, and passed **7/7 jobs on
attempt 1, with 1,407/1,407 CTests passed**.

| Hosted platform | Configuration | CTests passed |
| --- | --- | ---: |
| Ubuntu 24.04 | Debug | 203/203 |
| Ubuntu 24.04 | Release | 203/203 |
| Ubuntu 24.04 | Sanitizers | 203/203 |
| Windows 2022 | Debug | 197/197 |
| Windows 2022 | Release | 197/197 |
| macOS ARM64 | Debug | 202/202 |
| macOS ARM64 | Release | 202/202 |
| **Total** | **7 jobs** | **1,407/1,407** |

Hosted Debug editor integration passed on Linux, Windows and macOS. These editor
steps are separate from the CTest total. The run completed on 26 October UTC
(25 October in America/Los_Angeles). Earlier local Linux acceptance passed
**609/609**, recorded separately below; it is not included in the hosted total.

The lock covers the implemented [runtime globals contract](../RUNTIME_GLOBALS.md):
visibility, eager startup, Runtime ownership/GC/lifecycle, reload/cache, normal/O2
JIT, linked AOT and compiler-owned global tooling. It does not certify native
Windows/macOS Vim/Neovim host execution or physical second-volume Windows/editor
paths. Stage 10's Boss-accepted release exclusions remain exclusions, not passes.
The separate compiler-owned hover/documentation repair, standard-library expansion
and package-manager work remain future work. No next engineering stage is authorized.

The earlier LLVM 19.1.7 DSE crash remains **unexplained**: it was reproduced
locally, subsequently passed without a demonstrated repair, and did not recur
in hosted acceptance. This lock does not claim it was fixed. The separate local
ThinLTO cache/link incident is distinguished in the engineering record below.

This decision supersedes earlier pending-review/acceptance status only for the
accepted scope above. Historical machine-readable evidence, hashes, source-freeze
records, failures and the [optimizer observation](OPTIMIZER_FAILURE.md) remain
unchanged. This documentation-only lock update performs no Git mutation, hosted
dispatch, acceptance rerun or implementation work; Boss retains Git/release control.

## Historical local verification — 25 October 2026

The following records the pre-lock local engineering verification. Review and
Boss-controlled acceptance were pending when it was recorded; the official lock
above supersedes that status. No commit, branch, push, native Windows/macOS run
or hosted CI dispatch was part of that local verification work.

## Source and environment

- Baseline: `909c0c33d882f4f3f2b3f25f656271d42968c0bf` (Stage 10 lock record).
- [Source freeze](SOURCE_FREEZE.json), revision 10, hashes every changed production
  file, test and example. Documentation is outside the code freeze.
- Linux x86_64, PikaOS 4, kernel `7.2.11-pikaos`; Python 3.14.8 (Debug/sanitizer) and 3.11.16 (Release); CMake 4.3.4.
- Official pinned Clang/LLVM/LLD 19.1.7 under
  `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`.
- Debug: `build/step42-ci96-debug`; Release:
  `build/ci-official-release-step41`; strict ASan/UBSan RelWithDebInfo:
  `build/step42-ci96-san`.
- All three use the existing LLVM SDK RTTI ABI. UBSan vptr is unavailable with
  this SDK's RTTI-off ABI; no new sanitizer suppression was introduced.

The local executable linker flags use pinned LLD and per-configuration ThinLTO
caches (`build/stage11-lto-{debug,release,san}`). These are local CMake cache
settings, not workflow/source changes. Builds use
`MAKEFLAGS=--jobserver-style=pipe` because sandbox PID namespaces cannot share
GNU make FIFO jobservers. Final builds run serially after a parallel Debug
ThinLTO-cache temporary-file rename crash; the failed build is not a pass.

## Consolidated checks

All final runs use frozen **revision 10**, with every target rebuilt first.
All 609 named tests passed, without skips or focused reruns in this final round.

| Configuration | Passed | Failed | Elapsed seconds |
| --- | ---: | ---: | ---: |
| Debug | 203/203 | 0 | 2076.91 |
| Release | 203/203 | 0 | 1250.89 |
| Strict ASan/UBSan RelWithDebInfo | 203/203 | 0 | 2214.01 |

The [machine-readable record](ACCEPTANCE.json) preserves every test's output and
time. [Binary/library hashes](BUILD_ARTIFACTS.json), [source hashes](SOURCE_FREEZE.json)
and [evidence hashes](EVIDENCE_SHA256SUMS) identify the reviewed artifacts.
The final logs and JUnit files are under [evidence/](evidence/).

Each configuration passes **101 native global checks**, **157 real-program
commands** covering normal/O2 JIT and independently linked AOT, compiler-owned
LSP checks and actual 1 MiB inferred/annotated global-chain checks. Additional
native and shutdown stress checks on 1 MiB stacks are recorded below.

Earlier engineering rounds remain separate: revision 7's results are retained in
[PRE_CLOSE_REPAIR_ACCEPTANCE.json](PRE_CLOSE_REPAIR_ACCEPTANCE.json). A lifetime
audit found root-vector setup needed local construction and locked publication to
synchronize with Runtime.close; revision 8 adds the fix and overlap regression.
The superseded revision-8 matrix was interrupted for the binding repair and is
not final acceptance evidence. Revision 10 completes own-declaration precedence,
root-constant ambiguity and generic/root constructor contexts. The final matrix
runs two tests per configuration to avoid excessive concurrent load; an earlier
sanitizer operator AOT run hit its 600-second budget under the higher load.

Reproduction uses the configured pinned toolchain, with all targets rebuilt before
running CTest. The final commands are:

```sh
env MAKEFLAGS=--jobserver-style=pipe cmake --build build/step42-ci96-debug -j1
env MAKEFLAGS=--jobserver-style=pipe cmake --build build/ci-official-release-step41 -j1
ctest --test-dir build/step42-ci96-debug -j2 --output-on-failure --output-junit /tmp/stage11-debug-final10.xml
ctest --test-dir build/ci-official-release-step41 -j2 --output-on-failure --output-junit /tmp/stage11-release-final10.xml
# Execute the sanitizer build/tests outside restricted process namespaces.
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
env MAKEFLAGS=--jobserver-style=pipe cmake --build build/step42-ci96-san -j1
ctest --test-dir build/step42-ci96-san -j2 --output-on-failure --output-junit /tmp/stage11-san-final10.xml
```

Each configured Linux suite contains 203 CTests. Stage 11 adds native Runtime/IR,
real source JIT/AOT, constrained-stack and compiler-owned LSP regressions.

The sanitizer test environment is exactly:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Strict sanitizer tests run outside the restricted process sandbox: an initial
restricted run could not inspect its threads and reported LeakSanitizer's ptrace/environment failure; the identical host run passed.

Leak detection and existing source/IR/ownership validation remain enabled.
Sanitizer findings halt the process. Additional executed constrained-stack commands passed:

```sh
ulimit -s 1024
build/step42-ci96-debug/void_globals "build/step42-ci96-debug/stage11 stack native fixtures"
build/step42-ci96-debug/void_tests frontend "$PWD"
```

Final native checks report **101 global checks passed**; the frontend command
completes its scalar, aggregate, receiver and initialization groups. The sanitized
`voidls` frontend also checks both global chains on a real 1 MiB stack with strict
ASan/UBSan and leak checking enabled.

The preserved [root-close stress fixture](evidence/root-close-stress.cpp) executes
128 root-setup/shutdown overlaps and checks returned state and handle cleanup:
**256 assertions pass in each of Debug and strict ASan/UBSan**, both under 1 MiB.
A [readonly-global example](evidence/readonly-global.void) also verifies that a
readonly global reference observes changes through an independent mutable alias.

The 1 MiB source test creates real child processes with
`RLIMIT_STACK=1048576`: a 301-global inferred chain diagnoses the bounded
inference limit; a 301-global annotated chain checks successfully.

## Real editors

Executed against the rebuilt Debug `voidls`, with temporary projects/profiles:

| Client | Version | Result |
| --- | --- | --- |
| Neovim | `0.13.0-dev-1410+g510f61555b` | Passed |
| Vim | 9.2, patches 1–858, `/usr/bin/vim.basic` | Passed |
| VS Code | 1.143.0, `2d0b7506a36c1d6db9a466440131a8e01eab2289` | Passed |

Neovim and Vim exercise actual native LSP clients with the repository adapters.
Vim uses yegappan/lsp at `c741b2f1afb42b9f267824dac4049674908f4a41`;
the downloaded archive SHA256 is
`3a9e9e4fb435c21d8ca4d0d4741bb0751691239678dd038a1fbc5e3b6e0c8535`.
No editor or client was installed into the user's profile. `/usr/bin/vim` aliases
Neovim on this host, so the Vim claim uses the separate real Vim executable.

Reproduction from the repository, substituting local absolute executable paths:

```sh
export VOID_REPO="$PWD"
export VOIDLS_PATH="$PWD/build/step42-ci96-debug/voidls"
nvim --headless -u NONE -l tests/stage11/editor.lua
export VOID_VIM_CLIENT=/path/to/pinned/yegappan/lsp
export VOID_STAGE11_EDITOR_OUTPUT=/tmp/stage11-vim-result.txt
/usr/bin/vim.basic -Nu NONE -n -es -S tests/stage11/editor.vim
python3 tests/stage11/vscode.py /usr/share/code/code "$VOIDLS_PATH"
```

The VS Code runner creates a dedicated profile, extensions directory, project and
settings, then executes the checked-in extension-host fixture. The desktop test
requires the existing local display and permission to run outside the restricted
process sandbox. It checks hover, alias completion, cross-file definition and
references, guarded rename, unsaved diagnostics and repair, then deactivates the
extension cleanly. Neovim/Vim fixtures wait for native document synchronization
before checking overlay completion. Existing adapter rename restrictions remain.

Installed-standard global hover and rename protection, UTF-16 positions,
stale document versions and invalid-overlay repair are also exercised by the
compiler-owned LSP protocol suite; no separate editor semantic parser was added.

## Engineering failures and acceptance boundary

An earlier Debug run crashed inside LLVM 19.1.7 DSE, and a direct GDB run
reproduced it. The same standalone executable later passed 4,463 checks without
an intervening production-code change or rebuild; the final full Debug run also
passed. The [original backtrace and captured IR](OPTIMIZER_FAILURE.md) remain an
unresolved reliability observation. No causal repair was demonstrated. The crash
did not recur in hosted acceptance; that result does not establish a fix. The
default O2 pipeline is unchanged.

Separately, an earlier parallel local Debug build failed in LLD while renaming a
ThinLTO cache temporary object. A serial retry using the same cache succeeded.
That build-time cache/link failure is distinct from the in-process JIT O2/DSE
SIGSEGV; the successful link retry is not evidence of a DSE repair.


Failed development runs are not counted as passes. Retained logs include the
initial parser/completion placement and aggregate-IR compatibility compile errors,
a Clang crash from an in-place source rewrite, a stale-binary stack crash, and the
ThinLTO-cache linker failure. Repairs were verified with the final frozen source;
engineering source edits during compilation were changed to atomic replacement.
A strengthened regression initially used a private JIT helper; it now exercises
the public ReloadSlot replacement API. A callback-lifetime fixture was also
corrected to respect the existing provider's one-shot registration rule.

Hosted Windows/macOS and final cross-platform acceptance are recorded in the
official lock above. This record does not replace Stage 10's accepted native
editor/second-volume exclusions with a passing claim. Result/Ok/Err and
parameter/return hover-quality repairs, library expansion and package-manager
work remain out of scope. Further engineering requires Boss authorization.
