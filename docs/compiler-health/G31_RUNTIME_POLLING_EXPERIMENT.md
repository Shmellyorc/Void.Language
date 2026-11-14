# G31-P — disposable Runtime polling fast-path experiment

13 November 2026. **Recommendation: retain this minimal candidate for independent
Golden Wrench review; no production integration is authorized or performed.**
Baseline/final HEAD: `189be08423bdc2beb91183b0608b018190dc7524`, plus the unchanged
[G31-R research report](G31_VOID_GO_PERFORMANCE_BASELINE.md). The candidate was
built only in task-owned copies; its exact sources/diff now remain in the verified ZIP.

## Baseline and isolation

The active compiler matches Research exactly: SHA-256
`8a3f3fdbdf2ff2b84a51640870a9f0de1633cb2898bd5c080d0fbc5dc222b28b`,
build identity `4abda349c62caf231abda0f8bb49afd1d0d4f69763aae47f252e01b433d713c7`.
LLVM/Clang/LLD **23.1.4** and the existing verified provider profiles were reused.
The archived direct-call probe reproduced zero epochs/parks, approximately
71 ms for four million poll/checkpoint pairs and 8.2 ms for checkpoints alone.
The archived minimal managed numeric source reproduced checksum **62,999,875**
and 75.9401–76.2444 ms in the initial current-production control. This is a new
host cohort, not a claim of Research's identical wall times or old SIMD results.

Matched fresh Release builds use the same source paths, pinned dependencies,
`-O3 -DNDEBUG`, C++20 and LLVM-compatible no-RTTI ABI. A read-only Bubblewrap view
binds only the candidate header/coordination file over their logical source paths;
all build outputs are task-owned. Timed baseline/candidate commands use symmetric
two-file bindings. No production file was overwritten, even temporarily.

The matched baseline Runtime archive is **byte-identical** to production:
4,902,966 bytes, SHA-256
`4b2354548e778741d2f1c99b2fbc70b9a8455f1465df2967f96a8d726d5a2f66`.
Candidate archive: 4,903,006 bytes, SHA-256
`1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`.
**49 of 50 Runtime source objects are byte-identical**; only coordination differs.
Every candidate Runtime consumer was rebuilt with its matching header. No modified
layout was linked against old Runtime objects. Probe size/alignment is **1008/8**
on Linux and **1088/8** on Windows; these are platform observations, not a universal ABI promise.

## Original contract, minimal candidate and synchronization

Original `Runtime::poll()` first validates attachment/Managed state, then always
locks `mutex_` and parks if `requested_` is true. `vd_gc_poll` invokes that path;
`vd_worker_checkpoint` separately handles worker cancellation. Compiler-inserted
polls/checkpoints, frequency, allocation budget **256**, precise roots and all
backend/TargetMachine/SIMD/SLP policies remain unchanged.

Attachment/detachment, NativeSafe↔Managed transitions, participant observations,
request/epoch/result changes, root/heap scanning and shutdown admission remain
under the original mutex and lifecycle protocols. Frame/root words may be updated
by their owner while Managed. A collector scans only after **all** participants
are NativeSafe or Parked. Their locked transition and mutex release, including CV
wait release, publish prior frame/root writes; the collector's mutex reacquisition
establishes happens-before before scanning. An ordinary no-request lock acquisition
is not scanning authority. Only each participant writes its own state; cross-thread
observations stay locked.

Candidate: make the **existing** request flag atomic, use a false acquire load to
return after Managed validation, and keep the locked recheck/park authoritative.
The existing true/false assignments remain inside the mutex and now perform
conservative default seq_cst stores. No mirror flag or second coordination system
is introduced. The CV predicate needs an explicit boolean load because an atomic
itself cannot be returned by copying.

