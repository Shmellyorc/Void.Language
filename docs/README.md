# VOID documentation

## Getting started and language guides

- [Build, source compiler and CLI](SOURCE_COMPILER.md),
  [editor setup](EDITOR_INTEGRATION.md) and [examples](../examples/).
- [Modules and imports](MODULE_SYSTEM.md), [numeric types](NUMERIC_TYPES.md),
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

## CI #85 Windows editor repair handoff — 21 October 2026

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
