# Compiler documentation and hover repair — research

25 October 2026. **Research only; engineering awaits Golden Wrench review and
Boss authorization.** Companion: [ordered roadmap](DOCUMENTATION_HOVER_ROADMAP.md).

## Scope and evidence

Repository inspected at `7895608714c63451ff0c5c27eab7c02b42230d0c`. Stage 11 is
locked at tested commit `e8633b246fe6139d39d830afd4cb249f7dab4d8e`, with the
recorded 7/7 jobs and 1,407/1,407 CTests. That acceptance did not establish the
documentation behavior requested here. `docs/README.md`, `WRENCH_CLANKER.md` and
`LANGUAGE_SERVER.md` explicitly keep this repair separate from runtime globals.
Older role/status text and Language Bible §46's `## position: ...` example are
historical where they conflict with Boss's new `@param`/`@return` instruction.

Inspected compiler comment collection, parser/semantic records, pattern handling,
analysis export, tooling index, warnings, stdio LSP implementation, shipped editor
adapters and existing tests. No production, test, workflow, root README or Git
changes were made for this research. No builds or hosted jobs were launched.

Reproductions used the existing Python `tools/lsp_client.py` and temporary source
fixtures. Key findings were repeated with the Debug binaries whose SHA-256 values
match [Stage 11's artifact inventory](stage11/BUILD_ARTIFACTS.json):

| Artifact | Existing path | SHA-256 |
|---|---|---|
| `voidls` | `build/step42-ci96-debug/voidls` | `7b236fb21f01a99650f3a6fa6b4162a07b3034464dfde0c3355d196ee14f83b1` |
| `voidc` | `build/step42-ci96-debug/voidc` | `ecc6d14b39becc2db13af7a70f3b720e4e6c2fe33d42aba7e54c384132255728` |

Initial exploratory probes also used `build/ci-official-debug/voidls` and `voidc`;
those were not identified by the Stage 11 inventory and are not substituted for
the matched artifacts above. In particular, the inline-tag and reordered-named-
argument observations below were exploratory, corroborated by current source.
This is Linux protocol/CLI evidence and client-source review, not fresh interactive
VS Code/VSCodium, Neovim or Vim acceptance.

## Implemented behavior versus missing behavior

| Area | Verified current behavior | Required repair |
|---|---|---|
| Collection | Contiguous `##` lines immediately before a declaration are collected. Blank lines/ordinary comments break attachment. LF, CRLF and CR fixtures retain descriptions. | Keep this attachment rule and original source locations; parse the two approved tags within the collected block. |
| Function hover | Compiler signature plus the entire raw block, including every `@param` and literal `@return`. Plaintext LSP hover already exists. | Signature, description and applicable return text only. |
| Parameter extraction | First substring matching `@param NAME ` supplies one line. | Recognize tags at the start of a documentation line and bind to real formal parameters. |
| Signature help | Signature-level raw block plus all parameter documentation entries; results are returned for undocumented active parameters and completely undocumented functions. | Active parameter's documentation only; suppress automatic help when it is absent. |
| Tag warnings | No documentation-specific validation. Partial, unknown and duplicate tags all pass `voidc check` with empty stderr and empty LSP diagnostics. | Nonfatal missing/unknown/duplicate parameter warnings with Boss's conditional missing rule. |
| `result<T,E>` annotation | Hover on the written `result` token returns null. | Compiler-owned type documentation and a reference from the written token. |
| `Ok(...)` / `Err(...)` calls | Typed synthetic declarations, signatures and common description exist. Both variants currently describe `Ok(null)`; parameter documentation is empty. | Reuse these declarations, with specific descriptions and payload documentation. |
| `if r is Ok(var a)` | `Ok` hover resolves to the synthetic constructor metadata; `a` has typed local hover. | Preserve typed binding; describe the selected variant without implying that a pattern constructs a value. |
| Statement `match r: Ok(var b)` / `Err(var e)` | Variant-token hover returns null, while captures still hover as `b: i32`, `e: string`. | Export the missing pattern references using existing semantic targets. |
| Editor integration | All clients use `voidls`; adapters contain no documentation parser. | Keep that architecture. Client UI differences are described below. |

## Why the current behavior occurs

### One raw string does several jobs

[`source_documentation()`](../src/frontend/compiler.cpp#L542) walks backwards over
preceding `##` lines, removes leading indentation and the marker, and returns one
string. [`parameter_documentation()`](../src/frontend/compiler.cpp#L557) uses
`find("@param " + name + " ")`; it does not parse a line or validate a formal.
Consequences include silent first-match duplicate handling and an inline sentence
such as `## Mention @param x Accidental` being misread as parameter documentation.
There is no separate return field or tag location information.

`AnalysisAccess::build()` attaches source documentation to exported declarations.
`editor_call()` separately finds a function's raw comments or builtin record and
extracts its parameter strings. Completion has additional source-documentation
lookups. This is shared compiler infrastructure, but the different consumers
currently receive insufficiently structured metadata. Synthetic enum call handling
also assigns case documentation **after** `editor_call()` extracts parameters;
engineering should cover this path when making documentation selection explicit.
That ordering is a source finding, not a demonstrated enum signature regression.

In [`src/lsp/main.cpp`](../src/lsp/main.cpp#L389), signature help returns any
nonempty signature, exports raw function documentation at signature level, and
does not inspect the active parameter's documentation. Hover appends the same raw
declaration block. Removing just the parameter-level strings would therefore leave
the descriptions visible through the signature-level block. Completion would also
continue leaking tags unless it uses the same filtered callable presentation.

### Result metadata exists, but not every written use reaches it

`Unit::make_result()` canonicalizes the actual payload types.
`result_editor_target()` constructs typed `$result::...::Ok` / `Err` declarations
through the existing `builtin_record()` path. Constructor expressions and
`check_pattern()` already invoke it. This is a metadata coverage problem, not a
missing result implementation or a reason to add stdlib wrappers.

The expression visitor in `AnalysisAccess::build()` walks `Expr::patterns` and
exports references from their written variant tokens. The statement-body visitor
walks `Statement::pattern` payload expressions and children but omits the variant
reference. This explains the reproduced difference between an `is` expression
and statement `match` arms. The omitted operation also concerns ordinary enum
patterns; factor the small pattern metadata traversal rather than fixing only the
strings `Ok` and `Err` in the LSP server.

`result_type()` retains written tokens in `Unit::results` (`ResultSyntax`), and
resolution retains the concrete result type. Analysis export does not currently
publish those tokens as builtin type references. Reuse this record for annotation
hover; do not scan source text again. Preserve symbolic/generic contexts without
forcing unused generic bodies to instantiate just to obtain documentation.

Pattern captures are ordinary typed locals exported from `editor_bindings`.
Their current short hover is correct: the capture is neither the function's
parameter nor the result variant. Do not copy a callee's `@param` text onto it.
Also preserve the existing distinction between result `Ok` and nullable-presence
`Ok`: the latter follows a separate semantic branch and must not acquire an
`Err` alternative or result-construction description.

### Warning plumbing is already available

`Diagnostic`, `Severity::warning`, immutable `SourceSpan`, related locations,
`Diagnostics::warn()`, successful-analysis warnings, CLI rendering and LSP warning
publication already exist. `Diagnostics::failed()` checks error severity, and
`src/main.cpp` prints successful-analysis warnings before check/run/emit/build.
No warning framework or new compilation-failure path is needed.

Validate authored callable headers once, including unused generic declarations,
methods and interface requirements, without validating unused bodies. Use original
source identities when a declaration is specialized or reused through composition.
The existing warning helper deduplicates by source location and code: give missing
parameters their own formal-token spans, and unknown/duplicate tags their own tag
spans, so distinct warnings are not accidentally merged. Assign and document stable
warning codes using the existing diagnostic contract; do not reuse the semantic
duplicate error merely with new prose.

## Reproduction notes

A representative checked fixture was:

```void
## Describe foo.
## @param x Payload.
## @return Number.
func foo(x: i32, y: i32) -> i32:
    return x + y
func ok() -> result<i32, string>:
    return Ok(1)
func main():
    var r: result<i32, string> = ok()
    if r is Ok(var a):
        print(a)
    match r:
        Ok(var b):
            print(b)
        Err(var e):
            print(e)
    print(foo(1, 2))
```

`voidc check` exits 0 with no warning. Hovering `foo` includes both raw tags;
hovering the annotation's `result` returns null. Hover on `Ok(1)` and the `is Ok`
token succeeds; both `match` variant tokens return null. Capture hover succeeds.
For signature help, replace the last line through a versioned unsaved overlay
with `    print(foo(` and request at end of line, then change it to
`    print(foo(1, ` and repeat. Both requests return a signature; the latter has
`activeParameter: 1` and empty parameter documentation but still includes the
function's raw block. Replacing the comments with unknown/duplicate tags gives
the same successful compilation and no warnings.

Use UTF-16 positions derived from the actual overlay bytes, as the existing client
tests do. A request inside a numeric literal returns null by an intentional
`editor_assistance()` guard; that is not evidence that signature help is absent.
The successful probes used an incomplete argument position or identifier context.
Temporary fixtures were removed; no permanent test or instrumentation was added.

Existing `tests/lsp/stage7.py` and `tests/lsp/stage8.py` both passed against the
matched Debug server. Stage 8 reports its existing Windows-8.3 skip on Linux.
These tests establish retained infrastructure behavior, not acceptance of the
new contract: Stage 8 currently asserts raw signature-level documentation.

## Protocol and editor limits

LSP 3.17 defines signature help's result as `SignatureHelp | null`, optional
signature/parameter documentation, an active parameter, and optional trigger
context. It does **not** define an unsolicited server notification to close a
client's parameter-hint window. Empty documentation alone leaves a valid signature
available for display. Returning null is the appropriate server response when
automatic help is intentionally unavailable. An out-of-range active parameter or
`activeParameter: null` is not a 3.17 dismissal mechanism.
[Official 3.17 signature-help definition](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/signatureHelp.md).

| Shipped target | Verified source behavior | Consequence and minimum action |
|---|---|---|
| VS Code / VSCodium, extension targeting VS Code `^1.141.0`, languageclient `10.1.2` | The library forwards signature requests/results. VS Code's parameter-hint model cancels a current hint when the provider returns no result/no signatures. | Return null for an undocumented argument; no extension semantic middleware is justified. Verify actual show → null → hide and show again in the extension host. A VSCodium build must use matching VS Code behavior; it was not executed here. |
| Neovim `0.11.5+`, builtin client | Shipped adapter maps normal-mode Ctrl-K to `vim.lsp.buf.signature_help`; it does not install automatic signature triggers. In 0.11.5 the public function returns on no signatures and can notify. The legacy response handler also returns on null. Neither null branch explicitly closes an existing window. Default floating-preview events close on `CursorMoved`, `CursorMovedI`, `InsertCharPre`. | Null prevents a new hint; normal typing/movement usually closes the old float independently. Do not promise that null itself dismisses it under custom close-event/plugin settings. Use `silent=true` for a quiet no-help command if needed. Add an owned-window wrapper only if a supported-default editor test demonstrates a real stale window. |
| Vim `9.1+`, yegappan/lsp `c741b2f1afb42b9f267824dac4049674908f4a41` | Signature handler closes its popup for an invalid/null result; it checks request cursor/changedtick context. The pinned client supports active-parameter documentation and insert-mode triggers/retriggers. | Server null should close the owned popup on a current reply. Verify it on actual Vim, including delayed replies and movement. No client fork or editor tag parser is needed. |

Source references: [VS Code model, lines 216–224](https://github.com/microsoft/vscode/blob/1.141.0/src/vs/editor/contrib/parameterHints/browser/parameterHintsModel.ts#L216-L224),
[Neovim public function](https://github.com/neovim/neovim/blob/v0.11.5/runtime/lua/vim/lsp/buf.lua#L350-L369),
[Neovim legacy handler](https://github.com/neovim/neovim/blob/v0.11.5/runtime/lua/vim/lsp/handlers.lua#L411-L448),
[Neovim floating-preview defaults](https://github.com/neovim/neovim/blob/v0.11.5/runtime/lua/vim/lsp/util.lua#L1529-L1536),
[pinned Vim handler](https://github.com/yegappan/lsp/blob/c741b2f1afb42b9f267824dac4049674908f4a41/autoload/lsp/signature.vim#L1003-L1026).

Do not attempt a Neovim fix solely by overriding the deprecated response handler:
the mapped 0.11.5 public function has its own request/result/rendering path.
Arbitrary third-party signature plugins and custom UI settings cannot be
controlled by the language server. Suppression applies when a current request is
made; clients choose when to request/retrigger, so this is not a promise of a popup
on every cursor movement after a session has closed. Signature help also does not
control completion-detail windows or an explicit hover command.

Send documentation as plaintext (explicit `MarkupContent` when supported, with
the permitted plain-string fallback), never markdown/HTML. Client presentation
is still client-owned: Neovim's utility wraps the generated signature label in a
code fence for syntax highlighting and converts documentation into its markdown
preview lines. This does not authorize a rich source-documentation language.
Include literal underscores, angle brackets and markup-looking text in client
checks; do not promise identical typography or add a markup framework to the
compiler to compensate for client rendering.
[Neovim signature conversion](https://github.com/neovim/neovim/blob/v0.11.5/runtime/lua/vim/lsp/util.lua#L720-L793).

## Recommended bounded design

1. Extend existing compiler documentation metadata with description, optional
   return text and per-formal descriptions, retaining source spans for diagnostics.
   One small line-oriented extractor handles collected comments; builtin records
   populate the same shape directly. No registry, markup renderer or second parser.
2. Recognize only line-start `@param NAME DESCRIPTION` and `@return DESCRIPTION`.
   Ordinary lines remain description text. No implicit continuation, rendering of
   markup, or additional tag vocabulary. Preserve source text/UTF-8; recognize
   tokens without rewriting VFS sources. Parameter matching is case sensitive and
   uses declared names, excluding implicit receivers and generic type parameters.
3. Conditional missing warnings count known parameters with nonempty descriptions.
   Zero valid descriptions means no missing warnings. Unknown tags still warn;
   repeated names still warn. A documented subset triggers one warning for each
   undocumented explicit formal, including defaulted/variadic formals. Attach a
   duplicate's related location to its first tag. Warnings never invalidate a
   successful analysis, run, emit or native build.
4. Render callable hover/completion from signature, description and applicable
   `Returns: ...` text. Never include parameter descriptions there. In contextual
   signature help omit signature-level documentation and nonactive parameter
   descriptions; include the active description only. Keep all parameter labels
   and existing compiler argument mapping. This prevents client fallback from
   exposing the wrong parameter's text.
5. Reuse canonical typed result/variant metadata in calls, patterns and completion.
   Export missing pattern/type-token references as editor metadata. Builtins stay
   builtin: no fictitious file navigation, editable prelude or rename targets.
   Do not alter value storage, matching, GC, Runtime, native ABI or error semantics.
6. Keep docs out of native compatibility contracts. Reuse existing snapshots,
   overlays, refresh, invalidation and diagnostic publication. No new caching or
   background documentation service. Avoid adding large by-value metadata to
   recursive semantic frames; preserve existing small-stack protections.

Recommended minor edge policy: first nonempty description wins on repeated
parameter tags, while every duplicate warns. An empty recognized tag supplies no
popup. For repeated `@return`, use the first nonempty text without inventing another
warning category; omit return presentation for no-value functions. Unknown tag
spellings remain literal prose. These are proposed deterministic handling rules,
not claims of prior Boss decisions.

## Remaining decision and risks

**One user-visible choice needs Boss confirmation:** should a manually invoked
signature command show labels for undocumented parameters? Recommend uniform null
for undocumented active parameters for the smallest consistent repair. Boss's
explicit requirement mandates automatic suppression and does not settle manual
requests. A manual-label exception can use LSP trigger context when supplied, but
0.11.5 Neovim's mapped public function does not supply that context, so it would
need a small adapter request wrapper for consistent behavior. Do not infer a
manual request from a missing context. Confirm this policy before engineering.

Other risks are bounded implementation/review matters: warnings duplicated by
generic instantiation, incorrect original source ownership, named/default/receiver
argument mapping, stale client responses, and comments accidentally entering ABI
contracts. Existing undocumented APIs may lose automatic signature popups under
the approved rule; do not automatically generate parameter prose or expand the
stdlib to hide this change. Where maintained builtin documentation is supplied,
store it once with its existing compiler record.

Client source supports the stated recommendations; actual supported-version UI
transitions remain acceptance work. No universal third-party-client dismissal
guarantee is possible through LSP 3.17 alone.