```diff
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
@@ -280,7 +281,7 @@
     std::thread::id async_owner_ = {};
     std::size_t native_jobs_ = 0;
     bool closing_ = false;
-    bool requested_ = false;
+    std::atomic<bool> requested_{false};
     std::size_t callbacks_ = 0;
     std::size_t allocation_budget_ = 0, allocations_ = 0;
     std::uint64_t epoch_ = 0, completed_epoch_ = 0;
--- a/src/runtime/coordination.cpp
+++ b/src/runtime/coordination.cpp
@@ -135,6 +135,7 @@
 }
 void Runtime::poll() {
     require_managed();
+    if (!requested_.load(std::memory_order_acquire)) return;
     std::unique_lock lock(mutex_);
     if (requested_) park(thread(), lock);
 }
@@ -325,7 +326,7 @@
 bool Runtime::wait_for_request(std::chrono::milliseconds timeout) {
     if (current_runtime_ == this && thread().state == State::managed) throw std::logic_error("Managed thread cannot wait for a request");
     std::unique_lock lock(mutex_);
-    return changed_.wait_for(lock, timeout, [&] { return requested_; });
+    return changed_.wait_for(lock, timeout, [&] { return requested_.load(std::memory_order_relaxed); });
 }
 void Runtime::async_worker_limit(unsigned count) {
     if(!count || count>64)throw std::invalid_argument("worker limit must be 1..64");
```

The race argument is:

- A request after a false observation still sees that participant **Managed**.
  It must wait; no unsafe root scan is possible. A preserved later poll or the
  locked NativeSafe transition participates in rendezvous. The fast flag never
  publishes roots or permits an unlocked heap/epoch read.
- A stale true observation merely takes the mutex. If completion/timeout or a new
  epoch occurred before acquisition, the current locked predicate decides whether
  to park. Park continues waiting across successive requests; existing losing-
  collector result propagation, timeout/no-sweep and epoch-overflow rules remain.
- Native→Managed entry and final detachment still wait for no active request under
  the mutex. A false fast observation cannot bypass those entry/lifetime gates.
- `closing_`, callback admission/quiescence, native-job drain and retained code
  leases keep their existing protocols. Poll never supplied a shutdown or
  cancellation exception. Existing entries can drain; new entries are refused.
- Worker cancellation retains its separate atomic load and every existing
  predicate/checkpoint. Work/native-completion outcomes retain release/acquire
  handoffs and notification locks. Cleanup-failure precedence is unchanged.
- The relaxed `wait_for_request` load is inside its original locked CV predicate;
  mutex happens-before prevents an observation older than a preceding protected
  store. No plain unlocked request read exists.

This argument and bounded testing do not prove scheduler fairness or a hard
wall-time rendezvous bound. Platform-coherent atomic progress is exercised below;
ASan/UBSan alone would not establish data-race freedom.

## Maintained correctness and race-oriented controls

| Configuration | Results |
| --- | --- |
| Matched Linux baseline Release | **24/24 maintained CTests PASS** (coordination plus 23 selected regressions); custom race control PASS |
| Linux candidate Release | **24/24 PASS**; custom race control PASS |
| Linux candidate strict | **24/24 PASS** (107.47 s); additional direct coordination and custom race controls PASS |
| Linux candidate ThreadSanitizer | Instrumented Runtime + maintained coordinator **PASS**; instrumented root-writing race control **PASS**, no diagnostics |
| Native Windows 10 candidate Release | **24/24 PASS** (61.77 s) after correcting a missing task launcher; native race control PASS |
| Native Windows 10 candidate Debug | **24/24 PASS** (215.92 s); native race control PASS |

The selected registrations are:
`runtime_coordination`, `runtime_generated`, `runtime_callbacks`, `runtime_failures`,
`runtime_invariants`, `runtime_aot_normal`, `runtime_aot_optimized`, `gc_buffers`,
`gc_schemas`, `async_source`, `async_parallel`, `async_lifecycle`, `async_external`,
`native_workers`, `io_native_jobs`, `io_native_cancelled`, `io_native_shutdown`,
`io_native_pending_close`, `io_native_opening_shutdown`, `io_native_whole_shutdown`,
`native_aot_integrated_cancel_normal`, `native_aot_integrated_cancel_optimized`,
`async_aot_spawn_cancel_normal`, `async_aot_spawn_cancel_optimized`.
Original assertions cover timeout/interrupted rendezvous, competing epochs,
precise/generated/AOT roots, native transitions, callbacks/reload/retained leases,
running-worker cancellation, cleanup precedence and native shutdown/drain.

