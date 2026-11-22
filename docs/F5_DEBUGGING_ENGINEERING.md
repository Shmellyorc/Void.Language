# VOID F5 and debugging engineering report

21 November 2026, America/Vancouver. Boss-authorized continuation of the existing
editor work; no new G milestone. Implementation, independent review, corrections,
focused regressions, permanent installation and Linux editor qualification are
complete. Git integration remains subject to separate Boss authorization. Nothing
has been staged, committed, pushed, published or sent to hosted CI.

## Developer behavior

Run and Debug now offers **VOID Project** through VS Code's initial and dynamic
debug-configuration providers. Its welcome view links to debugging and project
creation; Explorer contributes the existing New Project command. Application
starters retain their generated configuration. Existing application projects can
use the provider without authoring `launch.json` or `tasks.json`; existing files
are preserved. The existing compiler-owned project and entry selection and task
provider remain authoritative.

F5 builds with the Debug profile, full debug information and `--no-opt`, then
launches the compiler-selected executable under `.void/build/`. These explicit
build overrides do not rewrite the manifest or alter ordinary Build/Run profile
selection. The existing actual-child build receipt still requires successful,
uncanceled compiler execution and a newly published executable for that launch.
A failed build or Debug Anyway action cannot substitute an old executable.

At a pause after initialization, supported locals can be inspected in Locals,
Watch, Debug Console and the debug hover over their source name. Editing hover
continues to show language information. Breakpoints and source stepping are
line-oriented; their DWARF line-table columns are zero, avoiding the additional
inline marker at a lowered member-call token.

## Reproduced causes and repairs

The initially installed TestGame executable stopped at `app.void:23:12`, but its
DWARF did not declare the loop's `dt`. Real pinned LLDB reported it as undeclared.
The compiler's debug-binding collection only visited top-level declarations;
`dt` was inside a loop. No extension-side replacement value or LLDB error
suppression was added.

The frontend now collects ordinary declarations and range/collection loop binders
through authored lexical bodies using an explicit work list. Declaration scopes
start at their declaration and end with the enclosing body. The backend emits
lexical blocks, associates declarations with the real local storage, and maps
instructions into their containing scopes. Loop binders have a distinct authored
initialization site because their lowering stores at the loop token. Sibling
bindings and variables after scope exit are verified with actual LLDB values.

F5 also needed an explicit debug-information override: an existing manifest can
legitimately request O2 or `debug_info='none'` in its Debug profile. The new
manifest build/emit `--debug-info none|line|full` option changes the effective
profile and cache key for that invocation. F5 negotiates this capability and
requests full symbols with `--no-opt`. Existing G95 cache implementation and
ordinary profile behavior are preserved.

The Run and Debug discoverability gap was a missing debugger contribution and
initial/dynamic configuration provider. The implementation delegates to the
existing LLVM LLDB DAP or Windows C/C++ adapter after configuration resolution;
it does not introduce another debugger implementation. An in-memory task is
provided when the project has no authored debug task.

## Native code review

The matching compiler advertises a bounded editor stdio approval protocol.
The extension requests it explicitly and binds requests to a fresh random
session token and the negotiated compiler file identity. The compiler computes
and displays the existing verified provider/application identity and SHA-256
approval digest. The extension presents a disposable review picker; only
**Approve Native Code** returns `approve <exact digest>` to that compiler child.
Deny or dismissal returns denial. There is no extension-authored approval record
and no conversion to `--allow-native`.

The compiler still performs normal input recapture before saving approval.
Exact saved records are reused without modifying their bytes or timestamps.
Changed provider/application/compiler identities require review again. Review-time
identity changes fail without saving the reviewed old identity. CI retains its
explicit authorization requirement. Ordinary redirected CLI execution remains
noninteractive; only the explicit editor mode waits, for at most 120 seconds.
Task cancellation and compiler exit dispose the review UI.

Independent review found and prompted two corrections. First, application stderr
could imitate an approval record after `voidc run` had started the application.
The compiler now emits a session-bound authorization seal before any application
startup, including pure VOID and already-approved runs; the parser permanently
passes through subsequent stderr. Second, an ordinary modal could outlive its
compiler. The disposable picker closes with its task, and late decisions are
bound to the original child. Independent re-review found no remaining actionable
code defects and separately passed the protocol, compiler-consent and real-LLDB
scope regressions.

Ordinary terminals, Vim and Neovim retain the existing foreground terminal
`voidc run .` review prompt. This uses terminal stdin/stderr and needs no GUI,
curses framework or new library. `:terminal` provides that interface in terminal
editors. Their DAP mouse/hover integrations are separate from this compiler flow.

