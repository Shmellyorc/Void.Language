# Validated analysis and source compatibility (Phase 7)

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

The user reports Phase 6 commit `9b9fdac` passed Actions run #9 on Ubuntu,
Windows and macOS. Phase 7 extends that foundation locally, without changing
language syntax, numeric semantics, LLVM 19.1.7 or the SDK RTTI correction.

## Successful result API

`include/void/frontend.hpp` exposes `analyze_source(SourceFile)` and
`analyze_project(entry_path)`. `AnalysisResult::succeeded()` is explicit: a
successful result contains `shared_ptr<const ValidatedAnalysis>`; failure contains
structured diagnostics and a null program. Failed graphs never expose usable
partial declarations or compatibility tokens. Internal/compiler failures are not
relabeled ordinary source errors. No warnings currently exist.

The existing parser, signature registration, binder, constant dependency walk and
semantic checker run once for the complete graph. Only after the error gate does
the factory build a successful result. Analysis neither lowers VOID IR nor invokes
LLVM code generation. The numeric software kernel retains its private LLVM Support
dependency; analysis is not a promise of an LLVM-free compiler binary.

Read-only accessors expose owned records:

- `modules()`: canonical VFS identity, immutable SourceFile and sorted, unique
  direct import targets.
- `declarations()`: stable identity, kind, module, containing owner, visibility,
  declaring span, ordered parameter types, result/value type, canonical constant
  tokens and evaluated module-constant bit patterns. Local constants are runtime
  readonly values and have no precomputed value.
- `references()`: resolved source-owner/target identities, written spelling and
  owning source span. Calls, module/root/import constant uses and local readonly
  references are retained even after module constants become typed AST literals.
- `entry_identity()`, `constant_contract()`, `compatibility_contract()`.
- `lower()`: lower the retained validated tree into fresh backend-independent IR,
  without reading disk or repeating analysis.

`compile_source` and `compile_project` are adapters over this pipeline and retain
their previous result types and structured exception behavior. The CLI is unchanged.
A successful analysis can describe libraries without an entry; native host execution
still requires exactly `i32(i32)`, not another type with the same storage width.

```cpp
auto result = vd::analyze_project("examples/analysis/main.void");
if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
auto validated = result.program; // May outlive all source files.
vd::Jit jit;
vd::ReloadSlot slot(jit.compile(*validated));
// After a deliberate source edit, analyze a complete fresh graph:
auto candidate = vd::analyze_project("examples/analysis/main.void");
if (candidate.succeeded()) {
    bool published = jit.replace(slot, *candidate.program);
    // false means manual restart is required; current code stays active.
}
```

## Identities, dependencies and ownership

Modules use existing project-root-relative exact-case VFS filenames. Functions and
module constants use `module::name`. Local readonly constants use
`function_identity::const::name::occurrence`, counting same-named declarations in
lexical traversal order, not physical offsets or unrelated variable declarations.
Their explicit owner is the containing function; other declarations belong to
their module. Same-name constants in disjoint scopes have distinct occurrences.
Identities are project-scoped, stable under equivalent physical project relocation.
In-memory `analyze_source` uses its supplied SourceFile path identity, as before;
use the project API for relocatable VFS identities.

References preserve direct edges, including function-only cycles, aliases resolved
to actual owners, imported/project-root constants and transitive constant chains.
For affected-declaration traversal, reverse reference edges, include the containing
function of affected local constants, and then reverse imports for affected modules.
There is no recursive expansion that assumes the dependency graph is acyclic.
The API supplies direct information; it does not implement incremental invalidation,
partial analysis or a build cache.

Successful results retain the complete typed graph through private owned storage.
SourceFile copies share immutable bytes/line tables. Public metadata contains no
borrowed AST, token or declaration pointer. Spans own their snapshots; IR and native
compatibility records retain the successful analysis. Editing/deleting files cannot
change old records, later lowering, runtime source excerpts or leased native code.
Holding a result deliberately retains its complete graph until the last owner leaves.

