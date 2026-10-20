# Stage 7: Developer Tooling Research and Engineering Roadmap

**Production authorization update:** Boss subsequently authorized Stage 7 engineering
in this chat. The research-only restrictions below are historical. See
[STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md) and
[local acceptance](stage7/ACCEPTANCE.md) for delivered contracts and limitations.
No hosted lock, Git or publication authorization is implied.


**20 October 2026. Research complete; proposed contracts await Wrench review and Boss authorization.**

Baseline: `4139297f92c195782f59a28546444e6b3bec4ad8`, matching local HEAD.
Boss confirms Stage 6 locked at CI #78, seven jobs and 973/973 tests. That hosted
result is owner-confirmed, not independently rerun here. The 19 October documents
still saying Stage 6 awaits its final lock are historical relative to this mission.
This mission authorizes research and bounded experiments only. Nothing below
authorizes Stage 7 production implementation, Git changes or hosted dispatch.

## 1. Recommendation and scope

Build one cohesive sprint around the existing `voidls`, `ToolingSession`, compiler
resolver and typed analysis. The dependency that needs the most engineering is
**current-source scope/binding information and completion on incomplete input**.
Adding LSP handlers alone will not deliver usable IntelliSense. Rename also needs
a deliberate scope and client edit-safety contract; formatting needs a
comment-preserving compiler lexical view. Neither exists today.

Recommended finished outcome: reliable server installation/selection; explicit,
visible project selection; completion, hover, signatures, broader navigation,
bounded safe rename, conservative document formatting, accurate diagnostics,
document/workspace symbol search within the selected graph, disk refresh, and
tested VS Code/VSCodium/Neovim/Vim adapters. Semantic tokens are a modest optional
consumer of the same records. Code actions should wait unless a compiler-owned,
structured fix is actually available. No new package/project format, language
syntax, native hot reload, incremental compiler or persistent analysis cache.

These are proposed choices, not newly approved language contracts. Engineering
may revise the API shapes below when source evidence supports a simpler solution.
One integrated sprint remains practical: no new backend, runtime or language
system is needed. Internal steps are dependency order, not separate deliveries.

## 2. Verified baseline versus gaps

The implementation audit used the actual files at the baseline, not just the old
Phase 10/11 feature summaries. Local reproductions are in
[the research evidence](research/stage7/README.md).

| Area | Implemented now | Stage 7 gap |
| --- | --- | --- |
| Transport/lifecycle | Stdio LSP/JSON-RPC, LLVM Support JSON, framed stdout, stderr logs, initialize/initialized/shutdown/exit and strict malformed-input handling | No cancellation, server requests, watched-file/save refresh, parent-process monitoring or analysis batching |
| Synchronization | Full-document open/change/close; signed LSP versions mapped onto monotonic logical revisions; immutable overlays; close returns to disk | No incremental patches; no disk notification path; each accepted change immediately analyzes the whole graph |
| Diagnostics | Compiler codes, original byte spans converted to UTF-16, related locations/version capability negotiation, repair/removal clearing | Only entry-reachable files are checked; external disk changes alone are invisible; no pull diagnostics or structured fix edits |
| Navigation | Definition/references over compiler-resolved source records, including declaration-token fallback | Parameters, mutable locals, receivers, loop/pattern captures and import-alias tokens lack general declaration/use records |
| Richer existing symbols | Functions/constants, value/managed types and fields, receiver methods, initializers, interfaces/requirements and generic type parameters; enum cases and property accessors have source records through existing categories | Generic origins and implicit edges need careful handling; records do not uniformly describe source-level properties, enums, events, locals or callable expressions |
| Completion/hover/signatures | Typed declarations and resolved references provide useful input | No LSP handlers; no public scope/candidate query; ordinary function metadata lacks parameter names/default spans; no call/argument context; failed analysis exposes no usable index |
| Rename/formatting | Source spans and compiler validation are reusable | Neither is implemented; text replacement over current references alone is insufficient |
| VS Code/VSCodium adapter | `.void`, TextMate syntax, indentation, pinned language client, trust check, literal executable/entry settings, direct spawn, serialized restart/configuration/disposal, VSIX | Server must be built/configured manually; one session per window; multi-root rejected; no automatic refresh; no VSCodium-specific execution evidence |
| Neovim/Vim | No shipped adapter | Bounded research probes successfully attached real clients to unchanged `voidls`; these probes are not shipped integrations |
| Distribution | Native CMake target and editor VSIX build | No `install()` rule/native runtime bundle, server release archive or platform VSIX; no release/license/publication decision |

