# Logging bounded measurements

**Historical measurements retained.** These tables are the original pre-repair observations. The immutable isolated baseline and component investigations are under [evidence/performance-repair](evidence/performance-repair/); see [PERFORMANCE_REPAIR.md](PERFORMANCE_REPAIR.md). Final isolated repair tables follow below; the original history is retained.

28 October 2026, local Linux x86-64 Release, pinned LLVM/Clang 19.1.7.
AMD Ryzen 7 1700X, 16 logical CPUs, about 31 GiB RAM. Other local acceptance
processes ran concurrently; these are bounded observations, not isolated-machine
throughput qualification. [Environment](evidence/benchmark-environment.json),
[raw 216 samples](evidence/benchmarks.json), [workloads](../../tests/log/benchmark.void),
[host](../../tests/log/host.cpp), [measurement driver](../../tests/log/benchmarks_default.py).

Each of 18 workloads has a warmup and three measured repetitions in each of four
modes. JIT source analysis/lowering/compilation occurs before execution timing.
AOT probes are emitted and linked independently before launch and link Runtime
without LLVM. The host's actual soft/hard stack limits are both 1,048,576 bytes.
Counts/checksums prevent discarded work; caller message construction, acquisition
and final checked close remain inside each execution interval.

The final timing host uses **Runtime(256)**, matching production JIT creation and
native bootstrap. Its [exact source overlay](evidence/benchmark-default-host.cpp)
changes only the test host's Runtime budget (1→256) and resolves its probe include.
All production libraries, source programs, optimization flags and probe behavior
are shared. The original [216 collection-at-every-allocation samples](evidence/benchmarks-gc-stress.json)
remain separate. An intermediate Runtime(0) experiment disabled automatic GC;
its completed JIT samples and interrupted AOT samples are preserved separately
and excluded from the tables below. The [stop journal](evidence/benchmark-unbudgeted-stop.json)
records the measurement correction; it was not a logger failure.

## Execution wall and CPU time

Milliseconds per whole workload, **median [minimum, maximum]**, three samples.
Payload sizes exclude framing. Console measurements use the existing isolated
output adapter; real OS redirection and broken pipes are functional acceptance
tests, not terminal-rendering performance measurements. File measurements use
actual temporary native files and checked synchronization.

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
| --- | ---: | ---: | ---: | ---: |
| Disabled direct / 10,000 | 26.62 [26.51, 26.82] | 26.47 [26.37, 27.23] | 22.45 [22.40, 22.50] | 22.78 [22.69, 22.86] |
| Disabled eager display / 10,000 | 60.07 [59.64, 64.06] | 59.02 [58.28, 59.81] | 50.56 [50.52, 51.20] | 51.43 [51.30, 51.59] |
| Disabled enabled-guard / 10,000 | 11.78 [11.70, 11.99] | 12.08 [12.04, 12.75] | 10.42 [10.39, 10.46] | 10.88 [10.81, 10.91] |
| Disabled named lazy / 10,000 | 20.43 [20.38, 20.57] | 21.26 [21.23, 21.40] | 18.80 [18.69, 19.01] | 19.52 [19.50, 19.78] |
| Disabled captured lazy / 10,000 | 30.98 [29.82, 32.82] | 29.26 [29.00, 29.39] | 25.46 [25.45, 25.66] | 26.00 [25.84, 26.33] |
| Console 64 B / 200 | 262.50 [261.43, 344.54] | 257.91 [257.22, 259.78] | 235.93 [235.92, 242.94] | 263.55 [254.59, 277.06] |
| File 64 B / 200 | 260.08 [258.94, 260.75] | 265.32 [263.77, 276.18] | 238.71 [238.28, 241.64] | 261.47 [260.30, 261.71] |
| File 1 KiB / 100 | 1867.17 [1860.99, 1963.68] | 1935.46 [1930.53, 1992.38] | 1778.64 [1769.75, 1781.15] | 1899.56 [1894.26, 1900.69] |
| File 65,500 B / 3 | 3521.21 [3451.80, 3534.42] | 3706.83 [3681.49, 3776.20] | 3369.69 [3349.76, 3401.93] | 3659.74 [3587.76, 3686.50] |
| File 64 B + UTC / 200 | 266.23 [265.52, 267.88] | 284.32 [281.80, 288.20] | 251.37 [250.86, 268.26] | 272.61 [271.12, 273.13] |
| 20 rotations / 0 backups | 29.76 [29.30, 29.76] | 30.81 [30.65, 33.88] | 27.86 [27.69, 27.90] | 29.14 [29.13, 29.27] |
| 20 rotations / 3 backups | 31.32 [31.22, 31.88] | 32.56 [32.09, 32.90] | 28.84 [28.77, 28.89] | 30.58 [30.56, 30.78] |
| 20 rotations / 32 backups | 48.32 [47.89, 48.96] | 49.04 [48.82, 49.78] | 42.33 [42.25, 42.55] | 44.69 [44.43, 44.82] |
| Direct io identical bytes / 200 | 3.02 [2.89, 3.02] | 3.04 [3.03, 3.21] | 2.65 [2.65, 2.84] | 2.82 [2.80, 2.86] |
| Direct console identical bytes / 200 | 1.39 [1.33, 1.46] | 1.41 [1.37, 1.46] | 1.26 [1.25, 1.43] | 1.39 [1.32, 1.42] |
| File + 20 explicit flushes / 200 | 254.27 [252.64, 255.47] | 264.70 [263.86, 266.31] | 239.05 [237.33, 243.23] | 263.35 [259.27, 267.40] |
| Sequential async / 100 | 67.68 [59.68, 70.20] | 68.57 [59.35, 75.59] | 55.42 [53.54, 60.47] | 63.43 [62.45, 63.65] |
| Sequential async / 1,000 | 3486.77 [3279.27, 3537.78] | 3318.72 [3294.34, 3344.30] | 2431.33 [2264.83, 2463.65] | 2450.96 [2234.76, 2456.01] |

