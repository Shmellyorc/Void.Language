# VOID 2.0: Development Strategy and Compiler Architecture Roadmap

## Current implementation status

As of **20 October 2026**:

| Stage | Current status | Evidence / next gate |
| --- | --- | --- |
| Stage 6 — Concurrency and native libraries | **Hosted LOCKED** at `4139297f92c195782f59a28546444e6b3bec4ad8` | Owner-confirmed CI #78, seven successful jobs (973/973 suites) |
| Stage 7 — Developer tooling and optimization | **Hosted LOCKED** at `f704d75ffa37bfd0e587ec2b1513054e07a8a4d8` | Owner-supplied CI #81 attempt 2, seven successful jobs, 980/980 suites |
| Stage 8 — Language completeness, Sprint 1 only | **Implemented / locally verified: 423/423 suites; Wrench review and hosted acceptance pending** | [Contract](STAGE8_DISCOVERY.md), [approval ledger and remaining sprints](LANGUAGE_COMPLETENESS_APPROVALS.md), [acceptance](stage8/ACCEPTANCE.md) |

Stage 7 is **hosted locked**. For Stage 8 the existing **seven-job hosted CI gate remains
mandatory**: Linux Debug/Release/Sanitizers, Windows Debug/Release and macOS ARM64
Debug/Release. Boss controls commit, push and hosted execution; local results and
source review do not replace that gate. LLVM remains **19.1.7**.

