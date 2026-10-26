# Local Sprint 3 verification receipt

Final source is frozen in [source-freeze.json](source-freeze.json). All final
local selections passed with Boss-confirmed plain encode plus try_encode.
[completeness-final.json](completeness-final.json) records the final integrity audit.

| Build | Passed / selected | Elapsed seconds | Exact results | Summary |
| --- | ---: | ---: | --- | --- |
| debug | 56 / 56 | 1164.093 | [debug-results.json](debug-results.json) | [debug-summary.log](debug-summary.log) |
| release | 56 / 56 | 637.231 | [release-results.json](release-results.json) | [release-summary.log](release-summary.log) |
| sanitizers | 53 / 53 | 1198.112 | [sanitizers-results.json](sanitizers-results.json) | [sanitizers-summary.log](sanitizers-summary.log) |

Earlier post-stack-repair verification also passed Debug 56/56, Release 56/56 and
strict sanitizers 53/53 before the API adjustment. Those exact results, logs,
selections and source hashes are in [pre-api-acceptance/](pre-api-acceptance/).
The first smaller selection is in [initial-acceptance/](initial-acceptance/).
Failed receipts remain historical; retries are not presented as first-attempt passes.

## Reproduction commands and environment

Repository working directory: /home/shmellyorc/Documents/Void.Language.
Baseline HEAD: e24d3a95db03fd282c87a5d9b885471057e38798. Linux x86-64, LLVM/Clang 19.1.7.
No hosted CI, Git writes or remote execution.

Final selection is obtained with `ctest --test-dir build/ci-official-<profile>
--show-only=json-v1`. [acceptance.py](../../../tests/text_encoding/acceptance.py)
runs the exact registered commands with their registered working directories,
two workers per profile, and installation afterward as a serial operation. This
is local execution of registered commands, not a claim that CTest itself executed
the suite. Selection JSON retains every command/property; result JSON retains each
status/time/cwd/log. The wrapper permits 1,800 seconds per process so instrumentation
and concurrent Debug work do not silently lose coverage to short legacy timeouts.

```sh
python3 tests/text_encoding/acceptance.py debug
python3 tests/text_encoding/acceptance.py release
python3 tests/text_encoding/acceptance.py sanitizers
```

The driver exports absolute crash-preserving PYTHONPATH and VOID_CRASH_EVIDENCE.
Strict sanitizer execution uses approved execution outside the sandbox's ptrace
restriction, with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Leak checking was not
disabled. The earlier restricted-environment failure is retained in
[sanitizers-build-environment-failure.txt](sanitizers-build-environment-failure.txt).
Build flags, cache hashes and final executable hashes are recorded in
[final-builds.json](final-builds.json).

## Coverage

All eight fixtures run source check, real normal/O2 JIT, and independently linked
normal/O2 AOT. Their logs contain exact compile/link/invocation commands and times.
The fixtures cover text, hex, Base64, ASCII, 929 independently expected oracle cases,
binary/convert/memory/filesystem interoperability, large inputs and structured errors.
Oracles include every changed Unicode 16.0 mapping in chunks and contextual sigma;
the mapping oracle shares the pinned UCD with generation, while codec/scalar text
expectations come from independent Python reference operations and RFC vectors.

Ownership tests exercise 19 operations in each JIT mode, force collection at actual
generated polling points, inspect roots/frame publication and sweep allocation
budgets through success. Every failure checks immutable inputs, collection back to
baseline and successful retry. Each profile recovered 2,668 failed budgets per JIT mode and completed
41,778 ownership/root checks; exact per-operation counts remain in its ownership log.
No new collector, allocator, string representation or async framework was added.

Compiler metadata validates 73 public declarations, descriptions, each explicit
parameter and meaningful returns, with zero documentation warnings. Eleven
independent typed-IR mutations reject forged private casing operations. Persistent
LSP tests perform 54 hover/definition/signature/completion/repeated malformed-constant
and corrected-edit checks. Existing globals, generic, documentation and const-recovery
regressions are included. Relocated installs verify all four module bytes and the
Unicode license, then execute check/JIT/O2/linked AOT.

