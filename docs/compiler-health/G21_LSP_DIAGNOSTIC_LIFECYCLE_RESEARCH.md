# G21-R — Diagnostic ownership, historical causality and experimental evaluation

Date: 2026-11-12 (America/Vancouver). Baseline: `49d2e66243d80957e7dd1bbeecb9767a43a13964`. Research complete; production repair awaits Golden Wrench/Boss review. LLVM/Clang/LLD 23.1.4 only.

**Recommendation: GO for a bounded test/contract reconciliation; do not change compiler diagnostic ownership.** The legacy `tests/lsp/protocol.py:126` assertion is obsolete under accepted G13. Removing an import does not close an independently analyzed open document. Its remaining `VOID2002` is freshly computed from its current buffer under a new session revision, not retained accidentally from `published` history.

A disposable graph-only publisher passes the complete old protocol test but fails G13 by hiding an undeclared-package diagnostic. Reject that candidate. The unchanged compiler passes the strengthened ownership contract on Linux Release, Debug and strict sanitizers, and native Windows 10 Release/Debug. A real VS Code test confirms retention after import removal and clearing after correction. It also demonstrates a distinct client limitation: closing a tab need not send document-close ownership notifications.

## 1. Evidence inventory and reproduction

The checkout was clean at investigation start. Production source, tests, build configuration and extension code remain unchanged. All candidate code and harnesses were created outside the checkout, principally `/tmp/void-g21r-qay7fo08`; native experiment transport used `/home/shmellyorc/VM/g21r-20261112`. Existing configured builds were rebuilt from unchanged sources where necessary.

The retained evidence archive is:

`/home/shmellyorc/Documents/Void.Language-archives/g21-diagnostic-lifecycle-2026-11-12.zip`

Its `linux/` entries contain the exact fixtures, ordered protocol messages, internal traces, candidate deltas, commands, source identities, upstream source/patch receipts, test logs and XML. `windows/` contains native source/executable identity checks, protocol transcripts, qualification logs, editor observations and cleanup receipts. `MANIFEST.json` records every archived member's SHA-256. Experimental binaries/build trees are excluded; their hashes and reproducible build recipes are retained. No previous G19/G20 artifacts were removed.

Important evidence names:

| Evidence | Meaning |
| --- | --- |
| `baseline-protocol-corrected.txt`, `baseline-transcript-assertion.txt` | Unchanged Release failure at the original assertion |
| `baseline-full-transcript.json` | All 152 ordered client/server messages through the failure, including exact URI spellings, versions and unsaved text |
| `failure-transcript.json` | Exact failing change/publication subsequence, without abbreviation |
| `observed-legacy-internal.jsonl`, `failure-ownership-trace.json` | Accepted session revisions, maps, original source bytes, diagnostic owners and publication history |
| `observed/delta.patch`, `graph-only/delta.patch`, `build_overlays.py` | Disposable observation and rejected publisher change, exact compilation/link commands |
| `protocol-contract.py`, `protocol-contract.patch` | Full legacy protocol copy with stronger G13-compatible expectations; original test untouched |
| `lifecycle.py`, `lifecycle-*.json` | Additional ordered lifecycle controls and bounded memory observations |
| `lifetimes.cpp`, `lifetimes-*.txt` | Direct immutable-analysis lifetime and stale-result acceptance checks |
| `dependency-concurrent.py`, `.patch`, `dependency-concurrent-*.txt` | Bounded external dependency mutation control, based on the existing G20 fixture |
| `pre-g13-protocol.txt`, `historical-source-boundary.patch` | Historical runtime control and exact introducing source delta |
| `identity.json`, `historical-identity.txt` | Tested source/OS/binary identities |
| `upstream-pyright-history-4.json` | Actual upstream close/repull fix and paired regression tests |
| `windows/initial-g21-editor-windows.*` | Preserved unsuccessful visual-close expectation |
| `windows/g21-editor-windows.*` | Subsequent explicit observation of editor model ownership |

Baseline command:

```sh
python3 -B tests/lsp/protocol.py build/llvm23-release/voidls /tmp/void-g21r-qay7fo08/baseline-fixture
```

