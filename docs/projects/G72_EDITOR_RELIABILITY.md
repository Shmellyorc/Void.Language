# G72 VOID editor reliability

Baseline `01019ced83c334233b4a593a4d05b198ebf8ec4d` (G71). Development took place on
`codex/g72-editor-reliability` in a separate Git worktree. The owner's checkout
and TestGame source were kept in place. Tests used copies or fresh projects.

## Reproduction

| Edit in a copy of TestGame | G71 result | G72 result |
| --- | --- | --- |
| Replace a valid raylib call with four spaces followed by `rl.` | The server returned zero semantic tokens. It returned 256 suggestions and omitted `set_target_fps`. | The server returned the other tokens and 1,112 suggestions, including `set_target_fps`. Problems kept the incomplete-member diagnostic. |
| Continue to `rl.set_t` | Zero semantic tokens. Prefix completion found six suggestions. | Existing tokens stayed colored and the six suggestions included `set_target_fps`. |
| Paste a tab before `rl.` inside the space-indented function | `VOID1002`, zero tokens and zero suggestions. | `VOID1002` remained visible. Coloring and completion worked because the tab reached an existing indentation column. |
| Use VS Code's verbose setting, then kill its test server | The Output log showed startup and exit. Verbose protocol traffic was missing at the default VS Code log level. Operation messages lacked consistent severity and crash context. | Full protocol tracing is available separately. The operational log identifies the project and executable, reports the crash with tracing off and records the exit signal. |

The extension forced tabs by default even in an existing space-indented file.
The strict lexer correctly rejected the mixed indentation. The parser's
editor recovery handled several local constructs, but a standalone unfinished
module selection still failed binding. A partly typed member also produced a
semantic diagnostic outside the supported recovery cases. Those failures left
semantic-token requests with no usable index. The completion probe separately
capped the qualified results at 256 without telling the editor the list was
incomplete. The language client sent trace output through a log method that
VS Code filtered at its default log level.

## Changes

- The extension follows existing file indentation unless the developer explicitly
  sets `void.editor.useTabs`. New unindented files still start with tabs.
- Editor recovery keeps the real source bytes and compiler-verified receiver.
  The presentation pass can reuse an existing indentation column and keep
  unrelated resolved tokens around an unfinished selector. The normal compiler
  pass still reports syntax, indentation and binding failures. A recovered
  namespace receiver contributes color while an unfinished member is being typed.
- Qualified completion gathers names from the resolved module, applies the
  prefix before lookup, and can return up to 2,048 matches. It identifies a list
  beyond that limit as incomplete. Replacing a member name preserves existing
  call parentheses. The extension checks the document version before using a
  completion response.
- **Output → VOID** records startup, successful operations, warnings, failed
  operations, persistent compiler diagnostics and `CRASH` events. **Problems**
  still updates with live analysis. The Output channel debounces diagnostics
  and reports up to four new warnings or errors per file in one update.
  **Output → VOID LSP Trace** follows `void.trace.server` independently.
  Changing the trace level takes effect in the running connection.

Only the presentation pass admits the recovery cases. Imports and their
visibility still go through the compiler. The package lock, native provider
admission, CLI diagnostics and native execution approval remain authoritative.
An unresolved import, ambiguous scope or unrecoverable syntax may still prevent
semantic analysis. The extension retains syntax highlighting in those cases.

## Coverage

- The LSP regression exercises an ordinary struct, a struct and method
  imported from another VOID file, a locked package with more than 256
  exports, UTF-16 positions, an existing call, repeated completion and
  mixed indentation. It requires strict CLI rejection of the mixed source.
- The VS Code host regression types a dot and a prefix, checks semantic
  tokens and completion, switches all trace levels, tests output severity,
  kills a server and restarts it. The physical-raylib host also checks
  that another imported function keeps its semantic role during the edit.
- A desktop run on Linux with VS Code 1.144.0 checked the rendered raylib
  colors and the real suggestion widget. It also verified namespace,
  function, constant, type and field token roles, repeated completion, clean
  repair, crash reporting with trace off and recovery in a new process.
- Existing development-host tests covered navigation, documentation,
  configuration, restart and Restricted Mode. The workbench lifecycle also
  passed project creation, Run, Build, F5 breakpoint and stepping, and
  library management. The native importer and headless bundled-raylib
  tests covered real C and C++ imports, JIT, AOT, lock validation and the
  terminal approval boundary.
- Ubuntu Release passed 44 of 45 selected tests. The one failure,
  `lsp_async_syntax`, reproduced with G71. The test expects a second
  diagnostic after an earlier parse error. The focused fatal ASan/UBSan
  suite passed all 10 tests. Extension build and config tests passed all
  eight tests and the VSIX passed ZIP integrity verification. The
  installed-VSIX host passed trusted, Restricted Mode and multi-root cases.