Process CPU milliseconds, with the same ordering and sample range:

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
| --- | ---: | ---: | ---: | ---: |
| Disabled direct / 10,000 | 26.41 [26.30, 26.59] | 26.32 [26.24, 27.05] | 22.17 [22.14, 22.25] | 22.66 [22.60, 22.75] |
| Disabled eager display / 10,000 | 59.68 [59.23, 63.29] | 58.69 [57.98, 59.46] | 50.26 [50.07, 50.75] | 51.23 [51.09, 51.38] |
| Disabled enabled-guard / 10,000 | 11.69 [11.61, 11.92] | 12.01 [11.98, 12.65] | 10.33 [10.32, 10.37] | 10.78 [10.77, 10.84] |
| Disabled named lazy / 10,000 | 20.29 [20.26, 20.35] | 21.14 [21.13, 21.28] | 18.68 [18.58, 18.89] | 19.38 [19.33, 19.61] |
| Disabled captured lazy / 10,000 | 30.69 [29.55, 32.42] | 29.07 [28.84, 29.19] | 25.23 [25.17, 25.53] | 25.88 [25.76, 26.06] |
| Console 64 B / 200 | 260.69 [259.79, 341.10] | 256.69 [256.10, 258.43] | 234.39 [234.37, 241.14] | 260.65 [253.22, 274.23] |
| File 64 B / 200 | 257.31 [256.37, 257.95] | 264.06 [262.48, 274.81] | 237.38 [237.15, 240.39] | 258.95 [258.81, 260.32] |
| File 1 KiB / 100 | 1857.06 [1851.66, 1952.41] | 1927.75 [1922.32, 1982.58] | 1767.94 [1760.25, 1772.38] | 1888.58 [1886.62, 1890.68] |
| File 65,500 B / 3 | 3505.31 [3436.73, 3514.61] | 3685.19 [3666.37, 3747.86] | 3353.46 [3332.65, 3384.43] | 3635.97 [3573.52, 3665.48] |
| File 64 B + UTC / 200 | 264.74 [264.02, 266.47] | 282.63 [280.18, 286.43] | 250.08 [249.56, 266.51] | 270.88 [269.79, 271.07] |
| 20 rotations / 0 backups | 29.52 [29.06, 29.53] | 30.58 [30.41, 33.55] | 27.63 [27.48, 27.65] | 28.94 [28.94, 29.06] |
| 20 rotations / 3 backups | 31.04 [30.96, 31.58] | 32.30 [31.87, 32.61] | 28.58 [28.52, 28.63] | 30.35 [30.32, 30.51] |
| 20 rotations / 32 backups | 47.79 [47.42, 48.44] | 48.56 [48.36, 49.27] | 41.98 [41.87, 42.12] | 44.22 [44.01, 44.35] |
| Direct io identical bytes / 200 | 3.00 [2.88, 3.01] | 3.03 [3.01, 3.18] | 2.64 [2.63, 2.79] | 2.80 [2.78, 2.80] |
| Direct console identical bytes / 200 | 1.34 [1.33, 1.46] | 1.40 [1.36, 1.45] | 1.26 [1.25, 1.42] | 1.36 [1.31, 1.38] |
| File + 20 explicit flushes / 200 | 252.72 [251.02, 253.85] | 263.30 [262.49, 264.84] | 237.42 [235.83, 241.82] | 261.60 [257.85, 265.34] |
| Sequential async / 100 | 67.68 [60.00, 70.95] | 69.49 [60.43, 76.02] | 56.38 [54.73, 61.33] | 64.35 [63.58, 64.52] |
| Sequential async / 1,000 | 3512.74 [3309.19, 3555.82] | 3348.49 [3323.24, 3370.67] | 2457.53 [2289.17, 2489.61] | 2465.17 [2256.25, 2484.61] |

## Operations and allocations

A single value means all modes/samples agree. Otherwise values are in the four
mode columns' order above. Managed constructions count the existing construction
probe, not every string/buffer/native allocation. Retained output bytes include
console capture and remaining managed files; deleted rotation history is excluded.

| Workload | Managed constructions | Retained output B | Console calls | Native writes | Flushes | Metadata | Moves | Deletes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Disabled direct / 10,000 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Disabled eager display / 10,000 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Disabled enabled-guard / 10,000 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Disabled named lazy / 10,000 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Disabled captured lazy / 10,000 | 10005 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Console 64 B / 200 | 1017 | 15000 | 200 | 0 | 0 | 0 | 0 | 0 |
| File 64 B / 200 | 1839 | 15000 | 0 | 200 | 1 | 4 | 0 | 0 |
| File 1 KiB / 100 | 939 | 103500 | 0 | 100 | 1 | 4 | 0 | 0 |
| File 65,500 B / 3 | 66 | 196533 | 0 | 3 | 1 | 4 | 0 | 0 |
| File 64 B + UTC / 200 | 1839 | 20000 | 0 | 200 | 1 | 4 | 0 | 0 |
| 20 rotations / 0 backups | 565 | 75 | 0 | 21 | 21 | 1 | 0 | 20 |
| 20 rotations / 3 backups | 571 | 300 | 0 | 21 | 21 | 4 | 60 | 20 |
| 20 rotations / 32 backups | 1027 | 1575 | 0 | 21 | 21 | 33 | 640 | 20 |
| Direct io identical bytes / 200 | 816 | 15000 | 0 | 200 | 1 | 0 | 0 | 0 |
| Direct console identical bytes / 200 | 0 | 15000 | 200 | 0 | 0 | 0 | 0 | 0 |
| File + 20 explicit flushes / 200 | 2019 | 15000 | 0 | 200 | 21 | 4 | 0 | 0 |
| Sequential async / 100 | 935 | 2300 | 0 | 100 | 1 | 4 | 0 | 0 |
| Sequential async / 1,000 | 9035 | 23000 | 0 | 1000 | 1 | 4 | 0 | 0 |

