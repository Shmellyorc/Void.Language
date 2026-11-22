# Windows storage cleanup and VOIDUP update/rollback completion

21 November 2026. Follow-up to the [production-readiness report](../projects/VOIDUP_PRODUCTION_READINESS.md), against accepted G96 HEAD `5fdfb3d9bd1c4933223a72023d882f0278877e8f` and its existing 24-file review candidate. This is a focused operational continuation; no compiler, Runtime, installer or website implementation change is included.

## Cleanup scope

Boss authorized verified disposable Windows milestone outputs, obsolete applications and Codex-generated leftovers to be removed, with source, current LLVM 23.1.4 dependencies, recovery and unique evidence preserved. VOID 1.0's C-based implementation is distinct from the current LLVM-based VOID 2.0. Age or use of Clang alone did not determine deletion.

The exact pre-deletion inventory is `windows/cleanup-plan.json` inside the
[verified review ZIP](/home/shmellyorc/Documents/Void.Language-archives/voidup-windows-completion-2026-11-21/windows-voidup-cleanup-completion-2026-11-21.zip).
It records every selected object/cache file, size, file identity, ownership
classification, reason and retained build configuration. `windows/cleanup-result.json`
records actual deletions and normal application-uninstall commands. No deletion
required changes to ownership or permissions.

| Operation | Exact scope / result |
| --- | --- |
| Generated CMake objects | 1,569 `.obj` files, 1,806,450,693 logical bytes, beneath the inventoried `CMakeFiles/*.dir` directories |
| Historical build roots | `C:\Users\a\VoidG53Write-20261116\{Debug,Release}`, `VoidG20-20261112\{Debug,Release}`, `VoidS7-G{1,2,3}-{Debug,Release}`, `VoidS7-G3Final-Debug`, `VoidS6-Final-Debug`, `VoidS6-Storage-Debug`; exact expanded paths are in the inventory |
| Codex uninstall | `OpenAI.Codex_26.1106.6628.0_x64__2p2nqsd0c76g0`, through Windows `Remove-AppxPackage`; subsequent package and sandbox-service queries returned none |
| Codex generated leftovers | Only inventoried runtime/cache files under `.codex\cache`, `.codex\.tmp`, `AppData\Local\OpenAI\Codex\{bin,runtimes}` and selected embedded-browser cache directories |
| Initial free bytes | 719,077,376 (0.67 GiB) |
| After cleanup free bytes | 8,165,064,704 (7.60 GiB), observed volume gain 7,445,987,328 bytes (6.93 GiB) |
| Deleted file bytes | 2,846,252,063 logical bytes plus the package manager's removal; volume changes include package removal, allocation and concurrent activity |
| Final free bytes after qualification and staging cleanup | **7,623,069,696 (7.10 GiB)**, a net gain of **6,903,992,320 bytes (6.43 GiB)** over task start, with both verified toolchains retained |

Windows is actually Windows 10 x64 build 19045; “Windows 11” is the VM label. Codex was not needed for the tests, which run through the existing host-side VirtualBox command channel. Its package-owned sandbox service disappeared through the normal uninstaller. No broad registry cleaner, package sweep or system-directory deletion was used. Other installed applications were retained because they were current build dependencies, shared runtimes, unrelated software or of uncertain ownership.

Current native SDK/provider paths, the `VoidCI42377496702-Native` environment, current Debug/Release environments and the disposable production-readiness compiler/server/install were retained. Historical source, configured builds, executables, libraries, debug symbols, frozen baselines and original results remain. The verified G53 archive retains accepted source/replay/results; its existing SHA-256 is `e0dd6349b889a51934bcd5cda011672e58702030272c29e2f26cd8839fd29582` and CRC verification passed before cleanup.

Codex authentication/configuration hashes remained unchanged. Chats/history, attachments, projects/worktrees, persistent browser storage and uncertain plugin contents were retained. Its small packaged settings were copied privately to `C:\Users\a\.codex\retained-package-settings-20261121` before uninstall; those bytes are not in the public review evidence. No verified obsolete C-based VOID 1.0 installation or LLVM 19 SDK was removed under an assumed identity; the selected historical build caches explicitly name LLVM 23.1.4. Potential legacy/source locations with uncertain consumers were retained.