- The physical-raylib host passed in VS Code 1.141.0. VS Code 1.144.0 passed
  the rendered-color and completion checks with both a private VSIX install and
  the owner's installed extension. The owner-installed extension also ran
  Check, Run and Build using the actual installed compiler.

Logs, artifact hashes and desktop receipts are in the sibling
`Void.Language-G72-evidence` directory. Windows and VSCodium were outside the
local verification performed here.

## Installed and verified

| Tool | Version or build identity |
| --- | --- |
| `/home/shmellyorc/.local/opt/void-g72-editor-reliability/bin/voidc` | VOID 0.1.0 development, LLVM 23.1.4, build `d4c491b816b840478c4b0c25ee8a00b7b1a9d1a40297e92fa86116dd0420fb78` |
| `/home/shmellyorc/.local/opt/void-g72-editor-reliability/bin/voidls` | LSP server version 0.2.0, SHA-256 `d4277b5a8526d7dca5836b317d8e1e921e05da575637d16c9b07552314b7f381` |
| `/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.1` | Extension 0.1.1, VSIX SHA-256 `7d2a076f33927ccf90036c64fa12619ef601c2304ce30ab3706fb0989e8f0043` |

The usual `/home/shmellyorc/.local/bin` links now point to the new prefix.
The new prefix uses the previously installed LLVM SDK for native linking.
The compiler SHA-256 is
`037d8bdd4dc7b1b654cfa9f605ca9e9825bd395aeb2e971010bb3b77606c865c`.
The installed extension JavaScript SHA-256 is
`f0fdfc4c82acc1052d8b622df14a878113806b0afbaf7a5384a4e5eb98ceee9c`.

The installed desktop receipt records the extension path and the executable
resolved from `/proc` for both the original and restarted server processes.
The already-open TestGame window still had its previous process after the
install. Reload that window once to load the new extension and executable.
VS Code user settings passed the before-and-after hash check.

The former compiler prefix and prior recovery copy remain available.
`/home/shmellyorc/.local/opt/void-recovery-before-g72/restore` verifies the
saved artifacts, reinstalls extension 0.1.0 and restores the old tool links.
The extension downgrade was rehearsed successfully in a separate VS Code profile.
An existing project's native approval may need a fresh terminal review for the
new compiler identity.

## Source inventory

| Purpose | Files |
| --- | --- |
| Member and indentation recovery, semantic roles, completion and LSP error context | `include/void/diagnostic.hpp`, `include/void/frontend.hpp`, `src/frontend/compiler.cpp`, `src/frontend/tooling.cpp`, `src/lsp/main.cpp` |
| Extension configuration, operations and logging | `editors/vscode/package.json`, `editors/vscode/package-lock.json`, `editors/vscode/src/extension.ts`, `editors/vscode/src/editorLog.ts`, `editors/vscode/src/libraryManagement.ts` |
| Protocol and desktop regression coverage | `tests/lsp/editor_recovery.py`, `editors/vscode/src/test/editorBasics.ts`, `editors/vscode/src/test/host.ts`, `editors/vscode/src/test/raylibHost.ts` |
| Guide and handoff | `editors/vscode/README.md`, `docs/EDITOR_INTEGRATION.md`, `docs/projects/G72_EDITOR_RELIABILITY.md` |

## Follow-up after desktop testing

The installation above was superseded for VS Code and `voidls` after the owner
reported that source errors still made the whole file white. Qualification used
the owner's working `f32` raylib example with the casts to `i32` for
`draw_circle`. Tests introduced one type, name, syntax or indentation error at
a time. The previous server returned successful LSP responses containing zero
semantic tokens for all four cases in the desktop reproduction. VS Code
removed the semantic colors while Problems showed the compiler error.

The old project pipeline only published semantic tokens from a validated graph
or a few specifically recognized repairs. Every editor request also forced any
pending full analysis. Instrumented Release measurements on the raylib example
put each lock verification at about 25–27 ms, import loading and parsing at
roughly 29–40 ms, validation of valid graphs at roughly 155–172 ms, and analysis
metadata generation at roughly 200–213 ms in representative samples. Editing
also forced a dependency comparison (about 60–100 ms on valid revisions), and
the completion request then ran its own probe (about 93–107 ms inside the probe
and 119–142 ms for the whole completion handler after flushing). Encoding the
semantic response took under 1 ms. The VS Code trace identified a second queue
stall when its automatic quick-fix lookup requested fresh analysis, even when
the completion and semantic handlers had stopped doing so.

