# W2 — Native Windows x64 D5 packaging research

Research baseline: `01c465d444060fae1e2806a21673db5ccacfcc8f`, inspected 11 November 2026. Research only; no production changes, native experiments, builds or CI dispatches were performed. Recommendations below are not native acceptance receipts.

## Recommendation

Reuse D5's package assembly, D4's managed payload, the existing Windows AOT linker and W1's private-directory ownership. Add a Windows target policy to the existing packaging implementation, not a second packaging engine. Most work is bounded: PE validation, platform permissions, notices, propagation of existing stage authority, and maintained tests.

**The unresolved contract is replacement of an existing nonempty package directory.** Linux D5 uses `RENAME_EXCHANGE`. The inspected Windows APIs do not provide that operation. A backup rename followed by publication is not an atomic exchange and cannot guarantee that the old package stays at the requested pathname after every failure. Do not silently implement that sequence as equivalent behavior.

Recommendation for independent review: **GO for the shared implementation design; owner decision required before implementing existing-directory `--replace`.** Preserve the strict contract by default: fail closed on Windows replacement of an existing directory. If Boss requires replacement in W2, obtain explicit approval of the weaker, offline replacement contract described below. Without that decision, do not claim all four requested command forms are complete. This is a W2 scope question, not a new release hold.

## 1. Verified implementation and acceptance boundaries

The local HEAD matches the reviewed W1 commit. GitHub's run and jobs APIs report [W1 run 42986407961](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961) successful at that commit, with successful [Release](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961/job/130408526334) and [Debug](https://github.com/Shmellyorc/Void.Language/actions/runs/42986407961/job/130408526896) jobs. This establishes W1 acceptance, not Windows D5 acceptance. The maintained W1 workflow selects six tests and native discovery/check/JIT/AOT smoke. The qualification document's hosted-pending wording is stale.

| Current component | Verified source behavior | W2 consequence |
| --- | --- | --- |
| CLI/project selection | D5 uses existing project entry, application identity, profiles, source/native locks and `--allow-native`; default destination is `dist` | Preserve command parsing and frontend authority |
| Package builder/verifier | `src/native_build.cpp:349–559` is restricted to Linux x86-64 GNU; header names that target, executable is `app`, verifier expects POSIX modes and ELF | Introduce a small target policy and Windows image/permission handling |
| AOT executable builder | `link_executable()` already supports pinned Windows native resources, private short linking workspace, checked copy and executable publication | Reuse it; do not redesign Stage 9 staging or linker invocation |
| Native catalog | `src/frontend/project_native.cpp` admits Windows AMD64 DLLs/import libraries, checks artifact hashes and exact catalog membership, and builds the managed payload | Reuse the selected catalog; do not infer or fetch undeclared DLLs |
| Runtime loading | `src/runtime/c_dynamic.cpp::installed_dynamic_root()` locates `native/` beside the running Windows executable; managed payload is checked before provider admission | Existing Runtime relocation mechanism is sufficient; no new loader |
| Metadata/resources | Existing encoded `ApplicationMetadata` revision 1; `os.resource_dir()` follows executable location for these applications | Keep `application.meta`, embedded metadata and explicit `assets/` |
| Notices | `cmake/NativeBuild.cmake:115` generates/installs `void-package-notices.txt` only on Linux | Enable Windows notice generation from its actual provider configuration |
| Publication | `src/frontend/project_creation.cpp::publish_application_directory()` is Linux-only; it uses parent `flock`, exclusive creation and `RENAME_EXCHANGE` | Windows needs publication plumbing and an explicit replacement decision |
| Tests | `application_packages.py` currently accepts Windows' unsupported diagnostic; `project_package_native` is registered only on Linux; installed-package coverage is Linux-only | Those branches are not Windows acceptance and must become real execution tests |

The [D5 closeout](DEPENDENCY_D5_CLOSEOUT.md), [D4 closeout](DEPENDENCY_D4_CLOSEOUT.md) and [W1 qualification](WINDOWS_NATIVE_PROJECT_QUALIFICATION.md) remain the scope records. Historical Linux evidence does not establish Windows filesystem, DLL or relocation behavior.

## 2. Shared package contract and Windows requirements

Proposed Windows layout:

