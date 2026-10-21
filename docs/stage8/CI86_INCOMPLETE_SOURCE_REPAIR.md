# Incomplete-source completion crash and CI #86 editor assertion repair

21 October 2026. Baseline `58384781c0e3a544ca081b5abd2f8032646b5793`.
**Locally verified; Wrench review and native Windows verification pending.
Stage 8 is not hosted locked.**

## Exact crash and missing key

The source state is `u\n` prepended to the unchanged
`examples/runtime_errors/main.void`, opened as an unsaved overlay. Request
`textDocument/completion` at line 0, character 1. Before repair this reproduced
error **-32603**, stderr `Internal language server failure: map::at`, and native
exit **2**. A retained baseline Release executable reproduced the exit; the
Debug debugger trace identifies the missing key as **`$application`** in
`std::map<std::string, vd::EditorCandidate>::at`, reached from
`Semantics::editor_candidates()` through `editor_assistance()` and
`validated_project()`.

The synthetic application function was correctly parsed and registered in the
unit's declaration map. Completion enumerated that declaration, resolved it, and
called its existing candidate-insertion lambda. That lambda intentionally rejects
non-source identifiers such as `$application`. Documentation attachment then
assumed insertion succeeded and indexed the separate **candidate map**, where
the key did not exist. This is not a missing declaration-registration problem.

The fix retains those eligibility rules, skips invalid source identifiers before
resolution, and returns an accepted candidate pointer from the existing insertion
lambda. Numeric, method, function and constant documentation is attached only to
that accepted candidate. Rejected synthetic names and poisoned result types can
no longer produce a map lookup. No blanket exception handler, invented declaration,
new compiler pipeline or relaxed analysis/IR admission was added. Adjacent numeric
and text/collection assistance suites retain their documentation regressions.

## Incomplete editing and lifecycle coverage

- Direct compiler tooling tests exercise 13 invalid top-level/declaration states,
  hidden-name filtering, unchanged overlays, retained valid snapshots and rejection
  of invalid source as validated analysis.
- The existing Sprint 6 stdio LSP suite adds 16 top-level fragments, five local
  expression/closure fragments and **172 character-by-character prefix states**
  through application/closure, poisoned function signature, import and generic
  declaration editing. Each sequence restores valid source and requires working
  navigation in the same server. Top-level states request completion, hover and
  signatures; valid empty/list completion responses remain supported. The suite
  shuts down cleanly with exit 0. Existing UTF-16, formatting, readonly/navigation
  and diagnostic tests remain enabled.
- Both repaired Debug and Release servers replay the exact runtime_errors `u`
  overlay, return safe completion, restore the original imported definition in
  `arithmetic.void`, retain the same process and exit cleanly with status 0.
  The example's disk bytes remain unchanged.
- Actual VS Code 1.141.0 uses its real `type` command for seven partial keyword,
  identifier, expression, function and import states before imported source. It
  requests completion, rejects leaked synthetic labels, checks the same native
  child remains running, then requires valid definitions and cleared diagnostics
  after repair. Original automatic import, navigation, failure/restart, trust,
  multi-root and shutdown assertions still execute.

`this.task is not a function` did **not** reproduce after the crash correction.
Both real editor configurations passed intentional transport-error/kill/restart
and shutdown checks with zero unhandled rejections; neither log contains that
message, map::at or an internal language-server failure. No speculative client
lifecycle or dependency change was made. Native Windows client behavior is still
a hosted verification requirement.

## Separate CI #86 failure

[Windows Debug job 119205355324](https://github.com/Shmellyorc/Void.Language/actions/runs/39595103500/job/119205355324)
passed **156/156** compiler suites. Actual editor integration later failed at
`host.js:388`: selected fallback `C:/.../worker space.void` differed only in slash
representation from the expected `C:\...\worker space.void`.

The three adjacent selected-entry assertions (fallback, fixed selection, deliberate
restart) now compare `path.normalize(actual)` to `path.normalize(expected)`.
Missing entries still fail, and full roots/basenames/case remain part of the exact
comparison. Production paths, containment, source identities and alias rules were
not changed. All CI #82–#85 discovery/Unicode/drive/security code is preserved.

## Final focused results

| Check | Result |
| --- | --- |
| Linux Debug focused compiler/LSP/application suites | 10/10 final unique outcomes |
| Linux Release, same suites | 10/10 |
| Extension TypeScript/compile/bundle | Passed |
| Explicit extension unit tests with each server | 4/4 each, zero skipped |
| Actual VS Code trusted/restricted/multi-root profiles, including real typing | 3/3 Debug and 3/3 Release |
| Exact runtime_errors replay and clean shutdown | Passed in both configurations |
| Python syntax, whitespace and preserved-source checks | Passed |

The eight main suites were `frontend`, `analysis`, `tooling`, `lsp_sprint6`,
`lsp_stage8`, `lsp_protocol`, `lsp_stage7`, `application_native_sprint4`; two focused
builtin-assistance suites, `lsp_sprint2` and `lsp_sprint3`, also passed per build.
Reruns do not increase the **20/20** unique suite total. No full matrix was run.

Two initial new test assertions were corrected: an empty struct in that context
was accepted by the existing parser, so it stays in LSP safe-recovery coverage
rather than the invalid-compilation list; import completion legally returns an
LSP CompletionList rather than only an array. The tests now accept both protocol
forms while checking every returned item. Initial 6/8 Debug outcomes, debugger
sandbox restrictions, corrected assertions and final focused reruns remain in
the [machine-readable evidence](ci86-repair-results.json), with source/log hashes.

No native Windows editor run occurred locally. Require Windows Debug to execute
the corrected assertions and typing/recovery profiles, and preserve Release and
all other owner-controlled hosted gates. No hosted action, commit/push/branch,
root README edit, website change or Stage 9 implementation occurred.