The extension opts into an interactive server path. A single background worker
checks the newest snapshot after a short idle period. Stale analysis results
are discarded. During typing, the server retains token ranges on unchanged
complete lines and moves them to the corresponding positions in the current
document. A presentation-only compiler pass can index checked statements in
a broken body and replace those interim tokens with current ones. This path
cannot lower IR, validate the project for execution or authorize navigation or
edits. If structural damage prevents binding, the affected portions still use
syntax highlighting. The strict compiler diagnostics and package verification
remain in force.

For a simple incomplete qualified selector on one changed line, completion can
use the previously checked compiler namespace and its visible declarations.
Package and manifest changes invalidate that catalogue. Other completion
contexts use the ordinary compiler probe. Automatic quick-fix queries use
current results when available. Requested fixes and their application retain
their existing checks.

| Measurement on the same raylib example | Previous install | Follow-up |
| --- | --- | --- |
| Time from typing `rl.` to the suggestions appearing in VS Code 1.144.0, median of five | 963 ms | 143 ms |
| Same desktop measurement, observed range | 890–977 ms | 143–187 ms |
| Raw edit-to-completion median for `rl.`, three samples | 731 ms | 19 ms |
| Raw edit-to-completion median for `rl.set_t`, three samples | 768 ms | 1.3 ms |
| First desktop token response across the four source-error cases | 185–293 ms | 2–20 ms |
| Settled tokens in those four invalid versions (valid source had 66) | 0 | 58–66 |

The raw follow-up `didChange` plus semantic request took 0.2–0.5 ms on
the warm process, including document synchronization and response encoding.
The paired desktop run saw temporary color loss in two of five previous-install
runs and zero of five repaired runs. The repaired desktop run kept rendered
raylib colors during each independent error. Every diagnostic cleared after
repair. Permanent tests exercise cold-open source errors, rapid edits,
automatic quick-fix lookup, locked-package mutation and restored completion.
The physical-raylib regression in VS Code 1.141.0 checks semantic tokens on
successive keystrokes. VS Code 1.144.0 also passed the diagnostics, tracing,
navigation, restart, Restricted Mode and multi-root host suite.
The workbench lifecycle with the installed tool links passed project creation,
Run, Build, F5 breakpoint and stepping, dependency add/remove and Restricted
Mode. Bundled raylib installation left native approvals untouched.

Before the final diagnostic fix, Release passed 45 of 46 tests. The
`lsp_async_syntax` failure also reproduced on G71. The focused fatal sanitizer
suite passed 11 of 11. Extension config tests passed 8 of 8 and VSIX archive
integrity passed.

Installed extension 0.1.2 at
`/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.2`.
The `/home/shmellyorc/.local/bin/voidls` link now selects
`/home/shmellyorc/.local/opt/void-g72-responsive/bin/voidls` (LSP version
0.2.0, SHA-256
`7a719ab922f85d74aa735cfed1f1b45fd5edc7b6b9b22ace1cc9261b8206483d`).
The installed `voidc` link and build identity are the ones recorded above.
The follow-up VSIX SHA-256 is
`355af77a1b2face54397c98f87818f75e1cd1588199314c9e8292d923493cf7b`.
An additional VS Code run loaded the installed extension directory, confirmed
the new server path through `/proc`, and used a copy of the owner's settings
with auto-save enabled. Suggestions appeared in 137–158 ms. All four source
errors retained rendered color and none of the five dot trials lost color.
The owner's settings passed the before-and-after hash check.
The previous pair is saved at
`/home/shmellyorc/.local/opt/void-recovery-before-g72-followup/restore`.
An isolated downgrade test verified the 0.1.1 VSIX. The earlier G68 recovery
remains available independently.

Follow-up timings, source-error evidence, protocol traces and install receipts
are under `Void.Language-G72-evidence/followup`. The follow-up also changes
`include/void/tooling.hpp` and `src/frontend/project_selection.inc` for
presentation-only snapshots and tolerant source parsing. The 19 source and
documentation files remain unstaged on `codex/g72-editor-reliability`.

## Final diagnostic recovery

When checking an entry-file project, the compiler reads the sources again to
verify they stayed stable. This second pass stopped at an already reported
syntax error and prevented analysis of the remaining code. Ordinary diagnostic
analysis now proceeds when both passes found the same parse errors and the
source paths and bytes match.

The unchanged `lsp_async_syntax` test now sees both the syntax error and the
unknown identifier and verifies that they clear after correction. The same
Release selection passed 46 of 46. The focused sanitizer suite, with the
previously failing test included, passed 12 of 12 with fatal ASan, LSan and
UBSan checks. An isolated VS Code 1.144.0 session with
extension 0.1.2 and the candidate server showed both errors in Problems and
cleared them after the source was repaired.

The candidate Release `voidls` SHA-256 is
`4b4fa6dba976625c0bd8b71d46c9d4ef3bc0ccc1013ace15c769465d8204a63d`.
The owner's active installation and recovery copies still use the builds
recorded above.
