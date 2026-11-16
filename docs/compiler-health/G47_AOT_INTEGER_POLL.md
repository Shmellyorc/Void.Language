# G47 — AOT integer poll correction

Accepted baseline `894e2e8d9930028f86737092d3ac74010676f389`. The candidate on `codex/g47-aot-poll` carries the exact G47 sorting source. `Runtime::poll()` now handles the attached Managed thread's idle case with the existing TLS record and acquire load. Pending requests and invalid contexts go through the full validator. A valid thread then rechecks the request under the rendezvous lock. The checked worker checkpoint, emitted poll sites, allocator, root descriptors, collector and public ABI keep their existing behavior.

OpenJDK HotSpot uses an inline acquire check of a thread-local poll word and sends requests to a separate processing path. Go uses an existing thread-local stack guard to detect a cooperative preemption request on a cheap path. Julia uses poll pages and trap handling. VOID applies the ordinary fast-path/slow-path split with its existing TLS and request flag. It needs no second per-thread request flag, signal handler or new cancellation rule. [OpenJDK poll implementation](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/runtime/safepointMechanism.inline.hpp) · [Go stack guard](https://github.com/golang/go/blob/master/src/runtime/stack.go) · [Julia safepoints](https://github.com/JuliaLang/julia/blob/master/src/safepoint.c).

`Runtime::Attachment` initializes the record with its owner's thread ID, installs it in that thread's private TLS and clears the TLS before the record expires. The idle path checks runtime identity, the record pointer and Managed state before reading `requested_` with the existing acquire ordering. A foreign runtime, absent attachment or NativeSafe state reaches `require_managed()` and retains its established failure. A pending request reaches the same locked recheck and `park`. The new coordination test exercises both the idle poll and rejection of a foreign runtime from a Managed thread. The previous unattached, NativeSafe and rendezvous controls also execute.

## Synchronization review

Only the thread owning a record can install its address in TLS or change its state. Other threads inspect the state under the coordination mutex, and state transitions are published under the same mutex. The idle check reads the owner's TLS and state, then issues the existing atomic acquire load. A request racing with an idle return leaves the mutator Managed. The collector publishes its request under the mutex and waits for every participant to leave Managed before scanning roots. A thread that observes a request rechecks it under the mutex, publishes Parked and waits for the current request to clear. A thread that leaves Managed through its existing NativeSafe boundary publishes that transition under the same mutex. These transitions retain the synchronization that makes precise roots visible before the collector reads them. [C++ atomic ordering](https://eel.is/c++draft/atomics.order) · [C++ mutex synchronization](https://eel.is/c++draft/thread.mutex.requirements.mutex).

The patch changes neither the acquire ordering nor the locked request publications, GC epochs, cancellation checkpoints or emitted poll sites. The Windows x64 Release COFF disassembly shows TLS accesses via `gs:0x58`, validation of the thread pointer and state, and a call to the standard library's acquire-load helper. Invalid states and observed requests reach the out-of-line validator and rendezvous. The Linux Release hot path has a direct request-byte load. The throughput figures in this report apply to the measured Linux host.

## Matched normal and optimized AOT replay

The archived G44 and G47 application objects were hash-verified. With the unmodified Runtime, all four reference executables relinked byte for byte against the G47 archive. The experiment replaces only `abi.cpp.o` in the same static archive position. The fully rebuilt Release candidate and the measured overlay have byte-identical stripped executables in both modes. The unstripped hashes differ because the overlay's input source filename survives in symbol metadata. No layout-specific linker flags were used.

Eight balanced blocks compare the exact archived applications, with and without the corrected Runtime. Each application result passes the independent source oracles. Timing uses logical CPU 3 on the Ryzen 7 1700X and a hard 1 MiB stack. The table gives medians of block medians in milliseconds.

| AOT workload | G44 | G47 | G47 with poll fix | G44 with poll fix |
| --- | ---: | ---: | ---: | ---: |
| Normal integer-1M | 25.024 | 27.558 | 14.388 | 14.311 |
| Normal integer-4M | 98.589 | 108.541 | 57.153 | 57.039 |
| Normal dependent-1M | 24.911 | 27.091 | 14.324 | 14.321 |
| Normal sort-1,024 | 26.966 | 25.274 | 24.970 | 26.242 |
| Optimized integer-4M | 105.642 | 105.927 | 56.226 | 54.104 |
| Optimized sort-1,024 | 26.859 | 24.634 | 23.285 | 26.493 |

Normal integer-4M takes 47.3% less time than G47 and 42.0% less than G44. Applying the same fix to G44 puts both normal-AOT integer images within 0.2% at 4M by median. Normal integer-1M is within 0.6%. The original G47 linked-image sensitivity and its particular microarchitectural cause remain unresolved. Removing the hot call solves the observed arithmetic throughput problem in both images on this host. The optimized array controls show remaining median costs of +1.57% at 1,024 and +1.06% at 4,096 versus G44. Their matched-pair medians are +1.34% and +0.83%, with individual pairs ranging from −7.70% to +5.36% and from −7.32% to +4.40%. All controls and paired measurements are preserved.

The integer loop performs no managed allocation. It repeatedly takes the no-request poll and the no-worker checkpoint paths. Native disassembly shows the old always-executed `require_managed()` call and the replacement inline TLS/state check. The timing measures the cost of checking for GC in that loop. Collector marking, sweeping and pauses require their own measurements.

## Final optimized AOT replay

Eight additional balanced blocks replayed the exact G44 and G47-with-poll linked images after Windows qualification ended. The SHA-256 identities matched the original measurements. The replay used the same Ryzen 7 1700X, logical CPU 3, observer-off execution and a hard 1 MiB stack. Independent oracles passed on all 672 samples across 14 workloads.

| Workload | G44 median, ms | G47 with poll median, ms | Change | Matched-pair median change |
| --- | ---: | ---: | ---: | ---: |
| Array-1,024 | 1.054269 | 1.058919 | +0.44% | +0.53% |
| Array-4,096 | 4.160642 | 4.171148 | +0.25% | +0.50% |
| Integer-4M | 105.173 | 54.996 | −47.71% | −47.75% |
| Sort-1,024 | 26.535 | 22.803 | −14.06% | −13.79% |

The array pairs range from −4.94% to +2.80% and from −6.94% to +2.02% respectively. Both archived and fresh runs show a small positive median array cost with pairs in both directions. I/O-200 was +0.79% by ratio of medians and −0.01% by matched-pair median. Raw results retain each adverse pair. The replay establishes the measured tradeoffs on one host, with Windows throughput and independent physical-host portability still open.

## Qualification

The 3,319-input manifest for native qualification differs from the qualified sorting candidate only in `src/runtime/abi.cpp` and `tests/runtime/coordination.cpp`. All four code and test postimages match the prior poll archive. The complete sorting report and its earlier 105/105 Linux Release, 104/104 fatal sanitizer and 71/71 Windows Debug and Release receipts preserve the evidence for the preceding sorting candidate. [G47 sorting report](G47_SORTING_BOUNDS_OPTIMIZATION.md).

The exact combined candidate's retained Linux Release, fatal ASan/UBSan/LeakSanitizer and fatal TSan receipts each cover 2/2 coordination and invariant tests. They cover NativeSafe and wrong-Runtime handling, collection timeouts, competing collectors, precise roots, corrupt-root rejection and stress. Sixteen independently linked normal and optimized AOT sort, worker and async oracle cases pass. All 2,688 original timing samples pass independent output checks.

Native Windows 10 x64 build 19045 passed 29/29 focused tests in both Debug and Release with LLVM, Clang and LLD 23.1.4 at `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. Both profiles exercised stop-the-world coordination, checked failure recovery, precise roots, GC buffers and schemas, callback reentry, worker shutdown, cancellation and async execution. Fourteen independently linked async AOT tests per profile covered normal and optimized emission. Generated normal and O2 JIT each performed exactly 2,000,003 polls and 100 completed collections per profile, including an enforced rendezvous with a Managed mutator. The observed cooperative parks were 2 and 3 in Debug and 3 and 6 in Release. Generated nested roots, managed returns and failure cleanup also passed.

Eight additional Windows cases matched independent stable-sort and exact comparator-failure oracles across both profiles and optimization modes, with one JIT and one independently linked AOT result per case. All 20 checked test and compiler PE headers and all eight separately linked sort PE headers reserve 1 MiB. The runner rechecked all 3,319 source input hashes and the protected SDK and provider hashes after both profiles.

**Recommendation — integrate the exact combined G47 candidate after review.** Keep the small optimized AOT array costs and the host-specific scope of the performance result visible. The final qualification evidence is `g47-aot-poll-final-qualification-2026-11-16.zip`, with the replay, Windows logs and disassembly, retained Linux receipts, exact postimage inventory and a source-restorable patch. The earlier sorting and poll archives keep their original reports and evidence.