The first inventory helper accidentally shadowed Python's `inspect` module. It was renamed before deletion. The first Codex process-stop attempt encountered a child already gone after its parent exited; object cleanup had succeeded, and uninstall resumed without repeating deletion. Final test-staging removal initially found nested empty directories; they were checked and removed from the bottom up. Original errors are retained. These were operational helper failures, not VOIDUP defects.

## Focused native verification

**PASS: 18 focused native cases.** The selected sequence was `stable-current`
(1.10.0) → `stable-next` (1.11.0) → rollback to `stable-current` → reactivation of
`stable-next`. Each selected inventory verified. A controlled 1 MiB interruption
left the prior version intact and runnable; the competing writer was refused,
retry retained the abandoned transaction, and the authenticated update succeeded.
The prior application ran after interruption and rollback. Reinstall, doctor,
compiler launch and a framed language-server initialize/shutdown passed.
Developer notes were preserved and the real user's registry PATH bytes/type
remained unchanged. No implementation repair was needed.

The existing native launcher harness additionally passed Debug and Release:
literal Unicode arguments, isolated volatile-HKCU PATH preservation/idempotence,
extended-length paths, and termination of both held descendants after parent
loss. Both binaries used `/MT` and a 1,048,576-byte stack; the actual user
Environment key was untouched. The launcher source SHA-256 remains
`50d748a9cadf7e37b142cf65d191247eeadda703d55e91a8583e2fc571ebe981`.

Qualification results are recorded in `qualification.json`, with commands, exits, stdout/stderr, source/build identities, signed-catalog digest, control states and storage measurements. The existing `tests/voidup/support.py` HTTPS service and retained engineering catalog/packages are reused. No new signing authority, trust override, native-code approval or production key is introduced.

The scope is authenticated installation, a distinct signed 1.10.0 to 1.11.0 release update, a real interrupted transfer and writer-lock refusal, retry, verified installed inventories, rollback, prior application execution, reactivation/reinstall, selected compiler/server launch and preservation of developer notes. Both release payloads use the same compiler build; this is version transaction qualification, not a second compiler build.

The original disk-full failure remains in the production-readiness evidence. Cleanup itself proves storage recovery only. Native update/rollback success must be established by the separate qualification receipt.

After testing, the remaining three inventoried browser-cache files (1,372,160
bytes) were removed. The test's complete ZIP and interrupted 1 MiB prefix
(159,296,461 bytes total) were matched to the preserved signed producer ZIP,
then their generated transaction staging was reclaimed. `storage-final.json`
records their hashes and exact prefix reconstruction. This was manual fixture
housekeeping after the retention assertions passed; VOIDUP still has no automatic
transaction pruning. Control bytes and developer notes stayed unchanged, doctor
passed with zero retained transactions, and protected tool/credential hashes
matched again.

The compiler/server identity remains `a8882ef8e875e768584632081a99d424bba10cbcdca8c4364d900ec57f4e6952`. Executable SHA-256 values remain `e1342ae84bd9f41e49a518a9f0f9e4f32c0998861e8ab514362628c2fc8ff509` (`voidc`) and `bcde23b9c583c42e84f32fafd7a8f2630a472ef937a7e4f46959ab58af8c40ba` (`voidls`). The retained bootstrap receipt's application/launcher/stdlib hashes are checked before qualification.

Windows raylib/F5, real-user registry propagation into a newly opened terminal, Server 2022, the remaining full first-run refusal matrix and native macOS remain outside this continuation. The real user's registry PATH is preserved; the existing isolated launcher harness supplies registry and process-lifecycle coverage.

## Review and integration

This closeout and the link from the original readiness report are the only new repository changes for this follow-up. The original candidate source, unrelated eight editor edits, installed development tools, Git HEAD and index remain preserved. Independent Sacred Wrench review is pending. Nothing has been staged, committed, pushed, dispatched to hosted CI or publicly published.

The evidence root retains the scoped patch, exact changed-file hashes and ZIP
verification index. The ZIP holds commands, original failures, cleanup manifests
and reproduction instructions with an embedded `_ARCHIVE_MANIFEST.json`.
CRC and extracted-member SHA-256 checks passed before the copied helper files
were retired. Credentials and private TLS keys are excluded. The earlier
full-work cleanup/ZIP remains tied to the reviewed push; this independently
authorized Windows cleanup does not retire the production-readiness source/evidence.

The normal uninstall behavior follows [Microsoft's Remove-AppxPackage documentation](https://learn.microsoft.com/en-us/powershell/module/appx/remove-appxpackage). Local package/service queries establish the actual result.
