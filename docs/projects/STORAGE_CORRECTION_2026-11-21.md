# Project storage correction — 2026-11-21

The Boss's later instruction narrowed retention: delete unnecessary test leftovers
and redundant evidence; Markdown with concrete results is the primary record.
No source changes were staged, committed or pushed. Nothing was installed or
published. The workspace already contained 41 changed/untracked paths; their
existing work was preserved.

## Root cause and corrected behavior

The installed compiler hard-coded `/tmp/void-native-approval-XXXXXX` for approved
native payloads. `/tmp` is a quota-enabled tmpfs. A write of exactly 2,100,056 bytes
(the size of TestGame's `libraylib.so.600`) failed with **errno 122, EDQUOT: Disk
quota exceeded**. It was the account's quota, rather than exhaustion of all tmpfs
space or inodes. `std::ofstream` failure was reported only as
`Cannot stage approved native artifact: libraylib.so.600`.

The source now stages approved native invocation files under
`<project>/.void/run/native-<random>/`, with an ownership inventory and a live lease.
Linux uses owned, unaliased `0700` directories and exclusively created `0600`
files through pinned directory descriptors. Saved approval identities and artifact
hash checks remain required. Runtime loading recaptures and hashes the approved
libraries, creates sealed anonymous kernel snapshots, validates managed dependency
order, then loads those snapshots. License notices are checked without execution.
A test replaced the staged libraries at the first loader call: execution still
used the approved sealed bytes and passed.

Manifest `run` selects the project root as application cwd, even when the caller
uses `--project` from elsewhere. TestGame loaded `assets/logo.png` from its real
project directory. Internal storage does not change relative application paths.
The default manifest AOT executable is now `.void/build/<entry>`; explicit `-o`
keeps its caller-relative meaning. The existing private `.void/cache/v1` cache
remains project-local.

Normal completion, application failure and recoverable staging-write failure
remove the invocation's native stage. Bounded subsequent-run recovery recognizes
only exact ownership records and inactive leases. Actual SIGKILL recovery passed.
Active leases, unmarked directories, symlinks and unknown user files are preserved.
The compiler never sweeps `.void/` or `/tmp`. See [the ownership and retention
policy](../PROJECT_STORAGE.md) for the precise namespace and platform rules.

Windows native staging was changed to private project storage with protected
DACLs, pinned ancestors and a no-sharing lease. Windows was not executed on this
Linux host. Existing Windows AOT short-path staging remains bounded and scoped to
its documented linker path-length constraint.

## Cleanup and verified migration

| Measurement | Before | After cleanup |
| --- | ---: | ---: |
| `/tmp` bytes used | 13,421,346,816 | 315,805,696 |
| `/tmp` GiB used | 12.500 | 0.294 |
| `/tmp` utilization | 79.99% | 1.88% |

At least **13,096,067,072 bytes
(12.20 GiB)** of inventoried allocations were removed.
Small concurrent changes in unrelated application usage can affect `df` totals.

**312 reviewed inactive VOID entries were removed from `/tmp`:** 299 identified
engineering entries, four additional native/project fixture directories, six
original Clang crash-reproducer files and three generated scratch files. Every
entry belonged to this account. Activity checks covered process cwd/executables,
arguments, open descriptors and memory maps. No confirmed VOID cleanup exception
remains. Unattributed random files, other applications' data, Codex previews,
active language servers and system resources were excluded.

Copied toolchains, abandoned builds, duplicate worktree snapshots, successful raw
output, package-transfer fixtures and redundant bundles were deleted. Retained
Markdown and needed original failure records were content-deduplicated and copied
only once. SHA-256 was checked at each destination before source removal, and
**all 3,740 unique retained destinations were checked again afterward**.
No replacement ZIP bundle was created. Existing archive bundles were not changed.

The verified destination is:

`/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/`

- [Complete per-entry inventory, sizes, owners and destinations](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md).
- `retained/<original-investigation>/...`: unique Markdown and original failure
  records. Identical records may share an earlier verified destination.
- `original-clang-failures/`: six original `.cpp`/`.sh` Clang crash reproducer
  files, migrated unchanged with exact original names.
- `cleanup-receipts.jsonl`: exact retained original-to-destination mapping and
  SHA-256. `inventory-before.json` records sizes, ownership, contents and activity.
- `preservation-verification.json`: final independent destination hash check.

Historical failure bytes were not rewritten. Verified Markdown links were
updated, and 18 current reports now explicitly identify their old `/tmp` paths as
historical and point to the migration inventory. Previously discarded generated
binaries are not represented as available evidence.

## Focused qualification

All compiler/LSP qualification used the original **1,048,576-byte hard stack**.
No full-suite marathon or new toolchain installation was performed.

| Check | Result |
| --- | --- |
| Debug and Release actual TestGame `run .`, real raylib window | PASS; asset loaded, project cwd observed, sealed mappings observed, private `.void/run` stage verified, normal cleanup verified |
| Debug, Release and fatal-sanitizer headless native JIT | PASS |
| Actual TestGame LSP, Debug/Release/fatal sanitizer | PASS; clean initialization diagnostics |
| Release managed-native regression | PASS; deterministic locks, JIT/AOT relocation, dependent native images and pre-load tamper refusal |
| Release LSP project protocol | PASS; 285 checks |
| Selection from unrelated caller cwd | PASS |
| Marked orphan reclamation, active lease and user-file preservation | PASS |
| Symlink staging refusal | PASS; target untouched |
| Recoverable EFBIG during payload write | PASS; stage cleaned |
| Private `.void/build` default executable | PASS |
| Staged-file replacement between verification and loading | PASS; sealed approved snapshots executed |
| Real SIGKILL followed by next-run cleanup | PASS |
| Application exit 29 after native load | PASS; stage cleaned |
| Debug, Release, strict compiler/LSP builds; Python syntax; diff whitespace | PASS |

**Strict graphical TestGame remains a failure:** it renders, loads the logo and
closes normally, then fatal LeakSanitizer reports **256 bytes in one allocation**.
This matches the previously recorded native raylib/graphics-driver failure and
its independent plain-C control in
[G77's strict native-device record](G77_FALLIBLE_RAYLIB_LOADERS.md#strict-native-device-failures-remain-failures).
No leak suppression, disabled leak detection or enlarged stack was used. Headless
fatal ASan/UBSan/leak checks pass. Windows native execution remains unqualified.

The original staging failure, exact EDQUOT probe, original strict graphics failure
and the two corrected implementation investigation failures remain in the archive.
The latter caught notice files being considered library candidates and an LLVM
random-name helper returning an absolute temporary path. Both were corrected;
final Linux checks verify actual project-local paths and notice-only verification.

## Source and workflow changes

- `src/main.cpp`: project cwd lifetime and default `.void/build` output.
- `src/native_approval.cpp`, `src/native_approval.hpp`: private project storage,
  secure creation, per-run inventory/leases, bounded abandonment cleanup, explicit
  write errors, private default build-directory admission and Windows lifetime
  ordering.
- `src/runtime/c_dynamic.cpp`: descriptor capture, runtime digest checks, sealed
  conventional-library snapshots and dependency ordering; public tamper diagnostics
  preserved.
- `.gitignore`: generated `.void/run/` and `.void/build/` namespaces.
- `tools/engineering_storage.py`: shared archive destination policy and owned
  disposable scratch. `tools/test_output.py` rejects retained `/tmp` diagnostics.
- `tools/build_voidup.py`, `tools/package_voidup_fixtures.py`,
  `tests/voidup/support.py`, `tests/voidup/evidence.py`,
  `tests/voidup/windows_cross.py`: retained qualification output uses the archive.
- `tests/voidup/acceptance.py`, `interruption.py`, `rejection.py`, `matrix.py`,
  `lifetime.py`, `recovery.py`, `profile_install.py`, `timeout.py`, and
  `tests/stage11/vscode.py`: disposable fixture work has process-owned cleanup.
  Caller-supplied acceptance `--reuse-work` remains explicitly caller-owned.
- `tests/text_encoding/crash.py`: real original crash evidence defaults to the
  permanent archive, independently of scratch cleanup.
- `tests/project/raylib_workflow.py`: existing workflow covers project storage,
  cwd, orphan cleanup and user-file preservation; default output path updated.
  `managed_native.py` observes project cwd for JIT constructors and keeps standalone
  executable cwd expectations. `simple_packages.py` uses the new default path.
- `docs/PROJECT_STORAGE.md`, `docs/README.md`, `docs/DEVELOPMENT_STRATEGY.md`,
  `docs/FIRST_PROGRAM.md`, `docs/wiki/Native-compilation.md`, wiki publication
  hashes, and the linked historical investigation reports: storage ownership,
  retention, output paths and migration handoffs.

The inventory/migration script and focused investigation harness are in this
archive rather than new permanent general-purpose test tools. Their successful
scratch directories were removed. No copied worktree, SDK, or installer was
retained for this correction.

## Installation boundary

The installed `/home/shmellyorc/.local/bin/voidc` has **not** been replaced. Cleanup
relieves its immediate EDQUOT failure, but that installed version retains its old
native-staging implementation. Corrected behavior is built and verified locally
in `build/source-debug`, `build/source-release` and `build/llvm23-strict`.
Installing a selected build requires separate Boss authorization, as requested.


## Focused P2 reliability correction — corrected candidate

Golden Wrench required two changes after the initial qualification: cleanup when
exclusive directory creation succeeds but opening that directory fails, and OS
error preservation for directory access and Windows payload writes. This section
supersedes the initial report's constructor-failure coverage; the earlier happy
path, native mapping, cwd, retention and graphics limitations remain unchanged.

### Original failure and correction

The new process regression was run against the original Release compiler
(SHA-256 `da74eeeb2ca1605351c87b4fd181f7245799a678d7924ac7a284935cc416b3ac`).
Injecting `EMFILE` at the open immediately after `mkdirat` reproduced:

```text
Cannot pin private native run storage.
('native-12712e1de5a37859', [])
```

The native leaf was left empty and unmarked. The exact failure is retained in
`p2-reliability/original-regression-failure.log`; the original report is preserved
as `p2-reliability/original-storage-report.md`. An initial regression-harness
`LD_PRELOAD` path-with-spaces error is retained separately, rather than relabeled
as the compiler failure. The harness now inherits an open DSO descriptor and uses
`/proc/self/fd/<fd>`; no second copy or extra scratch directory is needed.

The creating invocation now records successful exclusive creation separately
from its stage handle, then captures device/inode, owner and privacy information.
Constructor cleanup compares the recorded identity with the named directory even
when the directory open returned `-1`. It removes only its own exclusively created
ownership record, and removes the directory nonrecursively with `AT_REMOVEDIR`.
An inode replacement or unknown file prevents deletion. No ownership is inferred
from a `native-*` name alone. Existing valid-marker abandoned-run recovery is
unchanged.

A hard interruption before a complete marker remains deliberately conservative:
a later process preserves the unmarked or partial-record directory for manual
review. A failure of the initial identity capture is likewise reported without
claiming deletion authority. This is distinct from a recoverable constructor
failure, where the creating process retains its recorded identity and can clean
its verified empty directory. The `SIGKILL` regression checks that a partial marker
and a separate unmarked user directory both survive a later healthy invocation.
This candidate does not claim automatic deletion of ambiguous pre-marker entries.

Directory-open and metadata failures now retain `errno` and operation context;
owned-but-inaccessible storage reports **Permission denied**, while a deliberate
privacy rejection remains a policy diagnostic. The regression checks **Too many
open files** (`EMFILE`), **Input/output error** (`EIO`), **Permission denied**
(`EACCES`) and **Disk quota exceeded** (`EDQUOT`). Ownership-record writes retry
`EINTR`, complete short writes and use `EIO` for zero progress rather than reporting
stale error state.

Windows payload staging now uses exclusive `CreateFileW`, checked `WriteFile`
progress, `FlushFileBuffers` and `CloseHandle`. Errors retain the artifact path,
numeric Windows error and `std::system_category()` description. Zero-progress
writes produce `ERROR_WRITE_FAULT`, not a stale `GetLastError`. Private DACLs,
ancestor pins, leases, approval identities and the loading contract are unchanged.
Native Windows execution could not be qualified on this Linux host; no Windows
SDK or duplicate toolchain was installed.

### Focused verification

The existing `project_managed_native` fixture now invokes the focused staging
regression on Linux. It reuses its already-built tiny provider, adapter, catalog
and disposable project. It adds no CMake target or separate permanent fixture.

| Check | Debug | Release | Fatal ASan/UBSan/leaks |
| --- | --- | --- | --- |
| Compiler/LSP build in existing build directory | PASS | PASS | PASS |
| Exact post-mkdir `EMFILE` regression; no orphan after failure or next run | PASS | PASS | PASS |
| Metadata `EIO`, marker-open `EACCES`, marker-write `EDQUOT` | PASS | PASS | PASS |
| Payload-open permission/quota diagnostics and cleanup | PASS | PASS | PASS |
| Inaccessible directory vs deliberate privacy rejection | PASS | PASS | PASS |
| Ownership-record short write and `EINTR` | PASS | PASS | PASS |
| Substituted directory and user sentinel preservation | PASS | PASS | PASS |
| Pre-marker `SIGKILL`: partial record and unmarked entry preserved | PASS | PASS | PASS |
| Existing managed-native JIT/AOT, relocation, locks, tamper refusal and LSP | PASS | PASS | PASS |

All three process qualifications used the original **1,048,576-byte hard stack**.
Fatal settings remained enabled; no leak suppression, disabled sanitizer or stack
increase was used. Python syntax, strict C fault-shim syntax and `git diff --check`
also passed. The real graphics workload was not repeated: its independently
confirmed behavior and existing 256-byte graphics-driver leak were not reopened.
The cwd implementation and sealed native runtime source hashes are unchanged.

The correction uses only `build/source-debug`, `build/source-release` and
`build/llvm23-strict`. No copied worktree, toolchain, evidence bundle or disposable
build tree was created. Test fixture scratch was automatically removed. Retained
P2 logs and exact identities live beneath the existing archive:

`/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/p2-reliability/`

### Exact changed-file inventory for these two corrections

| File | Change |
| --- | --- |
| `src/native_approval.cpp` | Separate exclusive-creation identity from handle state; safe empty-leaf initialization cleanup; syscall-vs-policy diagnostics; complete marker writes; checked Windows payload I/O. |
| `tests/project/native_staging_fault.c` | Scoped Linux open/inspect/write faults, replacement and pre-marker kill probes. |
| `tests/project/native_staging.py` | Focused assertions against the existing native fixture with a 1 MiB hard stack. |
| `tests/project/managed_native.py` | Invoke that regression on Linux; reuse the existing provider and scratch lifecycle. |
| `docs/PROJECT_STORAGE.md` | Constructor cleanup authority, ambiguous pre-marker interruption policy and OS diagnostics. |
| `docs/projects/STORAGE_CORRECTION_2026-11-21.md` | Original failure, corrected behavior, qualification, exact inventory and re-review pointers. |

The full SHA-256 values for these six files, current compiler/LSP binaries and
compiler build identities are in
[p2-reliability/candidate-identities.json](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/p2-reliability/candidate-identities.json).
The original root-level `source-checksums.json` and `final-verification-summary.json`
remain historical records of the pre-P2 candidate; they have not been overwritten.
The prior 3,740 retained destinations are unchanged.

The installed compiler remains unchanged. Nothing was staged, committed, pushed,
installed or published. This is the corrected candidate for focused Golden Wrench
re-review, not a claim of independent approval or Windows native qualification.
