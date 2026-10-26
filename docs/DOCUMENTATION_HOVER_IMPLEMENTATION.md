# Documentation and hover repair: implementation handoff

25 October 2026. **Implemented and locally verified; Golden Wrench review and
Boss-controlled acceptance remain pending.** This is the separately authorized
bounded documentation sprint, based on `7895608714c63451ff0c5c27eab7c02b42230d0c`.
Stage 11's tested implementation `e8633b2`, documentation lock and historical
acceptance evidence are unchanged. No Git mutation, hosted CI, root README,
Runtime/GC/LLVM feature work, language syntax or native ABI change is included.

## Implemented behavior

The [approved contract](DOCUMENTATION_HOVER.md) supersedes the research/roadmap's
unresolved manual-policy text. Both automatic and manual help return null for an
undocumented active formal, including requests with no trigger context.

1. One compiler-owned line-oriented `source_documentation()` extracts ordinary
   description, optional return text, parameter descriptions and original tag
   spans. Only line-start `@param` and `@return` are recognized. Inline mentions
   and unknown tag spellings remain prose. Contiguous attachment, blank/ordinary
   comment boundaries, fields/types and other supported source symbols remain.
   First nonempty duplicate parameter/return text wins; duplicate parameter tags
   still warn. No additional documentation language or client parser is added.
2. Original function/method/interface headers are checked during original parsing,
   including unused generic declarations and generic receiver methods. Explicit
   formals exclude receivers and type parameters. VOID2022 warns on missing
   descriptions only after a known nonempty parameter description; VOID2023 warns
   on unknown names; VOID2024 warns on duplicates and relates to the first tag.
   Empty/unknown-only tags do not cause missing cascades. Specialization does not
   revalidate tags. The existing parser poison check now considers errors only,
   so a missing-doc warning cannot invalidate a reparsed generic header.
3. Existing analysis/editor records carry the same structured metadata. Callable
   hover/completion shows signature, ordinary description and applicable
   `Returns: ...` text. Recognized raw tags and parameter prose are excluded.
   Signature help retains all labels, active indices and compiler argument binding,
   omits signature-level prose and documents only the active formal. Plaintext
   MarkupContent is negotiated; clients without it receive a plain string.
   Per-call documentation selection clears prior state and selects enum case docs
   before extracting payload help. Existing named/default/receiver/generic/import
   and nested-call coverage is retained.
4. Written result type tokens export compiler builtin type references, including
   symbolic unused generic annotations. Existing `result_editor_target()` now
   supplies success/error-specific descriptions and payload help to constructors,
   patterns and completion. Nullable-presence Ok has separate presence metadata.
   One iterative pattern metadata walk serves expression and statement patterns,
   including nested result/enum patterns and ordinary enum variants. Captures keep
   their local types. Builtins remain non-navigable/non-renamable; no constructors,
   fake editable source or separate result implementation were introduced.
5. Documentation stays outside native compatibility serialization and invalidation
   metadata comparison. A docs-only generic edit verifies exact compatibility,
   unchanged binding/declaration summaries and no restart requirement. Large
   Function temporaries remain outside recursive semantic frames. Existing VFS,
   overlays, source identity, UTF-16 conversion and stale-result protections remain.
6. Existing maintained numeric/core/print records now supply explicit parameter
   descriptions to retain meaningful binding tests. The only standard-library
   source change adds three comment tags to existing `make_date` parameters;
   it adds no declaration, API or implementation.

## Measured editor repairs

The actual Neovim builtin command retained its old signature float after a
source-only edit caused a null reply. The before/after window IDs and buffer text
are retained in [baseline](documentation-hover/evidence/neovim-builtin-baseline.log)
and [owned-window probe](documentation-hover/evidence/neovim-owned-window.log).
The shipped Ctrl-K mapping now uses a small owned-window wrapper. It consumes the
server result and checks request generation, buffer, changedtick, window and
cursor before displaying a reply. Current null replies close its old window;
typing/movement/source events close the preview normally. A held real server
callback cannot reopen it after newer requests or source edits.

Actual Vim testing showed correct null dismissal but no visible parameter prose:
the pinned client's `showSignatureDocs` option defaults to false. The
[baseline failure](documentation-hover/evidence/vim-docs-disabled-baseline.log)
records the actual popup text. The adapter now enables that existing UI option.
The client's own null close path and request-context rejection remain unchanged.
No Vim client fork or tag parser was added.

VS Code's existing null dismissal works. Its host fixture inspects actual visible
parameter-hint DOM, types a comma to transition into an undocumented argument,
checks manual suppression, restores documented help, edits docs at the same
argument and holds/replays real native response bytes after a newer null request.
No production VS Code middleware change was needed. The existing automatic-import
UI test now waits for its expected visible selected row when CDP observation is
available, replacing its timing-only sleep. A fixed 750 ms sleep had raced menu
readiness; the readiness predicate also excludes the closing quote inserted by a
filename edit, which is not part of the visible label.

