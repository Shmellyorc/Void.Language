# G31-R — VOID versus Go: native Linux performance baseline

13 November 2026. Research baseline **189be08423bdc2beb91183b0608b018190dc7524**. **Recommendation: investigate the no-collection-request cost of existing Runtime polling; no optimization is implemented or promoted.** This is a local baseline and an engineering handoff for Golden Wrench/Boss review, not a release gate or a cross-platform claim.

VOID already competes effectively on a standalone, automatically vectorized numeric kernel. Its ordinary managed application context is much slower, even for an arithmetic helper with no allocations inside its loop. The strongest attribution combines current source, emitted code, a minimal managed-context control and a direct probe of unchanged Runtime calls. Collections, text and JSON also have consequential gaps; their costs are not completely partitioned. Durable checked I/O is much closer to Go. No aggregate score combines these very different workloads.

## 1. Identities and boundaries

| Item | Measured configuration |
| --- | --- |
| VOID | Existing `build/llvm23-release/voidc`; v0.1.0 development; LLVM 23.1.4; x86_64-unknown-linux-gnu |
| Compiler SHA-256 | `8a3f3fdbdf2ff2b84a51640870a9f0de1633cb2898bd5c080d0fbc5dc222b28b` |
| Compiler build identity | `4abda349c62caf231abda0f8bb49afd1d0d4f69763aae47f252e01b433d713c7` |
| LLVM/Clang/LLD | 23.1.4, upstream revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`; existing pinned SDK |
| Go | Official Go 1.27.4 linux/amd64 archive; default compiler optimizations; `-trimpath`; `GOAMD64=v1`, `CGO_ENABLED=0`, `GOTOOLCHAIN=local`, `GOMAXPROCS=1`, `GOGC=100`; no memory-limit override |
| Clang reference | Numeric C only; `-O2 -march=x86-64 -mtune=generic -ffp-contract=off`; no fast-math |
| Host | AMD Ryzen 7 1700X, 8 cores/16 logical CPUs, SSE2/AVX2/FMA capabilities; PikaOS 4; Linux 7.2.11-pikaos; native x86-64 |
| Memory | 32,771,920 KiB physical; approximately 9.0 GiB available at initial capture; existing swap use. Shared desktop, not an isolated lab |
| Scheduling | Every timed child pinned to logical CPU 2; governor `performance`, boost enabled; no privileged scheduler changes |
| Stack | Child soft and hard native stack limits 1,048,576 bytes. This does not constrain Go's dynamically growing goroutine stacks to the same limit |
| Filesystem | Actual I/O outputs on `/dev/sda3`, XFS, under a private task-owned directory in `~/.local/state`; not `/tmp` tmpfs |
| Not measured | Rust absent; no additional language setup. No new Windows/macOS campaign, Debug/sanitizer performance claim, task/channel/network throughput or buffered-I/O comparison |

Go was absent initially. Its task-local SDK came from the [official downloads](https://go.dev/dl/), archive `go1.27.4.linux-amd64.tar.gz`, 70,680,823 bytes, SHA-256 `7226736c2339719cda28fbf3a54d71aa63bcefe376e98cef00034268dab24840`. The exact download manifest, local Go source hashes, compiler hashes, CPU flags, OS, environment and commands are archived. No global toolchain was installed.

All primary VOID runs use the existing compiler and Runtime unchanged. Normal JIT means `run --no-cache --no-opt`; O2 JIT means `run --no-cache`; AOT means an independently linked `voidc build` executable. Production Runtime allocation budgets remain **256** (`src/runtime/abi.cpp:122`, `src/backend/llvm/backend.cpp:2547–2548,2682–2683`). There is no GC-disable optimization. Active builds, providers and toolchains are preserved.

## 2. Suite and fairness

Four process blocks alternate forward/reverse language order. Each process warms each case once, then records three wall-time samples: **12 samples per case/mode, 720 primary timed samples total**. Four VOID/Go modes cover all 14 cases; Clang covers four numeric cases. Each case's setup/allocations listed below is inside its timer. Input-file loading, timer-result printing and independent post-write file inspection are outside. Compilation is excluded from warm timers; process totals explicitly include it for JIT.

Inputs are runtime-sized, results are consumed, and Python computes independent checksums. JSON output is compared byte-for-byte with its input; I/O checks every 75-byte acknowledgement and verifies all 15,000 bytes afterward. Text checks round-trip join, counts and encoding lengths in timed execution; a separate untimed 256-unit oracle verified full payload/join/replacement/hex/Base64 output against Python in normal JIT, O2 JIT and linked AOT. No failed sample was dropped. Main-process stdout/stderr and all raw timings remain available.

| Family / cases | Work and output | Equivalence limitation |
| --- | --- | --- |
| Integer, 1M / 4M | Guarded positive range; i32 sum of `i % 127`, returns i64; sums 62,999,875 / 251,999,524 | No signed overflow for selected inputs; C unsigned accumulation agrees in this range |
| Dependent, 1M | u32 LCG recurrence, seed 1, wraps; final 366,300,225 | Dependence is real, but compilers may legally compose this affine recurrence; it is not a prohibition on unrolling |
| Float, 1M | f64 dependent recurrence, final scaled integer 1,604,294,581 | Exact selected checksum; not evidence about every floating-point edge case or transcendental |
| Growable array, 1,024 / 4,096 | Allocate empty list, append `i % 127`, iterate and sum | VOID identity, checked root/metadata and iterator behavior differ from Go slices; allocation and growth included |
| Stable sort, 256 / 1,024 | Generate data, outer snapshot, bottom-up index merges, two index arrays, fresh materialization, weighted sum | Go ports the VOID algorithm, not Go sort. Go direct comparison differs from VOID's generic callback/effect machinery; this is not isolated comparator timing |
| Dictionary, 1,024 | Fresh i32→i32 map, insert `3*i`, checked successful lookup, sum | No iteration; VOID preserves insertion order and additional ownership guarantees; implementations/hash policies differ |
| Unicode text, 256 / 1,024 units | Repeat `a雪` (1 / 4 KiB UTF-8), split/join, replace with 😀, hex and Base64 | **Informative application comparison**: Go substrings may share storage; VOID owns independent fields and checks Unicode/budgets/errors. No claim of equal invalid-input or ownership semantics |
| JSON, 32 / 128 fields | Parse canonical sorted ASCII keys and integer values; stringify to exact original bytes (311 / 1,299 bytes) | **Informative application comparison**: Go `map[string]json.Number` + UseNumber + Marshal; VOID dynamic DOM, typed conversion/metering and duplicate policy differ. Go's sorted output matches this deliberately sorted input |
| Durable I/O, 200 | Open/truncate, 200 separate checked 75-byte writes, final real sync, close | Prepared record outside timer in both; success path matched, failure contracts differ. **Not logger FILE200**: no formatting, rotation or logger delivery machinery |

This reuses the algorithms/corpus and practices from `tests/text_encoding/performance.void`, `tests/collections_stdlib/benchmark.py`, `tests/json/measure.py`, `tests/numeric/automatic_simd/guarded-range-i32.void`, and existing channel/IR research hosts. It is a disposable launcher and fixtures, not another installed benchmark framework. Searching, sustained allocation pressure, general fixed-array workloads, HTTP and channel concurrency are explicitly deferred. Go concurrent channels are not substituted for VOID's synchronous router.

## 3. Primary warm results

Milliseconds, **median [minimum–maximum]**. All observations retained, including Go allocation/sort outliers and the VOID I/O sample above 10 ms. Min/max are descriptive ranges, not confidence intervals; 12 samples are clustered in four processes, not 12 independent machines.

| Case | VOID AOT | Go | VOID O2 JIT | VOID normal JIT | Clang C |
| --- | ---: | ---: | ---: | ---: | ---: |
| integer 1,000,000 | 70.9675 [69.3960–73.3522] | 1.1083 [1.0317–1.2204] | 92.6282 [88.3597–97.4991] | 92.9893 [89.9609–98.9189] | 0.3996 [0.3776–0.5511] |
| integer 4,000,000 | 276.9599 [274.8729–296.4253] | 4.1290 [4.0598–4.1682] | 370.4046 [356.2655–391.5648] | 367.7728 [356.9474–394.5415] | 1.5893 [1.5308–1.6275] |
| dependent 1,000,000 | 68.9329 [67.2366–71.3662] | 1.1839 [1.1735–1.2059] | 90.6738 [87.7716–92.7396] | 96.2914 [92.7183–97.7563] | 0.1439 [0.1429–0.1526] |
| float 1,000,000 | 68.7406 [68.2237–71.7152] | 2.0747 [2.0436–2.1004] | 93.4776 [87.8789–97.5144] | 90.7178 [88.7591–92.9097] | 2.0426 [2.0116–2.0745] |
| array 1,024 | 1.3622 [1.3129–1.3879] | 0.0044 [0.0032–0.0133] | 1.3589 [1.3087–1.4612] | 1.4181 [1.3373–1.4495] | — |
| array 4,096 | 5.3639 [5.2264–5.4800] | 0.0200 [0.0167–0.2601] | 5.3523 [5.1909–5.6955] | 5.5771 [5.3226–5.7424] | — |
| sort 256 | 6.5628 [6.4686–6.6462] | 0.0094 [0.0086–0.0223] | 6.8616 [6.7333–7.1441] | 6.9441 [6.8420–7.1687] | — |
| sort 1,024 | 32.6540 [32.0104–34.1705] | 0.0437 [0.0391–0.1943] | 33.9775 [33.3456–35.3841] | 34.1555 [33.7307–34.3732] | — |
| dict 1,024 | 10.0793 [9.8856–10.2241] | 0.0798 [0.0761–0.0927] | 13.0415 [12.8826–14.8361] | 13.0837 [12.8733–13.3779] | — |
| text 256 | 14.8233 [14.4228–15.2181] | 0.0224 [0.0214–0.0235] | 15.8749 [15.0585–26.4558] | 15.9086 [15.4778–16.4693] | — |
| text 1,024 | 61.4839 [59.8114–62.4508] | 0.0890 [0.0848–0.0948] | 65.5933 [63.6059–78.3652] | 65.6052 [64.1023–68.5758] | — |
| json 32 | 7.0004 [6.7372–7.1429] | 0.0319 [0.0294–0.0394] | 7.8146 [7.3763–7.9724] | 7.7483 [7.4823–8.1691] | — |
| json 128 | 45.2639 [43.6350–46.2285] | 0.1146 [0.1113–0.1596] | 50.6688 [48.8777–51.5128] | 49.7898 [48.7149–51.6486] | — |
| io 200 | 7.0053 [6.6626–10.3250] | 6.1073 [5.7918–8.6949] | 7.2919 [6.9057–10.2447] | 7.1124 [2.9276–23.8595] | — |

The coordinated arithmetic table must not be presented as VOID's standalone arithmetic ceiling. Section 5 isolates that difference. The I/O distributions overlap: the median difference is measurable in this cohort, but storage variability prevents promising a universal sub-millisecond language advantage. No historical FILE200 threshold is imported here.

Selected AOT/Go throughput, computed from these medians (decimal MB; not an extra independent measurement):

| Work | VOID AOT | Go |
| --- | ---: | ---: |
| integer 4,000,000 | 14.443 million iterations/s | 968.756 million iterations/s |
| array 4,096 | 0.764 million elements/s | 205.108 million elements/s |
| sort 1,024 | 31359.124 elements/s | 23424721.774 elements/s |
| text 1,024 | 0.067 input MB/s | 46.041 input MB/s |
| json 128 | 0.029 input MB/s | 11.339 input MB/s |
| io 200 | 28549.951 records/s | 32747.476 records/s |

Array AOT time grows 3.94× for 4× elements; text grows 4.15× for 4× input; sort grows 4.98× for 4× input, consistent with increasing merge work. JSON grows 6.47× for 4× fields in this range. These two-point observations do **not** establish a quadratic algorithm. In particular, `text.split` uses a prefix table and advancing search cursor (`stdlib/text.void:328–352`); the previous investigation already established its linear matching algorithm.

## 4. Compilation, startup, memory and allocation

| Build operation | Elapsed seconds | Interpretation |
| --- | --- | --- |
| VOID full AOT build, three runs | 3.618 / 3.607 / 3.642 | Source pipeline and native link; existing installed dependencies |
| Go first build | 14.971 | Empty task-private package cache, including standard-library build work |
| Go unchanged repeat builds | 0.068 / 0.071 | Cache-hit builds; **not** equivalents of fresh VOID compilation |
| Go edited-source builds | 0.279 / 0.304 / 0.276 | Same program with distinct inert source comments; dependency cache warm, application source cache miss |
| Clang numeric-only build | 0.100 / 0.106 / 0.104 | Much smaller library/source scope; not a whole-suite compiler ranking |

Fresh-process factor-zero startup includes argument handling and `cold` output, but skips the workload. JIT still compiles the application/import graph. `--no-cache` disables VOID's code cache; OS page caches were not evicted. Python launch/reap overhead is included and materially affects the few-millisecond native rows.

| Mode | Startup ms median [min–max] | Full suite process wall / CPU seconds (median) | Largest measured process RSS KiB |
| --- | ---: | ---: | ---: |
| void-aot | 4.875 [4.792–5.077] | 2.7704 / 2.7269 | 12,848 |
| go | 3.413 [3.368–3.543] | 0.0672 / 0.0404 | 12,824 |
| void-jit-O2 | 3828.997 [3754.639–4530.088] | 7.3287 / 7.2448 | 129,676 |
| void-jit-normal | 3242.202 [3112.055–3741.966] | 6.5652 / 6.5136 | 129,080 |
| clang | 4.813 [2.515–10.330] | 0.0193 / 0.0182 | 12,772 |

CPU/RSS use `wait4`, not a profiler in timed bodies. Full-suite totals include warmups, printing, validation and process initialization; JIT totals also include compilation. They are **not per-case warm CPU measurements**. Clang's process runs only numeric cases, so its total is not comparable with the complete suite. The Python `preexec_fn` launcher produces an approximately 12–13 MiB RSS floor for small children; those rows cannot rank native application memory. JIT peaks exceed that floor but still describe compiler+execution, not the application alone. No leak or equal-memory conclusion follows.

Unstripped build-output sizes: VOID suite **5,005,880 bytes**; Go suite **3,972,481 bytes**; numeric-only C **20,432 bytes**. Defaults differ in debug/link metadata and included Runtime/library code; no stripped-distribution size claim.

Separate Go `ReadMemStats` observations, outside the primary timed campaign: numeric kernels 0 heap allocations; arrays 1,024/4,096: 8/11 allocations, 12,864/49,728 bytes; sort 256/1,024: 23/35, 13,056/80,128 bytes; dictionary: 20, 38,056 bytes; text 256/1,024: 8/8, 14,848/59,392 bytes; JSON 32/128: 83/309, 6,936/27,416 bytes; I/O: 3, 168 bytes. Each individual observation reported zero collections. This is not a no-GC configuration and does not establish zero collections in every primary sample. VOID per-case allocation/collection counts were not instrumented; they cannot be inferred from Go's counts or from process RSS. See the [Go GC guide](https://go.dev/doc/gc-guide) for the distinction between allocation, live heap and collector work.

## 5. Code generation and bounded attribution controls

The accepted AOT-only target-aware pipeline remains intact: `src/backend/llvm/backend.cpp:86–107` constructs O2 with an optional TargetMachine and explicitly disables SLP; JIT call sites at 2630/2772 do not pass the AOT machine; AOT at 2995/3015 does. No fast-math, JIT target-analysis restoration or checked-array specialization was attempted. The previous cold-start regression remains historical evidence against indiscriminately restoring the rejected global candidate.

An isolated copy of the backend added **only post-pipeline IR printing**. Its host/compiler build was outside the measurements; no optimization settings were changed. Its pure emitted object matches production `voidc emit` **byte-for-byte**. Archived IR and both object/executable disassembly show:

- Standalone guarded i32 sum: AOT `<4 x i32>` vector body/reduction; x86 instructions include `paddd`, `pmuludq`, `psrld`. This is generic x86-64 SSE2, not AVX2 chosen from the host's capabilities.
- Same standalone O2 JIT sum: scalar IR under the preserved JIT pipeline.
- Main managed suite's numeric helpers: scalar loops with `vd_gc_poll` and `vd_worker_checkpoint` calls. The integer helper has **four pairs per iteration**, plus entry/exit work. Those side-effecting calls prevent ordinary loop-vectorization treatment.
- Growable-array reads/counts/pushes remain checked Runtime calls, not direct vector memory operations (`backend.cpp:1028–1097`; `src/runtime/list.cpp:41–64`). No generalized checked-array SIMD claim.
- C's dependent u32 recurrence is much faster than Go's despite its dependency. Legal affine composition/unrolling is a relevant explanation, not proof that a dependent recurrence vectorized. The f64 recurrence remains a scalar control. Selected C and Go disassemblies are retained.

LLVM's [vectorizer documentation](https://llvm.org/docs/Vectorizers.html) explains target cost decisions and legal reductions; the claims above come from this compiler's actual emitted code, not assumptions from documentation.

A second bounded control uses the maintained guarded-range kernel unchanged. A minimal companion adds a managed string in its entry and calls that same numeric kernel. Twelve samples each, four balanced process blocks; no IR printing inside the timed runs. AOT uses a native host calling the production-emitted checked kernel; only its generated `main` symbol is renamed to link the host. This is kernel execution, not ordinary application startup.

| Control, 1M iterations | Median ms [min–max] |
| --- | ---: |
| aot | 0.4053 [0.3957–0.4133] |
| jit-O2 | 1.0767 [1.0749–1.0878] |
| jit-normal | 3.0059 [2.9994–3.0372] |
| coordinated-jit-O2 | 81.4751 [80.4273–82.0631] |
| Go, supplemental matched block | 1.1103 [1.0395–1.1316] |
| Clang, supplemental matched block | 0.4043 [0.3954–0.4999] |

The main Go/Clang references are 1.1083 / 0.3996 ms; the supplemental balanced control also reruns their numeric cases and preserves all rows. The pure VOID AOT result is competitive with Clang and faster than Go on **this kernel**. It does not predict managed-program performance. The coordinated control is approximately 81 ms despite no allocations in the arithmetic loop; this materially narrows the diagnosis.

The exact Runtime path is `vd_gc_poll` (`src/runtime/abi.cpp:311–313`) → `Runtime::poll` (`src/runtime/coordination.cpp:136–140`). Every poll validates Managed state and takes `mutex_`, even when no collection is requested. `vd_worker_checkpoint` (`src/runtime/async.cpp:655–659`) separately checks thread-local worker state. Backend insertion is per IR block (`src/backend/llvm/backend.cpp:919–926`), not just allocating instructions or loop backedges.

A final direct host probe calls the **unchanged existing functions**, four million times, with an attached Managed Runtime, production allocation budget 256, no managed allocations in the loop, and normal worker checks. Four alternating blocks, one discarded warmup and three samples per mode. Runtime epoch and park deltas were zero in every sample:

| Unchanged-call probe | Median ms [min–max] |
| --- | ---: |
| Worker checkpoint only | 8.2146 [8.2005–8.3722] |
| GC poll + worker checkpoint | 70.7643 [69.5656–72.5390] |

These are standalone call costs, **not additive slices of the primary suite**. They strongly support coordination/polling overhead, rather than actual GC pauses, as a major cause of the numeric gap. They do not distinguish every instruction inside `poll`, prove that mutex acquisition is its entire cost, or establish a future atomic-path speedup. The initial probe used the default host budget 0; it was preserved and repeated at production budget 256 to avoid presenting a different collection policy as the final control. Neither loop allocated; both reported zero epochs/parks. No production prototype was patched.

## 6. Responsible paths and confidence

| Finding | Current evidence and limits | Disposition |
| --- | --- | --- |
| Coordinated scalar-loop overhead | Source block insertion, actual scalar IR/calls, managed/pure contrast, direct unchanged-call measurement agree. High confidence in substantial polling cost; exact whole-program exclusive share not profiled | Strongest justified next target |
| Array/list cost | AOT 5.36 ms to append/sum 4,096 elements vs Go 0.020 ms median. `list_count`, `list_read_index`, `list_push` perform checked schema/header/root operations; `array.cpp:16–31` validates backing layout; growth uses bounded builders | Real end-to-end gap; individual validation/allocation/GC shares unknown. Preserve checks; do not declare them all redundant |
| Sort cost | 32.65 ms for 1,024; `stdlib/collections.void:66–105` is stable index merge with snapshot and callback. Go ports the merge, but direct comparison avoids VOID callback/effect machinery and checked index bridges | Not a reason to replace the sort algorithm before shared per-operation costs are measured |
| Dictionary cost | 10.08 ms for 1,024; `src/runtime/dictionary.cpp:76–91` includes metadata, poll, lock, key/header validation, hash lookup and growth/publication | Hash lookup already exists; do not mislabel it a linear dictionary. Different ordering/ownership semantics limit competitive attribution |
| Text cost | 61.48 ms for 4 KiB multi-operation workload; `stdlib/text.void:328,406,468,485`, `stdlib/encoding/hex.void`, `stdlib/encoding/base64.void` execute checked source loops/builders | Gap proven; valid-input Go comparison is informative. Linear matching is already implemented. Independent fields and byte-array conversions remain real work |
| JSON cost | 45.26 ms for parse+stringify 128 fields; `stdlib/json.void:602,1139` and `_json_to_value` before `_write` retain conversion/budget work | End-to-end gap proven; fresh conversion, checks and ordered representation differ from Go's map. No measured exclusive “clone cost” here |
| Native I/O | AOT 7.01 vs Go 6.11 ms median with matched 200 writes and real final sync | Relatively close and storage-sensitive; no durability change or logger optimization justified by this fixture |
| JIT startup | Roughly 3.2–3.8 seconds before the zero-work application's exit in this import-heavy suite | Real developer-latency cost, but not warmed execution. Do not multiply pure-kernel compile times into a whole-app explanation |

Existing checked-root tokens, canonical empty-layout metadata reuse and shared headers remain present (`coordination.cpp:213–230`, `buffer.cpp:83–100`, `list.cpp`, `dictionary.cpp`); they match the integrated repair record. This investigation does not rediscover those repaired costs as unfixed. No root-validation deletion, GC-frequency reduction, JSON metering bypass, source representation rewrite or scheduler replacement is justified by the present attribution.

## 7. Smallest coherent next assignment

**LIMITED GO for a disposable no-request polling optimization, only after review.** Investigate making `Runtime::poll()`'s common no-request path avoid an unconditional mutex while retaining Managed-state validation, every existing compiler safepoint and worker-cancellation check. The concrete scope is `include/void/runtime.hpp` request state, `src/runtime/coordination.cpp` request publication/clear/poll protocol and directly affected coordination tests. No public language/API change is needed. This is a candidate architecture, not an already proven safe patch.

A race-free atomic request indicator with the current locked, rechecking park path is the smallest plausible direction. A plain unlocked read of current `bool requested_` is a data race and is unacceptable. Before implementation, explicitly prove request publication, request completion, successive epochs, timeout, concurrent collectors, native→Managed entry, attachment/detachment and shutdown behavior. A request racing just after the fast check must still see the participant as Managed and wait for its next preserved poll; no roots may be scanned while that participant runs. Preserve object/layout ABI and rebuild dependent code/cache identities when required. Do not replace one translation unit against old headers and assume layout compatibility.

The established comparison is the principle of cheap common-path safepoint detection with a separate slow path. Go's [actual preemption source](https://go.dev/src/runtime/preempt.go) uses stack-bound checks for synchronous preemption and OS-supported asynchronous preemption. Its machinery is **not a drop-in VOID implementation**; do not import signal scanning or different root semantics. VOID's precise registered roots and coordination contract remain authoritative.

Validation should reuse `tests/runtime/coordination.cpp`, existing worker/cancellation/root/lease/native-callback tests and the archived kernels. Exercise pending requests, timeout, repeated epochs, native transitions, concurrent callbacks, collection and shutdown under existing fatal sanitizer settings. Then compare baseline/candidate with matched complete-suite ordering, pure controls and cold compilation/startup. Include one worker-cancellation workload and requested-collection progress control: a no-request benchmark alone cannot qualify the repair. Verify normal/O2 JIT and independently linked AOT on real 1 MiB native stacks. Native Windows/macOS acceptance must follow before any cross-platform claim; no hosted CI is requested here.

Stop/reject on a race, missed collection/cancellation, changed failure order, root/lifetime violation, material representative regression or inconclusive benefit. No invented percentage gate. Even a successful fast poll leaves external calls and worker checks in the loop: it **does not by itself unlock managed-loop SIMD or promise Go-equivalent throughput**. The roughly 70 ms current call probe is an investigation budget, not a promised saving. Do not simultaneously alter poll frequency, SLP, JIT TargetMachine policy or checked-array lowering; those would invalidate this focused attribution.

## 8. Reproducibility, failures and remaining uncertainty

The archive contains exact final VOID/Go/C fixtures, both JSON inputs, independent oracles, all primary and supplemental outputs, commands/settings, download/version manifests, emitted IR, disassembly, observation-only source/host and build recipes. Main sampling lives in `run.py`; codegen controls in `capture-build.py`, `capture-run.py`, `controls.py`; direct current-call attribution in `poll-probe.py`; untimed byte verification in `text-oracle.py`. `REPLAY.md` gives ordering and path setup. Binaries/SDK/cache are reproducible and removed after verified archiving.

Preserved authoring failures: the first VOID fixture shadowed imported `io.file` with a `with` binding and mixed i64 seconds with u32 nanoseconds without a cast. Existing diagnostics rejected both; the final fixture uses `handle` and an explicit cast. The original source/diagnostic remain. `/usr/bin/time` was unavailable; the first launcher stopped before invoking the compiler, then used standard Python `wait4`. These are not compiler/runtime crashes. The original generator and failed driver remain; the replay generator incorporates the source corrections. Every completed primary process and correctness oracle passed; no abnormal compiler/runner exit occurred and therefore no incident `_crash.void` was required.

Limits: this is one shared Ryzen/Linux host, Release compiler, bounded sizes, valid-input cases and no exclusive sampling profile. Fast Go operations show noisy maxima. Allocation observations are asymmetric; RSS has a launcher floor; general fixed-array workloads, unaligned vector buffers, network/channel and other platforms are not qualified. No claim that all overhead is fixable or that the standard libraries provide identical semantics. No source optimization was tested, so there is no candidate regression/promotion verdict. Historical LLVM19, earlier text and logger numbers remain historical and are not merged into these results.

## 9. Evidence retention and workspace closeout

Verified archive: [g31-void-go-performance-baseline-2026-11-13.zip](/home/shmellyorc/Documents/Void.Language-archives/g31-void-go-performance-baseline-2026-11-13.zip) — **201 source/evidence members**, 25,920,852 original bytes, **3,525,067 ZIP bytes**, DEFLATE level 9. SHA-256: `dc945f5d1c193cf3d5aae582f1407800ebeb3c12dac53021ab33d920853263cb`. Embedded manifest records original paths and original/member SHA-256. CRC passed, every member was actually extracted and hash-checked, and selected originals were rehashed before cleanup. The existing external `archive-index.json` records this archive; the [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g31-void-go-performance-baseline-2026-11-13.cleanup.json) records exact owned-root identities and free-space observations.

End-of-task owned scratch held **483,897,204 logical bytes / 530,505,728 allocated bytes** including the task-local Go SDK/cache and disposable executable/object files. The private physical-filesystem I/O root held **90,000 logical / 98,304 allocated bytes**. Both roots were removed after verification; the task pointer was removed. Available tmpfs space changed from 15,980,740,608 to 16,511,254,528 bytes; host filesystem space changed from 362,147,422,208 to 362,147,295,232 bytes during removal. Filesystem-wide deltas can include concurrent desktop activity. No claim of an earlier peak beyond this measured end-of-task inventory.

Exact checkout change: **`docs/compiler-health/G31_VOID_GO_PERFORMANCE_BASELINE.md` only**, newly untracked research documentation. Tracked/staged diffs empty; HEAD remains `189be08423bdc2beb91183b0608b018190dc7524`. External changes are this verified archive, its cleanup receipt and the existing archive index. No production/test/build configuration edits, Git mutations, hosted CI or package publication. Active build/toolchain/provider trees and previous evidence were not removed.

**Review conclusion:** VOID's pure AOT numeric code already competes with native references on the selected kernel; managed per-block coordination can erase that strength. Go is substantially faster in this managed numeric and source-library cohort. Durable I/O is much closer. The smallest evidence-supported next investigation is the existing poll's no-request cost, preserving all safepoints and ownership rules—not a broad SIMD or collection rewrite. Golden Wrench reviews; Boss controls implementation, integration and release.
