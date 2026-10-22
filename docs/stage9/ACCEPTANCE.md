# Stage 9 engineering and acceptance — 21 October 2026

> Windows update, 21 October 2026: **CI #95 accepted Debug 160/160 and Release
> 160/160 (320/320)** at `e25816bdb80e3f32613edf77c9cf3258232af966`.
> The blocking Windows investigation is **RESOLVED**; see the
> [permanent Windows record](WINDOWS_ENGINEERING_RECORD.md). The original snapshot/
> broader gate below remains historical; no complete seven-job matrix or actual
> second-volume pass is claimed here. Further CI remains Boss-controlled.

> CI #88 update, 21 October 2026: the owner reported five successful Linux/macOS
> jobs and Windows Debug/Release 152/158 with a physical output ancestor regression.
> [Focused repair/evidence](CI88_WINDOWS_PATH_REPAIR.md) supersede the current path
> disposition; this original local acceptance snapshot and its hashes remain intact.


Status: all three milestones implemented; final local gates passed; ready for Wrench
review with the unexplained preliminary process observations below disclosed.
Windows/macOS and hosted acceptance remain unverified.
Boss authorized all three milestones together; no owner semantic decision remains
pending. [Contract](../STAGE9_APPLICATION_WORKFLOW.md), [source hashes](SOURCE_HASHES.json)
and [required native hosted gate](HOSTED_GATE.md) define the review boundary.

## Delivered

- Compiler-owned automatic application/legacy selection, cwd run/build, arbitrary
  explicit .void filenames, deliberate option parsing and removed flag rejection.
  Existing check --no-opt, JIT optimization, Debug-cache, entry precedence,
  application status and library checking remain covered.
- Real Clang/LLVM 19.1.7 native executables, existing object emitter and Runtime/
  providers, target-derived resource configuration, private compiler installation
  and relocation. Host SDK/CRT/linker dependencies stay explicit; no LLVM compiler
  library is linked into generated applications.
- Physical source/output collision protection, exact spelling, same-parent staging,
  real percent-path linking, old artifact preservation, cleanup and platform
  replacement. Argv child execution uses no shell.
- AOT checked failures on stderr with unchanged captured messages/bounded traces;
  normal/O2 JIT/native parity compares status/stdout/stderr exactly. Existing
  compiler/LSP/editor source identity, security, snapshots and recovery stay shared.

## Local evidence

Final build/test logs and JUnit records are retained under
`build/stage9-acceptance/`; [tracked results/log hashes](EVIDENCE.json) preserve
their identities. Raw attempts are retained in its `engineering/` directory. The current Linux configurations use matching system LLVM/Clang/LLD
19.1.7, C++ RTTI enabled. Debug also retains its pre-existing optional research
configuration (13 additional suites); no suite was disabled to change totals.

| Final configuration | Passed / configured | Seconds |
| --- | --- | --- |
| Debug, including 13 pre-existing research suites | 171 / 171 | 449.51 |
| Release | 158 / 158 | 186.03 |
| Strict ASan/UBSan/leaks | 158 / 158 | 417.44 |

**487/487 final unique configuration/suite outcomes, zero final failures or skipped
suites.** Both complete Debug/Release frontend regressions also passed with actual
1,048,576-byte stack limits (10.84 s / 4.71 s). Each configuration executed 110
real Stage 9 workflow process checks, including normal/O2 native applications,
relocated private installation, physical paths and artifact failures. These process
checks, focused reruns and stack runs are not added to the unique suite count.

Strict sanitizer environment: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Builds completed
before final tests. Compiler/runtime/provider instrumentation remained enabled.
Final source/test hashes match [SOURCE_HASHES.json](SOURCE_HASHES.json); no C++
source changed after the completed final builds. The final macOS-only CMake SDK
resolution branch was parsed/configured on Linux and the unchanged Linux native
resources received an additional real workflow pass; native macOS remains pending.

