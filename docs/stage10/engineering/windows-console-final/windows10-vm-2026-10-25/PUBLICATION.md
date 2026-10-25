# Windows 10 VM evidence: public/private boundary

**Public:** [review](WRENCH_REVIEW.md), [reviewed result summary](review-results.json),
[ten path-free scripted test results](automatic-flow-results.json) and
[original-byte evidence hashes](evidence-manifest.json).

**Private:** `native-acceptance-evidence.zip` is retained by the project owner
outside the Git checkout. The original 406 native records and 9 toolchain receipts
are **not** published because they contain local Windows profile paths, host
identifiers and original console input/output transcripts.

Unchanged private curated archive SHA-256:
`f43d68604a06d6319c68767a6ff76c9eb08a65c28fdcde5d4bb0f9fa7becd4cc`.
Original full 109 MB upload SHA-256:
`d61e4769b3a3ba080976cadc8ec0e116fd27d2256b29625ecc7f2c70049199a8`.

The public scripted-flow JSON is a **derived projection**, not byte-for-byte raw
evidence: `launch_command` fields containing absolute paths were omitted, and
its source SHA-256 is recorded in the manifest. No test was rerun or reclassified.
The original raw evidence was reviewed and hash-checked before this publication
choice. Failures, uncertain observations, and remaining ConPTY/Win32 gaps stay
visible in the reviewed report. Stage 10 remains **INCOMPLETE**.
