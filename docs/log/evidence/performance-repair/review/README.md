# Golden Wrench review package

Release remains on hold. The synchronous screening target is unmet.

- `source.tar.gz`: all current modified/new source, test and documentation files, tracked diff and SHA-256 inventory.
- `evidence.tar.gz`: preserved incidents, raw measurements, generated instrumentation sources, commands/hashes, and fresh 88/88/87-test acceptance logs. Compiled binaries/objects are excluded; exact hashes and the frozen workspace binaries are retained.
- `manifest.json`: archive identities, counts, baseline HEAD and protected README identity.

For independent inspection, extract both archives over a checkout of baseline
HEAD `f9ac49f1558f326129948bc06c829263448b7714`. The source archive contains
changed/new files; unchanged project dependencies come from that HEAD. Verify
archive hashes against `manifest.json` and source hashes against the inventory.
See `docs/log/HANDOFF.md`, `PERFORMANCE_REPAIR.md` and the historical-plus-final
`BENCHMARKS.md` after extraction. No independent acceptance or
native Windows/macOS pass is claimed. No Git staging/commit/push or hosted CI.
