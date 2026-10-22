# Project-local VOID libraries

21 October 2026. **Implemented and locally verified; Wrench review and native
Windows/macOS verification pending.** [Acceptance evidence](local-libraries/ACCEPTANCE.md).
This is the owner-authorized local-library sprint, not Stage 9 or hosted acceptance.

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
filename rules. The selected entry's parent is the VFS root, independent of cwd.
`app.void` is an explicit entry, not a new automatic discovery filename. See
[project selection](STAGE8_DISCOVERY.md) for deterministic `main.void` discovery.

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

## Native and future boundaries

`raylib` is illustrative. Importing VOID source grants no native C bindings,
link libraries, handles, callbacks or additional host authority. Existing explicit
provider admission, generation leases, Runtime/GC and linkage rules remain intact.
Package installation, registries, manifests, versioning, external-project identity,
automatic native linking and executable packaging require separate authorization.
The separate CLI/build proposal and all Stage 9 work remain outside this sprint.

## Future external package manager — Boss direction, 21 October 2026

The future direction is a **Git-based package manager**: Git repositories as
external VOID library sources, with explicit versions/revisions, reproducible
dependency resolution, pinned identities/content hashes and a lockfile. Preserve
compatibility with project-local `libs/` and `use lib/...`; today's project-owned
imports are not already external packages and introduce no package trust boundary.

Future design must account for target-aware native dependencies, platform and
architecture compatibility, eventual C-library integration, package trust and
integrity checks. Do not permit unrestricted automatic installation scripts.
Package discovery and editor integration are future capabilities, using the
compiler-owned source/identity model where appropriate.

Exact manifest syntax, resolver design, version policy, trust rules, external
identities and native-link contracts remain **future design decisions**, not
approved implementations. No package manager is implemented by the local-library,
Stage 9 or CI #90 repairs. Complete VOID language features before substantial
standard-library implementation. This section records direction, not a new sprint.
