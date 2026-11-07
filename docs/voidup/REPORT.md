# VOIDUP Phase A engineering report

6 November 2026. Updated portability implementation and qualification for Golden Wrench/Boss review.

**Recommendation: REVISE platform acceptance; retain the Linux implementation and the new portability source integration.** The Ubuntu 24.04 userspace build and Release qualification now exercise the five host capabilities. Windows x64 has native source implementations, pinned-provider build integration and COFF cross-compilation checks. macOS ARM64 has pinned-provider integration and descriptor-relative directory implementation. **Windows and macOS remain UNQUALIFIED:** native machines and an Apple SDK were unavailable here; cross-compilation does not establish native acceptance.

The original PikaOS Linux implementation and qualification are retained below. The exact prior report and evidence index are preserved in [REPORT-before.md](evidence/portability/REPORT-before.md) and [INDEX-before.json](evidence/portability/INDEX-before.json). New receipts and all failed attempts are under [portability evidence](evidence/portability/). No TCP/UDP implementation, VOIDUP Phase B, root README or website work was performed. No Git staging, commits, branches, pushes, hosted CI dispatch or publication occurred.

## Portability implementation

| Area | Source integration and observable behavior |
| --- | --- |
| Dependencies | `tools/setup_host_deps_native.py`, `cmake/host-sodium.targets` and `cmake/HostDependencies.cmake` add native Windows x64 and macOS ARM64 recipes for the existing libsodium 1.0.22/libarchive 3.8.11 pins. Windows uses upstream MSBuild with the pinned clang-cl, Windows SDK and `/MT` for every profile; macOS uses the physical SDK, explicit ARM64/deployment settings and upstream configure. Libarchive reuses the matching pinned HTTP zlib with external codecs/crypto disabled. Missing, modified or mismatched receipts fail configuration. No ambient provider fallback is selected. These native recipes have not been executed on their native hosts. |
| Packaging and installation | Provider archives, receipts, exact corresponding-source archives and notices participate in all supported-host compiler installations and native inventories. Cache identity includes the native recipe and MSBuild settings. `tools/install.py` now prepares and verifies the matching host-provider prefix after HTTP dependencies; Windows requires MSBuild. Existing Linux prefix layout and build profile semantics remain. |
| Windows directories | `host_directory_windows.cpp` walks validated single components relative to held directory handles through `NtCreateFile`. It opens leaves without following reparse points, checks the returned handle, creates exclusively with a protected current-user DACL and validates private ownership without repairing existing objects. Native handles are noninheritable. Current-user, SYSTEM, Administrators and TrustedInstaller are the trusted ancestor owners; untrusted namespace mutation and ACL changes are rejected. Relative child/file operations use the held parent rather than reconstructed paths. |
| Windows aliases and locks | Independently admitted resources can hold the same native directory. Directory handles share delete access so one owned alias does not prevent another's admission or native rename; authority stays with the held object and checked ancestry. Lock files deny sharing and additionally acquire a nonblocking `LockFileEx` range lock. Their existence confers no ownership; closing the native handle releases the lock. Private regular files reject hardlinks, devices and reparse points. |
| Windows publication | `SetFileInformationByHandle(FileRenameInfo)` renames the held source into a held destination. Storage for the rename is allocated first. No-replace collision and cross-volume errors remain native failures. File publication checks source identity and flushes the held file before rename. After mutation, `published=true` survives directory flush failure, with `durable=false` and primary/secondary native errors. `FlushFileBuffers` failure is never converted into durable success. The per-filesystem ability to flush directory handles needs native qualification. |
| Windows process | `host_process_windows.cpp` supplies explicit `CreateProcessW` application identity, CRT-compatible argument quoting, strict UTF-8/UTF-16 conversion, a sorted Unicode environment and case-insensitive duplicate-name rejection. Only duplicated stdio handles enter the explicit inheritance list. Executable/working-directory ancestry is checked; the executable is held against write/delete substitution during launch. Wait preserves the native exit code and is repeatable. Close and Runtime stop terminate and drain the owned direct child. This does not supervise descendants or add source async/deadline process APIs. |
| Windows argument startup | The existing `wmain`/wide application adapter and per-Runtime argument snapshot are retained. `application.cpp`, the ABI and native providers were cross-compiled for Windows. A native fixture additionally includes invalid UTF-16 rejection; it has not run on Windows. |
| macOS directories | The existing Unix held-descriptor path now runs on macOS, with `openat`/`mkdirat`, no-follow/CLOEXEC descriptors, `flock`, exact held identity checks, `renameatx_np(RENAME_EXCL)` for exclusive publication and native namespace synchronization. `host_posix.hpp` conservatively rejects extended allow ACLs, including inherited grants that mode bits do not represent; deny-only ACLs remain eligible. The check also covers private-file mode application and publication. Existing POSIX spawn/wait and UTF-8 argument behavior are retained. |
| Archive held-file access | `filesystem::regular_length` and `read_at` provide bounded regular-file inspection and positioned reads. Unix retains `pread`. Windows saves/restores the synchronous handle cursor under the existing exclusive `FileBorrow`; restore failure terminalizes the file, preserving the error. ZIP uses this bridge on every platform, retaining the original preflight, ZIP64, CRC, method, path and metering restrictions. No CRT descriptor is manufactured from a Windows handle. |
| Errors and Runtime | `host_error.cpp` holds the existing POSIX classification and an explicit Win32 classification preserving native numbers. It permits native provider fixtures to link without the managed ABI adapter. All source execution/publication still uses the existing ABI, precise roots, per-Runtime ledger, native-job admission, acquisition rollback and quiescence. No new scheduler or source native-handle escape was added. |

