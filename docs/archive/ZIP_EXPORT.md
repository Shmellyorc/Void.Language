# Bounded ZIP application export

## Design review

The writer accepts explicit file names and owned byte arrays from the application. It returns an owned ZIP image that the application can pass to `io.private_directory.save_bytes_atomic`. A game can package a validated UTF-8 configuration, binary checkpoint and schema metadata in one export. Encoding requires no filesystem authority, directory discovery, native resource handle or publication side effect.

| Contract | Design |
| --- | --- |
| Source API | `archive.encode_zip(const files: [archive.zip_input], limits: archive.zip_write_limits = archive.zip_write_limits()) -> result<[u8], system_error.error>`. Each input has a `name: string` and `bytes: [u8]`. |
| ZIP profile | Single-disk classic ZIP with stored regular files (method 0), version needed 1.0, and MS-DOS made-by version 2.0. Emit one local record per file, a matching central directory and a 22-byte EOCD. Zero entries and zero-byte files are valid. |
| Budgets | 0–64 entries, 0–4 MiB per file, 0–16 MiB combined payload, 22 bytes–16 MiB serialized. Callers can lower each independent limit. The 16 MiB serialized ceiling matches the atomic-save byte limit. |
| Names | One flat ASCII file component of 1–255 bytes. Start with an ASCII letter or digit. Subsequent bytes may also include period, underscore and hyphen. Reject trailing periods, DOS device stems and case-insensitive duplicates. Reject non-ASCII names, all separators, spaces, NULs and path aliases. ASCII names are the interoperable UTF-8 subset. Contents are opaque bytes and can contain unrestricted UTF-8 or binary data. |
| Order and metadata | Sort by the original ASCII bytes. Use the fixed DOS timestamp 1980-01-01 00:00:00. All flags, extra fields, comments, disk fields and attributes are zero or absent. Same names and contents yield the same bytes regardless of caller order, host, timezone or input file metadata. |
| CRC and geometry | Compute IEEE CRC32 with the pinned zlib already used by Runtime. Put the same CRC and exact byte length in both headers. Preflight local offsets, central size and total image with checked arithmetic. Reject values requiring ZIP64 before narrowing. Maximum header overhead is 37,526 bytes. Copying and CRC work scale with admitted bytes, plus a sort of at most 64 names. |
| Errors and ownership | Invalid limits return `invalid_range`, unsafe or duplicate names return `invalid_argument`, and exceeded quotas return `limit_exceeded`. An unavailable host provider returns `unsupported`. Native allocation failures use the checked Runtime failure boundary. Snapshot admitted payloads with registered Runtime roots and existing bounded byte-copy facilities. Hold at most the bounded native input snapshots and one serialized image. The returned managed buffer is independent of the inputs. Failure discards temporary memory and publishes no partial archive. |
| Publication | Use the existing G06 admitted-private-directory operation. Check `published` and `synchronized` independently. A user-selected destination outside that authority requires a separate publication contract. |
| Implementation | Use one private operation through the established host ABI. Native code can use the existing bounded byte snapshot and pinned bulk CRC implementation. VOID source has no efficient checked bulk assembly and CRC primitive for this use case. No public compiler syntax, dependency, new filesystem operation or background job is required. |

Record layouts follow the [PKWARE APPNOTE §§4.3.1, 4.3.7, 4.3.12, 4.3.16 and 4.4.3](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).

### Reader compatibility established before implementation

The baseline reader admits restricted stored and deflate ZIP/ZIP64 from a held regular file. It accepts bounded ZIP64 records, approved timestamp and UID/GID extras, and checked data descriptors. It rejects encryption, other compression methods, outer filters, multiple disks, unsupported entry types, unsafe paths, case aliases, duplicate paths and regular-file ancestors. It admits printable ASCII relative names, with 255 bytes per component and 4096 bytes per path. Combined central and local metadata is capped at 16 MiB. The hard ceilings are 16 GiB archive input, 100,000 entries, 64 GiB expanded data and 1 MiB per read. Defaults are 1 GiB, 10,000 entries, 4 GiB and 64 KiB respectively. Drain each entry to EOF to check integrity. Close releases the file borrow without asserting integrity for unread data.

