# G31-I — integrated Runtime polling fast path

13 November 2026. Controlled local integration of the exact reviewed G31-P two-file
candidate. Starting/final HEAD: `189be08423bdc2beb91183b0608b018190dc7524`.
The [G31-R baseline](G31_VOID_GO_PERFORMANCE_BASELINE.md) and
[G31-P experiment](G31_RUNTIME_POLLING_EXPERIMENT.md) remain byte-identical,
including their original unfavorable samples and historical disposition.
This report records new production-source qualification. No new accepted
performance baseline, independent review or hosted acceptance is claimed.

## Exact production diff and identity

Before editing, G31-P's ZIP SHA-256, CRC and every manifest member SHA-256 were
verified. Both checkout files matched the archived baseline byte for byte.
Independently applying only the reviewed replacements reproduced both archived
candidate files before those exact bytes were installed.

| File | Integrated SHA-256 |
| --- | --- |
| `include/void/runtime.hpp` | `72907e5137525f0341aa2c6d086ac8d3cad8e6e53b9a9f7f992511af45827373` |
| `src/runtime/coordination.cpp` | `b017d8932b4fbf14115f1b508bb9d6c09f33811e95ef8b62becf02c96f462f71` |

```diff
diff --git a/include/void/runtime.hpp b/include/void/runtime.hpp
index 641f6df2..bb2bac8b 100644
--- a/include/void/runtime.hpp
+++ b/include/void/runtime.hpp
@@ -6,6 +6,7 @@
 #include "void/console.hpp"
 #include "void/application.hpp"
 #include <optional>
+#include <atomic>
 #include <array>
 #include <chrono>
 #include <condition_variable>
@@ -280,7 +281,7 @@ private:
     std::thread::id async_owner_ = {};
     std::size_t native_jobs_ = 0;
     bool closing_ = false;
-    bool requested_ = false;
+    std::atomic<bool> requested_{false};
     std::size_t callbacks_ = 0;
     std::size_t allocation_budget_ = 0, allocations_ = 0;
     std::uint64_t epoch_ = 0, completed_epoch_ = 0;
diff --git a/src/runtime/coordination.cpp b/src/runtime/coordination.cpp
index c93f8d78..0e115835 100644
--- a/src/runtime/coordination.cpp
+++ b/src/runtime/coordination.cpp
@@ -135,6 +135,7 @@ void Runtime::park(Thread& t, std::unique_lock<std::mutex>& lock) {
 }
 void Runtime::poll() {
     require_managed();
+    if (!requested_.load(std::memory_order_acquire)) return;
     std::unique_lock lock(mutex_);
     if (requested_) park(thread(), lock);
 }
@@ -325,7 +326,7 @@ Runtime::Snapshot Runtime::snapshot() const {
 bool Runtime::wait_for_request(std::chrono::milliseconds timeout) {
     if (current_runtime_ == this && thread().state == State::managed) throw std::logic_error("Managed thread cannot wait for a request");
     std::unique_lock lock(mutex_);
-    return changed_.wait_for(lock, timeout, [&] { return requested_; });
+    return changed_.wait_for(lock, timeout, [&] { return requested_.load(std::memory_order_relaxed); });
 }
 void Runtime::async_worker_limit(unsigned count) {
     if(!count || count>64)throw std::invalid_argument("worker limit must be 1..64");
```

Only the existing request boolean becomes atomic. The acquire false load follows
Managed-state validation; the original locked recheck/park remains authoritative.
The protected CV predicate explicitly returns a boolean load. Existing request
true/false stores remain under the original mutex and conservatively seq_cst.
No mirror flag, production hook, new API or coordination subsystem was added.

## Synchronization, roots and cancellation

A collector scans only when every participant is NativeSafe/Parked. Root/frame
writes by a Managed owner are published through its original locked safe-state
transition and mutex release; collector reacquisition establishes happens-before.
The removed no-request mutex acquisition was not permission to scan. Only the
participant writes its own state; cross-thread observations remain locked.

A false observation racing request publication leaves that participant Managed,
forcing the collector to wait. The next preserved poll or NativeSafe transition
rendezvouses; the fast flag publishes neither roots nor unlocked heap/epoch reads.
A true observation takes the mutex and rechecks current state. Completion, timeout
and successive epochs retain their ordering; the existing park loop remains
frozen across successive current requests. Competing collector results, timeout
without unsafe sweep, epoch overflow and exception behavior remain unchanged.

