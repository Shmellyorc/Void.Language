# Minimal VOID language server (Phase 10)

## Creation subprocesses (Slice 5)

The VS Code adapter uses an additive schema-1 subprocess contract. It calls
`--discover-creation WORKSPACE ACTIVE` (empty ACTIVE means no active selector)
for a bounded inventory without source parsing, then `--plan-project` for the
reviewed manifest path and identities. Only after approval does it send the
reviewed options and physical root identity to `--create-project`. A same-root
manifest collision is reported explicitly, so the editor can preserve the existing
file and run normal project selection. The regular schema-2 preflight, project LSP
sessions and legacy endpoints keep their existing contracts. See
[Slice 5](projects/SLICE5.md).

## Manifest project protocol (Slice 3)

Legacy `--discover WORKSPACE ACTIVE ENTRY`, its three-field JSON, `--entry FILE`
and `voidToolingProtocol: 1` remain available. The additive capability is
`experimental.voidProjectProtocol: 1`. New automatic/project adapters require it;
they never downgrade a selected manifest project to an entry-parent session.

`voidls --discover-project WORKSPACE ACTIVE ROOT ENTRY` accepts four literal
operands, with empty strings for absent values. With an explicit root it pins
exactly that manifest; an entry override is portable and manifest-relative. With
only an entry it uses legacy discovery. With neither it uses the compiler's shared
admitted ancestor walk within the selected workspace, nearest manifest first,
then the unchanged legacy helper only when no manifest is present. Explicit root
plus an active source across/outside its boundary diagnoses. No creation query or
project-writing workflow is implemented.

The schema-2 JSON includes mode/root, canonical entry or null, standalone flag,
manifest and requested root, application/library kind or null, status, decimal-string
selectionRevision, snapshotKey, D/E counts and bounded diagnostic summary. Project
errors such as missing main or malformed TOML return a successful error-bearing
summary. Bad invocation/launch/path discovery failures remain process errors.
Summary output is limited to 64 KiB; at most eight messages of 256 UTF-8 bytes are
previewed. Detailed ranges and related locations belong to the session. Preflight
revision "0" is an observation, not a session acceptance token or a stability promise.

`voidls --project DIR [--entry PATH]` starts a pinned project session even with
missing main or invalid TOML. The ordinary full-text didOpen/didChange/didClose
methods accept exact `void.project.toml` documents in a separate overlay namespace.
Manifest diagnostics use captured bytes and the existing UTF-16 conversion. Invalid
policy never drives source traversal. Source deletion/tombstones never fall back
to stale disk bytes. Library navigation uses validated declarationless-root metadata;
there is no library application entry or executable IR.

Project sessions support standard didCreateFiles/didRenameFiles/didDeleteFiles
notifications and watched-file invalidation. Watched create, change and delete
events refresh analysis using current filesystem state and retain open source
and manifest buffers. A delayed delete can therefore observe a restored file.
Explicit rename transfers owned dirty bytes to the new URI, and explicit delete
marks the old source unavailable. Closing a document releases its overlay and
returns to current disk authority. Newer full-text edits can repair an explicitly
tombstoned document. Broad project watches also invalidate on directory and
wrong-case manifest events, leaving admission to the compiler. Nested manifest
edits change boundaries without switching the root.

`void/projectState` requests return the current schema-2 summary; notifications of
that name accompany accepted project revisions. Project diagnostics carry a
`voidProjectRevision` decimal string, plus normal document versions when negotiated.
`void/reselectProject` with `{entry: string|null}` deliberately reselects inside the
same pinned root; root changes require a new server/session. Metadata/policy/entry
changes preview diagnostics and mark reselection required. No operation publishes
code. Historical successful indexes are never returned as current after failure.
Existing 8 MiB framing, JSON/output limits, cancellation and legacy error semantics
remain. See [S3 evidence and limits](projects/SLICE3.md).