## Compilation and memory observations

| Mode | Analysis/lower/JIT compile ms | Admission/preparation ms | Process high-water RSS range KiB |
| --- | ---: | ---: | ---: |
| JIT normal | 8939.23 | 8939.23 | 174192–174192 |
| JIT O2 | 11090.26 | 11090.26 | 183180–183180 |
| AOT normal | 0.00 | 0.57 | 14532–15736 |
| AOT O2 | 0.00 | 0.59 | 20028–20028 |

AOT's zero compile column means compilation occurred in the earlier emitter/link
commands; it does not mean compilation was free. Their exact wall times, source
and executable hashes are in process receipts. JIT preparation includes its
compilation interval. RSS is the cumulative process high-water mark, including
compilation, warmups and prior workloads, not per-record live memory.

Async repeated-root memory snapshots, bytes and host handles (three repetitions):

| Mode / calls | Heap before | Heap at root return | Heap after explicit GC | Handles before → after |
| --- | ---: | ---: | ---: | --- |
| JIT normal / 100 | 128, 128, 128 | 42319, 42319, 42319 | 128, 128, 128 | 3→3, 3→3, 3→3 |
| JIT normal / 1000 | 128, 128, 128 | 251311, 251311, 251311 | 128, 128, 128 | 3→3, 3→3, 3→3 |
| JIT O2 / 100 | 128, 128, 128 | 42319, 42319, 42319 | 128, 128, 128 | 3→3, 3→3, 3→3 |
| JIT O2 / 1000 | 128, 128, 128 | 251311, 251311, 251311 | 128, 128, 128 | 3→3, 3→3, 3→3 |
| AOT normal / 100 | 234, 234, 234 | 42713, 42713, 42713 | 234, 234, 234 | 7→7, 7→7, 7→7 |
| AOT normal / 1000 | 234, 234, 234 | 251705, 251705, 251705 | 234, 234, 234 | 7→7, 7→7, 7→7 |
| AOT O2 / 100 | 234, 234, 234 | 42713, 42713, 42713 | 234, 234, 234 | 7→7, 7→7, 7→7 |
| AOT O2 / 1000 | 234, 234, 234 | 251705, 251705, 251705 | 234, 234, 234 | 7→7, 7→7, 7→7 |

These are explicit boundary snapshots, not a continuous peak-live-heap trace.
The existing async root retains task results/ordering records until root exit;
the logger adds no message queue. Post-GC payloads and host handles return to their
per-mode baseline across repeated roots. No constant-RSS or total-allocation-free
claim follows from that observation.

The measured costs are material. In JIT O2, 200 logged 64-byte file records took
265.32 ms versus 3.04 ms for direct io with identical bytes. The 64-byte, 1-KiB
and 65,500-byte cases cost approximately 20.7, 18.9 and 18.9 microseconds per input
byte, respectively: the bounded framing path scales approximately linearly here,
but has substantial overhead. These whole-path measurements do not isolate an
individual helper for a justified implementation optimization.

The JIT O2 async workload took 68.57 ms for 100 records and 3318.72 ms for 1000.
That is superlinear within this bounded range; no constant per-record async cost
is claimed. The measurement includes the established scheduler and root-lifetime
task retention and does not isolate their individual contributions. The repeated
post-root memory/handle baselines above show no unexplained retained growth.

The disabled cases perform no output. Eager custom display still runs 10,000 times;
both lazy bodies run zero times, while captured callback construction remains
visible. Ordinary file records do not perform per-record path metadata calls.
Rotation's counts grow with the exact configured backup set. The framing source
makes bounded linear UTF-8 passes and appends to one byte builder; larger-message
rows expose its actual cost, including caller construction. Direct console/io
rows use identical record bytes and show the framing/admission cost separately
from transport. No fixed throughput target or speculative Runtime optimization
was introduced. The source-generation and GC-policy choices remain reviewable
alongside the raw data rather than hidden behind a single throughput number.

## Mandatory repair: final isolated measurements

The final source matches the Release acceptance freeze before and after every
measurement. [Environment and exact launch sequence](evidence/performance-repair/final-measurement-environment.json)
records isolation, affinity, GC=256, actual hard/soft 1 MiB stacks and compiler
separation. No build or validation ran concurrently. The desktop remained active.
[Original-workload raw samples](evidence/performance-repair/final-original.json)
contain 216 observations across normal/O2 JIT and independent normal/O2 native
AOT. The VOID workload is unchanged. New C++ byte/count oracles run **after**
timing; they do not omit any measured work. Their temporary native allocations
can affect process peak RSS, which is not used to claim a memory improvement.

The [immutable baseline](evidence/performance-repair/baseline/freeze.json) contains
source and executable hashes, original sources/results and frozen binaries.
Each final harness directory contains generated source, production-source hashes,
compiler/link receipts and executable hashes: [original](evidence/performance-repair/final-original/hashes.json),
[async](evidence/performance-repair/async-final/hashes.json),
[framing](evidence/performance-repair/framing-final/hashes.json).
The [summary JSON](evidence/performance-repair/summary.json) records every warm
observation, first-stage invocation, median/range, CPU time and resource counts.

**Performance gate: OPEN.** Small synchronous file logging is 17.415 ms versus
2.973 ms direct I/O, about 5.86×. It misses both the single-digit and 2–3× initial
screen. Both investigations and measured repairs are documented; no acceptance
is inferred from the improvement. Heavy escaping remains expensive. The original
1,000-call async workload improved from 3283.641 ms to 125.811 ms; the remaining
curvature is explicitly attributed below, not called universally constant cost.

### Original workload: before and after

JIT O2, milliseconds, median [minimum, maximum]. Baseline is the isolated repeat,
not the earlier measurements taken during validation.