Native-to-Managed entry, attachment/detachment, shutdown admission, callback
quiescence, native-job drain and retained leases retain their protected gates.
The relaxed CV load is inside the original mutex predicate. Worker cancellation
keeps its separate atomic check, checkpoint frequency, completion release/acquire
and cleanup-failure precedence. Compiler safepoints, GC thresholds/budgets,
precise roots/lifetimes, checked arrays, LLVM target/SIMD/SLP policy, JIT cache,
public syntax and native ABI policy were not edited. This source argument and
bounded instrumentation do not prove scheduler fairness or a hard rendezvous bound.

## Fresh matching builds and baseline protection

Integrated Linux builds compile the actual modified production checkout in a
read-only Bubblewrap view with writable task outputs. The baseline alone overlays
the two verified old files at their logical source paths. All selected consumers
are rebuilt with matching headers; no old archive is mixed with new consumers.
Existing accepted build directories/binaries remain baseline references, untouched
by these fresh profiles. No install/release binary is published.

Both variants use LLVM/Clang/LLD **23.1.4**, matched Release settings, C++20,
LLVM-compatible no-RTTI ABI and existing verified host/HTTP providers. Strict uses
RelWithDebInfo and the existing sanitizer configuration. The Windows packet's
3,267 maintained files match the integrated checkout hashes; its two-line task-only
CMake append links the unchanged race fixture through the existing Runtime target.
Debug/Release use separate native builds and matching headers.

The baseline Runtime archive is byte-identical to accepted production:
4,902,966 bytes, SHA-256
`4b2354548e778741d2f1c99b2fbc70b9a8455f1465df2967f96a8d726d5a2f66`.
Integrated archive is byte-identical to G31-P's candidate:
4,903,006 bytes, SHA-256
`1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`.
**49/50 Runtime objects match**, only coordination differs. Backend and emitted
pure/coordinated objects are byte-identical between new variants. Runtime observed
size/alignment is Linux 1008/8 and Windows 1088/8; no portable layout claim follows.

## Maintained correctness and race controls

| Configuration | Maintained selection | Additional controls |
| --- | --- | --- |
| Linux baseline Release | **24/24 PASS**, 41.60 s summed testcase time | Coordinator/race control PASS |
| Linux integrated Release | **24/24 PASS**, 40.06 s summed testcase time | Coordinator/race control PASS |
| Linux integrated fatal ASan/UBSan | **24/24 PASS**, 107.45 s summed testcase time | Coordinator/race control PASS |
| Native Windows 10 integrated Release | **24/24 PASS**, 68.08 s summed testcase time | Native race control PASS |
| Native Windows 10 integrated Debug | **24/24 PASS**, 220.60 s summed testcase time | Native race control PASS |
| Linux integrated TSan | Maintained coordinator PASS | Instrumented root-writing races PASS, empty diagnostics |

The unchanged 24 registrations are `runtime_coordination`, `runtime_generated`,
`runtime_callbacks`, `runtime_failures`, `runtime_invariants`, `runtime_aot_normal`,
`runtime_aot_optimized`, `gc_buffers`, `gc_schemas`, `async_source`, `async_parallel`,
`async_lifecycle`, `async_external`, `native_workers`, `io_native_jobs`,
`io_native_cancelled`, `io_native_shutdown`, `io_native_pending_close`,
`io_native_opening_shutdown`, `io_native_whole_shutdown`,
`native_aot_integrated_cancel_normal`, `native_aot_integrated_cancel_optimized`,
`async_aot_spawn_cancel_normal`, `async_aot_spawn_cancel_optimized`.
Original assertions/limits cover precise/generated/AOT roots, timeouts/competing
epochs, callbacks/lifetime leases, native transitions, worker cancellation,
cleanup precedence and shutdown/drain behavior.

The unchanged disposable G31-P C++ fixture checks 20 barrier-controlled requests
that cannot finish while a participant is held Managed; 2,000 epochs with three
mutators changing registered root words and making NativeSafe transitions; final
zero-object collection after detachment; and 20 shutdown/admission-refusal races.
It requires a lock-free atomic bool on the actual target. No product hook is added.

Fatal strict environment:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, pinned symbolizer.
The existing no-RTTI LLVM ABI retains its vptr coverage limit. TSan instruments
Runtime/test C++ at `-O1 -g -fsanitize=thread` with
`TSAN_OPTIONS=halt_on_error=1:exitcode=88`. It does not qualify JIT-generated code
or every external provider internal. Exact argv, diagnostics and XML are archived.

