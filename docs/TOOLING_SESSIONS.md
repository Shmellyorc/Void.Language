# Document revisions and tooling sessions (Phase 9)

**Stage 8 Sprint 1 (20 October 2026):** [STAGE8_DISCOVERY.md](STAGE8_DISCOVERY.md)
supersedes the historical import/discovery exclusions below. It specifies separate
standard/project source domains, importer-relative bare quoted imports, bounded
shared `main.void` selection, metadata completion, source docs and readonly standard
navigation. [Local acceptance](stage8/ACCEPTANCE.md) records actual verification;
no hosted lock is implied. Existing snapshot, UTF-16, rename and native safety
contracts remain in force.


Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Phase 8 commit `f25b268` passed hosted run #11 on Linux, Windows and macOS,
according to the user. Phase 9 adds local in-process document orchestration;
no language syntax, native publication mechanism or LLVM version changes.

## Public API

`include/void/tooling.hpp` provides `ToolingSession`, `ProjectSnapshot` and
`SessionAnalysis`. Sessions are single-threaded and noncopyable. Complete analysis
is synchronous; selection, execution and acceptance are separate operations:

```cpp
vd::ToolingSession session("examples/analysis/main.void");
auto selected = session.select(); // Revision 1, immutable selection handle.
auto result = vd::ToolingSession::analyze(selected);
if (session.accept(result) && result->result().succeeded()) {
    auto index = session.current_index();
    auto symbol = index->declaration("/worker.void::calculate");
}
session.open("/worker.void", 1, new_worker_text);
// current_index() is now null. Last successful analysis is historical.
auto pending = session.select();
session.replace("/worker.void", 2, newer_worker_text);
auto delayed = vd::ToolingSession::analyze(pending);
assert(!session.accept(delayed)); // Cannot publish an older selection.
auto fresh = vd::ToolingSession::analyze(session.select());
assert(session.accept(fresh));
```

`select()` identifies the current project revision. A retained selection identifies
what a caller has chosen for analysis; its document overlays cannot change during
later edits. `SessionAnalysis::snapshot()` identifies what was actually analyzed.
There is no hidden worker or single global in-flight slot: callers can retain
several selected handles and execute them in any order. Acceptance compares exact
owned identity tokens, including session identity. Displayed numeric project
revision alone is never the acceptance key. Equal bytes at different revisions
remain different selections. Tokens remain alive with historical snapshots, so
allocator reuse cannot make an outstanding old token equal to a new token.

Project revisions start at 1 and advance for every accepted document transition
or explicit `refresh()`. Document revisions are client-supplied nonzero uint64
numbers, strictly increasing independently for each exact VFS identity. Duplicate,
older, zero and out-of-order revisions throw `invalid_argument`, without changing
the session. Numbers may skip; exhaustion rejects rather than wrapping. Closed
identities retain their last document revision, including across reopen.

- `open(id, revision, text)`: create an editor override; reject already open or
  unavailable identities. A source file need not exist on disk.
- `replace(id, revision, text)`: whole-text replacement of an open/unavailable
  override; reject a closed or never-opened identity.
- `unavailable(id, revision)`: install a tombstone (deleted or unreadable editor
  source), including for a previously disk-backed identity. Never fall back to
  the old disk bytes. Replace can restore it with a newer revision.
- `close(id, revision)`: explicitly remove an override/tombstone and return to disk
  on the next analysis. Reject identities without an override. Closing does not
  save editor text or delete a disk file. Outstanding snapshots retain that text.
- `refresh()`: advance the project selection for caller-observed filesystem
  changes, conservatively marking all modules in the last successful graph.

`ProjectSnapshot::documents()` retains document revision identities (including
closed documents). `overrides()` distinguishes text, tombstone and absence/closed
state. Both accessors are read-only. There is no timestamp/hash-based equality.

## Overlay loading and filesystem boundary

`analyze_project(entry, SourceOverrides)` extends the existing SourceGraph loader.
Overrides use exact canonical `/folder/file.void` VFS keys; `optional<SourceFile>`
is owned text or a tombstone. `require_module_identity` validates keys using the
same import normalizer and portable-component rules, rejecting noncanonical keys
rather than silently merging aliases. Ordinary source import spellings still
normalize `.`/`..` and repeated internal separators as before. The session fixes the absolute entry path at construction so changing the host
working directory cannot retarget a selected project. The entry parent
remains the physical project root and must exist, even for a new virtual entry.

