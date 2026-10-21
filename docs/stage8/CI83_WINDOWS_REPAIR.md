# Stage 8 — CI #83 Windows Unicode repair

21 October 2026. Baseline `fbad2b642114a692f93d2f4ce7d3465ea489abf2`.
**Focused repair; Windows hosted verification pending. Stage 8 is not locked.**

## Inspected failures

[CI #83](https://github.com/Shmellyorc/Void.Language/actions/runs/39571077819),
[Windows Debug job 119125116028](https://github.com/Shmellyorc/Void.Language/actions/runs/39571077819/job/119125116028)
and [Windows Release job 119125116286](https://github.com/Shmellyorc/Void.Language/actions/runs/39571077819/job/119125116286)
both built successfully and passed 153/156 suites. Their decoded job logs confirm:

- `lsp_stage8` (#61): existing short/long workspace-ancestor checks passed;
  the subsequent `voidc check` lost the supplementary scalar in the temporary
  directory name (`é??`) and failed with VOID3002.
- `text_collections_sprint3` (#146): Python's default cp1252 fixture write
  raised UnicodeEncodeError for U+1F3AE before compiler execution.
- `lsp_sprint3` (#147): the same cp1252 fixture-write failure before LSP startup.

These Windows results are failures, not hosted acceptance. Other jobs and editor
steps are not certified by this repair record. No hosted rerun was requested.

## Repair and boundaries

`voidc` now uses Windows `wmain`, converts native UTF-16 arguments losslessly to
UTF-8 with the standard filesystem conversion, and runs the existing shared CLI.
Input and output filesystem arguments are decoded explicitly with `utf8_path`;
Linux/macOS retain their narrow entry point. This avoids the Windows ANSI code
page, including loss of supplementary Unicode characters. All configurations use
the same path, including Debug, Release, optimized/unoptimized JIT and Debug cache.

The shared LLVM object writer now passes `path_utf8(output)` to LLVM's UTF-8
filesystem API instead of converting through `path::string()` and the ANSI code
page. This covers ordinary emit and the existing proof-object command.

Both failing Sprint 3 fixtures explicitly write UTF-8; the process fixture also
explicitly decodes compiler output as UTF-8. The discovery fixture likewise uses
explicit encodings for its text I/O. No global Python UTF-8 mode, replacement
decoding, removed Unicode fixture or production validation bypass was introduced.

`src/frontend/source.cpp` and the CI #82 discovery repair are unchanged. Exact
case, containment, short/long workspace ancestors, nested selection, outside
paths, symlink/junction/reparse rejection, VFS and readonly protections remain.
The extended discovery test exercises Unicode ancestors with directory and file
CLI inputs, both JIT modes, two cache invocations, optimized/unoptimized emit to
Unicode object paths, proof output and Unicode failure diagnostics. It explicitly
retains the rejection of Unicode **module basenames**, which remain deferred.

## Local verification

Verification results and hashes are recorded in `ci83-repair-results.json`.
The final focused outcomes are **7/7 Debug and 7/7 Release (14/14 unique
configuration/suite outcomes)**: `debug_cache_processes`, `lsp_stage8`, `tooling`,
`modules`, `text_collections_sprint3`, `lsp_sprint3` and `source_cli`.
The initial selections passed 6/7 each because of the basename expectation below;
only `lsp_stage8` was rerun after correcting it (1/1 each). Reruns are not counted
as additional unique suites. The final discovery logs explicitly skip Windows
8.3 coverage on Linux.

Both repaired Sprint 3 suites also passed with cp1252 defaults forced, using each
Debug/Release executable pair: four additional process-harness executions;
`text_collections_sprint3` reports 106 process checks per execution, retaining
normal/O2 JIT, linked AOT, malformed UTF-8, readonly and collection failures.
Both unchanged baseline fixtures fail with the expected UnicodeEncodeError under
the same shim. Python syntax checks, explicit fixture-encoding inspection and
`git diff --check` passed. Root README, path helpers and source discovery match
the baseline byte for byte.

Commands used existing `build/debug` and `build/ci-release` configurations:

```sh
cmake --build build/debug --target voidc voidls void_tests -j4
cmake --build build/ci-release --target voidc voidls void_tests -j4
ctest --test-dir build/debug --output-on-failure -R '^(lsp_stage8|text_collections_sprint3|lsp_sprint3|modules|tooling|source_cli|debug_cache_processes)$'
ctest --test-dir build/ci-release --output-on-failure -R '^(lsp_stage8|text_collections_sprint3|lsp_sprint3|modules|tooling|source_cli|debug_cache_processes)$'
ctest --test-dir build/debug -V -R '^lsp_stage8$'
ctest --test-dir build/ci-release -V -R '^lsp_stage8$'
```

Only focused existing Linux Debug/Release targets and suites were selected; no
full local or hosted matrix, Runtime change, editor implementation change, root
README edit, Git mutation, website change or new stage was performed.

The first extended discovery run expected a Unicode module basename to compile;
both configurations correctly rejected it under the existing VFS contract. The
fixture was corrected to assert rejection, retaining positive Unicode ancestor
checks. This was a test expectation error, not a language-policy change.

The two baseline Sprint 3 scripts reproduce their UnicodeEncodeError under a
local cp1252-default I/O shim. This shim changes omitted pathlib/subprocess
encodings, not explicit UTF-8 or OS path handling; it does not emulate Windows.

Actual Windows Debug **and** Release builds/execution remain required, including
native UTF-16 argv conversion, supplementary-character paths, conditional 8.3
coverage and reparse/alias protections. Linux and the cp1252 shim cannot establish
Windows acceptance. Boss retains commit/push/branch and hosted-run control.