## Qualification

| Check | Result |
| --- | --- |
| Focused Release CTest selection | 8/8 pass: native DWARF, lexical scopes, editor consent, profiles, filesystem permissions, automatic cache, native cache identity, cache retention |
| Expanded scalar LLDB regression | Pass: f32/i32 loop and sibling locals; bool/u64/f64/i8/i64 locals and native expressions; scope exit, stack, source breakpoints and exit statuses |
| Extension strict build and unit suite | 17/17 pass, including protocol chunk boundaries, malformed requests and post-seal forged application output |
| Independent review and correction pass | Both demonstrated P2 findings corrected; no residual actionable finding |
| Installed resource and tool smoke | Installed stdlib, optimized/Debug runs, independent native build/run, resource hashes and LSP handshake pass |
| Normal Code user profile | VS Code 1.144.0, actual installed extension, default tool discovery, F5 build, nested `dt=0.25`, native hover evaluation and line breakpoint pass |
| Installed raylib editor flow | Actual installed extension and tools: no launch/tasks JSON; Run and Debug lists VOID Project; deny, approve, saved reuse, task cancellation, changed identity cancellation, source step, Locals, Watch, REPL and visible mouse hover pass |
| Exact Boss project reproduction | Copy of TestGame source/assets/catalog, installed compiler and real raylib: breakpoint line 23, `dt` local/expression, useful stack and step to line 24 pass; original source hashes unchanged |
| Reopened normal editor | Original Ninja window reopened; fresh installed voidls processes and installed extension activation verified |

The first focused run exposed an obsolete profile-test expectation. The
previously installed compiler failed it identically: G95's ordinary Debug run
already warmed the cache, while the test still expected the following explicit
run to miss. The test now requires hit/hit after automatic warmup. The unfavorable
baseline and initial run remain in the evidence; cache behavior was not changed
to satisfy the old expectation.

The source scope and frontend changes were kept out of recursive native frames.
No full unrelated test matrix was rerun. Editor qualification uses the existing
VS Code test host and a small CDP harness to exercise visible controls. The
installed raylib host loads a tiny test harness extension; the VOID extension
itself comes from the permanent installed directory. The normal-profile host
loads that same installed VOID directory, with no isolated user-data directory.
Both use the real pinned LLVM 23.1.4 debugger and installed LLDB DAP extension
0.7.20261117.

## Installed identities and recovery

| Installed artifact | SHA-256 |
| --- | --- |
| `/home/shmellyorc/.local/bin/voidc` | `fcc1f13610f042fd4a4b2b0bf922bed76f1017bcb153150342cd5cf0f93f4639` |
| `/home/shmellyorc/.local/bin/voidls` | `1cbc00669147619b4bf04beba6e21b9e7af6f9e464ad03203831e0271335f617` |
| `/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.2/dist/extension.js` | `3a31c2513a390ba49ebb5d3def5cb3f58dc126101ae84450b5245020865c61a2` |

Compiler build identity:
`5390ab043d8d8958f863c7bfee8801f3bd719ba697cf576597436da3ef72922f`.
Both tools were built from `build/source-release`; the extension was strictly
built, packaged and installed through the normal VSIX path. CMake's compiler
and tooling components were installed under `/home/shmellyorc/.local`.
Installation smoke used that prefix, not temporary tool copies.

The previous installation is retained at
`/home/shmellyorc/Documents/Void.Language-archives/f5-debug-2026-11-21/previous-installation.tar.gz`,
with `previous-identities.json`. It contains the prior tools, native resources,
stdlib, verified package resources, licenses and extension. Recovery can close
Code normally, extract that archive relative to `/home/shmellyorc`, and reopen
the editor. Native approval records were not replaced or synthesized.

Final identities, commands/results, running-server hashes, the task-only patch,
exact changed-file inventory and visible-editor captures are under
`build/f5-debug-review/`. A retained qualification copy accompanies the recovery
archive. `installed-editor-discovery/paused-hover.png` shows real Locals and
mouse hover; `debug-discovery-dom.txt` records the actual VOID Project choice.

## Exact changed files

This inventory is relative to the working state at the beginning of this task,
including its already completed filesystem/editor correction and separate G96
edits. Preexisting-only files are excluded. Twenty-two tracked preexisting-only
edits compare byte-for-byte with the reconstructed starting snapshot.

