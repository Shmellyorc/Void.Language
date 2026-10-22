# CI #91: two focused Windows test repairs

21 October 2026. Baseline `8880cab35101ebb852600bd2c2a986ff4a241fa9`.
**Locally verified; Windows-native acceptance pending. Wrench review pending.**
Compiler native-linking implementation, manifests, staging and publication are unchanged.

[CI #91 evidence](CI91_NATIVE_EVIDENCE.json) was retrieved read-only. Windows
Release passed 158/160; Debug was cancelled after showing the same two failures,
so no final Debug total is claimed. The workflow helper failed creating a restricted
token with Windows error 5. The deep-path suite successfully built/stat'ed its
executable, then Python's process launch failed with WinError 206. These are test
failures after the [native-link repair](CI90_WINDOWS_NATIVE_LINK_REPAIR.md).

## Changes and rationale

- `windows_native_staging.cpp` now calls `DuplicateTokenEx`, requesting exactly
  `TOKEN_QUERY | TOKEN_DUPLICATE`, `SecurityImpersonation`, `TokenImpersonation`.
  The token must support both AccessCheck and subsequent restricted-token creation.
  Owner-allowed and Everyone-restricted mutation-denied assertions remain mandatory;
  neither errors nor missing privileges are treated as a pass.
- The two genuine deep-executable launch sites in `long_paths.py` and `workflow.py`
  now pass an explicit `executable=` Windows application path. A small shared test
  helper adds `\\?\` for absolute drive paths or `\\?\UNC\` for UNC paths and
  preserves an existing extended prefix. It rejects relative/drive-relative paths.
  Only the application spelling changes: argv retains the original physical path.
- Before Windows launch, require the actual executable path to exceed 260 UTF-16
  units and prove `samefile(original, extended)`. Retain six deep components,
  original source-depth assertions, Unicode/spaces/#/%, expected exit status 7 and
  workflow empty stdout. Print the extended application only after successful
  native execution. There is no shell, copying, shortening, fallback or skipped gate.
  Other subprocess calls and Linux/macOS launch behavior remain unchanged.

Microsoft documents [TOKEN_DUPLICATE for CreateRestrictedToken](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken)
and [explicit DuplicateTokenEx access selection](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-duplicatetokenex).
[CPython 3.12.10](https://github.com/python/cpython/blob/v3.12.10/Lib/subprocess.py)
passes `executable` separately to CreateProcess. Microsoft's
[CreateProcessW contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
limits the parsed module portion to MAX_PATH when the application parameter is
NULL. This supports testing the explicit application approach; **it does not prove
that the extended-path invocation succeeds on native Windows**.

## Local evidence and remaining gate

[Local evidence and source hashes](CI91_EVIDENCE.json); logs under
`build/ci91-test-repair/`. Focused Linux Debug **3/3**, strict ASan/UBSan/leaks
**3/3**, no failures/skips: `application_workflow_stage9`,
`native_output_paths_ci88`, `long_physical_paths_ci89`. Strict execution was approved
outside the sandbox's known LeakSanitizer restriction, with leak detection and
halt-on-error enabled. Python syntax and eight pure Windows spelling cases passed.
Clang 19.1.7 Windows-target syntax checking of the updated C++ helper passed using
the previously extracted MinGW headers and temporary adjusted LLVM configuration.
These are not native Windows execution or MSVC linkage evidence.

Boss's native Windows Debug/Release verification must run the effective DACL
helper through both AccessCheck outcomes, then execute both genuine deep binaries
through the explicit extended application paths, retaining same-file, depth and
exit-status assertions. Require both normal 160-suite configurations to pass.
If the invocation fails, retain its exact exception and OS error; investigate the
launch boundary without changing the working compiler or weakening the fixtures.
Neither CI #90 nor this test repair is declared resolved. No hosted run, commit,
push, branch or root README change was performed.
