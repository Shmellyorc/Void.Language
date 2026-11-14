# G34-I — Corrected performance foundations integrated

Baseline: `9293084098383b12555f42c7d56a2799ca6e5f36`. **Production source now contains exact combinedF = B5 + C2 + D4 + E3, preserving accepted A.** Thirteen files match the archived candidate postimages. No optimizer, collector, heap, root authority, eligibility expansion, padding or alignment workaround was added. No commit, push or hosted CI was performed.

## Acceptance recommendation

**Accept combinedF with its documented array and GC-tail tradeoffs.** The mechanisms operate together correctly, and production-source qualification does not identify a component that must remain separate. This is a recommendation for review, not an automatic commit. Applications sensitive to the retained array/GC-tail costs still need workload-specific assessment; those costs are reported below and remain reproducible.

## Exact integration and identity

The archived DEFLATE-9 candidate was verified at SHA-256 `10b26a81708b7a6d2a958184e35fe94adf2c1aa91d13924c67b83ec7cecb35d3`, including CRC and every manifest member. The exact full patch was applied to the worktree after checking all 13 preimages and then checking all postimages. Source patch SHA-256 is `a47fd50e494cd0f19356e3067afebbbbfe72d4a19953ea2f3b4562f2026c7aa3`. The [retained exact source diff](/home/shmellyorc/Documents/Void.Language-archives/g34i-performance-foundations-integration-2026-11-14.patch) and archive contain every source identity.

The integrated build reads the real production worktree without source overlay bindings. All affected compiler/Runtime/test consumers are rebuilt after touching the affected production files. A fresh isolated combinedF replay uses the same toolchain and build group as a control. Runtime, LLVM-backend and Core archives are **byte-identical between isolated and production builds**. The integrated Runtime also matches the G34-F qualified Runtime archive. The accepted baseline Runtime reproduces SHA-256 `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`.

B5 keeps optional bounded payload/node reuse in the existing Heap, with one live registry and sanitizer poisoning. C2 retains block-maximum synchronous pointer homes and independent local/parameter/compound/native/async homes. D4 retains exact empty-frame omission, cold polling, constant-null TLS and the inlined generated-code poll path. E3 preserves one checkpoint/commit owner and returns surviving payload bytes from the existing sweep. Explicit/default host budgets remain fixed; source factories retain the qualified live policy, count cap 2×base and headroom `clamp(live/2, 64 KiB, 1 MiB)`.

Accepted A's owned helper matches the baseline by hash. Its concrete scalar, synchronous-block confinement proof, unsupported-alias/escape fallbacks, 256-byte object and 16-KiB function limits remain unchanged. No remaining observable failure or required poll/cancellation check is intentionally removed.

## Native qualification

Linux Release: freshly rebuilt baseline and integrated source each pass the maintained **40 selected CTests**, including normal/O2 JIT and independently linked normal/optimized AOT. Integrated fatal ASan/UBSan/LeakSanitizer passes the same 40 and native pool/pacing/race controls. Expected stale cached storage produces the required `use-after-poison` diagnostic. TSan passes Runtime coordination, race and live-pacing allocator controls. No sanitizer failure is suppressed.

Native Windows 10 build 19045, VS2022/SDK 10.0.19041.0 and LLVM/Clang/LLD 23.1.4: actual production-source exports, with zero source overlays, pass **Debug and Release 40-test suites**, native allocation/pacing controls, independent source oracles, normal/O2 JIT and independent AOT. Six PE headers per profile preserve the 1-MiB stack reserve. Linux qualification and measurement executions also use a 1-MiB stack. SDK/provider/helper hashes are checked before and after qualification.

The maintained assertions cover construction counts/effects/aliases, precise roots and rebinding, buffer/schema authority, post-rendezvous collection accounting, failure ordering and failed-GC retry, callbacks/native I/O, cleanup, workers/tasks/cancellation, async lifetimes, reload and cache processes. Unrelated applications retain independent result oracles. The E3 collector witness compares its returned live-byte sum with the independent public heap snapshot.

## Baseline versus integrated performance

Observer-off throughput uses four alternating blocks, three warm samples/cell; native pressure cases use five samples/block. Normal and O2 JIT, independent AOT, compilation and cold processes are separate. Numeric, managed-object, array/sort/dictionary, text/Unicode, JSON, native I/O, closures/calls, workers and async inputs/oracles are unchanged. Primary Linux timing has no overlapping build; the native VM is paused only at a compiler-build boundary or after native qualification completes. CPU affinity and flags match the qualified replay.