| Workload | Isolated baseline | Final |
|---|---:|---:|
| Console 64 B / 200 | 257.119 [256.484, 258.838] | 14.608 [14.258, 14.700] |
| File 64 B / 200 | 267.697 [266.983, 270.031] | 17.415 [17.329, 17.529] |
| File 1 KiB / 100 | 1952.759 [1951.398, 1973.467] | 74.005 [73.539, 74.267] |
| File 65,500 B / 3 | 3711.910 [3667.325, 3737.417] | 135.523 [134.581, 138.659] |
| File 64 B + UTC / 200 | 283.315 [283.264, 284.588] | 19.378 [19.280, 19.623] |
| Direct file identical bytes / 200 | 2.922 [2.877, 3.023] | 2.973 [2.889, 3.076] |
| Direct console identical bytes / 200 | 1.410 [1.373, 1.460] | 1.414 [1.405, 1.611] |
| Sequential async / 100 | 64.205 [63.109, 65.620] | 10.885 [10.675, 11.961] |
| Sequential async / 1,000 | 3283.641 [3261.956, 3310.093] | 125.811 [120.608, 128.536] |

Final wall time for all modes; same median [range] convention.

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
|---|---:|---:|---:|---:|
| Disabled direct / 10,000 | 24.942 [24.932, 25.017] | 25.491 [25.403, 26.012] | 22.027 [21.859, 22.056] | 22.569 [22.501, 22.913] |
| Disabled eager display / 10,000 | 53.535 [53.531, 53.728] | 56.738 [56.723, 56.817] | 49.453 [49.397, 49.519] | 50.983 [50.834, 51.227] |
| Disabled enabled-guard / 10,000 | 11.843 [11.749, 11.942] | 12.241 [12.034, 12.379] | 10.140 [10.109, 10.216] | 10.547 [10.541, 10.610] |
| Disabled named lazy / 10,000 | 21.100 [21.096, 21.241] | 21.343 [21.328, 21.399] | 18.423 [18.423, 18.447] | 19.135 [19.030, 19.137] |
| Disabled captured lazy / 10,000 | 28.088 [27.774, 28.184] | 28.811 [28.225, 28.819] | 25.839 [25.715, 25.932] | 26.379 [26.137, 26.512] |
| Console 64 B / 200 | 14.332 [14.285, 14.357] | 14.608 [14.258, 14.700] | 13.312 [13.285, 13.317] | 13.082 [12.917, 13.494] |
| File 64 B / 200 | 16.648 [16.502, 16.696] | 17.415 [17.329, 17.529] | 15.196 [15.168, 15.357] | 15.922 [15.798, 16.038] |
| File 1 KiB / 100 | 70.602 [70.540, 71.315] | 74.005 [73.539, 74.267] | 64.807 [64.428, 64.837] | 69.400 [69.295, 69.736] |
| File 65,500 B / 3 | 128.751 [127.145, 128.860] | 135.523 [134.581, 138.659] | 118.073 [117.210, 119.892] | 125.578 [125.174, 127.739] |
| File 64 B + UTC / 200 | 18.677 [18.505, 18.685] | 19.378 [19.280, 19.623] | 17.330 [17.272, 17.366] | 17.852 [17.749, 17.929] |
| 20 rotations / 0 backups | 3.567 [3.565, 3.710] | 3.668 [3.577, 3.684] | 3.299 [3.280, 3.331] | 3.450 [3.416, 3.458] |
| 20 rotations / 3 backups | 4.857 [4.754, 4.983] | 4.935 [4.867, 4.980] | 4.538 [4.477, 4.566] | 4.628 [4.543, 4.654] |
| 20 rotations / 32 backups | 16.216 [16.181, 16.655] | 16.371 [16.326, 16.812] | 15.293 [15.258, 15.296] | 15.616 [15.462, 15.764] |
| Direct file identical bytes / 200 | 2.907 [2.813, 3.094] | 2.973 [2.889, 3.076] | 2.733 [2.723, 2.827] | 2.740 [2.735, 2.784] |
| Direct console identical bytes / 200 | 1.287 [1.281, 1.292] | 1.414 [1.405, 1.611] | 1.464 [1.398, 1.488] | 1.313 [1.284, 1.333] |
| File + 20 explicit flushes / 200 | 17.120 [16.885, 17.156] | 17.667 [17.573, 17.945] | 15.431 [15.379, 15.533] | 16.410 [16.351, 16.462] |
| Sequential async / 100 | 11.293 [11.198, 14.085] | 10.885 [10.675, 11.961] | 13.800 [10.155, 14.058] | 10.818 [9.710, 11.312] |
| Sequential async / 1,000 | 123.307 [120.310, 130.372] | 125.811 [120.608, 128.536] | 116.387 [105.037, 119.465] | 119.196 [114.934, 121.179] |

