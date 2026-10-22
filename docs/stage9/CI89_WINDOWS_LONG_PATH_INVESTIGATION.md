# CI #89 — Windows long-path investigation and manual probe

> **Current Windows status: RESOLVED by CI #95**, at
> `e25816bdb80e3f32613edf77c9cf3258232af966`: Debug 160/160 and Release 160/160.
> [Permanent engineering record and verified evidence](WINDOWS_ENGINEERING_RECORD.md)
> supersede the earlier pending Windows acceptance statements below. Original
> failures, local results and evidence remain historical; coverage limits and
> independent Boss/Wrench review are recorded in the current reference.

## Original engineering record (historical)

> Historical preparation record. Boss subsequently ran native diagnosis #1;
> error 3 and the manifest correction are now confirmed. See the
> [production repair and current gate](CI89_WINDOWS_MANIFEST_REPAIR.md).


21 October 2026. Baseline `db4c979ecb9516ddf9544203c3e20c8bac10b5d9`.
**Native evidence pending. No compiler path-handling repair is implemented.**
Boss's follow-up authorizes preparation of one manual Windows-only diagnostic job;
Boss will review/push/authorize execution. Do not infer a demonstrated correction
from the presence of the experimental manifest.

## Historical evidence

[CI #89](https://github.com/Shmellyorc/Void.Language/actions/runs/39691943404)
Windows Release job `119539993932` and Debug job `119539994779` each passed 158/159.
`native_output_paths_ci88` passed in both. The sole failure is
`application_workflow_stage9`, original six-level source fixture at line 153,
`VOID3002: Cannot resolve the project root directory.` Both complete decoded job
logs are retained as gzip files under [ci89/](ci89/LOGS.json), with compressed and
uncompressed SHA-256 digests. UTF-8 bytes, BOM and CRLF from the connector's decoded
response are preserved. Existing CI #88 documents, logs and repair remain intact.

The old diagnostic discards `ec.value()`, `ec.category()` and `ec.message()`.
Consequently **the actual hosted Windows numeric error and message are unknown**;
the historical logs cannot establish error 206 or any other number. The diagnostic
now retains those fields without changing resolution, VFS identity or admission.

## Established boundaries and remaining uncertainty

The recorded runner uses Windows Server 2022 and MSVC 14.44 headers/runtime with
Clang 19.1.7. [MSVC STL 17.14 canonical implementation](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/inc/filesystem#L2969)
opens the physical path, then calls `GetFinalPathNameByHandleW`; its
[ABI implementation](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/src/filesystem.cpp)
passes `CreateFileW` failures through `GetLastError`. It is not safe to attribute
the hidden code to one of those calls without a native observation.

[Windows long-path documentation](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
describes two distinct mechanisms: extended-length absolute UTF-16 paths, and
process `longPathAware` together with the machine's `LongPathsEnabled` policy.
The current production targets do not supply a `longPathAware` manifest. This
makes awareness a hypothesis, not a measured diagnosis. No registry setting is
changed by the probes. No extended prefix or blanket normalization is added to
compiler inputs, source snapshots, imports or native outputs.

A disposable local Wine Win32 control used a real 298-UTF-16-unit directory.
Both ordinary and extended `CreateFileW` returned **0, `Success.`**. This does
not reproduce CI #89 and cannot establish Windows's error or validate a repair.
Wine is not accepted as native Windows evidence. Its freestanding investigation
source/executable/log remain in `build/ci89-investigation/`.

## Minimal owner-controlled native job

[windows-path-probe.yml](../../.github/workflows/windows-path-probe.yml) has only
`workflow_dispatch`, one `windows-2022` job, read-only repository permission and
no matrix. It reuses the existing MSVC SDK setup, Python 3.12 and official LLVM
19.1.7 SDK installer/cache. It configures the existing project and builds only:

- `void_windows_path_probe`: existing default process manifest.
- `void_windows_path_probe_aware`: identical source and runtime, with only the
  experimental `longPathAware` manifest supplied as a CMake manifest source.
  [CMake handles embedding/merging](https://github.com/Kitware/CMake/blob/v3.31.6/Source/cmcmd.cxx#L2391)
  and tracks the manifest as a link dependency.

Both targets are excluded from the normal all-target build. Neither target links
VOID libraries or inherits `void_options`. The aware manifest
is confined to its diagnostic target. No `voidc`, runtime, editor, CTest suites or
native application is built/run by this workflow. Normal CI triggers are unchanged.

The Python runner creates the original six components, including the same lengths,
Unicode, spaces, `#`, `%`, a `game # % é` directory and `main.void`; it asserts the
physical directory itself exceeds 260 characters. It runs **both** binaries on a
short control and the same deep directory. Every result records:

- `absolute`, `canonical`, direct `CreateFileW`, and, when open succeeds,
  `GetFinalPathNameByHandleW`: numeric code, category and system message.
- Extended-length `canonical` as an experimental control, with the actual spelling
  retained and only the required separator/prefix syntax changed.
- Machine long-path policy, UTF-16 directory length, process exit status,
  complete stdout and stderr.

`results.json`, both executables, CMake cache and configuration log are uploaded
with `if: always()`. A baseline canonical failure is an observation, not a failed
probe process. A green diagnostic job does **not** mean the compiler is repaired.
Process/control failures are recorded for both binaries before failing the runner.
No expected error code or successful-awareness result is hard-coded.

## Coverage prepared and local verification

The original workflow fixture retains all six directories and now explicitly
asserts its source path exceeds 260 characters. Added `long_physical_paths_ci89`
uses real deep paths for `check`, `run`, `emit` and `build`, directory discovery,
imports, wrong-case entries/imports, exact and case-sensitive output names,
source/import/hard-link collisions, conflicting hard-link module identities,
source/output symlinks, Windows output junctions and existing-output preservation
after analysis failures. It checks cleanup of same-parent staging artifacts.
The complete CI #88 suite and every original assertion are retained.

See [CI89_EVIDENCE.json](CI89_EVIDENCE.json) for focused local results and hashes.
Windows probe compilation/execution and the Windows-only security branches remain
unexecuted locally. Linux success does not establish Windows or macOS success.

## Next authorized step

Stop for Boss/Wrench review of the manual probe. After Boss supplies its native
artifact, identify the exact operation, number and message; compare baseline and
aware short/deep controls and extended-path results. Implement only the correction
supported by those observations. Retain exact-case VFS comparisons, 8.3 ancestor
support, drive-letter rules, reparse/junction checks, hard-link protection, import
identities, source snapshots and UTF-8. Native Windows Debug/Release acceptance
must still exercise the original fixture and all applicable commands/security
checks after any demonstrated repair. No commit, push, branch or hosted execution
has been performed by the agent.
