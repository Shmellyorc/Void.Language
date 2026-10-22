# CI #89 — verified Windows long-path manifest repair

21 October 2026. Focused production repair complete; Wrench review and native
Windows Debug/Release acceptance of the production executables remain required.

## Native cause and evidence

Boss authorized production repair after Windows path diagnosis #1.
The agent also retrieved the completed
[native diagnostic run](https://github.com/Shmellyorc/Void.Language/actions/runs/39697081031),
job `119556968370`, source `8cd9139f592af0a2d2566df158139951ec3a1ea4`.
[Extracted results](ci89-manifest/native-probe-results.json) and the
[complete decoded log metadata](ci89-manifest/NATIVE_LOG.json) preserve the evidence.
Original CI #89 failure logs and CI #88 repair/evidence are retained unchanged.

On Windows Server 2022 with `LongPathsEnabled=1`, the original six-level directory
was **353 UTF-16 units** long. Both short controls succeeded. For the deep fixture:

| Operation | Default manifest | `longPathAware` manifest |
| --- | --- | --- |
| `absolute` | 0 | 0 |
| `canonical` | 3 | 0 |
| `CreateFileW` | 3 | 0 |
| Extended-prefix `canonical` control | 0 | 0 |
| `GetFinalPathNameByHandleW` | Not called: open failed | 0 |

Windows error **3**, `ERROR_PATH_NOT_FOUND`, carried the message
**“The system cannot find the path specified.”** The existing directory opened
successfully with the extended prefix and with the aware process manifest.
The demonstrated cause is the default process's legacy path-length behavior at
the `CreateFileW` boundary used by MSVC `std::filesystem::canonical`. Process
awareness is the demonstrated correction. UTF-8 conversion, VFS identity and the
canonicalization algorithm are retained.

## Exact source change

The tested manifest was moved, byte-for-byte, from
`tests/stage9/long_path_aware.manifest` to
[`cmake/LongPathAware.manifest`](../../cmake/LongPathAware.manifest).
The diagnostic aware target now references that same shared file. CMake continues
to merge/embed a manifest source using its tested Windows mechanism.

[`CMakeLists.txt`](../../CMakeLists.txt) adds this block after defining `voidc`:

```cmake
if(WIN32 AND MSVC)
  # Both processes open project paths through the Windows filesystem APIs.
  foreach(target IN ITEMS voidc voidls)
    target_sources(${target} PRIVATE cmake/LongPathAware.manifest)
  endforeach()
endif()
```

`voidls` needs the same setting: startup canonicalizes the selected entry's parent
(`src/lsp/main.cpp`, `Server` constructor), discovery calls the shared
`discover_project`, and rename revalidates the canonical root. Its process would
otherwise retain the same failing Windows open behavior. The manifest is attached
to both actual executables, independently of `BUILD_TESTING`; installed copies
carry the embedded setting. Diagnostic controls remain excluded from the normal
all-target build. The default diagnostic target keeps its original manifest.

No path-resolution source was changed in this repair. Existing exact-case VFS,
8.3 ancestor/drive-letter handling, reparse/junction rejection, hard-link and
source/output collisions, snapshots and same-parent staging remain in place.
Linux/macOS receive no manifest source. Existing tests and the original genuine
six-level, greater-than-260-character fixture are preserved.

## Focused local verification

Built `voidc` and `voidls` in the existing Linux Debug and strict sanitizer builds.
Both passed **4/4 suites**, zero failed suites. After adding the deep LSP session,
the changed long-path suite passed again in both configurations:

- `application_workflow_stage9`
- `native_output_paths_ci88`
- `long_physical_paths_ci89`
- `lsp_stage8`

The long-path regression now also starts the real `voidls --discover` beneath the
same deep root, validates its selected entry/root, and rejects an incorrect-case
explicit entry. A real stdio LSP session also initializes at the deep entry, opens
the source without diagnostics, navigates to the imported module and shuts down
cleanly. Its CLI checks still cover `check`, `run`, `emit`, `build`, imported
modules, Unicode/spaces/#/%, hard links, symlinks, exact/case-sensitive output
names, preservation and staging cleanup. All previous assertions remain.
Strict settings: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

[Evidence and source hashes](CI89_MANIFEST_EVIDENCE.json) distinguish these local
results from the native diagnostic result. The native production binaries have
not yet been built/run by the agent. No full matrix, commit, push, branch creation
or hosted execution was performed. The existing hosted diagnostic was read only.

## System policy and remaining native gate

[Microsoft's documented opt-in](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
requires **both** the executable's `longPathAware` manifest and
`HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled=1` on supported
Windows. The native diagnosis proved the correction with that policy enabled.
This repair does not change machine policy. A disabled policy retains Windows's
legacy limits. Windows caches policy per process after affected filesystem calls;
restart `voidc`/`voidls` after a policy change. Administrators control policy; no
fallback, path shortening or project-root substitution is introduced.

Boss's native Windows Debug and Release gate must verify:

1. The actual `voidc.exe` and `voidls.exe` contain `longPathAware=true`, including
   installed copies; record `LongPathsEnabled=1` and the SDK/compiler identity.
2. The original `application_workflow_stage9` six-level build and resulting native
   executable succeed with an actual path longer than 260 characters.
3. `long_physical_paths_ci89` passes all CLI/import/output checks and deep `voidls`
   discovery/case rejection. `lsp_stage8` retains editor discovery/import/snapshot
   and alias checks. Exercise server startup/analysis at the deep entry as well.
4. `native_output_paths_ci88` still passes with actual OS 8.3 ancestors, both
   drive-letter cases, wrong-case output rejection, source/hard-link preservation
   and mandatory native junction/reparse rejection. Report symlink privilege
   limitations explicitly, as the existing tests do.

The successful native probe establishes the correction's mechanism; production
Windows acceptance and post-repair macOS execution remain owner-controlled gates.
