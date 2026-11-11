# W1 Windows repair: prerequisite research review

10 November 2026 (America/Vancouver). Review of the W1 instruction only; the
earlier G5 release-refusal assignment is not part of this work.

## Evidence and verdict

Reviewed all four requested records and the actual project publication/HTTPS
fixture source. Retrieved the completed [W1 run 42950139792](https://github.com/Shmellyorc/Void.Language/actions/runs/42950139792)
at `1e4c91ebdff41a672b9830acefe50880f3133cc5`, without dispatching anything.
Both Debug and Release built successfully, failed the native smoke at AOT root
admission, and passed **2/6 CTests**. The four failures are packages, managed
native, Git dependencies and Git HTTPS. They have three distinct causes.

The original complete log archive is retained as
`hosted/original/run-42950139792.zip` inside the external W1 evidence ZIP,
SHA-256 of the original member
`c6a8b021682bdd04a78b3fe302d8b5f8f5ea25764d3c801edc0ad5ca7628c404`.
Separate Debug/Release smoke and CTest excerpts are beside it. UTC timestamps
in those logs are 11 November and their local date is 10 November.
[Archive location and verification](WINDOWS_NATIVE_PROJECT_QUALIFICATION.md#evidence-and-cleanup)

**Proceed with the established private-root fixture. Preserve directory rename
exclusion. Treat OpenSSL certificate generation as a separate encoding boundary.**
No production, fixture or workflow correction is claimed by this research review.

## 1. Temporary-root creation

**Confirmed:** `.github/workflows/ci-windows-dependencies.yml:55–57` creates
`$RUNNER_TEMP/w1/t` using ordinary `New-Item -Force` and exports it as TMP/TEMP.
Both profiles subsequently report `Native temporary root permits untrusted
modification` in smoke AOT, `project_packages` and `project_managed_native`.
This is an ACL admission failure before native linking, not an LLVM failure.

CI #95 already established the correct primitive:
`tests/stage9/windows_native_staging.cpp:134–164`, `--create-private-root`.
It obtains the current token SID, creates the leaf exclusively with
`PrivateAccess`, verifies the protected exact DACL and root policy, and checks
effective owner/restricted-token access. Existing-name collision is an error,
not permission to adopt, delete or re-ACL the directory.

W1 did not record the root's SDDL. **Do not claim its offending ACE was OW**:
that was measured in CI #94/#95, while W1 could inherit another disallowed ACE.
Both are the same class of fixture-policy mismatch. Windows documents parent
ACL inheritance when creation has no explicit security descriptor.
[CreateDirectoryW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw).

Small correction to implement: build the existing `void_windows_native_staging`
helper along with the four W1 targets. Keep bootstrap/build scratch separate
from the compiler's specially admitted link root; do not export an unverified
`t` as TMP/TEMP. After building the helper, exclusively create an absent short
leaf through its UTF-8-stdin private-root command, require the verification
receipt, then publish TMP/TEMP for smoke and all six CTests. Record the original
inherited ACL in an owned disposable diagnostic leaf if needed. Do not weaken
`verify_root`, add trusted principals, or replace exclusive creation with
post-creation `icacls` on an adopted directory. No new CTest is needed merely to
run this existing setup helper.

## 2. Directory pinning: a self-conflicting open exists in source

**Confirmed native symptom:** both `project_git_dependencies` runs fail at the
four-fetch fixture with `Cannot pin native temporary directory (Windows error
32)`. The log does not identify the individual failed component or its handles.

**Concrete source explanation, not a captured native handle trace:**

1. `src/frontend/project_creation.cpp:532` calls `make_directory(stage, true)`.
2. `open_node`, lines 149–152, holds that stage with desired `DELETE` plus
   directory-read/metadata access and **without** delete sharing.
3. `publish_project_directory:560–563` constructs `Parent(path.parent_path())`
   for entries beneath the same stage while its owning handle remains open.
4. `Parent:132` calls `src/native_windows.hpp:44–59`, which reopens every
   ancestor with directory-read access and **without** delete sharing.
5. That new sharing mask does not permit the DELETE access already held by
   VOID's own staging handle. Windows sharing compatibility is bidirectional,
   so the new open can fail with error 32 even though it requests no DELETE.
   The four-process test exposes this path; concurrency is not needed for the
   described self-conflict.

[CreateFileW sharing rules](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
explicitly cover an existing DELETE-access handle and a new open lacking
FILE_SHARE_DELETE. CI #95's rename exclusion is the reason to keep the general
pinning policy unchanged.

**Do not globally add FILE_SHARE_DELETE, drop FILE_LIST_DIRECTORY, close the
owning stage handle during verification, or retry until the race disappears.**
These would remove or interrupt the established protection.

The correction should make private-stage traversal reuse the already admitted,
still-held directory authority rather than opening an incompatible second
reservation. An operation-scoped borrowed/duplicated stage handle is a candidate;
all independent ancestors still require ordinary pins. Preserve identity and
reparse checks, and ensure the verifier's recursive reads use the same authority.
Do not introduce an ambient path allowlist or a process-wide trust cache.
This is a design recommendation, not a reviewed corrective patch.

Minimum native proof before choosing the patch: reproduce the two exact open
requests on one owned directory, log error and file identity, then exercise the
proposed authority reuse while a separate handle/process attempts rename. Rename
must remain refused throughout traversal, work after reservations close, and
publication through the authorized owning handle must still work. Then rerun
the existing concurrent fetch, collision, rollback and immutable-snapshot checks.
The current log alone cannot rule out an additional conflicting ancestor handle;
capture the failing component if the minimal reproduction does not explain it.

## 3. Exact OpenSSL boundary

Both failed commands select **`C:\Program Files\OpenSSL\bin\openssl.EXE`**.
They do not select Git's bundled fallback. `tests/project/git_https.py:81–86`
uses PATH first and consults the Git-relative candidate only if PATH finds none.

Both jobs identify Windows image **20261101.353.1**, OS build 20348.5773. Its
[version-specific software inventory](https://github.com/actions/runner-images/blob/win22/20261101.353/images/windows/Windows2022-Readme.md)
lists **OpenSSL 3.6.5**. This identifies the advertised installed package;
**the selected executable's `version -a`, hash and build options were not logged**.
Do not present inventory as a direct version-query receipt.

The first `req -new -x509` command passes an absolute config path containing
`VOID D3 HTTPS 雪 .../tls.cnf`. OpenSSL reports `VOID D3 HTTPS ? .../tls.cnf`,
`Invalid argument`, and `BIO_new_file` / `fopen` errors. This directly demonstrates
loss of the path character at the third-party executable/file-opening boundary.
It occurs before CA creation, HTTPS service startup or Git TLS acquisition.
The logs do not by themselves separate narrow argv conversion from the later
CRT/BIO path conversion. It is not evidence that VOID's VFS, TLS verification or
wide-path APIs failed.

First record the resolved absolute executable, SHA-256 and `version -a`. Use the
same executable/config in a bounded control: Unicode absolute config argument
versus ASCII relative `tls.cnf` with Python setting the Unicode cwd. If the
relative form works, use relative filenames for **all** OpenSSL file operands
and keep the existing Unicode directory. That is smaller than relocation.
If it still fails, test an isolated ASCII certificate-generation directory;
copy generated files with Python into the original Unicode fixture and load
them there. Never relocate the repositories/project or reduce their Unicode
coverage. Record results instead of an automatic silent fallback. Preserve CA
verification, hostname checks, certificate lifetime and zero second-origin
requests. No HTTPS fixture change was made before these controls.

## Qualification and exact changes in this review

- Added this report, original downloaded run archive, and four failure excerpts.
- Read-only source/log inspection confirmed the same failure categories in
  Debug and Release. No tests were rerun; the original 2/6 results are failures,
  not new qualification.
- A local VirtualBox guest-control `cmd /c ver` attempt could not authenticate
  user `a`; no guest process or native probe ran. No credentials were changed.
  Native reproduction and direct OpenSSL-version capture remain unavailable
  in this session. Historical guest results do not qualify W1's hosted image.
- Production, tests, workflow, pinned dependencies, ACL principals and sharing
  flags are unchanged. No staging, commits, pushes or hosted dispatch occurred.

The implementation/verification scope remains the six exact W1 CTests plus
native discovery/check/JIT/AOT smoke in **both Debug and Release**, preserving
Unicode/space paths, static CRT/PE checks, ownership and publication guarantees.
The existing workflow's strict test-selection checks and failure diagnostics
must remain. Neither the unrelated G5 callback correction nor the Windows
DLL-loader investigation belongs in this repair.

Return this evidence for Golden Wrench review. The private-root remedy is
established; stage-authority reuse needs a native ownership proof, and the
OpenSSL fixture correction needs the exact-tool control above. W1 is not closed.
