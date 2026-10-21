# VOID 2.0 — Boss-approved completeness direction and implementation ledger

**Decision recorded:** 20 October 2026, after Research Clanker's language
completeness audit and Stage 7 hosted acceptance.

**Current status — 20 October 2026:**

Stage 8 Sprint 1 is **locally accepted by Boss**, including preservation of the
[Windows CI #82 path repair](stage8/CI82_WINDOWS_REPAIR.md). Sprint 2 is
**IMPLEMENTED / LOCALLY VERIFIED** (429/429 suites); Wrench review is pending. See the
[calls/numeric contract](STAGE8_CALLS_NUMERIC.md) and
[local verification record](stage8/SPRINT2_ACCEPTANCE.md). All three semantic
choices are approved in [DECISIONS.md](DECISIONS.md): consistent argument binding,
the four-member numeric prelude and exact numeric edge behavior.

**Hosted CI is deferred until the end of Stage 8**, unless Boss explicitly
authorizes an earlier run. Do not run or request hosted CI for Sprint 2.
Git, branches, remote changes and publication remain owner-controlled; no such
operations are authorized. Root README remains public-facing and unchanged.
Stop after Sprint 2 local verification for Wrench review. Sprints 3–6 remain
future work; do not progress automatically. Stage 7 remains hosted locked at
`f704d75f`, CI #81 attempt 2, seven jobs, 980/980 suites. Stage 8 is not hosted locked.

## Permanent documentation policy — 20 October 2026

[docs/README.md](README.md) is the central documentation index and instruction
entry point for Keyboard, Research and Wrench Clankers. Read it before preparing
future sprint prompts or handoffs.

Clanker instructions, AI workflow notes, sprint handoffs, internal status reports
and development-process discussion belong under `docs/`. Do not add them to the
repository root `README.md`. Preserve its public-facing installation, usage and
language documentation. Modify the root README only when Boss explicitly requests
a public-facing README change; sprint authorization alone does not authorize it.

Future sprint prompts must direct documentation updates to `docs/` by default,
including the index, relevant contracts, approval ledger and permanent handoffs.
This rule supersedes any older instruction to update the root README during
routine development. It authorizes no additional sprint, commit, push or hosted lock.

## CI #82 focused Windows repair — 20 October 2026

[CI #82 repair evidence](stage8/CI82_WINDOWS_REPAIR.md) records the confirmed
Windows Release `lsp_stage8` failure and a local repair shared by Debug and Release.
Focused Linux Debug/Release checks pass; Windows short/long paths and reparse-point
checks still require hosted verification in **both** configurations. CI #82 is
not a hosted lock; its Windows Debug test step was cancelled. The earlier 423/423
local result remains historical evidence, not a full rerun of this repair.
This historical repair gate is superseded by the Sprint 2 authorization above;
Windows hosted verification remains deferred with the Stage 8 gate.

## Baseline

- Compiler: native VOID 2.0, LLVM **19.1.7**, one frontend, typed IR,
  coordinated managed Runtime, shared JIT/AOT and `voidls`.
- Stage 7: **HOSTED LOCKED**, commit
  `f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`, CI #81 attempt 2,
  seven green jobs and **980/980** CTest suites.
- Research source documents:
  [audit](LANGUAGE_COMPLETENESS_AUDIT.md) and
  [engineering roadmap](LANGUAGE_COMPLETENESS_ROADMAP.md). Their bounded probes
  describe what worked or failed **before** the following proposed sprints.
- The historical C-emitting compiler's features do not count as VOID 2.0
  implementation evidence.

## Approved roadmap and live status

| Sequence | Cohesive sprint | Status | Required completion evidence |
| --- | --- | --- | --- |
| 1 | **Nested stdlib/project imports, compiler-owned VFS discovery, project `main.void` selection, import IntelliSense, source docs/navigation, shared VS Code/VSCodium/Neovim/Vim behavior** | **LOCALLY ACCEPTED by Boss; original 423/423 local evidence and focused CI #82 repair preserved; hosted gate deferred to Stage 8 end** | Correct parsing/resolution/visibility, valid completions and source navigation, real client checks, focused and full local verification, owner-hosted 7-job CI at Stage 8 end |
| 2 | **Ordinary named/default arguments and built-in type APIs**: `i32.max_value`, `i32.abs(value)`, `value.abs()`, chaining with one canonical implementation | **LOCALLY VERIFIED, 429/429; Wrench review pending; not hosted locked** | Approved signatures and numeric edge behavior; ordinary-call/receiver/generic/JIT/AOT/tooling acceptance |
| 3 | **Usable UTF-8 text APIs and recoverable collections**: scalar/byte inspection and safe `.get()`/`.pop()` contracts | **PLANNED; not implemented** | Approved error/index/ownership policies, strings/arrays/dictionaries normal/O2/GC regressions |
| 4 | **Optional-main/top-level application execution and bounded native-provider foundation**, with independently useful application-entry work able to be scheduled separately | **PLANNED; not implemented** | Entry coexistence, exit-code, legacy compatibility, JIT/AOT, native ownership and app-host acceptance |
| 5 | **Async completed-work retention**: measure first; change lifetime/retirement only if evidence demands it | **PLANNED; measurement only until justified** | Before/after retained work and resource evidence without weakening roots, cleanup or leases |
| 6 | **Selected language ergonomics** (`has`, if-expression, multiline calls, interpolation/formatting, etc.) | **DEFERRED pending need/individual design approval** | Implement only evidence-backed additions with focused tests |

This is a **work plan**, not authorization to implement every row at once.
Record future work as `PLANNED` / `IN PROGRESS` / `LOCALLY VERIFIED` /
`HOSTED LOCKED` with commit, CI run, suites, remaining limitations and date.
Never mark a feature LOCKED based solely on a summary or local success.

## Explicit owner-approved direction

### Module imports and language tooling

- **Unquoted imports mean official stdlib modules:** e.g. `use random`,
  `use io/files`. Nested slash-separated paths are supported by the proposed
  module model; example names are NOT a promised shipped module catalog.
- **Quoted imports mean project VFS source:** e.g.
  `use "systems/player.void"`. Bare quoted names are **importer-relative**
  (equivalent to `./`); leading `/` retains project-root-relative behavior.
  Existing `./` and `../` remain.
- Stdlib and project modules have **distinct identities and visibility
  boundaries**; project files cannot silently shadow stdlib or leak project-root
  constants into it. One stdlib root should be selected **deterministically**
  from the matching toolchain/installation, with a configuration override and
  clear failure when the selected source root is unavailable. No arbitrary
  workspace or current-directory search.
- Use exact `<segments>.void` lookup, no automatic `index.void`/`mod.void`
  fallback, folder-wide import, hidden reexport or new package-manager scope.
- `voidls` owns the **single compiler/VFS discovery query**. `use `, `use io/`,
  `use "`, `use "app/` should discover appropriate immediate modules, files or
  folders; selected file paths complete valid imports. Include correct edits,
  automatic/manual completion, hover/signatures/source documentation and
  navigation. No per-editor hardcoded module lists or duplicate resolver.
- Existing real clients retain their trust, selection, source-location and
  version/stale-edit rules. Standard-library source navigation must not grant
  write/rename authority to installed stdlib files.

### Project selection versus executable entry

- Prefer automatic exact-case `main.void` discovery within the supplied
  workspace boundary, **nearest candidate** from an active file. An explicit
  entry-file override always wins. No arbitrary function/file scanning;
  a missing main may use a clearly labelled editor standalone fallback.
- One shared compiler-owned project selection helper should be accessible
  through `voidls`; VS Code, VSCodium, Vim and Neovim share the rules rather
  than each independently traversing folders.
- **`main()` is optional**. Future applications may use executable top-level
  statements, `func main():`, or `func main() -> i32:`.
- **No-result successful completion exits 0.** An explicit
  `func main() -> i32:` requires an explicit integer return on every reachable
  path; **do not** implicitly insert return 0 for a typed `main()`. This
  supersedes Research Clanker's typed-main fallthrough recommendation in the
  pre-approval audit/roadmap.
- If a new selected application mixes executable top-level statements with
  an eligible `main()`, **diagnose competing entry styles**; do not silently
  choose one or execute both. Importing modules does not execute them.
- **Preserve the old `entry(i32) -> i32` examples/adapter**, including its
  input, printing and process-status contract. An existing declaration-only
  legacy `entry` and ordinary `main` helper continue to select legacy mode.
- A new optional-main application entry implementation is **NOT** part of
  Sprint 1: Sprint 1 discovers and selects the file/project; the application
  host, CLI dispatch and JIT/AOT entry behavior are separate later work.

### Scope and engineering boundaries

- Reuse the compiler-owned frontend/module graph, typed IR, LLVM 19.1.7
  JIT/AOT lowerer and the existing Runtime. KISS/DRY.
- Preserve actual VFS identity/path protections, UTF-16 edit ranges,
  readonly authority, collection copies, GC roots, safepoints, cleanup,
  native task/code lifetime and current entry/reload compatibility.
- No automatic restarts or invented auto-reload watcher; no new package
  manager, source stdlib implementation or public release decision now.
- Editor-specific advanced settings should be minimal; honor VS Code's
  existing editor settings and existing `void.server.path`,
  `void.project.entry` and `void.trace.server` overrides.
- `docs/README.md`, `docs/DEVELOPMENT_STRATEGY.md`, `docs/DECISIONS.md`,
  `docs/WRENCH_CLANKER.md`, this approval ledger, the engineering roadmap,
  contracts, issue dispositions and work log need **dated status updates** as
  implementation progresses, following the permanent documentation policy above.
  Keep research snapshots and earlier failures as history; add superseding
  evidence instead of rewriting the past.

## Remaining narrower design review

Sprint 2 numeric limits, signed-minimum abs, NaN/zero/infinity and clamp-bound
semantics, numeric prelude admission and argument/default policy were explicitly
approved by Boss on 20 October 2026; [the contract](STAGE8_CALLS_NUMERIC.md) records
the exact implementation and verification boundary. They are not pending owner
choices. No private/readonly field declaration work was required.

Sprints 3–6 retain their own design reviews, including recoverable text/collection
contracts, optional-main/application-host details, native provider admission and
measured async retirement. Their approval as direction does not implement them.

## Next handoff

Read [docs/README.md](README.md) first. Sprint 2 local verification is complete;
review [the evidence](stage8/SPRINT2_ACCEPTANCE.md) and stop for Wrench review. Do not request/run hosted CI;
the final seven-job gate is deferred to the end of Stage 8 unless Boss explicitly
changes that instruction. Update documentation under `docs/`; root README edits
require a separate explicit public-facing request. No commit, push, branch change,
remote modification, publication or Sprint 3 progression is authorized.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
