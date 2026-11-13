# G20 · Language-server root recovery and Stage 7 formatting

12 November 2026. **IN PROGRESS pending Golden Wrench source review.**
Starting revision `bd6e0ebe6e43dc26d9a4ce1258d85739806d22e1`.

## Baseline reproduction

Before editing, PikaOS Release replayed the archived G19 project and exact
`src/stock.void` bytes. A member probe returned `unit_price` and `units`.
After renaming the physical directory with the server alive, the same probe
at the original URI returned this exact response.

```json
{"jsonrpc":"2.0","id":3,"error":{"code":-32603,"message":"Internal server error"}}
```

The process exited with code 2. Stderr began
`Internal language server failure: filesystem error: cannot make canonical path: No such file or directory`
and named the missing directory. The unmodified binary SHA-256
`3fcd9fa2be728374b817a1444ed4009d80b2559ce260902c023a941d772c4386`
matches the G19 Release archive. The local replay used the `item.` in the
loop body at line 12, UTF-16 column 22. G19's original receipt and source are
preserved in `g19-completion-2026-11-12.zip`, SHA-256
`92f1d99469b5457924fc65663e46648f3e67fe64e233706a0d0c1fd70a92cce9`.
[Original G19 qualification](G19_COMPLETION_ATTRIBUTION.md#one-isolated-candidate-and-its-rejection)

The first `ProjectCapture` in `project_editor_assistance` called throwing
filesystem operations during admission. Its catch handled `CompilationError`.
The later manifest analyzer already translated filesystem exceptions into
project-boundary diagnostics. At the first capture the exception escaped to
LSP's fatal dispatch handler.

## Recovery contract

- A missing, inaccessible or aliased root reports `VOID5009` on the selected
  `void.project.toml` URI. A different physical directory at that pathname
  reports `VOID5011`, even when its manifest, lock and sources match byte for
  byte. Project state reports `error`, no entry, zero discovery and execution
  counts, and an empty snapshot key.
- The first affected request invalidates the selected analysis and publishes
  diagnostics under a new project revision. Open-document versions and exact
  URIs remain attached to their respective documents. Completion, symbols,
  references, formatting and code actions return empty lists. Hover, signature
  help and definition return null. Semantic tokens return empty `data`.
- A new analysis after refresh, a watched change, a document change or an
  explicit entry reselection can recover the original directory. Entry
  reselection within the session keeps its directory identity and dirty
  overlays. Selecting a different physical root takes a new session.
- Invalid source keeps existing bounded editor recovery. Unavailable imports
  keep their compiler read diagnostics. With no manifest overlay, physical
  manifest removal yields a manifest boundary error and a fresh analysis
  clears it after restoration. Explicit dirty TOML remains an overlay in an
  admitted root. Invalid locks and changed dependency source withhold
  completion even before a refresh. Fresh compiler checks still govern
  dependency and native authorization.

The compiler retains a directory handle and compares its OS identity at
the editor boundary. A failed observation produces the existing
`ProjectAnalysisResult` shape with a manifest-owned diagnostic and no
validated project. The session checks root admission around analysis and the
server checks around editor requests so previous indexes and in-flight
results are withheld on observed root loss. Holding the original directory
prevents a recycled file ID from passing as the original. Windows opens with
delete sharing. Existing exact spelling, source, manifest, and dependency
admission still runs during project capture. This remains a path-based
filesystem observation around an operation, with the usual limit that an
external mutation wholly between observations may escape detection.

The compiler catches the specific filesystem exception in project assistance,
including a failure between the request observation and its first capture.
A boundary or changed-project diagnostic from the probe's second capture
withholds even lexical fallback. The unexpected-exception handler still
returns `-32603` and exits with 2. No fault injection was added to that path.

## Stage 7 formatting

On the same unmodified Release binary, `lsp_stage7` reached the obsolete
line 86 assertion and raised `KeyError: 'error'` after the successful tab
formatting response. The test now applies a whole-document tab edit with a
supplementary scalar at the end of the last line and checks its UTF-16 end
position, resolved call and local binding, idempotence, conversion back to
spaces, mixed-indentation `VOID1002`, and rejection of a stale document
change. Later Stage 7 assertions run through shutdown. The existing Stage 10
source formatting test also compares compiler contracts and normal and
optimized JIT results for spaces and tabs. The formatter is unchanged.

## Native qualification

The focused selection covers project selection and sessions, project and
legacy protocol, dependencies and packages, G14 incomplete-source recovery,
G16 enum completion and roles, G18 CLI and LSP enum diagnostics, guarded
quick fixes, semantic tokens, Stage 7 and Stage 10 formatting, frontend,
analysis, tooling, and adjacent CLI diagnostics. PikaOS also registers the
Linux-specific manifest no-authority test. Test commands and logs live under
`build/g20-evidence/` and `/home/shmellyorc/VM/g20-20261112/`.

The same archived project replay on G20 returned
`{"jsonrpc":"2.0","id":3,"result":[]}` and published `VOID5009` to its
manifest. Restoring the root and requesting refresh recovered the identical
ordered completion objects. Shutdown exited with 0. The 12 G19 authority
scenarios also produced exactly matching normalized transcripts on PikaOS
Release.

The broader selection exposed a pre-existing `lsp_protocol` failure at line
126. That test expects the error on an open virtual legacy document to clear
when its import is removed. The pre-G20 Release server also publishes
`VOID2002` for that still-open document. The assertion remains in the
selection and its failure is counted below. The first Windows CTest run also
omitted the `void_project_packages_ir` executable from its build targets,
causing `project_packages` to fail with `WinError 2`. The final native build
includes that target.

| Final native profile | Focused CTest | G20 root and Stage 7 | G14, G16, G18 and guarded quick fixes |
| --- | --- | --- | --- |
| PikaOS 4 x64 Debug | 25/26 | Passed | Passed |
| PikaOS 4 x64 Release | 25/26 | Passed | Passed |
| PikaOS 4 x64 strict ASan/UBSan with leak detection | 25/26 | Passed | Passed |
| Windows 10 Home x64 Debug | 24/25 | Passed | Passed |
| Windows 10 Home x64 Release | 24/25 | Passed | Passed |

The Linux strict run used ASan leak detection and halt-on-error for both
sanitizers. No sanitizer report appeared. The PikaOS Debug and Release
frontend tests also passed with a 1 MiB stack.

Windows ran in the existing VirtualBox guest labelled “Windows 11”.
The registry and `ver` identified Windows 10 Home x64 19045.6466. The VS
2022 x64 developer prompt provided MSVC 14.44.35207 and SDK 10.0.19041.0.
Final CMake runs verified profile-specific host-provider and Schannel inputs,
and Ninja executable link rules used the pinned `lld-link.exe`. Builds and
fixtures ran on local NTFS, with TMP/TEMP under verified private guest roots.
Linux and Windows used LLVM/Clang/LLD 23.1.4.

The native VS Code 1.141.0 host test rendered the member popup, changed the
dirty buffer to include a supplementary scalar before the member access, and
checked its UTF-16 edit range. It then verified stale completion suppression,
the manifest diagnostic and visible project error, restoration after
`VOID: Refresh Project`, and an unchanged server PID and document version.
PikaOS exercised physical root rename. The Windows editor's root rename
returned `EBUSY` (errno -4082); the editor test used a physical manifest rename
in that same session. Windows protocol tests exercised root rename and
byte-identical replacement directly. The cached Windows editor reported a
missing background TextMate worker asset while the extension-host checks
completed. The existing PikaOS native G14 and G16 editor hosts and guarded
quick-fix host also passed.

To replay the focused G20 protocol checks from a qualified build:

```sh
ctest --test-dir build/llvm23-release --output-on-failure \
  -R '^(lsp_project_root_recovery|lsp_stage7|stage10_source_formatting|lsp_stage10_source_formatting)$'
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/llvm23-strict --output-on-failure \
  -R '^(lsp_project_root_recovery|lsp_stage7|stage10_source_formatting|lsp_stage10_source_formatting)$'
```

## Historical boundaries

G19 is independently reviewed and integrated at
`bd6e0ebe6e43dc26d9a4ce1258d85739806d22e1`. Its deferred-capture candidate
remains rejected. Its matched Linux median improvements of 27.8% Release
and 17.2% Debug and the original root-error incompatibility remain historical
findings. The five original 15/16 runs remain in that record.

The G18 Windows Adventure executable-removal `WinError 5` remains unexplained.
The G06/G12 Windows file-symlink prerequisite error 1314 remains separately
tracked. G00-A public redistribution, Windows persistent native caching and
general G08 production optimization remain on HOLD. The original seven-job
integrated gate remains deferred.

## Handoff

The raw reproduction, corrected reply, native build and CTest receipts, and
editor evidence are in `g20-root-recovery-2026-11-12.zip` alongside the G19
archive. Golden Wrench should review the compiler and session boundaries in
the Boss-controlled integration commit. The baseline legacy protocol assertion
can be reconciled separately against the independent-document contract.