Evidence: [`frontend.hpp`](../include/void/frontend.hpp),
[`tooling.hpp`](../include/void/tooling.hpp),
[`ToolingIndex/ToolingSession`](../src/frontend/tooling.cpp),
[`Server`](../src/lsp/main.cpp),
[`AnalysisAccess`](../src/frontend/compiler.cpp),
[`generic_tooling.inc`](../src/frontend/generic_tooling.inc),
[`protocol tests`](../tests/lsp/protocol.py),
[`extension`](../editors/vscode/src/extension.ts),
[`configuration`](../editors/vscode/src/config.ts) and
[`manifest/locked dependencies`](../editors/vscode/package.json).

The old server guide's claim that navigation covers only functions and constants,
and the editor guide's old syntax exclusions, understate the source. For example,
the real protocol suite covers fields, managed/value construction, methods,
inheritance, interface requirements, strings, spawn and the native admission
boundary. That does **not** imply every token in every language feature is indexed.

## 3. Architecture to extend

### Retain the existing ownership and authority boundaries

`SourceFile`/`SourceSpan` own immutable bytes. `ValidatedAnalysis` owns the typed
graph; `ToolingIndex` indexes compiler records without parsing. `ToolingSession`
separates select, analyze and accept; exact owned snapshot identity rejects stale,
duplicate and foreign-session results. `current_index()` disappears immediately
after a logical change or failed validation. Historical success remains historical.

The overlay analysis path loads the reachable graph twice and compares bytes and
identities before validation. It is a conservative consistency check, not an atomic
OS snapshot. Preserve it. `refresh()` already advances the logical selection and
seeds invalidation; expose it through LSP instead of inventing a disk cache.

Keep `voidls` linked to `void_lsp`, `void_core` and LLVM Support/numeric support.
It currently avoids `void_llvm`, ORC and Runtime. Preserve LLVM **19.1.7**, existing
RTTI/sanitizer configuration and no source execution/native publication in editors.

### Add compiler-owned tooling metadata, without changing native contracts

Extend the existing parser/binder's recording points to retain owned names,
selection/full declaration extents, lexical scopes and binding lifetimes, ordinary
parameters/defaults, local declarations/uses, receiver bindings, call/argument
spans and accessible member/import information. Reuse the compiler's visibility,
member selection, generic origin mapping and named/default argument binding.
Expose value records or queries, not mutable AST pointers.

**Keep new tooling-only data outside the native compatibility serialization.**
`AnalysisAccess::declaration` and `reference` currently also append to
`compatibility_contract()`. Blindly recording additional local uses through those
helpers could change reload classification and cache identity. Add a narrow
tooling-only recording path/flag and regression checks against existing native
contracts. Do not reclassify invalidation or weaken exact source compatibility to
make editor features easier. Project-relative identity is scoped to its session;
two projects can both contain `/main.void::entry`.

### Completion on incomplete source is the main architectural dependency

Today errors lead to `Diagnostics::finish()` before `AnalysisAccess::build()`, so
`obj.` or an unfinished call cannot use a successful current index. Serving the
previous successful graph would violate freshness. A global list of declarations
would also ignore lexical scope, visibility and receiver authority.

Recommended approach: a narrow compiler-owned editor query/recovery path over
the **current** snapshot, using the existing lexer/parser and binder. At the
cursor, allow a missing name/argument while collecting only context whose bindings
are established from those current bytes. Return owned candidates/call context;
never expose that recovered tree as `ValidatedAnalysis`, lower it, or authorize
native work. If a receiver or enclosing scope cannot be established, return only
valid syntax candidates or no result. Keep normal diagnostics from unmodified
source, not from a manufactured replacement program.

Before broadening other features on failed graphs, prove the small cases:
partial identifier, aliased import prefix, typed `receiver.`, unfinished call,
nested call and an unrelated error elsewhere. A failure in an import/signature
that invalidates the context must suppress semantic candidates. This is a focused
extension of compiler recovery, not an editor-side parser or a second type checker.

## 4. Proposed feature contracts