## Exact compatibility classification

The comparison data uses exact tagged, length-delimited records; no hash equality
can authorize publication. It contains every reachable module, canonical direct
import targets, all function and constant identities/kinds/types/visibility, exact
constant declaration tokens, and ordered resolved reference owner/target pairs.
Physical paths, line/column offsets, comments and trivia are excluded. Explicit
public and default-public have the same visibility. Internal and public remain
distinct even though both are currently accessible within the project.

Constant tokens are captured from the existing parser token stream before semantic
rewriting. They cover name, optional annotation and initializer, including operator,
literal spelling and parentheses; comments and spacing are absent. Changing `1`
to `0 + 1` therefore requires restart despite an equal evaluated value. Module,
local runtime readonly, imported, root and unused constants all participate.
Resolved references separately detect changed ownership when unchanged spellings
bind differently. Import spellings normalize to declaring VFS targets; unused alias
renaming or duplicate normalized imports do not change ownership.

Supported compatible transitions:

- Arithmetic/value/control-flow body edits preserving declarations, constants and
  the ordered dependency bindings.
- Comments, blank lines and token-equivalent spacing, including constant trivia.
- Changed physical source locations/checkout roots under the same VFS graph.
- Alias or normalized path spelling changes that preserve actual module targets
  and resolved symbol owners (without editing constant declaration tokens).

Manual restart is required for:

- Any added/removed/renamed constant, changed annotation/initializer tokens or
  changed resolved constant dependency, including local and transitive imports.
- Added/removed/renamed functions, changed parameter/result types or visibility.
- Module ownership, reachable graph/direct import target or resolved call target
  changes.
- Host layout, capture or other legacy host compatibility-contract changes.

Conservative limits are deliberate: declaration ordering and reference traversal
ordering/multiplicity participate. Reordering declarations or adding/removing calls
can require restart even when a whole-module generation could theoretically be
safe. Moving same-named local constants can change occurrence identities. There
is no signature compatibility based on LLVM widths, inferred layout migration or
finite-hash shortcut. Future refinement must prove safety and preserve constant
restart semantics.

## Native generation publication

The convenience native API selects the project entry; other host entry selections
remain on the trusted legacy IR interface.

`Jit::compile(ValidatedAnalysis, ...)` lowers the owned tree through the existing
shared LLVM implementation. `Jit::replace(slot, analysis, ...)` compares the complete
source contract plus host layout/capture contracts **before generating code**.
It returns false on incompatibility. On success it generates one whole dependent
module and `ReloadSlot::replace` rechecks compatibility while holding its publication
mutex. Concurrent competing publishers cannot bypass the slot's compatibility gate.

The legacy IR compile API remains available for architectural proofs. Compiler-owned
IR carries its analysis, and JIT compilation overwrites caller-supplied source
contracts and analysis metadata from that compiler result. Host-provided layout,
capture and legacy constants contracts remain additional requirements. The IR API
is a trusted compiler/development interface, not a security boundary against a host
manually constructing native code or altering IR; source hosts should use validated
analysis candidates and the preflight replacement API.

An invalid candidate has no validated program. An incompatible candidate leaves
the current pointer and leases untouched. Compatible publication changes only
future acquisitions; existing calls/leases retain the old resource tracker and
source graph. Destruction/ORC retirement remains outside the slot mutex. Runtime
failure propagation, per-call storage and original source excerpts are unchanged.
This proves one-slot replacement of a complete dependent module, not atomic
publication across unrelated slots. No watcher, background compiler, automatic
restart, capture migration or suspended-task system is added.

## Demonstration and verification

`examples/analysis/` is a real three-file source project with root, imported module
and local readonly constants. At input 10 it returns 53. Editing only
`return stable + 1` to `return stable + 9` publishes a compatible generation returning
61; the old lease returns 53. Editing imported `base` from 40 to 41 is incompatible
and cannot publish. Fresh-process compilation of that edited project returns 62.

