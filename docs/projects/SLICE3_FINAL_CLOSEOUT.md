# Slice 3 final preflight review

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. Awaiting Golden Wrench's final acceptance. Hosted CI is on strict hold.

## Control flow and correction

Originally, `check_project_documents()` caught the resolver's `CompilationError` and bound its records to the active source. `discover_project_context()` then rethrew those records. Its `std::invalid_argument` handler caught the rethrow because `CompilationError` derives from that type. The nested source case already exited 0 with schema-2 error JSON, but the handler replaced the records with a synthetic manifest-owned VOID5009 containing a rendered multiline error. That erased the original code for an alias or invalid identity. Pinned child manifests and unreadable active sources outside E separately returned **ready** in the original runs. [Original Linux process evidence](evidence/slice3-final-preflight/linux-release-original.json).

The final [compiler code](../../src/frontend/project_selection.inc) forwards the owned records from the shared document check. It catches `CompilationError` before its base for errors raised by active-identity validation, classifies filesystem and path failures inside the active check, and rejects a child manifest as the active manifest of a pinned parent. The existing resolver admits a physical active source before preflight opens it for read access. Preflight can still admit a named unsaved source and adds no source-size policy. Sessions retain authoritative dirty bytes through the same resolver. A failure clears the validated project and marks the retained observations unvalidated. The [wire summary](../../src/lsp/main.cpp) gets its entry from the validated project, so these failures produce a null entry. The [C++ regression](../../tests/project/sessions.cpp) checks the exact active path, canonical identity and owned zero-width span after the fixture is deleted.

| Active-document case | Observed final result |
|---|---|
| Pinned parent, source or manifest in child | Parent retained, error with null entry and VOID5009 |
| Pinned root, outside active document | Root retained, error with null entry and VOID5009 |
| Invalid source extension or reserved source identity | VOID3001 or VOID5006, respectively |
| Malformed or wrong-case child manifest with child source active | Pinned parent reports VOID5009. Nearest-child discovery retains the child and reports VOID5001 or VOID5009, respectively. |
| Child manifest itself active under a pinned parent | Exact spelling reports VOID5009 even with malformed bytes. Wrong-case spelling reports VOID3001. |
| Physical source alias | VOID3001 for the Linux symlink and Windows junction |
| Unreadable physical active source outside the selected graph | VOID3002 on Linux with denied read permission and Windows with denied read sharing |
| Valid nearest child with no pin | Ready, child root and child launcher selected |
| Selected root manifest or named unsaved source | Ready with the selected entry |

Domain results above exited 0. Missing operands and an unpinned active path outside the selected workspace exited 2 with stderr and empty stdout. A missing executable produced ENOENT at process launch. Preflight is a filesystem observation. [Original and final command receipts](evidence/slice3-final-preflight.json).

## Local qualification

| Configuration | Result |
|---|---|
| Linux Release | 11/11 focused CTests |
| Linux Debug | 11/11 focused CTests |
| Linux fatal ASan/UBSan | 11/11 focused CTests |
| Windows 10 x64 build 19045 Release and Debug | Each passed 169 wire assertions, 166 session assertions and four legacy LSP suites |
| Linux VS Code | 7/7 adapter tests with an actual older server and 14/14 checks in the real extension host |

The Linux runs rebuilt and relinked the focused consumers and exercised standalone and project JIT/AOT, nested imports, and negative source admission. The Windows runs rebuilt the compiler and relinked native LSP and session consumers against the verified local S3 setup. They exercised the junction and read-sharing denial with no ACL changes. Native legacy stage 8 used the verified S3 CLI. The editor runs covered nullable entries, malformed TOML repair, dirty overlays, old-server rejection, explicit legacy mode, stale results and project switching. Windows clean current-tree CMake, Windows sanitizers, Windows VS Code, Windows Server 2022 and macOS were unexecuted in this review.

The new wire regression failed on the original server's rendered diagnostic. Two Windows fixture attempts then failed before the read-denial assertion because Python's `_winapi` lacks `FILE_ATTRIBUTE_NORMAL` and expects an integer security-attributes pointer. The fixture now uses `stat.FILE_ATTRIBUTE_NORMAL` and a zero pointer. Both failures and the corrective reruns are preserved. [Evidence directory](evidence/slice3-final-preflight).

## Git inventory

The review amended one compiler file and two test files. The other 538 starting paths, including all earlier S1–S3 evidence, are byte-identical to the review checkpoint. The expanded inventory contains 585 paths, with 36 modified tracked paths and 549 untracked paths. All 68,996 index entries match HEAD. The exact three-file patch was reconstructed against the starting copies and checked in reverse against the working tree. [Source diff](evidence/slice3-final-preflight-source.patch) · [Complete layered inventory](evidence/slice3-final-preflight-inventory.txt).