The extra C++ control performs 20 barrier-controlled pending requests (the
collector cannot finish while the participant is held Managed), 2,000 epochs with
three attached mutators changing registered root words and making NativeSafe
transitions, final zero-object collection after detachment, and 20 shutdown/
attachment-refusal races. It requires a lock-free atomic bool on the actual target.
No measured-path instrumentation or Runtime test hook was added.

Strict uses fatal ASan/UBSan with leak detection:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, pinned symbolizer.
The existing no-RTTI LLVM ABI retains its vptr coverage limit. TSan uses
`-fsanitize=thread`, `-O1 -g`, `TSAN_OPTIONS=halt_on_error=1:exitcode=88` on the
Runtime and C++ probe. This is not TSan coverage of JIT-generated code or every
external provider operation. Neither sanitizer is a universal proof.

Windows is the existing **Windows 10 Home x64 22H2, build 19045.6466** guest,
with MSVC **14.44.35207**, SDK **10.0.19041.0**, native x64 VS 2022 environment,
LLVM 23.1.4 and `/MT`. Its historical VM name does not identify its OS.
Fresh task-local Debug/Release builds use complete matching headers, verified
existing SDK/providers and private NTFS outputs. A two-line task-only CMake target
links the extra probe through the existing Runtime interface. No hosted CI,
clean-machine install, Windows performance campaign or macOS qualification is claimed.

## Balanced performance results

Same archived source bodies, inputs, numeric sizes and independent output oracles;
all outcomes passed. Four forward/reverse balanced blocks, one warmup and three
samples per case/process give **12 samples per case/mode/variant**, **1,008 primary
suite samples**. No failed or slow sample was discarded. CLI JIT uses `--no-cache`
with/without `--no-opt`; AOT is independently linked. All timed children have actual
**1 MiB soft/hard native stacks**. Ordinary cases use CPU **2**; requested-progress
and cancellation controls use CPUs **2–5** for their attached threads.

Build/test activity finished before timing, and Windows qualification started
only after the primary cohort. This is the shared Ryzen/PikaOS host, not an isolated
lab; samples are clustered within processes. OS page caches were not evicted.
Go was not reinstalled/rerun and historical SIMD/Go timings are not replacement baselines.
Milliseconds below are **median [minimum–maximum]**, not confidence intervals.

### Application AOT

| Workload | Baseline | Candidate | Median change |
| --- | ---: | ---: | ---: |
| integer-1000000 | 75.2318 [71.5156–79.2101] | 31.9860 [31.8537–32.4707] | -57.5% |
| integer-4000000 | 292.7758 [286.3475–302.9411] | 125.5323 [124.3858–125.9606] | -57.1% |
| dependent-1000000 | 71.9476 [70.5894–76.2349] | 31.2342 [31.0510–32.2595] | -56.6% |
| float-1000000 | 72.7515 [71.7650–79.1962] | 30.8557 [30.4901–31.6600] | -57.6% |
| array-1024 | 1.3272 [1.3068–1.3492] | 1.2106 [1.1899–1.2475] | -8.8% |
| array-4096 | 5.3062 [5.2482–5.3985] | 4.8201 [4.7351–4.8848] | -9.2% |
| sort-256 | 6.6180 [6.5564–6.7439] | 6.3155 [6.0657–6.5223] | -4.6% |
| sort-1024 | 32.6668 [32.4644–33.3243] | 31.0261 [30.3298–32.0993] | -5.0% |
| dict-1024 | 9.9922 [9.7246–10.1297] | 9.8617 [9.6705–10.0432] | -1.3% |
| text-256 | 15.0941 [14.7677–15.2671] | 14.1187 [13.9099–14.6759] | -6.5% |
| text-1024 | 62.1970 [60.8852–63.4233] | 59.4724 [57.5991–60.2379] | -4.4% |
| json-32 | 7.0844 [6.9244–7.5752] | 6.6912 [6.5036–7.1414] | -5.6% |
| json-128 | 45.6779 [45.0093–46.4006] | 42.6265 [41.2415–43.4851] | -6.7% |
| io-200 | 7.0246 [6.8496–9.9362] | 7.0386 [6.8161–10.0262] | +0.2% |