Windows paths in the initial implementation are local drive-absolute paths, with optional extended drive prefix. UNC/device namespaces, alternate streams, dot components, trailing-dot/space names, reserved DOS names and `~` aliases are rejected. This is an explicit remaining Windows path scope restriction. Command lines are limited to fewer than 32767 UTF-16 units; the explicit environment has a corresponding bounded size. Windows `private_mode` validates private ownership/DACL for both classifications: Windows has no POSIX executable-mode bit. It never changes permissions on an adopted file. macOS admission conservatively excludes extended allow ACLs even if a particular grant could be safe; those directories are rejected rather than permission-repaired.

The Windows APIs follow the documented [handle-relative creation contract](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile), [handle-relative rename](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info), [Unicode process launch and handle inheritance](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), and [native flush requirements](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers). Darwin's ACL enumeration uses its [zero-on-success convention](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/acl_get_entry.3.html), rather than Linux's return convention. These are source integration references, not native execution evidence.

## Portability qualification

Final command/result inventory is recorded in [the portability index](evidence/portability/INDEX.json). Qualification below distinguishes the prior Linux baseline from the new runs; native platform limitations are not counted as passes.

The new runs used the existing LLVM/Clang/LLD **23.1.4**. PikaOS retains Release, Debug and strict ASan/UBSan profiles. Ubuntu is **24.04.5 LTS, glibc 2.39, x64**, in the local rootless container; compiler, HTTP closure and host providers were rebuilt in an isolated source/build copy using its userspace. The original compiler's newer glibc/libstdc++ loader requirements were not hidden with replacement system libraries. The Ubuntu ELF dependency/version record and container image ID are retained.

| Final check | PikaOS Release | PikaOS Debug | PikaOS strict | Ubuntu Release |
| --- | --- | --- | --- | --- |
| Reviewed host suites: receipts, native provider fixture, portable source, frontend, LSP and native failure suite | 6/6 pass | 6/6 pass | 6/6 pass | 6/6 pass |
| Expanded native provider fixture | 50 checks pass | 50 checks pass | 50 checks pass | 50 checks pass |
| Expanded source suite: normal/O2 JIT and independently linked AOT, including directory/file publication | 30 invocations pass | 30 invocations pass | 30 invocations pass | 30 invocations pass |
| Native boundary/GC/allocation/publication/process/ZIP/IR suite | 300 checks on tmpfs and 300 on XFS | same | same | 300 on tmpfs and 300 on the disk-backed filesystem |
| Existing normal plus hard-stack host execution suites | pass | pass | pass | pass |
| Installed/relocated compiler/providers/notices; normal/O2 JIT and checkout-denied AOT | pass | pass | pass | pass |
| Final cache identity and native provider recheck | 2/2 pass | 2/2 pass | 2/2 pass | covered in recorded Ubuntu suites |

The final provider and source fixtures enforce a real **1 MiB hard main-stack limit** on Linux. The retained/full host suites also enforce hard 1 MiB stacks. `portable_source.py` builds independent AOT executables but does not deny the checkout itself; the existing full host and installed-component suites separately exercise Linux Landlock denial. Neither fixture alone is substituted for that isolation test. Source inputs and result identities remain normal VOID APIs rather than a native installer implementation.

Release's selected regression set passed **16/16**, covering CLI, cache, OS, IO, async acquisition/external completion/jobs and HTTP frontend/LSP in addition to host suites. Strict's selected set passed **13/13**. Ubuntu's selected set passed **13/13** before the final reviewed and expanded fixtures. Debug's original five host suites passed **5/5**; a later mixed run had **12 passes and one missing-fixture executable failure**, retained below, and the final six host suites plus expanded source/provider checks passed after the fixture was built. The recorded command lists, not an aggregate fictional total, define each run's scope.