## Exact final local results

| Gate | Result | Retained evidence |
|---|---|---|
| Debug focused frontend/tooling/all LSP/CLI documentation suites | **19/19** | [CTest](documentation-hover/evidence/debug-ctest.log) |
| Release same focused suites | **19/19** | [CTest](documentation-hover/evidence/release-ctest.log) |
| Strict ASan/UBSan/leak same focused suites | **19/19** | [CTest](documentation-hover/evidence/strict-ctest.log) |
| Extended documentation process regression, including final Unicode tag-end ranges | **196 assertions per configuration; 588 total** | [Debug](documentation-hover/evidence/debug-documentation.log), [Release](documentation-hover/evidence/release-documentation.log), [strict](documentation-hover/evidence/san-documentation.log) |
| Nonfatal check, normal/O2 JIT, emit, linked build and native execution, with and without warnings | **18 processes per configuration; 54 total**, included in the focused CTests | [Route evidence](documentation-hover/evidence/execution.log) |
| Actual 1 MiB full frontend and tooling commands in Debug/Release/strict | **6/6 commands** | `documentation-hover/evidence/{debug,release,san}-stack-{frontend,tooling}.log` |
| Extension TypeScript compilation/typecheck | **passed** | Generated tests used by the actual editor |
| Extension config/real-server units | **4/4** | [Units](documentation-hover/evidence/extension-units.log) |
| Actual VS Code isolated trusted/untrusted/multi-root profiles | **3/3** | [Editor result](documentation-hover/evidence/vscode.log) |
| Actual Neovim/Vim shipped adapters, automatic entry selection and explicit entry | **4/4 editor/selection combinations** | [Automatic](documentation-hover/evidence/editors-automatic-selection.json), [explicit](documentation-hover/evidence/editors-explicit-selection.json) |
| Actual terminal Neovim/Vim automatic completion | **5/5 import prefixes in each editor** | [Neovim](documentation-hover/evidence/neovim-terminal.json), [Vim](documentation-hover/evidence/vim-terminal.json) |
| Actual terminal Vim signature transitions | **documented → undocumented → documented and named last passed**, including manual suppression | [Popup states](documentation-hover/evidence/vim-terminal.json) |

The focused configuration totals are **57/57 CTests**, not full-suite or hosted
acceptance. The later expanded Unicode regression was rerun independently in all
three configurations; no implementation changes followed those configuration
runs. The tests retain prior binding/navigation/rename/formatting coverage rather
than deleting assertions for the new suppression policy. Only affected fixtures
and their real source locations changed.

Strict execution used exactly:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Leak detection was enabled. Strict tests ran outside the restricted process
sandbox so LeakSanitizer could inspect real threads. Desktop VS Code and Neovim's
terminal RPC socket also required authorized host execution; their initial
restricted attempts failed at Electron sandbox startup / AF_UNIX socket creation,
not at a compiler or documentation assertion. No editor/client was installed and
no user profile or alternatives configuration was changed.

Executed Linux clients: **VS Code 1.143.0**, commit
`2d0b7506a36c1d6db9a466440131a8e01eab2289`; **Neovim
0.13.0-dev-1410+g510f61555b**; actual **Vim 9.2, patches 1–858** via
`/usr/bin/vim.basic`. `/usr/bin/vim` resolves to Neovim here and was not used as
Vim. The existing pinned yegappan/lsp checkout is
`c741b2f1afb42b9f267824dac4049674908f4a41`; the manifest hashes its exercised
signature/options source files. Parameter prose containing literal underscores,
angle brackets and non-BMP Unicode was checked in the real clients.

## Reproduction

Use the existing configured trees; no full seven-job run is required:

```sh
cmake --build build/step42-ci96-debug --target void_tests voidls voidc void_stage10_tooling -j2
ctest --test-dir build/step42-ci96-debug \
  -R '^(tooling|frontend|stage10_tooling|lsp_.*|documentation_execution)$' --output-on-failure
python3 tests/lsp/documentation.py build/step42-ci96-debug/voidls
ulimit -s 1024
build/step42-ci96-debug/void_tests frontend "$PWD"
build/step42-ci96-debug/void_tests tooling /tmp/void-doc-stack-tooling
```

Release uses `build/ci-official-release-step41`; strict uses
`build/step42-ci96-san` with the environment above. The native CLI route test is
`tests/documentation/execution.py COMPILER`. Editor reproduction uses the existing
`tests/editors/run.py`, `neovim_automatic.py`, `vim_automatic.py` and compiled
`editors/vscode/out/test/runHost.js` harnesses. For the VS Code host set
`VOIDLS_PATH`, `VSCODE_EXECUTABLE_PATH=/usr/share/code/code` and
`VOID_TEST_CDP_PORT` to an unused loopback port; it creates isolated test profiles.
Neovim/Vim window tests deliberately hold actual server results, not invented
semantic replies. The Vim handler replay uses its captured real request context.