It fails at `assert not notes[virtual_uri]["diagnostics"]`. Debug, strict and both native Windows profiles independently reproduce the same assertion. An earlier invocation omitted the fixture-directory argument and raised Python `IndexError` before running the test; `baseline-protocol.txt` preserves that setup mistake. It is not a VOID failure and is not the baseline reproduction.

## 2. Exact failing transition

The original test opens `/main.void`, `/worker space.void` and `/values.void`, then opens a nonexistent-on-disk `/new/virtual.void`. Its `file:` URI carries percent-encoded spaces, Unicode, `#` and `%`. It is a supported file-URI overlay, not an `untitled:` document.

The full unmodified protocol is retained in the archive. The decisive sequence, correlating the separately instrumented run, is:

| Accepted internal session revision | Operation | Virtual client version / internal document revision | Diagnostic authority/result |
| --- | --- | --- | --- |
| 10 | Open imported virtual source containing `return missing` | `0` / `1` | Failed selected graph and independent analysis report the same `VOID2002`; publisher emits one copy |
| 11 | Correct it to `return x` | `1` / `2` | Successful graph; old error cleared |
| 12 | Close while still imported | No open client version / close revision `3` | Missing disk file produces import error in the importing source |
| 13 | Reopen valid virtual source | `0` / `4` | Reopening resets client version, not internal document revision |
| 14 | Change to `return missing` | `2` / `5` | Current error from failed graph/independent analysis |
| **15** | Change main to remove the virtual import | **`2` / `5` unchanged** | **Selected graph succeeds without virtual; independent analysis freshly produces `VOID2002`** |

At revision 15:

- `current=true`, physical root available.
- Semantic graph: `/main.void`, `/values.void`, `/worker space.void`.
- Independent analysis: `/new/virtual.void`, invalid, no recovery result used for this error.
- Diagnostic primary owner: `/new/virtual.void`; authority: `independent:/new/virtual.void`.
- Original buffer: `func calculate(x: i32) -> i32:\r\n    return missing\r\n`.
- UTF-16 range: line 1, characters 11–18. Code `VOID2002`, severity 1.
- `published` seeds a destination to clear or replace; it does not supply the diagnostic payload.

