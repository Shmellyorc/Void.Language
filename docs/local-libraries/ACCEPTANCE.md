# Project-local libraries acceptance

21 October 2026. Baseline `9e946c2ecf176637ac67a07fdf86f492768433c6`.
**Implemented / locally verified; Wrench review and native Windows/macOS pending.**
[Contract](../LOCAL_LIBRARIES.md), [machine-readable results and hashes](results.json),
[raw logs](logs/). This evidence is separate from CI #87 and preserves its history.

## Implementation and invariants

Only `SourceGraph::resolve` and its existing import metadata assistance change
production behavior. The reserved unquoted prefix translates to ordinary project
VFS paths before the existing validation/loading pipeline. There is one parser,
resolver, canonical module identity and source snapshot, including when quoted
and local imports coexist. No Runtime, IR, LLVM, provider or adapter production
code changed. The existing 256-item completion limit also applies when a directory
supplies both its module and folder candidate.

Module regressions cover absent/rootless libraries, root/nested/default/explicit
aliases, diamonds and declaration cycles, canonical identity deduplication,
project-root constants, internal/private access, imported executable rejection,
selected application entry versus library main, case collisions, invalid paths,
escapes, symlinks, hard links, virtual sources and tombstones. They validate both
JIT optimization modes, retained snapshots, invalidation, actual compatible reload
and an old-generation lease. Tooling regressions cover directory-only discovery,
empty libraries, 256-item truncation, overlay-only entries, alias edits, UTF-16
suffixes, docs, standard-library isolation and project-shadow rejection.

The new ordinary `local_libraries` CTest uses an explicit `app.void` under a
Unicode/supplementary/space/#/% root from an unrelated cwd. Its **16 process
checks** cover missing imports, real alias/escape rejection, check, normal/O2 JIT,
linked normal/O2 AOT, cache hit and edited dependency miss, and application entry.
Its LSP session checks completion, mid-token aliases, diagnostics, incomplete
prefix recovery, definitions, docs, virtual didOpen/didClose and disk refresh,
requiring the same process and clean shutdown. Windows alias tests fall back to
junction creation; unavailable creation is explicitly reported rather than
counted as executed. Both Linux alias checks ran.

Actual VS Code uses the real `type` command, compiler completions, imported
source definition and documentation, followed by valid-source recovery without
another native child. Existing standard/quoted completions, incomplete typing,
restart/failure/shutdown, fixed discovery, trust and multi-root tests still run.
The prior CI #82–#86 source/discovery/security behavior is preserved.

## Final local results

| Configuration/check | Passed | Failed |
| --- | ---: | ---: |
| Linux Debug focused compiler/LSP/cache | 8/8 | 0 |
| Linux Release, same selection | 8/8 | 0 |
| Linux Debug native/source-admission guards | 2/2 | 0 |
| Linux Release native/source-admission guards | 2/2 | 0 |
| Strict ASan + UBSan + leak detection, all ten suites | 10/10 | 0 |
| Extension TypeScript check/compile/bundle | Passed | 0 |
| Explicit extension unit tests, Debug and Release servers | 4/4 each | 0 |
| Actual VS Code 1.141.0 trusted/restricted/multi-root | 3/3 each build | 0 |
| Python syntax, whitespace and preserved-file checks | Passed | 0 |

The eight focused suites are `debug_cache_processes`, `lsp_protocol`,
`lsp_stage8`, `tooling`, `analysis`, `modules`, `lsp_sprint6`, `local_libraries`.
Additional guards are `native_source` and `native`. Debug's eight took 25.41s,
Release's 8.72s; the strict ten took 31.89s. These are **30 unique configuration /
suite outcomes**, not repeated runs or a full compiler matrix. Builds completed
for voidc, voidls and void_tests in all three configurations. Strict verification
retained `detect_leaks=1`, `halt_on_error=1` and UBSan stacktrace/error settings.

## Initial failures and corrected verification

- Initial Release 7/8 failed linking an executable into a `%` directory. A trivial
  C program independently reproduced LLVM 19 lld's output-directory handling:
  plain, Unicode and `#` outputs worked; `%` output failed. The compiler's source
  and emitted object paths still contain all Unicode/#/% characters. Only the
  externally linked executable now uses a separate artifact directory. No VFS or
  Unicode validation was bypassed. This external linker limitation remains.
- Initial editor hover assertion treated plaintext MarkdownString spaces as raw
  spaces; VS Code encodes them as `&nbsp;`. The test normalizes that representation
  and still requires the specific documentation phrase. Production hover did not
  change. Final actual Debug and Release profiles passed.
- Initial sandbox strict runs failed 0/8 because LeakSanitizer cannot inspect
  threads under ptrace. Repeating outside the sandbox with checks enabled passed
  all ten; sanitizer errors were not suppressed. A sandbox Node run exposed only
  a file-level result; explicit host runs verified all four unit cases per build.

## Remaining verification and authority

Native Windows Debug/Release and macOS Debug/Release have not run locally. Their
next Boss-controlled run must execute local imports, Unicode roots, exact-case
and alias negatives, Windows junction/short-path/drive regressions, and the real
editor profiles. Wrench review remains pending. No owner semantic decision is
unresolved; these are platform verification limits.

Root README, production path discovery/Unicode conversion and production editor
configuration/lifecycle files are byte-identical to baseline. Historical
`docs/stage8/` acceptance and website files have no tracked changes. Source/log
hashes are captured in results.json. No hosted CI action, commit, push, branch,
package-manager/native-linker work, CLI redesign or Stage 9 work occurred.
