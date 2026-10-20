# Stage 7 implementation and local acceptance

20 October 2026. Boss authorized production engineering after the Stage 6 lock
at `4139297f92c195782f59a28546444e6b3bec4ad8`. This supersedes the research-only
restrictions in the dated roadmap and older handoffs. Stage 7 is **not locked**.
Git history, branches, remotes, hosted CI and public publication remain owner-controlled.

## Shared compiler service

Every adapter runs the same `voidls`, using the existing frontend, resolver,
`ToolingIndex` and `ToolingSession`. LLVM/Clang/LLD remains **19.1.7**. There is no
editor parser/type checker, worker pool, incremental compiler or persistent
analysis cache. Runtime/native execution and reload contracts are unchanged.

The 1 MiB gate exposed both a new cold-probe temporary in a recursive semantic
frame and pre-existing large capability/lowering case frames on legal receiver
chains. Cold editor argument construction and narrow noinline case boundaries
repair these without changing nesting limits, capability operations, IR evaluation
order or native contracts. The permanent depth regression now accepts 16/64/96/124
calls and retains the original deep-chain diagnostics/recovery assertions. Local
before/after evidence is in [semantic-stack.json](stage7/semantic-stack.json).

- Full-sync diagnostics and UTF-16 navigation retain exact owned source snapshots.
  Additional tooling-only bindings cover ordinary parameters, mutable declarations,
  written assignments, loop/pattern bindings, captured variables and explicit
  import aliases. These records are separate from native compatibility metadata.
- Hover uses plaintext signatures, visibility, constant/readonly qualifiers,
  receiver effects and representable integer constants. Its range belongs to the
  queried document, including cross-file references.
- Completion uses the current source and the normal compiler binder. It lists
  visible locals/parameters, declarations, import qualifiers and compiler-selected
  members, plus bounded syntax candidates. Type contexts use normal type lookup.
  Results are plain text edits, not snippets or guessed documentation.
- Cursor recovery replaces only the current identifier with a private probe and
  closes at most 32 unmatched parentheses on that line. The compiler returns
  owned query data at that probe; no recovered tree becomes `ValidatedAnalysis`
  or IR. Normal diagnostics always use the original bytes. Broken imports, invalid
  enclosing contexts, unknown/nullable/private receivers and lexical errors do not
  supply semantic candidates. Unrelated body errors can coexist with useful results.
  Existing call-callee replacement can be suppressed; incomplete member access and
  argument/name contexts have executed positive coverage.
- Signature help reads the resolved compiler call and parameter mapping, including
  nested calls and named custom-initializer arguments. Optional parameters are
  marked. Implicit field constructors and arbitrary callable-value contexts are
  outside this initial signature-help contract.
- Document symbols use source declarations, deduplicate accessors into properties,
  and expose enum/event kinds. Clients can request DocumentSymbol ranges or the
  flat SymbolInformation fallback. Workspace search is capped at 256 results in
  deterministic declaration order, within the selected entry-reachable graph.

Recovery does not repair arbitrary incomplete blocks, multiline calls, broken
signatures or imports. Readonly receiver queries conservatively omit methods when
final receiver effects are unavailable in recovery. Generic declarations retain
source identities; unused generic bodies are not independently validated for an
editor query. Semantic tokens and general code actions are not advertised.

## Rename and formatting safety

Rename covers ordinary non-generic functions, module/local constants and supported
uncaptured local/parameter bindings. Types, fields, import aliases, generic
families, closures' bindings, methods, properties, virtual/interface families and
language hooks are rejected. Names must pass the compiler's identifier policy;
`entry`, `init`, `deinit` and `call` are excluded as replacements.

Only exact declaration/written-reference spans participate. Proposed edits are
applied to immutable temporary overlays and revalidated. Every declaration and
written binding is compared at its translated source position, so successful
compilation alone cannot authorize legal rebinding. Spans are deduplicated and
checked for overlaps and exact spelling. Current selection and all closed module
bytes are checked before returning. The server never writes source files.

Open edits carry the exact signed LSP version in `documentChanges`; closed files
carry null versions. Rename is advertised only with documentChanges plus
`textOnlyTransactional` failure handling, or the shipped adapter's explicit
version-guard contract. The Neovim adapter preflights **every** open target version
(including zero/negative versions) before any edit. The pinned VS Code client
validates document versions before returning rename edits; adapter middleware also
rejects target model changes/close/reopen during the request. A newly loaded dirty
Neovim target cannot consume an unversioned closed-file edit. Vim rename remains
disabled because its client does not enforce that contract.

Formatting requires `{tabSize: 4, insertSpaces: true}` and a current valid graph.
The compiler's lexer retains token spans and reads original trivia gaps; it emits
four-space indentation and conservative spacing without rewriting comments,
literals, imports or line endings. Lexical tokens and indentation relationships
must remain identical; the entire proposed graph must validate with the same
native contract. A second formatting pass must produce identical bytes. Ambiguous
or invalid cases return no edits. There is no reflow, import sorting or on-type
formatting. The shipped adapters guard formatting results against document changes.

LSP formatting edits have no version field. Use the guarded adapter commands;
third-party clients own their application/undo and freshness behavior. Neither
rename nor formatting promises filesystem atomicity against an external writer
after the response.

## Synchronization, scheduling and lifecycle

`--entry` selects one graph; the entry parent fixes the import root. Workspace
folders never grant additional import access. Open unreachable files do not gain
independent semantic analysis. VS Code/Neovim retain an explicitly labeled/fixed
standalone-file fallback; Vim can select that standalone file as its entry.

`didSave`, `workspace/didChangeWatchedFiles` and `void/refreshProject` invalidate the
selection through `ToolingSession.refresh()`. Dirty overlays stay authoritative.
VS Code watches `.void` files under the selected root. Other clients can use the
explicit refresh command when external disk state changes.

The synchronous server drains a bounded group of ordered messages (64 messages,
less than 16 MiB of bodies) before analysis. Requests are analysis barriers;
mutations collapse into one current snapshot. A 10 ms quiet wait and 50 ms/64-event
budget prevent an idle edit from waiting indefinitely. Queued cancellations can
overtake a request, receive -32800 and never reorder document changes. Unknown or
completed cancellation IDs are discarded. Running compiler work is not interrupted.
Shutdown drops pending analysis, exit/EOF retain their lifecycle behavior, and
platform process checks terminate a server whose declared editor process exits.
Input uses a single 64 KiB transport buffer; no analysis worker/history queue exists.

## Installation and evidence

See [editor setup](EDITOR_INTEGRATION.md) and
[local evidence](stage7/ACCEPTANCE.md). Prepared archives/VSIXs are private local
acceptance artifacts. The repository remains UNLICENSED; no public distribution,
license grant, Marketplace/Open VSX publication or signing is authorized. Native
runtime dependency audits accompany archives; matching system libraries remain
required. A successful local extraction/launch is not clean-machine portability
or Windows/macOS verification.
