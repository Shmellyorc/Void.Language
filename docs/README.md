# VOID documentation

## File I/O and filesystem sprint — 26 October 2026

Boss authorized complete `io` and `path` modules from `084fe1eb`, including real
native async jobs, recursive filtered directory lookup and practical host paths.
The [contract](io/CONTRACT.md), [public API completeness audit](io/API.md) and
[engineering record](io/IMPLEMENTATION.md) cover this scope. The
[Golden Wrench handoff](io/HANDOFF.md) records **182/182** final selected local
tests passing, the earlier unresolved Release SIGSEGV and native-platform limits.
This is not an acceptance lock. `os` and application-data discovery
remain future work. Root README and `stdlib/console.void` remain untouched.
No hosted CI or Git mutation is authorized. Stop for Golden Wrench source review
after the completed local completeness audit; implementation work has stopped.

## Combined standard-library sprint — 26 October 2026

Boss authorized the error hierarchy, console completeness, convert module and
measured memory-write optimization as one scope, including the reported VOIDLS
constant-completion crash repair. The [current contract](combined/CONTRACT.md),
[implementation handoff](combined/IMPLEMENTATION.md) and [concise local receipt](combined/evidence/RECEIPT.md)
supersede earlier binary/memory source protection and copy-cost statements for
this sprint. Prior lock evidence remains historical. No hosted CI or Git mutation
is authorized; Golden Wrench review remains the next gate.

## Standard-library Sprint 2 — io/memory, 26 October 2026

Boss authorized the complete memory-stream slice after independent Wrench source
review of binary commit `64c485c5beb5df519e998fdb838a640fd9758376`. The
[memory contract and audit](memory/CONTRACT.md) and
[implementation and local evidence](memory/IMPLEMENTATION.md) define this scope.
Console, root README and binary source remain protected. No hosted CI or later
module is authorized; stop after local completeness review for Golden Wrench.

## Standard-library Sprint 1 — encoding/binary, 26 October 2026

Boss authorized this bounded module after the locks below. Its
[public contract and capability audit](binary/CONTRACT.md) and
[implementation/review evidence](binary/IMPLEMENTATION.md) cover the actual source
API, compiler support, tests and remaining acceptance limits. Historical lock
records remain unchanged. No `io/memory` or other module is authorized by this sprint.

## Documentation/hover repair: OFFICIALLY ACCEPTED AND LOCKED, 26 October 2026