| Feature | Recommended implementation and boundary |
| --- | --- |
| Completion | Visible locals/parameters, accessible declarations/import qualifiers, compiler-selected members and existing builtins/keywords. Honor readonly/mutation and generic constraints. Offer plain insertions/text edits first; snippets only when negotiated. Never list all project symbols as though all were in scope. |
| Hover | Source-level name/type/signature, visibility, readonly/receiver effects and existing constant value where representable. Use `ir::type_name` plus source generic forms; no fabricated docs or promise of constant evaluation for runtime locals. Escape source-derived Markdown; support plaintext fallback. |
| Signature help | Record written call extent, resolved callable, parameter names/types/defaults and actual argument mapping. Resolve active parameter using compiler parsing, including named arguments and nested calls; do not count commas in strings/generic syntax. Include functions, methods and constructors; unsupported callable contexts return null. |
| Navigation/references | Add the missing binding categories through the compiler, with declaration/use ranges and distinct lexical identities. Deduplicate generic specializations back to source origins. Preserve exclusion of implicit dependency edges from written locations. |
| Document symbols | Return real selection and full declaration ranges; use hierarchical symbols when supported, flat symbols otherwise. Model source properties/enums/events explicitly rather than showing synthetic accessor names as user symbols. |
| Workspace symbols | Search declarations in the current selected entry graph, with bounded result count and deterministic ordering. Label this scope clearly; unrelated disk files are not silently compiled. |
| Semantic tokens | Optional full-document tokens using the same declaration/use metadata and compiler token spans; negotiate kinds/modifiers and UTF-16 delta encoding. No new tokenizer, delta cache or token-refresh framework just for Stage 7. Syntax fallback remains useful during errors. |
| Code actions | Do not advertise general quick fixes yet. `Diagnostic.hint` is prose, not a safe machine edit. Add only concrete compiler-owned structured edits if the sprint identifies a worthwhile fix, with exact snapshot/range tests. No automatic import organizing or speculative repairs. |

Protocol details should follow the official LSP 3.17 sections for
[completion](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/completion.md),
[hover](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/hover.md),
[signature help](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/signatureHelp.md),
[document symbols](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/documentSymbol.md)
and [semantic tokens](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/semanticTokens.md).
Register capabilities only when implemented, and exercise clients with absent
optional capabilities. UTF-16 remains the server's position encoding.

### Rename: scope, conflicts and edits

Recommend **selected entry-reachable graph** scope, stated in the documentation
and editor setup. This matches the current compiler project. References in
unreachable files, other entries or external projects are outside that promise;
whole-directory scanning would require a different project contract.

Implement `prepareRename` and rename for categories with complete written-use
coverage, initially ordinary functions/constants/types/locals/parameters as that
coverage becomes available. Reject keywords, invalid spelling, aliases without
binding records, synthetic symbols, fixed builtin names, `entry` and initializer/
cleanup/call hooks with language-defined names. Reject virtual/interface families
and accessor families until their complete coupled rename is supported. Generic
uses rename their source declaration, not one concrete instance.

Collect exact declaration and explicit-use edits; deduplicate overlapping source
origins, never edit comments/strings/implicit edges. Include named argument labels
when renaming a parameter, constructor field labels for fields, and getter/setter
uses as one source property if those categories are admitted. Reuse the existing
identifier checks and compiler binding rules.

Apply proposed edits to temporary immutable overlays, validate through the same
compiler and check that affected uses resolve to the renamed declaration while
unaffected bindings remain unchanged. Successful compilation alone can miss a
legal rebinding. This is a focused rename preflight, not another semantic verifier.
Native compatibility need not remain equal after a source rename; no hot reload
is requested by this operation.

Recheck current snapshot and relevant disk bytes before returning edits. Use
versioned `documentChanges` for open targets; a closed document uses null version
and a fresh disk check. LSP cannot promise atomicity against an external writer
after the response. Do not claim it does. Clients own application/undo; the server
does not write files. Refuse unsafe capability combinations instead of silently
degrading to unversioned open-document edits. See official
[rename](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/rename.md),
[WorkspaceEdit](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/types/workspaceEdit.md)
and [TextDocumentEdit](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/types/textDocumentEdit.md).
Boss must approve this scope and the Vim capability limitation below.

### Formatting: useful, conservative and shared

Recommend document formatting first: canonical four-space block indentation and
consistent safe spacing, preserve comments, literal/import bytes, line endings
and existing statement/expression order. No import sorting, line wrapping,
identifier rewriting, on-type formatting or regex source reconstruction.
The language currently permits consistent non-four-space indentation; canonical
formatting is a style choice needing approval, not a compiler grammar change.