Native guest: **Windows 10 Home x64 22H2 build 19045.6466**, despite its historical
VM name; VS 2022 x64, MSVC **14.44.35207**, SDK **10.0.19041.0**, LLVM 23.1.4, `/MT`.
Established provider helper paths are reused to retain SDK identity. As learned
in G31-P, the generated task CTest launcher points to byte-identical packet
`test_output.py`; original/used generated files are archived. This is not a
maintained CMake or test assertion change.

## Balanced baseline-versus-production performance

Same archived source bodies, sizes, independent oracles and controls as G31-P.
Four balanced forward/reverse blocks give 12 warm samples/case/mode/variant:
**1,008 application samples**. No slow sample is discarded. All Linux/Windows
build/test activity finishes before timing. CLI JIT uses `--no-cache`, with/without
`--no-opt`. Children have actual 1 MiB soft/hard native stacks, CPU 2 for ordinary
work and CPUs 2–5 for collection/cancellation. Shared desktop hardware, clustered
process samples and unflushed OS page caches limit inference. Go/SIMD baselines
are not rerun/replaced. Values are milliseconds: median [minimum–maximum], not
confidence intervals. Warm body timers exclude launcher wall.

### Application AOT

| Workload | Baseline | Integrated production | Median change |
| --- | ---: | ---: | ---: |
| integer-1000000 | 74.6346 [71.7883–78.0299] | 30.7863 [30.7169–31.3379] | -58.75% |
| integer-4000000 | 290.4977 [286.9411–303.3516] | 125.1061 [123.0236–136.3302] | -56.93% |
| dependent-1000000 | 71.5268 [70.7398–76.5908] | 31.0981 [30.4012–32.0461] | -56.52% |
| float-1000000 | 71.8993 [71.3480–73.3349] | 30.8448 [30.4079–32.8949] | -57.10% |
| array-1024 | 1.3324 [1.2804–1.3475] | 1.2129 [1.1798–1.2596] | -8.97% |
| array-4096 | 5.2738 [5.1275–5.5318] | 4.8103 [4.6911–4.9672] | -8.79% |
| sort-256 | 6.5991 [6.3763–6.7175] | 6.2730 [6.1032–6.4521] | -4.94% |
| sort-1024 | 32.1699 [31.6405–32.9126] | 30.9571 [30.5252–31.7179] | -3.77% |
| dict-1024 | 9.7711 [9.6311–10.0663] | 9.6456 [9.5182–10.0646] | -1.28% |
| text-256 | 14.2014 [14.0315–15.3814] | 13.7303 [13.5180–14.0528] | -3.32% |
| text-1024 | 58.9722 [58.1944–60.1799] | 56.9778 [56.0808–58.7008] | -3.38% |
| json-32 | 6.7867 [6.6219–7.0275] | 6.5999 [6.3489–6.6975] | -2.75% |
| json-128 | 43.4906 [42.8212–45.3113] | 41.6491 [40.5520–42.9846] | -4.23% |
| io-200 | 7.0245 [6.7814–10.3857] | 6.9126 [6.6392–7.1406] | -1.59% |

### Application JIT