```text
app.exe
application.meta
void-package.txt
licenses/                 verified notices and optional application license
native/                   selected managed runtime DLLs and notices, when needed
assets/                   explicitly declared application resources, when needed
```

Keep inventory revision 1 and its sorted SHA-256/length/relative-path records. Use `target x86_64-pc-windows-msvc` for Windows. A target policy can supply the executable basename and image/permission checks; no new manifest language or cross-platform verifier is required. Preserve limits: 128 inventoried files, 256 MiB payload, 64 KiB inventory, at most 64 required notice entries of at most 1 MiB each, and the existing 32 flat application resources/16 MiB aggregate resource limit.

Keep the profile, lock and selected-input digest. Replace the hardcoded `record.ends_with("\tapp")` exclusion in `build_application_package()` with the selected executable name: otherwise `app.exe` would incorrectly enter the non-executable input digest. Preserve repeat-build receipts and separate actual executable hashes from cross-location determinism. Do not promise byte-identical PE files across build roots without a separate result.

Inventory verification is a content receipt, not a signature or publisher identity. Current verification decodes the sidecar metadata; it does not prove that sidecar metadata equals an independently extracted embedded copy. Runtime provider admission checks its existing managed payload, not the entire application inventory on every launch. Preserve these boundaries rather than implying new authentication guarantees.

### PE and native dependencies

