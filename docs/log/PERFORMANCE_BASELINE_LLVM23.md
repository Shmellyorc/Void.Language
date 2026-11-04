# Logging performance baseline — LLVM 23, 4 November 2026

**LOG-PERF remains OPEN / release HOLD. No production repair was made.**
Fresh measurements at `ffd8920befe04a67e3eaef6aadcdf0233df13563` still miss the absolute small-file and heavy-escape screens. The small-file relative-I/O aim is met on this host; it does not close the other gates. Installation acceptance is a separate, completed hosted result.

## Criteria and prior evidence

The requirements are the [bounded performance plan](ROADMAP.md#bounded-performance-and-memory-plan) and [frozen engineering screens](HOTPATH_REVIEW.md#explicit-engineering-screens), not a new throughput promise. Historical correctness 263/263 does not establish performance acceptance.
The existing LLVM19 original/async/framing datasets, exclusive Runtime timer profiles and native allocation counts were inspected before measurement. The latest historical baseline recorded file200 at 17.323 ms versus 2.981 ms prepared I/O, heavy framing at 926.213 ms and full heavy console at 932.257 ms. Schema-key views and the flat-builder prototype did not meet acceptance; occasional escaping regressed. They were not rerun. Historical crash campaigns remain closed, with no causal repair claim from LLVM23.

## Current measurement and reproducibility

Local AMD Ryzen 7 1700X Eight-Core Processor, 16 logical CPUs, affinity 0–15, 31.3 GiB RAM; Linux-7.2.11-pikaos-x86_64-with-glibc2.44. This is an active developer desktop, not an isolated or hosted performance qualification. Exact OS/CPU identity, flags, per-sample wall/CPU values and counters are in the [machine-readable results](evidence/llvm23-baseline-20261104/results.json).
A new private Release build used pinned LLVM/Clang/LLD 23.1.4, research/sanitizers OFF, and only `void_log_host`, `void_log_probe_normal`, `void_log_probe_optimized` and their dependencies. `BUILD_TESTING=ON` exposes those existing targets; no CTest was executed. Existing pinned HTTP build inputs were reused read-only; no dependency bootstrap or HTTP test ran.
The existing host/workloads, framing suffix and allocation shim were reused. Private host copies change only GC 1→production 256 and their include path. The component module is a copied stdlib plus the existing test suffix. No proposed logger algorithm is substituted. Source LLVM optimization stays normal/default O2; C++ Release flags are retained. Timed children have actual soft/hard 1 MiB stacks. JIT compilation, emission/linking and source setup precede timed execution.
Five bounded benchmark processes completed: three original-workload executables provide **216 measured observations** across normal/O2 JIT and independent AOT; two JIT O2 component executables provide **192 observations**, including warmups, for ASCII 64 B/1 KiB, heavy-control 1 KiB and Unicode 1 KiB. Each cell has one warmup and three repetitions. The second component binary counts ordinary C++ allocations; its timings are excluded from rankings. No build or validation ran concurrently with benchmarks.

Raw receipts, exact argv/cwd/status/stdout/stderr, source freezes, generated overlays, compiler/link commands, driver copies and executable hashes are retained privately at `/home/shmellyorc/.local/state/void-log-performance/ffd8920b-20261104-9v8echee`. All measured exits were 0 and benchmark byte/checksum/count oracles passed; this is not a logging correctness requalification. The unchanged logger SHA-256 is `8cf2a66301ab4b955a2506b69c8c331e4d62639235a68de61957935c294e1821`, also matching the historical measured logger.
Executed commands (their byte-identical driver copies are retained as `build-driver.py` / `measure-driver.py` in that directory; select a fresh output for replay):

```sh
python3 /tmp/void-log-measure.py
python3 /tmp/void-log-prepare-measure.py prepare
python3 /tmp/void-log-prepare-measure.py original
python3 /tmp/void-log-prepare-measure.py components
```

## Measurements

Median whole-workload wall milliseconds. Every cell has three samples/ranges and CPU observations in the JSON; direct controls use identical record bytes and the same file flush policy.

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
| --- | ---: | ---: | ---: | ---: |
| console 64 B / 200 | 14.278 | 14.313 | 14.111 | 13.319 |
| file 64 B / 200 | 23.199 | 24.042 | 23.467 | 22.892 |
| file 1 KiB / 100 | 77.958 | 81.332 | 80.963 | 78.208 |
| file 65500 B / 3 | 135.139 | 143.652 | 139.708 | 144.017 |
| file 64 B + UTC / 200 | 26.218 | 26.264 | 25.095 | 24.772 |
| prepared direct file / 200 | 11.144 | 11.662 | 9.232 | 9.693 |
| prepared direct console / 200 | 2.135 | 2.474 | 2.381 | 2.318 |
| file with 20 explicit flushes / 200 | 151.249 | 145.464 | 142.721 | 144.852 |
| async file 100 / 100 | 22.620 | 22.727 | 16.086 | 16.713 |
| async file 1000 / 1000 | 113.208 | 112.743 | 108.217 | 110.277 |

JIT O2 file200: **24.042 ms**, 8,319 records/s and 120.21 µs/record amortized, including acquisition/close. Prepared file I/O is **11.662 ms**; ratio **2.06×**. The direct floor itself exceeds 10 ms in JIT on this host. The absolute miss cannot be attributed entirely to logging, and historical timings are not a controlled toolchain speedup/regression comparison.

JIT O2 components: medians with min/max ranges. Counts come from separate allocation runs, not timed instrumentation.

| Input/stage | Wall ms [min, max] | CPU ms | C++ allocations | Managed constructions |
| --- | ---: | ---: | ---: | ---: |
| ASCII 64 B ×200 quote | 8.694 [8.663, 8.699] | 8.655 | 16,223 | 0 |
| ASCII 64 B ×200 frame | 11.133 [11.120, 11.193] | 11.073 | 24,523 | 0 |
| ASCII 64 B ×200 full console | 14.762 [14.732, 14.903] | 14.681 | 32,425 | 1,000 |
| ASCII 1 KiB ×100 frame | 62.397 [61.958, 63.189] | 62.059 | 108,267 | 0 |
| Heavy 1 KiB ×100 quote | 922.152 [921.527, 931.163] | 916.614 | 2,504,835 | 77,100 |
| Heavy 1 KiB ×100 frame | 910.628 [908.791, 911.368] | 904.914 | 2,509,955 | 77,100 |
| Heavy 1 KiB ×100 prepared console | 7.946 [7.877, 8.351] | 7.912 | 1,711 | 0 |
| Heavy 1 KiB ×100 full console | 947.525 [942.704, 954.318] | 937.829 | 2,516,265 | 77,600 |
| Unicode 1 KiB ×100 frame | 41.152 [41.115, 41.180] | 40.851 | 60,267 | 0 |

Heavy framing processes 102,400 input and 218,700 complete output bytes: **8892.85 ns/input byte**, about **109.8 records/s**. These are whole-workload averages, not per-record p99 latency. Independent stage times are not additive; quote/frame ordering differences include measurement noise.
Heavy framing requests 89,737,292 ordinary native allocation bytes, performs 1,162 collections and 2,555,315 observed polls per 100 records. Requested bytes are cumulative allocation traffic, not live memory. The shim excludes direct malloc/mmap/OS allocations.
Every original/component observation returns managed payload and persistent handles to its pre-sample baseline after explicit GC. JIT original baselines are 128 B / 3 handles; AOT 234 B / 7 handles. Post-root snapshots are not continuous peak-live traces. Original process high-water RSS is 160,608–173,624 KiB for JIT, 17,684 KiB for AOT normal and 17,824 KiB for AOT O2; JIT includes compilation and all prior workloads. No constant-RSS or zero-allocation claim follows.

## Criterion disposition

| Criterion | Status | Evidence / limit |
| --- | --- | --- |
| Small file200 <10 ms | FAIL | 24.042 ms JIT O2; all four mode medians exceed 10 ms |
| Approximately 2–3× contemporaneous direct-I/O aim | PASS, bounded aim | 2.06× JIT O2; all four ratios fall between 2 and 3 |
| Heavy complete framing ≤10 ms | FAIL | 910.628 ms JIT O2 |
| Heavy full console ≤20 ms | FAIL | 947.525 ms JIT O2 |
| Filtered sink work and lazy-body suppression | PASS, bounded oracles | All five disabled variants perform zero file/console operations; lazy bodies execute zero times. Captured builders still construct caller closures |
| No metadata per ordinary file record | PASS, bounded counters | Four acquisition metadata observations, 200 writes, one flush; no record-count metadata growth |
| No unexplained post-root collected resource growth | PASS, bounded snapshots | All 408 observations return heap/handles to their per-sample baseline |
| Complete scaling/nonregression matrix | UNKNOWN | Near-limit original byte oracles pass; current occasional/mixed Unicode, heavy AOT components and native Windows/macOS performance matrix are not measured |
| Tail latency and peak/native-live memory | UNKNOWN | No numeric cap is invented; existing probes do not continuously trace these quantities |

## Bottlenecks, hypotheses and next action

Confirmed in current measurements: heavy quote/frame work is CPU-dominated and vastly larger than prepared output; approximately 2.51 million native allocations and 77,100 source constructions recur in framing. Ordinary quoting also performs 16,223 native allocations for 200 short records. The source still performs checked scalar reads and creates per-escape replacement arrays/slices/transactional stream states. Historical exclusive timers and controlled experiments identified those mechanisms; the current operation counts reproduce their scale.
Not newly established: the exact current wall-time split among scalar metadata keys, locks/root validation, polling, copying and GC. Allocation totals alone do not prove each allocation's time cost. The reason for the changed file-I/O floor, universal async complexity, a platform-wide speed regression and LLVM19 crash causality remain unknown. No historical candidate or new speculative optimization is selected.
**Smallest justified next action:** Golden Wrench reviews a separately scoped escape/framing repair plan against the existing bounded scalar-substitution design and its GC/root/lease/budget/failure refinements. The evidence prioritizes that path; schema-key views or the rejected flat builder are not sufficient demonstrated remedies. Review must explicitly disposition the historical implementation stop condition before authorizing production work. Neither the primitive's attainable latency nor public-release acceptance is proven here.

Production compiler/Runtime/CMake, logging implementation, root README and historical evidence remain unchanged. No full regression, HTTP/HTTPS, installation, VM, sanitizer, ptrace or historical crash campaign ran. Logging investigation files remain uncommitted for separate Golden Wrench review.
