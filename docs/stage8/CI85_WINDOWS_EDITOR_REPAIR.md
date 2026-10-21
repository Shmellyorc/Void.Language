# CI #85 — Windows editor discovery repair

21 October 2026. Baseline `e8a1ea7433b465c3800090a7b84737d7474c9228`.
**Focused repair; native Windows execution and Wrench review remain required.
Stage 8 is not hosted locked.**

## Failure and diagnosis

[Windows Debug job 119180481831](https://github.com/Shmellyorc/Void.Language/actions/runs/39587658170/job/119180481831)
built, passed **156/156 compiler suites** and extension units, then failed actual
editor-host discovery. The inspected log records this exact argument pair:

```text
workspace: c:\Users\runneradmin\AppData\Local\Temp\VOID editor é😀 osIlXV\project space # %
active:    c:\Users\runneradmin\AppData\Local\Temp\VOID editor é😀 osIlXV\project space # %\main.void
```

`voidls` rejected containment before starting the LSP session; the initial-server
timeout is consequential, not the primary failure. CI #85 Windows Release
succeeded. The other five jobs were cancelled; cancellation is not acceptance.

The demonstrated defect is **case-sensitive comparison of DOS drive roots**.
`start.lexically_relative(boundary)` rejects a differing root name, and the
Windows fallback then compares `GetLongPathNameW(prefix)` to the canonical boundary
with exact path equality. A valid `c:`/`C:` difference can therefore survive every
prefix iteration and falsely produce “outside selected workspace”. The explicit
entry used by the old configuration unit test bypassed this fallback entirely.

This conclusion is supported by the exact lower-drive hosted arguments, inspection
of the existing algorithm, a compiled drive-only lexical counterexample and a
WinAPI experiment using the Unicode/spaces/#/% fixture. Microsoft's
[VS 2022 STL source](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/inc/filesystem)
shows that canonical uses GetFinalPathNameByHandleW and lexical-relative rejects
different root names; [GetLongPathNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getlongpathnamew)
expands short names and does not promise to unify spelling with that handle API.

**Evidence limit:** CI did not log its intermediate canonical/expanded strings.
Native Windows tracing was unavailable locally. Wine preserved the lower drive
in GetLongPathNameW; canonical handle queries with an upper drive yielded an upper
drive, while lower/lower queries stayed lower. Thus the mixed-drive experiment
demonstrates the comparison defect but does not reproduce the exact hosted
lower/lower canonical transformation. Wine also differs in ancestor directory
case handling. It is not Windows compiler or hosted acceptance. New Windows test
output records original, canonical and expanded spellings for the next owner run.

## Small correction and retained security

Only the existing Windows prefix comparison changes. After long-name expansion,
if both roots are ordinary two-character DOS drive names and their letters match
in ASCII case, the expanded drive letter adopts the canonical boundary letter.
Every directory character remains unchanged. Different drives and UNC/device
roots receive no new equivalence rule. There is no blanket path lowercasing,
physical-equivalence containment shortcut or source-identity change.

The original prefix metadata checks, reparse-point rejection, short-name expansion,
lexical suffix retention and exact descendant canonical checks remain. A wrong-case
descendant, junction, symlink or 8.3 alias inside the VFS still rejects. Outside
siblings and unrelated drives cannot become contained. Canonical result paths,
nearest exact main selection, Unicode CLI conversion, UTF-8 fixture I/O, object
output, readonly authority and import identities are preserved. POSIX discovery
is byte-identical to the baseline, including the CI #84 ancestor-alias repair.

## Regression coverage

- `tests/lsp/stage8.py` retains Unicode and adds literal `# %` to the temporary
  ancestor. On Windows, upper/lower workspace and active drives are independently
  crossed with long and supported 8.3 ancestor spellings. Cases include root and
  nested nearest main, explicit override, no-active discovery, wrong-case
  descendants, saved/unsaved standalone selection,
  sibling escape, descendant short aliases and mixed-drive
  symlink/junction rejection. Drive-only coverage runs even without 8.3 support.
- Unrelated-drive rejection always runs. Where SUBST is available, a disposable
  drive maps the very same fixture bytes and must still be rejected; the mapping
  is removed in a finally block. Unsupported mappings/8.3 names report skips.
- The extension configuration test now exercises automatic discovery with all
  upper/lower workspace/active combinations, retaining exact root/entry equality
  and every existing negative/executable assertion.
- The actual trusted Windows editor host preflights both workspace drive cases
  with the real URI.fsPath active file, checks canonical selection and still
  requires the real language server to reach running and pass the complete
  editor checks. Restricted-trust/multi-root profiles return before this preflight;
  they gain no native authority. No fake server or transport is substituted.

The real Linux host also exposed a **stale test expectation** after successful
startup: it waited for the pre-Sprint-6 “interpolation is deferred” message.
The test now checks supported string interpolation, rejects an unknown substitution
with VOID2002 naming `braces`, and retains failed-navigation and subsequent repair
assertions. No compiler diagnostic or language semantics changed for this repair.

## Verification and remaining gate

Exact local commands, outcomes, source/protected hashes, experiments and logs are
recorded in `ci85-repair-results.json`. No complete compiler matrix was selected.

| Local check | Final outcome |
| --- | --- |
| Linux Debug focused compiler/LSP suites | 7/7, 29.42s |
| Linux Release, same focused suites | 7/7, 12.40s |
| Final discovery after Windows fixture helper changes | 1/1 each; not extra unique suites |
| Extension TypeScript/compile/bundle | Passed |
| Explicit extension unit tests with Debug and Release servers | 4/4 each; zero skipped |
| Actual VS Code 1.141.0 trusted, restricted and multi-root profiles | 3/3 per server configuration, 6/6 total |
| Python syntax / whitespace / preserved-source checks | Passed |

The focused suites are `debug_cache_processes`, `lsp_protocol`, `lsp_stage8`,
`lsp_stage7`, `tooling`, `modules`, `source_cli`. These establish 14/14 final unique
configuration/suite outcomes, not a new unfiltered matrix. Native Windows drive,
8.3, SUBST and junction cases are prepared but not executed on this Linux host.

Unsuccessful attempts remain in evidence: the first Wine launch was blocked by
the syscall sandbox, then prefix initialization required a prepared private
directory. The first Electron launch was sandbox-blocked. Outside that sandbox,
the first actual host reached running but failed the stale interpolation assertion.
After correcting that expectation, all profiles passed in both configurations.
No failed attempt is counted as acceptance and no rerun inflates suite totals.

Linux results cannot certify the Windows-only branch. Native Windows Debug and
Release must run the drive/8.3/junction/mapped-drive cases, and Debug must complete
the real trusted/untrusted/multi-root editor profiles. Review all seven eventual
owner-controlled jobs before any Stage 8 lock; no Stage 9 is authorized.

No hosted action, commit, push, branch, website or root README change was made.