The [Phase 1 ownership/protocol proposal](PROJECT_ARCHITECTURE_ROADMAP.md#phase-1-ownership-and-data-flow)
uses a separate schema-2 `--discover-project` summary and `--project` session, with
additive `voidProjectProtocol: 1`. Existing `--discover`, `--entry` and
`voidToolingProtocol: 1` remain the legacy contract. Manifest snapshots, inventory,
nullable entry/library mode and errors must be represented explicitly; a missing
main need not prevent an editing session. Failed discovery cannot provide usable
ValidatedAnalysis/IR. No protocol/source change is implemented; HOLD remains.

## Approved project analysis roadmap, 8 November 2026

The [project roadmap](PROJECT_ARCHITECTURE_ROADMAP.md#proposed-ide-and-public-usability-milestones)
proposes shared manifest diagnostics/metadata, source-set and main candidate
selection, dependency/native errors, numeric enum conversion analysis and typed
FFI symbols. Reuse compiler `ValidatedAnalysis`, `ToolingSession`/`ToolingIndex`,
snapshots, UTF-16 spans and guarded navigation/edits. Discovery protocol changes
must be explicit; adapters must not parse manifests/main/FFI independently or
silently change fixed sessions with dirty buffers. Analysis never executes source,
installs dependencies or loads native libraries. All additions are unimplemented;
existing `--discover`/`--entry` selection and protocol remain the baseline contract.

## Compiler documentation and result metadata repair

[Contract](DOCUMENTATION_HOVER.md) and [local evidence](DOCUMENTATION_HOVER_IMPLEMENTATION.md)
cover the separate Boss-authorized repair after Stage 11. Callable hover/completion
filters recognized parameter/return tags into description and applicable return
text. Signature help retains labels/binding metadata but returns null unless the
active parameter has nonempty documentation, for both automatic and manual requests.
Only the active parameter carries documentation; signature-level documentation is
omitted. The server negotiates plaintext MarkupContent and otherwise uses a plain
string. Clients never parse documentation tags.

The compiler exports typed builtin result annotation/Ok/Err references and shared
statement/expression/nested pattern references, including ordinary enum variants.
Nullable-presence Ok has distinct metadata. Captures retain their local types;
builtins have no filesystem definition or rename target. Documentation is excluded
from native compatibility contracts. Existing snapshots, UTF-16 conversion, version
checks, cancellation and diagnostic publication remain authoritative.


## Stage 11 compiler-owned global symbols

Global declarations/references use the shared compiler's mutable-variable records
for hover, completion on both assignment sides, definition, references and guarded
rename. Root visibility, import aliases, UTF-16 ranges, unsaved overlays, invalid
snapshot rejection and installed-source protection follow ordinary compiler rules.
Project rename validation excludes installed standard sources from editable overlays
and rechecks their disk snapshots. Existing Vim rename restrictions remain. The
separate result/Ok/Err/parameter-documentation hover repair is outside this assignment.
See [RUNTIME_GLOBALS.md](RUNTIME_GLOBALS.md) and [verification](stage11/IMPLEMENTATION.md).


# Stage 7 language service — current contract

Boss authorized Stage 7 production engineering on 20 October 2026.
[STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md) is the current protocol,
recovery, rewrite, synchronization and scheduling contract. It supersedes the
historical Phase 10 exclusions below. Added methods: completion, hover,
signatureHelp, documentSymbol, workspace/symbol, guarded prepareRename/rename,
formatting, didSave, watched-file refresh, cancellation and void/refreshProject.
Initialization exposes `experimental.voidToolingProtocol: 1` and server version
0.2.0. The same compiler-owned server serves every editor. Local evidence and
remaining limitations are in [acceptance](stage7/ACCEPTANCE.md); Stage 7 is not locked.

---


**20 October 2026:** [Stage 7 research roadmap](STAGE7_ROADMAP.md) verifies the
current source, including later symbol coverage that exceeds the original Phase 10
summary below. Boss confirms Stage 6 locked at `4139297f` / CI #78. Research is
complete; Stage 7 implementation is not authorized and no new capability is claimed.

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

`voidls` is a synchronous, single-threaded stdio implementation of the
[Language Server Protocol 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/).
It analyzes real `.void` source through the existing compiler and Phase 9
`ToolingSession`. It never executes source, publishes native code or restarts a host.

## Stage 7 planning goals (not implemented)

Stage 7 should extend this existing compiler-owned LSP 3.17 server into a shared,
practical editing service for VS Code/VSCodium, Neovim and Vim. **This is planning,
not Stage 7 implementation authorization.** Stage 6 and its Delivery 5/hosted gates
continue unchanged. The implemented protocol table below remains the current
capability contract; future goals do not change it.

Priorities are completion, hover, signature help, rename, formatting, better symbol
coverage, project handling and responsiveness, with semantic highlighting, code
actions and workspace features where appropriate and supported. Extend the
compiler's retained analysis/index rather than adding a second parser, symbol
resolver or type checker in any editor. All editors use the same authoritative
`voidls`; client differences belong in adapters/configuration and capability
negotiation, not competing language semantics.

Preserve immutable snapshots, diagnostic codes/ranges, UTF-16 conversion, canonical
source identities and stale-result rejection. Richer symbol coverage must be
checked against actual declarations/references and access rules. Rename needs a
reviewed scope/conflict policy and edits tied to the correct document versions;
formatting needs an explicit policy that preserves program meaning. Project
selection, synchronization and disk refresh need defined behavior against today's
explicit entry/root and full-document model. This plan selects no new project
format, background scheduler, incremental parser or caching subsystem.

Investigate real source and official protocol/client documentation before complex
changes. Measure representative startup, edit/query latency and memory where
useful; optimize demonstrated bottlenecks. Plan one cohesive Stage 7 sprint with
focused development tests and consolidated acceptance; split only for demonstrated
dependencies or risk. Real editor tests must cover attachment, unsaved changes,
diagnostic repair, navigation, stale results and lifecycle on supported client/
platform combinations. Optional LSP extensions are not unconditional requirements.
Server discovery/distribution, workspace policy, symbol coverage and exact feature
contracts remain review decisions. See the [canonical plan](DEVELOPMENT_STRATEGY.md#owner-workflow-and-stage-7-planning--19-october-2026).
No package manager, general FFI, new language syntax or automatic native hot reload
is implied by editor refresh or richer LSP support.

## Build, start and demonstrate

Build the project normally with the complete LLVM/Clang/LLD **23.1.4** development
SDK (see the [toolchain requirements](SOURCE_COMPILER.md#active-toolchain)). Tests require Python 3.12+.

```sh
cmake --build build/ci-debug -j2
build/ci-debug/voidls --entry examples/analysis/main.void
```

The second command waits for framed protocol input. Configure an LSP client with
that executable and arguments `--entry` and the absolute project entry filename.
The optional Phase 11 extension is documented in [EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md).
There is no project discovery. On Windows use
`build/debug/voidls.exe` (or the executable in your chosen build directory).
The entry's parent directory selects the project VFS root. Initialization's
workspace/root fields do not select or expand that root.

Run the reproducible client demonstration through real subprocess pipes:

```sh
python3 tools/lsp_demo.py build/ci-debug/voidls
ctest --test-dir build/ci-debug -R '^lsp_' --output-on-failure
```

The demonstration creates a disposable two-file project, initializes, opens source,
receives diagnostics, finds a function definition and its references, edits an
import without saving, observes `VOID2002` and null navigation, repairs the file,
verifies empty diagnostics and restored navigation, then shuts down and exits.
It does not edit repository examples. `tools/lsp_client.py` is a small portable
Python test/client helper, not a second implementation of compiler analysis.
Its reader threads enforce bounded test waits; the server itself has no workers.

## Implemented protocol

| Method | Behavior |
|---|---|
| `initialize` | One request; returns UTF-16, full synchronization, definition and references capabilities |
| `initialized` | Notification; enables analysis and publishes initial diagnostics |
| `shutdown` | Request; null response, no further work accepted |
| `exit` | Notification; status 0 after shutdown, otherwise 1 |
| `textDocument/didOpen` | Full UTF-8 text overlay, `languageId: "void"`, signed int32 client version |
| `textDocument/didChange` | Strictly newer client version and full-document replacement events |
| `textDocument/didClose` | Removes overlay and returns to existing disk fallback |
| `textDocument/definition` | Location or null, from the current validated symbol index |
| `textDocument/references` | Locations; honors required `context.includeDeclaration` |
| `textDocument/publishDiagnostics` | Notification, including empty arrays to clear old errors |

Full-change events are applied in order; the final replacement becomes the selected
revision. An empty event array advances the revision without changing text. Ranges
and `rangeLength` are rejected, rather than interpreted as supported patches.
Duplicate opens, closed-file changes, stale/equal versions and malformed updates
are logged to stderr without responses or session mutations. Close/reopen accepts
a new signed version sequence, including negative values. Each accepted transition
independently advances the session's positive uint64 logical document revision;
LSP versions never weaken Phase 9's checks.

Requests preserve string and numeric ID values, including zero and negative signed
int32 IDs. Unknown requests receive -32601; bad parameters -32602; invalid requests
-32600; pre-initialization requests -32002; malformed JSON -32700. Notifications
never receive method responses. Unsolicited client responses are ignored. JSON-RPC
batch arrays are unsupported (LSP uses individual messages). `initialized` must
follow the initialize response. After shutdown only exit is accepted; EOF at a
message boundary exits 0 after shutdown and 1 otherwise. Truncated/invalid framing
emits a framed parse error and exits 2, because stream resynchronization is unsafe.
Internal failures log details on stderr, respond -32603 where possible and exit 2.
There is no server-initiated request, progress or cancellation facility.

## UTF-16 and URI adaptation

`vd::lsp::Positions` retains an immutable `SourceFile` snapshot and converts UTF-8
byte offsets directly to/from zero-based UTF-16 code units. It reuses the compiler's
line index and UTF-8 scalar validator. ASCII, multibyte text, supplementary
characters, LF/CRLF/CR, empty files/lines, zero-width spans and EOF are covered.
The adapter rejects offsets inside UTF-8 sequences, CRLF interiors and positions
inside surrogate pairs. Invalid lines reject; characters beyond EOL clamp to EOL
as specified by LSP. A BOM counts as the code unit actually present in the text;
human diagnostic column formatting is not reused. Invalid UTF-8 disk bytes in
compiler error snapshots map to one replacement code unit per bad byte.

Reusable file URI helpers encode UTF-8 bytes, spaces, `#` and `%`. Physical Unicode
project folders are supported with explicit UTF-8 filesystem conversion and Windows
wide process arguments/binary stdio. This does not change the compiler's existing
portable ASCII VFS module-component policy: Unicode module filenames remain
unsupported, while Unicode comments/source text and physical root folders work.

Only normalized absolute local `file:` URIs are accepted, with empty/localhost
authority and Windows drive notation where applicable. Drive-letter case is
normalized; another drive is rejected. Network/UNC authorities, other schemes,
NUL, bad escapes, invalid UTF-8, query/fragment suffixes, backslashes and dot-path
aliases reject. URI paths must map inside the selected root. Exact component case,
symlink/hardlink and escape restrictions remain those of the existing VFS loader.
Output uses the canonical physical root URI; clients should use that spelling.
The explicitly selected root spelling is also accepted on input. This is local
project tooling, not a sandbox for running untrusted compiler processes.

## Freshness, diagnostics and navigation

Each accepted open/change/close selects and completely analyzes a new project
snapshot, then uses the normal session acceptance and current-result checks.
The selected project graph owns application analysis. Since G13, eligible open
source outside that graph has its own analysis of the current editor buffer.
Both use compiler-owned validation and apply the same root identity, source
admission and dependency authorization rules. Independent analysis supplies
editor intelligence without granting execution authority. Queries use current
analysis for the relevant owner, including bounded editor recovery where
supported. Failed or stale analysis cannot supply historical success.

Diagnostics preserve compiler codes (`VOIDxxxx`), severity, description and hints.
Available related locations and open-document versions are included when the
client advertises the corresponding publishDiagnostics capabilities. All primary
and related ranges come from their owning snapshots. The LSP merges and
deduplicates diagnostics from the selected graph and open-document analysis
into a single replacement `textDocument/publishDiagnostics` array per URI.
Removing an import changes graph membership while leaving the document open.
The current buffer can still produce the same error under independent analysis,
at the same client version. When correction, loss of the final owner or project
invalidation leaves a previously erroneous URI without diagnostics, the
server sends an empty array.

Dirty overlays supply editor bytes independently of the physical source,
including for file URIs with no file on disk. An actual `textDocument/didClose`
releases the overlay and the open document's client version. A referenced
physical source can still produce its own diagnostics through the selected graph,
and a missing referenced source can produce an import error in the importer.
Closing a visual editor tab can leave the text document LSP-open. The Stage 7
acceptance record describes the earlier contract.
[G13 implementation](roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#batch-1-implementation-record) ·
[G21 qualification](compiler-health/G21_LSP_DIAGNOSTIC_LIFECYCLE_QUALIFICATION.md) ·
[G22 watcher repair](compiler-health/G22_WATCHER_OVERLAY_RESEARCH.md#authorized-production-repair)

Definition and references support resolved direct/aliased imported functions,
forward/cyclic functions, project-root/imported constants and local readonly
constants, across multiple callers/files. Declaration-name queries consume the
index's existing declaration records; references consume its resolved references.
Mutable locals, parameters, aliases themselves, comments, punctuation and arbitrary
expressions return null/empty results. There is no second symbol scanner.

Disk fallback reads occur during analysis and retain the existing inconsistency
checks. Save and watched-file notifications, `void/refreshProject`, and accepted
open/change/close events can trigger reanalysis. In-process clients can use the
Phase 9 `refresh_disk` API. Each result describes the bytes and authority checked
for its snapshot. Later filesystem changes require a fresh check.

## Transport and dependency boundaries

Headers are ASCII with CRLF termination and mandatory byte-count Content-Length.
Input supports fragmented/coalesced messages. Header names are case-insensitive;
duplicate headers, malformed lengths and truncation reject. Optional Content-Type
accepts application/vscode-jsonrpc with UTF-8 (including legacy `utf8`). All protocol
output is framed on stdout; operational logs are confined to stderr.

Each incoming header block is limited to 8192 bytes, each incoming body to 8 MiB,
JSON nesting to 128,
and simultaneously open overlays to 4096. LLVM Support's maintained JSON parser
handles JSON syntax, escaping, serialization and UTF-8 validation; the small depth
scan only guards resource use. Unpaired escaped JSON surrogates follow LLVM's
replacement-character normalization. These bounds do not claim a global workspace
memory/time quota: full-graph analysis and synchronous disk reads can be expensive,
and retained session revision history grows with distinct document identities.

`voidls` links `void_lsp` and `void_core` plus LLVM Support/numeric support required
by the existing frontend (APInt/APFloat). It does not link `void_llvm`, LLVM ORC JIT,
or `void_runtime`. The complete pinned SDK remains a build dependency; this phase
does not claim a wholly LLVM-free executable. RTTI and sanitizer configuration
continue to match the selected SDK.

Completion, hover, rename, formatting, semantic tokens, code actions, packages,
workspace discovery, file watching, background analysis, persistent caches,
incremental parsing and automatic native publication/restart remain deferred.
No VOID syntax or numeric semantics change. See WORK_LOG.md for measured local
verification and the pending hosted Linux/Windows/macOS run.
