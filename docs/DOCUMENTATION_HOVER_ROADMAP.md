# Compiler documentation and hover repair — engineering roadmap

25 October 2026. **Proposed; no implementation authorized by this report.**
Evidence and client limits: [research report](DOCUMENTATION_HOVER_RESEARCH.md).
Start from the officially locked Stage 11. Golden Wrench reviews this architecture;
Boss approves the remaining manual-invocation policy before Keyboard engineers it.

## Fixed scope

Implement Boss's optional plain `##` description, `@param NAME` and `@return`
contract. Function/method hover shows signature, description and applicable return
text. Parameter text appears only in the corresponding argument's signature help.
Unknown/duplicate parameter tags warn; partially documented parameter lists warn
for missing entries; wholly undocumented lists do not. Every such diagnostic is
nonfatal. Repair result/type/variant metadata coverage in the same compiler path.

Keep LLVM 19.1.7, frontend, typed IR, Runtime, GC, source identity, UTF-16 conversion,
snapshots and VFS protections. No language syntax changes, stdlib expansion,
editor tag parser, rich-markup framework, doc generator or repeated CI. No
production changes have been made in this research assignment.

## Dependency order

| Order | Work and existing owner | Completion evidence |
|---|---|---|
| 1 | Record the approved behavior and manual-invocation decision in the internal contract. Preserve `##` attachment rules; replace the superseded Language Bible §46 example during engineering. | Reviewer can read exact hover/help/warning expectations before code changes. |
| 2 | Add focused cases to existing frontend/tooling/LSP suites for the reproduced gaps. Use `tools/lsp_client.py`; no new harness. | Failures distinguish raw-tag leakage, missing warnings, undocumented help and statement-pattern/type-token gaps. |
| 3 | Extend compiler-owned documentation records and replace substring extraction with one small line-oriented helper adjacent to current collection. Reuse `SourceSpan`; bind tags against authored formal names. | Description/return/parameter fields and original tag spans agree for functions, methods and selected synthetic call targets. No LSP parsing. |
| 4 | Validate doc tags once per original callable header using existing warning plumbing, including uninstantiated generic headers. Allocate/document warning IDs in the existing diagnostic contract. | Zero/subset/all, unknown and duplicate cases have exact nonfatal warning counts/ranges. No per-specialization duplicates. |
| 5 | Populate analysis and editor-assistance metadata from the same record. Select callee/variant documentation explicitly before extracting parameter descriptions. Clear per-request state. | Named, reordered, defaulted, imported, receiver and generic calls use the selected declaration's docs and parameter mapping. No previous-callee leakage. |
| 6 | Factor the existing pattern metadata traversal for both expression and statement patterns. Reuse `result_editor_target()` and export references from `ResultSyntax` tokens to builtin type metadata. | `result`, constructors, `is` patterns, statement match arms and nested patterns have appropriate typed hover. Ordinary enum variants retain equivalent coverage. |
| 7 | Filter callable hover and completion in the existing presentation path; export signature help with only active parameter documentation. Return null according to the approved no-doc policy. | No raw tags or nonactive parameter prose leak through any callable display. Existing signature labels and active indices remain correct. |
| 8 | Verify shipped-client transitions with existing editor tests. Make a thin UI-only adjustment only for a reproduced default-client failure; keep semantic decisions in `voidls`. | Supported VS Code/VSCodium, Neovim and pinned Vim results record actual dismissal behavior and limitations. No arbitrary plugin guarantees. |
| 9 | Run focused Linux gates, review the bounded diff, and update internal handoff/diagnostic/editor documentation with actual evidence. | Wrench receives a concrete implementation and results; Boss controls Git and any final hosted acceptance. |

Steps 3–7 depend on the same small metadata improvement; do not implement separate
hover, signature and result documentation engines. Warnings must be established
before editor-only presentation so CLI and `voidls` use identical validation.
No native linking or Runtime feature work is required by these findings.

## Acceptance matrix