| Workload | O2 baseline | O2 integrated | Normal baseline | Normal integrated |
| --- | ---: | ---: | ---: | ---: |
| integer-1000000 | 92.3952 [90.4721–93.7950] | 37.5899 [36.6504–39.3196] | 95.4916 [93.1716–105.9687] | 37.0873 [36.4196–38.9851] |
| integer-4000000 | 363.2693 [356.9933–476.6778] | 150.4314 [146.4122–151.1360] | 378.7303 [370.7129–424.0387] | 150.5057 [145.9400–155.2392] |
| dependent-1000000 | 91.6298 [88.4756–104.8730] | 37.2447 [36.2064–37.7838] | 97.3761 [94.3333–107.8124] | 37.1537 [36.4913–38.0564] |
| float-1000000 | 92.7600 [89.3535–96.6038] | 36.7003 [36.5156–37.6167] | 93.2691 [89.9978–106.5787] | 37.4819 [36.7474–38.2509] |
| array-1024 | 1.3998 [1.3789–1.4256] | 1.2553 [1.2118–1.2797] | 1.4019 [1.3833–1.4530] | 1.2595 [1.2202–2.1253] |
| array-4096 | 5.5312 [5.4296–5.7543] | 4.9682 [4.8384–5.2094] | 5.4969 [5.4722–5.5696] | 4.9769 [4.8144–5.2318] |
| sort-256 | 6.8423 [6.6760–7.0046] | 6.4414 [6.2981–6.7814] | 6.8667 [6.6555–7.2143] | 6.4577 [6.2847–6.6880] |
| sort-1024 | 33.9470 [33.0140–34.5460] | 31.7479 [31.1339–33.6403] | 33.9533 [32.4190–35.8552] | 31.6634 [30.9340–33.2459] |
| dict-1024 | 13.2149 [12.9147–13.4395] | 13.0739 [12.8554–13.3973] | 13.0888 [12.6825–13.5443] | 12.8151 [12.6641–13.0790] |
| text-256 | 15.8396 [15.5075–16.1035] | 15.1622 [14.6414–15.4419] | 15.9225 [15.6251–16.8365] | 14.8762 [14.5909–15.1560] |
| text-1024 | 64.7408 [63.2471–67.1663] | 62.7747 [60.3095–63.6265] | 66.3629 [62.9327–67.8113] | 62.2561 [60.1063–64.6251] |
| json-32 | 7.5457 [7.4874–7.8227] | 7.2772 [7.0821–7.5776] | 7.6189 [7.3325–8.0148] | 7.1941 [6.9289–7.6275] |
| json-128 | 50.9353 [48.5715–51.8772] | 47.0036 [45.1275–48.0558] | 49.5622 [48.5098–51.2279] | 46.8252 [45.1030–48.2932] |
| io-200 | 7.1737 [6.9634–16.4470] | 6.9636 [6.5886–10.1793] | 7.1555 [6.8696–10.4966] | 7.0814 [6.8714–10.3702] |

### Polling, pure numeric, collection and cancellation

| Control | Baseline | Integrated |
| --- | ---: | ---: |
| 4M worker checkpoints | 8.2053 [8.0265–8.4891] | 8.2109 [8.0272–8.4913] |
| 4M poll/checkpoint pairs | 70.9556 [69.3800–74.6244] | 33.3992 [32.1692–36.1394] |
| Pure AOT numeric 1M | 0.3975 [0.3773–0.4999] | 0.4018 [0.3774–0.5114] |
| Minimal managed O2 JIT 1M | 77.8590 [75.5597–84.2547] | 37.5362 [36.1671–38.6691] |
| Minimal managed normal JIT 1M | 76.9890 [76.1741–80.6425] | 37.4330 [36.5549–38.1067] |
| 200 requested epochs / three mutators | 0.9352 [0.7186–1.0088] | 0.7048 [0.2817–1.0026] |
| Maintained cancellation workload process wall | 1120.9750 [1093.3578–1153.6707] | 1164.4983 [1108.6431–1364.0160] |

### Startup, JIT graph compilation and native source build

| Control | Baseline | Integrated |
| --- | ---: | ---: |
| AOT factor-zero startup | 9.1055 [8.8285–10.6378] | 8.9115 [8.8099–9.1109] |
| O2 JIT startup + compilation | 3838.3225 [3796.2249–3885.9954] | 3844.4061 [3821.9455–3866.8135] |
| Normal JIT startup + compilation | 3142.9114 [3065.7090–3268.0383] | 3111.6801 [3082.9905–3175.9255] |
| Full application AOT source build/link | 3681.4464 [3645.3259–3754.6821] | 3665.8972 [3630.7306–3691.7912] |
| Launcher/sandbox true calibration | 6.3049 [6.0664–6.5468] | 6.0813 [5.9337–6.3438] |

Poll controls have 48 samples/mode/variant and check zero epoch/park deltas.
Pure/managed controls have 12 samples/variant; progress/cancellation and cold/build
have four process samples/variant. Progress verifies fixed epochs and root/drain
outcomes, but mutator poll counts vary; it is not isolated collector instruction
timing. Cancellation is unchanged `void_async_tests --parallel`, including source
compilation/progress/cleanup, not a single cancellation-acknowledgement bound.
Durable I/O checks 200 acknowledged/synchronized/closed 75-byte writes and exact
15,000-byte readback. Startup includes launcher/sandbox wall; no small favorable
median alone establishes a startup or compilation win.

## Regression/outlier analysis and disposition