Existing string, UTF-8, collection, GC, binary, memory, convert/combined and filesystem
regressions remain selected. Existing io_async_execution passed in all three builds. These passes are
regression evidence, not an explanation or repair of its historical SIGSEGV.

The native Debug/Release text and LSP execution checks explicitly use a 1 MiB stack.
Strict sanitizer runs include parser-depth and full semantic-depth tests at 1 MiB;
the instrumented source-execution/LSP stack variants follow existing non-sanitizer
registration, rather than pretending an unrun sanitizer variant passed. Linux
stage11 globals stack checks are also selected. Native Windows/macOS execution is
pending the owner's later platform matrix; local Linux is not cross-platform acceptance.

## Crash evidence and actual corrections

One real compiler crash was found: Debug semantic-depth rejection of a 400-method
chain at 1 MiB. Exact original bytes were preserved before temporary cleanup:
[deep-methods_crash.void](crashes/incident-splghldg/inputs/deep-methods_crash.void),
SHA-256 73871bef34c030e336699c8e7ff706917428d0450dfa5e7c7b923c9419245b00.
[receipt.json](crashes/incident-splghldg/receipt.json) records command, cwd, -11/SIGSEGV,
exact stdout/stderr, original binary hash, stdlib source set and flags. Immutable
provenance includes the pre-repair compiler and generator. The original executable
is retained privately in build/text-encoding-crash-binaries/voidc-before-stack-repair;
it is not a tracked/public evidence artifact.

[GDB replay](stack-crash-gdb-before.txt) proves stack exhaustion at anchor's prologue,
with SP below the mapped stack. Isolating cold Function/name-resolution temporaries
reduced [measured frame subtraction](stack-frames.json): anchor 3840 to 3584 bytes,
call_expression 19904 to 16224. No semantic budget, reserve, resolver order or broad
exception behavior changed. Full legal-depth and safe-rejection regressions passed
after repair. This is separate from the unresolved historical filesystem crash.

Synthetic preservation artifacts live in [synthetic-preservation/](synthetic-preservation/),
not in the real-crash incident set. The permanent self-test checks exactly three
signal-only incidents, disk CRLF source bytes, suffix naming, non-overwrite/hash
invariants and unsaved LSP input snapshots. Harness duplicate-capture and missing
communicate-stdin failures were corrected; failing receipts were preserved.
Other actual diagnostic/harness corrections are recorded in
[IMPLEMENTATION.md](../IMPLEMENTATION.md), including unsupported enum casts, a local
name shadowing an imported type, an initial ownership-observation assumption and
the existing parser depth policy. No failed receipt was deleted or relabeled.

## Performance and practical limits

[PERFORMANCE.md](PERFORMANCE.md) reports measured invocation medians for repeat,
split, join, replace, hex and Base64 on 1,024 through 65,536 input bytes. Final
normal/O2 Debug/Release CSVs use the confirmed plain encode API. No prior implementation
existed, so there is no invented before/after speedup. Shared-host timings include
allocation, roots and collection; fourfold input sometimes costs more than fourfold
time. Algorithmic geometric growth/KMP review does not promise linear wall time.
The large fixture separately exercises 256 KiB text/binary, 512 KiB hex output and
349,528-byte Base64 output through all execution modes. Output caps are 64 MiB;
not every 64 MiB codec boundary is dynamically allocated in these focused tests.
Overflow/preflight paths additionally receive source review and extreme text-count tests.

## Boundaries

[protected.json](protected.json) retains original hashes for root README, console,
existing stdlib modules and filesystem research. Boss independently reformatted
stdlib/time.void during this sprint and confirmed ownership in chat. That formatting
is preserved, excluded from Sprint 3 implementation and separately recorded in
final completeness evidence. Root README/console/binary/memory/io/path/convert/error
and research files must still match their protected hashes. No branch, commit or
push is authorized. Local verification and completeness audit are complete. Stop for Golden Wrench review.
