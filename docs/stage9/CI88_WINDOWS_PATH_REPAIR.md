# CI #88 — Windows physical output ancestor repair

21 October 2026. Baseline `aef1ad7c1dfc49a1940f0d239ed76e13ffcb972f`.
**Focused source repair and Linux verification complete; Wrench review and native
Windows Debug/Release execution remain required. No hosted lock is claimed.**

## Historical hosted evidence

Boss supplied [CI #88](https://github.com/Shmellyorc/Void.Language/actions/runs/39645461021):
Ubuntu Debug, Release and Sanitizers and macOS ARM64 Debug/Release passed.
Windows Debug and Release each passed **152/158** suites; the same six failures
rejected the valid OS-generated ancestor `C:/Users/RUNNER~1` in
`src/native_build.cpp::exact_path()`. Both emit and build were affected.
These are the original owner's reported results, not an agent rerun or a new
post-repair platform claim. The earlier [Stage 9 acceptance](ACCEPTANCE.md),
source hashes and logs remain historical snapshots without rewritten outcomes.

## Correction and safety

The existing `discover_project()` Windows policy in `src/frontend/source.cpp`
was inspected first: inspect directory prefixes, reject reparse points, expand
only the admitted prefix and retain descendant spelling. Its workspace/VFS
boundary and identity logic is unchanged.

Physical output validation has no project-discovery boundary: output overrides
can live outside the selected project. Its existing component walk now admits a
Windows directory component only when it is either an exact enumerated filename
or an **exact OS-registered 8.3 alternate directory name**. `FindFirstFileW` obtains
the native `cAlternateFileName` and corresponding long leaf. Only that checked
prefix advances; subsequent components are validated independently as supplied.
The [Windows metadata contract](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-win32_find_dataw)
defines that alternate field as the OS 8.3 name. A case-insensitive lookup by
itself is insufficient to authorize a component.

Wrong-case ordinary directories and existing output filenames still reject.
Short-name aliases of output **files** are not admitted. Windows directory
reparse points reject before traversal, whether their long or short spelling is
used; output file symlinks retain the existing ordinary-file rejection. No whole
destination canonicalization, case folding, recursive search or relaxed VFS
identity rule was added. DOS drive-letter equivalence remains an OS root property;
no directory/filename character is folded.

The POSIX walk still compares exact directory entries and retains its existing
ancestor-symlink behavior, including macOS /var-style spellings. Source/import
collision checks and hard-link equivalence remain before publication. Staging,
percent-path handling, compiler resources, native Runtime linking, output
replacement and failure preservation are unchanged.

## Focused verification

[Recorded results and hashes](CI88_EVIDENCE.json) retain local evidence under
`build/ci88-path-repair/` (build/configure logs, JUnit and LastTest logs).

| Linux configuration | Focused suites | Result | Seconds |
| --- | --- | --- | --- |
| Debug | 8 | 8/8 passed | 67.41 |
| Release | 8 | 8/8 passed | 56.08 |
| ASan/UBSan/leaks | 8 | 8/8 passed | 74.14 |

**24/24 focused configuration/suite outcomes**, no failures or skipped suites.
No full local or hosted matrix was repeated. Strict sanitizer checks ran outside
ptrace with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

The selected suites are `native_output_paths_ci88`, `application_workflow_stage9`,
`numeric_sprint2`, `text_collections_sprint3`, `application_native_sprint4`,
`ergonomics_sprint6`, `local_libraries` and `lsp_stage8`. Existing assertions and
expectations were not edited. They cover real normal/O2 emission/linking, native
Runtime/collections, artifact failure preservation, private installation,
percent/Unicode paths, local imports and the established discovery policy.

The new suite executes 16 portable process checks per Linux configuration:
real emit/build outputs, managed executable status/stdout/stderr, exact filenames,
case-sensitive distinct names, source/import/hard-link collision rejection,
symlink-file rejection and preserved POSIX directory-symlink behavior. It checks
that failed temporary artifacts do not remain. These process counts are not added
to the unique suite total.

## Native hosted checks still required

Windows was unavailable locally; the guarded production branch and Windows-only
subchecks are **not locally verified**. The added CTest suite runs automatically
in both existing Windows configurations. Require:

- An actual registered 8.3 ancestor obtained using `GetShortPathNameW`, preferably
  the existing runner profile, with same-file proof against its long spelling.
  The test fails explicitly if no real 8.3 ancestor is available; it does not
  fabricate a literal RUNNER~1 directory or silently count missing coverage.
- Both emit and build through the raw temp spelling, long spelling, short ancestor
  and both drive-letter cases, with Unicode, spaces, # and % in descendants.
- Wrong-case existing output leaves and ordinary descendant directories reject
  without changing the original artifact. Exact names succeed. Output-file short
  aliases reject when the filesystem supplies such an alias.
- Source/import/hard-link collisions reject through those spellings. File symlinks
  reject where privilege is available; privilege error 1314 is reported explicitly.
- Mandatory actual directory junction rejection, including its registered short
  leaf when available, with target bytes preserved. The fixture uses
  [CPython's native CreateJunction API](https://github.com/python/cpython/blob/3.12/Modules/_winapi.c)
  so cmd.exe cannot interpolate percent-containing paths.
- Every previous assertion in the original 158-suite configuration remains; the
  added suite brings the standard configured inventory to 159. Inspect the same
  six original failing suites and the new regression in **both** Windows builds.

Native macOS post-repair execution also remains owner-controlled. The owner's
five successful pre-repair jobs do not establish post-repair verification. No
Windows/macOS success is inferred from Linux, and no hosted workflow was started.

## Handoff boundary

Only physical native path validation, one focused regression and its CMake test
registration changed. Frontend discovery, CLI/language semantics, Runtime,
installation/link configuration, editor production systems and the seven-job
workflow are unchanged. Root README and CI #87 repair are untouched. No commit,
push, branch, hosted action or publication occurred. Stop for Wrench review.