The meaningful G31-P benefit reproduces in the integrated production source.
Four million poll/checkpoint pairs improve from **70.9556 to 33.3992 ms**;
worker-only checkpoints are effectively unchanged (**8.2053 / 8.2109 ms**).
Managed numeric application medians improve **56.5–61.9%** across AOT and both JIT
modes. Array medians improve roughly **9–10%**, sort **4–7%**, text/JSON **3–8%**.
Dictionary medians improve only **1–2%** with strongly overlapping ranges; no broad
dictionary throughput claim follows. Every primary application median is lower,
but durable I/O changes only **1.0–2.9%** with overlapping/noisy ranges and is not
an established I/O win. Pure AOT's integrated median is **1.07% higher**, with
nearly identical/overlapping ranges and byte-identical code. That unfavorable
control result is retained, with no pure-numeric optimization claim.

Primary cancellation process wall is **3.88% higher** (1120.9750 → 1164.4983 ms),
including a **1364.0160 ms** integrated maximum versus **1153.6707 ms** baseline.
Normal-JIT `array-1024` has one integrated **2.1253 ms** maximum exceeding baseline
**1.4530 ms**, although its integrated median is lower. Both triggered the smallest
unchanged-source follow-up: four balanced blocks of the complete normal-JIT suite
(to preserve preceding workload/GC context, another 336 checked samples), plus
four balanced process pairs of the unchanged cancellation workload.

| Follow-up control, separate cohort | Baseline | Integrated |
| --- | ---: | ---: |
| Normal-JIT array-1024, 12 body samples/variant | 1.4221 [1.3859–1.4996] | 1.2548 [1.2457–1.3070] |
| Cancellation process wall, four processes/variant | 1115.8387 [1091.7330–1133.2514] | 1108.6366 [1093.5362–1116.6547] |

The array tail did not reproduce: every integrated follow-up value is below the
baseline range. Cancellation's follow-up integrated median is **0.65% lower**,
with overlapping ranges. All outcomes/assertions passed. The original primary
samples remain unchanged and are not pooled away or replaced. There is no causal
proof for individual scheduling/desktop tails, nor an isolated cancellation-latency
or universal tail guarantee. Baseline O2-JIT integer-4M's **476.6778 ms** maximum
and all other unfavorable/baseline outliers are likewise retained in raw data and
full ranges. No repeatable representative regression was observed in these bounded
cohorts; no further implementation change is justified.

Startup/compilation changes are small and ranges overlap: AOT startup **-2.13%**,
O2 JIT startup/compilation **+0.16%**, normal JIT **-0.99%**, full application AOT
build/link **-0.42%**. Launcher calibration is **6.3049 / 6.0813 ms**, comparable to
the small AOT-startup difference; baseline's two additional source bind mounts
also belong to launcher wall. No startup or compilation benefit is established.
The VM remained running with no task build/test activity; a pre-cohort one-second
sample measured roughly 15% of one core in its background process. Shared desktop
activity was recorded rather than attributed as the cause of an individual tail.

**Disposition: retain the exact integrated two-file change for independent Golden
Wrench handoff.** Scoped Linux/Windows correctness, strict sanitizers and targeted
TSan passed; the relevant application gain reproduces. Collection progress/root
survival, native transitions, timeout/shutdown and worker cancellation remain
covered by unchanged assertions. No race, missed rendezvous, unsafe root observation,
deadlock or changed cancellation outcome was observed. This is completed local
integration engineering, not independent/hosted acceptance or promotion of a new
accepted performance baseline.


## Reproduction and preserved evidence

Archived `replay/build.py` creates fresh profiles. `baseline release` binds the
verified old files; inherited `candidate` labels mean the integrated checkout.
`qualification.py` builds selected consumers, `harness.py` derives CMake compile/
link settings for the controls, and `correctness.py` runs maintained registrations.
`prepare_performance.py` verifies codegen equivalence, `performance.py` checks all
oracles and balanced controls, and `summarize.py` reports every sample. Windows
`qualified.cmd` enters the established native VS environment, then `qualify.py`
uses the existing verified SDK/providers. Restore/remap task paths for replay;
installed SDK/provider roots and pinned inputs remain required. Exact argv,
environment, CMake caches/flags, source hashes, streams and XML are archived.

Original G31-P ZIP remains unchanged, SHA-256
`e2c619723b873c005b444ada24d2eeeab6b34051a751e5c371d6d849cb9aac1b`;
G31-R ZIP remains unchanged, SHA-256
`dc945f5d1c193cf3d5aae582f1407800ebeb3c12dac53021ab33d920853263cb`.
No historical unfavorable result or accepted benchmark reference is rewritten.

## Exact file inventory, archive, cleanup and Git

Checkout inventory:

