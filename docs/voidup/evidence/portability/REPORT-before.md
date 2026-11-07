# VOIDUP Phase A engineering report

6 November 2026. Prepared for Golden Wrench and Boss review.

**Recommendation: REVISE acceptance scope; retain the Linux implementation.** All five reusable capabilities are implemented and exercised on the native Linux x64 host described below. Release, Debug and strict sanitizer checks pass, including real normal/O2 JIT, independently linked AOT and hard 1 MiB stacks. This does not establish the proposed Ubuntu 24.04 userspace floor or native Windows/macOS acceptance. Windows/macOS provider and filesystem work remains explicit rather than being replaced with ambient libraries or unsafe fallbacks.

Phase B has not started. The `.void` version manager, signed release catalog policy, installation decisions, activation, launch selection and rollback remain subject to the requested review. No commit, branch, push, hosted CI dispatch or binary publication occurred. The protected README and the six preexisting website modifications were preserved.

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

## Real validation results

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

## Changed paths and evidence

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

Linux qualification is **PikaOS/glibc 2.44 x64**, including tmpfs and XFS native operation tests. It does not prove Ubuntu 24.04/glibc-floor execution, clean-machine bootstrap, network filesystem durability, real power-loss recovery, disk-full behavior on a filled physical volume or every provider C-allocator failure. Native syscall fault injection and C++ allocation sweeps cover the stated boundaries, without claiming exhaustive storage/provider failures. There is no new async/deadline process API or portable power-loss transaction contract.

Windows and macOS were not built or executed here. The Linux dependency builder does not provide their qualified closure. Crypto and ZIP return unsupported when their native providers are unavailable; private-directory operations currently return unsupported outside Linux. Process launching uses POSIX code on Unix and returns unsupported on Windows. The wide application startup change needs Windows CRT/native Unicode testing. Windows DACL/reparse/long-path/CreateProcess qualification and macOS native filesystem/provider/process/deployment-floor qualification remain substantive engineering work. Notice-rule simulation is not native Windows evidence.

**REVISE** means retain the implemented Linux foundations and their evidence, while revising any claim that Phase A has established the architecture's full platform acceptance. Golden Wrench/Boss should review the root admission threat model, the deliberately restricted ZIP profile, child lifetime rules and `published/durable` failure contract. Then either approve a specifically bounded Linux next milestone after its declared userspace-floor qualification, or require the remaining native platforms first. Signing-key custody, threshold, bootstrap channel and actual release package policy remain Phase B owner decisions. Work stops here for that review.
