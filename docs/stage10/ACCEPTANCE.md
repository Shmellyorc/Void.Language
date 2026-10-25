# Stage 10 final engineering acceptance — 24 October 2026

## 25 October 2026 — independent final Windows native acceptance review

The [Golden Wrench final Windows review](engineering/windows-console-final/windows10-vm-2026-10-25/FINAL_WRENCH_REVIEW_2026-10-25.md) **ACCEPTS the five measured Windows native behavior boundaries** after independently checking both final/private evidence manifests (**228 + 2,431 SHA-256 entries**), matching **100/100 human-origin KEY_EVENT/KeyResult records** (60 earlier + 40 new), final **Debug/Release 6/6** focused CTests, **4/4** real-host mode/restoration matrices and accepted Windows Terminal/ConPTY, Win32 wide-I/O and borrowed-handle cancellation observations. Four new real Alt+0233 Unicode sessions passed on both hosts and both configurations. Source/test inputs match reviewed GitHub `main` `6175e83` after Windows CRLF normalization; the older local HEAD is **not** presented as a clean current checkout. Prior failed attempts and unexecuted human cross-product coverage remain explicit.

This final review supersedes the older Windows "UNVERIFIED" historical status summaries **only for tested native boundaries**; do not delete the earlier records. No production repair was demonstrated. **Stage 10 remains INCOMPLETE / NOT LOCKED** pending Boss-authorized final cross-platform CI and Boss's decision on remaining separately identified acceptance scope. The private Windows archive is **not** a publication artifact.


## 25 October 2026 — native Windows 10 Console Host observations

[Independent VM evidence review](engineering/windows-console-final/windows10-vm-2026-10-25/WRENCH_REVIEW.md) records successful Debug/Release builds, focused console CTests **4/4**, genuine attached classic Console Host flows with **scripted input 10/10**, and **four Boss-attested physical Up-arrow runs**, each with two observed key rows and native child exit 0. This is a separate Windows 10 VM at commit `046fcf152a786ce6fe27d58ea4bfb709028f4a21`, not a new hosted CI matrix. Complete original native evidence, including earlier failed attempts, is preserved **privately by Boss**. Public path-free summaries and the unchanged raw-archive SHA-256 are available in [the VM evidence manifest](engineering/windows-console-final/windows10-vm-2026-10-25/evidence-manifest.json). A narrow one-line docs-only PowerShell observer exit-status correction is provided; the successful probes used the corrected evidence copy.

**Evidence boundaries remain explicit:** Windows Terminal/ConPTY, complete modifier and key-down/repeat metadata, actual in-process nested-mode restoration, Win32 UTF-16 partial/error traces and borrowed synchronous/overlapped cancellation/lifetime are **UNVERIFIED**. The VM used explicit output CP 65001; unresolved default-437 and manual-input observations remain historical. The previous 24 October "no interactive Windows host" paragraphs below are **historical**; they are superseded only for the classic Console Host observations named here. **Stage 10 remains INCOMPLETE, no release lock or next stage authorized.**

**Step 43 complete. Step 44 prepared for independent Golden Wrench review and
Boss release decisions. Stage 10 remains INCOMPLETE and is not release locked.**
The four authorized systems are implemented. This reconciliation found no new
demonstrated production defect. Required native evidence gaps below remain open;
documentation and a green experimental workflow cannot close them.

## Native stack gates and final Windows console handoff — 24 October 2026

Targeted Windows stack CI #1 (`40147114803`, source
`b490579f84a702ec5d11da51a82934a498f88afa`) passed Debug/Release, both actual
1,048,576-byte PE reserves and original frontend/depth tests. Targeted macOS
stack CI #2 (`40152738145`, source `383d2ce0a404a4bbf3df01d43097de100f5829a1`)
passed both ARM64 configurations, Mach-O checks, exact inherited 1 MiB limits
and original frontend/depth tests. These close the specific stack evidence gaps
listed in the earlier acceptance snapshot below; CI #1's macOS setter failures
remain historical. Full CI #4 and all previous failure evidence are preserved.