Final process CPU milliseconds, median [range].

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
|---|---:|---:|---:|---:|
| Disabled direct / 10,000 | 24.822 [24.796, 24.908] | 25.350 [25.252, 25.849] | 21.931 [21.769, 21.941] | 22.470 [22.400, 22.785] |
| Disabled eager display / 10,000 | 53.274 [53.269, 53.444] | 56.398 [56.389, 56.478] | 49.226 [49.163, 49.291] | 50.680 [50.519, 50.910] |
| Disabled enabled-guard / 10,000 | 11.776 [11.707, 11.876] | 12.144 [11.978, 12.326] | 10.088 [10.059, 10.155] | 10.490 [10.474, 10.523] |
| Disabled named lazy / 10,000 | 21.013 [20.979, 21.149] | 21.235 [21.220, 21.288] | 18.346 [18.338, 18.369] | 19.018 [18.922, 19.038] |
| Disabled captured lazy / 10,000 | 27.936 [27.630, 28.029] | 28.632 [28.087, 28.633] | 25.732 [25.568, 25.790] | 26.228 [26.000, 26.349] |
| Console 64 B / 200 | 14.236 [14.176, 14.261] | 14.517 [14.162, 14.564] | 13.221 [13.164, 13.227] | 12.980 [12.847, 13.410] |
| File 64 B / 200 | 16.537 [16.409, 16.562] | 17.263 [17.199, 17.412] | 15.104 [15.060, 15.232] | 15.803 [15.695, 15.928] |
| File 1 KiB / 100 | 70.225 [70.192, 70.987] | 73.421 [73.117, 73.870] | 64.429 [64.070, 64.460] | 68.978 [68.901, 69.285] |
| File 65,500 B / 3 | 128.246 [126.694, 128.360] | 134.964 [133.994, 137.985] | 117.435 [116.661, 119.297] | 124.925 [124.644, 127.088] |
| File 64 B + UTC / 200 | 18.550 [18.390, 18.551] | 19.234 [19.143, 19.460] | 17.210 [17.144, 17.241] | 17.740 [17.630, 17.761] |
| 20 rotations / 0 backups | 3.547 [3.540, 3.638] | 3.608 [3.544, 3.657] | 3.284 [3.258, 3.304] | 3.429 [3.396, 3.433] |
| 20 rotations / 3 backups | 4.779 [4.722, 4.897] | 4.870 [4.841, 4.901] | 4.462 [4.453, 4.483] | 4.541 [4.515, 4.556] |
| 20 rotations / 32 backups | 15.947 [15.900, 16.339] | 16.133 [16.082, 16.524] | 15.053 [15.017, 15.076] | 15.368 [15.219, 15.544] |
| Direct file identical bytes / 200 | 2.890 [2.795, 2.976] | 2.959 [2.872, 3.015] | 2.719 [2.711, 2.807] | 2.715 [2.699, 2.726] |
| Direct console identical bytes / 200 | 1.282 [1.276, 1.284] | 1.403 [1.399, 1.546] | 1.452 [1.311, 1.482] | 1.309 [1.280, 1.326] |
| File + 20 explicit flushes / 200 | 17.018 [16.769, 17.034] | 17.548 [17.453, 17.799] | 15.353 [15.285, 15.412] | 16.283 [16.215, 16.284] |
| Sequential async / 100 | 11.601 [11.275, 14.708] | 11.609 [11.545, 11.979] | 14.793 [10.815, 15.166] | 11.555 [10.772, 12.193] |
| Sequential async / 1,000 | 128.051 [124.630, 134.330] | 130.762 [125.582, 131.574] | 121.207 [110.987, 123.787] | 124.977 [119.366, 126.200] |

### Independent async matrix

Final JIT O2: **total milliseconds (microseconds per call)**, warm medians. Each
cell has three individual samples/ranges and CPU times in the summary/raw JSON.
All counts run within their natural long-lived async root. Timestamp rows perform
real clock sampling followed by the documented test-only pinned instant, enabling
an exact complete-byte oracle for both paths. Direct async console means an
ordinary async activation around the same synchronous console API.

| Path | 100 | 250 | 500 | 1,000 |
|---|---:|---:|---:|---:|
| Direct sync file | 1.318 (13.18) | 3.090 (12.36) | 5.943 (11.89) | 11.955 (11.96) |
| Direct async file | 4.743 (47.43) | 11.031 (44.13) | 22.044 (44.09) | 45.300 (45.30) |
| Sync log file | 5.274 (52.74) | 12.328 (49.31) | 24.978 (49.96) | 48.747 (48.75) |
| Async log file | 10.792 (107.92) | 27.525 (110.10) | 58.458 (116.92) | 138.219 (138.22) |
| Direct sync console | 0.484 (4.84) | 1.137 (4.55) | 2.287 (4.57) | 4.841 (4.84) |
| Direct async console activation | 0.773 (7.73) | 1.850 (7.40) | 3.944 (7.89) | 7.599 (7.60) |
| Sync log console | 3.830 (38.30) | 9.574 (38.30) | 19.607 (39.21) | 39.291 (39.29) |
| Async log console | 5.345 (53.45) | 13.416 (53.66) | 26.329 (52.66) | 54.373 (54.37) |
| Direct sync file + UTC | 2.553 (25.53) | 6.104 (24.42) | 12.333 (24.67) | 24.418 (24.42) |
| Direct async file + UTC | 6.285 (62.85) | 15.726 (62.90) | 30.623 (61.25) | 64.677 (64.68) |
| Sync log file + UTC | 6.318 (63.18) | 15.225 (60.90) | 29.676 (59.35) | 60.610 (60.61) |
| Async log file + UTC | 12.091 (120.91) | 32.341 (129.37) | 65.918 (131.84) | 164.246 (164.25) |
| Direct sync console + UTC | 1.742 (17.42) | 4.117 (16.47) | 8.145 (16.29) | 16.283 (16.28) |
| Direct async console activation + UTC | 2.322 (23.22) | 5.107 (20.43) | 9.449 (18.90) | 19.831 (19.83) |
| Sync log console + UTC | 4.994 (49.94) | 12.113 (48.45) | 24.805 (49.61) | 49.139 (49.14) |
| Async log console + UTC | 6.166 (61.66) | 15.844 (63.37) | 32.024 (64.05) | 66.304 (66.30) |
| Empty async activation | 0.184 (1.84) | 0.409 (1.64) | 0.992 (1.98) | 1.746 (1.75) |

The final matrix comprises 272 JIT timing, 272 independent AOT timing and 272 AOT
instrumented observations (first invocation plus three warm samples per cell).
[Timing AOT](evidence/performance-repair/async-final/void_log_probe_optimized-timing.json)
provides the matching instrumentation-overhead control. The original four-mode
workload supplies normal-mode performance coverage; this expanded matrix is O2.

Instrumented AOT file logging, timestamps off; warm median work counters. Native
timers are exclusive per thread, but worker execution overlaps main-thread wait.
These timings cannot be summed as process CPU or substituted for production time.

