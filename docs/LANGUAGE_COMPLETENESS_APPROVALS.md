# VOID 2.0 — Boss-approved completeness direction and implementation ledger

**Decision recorded:** 20 October 2026, after Research Clanker's language
completeness audit and Stage 7 hosted acceptance.

**Status:** Stage 8 Sprint 1 IMPLEMENTED / LOCALLY VERIFIED (20 October 2026).
**423/423 consolidated suites passed; Wrench review and hosted acceptance pending.** Boss authorizes coding, Git
operations and hosted CI separately. Completed work must receive dated evidence
and a real hosted lock before changing a sprint's status to **LOCKED**.

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
| 1 | **Nested stdlib/project imports, compiler-owned VFS discovery, project `main.void` selection, import IntelliSense, source docs/navigation, shared VS Code/VSCodium/Neovim/Vim behavior** | **LOCALLY VERIFIED; 423/423 suites; Wrench review and owner-hosted acceptance pending** | Correct parsing/resolution/visibility, valid completions and source navigation, real client checks, focused and full local verification, owner-hosted 7-job CI |
| 2 | **Ordinary named/default arguments and built-in type APIs**: `i32.max_value`, `i32.abs(value)`, `value.abs()`, chaining with one canonical implementation | **PLANNED; not implemented** | Approved signatures and numeric edge behavior; ordinary-call/receiver/generic/JIT/AOT/tooling acceptance |
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

Boss approved the overall direction and the choices above. Numeric
overflow/NaN details, primitive provider declaration syntax, general API
signature details, imported top-level-code diagnostics and later capability/version changes remain
implementation-design details to review for their authorized sprints. Sprint 1
source-doc grammar, keyword path segments, root configuration, identity encoding
and preflight failure behavior are now concretely recorded in
[STAGE8_DISCOVERY.md](STAGE8_DISCOVERY.md) for Wrench review, without a hosted lock. Do not silently declare that
every table recommendation in the research files is an implemented language
contract.

## Next handoff

Future sprint prompts must begin with [the documentation entry point](README.md)
and direct documentation updates to `docs/`; root README edits require Boss's
explicit request for a public-facing change.

**Sprint 1 only** was explicitly authorized by Boss on 20 October 2026.
Implementation choices and verification boundaries are in [STAGE8_DISCOVERY.md](STAGE8_DISCOVERY.md);
[actual acceptance](stage8/ACCEPTANCE.md) records 8/8 focused, 423/423 consolidated,
15/15 final LSP checks, small-stack and real-editor evidence. Stop for Wrench review.
Do not start the other sprints automatically. At the end, produce concrete
source changes, focused regressions and one consolidated local
Debug/Release/strict-sanitizer run, preserve current cross-platform concerns,
stop for Wrench review, and leave Git/hosted CI to Boss.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