| Workload / mode | Baseline ms | Integrated ms | Change |
| --- | --- | --- | --- |
| array/1024 / aot | 1.226 | 1.175 | -4.2% |
| array/4096 / aot | 4.841 | 4.697 | -3.0% |
| dependent/1000000 / aot | 30.879 | 27.082 | -12.3% |
| dict/1024 / aot | 9.843 | 9.940 | +1.0% |
| float/1000000 / aot | 30.888 | 26.477 | -14.3% |
| integer/1000000 / aot | 31.119 | 29.409 | -5.5% |
| integer/4000000 / aot | 124.439 | 117.674 | -5.4% |
| io/200 / aot | 7.109 | 6.959 | -2.1% |
| json/128 / aot | 41.547 | 37.103 | -10.7% |
| json/32 / aot | 6.546 | 6.035 | -7.8% |
| sort/1024 / aot | 31.590 | 27.867 | -11.8% |
| sort/256 / aot | 6.236 | 5.655 | -9.3% |
| text/1024 / aot | 56.472 | 53.436 | -5.4% |
| text/256 / aot | 13.762 | 13.169 | -4.3% |
| calls / aot | 1.209 | 0.650 | -46.2% |
| managed-escape / aot | 9.586 | 7.675 | -19.9% |
| managed-local / aot | 0.658 | 0.557 | -15.3% |
| managed-survives-allocation / aot | 42.364 | 42.228 | -0.3% |
| async-tasks / jit-O2 | 1.640 | 1.698 | +3.5% |
| calls / jit-O2 | 1.343 | 0.737 | -45.1% |
| workers / jit-O2 | 0.818 | 0.624 | -23.7% |
| async-tasks / jit-normal | 1.637 | 1.690 | +3.2% |
| calls / jit-normal | 1.359 | 0.779 | -42.7% |
| workers / jit-normal | 0.824 | 0.643 | -22.0% |


The full raw/summary data include normal/O2 numeric and collection cases, ranges, CPU time, RSS and every unfavorable sample. The archived G31 Go reference is unchanged and was reused.

### Fresh isolated candidate versus production

| Workload | Fresh isolated F ms | Production ms | Difference |
| --- | --- | --- | --- |
| array/1024 | 1.190 | 1.178 | -1.1% |
| array/4096 | 4.694 | 4.677 | -0.4% |
| dependent/1000000 | 26.760 | 27.087 | +1.2% |
| dict/1024 | 9.823 | 9.866 | +0.4% |
| float/1000000 | 26.496 | 26.417 | -0.3% |
| integer/1000000 | 29.403 | 29.007 | -1.3% |
| integer/4000000 | 117.159 | 116.358 | -0.7% |
| io/200 | 6.990 | 6.976 | -0.2% |
| json/128 | 36.792 | 36.613 | -0.5% |
| json/32 | 5.849 | 5.805 | -0.8% |
| sort/1024 | 27.904 | 27.780 | -0.4% |
| sort/256 | 5.709 | 5.630 | -1.4% |
| text/1024 | 53.356 | 53.527 | +0.3% |
| text/256 | 13.074 | 13.152 | +0.6% |


This matched six-block control accompanies the byte-identical Runtime/LLVM/Core evidence. The compiler/cache identity difference is exactly reproduced by the per-variant ThinLTO cache-directory string; Runtime/LLVM/Core code is unchanged. The whole emitted suite object also matches the earlier qualified object after substituting only the equal-length physical source paths in diagnostic strings. No measured object was modified. Any measured layout/timing sensitivity remains visible; no source eligibility or code-placement flag was adjusted to force a favorable result.

## AOT arrays and GC tails

The prior array regression is retained explicitly even if the fresh primary run changes sign. G34-F recorded +4.9% for array/1024 and +4.0% for array/4096; isolated C2 controls and same-object relinks showed register-allocation/placement sensitivity. C2's array loop includes changed stack/register allocation, including a spill and extra moves. The earlier controlled relinks prove placement effects without assigning the entire residual cost to one spill or cache mechanism. The integrated source retains the exact qualified representation and ordinary linking. In this run the array comparison changes sign because the fresh baseline is slower (array/4096: prior baseline 4.489 ms, fresh baseline 4.841 ms), while the candidate remains near its prior result (4.668→4.697 ms). Prior/fresh baseline IR matches after diagnostic-path substitution; array instructions and sampled hot addresses match. The cross-run cause is not established. Fresh isolated-versus-production arrays differ by about 0.4–1.1%, so this is not evidence of an integration-specific correction. The historical +4–5% cost remains an acceptance risk rather than being erased by the fresh favorable comparison.

Full stopped intervals run from the ready rendezvous through epoch release, including policy work, immediately before mutex release. They do not repeat the old G34 partial observer that omitted the post-sweep policy scan. G34-F full-interval medians were text 764→822 µs and JSON 704→688 µs, with adverse p95 values 1025→1190 and 921→1021 µs. The new measurements are:

