# CI #93: private Windows TMP/TEMP test fixtures

> **Current Windows status: RESOLVED by CI #95**, at
> `e25816bdb80e3f32613edf77c9cf3258232af966`: Debug 160/160 and Release 160/160.
> [Permanent engineering record and verified evidence](WINDOWS_ENGINEERING_RECORD.md)
> supersede the earlier pending Windows acceptance statements below. Original
> failures, local results and evidence remain historical; coverage limits and
> independent Boss/Wrench review are recorded in the current reference.

## Original engineering record (historical)

21 October 2026. Baseline `a6405689399019969f2dd6f76dd8434c097431cd`.
**Focused fixture repair locally verified; new Windows-native acceptance pending.**
Compiler, linker, directory pins, DACL checks and `verify_root()` are unchanged.

## Native evidence and inherited-ACL investigation

[Boss-run CI #93](https://github.com/Shmellyorc/Void.Language/actions/runs/39720055655)
Windows Debug and Release each passed **159/160**. Both passed the genuine deep-path
suite and native staging helper, including effective DACL access and insecure-root
rejection. [Exact excerpts](CI93_NATIVE_EVIDENCE.json) now confirm the CI #92 probes:
metadata-only handles allowed a real rename and retained original file identity;
directory-read pins rejected rename with error **32**, preserving source and all
15 handle identities. These are native observations, not predictions from Linux.

The sole workflow failure occurred after assigning `linkroot` as TMP/TEMP and
starting concurrent builds: **Native temporary root permits untrusted modification.**
The compiler correctly rejected a test root created with ordinary `Path.mkdir()`.

Source-supported diagnosis: the configured CPython 3.12 runner uses
[os.mkdir's Windows implementation](https://github.com/python/cpython/blob/v3.12.10/Modules/posixmodule.c).
For mode 0700, used by tempfile's directory creation, it supplies a protected DACL
with inheritable SYSTEM, Administrators and **OWNER RIGHTS (`OW`, S-1-3-4)** full
access entries. Ordinary mode-0777 mkdir instead supplies null security attributes,
so children inherit the parent's ACEs. [Windows documents that inheritance](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw).
Thus the fixture can inherit an OWNER RIGHTS mutation ACE, while `verify_root()`
admits explicit current-user/SYSTEM/Administrators mutation principals. Recognizing
new principals in that allowlist is outside this repair; the fixture must establish
the existing contract.

**The CI #93 log did not dump the exact ACL.** OWNER RIGHTS is the diagnosis from
the matching source/inheritance chain; the new inspection mode records the actual
native owner/DACL SDDL and admission reason before the fixture is replaced.

## Focused implementation

Only `tests/stage9/workflow.py` and its existing Windows native helper change:

- Add test-only `--inspect-root` and `--create-private-root` modes. Paths travel as
  UTF-8 stdin, preserving Unicode/emoji/% independently of narrow Windows argv.
  Inspection logs actual SDDL and the unchanged `verify_root()` outcome.
- Private creation reuses production `PrivateAccess`, `pin_directories()`, exact
  DACL readback and `verify_root()`, plus the existing effective `AccessCheck` test.
  `CreateDirectoryW` exclusively creates the requested leaf with the protected,
  explicit current-user SID DACL **at creation**. There is no adoption, re-ACL of
  an existing directory, new temporary-directory system or compiler test hook.
  Successful readback, root admission, owner access and restricted-token denial
  are required before Python receives `private_root_verified=1`.
- Observe the original inherited `linkroot` incarnation without assigning it to
  TMP/TEMP, remove that empty diagnostic directory, then create the real root
  privately at the same Unicode/space/#/% location. A duplicate-create attempt
  must fail while preserving an existing ownership marker.
- Create the **overlong** root's final leaf through the same verified native
  primitive after making its parents. Require DACL admission first; the compiler
  must then reach its intended complete-path-length rejection. Remove the unused
  intermediate environment pointing TMP/TEMP at the unsecured project directory.
- The junction-root test still redirects to the verified private root and must
  reject reparse traversal. The intentionally null-DACL root in the native helper
  still rejects. Ambient system TMP/TEMP is untouched; production validates it.
- Scan of test/tool sources found no other TMP/TEMP overrides. Native helper
  fixture/deep directories already descend from production private staging.
  Concurrent compilation, old-byte preservation, cleanup checks, four workers,
  six-level deep paths, Unicode/% and all original assertions remain.

Creation/verification failures close handles and remove only the newly owned empty
leaf. Collision failure never obtains cleanup ownership. On success, the existing
Python TemporaryDirectory context retains eventual cleanup ownership. SDDL and
ASCII-escaped path diagnostics remain safe on the Windows console codec.

## Verification and Wrench handoff

[Machine-readable evidence/hashes](CI93_EVIDENCE.json); local logs under
`build/ci93-fixture-repair/`. Linux Debug **3/3**, strict ASan/UBSan/leaks **3/3**,
Python syntax and Windows-target Clang 19.1.7 syntax checking of the helper passed.
The strict suites ran outside the sandbox's LeakSanitizer restriction with leaks
and halt-on-error enabled. Cross syntax used the existing extracted MinGW headers
and temporary LLVM configuration; it is not MSVC linkage or native execution.

Wrench should inspect exclusive creation, protected current-user DACL readback,
unchanged production root admission, UTF-8 path transport, collision ownership,
and the overlong fixture's check ordering. Boss's next native verification must
retain the inherited/private SDDL lines, confirm both private roots pass owner and
restricted-token checks, reach the intended long-root error, and pass all original
concurrency/cleanup/failure checks. Windows Debug/Release **160/160 remains pending**.
No commits, pushes, branches, hosted CI dispatch or root README changes occurred.
