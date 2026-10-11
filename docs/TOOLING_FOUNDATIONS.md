# In-process tooling and invalidation planning (Phase 8)

The user reports Phase 7 commit `04768bf` passed Actions run #10 on Linux,
Windows and macOS. This phase consumes its existing successful analysis records;
it changes neither language semantics nor exact native reload compatibility.

## Read-only symbol queries

`include/void/tooling.hpp` exposes `ToolingIndex`, constructed with a non-null
`shared_ptr<const ValidatedAnalysis>`. A null result from failed analysis is rejected.
The index stores stable identity-to-declaration, module-to-declarations,
target-to-references and module-to-reference-position maps. Construction does not
read files, parse, type-check, lower IR or generate native code.

```cpp
auto analysis = vd::analyze_project("examples/analysis/main.void");
if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
vd::ToolingIndex index(analysis.program);
auto symbol = index.declaration("/worker.void::calculate");
auto callers = index.references_to("/worker.void::calculate");
auto definitions = index.declarations_in("/worker.void");
auto target = index.definition_at("/main.void", byte_offset);
```

Queries return optional declaration records or vectors of records, never formatted
console strings. Declaration records already contain type, ordered parameter types,
result type, visibility, module/owner identities and SourceSpan. Module constants
also contain their evaluated typed bit pattern; local readonly values remain runtime
values. The declaring module is the declaration's canonical `module` field, regardless
of the caller's alias or implicit project-root lookup.

Supported categories are functions, module/project-root constants and local readonly
constants. Reference navigation uses only references the compiler actually resolved.
It includes forward references and cycles. Parameters, mutable locals, import
qualifier tokens, declaration-site navigation, arbitrary expressions, members and
completion are not synthesized. Calling `definition_at` on these unsupported
positions returns absence. The qualified **symbol name** is navigable; an alias
prefix itself is not a collected symbol reference.

Module arguments are exact canonical VFS identities, not host paths or aliases.
For standalone in-memory analysis, use its supplied source identity, as in Phase 7.
Offsets are zero-based **UTF-8 bytes** within the retained snapshot, using existing
half-open `[begin,end)` SourceSpan ranges. Name interiors resolve; the exclusive end,
punctuation, EOF and unknown identities return absence. No UTF-16/scalar conversion
occurs. Future IDE clients must convert protocol positions using the snapshot they
are querying, not the latest contents of disk.

Declarations in a module sort by stable identity. Reference query results sort by
VFS module, byte begin/end, owner/target and spelling. Position lookup uses the
per-module sorted reference list. Equivalent physical relocations preserve keys and
query ordering; physical diagnostic source paths appropriately change.

## Ownership

The index retains its successful analysis. Query records are **copies**, and their
SourceSpans retain shared immutable SourceFiles. Returned records can therefore
outlive the index, original analysis, source files and subsequent analyses/native
publications. The private analysis continues to own the typed graph; tooling never
borrows an AST, token or declaration pointer. Plans contain owning strings, vectors
and value flags only. Temporary internal graph maps borrow records solely during
the planning call. There is no global mutable registry.

## Invalidation plans

`plan_invalidation(before, changed_vfs_modules, after = nullptr)` does no disk I/O,
compilation or publication. Inputs are one successful old snapshot and optional
successful comparison snapshot. Failed/missing analysis supplies no `after`.
The return type `InvalidationPlan` separates:

- `directly_changed_modules`: sorted unique hints plus detected byte-snapshot
  differences and added/removed reachable modules when comparing validated graphs.
- `affected_declarations`: declarations in those directly changed modules,
  including both old/new records where present.
- `dependent_declarations`: transitive potential dependents, excluding the direct
  seeds, through actual reference edges, local-constant containment and conservative
  namespace effects.
- `dependent_modules`: reverse direct-import closure plus modules owning dependent
  declarations, excluding directly changed modules.
- `added_modules`, `removed_modules`: confirmed differences between the reachable
  validated graphs. A file can leave that graph without being deleted on disk.
  A rename appears as removal/addition, never an invented identity migration.
- `unknown_modules`: hints absent from the available graph(s); they cannot be
  asserted to be new valid modules or ignored as unrelated.
- `changed_declarations`: confirmed kind, owner, type/signature, visibility, constant
  syntax or constant-value differences, including additions/removals. Locations
  alone are excluded. This is a useful metadata summary, not a complete AST diff.
- `changed_bindings`: owners whose ordered resolved target sequences differ.
- `graph_changed`: confirmed module/direct-import/entry graph difference with two
  successful analyses.
- `requires_graph_discovery`, `requires_full_reanalysis`: unresolved work when the
  new graph is unknown. Both remain true without a comparison analysis, even when
  hints happen to be empty; an old graph cannot certify current source.
- `has_validated_comparison`: a complete successful second snapshot was supplied
  and no hint lacks a representation in either graph.
