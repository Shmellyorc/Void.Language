# Await syntax local evidence

Final acceptance uses the latest passing Release, Debug and strict sanitizer
receipt. The earlier Release fixture failure remains separate. XML/logs retain
per-test output, including hard stack limits, independent source/IR checks,
precise-root allocation sweeps, AOT object comparisons and editor results.
Raw CTest logs use `.txt` so the repository's general log ignore pattern does
not omit them; their contents are unchanged.

`baseline-sources.tar.xz` contains the starting committed build/source/tool files;
`current-sources.tar.xz` contains the complete corresponding current files,
including new tests. Both include protected README solely for reproduction.
`reproduction.tar.xz` contains exact drivers, source controls, original attempt
logs, performance fixtures/results, configured CMake/compile/link flags and tools.
Paths are repository-relative; helpers record the original absolute workspace.
Local frozen compiler images remain in `build/async-await-syntax/baseline/`.

`regression-receipts.tar.xz` preserves this sprint's newly generated reports from
existing test wrappers under their original repository paths. Their exact path
and hash list is in `regression-receipts.json`; earlier tracked receipts and the
original compiler-stabilization evidence packet remain unchanged. Extract this
archive at the repository root to restore those report paths if needed.

Rebuild using `build.py <release|debug|san>` after extracting reproduction helpers.
Run `acceptance.py <profile> <selection>` with `selection.txt`; sanitizer selection
omits `lsp_const_recovery_stack`, matching the preexisting policy documented in the
handoff. Strict ASan/leak/UBSan options are set by the acceptance driver. Run
`compatibility.py` for frozen baseline source controls. Run `performance/run.py`
serially after all builds/tests finish; use a fresh results directory when
replaying so new attempts do not replace retained measurements.

`manifest.json` binds source/build/profile/measurement identities;
`inventory.json` lists every modified/new delivery file. The inventory and
checksum list do not embed their own hashes; `SHA256SUMS` checks the
inventory and all other evidence files. Hosted and native Windows/macOS
acceptance are not claimed.
