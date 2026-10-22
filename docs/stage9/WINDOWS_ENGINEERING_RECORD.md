# Windows engineering record: CI #88–#95

21 October 2026. **RESOLVED: blocking Windows Stage 9 investigation.**
Verified source: `e25816bdb80e3f32613edf77c9cf3258232af966`.
Boss and Wrench review of this permanent record is the next handoff.

## Verified milestone and scope

[Compiler Foundation CI #95](https://github.com/Shmellyorc/Void.Language/actions/runs/39724345062)
established native **Windows Release 160/160 + Windows Debug 160/160 = 320/320**.
Both jobs completed successfully. In each, `application_workflow_stage9`,
`native_output_paths_ci88` and `long_physical_paths_ci89` passed.

| Configuration | Native job | Suites | Failures |
| --- | --- | --- | --- |
| Windows 2022 / Release | [119644734324](https://github.com/Shmellyorc/Void.Language/actions/runs/39724345062/job/119644734324) | 160/160 | 0 |
| Windows 2022 / Debug | [119644734222](https://github.com/Shmellyorc/Void.Language/actions/runs/39724345062/job/119644734222) | 160/160 | 0 |

[Acceptance evidence](CI95_WINDOWS_ACCEPTANCE.json) preserves exact native test ZIPs,
GitHub artifact digests, extracted-log hashes, source hashes and observations.
Both ZIP SHA-256 values match GitHub's artifact metadata. Each `LastTest.log`
contains 160 passing tests; the job logs independently report zero failures out
of 160. This is Windows acceptance, **not a verified complete seven-job matrix**.
No new compiler build, test suite or hosted execution was needed for this documentation work.

## Investigation chronology

These are original outcomes; a later repair does not turn an earlier run green.
Historical reports/JSON/logs remain evidence of their dated state.

| CI | Original Windows result and demonstrated failure | Correction and retained evidence |
| --- | --- | --- |
| #88 | Debug/Release 152/158: legitimate `RUNNER~1` output ancestors rejected. | Admit exact OS-registered 8.3 **directory ancestors**, retaining exact descendants/leaves, reparse and identity checks. [Original repair](CI88_WINDOWS_PATH_REPAIR.md). |
| #89 | Debug/Release 158/159: genuine deep project discovery failed; a 353-UTF-16-unit native probe returned error 3. | `longPathAware` on both voidc/voidls; aware control succeeded with LongPathsEnabled=1. [Investigation](CI89_WINDOWS_LONG_PATH_INVESTIGATION.md), [manifest repair/probe](CI89_WINDOWS_MANIFEST_REPAIR.md). |
| #90 | Debug/Release 158/160: both deep native-build suites failed, `clang: error: unable to set working directory`. | Boss's short private Windows native-link workspace, checked transfer into destination staging, unchanged publication. [Repair and failed logs](CI90_WINDOWS_NATIVE_LINK_REPAIR.md). |
| #91 | Release 158/160; Debug cancelled after the same two failures: restricted-token error 5 and Python launch WinError 206 after successful build. | DuplicateTokenEx with QUERY/DUPLICATE rights; explicit extended Windows application path for subprocess. [Test repair/evidence](CI91_WINDOWS_TEST_REPAIR.md). |
| #92 | Release 158/160; no final Debug total claimed in the original record: post-launch console encoding failure and successful rename despite metadata handles. | `ascii(application)` diagnostics; FILE_LIST_DIRECTORY access on pins; retain rename assertion and measure path/handle identities. [Repair/evidence](CI92_WINDOWS_RESERVATION_REPAIR.md). |
| #93 | Debug/Release 159/160: concurrency failed because the ordinary mkdir-created TMP/TEMP fixture inherited a rejected ACL. | Exclusive private fixture creation using production DACL primitives, including the overlong root. [Fixture repair/evidence](CI93_WINDOWS_TEMP_ROOT_FIXTURE.md). |
| #94 | Debug/Release 159/160: fake Clang lost Unicode arguments, reporting no mapping to the target multibyte code page. | Test helper wmain + once-only UTF-16-to-UTF-8 conversion matching production; existing helper body preserved. [Native failure/SDDL excerpts](ci95/CI94_FAILURE_EXCERPTS.json), [original local evidence](ci95/CI94_LOCAL_EVIDENCE.json). |
| #95 | Debug/Release 160/160: 320/320 native suites passed. | Blocking investigation resolved at the verified baseline above. [Acceptance evidence](CI95_WINDOWS_ACCEPTANCE.json). |

## Unicode is a boundary contract

On Windows, narrow `main(int, char**)` arguments cannot be assumed to contain
UTF-8. Production `src/main.cpp` already received UTF-16 through `wmain()`;
the fake Clang helper did not. The helper now receives `wmain(int, wchar_t**)`,
converts **every** argument, including argv[0], once into owned UTF-8 strings,
then builds stable pointers plus the null sentinel for its unchanged shared body.
Keep that storage alive for the entire call. Its malformed PE output, staged-copy
collision, private-root modes, native security tests and optional second-volume
mode keep their prior behavior.

The same native boundary must apply to real and fake tools. In particular,
working-directory and copy-collision arguments contain Unicode, emoji, spaces,
`#` and `%`. Do not ban emoji or shorten fixtures to hide an argument-encoding
bug. Keep UTF-16 at Windows API boundaries and UTF-8 internally; do not route paths
through the ANSI code page. The fixture's existing UTF-8 stdin protocol is explicit.

Console encoding is separate: CI #92's genuine deep executable had already
launched with same-file proof and status 7 when cp1252 printing failed on an emoji.
`ascii(application)` makes that test diagnostic console-safe without changing the
path or execution. A failed print is not evidence that the path or native linker
failed.

Python's deep launch uses `subprocess.run(..., executable=extended_application)`
with `\\?\` for absolute drive paths, or `\\?\UNC\` for UNC paths. argv keeps the
original path. Both suites require the actual executable to exceed 260 UTF-16
units, prove original/extended names identify the same file, and check exit 7;
workflow also checks empty stdout. CI #95 records both real launches succeeding.
There is no copying to a shorter execution location, shell or fallback.

## Long paths, linking and publication

The CI #89 manifest correction applies to **both voidc and voidls** and requires
Windows's supported long-path policy, including LongPathsEnabled=1. Process
awareness fixes the observed discovery/open boundary; it does not make every child
tool/API accept an arbitrarily long working directory. In CI #90, Clang 19.1.7
could not enter staging under the deep project even though discovery now worked.

The previous relative `program.exe` output avoided LLVM 19's absolute percent-path
interpretation, but shortening that leaf did not shorten Clang's working directory.
**Boss chose a short, randomly named private Windows native-link workspace.**
Preserve that design and the project's real physical location, source identities,
ValidatedAnalysis, VFS and output validation. Keep LLVM/Clang **19.1.7** pinned.

- Reuse LLVM random generation on the leaf only, extract that leaf, and reserve
  exclusively with CreateDirectoryW. Retry genuine collisions; never adopt or
  delete another process's existing directory. Bound the **complete** linker path,
  including `program.exe` and NUL, to 240 UTF-16 units.
- Validate the trusted temporary root; reject reparse paths and untrusted ACLs.
  Apply/verify a protected current-user DACL through native Windows mechanisms,
  not random names or `std::filesystem::permissions()` alone.
- Emit `program.o` there and copy admitted archives to `runtime.lib` / `fixture.lib`.
  Use that short cwd and relative linker input/output names. No global cwd change,
  project relocation, FNV/GUID subsystem or second linker/build framework.
- Validate the generated private executable with LLVM's PE/section reader and
  ordinary-file/reparse/hard-link checks. Successful tool exit or nonempty bytes
  alone is insufficient. Hold source handles against mutation during validation.
- Copy the executable into the existing private **destination** staging directory
  with CREATE_NEW, checked reads/writes, flush, size and byte comparison. Never
  silently overwrite a staged artifact. Revalidate the requested destination and
  call existing `publish()` unchanged.
- Final Windows MoveFileExW uses REPLACE_EXISTING / WRITE_THROUGH, without
  COPY_ALLOWED. Transfer can precede publication across volumes; final replacement
  remains in the destination's parent. Failure must preserve previous executable
  bytes. Owned staging is cleaned on success and ordinary exception unwinding.

Retain registered 8.3 ancestors, drive-letter rules, exact-case descendants and
output leaves, source/import/hard-link collisions, reparse/junction rejection and
Unicode/% paths. Retain locked-output, copy-failure, malformed-output, cleanup and
atomic replacement assertions. Existing external filesystem mutation/forced-kill
limitations are not a transactional filesystem guarantee. Linux/macOS retain their
existing native-build behavior.

## Security: prove reservations and inspect inherited ACLs

Metadata-only opens with FILE_READ_ATTRIBUTES / READ_CONTROL did **not** reserve
a directory against rename despite excluding FILE_SHARE_DELETE. CI #93 and #95
measured a real move: BOOL 1, old name absent, new name matching the original file
ID, and both metadata handles retaining their original directory identity.
Adding **FILE_LIST_DIRECTORY** requests directory-read access for sharing checks.
With delete sharing still excluded, native CI measured BOOL 0 and
**ERROR_SHARING_VIOLATION (32)**, original name present, destination absent, and
all 15 live handle identities unchanged. Do not replace this measured guarantee
with an assumption about the share flags alone. Private DACLs are an account
boundary, not isolation from the same user or privileged administrators. Keep the original rejection
assertion and the positive/negative identity controls. Harness `failure=0` on a
successful call is a sentinel; GetLastError is meaningful only after failure.

CI #94/#95 observed this SDDL on the original inherited fixture:

```text
O:BAD:(A;OICIID;FA;;;SY)(A;OICIID;FA;;;BA)(A;OICIID;FA;;;OW)
```

The owner is BA; the DACL includes inherited SYSTEM, Administrators and OWNER
RIGHTS (`OW`, S-1-3-4) full-access ACEs. CPython 3.12.10's mode-0700 temporary
parent supplies inheritable SY/BA/OW entries; ordinary child mkdir inherits them.
Production `verify_root()` rejects mutation grants outside its explicit trusted
principal policy. The native fixture observation was:
`inherited_root_admitted=0 reason=Native temporary root permits untrusted modification.`
This was a **test fixture defect**, not permission to relax production root trust.

Both the normal and overlong roots now use exclusive CreateDirectoryW with the
existing PrivateAccess descriptor at creation, exact protected-DACL readback,
unchanged verify_root(), and effective AccessCheck. CI #95 observed for both:

```text
O:LAD:P(A;OICI;FA;;;LA)
private_root_verified=1 owner_access=1 restricted_mutation=0
```

`D:P` records protection from inheritance. On these runners Windows rendered the
explicit current-user owner/ACE as `LA`; code obtains the SID from the current
token. Do not hard-code a runner's alias/SID. Owner mutation must succeed and an
Everyone-restricted token must fail mutation checks. Existing-root reservation
must fail without re-ACL/adoption, preserving its marker. Only a successfully
created leaf confers cleanup ownership. The overlong root must pass security
admission before its intended length rejection. Keep null-DACL rejection and the
junction test; never weaken security to make a fixture pass.

## Investigation guardrails and remaining limits

Classify each failure at the actual boundary: source discovery, tool cwd, process
launch, console printing, token rights, ACL admission or fake-tool argument decoding.
Keep error numbers, exit codes, identities and actual ACL observations. Fake tools
must reproduce production platform boundaries while preserving deliberate failures.
Don't remove assertions after an unexpected result; add a bounded observation and
correct the demonstrated assumption with the smallest safe change.

Linux Debug/sanitizer tests and Windows cross-target syntax checks were useful
local evidence throughout. **Neither proves native Windows behavior.** Native CI
is the acceptance gate for these Windows contracts. Preserve historical failed
attempts and distinguish them from the later native acceptance.

Actual second-volume transfer remains unverified: both CI #95 native helper logs
explicitly say `Cross-volume copy pending: supply a writable second-volume directory`.
The design contract is retained, but no second-volume pass is claimed. This record
also makes no complete seven-job matrix, post-change macOS or general filesystem/
Windows-version certification claim. Those coverage limits do not reopen the
resolved blocking Windows reproduction. Boss controls further architecture, Git
and CI; future work follows Boss's chosen language-development scope.