| Group | Required cases | Expected result |
|---|---|---|
| Attachment and plain text | LF/CRLF/CR, EOF, blank line, ordinary comment, indentation, Unicode/non-BMP description, imported source and unsaved comment edits | Existing attachment semantics preserved. Server documentation stays plaintext; locations use immutable original source and correct UTF-16 ranges. |
| Description/return | Neither, either, both; no-value return; literal markup-looking characters; inline `@param` in prose | Correct hover text and compiler signature; no accidental substring tag recognition. Check client rendering without treating client styling as a new documentation syntax. |
| Parameter tags | None, all, subset, unknown-only, unknown plus valid, duplicate, empty description, defaulted/variadic formal, implicit receiver, generic type parameter name | No missing warnings with zero valid descriptions. Otherwise one per missing explicit formal; unknown/duplicate warnings independently. No new errors. |
| Diagnostic ownership | Imported module, unused generic function/method/interface header, repeated specializations and repeated analysis | Original source spans and related first-tag location; one warning per authored issue per analysis. No warning multiplication or error promotion. |
| Hover/completion isolation | Function definition/use, method, alias/composition target, constructor/variant, completion details, parameter declaration/use, local capture | Callable signature/description/return only; `@param` prose absent outside contextual signature help. Captures keep local types. |
| Argument mapping | First/second argument, named/reordered labels, omitted defaults, bound receiver, overload/generic selection, nested calls and multiline calls | Active formal and text agree with compiler binding; no docs from outer/prior call or another specialization. |
| Suppression | All-undocumented callable and documented → undocumented → documented argument transitions; with/without trigger context; manual invocation policy | Current response is null when required; documented argument response contains only its text. Client transitions tested separately from protocol payloads. |
| Result metadata | `result<T,E>` written tokens, concrete and generic payloads, Ok/Err calls, `is` and statement `match`, nested arms, ordinary enum control cases, nullable-presence Ok | Correct type/variant role and payload names/types. No confusion between construction, matching and nullable presence. No fabricated source target or rename permission. |
| Snapshot/lifecycle | Full overlay edit/repair, stale version, invalid unrelated/current context, imported-file refresh, close/reopen and delayed signature response | Existing stale-result rejection and diagnostic clearing preserved. No cross-request documentation state. |
| Execution | One representative documented program with warnings and one without, check, normal/O2 JIT, existing linked-AOT route | Same program output/exit behavior. Warnings do not block compilation/publication or change semantics. No new execution matrix per tag combination. |

Keep existing meaningful binding and recovery tests. Where old tests expect a
signature from an undocumented fixture, document the fixture to retain that
binding test and add a separate explicit null test. Update the old assertion that
raw function docs belong at signature level. Do not remove semantic coverage or
mark failing UI tests as passing because the JSON response is correct.

## Focused test execution

During engineering, extend `tests/tooling/tests.cpp` and relevant frontend tests
for metadata/warnings; extend `tests/lsp/stage7.py`, `tests/lsp/stage8.py` or one
small adjacent docs-focused process test where it is clearer. Keep the existing
Client, fixtures and assertion style. Useful Linux gates are the affected
frontend/tooling targets and the LSP feature/discovery suites. The two existing
LSP scripts already pass on the matched locked Debug server in this research.

After the metadata repair, run affected Debug and Release gates, plus the
repository's existing strict sanitizer profile for changed ownership/metadata
paths. Reuse a small representative JIT/AOT test for nonfatal-warning behavior;
do not create many extra executable links just for documentation permutations.
If recursive semantic frame size is affected, run the existing focused 1 MiB stack
regression rather than introducing another validation framework.

Extend `editors/vscode/src/test/host.ts`, `tests/editors/neovim.lua` and
`tests/editors/vim.vim` for actual client behavior. An execute-provider assertion
proves data, not that a visible hint closed: verify both the response and window
lifecycle through the existing host capabilities, with a small manual observation
where UI state is inaccessible. Record the actual editor/client versions. Never
claim that `/usr/bin/vim` is actual Vim without checking it; on this research host
it resolves to Neovim. No fresh real-editor run was made here.

For Neovim, test shipped defaults first: a new null result must create no window;
typing/moving away from a documented argument should close its float. Its 0.11.5
public signature command can be made quiet with `silent=true`. If a stale float
survives under supported defaults, the smallest justified adapter change is an
owned signature-window lifecycle/request wrapper with buffer/version/cursor
guards. It must consume the server's result, never interpret tags or infer docs.
Overriding only the deprecated response handler does not cover the mapped public
function. Third-party automatic signature plugins remain outside the guarantee.

VS Code/VSCodium and pinned Vim already have a null-result closing path in source;
verify it before adding middleware or client hooks. Test cancellation/delayed
responses so an older documented response cannot reopen a suppressed popup.
Do not install new editor versions or trigger hosted jobs as part of research.

## Review gate and remaining decision

Boss has settled the documentation syntax, optionality, display separation and
parameter warning policy. The remaining product decision is **manual signature
help on an undocumented active parameter**. Recommend null for all invocations
for consistency and minimal code. If Boss wants a label-only manual exception,
use an explicitly supplied `Invoked` context; missing context cannot establish
manual intent. A Neovim adapter wrapper would be required for consistent support.

Wrench should also confirm the recommended small edge policies in the research
report: first nonempty duplicate text wins, only known/nonempty parameter docs
count toward missing warnings, and no invented warning categories for return
tags or unsupported markup. Those are implementation rules for approval, not an
invitation to add a richer documentation language.

The repair is complete only when the matrix has focused evidence, no raw
parameter tags leak into callable hover/completion, required warnings remain
nonfatal across execution routes, result-pattern/type references are present,
and supported-client limitations are recorded accurately. Return that bounded
result for Golden Wrench review and Boss authorization. Do not reopen the Stage
11 lock or bundle unrelated language/library work into this repair.
