# VOID 2.0 — Boss-approved completeness direction and implementation ledger

**Decision recorded:** 20 October 2026, after Research Clanker's language
completeness audit and Stage 7 hosted acceptance.

**Current status — 21 October 2026:**

**Later CI #83/#84 repair:** See [cross-platform discovery/editor evidence](stage8/CI84_DISCOVERY_REPAIR.md).
CI #84 Windows compiler Debug passed 156/156; Release succeeded. A test-only
separator assertion and CI #83 macOS /var discovery failure have focused repairs,
verified on Linux Debug/Release (7/7 each) and extension units (4/4). Both macOS
configurations were inspected; native macOS Debug/Release and Windows editor
verification remain pending. No new language scope or hosted lock is approved.

**CI #83 repair update:** Owner-run baseline `fbad2b642114a692f93d2f4ce7d3465ea489abf2`
built on Windows Debug/Release, passing 153/156 each. See the
[three-failure Unicode repair](stage8/CI83_WINDOWS_REPAIR.md) and its focused
Linux Debug/Release evidence. Windows verification of the repair remains pending;
no hosted lock, new language semantics or subsequent stage is approved here.
The earlier Sprint 6 local snapshot follows.

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
| 3 | **Usable UTF-8 text APIs and recoverable collections**: scalar/byte inspection and safe `.get()`/`.pop()` contracts | **LOCALLY VERIFIED; 435/435 final unique suite outcomes; Wrench review pending; not hosted locked** | Approved error/index/ownership policies, strings/arrays/dictionaries normal/O2/GC regressions |
| 4 | **Optional-main/top-level application execution and bounded native-provider foundation**, with independently useful application-entry work able to be scheduled separately | **LOCALLY VERIFIED; 444/444; Wrench review pending; not hosted locked** | Entry coexistence, exit-code, legacy compatibility, JIT/AOT, native ownership and app-host acceptance |
| 5 | **Async completed-work retention**: measure first; change lifetime/retirement only if evidence demands it | **LOCALLY VERIFIED; justified frame-root retirement; 459/459; records/results retained** | Before/after retained work and resource evidence without weakening roots, cleanup or leases |
| 6 | **Selected language ergonomics** (`has`, if-expression, multiline calls, interpolation/formatting, etc.) | **LOCALLY VERIFIED; 468/468; Wrench review and final hosted gate pending** | Bounded has/if/calls/string-only interpolation; local acceptance and Wrench review required |

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

Sprint 3 text/collection policy and all three Sprint 4 checkpoint decisions are
approved in DECISIONS.md. Sprint 5 delivered measured internal frame retirement
without changing public task/result semantics. Broader record/handle ownership
retirement still requires review; Sprint 6 retains its own design approvals.
Approval as direction does not implement future work.

## Next handoff

Read [docs/README.md](README.md) first, then [Sprint 6 acceptance](stage8/SPRINT6_ACCEPTANCE.md),
[the contract](STAGE8_ERGONOMICS.md) and [final hosted checklist](stage8/FINAL_HOSTED_GATE.md).
Wrench reviews the complete six-sprint evidence and deferred scope. Boss controls
Git and explicit hosted authorization. No next sprint, Stage 9, stdlib work or
root README change is authorized. All four Sprint 6 decisions are approved; no
current-sprint owner semantic answer remains pending.

## Sprint 4 checkpoint approvals — 20 October 2026

Boss explicitly authorized Sprint 4 and answered every required question before
implementation. [DECISIONS.md](DECISIONS.md) records explicit `--app` CLI/status
policy, top-level return validation and the independently admitted static sink
provider. [Implementation contract](STAGE8_APPLICATION_NATIVE.md) separates those
approved rules from verification. [Sprint 4 local acceptance](stage8/SPRINT4_ACCEPTANCE.md)
now records 444/444 final outcomes and both 1 MiB gates. Wrench review remains
pending; no hosted lock is claimed. Sprints 5–6 remain future work.

## Sprint 5 authorization — 20 October 2026

Boss authorized measurement first and only a justified completed-work retention
improvement from `726b34b658f0d939b1da065acb4e9c735ddfa39d`. The
[Sprint 5 contract](STAGE8_ASYNC_RETENTION.md) records a material paired baseline
and the quiescent execution-frame retirement proof. Public task/result/ownership
semantics are unchanged; no additional owner decision was needed. [Local acceptance](stage8/SPRINT5_ACCEPTANCE.md) records 459/459 final outcomes
and both 1 MiB gates. Task/result/Work records and broader ownership remain
deferred. Sprint 6, Git mutations, hosted CI and root README changes are not
part of this authorization.


## Sprint 6 authorization and four checkpoint approvals — 21 October 2026

Boss explicitly authorized Sprint 6 from
`3d61279237c62157f2ecfff01aa24d6c937f9a0d` and answered every consolidated
question before implementation. [DECISIONS.md](DECISIONS.md) records type-operand
has with both generic branches checked, indented single-value if/mandatory else,
delimiter-aware multiline calls and string-only automatic interpolation.
[Implementation contract](STAGE8_ERGONOMICS.md) bounds the work. Numeric/custom
formatting, unrestricted continuation and conditional compilation remain deferred.
Git mutations, hosted CI, publication, root README and Stage 9 remain unauthorized.
This authorization supersedes earlier Sprint-6-future handoff wording; local
verification, Wrench review and hosted lock remain distinct.
