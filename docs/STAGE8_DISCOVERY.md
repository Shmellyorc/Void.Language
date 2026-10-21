# Stage 8 Sprint 1: project and module discovery

20 October 2026. Sprint 1 is implemented and locally verified (423/423 consolidated suites);
hosted acceptance is pending. The [approval ledger](LANGUAGE_COMPLETENESS_APPROVALS.md) governs scope.
No real standard-library modules or new application entry semantics ship here.

## Source domains and selection

`use io/files as files` selects the exact `io/files.void` under one matching
standard source root. Segments are lowercase ASCII identifiers; keyword segments are allowed.
A keyword leaf needs an explicit non-keyword alias for qualified calls. There is
no extension in this spelling, implicit directory module, index/mod fallback,
reexport or package install. A file and same-stem directory may coexist.

`use "app/player.void"` is importer-relative, as is `./`; `../` and leading
project-root `/` retain their established normalization and safety rules.
Standard sources cannot import quoted project sources; standard-to-standard
imports use unquoted module paths. Internal declarations are accessible within
the same logical source library, private declarations only in their declaring
file, and public declarations through existing direct-import rules. Project-root
constants, types and method privileges do not cross the library boundary.

Project identities retain `/path.void`. Standard identities use the reserved
`/@stdlib/path.void` domain. Project imports/overlays cannot claim that domain.
Physical paths remain separate in source snapshots, diagnostics and debug records.
The existing SourceGraph owns both domains, exact-case checks, portable filenames,
symlink/redirect/hard-link rejection, deduplication, cycles and source capture.
Existing cache keys include each module identity, physical source path and exact
bytes; existing analysis/invalidation includes imported modules and bindings.
Source imports never authorize native provider bindings.

The frontend uses the absolute `VOID_STDLIB_ROOT` environment override, otherwise
the matching compiler build's CMake `VOID_STDLIB_ROOT` source directory (default:
that checkout's `stdlib`). No cwd, workspace, home or recursive search occurs.
A relocated installation must set the override or be built with its installation
source directory. An unavailable root produces an import diagnostic. This sprint
uses disposable fixture libraries; it promises no official module catalog.

## Shared project selection

`vd::discover_project(workspace, active, explicit_entry)` is the one frontend
selection policy. Explicit existing regular `.void` entries win. Otherwise inspect
exact-case `main.void` in the active file's directory and successive parents up to
the selected workspace boundary, nearest first. With no active file inspect the
workspace directory only. No recursive search, source parsing or function scan.
Invalid/wrong-case/aliased main candidates fail instead of selecting another project.
Source errors in a selected main never cause fallback. Entry parent remains the
project VFS root. An absent main permits a labelled standalone active-file editor
selection, including a named unsaved file whose parent exists. An explicit override
must exist. Selection without a workspace uses the labelled standalone fallback.

`voidls --discover <workspace-or-empty> <active-or-empty> <override-or-empty>`
returns JSON `entry`, `root`, `standalone`, then exits. All shipped adapters call
this preflight before fixing their existing `--entry` LSP session. VS Code uses
its selected workspace, Neovim `workspace` (default cwd), Vim optional third setup
argument (default cwd). `voidc check/run/emit <directory>` uses the same helper
with that directory as boundary; explicit source-file CLI arguments remain explicit.

A session stays fixed across edits, file creation and refresh. Deliberate selection
or adapter setup/restart determines a new selection; no automatic rebase of dirty
buffers. The legacy `entry(i32) -> i32` executable adapter remains unchanged.
Future optional `main()` and top-level execution belong to Sprint 4.

## Metadata completion, documentation and navigation

Before ordinary semantic recovery, the compiler recognizes an import line and
queries only immediate directory children. `use `, `use io/`, `use "`, and nested
quoted prefixes enumerate the appropriate domain. Files/folders use ordinary LSP
completion items and exact UTF-16 text edits; selecting a project source retains
`.void` and closes a missing quote. Existing quotes, aliases and comments survive.
Folder edits leave the cursor before an existing closing quote for continued
selection. New named overlays and virtual parents merge with disk; tombstones
suppress sources. Each candidate still passes loader path admission without parsing.
Query-local disk spelling and virtual-child maps avoid repeated enumeration and
overlay scans;
no persistent cache, recursive index, alternate parser or background worker exists.
Results cap at 256 with honest `isIncomplete`; prefix filtering precedes the cap.
Space, quote and slash are additional LSP completion triggers.

Contiguous preceding `##` lines attach documentation to real declaration records.
`## @param name description` supplies parameter documentation in signature help.
Hover retains signatures when docs are absent. Function/type/constant completion
and function/method signatures use compiler-resolved declarations. Metadata is
excluded from native compatibility contracts; exact source cache fingerprints
still change with source bytes. Missing or invalid bodies never authorize IR.

Navigation maps both import paths and API declarations to real source. Tooling-only
module records never enter native compatibility. Known standard identities map to
their captured physical URIs; Windows mapping folds only drive-letter notation.
Opening a known installed source does not create a writable project overlay.
Rename and formatting reject installed targets, including edits indirectly reaching
standard source. Existing project rename, version, UTF-16, immutable snapshot,
refresh and stale-edit guards remain authoritative. No arbitrary external URI
admission follows from recognizing the standard root.

## Verification and remaining work

See [acceptance](stage8/ACCEPTANCE.md) for actual commands, results and unsupported
client/platform combinations. Do not infer hosted acceptance from local results.
Sprints 2–6 remain approved future work with their existing narrower design reviews.