The [final Windows console checklist and exact evidence](engineering/windows-console-final/CHECKLIST.md)
map existing native probes to what they can actually observe. No interactive
Windows host is available here. Genuine physical metadata, in-process mode/code-page
restoration and borrowed-handle cancellation/lifetime remain UNVERIFIED. A small
parent-state observer is prepared, unexecuted, and explicitly cannot certify
those in-process boundaries. Stage 10 remains INCOMPLETE for Wrench/Boss review;
no new stage, hosted dispatch or release lock is authorized.

## Exact source and experiment #4

[Compiler link experiment #4](https://github.com/Shmellyorc/Void.Language/actions/runs/40118666209)
ran `workflow_dispatch`, attempt 1, in `thinlto-cache` mode at
`90c0cf64b9aba60d7c642096d2aaac45aae70d56`. All seven jobs succeeded.
Created **2026-10-24 18:00:03 UTC**, updated **19:08:47 UTC**: **4,124 seconds
(1 h 8 m 44 s)** overall interval, not a sum of parallel job durations.

| Configuration | Job ID | Main CTest executions |
|---|---|---:|
| Ubuntu 24.04 Debug | 120945262672 | 199/199 |
| Ubuntu 24.04 Release | 120945262661 | 199/199 |
| Ubuntu 24.04 strict sanitizers | 120945262621 | 199/199 |
| Windows 2022 Debug | 120945262512 | 194/194 |
| Windows 2022 Release | 120945262642 | 194/194 |
| macOS ARM64 Debug | 120945262292 | 199/199 |
| macOS ARM64 Release | 120945262688 | 199/199 |
| **Total** | **7/7 jobs** | **1,383/1,383** |

All main-suite test markers were counted from the seven decoded job logs: zero
failures or skipped tests. Five POSIX-only suites are absent on Windows by design.
The separate Linux Debug and Release frontend CTest reruns each passed 1/1;
they are additional executions, excluded from 1,383. The strict job retained
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
The native SDK compiler reports LLVM/Clang **19.1.7** on every platform.

macOS Release `runtime_console_native_transfers` passed in **0.35 s**,
`runtime_console_native_terminal` in **1.17 s**, and `stage10_console_aot`
in **311.20 s**. Debug passes are independently present. Debug VS Code integration
passed on Linux, Windows and macOS: trusted, restricted and multi-root profiles,
including compiler-owned nested import completion, source navigation and repair.
No native Vim/Neovim result is inferred from those VS Code steps.

[Run metadata](engineering/ci4/run.json), [jobs and step conclusions](engineering/ci4/jobs.json),
[counted results/log receipts](engineering/ci4/summary.json) and
[17 artifact receipts](engineering/ci4/artifacts.json) are permanent evidence.
Seven complete decoded logs are stored alongside them as `job-ID.log.gz`;
decompressed and compressed SHA-256 hashes are in the summary. Artifact ZIP
metadata/digests were read; ZIP contents were not downloaded or independently
rehash-verified. GitHub artifact retention is finite.

The local starting HEAD is `92136acaf114a492452a7d84b31f174f5b305def`.
Its only change after the tested SHA is the 550-line future stdlib/global handoff.
This closeout adds documentation, evidence and one example; it changes no compiler,
Runtime, tests, dependency, editor or workflow. The new example is locally verified
and was not present in experiment #4. Neither main CI nor the experimental
workflow is edited. No new hosted run is requested for this closeout.

[Closeout results](engineering/closeout/results.json),
[local validation](engineering/closeout/validation.json),
[exact changed-file inventory](engineering/closeout/changed-files.json) and
[the example source diff](engineering/closeout/example.diff) make this increment
reviewable. The pre-existing independent CI #4/design review is left unchanged.

## Reconciliation and traceability

The [44-step ledger and feature matrix](IMPLEMENTATION.md#all-44-roadmap-obligations)
map implementation to existing tests. B1–B4 and amendments remain the approved
contracts: [display](DISPLAY_FORMATTING.md), [fields](FIELD_ACCESS.md),
[operators](OPERATORS.md), [console](CONSOLE_IO.md), and [test plan](TEST_PLAN.md).
No approved functionality is silently reclassified as a future library feature.

| Evidence layer | What it establishes |
|---|---|
| [Step 40 final integration](engineering/step40-final-integration-results.json) | Four-system scenarios, precise roots, GC/faults/cleanup, normal/O2 JIT, independent AOT, production builds, real PTYs, reload/cache and measured workloads; Debug/strict 25/25 each |
| [Original Step 41](engineering/step41-results.json) | Unfiltered Linux Debug/Release/strict 199/199 each (597), original 1 MiB gates, full 203/deep 72 regressions and actual Linux VS Code/Neovim/Vim evidence |
| [Step 42](engineering/step42-results.json) and experiment #4 | Current-source native automated Debug/Release acceptance on Windows/macOS, strict Linux and applicable editor checks; remaining host-specific gates below |
| [Step 43 examples](EXAMPLES.md) and [local raw records](engineering/closeout/example-attempts.json) | A small runnable integration example, exact Unicode output, Debug/Release check/JIT/O2/native executable validation and explicit compiler provenance |
| Step 44, this record | Review package and remaining acceptance decisions; no self-issued Wrench approval or Boss release |

Preserve the [CI #98 / experiment #3 diagnosis](engineering/ci98/DIAGNOSIS.md)
and all raw reproductions. At `9a4985f89f40c56b8eaa984775faf18c52b69f19`,
main #98 (`40091828693`) and experiment #3 (`40091853317`) each passed 6/7 jobs.
Their macOS Release jobs each passed 198/199, with **different** failures: main's
redirected-input probe could run before its parent supplied bytes; experiment's
cancelled write could be cancelled after admission but before native progress.
The repairs prequeue the input and establish writable→not-writable pipe progress
before cancellation. They change test synchronization, preserve every correctness
assertion, and do not change production Runtime. Experiment #4 verifies these
repairs natively; it does not prove ThinLTO caused or repaired either failure.
No same-commit control run is recorded here, so no cache speedup or cold-versus-warm
causal comparison is claimed. Main #98 remains historical failed evidence.

## Remaining native acceptance and release decisions

These are evidence gaps, not newly diagnosed production failures. The original
[native acceptance preparation](engineering/step42-native-commands.md) explains
the mechanisms; its pre-CI96 source receipt is historical and must not be reused
as the current source freeze.

| Gate | Actual evidence and remaining requirement |
|---|---|
| Real Windows console/ConPTY | Windows 194/194 covers redirected streams and injected wide-console adapters. It does not prove actual ReadConsoleW/WriteConsoleW, physical key/modifier/repeat events, host mode/code-page restoration or borrowed synchronous/overlapped handle cancellation. No automated ConPTY driver exists. Existing native probes need Boss-authorized real-host observation/instrumentation in Debug and Release. |
| Native 1 MiB stack boundary | Original Linux Step 41 and the Linux hosted frontend gate pass without relaxed guards. The experiment does not explicitly enforce both macOS RLIMIT_STACK values at 1,048,576 or retain a Windows PE StackReserve readback. Keep those exact native receipts unverified; do not infer them from default-stack CTest passes. |
| Additional host/editor scope | Native Windows/macOS Vim/Neovim and a real second Windows volume were not exercised. Linux editor results remain valid for their recorded hosts. Second-volume publication is an older Stage 9 exclusion, not a new Stage 10 implementation feature. |
| Independent review and release | Golden Wrench reviews the actual diff, completeness mapping, preserved failures and evidence exclusions. Boss alone decides outstanding acceptance disposition, commit/push, final hosted authorization, linker adoption and release lock. No approval is inferred from experiment #4. |

Do not request another seven-job run for documents or individual features.
Any additional native check requires a concrete boundary to verify and Boss
authorization. Stage 10 stays active until its required gaps and review decisions
are resolved. No next stage starts here.

## Preserved observations and future scope

Step 37 publication/default-root anomalies, the original pinned-Clang crash,
deep-collection compile cost, successful-build LLD/GVN emission, and deferred
F12/F2/hover/documentation observations retain their original evidence and unknown
causes in IMPLEMENTATION and Step 41. Passing later tests do not establish fixes.
Compiler/Runtime sanitizer coverage does not equivalently instrument generated
machine code; native Windows/macOS use their existing non-sanitized settings.

The [future stdlib/global handoff](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md)
and its dated correction preserve nested official modules, separate project-local
`libs/`, compiler-owned LSP discovery, lexical file-level `var` semantics and open
initialization/visibility/GC/ownership/reload questions. All remain future design.
The [permanent stage workflow](../WRENCH_CLANKER.md#permanent-workflow-for-every-new-development-stage)
governs subsequent stages only after Boss authorizes them.