**Boss confirms Golden Wrench source review and final hosted acceptance** at tested
commit `94b00c682314fb239ba2dfdd99c02df87ba0a8af`.
[Final CI 40271408951](https://github.com/Shmellyorc/Void.Language/actions/runs/40271408951) used
`ci-link-experiment.yml`, mode `thinlto-cache`: **7/7 jobs passed on attempt 1;
1,421/1,421 CTests passed**. Ubuntu Debug/Release/Sanitizers passed **205/205 each**,
Windows Debug/Release **199/199 each**, macOS ARM64 Debug/Release **204/204 each**.
Documentation-specific LSP and execution tests passed in all seven configurations;
hosted Debug editor integration passed on Linux, Windows and macOS. Prior local
focused acceptance remains **57/57**, with additional editor and stack evidence.

[Accepted contract](DOCUMENTATION_HOVER.md) and
[acceptance record, historical local evidence and client/version limits](DOCUMENTATION_HOVER_IMPLEMENTATION.md)
retain uniform automatic/manual suppression for undocumented parameters, nonfatal
warnings, filtered hover and compiler-owned result/pattern metadata. This is a
separately accepted tooling/documentation repair. Stage 11's locked evidence and
historical LLVM DSE observation below remain unchanged; no DSE fix is claimed.
Earlier future/pending hover-repair statements are superseded only for this scope.
Boss retains Git, CI and release control; root README remains protected.
**No next-stage engineering is authorized. Stop for Golden Wrench documentation
review of this documentation-only acceptance lock.**


## STAGE 11: OFFICIALLY LOCKED, October 25, 2026

**Golden Wrench reviewed and accepted Stage 11; Boss officially authorizes the
runtime globals lock** at tested commit
`e8633b246fe6139d39d830afd4cb249f7dab4d8e`.
[Final CI 40248573233](https://github.com/Shmellyorc/Void.Language/actions/runs/40248573233)
used `ci-link-experiment.yml`, `mode=thinlto-cache`: **7/7 jobs successful on
attempt 1; 1,407/1,407 CTests passed**. Ubuntu Debug/Release/Sanitizers passed
203/203 each, Windows 2022 Debug/Release 197/197 each, and macOS ARM64
Debug/Release 202/202 each. Hosted Debug editor integration passed on Linux,
Windows and macOS. Earlier local Linux acceptance passed 609/609 separately.
[Official acceptance](stage11/ACCEPTANCE.md), [contract](RUNTIME_GLOBALS.md) and
[implementation handoff](stage11/IMPLEMENTATION.md) define the locked scope.

The locally reproduced LLVM 19.1.7 DSE crash remains an
[unexplained reliability observation](stage11/OPTIMIZER_FAILURE.md). Subsequent
execution passed without a demonstrated repair, and the crash did not recur in
hosted acceptance. It is not claimed fixed; the local ThinLTO cache/link failure
is a separate incident. Stage 10's native Vim/Neovim host and physical
second-volume Windows exclusions remain exclusions, not passing tests.

This lock supersedes earlier unauthorized/pending statuses only for Stage 11
runtime globals. Historical evidence, hashes and source-freeze records remain
unchanged. Compiler-owned hover/documentation repair, standard-library expansion
and package-manager work remain future work. **No next engineering stage is
authorized.** Boss retains Git, CI and release control; the root README stays protected.


## STAGE 10: OFFICIALLY LOCKED, October 25, 2026

**Boss officially approves the release lock** at commit
`d507240484f94aa4782bdb8828ea1ba0017bcda6`.
[Final CI 40198872407](https://github.com/Shmellyorc/Void.Language/actions/runs/40198872407)
passed **7/7 jobs and 1,383/1,383 main CTest executions** using
`ci-link-experiment.yml`, `mode=thinlto-cache`. Golden Wrench accepted all five
measured Windows console boundaries; targeted native Windows/macOS 1 MiB stack
verification passed. [Official acceptance](stage10/ACCEPTANCE.md) records scope
and evidence.

Native Windows/macOS Vim/Neovim host execution and physical second-volume Windows
verification remain **Boss-accepted release exclusions, not passing tests**.
This section supersedes earlier dated incomplete/unverified statuses for accepted
scope; historical failures, evidence and reviews remain preserved below.
**Stage 11 is not authorized.**

## Current Stage 10 handoff — 24 October 2026

ThinLTO experiment [#4](https://github.com/Shmellyorc/Void.Language/actions/runs/40118666209)
at `90c0cf64b9aba60d7c642096d2aaac45aae70d56` passed **7/7 jobs and
1,383/1,383 main CTest executions**, including both former macOS Release console
failures, Linux strict sanitizers and applicable Debug editor integration.
Overall interval: **1 h 8 m 44 s**, 18:00:03–19:08:47 UTC on 24 October.
Main CI is unchanged; CI #98 and experiment #3 failures remain historical evidence.

Step 43 contracts/examples/evidence reconciliation is complete. Step 44 is
prepared for independent Golden Wrench review and Boss release decisions.
**Stage 10 remains active and INCOMPLETE:** real Windows console/ConPTY and exact
native 1 MiB stack receipts remain unverified. The experimental green result
does not issue a release lock. See [final acceptance](stage10/ACCEPTANCE.md),
[current implementation](stage10/IMPLEMENTATION.md) and
[exact Step 42 evidence](stage10/engineering/step42-results.json).
Earlier dated statuses below remain history. No next stage is authorized.

[Runnable Stage 10 examples](stage10/EXAMPLES.md),
[future stdlib/global design](VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md),
[independent CI #4/design review](VOID_WRENCH_REVIEW_CI4_STDLIB_GLOBALS_2026-10-24.md)
and [permanent stage workflow](WRENCH_CLANKER.md#permanent-workflow-for-every-new-development-stage).

## Stage 10 research design — 21 October 2026

[Complete design and ordered roadmap](STAGE10_DESIGN.md) for console I/O, shared
display/formatting/interpolation, private/readonly fields and operator overloading.
Includes current-source evidence, original VOID console findings, full behavior
contracts, 44 engineering steps and focused acceptance criteria. **Boss authorized the complete 44-step implementation, B1–B4 and Wrench
amendments on 21 October 2026. Engineering is in progress; acceptance is pending.**
[Current source progress, all 44 obligations and open matrix rows](stage10/IMPLEMENTATION.md).

## Windows Stage 9: RESOLVED at CI #95 — 21 October 2026

[Permanent Windows engineering record, CI #88–#95](stage9/WINDOWS_ENGINEERING_RECORD.md):
verified baseline `e25816bdb80e3f32613edf77c9cf3258232af966`, Windows Debug and
Release **160/160 each, 320/320 combined**. [Acceptance artifacts and hashes](stage9/CI95_WINDOWS_ACCEPTANCE.json)
include both native logs and observed ACLs/reservations. The blocking investigation
is resolved; older pending handoffs below retain their historical state.
Actual second-volume transfer and complete seven-job matrix verification are not
claimed. [Wrench's permanent guardrails](WRENCH_CLANKER.md#permanent-windows-guardrails)
carry these lessons into future work.


## CI #93 private Windows test roots — 21 October 2026

[Fixture repair, inherited-ACL investigation and Wrench gate](stage9/CI93_WINDOWS_TEMP_ROOT_FIXTURE.md).
Compiler/security checks remain unchanged. Normal and overlong test TMP/TEMP
roots now use verified private DACL creation. Linux Debug/strict each **3/3**;
new native acceptance pending. Historical CI #93 Windows jobs each **159/160**.


## CI #92 focused Windows follow-up — 21 October 2026

[Directory reservation, Unicode repair and native evidence gate](stage9/CI92_WINDOWS_RESERVATION_REPAIR.md).
Linux Debug/strict each **3/3**; actual old-mask movement/handle identity and
repaired Windows reservation remain pending. Working native linker preserved.

## CI #91 focused Windows test repair — 21 October 2026

The working short-directory native linker is preserved. Restricted-token tests
now explicitly request duplicate/query rights; genuine deep executable tests use
Python's explicit extended Windows application path while retaining location,
identity, depth and exit-status checks. Linux Debug and strict sanitizers each
passed **3/3**. **Native Windows acceptance remains pending**; Linux and syntax
checks do not establish that the launch approach works on Windows.
[Source rationale, CI #91 failures, evidence and Wrench gate](stage9/CI91_WINDOWS_TEST_REPAIR.md).
Earlier CI #90 records remain historical; no compiler, manifest, Git or hosted
workflow changes are included.


## CI #90 Windows linking and permanent handoff — 21 October 2026

[Implemented repair and native acceptance gate](stage9/CI90_WINDOWS_NATIVE_LINK_REPAIR.md):
short private Windows link directory, verified transfer and unchanged atomic
publication. Linux Debug and strict sanitizers each **9/9**; Windows Debug/Release
**160/160 pending native verification**. Historical CI #90 remains **158/160** in
both jobs; CI #88's 8.3 repair subsequently passed. Both longPathAware manifests
and original deep-path regressions remain. Earlier dated entries below are history.

Authoritative current records: [native-build contract](STAGE9_APPLICATION_WORKFLOW.md),
[local evidence](stage9/CI90_EVIDENCE.json), [Wrench succession and Boss governance](WRENCH_CLANKER.md#permanent-boss-decisions--current-governance-21-october-2026)
and [future Git-based package-manager direction](LOCAL_LIBRARIES.md#future-external-package-manager--boss-direction-21-october-2026).
No package manager, root README edit, Git mutation or hosted dispatch is included.


## CI #89 verified Windows manifest repair — 21 October 2026

Native diagnosis #1 confirmed Windows error 3 at a 353-unit deep directory; the
identical longPathAware control succeeded with LongPathsEnabled=1. The tested
manifest now applies to both voidc and voidls using the same CMake mechanism.
[Exact repair, native evidence and policy dependency](stage9/CI89_WINDOWS_MANIFEST_REPAIR.md):
Linux Debug and strict sanitizers each passed 4/4 focused suites. Production
Windows Debug/Release verification remains required. No Git mutation or hosted
execution was performed; historical evidence is preserved.


## CI #89 Windows long-path diagnosis — 21 October 2026

CI #89 Windows Debug/Release each passed 158/159; the CI #88 regression suite
passed both. The remaining deep-path failure hides its Windows error code.
[Investigation and manual probe](stage9/CI89_WINDOWS_LONG_PATH_INVESTIGATION.md)
provide two isolated native controls and a manual-only single-job workflow for
Boss review. Native results are pending; no speculative path repair or hosted
action was performed. Historical CI #88/#89 evidence is preserved.


## CI #88 Windows output-path repair — 21 October 2026

Boss reports Linux/macOS jobs passed and Windows Debug/Release each 152/158, with
six failures rejecting the legitimate RUNNER~1 ancestor. The focused native path
repair admits exact OS-registered short directory ancestors while retaining exact
descendants/output leaves, reparse rejection and source/hard-link protection.
[Repair and native gate](stage9/CI88_WINDOWS_PATH_REPAIR.md),
[focused evidence](stage9/CI88_EVIDENCE.json): Linux Debug/Release/strict sanitizers
8/8 each (24/24); native Windows post-repair checks remain mandatory and unexecuted
locally. No tests/expectations were removed, full matrix repeated, Git mutation or
hosted action performed. Earlier Stage 9/CI evidence remains historical.


## Stage 9 implemented / locally verified — Wrench review

Boss approved all three milestones on 21 October 2026; all are implemented.
Final local acceptance is 487/487 plus 1 MiB and real editor gates; Windows/macOS
and hosted acceptance remain pending. Read the
[application workflow contract](STAGE9_APPLICATION_WORKFLOW.md) and
[acceptance/handoff](stage9/ACCEPTANCE.md). The research/roadmap documents remain
historical proposals; the dated authorization supersedes their optional flag and
stop-for-approval recommendations. Existing CI #87 and local-library evidence stay
intact. Git/hosted actions remain Boss-controlled.


## Getting started and language guides

- [Build, source compiler and CLI](SOURCE_COMPILER.md),
  [editor setup](EDITOR_INTEGRATION.md) and [examples](../examples/).
- [Modules and imports](MODULE_SYSTEM.md), [local libraries](LOCAL_LIBRARIES.md), [numeric types](NUMERIC_TYPES.md),
  [value semantics](VALUE_SEMANTICS.md) and [strings](STRINGS.md).
- [Value structs](VALUE_STRUCTS.md), [managed structs](MANAGED_STRUCTS.md),
  [initialization](INITIALIZATION.md), [composition](COMPOSITION.md),
  [inheritance](INHERITANCE.md), [receiver methods](RECEIVER_METHODS.md)
  and [properties](PROPERTIES.md).
- [Fixed arrays](FIXED_ARRAYS.md), [growable arrays](GROWABLE_ARRAYS.md),
  [array operations](ARRAY_OPERATIONS.md) and [dictionaries](DICTIONARIES.md).
- [Generics](GENERICS.md), [interfaces](INTERFACES.md),
  [advanced language features](ADVANCED_LANGUAGE.md), [async](ASYNC.md),
  [native bindings](NATIVE_LIBRARIES.md) and [GC Runtime](GC_RUNTIME.md).
- [Diagnostics](DIAGNOSTICS.md), [language server](LANGUAGE_SERVER.md)
  and [known issues](KNOWN_ISSUES.md).

The [Language Bible](LANGUAGE_BIBLE.md) describes the broader design.
Implementation guides and acceptance evidence determine what works today;
proposals and approved future work are not implementation evidence.

## Clanker entry point and permanent documentation policy

**Recorded 20 October 2026; applies to Keyboard, Research and Wrench Clankers.**

Read this index, [Wrench instructions and current handoff](WRENCH_CLANKER.md),
[current development status](DEVELOPMENT_STRATEGY.md#current-implementation-status)
and [the approval ledger](LANGUAGE_COMPLETENESS_APPROVALS.md) before planning work.
[Research instructions](RESEARCH_CLANKER.md) provide the research role's context.
Only implement the sprint Boss has authorized.

Keep Clanker instructions, AI workflow notes, sprint handoffs, internal status
reports and development-process discussion under `docs/`. The repository root
`README.md` is for public-facing installation, usage and language documentation.
Modify it only when Boss explicitly requests a public-facing README change.
Routine sprint authorization does not grant that permission. This rule supersedes
older instructions to update the root README as part of development.

Future sprint prompts must direct documentation updates to `docs/` by default.
Use this instruction in future prompts: “Read `docs/README.md` first. Update the
relevant documentation and permanent handoffs under `docs/`. Modify the root
README only if Boss explicitly requests a public-facing README change.”

Preserve historical evidence and distinguish approved direction, local verification,
review and hosted locks. Never mark work implemented or locked without verification.

## Stage 9 research handoff — 21 October 2026

Boss authorized **Stage 9 research and planning only** in the current mission.
[Independent gap audit](STAGE9_RESEARCH.md) and [proposed milestone roadmap](STAGE9_ROADMAP.md)
review the current local-library source, remaining CLI/Runtime/language/tooling
boundaries and the native executable workflow. Fresh focused Release verification
passed 16/16 suites; source/CLI and real AOT probes retain their own evidence.
Recommended scope is automatic application dispatch without mandatory `--app`,
preserved legacy behavior, `voidc build` and consistent failures/artifact handling.

This authorization supersedes older statements prohibiting Stage 9 **research**,
not Stage 8's pending hosted lock or any engineering gate. Implementation remains
unauthorized. The future Git-based package manager is separate. Existing CI #87
repair/workflow edits are preserved; root README, production and hosted CI are
unchanged by research. Stop for Wrench review and Boss authorization.

## CI #87 extension path repair — 21 October 2026

[Focused repair evidence](ci87-extension/REPAIR.md) records the Debug directory
case mismatch and the existing `VOIDLS_TEST_SERVER` step override. TypeScript/build
and all four real-server units passed locally. All seven matrix configurations,
strict settings and real editor tests are preserved. Wrench review and hosted
verification remain pending; historical CI #87 evidence is unchanged.

## Project-local libraries handoff — 21 October 2026

[Local libraries contract](LOCAL_LIBRARIES.md) and
[acceptance evidence](local-libraries/ACCEPTANCE.md) record the owner-authorized
`libs/` / `use lib/...` implementation, shared VFS identities and compiler-owned
editor assistance. Local verification is separate from already-running CI #87;
its historical Stage 8 evidence is preserved. Wrench review and native
Windows/macOS execution remain pending. No hosted lock or Stage 9 progression.

## Incomplete-source crash / CI #86 handoff — 21 October 2026

[Repair evidence](stage8/CI86_INCOMPLETE_SOURCE_REPAIR.md) records the exact
`$application` candidate-map miss while completing `u`, its Debug trace and
baseline exit 2. Candidate insertion/documentation now share the same eligibility
invariant. Incomplete typing recovers without restarting the server; CI #86's
separate selected-filename slash assertions are normalized without production
path changes. Focused Linux suites passed 10/10 per configuration, extension units
4/4 each and actual VS Code profiles 3/3 each. Native Windows and Wrench review
remain pending; no hosted lock or subsequent stage is implied.

## Historical CI #85 Windows editor repair handoff — 21 October 2026

At `e8a1ea7433b465c3800090a7b84737d7474c9228`, Windows Debug passed 156/156
compiler suites and extension units, then failed actual editor discovery with a
lowercase drive. The [focused repair and evidence limits](stage8/CI85_WINDOWS_EDITOR_REPAIR.md)
cover drive-only boundary equivalence, retained path security and expanded native
regressions. Linux focused Debug/Release passed 7/7 each and units 4/4 each;
real editor profile outcomes are retained in the repair evidence. Native Windows
Debug/Release execution is still required. No hosted lock or next stage is implied.

## Historical CI #83/#84 discovery repair handoff — 21 October 2026

At `dbe01209e4e32da01260b4faaa66b8ce4020ad82`, CI #84 Windows Debug passed
156/156 compiler suites, then failed an extension path-separator assertion;
Windows Release succeeded. CI #83 macOS Release passed 155/156, failing discovery
through `/var`. The [cross-platform repair](stage8/CI84_DISCOVERY_REPAIR.md)
normalizes exact editor-test comparisons and admits POSIX workspace-ancestor
spellings while retaining descendant alias/security checks. Focused Linux
Debug/Release passed 7/7 each; extension checks and all four unit tests passed.
Both macOS configurations were inspected; available Debug evidence remains
incomplete. Native macOS Debug/Release and Windows editor verification are pending.
**Stage 8 is not hosted locked.** No hosted action or next stage is authorized.

## Historical CI #83 repair handoff — 21 October 2026

Owner-run CI #83 at `fbad2b642114a692f93d2f4ce7d3465ea489abf2`
built Windows Debug/Release successfully; each passed 153/156 suites. The three
Unicode CLI/fixture failures have a [focused repair record](stage8/CI83_WINDOWS_REPAIR.md).
Final local focused outcomes are 7/7 each in Linux Debug/Release after correcting
one regression expectation; baseline cp1252 fixture failures were reproduced.
Windows execution of the repair remains pending; Stage 8 is **not hosted locked**.
The Sprint 6 acceptance below describes its earlier local snapshot, not CI #83.

## Current stage and acceptance — 21 October 2026

Stage 8 Sprint 6 is **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending**.
The final planned engineering sprint delivers compile-time `Type has Target`,
indented typed if-expressions, delimiter-aware multiline calls and bounded
**string-only** automatic interpolation through existing compiler/IR/LLVM paths.
All four owner decisions were answered before implementation.

Final unfiltered Debug, Release and strict ASan/UBSan/leak acceptance passed
**156/156 each: 468/468 unique configuration/suite outcomes; zero final failures**.
Both complete Debug/Release frontend gates passed on real 1 MiB stacks. Extension
unit checks passed 4/4; compiler-owned stdio tooling and headless Vim syntax checks
passed. These do not establish fresh interactive-client acceptance.
[Contract](STAGE8_ERGONOMICS.md), [Sprint 6 acceptance](stage8/SPRINT6_ACCEPTANCE.md)
and [final hosted checklist](stage8/FINAL_HOSTED_GATE.md) retain exact scope,
source/log hashes, unsuccessful engineering attempts and verification limits.

All six planned Stage 8 sprints now have bounded local implementation evidence.
Sprints 1–5 remain preserved, including [CI #82 repair](stage8/CI82_WINDOWS_REPAIR.md),
call/numeric/text/collection APIs, application entry, static providers and measured
async frame retirement. Native Task/result/Work records, null Handle registration
and capacities still grow within long-lived roots; no constant-memory claim.

**Stage 8 is not hosted locked.** Wrench review, Boss-controlled commit/push,
explicit final hosted authorization, all seven hosted jobs and fresh actual editor
checks remain pending. Linux results do not establish Windows/macOS, including
Windows Debug/Release short/long-path and reparse-point repair execution.

**Stopped after Sprint 6 for Wrench review. No next sprint or Stage 9 is authorized.**
Formatting/conversion beyond strings, triple strings, recursive component search,
conditional compilation, broader task ownership and other deferred Bible features
remain future work. No commits, pushes, branch/remote changes, hosted CI or publication
occurred. Root README and CI #82 repair hashes are unchanged. Documentation and
future prompts belong under `docs/`; public root README work needs explicit Boss
approval.


## Engineering contracts, decisions and handoffs

- [Language completeness roadmap](LANGUAGE_COMPLETENESS_ROADMAP.md),
  [pre-implementation research audit](LANGUAGE_COMPLETENESS_AUDIT.md)
  and [owner approvals and pending decisions](LANGUAGE_COMPLETENESS_APPROVALS.md).
- [Stage 8 selected ergonomics](STAGE8_ERGONOMICS.md),
  [Sprint 6 local acceptance](stage8/SPRINT6_ACCEPTANCE.md) and
  [final hosted gate checklist](stage8/FINAL_HOSTED_GATE.md).
- [Stage 8 async retention](STAGE8_ASYNC_RETENTION.md),
  [Sprint 5 local evidence](stage8/SPRINT5_ACCEPTANCE.md).
- [Stage 8 applications and static providers](STAGE8_APPLICATION_NATIVE.md),
  [Sprint 4 local evidence](stage8/SPRINT4_ACCEPTANCE.md).
- [Stage 8 UTF-8 text and recoverable collections](STAGE8_TEXT_COLLECTIONS.md),
  [Sprint 3 local evidence](stage8/SPRINT3_ACCEPTANCE.md).
- [Stage 8 calls and numeric prelude](STAGE8_CALLS_NUMERIC.md),
  [Sprint 2 local evidence](stage8/SPRINT2_ACCEPTANCE.md).
- [Stage 8 discovery contract](STAGE8_DISCOVERY.md) and
  [acceptance evidence](stage8/ACCEPTANCE.md).
- [Stage 7 tooling contract](STAGE7_IMPLEMENTATION.md),
  [tooling foundations](TOOLING_FOUNDATIONS.md) and
  [tooling sessions](TOOLING_SESSIONS.md).
- [Decisions](DECISIONS.md), [work log](WORK_LOG.md),
  [known issues](KNOWN_ISSUES.md) and [permanent Wrench handoff](WRENCH_CLANKER.md).
