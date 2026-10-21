# Stage 8 Sprint 1 — CI #82 Windows path repair

20 October 2026 (America/Los_Angeles). **Focused local repair verified on Linux
Debug and Release; Windows Debug/Release hosted verification pending. No lock.**

## Inspected hosted evidence

[CI #82, attempt 1](https://github.com/Shmellyorc/Void.Language/actions/runs/39533326046)
ran commit `cf06d166f60639cc883d80e57390d3990a817063`.
[Windows 2022 / Release, job 119004113561](https://github.com/Shmellyorc/Void.Language/actions/runs/39533326046/job/119004113561)
passed 140/141 suites; test #61, `lsp_stage8`, failed at its initial discovery
request with `voidls: Active file is outside selected workspace`. The logged
workspace and active paths both use `C:\Users\RUNNER~1\AppData\Local\Temp`.
Canonicalizing the workspace expands that ancestor spelling while the active
parent originally retained it, making lexical containment fail.

[Windows Debug, job 119004113913](https://github.com/Shmellyorc/Void.Language/actions/runs/39533326046/job/119004113913)
built successfully but its test step was cancelled. The inspected log has no
`lsp_stage8` execution; it establishes neither a pass nor a failure for discovery.
The overall run and the other jobs were cancelled, so CI #82 is not acceptance.

## Exact repair and retained boundaries

In `vd::discover_project()` (`src/frontend/source.cpp`), a Windows lexical
containment mismatch now inspects active-parent prefixes using
[GetLongPathNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getlongpathnamew).
It accepts a prefix only when its expanded spelling equals the already canonical
workspace boundary. Original descendant components remain unchanged for the
existing symlink/canonical-equality checks. Prefix inspection explicitly rejects
reparse points and fails on metadata errors. Ancestor search then uses the checked
canonical-boundary spelling, ensuring that the stopping comparison also agrees.
It does not canonicalize the whole active parent before validating descendants.

This allows legitimate OS 8.3/long spelling differences at or above the workspace
boundary while retaining exact descendant spelling, exact `main.void` case,
nearest-main selection, workspace containment and alias rejection. Short-name
components below the workspace remain disallowed VFS aliases. SourceGraph,
module identities, import admission, hard-link checks and write authority are
unchanged. The same frontend function is compiled in Debug and Release; there is
no build-mode-specific discovery path or test exception.

`tests/lsp/stage8.py` now compares discovered existing paths and source-definition
URIs with physical file identity on Windows. Non-Windows comparisons remain exact.
The LSP test opens the canonical entry URI. These assertion changes do not alter
server admission or grant source write access.

Added process checks cover Unicode paths, root and nested nearest-main selection,
invalid selected source without fallback, explicit override, saved/unsaved standalone
selection, outside-workspace siblings (including a shared textual prefix), wrong-case
main and Windows descendant case, directory aliases into/out of the workspace,
and an outside alias pointing back in. Windows uses junctions if symlink creation
is unavailable. Existing import completion, Unicode edit ranges, source docs,
installed-source rename/format rejection and overlay resistance remain exercised;
the focused tooling/modules suites retain VFS and hard-link regressions.

On Windows with distinct 8.3 names available, the same suite tests every short/long
workspace-versus-active ancestor combination, root-level and nested active files,
nested nearest-main selection, outside rejection, descendant case and short-name
alias rejection. Unsupported short names or alias creation emit explicit skips.

## Focused local verification

Only `voidls`, `voidc` and `void_tests` were rebuilt in the existing configurations.
No full matrix was run or triggered. [Machine-readable results and source hashes](ci82-repair-results.json)
retain the exact commands and test output. Raw local build/test logs are
`/tmp/void-ci82-{debug,release}-{build,tests}.log`.

| Local configuration | Focused suites | Result |
| --- | --- | --- |
| Linux Debug, `build/debug` | `lsp_protocol`, `lsp_stage8`, `tooling`, `modules` | 4/4 passed |
| Linux Release, `build/ci-release` | Same four suites | 4/4 passed |

```sh
cmake --build build/debug --target voidls voidc void_tests -j4
ctest --test-dir build/debug --output-on-failure -R '^(modules|tooling|lsp_stage8|lsp_protocol)$'
cmake --build build/ci-release --target voidls voidc void_tests -j4
ctest --test-dir build/ci-release --output-on-failure -R '^(modules|tooling|lsp_stage8|lsp_protocol)$'
```

The Windows-only API branch and 8.3 regressions cannot execute on this Linux host.
Actual Windows compilation and `lsp_stage8` execution with the new short/long and
reparse-point checks remain required in **both Debug and Release**. Local success
does not establish that hosted repair. Earlier 423/423 consolidated evidence covers
the original Sprint 1 source snapshot, not this repair. No new editor acceptance,
Git mutation, root README edit, hosted rerun or Sprint 2 is claimed or authorized.