| Calls | Task records | Reception visits | Alias visits | GC handle visits | GC ms | Handle removal ms | Native execution ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| 100 | 429 | 20,393 | 51,240 | 4,670 | 0.536 | 0.044 | 0.374 |
| 250 | 1,029 | 49,009 | 142,660 | 24,968 | 2.100 | 0.089 | 0.498 |
| 500 | 2,029 | 96,793 | 292,412 | 101,150 | 6.698 | 0.214 | 0.994 |
| 1000 | 4,029 | 192,276 | 600,311 | 398,988 | 23.682 | 0.388 | 2.070 |

Before repair, the 1,000-call sample performed 30,290,401 task lookup visits,
101,349,082 reception visits and 131,993,278 heap-alias visits. Lookup now uses a
hash index; successful pending/failure checks do not scan task history. Aliases
scan buffers only. Cleanup uses indexed root removal. Ready/worker queues,
completed task lifetimes and result availability are unchanged.

GC remains significant: retained results are traced repeatedly under unchanged
budget 256, so arbitrary unbounded root lifetimes do not have linear GC work.
The bounded matrix's remaining curvature is consistent with that measured cost.
At 1,000 calls the direct async file path creates 2,008 task records, compared
with 4,029 for async logging; both fully synchronous controls create only their
one outer root task. Before native close, the instrumented direct/log paths
observe 4,018/8,060 registered handles and 214,280/245,242 transient managed
payload bytes. These snapshots include uncollected garbage and empty registered
frame handles; they are not a count of retained live frames. The separate
retention suites verify only the active root frame remains after awaited
children finish, while completed results remain usable. All final async
samples return post-collection managed payload and persistent handles to baseline.

### Framing and remaining synchronous costs

Final JIT O2 component medians for 200 ASCII 64-byte inputs. These are independent
probes, not additive nested spans: subtracting them is an attribution estimate,
with repeated setup/loop costs and measurement noise.

| Component | ms |
|---|---:|
| Loop/count control | 0.392 |
| Filtering and state admission | 0.529 |
| UTC sample | 0.510 |
| Fixed-instant timestamp formatting | 0.828 |
| UTF-8 conversion | 0.967 |
| Legacy sizing (configuration path) | 35.758 |
| Escaping / immutable-text fast path | 9.131 |
| Complete frame | 11.541 |
| Standalone byte pushes | 31.741 |
| Standalone checked array traversal | 18.763 |
| Existing bulk memory writer | 3.899 |
| Direct complete-record console transfer | 1.627 |
| Full console logging | 15.023 |
| Unescaped interpolation lower bound | 1.725 |

The ~9.1 ms quote stage accounts for most of the ~11.5 ms framing stage; it
performs one checked byte read per ASCII byte, even on the no-escape path. The
UTF-8 conversion probe is ~1.0 ms and the unescaped interpolation lower bound is
~1.7 ms. Full console logging adds roughly 3.5 ms over framing, including the
~1.6 ms direct-transfer component and source admission/guard/receipt work. The
full method makes 1,000 source managed-construction calls for 200 records; pure
framing makes zero such calls. File logging additionally performs owned-file
acquisition, four metadata observations, native writes and one checked flush.

In the final instrumented 200-record full-console probe, median exclusive time
includes 3.462 ms in list-header validation, 2.069 ms in scalar reads, 0.766 ms in
root lookup, 0.984 ms in reference validation, 3.437 ms in polling, 1.323 ms in
object construction, 0.973 ms in string allocation, 0.830 ms in UTF-8 encoding,
0.428 ms in alias validation and 0.349 ms in GC. Counts include 12,800 scalar reads
and 111,017 Runtime polls. Instrumentation itself is material: the matching AOT
full-console median is 13.185 ms without timers versus 22.670 ms with timers.
The scalar-header overload also materializes its schema lookup key per read;
that known allocation/copy work is included in scalar-read time and explains
much of the 16,222 native allocations in the small quote probe. It remains an
optimization candidate, not an assumed unavoidable cost.
Unattributed instrumented time includes timer bookkeeping, generated control flow
and uninstrumented ABI/frame transitions; it is not asserted to be zero or all
one production cost. Separate component times identify the significant residual
checked-scan and per-record method work without treating timer overhead as useful
work. The single-digit target is still unmet.

Complete-frame medians and selected resource counts, all final shapes:

| Input/count | JIT ms | Native allocations (instrumented) | Requested allocation bytes | Explicit copy bytes | Source constructions | GC epochs |
|---|---:|---:|---:|---:|---:|---:|
| ASCII 64 B / 200 | 11.541 | 24,522 | 941,608 | 306,568 | 0 | 8 |
| ASCII 1 KiB / 100 | 66.036 | 108,266 | 4,599,480 | 1,593,284 | 0 | 4 |
| ASCII 65,500 B / 3 | 119.977 | 196,684 | 8,456,456 | 2,948,841 | 0 | 0 |
| Escaped 64 B / 200 | 128.903 | 348,612 | 12,486,112 | 3,216,584 | 10,200 | 165 |
| Escaped 1 KiB / 100 | 914.881 | 2,509,954 | 89,737,148 | 23,499,120 | 77,100 | 1,162 |
| Escaped 16,000 B / 3 | 424.292 | 1,167,336 | 41,734,127 | 10,944,367 | 36,009 | 539 |
| Unicode 64 B / 200 | 8.444 | 18,522 | 755,608 | 252,568 | 0 | 8 |
| Unicode 1 KiB / 100 | 42.161 | 60,266 | 3,111,480 | 1,161,284 | 0 | 4 |
| Unicode 65,472 B / 3 | 77.114 | 104,530 | 5,598,674 | 2,118,951 | 0 | 0 |
| Escaped 30,816 B / 3 | 824.692 | 2,247,628 | 79,989,611 | 20,890,779 | 69,345 | 1,038 |

