# CI #92: Unicode diagnostics and directory reservation

21 October 2026. Baseline `051f463ea35b2b738ce282cb2e93e5e60cdb1c2c`.
**Focused repair implemented; native Windows observation/acceptance pending.**
No link/emission/copy/publication changes. The sole production change adds
`FILE_LIST_DIRECTORY` to the existing directory-handle access mask.

## Observed native evidence and its limits

[Boss-run CI #92](https://github.com/Shmellyorc/Void.Language/actions/runs/39717364089),
Windows Release job `119622595777`, passed **158/160**. The
[retrieved failure excerpts](CI92_NATIVE_EVIDENCE.json) establish:

- The native helper's exact `require(!MoveFileExW(...))` failed. Therefore the API
  returned nonzero success. The old test did **not** capture its raw BOOL, whether
  source/destination names changed, or whether handles retained directory identity.
  Those observations cannot honestly be reconstructed from this log.
- The long-path test reached its diagnostic after same-file proof, launch and
  expected exit status **7**, then cp1252 printing failed on U+1F3AE. This is native
  evidence that this genuine extended-application launch succeeded, followed by a
  diagnostic encoding failure. It does not complete either Windows suite.

No final Windows Debug total is claimed from the incomplete log available during
inspection. No new hosted execution was started.

## Cause and smallest security correction

The old pin opened directories with `FILE_READ_ATTRIBUTES | READ_CONTROL` only.
Those metadata permissions do not request data read/write/delete access. Omitting
FILE_SHARE_DELETE from a metadata-only handle is insufficient evidence that the
handle participates in rename-sharing exclusion. Microsoft's
[sharing API](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iochecklinkshareaccess)
explicitly distinguishes requests lacking read/write/delete access; its FORCE_CHECK
flag exists to check those requests too. Directory listing shares the read-data
access bit ([access constants](https://learn.microsoft.com/en-us/windows/win32/fileio/file-access-rights-constants)).

`pin_directories()` now also requests **FILE_LIST_DIRECTORY**, retaining its
existing read/write sharing, omission of delete sharing, OPEN_REPARSE_POINT,
BACKUP_SEMANTICS, DACL checks, ownership and RAII. This supplies read access for
the sharing reservation instead of assuming metadata inspection pins a name.
Failure to acquire that access fails closed. The working short-directory native
link workflow and final publication are otherwise unchanged. Native confirmation
of the corrected sharing behavior is still required.

## Tests retain the assertion and collect missing facts

The native helper now contains two bounded controls inside its existing owned
fixture, with no new temporary-directory system:

1. Open an exclusively created child twice with the **old metadata-only mask**.
   Attempt its rename. Log raw BOOL, failure code only on failure, source and
   destination existence, each name's match to the original volume/file ID and
   whether **every live handle** still identifies its original directory. If the
   move succeeds, require real source disappearance and matching destination
   identity, then restore it. After closing handles, a positive rename/restore
   control must succeed, ruling out permissions or an invalid rename operation.
2. Attempt the same operation against production **directory-read pins**. Keep
   the original `directory reservation can be renamed` rejection assertion, and
   additionally require **ERROR_SHARING_VIOLATION (32)**, unchanged source identity,
   absent destination and stable identities for all live handles. On an unexpected
   real move, track/restore only the identity-verified owned directory before
   failing, so the fixture's cleanup still owns its artifacts.

`rename_probe=metadata_only` and `rename_probe=list_directory` lines contain the
observations, not predicted outcomes. Workflow forwards successful helper stdout
so the observations remain in test evidence; failing helper output already appears
in the assertion. New native probe output has **not been obtained locally**.
No claim that the original directory genuinely moved or that its handles remained
valid is made before this probe supplies those facts.

Both Python diagnostics now print **ascii(application)**, preserving the successful
explicit executable launch, same-file, >260 UTF-16-unit and exit-code checks.
The diagnostic cannot depend on the Windows console's Unicode codec. All previous
Python assertions, six-level fixtures and native security/cleanup tests remain.

## Verification and Wrench gate

[Local evidence/hashes](CI92_EVIDENCE.json); logs in `build/ci92-test-repair/`.
Linux Debug **3/3** and strict ASan/UBSan/leaks **3/3** passed for Stage 9 workflow,
CI #88 paths and CI #89 genuine long paths. Four checks executed the actual changed
print expressions through strict ASCII/cp1252 streams, retaining escaped emoji.
Windows-target Clang 19.1.7 syntax checking passed for source and helper using the
existing extracted MinGW headers and temporary LLVM configuration. None of these
executes Windows sharing semantics.

Wrench should review the one-bit production change and both probe branches.
Boss's next native verification must preserve the raw `rename_probe` lines and
establish actual movement plus handle identity for the old mask, then error 32
and stable paths/handles for the repaired pins. Run the full existing native helper,
both original deep-path suites and Windows Debug/Release 160/160. If the repaired
pin still permits movement, do not waive the assertion or mark this resolved.
CI #90/#91/#92 native acceptance remains pending. No commits, pushes, branches,
CI dispatch or root README changes were performed.