## Engineering attempts and remaining observations

Initial focused Debug tests passed **16/19**, then **18/19** while fixture/source
line expectations were repaired. The retained
[first](documentation-hover/evidence/initial-debug-ctest.log) and
[follow-up](documentation-hover/evidence/fixture-followup-debug-ctest.log)
logs preserve those failures. Added docs shifted declaration lines; a temporary
accidental duplicate in already documented console fixtures was removed. The
first compilation used an unsupported `llvm::json::Object::emplace`; indexed
assignment corrected it. Final builds and all required gates above passed.
The actual client defects and their measured UI repairs are described separately
above. No sanitizer finding or remaining compiler failure is being deferred.

- This is Linux local verification. Windows/macOS execution and Boss-controlled
  hosted acceptance were not performed or claimed. Stage 11's prior acceptance
  cannot certify these new changes.
- The installed versions above were tested. Exact minimum VS Code 1.141.0 and
  Neovim 0.11.5 binaries, and VSCodium, were **not executed** in this sprint.
- Neovim's shipped adapter provides explicit Ctrl-K help and automatic completion;
  it installs no automatic signature trigger plugin. The wrapper owns its own
  window only. Arbitrary third-party signature plugins and custom float settings
  remain client-owned. Clients also decide whether cursor movement retriggers a
  session after null; no unsolicited server popup event was added.
- Client preview utilities may style plaintext differently. No identical typography
  or rich source documentation is promised. The Vim display option can be
  overridden by later explicit client configuration.
- The pre-existing untracked research and roadmap documents were preserved. Their
  original proposal/unresolved-policy text is historical; the approved contract
  records the Boss's final policy.

## Changed files

The following inventory lists task changes only; it excludes the pre-existing
untracked research/roadmap files. Machine-readable paths, source/binary hashes and
result counts are in [the manifest](documentation-hover/RESULTS.json).

Source, contract and test files:

```text
CMakeLists.txt
docs/DIAGNOSTICS.md
docs/DOCUMENTATION_HOVER.md
docs/DOCUMENTATION_HOVER_IMPLEMENTATION.md
docs/EDITOR_INTEGRATION.md
docs/LANGUAGE_BIBLE.md
docs/LANGUAGE_SERVER.md
docs/README.md
docs/WRENCH_CLANKER.md
editors/neovim/lua/void.lua
editors/vim/autoload/void.vim
editors/vscode/src/test/host.ts
editors/vscode/src/test/runHost.ts
include/void/diagnostic.hpp
include/void/frontend.hpp
include/void/numeric.hpp
src/frontend/compiler.cpp
src/frontend/pattern_semantics.inc
src/lsp/main.cpp
src/numeric.cpp
stdlib/time.void
tests/documentation/execution.py
tests/editors/neovim.lua
tests/editors/vim.vim
tests/editors/vim_automatic.py
tests/lsp/documentation.py
tests/lsp/sprint2.py
tests/lsp/stage7.py
tests/lsp/stage8.py
tests/stage10/lsp_display.py
tests/stage10/lsp_intelligence.py
tests/stage10/tooling.cpp
tests/stage8/lsp_sprint6.py
tests/tooling/tests.cpp
```

Evidence files (new, separate from historical stage evidence):

```text
docs/documentation-hover/evidence/.gitignore
docs/documentation-hover/evidence/debug-ctest.log
docs/documentation-hover/evidence/debug-documentation.log
docs/documentation-hover/evidence/debug-stack-frontend.log
docs/documentation-hover/evidence/debug-stack-tooling.log
docs/documentation-hover/evidence/editors-automatic-selection.json
docs/documentation-hover/evidence/editors-explicit-selection.json
docs/documentation-hover/evidence/execution.log
docs/documentation-hover/evidence/extension-units.log
docs/documentation-hover/evidence/fixture-followup-debug-ctest.log
docs/documentation-hover/evidence/initial-debug-ctest.log
docs/documentation-hover/evidence/neovim-builtin-baseline.log
docs/documentation-hover/evidence/neovim-owned-window.log
docs/documentation-hover/evidence/neovim-terminal.json
docs/documentation-hover/evidence/release-ctest.log
docs/documentation-hover/evidence/release-documentation.log
docs/documentation-hover/evidence/release-stack-frontend.log
docs/documentation-hover/evidence/release-stack-tooling.log
docs/documentation-hover/evidence/san-documentation.log
docs/documentation-hover/evidence/san-stack-frontend.log
docs/documentation-hover/evidence/san-stack-tooling.log
docs/documentation-hover/evidence/strict-ctest.log
docs/documentation-hover/evidence/vim-docs-disabled-baseline.log
docs/documentation-hover/evidence/vim-terminal.json
docs/documentation-hover/evidence/vscode.log
docs/documentation-hover/RESULTS.json
```

Stopped for Golden Wrench review and Boss-controlled acceptance.
