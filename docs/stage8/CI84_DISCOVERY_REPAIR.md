# CI #83/#84 — cross-platform discovery and editor assertion repair

21 October 2026. Baseline `dbe01209e4e32da01260b4faaa66b8ce4020ad82`.
**Focused local verification complete; Wrench review and native platform
verification pending. Stage 8 is not hosted locked.**

## Inspected hosted evidence

- [CI #84 Windows Debug, job 119159023140](https://github.com/Shmellyorc/Void.Language/actions/runs/39581217844/job/119159023140):
  compiler CTest **156/156**; extension configuration unit test fails because
  actual root uses `/` while Node realpath uses `\`. TypeScript/build completed;
  the failed assertion prevents later root/entry/fallback assertions and real
  editor-host integration from establishing acceptance.
- [CI #83 macOS Release, job 119125116009](https://github.com/Shmellyorc/Void.Language/actions/runs/39571077819/job/119125116009):
  **155/156**, with `lsp_stage8` failing at its first discovery request under
  `/var/folders/.../VOID discovery é😀 .../project`. Canonical workspace spelling
  differs from the active parent's lexical spelling on macOS (`/private/var`).
- Both macOS configurations were checked, not only Release. CI #83 macOS Debug
  (119125115766) was still reported building; its job-log request returned 404
  BlobNotFound. CI #84 macOS Debug (119159023163) and Release (119159022745)
  were still building at the final status check, **10:28 UTC**. They establish
  no native Debug or Release acceptance for this repair.
- At that CI #84 check Windows Release (119159022980) succeeded; Windows Debug
  had the extension failure above. Ubuntu Debug/Release/Sanitizers were still
  building. No additional completed failure was reported. This is a dated
  snapshot, not a prediction of remaining job outcomes or a seven-job lock.

## Repair

The TypeScript test now applies the platform's `path.normalize` to **both** sides
of the root and entry equality assertions. Fallback selection now checks the full
canonical root and full entry location, replacing the weaker basename suffix
check. No lowercasing, nonempty checks, production adapter changes or removed
executable/negative-path tests were introduced. `voidls` continues returning its
existing canonical UTF-8 generic paths; Node's native separator spelling is a
test representation concern.

On POSIX only, a failed lexical containment comparison now examines prefixes
**from the filesystem root toward the active parent**, stopping at the first
prefix whose canonical path equals the canonical workspace boundary. The prefix
itself must not be a symlink. This permits ancestor spellings such as `/var` and
`/private/var`, but does not accept an outside symlink directly pointing back to
the workspace. It computes a lexical suffix from that prefix and feeds every
original descendant component to the existing alias/canonical checks.

The root-first stopping order matters: resolving the whole active directory, or
accepting the first match while walking upward, could erase a descendant link
that escapes and returns to the workspace. This repair never canonicalizes a
prefix below the first workspace match. Existing containment, exact main case,
nearest nested main, source admission, VFS/import identities and readonly rules
remain unchanged. Windows' CI #82 GetLongPathNameW/reparse branch is byte-identical
to baseline. CI #83 Unicode argv, fixture I/O and object-output fixes are preserved.
Debug and Release compile the same discovery logic; there is no mode-specific fix.

The Python fixture compares canonical POSIX results to resolved expected paths
and definition URIs, retaining exact canonical output assertions. Windows keeps
its existing physical-identity assertions. New POSIX tests model the macOS
temporary-root alias with a disposable symlink **above** the workspace, exercised
against canonical, native temporary and aliased root spellings. They cover root
and nearest nested main, invalid nested source without fallback, sibling escape,
inside/outside links, a link to the parent that returns through a regular project
directory, wrong descendant case and wrong main case. The original Unicode,
Windows 8.3 and all negative/readonly checks remain enabled.

## Focused verification

Before rebuilding, the new POSIX ancestor-alias regression failed against the
unchanged baseline executable with `Active file is outside selected workspace`.
After repair:

| Local check | Result |
| --- | --- |
| Linux Debug: lsp_protocol, lsp_stage8, lsp_stage7, tooling, modules, source_cli, debug_cache_processes | 7/7 |
| Linux Release: same focused suites | 7/7 |
| Final strengthened lsp_stage8 case/main rejection checks | 1/1 each; reruns not extra unique suites |
| Extension Node 24.19.0 TypeScript check, compile and esbuild bundle | Passed through npm test |
| Extension explicit TAP unit execution, including real native server selection | 4/4, zero skipped |
| Node win32 root/entry/fallback separator reproducer on Linux | Passed; different locations remain unequal |
| Python syntax and git diff whitespace checks | Passed |

Commands used `cmake --build build/{debug,ci-release} --target voidc voidls
void_tests -j4`, followed by focused `ctest -V -R
'^(lsp_protocol|lsp_stage8|lsp_stage7|tooling|modules|source_cli|debug_cache_processes)$'`
in each build. Only discovery was repeated after strengthening negative tests.
Extension commands were `npm test` and `node --test --test-isolation=none
--test-reporter=tap out/test/config.test.js` in `editors/vscode`, using Node
24.19.0 and the local Debug server. The explicit TAP execution records all four
test bodies beyond npm's per-file summary. Logs, hashes and dated hosted status
snapshots are retained in [machine-readable evidence](ci84-repair-results.json).

No native macOS or Windows environment is available locally. POSIX alias and
Node win32 reproducers are focused evidence, not native platform certification.
Both macOS **Debug and Release** must execute discovery/case/alias regressions;
Windows Debug must execute both normalized path assertions, unit tests and the
previously skipped editor-host steps. Keep Windows Release and its 8.3/reparse
coverage in the owner-controlled gate. Other in-progress CI #84 outcomes must be
reviewed when available. No complete matrix, hosted action, Git mutation, website,
root README change, language feature or Stage 9 work was performed.