| Case / variant | N | Median µs | p95 µs | p99 µs | Max µs |
| --- | --- | --- | --- | --- | --- |
| text-1024 / base | 816 | 811.9 | 1126.8 | 1262.3 | 1477.3 |
| json-128 / base | 388 | 769.2 | 1054.8 | 1176.7 | 1288.9 |
| text-1024 / combinedI | 440 | 899.5 | 1276.3 | 1432.3 | 1538.6 |
| json-128 / combinedI | 192 | 882.1 | 1085.6 | 1129.8 | 1177.8 |


Nearest-rank p95/p99 and maximum are reported with sample counts; these desktop results are not hard latency guarantees. Full collection latency including rendezvous is also retained. Fewer collections can produce larger individual events. E3 removes the avoidable second table walk; it does not promise uniformly shorter pauses or a hard RSS bound. This tradeoff remains part of the acceptance recommendation.

## Allocation, roots and memory pressure

| Case | Heap constructions baseline → integrated | Native requests | Collections |
| --- | --- | --- | --- |
| managed-local-True | 0 → 0 | 0 → 0 | 0 → 0 |
| managed-escape-True | 20000 → 20000 | 40624 → 624 | 78 → 78 |
| managed-reference-True | 20000 → 20000 | 80468 → 40468 | 78 → 78 |
| managed-survives-allocation-True | 40000 → 40000 | 201248 → 161248 | 156 → 156 |
| text-1024 | 5233 → 5233 | 74327 → 51869 | 20 → 11 |
| json-128 | 2495 → 2495 | 65481 → 54316 | 10 → 5 |


Heap constructions include cache hits; native allocation requests are separate. Eligible A objects remain at zero heap constructions. Escaping/reference-bearing objects remain real heap constructions. Warm reuse, cold failures/retry and cache poisoning remain asserted.

| Case | Root words baseline → integrated | Checked offsets baseline → integrated |
| --- | --- | --- |
| text-1024 | 7226 → 2587 | 1776775 → 423999 |
| json-128 | 7930 → 2607 | 4082612 → 140412 |


Integrated root totals reflect both descriptor representation and fewer paced collections. The maintained frame probe verifies one frame enter/leave rather than 1001, with 5003 polls and one construction unchanged in normal/O2 JIT; the accepted baseline probe is rebuilt with its own matching ABI.

| Native pressure | Baseline → integrated wall ms | CPU ms | GC epochs | Live payload bytes |
| --- | --- | --- | --- | --- |
| small | 3.728 → 2.779 | 3.697 → 2.755 | 78 → 78 | 528 → 528 |
| mixed | 4.665 → 3.421 | 4.628 → 3.383 | 78 → 79 | 8848 → 23056 |
| large | 10.564 → 3.735 | 10.384 → 3.692 | 3 → 1023 | 16842752 → 131072 |
| retained | 6.834 → 6.126 | 6.783 → 6.078 | 78 → 40 | 8704 → 8704 |
| threads | 18.293 → 16.056 | 44.401 → 38.031 | 77 → 77 | 3960 → 3936 |


The byte/live policy is a soft growth allowance. Large requests and concurrent NativeSafe preparation can overshoot it; requested-byte epoch peaks omit pre-existing/reused storage. They are not global physical-memory peaks. Native cache bounds remain 16 classes, 256 blocks/class through 256 bytes: 557,056 payload bytes plus 98,368-byte Data and bounded node/reference-map metadata on Linux. Cache storage stays outside the live registry. Heap remains 120 bytes, Runtime 1008→1048, Frame 40, Slot 32, with pre-existing hot-field offsets preserved. D4/E3 add no persistent state beyond the previously qualified B/E tail; E3's sum is local to the sweep.

## Compilation, startup and process memory

| Operation | Baseline ms | Integrated ms | Change |
| --- | --- | --- | --- |
| cold / aot | 4.979 | 5.039 | +1.2% |
| cold / jit-normal | 3107.150 | 3071.151 | -1.2% |
| cold / jit-O2 | 3832.160 | 3750.984 | -2.1% |
| compile / compile-link | 3687.028 | 3627.438 | -1.6% |


The separate 24-pair cold-AOT median is 4.974→4.962 ms (-0.2%). Cache initialization/retention and small startup costs remain reported. Native compiler rebuild time is not substituted for source compile/link time.

| Process | Baseline median peak KiB | Integrated median peak KiB |
| --- | --- | --- |
| g31 / aot | 13676.0 | 13678.0 |
| g31 / jit-normal | 127440.0 | 128120.0 |
| g31 / jit-O2 | 127480.0 | 125990.0 |
| native / None | 21830.0 | 13728.0 |
| compile / None | 122888.0 | 120760.0 |