See [Stage 7's technical contract](STAGE7_IMPLEMENTATION.md) and
[exact local evidence and compatibility boundaries](stage7/ACCEPTANCE.md).
The implemented subset includes compiler-owned language assistance, guarded
rename/formatting, project refresh, bounded message batching, editor adapters and
local native packaging. VSCodium, new Windows/macOS editor combinations, research
minimum editor versions and clean-machine portability remain unverified.

## Reading the historical records

The dated research/planning/delivery records below preserve the decisions and
results as they stood at the time. **Stage 6 pending-delivery/pending-lock statements
and Stage 7 partial, research-only or unauthorized statements are historical.**
They do not override the current status above. The original Stage 0–7 roadmap is
design intent; implemented technical contracts determine available behavior.

**Project/repository:** `Void.Language`  
**Language:** VOID 2.0 (`.void`)  
**Original roadmap date:** 10 October 2026

**Companion design:** [Language Bible](LANGUAGE_BIBLE.md)

## Historical Stage 7 research handoff — 20 October 2026

Boss confirms Stage 6 locked at `4139297f92c195782f59a28546444e6b3bec4ad8`,
CI #78, seven jobs and 973/973 tests. Local HEAD matches that baseline; the hosted
result is owner-confirmed, not rerun by research. This supersedes the historical
pending-lock status below. Boss authorized Stage 7 **research only**.

The [Stage 7 roadmap](STAGE7_ROADMAP.md) records the completed source/official-doc
investigation, local measurements, real editor baseline probes, proposed feature
contracts, implementation order, testing and decisions for Wrench/Boss. No Stage 7
production implementation is authorized or claimed. Stop for Wrench review and
Boss authorization; earlier workflow and technical history remain preserved.

<a id="owner-workflow-and-stage-7-planning--19-october-2026"></a>

## Historical owner workflow and Stage 7 planning — 19 October 2026

**Planning only; Stage 7 implementation is not authorized.** Stage 6 continues
unchanged through its existing five deliveries and owner-controlled hosted
acceptance. This update changes no Stage 6 contract, implementation status,
remaining delivery authorization or acceptance gate. Previous engineering history
and the current implementation records below remain intact.

### Development workflow recorded on 19 October

#### Stage 7 trial: research, engineering and acceptance

Boss approved this **trial workflow beginning with Stage 7, after Stage 6 receives
its final hosted lock**. It is not an inflexible permanent process. Recording the
workflow does not begin Stage 7 research or implementation or change Stage 6's
contracts, status or remaining acceptance. Stage 7 retains one cohesive
implementation sprint by default; these phases are not additional deliveries.

**Phase 1 — GPT-6.1 High: Research and Roadmap.** Conduct bounded,
architecture-focused research using the existing source and relevant official
documentation. Identify dependencies, complexity, reusable infrastructure,
platform risks, testing needs and unresolved decisions. Record an actionable
roadmap in permanent documentation. Scale research to the complexity; routine
features do not require exhaustive investigation. Small technical experiments
are allowed when needed to answer a concrete question.

**Phase 2 — GPT-6.1 Medium: Engineering.** Following owner authorization,
implement the complete stage in dependency order. Preserve engineering freedom,
KISS/DRY and the existing architecture. Run focused tests, correct inaccurate
roadmap assumptions and investigate new problems as they arise. A focused return
to High is permitted for genuinely difficult unresolved questions, without
automatically creating another delivery.

**Phase 3 — Acceptance.** Complete consolidated local regression and sanitizer
testing, receive Wrench Clanker review, then stop for owner commit/push and the
seven-job hosted CI. Normally run hosted CI once at stage completion.
Intermediate hosted checks are justified only by consequential platform-specific
risks that cannot reasonably be resolved locally. Boss retains Git/CI control;
this trial adds no automatic intermediate hosted checkpoints.

The following engineering principles continue to support this trial and future
work; the trial's model/effort split does not restrict useful Work usage.

Deliver substantial engineering sprints with a clear finished outcome. Simple and
moderately complex features normally fit one integrated sprint: use focused tests
while developing, repair concrete failures, then complete consolidated acceptance.
For highly complex features, investigate the actual source and relevant official
documentation first, assess dependencies and risks, and split into a few meaningful
deliveries only when the engineering justifies it. Research is supported and
encouraged when it answers an architectural, compatibility or correctness question;
it is neither prohibited nor a compulsory gate for routine work.

Avoid arbitrary micro-milestones, repetitive full test matrices, speculative
verification frameworks and unnecessary stop-and-ask checkpoints. Repeat or broaden
verification when changes, failures or unresolved concerns justify it. Existing
Stage 6 delivery gates remain binding; this workflow does not collapse them.

Use **ChatGPT Work with GPT-6.1 in Codex mode aggressively when valuable** for
substantial implementation, integration and debugging. Work is not restricted to a
last resort, sandbox failure or one consolidated invocation. Do not impose an
artificial usage restriction. This is an owner workflow preference, not permission
to start an unapproved stage or change Git/CI ownership.

Preserve KISS/DRY, LLVM **19.1.7**, the existing frontend/typed IR/LLVM backend and
Runtime, and cross-platform correctness. Wrench reviews architecture and challenges
unsafe or unnecessarily complex decisions. Boss retains final product decisions,
feature authorization, commits, pushes, branches and hosted CI. These current
preferences supersede older small-milestone and restricted-Work instructions;
those dated practices remain historical records.

### Stage 7 goals: Developer Tooling and Optimization

Plan Stage 7 as **one cohesive implementation sprint by default**, with a practical,
developer-ready editing experience as its outcome. Recommend splitting only if
investigation identifies real architectural dependencies, high-risk contracts or
acceptance problems that make a single sprint impractical. The areas below are
parts of that outcome, not separate mandatory deliveries or approval checkpoints.
They are future goals, not implemented capabilities or a requirement to implement
every possible LSP extension.

| Area | Planning goals |
| --- | --- |
| VS Code / VSCodium | Full-featured extension experience: reliable installation, configuration, native server discovery/startup; IntelliSense, completion, hover, signature help, navigation, rename, formatting and diagnostics; suitable semantic highlighting, code actions and workspace features where supported; project handling, synchronization, refresh, lifecycle, cross-platform tests and packaging. |
| Neovim | Native LSP integration with shared `voidls`; filetype recognition, syntax highlighting and indentation; completion, navigation, hover, rename and diagnostics; reproducible configuration examples, documentation and real integration tests. |
| Vim | An appropriate maintained Vim LSP client using the same `voidls`; filetype, syntax and indentation; compatible diagnostics, completion and navigation; documented supported Vim versions/plugins and realistic integration testing. |
| Shared LSP | Extend existing compiler-owned analysis and LSP 3.17. Prioritize completion, hover, signature help, rename, formatting, richer symbol coverage, project handling and responsiveness. Preserve immutable snapshots, accurate diagnostics, UTF-16 positions, source identities and stale-result rejection. |
| Optimization and usability | Measure useful compiler, server and editor workloads; improve demonstrated bottlenecks, developer setup, documentation and packaging. Avoid speculative optimization projects. |

All editors share **one authoritative `voidls`**. Editor adapters, syntax rules and
configuration must not become editor-specific parsers or type checkers. Preserve
one compiler and its analysis ownership. Better project discovery and editor disk
refresh do not imply a new project/package format or automatic native hot reload.
No native package manager, arbitrary public FFI or new language syntax is silently
included in Stage 7.

### Investigation and review decisions still open

Choose a maintained Vim client and supported Vim/Neovim version/plugin combinations
from official documentation and actual compatibility tests; none is selected or
certified by this plan. Investigate native server distribution/discovery, platform
packaging and workspace/entry selection against the existing explicit-root policy.
Define rename scope/conflict handling, formatting policy, richer symbol coverage
and appropriate optional semantic-token/code-action/workspace support before
claiming those contracts. Do not invent language semantics to fill tooling gaps.

Use representative projects to measure startup, edit-to-diagnostic and query
latency, refresh behavior and memory where useful; select workloads and targets
from evidence. Validate actual editor/client/server attachment, unsaved edits,
navigation, repairs, stale results, shutdown/restart and packaging on the declared
supported platforms and versions. Distinguish tested combinations from assumptions.
These are acceptance planning questions for the future authorized sprint, not new
Stage 6 blockers or an automatic set of separate research deliveries.

See [LANGUAGE_SERVER.md](LANGUAGE_SERVER.md#stage-7-planning-goals-not-implemented)
and [EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md#stage-7-editor-goals-planning-only)
for the current baseline versus future server/editor goals.

## Historical Delivery 5 local acceptance — 19 October 2026

Delivery 4 is owner-confirmed hosted-accepted at
`619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
7/7 jobs, 131/131 each (917/917). Delivery 5 alone is now authorized: integrated
async/groups/workers/native/GC/reload/cache/tooling acceptance, repairing only
demonstrated defects. Preserve all existing boundaries and 131 suites. Focused
checks precede one consolidated final Debug/Release/strict, 1 MiB and editor
acceptance. Stop after local acceptance for owner review and hosted CI; Stage 6
locks only after seven green hosted jobs. Stage 7 remains planning-only and
unauthorized; its documented goals and research/Work preferences are unchanged.
The earlier Delivery 4 pending-hosted status below is historical.

**Delivery 5 is locally complete:** 139/139 suites each in Debug, Release and
strict ASan/UBSan/leak configurations (417/417); corrected cancellation-fixture
checks passed in all three. All 42 native 1 MiB AOT runs, three semantic-depth
checks, editor unit tests, three real editor profiles and VSIX packaging passed.
No compiler/Runtime production repair was required. Evidence and the initial
fixture-race correction are recorded in [WORK_LOG.md](WORK_LOG.md).
**STOPPED for owner review, commit/push and seven-job hosted verification.**
The final Stage 6 lock is pending; Stage 7 implementation is not authorized.

## Historical Stage 6 delivery status — 19 October 2026

| Implementation delivery | Status and dependency recorded on 19 October |
| --- | --- |
| 1 — Native Async Activation | Hosted accepted, CI #73 |
| 2 — Structured Task Lifecycle | Hosted accepted, CI #74 |
| 3 — Bounded Parallel Spawn | Hosted accepted at `62b17c8c`, CI #75 |
| 4 — Native Libraries | Hosted accepted at `619331cc`, CI #76; current locked baseline |
| 5 — Integrated Stage 6 Acceptance | Locally accepted: 139/139 Debug, Release and strict; owner review/seven-job hosted acceptance pending |

These are the five implementation deliveries, not the six D1–D6 decision groups.
D1 parallel execution now approves only the documented initial value-only scope.
D2 enqueue/await/inferred handles, D3 root/group ownership and D4 lifecycle rules
retain their approved contracts. D5 cross-task callable escapes stay restricted;
D6 now approves the bounded trusted static fixture described in [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md); general FFI remains deferred. No general sharing, task-type spelling,
async-callable or cancellation recovery contract has been silently added.


## Historical Delivery 4 authorization — superseded by Delivery 5 above

Delivery 3 is owner-confirmed hosted-accepted at `62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090`,
[CI #75 checks](https://github.com/Shmellyorc/Void.Language/commit/62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090/checks),
7/7 jobs, 120/120 each (840/840). Delivery 4 alone is authorized: one explicitly
trusted statically compiled C fixture, typed project-local binding descriptors,
ordinary VOID wrappers, owned UTF-8 transfers, opaque managed resource identities
and quiescent rooted callbacks. No public native grammar, dynamic loading, package
management or worker/native effects are approved. The bounded implementation is complete.
**Local Delivery 4 acceptance passed: 131/131 each in Debug, Release and strict ASan/UBSan/leak configurations (393/393).** The 1 MiB semantic/native checks, explicit native host and editor/package checks passed. STOPPED for owner review and seven-job hosted verification; Delivery 5 remains unauthorized. See [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md).
Delivery 5 requires Delivery 4 hosted acceptance and separate owner authorization.
Boss controls Git/hosted CI; Wrench reviews; Keyboard implements this delivery and
stops after complete local acceptance. Earlier pending-hosted statuses are historical.

## Historical Delivery 3 authorization — superseded by Delivery 4 status

**Locked baseline:** Delivery 2 at `5d228804be809465ce2c97b7e0b58b89caa718ac`,
[CI #74 checks](https://github.com/Shmellyorc/Void.Language/commit/5d228804be809465ce2c97b7e0b58b89caa718ac/checks),
owner-confirmed 7/7 jobs, 113/113 each (791/791). Delivery 3 alone is now authorized:
bounded native workers for directly named synchronous noncapturing source functions;
scalar/string and recursively safe fixed value transfers only. No mutable managed
references, task/callable/native-resource transfers, worker task creation or joins.
Preserve structured cancellation/drain, precise GC, generation leases and shared
JIT/AOT lowering. Local acceptance passed **120/120 each** in Debug, Release and strict
ASan/UBSan/leak configurations (**360/360**), plus editor/package and 1 MiB checks.
**STOPPED for owner review and seven-job hosted verification.** Deliveries 4–5
remain unauthorized. Stop after local acceptance for owner review and hosted CI.
D1 parallel sharing is approved only within this initial value-only boundary;
the six design-decision groups remain distinct from the five deliveries.

Earlier Delivery 2 pending-hosted handoffs below are historical, superseded here.
Boss controls Git/CI, Wrench reviews architecture, Keyboard implements authorized work.

## Historical Delivery 2 handoff (superseded by Delivery 3 status above)

**Current locked baseline:** Delivery 1 at `34efb761965a5e19841e2fcf2816a5b851e902a7`,
[CI #73](https://github.com/Shmellyorc/Void.Language/commit/34efb761965a5e19841e2fcf2816a5b851e902a7/checks),
7/7 jobs, 98/98 suites per configuration (686/686), as confirmed by the owner.
Stage 5 remains historically locked at `4627687b3a18349633715e4294bc915cf74ddd41`,
[CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484).

**19 October owner authorization:** implement Delivery 2 only. Cooperative
idempotent `job.cancel()`, distinct success/cancellation/runtime failure (`Err`
is an ordinary result), activation ownership and lexical
`with tasks.group() as jobs:` scopes. Normal exits join; early exits cancel and
join. Cancellation/failure cancels and joins descendants before parent cleanup;
draining is shielded. Preserve parent runtime failure; otherwise select unobserved
child failures in creation order within the bounded diagnostic policy. Child
runtime failure cancels siblings and wakes the waiting parent. Keep one owning
executor, explicit root adapter and synchronous CLI ABI. No implicit detach,
workers, parallel spawn, cancellation recovery API or general cleanup await.
Independent IR and source negatives, JIT/AOT, GC/allocation/reload and complete
Debug/Release/strict sanitizer acceptance are required before owner review.

| Sequential implementation delivery | Scope and dependency | Status |
| --- | --- | --- |
| Delivery 1 — Native Async Activation | Async functions, await, traced frames, owning executor, leased resumptions | Hosted accepted at CI #73 |
| Delivery 2 — Structured Task Lifecycle | Cancellation, groups, failures, structured cleanup/shutdown | Locally accepted: 113/113 Debug, Release and strict sanitizers; owner review/hosted CI pending |
| Delivery 3 — Bounded Parallel Spawn | Workers, reviewed value transfer, parallel source jobs, GC coordination; requires Delivery 2 lock | Not authorized |
| Delivery 4 — Native Libraries | Trusted bridges, typed scalar/string/opaque handles, ownership/callbacks; requires Delivery 3 lock | Not authorized |
| Delivery 5 — Integrated Stage 6 Acceptance | Combined async/parallel/native, reload/cache/lifetime/cross-platform; requires Delivery 4 lock | Not authorized |

These five **implementation deliveries are distinct from D1–D6 design decisions**.
D1: owning-executor async is approved; worker sharing/parallel restrictions remain
future review. D2: enqueue semantics, inferred task handles and await are approved;
public task-type/async-callable syntax is not approved. D3: explicit root adapter
and unchanged sync CLI approved; lexical groups approved for Delivery 2; detach and async CLI rules deferred.
D4: preserve reached cleanup and checked failures; the assigned Delivery 2 cancellation/group policies are approved. D5: per-activation leases approved; new cross-task
callable escapes restricted instead of approving session-wide generation pinning.
D6: native-library scope/grammar/ownership remain Delivery 4 review.

**Workflow for every delivery:** implement selected delivery → focused tests →
complete local acceptance → owner review/commit/push → existing seven-job hosted
CI → lock only when green. STOP after local Delivery 2 review; do not automatically
begin another delivery. No assistant commits, pushes, branch creation, remote
writes or hosted dispatch. Boss owns product decisions and Git/CI; Wrench reviews
architecture/evidence; Keyboard implements only authorized scope.

**Acceptance and recovery:** existing JIT/AOT, GC roots, failure cleanup, reload
compatibility, old-code leases and Debug cache remain mandatory. Record exact local
results, incomplete code, failures and next action in WORK_LOG.md and this handoff.
Main risks are unrooted suspended values, malformed resume states, cleanup loss,
stale generation pointers and callable escapes. Language-facing choices not covered
by authorization return to owner review; do not invent contracts. Owner-approved root policy: drain every started task, including unawaited tasks,
before the adapter returns. Preserve root failure as primary; if root succeeds,
report otherwise unobserved task failures deterministically in creation order.
Retain roots/leases and run all cleanup. Delivery 2 adds the approved cooperative cancellation and lexical groups; detach remains unavailable.
Implementation and exact boundaries are in [ASYNC.md](ASYNC.md). The adapter
drains within its existing NativeCode lease, covering every retained task/frame
and resumption; a generation token rejects cross-generation publication. No task
escapes the drained invocation. Private frame offsets are copied into Runtime
heap layouts, without code pointers or public metadata admission exceptions.
**Delivery 1 historical local acceptance:** Debug, Release and strict sanitizers
passed 98/98 each; subsequent owner-confirmed CI #73 locks Delivery 1 above.
**Delivery 2 current work:** native cancellation outcomes, activation ownership,
lexical groups, shielded nonblocking joins, cleanup ordering and independent IR
proofs are implemented. Final local acceptance passed **113/113** in Debug, Release and strict
ASan/UBSan/leak configurations (**339/339**), plus 1 MiB frontend/native-stack
checks and editor test/package checks. No incomplete Delivery 2 implementation
or failing local test remains. Exact evidence is in WORK_LOG.md; logs are under
`build/stage6-delivery2`. **STOPPED for owner review and hosted acceptance.**
Owner review, commit/push and the seven-job hosted gate must precede a Delivery 2
lock or any authorization to begin Delivery 3. No later delivery is active.


**18 October 2026.** This is the canonical current-status summary. The roadmap and
older proposal/proof notes below remain historical design evidence where superseded.
Feature guides define implemented contracts; LANGUAGE_BIBLE.md includes both those
contracts and approved future design, and is not a claim that every illustrated
syntax executes today.

**Accepted baseline:** local HEAD verified at `4627687b3a18349633715e4294bc915cf74ddd41`.
**Stage 5 is hosted-verified and LOCKED** at that commit, [CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484):
seven successful configurations, 82/82 suites each (574/574 total), independently
verified from all seven job logs. The lock covers the delivered cache/reload subset;
Windows persistence still bypasses safely and automatic watching remains future work.
**Stage 4 is hosted-verified and LOCKED** at `483e67e407b65674bdc82d44767f29bd83d73ae8`, [CI #68](https://github.com/Shmellyorc/Void.Language/actions/runs/39174408997): seven successful configurations, 80/80 suites each (560/560 total).
The owner supplied the suite totals; read-only GitHub run metadata independently
confirms this commit and all seven successful jobs. Stage 3 remains locked and
all delivered Orders 1–5 and Phases 1–25 are preserved. Stage 5
has hosted-accepted optional persistent Debug native-object caching and
revision-checked hot-reload integration. See [STAGE5_CACHE_PROPOSAL.md](STAGE5_CACHE_PROPOSAL.md)
for the implemented contract and retained local evidence. Automatic watching is not
implemented. [STAGE6_PROPOSAL.md](STAGE6_PROPOSAL.md) preserves the architecture research and current delivery authorization;
overall architecture direction is approved, Delivery 1 is hosted-accepted, and Delivery 2 is authorized under the permanent handoff above. Deliveries 3–5 and unresolved language contracts still require owner approval.

| Implemented phases | Delivered source/toolchain behavior |
| --- | --- |
| 1–4 | Native compiler foundation, real source functions/control flow, modules, value semantics and JIT/AOT execution |
| 5–7 | Structured/recovering diagnostics, exact-width numeric types/conversions, validated analysis and compatibility |
| 8–11 | Owned tooling snapshots/revisions, LSP, VS Code/VSCodium extension and VSIX workflow |
| 12–14 | Value structs, receiver methods and custom value initialization |
| 15–18 | Coordinated precise GC, managed structs/cycles, staged initialization, managed references inside value structs |
| 19–21 | Composition, managed inheritance/virtual dispatch, structural interfaces |
| 22 | Immutable UTF-8 strings, exact content equality/concatenation and owned templates |
| 23 | Real fixed/growable arrays, nested/composite elements, forward/reverse iteration and integer ranges |
| 24 | Concrete native generic functions/types/methods, exact interface constraints and specialization-time body checking |
| 25 | List insertion/clear and independent inclusive/exclusive slicing; approved pop deferral |

Stage 3 implements **native ordered dictionaries**; DICTIONARIES.md
records the owner-approved key, lookup, mutation, readonly and iteration contract.
It reuses the existing Heap, typed buffers, Frame/Slot roots and shared LLVM lowerer.
Stage 3's acceptance is managed-memory correctness under optimization, not a new
GC certification framework or a promise of concurrent collection/production tuning.

The original **Stage 0–7** roadmap (Section 4) remains authoritative as a roadmap;
its numbers are distinct from implementation phases and Phase 23 research substages:

| Roadmap stage | Current state and remaining boundary |
| --- | --- |
| 0 Architecture proofs | Implemented and regression-tested LLVM/GC/leased reload/native-call foundations |
| 1 Frontend | Implemented for the delivered language subset; future Bible syntax remains future work |
| 2 Core native language | Implemented functions/types/control flow/value and managed structs with native execution |
| 3 Managed runtime | Allocator/precise coordinated GC, reference/value structs, strings, lists and dictionaries; LOCKED at cd95e373 / CI #65, seven jobs successful |
| 4 Advanced language | Hosted-verified and LOCKED at 483e67e4 / CI #68: properties/callable structs, tagged enums, result/propagation, shared is/match patterns, defer/with/one-shot managed deinit, function values/escaping closures, callbacks and synchronous events; 80/80 suites in all seven configurations. See ADVANCED_LANGUAGE.md and WORK_LOG.md |
| 5 Hot reload | Delivered cache/reload subset hosted-verified and LOCKED at 4627687b / CI #70, 82/82 in all seven configurations. Whole-graph Debug objects, current-source validation, isolated generations and revision-checked replacement; Windows persistence bypass and automatic-watching deferral remain |
| 6 Concurrency/native libraries | Deliveries 1–4 hosted accepted through CI #76 at `619331cc` (131/131 in seven jobs). Delivery 5 integrated acceptance locally passed 139/139 each in Debug, Release and strict; final owner review/seven-job hosted acceptance pending. See ASYNC.md and NATIVE_LIBRARIES.md. |
| 7 Tooling/optimization | Partial: LSP diagnostics/navigation, editor integration/VSIX and LLVM O2; completion/rename/formatting, library discovery and broad performance work remain deferred |

Ordinary `.void` programs execute all delivered features together through normal/O2
ORC JIT or independently linked AOT. Examples live under examples/managed_structs,
strings, arrays, generics, collections and dictionaries. Fixed arrays copy values;
lists/dictionaries and managed references share identity. Precise roots, safepoints,
transactional publication and failure cleanup apply across these features. Large
payloads use the existing GC Heap; no arbitrary 64 KiB production or native-stack
payload limit applies. Public managed aggregate FFI is not promised.

**Architecture:** C++20 frontend/semantic checker → typed validated IR → one LLVM
**19.1.7** lowerer shared by JIT/AOT; one coordinated Runtime/Heap with precise
Frame/Slot roots and runtime-owned data-only metadata. LSP owns immutable analysis
snapshots; the extension launches voidls. GitHub Actions retains seven Linux,
Windows and macOS Debug/Release/strict sanitizer configurations, unfiltered
production CTest, editor integration and VSIX artifacts; timeout is **180 minutes**.

**Historical supersession:** Phase 23 runtime stages 2A–2F/3A–3B and later envelope/
final-byte experiments are dated research, not current source-feature blockers.
Orders 1–5 made certification optional, simplified private calls, delivered real
fixed arrays, removed measured root/lookup overhead and corrected linkage. Source
arrays and Phases 24–25 supersede their old nonactivation claims. No further
certification gate is required for ordinary compiler acceptance.

**Meaningful limits:** see KNOWN_ISSUES.md and the individual guides. Dictionary
float/interface/collection/custom-equality keys and result-based get remain deferred;
so do pop, missing slice endpoints/views, string indexing/interpolation,
source concurrency and future Stage 5–7 work. The known deep valid receiver
chain lowering stack issue remains open unless a later reproduced repair is recorded.
Do not interpret Stage 3 completion as completion of all language or tooling design.

> **Purpose:** This document preserves our agreed development strategy so a future conversation, contributor, or milestone team can continue without rebuilding architectural context. The Language Bible defines what VOID 2.0 should do. This roadmap explains how to develop it, validate its foundations, and decide when a milestone is truly complete.
>
> **Important naming decision:** The project is called **`Void.Language`**, not `Void.Language2`. The earlier project has been backed up. This document does not assume any existing files have been modified.

> **Historical 15 October 2026 Phase 23 activation discussion (superseded by Orders 1–5 and Phases 23–25):** CI #45 is
> owner-confirmed green (7/7 jobs, 61 suites each), locking Part 5A. The earlier
> 65,536-byte production array-function policy is superseded. See the large-array
> review gate below and ARRAYS_PROPOSAL.md A10/Section 21; historical roadmap entries
> do not authorize reactivation or an alternative storage architecture.

---

## 1. Overall direction

VOID 2.0 is a new native programming language with a deliberately small, readable surface and a capable compiler/runtime underneath it. Unlike VOIDC 1.0, VOID 2.0 is **not obligated to reproduce C# language semantics**.

Priorities:

1. **Easy and enjoyable to use.** Simple syntax, useful defaults, helpful errors, strong IntelliSense and documentation.
2. **Fast native execution.** LLVM ahead-of-time (AOT) compilation for release builds.
3. **Automatic memory management.** Precise GC without exposing ordinary pointer management or forcing manual allocation/freeing.
4. **Built-in hot reload.** Compatible function-body changes apply while the application is running. No automatic restarts or nagging dialogs.
5. **Excellent developer tooling.** Compiler-quality source diagnostics, source-level runtime stack traces, documentation, completion, navigation and library discovery.
6. **Cross-platform consistency.** Windows, Linux and macOS use the same language rules. Target-specific implementation work stays inside the compiler, runtime and packages.
7. **Simple native interoperability.** Existing C libraries are accessible through typed bindings and normal VOID wrappers, without requiring application developers to learn C pointers or C string conversions.

**Core principle: The compiler should work hard so the developer does not have to.**

### Five development rules

1. **KISS and DRY.** One implementation of each language rule. Avoid duplicate compilers, unnecessary abstraction layers, redundant paths and competing subsystems.
2. **Prove difficult mechanisms early.** Validate LLVM code generation, precise GC references, native interop and hot reload in small isolated experiments before creating hundreds of dependent features.
3. **One milestone, one responsibility.** Complete, test and audit each milestone before advancing. Use focused deliverables and clear acceptance conditions.
4. **Cross-platform from the beginning.** Treat Linux and Windows as first-class targets, not a port deferred until late in the project. Add macOS as soon as a suitable test environment is available.
5. **The Language Bible is the source of truth.** Every feature must conform to its documented semantics. Deliberate design changes update the Bible and a decision record.

### Non-negotiable performance principle

**Never sacrifice GC, safety, language semantics, or ordinary developer simplicity solely to improve a benchmark. Optimize the implementation instead.**

This specifically means no requirement for unsafe application code, manual memory management, explicit pointer manipulation or platform-specific source just to obtain good baseline performance. Optimization claims must be validated on representative workloads.

---

## 2. One compiler, two execution modes

```text
                     VOID 2.0 source (.void)
                               |
                               v
                      Compiler frontend
                  lexer / parser / resolver
                 semantic analysis / types
                    source diagnostics
                               |
                               v
                       Typed VOID IR
                               |
                    Shared LLVM lowering
                               |
                   +-----------+-----------+
                   |                       |
                   v                       v
               LLVM AOT                LLVM ORC JIT
             Release mode             Development mode
                   |                       |
                   v                       v
        Optimized native executable   Running native application
                                     + compatible hot reload
                   |                       |
                   +-----------+-----------+
                               |
                               v
                      Shared VOID runtime
            GC / allocator / strings / threads
               tasks / bindings / diagnostics
```

**One parser, one type checker, one set of semantics, one typed IR.** The AOT and JIT execution paths differ in optimization and code-lifetime policy, not in what the language means.

- **Release:** AOT native executable. No JIT required at runtime. Compiler may inline and specialize aggressively when safe.
- **Development:** Native code generation via LLVM ORC. Functions eligible for replacement have stable dispatch identities. Compatible edits can be compiled and applied safely while the application stays alive.
- **Both:** The same GC model, string rules, type behavior, native bindings, error semantics and runtime contracts.

LLVM provides native code generation, optimization and ORC JIT infrastructure. It **does not** implement our complete GC, source language, hot-reload safety or threading policy for us.

### Architecture independence

**VOID 2.0's language specification must not depend on LLVM-specific concepts.** LLVM is the intended implementation backend, not part of the observable language contract. The IR should make it possible to replace or supplement LLVM later without changing VOID syntax or semantics.

---

## 3. Architectural boundaries

| Component | Responsible for | Should not be responsible for |
|---|---|---|
| **Frontend** | Lexer, indentation, parser, VFS/module resolution, symbol resolution, type checking, language diagnostics | Native machine-code instruction selection |
| **Typed VOID IR** | Well-defined typed operations, control flow, object/reference information, lowered language semantics, source locations | Platform-specific OS implementation |
| **LLVM backend** | Optimizations, native machine code, object files, calling convention lowering, debug metadata | Deciding what `match`, `defer`, `Ok`, structs or properties mean |
| **Runtime** | Memory allocator, GC, safepoint coordination, strings, tasks, threads, panic/runtime handling, native-boundary services | Parsing `.void` source files |
| **Standard library** | Collections, files, synchronization APIs, documented helpers, platform abstractions | Compiler parsing/type-checking rules |
| **Language server** | Completion, hover, signatures, definition/reference lookup, rename, diagnostics | An independent, competing parser and type checker |
| **Development host** | Watching files, incremental compilation, JIT loading, versioned function dispatch, compatible reload coordination | Different semantics from release builds |

### Sharing and reuse rules

- The language server **reuses the compiler frontend**, including symbols, types, documentation comments and source spans.
- Both AOT and JIT paths **reuse the same LLVM lowering** as far as practicable.
- The runtime has explicit, small contracts with generated code. Avoid forcing high-level language implementation details into every subsystem.
- Common standard-library behavior should live in readable `.void` source wherever practical. Reserve compiler/runtime primitives for operations that actually need low-level support.
- Do not copy VOIDC 1.0's implementation wholesale. Reuse proven ideas, diagnostics conventions and tests without importing its architectural baggage.

---

## 4. Eight development stages

Each stage is divided into small milestones. Stage completion requires integration validation, not merely feature demonstrations.

| Stage | Focus | Required outcome |
|---|---|---|
| **0. Architecture proofs** | LLVM, GC roots, JIT replacement, native calls | Evidence that the foundation is technically viable |
| **1. Compiler frontend** | Lexer, indentation, parser, VFS, diagnostics | Compiler understands VOID source faithfully |
| **2. Core native language** | Types, `var`, `const`, functions, branching, loops, structs | Useful programs compile and execute natively |
| **3. Managed runtime** | Allocator, GC, reference and value structs, strings, lists, dictionaries | Automatic managed memory is correct under optimization |
| **4. Advanced language** | Generics, interfaces, properties, `match`, `result`, `defer`, closures, callbacks, events | Language systems cooperate with predictable semantics |
| **5. Hot reload integration** | Incremental compilation, dispatch, compatibility checks, old-code retirement | Running programs update compatible behavior without restart |
| **6. Concurrency and native libraries** | Async, scheduler, thread pool, synchronization, C bindings | Managed concurrency and native interop work safely together |
| **7. Tooling and optimization** | LSP, extension, library discovery, benchmarks, packaging | Developer-ready toolchain and validated performance |

**Important:** These stages express emphasis, not a rule to defer architecture-sensitive concerns. GC metadata and reload identities must influence early IR design. Source spans, documentation metadata and LSP-ready symbol information must begin in Stage 1. Windows/Linux checks must begin in Stage 0/2, not Stage 7. Native bindings also need an early Stage 0 proof even if their full library ecosystem comes later.

### Stage 5: persistent Debug cache and shared hot-reload integration

The owner approved the architecture and authorized the first production implementation. [STAGE5_CACHE_PROPOSAL.md](STAGE5_CACHE_PROPOSAL.md) records the implementation, measurements and boundaries. The original investigation requirements below remain design rationale where not superseded by that implementation record.

**Status (18 October 2026): delivered subset hosted-verified and LOCKED at 4627687b / CI #70, seven configurations, 82/82 each.** Stage 7 may eventually measure and tune its performance. This is a compiler/development-host capability, not new VOID source syntax or a new bytecode runtime. It is separate from the currently implemented, process-local ORC generations, validated source analysis and compatible-code replacement.

**Intended Debug workflow:**

- **First launch:** Validate and compile the project normally, then persist reusable relocatable native artifacts under `.void/cache/` when safe and useful.
- **Later launches and machine restarts:** Validate that cached artifacts still match the current sources, toolchain, target and runtime; reuse valid compiled code without repeating its native compilation. A new process still creates its own Runtime, roots, JIT session and code-generation leases.
- **Source edits:** Identify affected code through the existing dependency/compatibility machinery. Revalidate relevant source and recompile affected units where technically sound; avoid rebuilding unaffected units when measured granularity and dependency correctness allow it.
- **While running:** Combine newly validated compiled artifacts with the existing compatible hot-reload publication and old-generation retirement rules. Cache updates do **not** by themselves authorize live replacement; incompatible changes still require the existing restart policy.
- **Release:** Preserve the current optimized LLVM AOT pipeline and its behavior. The new disk-cache policy is Debug-only.

**Reuse the current architecture, not a competing compiler:** The compiler already owns project/VFS identities, parsed and validated analysis, exact source/declaration dependency and reload-compatibility information, typed IR, one LLVM 19.1.7 lowering path, ORC JIT object generation and resource-tracked native generations. A future caching implementation should consume these existing authoritative facts. A cache validity decision may need stronger or different inputs than hot-reload compatibility: an edit may be safe for live replacement yet still require recompilation, and an artifact reusable after restart is not automatically eligible for replacement in an active generation. Never create separate dependency graphs, semantic models, backends, or Runtime/GC systems just for the cache.

**Research candidates, not selected designs:** Begin with LLVM 19.1.7's existing relocatable object emission and ORC object-file linking. Compare persisted native object files with LLVM bitcode if measurement and target semantics warrant it; neither requires a VOID-specific bytecode format or interpreter. Confirm the pinned SDK's object-cache/IR-compile-layer hooks, `LLJIT` object loading, target portability and ownership behavior before selecting an interface. ORC lazy reexports / compile-on-demand may defer work until first call, but are optional research, **not** an approved implementation mechanism. Prior-execution function profiles or speculative warming are likewise optional only after simpler caching works and shows a measured need. Prefer the simplest practical cache unit first (for example, a compilation unit); use function-level artifacts only when the benefit clearly outweighs symbol, generic-specialization, invalidation and linking complexity.

**Correctness and invalidation requirements:**

- A cache hit must reflect validated source and every compilation-relevant transitive dependency: imported declarations, generic specializations, constants/defaults, type layouts, methods, virtual/interface bindings and other runtime-visible metadata. All reachable source must receive appropriate validation even when an unchanged artifact is reused; a cached object must never conceal a current source error.
- Include sufficient identity for compiler/build format, exact LLVM/Clang version and relevant ABI, target triple/architecture/CPU features, data layout, compile/optimization/debug options, dependency and runtime contract versions, and any instrumentation affecting the emitted object. Do not treat a filename or stale modification time as proof of compatibility.
- Persist relocatable artifacts rather than raw in-memory code addresses. After a restart, resolve symbols and register runtime metadata afresh; do not deserialize previous process heap objects, GC roots, JIT addresses, code leases or active generations. Preserve current Frame/Slot tracing, safepoints, failure cleanup and native lifetime/retirement guarantees.
- Missing, corrupt, partial, outdated, unreadable or otherwise invalid entries must be rejected safely and fall back to normal validation/compilation. Cache publication must tolerate interruption (for example, atomic complete-entry replacement), and failures must not overwrite known-good output or publish invalid native code. The entire cache must remain disposable without affecting correctness. Treat cache content as untrusted until validated against the intended local build inputs and expected format.
- Confirm Linux, Windows and macOS object/linking behavior and existing hot-reload/GC tests. A cold or invalid cache must compile the same program with the same diagnostics and observable behavior; a valid cache must preserve optimized/rooted execution semantics. Keep the existing owner-controlled seven-job hosted acceptance procedure when this goal is eventually implemented.

**Measure before design and optimization:** Use a realistic project (including a roughly 100,000-line workload) and record source discovery/parsing, semantic analysis, IR generation, LLVM optimization/codegen, object linking/relocation, metadata admission and process startup separately. Compare a cold first launch, warm unchanged launch in a fresh process after machine restart, a one-function edit, an imported declaration/layout change, and deliberate cache corruption/invalidation. Measure time to main/first useful frame, time to first call for optional laziness, cache hit/miss rates, invalidated work, disk usage and memory overhead. Determine where latency actually goes; "near-instant" is an aspirational outcome, not a guaranteed target or excuse to skip correctness validation.

**Authorization:** the owner subsequently approved whole-graph native-object caching and its first implementation. Release AOT, language semantics and CI configuration remain unchanged. Automatic watching and finer-grained native partitioning are outside this delivery.

**LLVM research reference:** [LLVM 19 ORC Design and Implementation](https://releases.llvm.org/19.1.0/docs/ORCv2.html), especially relocatable object linking, LLJIT and laziness; verify any chosen C++ interface against the pinned LLVM 19.1.7 headers/source before engineering work.

### Stage 0: Four early architectural proofs

#### Proof A: LLVM AOT native executable

- Generate and execute a minimal native program from hand-written or trivially generated LLVM IR.
- Call a function, return values and verify control flow.
- Generate usable source-location/debug information.
- Validate toolchain and linking on Linux and Windows.
- Keep the experiment independent of the full VOID parser.

#### Proof B: Precise garbage-collector roots

- Allocate managed objects and construct metadata describing their reference fields.
- Keep reachable references live across calls, loops and optimized code.
- Collect unreachable objects while preserving reachable ones.
- Validate reference tracking under LLVM optimization, not only in unoptimized debug builds.
- Verify GC safepoint and stack-map/statepoint integration against the pinned LLVM version.

#### Proof C: Hot replacement of native functions

- Give a compiled function a stable logical identity.
- Compile a replacement, make new invocations use it and preserve existing process state.
- Demonstrate safe behavior when old implementations are active on another thread.
- Retain old code until it is provably safe to retire.
- Do not assume ORC's ability to remove JIT resources automatically proves safe active-frame retirement.

#### Proof D: Native C interoperability

- Call a real C function through the target ABI.
- Pass a UTF-8 string with the appropriate lifetime and termination.
- Handle a native resource through an opaque wrapper and deterministic cleanup.
- Register and invoke a C callback with a stable callable address.
- Verify basic cross-platform linking and missing-library diagnostics.

**Gate:** If a proof fails, revise the architecture while the dependent codebase is still small. Do not expand the language surface until the relevant risk is resolved.

---

## 5. First ten milestones: #001-#010

Use milestone blocks of ten with an integration audit at the end. Numbering is for focus and traceability, not for racing to high milestone counts.

| Milestone | Responsibility | Minimum acceptance evidence |
|---|---|---|
| **#001** | Project structure, native build system, pinned LLVM toolchain, Linux/Windows smoke-test setup | Reproducible toolchain configuration and basic builds on available target systems |
| **#002** | LLVM AOT executable generation | Native program compiles, links, runs, returns the expected result, with source metadata |
| **#003** | LLVM ORC JIT function invocation | Generated native function can be JIT-compiled and called with validated ABI/signature |
| **#004** | Stable function identities and first hot replacement | Existing dispatch entry uses a new compatible function after an update; state remains unchanged |
| **#005** | Managed allocation and type metadata prototype | Test managed objects can be created and their reference-bearing fields described |
| **#006** | Precise GC root tracking and collection under optimization | Optimized code preserves live references; unreachable objects are reclaimed correctly |
| **#007** | Thread coordination for GC and code replacement | Safe thread rendezvous and no early retirement of currently executing code |
| **#008** | Native calls, UTF-8, ownership and callbacks | ABI tests, string-lifetime tests, callback test and deterministic resource release |
| **#009** | Combined runtime/LLVM stress tests and finalize cross-subsystem contracts | AOT/JIT/GC/native tests remain correct together; known limitations documented |
| **#010** | Architecture integration audit and decision review | Evidence-backed go/no-go decision for the broader compiler implementation |

### Scope cautions for the first block

- **#004 is an initial single-system proof**, not an excuse to claim fully production-ready multithreaded hot reload. #007 and #009 complete the cross-system evidence.
- **#006 cannot be locked solely because an unoptimized test passes.** LLVM-optimized code must preserve all required reference information.
- **#008 uses representative native calls**, not a giant package manager or SDL wrapper implementation.
- If experiments reveal a different dependency order, adjust the milestone plan deliberately and record why.
- Do not introduce the full VOID syntax, editor extension or every collection method into this first block.

---

## 6. Compiler implementation language: decision still open

**Current preference to evaluate: C++ for the compiler**, largely because LLVM's most extensive interfaces and much of the ORC tooling are C++ oriented. This is an engineering choice, **not** a change to VOID 2.0 syntax or a desire for developers to write C++.

Considerations:

- **C++ with LLVM C++ API:** Broad direct LLVM access and mature examples, but more C++ and LLVM-version complexity.
- **C using LLVM C API:** Potentially simpler compiler implementation style, but validate that the required ORC/JIT, GC and debug operations are adequately exposed before choosing it.
- **Runtime implementation:** C or C++ depending on actual requirements; the runtime ABI must be explicit and stable.

**Decision gate:** Choose the implementation language only after surveying the LLVM APIs needed by the four architecture proofs. Pin a tested LLVM version instead of chasing every upstream release.

---

## 7. Proposed repository structure

Use the existing project name **`Void.Language`**.

```text
Void.Language/
├── docs/
│   ├── LANGUAGE_BIBLE.md
│   ├── ARCHITECTURE.md
│   └── DECISIONS.md
│
├── src/
│   ├── frontend/
│   │   ├── lexer/
│   │   ├── parser/
│   │   ├── symbols/
│   │   └── semantics/
│   ├── ir/
│   ├── backend/
│   │   └── llvm/
│   ├── runtime/
│   │   ├── gc/
│   │   ├── memory/
│   │   ├── threading/
│   │   └── native/
│   └── tooling/
│       └── lsp/
│
├── stdlib/
│   ├── collections/
│   ├── files/
│   └── sync/
│
├── tests/
│   ├── compiler/
│   ├── runtime/
│   ├── integration/
│   └── performance/
│
└── examples/
```

This is a **starting organization**, not an instruction to create empty directories or speculative abstractions before they are needed. Keep individual milestones small.

### Documentation roles

- `LANGUAGE_BIBLE.md`: Language syntax, semantics, rules, examples and confirmed design decisions.
- `ARCHITECTURE.md`: Compiler, runtime, GC, hot reload and native interoperability contracts.
- `DECISIONS.md`: Short records of major decisions, rejected approaches, evidence and revisions.

Documentation updates should accompany behavior changes, not lag behind for months.

---

## 8. Testing strategy

The original VOIDC 1.0 work showed why platform differences must be discovered early. VOID 2.0 should continuously validate both expected behavior and diagnostics.

| Test category | Cadence / expectation |
|---|---|
| **Focused tests** | Every milestone, targeting exactly what changed |
| **Linux smoke tests** | Every relevant milestone |
| **Windows smoke tests** | Every relevant milestone on a real matching toolchain/environment |
| **macOS smoke tests** | As soon as a suitable test host/toolchain is available |
| **Optimized-code correctness** | From the first LLVM stage, not only after release builds |
| **GC/thread stress** | After changes to object layout, safepoints, allocator, threading or live-code lifetimes |
| **Integration tests** | At the end of each milestone block, especially #010 |
| **Sanitizers and memory checks** | Regularly where supported by the chosen compiler/runtime configuration |
| **Benchmarks** | Establish baselines early; rerun after performance-relevant changes |
| **Diagnostic regression tests** | Throughout frontend and runtime development |
| **Release audit** | Before each releasable version |

### Cross-platform testing contract

- Prefer running **the same `.void` program** on Linux and Windows and comparing observable behavior where platform-independent results are expected.
- Do not confuse cross-compilation success with successful execution on the destination OS.
- Check compiler invocation, linker discovery, paths, Unicode, line endings, library loading, platform calling conventions and runtime shutdown behavior early.
- If a platform is temporarily unavailable, record that as **not tested**, not passed.
- Avoid accumulating dozens of platform-specific failures until a late final audit.

### Source diagnostics and runtime diagnostics

The compiler and runtime should report VOID source locations, not backend implementation details in ordinary operation.

Example compiler diagnostic:

```text
src/player.void:12:19: error: Cannot assign string to i32
    code: VOID2001

  12 | var health: i32 = "hello"
     |                   ^^^^^^^

     Expected: i32
     Received: string

     Hint: Use an integer value or parse a valid numeric string.
```

Example runtime diagnostic:

```text
src/units/player.void:27:26: runtime error: Index out of range
    code: VOIDR1003

  27 | var item = inventory[5]
     |                      ^

     Index: 5
     Valid indices: 0..2

     Hint: Check the list length or use a safe lookup.

Stack trace:
  at player.get_item(index: i32)
     src/units/player.void:27
  at game.update(dt: f32)
     src/main.void:42
  at main()
     src/main.void:10
```

The codes and file paths above are illustrative. The **format, source-first philosophy, and stack trace placement** reflect the intended design. The editor should underline the same spans and offer accurate quick fixes where possible.

---

## 9. LSP and extension strategy: begin in Stage 1

The completed editor extension is a later deliverable, but source information must be captured from the first frontend milestones.

Required shared information:

- Stable symbol identities and precise source spans.
- Type and generic information.
- VFS import paths and module relationships.
- Documentation comments such as `##` associated with fields, structs, methods and parameters.
- Completion candidates and visibility (`public` by default, explicit `private`/`internal`).
- Definition locations, references, signatures and rename support.
- The same diagnostic codes, ranges, explanations and hints in CLI and editor.

Example language source:

```python
## Represents a controllable player.
struct player:
    ## Current health points.
    health: i32 = 100

## Restores the player's health.
func (p player) heal(amount: i32):
    p.health += amount
```

When someone types `hero.`, completion should show documented members from actual VOID declarations, even if a library is backed by C. Developers should not see C headers or mangled native names in ordinary popup help.

Future package discovery should allow searching, inspecting, installing and documenting compatible packages through the extension, with helpful diagnostics for missing or unsupported native dependencies.

**Do not create a second independent language parser or type checker for the LSP.**

---

## 10. Performance strategy and acceptance criteria

Performance requires measured tradeoffs, not blanket claims that LLVM or a particular collector is automatically fastest.

Priority order:

1. **Correctness:** GC, error handling, threads and optimization preserve semantics.
2. **Execution speed:** Efficient native code without redundant barriers or helper calls.
3. **Allocation cost:** Avoid provably unnecessary allocations; make remaining small allocations cheap.
4. **GC latency:** Track average and worst-case pauses, including game frame-time consistency.
5. **Memory usage:** Monitor fragmentation, live heap, metadata and allocation overhead.
6. **Compilation speed:** Fast incremental development while permitting aggressive release optimization.

Persistent Debug compilation caching has an initial production implementation described in STAGE5_CACHE_PROPOSAL.md. Lazy native materialization remains future-only. Stage 7 tuning requires measured evidence and must preserve the Stage 5 correctness contracts.

Potential mechanisms to evaluate:

- Precise initially non-moving mark-sweep GC.
- Per-thread allocation caches and reusable size-class storage.
- Inline `value struct` representation where safe.
- Escape analysis / scalar replacement where observable behavior is unchanged.
- Carefully placed necessary GC safepoints; do not insert redundant checks in every statement.
- Concurrency/GC thread coordination and low-contention allocation.
- More aggressive AOT optimizations and inlining in release builds.
- Development-mode indirect dispatch only where required for live code replacement.
- Later concurrent or generational GC work **only after measurements justify it**.

Do not optimize based on source-code aesthetics or comparisons against a C benchmark that omits VOID's safety and GC semantics. Benchmark comparable workloads and preserve checks/behavior or disclose differences clearly.

### Important distinction

- **VOID `struct`:** GC-managed, reference semantics; compiler may eliminate allocation when safe.
- **VOID `value struct`:** Value semantics and copy behavior; may live inline in registers, stack storage or larger managed objects.
- Field initializers such as `= 100` **do not determine stack versus heap allocation**.
- Safe VOID values are initialized. No exposure to uninitialized garbage data.

---

## 11. Milestone completion contract

A milestone may be called **locked** only when there is evidence for all applicable criteria:

- [ ] Scope is focused and the implementation matches the Language Bible.
- [ ] Code builds successfully in the relevant configurations without new unexplained warnings.
- [ ] All focused tests for the milestone pass.
- [ ] Relevant existing regression tests pass.
- [ ] Appropriate Linux/Windows checks pass, or unavailable platforms are explicitly documented as untested.
- [ ] Optimized-code correctness is checked when GC, LLVM lowering, arithmetic or code lifetime could be affected.
- [ ] Source and runtime diagnostics retain readable VOID locations and messages.
- [ ] Documentation and decision records are updated where behavior/architecture changed.
- [ ] No speculative abstractions, untracked temporary hacks or unnecessary duplicate paths are left behind.
- [ ] Performance changes include representative before/after measurement where relevant.
- [ ] Limitations and unresolved defects are reported candidly; a failed or unrun test is never presented as a pass.

**Block audit rule:** Every ten milestones, pause feature work and perform an integration audit. The milestone count is not itself the goal.

---

## 12. Architectural risks and review decisions

These are known design risks. They are not necessarily reasons to delay the first prototype, but they should be acknowledged and tested before they become difficult to change.

| Risk | Why it matters | Proposed handling |
|---|---|---|
| **Deep `const`/readonly semantics** | A readonly reference may still alias an object other code can mutate | Specify whether readonly access means immutable object or merely a nonmutating view; tests must match the Language Bible |
| **Root-level constants** | Automatically visible constants in `main.void` may create initialization dependencies | Separate true compile-time constants from runtime module initialization |
| **Struct initialization** | Field defaults, named arguments, `init()` and fallible constructors can conflict | Specify exactly one construction sequence; partially initialized objects never escape |
| **`value struct` GC references** | Inline values can contain managed references | Preserve precise embedded-reference metadata through copies and optimizations |
| **Closures** | Captured locals can outlive declaring frames | Runtime-managed closure environments and correct root tracking |
| **Generic specialization** | Monomorphization can inflate native code size | Choose specialization carefully and benchmark code size and performance |
| **Hot-reload versions** | Old code can remain active in threads, closures and suspended async functions | Stable identity, compatible updates, versioned metadata and safe retirement |
| **C string/handle ownership** | C headers rarely communicate full lifetime contracts | Explicit binding ownership metadata and automated safe conversions |
| **LLVM statepoint/version coupling** | GC metadata is deeply tied to emitted native code | Pin tested LLVM and build optimized-root correctness proofs early |
| **Cross-platform toolchains** | Linking, ABI, Unicode and OS behavior vary | Continuous Linux/Windows smoke tests; clear untested status for missing hosts |

### Current intended language/runtime contract (brief recap)

The full rules and examples belong to the Language Bible, but architectural milestones should preserve:

- `func` for functions, methods, callbacks and callable structs (`call()`).
- `struct` reference semantics; `value struct` value semantics.
- Public by default, optional `private` and `internal`, `static` for type-level members.
- `var` for mutable declarations and deeply readonly access through `const` (with aliasing semantics to be made exact).
- `[]` for growable or explicitly fixed-size lists (`[i32, 4]`), `{}` for dictionaries.
- `match`, `is`, `Ok`/`Err`, `result<t, e>`, error propagation through `?`.
- `defer`, `defer { ... }`, `with`, `init()` and deterministic `deinit()`.
- `async func`, `await`, `spawn`, task groups, cooperative cancellation.
- Automatic precise GC and native handles via bindings; no ordinary `unsafe` requirement.
- Hot reload on compatible body changes; manual restart for `const` values, layout or incompatible signature changes.
- VFS root and relative `use` paths with `/` across platforms.
- UTF-8 `string`, automatic interpolation and developer-friendly formatting.

The compiler's internal runtime and backend can be complex; that complexity should not leak into the language experience.

---

## 13. What to do first

**Start with VOID 2.0 #001: Compiler Foundation and LLVM Toolchain.**

Its responsibility is deliberately narrow:

1. Establish the new `Void.Language` repository's initial build and test structure.
2. Choose a working compiler implementation approach only after checking required LLVM interfaces.
3. Pin and document a specific LLVM/toolchain version.
4. Make the minimal compiler harness reproducible.
5. Prove basic local native builds and establish Linux/Windows smoke-test infrastructure.
6. Record what passed, what failed and what was not tested.

**Do not implement the entire parser, GC, async, native package ecosystem or hot reload in #001.** Each earns its own focused milestone.

**Do not begin #002 until #001's toolchain and portability assumptions are understood.**

---

## 14. Instructions for future conversations or milestone implementers

If this file is used after context has been lost:

1. Read this document and the **VOID 2.0 Language Bible** before proposing changes.
2. Treat confirmed decisions as constraints. Do not quietly replace VOID's chosen semantics with C#, Go, Rust, Odin or C conventions.
3. Distinguish **agreed language behavior**, **proposed implementation choices**, and **unresolved details**.
4. Do not assume the earlier VOIDC 1.0 implementation or a previously delivered ZIP is current source. Inspect the actual current `Void.Language` tree when implementation work is authorized.
5. Keep the project name `Void.Language`. The earlier project was backed up.
6. Follow KISS/DRY, focused milestone scope, periodic audits and source-level diagnostics.
7. Do not trade away GC, automatic safety, simple strings or other core guarantees to make synthetic benchmarks faster.
8. Do not claim a proof, platform test, successful hot reload or locked milestone without actual evidence.
9. If an implementation decision changes, explain the tradeoff and update the decision record and Bible.
10. **No automatic restarts:** hot reload applies compatible code changes, while layout/constant/signature changes wait until the developer restarts manually.

### Final architectural recommendation

**Proceed with the architecture, conditional on the four early proof-of-concept gates.**

Develop VOID 2.0 in two broad periods:

- **Architecture validation:** Prove LLVM AOT, optimized GC root tracking, safe live native-code replacement, and C interoperability.
- **Incremental implementation:** Build the documented language on those foundations, adding one tested feature at a time.

Every ten milestones, pause for an integration audit. Advance based on evidence and maintainability, not milestone count.

**A small language with a sophisticated runtime is the goal. Keep the developer experience simple even when the underlying implementation is hard.**

---

*Prepared as a portable Markdown companion to the VOID 2.0 Language Bible. It is a strategy and specification artifact, not an implementation-status claim.*

## Historical execution policy: 10 October 2026 (superseded by the owner workflow above)

The planning snapshot above is preserved. The user's current instructions
supersede its small numbered milestone process: develop substantial cohesive
phases, complete related work and verification together, and record actual
results. The first phase combines project/toolchain setup with executable AOT,
ORC invocation, leased function replacement, explicit-root tracking and C ABI
proofs. This does not satisfy the full multi-thread GC/statepoint integration
gate and must not be described as a completed production runtime.

C++20 with the LLVM 19.1.7 C++ development SDK is now the implementation choice.
The canonical design companion is [LANGUAGE_BIBLE.md](LANGUAGE_BIBLE.md).
Build instructions are in the repository README; decisions, tests and remaining
work are maintained in [DECISIONS.md](DECISIONS.md), [WORK_LOG.md](WORK_LOG.md),
and [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

Work remains local. The user will create the public GitHub repository and make
the initial push. The local workflow targets standard Linux, Windows and macOS
hosts; no hosted platform result can be claimed until its executable tests run.


## Progress after source compiler and module phases

The user reports the first real source compiler passed Linux/Windows/macOS
Debug/Release and Linux sanitizers. The next cohesive phase implements a real
VFS/import graph, function ownership/visibility and multi-file native JIT/AOT;
see MODULE_SYSTEM.md, DECISIONS.md and WORK_LOG.md for its current evidence and
provisional path policies. Its expanded hosted matrix remains pending. The user
owns commits/pushes and hosted runs; compiler work remains local unless authorized.

The next recommended language phase strengthens const/project-root constants,
numeric policy/conversions and source-first runtime diagnostics. Module cycles
for functions do not settle constant initialization. Managed types still require
the separate runtime root/thread integration gate; packages and external imports
need deliberate identity/permission contracts.

### Current value-semantics phase, 10 October 2026

The user reports the preceding ten-suite module phase passed Linux, Windows and
macOS Actions. Constants, compile-time dependency analysis, numeric parity and
source-first native arithmetic diagnostics now extend the same frontend/IR/backend.
See VALUE_SEMANTICS.md and WORK_LOG.md for scope, exact verification and limits.
Recommended next work: diagnostic structure/recovery and dependency compatibility,
then deliberate contextual literal/conversion policy before wider numeric types.
Managed types still require the separate GC root/thread integration gate.

### Phase 5 diagnostic recovery, 10–11 October 2026

The user reports Phase 4 commit `d78b6b8`, Actions run #6, passed Linux, Windows
and macOS. Structured codes, byte spans, immutable snapshots, related declarations
and bounded lexer/parser/semantic recovery now extend that foundation. The complete
graph is validated before lowering, with invalid source excluded from native
execution, object publication and reload. See DIAGNOSTICS.md and WORK_LOG.md for
contracts, measured local verification and the pending Phase 5 hosted run.
Next design precise validated source/dependency compatibility and analysis-result
APIs before incremental IDE integration. Numeric expansion still needs deliberate
contextual literal/conversion decisions; managed types still require the GC gate.

### Phase 6 approved numeric language

The user reports Phase 5 commit 4f3688d passed hosted run #8, then approved the
numeric policy table before implementation. The full ten-type numeric family,
contextual literals, strict existing-value conversions and checked numeric as
extend the same compiler pipeline. See NUMERIC_TYPES.md, DECISIONS.md and
WORK_LOG.md. Preserve the separate GC integration gate and defer nullable casts,
managed types and arbitrary public ABIs. The next recommended cohesive phase is
validated analysis/dependency APIs and compatibility metadata for incremental
tooling. Phase 6 platform claims must come from its own hosted executable runs.

### Phase 7 validated analysis and dependency compatibility

Phase 6 commit `9b9fdac` passed hosted Actions run #9 according to the user.
Phase 7 exposes reusable owning successful analysis and exact compiler-derived
source compatibility through the same frontend. See ANALYSIS_COMPATIBILITY.md.
Next evaluate an in-process tooling consumer and invalidation planning before
incremental compilation or LSP transport. The GC/thread-root integration gate,
external package identity policy and arbitrary native ABI work remain separate.

### Phase 8 in-process tooling foundations

The user reports Phase 7 commit `04768bf` passed hosted run #10 on all platforms.
TOOLING_FOUNDATIONS.md documents the owned symbol-query index and deterministic
invalidation planner consuming existing validated analysis records. Old graph
plans remain conservative; exact source compatibility remains compiler-owned.
Next establish explicit client document/project revisions and invalidation
orchestration before partial/incremental analysis or LSP transport. Preserve the
managed-runtime/root integration and external-library identity gates separately.


## Phase 9 document revisions and tooling sessions

The user reports Phase 8 commit `f25b268` passed hosted run #11 on all platforms.
[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) defines owned whole-document overlays,
strict per-document logical revisions, project identity tokens, explicit close versus
unavailable transitions, complete analysis selection and stale-result acceptance.
Current queries never fall back to historical success after edits/failures. Dependency
orchestration reuses Phase 8 and the exact Phase 7 compiler contract; native
publication remains separate. Disk fallbacks are reread and checked during analysis,
but are not an atomic filesystem image; clients explicitly refresh for observed
disk changes. All sixteen preceding suites remain, with sessions/sessions_demo
bringing the total to eighteen. Actual local results are recorded in WORK_LOG.md.
Hosted Phase 9 verification remains pending. No LSP, watcher, worker, persistent
cache, partial compiler, new language feature or automatic native reload is added.
Next consider protocol position adaptation and a minimal tooling client/transport
before partial compilation; retain the separate managed-runtime and package gates.


## Phase 10 minimal language server

The user reports Phase 9 commit `02a9aa7` passed hosted Actions run #12 on Linux,
Windows and macOS. Phase 10 connects that immutable session foundation to real
stdio LSP clients: full-document edits, current diagnostics, definition and
references, using exact UTF-16 snapshot ranges. [LANGUAGE_SERVER.md](LANGUAGE_SERVER.md)
documents its actual scope. Three portable position/process suites bring CTest
to twenty-one without changing the eighteen existing suites or language semantics.
See WORK_LOG.md for local Debug/Release/strict sanitizer evidence from both SDKs;
a new user-triggered hosted run is still required.

Next validate hosted integration, then consider a small editor client/extension
and an explicit disk refresh/cancellation policy before asynchronous or partial
analysis. Preserve exact compatibility and source ownership when designing workers;
do not mistake dependency planning for an implemented incremental compiler.
Completion/hover/rename, caches, watchers and automatic native reload remain
deferred. Managed-runtime integration and package identity remain separate gates.
The user retains control of all commits, branches, pushes and publication.


## Phase 11 editor integration

The user reports Phase 10 commit `ccfea79` passed hosted run #13 on Linux, Windows
and macOS. Phase 11 adds an isolated TypeScript VS Code/VSCodium client over voidls,
real editor API integration tests and local VSIX packaging. The native compiler
still builds/tests without Node.js. [EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md)
explains configuration and use; WORK_LOG.md records actual evidence. Hosted CI
retains the 21 native suites and adds pinned Node, editor tests and packaging.

Next verify the expanded hosted matrix and VSCodium/minimum-editor compatibility.
Then consider explicit disk refresh/cancellation and client usability before
workers, partial analysis or wider IDE features. Preserve exact analysis/reload
contracts and owned revisions. Managed-runtime and external-package design gates
remain separate; no language expansion or automatic native publication is implied.

## Phase 12 local implementation: value struct foundations

The owner reports Phase 11 run #16 at `1b32151` passed all three hosted platforms.
Phase 12 returns to source language work with numeric/bool/acyclic nested value
structs, shared nominal types, aggregate IR, copy semantics, pure defaults and
internal aggregate parameters/returns. The owner approved Option A in
VALUE_STRUCTS_PROPOSAL.md. VALUE_STRUCTS.md describes the implementation and
provisional layout/ABI boundaries; WORK_LOG.md records actual verification.

The next recommended cohesive phase is receiver methods and initialization
policy, settling readonly receivers and observable initialization ordering before
custom init or managed reference fields. No production-GC readiness is implied.

## Phase 13: owner-module receiver methods and initializer groundwork

Phase 12 `acf7355` passed hosted run #18 on all platforms according to the owner.
The owner approved M1 + I1: real methods for supported value structs and validated
receiver storage/effects, with public custom init execution deferred. Shared typed
analysis, IR calls and LLVM lowering preserve copies and native failure propagation.
Tooling and exact reload contracts consume the same inferred method metadata.

All 22 previous suites remain, plus receiver_methods_cli (23 total). Preserve both
SDK/RTTI validation configurations, strict sanitizers, small-stack regression and
real editor/package checks. RECEIVER_METHODS.md describes scope; WORK_LOG.md records
actual local results and pending hosted checks. The next language design phase
should resolve initializer/named-field/parameter/nested-default/purity interactions
coherently before custom construction. Managed types still require the separate
GC/root/thread/runtime gate. No further phase is authorized by this recommendation.


## Phase 14 custom value initialization

Phase 14 implements owner-approved C1 value initialization through the existing semantic
model, typed construction plans and receiver-place calls. Preserve required-child and
pure-context boundaries; do not bypass init through implicit zeroing. INITIALIZATION.md
defines the surface. Validate all 24 suites with both SDKs, strict sanitizers,
small-stack and actual editor checks. Future fallible/resource/managed construction
still requires separate design approval; runtime field defaults and general function
named/default parameters are not implied by this phase.


## Phase 15 runtime integration progress

The owner approved G1–G6 in GC_RUNTIME_PROPOSAL.md. Implement the foundation as one
cooperative runtime over the existing tracer and shared IR/LLVM path. Preserve the
24 prior suites and add seven focused runtime suites covering coordination, failures,
callbacks, optimized roots and separately linked AOT. GC_RUNTIME.md describes the
contracts. Source managed structs and broader runtime features remain a later design
phase; explicit root success does not claim completion of the separate statepoint/
stack-map experiment or production latency/throughput goals. Hosted Windows/macOS
must validate this phase after the owner's push.

## Phase 16 managed source integration

Owner-approved P1–P7 now connect managed source construction, reference identity,
nullable cyclic graphs and deep readonly views to the existing shared frontend,
IR, LLVM and coordinated runtime. Complete-object publication is reusable runtime
work, not a source-specific incomplete-object exception. MANAGED_STRUCTS.md records
implementation and deferred managed init/reference-containing value work. The
seven-runner workflow remains unchanged; four additional suites extend coverage.

## Phase 17 managed custom initialization

Owner-approved S1 + MI1–MI7 is implemented through the existing typed AST, validated
analysis, shared IR/LLVM and unchanged coordinated runtime. Staged construction
views, required-reference readiness and bounded safe-helper contexts precede the
existing complete-object commit. P3 deep readonly and P4 publication remain intact;
no parent identity or managed root exists before successful commit. See
MANAGED_INITIALIZATION.md for source behavior, restrictions, failures and reload.

The owner reports Phase 16 run #28 passed all seven jobs, locking its 35 suites.
Four additive Phase 17 source/normal-AOT/optimized-AOT/CLI suites bring coverage to
39. Preserve six Linux SDK configurations, strict sanitizers/leaks, small-stack,
repeated GC/coordination stress, real editors and VSIX, then all seven hosted jobs.
Do not infer Windows/macOS ARM64 results from Linux. Next investigate reference-
containing value capabilities; broader construction identity/escape, fallible
results or resource lifetime require their own approved contract.

## Phase 18: reference-containing values

The owner locks Phase 17 at seven-job run #29 (39 suites) and approves RV1–RV7.
REFERENCE_VALUE_STRUCTS.md documents the extension through shared semantic facts,
IR capability/readiness validation and existing Frame/Slot/heap publication. Four
additive suites bring the inventory to 43. Preserve both LLVM 19.1.7 SDKs across
Debug/Release/strict sanitizers, stress/small-stack and real editor gates, then all
seven hosted jobs. Review hosted acceptance and frame/analysis pressure before
selecting a separately designed language expansion. C1, S1 and P3/P4 remain
authoritative; broader initialization and heap-field capabilities are deferred.


## Phase 23 large-array review gate, 15 October 2026

The language must support large fixed and growable arrays without an arbitrary
64 KiB source-level ceiling. Keep the target-aware inventory, precise GC, machine
CFG, ABI, stack-depth and native cross-platform research. Separate stack safety
from payload size; the 65,536-byte threshold can remain a research parameter,
not a universal production rejection policy.

Next investigate large fixed-value placement/copying using the existing coordinated
GC and compare safe storage candidates. Preserve snapshots and alias-conflict
fallbacks, readonly/reference semantics, precise roots, complete publication,
allocation failure, safepoint/reload contracts and one memory-management system.
No representation, changed numerical ceiling or inline/heap cutoff is selected.
Review the frontend/IR 65,536 expanded-layout/multiplicity checks and eager default/
metadata expansion as analysis-resource work. Do not move the same source-visible
ceiling into a renamed guard. Retain finite-type, overflow and genuine work protection.

Owner/wrench approval of revised storage and resource policy precedes implementation
and native-array activation. Thereafter require large fixed/growable positives,
megabyte-scale payload/copy/GC/failure tests and shared frontend/IR/JIT/AOT/LSP
consistency. Every required function must be represented in successful output;
unsupported/failed compilation must diagnose and fail the entire build atomically.
Preserve fail-closed coverage and all current tests, then the existing full local
and seven-job hosted acceptance. Do not trade safety checks for acceptance or
claim cross-compilation as native execution. No compiler/runtime or CI changes
are authorized by this documentation correction.