The loader merges virtual and disk directory children in one exact-case namespace.
New virtual parent directories need not exist. Case-only collisions, wrong-case
imports, virtual file/directory collisions, root escapes, OS/drive/network paths, reserved components, existing
symlinks/junction redirects, non-file host objects and hard-link aliases retain
rejection. Tombstones never read their old disk file. Reachable modules alone
participate in analysis; cycles and deduplication use the existing graph loader.
There is one parser, binder, type checker and validated-analysis factory. Original
`analyze_project(entry)`, `compile_project`, CLI, native AOT and ORC paths retain
their established behavior. The overlay overload generates no native code.

A selected session freezes **all in-memory overrides**, not an atomic OS filesystem
image. Disk fallbacks are read when `analyze(selected)` executes. Before semantic
validation accepts that graph, the overlay overload rediscovers the graph through
the same loader and compares exact source bytes and reachable identities. Detected
changes fail with a structured source/import diagnostic and a retry hint. Path
checks run again. This is deliberately conservative and performs extra parsing;
it is not incremental compilation or an independent resolver.

Sequential rereads cannot exclude an ABA rewrite, malicious race, or a change after
the final check. This resolver is not a filesystem security sandbox. Clients must
keep fallback files stable during analysis, open overlays for editor-owned source,
and call `refresh()` for observed disk changes before accepting or querying results.
There is no watcher; “current” certifies the session's **logical** revision, not
undetected later disk mutations. Selecting the same logical revision twice does
not assert that mutable disk bytes were frozen at selection time. Once produced,
analysis records, ASTs and diagnostics own their captured bytes permanently.

## Fresh queries, diagnostics and history

`current_analysis()` returns only the accepted current selection, including its
failure diagnostics. `current_index()` returns an index only for a successful
accepted current selection. Before analysis/acceptance, after an edit, and after
failed validation, it returns null. Callers must obtain this handle for ordinary
current definition/reference queries. A previously held index is explicitly
historical; it remains safe to inspect but never silently becomes current again.

`latest_accepted()` exposes the last accepted success **or failure**, potentially
historical after another edit. `last_success()` separately retains the last
successful revision. A failure never advances last success. Old and duplicate
results return false at acceptance without altering either state or the plan.
Foreign-session results are rejected even if revision numbers match.

A SessionAnalysis owns its ProjectSnapshot, structured AnalysisResult and successful
ToolingIndex. Diagnostics carry their session revision through this wrapper, plus
existing source-owning primary/related spans. Byte offsets, UTF-8 scalar columns,
CRLF bytes and source lines are retained; clients must never apply a historical
span to newer document text. Results and copied records survive session destruction,
close, replacement and deletion. No partial invalid AST or mutable source buffer
is exposed. Existing ToolingIndex category/offset limitations still apply.

## Dependency and compatibility orchestration

`invalidation()` delegates to Phase 8 `plan_invalidation`. Pending edits seed the
last successful graph with changed identities; an unknown new graph always needs
complete discovery and validation. An explicit disk refresh seeds all known
modules. Failed acceptance retains the previous successful baseline and unknown
compatibility. Successful acceptance computes the refined plan against that
baseline **before** advancing last success, preserving graph additions/removals,
old/new dependency union, declaration/binding changes and exact compiler-derived
source compatibility. The successful plan remains available until another edit.
Without a successful baseline classification remains unknown. Changed but
unreachable override hints remain conservative unknowns per the existing planner.

Stale results cannot update a session plan. There is no safe-to-publish boolean:
`exact_match` describes source contracts only. Native host ABI/layout/capture and
publication remain Jit/ReloadSlot responsibilities. Nothing in this API publishes
code or restarts an application. Existing native proofs retain 53 -> 61, old lease
53, and constant-edit restart behavior.

## Tests, demonstration and next phase

`void_sessions_example examples/analysis` uses the actual three-file project,
without editing disk. It queries revision 1, edits an unsaved body, demonstrates
staleness, reanalyzes, reports affected importers, prints invalid-edit diagnostics,
repairs and distinguishes constant restart from compatible body edits. It links
only void_core, without the native runtime/code-generation backend. CTest
`sessions_demo` runs this consumer; `sessions` tests real filesystem projects,
virtual entry/imports, multiple edits, deletion/close, exact-case cycles, byte
locations, delayed execution/acceptance, duplicate revisions, owned history,
signature/binding/constant contracts, dependency queries and repeated transitions.
All sixteen previous suites/assertions remain, for eighteen total.

WORK_LOG.md records actual six-configuration verification and sanitizer repeats.
Hosted Phase 9 verification requires the user's push and another Linux/Windows/macOS
run. No remote mutation or commit is performed. Next consider an explicit protocol
position adapter and minimal client transport consuming this API, with separate
protocol tests, before optimizing partial analysis. LSP/JSON-RPC, extensions,
watchers, workers, persistent caches, partial AST reuse, incremental LLVM generation,
managed types and automatic native publication remain outside this phase.