Peak RSS includes compiler/JIT state where applicable and is separate from managed live payload and bounded cached storage. Ranges and artifact/code sizes are retained with identities.

## Diff, ledger, replay and storage

The production candidate file inventory and postimage SHA-256 values are:

- `CMakeLists.txt` — `fdc3dabb0de53e65fbd04128d5bb72aa29e8afbd8787043f93473ccb244f7e03`
- `include/void/gc_schema.hpp` — `39542c2831116a838fd181e02a632e78c67d89285b7251e20280a6232c8d5bfc`
- `include/void/heap.hpp` — `23b908d9fcb7bf544c49ee845bafbba4e23370b3d2b6287e9e4a76a95d474e4e`
- `include/void/runtime.hpp` — `4207db589ebf499a757562c6ee2fec2c3c7814b227da6644130a3f524e2d1afb`
- `src/backend/llvm/backend.cpp` — `f28c9907c43ca74492a18e0bd8c1d0fc89e6a36738f3cf75b4ebf1da1182d2ee`
- `src/runtime/abi.cpp` — `5da286ba05ecd290c1b9833a25fb770300823d804d9b8a32783d25909979c686`
- `src/runtime/buffer.cpp` — `f9ee7ca189f2e938664eba09e8de94c64f129a8414f2fc7fd58a466c554b19fd`
- `src/runtime/coordination.cpp` — `348069f9b1acaab813827519e2a7ed2fac5f9a04331c4817b4a26abe40d2ffc6`
- `src/runtime/heap.cpp` — `96d7c03ca053e699b95437034d225c4aee9c05a3c6bafa3a3475ae80413d0f83`
- `tests/allocation_elimination/aot.cpp` — `07bbe9b27bbcaf1fa8bded4e08fb0d3f3d37dc2168110f418056a5c0d8aaf3ec`
- `tests/allocation_elimination/check.hpp` — `0cbe90984faead36effbfc3f9b0add6d03867cac5a4bda9f38af02e6efb98e20`
- `tests/allocation_elimination/tests.cpp` — `27b1ab13718f31e71d545a5840fd24f4fddd9a934885f257575dbcd5997e6bed`
- `tests/runtime/buffers.cpp` — `17cf097d76a2c4b060b22736fbe0279c3bc5239a31b892c6d15b033d6b822c7f`

The permanent [implementation ledger](/home/shmellyorc/Documents/Void.Language/docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md) distinguishes accepted/integrated A and B–E from broader escape work, which remains active and unimplemented. G32/G33 research reports already tracked at the baseline are preserved by hash. Prior G34 evidence and its unfavorable results remain unchanged. Pre-existing uncommitted reports remain in the final Git inventory.

The [integration archive](/home/shmellyorc/Documents/Void.Language-archives/g34i-performance-foundations-integration-2026-11-14.zip) retains selected source/postimages, exact diff, inputs/oracles, identities, essential raw results, significant failures, IR/disassembly and replay instructions. It excludes full build trees, SDKs, provider installations, caches and routine intermediates. Adjacent DEFLATE-9/manifest/SHA-256 verification and cleanup receipts record retained files, archive bytes, observed reclamation, consumer checks and protected installations. The archived report is the qualification snapshot; external closeout records final cleanup and inventory.

## Verified archive and cleanup closeout

The DEFLATE-9 archive retains **255 selected evidence files plus one manifest**, 3,032,603 compressed bytes; SHA-256 `3688231f238ed57ae8bd3f62e501485f4c149a8a5ad193fac6368cbfb75e7ad1`. CRC, every extracted/source SHA-256 and a fresh source replay passed. The [verification receipt](/home/shmellyorc/Documents/Void.Language-archives/g34i-performance-foundations-integration-2026-11-14-verification.json) and [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g34i-performance-foundations-integration-2026-11-14-cleanup.json) remain adjacent to the archive.

Linux disposable Release, fatal-sanitizer and TSan groups, frozen consumers and task transport are removed. Observed free-space reclamation across the recorded Linux phases is **5,306,134,528 bytes** (4.942 GiB); per-phase logical/allocated sizes are recorded separately. Native Windows Debug/Release build groups and the private task root are also removed; observed Windows reclamation totals **1,676,259,328 bytes** (1.561 GiB) across the profile and final-removal observations retained in its receipt. Free-space observations can include unrelated filesystem activity. Open-consumer checks passed; protected SDK/provider/toolchain hashes, exact production postimages, previous archives and unstaged Git index were verified after cleanup. No commit, push or hosted CI occurred.