This exact notification appears in the uninstrumented transcript after the import-removal change and before the request barrier reply:

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/publishDiagnostics",
  "params": {
    "diagnostics": [{
      "code": "VOID2002",
      "message": "Unknown identifier 'missing'.\nHint: Declare it with var before use.",
      "range": {
        "end": {"character": 18, "line": 1},
        "start": {"character": 11, "line": 1}
      },
      "severity": 1,
      "source": "VOID"
    }],
    "uri": "file:///tmp/void-g21r-qay7fo08/transcript-fixture/project%20space%20%C3%A9%F0%9F%98%80%20%23%20%25/new/virtual.void",
    "version": 2
  }
}
```

The actual test stops before its following close. The extended controls execute that close and verify a versionless empty publication. The `void/barrier` method-not-found replies in the retained transcript are the existing test client's ordered synchronization convention, not an unexpected server error.

## 3. Actual implementation and ownership

Source locations below refer to the baseline, not temporary instrumentation.

| Component | Responsibility and evidence |
| --- | --- |
| `src/lsp/main.cpp:279–335`, `Server::analyze()` | Accept current analysis; build fresh per-module diagnostic groups; seed open/reachable/previously erroneous destinations; combine compiler results; deduplicate; publish one complete array per URI; retain only nonempty publication history |
| `src/lsp/main.cpp:308–310` | Includes diagnostics from `SessionAnalysis::editor_documents()` only while that source is open |
| `src/lsp/main.cpp:314–331` | Clears prior URI spelling when identity spelling changes; includes current client version when supported; discards empty history entries |
| `src/lsp/main.cpp:714–751`, `synchronize()` | Close removes overlay/open authority; change rejects stale/duplicate client versions; full-buffer edits create new internal revisions |
| `src/frontend/tooling.cpp:367–439`, `ToolingSession::analyze()` | Creates a fresh immutable result, analyzes selected project, independently analyzes eligible open overlays, preserves original diagnostics during bounded recovery, and rechecks physical-root authority |
| `src/frontend/tooling.cpp:441–460` | Rejects cross-revision results and duplicate acceptance; keeps historical success explicitly for invalidation; current queries require current snapshot identity |
| `include/void/tooling.hpp:127–165,193–195` | Separate graph, independent-document and recovery owners already exist. Immutable snapshot identity and current-root checks determine freshness |
| `src/frontend/project_selection.inc:621–649`, `analyze_editor_document()` | Reuses compiler parsing/validation, project admission, overlays, package policy and original diagnostic attribution; no editor-side parser |
| `src/frontend/project_selection.inc:25–119`, `ProjectCapture` | Owns source capture and locked dependencies; invalid package/lock input becomes a compiler-owned project error |
| `src/frontend/compiler.cpp:7500–7564,7729–7735`, `validated_project` and recovery call sites | Shared language validation and bounded editor recovery; recovery does not convert a failed source into authorized execution |
| `src/lsp/main.cpp:339–347,535–545` | G20 physical-root observation and existing selection/source freshness checks preserve current query authority |

An open document outside the execution graph can have valid editor intelligence without becoming an application entry or executable dependency. Conversely, a closed physical file can still be diagnosed through the selected graph. Neither `didClose => always empty` nor `not imported => always empty` is the current contract.

### Recommended contract, answering the ownership questions

1. **Opened unreferenced source:** independently analyze its current admitted overlay, including diagnostics. This is accepted G13, not a new proposal.
2. **Import removal:** changes graph membership only. It does not revoke open-document authority or discard the overlay. Recompute the diagnostics under the remaining authority.
3. **Graph authority:** does not exclusively determine all diagnostics. It exclusively controls the selected executable graph; independent editor analysis remains outside it.
4. **Channels:** retain the existing internal owners and one merged `publishDiagnostics` stream. Separate public notifications from each owner would replace each other on LSP clients rather than merge.
5. **Clearing:** publish empty when recomputation finds no diagnostic under any surviving authority, when the final owner disappears, or when authority is invalidated. Clear obsolete URI spellings separately. Do not clear a valid independent error merely because the graph changed.
6. **Republication:** a graph change may require a complete replacement even when the document's client version is unchanged. Internal session identity changes independently. Unchanged diagnostic contents are legitimate; duplicate entries within one array are not.
7. **Unsaved versus disk:** the overlay supplies open source bytes even when the physical file never existed or was deleted. Closing restores disk authority; a missing referenced file produces an import error, while a bad referenced disk file retains its own current error.
8. **Reimport:** use current overlay bytes, remove any ownership duplication and preserve exact diagnostic ranges. No save is necessary.
9. **Close:** remove independent overlay authority; recompute surviving graph/disk diagnostics. An unreferenced virtual-only error clears with no closed client version attached.
10. **Entry reselection:** preserve admitted open overlays while rebuilding project membership; independent analysis still obeys project boundaries, imports, visibility and locked dependencies. It cannot authorize execution or bypass a missing/replaced root.
11. **Failure/recovery:** original source diagnostics survive editor recovery; a previous successful index is historical, not a fallback authorization. Physical-root restoration requires the existing recovery barrier.
12. **Visual tabs:** LSP ownership events are authoritative. Immediate diagnostic removal when a tab vanishes is a distinct client presentation policy, not implied by this contract.

## 4. Historical causality

Only one historical compiler was built. Its C++ source is the exact selected pre-G13 revision, archived outside the checkout without creating a branch/worktree or cloning. The pinned current SDK was reused. A disposable CMake path adaptation routed the historical host-dependency verifier to the existing attested prefix; verifier recipes were compared exactly, and verification was not disabled. The adaptation is retained separately. Initial configuration failures from omitted review records and an unsuccessful include wrapper are retained in `historical-build.txt`; the final build used the direct verifier-path substitution and succeeded.

| Boundary | Evidence and conclusion |
| --- | --- |
| Stage 7 protocol, introduced in `ccfea79f9c4c9a6deaad12cb58a8e7379e62c6db` | The old test and `docs/LANGUAGE_SERVER.md:292–293` describe graph-only analysis. That historical expectation was coherent then |
| Pre-G13 `4d39a2b121b8ba6fb6cfa3b60d02d8e048b27b59` | Built actual historical `voidls`; its complete historical `tests/lsp/protocol.py` passes, including this exact removal/clear assertion |
| G13/Batch 1 `29d50c9a21cdf93579941b872ef815b2ae7f881a` | Source diff adds `editor_documents_` analysis and publication. `tests/project/protocol.py::unreferenced` requires independent diagnostics. The old assertion remains unchanged. This is the earliest useful source boundary of the contract/test conflict |
| G14 `131db64368751f9fdac5608aba11f3241dd9ba6d` | Adds bounded incomplete-source recovery, not revocation of independent ownership |
| G16 `6b28c57ff50256734fb804387f7c0ffca828d51f` | Extends enum recovery; no replacement of G13's ownership rule |
| G18 `3c81b7eb75c44533a5326da589a6fe47144c3c0f` | Improves enum diagnostics; not the introducing independent-analysis change |
| G19 `bd6e0ebe6e43dc26d9a4ce1258d85739806d22e1` | Records rejected performance research; does not introduce this production ownership behavior |
| G20 / G21 baseline `49d2e66243d80957e7dd1bbeecb9767a43a13964` | Physical-root recovery clears unavailable authority. Current failure reproduces with a healthy unchanged root and newly computed independent diagnostic |

The G13 contract is explicit in `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md:132–134` and its Batch 1 implementation record. The stale Stage 7 paragraph conflicts with it. We did not build every intermediate revision or claim a runtime bisect; the pre-G13 runtime control, introducing diff, unchanged assertion and current trace establish the source boundary.

## 5. Internet research: actual mechanisms, fixes and applicability

Upstream source was inspected, not executed. No downloaded scripts, binaries or upstream test suites were run. VOID equivalents of the applicable ownership theories were exercised locally. This distinction matters: upstream tests are source evidence, while the five-profile results below are direct VOID evidence.

### LSP 3.17 and Microsoft's ownership clarification

The official [publishDiagnostics contract](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics) specifies full replacement, server-owned clearing and different single-file/project behavior. [didOpen](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didOpen) transfers content authority; close returns it. The actual 3.17 Markdown sources were also retrieved because the rendered site sometimes returned only navigation.

[Microsoft issue #684](https://github.com/microsoft/vscode-languageserver-node/issues/684) documents the mistaken equivalence between visible tabs and document ownership. The implemented clarification says open/close need not reflect visible UI. It is not a fix instructing servers to clear still-open, unreferenced files. A missing diagnostic version clears current client diagnostics; it does not establish that clearing is semantically appropriate.

**VOID test:** explicit open/close behavior passes; native VS Code independently demonstrates visual close without a text-document close event. Therefore do not implement tab-based clearing in `voidls` or infer `didClose` from import removal.

### Pyright: concrete close/repull repair with paired regression tests

Actual fix: [commit `de5220a32dc46997d0d7940633cdcf963e443a77`](https://github.com/microsoft/pyright/commit/de5220a32dc46997d0d7940633cdcf963e443a77), *Pull Pylance with Pyright 1.1.410 (#11506)*. The relevant hunks in `packages/pyright-internal/src/languageServerBase.ts`:

- Remove the URI from `openFileMap` **before awaiting** workspace lookup, so an immediate diagnostic repull sees closed ownership.
- In open-files-only mode, return an empty full diagnostic report for a closed source instead of analyzing it again.
- Preserve workspace-mode diagnostics for still-owned closed files.

The same commit adds two regression tests in `languageServer.test.ts`: closed out-of-workspace source clears on repull; closed workspace source still reports its error. These tests distinguish owner removal from blanket clearing.

Pinned current source reviewed at `227769493b2cd2fce5c82d3be42c769a947e0f70`: [close/diagnostic handlers](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/languageServerBase.ts#L1142), [program ownership](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/analyzer/program.ts#L1528), [paired tests](https://github.com/microsoft/pyright/blob/227769493b2cd2fce5c82d3be42c769a947e0f70/packages/pyright-internal/src/tests/languageServer.test.ts#L519). `_isFileNeeded` retains client-open sources independently of import reachability; `setFileClosed` and garbage collection of unneeded files distinguish tracked, open and imported owners.

**VOID applicability tested:** retain an erroneous unreferenced open source; clear a closed unreferenced virtual source; on close, retain/recompute errors from a referenced bad disk source. All pass unchanged. VOID's synchronous notification processing already erases open ownership before reanalysis. It has no equivalent asynchronous close/repull race to repair here. Importing Pyright's pull machinery or open-files-only mode would add an unnecessary contract.

### rust-analyzer: multiple internal owners, generation filtering

Pinned `8cbebb0c2c23562b11e4d56efb85ca457c758b2f`: [close handler](https://github.com/rust-lang/rust-analyzer/blob/8cbebb0c2c23562b11e4d56efb85ca457c758b2f/crates/rust-analyzer/src/handlers/notification.rs#L137) removes memory-document authority and invalidates disk loading. [Diagnostic collection](https://github.com/rust-lang/rust-analyzer/blob/8cbebb0c2c23562b11e4d56efb85ca457c758b2f/crates/rust-analyzer/src/diagnostics.rs#L149) clears native syntax/semantic results separately from checker results, merges surviving owners and rejects older generations.

This is a source-backed design comparison, not an identified upstream fix for VOID's exact bug. Reviewed slow tests did not establish a directly equivalent close/import regression, so none is claimed. VOID already has graph/document/recovery owners and immutable revision rejection. The disposable direct API test checks old success and failed results cannot be accepted after revision advance; repeated ownership retirement checks pass. Cargo/checker-specific scheduling and extra public channels would be unsuitable additions.

### TypeScript: open orphan files are inferred-project roots

Pinned TypeScript 5.9.3, `c63de15a992d37f0d6cec03ac7631872838602cb`: [editorServices.ts](https://github.com/microsoft/TypeScript/blob/c63de15a992d37f0d6cec03ac7631872838602cb/src/server/editorServices.ts#L2236) assigns open orphan files to inferred projects; `closeClientFile` at line 5053 removes open state and updates containing projects rather than treating every closed source as nonexistent. [inferredProjects.ts tests](https://github.com/microsoft/TypeScript/blob/c63de15a992d37f0d6cec03ac7631872838602cb/src/testRunner/unittests/tsserver/inferredProjects.ts) cover inferred roots, configured-project retention and closing shared resolutions. These are project-ownership tests, not proof of VOID protocol semantics.

The historical [VS Code TypeScript close handling](https://github.com/microsoft/vscode/blob/e41c19505142614f706ad853bd5c4c2ad0f74e89/extensions/typescript-language-features/src/features/bufferSyncSupport.ts#L500) linked in #684 uses client diagnostic presentation behavior; it is not an LSP requirement. VOID's independent compiler analysis is already the simpler equivalent of maintaining useful intelligence for an open orphan. No separate inferred execution project is needed.

### clangd: invalidate work before publishing the close clear

LLVM 23.1.4, `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`: [ClangdLSPServer::onDocumentDidClose](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang-tools-extra/clangd/ClangdLSPServer.cpp#L930) removes the document and guarantees no subsequent diagnostic callbacks before sending empty diagnostics. [ClangdTests.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang-tools-extra/clangd/unittests/ClangdTests.cpp#L863) checks no concurrent diagnostic callbacks for one file; threading tests track latest contents. `TUSchedulerTests.cpp` also exercises removal and worker shutdown. These support scheduling discipline, but the inspected test is not itself a proof of every close ordering case.

VOID tested rapid batched open/change/close and direct stale snapshot rejection. There is no demonstrated late-worker publication in its current single-threaded orchestration. Copying clangd's open-translation-unit scope would incorrectly eliminate G13 diagnostics or referenced disk diagnostics.

**Upstream conclusion:** the reusable principle is to recompute from surviving owners and reject stale results. VOID already implements that principle. The web research supplied useful adversarial tests and exposed the visual-close caveat; it did not justify a new compiler architecture.

## 6. Alternatives and disposable candidates

| Design | Experiment / source evidence | Verdict |
| --- | --- | --- |
| A — retain independently owned open diagnostics | Current compiler plus strengthened complete protocol and new lifecycle controls, five profiles | **Recommended**; reconciles accepted G13 with LSP ownership |
| B — publish selected graph only | Copied LSP main removes the independent-document publication loop; same unchanged compiler libraries | **Reject**: complete old protocol passes, G13 `project_protocol` fails at `independent imports retain lock policy` |
| C — distinct graph/document diagnostic owners | Already present in `SessionAnalysis`, then merged/deduplicated by `Server::analyze()`; trace verifies authority transition | **Reuse existing mechanism**; no new public channels or persistent cache |
| D — recompute complete diagnostics on membership changes | Already creates a fresh accepted result and fresh publication groups; revision-15 trace proves recomputation | **Retain**; no additional parser or invalidator required |
| Clear on every close / clear on import removal | Contradicted by the referenced bad-disk close control and independent open-source controls | **Reject**; loses valid diagnostics |
| Clear when an editor tab disappears | Neither import membership nor LSP ownership implies tab state; native UI test demonstrates divergence | **Out of scope without an explicit client presentation decision** |

Observation-only server: inserted an environment-gated JSON trace before and after publication, recording sources, document revisions, graph membership, independent/recovery state and history. It fails the unchanged original assertion and passes G13's 205-check protocol, matching the uninstrumented baseline. It is not a proposed production feature.

Graph-only server: identical observation patch plus deletion of the independent publication loop. After its decisive G13 regression, further candidate qualification was stopped; it was not advanced to Windows or presented as a viable repair. Its passing legacy test is evidence that merely making line 126 green is insufficient.

Recommended test experiment: the disposable complete protocol replaces the old assertion with exactly one `VOID2002` at client version 2 after import removal, then adds an empty/versionless diagnostic assertion on actual close. A separate fixture asserts exact diagnostic equality/ranges and broader lifecycle behavior. Only the disposable script's tools import path differs for portability. Production assertions remain untouched.

## 7. Qualification and scenario coverage

| Profile | Original legacy protocol | Strengthened full protocol | New lifecycle fixture | Maintained focused CTests |
| --- | --- | --- | --- | --- |
| PikaOS 4 x64 Release | Reproduces original assertion | PASS | 313 checks PASS | 16/16 PASS |
| PikaOS 4 x64 Debug | Same assertion | PASS | 313 checks PASS | 16/16 PASS |
| Linux strict ASan/UBSan | Same assertion; no sanitizer diagnosis | PASS | 313 checks PASS | 16/16 PASS |
| Native Windows 10 x64 Release | Same assertion | PASS | 313 checks PASS | 16/16 PASS |
| Native Windows 10 x64 Debug | Same assertion | PASS | 313 checks PASS | 16/16 PASS |

Strict runs used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. These are focused results, not full-suite or hosted acceptance.

The 16 maintained tests are `tooling`, `analysis`, `project_sessions`, `project_selection`, `project_dependencies`, `project_protocol`, `lsp_semantic_tokens`, `lsp_project_root_recovery`, `lsp_stage7`, `lsp_editor_recovery`, `lsp_enum_recovery`, `lsp_enum_equality`, `lsp_quick_fixes`, `numeric_enum_lsp`, `lsp_documentation`, `lsp_stage10_intelligence`. Their XML preserves test names, platform-specific IDs and output. G14/G16/G18/G20 controls pass with unchanged source. The G20 root test explicitly reports successful physical-root rename, byte-identical replacement and restoration on all five profiles.

Additional lifecycle controls, in both legacy and manifest modes:

- Erroneous open virtual source remains diagnosed after import removal, with identical diagnostic content and unchanged client version; corrected buffer clears; actual close clears; reimport does not duplicate.
- Missing disk virtual source is never accidentally written by tooling. Closing a still-referenced virtual source reports an import failure in the importer.
- Bad physical disk source shadowed by a good overlay is rediagnosed on close. Deleting the physical source does not displace an open overlay; closing it exposes the missing import.
- Same URI reopens with different contents and a reset negative client version; internal document revision remains monotonic. `INT32_MIN`, `INT32_MAX`, stale and out-of-range versions are exercised.
- Invalid virtual imports, syntax errors and a supplementary Unicode scalar before an error retain compiler codes and exact UTF-16 positions.
- Explicit entry switching preserves independent errors without changing the dirty source. Failed project analysis recovers through existing controls.
- Rapid open/bad-change/good-change/close batches do not publish an obsolete intermediate error. Publication order is recorded, not reconstructed from final client state.
- Forty repeated open/close cycles per mode check error/clear lifecycle. Duplicate entries and required empty arrays are checked. Repeated complete notifications for an unchanged still-owned file are permitted.
- Existing root/dependency fixtures check missing manifest, replaced physical root, invalid lock, changed locked package, reselection, stale intelligence withholding and restoration.

A separate bounded concurrent dependency control extends the existing G20 fixture outside the checkout: 64 atomic replacements of a scratch locked package source overlap 12 completion requests. Release and strict runs return either the previously valid exact completion set or an empty set during overlap; after a stable invalid package, results are withheld; restoring exact bytes plus refresh restores authority. The full G20 fixture still passes. This is bounded evidence, not exhaustive proof of every filesystem race or atomic capture across all independent analyses. It performs no execution and grants no new execution authority.

### Lifetime evidence

The disposable C++ fixture passes Release, Debug and strict. It verifies stale successful/failed analyses cannot be accepted after revision advance; current queries cannot borrow historical success; one historical success may remain intentionally during a failed revision; that owner expires after successful recovery. Eighty independent open/close cycles verify both session-analysis and independently validated-program weak pointers expire after replacement; session destruction releases the final owner. No production lifetime changes are indicated.

Observed RSS over 40 close cycles per mode (KiB):

| Profile | Legacy min–max | Manifest min–max |
| --- | --- | --- |
| Release | 12,804–12,812 | 13,052–13,052 |
| Debug | 13,368–13,376 | 13,684–13,684 |
| Strict | 66,520–110,084 | 81,356–153,220 |

Strict RSS growth is preserved, not called flat or silently discarded. Sanitizer allocators/quarantine and metadata can retain freed storage; RSS alone cannot identify a live compiler-owner leak. Weak-pointer retirement and leak detection pass, but this bounded run is not an unbounded memory proof. No allocator change or performance gate is proposed.

### Native Windows and real editor

The approved existing VirtualBox guest was used, despite its “Windows 11” label. Native Windows 10 Home build 19045.6466, x64 developer environment, MSVC 14.44.35207, SDK 10.0.19041.0 and pinned tool versions were verified. Existing G20 native source files were SHA-256 compared with the G21 baseline, and both `voidls.exe` hashes matched the retained G20 final records before reuse. No cross-compilation/shared-folder build was substituted for native acceptance. Test scratch roots were created by the existing private-root verifier; ACL output confirms owner-only mutation. No trusted principals were broadened.

VS Code **1.141.0**, Windows Release server, existing native extension/test environment:

1. Referenced error appears.
2. Dirty file-URI buffer remains authoritative after its physical file is deleted.
3. Removing the import retains exactly one diagnostic; current document version is unchanged.
4. The visible Problems panel renders the independent error.
5. Correcting the buffer clears both diagnostic API and rendered problem.
6. Reintroducing the error and import does not duplicate it.
7. After import removal, `revertAndCloseActiveEditor` removes the visible tab but **does not emit the workspace text-document-close event** in this run. `isClosed=false`, `isDirty=false`, only the main tab remains, and one diagnostic is still visible.

The first UI run failed waiting for ownership close; its original failure and receipts are preserved. `editor-initial-reconstructed.cjs` reconstructs the initial harness from the exact subsequent local edit; no original harness hash was captured. The follow-up deliberately records the actual state rather than relabeling it a successful close-clear test. The successful observation run is **not** proof of immediate tab-close diagnostic removal. Explicit LSP `didClose` is separately qualified on all five profiles. The editor test uses a file-backed buffer deleted after opening; it does not claim that VS Code can directly open every nonexistent URI or that `untitled:` is supported.

The cached editor also reports a missing TextMate worker asset, as in G20. It did not prevent the six diagnostic/Problems checks; it remains an environment limitation, not a fixed packaging defect. No full UI qualification of Debug or other editors is claimed.

## 8. Risks, excluded interpretations and historical failures

- **No stale-map diagnosis:** the trace proves a fresh independent diagnostic. Clearing `published`, deleting cached errors or suppressing notification versions cannot fix a nonexistent stale-map cause.
- **No second parser:** independent compiler analysis already exists and shares source-language validation. New editor-specific parsing or diagnostic caches are unjustified.
- **Client policy is not server ownership:** if Boss requires errors to vanish immediately when the last visible tab closes, that is a separately specified client presentation decision. It must not revoke graph-owned errors or pretend a retained model was closed. Current G13 and LSP contracts do not require that change.
- **Disk races:** the concurrent control and existing admission checks pass; arbitrary external mutation is not exhaustively linearized by these tests. Do not promise that a metadata snapshot means live files can never change afterward.
- **Historical owners:** retaining the last successful analysis during failure is intentional. Current guards must remain mandatory; never substitute historical results on an authorization failure.
- **Deduplication:** current tuple deduplication is sufficient for the reproduced identical errors. This investigation does not prove every possible multi-context diagnostic collision equivalent or justify a new diagnostic identity system.
- **No performance conclusion:** instrumentation changes observation cost and was not benchmarked. No speedup, memory target, release hold or new performance gate is introduced.
- **G19 deferred capture:** remains rejected for root-error inequivalence; this work neither revives it nor attributes G21's conflict to it.
- **G20:** accepted physical-root recovery remains intact; successful root controls are recorded independently.
- **G18 WinError 5:** historical executable cleanup refusal remains a separate unresolved incident.
- **G06/G12 error 1314:** native file-symlink privilege control remains separate; no privilege bypass was attempted here.
- Setup invocation/configuration mistakes and VM display wake-up are research-environment receipts, not VOID regressions.

## 9. Bounded Keyboard handoff, subject to review

**Do not change `Server::analyze`, `ToolingSession`, compiler recovery, Runtime or diagnostic channels for this failure.** The current production model already implements A, the necessary part of C, and D.

One cohesive repair is justified after approval:

1. Update the old graph-removal portion of `tests/lsp/protocol.py` to assert the current independent error, exact source range/current client version and no duplicate. Then assert empty/versionless publication after actual close. Preserve correction, import restoration, graph navigation and signed-version checks. The disposable patch demonstrates the full test can pass without compiler changes.
2. Consolidate the small lifecycle cases into existing `tests/project/protocol.py` / `tests/lsp/protocol.py` fixtures: selected-graph versus open owner; correction; close/reopen; reimport; referenced bad-disk close; entry switching and version/UTF-16 controls. Reuse existing G20 root/dependency tests and existing tooling lifetime machinery; do not import a permanent tracing framework or duplicate the whole 313-check research harness automatically.
3. Reconcile `docs/LANGUAGE_SERVER.md:284–293` with accepted G13. Explicitly distinguish open-document analysis from execution selection, overlay close from physical-source deletion, and visual tabs from LSP ownership. Preserve the historical Stage 7 record rather than claiming it always described G13.
4. Keep a focused editor regression for import removal and correction. Document the native visual-close observation; do not make a test depend on an immediate VS Code `didClose` that the editor does not promise.
5. Validate the maintained complete protocol plus the 16 related tests on the existing native profiles after final integration. These research receipts establish feasibility; they are not a substitute for testing the final reviewed patch. Hosted CI remains Boss-controlled.

Acceptance: original valid independent error remains visible while owned; correction/final owner removal clears it; referenced disk errors survive close; current project authority and source ranges remain exact; G13/G14/G16/G18/G20 controls pass; no discarded historical failures. Stop and return to review if these changes expose a real publisher defect, require new ownership semantics or alter execution admission.

**GO** for that bounded contract/test reconciliation. **HOLD** any diagnostic-engine redesign, graph-only filtering or new client tab-clearing policy. This is a recommendation, not implementation authorization.

## 10. Changed-file and cleanup inventory

Only these checkout documents are changed by G21-R:

- `docs/compiler-health/G21_LSP_DIAGNOSTIC_LIFECYCLE_RESEARCH.md` — this research handoff.
- `docs/roadmap/MILESTONE_REGISTER.md` — records G20 accepted, G21-R research completed and awaiting review; no implementation acceptance claim.

No production source, maintained tests, root README, build configuration, extension or prior evidence was edited. No staging, commits, branches, pushes, hosted CI or system dependency installations occurred. Disposable code, build trees and fixtures are archived selectively and removed after archive verification; the existing toolchain and G20 build/source directories are retained. The historical `.toolchains` symlink is not traversed during cleanup. Native task-only scratch removal is recorded in `windows/cleanup.txt`.

Final `git status --short` and `git diff --check` are recorded in the archive's closeout receipt. No production implementation was performed. Golden Wrench reviews; Boss controls integration.