The current lexer discards comments/ordinary spaces and emits structural
indent/dedent tokens; the AST is not a round-trip printer. Extend that same lexer
with a tooling lexical/trivia view and retained byte gaps. Use parser structure
where indentation is semantic, especially braced literals containing match or
anonymous-function blocks. Skip a file when that structure is ambiguous or
invalid. Preserve exact nontrivia tokens and block relationships, and verify
idempotence and unchanged execution/diagnostic behavior on meaningful fixtures.
One server formatter serves all editors. Honor supported LSP formatting options;
document the fixed space policy and reject unsupported tabs rather than emit
compiler-invalid text. Start with a whole-document edit if simpler. The official
[formatting request](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/formatting.md)
returns edits; it does not supply a VOID formatter.

## 5. Project selection, synchronization and responsiveness

### Project selection without a new project system

Keep `--entry <absolute file>` authoritative and entry-parent-as-root. Initialization
workspace folders never expand compiler import permissions. Recommended first-run
UX: discover the installed server, show the selected project/entry, and offer an
entry-file picker when the user wants a multi-file project. Persist the existing
`void.project.entry` setting; no guessed import graph or execution to find `entry`.
With no explicit selection, retain a clearly labeled standalone-file session.
Switching arbitrary tabs must not silently change the established project.

Recommend retaining one selected project per window for Stage 7. Workspace
symbols, watchers and diagnostics are still useful within that root. Multi-root
support can use one instance of the **same** server per explicitly configured
entry later; it does not justify a global semantic engine or cross-project
identity merging. If Boss requires that in Stage 7, implement disjoint project
routing in each adapter and test it as part of the same sprint; reject ambiguous
overlapping roots. `.git` and a filename `main.void` are not authoritative VOID
project definitions.

### Disk refresh with overlays remaining authoritative

Add standard `didSave` and `workspace/didChangeWatchedFiles` handling, plus a small
explicit `void.refreshProject` request returning the refreshed logical revision
for clients without reliable watching. Route all three to `ToolingSession::refresh`
and the same complete analysis/acceptance path. Ignore irrelevant/out-of-root
events and batch bursts. A watcher event never overwrites an open overlay; deleted
disk source still resolves to its overlay until close, then produces the ordinary
disk diagnostic. Never convert ordinary disk deletion into an overlay tombstone.

Prefer client-provided watching: VS Code's existing language client supports a
scoped filesystem watcher; Neovim can negotiate registration; Vim may need
save/focus/manual refresh. Only dynamically register if the client advertises it,
and correctly track registration responses. Explicitly documented static client
watching is also sufficient. Standard file watching is not a universal client
guarantee. See official
[watched files](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/workspace/didChangeWatchedFiles.md)
and [didSave](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/textDocument/didSave.md).

### Address the measured backlog before speculative compiler optimization

Full synchronization itself is not yet the demonstrated bottleneck. On the
3,201-function generated graph, a valid edit took about 234 ms, but 20 queued
full edits took **4.80 s** and generated 660 diagnostic notifications. Current
synchronous dispatch cannot collapse already queued revisions.

Recommended smallest scheduling change: keep compiler work and session mutation
on one owner thread; allow a bounded stdio reader queue so adjacent document/disk
notifications can be applied in order and analyzed once for the latest selection.
Drain pending ordered changes before accepting/publishing a completed result;
retain exact snapshot checks. Never coalesce across an intervening query,
open/close boundary, shutdown or project transition. Bound queued bytes, retain
backpressure and handle EOF/errors without dropping a request or detaching a
thread blocked on stdin. A platform-neutral tested blocking-I/O shutdown strategy
is part of this change, not an assumed detail.

A single analysis worker with the session on the I/O owner is an alternative if
it is simpler on the actual platforms. Before using it, audit compiler-owned type
lifetimes and concurrent access; allow at most one compiler analysis per process,
one replaceable pending selection and no unbounded task/history queue. Do not
build a worker pool or incremental frontend. Cancellation may discard publication
without interrupting compiler internals. Requests canceled while queued still
need a response; stale mutating edits should fail rather than apply old spans.

Measure the chosen design against the supplied burst probe. No scheduling
implementation has been prototyped or certified in this research.

## 6. Editors, installation and distribution

### VS Code and VSCodium