At baseline `79f212f2907b3fb5eeeabebb0aa8fa5705b82c86`, independent hand-framed fixtures used Python `struct.pack` and `binascii.crc32` for local and central records and EOCD. Python `zipfile.testzip()` verified all four. The existing VOID reader accepted the stored payload `00 80 ff`, a zero-byte file and a 22-byte empty archive, and released its input in each case. It refused the UTF-8-flagged name `雪.txt`. The writer therefore stays within the reader's ASCII policy. The baseline command was:

```sh
build/llvm23-release/voidc run tests/host_capabilities/smoke.void --no-cache -- \
  zip-many "$proof/stored.zip" "$proof/zero-file.zip" "$proof/empty.zip" "$proof/unicode-name.zip"
```

Writer scope excludes compression, ZIP64, encryption, filesystem trees, links, executable attributes, archive signing and background work.

## Game-save export

[Run the example](../../examples/archive/save_export.void) with a UTF-8 JSON
object containing a nonempty `player` and integer `volume` from 0 through 100,
plus a checkpoint of up to 4 MiB. Pass a new absolute directory under a parent
eligible for G06 admission. The command accepts explicit input paths. It reads
no additional files or directory contents.

```sh
voidc run examples/archive/save_export.void --no-cache -- \
  /absolute/input/game.json /absolute/input/checkpoint.bin /absolute/private/export
```

The example validates the configuration with `json.parse_as` under local
limits and encodes `checkpoint.bin`, `game.json` and `manifest.json`. It keeps
the original JSON text and binary data, including their whitespace and NUL
bytes. The generated manifest records the export version and payload names.
The example publishes `save-export.zip` with replacement disabled, reopens the
held file and drains every entry to verified EOF. It checks the read-back
against the original bytes, which preserves the validated JSON semantics.

