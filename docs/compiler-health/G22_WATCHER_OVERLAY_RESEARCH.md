# G22-R filesystem watcher, dirty overlay and project authority research

Date 2026-11-12 (America/Vancouver). Baseline
`59db769cacd80ceba60d3bf9041007d21f300428`. G21 is accepted at that SHA.
Boss subsequently authorized the production repair documented in
[Authorized production repair](#authorized-production-repair).
The research findings and original HOLD below record the investigation phase.
G22 implementation is pending Golden Wrench review.

## Decision

**A watcher/overlay ordering defect is reproducible.** An unversioned disk
deletion can replace a current editor source override with an unavailable
marker while the same URI remains LSP-open at the same client version.
A late delete can also hide a closed file already restored on disk. The
compiler's `VOID3002` is correct for the tombstoned snapshot it receives.
The watcher handler is what creates that snapshot.

The G21 harness intentionally deleted a physical file during its import
sequence. VS Code sent ordinary watched-file notifications and kept the
dirty document open. Changing the harness order made the G21 qualification
deterministic. G22 reproduced the source-authority failure with real
VS Code events on both native hosts and with a minimal protocol fixture.

Manifest mode deliberately gave deletion precedence over dirty sources
in the pre-rename workflow. The maintained suite asserts that an open
entry disappears on the watch alone. That existing contract requires a
review decision before an implementation can ship. The narrow candidate
below preserves explicit delete and rename, source admission, lock
verification, and G20 physical root checks.

## Exact G21 event timeline

The preserved Linux LSP trace records local timestamps on 2026-11-12.
The source URI ends in `/virtual.void`. Its original disk contents were
`public func calculate(x: i32) -> i32:\n    return x\n`. The dirty text
uses `return missing`. The trace contains exact URIs, texts and
diagnostic objects. The failing harness did one physical unlink before
refresh request 8. The trace does not timestamp the unlink itself or
identify why the client queued two matching entries for it.

| Local time | Operation and client versions | Accepted project revision and result |
| --- | --- | --- |
| 21:44:49.561–49.799 | Initialize. Open `main.void` v1 and `virtual.void` v1. | 1–3, ready. Empty arrays. Both open overrides contain saved text. |
| 21:44:50.022–50.030 | Send virtual v2 with `missing`, then main v2 with `use "/virtual.void" as v` and `return v.calculate(0)`. Refresh request 5. | 4, ready with independent `VOID2002`. 5–6, selected graph error with one `VOID2002` at virtual line 1, UTF-16 columns 11–18. |
| Before 21:44:50.114 | Unlink virtual disk file while its document is open and dirty. No close. | The unlink itself has no session revision. |
| 21:44:50.114–50.116 | Refresh request 8. No watched delete received yet. | 7, graph still uses virtual v2 and reports `VOID2002`. |
| 21:44:50.150–50.154 | Main v3 drops import, then refresh request 9. | 8–9, ready. One independent `VOID2002` still belongs to virtual v2. |
| 21:44:50.275–50.278 | Virtual v3 restores `return x`. | 10, ready. Empty virtual array. |
| 21:44:50.540–50.543 | Virtual v4 restores `return missing`. | 11, ready. One independent `VOID2002`. |
| 21:44:50.578–50.590 | One `workspace/didChangeWatchedFiles` contains two type-3 entries for the same URI. No version or document close. | 12 and 13 install the tombstone twice. The watch refresh advances to 14, ready with one execution source and empty diagnostics for both URIs. Virtual stays LSP-open at v4. |
| 21:44:50.691–50.695 | Main v4 reimports virtual. Refresh request 16. | 15–16, error, entry null, one captured execution source and reselection required. Main gets severity 1, source `VOID`, `VOID3002` at line 0, UTF-16 columns 4–19. Virtual gets an empty array. |

The exact primary message at 15 and 16 was
`Source is explicitly unavailable: '/virtual.void'.` The hint was
`Restore the document or close its override to return to disk.` The client
advertised `publishDiagnostics.versionSupport: false`. The publications
contain `voidProjectRevision` and omit the client document version throughout.
The document versions above come from the input messages.

The original build carried no per-document trace. Source reconstruction gives
virtual internal document revisions 1 at open, 2 at v2, 3 at v3, 4 at v4,
then 5 and 6 after the duplicate delete. Refresh advances the project revision
only. The model and the server's retained open-document record still hold v4
bytes after the tombstone. The disposable observation build verified those
state transitions in the synthetic replay, including the duplicate-delete
increments and retained text. The G21 trace ends at the failed reimport
condition. The later successful G21 run used a different harness ordering.
[G21 qualification](G21_LSP_DIAGNOSTIC_LIFECYCLE_QUALIFICATION.md#native-vs-code-11410)

## Source-level cause and historical contract

| Boundary | Current behavior at the baseline |
| --- | --- |
| Editor and wire | The extension supplies a `**/*` manifest-project watcher to vscode-languageclient. In the installed 10.1.2 client, file events queue on a 250 ms delayer separately from text synchronization. A notification has URI and type, with no document version, disk identity or generation. [Extension](../../editors/vscode/src/extension.ts#L211) |
| Open buffers | `didOpen` and full `didChange` maintain a client version and a copy of source text in `Server::open`. Strictly newer versions update the session override. `didClose` removes the override and the open record. No-op full-change arrays take their starting text from the current override, which can already be tombstoned. [Synchronization](../../src/lsp/main.cpp#L715) |
| Watched deletion | Type 3 installs `session.unavailable(module,next(module))` for manifest mode and for open legacy modules, then refreshes. Every duplicate can advance the document counter. The handler leaves the open map intact and does not check current physical existence. Type 1 clears a tombstone only when the URI is closed. Type 2 refreshes without clearing it. [Watch handler](../../src/lsp/main.cpp#L944) |
| Project snapshot | `unavailable` writes a present map entry with an empty optional. The project and document revisions are distinct from the signed client version. Current-snapshot checks enforce acceptance order. They cannot determine when the physical event occurred relative to the client edit. [Session mutation](../../src/frontend/tooling.cpp#L331) · [Snapshot identity](../../include/void/tooling.hpp#L102) |
| Import and independent analysis | A tombstone wins before graph disk lookup. A live overlay can contribute a virtual file inside the existing admission policy. Independent analysis skips null overrides. Diagnostics are freshly merged and an old error URI is cleared when it has no current result. Completion reads the current snapshot or its admitted semantic source. Retained rename bytes in the open record supply neither independent analysis nor completion on a tombstone. [Import resolution](../../src/frontend/compiler.cpp#L5910) · [Analysis](../../src/frontend/tooling.cpp#L367) · [Publication and completion](../../src/lsp/main.cpp#L280) |
| Explicit file operations | Explicit delete retains its tombstone. Explicit rename can transfer the retained dirty bytes even when a watch already tombstoned the old URI. It makes the old identity unavailable. An explicit create can clear a closed tombstone. [File operations](../../src/lsp/main.cpp#L917) |
| Project and dependency authority | `ProjectCapture` owns exact-case/alias, source-root and nested-boundary admission and verifies locked dependencies. G20 additionally checks the held physical root before and after analysis and on editor queries. G19 showed why a completion path must keep current capture and refuse changed dependencies. Retaining document bytes confers no filesystem or execution authorization. [Project capture](../../src/frontend/project_selection.inc#L44) · [Completion guard](../../src/lsp/main.cpp#L535) · [G19](G19_COMPLETION_ATTRIBUTION.md) · [G20](G20_PROJECT_ROOT_RECOVERY.md) |

Stage 7 at `b29b96badf25daf42b19401dec00edc12d60be6f` used a refresh-only
legacy watch path and documented dirty overlay authority. Manifest mode
introduced watched tombstones at `a642ce746a62a7646d06d42d9f87931813aaa812`.
Its Slice 3 contract describes retaining dirty bytes for a rename after
a deletion watch, and preventing disk fallback on deletion. The G13/G01
integration at `29d50c9a21cdf93579941b872ef815b2ae7f881a` extended watch
tombstones to open legacy documents as part of rename recovery. G13 also
added independent compiler-owned analysis of open, unreferenced documents.
G21 reconciled import removal and actual close with that ownership and
preserved the watched-deletion observation. The conflict is observable
even with a truly deleted file and a completely valid dirty overlay.
[Stage 7](../STAGE7_IMPLEMENTATION.md#synchronization-scheduling-and-lifecycle) ·
[Slice 3](../projects/SLICE3.md) ·
[G13 acceptance](../roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#batch-1-implementation-record)

## Upstream research

- **LSP 3.17.** Open and close govern client ownership of document contents.
  A watched event carries only a URI and a change kind. `workspace/didDeleteFiles`
  describes a separate operation performed from the client. The specification
  supports keeping content synchronization and disk observation separate.
  VOID still owns whether the document is admitted to a particular project.
  [Open](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didOpen) ·
  [Close](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didClose) ·
  [Watch events](https://github.com/microsoft/language-server-protocol/blob/ba1f1947af19dbf3c33fc763eadd7e7848f80766/_specifications/lsp/3.17/workspace/didChangeWatchedFiles.md#L176) ·
  [Explicit delete](https://github.com/microsoft/language-server-protocol/blob/ba1f1947af19dbf3c33fc763eadd7e7848f80766/_specifications/lsp/3.17/workspace/didDeleteFiles.md)
- **VS Code 1.141.0.** Watchers observe disk/provider changes. The watcher
  coalescer converts delete/create for one path in one batch into change
  and can collapse create/delete. That does not guarantee ordering across
  batches or across the text sync stream. The language client forwards
  deletes and queues them independently. VS Code maintainers also document
  that an externally deleted file can retain a live text model. The native
  G22 runs directly verify both the watch and the retained dirty document.
  [File watcher API](https://code.visualstudio.com/api/references/vscode-api#workspace.createFileSystemWatcher) ·
  [Watcher internals](https://github.com/microsoft/vscode/wiki/File-Watcher-Internals) ·
  [1.141.0 coalescer](https://github.com/microsoft/vscode/blob/2a59476c9bfcb90b3ddc372c36762471b7dfad1c/src/vs/platform/files/common/watcher.ts#L403) ·
  [Client queue](https://github.com/microsoft/vscode-languageserver-node/blob/release/client/10.1.2/client/src/common/client.ts#L1695) ·
  [External-deletion behavior](https://github.com/microsoft/vscode/issues/58088)
- **rust-analyzer.** Watch notifications invalidate unique paths in the
  loader. Loader results yield to client-managed `mem_docs` for ordinary
  sources, including when the loader has no bytes. The library-root
  exception retains disk authority. Close removes memory ownership and
  invalidates the loader path. This separation applies to VOID source
  overlays and verified dependency inputs. Rust workspace discovery does
  not substitute for VOID's root and lock checks.
  [Loader application](https://github.com/rust-lang/rust-analyzer/blob/8cbebb0c2c23562b11e4d56efb85ca457c758b2f/crates/rust-analyzer/src/main_loop.rs#L996) ·
  [Close and watcher handlers](https://github.com/rust-lang/rust-analyzer/blob/8cbebb0c2c23562b11e4d56efb85ca457c758b2f/crates/rust-analyzer/src/handlers/notification.rs#L137)
- **clangd.** At the inspected LLVM SHA, the draft store overlays the
  filesystem and the document close path removes the draft. The server
  watch hook at that SHA does no analysis work. The draft regression
  tests version and text replacement, including backward versions.
  Draft separation is applicable. VOID needs watch-driven revalidation
  for its project inputs and has its own strict client-version rule.
  [Draft filesystem](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/clang-tools-extra/clangd/ClangdServer.cpp#L176) ·
  [Watch hook](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/clang-tools-extra/clangd/ClangdServer.cpp#L944) ·
  [Draft test](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/clang-tools-extra/clangd/unittests/DraftStoreTests.cpp#L17)
- **Pyright.** Watch handling checks current filesystem state. Structural
  changes invalidate import resolution, while disk-content invalidation
  excludes client-open files unless explicitly forced. The source file
  retains client content until close. A regression verifies that changing a
  closed dependency updates semantics for an open importer. Separate tests
  cover structural marker deletion. These support separating open text
  from import invalidation, with VOID's compiler capture as the authority
  for project structure. They do not establish Python/VOID import parity.
  [Watcher handling](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/analyzer/service.ts#L1444) ·
  [Content invalidation](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/analyzer/program.ts#L2002) ·
  [Watcher regressions](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/tests/service.test.ts#L1015)
- **TypeScript 5.9.3 tsserver.** The source-file watcher callback asserts
  a closed `ScriptInfo`. Opening uses client text. Close checks physical
  existence and either resumes disk watching or processes deletion.
  Configured and inferred membership remain separate from whether the
  buffer is open. This supports a disk fallback boundary at close.
  VOID needs broader project watches for manifests, roots and dependencies
  and cannot use the tsserver ownership model alone to validate them.
  [Disk callback](https://github.com/microsoft/TypeScript/blob/c63de15a992d37f0d6cec03ac7631872838602cb/src/server/editorServices.ts#L1896) ·
  [Close handling](https://github.com/microsoft/TypeScript/blob/c63de15a992d37f0d6cec03ac7631872838602cb/src/server/editorServices.ts#L2369) ·
  [Project tests](https://github.com/microsoft/TypeScript/blob/c63de15a992d37f0d6cec03ac7631872838602cb/src/testRunner/unittests/tsserver/inferredProjects.ts)

Upstream code and tests were read as source. No downloaded code was executed.

## Disposable experiments

The fixture records every framed request, notification and reply, including
complete diagnostic and completion objects. It records disk bytes at every
barrier. The observed build also records project revision, internal document
revisions, optional overrides, open buffer versions and text, selected and
independent owners, and physical-root availability. It reproduces the
unmodified Release results. Each run has 43 steps with assertions for its
expected outcomes, including the baseline failure modes.

### Minimal process reproduction

1. Create a pinned manifest application. Import `./part.void` from
   `main.void`. Put a valid member `count` on disk in the imported source.
2. Open the imported URI at client version 1. Send a full v2 dirty edit
   changing the member to `dirty_count`. Query completion after `p.`.
   The current member is `dirty_count`.
3. Unlink the physical imported source and send type 3 in
   `workspace/didChangeWatchedFiles` for its URI. Keep the document open.
   After a request barrier, the importer reports `VOID3002` and completion
   at the same buffer position returns `[]`. The open record still holds
   v2 and the dirty bytes. The source override is null.
4. A repeated v2 is ignored. A v3 full edit restores overlay authority
   with the disk file still absent. Actual close restores disk authority
   and reports the distinct missing-file import error.

For a second minimal control, restore the same file on disk while its URI is
closed and then deliver a pending type-3 event. Baseline reports explicitly
unavailable even though compiler admission would accept the current disk file.
A type-1 event clears this closed tombstone. This control uses synthetic
delivery order. The native editor tests establish the dirty-document delete
sequence independently.

### Candidate comparison

Both variants change only the watched source-deletion condition in a
disposable server translation unit. Both retain project refresh, explicit file
operations, manifest events, package handling and legacy behavior.

| Scenario | Baseline | Preserve open manifest-project source | Refresh only for manifest-project source watches |
| --- | --- | --- | --- |
| Real disk delete while a clean source is LSP-open | Import `VOID3002`, empty completion | Open text and completion remain current | Same |
| Watch before newer edit vs. watch after newer edit | Newer edit repairs. A later watch revokes that edit; resending its version is rejected | Current bytes survive either order | Same |
| Remove import, then watch and reimport | Independent `VOID2002` clears. Reimport reports `VOID3002` at the importer | One current `VOID2002` survives both transitions | Same |
| Explicit entry switch with queued duplicate delete | Other entry ready with no independent completion for the tombstoned URI. Switching back reports the missing import | Other entry ready with current independent completion. Switching back uses the same overlay | Same |
| Reopen absent URI at v0 | Full overlay and completion recover | Same | Same |
| Restore or atomically replace disk file under an open dirty URI, then deliver a late delete | Tombstone hides both sources. Close selects new disk bytes | Open dirty bytes remain authoritative until close. Close selects new disk bytes | Same |
| Deliver deleted/created/changed/deleted/created in one watch array while dirty URI remains open | Two delete mutations and one refresh. Create skips the open tombstone | One refresh. Current open bytes survive | Same |
| Late delete after restoration with the URI closed | Tombstone hides the restored source until a create event clears it | Same failure | Current admitted disk bytes and completion stay available |
| Genuine absence after close | `VOID3002` | `VOID3002` | `VOID3002` |
| Watch then explicit rename, explicit delete and create | Explicit rename transfers dirty bytes. Explicit delete tombstones. Create restores closed disk authority | Same explicit operation results | Same |
| Watched manifest deletion and create while manifest remains open | `VOID5009` until newer TOML text repairs the open tombstone | Same | Same |
| Physical root removed or byte-identical replacement root | `VOID5009` for loss, `VOID5011` for replacement. Completion withheld. Original identity plus refresh/edit recovers | Same | Same |
| Invalid lock and modified locked package | Before refresh, completion already withholds results. Watch triggers `VOID5011`. Verified bytes restore results | Same | Same |
| Open a source in a nested project from the parent | `VOID5009` for the inadmissible source | Same | Same |

In the independent synthetic replay, baseline project revision 4 holds
`part.void` client v2 with a current `VOID2002` and completion
`dirty_count`. The watch advances the project to 6 and the internal document
revision from 2 to 3, leaving client v2 and the retained text intact. The
independent owner disappears, its diagnostic array empties and completion
returns `[]`. Reimport at 7 emits `VOID3002` on the importer. The
refresh-only variant advances project revision 4 to 5 with the document still
at internal revision 2 and client v2. Completion and `VOID2002` survive
reimport.

The source refresh variant is the stronger research candidate. The open-only
variant leaves a restored closed disk source vulnerable to old delete events.
Both fail the maintained manifest `project_protocol` assertion
`watch deletion hides open source`. That test uses an overlay-only entry
and a synthetic watch before explicit rename. The separate explicit-rename
experiment verifies that both candidates still transfer dirty bytes on
`didRenameFiles`. The present distinction is whether the old entry disappears
at the earlier watch or at the explicit rename. No assertion was changed.

A physical-existence check in the watcher could address some late events on a
restored path. It would still tombstone a current open overlay when the physical
file was genuinely absent. A file timestamp, client-version comparison or debounce
cannot order an unversioned watch against an editor lifetime. Broad suppression
of delete notifications would lose manifest and dependency invalidation. Reading
disk through a tombstone would weaken explicit deletion. Bypassing compiler
capture, project boundary checks, dependency verification or G20 root identity
to recover completion is disqualified. The experiments preserve those checks.

## Native qualification

The verified LLVM/Clang/LLD 23.1.4 builds are the same production inputs
used for G21. The baseline changes since G20 are confined to tests and
documentation. The Linux binary hashes match the G21-R identities.
The strict runs enable ASan leak detection and halt-on-error for ASan and
UBSan. No sanitizer report appeared.

| Baseline profile | G22 protocol probe | Maintained CTest |
| --- | --- | --- |
| PikaOS 4 x64 Release | 43/43 | 8/8 |
| PikaOS 4 x64 Debug | 43/43 | 8/8 |
| PikaOS 4 x64 strict ASan/UBSan | 43/43 | 8/8 |
| Windows 10 Home x64 19045.6466 Debug | 43/43 | 8/8 |
| Windows 10 Home x64 19045.6466 Release | 43/43 | 8/8 |

The eight maintained cases are `lsp_protocol`, `project_protocol`,
`lsp_project_root_recovery`, `lsp_stage7`, `lsp_editor_recovery`,
`lsp_enum_recovery`, `lsp_enum_equality`, and `lsp_quick_fixes`.

Linux Release observation and both Release candidates completed all 43
expected-outcome steps. The strict refresh-only candidate completed the same
43 steps plus the full legacy protocol and G20 root-recovery fixture.
Both Release candidates passed the legacy and G20 fixtures and failed the
maintained pre-rename watch assertion described above. Candidate qualification
is limited to Linux. The Windows runs establish production reproduction and
authority controls.

Windows used the existing VirtualBox guest labelled “Windows 11,” with the
registry identifying Windows 10 Home x64 19045.6466. The VS 2022 x64 Native
Tools environment used MSVC 14.44.35207 and SDK 10.0.19041.0. The harness
verified fixed local NTFS, private TMP/TEMP roots, profile-matched native
host-provider and Schannel inputs, and baseline binary and source hashes.
The guest replayed the exact accepted G21 test files and restored its prior
copies. The closeout verified those restorations and removed the four
G22-only private guest roots.

### Native VS Code 1.141.0

The PikaOS and Windows Release runs opened a valid disk file, made it dirty
with `missing`, imported it, displayed the error in Problems, and then removed
the import. The harness performed a physical unlink with its filesystem API.
The extension sent a real watched-file delete without a close or document edit.
The current dirty v2 still contained `missing` when the server cleared its
error. Reimport produced the explicitly-unavailable `VOID3002` on the main
file. A newer edit restored the overlay and reported `missing_new`. Switching
that document to Plain Text emitted an actual `didClose`. The importer then
reported the ordinary missing-disk `VOID3002`. Recreating the physical
source produced a real creation watch and cleared the error.

The PikaOS Release refresh-only variant received the real watch while the dirty
source was open. It retained v2's independent `VOID2002`, kept exactly one
error on reimport, and reported physical absence after the actual close. The
Windows cached editor still reported the background TextMate worker limitation.
It completed the diagnostic checks. The native runs exercise external delete,
close and recreation. Late delivery after restoration, synthetic duplicate
batches, and pending entry switches are protocol controls.

## Recommended next action

Golden Wrench should review a bounded ordinary-source watcher contract.
Preserve current admitted editor text through disk watch events and refresh
closed source state from the compiler's current filesystem capture. Retain
explicit file-operation behavior. Define precisely when the selected entry
must be invalidated during an external rename with only watch notifications,
and whether the editor can supply a separate authoritative rename signal.
Review legacy G01 and manifest deletion separately. Both currently have
different obligations from an imported open source. The maintained
pre-rename assertion must stay visible until that review resolves the
contract. Production implementation stays on HOLD.

Preserve the G13 editor/execution boundary, G19 freshness checks, G20 root
identity, and G21 publication/close ownership. Keep the prior research,
qualification, deferred hosted gate and release holds intact.

## Evidence and changed-file inventory

- `docs/compiler-health/G22_WATCHER_OVERLAY_RESEARCH.md` records this research.
- `docs/roadmap/MILESTONE_REGISTER.md` marks G21 accepted at the baseline and
  G22-R pending review.

Local evidence is under `build/g22-evidence/` and
`/home/shmellyorc/VM/g22-20261112/`. It includes the G21 timeline extraction,
all 43-step wire transcripts, observed internal snapshots, exact variant
sources and build recipes, CTest logs and XML, editor receipts and traces, and
pinned upstream source references. The preserved G21 trace remains in
`build/g21-evidence/editor-linux-watch-interleaving.trace`.

The checkout production, tests and prior research records are unchanged.
No hosted CI, staging, commits, pushes or branch changes occurred.

## Authorized production repair

Boss authorized the G22 production repair after the research handoff, using
baseline `59db769cacd80ceba60d3bf9041007d21f300428`. This section records
the implementation. The preceding timelines, failed candidates, qualification
and original HOLD describe the research phase.

The production diff is confined to the existing watched-file handler in
`src/lsp/main.cpp`. It removes the deletion-driven unavailable mutation.

```diff
-                    if(type==3 && (manifest_mode || open.contains(module)))session.unavailable(module,next(module));
+                    // Disk watches invalidate analysis; document ownership changes through synchronization or explicit file operations.
```

The existing handler validates the event URI and type and calls the current
session refresh mechanism. Open source and manifest buffers survive disk
events. Closed sources are captured from the current filesystem, including a
file restored before a delayed deletion arrives. Actual close releases the
overlay. Explicit rename and delete keep their existing ownership transitions.
The compiler continues to enforce source admission, dependency verification
and physical-root identity. No diagnostic code, cache, watcher service or
background execution mechanism was added.

### Maintained regression changes

`tests/project/protocol.py` had two deletion assumptions. The legacy rename
control physically moves an open entry and expected a missing-file error
before close. It now requires the open entry to remain navigable, then closes
it and requires the original missing-source diagnostic and unavailable index.
The manifest pre-rename control sends a synthetic watch for a virtual entry
without a disk file. It now requires the selected entry and ready state to
survive the watch. Its following explicit rename, reselection and delete
checks remain active.

A focused fixture in that maintained test additionally verifies the following.

- Real physical deletion preserves the dirty source's exact current diagnostic
  at client version 2 and its `dirty_count` completion.
- Removing and restoring the import preserves one independent error. Duplicate
  delayed delete notifications retain that error and client version.
- Actual close clears the source's diagnostic without a client version and
  reports the compiler's ordinary missing-file import error on the importer.
  The missing closed source supplies no completion.
- Restoring disk bytes followed by a late delete event recovers current disk
  analysis and `disk_count` completion.
- Reopen at reset client version 0 and mixed watch batches preserve the current
  dirty overlay. Close restores the replacement disk source's completion.
- A real disk rename followed by explicit rename transfers the dirty bytes.
  Explicit delete continues to make the imported source unavailable.
- Watched physical manifest deletion preserves its open buffer. Close exposes
  the missing-manifest `VOID5009`. A late deletion after manifest restoration
  observes the valid disk source.

The reconciled test fails on the original observed server at the first dirty
diagnostic retention check. That server publishes an empty array at client
version 2. The research baseline editor traces retain the same original
failure on PikaOS and Windows.

### Implementation qualification

All five profiles used LLVM/Clang/LLD 23.1.4. The final maintained selection
passed 17/17 per profile, with `project_protocol` reporting 285 checks.
Each profile also passed the 43-step G22 process probe against the repaired
production binary. The original probe's manifest-watch expectations described
the earlier candidate, which retained manifest tombstones. The implementation
probe now checks the authorized open-buffer behavior for manifests as well.
The first replay exposing that obsolete probe expectation is retained.

| Repaired native profile | Focused CTest | G22 process probe |
| --- | --- | --- |
| PikaOS 4 x64 Debug | 17/17 | 43/43 |
| PikaOS 4 x64 Release | 17/17 | 43/43 |
| PikaOS 4 x64 strict ASan/UBSan with leak detection | 17/17 | 43/43 |
| Windows 10 Home x64 19045.6466 Debug | 17/17 | 43/43 |
| Windows 10 Home x64 19045.6466 Release | 17/17 | 43/43 |

The strict runs enabled leak detection and halt-on-error for ASan and UBSan.
No sanitizer finding appeared. The selection consists of `lsp_protocol`,
`project_protocol`, `lsp_project_root_recovery`, `lsp_stage7`,
`lsp_editor_recovery`, `lsp_enum_recovery`, `lsp_enum_equality`,
`lsp_quick_fixes`, `numeric_enum_lsp`, `tooling`, `analysis`,
`project_sessions`, `project_selection`, `project_dependencies`,
`lsp_semantic_tokens`, `lsp_documentation`, and
`lsp_stage10_intelligence`. The root-recovery fixture and the G22 probe
exercise physical root replacement and dependency mutation, preserving the
G19/G20 refusal behavior. The complete G21 legacy diagnostic lifecycle remains
in the selection.

Windows rebuilt `voidls` with the established VS 2022 x64 Native Tools
environment, MSVC 14.44.35207 and SDK 10.0.19041.0. The run verified local
NTFS, private scratch roots, exact source hashes, pinned compiler/linker paths
and the existing profile-matched host-provider and Schannel prefixes.

Native VS Code 1.141.0 on both Release hosts ran the repaired production
server. Both sent a real watched-file delete after an external physical
unlink. The open dirty v2 retained its `VOID2002` and survived reimport.
A newer edit reported `missing_new`. Changing the document to Plain Text
sent an actual `didClose`, after which the importer reported the ordinary
missing physical file. Recreating the file sent a creation watch and cleared
the error. The Windows cached TextMate worker limitation remained visible,
and the diagnostic lifecycle checks completed. The reused editor harness calls
its corrected-behavior mode `refresh-source` and records `candidate: true`.
Its executable path and the native build receipt identify the rebuilt
production server used for this qualification.

### Final implementation inventory and disposition

The complete checkout inventory includes the research work already present
when the implementation began.

- `src/lsp/main.cpp` removes the watched-deletion unavailable mutation.
- `tests/project/protocol.py` reconciles open-entry watch expectations and
  adds the maintained overlay, disk, explicit-operation and manifest controls.
- `docs/LANGUAGE_SERVER.md` documents watch refresh, open ownership and
  explicit file operations.
- `docs/roadmap/MILESTONE_REGISTER.md` records G21 acceptance and the
  authorized G22 implementation pending Golden Wrench review.
- `docs/compiler-health/G22_WATCHER_OVERLAY_RESEARCH.md` retains the research
  evidence and adds this implementation result.

Implementation receipts are under `build/g22-fix-evidence/` and
`/home/shmellyorc/VM/g22-fix-20261112/`. Original research evidence remains
under the earlier G22 directories and the preserved G21 trace. The original
production failure, updated-test rejection on the original server and corrected
native editor traces are retained separately.

`git diff --check` passes. The working branch and HEAD remain `main` at the
starting SHA. No staging, commits, pushes or hosted CI occurred. The patch is
ready for Golden Wrench review and Boss-controlled integration. The original
deferred integration gate and release holds remain recorded in the milestone
register.
