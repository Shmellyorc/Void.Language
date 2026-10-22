# CI #90 Windows native-link repair

> **Current Windows status: RESOLVED by CI #95**, at
> `e25816bdb80e3f32613edf77c9cf3258232af966`: Debug 160/160 and Release 160/160.
> [Permanent engineering record and verified evidence](WINDOWS_ENGINEERING_RECORD.md)
> supersede the earlier pending Windows acceptance statements below. Original
> failures, local results and evidence remain historical; coverage limits and
> independent Boss/Wrench review are recorded in the current reference.

## Original engineering record (historical)

21 October 2026. Baseline `ab357879599edaf816e18fc43587deca411d8aeb`.
**IMPLEMENTED / LOCALLY VERIFIED; pending native verification.** Wrench review
and Boss's Windows acceptance remain required. Do not mark CI #90 resolved from
Linux results. After Boss supplies passing Windows Debug **160/160** and Release
**160/160**, update this record to **RESOLVED**, retaining the failed historical run.
This is a blocking-defect repair, not a permanent known-issue deferral.

Follow-up: [CI #92 directory reservation correction](CI92_WINDOWS_RESERVATION_REPAIR.md)
adds directory-read access to the original metadata-only pins and retains the
rename rejection gate. Its native movement/handle observations remain pending.

## Evidence and cause

- CI #88 rejected valid registered `RUNNER~1` ancestors. The
  [focused alias repair](CI88_WINDOWS_PATH_REPAIR.md) was subsequently verified:
  `native_output_paths_ci88` passed both Windows jobs in CI #89 and #90.
- CI #89 Windows jobs each passed 158/159. Genuine long project discovery failed.
  [Native diagnosis and manifest repair](CI89_WINDOWS_MANIFEST_REPAIR.md) established
  error 3 at 353 UTF-16 units and a successful `longPathAware` control with
  `LongPathsEnabled=1`. The shared manifest remains on **voidc and voidls**.
- [CI #90](https://github.com/Shmellyorc/Void.Language/actions/runs/39698313538)
  Windows Debug job `119561057931` and Release job `119561057968` each passed
  **158/160**. `application_workflow_stage9` and `long_physical_paths_ci89` failed
  with `clang: error: unable to set working directory`. Discovery progressed;
  Clang could not enter destination staging beneath the genuine deep project.
  [Retained decoded Windows logs, hashes and excerpts](ci90/LOGS.json) preserve
  both failures. Earlier evidence is unchanged.

The previous percent-output repair used `-working-directory <destination-stage>`
and relative `program.exe`. This avoids LLVM 19's percent interpretation in an
absolute output argument, but the directory itself can exceed Windows's legacy
current-directory limit. Shortening a filename **inside that same deep directory**
does not shorten the directory Clang must enter. `voidc`'s manifest does not change
Clang's process behavior.

**Boss's chosen solution:** use a short, uniquely named private Windows native-link
directory, transfer the verified executable into existing destination staging,
then use unchanged atomic publication. The physical project remains in place.

## Implemented contract

`src/native_build.cpp` uses Windows-only primitives in `src/native_windows.hpp`:

1. Keep validated analysis/lowering, source identities, VFS, configured resources,
   output admission and collision checks. Preserve the pinned Clang **19.1.7**
   version gate. Linux/macOS retain their existing staging and linker arguments.
2. Select the Windows `GetTempPathW` root. Validate its exact physical spelling,
   reject directory reparse points, and pin each ancestor with native handles
   excluding delete sharing before descending. Check root ownership and its real
   DACL: only the current user, SYSTEM or builtin Administrators may own it or
   receive directory mutation rights. Null/unsupported or broadly writable ACLs
   fail closed. An unsuitable configured temp root diagnoses; it does not silently
   fall back to an untrusted location.
3. Reuse LLVM unique-path generation **only for the leaf**, extracting its filename
   after generation. `vd-` plus 16 random hexadecimal positions is short. Validate
   the **complete UTF-16 directory + `program.exe` + NUL**, at most 240 units.
   `CreateDirectoryW` creates exclusively; retry only actual existing-name errors,
   at most 32 times. Failed reservations never confer cleanup ownership.
4. At creation, apply a protected, explicit DACL granting only the current token's
   user SID full access, inheritable to files/subdirectories. Read back the owner,
   protection flag and exact ACE through `GetSecurityInfo`; reject unexpected
   access. Randomness and `std::filesystem::permissions()` are not DACL evidence.
   Apply the same native protection to destination `.void-build-*` staging.
5. Emit `program.o` into the short directory. Copy the already admitted Runtime
   and fixture archives to `runtime.lib` and `fixture.lib`. Clang receives this
   short working directory and relative object/archive/output names. Tool-version
   capture also lives there. There is no process-wide cwd change or shell command.
6. Require a regular generated file without reparse redirection or multiple hard
   links. Hold a read handle denying writes/deletion. LLVM's COFF reader must
   recognize a PE executable, optional header and sections, executable flag and
   absence of the DLL flag; section contents must be readable within the file.
   Successful exit or nonempty bytes alone cannot pass this gate.
7. Transfer to `destination-stage/program.exe` using Windows `CREATE_NEW`, checked
   reads/writes and `FlushFileBuffers`. Rewind both still-open handles and compare
   every byte and file size. No existing staged artifact is overwritten. This
   same checked copy routine handles archive copies and works across volumes.
8. Revalidate the requested destination, then call existing **`publish()` unchanged**.
   Final Windows `MoveFileExW` retains REPLACE_EXISTING / WRITE_THROUGH and omits
   COPY_ALLOWED. Copying across volumes happens **before** final same-parent atomic
   replacement. Compile, link, copy, ACL or publication failure leaves the old
   executable untouched. Executing/locked outputs can reject replacement safely.
9. RAII closes artifact/directory handles and removes only successfully owned
   temporary directories on success and ordinary exception unwinding. Collision
   directories remain untouched. Forced process termination and external filesystem
   mutation/locks are not transactional cleanup guarantees.

Exact-case output leaves, registered 8.3 directory ancestors, drive-letter rules,
Unicode, literal `%`, spaces and `#`, source/import/hard-link collisions and
junction/reparse rejection remain in force. No project relocation, new frontend,
linker implementation, package machinery or general build framework was added.
DACL isolation is an account boundary, not isolation from another process running
as the same user or privileged administrators. Existing concurrent external
source/output mutation limitations remain.

Native security API references: [creation security descriptors](https://learn.microsoft.com/en-us/windows/win32/fileio/file-security-and-access-rights),
[handle DACL inspection](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo)
and [CreateFile sharing/reparse behavior](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).

## Verification and remaining native gate

[Machine-readable local evidence](CI90_EVIDENCE.json) records tests, attempts,
logs and source hashes. Linux Debug **9/9** and strict ASan/UBSan/leaks **9/9**
passed, including Stage 9 workflow, CI #88 paths, genuine CI #89 long paths,
Stage 8 discovery, numeric, text/collections, application/native, ergonomics and
local libraries. Strict checks ran outside the sandbox's LeakSanitizer restriction;
leaks and halt-on-error stayed enabled. A first Debug run lacked the Sprint 4 host
binary; it was built and the complete focused set passed. A sandboxed strict run
aborted in LeakSanitizer; that is retained separately from the successful run.

Clang 19.1.7 cross-target **syntax-only** checking of both Windows source and the
native helper passed against extracted MinGW headers. The host LLVM configuration
was adapted only in `/tmp` to disable LLVM_ON_UNIX for that syntax check. This
establishes neither MSVC linkage nor native execution. No Windows pass is claimed.

Both existing failing suites and their genuine six-level >260-character fixtures
remain. Windows-only checks were added **inside** `application_workflow_stage9`,
retaining the normal **160-suite** inventory. Its helper shares production helpers;
there are no production environment-variable fault-injection hooks. Native checks:

- Real concurrent compiler builds beneath Unicode/space/#/% roots; no short-temp
  leftovers. Deterministic exclusive reservation collisions and exhaustion preserve
  a foreign sentinel; concurrent random reservations, owned cleanup after exceptions
  and full-path-length rejection are checked directly.
- Protected owner-only DACL readback and effective `AccessCheck`: owner permitted,
  an Everyone-restricted token denied mutation. Insecure null-DACL roots reject.
  Directory reservation prevents rename; temporary-root junctions reject.
- Large binary byte comparison, exclusive target collision, locked source and
  generated-executable hard-link rejection. A disposable configured driver returns
  success with invalid nonempty output, or places a collision in **owned destination
  staging** before transfer; real compiler rejection preserves the old executable
  and cleans both workspaces. Real missing-library/version failures check cleanup.
- A deterministic Windows handle denying delete sharing forces publication failure;
  saved executable bytes survive. The existing running-executable and successful
  replacement checks remain, as do source/path/junction/8.3 protections.

**Pending:** native MSVC build/link of the production code and helper; every new
Windows subcheck; original long-path execution; Windows Debug/Release 160/160;
post-repair macOS execution. Actual second-volume transfer is also pending. When a
writable second volume exists, run `void_windows_native_staging.exe <directory>`;
it requires distinct volume serial numbers before verifying the copy. Also build
an application with TEMP on one volume and `-o` on the other, including replacement
and preservation failures. Missing hardware is reported, never counted as coverage.

Wrench should review root/DACL admission, exclusive ownership and failure cleanup,
PE/byte validation, and the boundary between cross-volume copy and unchanged
same-parent publication. Boss owns commits, branches, pushes and hosted acceptance.
