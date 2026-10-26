# Focused follow-up verification receipt

Baseline `108d7639f1dda45df201ea950d9abd751d024ff7`. Working directory:
`/home/shmellyorc/Documents/Void.Language`. Local Linux; existing Debug, Release and
strict sanitizer binaries. Production sources were unchanged and no compiler
rebuild or full acceptance matrix was necessary. CMake was reconfigured in each
cached build to register the new IO preservation self-test.

## Compiler/LSP and full 1 MiB semantic depth

Exact regex:

```text
^(text_encoding_semantic_depth|lsp_const_recovery|lsp_const_recovery_stack|lsp_protocol|lsp_stage11_globals|lsp_stage8|lsp_text_encoding)$
```

| Profile | Result | Seconds | Exact command/environment/hash receipt | Verbose log |
| --- | --- | ---: | --- | --- |
| Debug | 7/7 | 8.983 | [JSON](verification/crash-audit-debug-result.json) | [Log](verification/crash-audit-debug.log) |
| Release | 7/7 | 1.868 | [JSON](verification/crash-audit-release-result.json) | [Log](verification/crash-audit-release.log) |
| Strict | 6/6 | 6.328 | [JSON](verification/crash-audit-sanitizers-result.json) | [Log](verification/crash-audit-sanitizers.log) |

Full semantic-depth performs 34 cases per profile with Linux RLIMIT_STACK exactly
1,048,576 bytes. It preserves legal receiver/numeric/string chains through 124,
excessive-chain diagnostics and the later independent diagnostic. Native const
recovery checks 472 assertions twice (ordinary and actual 1 MiB server); strict
registration runs its ordinary variant and actual 1 MiB compiler depth. Existing
protocol, globals, invalid UTF-8 overlays, unsaved edits and text documentation
recovery tests passed. No absent sanitizer LSP-stack variant is claimed.

## Filesystem harness regression

One registered run per profile, with this exact regex:

```text
^(io_execution|io_async_execution|io_crash_preservation|text_encoding_crash_preservation)$
```

| Profile | Result | Seconds | Exact command/environment receipt | Log | JUnit |
| --- | --- | ---: | --- | --- | --- |
| Debug | 4/4 | 242.626 | [JSON](verification/io-debug-result.json) | [Log](verification/io-debug.log) | [XML](verification/io-debug-junit.xml) |
| Release | 4/4 | 179.827 | [JSON](verification/io-release-result.json) | [Log](verification/io-release.log) | [XML](verification/io-release-junit.xml) |
| Strict | 4/4 | 264.742 | [JSON](verification/io-sanitizers-result.json) | [Log](verification/io-sanitizers.log) | [XML](verification/io-sanitizers-junit.xml) |

Sync execution checks actual source/JIT/O2/independently linked AOT, including 18
source/file checks, 42 numeric vectors and both endian modes. Async execution checks
the same vector/byte-order/filesystem work through normal/O2 JIT. Successful runs
are regression coverage, not proof of a historical SIGSEGV repair. Workers and GC
were not disabled or reduced.

## Final preservation branch verification

After a disposable test demonstrated a missing hash for a relative executable, the
helper's cwd lookup and explanatory trigger text were corrected. IO success-path
source/generator behavior did not change. Both preservation tests were rerun in
all three profiles; large IO workloads were not repeated for this branch-only fix.

Exact selection:

```text
^(text_encoding_crash_preservation|io_crash_preservation)$
```

All **2/2 passed** per profile. [Exact commands, environment and final source hashes](preservation-final-commands.json):
[Debug log](verification/preservation-final-debug.log),
[Release log](verification/preservation-final-release.log),
[strict log](verification/preservation-final-sanitizers.log).
Each log records actual CTest commands and six intentional signal events inside
disposable test storage. The self-tests verify evidence before cleanup and assert
disposal, unchanged repository inventory, no duplicate capture, non-overwrite,
checked-failure preservation, colliding arbitrary filenames, editor snapshots,
source-free executable commands, companion/generator/stream bytes and hashes.
Ordinary exit-one diagnostics are excluded.

Every real run inherited absolute PYTHONPATH for reusable preservation and the
genuine crash root. Strict runs retained:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Approved execution outside the ptrace-restricted sandbox retained LeakSanitizer.
Sanitizer coverage of actual compiler/runner/AOT execution is in the real IO and
compiler/LSP runs; Python-only synthetic self-tests do not claim to instrument VOID.

## Source and evidence integrity

[completeness.json](completeness.json) records final source hashes, per-group logs
and JUnit hashes, baseline HEAD and unchanged evidence. All 2,937 original evidence/
protected files, all 13 genuine source copies and 14 provenance hashes matched;
the actual incident and original archived executable are intact. Git's index is
unchanged, root README/stdlib/production sources have no diff, and diff whitespace
checks pass. [Cleanup evidence](synthetic-cleanup.json) records 940 verified
synthetic deletions; the old subtree remains absent after every test profile.

[Generator comparison](generator-comparison.json) proves the current and baseline
sync/async/error-async text match in memory. The historical original temporary
source was not recovered by that comparison. [Classification](verification/crash-classification.md)
retains all unresolved distinctions and exact source references. No new genuine
crash, unproved production fix, hosted CI or Git mutation occurred.

## Final fatal-exit correction — baseline cc8d476

The [final correction receipt](fatal-exits/RECEIPT.md) supersedes the helper/self-test
source hashes above. Reserved ASan 87 and UBSan 86 exits now preserve evidence;
ordinary diagnostic exit 1 remains excluded. Windows uses recognizable ExitProcess
status, and signed Windows status cannot fail POSIX signal-name interpretation.
Focused preservation CTests passed 2/2 in Debug, Release and strict profiles. All
1,971 protected evidence files match; no full IO/compiler workload was rerun.
Historical receipts and genuine crash artifacts remain unchanged.