| Path | Disposition |
| --- | --- |
| `include/void/runtime.hpp` | Modified: exact reviewed atomic request member/include |
| `src/runtime/coordination.cpp` | Modified: exact reviewed fast poll/CV predicate |
| `docs/compiler-health/G31_RUNTIME_POLLING_INTEGRATION.md` | New consolidated integration report |
| `docs/compiler-health/G31_VOID_GO_PERFORMANCE_BASELINE.md` | Pre-existing untracked G31-R report, byte-identical |
| `docs/compiler-health/G31_RUNTIME_POLLING_EXPERIMENT.md` | Pre-existing untracked G31-P report, byte-identical |

All other tracked files, maintained tests, root README, source inputs, original
accepted Linux/Windows binaries, toolchains/providers and earlier evidence are
preserved. Tracked diff is **two files, four insertions/two deletions**; staged
diff is empty. Existing build directories remain accepted-baseline references;
new header consumers must use matching freshly rebuilt Runtime objects. This
source integration does not install or activate a new binary package.

Verified ZIP: `/home/shmellyorc/Documents/Void.Language-archives/g31-runtime-polling-integration-2026-11-13.zip`.
SHA-256: `79db9830b1c9dd0893092ee44269e78c4b3f0515bf024b44b547d7a6f69171b1`.
**4,819 source/evidence files**, **10,519,192 original bytes**, **3,820,843 ZIP bytes**,
ordinary DEFLATE level 9. `_ARCHIVE_MANIFEST.json` records original path, source
SHA-256 and member SHA-256. CRC, full extraction, every restored-member hash and
source rehash passed before cleanup. The archive keeps exact old/integrated source,
source proof/diff, all primary/follow-up measurements and unfavorable tails,
coordinator/race fixtures, oracles/sources, sanitizer/Windows receipts, disassembly,
codegen identity, CMake caches/flags and replay scripts. Binaries/object trees and
the source transport ZIP are reproducible disposable outputs. The final report
remains here outside the ZIP, avoiding a circular report/archive hash.

The external `archive-index.json` and adjacent `g31-runtime-polling-integration-2026-11-13-{archive,cleanup,final}.json`
receipts record the archive and closing checks.

| Removed task-owned root | Files | Logical bytes | Allocated bytes |
| --- | ---: | ---: | ---: |
| `/home/shmellyorc/.local/state/void-g31i-ehvl8441` | 29,805 | 3,915,515,177 | 4,008,468,480 |
| `/home/shmellyorc/VM/g31-integration-20261113` | 34 | 14,121,705 | 14,213,120 |
| `C:\Users\a\VoidG31Integration-20261113` | 4,187 | 2,319,578,256 | Not measured |

Windows verified the same ZIP SHA-256/CRC before deletion. Its tools junction was
explicitly removed without following it; the established helper target still
exists. Host cleanup rechecked archived source hashes and absence of active task
working directories. Both host roots, the Windows task root and scratch pointer
are gone. No active dependency or unrelated build was removed.

Windows C: free bytes immediately before/after cleanup: **5,222,871,040 / 7,551,676,416**.
Host filesystem before/after: **358,173,134,848 / 362,178,199,552**.
`/tmp` before/after: **16,510,758,912 / 16,510,763,008**.
These are immediate observations, not peak usage or exact attribution of concurrent
filesystem/guest activity. Allocated host bytes may include shared extents.

Final `git diff --check`: **PASS**. The new report's explicit untracked-file
whitespace check passes; its two local Markdown links resolve. HEAD remains the
assigned baseline and the staged diff is empty. Exact final Git status:

```text
 M include/void/runtime.hpp
 M src/runtime/coordination.cpp
?? docs/compiler-health/G31_RUNTIME_POLLING_EXPERIMENT.md
?? docs/compiler-health/G31_RUNTIME_POLLING_INTEGRATION.md
?? docs/compiler-health/G31_VOID_GO_PERFORMANCE_BASELINE.md
```

No staging, commit, push, publication, hosted CI or baseline promotion occurred.


## Remaining platform boundaries

Native Linux correctness/performance and Windows 10 Debug/Release correctness
are bounded by the recorded profiles/hardware. Windows performance, macOS/native
Apple, other architectures/OS versions, clean-machine installation, portable
release binaries, exhaustive races and fairness/tail guarantees remain unqualified.
No managed SIMD or Go parity claim follows. Independent Golden Wrench review and
any later acceptance remain separate from this local integration.
No staging, commit, push, publication or hosted CI occurred.