Ubuntu's fresh libsodium build ran **87/87 upstream tests successfully**. Libarchive's full upstream suite remains disabled; the restricted ZIP reader is tested through the host/source fixtures. The unchanged PikaOS pinned libraries retain their original upstream qualification and exact archive/source hashes. The compiled [process fixture](../../tests/host_capabilities/portable_process.void), [filesystem fixture](../../tests/host_capabilities/portable_filesystem.void) and [full API fixture](../../tests/host_capabilities/smoke.void) show the ordinary source APIs used in these checks. The new receipt admission suite passes **9 checks** for real prefixes and rejection of changed pins, profiles, platform/architecture, file hashes, missing archives and path/recipe escapes. Notice qualification passes all **five** source/install/rule checks; Windows notice-rule simulation is only file-install evidence.

Windows cross checks compile all host implementation translation units and the managed ABI adapter, build the pinned libsodium/libarchive/zlib sources for MinGW, and link a PE x64 fixture using the actual native directory/process/argument/crypto/ZIP implementations. PE inspection records a 1 MiB stack reserve and the OS/MinGW CRT imports. **No Windows executable was run.** The scratch cross-provider prefix is separate from production prefixes and has no production dependency receipt. This cannot establish MSVC CRT, native Windows SDK, kernel, filesystem, compiler JIT/AOT or runtime cleanup acceptance. No macOS SDK/object/link/runtime result is claimed.

### Regressions and retained failed attempts

- **Actual compatibility failure:** the existing PikaOS compiler fails to load on Ubuntu with newer required glibc/libstdc++ symbols. [The exact loader failure](evidence/portability/ubuntu-host-binary-failure.json) remains. Building the compiler and pinned providers in Ubuntu supplies the floor-compatible result; the PikaOS artifact itself remains incompatible.
- **Actual native recipe finding:** libarchive's default regex discovery enters a fatal libgcc search on Windows when that external dependency is disabled. The MinGW cross build exposed this. The native Windows/macOS recipe now explicitly selects `POSIX_REGEX_LIB=NONE` for the restricted ZIP profile, and the complete cross-provider build/link passes. The [first failed configure](evidence/portability/cross-archive-configure-initial.log) and [command inventory](evidence/portability/windows-cross-providers-initial.json) are retained.
- **Portability review fixes:** independent Windows aliases require compatible delete sharing; process ancestry inspection requires `READ_CONTROL`; Darwin ACL enumeration uses a different return convention from Linux. These were corrected during source review. They are not presented as native Windows/macOS test discoveries.
- **Fixture/setup failures:** an initial rootless network backend lacked `pasta`; the dedicated container was recreated with host networking for prerequisites. Two Ubuntu fixture-copy attempts omitted evidence directories and the required curl verification patch. Initial CMake invocations requested a new target before regenerating the Makefile; the resulting missing-target/missing-executable failures are retained. Subsequent explicit target builds and final tests passed.
- **Source fixture failures:** the first process and filesystem fixtures shadowed imported declarations (`args` and `publication`), producing normal compiler diagnostics. The filesystem expectation also treated `io.flush()` as boolean instead of its actual zero numeric result. Original inputs and all failed logs remain; the corrected 30-invocation suite passes in all four available builds. No compiler semantics were weakened.
- **Notice bookkeeping failure:** changing the redistribution wording initially left its inventory hash stale. The five failed notice checks are retained in `licenses/`; the corrected inventory passes all five checks in `licenses-final/`.
- **Reduced cross-link attempt:** initially linking the ABI adapter into a standalone provider fixture pulled managed Runtime symbols absent from that reduced link. Moving unchanged native error helpers into `host_error.cpp` separated that dependency; the full provider cross-link now succeeds. The failed link output remains. This was a fixture/link-boundary issue, not a native Windows execution failure.

There were **no new abnormal compiler/runner exits** in this portability mission. All **181** original indexed evidence files were re-hashed and match their pre-mission values, including the original crash incidents and unfavorable results. Intermediate failures are not counted as passing acceptance, and successful retries do not erase them.

### Reproduction and exact commands

Each `linux-*.log`, `ubuntu-*.log`, cross-build JSON and provider/source receipt under `evidence/portability` records its actual scope and outputs. Representative local commands (run the same profile commands separately for Debug/strict):

