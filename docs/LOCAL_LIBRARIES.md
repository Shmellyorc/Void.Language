# Project-local VOID libraries

VOID supports project-owned `lib/...` source and declared, locked `pkg/...`
packages. Both use the existing source graph, visibility and path checks.
[G26 maintenance and ownership](#g26-installed-source-packages-and-maintenance)
explains acquisition and publication; [G27 qualification](projects/G27_LIBRARY_AUTHORING.md)
records the completed authoring integration. Earlier local-library delivery is
retained in its [dated acceptance record](local-libraries/ACCEPTANCE.md).

## Creating and composing libraries

From an existing empty parent directory:

```sh
voidc new app
voidc new geometry --kind library --package-name geometry
voidc add geometry --project app --path ../geometry
voidc tidy --check --project app
```

`new` writes `package.entry = "src/geometry.void"` under `[package]`, using the
actual generated path. `--source api/geometry.void` records that custom path
instead. `init --kind library` preserves existing source and does not guess an
entry; set `[package].entry` explicitly before offering that project to `add`.
Applications continue discovering a unique synchronous main without requiring
`project.entry`. Libraries are checked/imported, not directly run or built as
executables.

For a reusable library that depends on another library, start a separate graph
in that same parent directory:

```sh
voidc new consumer --kind library --package-name consumer
voidc add geometry --project consumer --path ../geometry
voidc tidy --check --project consumer
```

Replace `consumer/src/consumer.void` with:

```void
use pkg/geometry as g
public func answer() -> i32:
    return g.answer() + 7
```

After saving the source, check and consume it from a fresh application:

```sh
voidc check --project consumer
voidc new transitiveapp
voidc add consumer --project transitiveapp --path ../consumer
voidc tidy --check --project transitiveapp
```

Replace `transitiveapp/src/app.void` with:

```void
use pkg/consumer as c
func main() -> i32:
    print(c.answer())
    return 0
```

`voidc run --project transitiveapp` prints `49`. The application declares
`consumer`; only that library declares `geometry`. There is no implicit re-export
of the transitive alias. External paths are editable copies: the first `app`'s
`libs/geometry` and `transitiveapp`'s `libs/consumer/libs/geometry` are distinct
origins. Adding another copied `geometry` directly to `transitiveapp` conflicts
with VOID5007 even when IDs agree. Reuse a deliberately shared in-project origin
when a graph needs shared editable identity.

`add --git ORIGIN --rev FULL_SHA` also supports library consumers through the
same D3 lock/snapshot checks. Transitive Git acquisition still requires explicit
D3 authorization; add does not grant it implicitly. Native requirements, catalog
selection and execution approval remain the application's separate D4 boundary.

## Source spelling and layout

```text
game/
├── app.void
└── libs/
    └── raylib/
        ├── raylib.void
        └── graphics.void
```

```void
use random
use lib/raylib as rl
use lib/raylib/graphics as gfx
use "/libs/raylib/graphics.void" as same
```

The physical folder is `libs/`; the reserved unquoted source prefix is `lib/`.
`lib/raylib` selects exactly `/libs/raylib/raylib.void`.
`lib/raylib/graphics` selects exactly `/libs/raylib/graphics.void`.
Only the first-level library has the repeated-name entry convention. Deeper paths
name a file directly; no index, recursive import or directory-module fallback.
A direct `/libs/raylib.void` is accessible by quoted path, but does not satisfy
`use lib/raylib`. Missing entries diagnose. Ordinary aliases and default leaf
qualifiers work; `using` remains struct composition.

Unquoted segments retain the existing lowercase ASCII identifier grammar and `/`
separators; omit `.void`. Quoted project paths retain their existing portable
filename rules. Explicit-file selection uses the selected entry's parent as VFS root; manifest
selection uses the manifest root, independent of cwd. Manifest applications
select a unique supported main across their source roots regardless of filename;
manifestless directory selection retains exact `main.void`. See [CLI selection](CLI.md).

## Identity, ownership and safety

`SourceGraph::resolve` translates the prefix into an ordinary project path before
running the existing resolver. Local and equivalent quoted imports have **one**
`/libs/...void` identity, source snapshot and compiled module, including diamonds
and declaration cycles. Existing constant-cycle validation still applies.
Analysis, source navigation, dependency invalidation, Debug cache and compatible
reload use those same identities; old JIT leases retain their old snapshots.

Local libraries are project-owned sources. Project-root constants and existing
public/internal/private access rules apply. This is not an external package
boundary: `internal` is project-visible, `private` remains file-local. Unsaved
sources and unavailable-source tombstones use canonical project identities.

All referenced components pass the existing exact-case, portable-name, collision,
containment, symlink/redirect/reparse and hard-link alias checks. Unicode physical
root paths remain supported; no new Unicode module-filename policy is introduced.
No descendant alias is trusted through this spelling. Snapshot/refresh semantics
and the existing concurrent-filesystem-change limitations remain unchanged.

Official unquoted imports still select the separate `/@stdlib/` source domain;
project files cannot shadow them. `lib/` never falls back to the installed sources.
Installed standard modules cannot import project-local libraries or quoted project
sources. A quoted `"lib/..."` remains an ordinary relative project path.

Only the selected application controls execution. Imported ordinary `func main()`
is a declaration, never an implicit entry or initializer. Executable top-level
statements in any imported library remain compile-time errors.

## Compiler-owned assistance

The existing metadata resolver enumerates only the requested directory and its
virtual overlay children. Absent `libs/` gives no local suggestions. `use lib/`
offers `raylib/`, plus `raylib` only if the exact repeated-name entry exists.
`use lib/raylib/` offers nested files and folders. Existing bounded completion
(256 items and incomplete flag), aliases, UTF-16 edits, docs, signatures, navigation,
refresh and same-server recovery apply. Empty folders offer traversal without an
invented module. No adapter implements library discovery.

## Native and package boundaries

Importing ordinary VOID source grants no native C bindings, link libraries,
handles, callbacks or additional host authority. Typed C imports and verified
raylib packages are implemented through explicit provider admission, generation
leases, Runtime/GC and linkage rules. See [G25 header import](projects/G25_NATIVE_HEADER_IMPORTER.md)
and [G24 physical source](projects/G24_PHYSICAL_SOURCE_IMPLEMENTATION.md).
No registry, automatic installation scripts or implicit native approval is added
by library creation or maintenance.

## G26: installed source packages and maintenance

The implemented commands are:

```sh
voidc add geometry --path vendor/geometry --project .
voidc add geometry --path ../geometry --project .
voidc add geometry --git https://example.org/geometry.git --rev FULL_LOWERCASE_SHA --project .
voidc add geometry --git https://example.org/libraries.git --rev FULL_LOWERCASE_SHA --subdir geometry --project .
voidc tidy --project .
voidc tidy --check --project .
```

Both application and library consumers use these commands. The project defaults
to the current directory. Local source paths are relative to that selected
project. There is no registry lookup. Git acquisition uses D3's
existing HTTPS or `file:<project-relative-repository>` pins and its existing
whole-repository limits.

`use lib/math` still means project-owned `libs/math/math.void`. It needs no
manifest, registration, Git or maintenance operation. Only referenced libraries
are loaded; entry discovery still excludes `libs/`. Edits remain ordinary source
edits, visible to the compiler and editor.

`use pkg/geometry` means the declared package identity, entry and dependency
edges verified by `void.lock`. For Git packages, `libs/geometry/` is a checked
materialized view of D3's `.void/git-deps/<pin>/<digest>/tree` snapshot, with the
package manifest and internal layout intact. `.void-package` contains the
existing D3 proof as an ownership receipt. It is not an alternative source
index, cache or approval. Compilation verifies the exact view against D3 and
reads verified snapshot bytes. Definitions point to the physical installed
files. Importing those files through `lib/` or a quoted project path cannot
cross their package boundary. Do not edit a Git view; use an editable local
package for development.

An existing in-project `--path` keeps its location and live path-dependency
behavior. A source outside the selected project is copied into `libs/<alias>`;
that installed copy becomes the editable source. This does not create a live
link to the original directory. External sibling dependencies that escape the
installed tree are rejected by the existing resolver enclosure. Move the
editable package graph inside the project deliberately when that is needed.
User directories, case collisions, aliases and incompatible declarations are
never overwritten. Repeating the same addition preserves editable local changes.

`add` appends the smallest dependency table, retaining existing TOML bytes,
comments, table order and LF/CRLF style. It pins the discovered package ID and
exact version, validates the graph and writes the existing D3 lock format.
Existing manual manifest/fetch/resolve workflows remain supported; a Git view is
optional for those workflows. A missing view is advice in `tidy --check`; `tidy`
materializes it. A present unowned or altered view is an error.

`tidy` parses relevant source roots or the explicit entry using the compiler's
source graph and follows referenced project/local imports. It validates package
metadata and locks without executing or type-checking the selected project. Missing
local entry files and undeclared `pkg/` imports are errors. Unused direct
packages are advice and are retained. No developer `.void` source is changed.
Legitimate editable-source or manifest changes refresh the lock. Immutable Git
claims cannot be silently refreshed after tampering. After intentionally editing
a Git pin, run the existing `deps fetch` first; tidy replaces a managed view only
when it still exactly matches its previous verified snapshot. It never upgrades
a pin on its own. The second unchanged tidy writes nothing.

`tidy --check` never writes manifests, locks, sources, views or recovery records.
It fails when consistency requires correction, while optional advice remains a
successful result. Tidy does not guess missing source locators or native approval.

Publication is serialized with the existing dependency writer. A temporary
`.void/package-edit/` record saves prior files and the exact installed trees.
Compiler/resolver readers reject a pending edit. Failure rolls back; after a
process interruption, an explicit `add` or mutating `tidy` restores the saved
project before proceeding. Read-only checking reports the pending operation.
Recovery refuses to overwrite subsequently changed files or remove a changed
installed tree, retaining the record for inspection. This is a process-crash
recovery contract; no power-loss or hostile concurrent-filesystem transaction
claim is made.

Source installation grants no native authority. Native declaration advice names
providers and refers to the package's existing README/preparation instructions.
D4's application requirements, selected catalog, descriptor verification and
`--allow-native` remain separate. No installation scripts, binary downloads or
native builds run during add/tidy. Raylib's existing `prepare.py` and catalog
metadata already describe its preparation; no additional TOML format is needed.

The earlier design discussion in this document is historical. The rules in this
section describe the implemented G26 workflow; see
[the qualification report](projects/G26_PACKAGE_MAINTENANCE.md).
