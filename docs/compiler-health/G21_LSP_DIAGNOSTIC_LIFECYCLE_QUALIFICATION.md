# G21 diagnostic lifecycle qualification

Batch 19 starts at `c80e686a30e83ebf154985dcd00f5f40296882f8`. G21-R is
accepted and integrated at that SHA. G21 implementation is in progress pending
Golden Wrench review. This record covers the maintained test changes and their
local qualification. [G21-R experiments and historical evidence](G21_LSP_DIAGNOSTIC_LIFECYCLE_RESEARCH.md)

## Reconciled contract

The original Stage 7 test failed at `tests/lsp/protocol.py:126` on all five
profiles. It expected an empty diagnostic array after removing an import of a
virtual file that was still open. Stage 7 originally used the selected graph
for diagnostic ownership. G13 added independent compiler analysis of eligible
open documents. Import removal leaves the overlay open, so current independent
analysis still reports the unresolved identifier. The selected graph can
recover navigation while the independent error remains visible. The publisher
merges current diagnostics by URI and deduplicates across owners.

The reconciled test requires one publication at client version 2 containing one
`VOID2002` for `missing`, severity 1, source `VOID`, at zero-based UTF-16 line 1,
characters 11–18. The following `didClose` must publish one empty array without
a client version. The prior referenced close continues to verify that the
importer diagnoses the missing physical file. Every later legacy protocol case,
including shutdown and client cleanup, runs to completion.

The archived graph-only publisher was rebuilt as a disposable PikaOS Release
executable. It passes the original legacy test, fails the reconciled assertion
by publishing an empty array at version 2, and fails the existing G13
undeclared-package diagnostic check in `project_protocol`. Independent analysis still follows compiler root
identity and dependency authorization. Its editor results do not grant
execution authority.

## Maintained changes

- `tests/lsp/protocol.py` verifies exact diagnostic identity, range, publication
  count and close clearing. Reopen at a reset version checks new text and a
  different identifier with a supplementary Unicode character before its
  range. The test accepts both signed 32-bit endpoints, rejects stale, equal and
  overflowing changes, and checks a single-write open/change/close sequence for
  superseded diagnostics.
- `tests/project/protocol.py` extends the G13 fixture to check retention,
  correction, reintroduction, reimport deduplication, disk fallback after
  closing a referenced overlay, and publication revision advance on reopen with
  client version 0. Explicit entry switches exercise the open diagnostic under
  both owners. Final close clears the independent error. The original
  dependency, project readiness and navigation checks remain in the fixture.
- `docs/LANGUAGE_SERVER.md` records G13 ownership, overlay and disk behavior,
  clearing and refresh triggers. The historical Stage 7 acceptance record
  remains intact.
- `docs/roadmap/MILESTONE_REGISTER.md` records research acceptance, the rejected
  candidate and G21 review status.
- `docs/compiler-health/G21_LSP_DIAGNOSTIC_LIFECYCLE_QUALIFICATION.md` records
  the maintained integration results.

Production diagnostic ownership, compiler diagnostic definitions, authorization
and editor code are unchanged. G19 and G20 records remain intact.

## Native protocol results

The final maintained selection contains 17 CTest cases. Each profile ran the
original legacy test separately to reproduce the assertion, followed by the
selection with the reconciled files.

| Native profile | Original legacy test | Maintained selection |
| --- | --- | --- |
| PikaOS 4 x64 Debug | Failed at original assertion | 17/17 passed |
| PikaOS 4 x64 Release | Failed at original assertion | 17/17 passed |
| PikaOS 4 x64 strict ASan/UBSan | Failed at original assertion | 17/17 passed |
| Windows 10 Home x64 19045.6466 Debug | Failed at original assertion | 17/17 passed |
| Windows 10 Home x64 19045.6466 Release | Failed at original assertion | 17/17 passed |

`project_protocol` reports 240 checks per profile. The original fixture
reported 205 on PikaOS Release. The maintained selection is:

- `lsp_protocol`, `project_protocol`, `lsp_project_root_recovery`, `lsp_stage7`
- `lsp_editor_recovery`, `lsp_enum_recovery`, `lsp_enum_equality`,
  `lsp_quick_fixes`, `numeric_enum_lsp`
- `tooling`, `analysis`, `project_sessions`, `project_selection`,
  `project_dependencies`, `lsp_semantic_tokens`, `lsp_documentation`,
  `lsp_stage10_intelligence`

All runs reused verified native LLVM/Clang/LLD 23.1.4 builds. The Linux server
hashes match the G21-R receipts. The strict run enabled ASan leak detection
and halt-on-error for ASan and UBSan. Its focused logs contain no sanitizer
reports.

The existing VirtualBox guest labelled “Windows 11” reported Windows 10 Home
19045.6466. Microsoft's VS 2022 x64 Native Tools environment provided MSVC
14.44.35207 and SDK 10.0.19041.0. Tests ran on a local NTFS volume with verified
private TMP/TEMP roots. G20's qualified binary hashes and 307 archived source
identities matched, and both CMake caches pointed at the existing
profile-matched host providers and Schannel dependencies. The guest ran the
exact two maintained files by SHA-256 and restored the original copies after
qualification.

## Native VS Code 1.141.0

Release editor runs on PikaOS and Windows displayed one imported `VOID2002`
in the editor API and Problems. Removing the import retained the error at the
same client version. Correction cleared it, reintroduction restored one error,
and reimport produced no duplicate.

An earlier Linux replay deleted the disk file during the import sequence.
A delayed watched-file deletion tombstoned the overlay before reimport and
the importer reported `VOID3002`. The trace is retained. The final runs
completed the import sequence before deleting the unreferenced file, observed
the watched deletion clear the error, and used a whole-buffer edit to restore
one independent error for the now virtual-only source. Both runs then observed
that the tab disappeared while the document stayed open and Problems retained
the error.

Changing the retained document's language mode to Plain Text triggered the
editor document-close event and a traced `textDocument/didClose` for the VOID
document. The server published an empty array without a client version, and
Problems cleared. The same server session stayed running. The Windows cached
editor separately reported a missing background TextMate tokenization worker.
The diagnostic checks completed on that host. Editor qualification covers these
two Release runs. The five-profile result above is native process qualification.

Local CTest XML, baseline failure logs, graph-only controls and editor traces
are under `build/g21-evidence/` and
`/home/shmellyorc/VM/g21-20261112/`. The G21-R archive remains the source for
the earlier experimental runs.

The original seven-job hosted gate stays deferred. G08 optimization, Windows
persistent caching and public redistribution remain on HOLD. The maintained
patch is ready for Golden Wrench review and Boss-controlled integration.