```text
cmake --build build/llvm23-release --target voidc voidls void_host_capabilities
cmake --build build/llvm23-release --target void_host_portable
VOID_HOST_EVIDENCE_DIR=$PWD/docs/voidup/evidence/portability ctest --test-dir build/llvm23-release -R '^host_capabilities' --output-on-failure
VOID_HOST_EVIDENCE_DIR=$PWD/docs/voidup/evidence/portability python3 tests/host_capabilities/native.py build/llvm23-release/void_host_capabilities build/llvm23-release/void_host_child --disk
VOID_HOST_EVIDENCE_DIR=$PWD/docs/voidup/evidence/portability python3 tests/host_capabilities/install.py build/llvm23-release/voidc build/llvm23-release/void_host_child
python3 tests/release/check_licenses.py --build-dir build/llvm23-release --evidence-dir docs/voidup/evidence/portability/licenses-final
podman start voidup-portability-ubuntu
podman exec voidup-portability-ubuntu python3 docs/voidup/evidence/portability/windows_cross_build.py
```

Strict execution uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The Ubuntu source copy is `build/voidup-ubuntu-source`; its Release build is `build/ubuntu-release`. The recorded dependency/build logs contain its complete configure commands. The dedicated container is stopped after qualification and retained locally for reproduction. It uses the same pinned LLVM source/toolchain identity and fresh Ubuntu-linked HTTP/host libraries, without overwriting the PikaOS providers.

On an actual native Windows x64 or macOS ARM64 host, prepare the existing LLVM/platform prerequisites, run `python3 tools/setup_http_deps.py --profile Release` and `python3 tools/setup_host_deps.py --profile Release`, configure the host-native CMake build, build `voidc`, `voidls` and `void_host_portable`, then run the `host_capabilities_dependencies`, `host_capabilities_portable` and `host_capabilities_portable_source` tests. Repeat Debug/strict where the native sanitizer/toolchain checks support them. Those are prepared qualification commands, **not commands executed on Windows/macOS here**. Broader native security/fault/GC and installed-platform acceptance remains required as listed at the end.

## Implemented capabilities

| Capability | Ordinary VOID API | Implemented contract |
| --- | --- | --- |
| Application arguments | `os.args()` | Checked owned `result<[string], os.error>` excluding the executable; per-Runtime context admitted before global initialization. Independent list snapshots preserve empty strings, Unicode and option-looking arguments. Limits are 4096 arguments and 1 MiB of UTF-8. Invalid encoding, NUL in embedding input and excessive extents fail explicitly. |
| Cryptography | `crypto.sha256()`, state `update`, `finish`, `close`; `crypto.verify_ed25519` | Incremental SHA-256 with a checked non-wrapping bit count and erased finalized state. Ordinary detached Ed25519 verification over exact bytes; no Ed25519ph substitution. Keys/signatures require 32/64 bytes. Each byte input is independently bounded to 16 MiB. Invalid signatures return `Ok(false)`; invalid sizes return errors. |
| Archives | `archive.open_zip(held_file, limits)`, reader `next`, `read`, `close` | A held ordinary IO file is borrowed exclusively. Preflight precedes libarchive allocation. Only the curated stored/deflate ZIP/ZIP64 profile is registered, with no outer compression filter. Headers are owned snapshots; data streams in chunks of at most 1 MiB. The reader never writes destinations. |
| Private filesystem | `io.create_private_directory`, `open_private_directory`; capability child/file creation, opening, locking, synchronization and publication; `file.private_mode` | Linux directory-relative held descriptors, exclusive creation, no-follow traversal, private permissions, single-link regular files, OS-held nonblocking locks, same-volume publication and checked close. No recursive deletion, permission repair or adoption of another owner's objects. |
| Child processes | `process.start`, child `wait`, `close` | Absolute executable and cwd, literal argument list, complete explicit environment, inherited stdio, direct `posix_spawn`, checked waiting and separate exit/signal status. No PATH search or ENOEXEC shell fallback. An un-waited child is terminated and reaped during explicit cleanup or Runtime shutdown. |

The new shared `system_error` module supplies checked crypto/archive/process errors. Existing IO and OS error categories remain in their established facades. Public declarations carry compiler-owned documentation; the existing LSP discovers them from source metadata.

`spawn` is reserved language syntax, so the public process factory is named `start`. The directory resource is named `io.private_directory`: introducing `io.directory` would collide with the existing public `directory:` argument of `io.create_symlink`. No new syntax, main overload, compiler mode or installer-specific intrinsic was added.

### Application entry and embedding

The CLI accepts `voidc run source.void -- application-arguments...`. An explicit source is required before forwarding. The existing `--` before a dash-leading source still works. Compiler option scanning and the early removed-`--app` diagnostic stop before forwarded arguments; literal `--app` and other compiler-looking tokens then reach the application.