Real Node 24.19.0 extension typecheck/build and all four direct real-server units
passed. Real installed VS Code trusted/untrusted/multi-root profiles passed typing,
local libraries, unsaved UTF-16 diagnostics/repair, navigation/references, lifecycle
and clean shutdown. Actual Neovim 0.13 development build and Vim 9.2 with the
existing pinned vim-lsp client passed automatic and explicit selection, shipped
adapters and lifecycle checks. These versions do not establish VSCodium or any
unexecuted platform/client combination.

## Failed attempts and corrections

All failures are retained; preliminary runs/repeated probes are not additional
unique suite outcomes.

- Initial staging used LLVM's whole-path percent substitution. Generating only a
  leaf fixed literal percent ancestors; a later workspace-filesystem build exposed
  that LLVM also prepended /tmp. Taking the leaf before joining the physical output
  parent fixed cross-device rename and the hidden clean-directory issue. The new
  real-process observation asserts actual intermediates use the percent-containing
  output parent. Source/output behavior was not bypassed through other directories.
- A new native fixture initially used unsupported positional no-init construction
  and incorrect dictionary syntax. Correct named fields and existing `{K: V}`
  syntax fixed the fixture; language semantics were not expanded.
- Preliminary source_cli failed because check --no-opt had been rejected. Its
  existing inert meaning was restored, retaining every strict UTF-8/span/source
  preservation assertion in both modes.
- The stderr adapter altered LLVM's module-wide inlining budget, removing a
  virtual tail dispatcher from a research negative. The fixture now explicitly
  keeps that real dispatcher outlined before optimization. The original final
  encoded-call rejection, old-output preservation and correct ordinary JIT/AOT
  assertions remain. Production research checks were not weakened.
- Initial sanitizer build failed in the traced sandbox because LeakSanitizer
  cannot operate under ptrace. Strict rebuilding/testing runs outside tracing;
  leak detection, halt-on-error, exit codes and all sanitizer categories remain.
- Electron initially failed sandbox launch. The unchanged actual editor harness
  subsequently passed outside that sandbox. No editor assertion was removed.
- A preliminary Debug async object generator and preliminary numeric process each
  segfaulted once without a captured stack. Investigation retains exact failures,
  45 successful emitter reproductions (including four concurrent processes), 12
  numeric reproductions and a real GDB numeric pass, followed by 20 current and
  20 baseline numeric controls with core dumps enabled. All controls passed; no
  crash/core was captured. The cause remains unexplained; no fix is claimed from
  passing reruns. Final full strict acceptance is recorded separately below.

- A preliminary full Debug run caught a race in the new test observer: recursive
  glob entered a staging directory after successful publication removed it. The
  observer now enumerates the known output parent directly, retaining its actual
  stage-location and successful executable assertions. Hard-link admission is
  mandatory; only Windows symlink privilege error 1314 is explicitly reported as
  unavailable, with all existing native reparse gates still required.
- macOS resource generation resolves named/default SDKs through xcrun before
  serializing a physical sysroot, including Xcode's named-SDK convention. Linux
  CMake parsing/resource acceptance passed; native macOS execution remains pending.

- Early harness invocations before the host fixture was built and npm from the
  repository root failed for missing executable/package.json respectively. The
  full builds and the actual extension directory resolved those setup errors;
  their raw logs remain. Neither attempt is counted as acceptance.

## Verification limits and ownership

Windows and macOS were not executed locally. Their real hosted gates, SDK/CRT,
case-sensitive APFS and Windows long-path/reparse/replacement verification remain
mandatory. [HOSTED_GATE.md](HOSTED_GATE.md) gives exact requirements. Strict C/C++
instrumentation does not automatically instrument LLVM-generated instructions.
Private install tests prove bin/config/archive relocation with an already configured
host SDK; they do not prove a self-contained clean-machine toolchain distribution.

The pre-existing CI #87 two-line override, local libraries and research/roadmap
records are preserved. Root README SHA-256 is recorded in SOURCE_HASHES.json and
has no diff. No commits, pushes, branches, hosted workflow, publication, downloads,
package manager, generalized FFI or unrelated language feature was added.