### Application JIT

| Workload | O2 baseline | O2 candidate | Normal baseline | Normal candidate |
| --- | ---: | ---: | ---: | ---: |
| integer-1000000 | 94.4628 [89.4455–97.8874] | 37.0367 [36.4349–37.9867] | 93.8273 [92.1890–96.6668] | 37.6789 [37.4868–51.0039] |
| integer-4000000 | 368.6495 [359.2812–390.2086] | 146.9707 [145.9404–150.5391] | 379.7176 [358.9567–389.3258] | 149.7363 [146.1236–154.8736] |
| dependent-1000000 | 92.4808 [88.5633–93.2927] | 36.2803 [36.1739–37.2858] | 95.4558 [94.4540–97.6984] | 36.6185 [36.5191–38.7370] |
| float-1000000 | 95.9405 [93.8797–97.6002] | 37.3745 [36.4337–37.9512] | 91.2329 [88.9208–95.5446] | 37.8522 [36.8576–41.9814] |
| array-1024 | 1.4216 [1.4077–1.4801] | 1.2584 [1.2274–1.2943] | 1.3854 [1.3515–1.4709] | 1.2841 [1.2093–1.3319] |
| array-4096 | 5.5747 [5.4886–5.7634] | 5.0058 [4.8730–5.1446] | 5.5013 [5.3114–5.6353] | 5.0411 [4.7315–5.6293] |
| sort-256 | 6.9863 [6.7687–7.1857] | 6.5199 [6.1296–6.8686] | 7.0067 [6.8215–7.7296] | 6.4968 [6.2882–11.0074] |
| sort-1024 | 34.4159 [33.3932–36.2091] | 31.7349 [30.3323–32.6350] | 34.2261 [33.7853–34.5681] | 32.0089 [31.2008–37.2667] |
| dict-1024 | 13.0160 [12.8494–13.7050] | 12.8922 [12.6686–13.3407] | 13.2565 [12.7428–13.5890] | 13.0312 [12.5371–13.2493] |
| text-256 | 15.8532 [15.6505–16.2002] | 14.8800 [14.3410–15.2649] | 16.2885 [15.3206–16.9176] | 14.9635 [14.7606–15.7788] |
| text-1024 | 66.1565 [64.8847–68.3750] | 61.5416 [59.9415–63.3540] | 67.8194 [63.6396–69.8863] | 62.5093 [61.2426–64.8789] |
| json-32 | 7.7091 [7.4433–8.5031] | 7.3100 [7.0400–7.6878] | 7.8402 [7.4489–8.2848] | 7.4770 [7.1896–7.9673] |
| json-128 | 50.5707 [49.4482–51.2743] | 46.2694 [45.5998–48.0150] | 50.5974 [49.4680–51.6605] | 48.2957 [47.2360–72.3460] |
| io-200 | 7.3002 [6.5961–10.4650] | 6.9542 [6.7765–7.5006] | 7.2791 [6.6818–33.1489] | 7.2606 [3.2406–10.1912] |

### Direct, pure, progress and cancellation controls

| Control | Baseline | Candidate |
| --- | ---: | ---: |
| 4M worker checkpoints only | 8.2112 [8.0431–9.8975] | 8.2116 [8.0459–8.5228] |
| 4M poll + worker checkpoint pairs | 70.8702 [69.1998–75.7249] | 32.8696 [32.1953–35.4691] |
| Pure AOT numeric, 1M | 0.4045 [0.3773–0.5243] | 0.3989 [0.3840–0.4206] |
| Minimal managed numeric, O2 JIT, 1M | 77.0337 [75.6357–81.1275] | 37.1054 [36.4175–38.9215] |
| Minimal managed numeric, normal JIT, 1M | 77.9665 [76.0349–79.9581] | 37.6190 [36.4916–38.9292] |
| 200 collection epochs, three mutators | 0.9440 [0.3499–1.8919] | 0.6714 [0.4938–1.0074] |
| Maintained worker-cancellation workload, process wall | 1155.9436 [1073.4554–1274.9672] | 1122.2896 [1102.2677–1146.4582] |