| File | Purpose |
| --- | --- |
| `CMakeLists.txt` | Register focused scope and editor-consent regressions |
| `docs/EDITOR_INTEGRATION.md` | Automatic F5, review flow, supported inspection and terminal guidance |
| `docs/F5_DEBUGGING_ENGINEERING.md` | Engineering report and review handoff |
| `editors/vscode/README.md` | Discoverable F5 and consent instructions; retain prior corrections |
| `editors/vscode/package.json` | VOID debugger contribution, activation, welcome links, existing creation command and unit registration |
| `editors/vscode/src/config.ts` | Bounded capability negotiation for the new compiler options |
| `editors/vscode/src/extension.ts` | VOID configuration providers, automatic task, debug overrides and approval revalidation |
| `editors/vscode/src/nativeApproval.ts` | Bounded, session-bound compiler review parser and phase seal |
| `editors/vscode/src/nativeReviewUI.ts` | Explicit disposable provider-review picker |
| `editors/vscode/src/projectTask.ts` | Connect compiler review to its original task/child and dispose on exit |
| `editors/vscode/src/test/creationWorkbench.ts` | Visible-state, mouse and capture support in the existing editor harness |
| `editors/vscode/src/test/f5ExperienceHost.ts` | Real installed raylib consent/discovery/locals/hover/stepping checks |
| `editors/vscode/src/test/f5InstalledDefaultHost.ts` | Actual normal-profile installed-editor regression |
| `editors/vscode/src/test/nativeApproval.test.ts` | Protocol boundaries, rejection and forged application-output regressions |
| `editors/vscode/src/test/runF5Experience.ts` | Representative installed/development raylib test setup and launch |
| `include/void/frontend.hpp` | Presentation metadata for lexical scopes and initialization sites |
| `src/backend/llvm/backend.cpp` | Actual scoped local DWARF declarations and source-line locations |
| `src/cli_help.hpp` | Document explicit debug-information override |
| `src/frontend/compiler.cpp` | Collect nested scalar declarations and loop binders without recursive traversal |
| `src/main.cpp` | Negotiate/validate editor consent and explicit debug-information override; seal before startup |
| `src/native_approval.cpp` | Compiler-owned bounded review request/response and phase seal |
| `src/native_approval.hpp` | Editor authorization mode and lifecycle interface |
| `tests/project/debug_scopes.py` | Real DWARF/LLDB lexical scope and scalar evaluation regression |
| `tests/project/native_stdio_approval.py` | Exact consent, refusal, reuse, identity-change and startup-seal regression |
| `tests/project/profiles.py` | Correct the obsolete pre-G95 cold-cache expectation |

`task-only.patch` is computed against the reconstructed initial working files,
so it excludes unrelated G96 work and preserves the preexisting `.void/build`
editor correction. The full Git working tree remains unstaged for Boss review.

## Limits

Current local-variable records cover uncaptured numeric and boolean bindings in
ordinary synchronous, non-generic functions. Strings, native handles, aggregates,
managed objects and collection contents do not gain local-variable displays from
this change. Captured, anonymous, asynchronous and generic locals remain outside
this inspection support; globals are not qualified here. LLDB evaluates its
native expression syntax rather than the VOID expression language. Manual
optimized builds may omit variable locations. `line` debug mode has no locals.

This qualification is Linux x64 with VS Code 1.144.0. The existing Windows x64
GDB route is retained, but this change's native Windows consent/UI path has not
been qualified. VSCodium, Vim/Neovim DAP UI, macOS, remote and multi-root debugging
have no new qualification claim. Native adapter installation and its pinned
executable configuration remain required.

Filesystem policy is preserved: ordinary project operations use legitimate OS
permissions. This work adds no ownership, chmod, ancestor-mode or private-folder
restriction. Existing credential-store protection, path integrity, native
identity verification and consent remain in force. Existing user projects were
not rewritten, and G96 implementation files were not edited by this task.

## Established mechanisms consulted

The implementation follows VS Code's
[debugger configuration-provider architecture](https://code.visualstudio.com/api/extension-guides/debugger-extension),
[dynamic configuration API](https://code.visualstudio.com/api/references/vscode-api),
and [task provider mechanism](https://code.visualstudio.com/api/extension-guides/task-provider).
[C# debugging](https://code.visualstudio.com/docs/csharp/debugging) informed the
project-first F5 workflow. LLVM's
[source debugging model](https://llvm.org/docs/SourceLevelDebugging.html) and
[LLDB DAP integration](https://lldb.llvm.org/use/lldbdap.html) informed the choice
to repair real variable metadata and reuse the existing adapter. These sources
support the mechanisms, not the local qualification claims above.
