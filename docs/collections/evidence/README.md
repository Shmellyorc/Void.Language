# Collections evidence

[FINAL_VALIDATION.json](FINAL_VALIDATION.json) is the final machine-readable index:
30/30 focused and 12/12 adjacent suites, profile/source/executable/configuration
hash checks, targeted malformed-guard checks and archived byte/hash audit.
[The validation map](../VALIDATION.md) explains coverage and its limits.

Final selections: [Debug](acceptance-debug-p5d5zp84/receipt.json),
[Release](acceptance-release-q24zvwya/receipt.json),
[strict ASan/UBSan/LSan](acceptance-san-9khqxhvs/receipt.json). Each has JUnit and
exact stdout/stderr. Earlier selections and all individual run receipts remain
separate; repeated successes are not added together to inflate the final count.

`crashes/` follows the existing tests/text_encoding/crash.py conventions. Its
historical name does not label all failures as native crashes. Seventeen original
status-1 diagnostics/IR/test-assertion failures retain exact readonly `_crash.void`
copies/imports and provenance, command, output, status and available build/tool
hashes. All 612 source and 369 provenance copies passed byte/hash verification.
[FAILURES.md](../FAILURES.md) records diagnoses and corrections separately.
Deliberate recorder controls use disposable storage; their thirteen induced
incidents are not actual VOID compiler crashes. H1–H7 remain separate.

The successful [benchmark](benchmark-ci-official-release-1zgqnc53/measurements.json)
and original failed generated workload are both retained. `build-logs/` contains
71 exact local logs, including rejected build attempts and the intermediate
recorder assertion. Log filenames follow the repository's existing ignore rule;
they are still included explicitly in the delivered inventory. They are supporting
history, not additional passing suite outcomes. `FRONTEND_PROBES.json` is the
pre-existing research evidence and is not execution acceptance.

[CHANGED_FILES.json](CHANGED_FILES.json) inventories changed/untracked repository
files plus all delivered collections evidence, including ignored archived logs.
It excludes its own bytes and [detached digest](CHANGED_FILES.sha256) to avoid a
recursive hash, and excludes incidental ignored build/bytecode products outside
the delivered evidence. No file was staged. Verify the detached digest from the
repository root with `sha256sum -c docs/collections/evidence/CHANGED_FILES.sha256`.
