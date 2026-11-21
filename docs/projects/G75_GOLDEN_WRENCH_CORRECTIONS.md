# G75 — Three Golden Wrench corrections

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

18 November 2026. Baseline HEAD `fdcbeadbdd8a0011751b448afa9fe40e5151ab1f`.
Boss authorized exactly the three independent review findings. This is Keyboard's
implementation/qualification record, awaiting independent review and commit
approval. No additional feature work, staging, commit, push, installed-prefix
change, publication or hosted CI occurred. G69 remains on hold; G76 remains queued.

The starting 56-path candidate is preserved in `candidate-before.zip` with a
SHA-256 inventory. The accepted sentinel backend, four frontend boundaries,
low-stack tests, language semantics and native authorization code are unchanged.
The original independent review and its evidence archive remain untouched.

## Corrections and research

1. **F5 abort/retry.** The original configuration provider registered a pending
   receipt before the prelaunch task. A host abort could return false without
   canceling that provider's token or calling the final provider, leaving a
   global latch that rejected later F5 attempts. A new launch now supersedes an
   abandoned pending/unconsumed receipt and disposes its callback/artifact/task
   state. Its ephemeral provided task carries its own receipt in the task
   definition, rather than discovering the latest pending launch when opening.
   Obsolete tasks and callbacks cannot grant a newer receipt. A real running
   child still blocks concurrent launches, including the interval after
   cancellation until child completion. The selected-project serial is checked
   before child admission and final receipt consumption. Actual spawn, exit 0,
   ordinary freshly published artifact, selection identity and one-time
   consumption remain required. Saved `tasks.json`/`launch.json` are not rewritten.

   [C#'s pinned launch resolver](https://github.com/dotnet/vscode-csharp/blob/ef4f2b6b2538d24a95c2f686725d9165780df938/src/shared/dotnetConfigurationProvider.ts)
   handles `launchCancelled` as a launch-local result and disposes its service
   proxy in `finally`; its base provider does not use this global build-receipt
   latch. This supplies the lifecycle principle, not a replacement for VOID's
   stricter build-success proof. [VS Code 1.144](https://github.com/microsoft/vscode/blob/1.144.0/src/vs/workbench/contrib/debug/browser/debugService.ts#L505-L510)
   confirms prelaunch failure may return before the substituted provider.
   Provided tasks preserve the configured task-definition object when resolving
   it and use the host's source-qualified name for invocation-specific lookup.

2. **Unknown timing configuration.** `Presentation::project_context` records
   only the established project-mode bit. Manifest invocations leave effective
   profile, optimization and debug information null until `resolve_profile`
   succeeds. Bare-source CLI settings remain recorded when already established.
   No compiler/profile/cache execution was changed. Semantic assertions now
   cover malformed TOML/VOID5001 exit 1, rejected release-plus-cache exit 2,
   successful Debug/Release, effective O0/O2, bare sources and cache selection.
   The expanded test fails on the preserved original compiler's fabricated O2/
   none settings, and passes on all three repaired profiles. Both negative
   after-reports are retained with null configuration fields.

3. **Windows report containment.** The report destination and optional artifact
   are compared using canonical filesystem locations before fresh-file
   construction. A private Windows-only comparison helper normalizes drive
   spelling to uppercase (`c:`→`C:`, `d:`→`D:`), recognizes equivalent extended
   DOS/UNC comparison forms and uses `CompareStringOrdinal` on native Unicode
   components. API comparison failure rejects admission. POSIX comparison
   remains case-sensitive. Actual publication still uses the original path and
   the existing pinned-directory, alias/reparse, protected-store, collision,
   restrictive staging, integrity, synchronization and exclusive-publication
   checks. Existing output replacement and exit-code rules are unchanged.

   Microsoft's [path rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)
   identify drive letters as case-insensitive;
   [ordinal comparison](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-comparestringordinal)
   provides the native uppercase-table comparison. No arbitrary POSIX path is
   lowercased. Focused native Windows branches cover mixed drive spelling,
   distinct drives/component boundaries, exact artifact collisions, UNC,
   extended DOS/UNC, Unicode and long lexical paths. The Python integration
   also covers physical Unicode paths, fresh outside reports, junction refusal
   and long paths where the host supports creating them. These Windows branches
   did **not** execute on Linux.

## Verification

| Gate on final changed implementation | Result |
| --- | --- |
| Editor typecheck/build and unit tests | 11/11; zero failures/skips |
| Real Linux VS Code 1.144 launch cases | 8/8, exact events retained |
| Debug integration | 44/44, zero failures/skips |
| Release integration, final rerun | 44/44, zero failures/skips |
| Fatal ASan/UBSan/leak integration | 44/44, zero failures/skips |
| Final CLI contract/presentation/path focused gates | 3/3 per profile |
| Unchanged Release selection-test repetitions | 10/10 |
| Retained actual JSON reports against version-1 schema | 46/46; two negative semantic null assertions |
| Additional tiny-program O0/O2 object comparison | 2/2 byte-identical before/after |

The integration selector covers all `project_*`/`cli_*` registrations, VOIDUP
presentation, onboarding, source CLI, both frontend depth tests, allocation
elision and its O0/O2 AOT registrations. Original native cases retain expected
missing-symbol failure/exit 1, legacy exit 19 and successful mixed interop/exit 0
in all profiles. Exact lists/argv/environments, XML and exit receipts are retained.
Both required frontend registrations retain the actual 1,048,576-byte child
stack. Fatal settings are `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The new C++ path test links
the existing `void_options`, including fatal-profile ASan/UBSan instrumentation.

The real-editor cases are cancel missing-task→restore→F5 in one host,
concurrent launch refusal, active-child cancellation then fresh retry, project
entry change then fresh retry, slow-task Debug Anyway, missing-task exception
Debug Anyway, exit-zero-without-publication and older compiler fallback.
Only authorized successful builds start adapters. Old artifacts remain intact
in failed/override/zero-publication controls. Cancellation/selection retries
have distinct task receipt IDs. These are isolated development hosts using the
existing editor and LLDB adapter, not extension installation.

### Preserved unsuccessful evidence

The first Release integration run was **43/44**: the unchanged
`project_selection` race fixture expected VOID5009 but received fail-closed
VOID5011, "Project changed during discovery; retry." Its original XML, status 8
and log are retained separately. Its test binary predates these corrections;
neither its source nor the compiler discovery implementation changed. The
fixture mutates files from an `IN_ACCESS`-notified thread and assumes which
capture boundary observes the mutation. [inotify](https://man7.org/linux/man-pages/man7/inotify.7.html)
reports access, not a compiler-phase barrier. This supports a scheduling-race
explanation; no thread trace proves the precise interleaving. Ten unchanged
binary repetitions and the complete Release rerun pass without changes to
assertions, diagnostics, timeouts or source. The intermittent classification
failure remains a qualification caveat, not a claimed repair.

Intermediate evidence also retains a TypeScript optional-value narrowing error,
a test Thenable typing error, an unreaped synthetic child in the updated unit
fixture, task-definition identity/name lookup mistakes in the first draft,
and selection fixtures that initially selected legacy mode or introduced an
ambiguous default entry. The final fixture explicitly pins its original
manifest root/entry before changing selection. An attempted POSIX case-sibling
publication was rejected by the existing stricter collision policy; the direct
helper test instead verifies POSIX comparison without weakening that policy.
Initial multi-target make invocations encountered the new target before the
regenerated top-level target list was reloaded; separate builds succeeded.
No unsuccessful result was reclassified as a pass.

## Performance and qualification limits

Twenty alternating paired samples per mode, with two excluded warmups per
binary, compare the preserved before-correction CLI translation unit against
the repaired Release compiler. The before executable was reconstructed outside
the checkout from the exact preserved CLI sources and unchanged accepted
frontend/backend/runtime archives, sharing current build-identity metadata.
It is not claimed byte-identical to the overwritten earlier final binary.
Source/build argv and executable/library identities are retained. Task-owned
builds/tests were quiescent; ordinary desktop, VM and Steam/RimWorld activity
was not controlled.

| Tiny source check | Before median ms | After median ms | Delta |
| --- | ---: | ---: | ---: |
| Detailed instrumentation disabled | 6.472 | 6.508 | +0.55% |
| Text timing enabled | 6.252 | 6.238 | -0.23% |
| Explicit JSON report | 13.848 | 14.190 | +2.47% |

No substantial regression was observed in these bounded samples. Raw values,
p95/worst and unfavorable samples remain. This is not an editor-latency or
all-workload guarantee. Security admission is preserved; the extra physical
destination check runs only for requested JSON reports. Boss's prior opt-in JSON
cost acceptance supersedes historical pending statements in earlier records.

Native qualification remains Linux x64. A running Windows 11 VM was detected,
but guest-control authentication failed before any guest command executed.
No credentials/settings, VM state or installed compiler prefixes were changed.
Windows execution, its new native path branches and Windows F5 are
**UNQUALIFIED**; macOS native execution also did not run. Linux tests and source
review do not substitute for those native gates. The pinned LLVM SDK is not
itself sanitizer-built; generated JIT code is not proven fully instrumented;
RTTI/vptr qualification remains unavailable. The existing complete matrices
remain Debug 516/516, Release 503/503 and strict 493/493 from the previous
qualification. This correction adds one registration (517/504/494 total), but
does not claim newly rerun full matrices. Earlier 600 low-stack repetitions,
54 valid deep executions and 28+28 object comparisons remain preserved and
applicable to unchanged backend/frontend inputs.

The accepted non-blocking AOT incident remains unexplained, not claimed fixed.
Steam/RimWorld is contemporaneous context, not established causation. No new
AOT crash occurred in these correction gates.

## Inventory and retention

The initial three-finding correction touched fourteen paths: `CMakeLists.txt`,
`editors/vscode/package.json`, `editors/vscode/src/extension.ts`,
`editors/vscode/src/debugBuildReceipt.ts`,
`editors/vscode/src/test/debugBuildReceipt.test.ts`,
`editors/vscode/src/test/g75LaunchHost.ts`,
`editors/vscode/src/test/runG75Host.ts`, `src/main.cpp`,
`src/cli_presentation.hpp`, `src/cli_report_path.hpp`,
`tests/cli/presentation.py`, `tests/cli/report_path.cpp`,
`docs/projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md`, and this record.
The last helper, C++ test and this record are the three added paths; the other
eleven were already in the candidate. The initial correction candidate contained **59 paths, 41
modified and 18 new**. The focused follow-up below adds the existing tracked
project-selection test to the candidate: **60 G75 paths, 42 modified and 18 new**.
Boss's concurrent `docs/WRENCH_CLANKER.md` edit is separate and was left untouched.
There are no staged paths. See the
[exact inventory](G75_CLI_EXPERIENCE_IMPLEMENTATION.md#exact-changed-file-inventory).
`git diff --check` passes with no output. Commit readiness awaits independent
review/Boss approval and explicit acknowledgement of native platform limits.

Raw evidence is retained at `/tmp/void-g75-review-corrections-dhqo94v0` and in
`/home/shmellyorc/Documents/Void.Language-archives/g75-three-review-corrections-2026-11-18.zip`.
The archive includes before/after sources, commands, output, hashes, original
unsuccessful results and final receipts with a verified restoration manifest.
Its SHA-256 is recorded beside the archive, outside the archive's own content.


## Focused re-review and authorized follow-up

18 November 2026. The 59-path candidate was inspected directly, including all
513 hashed archive members, raw final XML, compiler hashes, timing samples,
negative JSON and the actual editor case bodies/events. The review returned
**CORRECTION REQUIRED** for the two bounded issues below. Boss then authorized
proceeding with their correction; the resulting candidate is ready for review
with the existing native-platform limits. No new milestone or HOLD was created.

### Findings and minimal corrections

- **P2 — Project-selection fixture scheduling.** The original
  `tests/project/selection.cpp` symlink mutation removed `src/app.void` and then
  created its replacement in separate operations, while another thread let
  analysis continue. `ProjectCapture::directory` compares directory membership
  at `src/frontend/project_selection.inc:213`, before leaf alias admission. A
  valid observation of the intermediate missing file produces VOID5011 at line
  156; an observation after replacement produces the asserted VOID5009. Thus
  VOID5011 is a correct fail-closed compiler result for that intermediate
  filesystem state. The fixture did not establish the ordering needed for its
  exact diagnostic assertion. This is a test synchronization defect, not
  evidence of unsafe compiler acceptance.

  A scratch reproducer deliberately held that missing-file window open. Debug
  GDB stopped in `ProjectCapture::directory`→`changed`; `/src` contained only
  `zz-slow.void`. It reproduced the exact VOID5011 message. This establishes a
  concrete mechanism matching the historical failure; the original historical
  thread interleaving was not captured and is not retroactively claimed proven.

  The test now uses the existing invocation-local observation callback at
  semantic-analysis entry, after source capture and before recapture. The
  mutation finishes synchronously before compilation resumes. No new production
  hook, delay, exception suppression, diagnostic alternative or public API was
  added. Exact diagnostic/source-span checks remain; failure now also explicitly
  requires no published `ProjectAnalysis`. Mutator exceptions are rethrown.
  The independent concurrent mutation tests later in the file remain unchanged.

  [Roslyn's compiler-cache test](https://github.com/dotnet/roslyn/blob/5e0f3ce851031b964ee84f5493ba24b73f34d3f4/src/Compilers/Server/VBCSCompilerTests/CompilationCacheTests.cs#L974-L1026)
  uses explicit acquired/release synchronization rather than assuming thread
  order. [MSBuild's analysis](https://github.com/dotnet/msbuild/blob/main/documentation/specs/rar-core-scenarios.md)
  explains asynchronous file notifications cannot establish the required
  observation ordering. These support the test-control principle; neither is
  claimed to contain an identical VOID defect or a directly imported patch.

- **P2 — Nested selected-project F5 compatibility.** The task lookup compared its
  workspace-folder scope to the selected compiler project root. Those locations
  differ in a supported parent-workspace/child-project configuration. The real
  editor reproduced build task status 0 with no receipt and no adapter launch.
  The predicate in `extension.ts` now matches the task to its actual workspace
  folder, as the existing resolver does. Selected root/entry, invocation ID,
  child success and new-artifact checks remain authoritative. The new real
  `nested` editor case verifies explicit child `--project`, a numeric receipt
  and a successful adapter start. The original eight protections still pass.

Timing configuration passes re-review: the effective fields remain null before
successful resolution, including the retained malformed-manifest and rejected
profile reports. Valid profiles/cache cases retain their semantic assertions.
Windows comparison fixes the specific drive-case issue, remains Windows-only,
and preserves physical publisher checks. Drive-letter casing alone is not
claimed to establish full Windows filesystem identity. Native Windows and macOS
remain **UNQUALIFIED**; the guest authentication failure is preserved.

### Final qualification

| Final follow-up gate | Result |
| --- | --- |
| Full registered project-selection test, repeated without source changes | 10/10 per profile; 30/30 total |
| Debug integration | 44/44, zero failures/skips |
| Release integration | 44/44, zero failures/skips |
| Fatal ASan/UBSan/leak integration | 44/44, zero failures/skips |
| Editor build/typecheck and units | 11/11 |
| Real Linux VS Code launch cases | 9/9, including nested selected project |
| Preliminary controlled recapture probes | 30 Release + 10 Debug + 10 strict; 50/50 |

The full registered repetitions retain their existing 300-second timeout; no
limit was extended. The two original frontend tests still run with the real
1,048,576-byte child limit. Fatal sanitizer/leak settings are unchanged. Original
43/44 XML/status/output and all prior archives remain preserved. Initial scratch
unused-function warnings, the debugger's first pretty-printer errors and a
missing-field initializer warning are retained; the integrated final builds
have no new warnings. The last warning was corrected by explicit initialization.

All six compiler/server executables are byte-identical to the preceding
three-finding qualification. The existing bounded +0.55% default, -0.23% text and
+2.47% JSON timing samples therefore remain applicable to the same binaries;
no fresh compiler performance or code-generation claim is needed. No editor
latency benchmark is claimed for the one-predicate workspace-scope correction.
Earlier complete 516/503/493 suites remain historical evidence, not newly rerun
full 517/504/494 matrices. No AOT incident recurrence occurred in this follow-up.

This follow-up touches exactly six G75 paths: `tests/project/selection.cpp`,
`editors/vscode/src/extension.ts`,
`editors/vscode/src/test/g75LaunchHost.ts`,
`editors/vscode/src/test/runG75Host.ts`, this record, and
`docs/projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md`. No compiler frontend/backend, ABI,
installed-prefix, permission, parser, nesting-limit or optimization change was
made. `git diff --check` passes. No staging, commit, push, install, publication or
hosted CI occurred. G69 remains on its existing HOLD and G76 remains queued.

Raw follow-up evidence is `/tmp/void-g75-rereview-jkptvfuq`. Its preserved archive
is `/home/shmellyorc/Documents/Void.Language-archives/g75-focused-rereview-followup-2026-11-18.zip`,
with a SHA-256 receipt beside it. This includes the prior review, before/after
fixture sources, forced-gap debugger evidence, commands, repeated/full gate
results, nested-launch before/after events, source hashes and upstream findings.