Use LLVM's existing COFF reader. `native_windows::verified_executable()` already checks executable characteristics, non-DLL status and section contents, but accepts both PE32 and PE32+ and does not itself establish AMD64. W2 package validation must additionally require PE32+ and `IMAGE_FILE_MACHINE_AMD64`. Reuse/factor the byte-level validation where useful so packaging validates the same bytes it hashes. Keep single-link/read authority from the filesystem layer. Reject truncated or malformed images, DLLs masquerading as `app.exe`, COFF objects and wrong-machine files. See Microsoft's [PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

`CMakeLists.txt` selects `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`; `cmake/NativeBuild.cmake` supplies `-fms-runtime-lib=static`. Preserve that in both compiler build configurations: Debug is not an authorization to switch deployed applications to `/MTd`. `/MT` statically links the CRT; Windows system DLL dependencies remain. Inspect normal and delay-load imports with the pinned LLVM tooling during qualification, and record the actual application and provider DLL closure. Do not invent a universal fixed DLL list from linker flags. Microsoft's [CRT library table](https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=msvc-170) distinguishes `/MT`, `/MD` and nonredistributable Debug CRTs.

The compiler's LLVM SDK dependencies are not automatically generated-application dependencies. Package only the D4-selected runtime payload; import libraries, compiler binaries and arbitrary DLLs discovered through PATH are not package contents. Windows HTTP providers use Schannel/WinIDN and pinned static archives, not Linux's AWS-LC/libidn2 deployment assumptions. Provider-specific prerequisites must be stated and independently executed on Windows.

The existing loader uses `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`. Retain its descriptor, payload hashes, NativeSafe admission and code/resource leases. This is ordinary Windows loading: API sets, known DLLs, redirection and loaded-module behavior remain OS concerns, not a newly proven cryptographic dependency closure. W2 must not restart the DLL-loader investigation or broaden PATH/CWD search. See [Windows DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).

### Notices, names and permissions

Generate build-tree and installed-compiler notice inventories using the existing CMake machinery. Share the common Runtime notice list and select backend-specific entries: Windows Schannel must not require absent AWS-LC notice inputs. Preserve VOID MIT, applicable LLVM/compiler-rt, Ryu/date/tzdata/Unicode, host-provider and HTTP/PSL attribution, including corresponding source/provenance already required by the selected distribution. Preserve D4 provider notices in `native/`. Missing or altered required notices must fail packaging before publication.

The [redistribution audit](../release/REDISTRIBUTION_AUDIT.md) is dated evidence; its earlier unselected-license status is superseded by the [owner-selected licensing scope](../release/LICENSING_SCOPE.md). Original Runtime code is included in that MIT scope. Third-party and Microsoft redistribution terms remain applicable; `/MT` is not a licensing waiver. W2 assembles local application directories, not a public SDK release or installer.

Preserve UTF-8/UTF-16 conversion through `utf8_path`/`path_utf8` and wide APIs. Unicode, spaces and literal `%` in outer project/output paths remain supported. Keep the resource parser's portable ASCII internal names and all existing reserved-name, traversal and exact-case checks. `package_leaf()` currently also admits UTF-8 notice/payload leaves, whereas `fold_project_name()` only folds ASCII: Windows physical-name admission must catch OS-equivalent Unicode names and 8.3 aliases too. Do not silently treat ASCII folding as complete Windows identity or normalize source identities. Reuse existing physical admission; if an additional comparison is needed for accepted non-ASCII leaves, keep it Windows-local and verify against actual filesystem collisions.

Do not use `std::filesystem::permissions()` to simulate POSIX 0755/0644 on Windows. Retain Linux's checks; on Windows preserve private creation DACLs, no reparse points, regular/single-link files, exact names, checked identities and held ownership. Published files inherit the private application-stage policy; do not broaden trusted principals. `--verify` must remain read-only and accept a legitimate copied directory under its current filesystem identity, rather than requiring the original creator SID encoded in an inventory. It must not silently reset ACLs. The existing inventory describes ordinary file data, not an authenticated NTFS security descriptor or arbitrary alternate-stream inventory. Reject colon/stream paths through existing path admission; do not claim undocumented stream authentication.

## 3. The W1 handle fix must reach package preparation

This is a concrete integration dependency, not a reason to loosen sharing flags.

W1's `Parent(..., const Node* stage)` in `src/frontend/project_creation.cpp:184` borrows the owning stage handle after checking its identity. An owned `Node` has DELETE authority with no delete sharing. Reopening that directory through ordinary `pin_directories()` can conflict with the operation's own reservation. `ProjectDirectoryReader` allows staged verification to reuse this authority inside the verification callback.

D5 currently passes only a pathname to its preparation callback. Inside it, `verify_package()` and `package_inventory()` use ordinary project readers. More importantly, `link_executable()` constructs `Staging`, whose Windows `Directory` calls `pin_directories(output.parent_path())`; managed `native/` publication also opens a parent. Simply using a borrowed reader in the final inventory scan would leave these nested operations exposed to the same self-conflict.

Extend the existing operation-scoped stage-authority plumbing to preparation, checked reads and nested native staging/publication. The owning publisher must outlive all borrowers; borrowers may operate only beneath the admitted path and must retain identity/reparse/exact-name checks. If Windows uses multiple owned nested nodes, account for each reservation instead of borrowing one and reopening another. A Windows fresh package may use the existing directly owned private stage as its package root, avoiding an unnecessary second owned directory. Keep the Linux exchange layout unchanged.

Do not add a global trusted-path registry, close the owning handle early, downgrade pins to metadata-only access, or add `FILE_SHARE_DELETE` to ancestor pins. The existing short native-link workspace and checked executable transfer remain unchanged. Close finished read/link handles at the publication boundary while retaining the publisher's ownership and ancestor pins. Qualify this nesting explicitly before interpreting any error 32 as an external file lock.

## 4. Publication and replacement decision

### Safe fresh publication

Build and verify completely within an exclusively created private stage on the destination volume. Revalidate the pinned destination parent and stage identities. Publish by the existing handle-based exclusive rename pattern only if the destination is absent. Concurrent creators must yield one complete winner and a collision/busy loser; no merge, overwrite or recursive deletion of an unowned destination. Keep cleanup ownership conditional on successful exclusive creation.

No change to ACL principals or directory rename-exclusion guarantees is necessary. Sharing violations from an executing/locked output are failures to report, not permission to weaken reservations. Test both fresh absence and a destination appearing immediately before publication.

### Why a direct port of replacement is unavailable

Microsoft documents that `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` fails when the destination is a directory; directory moves stay on one volume. Its write-through flag is not a multi-directory exchange. [MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw).

The native rename information likewise excludes replacing an existing directory. `FILE_RENAME_POSIX_SEMANTICS` changes treatment of open replacement files; it does not supply Linux `RENAME_EXCHANGE`. [FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information). `ReplaceFileW` is a file replacement API, not an application-directory transaction. [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew).

Do not introduce TxF, a junction/current-directory pointer, launcher or installer framework to hide the difference. Microsoft recommends alternatives to adopting TxF, and pointer-based layouts would change the current package and alias contracts. [TxF guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/deprecation-of-txf).

| Choice for existing output | Preservation | Recommendation |
| --- | --- | --- |
| Fail closed; package to a new absent destination | Existing output remains untouched; no replacement claim | Default under the current strict contract; explicitly incomplete `--replace` support |
| Explicitly approved offline backup/publish/rollback | Old bytes retained, but target pathname can be temporarily absent; rollback can itself fail | Only if Boss accepts this documented Windows difference |
| Delete old tree, copy files over it, or emulate exchange without rollback | Can destroy good output or expose mixed versions | Reject |

If offline replacement is approved, the smallest candidate stays inside `publish_application_directory()`: finish all preparation first; serialize cooperating publishers; admit a valid old inventory with the same application ID; retain old directory identity/ownership; rename old to an exclusively reserved same-volume backup; exclusively publish new; retire only the verified old backup after commit. On a second-rename failure, attempt exclusive restoration, never overwrite an intervening destination. If restoration is impossible, retain old output and report its exact recovery location. This is a proposed state machine requiring native tests, not a proven atomic operation.

Reuse the checked writer-file/`LockFileEx` pattern in `publish_dependency_lock()` if this option needs serialization. It locks a file range, not a directory as Linux `flock` does. A stable private administrative lock object must be admitted without aliases; do not unlink/recreate it while waiters might hold the old object. The lock coordinates VOID publishers, not hostile writers or arbitrary applications. [LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex).

**Critical cleanup condition:** `with_project_stage()` currently removes its owned workspace on ordinary failure. Once that workspace contains a displaced valid package, ordinary cleanup is no longer safe. Explicitly retain it until commit or successful restoration; on ambiguous identity, preserve evidence and output. Process termination between renames can leave the original pathname absent. Tests and documentation must expose that limitation. Linux D5 itself does not claim tested power-loss durability; W2 must not invent it for either platform.

## 5. One implementation assignment for Keyboard

After Wrench/Boss resolve replacement semantics, implement and qualify W2 as one coherent change:

| Files/components | Required work |
| --- | --- |
| `src/native_build.cpp` | Shared target/header/executable policy; PE byte validation; Windows permission handling; stage-aware package assembly/verification; correct executable exclusion in input digest; retain all limits and failure diagnostics |
| `src/frontend/project_creation.cpp`, `src/frontend/project_files.hpp` | Windows application-directory publication; propagate existing borrowed stage authority through preparation and verification; chosen replacement policy, identity-safe cleanup and concurrent publication |
| `src/native_windows.hpp` | Only the minimal shared pin/authority or PE-helper factoring needed by nested package linking; preserve existing private-root and rename-exclusion protections |
| `cmake/NativeBuild.cmake` | Shared/common and platform-specific required notice inputs; generate/install Windows package notice inventory; keep verified native resources and `/MT` |
| `tests/project/application_packages.py` | Real Windows package/verify/run, negative inventory and publication tests; platform-specific permissions/isolation instead of successful unsupported exit |
| `tests/project/managed_native.py` | Enable maintained `--package` Windows route, use `app.exe`, verify selected DLL payload, relocation, corruption rejection and resource lifecycle |
| `tests/cli/install.py`, `CMakeLists.txt` | Windows installed compiler/package execution and native-package registration; retain Linux coverage |
| `tests/stage9/windows_native_staging.cpp`, `tests/project/creation.cpp`, `tests/project/creation.py` | Nested owning-handle controls, sharing violations, identity/collision/cleanup and the approved replacement failure controls |
| One new `.github/workflows/ci-windows-packaging.yml` | Manual-only focused Debug/Release Windows x64 qualification, using W1's pinned setup and private root helper |
| D5/CLI and W1 Markdown closeouts | Document actual Windows layout, limitations and native results; incorporate accepted W1 hosted run and both jobs without erasing original failures |

The Runtime loader, GC, source semantics and metadata encoding should require no changes. If implementation discovers otherwise, report the concrete dependency before expanding W2. No changes to W1's six-test selection or a renewed TLS certificate/loader investigation are needed.

## 6. Focused acceptance plan

Use native Windows x64 Debug and Release, including both application profiles where maintained fixtures already do so. Cross-compilation and Python-only checks do not qualify Windows. Reuse focused CTests: `project_application_package`, `project_package_native`, `project_managed_native`, `project_creation`, `project_creation_process`, `project_manifest` and `cli_install`; add only existing directly affected native-staging controls. Assert exact discovered names and actual execution so unsupported/skip branches cannot count as success.

| Area | Required result |
| --- | --- |
| Ordinary application | All requested non-replacement command forms produce/check a PE32+ AMD64 `app.exe`; metadata/resources match; native execution returns expected results from an unrelated CWD |
| Installed compiler | Package using a relocated compiler installation and its installed notices/resources, not only the build tree |
| Managed native | Synthetic selected DLL/provider payload works after relocation; missing/changed DLLs fail before provider initialization; expected lifecycle/cleanup remains correct |
| Independence | Run with compiler checkout/build, original project and native catalog genuinely inaccessible; include a control proving those paths cannot be read. Scrubbing PATH or moving just the project is insufficient |
| Path handling | Spaces, Unicode, literal `%`, accepted long-path forms; wrong-case names, 8.3 aliases, case-equivalent collisions, reserved names, traversal and colon/stream inputs fail according to admission rules |
| Inventory | Missing/modified/extra executable, metadata, assets, notices or native files fail; wrong target, malformed PE and wrong architecture fail; required notices absent at packaging preserve prior output |
| Filesystem authority | Symlink, junction, other reparse points and hard-linked files rejected; nested stage readers/linker do not self-conflict; ancestor-rename exclusion preserved; access-denied and externally locked destinations leave valid output untouched |
| Publication | Competing creators produce one whole package; unrelated/existing wrong-ID or corrupt output never replaced. Inject preparation/link/verification/publication failures and inspect both output and owned workspace |
| Replacement | Strict option: existing-output refusal leaves exact prior bytes. Approved offline option: successful replacement, second-rename refusal, rollback refusal, concurrent collision and termination between renames preserve old bytes/recovery location, with the admitted pathname-gap limitation |
| CRT/dependencies | Record pinned toolchain and provider identities; inspect PE normal/delay imports and actual loaded DLL locations; no accidental dynamic/Debug CRT or original-checkout dependency in the maintained fixture |
| Reproducibility/regression | Existing same-input receipt checks, location-independent selected-input digest and data identities; focused Linux D5/publication regressions after shared changes |

For Windows isolation, use a disposable, explicitly owned test environment and a child whose access to the original roots is denied, or an equivalently isolated native environment. Verify denial in that child before launch. Do not alter the shared checkout ACL or delete an active compiler build. A token/ACL helper, if needed, belongs in the focused fixture and must restore its owned setup in `finally`; retain no general sandbox framework. Native feasibility of this control is still unverified.

The later workflow should use `workflow_dispatch` only, `windows-2022`, Debug/Release matrix, the W1 pinned LLVM/HTTP/host setup, and its tested private temporary-root creation helper. Build only the compiler and selected fixture dependencies. Run focused tests and import/isolation controls, with no full compiler CTest or graphics marathon. Local implementation and independent review precede an owner-authorized dispatch. Record Windows 10 local results separately from Server 2022 hosted results; neither proves every Windows release or filesystem.

Follow the [archive policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md): Markdown outcomes and concise command/test summaries; preserve exceptional failure evidence selectively outside the checkout, compressed when appropriate. Delete only this assignment's owned disposable applications/workspaces after verification. Preserve active SDKs, builds and previous incident archives. No raw log or binary bundle was generated by this research.

## 7. Decisions and remaining uncertainty

**One genuine owner decision:** retain strict atomic directory replacement and leave Windows existing-output `--replace` unsupported, or explicitly approve offline replacement with a pathname gap and recoverable backup. The latter does not preserve the former contract. Recommend strict refusal until that choice is recorded; do not label a partial implementation full D5 acceptance.

Native qualification must still establish nested handle behavior, PE dependency closure, relocation isolation, locks/antivirus interference, Windows filesystem name equivalence and any approved rollback state machine. No source evidence alone proves those execution results. Minimum supported Windows versions and non-NTFS/network-volume behavior remain bounded by existing supported-platform contracts and actual tests; do not silently promise universal filesystem support.

The final Keyboard closeout should link the accepted W1 run, state precisely which W2 commands and replacement semantics passed, list remaining limitations, and record temporary-workspace cleanup. Stop for Golden Wrench review before integration, Git actions or CI authorization.