A prepublication failure leaves the destination file intact. A postpublication
failure reports that the archive was installed, verifies the installed content
and returns a failure status. Native synchronization and cleanup outcomes remain
visible. The private-directory, filesystem and platform restrictions are those
of [G06](../io/CONTRACT.md#atomic-application-saves). The writer itself retains
no filesystem state. The macOS export/publication lane has no qualification in
this increment.

## Maintained coverage and replay

| Control | Maintained evidence |
| --- | --- |
| Independent format and CRC | `archive_export` uses Python `zipfile.testzip()` and independent `struct.unpack_from`/CRC32 checks over every local header, central record, EOCD, offset and payload. It verifies empty archives, one and multiple files, zero-byte files, fixed metadata, sorting, binary NULs and exact UTF-8 payloads. It compares repeated encodes, changed caller order and all execution modes. |
| Names and budgets | The same suite checks unsafe paths, reserved stems, byte NULs, invalid UTF-8 decode, the non-ASCII name restriction, exact and case-folded duplicates, 255/256-byte names, 64/65 members, per-file/aggregate/output refusals and exact 4 MiB file and 16 MiB image admission. `archive_export_geometry` checks ZIP32 sentinel/offset arithmetic with synthetic counts and no large allocation. |
| Existing reader | The game example drains all entries and verifies bytes. The source fixture checks read-back of an empty archive, release of the input file, and rejection of corrupt CRC, truncated EOCD, a bad local offset, a disallowed method and a Unicode filename accepted by Python. Existing deflate, ZIP64 and data-descriptor controls remain registered. |
| Save and ownership | The game workflow verifies exact JSON bytes and decoded fields, metadata and checkpoint bytes. Invalid JSON leaves the destination absent. The harness inspects the private directory for any leftover stage. Source tests mutate input and output buffers independently and verify the original archive bytes remained owned. |
| Native execution | The example and controls run uncached JIT and separately emitted, independently linked AOT at O0 and O2. The harness verifies the actual Clang/LLD 23.1.4 tools and checks the Windows link driver's selected `lld-link`. |

[Source API](../../stdlib/archive.void) · [Native writer](../../src/runtime/host_archive.cpp) · [Format oracle](../../tests/archive/export.py) · [Source controls](../../tests/archive/export.void) · [Geometry controls](../../tests/archive/geometry.cpp)

Use the existing pinned build for the intended profile and a task-owned,
admitted temporary location. On Linux the location must have no group- or
other-writable non-sticky ancestors. On Windows use the native x64 developer
shell and a verified private NTFS scratch root.

```sh
cmake --build <build> --target voidc void_archive_geometry
ctest --test-dir <build> --no-tests=error --output-on-failure -R '^archive_export(_geometry)?$'
```

The new tests have 1200 s and 30 s CTest timeouts, respectively. Individual
new Python child invocations have a 180 s timeout. No existing timeout or
assertion was changed. The full focused qualification selection adds
`host_capabilities_portable`, `host_capabilities_portable_source`,
`io_atomic_save_native`, `io_atomic_save_source`, `io_kernel`,
`io_execution`, `io_ownership`, `gc_buffers`, `runtime_coordination`,
`memory_execution`, `json_configuration`, and
`text_encoding_text`, `text_encoding_errors`, `text_encoding_interop`.
Linux also executes `host_capabilities` and `host_capabilities_native`.
Use an exact anchored alternation for these names. Qualification checked every
selected command and its timeout with CTest `--show-only=json-v1`. All were
active and had no skip properties. For example, with the profile's exact
alternation assigned to `selection`:

```sh
ctest --test-dir "$build" -R "^($selection)$" --show-only=json-v1
ctest --test-dir "$build" -R "^($selection)$" --parallel 2 --output-on-failure --no-tests=error
```

Strict Linux uses `VOID_SANITIZERS=ON` in RelWithDebInfo with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

## Qualification record

Source baseline `79f212f2907b3fb5eeeabebb0aa8fa5705b82c86` plus the G12
worktree changes. The Windows VM received the same 16 build/test/example
inputs as the Linux checkout. SHA-256 for the sorted per-file digest list
(`<sha256><two spaces><repository path><LF>`) is
`a66952d46e50cb7839350464d81a36185a348ca954cc0ad21420bef7154395b7`.
The Windows launcher verified every file before building both profiles.
The native profiles used LLVM/Clang/LLD 23.1.4, the existing verified host
providers and pinned zlib 1.3.2. PikaOS 4 reported x86_64 kernel
`7.2.11-pikaos`. The Windows guest reported Windows 10 Home x64
`10.0.19045.6466`, SDK 10.0.19041.0 and MSVC toolset 14.44.35207.

### Recorded development and runner failures

- Early source checks exposed collisions with the imported `io.entries` and
  `io.metadata` names. The writer parameter became `files` and the example
  uses `description`. The new fixture was corrected to use the existing VOID
  list, enum-pattern and line-continuation rules. The first full fixture run
  hit `many_names` because the inclusive test range constructed a 256-byte
  name for the 255-byte success case. The revised bounds exercise exactly
  255/256 bytes and 64/65 entries.
- The first Linux Debug build of the existing native host fixture found one
  reference to the previous private ABI field name `ReplyLayout::strings`.
  It now uses `ReplyLayout::element`, with the existing checks intact.
- PikaOS Release first reported 13/18. The qualification launcher's
  mode-0775 scratch parent was correctly rejected by five existing tests
  using G06 or private-directory admission. The same selected tests
  passed 5/5 after the task-owned parent was corrected to mode 0700.
- The first Windows Debug `host_capabilities_portable` run printed all
  151 native checks passed, then Python failed deleting
  `owner-rights/child.exe` with WinError 5. The file was owned by the
  test account, had attribute 0x20 and inherited full OWNER RIGHTS.
  Delayed removal succeeded under the same ACL. The unchanged registered
  command passed on a fresh rerun. The process responsible for the temporary
  deletion refusal was not established.

The initial game archive also passed an independent Info-ZIP `unzip -t` check
for all three entries.

### Native results

| Profile | Focused CTests | `archive_export` | Execution lanes |
| --- | --- | --- | --- |
| PikaOS 4 Debug | 18/18 in 648.97 s | PASS, 139.86 s | O0/O2 JIT and independent AOT |
| PikaOS 4 Release | 13/18 initial in 135.28 s, then 5/5 in 150.65 s with the corrected scratch parent | PASS, 44.19 s | O0/O2 JIT and independent AOT |
| PikaOS 4 strict ASan/UBSan with leak detection | 18/18 in 646.18 s | PASS, 132.08 s | O0/O2 JIT and independent AOT |
| Windows 10 Home Debug | 14/16 initial in 868.08 s, 15/16 after the portable-provider rerun | PASS, 231.04 s | O0/O2 JIT and independent AOT |
| Windows 10 Home Release | 15/16 in 243.82 s | PASS, 65.28 s | O0/O2 JIT and independent AOT |

`archive_export_geometry` passed in all five profiles. The strict Linux
run reported no AddressSanitizer, LeakSanitizer or UndefinedBehaviorSanitizer
findings. The Linux reader regression reported 54 processes and 28 ZIP cases
per normal/O2 JIT/AOT in each profile.

Windows Debug and Release reproduced the existing `io_execution` check 91 failure
while creating a file symlink. The native atomic-save control reported
`ERROR_PRIVILEGE_NOT_HELD` (1314), passed its existing junction rejection and
reported directory synchronization completed with code 0. `io_kernel` and
`io_atomic_save_source` passed. The prior G06 file-symlink limitation still
applies. [G06 qualification](../roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g06-atomic-application-saves)

All five profiles and all their execution lanes produced identical
control and game ZIPs:

| Image | SHA-256 |
| --- | --- |
| Multi-file control | `fd8f5da0daab558efc474c45c9583c69fd9797da3525af1c0b45b601cf27ff14` |
| Game export | `dccb3d59003492d7e5b71c1e3e50b369ffc3fa3ee090d6b4b33cb7e476eaa3b7` |

### Review inventory

| Area | Paths |
| --- | --- |
| Public API and executable example | `stdlib/archive.void`, `examples/archive/save_export.void` |
| Private host ABI | `include/void/host_ops.hpp`, `include/void/host_abi.hpp`, `src/frontend/host.inc`, `src/ir/host.inc`, `src/backend/llvm/host.inc`, `src/runtime/host_abi.cpp`, `src/runtime/host_internal.hpp` |
| Writer and checked sizes | `src/runtime/host_archive.cpp`, `src/runtime/zip_geometry.hpp` |
| Test registration and fixtures | `CMakeLists.txt`, `tests/archive/export.py`, `tests/archive/export.void`, `tests/archive/geometry.cpp`, `tests/host_capabilities/native.cpp` |
| API, evidence and milestone records | `docs/archive/ZIP_EXPORT.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `docs/roadmap/MILESTONE_REGISTER.md`, `docs/voidup/REPORT.md`, `stdlib/README.md` |

The milestone register records G11 integrated at the baseline and G12
**IN PROGRESS** pending independent Golden Wrench review. The G08 optimization
and Windows native-cache holds, G06 symlink privilege limitation, G10 general
deadline/queue deferrals, G11 retained raylib callback HOLD, unsupported macOS
managed-native/D5 scope and deferred seven-job gate remain in their existing
records. [Milestone register](../roadmap/MILESTONE_REGISTER.md)

### Cleanup and handoff

The task-owned Linux scratch, Windows build/test root and VM transfer share
were removed after qualification. Measured free space went from
16,380,977,152 to 16,586,932,224 bytes on Linux tmpfs and from
12,735,795,200 to 14,995,148,800 bytes on the Windows system volume.
The existing Linux build configurations and verified toolchains and providers
were retained. Evidence is recorded here and in the maintained fixtures.
The Git worktree contains 15 modified tracked files and six untracked files,
all text or source. Documentation links, Python syntax and `git diff --check`
passed. The staged diff is empty and HEAD remains the baseline. Hosted CI was not
dispatched.
