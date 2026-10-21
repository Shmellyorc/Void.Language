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

## Current stage and acceptance — 20 October 2026

Stage 8 Sprint 3 is **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending**.
Debug and strict sanitizers passed **145/145 each**. Release passed **144/145**,
then its obsolete arrays_foundation diagnostic assertion was repaired and passed
its focused **1/1** rerun: **435/435 final unique configuration/suite outcomes**.
The original failure is retained, and no production source changed for that test
repair. Both 1 MiB frontend gates passed. See [the Sprint 3 contract](STAGE8_TEXT_COLLECTIONS.md)
and [exact acceptance evidence and limits](stage8/SPRINT3_ACCEPTANCE.md).

All three Sprint 3 API decisions are approved in [DECISIONS.md](DECISIONS.md).
No Sprint 3 owner decision remains pending. Queries are runtime-only; builtin
method values are absent, and worker calls retain conservative admission. The
contract records these limits; no broader API or async admission is claimed.
Sprint 1 remains locally accepted by Boss, with its
[CI #82 Windows repair](stage8/CI82_WINDOWS_REPAIR.md) source/test hashes preserved.
Sprint 2's named/default, numeric, tooling and reload contracts remain covered.

**Hosted CI remains deferred until the end of Stage 8**, unless Boss explicitly
authorizes an earlier run. No hosted CI was run or requested. Windows/macOS,
Windows Debug/Release CI #82 execution and fresh actual editor-client verification
remain pending; Linux LSP pipe tests do not establish them. Stage 8 is not hosted locked.

**Stopped after Sprint 3 for Wrench review. Sprints 4–6 remain future work.**
Do not begin Sprint 4 without explicit Boss authorization. No commits, pushes,
branch/remote changes, publication or website work occurred. Root README is
unchanged; future sprint prompts direct documentation updates under `docs/` and
require separate explicit Boss authorization for public-facing root README edits.


## Engineering contracts, decisions and handoffs

- [Language completeness roadmap](LANGUAGE_COMPLETENESS_ROADMAP.md),
  [pre-implementation research audit](LANGUAGE_COMPLETENESS_AUDIT.md)
  and [owner approvals and pending decisions](LANGUAGE_COMPLETENESS_APPROVALS.md).
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
