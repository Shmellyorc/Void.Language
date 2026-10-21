# Module system and multi-file native compilation

**Stage 8 Sprint 1 (20 October 2026):** [STAGE8_DISCOVERY.md](STAGE8_DISCOVERY.md)
supersedes the historical import/discovery exclusions below. It specifies separate
standard/project source domains, importer-relative bare quoted imports, bounded
shared `main.void` selection, metadata completion, source docs and readonly standard
navigation. [Local acceptance](stage8/ACCEPTANCE.md) records actual verification;
no hosted lock is implied. Existing snapshot, UTF-16, rename and native safety
contracts remain in force.


Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Implemented locally on 10 October 2026. The user confirmed the preceding source
compiler passed Linux, Windows and macOS Actions. The user reports this ten-suite
module phase passed all three hosted platforms. The user also reports Phase 4 value semantics (commit `d78b6b8`, run #6) passed
all three platforms. Phase 5 recovery requires another hosted run. The Language Bible remains unchanged and authoritative; policies
marked provisional below settle this implementation slice, not the full language.

## Project-local library extension — 21 October 2026

[LOCAL_LIBRARIES.md](LOCAL_LIBRARIES.md) specifies implemented `use lib/...`:
`lib/raylib` selects `/libs/raylib/raylib.void`; `lib/raylib/graphics` selects
`/libs/raylib/graphics.void`. Both are ordinary project-owned VFS identities,
shared with quoted imports. Installed standard modules cannot use this prefix.
The shared resolver and metadata assistance preserve exact-case/alias/security,
overlay, snapshot, cache and reload rules. Native privileges remain explicit.
[Focused acceptance](local-libraries/ACCEPTANCE.md) is local evidence only.
The dated no-discovery/Unicode exclusions below are historical; current shared
selection and Unicode physical-root behavior follow STAGE8_DISCOVERY.md and the
later CI repair records.

## Working imports

```python
# A module at /feature/worker.void, imported by the project entry.
use "/math/calculator.void" as calculator
use "./helper.void"
use "../shared/common.void"

func calculate(value: i32) -> i32:
    return calculator.double(value) + helper.increment(value)
```

The relative examples require files at those locations within the project root.
See `examples/modules/main.void` for a complete runnable three-file program:

```sh
build/debug/voidc check examples/modules/main.void
build/debug/voidc run examples/modules/main.void 10
# 31
build/debug/voidc emit examples/modules/main.void -o build/debug/modules.o
clang-19 build/debug/modules.o -o build/debug/modules
build/debug/modules 10
# 31
```

Select matching LLD for a Linux SDK that requires it. On Windows use the pinned
Clang driver in the Visual Studio developer environment, retaining
`-llegacy_stdio_definitions`. On macOS use the pinned SDK's Clang. README documents
the full build/link setup. `--no-opt` works with both source execution modes.
No namespace declarations or folder-based import restrictions are introduced.

## VFS and canonical identity

The explicitly supplied entry file's parent establishes the virtual project
root. There is no automatic entry discovery or root override in this phase.
The entry argument is an ordinary host path, interpreted once; all source imports
then resolve independently of the caller's working directory.

- `/math/calculator.void` starts at the virtual project root, never the OS root.
- `./helper.void` and `../shared/common.void` start at the importing file's VFS
  parent. `../` can reach any project sibling that remains inside the root.
- Source separators are `/` on every OS. Backslashes, drive paths and network
  paths are rejected. Every file import must name an exact-case `.void` file.
- Normalize repeated internal separators, `.` and `..` lexically. Reject a `..`
  that would leave the root, even if subsequent components would reenter it.
  A doubled leading `//` is rejected as a possible network/OS path.
- Discover only the reachable imported graph. Different normalized spellings of
  the same VFS path load and compile that file once. Direct self-imports, repeated
  imports and diamond dependencies do not duplicate declarations.

Canonical module identity is the normalized root-relative VFS filename, preserving
case. Function identity is that module identity plus its declared function name.
These keys are project-scoped, independent of aliases, physical checkout location,
native addresses and JIT generations. Relocating the same project preserves them.
A future project/library manifest must provide persistent identity when combining
independent projects; these VFS keys are not globally unique project IDs.
Diagnostics and debug line tables retain the originating physical source path.
The compilation-unit debug record carries a separate physical entry path, so a
stable VFS identity never becomes a nonexistent debugger filesystem path.

### Provisional filesystem policy

Use exact on-disk spelling on all platforms, including case-insensitive hosts.
The resolver enumerates each referenced parent directory: a case mismatch or
case-only siblings matching a requested component produce a source diagnostic.
Files/folders with the same stem but different VFS paths remain distinct modules.

Module path components currently use printable ASCII, including internal spaces,
and exclude Windows-reserved characters/device names and trailing dots/spaces.
The Windows exclusions follow [Microsoft’s filename rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file).
This is an explicitly provisional portable filename subset. Unicode source text
is still supported; Unicode module filename/case normalization needs its own
contract. Non-ASCII physical project-root paths on Windows are unverified.

The selected physical root's ancestors are canonicalized once, so OS aliases used
to select that root do not change VFS identity. Entry-file symlinks and symlinks in
surviving imported VFS components are rejected, as are canonical redirects/junctions.
Distinct hard-link filenames referring to one physical source within a graph are
rejected rather than assigned order-dependent module identities. This is a source
resolution policy, not a security sandbox against concurrent filesystem changes.

External imports that escape the root are disabled. The Bible permits them when
explicitly authorized; permission configuration and stable external identities
remain deferred. Installed/standard package imports such as `use "math"` are
also deferred and receive a targeted diagnostic, not an OS-path fallback.

## Names, aliases and visibility

Public functions are visible by default; explicit `public func` works.
`private func` is file-private, including within import cycles. Its declaring file
can call it; other files cannot access it through simple or qualified calls.
`internal func` is visible through imports anywhere in this compiled project,
not merely its folder. External projects/compiled-library exports are not yet
implemented, so their internal/public boundary remains future work.

Current lookup choices, made explicit for this slice:

1. An unqualified call first finds a declaration in its own file. Otherwise it
   considers accessible functions from direct unaliased imports only. Exactly one
   declaring owner succeeds; multiple owners diagnose ambiguity at the call.
2. An unaliased import exposes simple function names and, when its filename stem
   is a valid identifier, a default qualifier such as `calculator.double(...)`.
   Identical stems are allowed when simple calls are unambiguous; using an
   ambiguous qualifier requires explicit aliases.
3. `as calculator` exposes the module through that explicit qualifier only.
   This deliberately keeps its functions out of unqualified lookup and provides
   a small way to avoid collisions. There is one qualifier level, not a namespace
   hierarchy. A non-identifier filename stem can still expose simple functions;
   supply an alias to qualify it.
4. A repeated import of the same normalized module/binding is idempotent.
   Different aliases may name that same module. An explicit alias bound to
   different modules, or conflicting with a local function declaration, is an
   error. Implicit qualifiers may coexist with local function names because
   `name(...)` and `name.function(...)` are distinct syntactic forms.
5. Imports do not transitively reexport names. Import the declaring file directly
   or call a public wrapper from a direct import. Local variables/parameters may
   not shadow visible function names or import qualifiers, matching the existing
   no-shadowing rule. Duplicate functions within one file remain errors.

These choices preserve unambiguous simple names without silently selecting one
of multiple imported functions. Diagnostics name user-visible functions, aliases
and VFS paths, not LLVM symbols. Candidate owners are ordered deterministically.

## One compilation pipeline, including cycles

`compile_project(entry)` owns the source graph, discovers and parses each file,
then registers every function signature across the full graph before checking
any body. Per-file symbol lookup supplies declaration owners to the existing
type checker. Argument counts/types, return/control-flow checks, short-circuiting,
evaluation order and `for` semantics remain shared with single-file compilation.
All discovered bodies are validated, including unused functions.

Function-only cycles and cross-file mutual recursion are supported, including
forward references and self-imports. Module and project constants now use a
separate compile-time dependency walk; see VALUE_SEMANTICS.md. Runtime global
initialization remains unsupported. Unsupported top-level declarations still
receive diagnostics.

The same AST lowering emits one validated VOID IR unit containing the reachable
graph. Each function carries its declaring module identity and source name; calls
refer to the resolved owner. The IR validator checks ownership consistency.
The shared LLVM implementation encodes these owned keys injectively into portable
native symbols, retaining friendly debug function names. ORC generation suffixes
and resource leases retain the existing lifetime model. AOT uses the same lowering
and adds its native adapter. Functions named `update` or `entry` in other files
cannot accidentally replace each other or the entry-file adapter.

The in-memory `compile_source(SourceFile)` API remains useful for frontend checks
and compiles through the same graph/type/lowering implementation. It explicitly
rejects imports without a filesystem project context. `compile_project` returns
the entry snapshot and IR, so CLI entry diagnostics need not reread changed files.

This phase does not implement file watching, incremental recompilation, automatic
source hot reload, incremental dependency fingerprints or global initialization.
Const-containing source files now carry an exact conservative restart contract (VALUE_SEMANTICS.md).
The module replacement proof checks stable identities and active old leases;
it is not a claim of a live source-reload product.

## Tests and subsequent work

The original eight suites remain. `modules` adds real filesystem graph tests for
ownership, nested/normalized/repeated imports, aliases, duplicate stems, visibility,
cycles, exact case, malformed paths, source locations and replacement identity.
Filesystem-specific symlink/hard-link/case-collision fixtures report capabilities
explicitly; ordinary module compilation and exact-case rejection run everywhere.

`modules_cli` copies the actual acceptance example into paths with spaces, then
checks/emits/links/verifies DWARF/executes normal and O2 JIT/AOT. Independent nested
and cyclic projects run the same way. Editing only an imported source changes
both native engines' results. A nested invalid body reports its actual imported
file, line and column. Existing GC/native/hot-reload regressions remain intact.
See WORK_LOG.md for actual Linux commands, results and hosted verification gaps.

Constants, numeric correctness and runtime diagnostics now extend this foundation;
see [VALUE_SEMANTICS.md](VALUE_SEMANTICS.md) and WORK_LOG.md. Structured recovery is
now implemented in [DIAGNOSTICS.md](DIAGNOSTICS.md); next refine source/dependency compatibility. External imports/packages still
need explicit identity/permission contracts, and managed types need the GC gate.

## Phase 5 diagnostic collection

Discovery now continues after individual import failures and checks unrelated
reachable units. Structured diagnostics retain physical paths, VFS identities and
immutable source snapshots, with cross-file related declarations. All module
validation finishes before any AST lowering. See [DIAGNOSTICS.md](DIAGNOSTICS.md)
for recovery, deterministic ordering, deduplication and suppression limits.
Function-only cycles, visibility, path policy and exact const restart tokens are unchanged.

## Phase 7 analysis and compatibility update

The earlier whole-file constant-token and analysis-API limitations above describe
the preceding phases. They are superseded by [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md):
one validated, owning analysis result now supplies declarations, snapshots and
resolved dependencies without IR/machine-code generation. Exact declaration tokens
and compiler-derived bindings replace whole const-containing source files in
reload compatibility. Ordinary body/trivia edits beside unchanged constants can
reload; actual constant changes require manual restart. Language rules, numeric
semantics and structured recovery remain unchanged.


## Phase 9 revision integration

[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) adds immutable unsaved project overlays
through the shared loader and an explicit revision-safe session acceptance boundary.
Its diagnostic/query wrappers retain the analyzed project revision. Existing disk
APIs, complete-validation gates, byte spans and exact native contracts remain.

## Selected application source — Stage 8 Sprint 4

The [application contract](STAGE8_APPLICATION_NATIVE.md) adds executable statements
only to the explicitly selected source. Their variables live inside a synthetic
function, not module storage; ordinary declared functions cannot access them.
Imports remain declaration-only: an imported executable body is a compile-time
error. Exact-case discovery and explicit alternate-entry files remain unchanged.
Neither project discovery nor LSP analysis executes source or authorizes a native
provider. Ordinary root constants keep compile-time semantics.