The final full framing probes have 544 JIT timing and 544 AOT instrumented samples,
plus 224 AOT timing controls for selected stages. Shape 9 adds a 65,495-byte
complete escaped record while preserving the original nine inputs. Independent
C++ oracles verify complete direct/log bytes, escaped lengths and framing lengths.

Escape-heavy work remains much slower than ordinary text: each replacement uses
checked literal/slice/bulk-writer operations and source stream-state construction.
For the 1-KiB escape-heavy quote stage, the preceding fully equivalent checkpoint
measured 76,900 bulk writes, 784,400 header validations and 77,100 source managed
constructions across 100 records. Final counters are in the summary. This is
linear work with a high constant, not accepted high throughput. Unchanged spans
are disjoint; geometric buffer growth and a bounded number of final copies avoid
quadratic whole-buffer reconstruction. Explicit copy counters include small
root/header copies and selected Runtime memcpy/memmove calls; they exclude hidden
C++ library/OS copies and should not be read as payload-copy counts.

All original and intermediate measurements remain available. See
[PERFORMANCE_REPAIR.md](PERFORMANCE_REPAIR.md) for mechanisms, safety arguments,
remaining limits and the independent-review hold.

## db808efe hot-path continuation: architecture checkpoint

**Neither new hot path is accepted.** See [HOTPATH_REVIEW.md](HOTPATH_REVIEW.md)
for the required architecture decision. No production or repository test source
was changed. The earlier sprint and its raw data below remain historical evidence.
New raw data is external at
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/`.
`EVIDENCE_INDEX.md` indexes source/executable identities, commands, receipts,
failures, CPU/wall observations, counters and the compact review archive.

The unchanged baseline was frozen (787 source/document/test files, 11 executables)
and repeated before experimentation: 216 original-workload observations, 544 JIT
framing observations, 544 independently emitted AOT O2 instrumented framing
observations and 272 JIT async observations. Historical logging evidence has its
own hash inventory. New observations do not overwrite old results.

New expanded measurements use JIT O2 with GC budget 256 and soft **and hard**
1 MiB stacks, one warmup and three measured samples per component. Compilation
is outside execution timing. Builds and validations run sequentially; the user's
desktop remains active, so small differences are not precision guarantees. Native
operation counters come from separate instrumented runs, not timed ranking runs.
The expanded harness adds source control branches to both compared variants;
do not mix its counts with the earlier harness as if only the formatter changed.

### Expanded framing comparison

Median wall milliseconds. Baseline is unchanged production formatting; candidate
is the test-only flat byte builder plus immutable read/write schema keys. This
candidate is rejected for acceptance: ordinary text remains too slow and occasional
escapes regress. Full per-stage CPU, min/max wall, ns/input byte, ns/complete-output
byte and counter summaries are in `summary.json`; all samples remain separately.

| Input | Bytes / record | Complete output / record | Records | Baseline frame ms | Candidate frame ms | Candidate CPU ms |
|---|---:|---:|---:|---:|---:|---:|
| ASCII 64 B | 64 | 75 | 200 | 10.884 | 10.105 | 10.055 |
| ASCII 1 KiB | 1,024 | 1,035 | 100 | 60.883 | 53.249 | 52.988 |
| ASCII near budget | 65,500 | 65,511 | 3 | 111.035 | 97.719 | 97.244 |
| Heavy controls 64 B | 64 | 147 | 200 | 130.130 | 50.852 | 50.579 |
| Heavy controls 1 KiB | 1,024 | 2,187 | 100 | 926.213 | 343.877 | 342.235 |
| Heavy controls 16,000 B | 16,000 | 34,011 | 3 | 431.623 | 157.554 | 156.887 |
| Unicode 64 B | 64 | 75 | 200 | 8.340 | 7.815 | 7.773 |
| Unicode 1 KiB | 1,024 | 1,035 | 100 | 39.694 | 35.805 | 35.513 |
| Unicode near budget | 65,472 | 65,483 | 3 | 71.685 | 64.363 | 63.857 |
| Heavy controls near budget | 30,816 | 65,495 | 3 | 842.008 | 299.759 | 298.280 |
| Occasional ASCII 64 B | 64 | 77 | 200 | 27.182 | 30.175 | 29.817 |
| Occasional ASCII 1 KiB | 1,024 | 1,067 | 100 | 138.957 | 181.101 | 179.668 |
| Occasional ASCII near budget | 63,539 | 65,536 | 3 | 245.763 | 326.894 | 325.165 |
| Mixed Unicode 64 B | 64 | 92 | 200 | 41.410 | 36.428 | 36.219 |
| Mixed Unicode 1 KiB | 1,024 | 1,307 | 100 | 246.120 | 222.060 | 220.875 |
| Mixed Unicode near budget | 51,772 | 65,536 | 3 | 363.380 | 328.776 | 327.258 |

### Fair controls and byte costs

Stage 11 writes complete bytes prepared before timing: an I/O floor. Stage 14
calls the **same source `_frame` on every record**, then direct console output:
a complete encoding/framing/I/O comparator without logger admission/delivery guards.
Stage 15 performs independent native UTF-8 decoding, exact escaping and complete
framing, including its exact-byte equality check inside timing: an algorithmic
lower bound, without managed roots, Runtime validation or sink I/O. It is not an
implementation of a new Runtime contract. Stages 11/12/14 verify complete bytes
and write counts outside timing; 6/7 check lengths; all check stable checksums.
The oracle handles C1, bidi, separators, emoji and combining characters by scalar.

| Input | Prepared I/O ms | Complete source frame + direct I/O ms | Native encoding/frame ms | Baseline full logger ms | Candidate full logger ms |
|---|---:|---:|---:|---:|---:|
| ASCII 64 B | 1.610 | 12.439 | 0.198 | 14.612 | 13.954 |
| Heavy controls 1 KiB | 7.926 | 949.305 | 1.976 | 932.257 | 355.783 |
| Heavy controls near budget | 6.681 | 853.640 | 1.962 | 852.033 | 310.076 |
| Occasional ASCII 1 KiB | 4.271 | 141.815 | 1.764 | 145.588 | 187.917 |
| Occasional ASCII near budget | 6.663 | 251.812 | 3.260 | 256.389 | 334.149 |
| Mixed Unicode 1 KiB | 5.077 | 254.221 | 1.600 | 255.874 | 228.816 |
| Mixed Unicode near budget | 6.638 | 372.516 | 2.378 | 373.349 | 333.010 |

| Heavy controls, 1 KiB × 100 | Wall ms | CPU ms | ns/input byte | ns/complete-output byte |
|---|---:|---:|---:|---:|
| Baseline framing | 926.213 | 919.429 | 9045.05 | 4235.09 |
| Candidate framing | 343.877 | 342.235 | 3358.17 | 1572.37 |
| Baseline full logger | 932.257 | 925.348 | 9104.07 | 4262.72 |
| Candidate full logger | 355.783 | 353.859 | 3474.45 | 1626.81 |
| Complete source control | 949.305 | 943.197 | 9270.55 | 4340.67 |
| Native algorithm control | 1.976 | 1.907 | 19.30 | 9.04 |

Before production selection, `targets.json` fixes the heavy 1-KiB/100 screen at
**10 ms complete framing** and **20 ms complete console logging**, corresponding
to 97.66/195.31 ns per input byte and 45.72/91.45 ns per complete output byte.
Measured native framing is about 1.976 ms and prepared output about 7.926 ms;
neither is functionally identical to a safe full logger. No sum of separately timed
stages is claimed as an end-to-end observation. Ordinary file200 retains the
single-digit-ms and approximately 2–3× prepared-I/O screen. No candidate meets these
screens. Near-budget output is checked at 65,495–65,536 bytes in relevant cases;
this is the default configured record budget, not a new global cap.

### Async preservation and baseline variability

The full unchanged-binary matrix includes 100/250/500/1,000 operations across all
17 modes: direct sync/async and logged sync/async file/console, timestamps off/on,
and empty async activations. The original file200 repeat is 17.323 ms versus
2.981 ms prepared direct I/O (5.81×). Its async1000 result was elevated relative
to the prior sprint: **158.233 ms**, versus historical **125.811 ms**. Original
samples are retained. A justified unchanged-executable confirmation gave
**141.688 ms**, not a replacement result. Confirmation file200/direct values
were 18.045/3.141 ms. CPU moved as well as wall time; a specific system cause has
not been established, and these differences are not called a repaired regression.

The isolated async comparison is a different harness/workload and must be kept
separate from those original-workload observations:

| File workload, timestamps off | 100 | 250 | 500 | 1,000 |
|---|---:|---:|---:|---:|
| Direct sync | 1.213 | 3.095 | 5.732 | 11.378 |
| Direct async | 4.483 | 10.554 | 21.208 | 44.283 |
| Logging sync | 5.092 | 12.347 | 24.679 | 47.883 |
| Logging async | 11.308 | 27.550 | 59.848 | 132.280 |

Unchanged-binary confirmation (same isolated matrix, milliseconds):

| File workload | 100 | 250 | 500 | 1,000 |
|---|---:|---:|---:|---:|
| Direct sync | 1.273 | 3.161 | 5.882 | 11.668 |
| Direct async | 4.817 | 11.945 | 21.750 | 48.588 |
| Logging sync | 5.347 | 12.644 | 25.686 | 50.972 |
| Logging async | 11.652 | 26.664 | 55.225 | 120.285 |

The scheduler, alias index, retained-result policy and Handle removal code are
byte-identical to the baseline. No new async repair was selected. Completed
output/cleanup oracles passed. These are baseline preservation observations,
not a post-production-change async signoff. The formatter candidates did not
receive the required full async/correctness matrix because none was selected.

The expanded timing runs contain 640 observations, and instrumentation adds 384.
The later original confirmation adds 108 and async confirmation 64. The async
confirmation initially used the wrong JSON prefix (`PERF` instead of `ASYNC`),
producing an empty derived file; original stdout contains all 64 passing samples.
`async-confirm-parsed.json` corrects extraction without rerunning or overwriting
that empty file. This was a reporting error, not a failed native execution.

A test-only combined/flat launch terminated with SIGSEGV before its first timing
row. The original incident remains unresolved; a phase-marked, signal-stack-enabled
host completed without reproducing it. A separate missing LLVM header in that
diagnostic host was corrected. See the failure inventory in `HANDOFF.md` and raw
incident receipts. There is no new sanitizer/full-suite pass at this checkpoint;
the prior 263/263 evidence remains preserved and native Windows/macOS is pending.


## Approved-architecture crash gate

Before implementing the approved general text operation, the preserved combined
keys/flat prototype's original O2 executable was run three times with matched
source/command identities and actual hard 1 MiB stacks. The third reproduced
SIGSEGV (-11), with no measurement output. Thirteen subsequent controlled
phase/signal/sanitizer invocations passed, including normal/O2 strict
ASan/UBSan/leak diagnostics. These controls produced no failing stack and do not
classify the original fault. There are 15 passing invocations, one signal and
720 diagnostic rows including warmups; they are not new performance acceptance
measurements. No formatter, schema or Runtime production change was selected.

The original heavy 10-ms framing / 20-ms console and small-file single-digit-ms
screens remain unchanged and unmet. Source/executable/environment identities and
per-attempt results are preserved under the external `approved-crash-classification`
directory, alongside `ESCALATION.md`. Section 1 of the architecture approval now
requires stopping implementation because a relevant production defect cannot be
excluded. Prior benchmark tables and the 263/263 baseline remain historical,
unchanged evidence; the final implementation validation matrix remains pending.