Reuse `vscode-languageclient` **10.1.2** and its locked dependencies. Current manifest
and installed client both declare VS Code **^1.91.0**; actual local verification is
**1.141.0**, not a proof of every older release. Build/package Node is **24.19.0**;
the measured editor extension host reports Node **24.21.0**. Those are distinct
runtimes. Test the advertised minimum or raise it explicitly rather than infer
compatibility from TypeScript types.

Recommended server discovery precedence: explicit `void.server.path`; a matching
native server bundled with the extension, if Boss selects that distribution;
then a documented installed `voidls` on PATH. Resolve an absolute executable,
validate it, log the selection, and spawn with the existing argument array and
`shell:false`. No automatic repository build, scanning arbitrary build folders,
shell substitution or silent runtime download. Trust checks precede discovery
that could execute anything. Preserve failure reporting and serialized restart;
add visible entry/server status and refresh command. A native protocol/version
identifier should be checked during initialization so a wrong server installation
fails clearly; do not execute an arbitrary file merely to probe its version.

Keep VSIX installation as the common offline workflow:
`code --install-extension <file.vsix>` / `codium --install-extension <file.vsix>`.
VSCodium normally uses Open VSX; marketplace visibility and compatibility are
separate. No Marketplace/Open VSX publication, account creation or signing is
authorized here. The primary sources are the
[VS Code LSP guide](https://code.visualstudio.com/api/language-extensions/language-server-extension-guide),
[Workspace Trust guide](https://code.visualstudio.com/api/extension-guides/workspace-trust),
[platform VSIX packaging](https://code.visualstudio.com/api/working-with-extensions/publishing-extension#platform-specific-extensions)
and [VSCodium's own extension guide](https://github.com/VSCodium/vscodium/blob/master/docs/extensions.md).

### Neovim and Vim: verified clients and limitations

| Client | Upstream evidence and proposed support | Local research evidence |
| --- | --- | --- |
| Neovim built-in LSP | Prefer native `vim.lsp.config`/`enable`, built-in completion, diagnostic and buffer APIs. Those configuration APIs exist in pinned 0.11.5 docs. Target 0.11.5 minimum plus current 0.12.6, with both tested before claiming the range. No required nvim-lspconfig/Mason/completion plugin. | Portable official **0.12.6**, Linux x64: attach, definition, full unsaved edits, VOID2002, stale suppression, repair, references and client stop passed against unchanged `voidls`. 0.11.5 was documentation-checked, not executed. |
| Vim + `yegappan/lsp` | Recommended lightweight core client, pure Vim9 with no Node requirement. Upstream says Vim 9.0+, but its UTF-16 conversion checks **patch 9.0.1629**. Recommend supported Vim 9.1+ and exact pinned client, with minimum-version testing. | Vim **9.2 (patches 1–858)**, client **c741b2f1afb42b9f267824dac4049674908f4a41**: attach, definition, unsaved VOID2002, repair, restored definition and stop passed. No completion/rename claim: the baseline server lacks those methods. |

Neovim primary references: [pinned 0.11.5 LSP help](https://raw.githubusercontent.com/neovim/neovim/v0.11.5/runtime/doc/lsp.txt),
[0.12.6 release](https://github.com/neovim/neovim/releases/tag/v0.12.6).
The optional [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig) is configuration,
not required language semantics; do not introduce its own changing support floor
as a VOID dependency without a need.

Vim primary references: [upstream help](https://github.com/yegappan/lsp/blob/c741b2f1afb42b9f267824dac4049674908f4a41/doc/lsp.txt),
[maintenance history](https://github.com/yegappan/lsp/commits/main/),
[UTF-16/capability code](https://github.com/yegappan/lsp/blob/c741b2f1afb42b9f267824dac4049674908f4a41/autoload/lsp/capabilities.vim).
Upstream history shows continuing 2026 work; maintenance is evidence for selection,
not a guarantee of every feature's correctness.

**Concrete Vim rename limitation:** that pinned client does not explicitly
advertise `workspaceEdit.documentChanges`; its
[ApplyTextDocumentEdit](https://github.com/yegappan/lsp/blob/c741b2f1afb42b9f267824dac4049674908f4a41/autoload/lsp/textedit.vim#L217)
applies the edits without checking `textDocument.version`. Do not advertise safe
version-dependent rename to it. Recommend core Vim support with rename disabled
unless an upstream-supported version guard is available and tested. If full Vim
rename parity is required, select a maintained client with demonstrated version
enforcement or authorize a small client guard; do not build another semantic engine.
`prabirshrestha/vim-lsp` is maintained too, but its inspected
[workspace-edit helper](https://raw.githubusercontent.com/prabirshrestha/vim-lsp/master/autoload/lsp/utils/workspace_edit.vim)
also discards document versions, so switching names alone does not solve this.

Ship small `ftdetect`, `syntax`, `indent` and configuration adapters using `void`
filetype, four spaces and exact server/entry arguments. Include mappings for
supported navigation, hover, completion, signatures, diagnostics, formatting,
refresh and restart; disable unsupported features clearly. Syntax/indent rules
may be editor-native; name resolution and types remain exclusively in `voidls`.

### Native artifacts and lifecycle

Build Release server bundles from the pinned SDK on the existing platforms:
Ubuntu 24.04 x64, Windows x64 and macOS ARM64. Those are candidate distribution
targets, not proof of compatibility with every Linux/macOS release or CPU. Keep
separate archives usable by every editor. If approved, package the identical
server payload into target-specific VSIXs (`linux-x64`, `win32-x64`, `darwin-arm64`).
Use an explicit executable override for developer builds/unsupported targets.

Audit each built artifact's actual dependencies (`ldd`/ELF inspection,
`dumpbin` or LLVM PE inspection, `otool -L`), stage required permitted runtime
libraries/notices and launch on a clean machine without the developer SDK on PATH.
The measured local Linux server links system libz3, zlib, zstd, C++/C runtimes and
libm despite not linking the JIT. Copying that one executable is not a verified
portable distribution. Official SDK artifacts may differ. End users should not
need CMake/Node/LLVM headers just to run a prepared server bundle.

Keep graceful shutdown/exit and bounded client fallback termination; test process
death, bad protocol, restart during queued work, configuration races and no orphan
children. Add `processId` monitoring using platform APIs if supported; EOF alone
does not cover every inherited-pipe situation. Never log operational text to
stdout. Preserve UTF-8 Windows argv/binary stdio, Unicode physical roots and the
existing portable module-identity restrictions. Remote filesystem hosts, UNC,
virtual documents and browser editors remain explicitly unsupported until tested.

## 7. Measurements and demonstrated priorities

Release binaries were rebuilt at the baseline with Clang/LLVM 19.1.7. Measurements
used Linux x64, Ryzen 7 1700X, Python 3.14. They are warm local observations, not
cross-platform limits. Seven samples per operation; 70 definition requests.
The subprocess client uses real pipes and a method-not-found barrier after
diagnostics. The barrier is a harness technique, not an advertised production API.

| Generated workload | Modules / source bytes | CLI check median | Server startup median | Valid edit median / observed p95 | Definition median | Server RSS after edits |
| --- | --- | --- | --- | --- | --- | --- |
| Small, 11 functions | 3 / 522 | 7.53 ms | 4.15 ms | 0.65 / 0.89 ms | 0.093 ms | 8,800 KiB |
| Medium, 801 functions | 17 / 34,822 | 226.51 ms | 48.49 ms | 40.29 / 41.63 ms | 0.086 ms | 28,232 KiB |
| Large, 3,201 functions | 33 / 138,438 | 3,173.39 ms | 246.75 ms | 233.93 / 251.62 ms | 0.078 ms | 94,596 KiB |

CLI `check` currently calls `compile_project`, including typed IR lowering, so it
is not an analysis-only benchmark. Startup includes process creation, initialize,
initial diagnostics and barrier. Editing the open entry leaves imports on disk,
exercising existing fallback consistency checks. Generated modules are flat,
simple scalar functions; they do not represent every difficult type system case.

Existing source examples also passed seven repeated CLI/server runs:
`analysis/main` **7.02/3.95 ms**, `generics/main` **7.38/3.66 ms**,
`advanced/combined` **13.34/6.00 ms**, `async/spawn` **18.02/6.23 ms**
(CLI check/server startup medians). No source execution or runtime/JIT performance
claim follows from these analysis measurements.

Real VS Code **1.141.0** with the current extension and Release server measured
**259.67 ms** from opening to activated/ready/first verified definition, excluding
editor GUI startup. Seven invalid edits and repairs had medians **304.85/304.33 ms**;
70 definition commands had median **1.28 ms**. The pinned language client has a
250 ms pending-change delay (`lib/common/client.js`); that is a plausible component
of observed editor latency, not a complete causal profile. Tune it only with
before/after typing/burst tests; removing debounce can worsen full-graph backlogs.

Prioritize queued-edit collapse and duplicate diagnostic traffic. Cache position
adapters per immutable source only if allocation/query profiles justify it;
sub-millisecond bare definition requests are not currently the problem. Profile
larger CLI checks before changing lowering or compiler graph structures. Do not
remove validation, the second graph read, or exact byte checks to improve timings.
Memory figures are one workload endpoint/high-water observation, not leak evidence
or a bound on long-lived document identity history.

Proposed performance acceptance on this machine: no material regression of ordinary
queries; record before/after p50/tail observations for all supplied workloads;
20 queued edits should converge with bounded work to the newest revision instead
of 20 complete analyses. A practical target is within roughly two full-analysis
durations plus transport time after the burst, but this is an engineering target,
not a hard portable CI timer. Count analyses and assert final versions in tests;
keep shared-runner timing observational. Re-measure actual completion/signature/
rename/formatting once they exist.

## 8. Implementation order for one authorized sprint

1. **Settle the decisions below and establish tooling records.** Preserve baseline
   behavior; add compiler-owned scopes, missing binding categories, parameter/call
   metadata, source extents and tooling-only recording. Prove native contracts and
   ownership remain unchanged. Build the narrow incomplete-source completion path.
2. **Implement shared editor queries.** Broader definition/references, hover,
   signature help, completion and document/workspace symbols consume those records.
   Negotiate capabilities; keep invalid-source results honest. Optional semantic
   tokens follow only once these records are sound.
3. **Implement rename and formatting on the same snapshots.** Add rename preflight,
   capability/version guards and approved exclusions. Add compiler lexical trivia
   retention and the approved conservative formatter. Keep source rewrite policy
   independent of native reload.
4. **Integrate refresh and responsiveness.** Standard saves/watch notifications,
   explicit refresh, ordered batching and bounded lifecycle/cancellation handling.
   Reuse session acceptance. Measure the burst workload before choosing any deeper
   compiler optimization.
5. **Finish editor setup and native artifacts.** Discovery/entry/status UX in the
   existing extension; tested VSCodium installation; Neovim/Vim runtime files and
   exact setup examples; approved server archives/platform VSIX packaging. Adapt
   existing tests rather than make another harness framework.
6. **Consolidated acceptance and handoff.** Run focused tests during development,
   then one complete local Debug/Release/strict sanitizer and editor/package pass.
   Update feature contracts, known limitations and exact measured evidence.
   Wrench reviews; stop for Boss's commit/push and existing seven-job hosted gate.

Steps may overlap inside the sprint; they do not require repeated owner/CI gates.
If incomplete-source binding or cross-platform queue shutdown proves substantially
harder than expected, return to High for that concrete question. Do not split the
stage merely because a feature touches several files.

## 9. Genuine decisions for Boss and Wrench

| Decision | Recommended choice | Consequence/alternative |
| --- | --- | --- |
| Project scope/discovery | Explicit entry plus setup picker; standalone fallback; one project/window; symbols and rename cover its reachable graph | Automatic conventional entry selection, full-directory checking or multi-root routing expands the contract and tests. No new manifest needed for the recommended choice. |
| Rename policy | Versioned open edits, fresh closed-file bytes, selected graph only; reject incomplete symbol families/hooks | Closed-file changes after response remain an external race. Full-folder/global rename or stronger transactional guarantees requires more design. |
| Vim client/parity | Pin yegappan/lsp, Vim 9.1+ with tested exact versions; disable rename when version guarantees are absent | If Vim rename parity is required, approve a client/version guard solution and its tests first. Upstream's generic Vim 9.0 claim is insufficient for correct UTF-16 on old patches. |
| Formatting style | Four spaces, conservative spacing, comments/literals/EOL preserved; document formatting first | Tabs conflict with current grammar. Broad reflow/import sorting is unnecessary scope. |
| Distribution | Shared native archives; matching platform VSIX payloads when redistribution is authorized; explicit path override and documented PATH fallback | Repository metadata is currently UNLICENSED. Boss selects license/redistribution/signing/publication policy before public release; research creates no such authorization. Manual build/path remains a supported fallback. |
| Supported versions/platforms | Test current installed editor versions plus advertised minimums; begin native distribution with current CI architectures | Older/new architectures, remote hosts and all editor/version/platform combinations cannot be inferred from one Linux probe. Record tested cells and explicit exclusions. |

The research is not blocked on these answers. Implementation should obtain one
coherent authorization covering the chosen policies; no production work began.

## 10. Focused tests and final acceptance

Reuse `tests/analysis`, `tests/tooling`, `tests/sessions`, `tests/lsp`, the Python
pipe client and existing VS Code extension-host profiles. Keep probes here as
research evidence; migrate useful cases into the normal suites during engineering.
No new independent compiler/validation framework.

| Layer | Required focused evidence |
| --- | --- |
| Compiler metadata | Parameters, mutable/readonly locals, disjoint scopes, captures, receivers, loop/pattern bindings, aliases/visibility, named/default arguments, generic origins and implicit edges; returned records survive edits/destruction; unchanged native contracts |
| Incomplete input | Identifier prefix, `receiver.`, partial/nested calls, imports and unrelated errors; invalidated context suppresses semantic suggestions; ordinary source diagnostics unchanged |
| Protocol | Capability absence/fallback, full-sync versions, close/reopen, UTF-8/UTF-16 astral text, CRLF/CR/EOF/BOM, diagnostic related ranges, invalid UTF-8, malformed requests; preserve existing resource bounds |
| Rewrite safety | Rename conflict/rebinding, repeated names, named labels, read/write uses, generic source deduplication, excluded families, overlapping spans, versions and external disk refresh; formatting comments/strings/brace blocks, idempotence and semantic preservation |
| Refresh/scheduling | Dirty overlays versus saves/deletes/creates, previously missing imports, unreachable/removed nodes and clearing; edits arriving during analysis, out-of-order/foreign results, queue limits, query ordering, cancellation, EOF/shutdown/restart and bounded process cleanup |
| Real editors | Open/attach; completion selection and incomplete-input behavior; hover/signatures; diagnostics/repair/navigation; advertised rename/formatting; watched/manual refresh; config changes, crash/restart and disposal; no stale result application |
| Install/package | Install the actual VSIX in clean VS Code **and VSCodium** profiles, not only an extension development directory; launch native archives without the SDK; paths with spaces/Unicode; missing/wrong/incompatible server; trust/multi-root rejection; license notices and executable permissions |

Run protocol/compiler tests in the normal existing seven configurations at final
owner CI. Run GUI editor tests in relevant Debug jobs using existing Electron
testing, with headless Neovim/Vim probes for their actual clients. Extend those
jobs in the authorized engineering sprint; no workflow was dispatched or changed
by this research. Test VSCodium independently on each platform claimed supported;
likewise pin actual Vim/Neovim versions and clients for each claimed OS. If a cell
cannot be tested, mark it unverified and narrow the support claim. A Linux probe
does not establish Windows/macOS client compatibility.

Final acceptance requires all core advertised features working on the approved
matrix, every deliberately unsupported feature documented/negotiated away,
source identity and stale-result invariants retained, normal native behavior and
LLVM 19.1.7 preserved, no orphan servers, reproducible installation, before/after
performance evidence and no unresolved safety defect in advertised behavior.
Retain existing meaningful 1 MiB/frontend/depth and sanitizer gates without
inventing a second certification system. Separate local evidence from Boss's
final hosted lock.

## 11. Research result and stop

Locally reproduced: Release build; **6/6** focused compiler/session/LSP/Stage 6
integration suites; extension build/typecheck/unit runner; all **three** existing
real VS Code profiles; VSIX packaging; generated and real-source measurement
probes; real Neovim and Vim baseline probes. Evidence and limitations are retained
in [research/stage7](research/stage7/README.md). Initial experiment issues were
fixture names colliding with reserved `f32`/`f64`, disabled filetype detection in
clean editors and a read-only default Neovim state directory; the bounded probes
were corrected without changing production code or weakening assertions.

No new Stage 7 feature, performance optimization or cross-platform result is
claimed. VSCodium execution, minimum editor versions, cross-platform new client
support, safe Vim rename and production bundle portability remain unverified or
pending the explicit decisions/tests above. **Stop for Wrench review and Boss
authorization. HIGH RESEARCHES. MEDIUM ENGINEERS. WRENCH REVIEWS. BOSS AUTHORIZES.**
