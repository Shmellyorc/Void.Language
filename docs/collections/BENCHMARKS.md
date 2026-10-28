# Collections measurements, Linux Release

The [immutable measurements and generated workloads](evidence/benchmark-ci-official-release-1zgqnc53/measurements.json)
contain compiler-host hashes, exact inputs, samples, checksums and callback totals.
The existing `void_channel_benchmark` executable is a generic source runner and
was reused unchanged. It analyzes/JIT-compiles once, performs separate setup and
warmup, then records three timed samples. Persistent cache is disabled. These
measurements use LLVM 19.1.7 O2 with the Release Runtime, on the current shared
machine; they are not cross-platform or isolated-hardware performance guarantees.

Times below are median milliseconds per algorithm workload. The bounds workload
contains 100 queries. Compilation is seconds and excluded from execution. Setup is
excluded; result checks and one callback-count stderr write per invocation are
included. Ordinary-loop baselines omit callback increments, so their difference
is not a pure dispatch-only measurement. Process RSS in the raw record is a
process-wide high-water mark and is not algorithm allocation count or a leak test.

| Workload | Median ms | Callback calls/workload | Compile seconds |
|---|---:|---:|---:|
| sort-0-ordered | 0.0123 | 0 | 0.859 |
| sort-0-reverse | 0.0124 | 0 | 0.850 |
| sort-0-duplicates | 0.0128 | 0 | 0.926 |
| sort-0-mixed | 0.0137 | 0 | 0.931 |
| sort-1-ordered | 0.0300 | 0 | 0.841 |
| sort-1-reverse | 0.0295 | 0 | 0.824 |
| sort-1-duplicates | 0.0306 | 0 | 0.844 |
| sort-1-mixed | 0.0304 | 0 | 0.841 |
| sort-32-ordered | 1.7409 | 80 | 0.868 |
| sort-32-reverse | 1.7977 | 80 | 0.860 |
| sort-32-duplicates | 2.2148 | 120 | 0.851 |
| sort-32-mixed | 1.7184 | 80 | 0.825 |
| sort-1024-ordered | 105.3995 | 5120 | 0.854 |
| sort-1024-reverse | 101.5225 | 5120 | 0.816 |
| sort-1024-duplicates | 142.8010 | 8476 | 0.911 |
| sort-1024-mixed | 125.7600 | 7591 | 0.831 |
| sort-16384-ordered | 2313.3600 | 114688 | 0.869 |
| sort-16384-reverse | 2265.4000 | 114688 | 0.806 |
| sort-16384-duplicates | 3125.4700 | 194564 | 0.834 |
| sort-16384-mixed | 3039.9000 | 188387 | 1.102 |
| bounds-1024 | 3.0790 | 1002 | 0.788 |
| filter | 1.7985 | 1024 | 0.728 |
| map | 2.0466 | 1024 | 0.735 |
| fold | 1.3326 | 1024 | 0.724 |
| filter-loop | 1.7134 | 0 | 0.733 |
| map-loop | 1.9884 | 0 | 0.704 |
| fold-loop | 1.1727 | 0 | 0.718 |
| dictionary-keys | 3.4234 | 0 | 0.703 |
| dictionary-values | 6.8532 | 0 | 0.774 |
| dictionary-copy_dictionary | 5.7487 | 0 | 0.759 |
| sort-text | 8.1869 | 80 | 0.831 |
| sort-references | 10.9070 | 448 | 0.877 |
| sort-fixed-256KiB | 453.4390 | 4 | 0.883 |

The measurements demonstrate logarithmic query comparisons (1,002 comparisons for
100 searches in 1,024 elements) and merge-comparison growth across the measured
sizes/distributions. Stable sorting is still substantially more expensive than a
single source loop. Text comparison's two owned UTF-8 conversions and ordinary
large fixed-value copying remain visible costs: four 256 KiB values take about
453 ms here. No zero-allocation, deep-copy elimination or game-frame latency claim
is made. The bounded index workspace avoids per-pass payload buffers and recursion;
this sprint does not add raw borrows, an alternative container or a speculative
Runtime/GC optimization to conceal those existing costs. Callback counts are exact;
complete allocator-event accounting was not collected for these timing runs.

The initial benchmark attempt had a source-name collision from unaliased console;
its failing source/command/outputs remain in `evidence/crashes/incident-kbiww20w`.
It is separate from this corrected successful run. Allocation/GC correctness is
verified by the dedicated host sweeps, not inferred from timing or RSS.