Direct polling has **48 samples per mode/variant**, with **zero epoch/park deltas**.
Pure/managed controls have 12 samples per row/variant. Progress/cancellation have
four process samples per variant. The progress workload verifies fixed epochs and
root/drain outcomes, but mutator poll counts vary; it is not isolated collector
instruction timing. Cancellation is the unchanged `void_async_tests --parallel`
workload, including source compilation/progress/cleanup assertions, not a bound on
one cancellation acknowledgement.

The pure and coordinated emitted objects are **byte-identical** between variants,
as is the compiled backend object. Research's relevant IR is retained/referenced,
and new poll assembly shows the atomic byte test/return before the mutex call.
The candidate preserves scalar managed-loop safepoints and worker calls: it does
not enable managed SIMD or establish Go-equivalent application throughput.

### Startup and compilation

| Operation, four balanced process samples/variant | Baseline | Candidate |
| --- | ---: | ---: |
| AOT factor-zero process startup | 9.2584 [8.6604–11.1304] | 9.4034 [9.0060–10.0276] |
| O2 JIT factor-zero startup + graph compilation | 3824.8250 [3803.3885–4351.1041] | 3831.3151 [3813.3791–4200.2485] |
| Normal JIT factor-zero startup + graph compilation | 3183.0089 [3110.3399–3255.5650] | 3180.2816 [3102.0288–3308.4437] |
| Full suite AOT source build/link | 3744.7306 [3689.5073–3839.5480] | 3780.3963 [3728.6787–3864.6591] |
| Paired sandbox/launcher true-command calibration | 6.3126 [6.0895–7.3868] | 6.1355 [5.9183–6.6372] |

The approximately 6 ms launcher/sandbox cost is included in process walls and
matters to the small native-startup rows. Warm body timers exclude it. Compiler/JIT
startup remains largely unchanged in this cohort; AOT compilation's candidate
median is **0.95% higher**, AOT startup **1.57% higher**, O2 JIT startup **0.17%
higher**, with overlapping ranges. These unfavorable medians are preserved and
are not evidence of an established compilation/startup win.

### Variability and regressions

The large managed numeric gain survives the complete application context. Array,
sort, text and JSON median gains are smaller; dictionary ranges overlap strongly.
Durable AOT I/O's candidate median is **0.20% higher**, with overlapping distributions;
no durable-I/O improvement is established. Exact 200×75-byte acknowledgements,
synchronization, close and 15,000-byte readback remain checked.

Primary normal-JIT candidate sort/JSON maxima of **37.2667 / 72.3460 ms** exceeded
primary baseline maxima **34.5681 / 51.6605 ms**, despite lower candidate medians.
A separate four-balanced-block **unchanged-source normal-JIT follow-up**, with the
VM build idle, preserved the full preceding workload/GC context and all oracles.
It produced another 12 samples per case/variant (336 suite samples overall):

| Follow-up case | Baseline | Candidate |
| --- | ---: | ---: |
| sort-1024 | 34.2189 [33.2867–35.9501] | 31.3356 [30.9081–31.7346] |
| json-128 | 50.3743 [48.8114–52.4502] | 46.9773 [45.9841–48.5246] |

The slower tails did not reproduce; every candidate follow-up value for those two
cases was below the baseline range. Original primary samples remain untouched.
There is no causal proof for individual desktop/scheduling outliers or a tail-latency
guarantee. Baseline normal-JIT I/O's **33.1489 ms** maximum is also retained.
No reproducible representative regression, missed collection/cancellation, unsafe
root observation, deadlock or sanitizer finding was observed in this bounded work.

## Failures, disposition and remaining limits