Synchronous `main()`, `main()->i32`, top-level applications and legacy `entry(i32)->i32` retain their entry signatures and exit/printing behavior. Legacy numeric input remains scalar and its CLI/AOT argument snapshot is empty. A Runtime-free application still follows its original direct entry path. An embedding-owned Runtime without an explicitly supplied argument context returns `os.error_kind.unavailable`; it does not infer compiler operands or process-global mutable storage.

Linked application startup admits argv into the new application entry adapter before globals run. The Windows adapter uses `wmain` and strict wide-to-UTF-8 capture in source; native Windows execution of that path is unqualified.

### Archive restrictions and bounds

Native hard ceilings are 16 GiB of archive input, 100000 members, 16 MiB of combined central/local encoded metadata and 64 GiB of declared/actual expanded data. Callers supply independent lower compressed/member/expanded ceilings; the default facade uses 1 GiB, 10000 members and 4 GiB. Metadata object overhead is additional to the encoded-metadata limit, but remains bounded by these ceilings. Input reads use a fixed native buffer and `pread` callbacks, preserving the held IO file's logical position.

The curated profile requires portable ASCII relative components. It rejects encryption, other codecs, multidisk archives, unsupported metadata extensions, links/devices, duplicate/case/type aliases, regular-file ancestors, absolute/drive/backslash paths, dot traversal, reserved device names and trailing-dot/space aliases. Local and central names/methods/flags/sizes must agree. ZIP64 local headers, ZIP64 end records and data descriptors are exercised. Directories have empty data and zero CRC. Whole-entry EOF and CRC verification precede admission of the next file; unread file data returns busy. Closing an unread archive does not claim integrity.

This restriction is a reusable reader profile, not an extraction or installation policy. Phase B must still authenticate the archive, match the signed inventory, meter writes, verify every extracted file and control activation entirely in `.void`.

## Ownership and publication safety

The bridge follows the existing frontend builtin, typed IR, LLVM native-call, Runtime root and `NativeLibrary` ledger architecture. IR validation restricts each private operation to its canonical stdlib module and checks resource authority, arity, types and reply geometry. Ordinary/transitive workers cannot acquire these owner-side effects. Byte and string-list inputs become bounded owned native snapshots before entering NativeSafe state. No managed array pointer is retained by a provider or child process.

Resources retain their managed identity through the existing precise handles and owning-thread rules. Synchronous native calls hold Runtime admission until execution/publication/rollback finishes. Failed acquisitions close native state and release file borrows; any newly created private directory/file can remain inactive rather than being deleted through an unproven pathname. ZIP quiescence releases its file reservation before the ledger closes ordinary IO resources. Child quiescence terminates and reaps entered children before other resources retire. The published provider pointer remains immutable until ledger retirement so concurrent stop and close cannot race a shared-pointer reset.

Linux admission walks held no-follow directory descriptors. Ancestors must be owned by root/current euid and not writable by others, except root-owned sticky directories such as `/tmp`. Final admitted directories must belong to the current euid with no group/other permissions. Child names are single literal UTF-8 components. Creation requests mode 0700 for directories and 0600 for files; restrictive umasks may further limit access. Existing objects are never chmod-repaired. Only files newly created through the capability can receive private 0600/0700 classification. Symlinks, hardlinked files, FIFOs and unrelated ownership are rejected.

`private_directory.publish` uses Linux `renameat2(RENAME_NOREPLACE)` and preserves the held source identity. `publish_file` verifies the same held regular file, flushes it and publishes/replaces one directory entry. Both synchronize source and target namespaces after rename and prohibit cross-volume copy fallbacks.

**Publication returns an allocation-free `io.publication` value.** `published=false` means the rename did not occur. `published=true, durable=false` reports an uncertain commit after rename. Portable/native/secondary error codes remain available without allocating a managed error after the namespace mutation. `durable=true` records successful required native synchronization calls; it is not a universal guarantee about storage hardware, network filesystems or power loss. Source code must inspect this outcome before making further transaction decisions.

Process operations inherit the parent's process group; terminal group interrupts therefore reach parent and child. Waiting has no deadline/cancellable-worker API. Runtime stop requests terminate and drain owned direct children; process-tree supervision is not provided. Phase B must retain inactive staging when interrupted tools may leave descendants, rather than assuming direct-child reap proves a whole tree is idle. Abrupt termination of the parent cannot promise that every child has already stopped; no cleanup deletes staging based on a PID. Embedders must respect the Runtime's child ownership and not independently reap admitted children.

## Pinned dependency qualification

The exact pins are recorded in [host-versions.json](../../third_party/host-versions.json), with source-tree hashes, installed archive hashes, compiler identity, object-member inventories and disabled features in [dependencies.json](evidence/dependencies.json).