- `source_compatibility`: `unknown`, `exact_match` or `restart_required`, using the
  existing exact compiler contract only when validated comparison is available.

With a complete validated comparison, discovery/validation has already happened,
so the two work-required flags are false. Confirmed graph changes remain visible
in `graph_changed`. An unrepresented hint restores unknown status and requires
broader discovery/full validation. Known hints remain conservative direct seeds
even if their supplied snapshot bytes compare equal.

These statements concern **the supplied immutable snapshots**. The planner cannot
certify that an `after` snapshot represents edits made later, or that caller-provided
edit hints include every current filesystem change. Clients must run complete
project analysis for the candidate revision before considering native publication.
No API here authorizes publication from stale or failed analysis.

### Dependency traversal and conservative scope

Build reverse edges from direct imports and resolved symbol references. Add an edge
from each local readonly constant to its containing function: its initializer executes
even if the binding is never subsequently read. Traverse iteratively, visiting each
identity at most once. Function-only cycles terminate without recursion or repeated
unbounded expansion. With two graphs, traverse their **union**, preserving old
callers of deleted/rebound declarations as well as newly validated dependents.

An unknown new graph can introduce exports, imports or resolutions absent from the
old reference graph. Changes therefore invalidate declarations in reverse importers,
in addition to known reference dependents. Changes to the selected root can affect
implicit project-wide constant lookup, so unknown root edits conservatively include
the whole known graph. Unknown module hints likewise include the whole known graph
because their future importers cannot be inferred. Full graph discovery is still
required to discover identities absent from the old analysis.

With two validated snapshots, confirmed declaration/import changes retain those
namespace precautions. Body/trivia-only changes can use known reference dependents
without invalidating unrelated module declarations. Physical path changes alone do
not count as source edits. Because Phase 7 records do not include function-body
extents/fingerprints, a changed file seeds **all** its declarations; this phase does
not claim to locate the one edited function or prove a comment-only delta from raw
source text. Metadata, bindings and the exact compatibility contract provide the
supported refinement. No second parser or text-based symbol scanner is added.

## Exact native reload remains authoritative

Source classification compares `ValidatedAnalysis::compatibility_contract()`
verbatim. Metadata summaries and potential-dependency sets are never substitutes
for that comparison. No hash, inferred equal storage width or shortened tooling
signature can authorize native publication. Source `exact_match` excludes additional
host entry, layout, capture and legacy compatibility contracts. Those remain checked
by `Jit` and `ReloadSlot`; the tooling API cannot change an active slot.

Phase 7's constant/signature/visibility/binding/import restart conditions and its
conservative declaration/reference-order restrictions remain unchanged. Native tests
still prove a body edit publishes **53 -> 61**, the old lease remains **53**, and
actual constant changes cannot publish. This planner neither relaxes that rule nor
introduces a native transaction across unrelated slots.

## Executable in-process demonstration

`examples/tooling.cpp` is built as `void_tooling_example` during testing, linked only
to `void_core`, with no LLVM code-generation backend or runtime publication API.
It analyzes the real three-file analysis example, finds `/worker.void::calculate`,
retrieves its declaration and one resolved reference, deliberately edits the copied
source, plans invalidation, reanalyzes, and reports the exact body/constant contract
results. It uses compiler records, not a regex-based symbol scanner.

The demonstration deliberately edits its argument directory. Run it against a
**disposable copy** of `examples/analysis`, as CTest does:

```sh
ctest --test-dir build/ci-debug -R '^tooling_demo$' -V
```

Expected actual output:

```text
/worker.void::calculate declared in /worker.void; references: 1
edit plan: full validation required; fresh body source contract: exact match
fresh constant source contract: restart required; no native generation modified
```

`tooling` tests real projects, UTF-8 byte offsets, source movement, ownership,
forward/cyclic calls, root/local constants, unknown symbols, graph changes,
invalid source, relocation, query/plan determinism, and the real JIT lease/reload
proof. `tooling_demo` runs the standalone consumer. All fourteen prior suites remain,
for sixteen total. WORK_LOG.md records actual six-configuration local validation
and repeated sanitizer execution. Hosted Phase 8 verification awaits the user's push.

Next: choose explicit document/project revision handling and client invalidation
orchestration before introducing partial analysis or LSP position adapters. A future
LSP can consume these records directly; an incremental compiler must still discover
new dependencies and validate complete candidate graphs. This phase adds no watcher,
JSON-RPC, extension, persistent cache, partial pipeline or background compiler.


## Phase 9 revision integration

[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) adds immutable unsaved project overlays
through the shared loader and an explicit revision-safe session acceptance boundary.
Its diagnostic/query wrappers retain the analyzed project revision. Existing disk
APIs, complete-validation gates, byte spans and exact native contracts remain.