The first Windows Release run was **16/24 PASS, eight launcher failures**: the
retained provider-helper junction lacks `test_output.py`. No native test body ran
for those eight. Only the generated task CTest launcher path was redirected to
the byte-exact maintained helper in the complete source packet. The affected
**8/8** passed first, then the full **24/24**, without a Runtime fix or weakened
assertion. Debug used the corrected path. Original/corrected logs and XML remain.
The task inventory helper was renamed from `inspect.py` to avoid shadowing Python's
stdlib during exception formatting. An initial demangled-objdump filter returned
no body; the corrected filter produced checked assembly. Neither is a product repair.

**Retain for review** is justified by the minimal two-file production-source delta,
explicit synchronization argument, practical race instrumentation, unchanged
safepoints/worker/root protocols and consistent relevant application benefit.
It is not production acceptance. Golden Wrench/Boss must independently review the
candidate and choose any later integration/qualification. No macOS/native Apple
result, portable binary, exhaustive race proof or broad performance promise follows.
All production changes remain absent; no unrelated optimization or new Runtime,
scheduler, GC policy or coordination subsystem was introduced.

## Evidence, storage and Git

The final verified-ZIP inventory and cleanup measurements follow below. Exact
baseline/candidate sources and diff, archived benchmark sources/oracles, primary
and follow-up samples, original failures, relevant IR/assembly, identities,
CMake/ABI flags, synchronization notes, maintained regression receipts and Windows
source/launcher fixture records are retained. Rebuildable binaries, object trees,
transport ZIP and task profiles are disposable. Active toolchains/providers,
production builds, original VM source/editor state and earlier G26–G31 evidence
are preserved. No staging, commit, push, publication, signing or hosted CI occurred.

Verified archive:
`/home/shmellyorc/Documents/Void.Language-archives/g31-runtime-polling-experiment-2026-11-13.zip`.
SHA-256: `e2c619723b873c005b444ada24d2eeeab6b34051a751e5c371d6d849cb9aac1b`.
**321 source/evidence files**, **9,119,791 uncompressed bytes**, **1,186,474 ZIP bytes**,
DEFLATE level 9. CRC, complete extraction, restored member hashes and original
rehashes passed before cleanup. The embedded `_ARCHIVE_MANIFEST.json` gives the
exact member inventory and original/member SHA-256 values. The existing external
archive index records verification and cleanup; adjacent archive/cleanup/final JSON
receipts retain the closing checks. Research's earlier ZIP remains unchanged at
`dc945f5d1c193cf3d5aae582f1407800ebeb3c12dac53021ab33d920853263cb`.

| Removed task-owned root | Logical bytes | Allocated bytes |
| --- | ---: | ---: |
| Linux `/home/shmellyorc/.local/state/void-g31p-68styf8v` | 4,007,521,968 | 4,087,468,032 |
| Host transport `/home/shmellyorc/VM/g31-polling-20261113` | 11,503,397 | 11,632,640 |
| Windows `C:\Users\a\VoidG31Polling-20261113` | 2,318,909,353 | Not measured |

Windows checked the same archive hash and CRC before cleanup. Its tools junction
was removed without following it; the established helper target still exists.
The task roots and scratch pointer are gone. Host filesystem free bytes before/
after Linux/transport cleanup: **358,091,661,312 / 362,194,903,040**. Windows C:
before/after its cleanup: **6,474,690,560 / 8,802,295,808**. `/tmp`:
**16,510,881,792 / 16,510,885,888**. These are immediate end-of-task observations,
not peak storage or exact attribution of concurrent filesystem activity.

Exact checkout inventory: new untracked
`docs/compiler-health/G31_RUNTIME_POLLING_EXPERIMENT.md`; pre-existing untracked
`docs/compiler-health/G31_VOID_GO_PERFORMANCE_BASELINE.md`, unchanged.
**Tracked production/source/test/build configuration diff empty; staged diff empty.**
Final `git diff --check`: **PASS**; the new report also passes the explicit
untracked-file whitespace check. HEAD remains the assigned baseline. The production
header, coordinator, compiler and Runtime archive hashes match their original
identities; the original Windows header/coordinator match the baseline source.
Root README and all other tracked files remain unchanged.
