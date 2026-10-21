# VOID 2.0 language completeness roadmap


20 October 2026. **Research reviewed; development direction and sprint sequence
approved by Boss. The full roadmap is NOT implemented.** The inspected baseline is
`f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`; Stage 7 was hosted LOCKED by
CI #81 attempt 2, seven green jobs and 980/980 test suites. The research evidence
below remains a snapshot, not proof that the recommended features are implemented.
See [Boss approvals and sprint ledger](LANGUAGE_COMPLETENESS_APPROVALS.md).

## Current execution status — 20 October 2026

Stage 8 Sprint 4 is **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending**.
Final consolidated Debug, Release and strict ASan/UBSan/leak runs passed
**148/148 each: 444/444 unique configuration/suite outcomes**. Both complete
1 MiB frontend gates passed. See [the application/static-provider contract](STAGE8_APPLICATION_NATIVE.md)
and [Sprint 4 acceptance](stage8/SPRINT4_ACCEPTANCE.md) for exact evidence,
preliminary failures/corrections, source hashes and limits.

All three Sprint 4 design decisions are approved in [DECISIONS.md](DECISIONS.md);
no Sprint 4 owner decision remains pending. The bounded sink requires explicit
C++ admission, one provider descriptor per graph; CLI/LSP imports grant no trust.
Sprints 1–3 remain preserved, including [CI #82's Windows repair](stage8/CI82_WINDOWS_REPAIR.md),
argument binding, numeric prelude, UTF-8/collections, tooling and reload contracts.
Prior failures and verification records remain historical evidence.

**Hosted CI remains deferred until the end of Stage 8**, unless Boss explicitly
authorizes an earlier run. Windows/macOS, hosted CI #82 Debug/Release execution
and fresh actual editor-client verification remain pending. Linux local tests
cannot establish those results. Stage 8 is not hosted locked.

**Stopped after Sprint 4 for Wrench review. Sprints 5–6 remain future work.**
No commits, pushes, branch/remote changes, hosted CI, publication or website work
occurred. Root README is unchanged; documentation and future sprint prompts belong
under `docs/` by default. Do not begin Sprint 5 without Boss authorization.

The research narrative below remains the pre-sprint snapshot. Its earlier
missing-feature statements are superseded only by the delivered contracts and
actual acceptance records. The full roadmap is not implemented.

The [companion audit](LANGUAGE_COMPLETENESS_AUDIT.md) contains the feature inventory,
known-issue dispositions, source references, reproducer results and design research.
This document orders the actual dependencies for substantial standard-library work.

## Recommendation and dependency order

**Implement the nested-module, shared project discovery and import IntelliSense
foundation first.** It must cover automatic main.void selection with explicit-file
overrides and both requested import domains: unquoted stdlib paths and quoted
project-VFS files, including nested-folder completion. A partial solution containing only
`use io` parsing or a hardcoded VS Code list would not satisfy the mission.

No new production safety failure was reproduced that demands a repair sprint
ahead of it. Priority A remains a release gate throughout: any confirmed root,
ownership, source identity, stale-edit, numeric or readonly defect takes precedence.
The currently misleading `use ` keyword completion is a tooling defect addressed
by the first sprint. Historical large-array/deep-chain blockers are not reasons to
delay it. Long-lived async retention is an evidence-backed resource limitation,
with a separate measurement-led sprint before async-heavy application claims.

| Cohesive sprint | Priority | User benefit and dependency | Standard-library readiness |
| --- | --- | --- | --- |
| Nested modules, shared project discovery, import discovery and source docs | B, with A invariants | Find main.void, standard modules and project files consistently in every editor; import documented APIs with stable identities. Reuses current VFS/analysis/voidls. | Establishes where library sources live and how developers discover/use them. |
| Consistent ordinary calls and built-in APIs | B/C, with A numeric/readonly checks | Ordinary defaults/names, type properties, one implementation behind `T.operation(x)` and `x.operation()`. Depends on canonical core ownership. | Removes disruptive API-authoring gaps before substantial numeric/core modules. |
| Text and recoverable collection primitives | B | Inspect/convert UTF-8 and perform safe lookup/pop using existing storage and results. Depends on approved error/text contracts and shared member admission where needed. | Enables parsing, data handling and a useful initial pure library. |
| Optional-main application entry and static native providers | B, with A ownership gates | Run top-level code or no-argument main, honor exit status, preserve legacy examples; support real IO/resource wrappers beyond the fixture. Application entry need not wait for native generalization. | Required before substantial IO/process/native-backed modules, not before every pure utility. |
| Long-running async resource readiness | A/P when demonstrated; otherwise targeted follow-up | Bound completed-work retention without breaking awaits/cleanup/leases. Independent measurement can precede native/application sprint. | Required for long-running async-heavy use, not a blanket gate on pure modules. |
| Selected language ergonomics | C | Approved has, if-expression, convenient multiline calls, text formatting/interpolation and other demonstrated needs. | Schedule only what initial library/application code actually needs. |

These are substantial bounded sprints, not dozens of micro-milestones. Each needs
its own explicit engineering authorization. First-sprint success does not authorize
the remaining rows or a standard-library implementation.

## First sprint: nested modules, project/import discovery and source docs

### Objective and scope

Provide one compiler-owned project-selection and resolver/discovery model for
main.void projects, standard source modules and developer-created project files.
Users should be able to open a conventional project without configuring its entry, discover a module,
finish a valid import, then inspect and call documented APIs without visiting a
website. No registry, install workflow, external package manager or editor-specific
semantic engine is necessary.

Owner-requested behavior is fixed:

```void
use io
use io/files as files
use random/hash
use "systems/player.void" as player
```

Names are illustrative, not an approved library catalog. Unquoted imports belong
to stdlib; quotes select the project VFS. Preserve existing quoted `/`, `./` and
`../` imports. Current compiler rejects all unquoted examples and the bare quoted
example, so both parser and resolver changes are required.

### Reusable architecture and implementation boundaries

1. **Extend the existing import representation and loader.** Add import kind;
   resolve an unquoted module directly to `<stdlib root>/<segments>.void`.
   `io.void` and `io/files.void` may coexist; no index/mod fallback, folder-wide
   imports or automatic reexports. Existing per-file parse, signature registration,
   constant dependencies, ownership and shared IR lowering remain authoritative.
2. **Keep source domains distinct.** Preserve project `/path.void` identities;
   add unambiguous stdlib origin/library identity without mounting a user-shadowable
   project directory. Keep stable identity separate from physical diagnostic/debug
   filenames. One configured matching root must serve CLI/compiler and voidls.
   No cwd-dependent search or network access is required.
3. **Make visibility library-aware.** Reuse current private/public/direct-import
   rules. Recommend stdlib-internal access within one logical library, never
   automatically within the user project. Prevent root constants and root-owned
   method privileges from crossing into standard modules. Reject dependencies
   from standard sources into project files under the proposed initial policy.
4. **Expose a small metadata discovery query from the frontend.** Reuse/factor
   SourceGraph's immediate-child enumeration, portable path checks, exact-case
   collision rules and overlay merge. Return module/file/folder records and exact
   edit spans. Reachable semantic modules alone cannot discover unimported files.
   Do not parse every candidate file, recursively crawl the tree or build a catalog
   framework. Full imports still pass normal loader admission.
5. **Recognize incomplete imports in compiler recovery.** Handle `use `,
   `use io/`, `use "` and `use "app/` before full lexical/semantic success.
   Preserve normal diagnostics on original immutable bytes. This recovery is
   metadata-only and cannot produce validated IR or repair arbitrary statements.
6. **Reuse the shared LSP service.** Translate compiler candidates to ordinary
   completion items and UTF-16 text edits; support manual invocation and automatic
   suggestions on space, quote and slash in valid contexts. All editor adapters use
   the same server. Folder choices allow continued nested completion; choosing a
   project `.void` file completes a valid quoted path without duplicate quotes or
   suffixes. Preserve aliases/comments and middle-of-path edit correctness.
7. **Retain source documentation in tooling metadata.** Attach `##` and parameter
   descriptions to real compiler symbols, then feed completion, hover and signature
   help for imported project/standard APIs. Keep comment metadata separate from
   native compatibility. Missing docs retain signature fallback. Navigation uses
   actual declarations; no website catalog or raw-C signature guessing.
8. **Extend known-root navigation and freshness.** Map standard-source physical
   URIs explicitly for navigation/docs without admitting arbitrary external paths
   or standard-source project overlays. Server guards prohibit project rename/
   format edits into installed standard sources. Refresh metadata on observed
   directory/overlay/root changes and reject stale results. Reuse session selection,
   save/watch/refresh and existing request ordering instead of a new scheduler.
9. **Integrate dependencies and compatibility.** Include origin, standard sources
   and provider contracts in existing fingerprints, invalidation and cache/native
   compatibility logic. Do not accidentally reinterpret every source origin as
   the entry project or bypass existing restart/lease rules. Source imports never
   authorize native bindings.
10. **Share main.void project selection.** Factor one frontend selection helper
    for CLI and voidls: explicit entry wins; otherwise bounded nearest main.void
    discovery; no arbitrary scan for functions. Expose a small read-only voidls
    preflight so every adapter can obtain entry/root before creating its LSP client
    and selectors. Keep the current `--entry` server startup after resolution.
    Standardize inputs, standalone labeling, exact-case checks, failure handling
    and deliberate reselection; do not copy search rules into each editor.
    This work chooses a source graph, never executes main/top-level statements.

Primary touchpoints: [compiler.cpp](../src/frontend/compiler.cpp) (`Parser`,
`normalize`, `SourceGraph`, binding/root lookup, `AnalysisAccess`, editor assistance),
[frontend.hpp](../include/void/frontend.hpp), [tooling.hpp](../include/void/tooling.hpp),
[tooling.cpp](../src/frontend/tooling.cpp), [main LSP adapter](../src/lsp/main.cpp),
[positions.cpp](../src/lsp/positions.cpp), CLI/cache compatibility paths and
`editors/`. Relevant contracts: [MODULE_SYSTEM.md](MODULE_SYSTEM.md),
[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md), [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md),
[STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md), Bible §§10–11/46–47.

### Focused tests and acceptance

Use current `modules`, `analysis`, `tooling`, `sessions`, source CLI/JIT/AOT,
cache/reload, LSP and actual editor harnesses; extend them where the new behavior
crosses their existing boundaries. No standalone validation platform is needed.

| Test family | Required positive and negative evidence |
| --- | --- |
| Resolver compatibility | Old quoted roots/relative forms still work. New bare quoted spelling obeys the chosen nested-importer relativity. Unquoted top-level/nested modules work; `.void` is omitted only for stdlib. Same file/folder stem works without index fallback. |
| Shared project discovery | CLI/voidls agree for identical root/active-file inputs: main.void, nearest nested main, absent/invalid main, explicit alternate entry and invalid override. No cwd/home walk outside the selected boundary, case/alias fallback or recursive source scan. |
| Namespace and ownership | Same project/stdlib filename cannot collide semantically; repeated/diamond/cyclic imports register once; relocation preserves identity; changing source bytes invalidates artifacts. Library internal/private rules and no project-root-constant leakage are tested. |
| Alias/name collisions | Leaf default qualifiers, explicit aliases, local/simple-call precedence, duplicate leaves, mixed-domain ambiguity, idempotent repeats and no transitive reexport match approved rules. Test keyword-named module path policy. |
| Import-path safety | Wrong case/case siblings, malformed segments, root escapes, drive/UNC/URL/backslash, symlink/redirect, unreadable parents, unavailable stdlib and hard-link graph aliases diagnose correctly. No arbitrary external URI admission. |
| Stdlib discovery | `use ` lists modules/folders; `use io/` lists only that directory; additional levels work; module versus same-stem folder labels are distinguishable. No project files leak into unquoted completion. |
| Project discovery | `use "` lists eligible files/folders; `use "app/` and deeper prefixes list immediate children, including currently unimported sources. Selecting a file yields valid quotes, extension and path; selecting a folder permits continued completion. No stdlib suggestions in quoted imports. |
| Overlay/disk synchronization | Dirty edits win; new named overlay-only files/parents appear; unavailable entries disappear; close restores disk. Create/delete/rename/refresh changes listings. Non-atomic deletion after completion becomes a normal loader diagnostic. |
| Project-selection lifecycle | Named unsaved main handling is explicit; source errors never trigger fallback. Opening another file or creating/removing main cannot silently rebase a fixed session or dirty overlays. Deliberate restart/reselection creates fresh tokens. |
| Incomplete text/edit correctness | Unterminated quote, partial leaf, cursor in middle, existing quote/suffix/alias/comment, CRLF, Unicode before cursor and no final newline have correct spans. No candidates in unrelated strings/comments/division. Existing `use ` replacement with `func` is prevented. |
| Semantic API integration | Imported visible functions/members complete, hover source docs, display parameter docs/signature mapping and navigate to actual source in both domains. Private/internal APIs are filtered. Broken unrelated bodies do not prevent safe name discovery. |
| Version/read-only safety | Requests across edit/close/reopen/root change do not return applicable stale edits. Standard navigation works without writable standard overlays. Rename/format cannot edit standard targets; current project rename guards remain intact. |
| Editor behavior | Actual VS Code, VSCodium, Neovim and Vim checks use the same discovery fixtures/inputs and explicit-entry override, then verify automatic prefixes, nested folder selection, valid file acceptance, API docs/navigation and lifecycle. Unexecuted combinations remain unverified; manual request success alone is insufficient. |
| Regression integration | Existing whole-graph semantics, UTF-16 diagnostics, JIT/AOT, cache/reload and full current suites remain green. Owner alone schedules the seven-job hosted gate after local review. |

For metadata performance, measure cold/warm discovery at 10, 1,000 and 10,000
sibling entries plus deep irrelevant descendants and overlay-heavy projects.
Separate enumeration/filtering, semantic analysis and protocol costs. Record
median/tail, result count/bytes, peak memory and edit validity; test prefix matches
beyond any result cap. The structural acceptance criterion is no recursive scan or
whole-project compilation for a directory menu, bounded honest incomplete results,
and repeatable responsiveness under typing/refresh. Agree any numerical latency
threshold from the representative baseline rather than inventing a universal SLA.

Reuse exact-case directory work before adding a cache. A small revision-scoped
directory memo is justified only if measurement shows repeated enumeration is
material; invalidation must include overlay and observed disk/root changes. No
database, worker pool, incremental compiler or persistent symbol catalog is part
of this sprint. Existing measured large-graph semantic completion is about 192 ms;
import discovery should not need that full semantic probe path.

**Sprint exit:** conventional projects and explicit overrides select consistently
through shared voidls discovery; all requested import prefixes work through the
same metadata query in real editors, selected files yield loader-valid imports, documented imported APIs
resolve with correct visibility/source identity, safety/regression checks pass,
and contracts/examples are updated to the approved spelling. Synthetic disposable
standard modules suffice for tests; no real IO/hash library is required.

### Historical design questions before authorization

Boss must choose bare quoted relativity, standard-root selection, library-internal
boundary, keyword-segment policy and bounded project search/reselection rules,
with Wrench reviewing the identity encoding
and navigation/overlay guards. The user has already selected unquoted stdlib,
quoted project VFS, nesting, automatic main.void discovery and both forms of IntelliSense: these are not questions
to reopen. See the consolidated decision table below for recommended defaults.

## Sprint: consistent ordinary calls and built-in APIs

**Objective:** enable ordinary reusable APIs and the requested numeric/property
ergonomics without duplicating implementations or creating runtime type objects.
Dependencies: accepted standard/core ownership and import identities; approved
numeric edge behavior and encapsulation scope.

Reuse initializer argument mapping for ordinary named/default calls; preserve
evaluation order, duplicate/missing-name diagnostics, compile-time default rules,
generic inference and signature help. Default-based parameter inference requires
an explicit parser/type-check addition; it is not currently delivered. Do not add
overloads, runtime defaults or constructor redesign as collateral work.

Extend the existing typed receiver/member machinery with primitive/type-qualified
selection. `i32.abs(x)` and `x.abs()` must resolve one canonical callable, use the
same argument mapping, evaluate x once and produce the same effects/diagnostics.
Keep postfix chaining. Start with nonmutating numeric operations and exact typed
limits, not every imaginable string/collection method. Ordinary source algorithms
or thin bindings to existing operations are preferred; APInt/APFloat supplies
constant bit patterns. Include Type::kind in primitive member keys.

Keep source docs, completion, hover, bound/unbound signature presentation and
navigation tied to that canonical declaration. Decide whether imports or a tiny
core prelude admit members; do not let arbitrary modules attach primitive methods
or let duplicate providers depend on import order. Primitive runtime boxing,
numeric trait hierarchies and generic interfaces remain outside scope.

If public library wrappers need protected storage, include a bounded private-field
and declared-readonly-field contract in this sprint, after explicit approval.
It must cover initialization, getters/setters, composition promotion, generics and
tooling visibility. Existing const views alone cannot enforce private storage.
General static mutable state and a new inheritance model are unnecessary.

**Tests/acceptance:** ordinary/constructor argument parity and order; negative
names/defaults/inference; all numeric widths/extrema; signed-min checked abs;
float NaN/infinity/zero contracts; constant contexts; both call forms and chains;
one-evaluation side effects; readonly rejection; canonical identity/collisions;
generic specialization; normal/O2 JIT/AOT agreement; completion/signature/docs and
compatibility invalidation. If fields are included, prove no access/authority
escape through properties, promotion or callbacks. Use existing frontend/numeric/
generics/properties/analysis/IR/native-engine suites.

Primary files: frontend compiler and generic includes, `src/ir/interfaces.inc`,
`include/void/ir.hpp`, `src/numeric.cpp`, existing backend calls and tooling metadata.
Contracts: NUMERIC_TYPES, VALUE_SEMANTICS, PROPERTIES, GENERICS, DECISIONS and Bible
§§11/14/19/31. No algorithm-specific intrinsic is accepted just for syntactic sugar.

**Exit:** approved ordinary arguments and a small documented built-in surface
work consistently with one implementation and preserve wrapping/readonly/native
contracts. Do not gate this on all numeric/collection conveniences.

## Sprint: text and recoverable collection primitives

**Objective:** make ordinary source capable of parsing and safely handling text
and data. Dependencies: chosen string indexing/error policy, minimal lookup/pop
errors and shared member exposure; no dependency on generic enums or `()` sugar.

Add narrow Runtime-backed scalar count/indexing and UTF-8 byte count/owned
conversion as approved. Source currently cannot inspect string contents except
equality. Reuse immutable UTF-8 storage, precise roots and checked failures. Define
index width/negative-index behavior, scalar versus byte meaning, range failures
and malformed-byte result semantics. No dangling borrowed data, UTF-16 second
string type, grapheme engine or premature per-string index cache.

Expose safe list/dictionary get and list pop through existing results/generics and
operations wherever possible; the audit demonstrates successful ordinary helper
implementations. Select one public error contract and preserve constness, negative
indices, fixed-child copies and shared list/dictionary identity. Decide fixed-array
family coverage without pretending `[T]` covers `[T,n]`. Avoid a new opcode for each
helper or new storage types. Value-remove/search needs explicit equality policy.

**Tests/acceptance:** empty/non-ASCII/multibyte/NUL strings, exact scalar/byte
counts, boundary and invalid UTF-8 conversion, independent owned bytes, forced GC,
normal/O2 JIT/AOT parity; empty/present/missing/negative collection lookup/pop,
readonly rejection, nested fixed/reference children and iterator invalidation.
Use current strings/arrays/dictionaries/advanced/runtime/source harnesses and test
the documented API through voidls. Benchmark repeated text traversal before any
optimization. An initial linear scalar scan is acceptable if documented.

Primary files: frontend strings/member paths, Runtime coordination/string ABI,
existing array/list/dictionary primitives and ordinary future library declarations.
Contracts: STRINGS, ARRAY_OPERATIONS, GROWABLE_ARRAYS, DICTIONARIES, ADVANCED_LANGUAGE.
**Exit:** enough text access and recoverable collection primitives for useful pure
modules; interpolation, format grammar, views, generic enums and custom hashing can
follow demonstrated demand.

## Sprint: optional-main application hosting and static native providers

**Sprint 4 is implemented and locally verified; Wrench review pending.** All three checkpoint decisions are
approved in [DECISIONS.md](DECISIONS.md). [Acceptance](stage8/SPRINT4_ACCEPTANCE.md)
records 444/444 final outcomes and both 1 MiB gates. The current
[implementation contract](STAGE8_APPLICATION_NATIVE.md) supersedes earlier research
recommendations, including implicit-zero fallthrough for explicitly typed main.

| Form/policy | Approved contract |
| --- | --- |
| Top-level statements | Selected source only; body-local variables; i32 return is status; fallthrough/bare return is 0. |
| `func main():` | Synchronous, no parameters; completion/bare return yields 0. |
| `func main() -> i32:` | Explicit i32 return on every reachable path; no implicit zero. |
| Eligible main plus executable top-level code | Competing-style compile error. |
| Legacy `entry(i32) -> i32` | Preserve numeric input, printed result, success status 0 and declaration-only precedence over a main helper. |
| Imported executable statements | Reject during compilation; no import-time execution. |
| CLI | Explicit `--app` for run/emit; no numeric operand or printed status; failure 1, invalid invocation 2; retain no-opt and Debug cache. |
| Static native admission | Explicit C++ descriptor checked against its own compiled catalog; independent disposable in-memory sink alongside original fixture. |

Reuse the current i32(i32) hosting adapter, typed IR, LLVM 19.1.7 JIT/AOT,
Runtime, exact-case discovery, GC roots, checked cleanup and generation leases.
Entry kind participates in cache/reload identity. Editors cannot execute source
or authorize native imports. Provider resource identities remain opaque, isolated
and bound to the acquisition thread. Returned/retained UTF-8 is owned; callbacks
quiesce before resource/root/lease retirement.

Focused tests precede consolidated local Debug/Release/strict sanitizer and 1 MiB
frontend acceptance. No hosted lock is implied. General argv, async main,
top-level await, arbitrary entry/export ABI, dynamic native loading, public FFI
DSL, raw pointers, broader callbacks and new worker privileges remain deferred.
One descriptor per graph is the bounded admission surface; this is not a native
package manager or full IO library. The historical research remains in
[the audit](LANGUAGE_COMPLETENESS_AUDIT.md).

## Sprint: long-running async resource readiness

**Objective:** determine and, if material, reduce retained completed-work memory
without weakening task/cleanup/root/generation safety. This is the strongest known
resource concern, not a reproduced collector corruption defect.

Measure a root performing increasing batches of awaited tasks with significant
frame/referenced data; separate live managed objects, rooted frames, task records,
Runtime metadata and process RSS. Existing `Scope::tasks` owns completed records
until root teardown, and repeated awaits are supported. Establish when result,
awaiters, cancellation/group state and generation leases can safely retire.

Engineering should release only what a concrete ownership proof permits. A task
handle/owner change needs Boss review if required; do not replace the scheduler,
GC or native owner model merely to reduce RSS. If measurement does not show a
material application problem, retain the documented limitation and defer changes.

**Tests/acceptance:** repeated/multiple awaits, success/error/cancel, nested groups,
defer/with, worker return, suspension during reload and forced GC remain correct.
Observable roots/frames decline at the approved safe point; active results/code
stay alive. Include shutdown/failure and sanitizer regression suites, plus the
same growth workload before/after. Do not use zero post-collection objects as a
proxy for a total process-memory bound.

Primary files: `src/runtime/async.cpp`, backend async activation/root lowering,
Runtime coordination/leases and existing async lifecycle/integrated tests.
Contract: ASYNC and managed/reload ownership contracts. **Exit:** a documented
measured bound or a clearly scoped retained limitation with evidence.

## Priority C and D work

After the actual prerequisites, choose a cohesive ergonomics sprint from approved
`has`, if-expression, multiline calls, richer literals, text interpolation/formatting,
multiple returns and custom iteration. Each needs its own semantics/test scope;
do not bundle all missing Bible syntax as a prerequisite for initial library work.
For `has`, decide supported families/order/equality and reuse contains/pattern
machinery. For custom iterators, approve protocol, mutation/cleanup/reverse behavior
before adding compiler hooks. Formatting algorithms should build on usable text
access, not precede it.

Safely defer absent a concrete application dependency: generic enums/unit sugar,
static mutable fields, interface inheritance/default methods/boxing, advanced OOP,
weak references/finalization, views/omitted slices, custom dictionary hashing,
grapheme indexing, general shared-memory/async FFI, dynamic packages/native loading,
registry install UI, incremental compilation, persistent metadata catalogs,
per-function/remote/shared caches, automatic native source publication and
lock-free reload. Archive integrity hardening and diagnostic stdout/stderr
consistency are useful focused maintenance, not evidence that LLVM must change.
Retain the unconfirmed historical LLD backtrace observation if it recurs.

## Decisions requiring Boss approval

The Boss approved the main sprint direction and the specific defaults recorded in
[LANGUAGE_COMPLETENESS_APPROVALS.md](LANGUAGE_COMPLETENESS_APPROVALS.md).
Other fine-grained research recommendations in this table remain proposals until
explicitly accepted. No feature below is implemented by approval alone.
Wrench reviews concrete architecture; Boss authorizes each engineering sprint.

| Decision | Recommended default | Consequence if unresolved |
| --- | --- | --- |
| Bare quoted project path relativity | Importer-relative, equivalent to `./`; preserve leading `/` for project root. | Nested imports and completion insertion cannot have a stable documented meaning. |
| Stdlib layout and keyword segments | Exact `<segments>.void`, no implicit index/mod lookup; allow lexical path segments without confusing them with value keywords, requiring alias where qualifier is invalid. | Resolver/discovery agreement and collision diagnostics remain ambiguous. |
| Library visibility/root constants | Stdlib is a separate logical library; internal stays there, private stays file-local, no project-root constants/privileges or reverse project dependencies. | New imports could silently change encapsulation and dependency identity. |
| Stdlib-root selection | One explicitly selected matching source root shared by compiler/server; deterministic startup/configuration error if unavailable when required. | Discovery and compilation could disagree about available modules/providers. This does not decide packaging/distribution. |
| Project discovery boundary/lifecycle | Explicit file wins; nearest main.void within supplied boundary; labeled standalone editor fallback; fixed selection until deliberate reselection. One frontend helper exposed through voidls preflight. | Nested projects, root attachment and named unsaved main handling otherwise diverge. No recursive search for main declarations. |
| Entry coexistence/compatibility | Diagnose competing top-level/main styles; retain existing declaration-only legacy entry+main selection. Never run imports or both bodies automatically. | C# precedence is not automatically VOID's rule; working legacy helpers need preservation. |
| Default-zero scope and top-level locals | **Boss-approved correction:** top-level successful fallthrough and no-result `func main():` exit 0. `func main() -> i32:` must explicitly return an `i32` on every reachable path, like an ordinary typed function. Program vars are entry locals; imported executable bodies diagnose. The exact Sprint 4 return/CLI/provider policies are approved in DECISIONS.md. | Preserve current typed-function return checking; no implicit zero for a typed `main()`. |
| Core built-in provider admission | **APPROVED:** compiler-owned numeric prelude; no imports or user primitive extensions; four members only. | See Sprint 2 contract; no owner choice remains for this sprint. |
| Numeric properties and edges | **APPROVED:** exact integer/finite float limits; checked signed-min abs and invalid bounds; bounds validated before NaN propagation; signed-zero/infinity rules in DECISIONS.md. | See Sprint 2 contract and actual verification. |
| Ordinary arguments/defaults | **APPROVED:** shared initializer binder, pure compile-time defaults and concrete-default annotation inference; explicit generic parameter annotations. | See Sprint 2 contract and actual verification. |
| Field encapsulation | Approve bounded private/readonly declaration semantics before requiring protected public-wrapper invariants. | Public fields cannot be advertised as protected by existing function privacy/const views. |
| Recoverable collection/text API | **Approved 20 October:** scalar/i64 counts, one-scalar indexing with negative indexes/checked failure, owned u8 conversion and string-error get/pop results, including fixed-array get. | [Sprint 3 contract](STAGE8_TEXT_COLLECTIONS.md); local implementation acceptance remains separate. |
| Native/application scope | Requested synchronous top-level/main forms through existing native invocation, separately from trusted static provider generalization. Preserve legacy output/status. | IO remains fixture-bound, but simple application entry need not wait for a general FFI/linker driver. |
| Async retirement changes | Measure first; approve handle/lifetime changes only if reclamation needs them. | Cannot promise bounded long-running root memory solely from the current collector. |

Completion result caps, internal data layout and small helper factoring are routine
engineering choices within the approved contracts. They do not justify stopping
for approval repeatedly. Unquoted/quoted domains, nesting, API/path IntelliSense,
optional main/top-level execution, successful default exit 0, main.void discovery
and explicit-file/legacy compatibility are already owner requirements.

## Shared acceptance and handoff

Keep LLVM 19.1.7, one frontend/binder, immutable owned snapshots, shared typed IR,
common JIT/AOT lowering, source identities, UTF-16, diagnostics, precise roots,
readonly/copy semantics, cleanup and native/task leases. Extend existing tests at
the changed boundaries; run full existing local regression/strict sanitizers for
the eventual compiler/runtime sprint and the applicable 1 MiB stress gate.
Owner-controlled hosted acceptance follows Wrench review; research does not trigger
it. Expand testing only for new changes, failures or unresolved risks.

Every implemented sprint should leave updated feature contracts, permanent focused
regressions and before/after evidence for any claimed optimization. Update historic
status summaries with dated cross-references rather than erasing prior evidence.
No website changes, licensing/publication policy, new compiler framework or
unsolicited library implementation belongs in this assignment.

Research delivery verification: Release compiler/server rebuild succeeded;
28/28 selected existing suites passed; 51 source probes matched their expected
acceptance/rejection outcomes, with normal/O2 behavior checked for eleven programs.
Four import-completion scenarios confirmed the missing/contextually incorrect
behavior. No production fix was applied; no new full-matrix result is claimed.

Entry follow-up: 15 source scenarios checked and run in normal/O2 modes, plus
three startup/discovery checks, confirmed current behavior (48 invocations).
Eight existing frontend/module/CLI/analysis/session/LSP suites passed, 15.56 seconds.
This follow-up did not rebuild or alter production code and is not a new full
acceptance run. Detailed source/C# evidence and proposed rules are in the audit.

## Historical research executive summary for Boss and Wrench

The dated current status and approved Sprint 4 contract above supersede the
initial gap list and sequencing recommendations below.

1. **Already in good shape:** LLVM/native architecture, precise GC roots,
   value/reference collections, bounded interfaces/generics, errors/closures/
   resources/async, and one shared Stage 7 language service.
2. **Biggest gaps:** shared project/stdlib discovery and source docs, optional-main
   application entry/exit semantics, ordinary argument
   semantics, built-in member/property admission, usable text operations,
   encapsulation and real native/application admission.
3. **Known issues that matter:** task/frame retention under long-lived roots,
   preserved native/readonly/lifetime boundaries, and new domain/overlay identity
   risks. Several alarming historical exclusions are already superseded.
4. **Implement first:** the cohesive nested-module, shared main.void discovery and
   import IntelliSense sprint, covering both stdlib and project VFS with compiler-owned metadata, valid edits,
   documented APIs, navigation and real-editor acceptance. Optional-main execution
   has its own bounded host work and can advance independently of native providers.
   Settle path/root/selection and entry coexistence/default-zero rules before their
   engineering authorization.
5. **Can wait until the library starts:** speculative language expansions,
   general package/dynamic FFI systems and unmeasured compiler/GC optimizations.
   Proceed from a small reviewed core after actual prerequisites; the entire Bible
   does not need to be implemented first.
