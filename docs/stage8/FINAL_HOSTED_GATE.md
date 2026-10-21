# Stage 8 final acceptance gate — CI #85 editor repair; not locked

21 October 2026 latest update: CI #85 at
`e8a1ea7433b465c3800090a7b84737d7474c9228` passed Windows Debug compiler
156/156 and extension units, then failed actual editor discovery with lowercase
drive spelling. Windows Release succeeded; the other five jobs were cancelled.
Review [the drive-prefix/editor repair](CI85_WINDOWS_EDITOR_REPAIR.md). Require
Windows Debug **and Release** drive/8.3/alias/junction/other-drive regressions,
record optional SUBST/8.3 skips, and complete Debug's actual editor profiles.
The stale interpolation assertion found by local editor execution is corrected
without losing rejection/recovery coverage. Native results and all seven required
job outcomes remain owner-controlled; Wine/Linux do not establish a hosted lock.
No rerun was triggered. Earlier dated evidence follows.

21 October 2026 later update: CI #84 Windows compiler Debug passed 156/156 and
Release succeeded, establishing execution of the earlier Unicode repair. Debug
then failed extension root-path separator equality. CI #83 macOS Release passed
155/156 with /var discovery failure. Review the
[cross-platform repair](CI84_DISCOVERY_REPAIR.md): focused Linux Debug/Release
7/7 each and extension 4/4 do not certify native platforms. Inspect **both macOS
Debug and Release**, Windows normalized root/entry/fallback assertions and actual
editor-host steps. CI #84 still had both macOS jobs and Ubuntu jobs building at
the recorded status check; all eventual outcomes need review. No hosted rerun
was started. Earlier evidence below is historical, and no lock is established.

21 October 2026 update: Boss ran
[CI #83](https://github.com/Shmellyorc/Void.Language/actions/runs/39571077819)
against `fbad2b642114a692f93d2f4ce7d3465ea489abf2`. Windows Debug/Release each
built and passed 153/156 suites; three Unicode failures block acceptance. Review
the [focused repair](CI83_WINDOWS_REPAIR.md) and local evidence. Both Windows
configurations must execute the repair before any lock. No rerun was triggered
or requested by Keyboard. Other platform/editor outcomes are not inferred here.
The original prepared checklist below remains required.

21 October 2026 original preparation. This is a checklist, not hosted authorization or a lock.
Read [docs index](../README.md), [Sprint 6 contract](../STAGE8_ERGONOMICS.md),
its local acceptance record and the preserved Sprint 1–5 evidence first.

## Owner-controlled sequence

1. Keyboard completes Sprint 6 engineering and records actual local outcomes.
2. Wrench reviews the complete diff, semantic approvals, failures/repairs, hashes,
   source discovery, capabilities, roots, async retirement and native admission.
3. Boss decides whether to commit and push. No agent Git mutation is authorized.
4. Boss explicitly authorizes the final hosted gate. The existing workflow also
   runs on push; Boss controls the push and its hosted consequences.
5. Verify every required hosted job against the exact reviewed commit/run/attempt.
   Only then may Boss/Wrench determine whether Stage 8 can be locked.

No hosted workflow was started or requested by Keyboard during this sprint.
No Stage 9, stdlib implementation, publication or website work is authorized.

## Seven required jobs

The checked-in workflow defines:

- Ubuntu 24.04 Debug, Release and strict Sanitizers (ASan/UBSan/leaks).
- Windows 2022 Debug and Release.
- macOS ARM64 Debug and Release.

Require complete official LLVM 19.1.7 SDK admission, clean builds, **unfiltered**
CTest results and retained per-job logs. Record actual suite counts and explain
any platform difference; focused reruns are not extra unique suites. Keep strict
sanitizer/leak checks enabled and report their instrumentation limits. Linux
Debug/Release also require the full frontend test with a real 1 MiB stack.
Builds must finish before tests; do not relink an executable during its test run.

The workflow's Debug jobs typecheck/build/test/package the extension and execute
its real editor-host integration. Passing stdio LSP tests is separate evidence
from an actual client session. Record any cancellations, failures and repairs;
never treat a cancelled Windows Debug job as verification.

## Cross-platform and interactive review

- Execute [CI #82 repair coverage](CI82_WINDOWS_REPAIR.md) in **both Windows
  Debug and Release**, including supported 8.3 short/long spelling identity,
  exact case, Unicode, nearest nested project, containment and outside/alias/
  symlink/reparse/VFS rejection. Conditional short-path skips must be reported.
- Verify LF/CRLF source handling, UTF-16 positions after supplementary scalars,
  nested interpolation quotes, original spans, multiline bodies inside calls,
  formatter equivalence/idempotence and grammar presentation on each platform.
- In actual VS Code and VSCodium sessions, check project selection and trust,
  imports, ordinary/default/named/generic/method multiline calls, if expressions,
  has type navigation, interpolation completion/hover/rename, diagnostics and
  repair, formatting, save/unsaved version guards and installed VSIX behavior.
- In actual Neovim and Vim sessions, repeat shared compiler-owned discovery,
  diagnostics/navigation/completion where supported, Unicode positions and
  supported formatting behavior. Preserve Vim's existing rename restriction.
- Confirm no editor grants native admission or writes/renames installed stdlib.
- Preserve JIT normal/O2, independent linked AOT, Debug cache and compatible
  reload/old leases, checked cleanup, precise GC and readonly/value/reference
  contracts. Exercise Sprint 5 repeated awaits and quiescent frame retirement;
  Task/result/Work/Handle-capacity retention remains an explicit limitation.

Fresh Windows/macOS and interactive-client results cannot be inferred from Linux
local CTest, extension unit tests or compiler-owned stdio LSP checks. Stage 8 is
not hosted locked until the owner-controlled evidence establishes that outcome.

## Deferred scope stays deferred

Numeric/bool/custom interpolation conversion, formatting/alignment/locale rules,
triple strings, recursive component search, value-based has/refinement/conditional
compilation, unrestricted continuation, advanced OOP/generic extensions, broader
Task/record ownership, general FFI/dynamic loading and substantial stdlib work
are outside this sprint. The six planned sprints are bounded implementation
steps, not proof that every Language Bible example is executable.
