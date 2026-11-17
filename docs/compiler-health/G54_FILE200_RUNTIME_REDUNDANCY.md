# G54 — Reuse leased rule extents in short ASCII translation

G54 starts from accepted G52 `771fbfc56b5dcec16b01b84473351e05e417835b`. The runtime change is in `src/runtime/text_case.cpp`, and the boundary test is in `tests/text_encoding/text.void`. The November 5 owner disposition treats the original File200 10 ms target as an optimization aim. [Owner decision](../log/LOG_PERF_DECISION.md#current-boss-disposition--5-november-2026).

## Trace and proof

In File200, `logger.write` admits a record and calls `_prepare`, which calls `_frame`. The empty logger name returns from `_quote` without translation. The 64-byte message goes through `translate_scalars` with 81 sorted escape rules. The common short ASCII case proves that no rule matches and publishes the original rooted string. Interpolation builds the record, then UTF-8 encoding makes the byte list. Delivery checks its size against the rotation budget and calls the existing synchronous file write. The native bridge snapshots the verified byte list into owned storage, then executes the checked file request. Close syncs the file and closes it. [Source framing and delivery](../../stdlib/log.void) · [Native request and write path](../../src/runtime/filesystem_abi.cpp) · [Platform sync](../../src/runtime/filesystem.cpp#L550).

G52's list validator first proves the layout, backing completeness and extent. `BufferReadLease` admits the exact layout and saves the live prefix while rooting the selected backing. The translator checks that the two lease counts match before entering the bounded proof. Inside that proof, the loop condition proves each visited index is below the common live prefix. Each call to `BufferLayout::element_offset` then performs a capacity check, once for the key and again for the replacement. The loop and lease prove `index < count <= capacity` for each layout. The immutable layout constructor verifies representability of the whole extent, including `base + stride * capacity`, and its schema ensures the element bytes fit inside the stride. [Lease and layout construction](../../src/runtime/buffer.cpp) · [Generic offset helper](../../src/runtime/heap.cpp#L99).

G54 hoists each base and stride and uses them to derive addresses in the existing loop. The proof still reads key, replacement and replacement string extent in the original order. The zero-count return follows the ASCII scan and precedes any backing address calculation, so an empty list works with null backing. The helper holds the Runtime mutex throughout. Its live leases freeze the selected buffer contents and its exact roots protect both buffers. No address survives the helper or crosses a poll, callback, native transition or unlock. An ambiguous rule still sends the original invocation to the checked general path. The same code still validates root ownership, ordered unique keys, Unicode scalar range and replacement byte budget, and performs the existing result revalidation at publication. [Changed helper](../../src/runtime/text_case.cpp#L191).

Other repeated-looking operations protect different events. The list header check obtains the backing before the read lease acquires its own lock and publishes a reader on that backing. The second check admits that reader against current state. Translation also validates the destination again when it publishes, after the bounded proof has released its borrows. The owned byte snapshot preserves record storage across native waiting, and a successful write is distinct from the final durability sync. G54 retains these boundaries.

The maintained source fixture checks 256 live rule pairs, rejection of a duplicate in the last slot before reporting a negative output budget, and the unmodified 257-pair fallback. Existing text and logging suites exercise replacement, Unicode, exact bytes, error order, ownership, native cancellation and partial delivery. [Boundary fixture](../../tests/text_encoding/text.void).

## Prior work and external precedents

The bulk byte snapshot and scalar write result proposed or studied in the older reports are integrated. The 16,200-to-200 allocator experiment was rejected, as was automatic join/encoding fusion. None is repeated here. Go's range lowering and Rust's slice iteration drive repeated addresses from a bounded extent. LLVM's loop predication considers replacing per-iteration checks with a proved range before the loop. We use that same range reasoning inside the existing read lease and mutex, with VOID's validation and fallback. Go's slog and Rust's BufWriter are examples of buffering at the output boundary, while this task saves CPU work earlier in translation. [Integrated logging history](../log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md) · [Go range lowering](https://github.com/golang/go/blob/go1.25.0/src/cmd/compile/internal/walk/range.go) · [Rust slice iteration](https://github.com/rust-lang/rust/blob/1.91.0/library/core/src/slice/iter/macros.rs) · [LLVM LoopPredication](https://github.com/llvm/llvm-project/blob/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Transforms/Scalar/LoopPredication.cpp) · [Go slog handler](https://go.dev/src/log/slog/handler.go) · [Rust BufWriter](https://github.com/rust-lang/rust/blob/1.91.0/library/std/src/io/buffered/bufwriter.rs).

## Work removed

The matched Linux builds use the installed LLVM/Clang/LLD 23.1.4 at `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. The reconstructed G52 Runtime has SHA-256 `78b5da953ce0b98b2046f51895cc2156b331a6f28e1b819f1f59848b03c5c5b7`, which matches the prior G52 receipt. Relative to G54 only `text_case.cpp.o` changes within the archive. Independently emitted normal and optimized File200 and guard application objects match byte-for-byte across versions. In the Runtime helper, LLVM emits two calls to the generic checked offset routine per loop iteration on G52 and zero on G54. The retained native disassembly and IR show the change.

| File200 operations per complete sample | G52 | G54 |
| --- | ---: | ---: |
| Translation invocations | 200 | 200 |
| Rule pairs visited and validated | 16,200 | 16,200 |
| Repeated capacity checks inside the proof | 32,400 | 0 |
| Checked offset calls in the fallback | 0 | 0 |
| Observed ordinary C++ allocation requests | 7,432 | 7,432 |
| UTF-8 encodes | 201 | 201 |
| Owned byte snapshots | 200 | 200 |
| Successful native write calls and total bytes | 200 / 15,000 | 200 / 15,000 |
| Actual `fsync` calls | 1 | 1 |

The counters are from a separate instrumented binary. It counts visits in the existing proof and wraps the actual writes and `fsync` while forwarding the original calls and error values. Allocation accounting covers ordinary C++ new-family requests on the invoking thread. The primary timing binaries run with the observer disabled.

## Qualification

The affected Linux Release and fatal ASan/UBSan/LeakSanitizer CTests each passed 18/19. The native JIT logger suite stopped during fault injection in file acquisition at `Interpolation failed` in `io._error`. The exact focused test objects independently linked to the accepted G52 Runtime and to G54 reproduce that diagnostic and exit status. Both independently linked AOT logger suites passed in Release and the fatal sanitizer build. The retained failed test contains the application diagnostic and no sanitizer report. A separate native JIT cancellation run passed all 208 checks in Release and under fatal address, undefined-behavior and leak sanitizers.

Fatal TSan passed four Runtime and native-job CTests. The independently emitted normal and optimized AOT text fixtures also passed against the fatal TSan Runtime. They include the new bounded proof and fallback tests. Linux runs used actual hard and soft 1 MiB stacks.

Windows 10 x64 build 19045 Release passed 10 of 13 CTests. Passed coverage includes the native logger and its fault, partial transfer, reload and cancellation cases, both native AOT logger probes, and the translation and ownership fixtures. The separate cancellation replay passed as well. `log_files` hit WinError 1314 creating a test symlink. `io_execution` and `io_async_execution` hit their explicit check 91 for unsuccessful symlink creation. Linux Release passed all three fixtures. Debug passed all ten remaining checks. These included both JIT logger modes and both independently linked AOT logger probes. Both profiles verify the pinned Clang and LLD and eight executable PE headers with 1 MiB stack reserves.
## Paired Linux measurements

The observer-off run uses six paired blocks pinned to one logical CPU on the same Ryzen 7 1700X host. Variant order alternates and the workload order rotates. Each fixture warms up and checks three measured samples. Each reported change is the median of the six paired differences between sample medians. Negative means less time. File200 includes acquisition, record preparation, all writes, sync and close. The retained file output oracles compare complete ordered bytes and transfer counts. The timing run checks 6,048 observations across File200 and the controls.

| File200 mode | Wall ms, G52 → G54 | Paired wall | Paired process CPU | Faster pairs | Pair wall range |
| --- | ---: | ---: | ---: | ---: | ---: |
| Normal JIT | 11.661 → 11.486 | -0.96% | -3.63% | 3/6 | -4.99% to +2.88% |
| O2 JIT | 11.847 → 11.491 | -5.91% | -5.24% | 5/6 | -36.01% to +6.58% |
| Normal AOT | 11.393 → 11.434 | +0.17% | -1.59% | 3/6 | -1.93% to +2.54% |
| Optimized AOT | 11.117 → 11.172 | +0.74% | -1.11% | 3/6 | -1.25% to +55.19% |

Process high-water RSS includes JIT compilation and the complete process across all cases.

| File200 host mode | G52 RSS KiB, median | G54 RSS KiB, median |
| --- | ---: | ---: |
| Normal JIT | 174,306 | 174,504 |
| O2 JIT | 192,018 | 191,960 |
| Normal AOT | 34,232 | 34,332 |
| Optimized AOT | 32,044 | 32,048 |

The retained short and long delivery controls.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| 64-byte console ×200 | -4.65% | -1.69% | -2.15% | -0.73% |
| 1 KiB file ×100 | +0.64% | -0.44% | -0.41% | +3.98% |
| 65,500-byte file ×3 | +0.35% | -0.82% | +1.34% | +0.18% |
| Prepared direct file writes ×200 | +0.74% | -0.76% | +1.26% | +1.84% |
| File200 with periodic flushes | +1.38% | +0.97% | -1.26% | -0.61% |

The original framing and console controls include matched independent output oracles.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| 64-byte ASCII quote | -10.47% | -7.57% | -6.15% | -6.44% |
| 64-byte ASCII frame | -6.67% | -4.44% | -3.51% | -4.57% |
| 64-byte ASCII console | -4.73% | -4.99% | +0.72% | -3.61% |
| 1 KiB ASCII quote | -6.38% | -3.36% | +0.31% | -0.05% |
| 1 KiB ASCII frame | -2.11% | -2.85% | -0.38% | -1.84% |
| 1 KiB ASCII console | -6.99% | -3.37% | +3.14% | +0.13% |
| 1 KiB escaped quote | -1.51% | +0.41% | -1.28% | -0.55% |
| 1 KiB escaped frame | -1.33% | -2.79% | +0.85% | -0.26% |
| 1 KiB escaped console | -1.85% | -4.17% | +1.00% | +0.47% |
| 1 KiB Unicode quote | -1.30% | -1.50% | -0.32% | +0.17% |
| 1 KiB Unicode frame | +3.04% | +1.27% | +0.79% | +2.09% |
| 1 KiB Unicode console | +0.45% | -3.84% | +0.88% | +0.68% |

The independent mixed-workload guards use the G52/G53 source and answer oracles.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| nested-96 | -3.60% | +2.48% | +0.32% | +0.57% |
| typed-128 | -0.38% | +0.74% | +0.30% | +0.07% |
| dictionary-1024 | -1.76% | +2.00% | +1.90% | +0.72% |
| sort-1024 | +0.81% | -0.39% | +1.88% | -0.10% |
| array-1024 | -0.03% | -1.08% | -0.47% | +0.47% |
| array-4096 | -0.24% | -1.64% | -0.21% | -0.31% |
| integer-1000000 | -0.18% | -0.09% | +0.17% | +0.25% |
| integer-4000000 | -0.26% | -2.25% | +0.33% | +0.87% |
| concat-short | -1.12% | -2.57% | +1.25% | +1.34% |
| concat-1024 | -2.80% | -1.01% | +0.42% | -0.83% |
| concat-long | -0.49% | +0.97% | +0.95% | -0.21% |
| concat-unicode | -4.02% | -0.21% | +0.89% | -0.20% |
| join-short | -0.06% | -1.27% | +2.28% | -0.84% |
| join-1024 | -1.81% | -0.88% | +2.07% | -0.44% |
| concat-chain | -4.09% | +1.51% | -0.55% | +0.36% |
| private-256 | +0.75% | +1.08% | +4.06% | -0.09% |
| private-1024 | +0.59% | +2.57% | +0.16% | +0.19% |
| private-4096 | +0.15% | +0.55% | +1.09% | +1.64% |
| array-writes | +1.86% | -1.94% | +0.63% | +3.56% |
| write-256 | +2.54% | -2.45% | -1.26% | -0.51% |
| write-1024 | +1.71% | -1.41% | -0.99% | +0.71% |
| write-4096 | +2.26% | -1.48% | +2.01% | +0.93% |
| mixed-update | +0.86% | -2.01% | +2.25% | +1.66% |
| append-insert | +0.21% | -0.17% | +1.08% | +1.58% |

The next table shows the separate instrumented File200 runs. Entries are sampled elapsed scope medians in ms. The bounded proof is nested inside translation. Native writes are timed at the real write calls, and sync is timed at the real `fsync`. Encoding and snapshot copy use their Runtime function boundaries. Interpret each observed scope separately. In particular, adding the bounded proof duration to translation would count it twice.

| Measured scope | Normal JIT G52 → G54, ms | O2 JIT G52 → G54, ms |
| --- | ---: | ---: |
| Translation | 0.976 → 0.868 | 0.938 → 0.842 |
| Bounded proof within translation | 0.522 → 0.393 | 0.490 → 0.393 |
| UTF-8 encoding | 0.619 → 0.595 | 0.583 → 0.592 |
| Owned byte snapshot | 0.185 → 0.185 | 0.176 → 0.190 |
| Actual write calls | 0.458 → 0.440 | 0.419 → 0.441 |
| Actual fsync | 5.556 → 5.698 | 5.700 → 5.719 |

The 64-byte ASCII quote step takes 6.15–10.47% less elapsed time across the four modes. Full framing takes 3.51–6.67% less. The instrumented proof on File200 saves 0.129 ms in normal JIT and 0.097 ms in O2 JIT. Full File200 is 5.91% faster in O2 JIT with five faster pairs out of six. The other three full-wall medians move less than 1% in either direction. The process CPU medians improve in all four modes. The historical 10 ms aim remains open.

### Adverse results and replays

The primary optimized AOT File200 run has paired outliers up to +55.19% and a median of +0.74%. The prepared direct-write control has two increases over 118% in the same primary blocks. That control makes the same 200 native writes and one sync without calling translation. In the six subsequent optimized AOT diagnostic pairs, File200 wall time is −3.66%, process CPU is −2.04%, and that same sample's wall time outside the measured `fsync` is −0.97%. For the prepared direct control, one diagnostic pair has +102.08% wall and +121.77% actual sync time, with +6.76% wall outside sync. Both builds still make exactly one sync per File200 and direct sample. Optimized AOT has 7,446 observed C++ allocation requests for File200 on both sides. These observations show how sync duration can dominate individual comparisons on this XFS volume. The test measures the syscall boundary and leaves kernel and device attribution open. [XFS regular-file sync implementation](https://github.com/torvalds/linux/blob/master/fs/xfs/xfs_file.c).

The largest mixed guard increases are +4.06% in normal AOT indexed reads of 256 elements and +3.56% in optimized AOT array writes. An observer-off comparison of the same binaries, with eight isolated pairs, measures +0.58% and −1.49% respectively. The optimized AOT 4,096-element read control remains +1.09% in isolation after +1.64% in the mixed run. Preserve these results along with +3.98% optimized AOT wall for the 1 KiB file case, +3.04% normal JIT Unicode framing and +3.14% normal AOT 1 KiB ASCII console. These larger messages take the unchanged general translation path. The G52 and G54 normal AOT integer 4M control measures +0.33% in the mixed run. The O2 JIT chained-concatenation control measures +1.51%; its other three modes are listed above.

**Recommendation: advance G54 to integration review as a bounded Runtime CPU improvement.** The source reuses checked extents and improves short quoting and framing in all four measured modes. Full File200 has a clear O2 JIT improvement on this host and approximately flat median wall time across the other modes. Keep the adverse timings, the shared Linux JIT allocation-fault assertion and the Windows symlink coverage gap in the decision. No corrective source experiment was needed to isolate the reported tradeoffs.

The G54 evidence archive retains the exact G52 diff and verified postimages, the G52 and G54 Runtime identities, native and sanitizer receipts, the original fixture comparisons, all raw samples and the source restoration procedure. The G53 report and held source remain in their original separate archive.