`analysis` exercises these transitions in normal/O2 JIT, metadata after relocation,
source deletion, cycles, rebinding, visibility/signatures, all ten numeric types,
concurrent callers, failed publication, host contracts, retained runtime failures
and 480 malformed-source recovery cases. `analysis_cli` checks the same real project
before/after body and constant edits in normal/O2 JIT and linked AOT. All twelve
preceding suites remain, making fourteen. WORK_LOG.md records actual local SDK,
build, sanitizer and stress results; hosted Phase 7 results await the user's push.

Next: build a small in-process tooling consumer around these validated records and
explicit invalidation planning before choosing incremental compilation or LSP
transport. Managed types still require the separate GC/thread-root integration gate.

## Phase 8 tooling consumer

The user reports Phase 7 passed hosted run #10 at commit `04768bf`.
[TOOLING_FOUNDATIONS.md](TOOLING_FOUNDATIONS.md) documents the read-only symbol
index and invalidation planner consuming these records. Exact compatibility,
source ownership and complete validation gates above remain authoritative.


## Phase 9 revision integration

[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) adds immutable unsaved project overlays
through the shared loader and an explicit revision-safe session acceptance boundary.
Its diagnostic/query wrappers retain the analyzed project revision. Existing disk
APIs, complete-validation gates, byte spans and exact native contracts remain.

## Phase 12 shared value types

Successful analysis now also exposes value_structure and value_field declarations
with owned nominal ir::Type descriptors. Aggregate module constants retain exact
syntax and types; the legacy optional 64-bit evaluated value is scalar-only.
Type annotations, constructors, named fields and field access contribute references
between owned declarations, including qualified type-name spans. The existing
index/session/LSP uses these records directly. Compatibility includes nominal
identities, ordered field types/names and declared default syntax; no independently
approximated aggregate compatibility policy is added to tools. See VALUE_STRUCTS.md
for implementation boundaries, limits and native verification.

## Phase 13 receiver contracts

AnalyzedDeclaration appends receiver_method without renumbering earlier kinds.
Records own the receiver Type, inferred receiver_mutates flag and containing nominal
type identity; parameters remain the explicit language parameters. Method identities
are `type_identity::method::name`. Receiver annotations and call/field spans join
the existing reference graph. Method metadata does not masquerade as constant syntax.

Exact compatibility serializes receiver kind/type and effect alongside existing
signature/visibility/binding/layout/default data. Compatible method body edits can
reload; changing inferred mutation, including through a recursive/transitive callee,
requires restart. Tooling compares these fields and retains containment edges.
Invalid init/method source never publishes analysis or code. See RECEIVER_METHODS.md.


## Phase 14 custom value initialization

Phase 14 adds initializer declaration kind, parameter/default source metadata and exact
initialization policy syntax. Init presence/identity/access/parameter
labels/types/defaults/effects and bodies require restart when changed. Transitive
initializer helper bodies are frozen by iterative call-graph closure, including cycles.
Implicit construction edges are retained for invalidation but excluded from written
reference/navigation positions. Compatible unrelated bodies retain generation
replacement and old-lease safety.

## Phase 17 construction contexts

Managed initializer declarations retain construction-parameter masks and the names
of direct required reference fields unavailable at body entry. A successful analysis
proves them ready on every normal exit. Private typed-AST contexts retain aliases and
resolved compatible calls; context identity is declaration plus passing mask, bounded
at 1024/project. The IR independently checks these origins and readiness.

Shared declaration metadata/compatibility and tooling invalidation compare these
facts alongside P3 effects and C1 initialization closure/default policy. Synthetic
helper variants preserve source declaration/navigation identities. Invalid unsaved
required-field source suppresses stale queries; repair restores them. Init/helper
body changes require restart while compatible unrelated bodies may reload. No
separate editor checker or managed constructor ABI is introduced.
