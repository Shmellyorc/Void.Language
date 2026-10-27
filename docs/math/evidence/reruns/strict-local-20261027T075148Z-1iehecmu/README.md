# Frozen Math strict sanitizer rerun

27 October 2026. Local verification only; no hosted CI or production rebuild.

**28/28 previously failing tests passed. Zero failures, zero skips, exit status 0.**
CTest elapsed time: 176.75 seconds. Total verification run: 178.22 seconds.

Execution occurred outside Codex's process sandbox under danger-full-access.
The runner, CTest and sampled frozen test processes report TracerPid=0,
Seccomp=0 and NoNewPrivs=0. The recorded ancestor chain has no
codex-linux-sandbox launcher. See [execution context](execution-context.json).

Settings match the earlier recorded strict run:

- ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87
- UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
- LSAN_OPTIONS remains unset; leak checking is enabled through detect_leaks=1.

The exact selection is the 28 failures from the original
acceptance-sanitizers.json. [Selection](selection.json) and
[baseline](baseline.json) retain their commands, environment and hashes.
The existing frozen binaries were used without rebuilding the compiler/Runtime;
AOT test fixtures and relocated installs execute through those existing binaries.

All 16 frozen executable/configuration/library files are unchanged. All 20
original incident archives, comprising 967 files, are unchanged by SHA-256.
All 5,412 previously tracked files and all 138 original top-level evidence
receipts are unchanged. No new incident archive was produced. Git HEAD remains
194603ea00eb38aff72b7d6e950865b3a23df92a; the only new workspace content is this rerun directory.
No remaining failures were observed.

[Verification receipt](verification.json), [JUnit results](results.xml),
[original stdout](stdout.txt) and [original stderr](stderr.txt) are separate
from all prior receipts. Child-produced execution/install/LSP records are copied
into generated-evidence/. original-receipts/ retains pre-run evidence copies.
The raw receipt is preserved as verification-raw.json; its no-build wording is
clarified in the finalized receipt without changing measured results.

[File hashes](receipt-manifest.json) cover every file in this verification bundle
except that manifest itself. No production source, sanitizer options, Git state
or hosted CI was changed. This run does not establish native Windows/macOS results.