- **libsodium 1.0.22**, resolved release revision `77e1ce5d6dee871c49ef211222ba18ef0c486bda`. Source archive SHA-256: `adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`. Static minimal builds use upstream source and run all **87 upstream tests successfully in each of Release, Debug and strict**. See the [upstream release](https://github.com/jedisct1/libsodium/releases/tag/1.0.22-RELEASE).
- **libarchive 3.8.11**, resolved release revision `03036ffa409a2aa1c5bbad35be2202641a040fda`. Source archive SHA-256: `a3dc0872b1b39da34dc18ef1b944f0f11ffd37425d70ae04be679279f1cf43d2`. Static builds reuse the existing profile-specific pinned **zlib 1.3.2** and disable external crypto, iconv, XML and other codecs/tools. See the [upstream release](https://github.com/libarchive/libarchive/releases/tag/v3.8.11).

The builder verifies downloaded archive hashes and rejects a source tree differing from that archive before building. Qualification separately verifies all installed receipt hashes and the exact existing zlib hash. CMake refuses mismatched pins/profiles/provider files. Static archives, receipts, recipes and pin files participate in compiler cache identity; the native-link resource inventory and relocated installation include both new archives. Linux link rules keep provider symbols local, as for the existing HTTP closure. Readelf confirms the selected archive/SHA/Ed25519 symbols are local; AOT receipts retain ELF dependency records and archive-reader symbol inventories.

Libsodium's top-level license is ISC; its SHA-256 component carries Colin Percival's BSD terms. Libarchive's COPYING and controlling per-file terms include BSD/public-domain and the stated component alternatives. The full pinned source archives and controlling per-source notice inventories accompany compiler installation, together with receipts and build/verification scripts. The [notice inventory](../release/NOTICE_INVENTORY.json) and installed-notice checks include the additions. This is exact-content and license-material qualification, not a claim of an independent cryptographic audit, upstream release-signature verification or redistribution clearance for the complete LLVM/SDK/HTTP closure. The full libarchive upstream test suite was not enabled; the restricted reader is covered by the tests below.

## Retained PikaOS baseline validation

Qualification host: **PikaOS 4, Linux x86_64, kernel 7.2.11-pikaos, glibc 2.44**. The compiler/driver/linker are the repository's exact LLVM/Clang/LLD **23.1.4** toolchain. Build profiles are Release, Debug and strict RelWithDebInfo with `VOID_SANITIZERS=ON`. Strict runs use ASan/UBSan halt-on-error, leak detection and strict string checks. LLVM/system libraries and assembly are not claimed to gain instrumentation merely by linking them.

| Qualification | Release | Debug | Strict ASan/UBSan |
| --- | --- | --- | --- |
| Final selected CTest suites | 35/35 pass | 12/12 pass | 12/12 pass |
| Recorded new-capability subprocess invocations, ordinary plus hard-stack suite | 54 + 54 pass | 54 + 54 pass | 54 + 54 pass |
| Execution modes in each acceptance suite | normal/O2 JIT and normal/O2 independent AOT | same | same |
| Final native boundary/failure suite, hard 1 MiB stack | 300 checks on tmpfs; 300 on XFS | same | same |
| Frontend authority/rejection checks | 23 pass, hard 1 MiB stack | same | same |
| New API LSP hover/navigation checks | 31 pass | same | same |

The six main acceptance runs record **324 driver invocations** and exercise **28 ZIP cases through each of four execution paths per run**, for **672 archive examinations**. Additional contention-owner and launched child processes are outside that driver-invocation count. The native suite adds real cross-volume rejection between its source filesystem and `/dev/shm`. Disk runs use exclusively created temporary directories below the private home on XFS; fixtures clean only their owned temporary trees.

Each AOT image is independently linked and executed under Linux Landlock with the source checkout/build tree unreadable. The launcher proves a repository file cannot be opened. Required IDN shared libraries are copied into the admitted temporary runtime closure; hostile PATH and an absent stdlib override do not become fallback dependencies. Receipts retain image SHA-256 and ELF/link information. A separate compiler-component installation/relocation test exercises normal/O2 JIT and linked AOT with the installed providers and stdlib; those AOT runs also deny the installed compiler and compilation sources. All of these AOT tests use real 1 MiB hard stack limits.

Coverage includes:

- Owned argument snapshots during global initialization; empty/space/quote/backslash/Unicode/#/%/option/shell-metacharacter arguments; invalid UTF-8, NUL/extent rejection; legacy numeric entry, dash-leading source selection and fresh arguments with cached Debug code.
- SHA-256 empty/`abc` known answers and incremental chunks; detached RFC 8032 Ed25519 verification; altered bytes, bad signatures/keys and malformed lengths; repeated finish/update/close behavior.
- Stored/deflate archives, ZIP64 local/end structures and data descriptors; traversal/alias/device/link/encryption/codec/metadata/size/truncation/multidisk/CRC rejection; bounded streaming hashes and exclusive input borrows.
- Private creation, no-follow file/directory opens, held-file mode application, changed ancestor namespaces, symlink/hardlink/FIFO rejection, exclusive destinations and held native locks across independent processes. Abrupt owner exit releases the lock without interpreting lock-file existence as ownership.
- Literal executable arguments, explicit Unicode environment/cwd, inherited stdout, private-handle noninheritance, normal/repeated/signal waits, launch failures and rejection of a plain executable text file without a shell fallback. No shell marker is executed.
- Forced GC, concurrent collection during native waiting, allocation-failure sweeps across hash/process/ZIP admission, released borrows, stop/close contention, deterministic child reap and Runtime archive/file shutdown ordering.
- Injected file/directory flush and close failures, no-replace conflicts, cross-volume failure, directory and single-control-file pre/post-rename failure ordering, secondary sync errors and allocation exhaustion immediately after rename. Publication status survives without a new managed allocation.

The Release regression suite covers CLI, cache, OS, IO, globals, native fixtures, async acquisition/jobs and HTTP negative/LSP paths. Debug and strict add CLI, OS host/errors/frontend/LSP, IO execution and automatic-cache regressions. Separate installed-component/source-notice checks pass. This is the selected recorded suite, not a claim that every repository test ran.

### Failures found and resolved

1. **Runtime-free JIT application crash.** Argument admission initially dereferenced a null Runtime owner for a no-import application. The guard now preserves the direct entry path. The exact SIGSEGV command, executable hash, compiler source hashes and `-source_crash.void` are retained in [incident-id03n32t](evidence/crashes/incident-id03n32t/receipt.json). Final CLI and hard-stack suites exercise the reproducer successfully.
2. **Early removed-option diagnostic regression.** A first parser-only implementation changed the diagnostic for `run --help --app`. A bounded early scan now preserves the old diagnostic and stops before application forwarding. The existing 76-check CLI fixture passes in all three profiles.
3. **OS executable-identity test orchestration.** One regression run overlapped relinking of the tested compiler. Its process exited normally but produced `image false`, consistent with Linux executable identity changing when the running image is unlinked during relinking. [Preserved assertion evidence](evidence/crashes/os-executable-relink/receipt.json) records return code 0, not a second compiler crash. The final suite ran after linking completed and passed.
4. **Safety findings during implementation review.** ZIP quiescence now releases its IO borrow before file close; publication returns an allocation-free outcome; stop requests cannot race a mutable published provider pointer. Dedicated failure/concurrency tests cover these final contracts.

Initial harness mistakes concerning Landlock metadata visibility, byte-argument stringification, test Runtime attachment scope and notice hashes were corrected before final qualification. They are not counted as product acceptance results.

## Retained Linux foundation and evidence

| Area | Changed or added paths |
| --- | --- |
| CLI and application context | `src/main.cpp`, `include/void/application.hpp`, `src/runtime/application.cpp`, `include/void/runtime.hpp`, `include/void/runtime_abi.hpp`, `src/runtime/abi.cpp`, `stdlib/os.void` |
| Compiler and native ABI | `include/void/host_ops.hpp`, `include/void/host_abi.hpp`, `include/void/ir.hpp`, `src/frontend/compiler.cpp`, `src/frontend/host.inc`, `src/ir/module.cpp`, `src/ir/host.inc`, `src/backend/llvm/backend.cpp`, `src/backend/llvm/host.inc` |
| Providers and resource ownership | `src/runtime/host_internal.hpp`, `host_abi.cpp`, `host_crypto.cpp`, `host_archive.cpp`, `host_directory.cpp`, `host_process.cpp` |
| IO reuse | `include/void/filesystem.hpp`, `include/void/filesystem_abi.hpp`, `src/runtime/filesystem.cpp`, `src/runtime/filesystem_abi.cpp`, `stdlib/io.void` |
| Ordinary source modules | `stdlib/crypto.void`, `stdlib/archive.void`, `stdlib/process.void`, `stdlib/system_error.void` |
| Build and source closure | `CMakeLists.txt`, `cmake/HostDependencies.cmake`, `CurlProvider.cmake`, `NativeBuild.cmake`, `InstallLicenses.cmake`, `tools/setup_host_deps.py`, `tools/verify_host_deps.py` |
| Dependencies and notices | `third_party/host-versions.json`, `third_party/host-licenses/`, `docs/release/NOTICE_INVENTORY.json`, `docs/release/REDISTRIBUTION_NOTICES.md` |
| Qualification | `tests/host_capabilities/`, this report and `docs/voidup/evidence/`; the preserved OS regression assertion also remains in `docs/os/evidence/crashes/incident-2cmxfrao/` |

[The evidence index](evidence/INDEX.json) records final hashes, build profiles, source paths and verification logs. Per-profile JSON files retain full invocation/output records. The feasibility architecture remains unchanged. The incidental tracked OS deleted-image measurement written by the old regression test was restored to its original bytes.

Reproduction commands, from the repository root:

```text
python3 tools/setup_host_deps.py --profile Release
python3 tools/setup_host_deps.py --profile Debug
python3 tools/setup_host_deps.py --profile strict
python3 tools/verify_host_deps.py
cmake --build build/llvm23-release --target voidc voidls void_host_capabilities void_host_child
ctest --test-dir build/llvm23-release -R '^host_capabilities' --output-on-failure
python3 tests/host_capabilities/native.py build/llvm23-release/void_host_capabilities build/llvm23-release/void_host_child --disk
python3 tests/host_capabilities/install.py build/llvm23-release/voidc build/llvm23-release/void_host_child
```

The existing LLVM/HTTP dependency setup and configured profile directories are prerequisites. The same CTest/native commands apply to Debug/strict, with the recorded sanitizer environment for strict. These commands are local; they do not dispatch hosted CI.

## Platform limits and review decision

**REVISE platform acceptance.** Retain the Linux foundations, the Ubuntu-native userspace build and its recorded Release results, and the cross-platform source integration. Windows x64 and macOS ARM64 remain **UNQUALIFIED** until actual native builds and execution establish their behavior. An available native Windows machine/SDK and Apple ARM64 machine/SDK are the remaining external prerequisites; no hosted CI was dispatched.

The Ubuntu check establishes an Ubuntu 24.04 userspace build/execution result on the existing PikaOS Linux kernel through a local rootless container. It does not certify an Ubuntu kernel, clean physical host, binary distribution bootstrap or every filesystem. **The original PikaOS binary still cannot run on the Ubuntu floor.** Its loader failure is retained. The floor-compatible result is the separately rebuilt compiler/provider closure; a future release artifact must be built on its declared compatibility floor rather than copied from this newer host.

Windows still needs native MSVC/Windows SDK provider builds, Unicode startup, CRT argument/environment and inheritance checks, protected DACL and reparse attacks, independent aliases, long paths, locks, held-file ZIP cursor/failure behavior, native filesystem rename/flush results, allocation-failure/GC/Runtime shutdown, and source JIT/AOT/install qualification. The COFF compilation/link checks use MinGW and its CRT, so they cannot certify the required MSVC CRT integration. They do not run the Windows kernel or NTFS. Native Windows paths currently exclude UNC/device namespaces and DOS aliases as described above. Windows directory synchronization can report a native failure; the implementation preserves uncertain publication instead of promising portable durable success.

macOS still needs a real Apple ARM64 SDK build and native APFS/filesystem, ACL, process, provider, source JIT/AOT, Runtime/GC and installed/deployment-floor qualification. No complete Apple SDK is available here, so there is no macOS object/link or execution claim. The macOS source uses documented native mechanisms and a conservative ACL policy, with actual acceptance reserved for that host.

The original limits concerning network filesystems, real power-loss recovery, filled physical volumes and exhaustive provider allocator failures remain. There is no new async/deadline process API or process-tree supervisor. Namespace and file-flush outcomes retain the existing conditional durability contract. Linux failure/GC/allocation tests do not certify Windows or Darwin equivalents.

Work stops for Golden Wrench/Boss review with native platform acceptance revised as above. TCP/UDP implementation and VOIDUP Phase B remain separate assignments. No throughput or release-performance threshold was invented or evaluated here.


## 6 November 2026 CI repair addendum

The [LLVM 23.1.4 CI repair](INTEGRATION_REPORT.md#llvm-2314-ci-repair) preserves run 42334902548 and
separately reproduces fatal upstream libsodium sanitizer failures. The earlier
local upstream 87/87 result did not establish fatal UBSan acceptance; recover-mode
checks can exit successfully after reports. The current strict bootstrap enforces
fatal settings. Original measurements and qualification records above are retained
unchanged; focused VOIDUP success does not qualify all upstream provider tests.
Windows resource selection is corrected in source; native Windows/macOS evidence
for this repair remains unqualified. No hosted acceptance is claimed.
