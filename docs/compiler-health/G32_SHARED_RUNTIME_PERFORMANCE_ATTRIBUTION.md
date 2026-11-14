# G32-R — Shared Runtime and standard-library performance attribution

13 November 2026. Research baseline `0407f83f9d6dd660b1503df1e68cdcee9b34b77b`. Includes the escape-analysis, Language Bible and FILE200 addenda. **HOLD production changes from this experiment.** Golden Wrench reviews; Boss controls integration, CI and release.

The largest demonstrated opportunity is the accumulated cost of **checked element operations and temporary construction**, shared by text, JSON and collections. The evidence does not isolate one safely removable dominant check. Sorting is slow with only 42 managed constructions and no collection; replacing the collector or allocator would not solve that workload. Text and JSON additionally construct thousands of objects and perform tens of thousands of native allocation requests. These are different contributions, not interchangeable explanations.

One small root-search correction was tested and rejected: `std::find` preserves the required first-match behavior, but produces mixed microbenchmark results and representative end-to-end regressions. Do not integrate it. General managed escape analysis is absent in the inspected pipeline and probes. Existing scalar promotion, explicit value storage and specialized cleanup elimination are real optimizations; none establishes general managed allocation elimination. No new escape-analysis pass, allocator or GC is justified by these measurements alone.

## 1. Accepted baseline and methodology

The active `build/llvm23-release` compiler/Runtime were stale. Their identities were checked before measurement; they were not used as the performance baseline and were not overwritten. A fresh disposable Release compiler and Runtime were built from the exact accepted source using LLVM/Clang/LLD **23.1.4**, matched headers, pinned host/HTTP dependencies and the normal CMake recipes.

| Artifact | SHA-256 |
| --- | --- |
| Fresh current compiler | `b1c53ba0b0bf9fb5409389e7b38c4e18e225b640bc93ee2d10d03b48a6345712` |
| Fresh current Runtime; matches accepted G31-I | `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc` |
| Current `runtime.hpp` | `72907e5137525f0341aa2c6d086ac8d3cad8e6e53b9a9f7f992511af45827373` |
| Current `coordination.cpp` | `b017d8932b4fbf14115f1b508bb9d6c09f33811e95ef8b62becf02c96f462f71` |
| Stale active Runtime, excluded | `4b2354548e778741d2f1c99b2fbc70b9a8455f1465df2967f96a8d726d5a2f66` |
| Disposable root-search candidate Runtime | `0e7c61300718b89fd8c4209c0613f185575f85822f46ec4ff1e0a5d5b8bb0868` |

[Accepted G31 polling integration](G31_RUNTIME_POLLING_INTEGRATION.md), [original G31 baseline](G31_VOID_GO_PERFORMANCE_BASELINE.md), [G31 polling research](G31_RUNTIME_POLLING_EXPERIMENT.md), [earlier internal audit](INTERNAL_PERFORMANCE_AUDIT.md) and [accepted internal repairs](INTERNAL_PERFORMANCE_REPAIR_RESULT.md) establish precedence over older descriptions.

Primary measurements reuse the archived G31 source and independent oracles unchanged: 14 workload/size cases, normal JIT, uncached O2 JIT and independently linked AOT; four balanced forward/reverse blocks and three timed samples per case. All **1,008 timed output checks passed**. Compilation/linking are outside warm timers. Cold startup and native build timings are separate. CPU 2 affinity and actual soft/hard **1,048,576-byte stacks** were applied to measured children. This is the same Ryzen 7 1700X Linux desktop, not a dedicated benchmark machine. No timed campaign overlapped the private compiler builds. Raw outliers and negative results remain preserved.

Private observation hosts reuse `experiments/internal-performance` counters and `experiments/log-integrated` nested timers, with observation-off controls. Native allocation wrappers count C++ allocation requests, not OS allocations; requested bytes are not RSS or exact GC payload. Managed construction counters include ordinary/string allocation and buffer construction attempts. Valid fixtures succeeded. Selected UTF-8-copy counters do not cover every copy. No counts were obtained by disabling checks or collections.

**Instrumentation limitation:** per-operation wall/CPU clocks substantially perturb the text/JSON probes; full timing observation expanded sorting from roughly 33 ms to about 497 ms. Those instrumented exclusive times are not production CPU shares. Counts and separate uninstrumented suboperation probes establish mechanisms; they do not establish that every counted check is redundant. The logger uses the lighter existing steady-clock instrumentation and still has separately reported perturbation.

## 2. Matched current/candidate results

Milliseconds; median of 12 observations per workload/variant/mode. Positive Δ is a regression. Full ranges, per-block changes, CPU/RSS, throughput and normal-JIT rows are in `evidence/summary.json`.

| Workload | Current AOT ms | Candidate AOT ms | Δ | Current O2 JIT ms | Candidate O2 JIT ms | Δ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| array 1,024 | 1.1771 | 1.2061 | +2.5% | 1.2330 | 1.2741 | +3.3% |
| array 4,096 | 4.7073 | 4.7860 | +1.7% | 4.8469 | 5.0641 | +4.5% |
| dependent 1,000,000 | 31.0037 | 32.1911 | +3.8% | 36.2852 | 36.2642 | -0.1% |
| dict 1,024 | 9.8825 | 9.6707 | -2.1% | 12.9791 | 13.0461 | +0.5% |
| float 1,000,000 | 30.7817 | 31.6333 | +2.8% | 36.5814 | 36.7153 | +0.4% |
| integer 1,000,000 | 31.4829 | 31.9602 | +1.5% | 36.5482 | 36.6106 | +0.2% |
| integer 4,000,000 | 124.5447 | 126.4697 | +1.5% | 146.2626 | 146.1903 | -0.0% |
| io 200 | 6.8494 | 6.8465 | -0.0% | 7.1988 | 6.9079 | -4.0% |
| json 32 | 6.5460 | 6.5083 | -0.6% | 7.0635 | 7.3022 | +3.4% |
| json 128 | 42.8982 | 41.2176 | -3.9% | 45.7632 | 46.9174 | +2.5% |
| sort 256 | 6.1682 | 6.0491 | -1.9% | 6.3325 | 6.4259 | +1.5% |
| sort 1,024 | 30.5599 | 30.0290 | -1.7% | 31.2997 | 31.6818 | +1.2% |
| text 256 | 14.2616 | 14.2861 | +0.2% | 15.0154 | 15.2824 | +1.8% |
| text 1,024 | 58.5890 | 58.8032 | +0.4% | 61.6424 | 63.9044 | +3.7% |

The candidate adds no new language behavior. Even so, O2 JIT array 4096 regresses 4.5%, text 1024 3.7%, and JSON 128 2.5% in these cohorts. AOT JSON improves 3.9%, while AOT text is essentially unchanged. These mixed results do not meet the mission's integration criterion. Small changes can include code-layout and host variation; the evidence does not establish a universal causal slowdown from the search algorithm. No samples were discarded and no favorable mode was selected as the overall verdict.

### Historical Go comparison

Go was not rerun. The archived Go 1.27.4 medians are **historical context**, not a new paired experiment or a speedup attribution: array 4096 **0.019970 ms**, sort 1024 **0.043715 ms**, dictionary 1024 **0.079770 ms**, text 1024 **0.088965 ms**, JSON 128 **0.114565 ms**. The large remaining gap is real motivation for attribution, not proof that Go and VOID perform identical ownership work. VOID's independently owned strings, ordered/metered JSON DOM, duplicate-key rules and checked managed operations differ from Go's selected representations. Original Go samples, including its array/sort outliers, remain intact in the G31 archive.

### Startup, memory and code size

Four cold observations per variant/mode: current/candidate AOT launch **5.185/4.983 ms**; O2 JIT **3.792/3.777 s**; normal JIT **3.100/3.138 s**; complete native build **3.664/3.763 s**. These include the full imported test graph. They do not demonstrate a compiler/startup improvement. Process CPU tracks these wall times closely. JIT peak RSS is approximately 124–125 MiB and native-build peak RSS approximately 119–120 MiB. The roughly 13 MiB AOT RSS observation has a Python fork/launcher floor and is not an exact executable-residency measurement.

The linked suite grows **5,011,352 → 5,011,448 bytes**; compiler grows by 128 bytes. A pure emitted object is byte-identical (5,464 bytes, SHA `f38f306cc21fbead0fb04a731fe5dabfe922a650c6b763c25632bc32c011d114`). No source compiler, ABI, cache policy or GC settings changed in the candidate.

## 3. Shared mechanisms versus algorithms

Exact current source ownership:

| Area | Source and verified behavior | Attribution / boundary |
| --- | --- | --- |
| Registered roots | `src/runtime/coordination.cpp:199–231`; `buffer.cpp:229–246` | Frame/slot search and exact offset matching remain. Checked tokens revalidate Runtime/thread, frame identity, descriptor and current object. The accepted token path already avoids repeating full discovery when reused; no universal per-frame cache exists. |
| Scalar lists/arrays | `list.cpp:32–64`; `array.cpp:16–31,137–169`; LLVM `backend.cpp:1090–1165` | Every checked scalar operation may cross the ABI, acquire a lock, locate a root and validate catalog/header/buffer geometry. Stack `BufferLayout` construction is not itself a heap allocation. Scalar writes already skip reference-cursor preparation; growth is geometric. |
| Dictionaries | `dictionary.cpp:13–50,76–90` | Hashing is implemented. Native entry/key metadata, geometry and key admissibility are repeatedly checked. `validate_dictionary_key_schema` scans catalog/schema policy; catalog/header reuse already exists. Skipping that scan requires proof about later admissions and forbidden identities. |
| Stable sort | `stdlib/collections.void:34,53–105` | Bottom-up stable merge sort over snapshots, not a quadratic sorting algorithm. Checked reads/writes and comparator calls dominate the number of operations. Preserve callback effects and snapshot semantics. |
| Text/encoding | `stdlib/text.void:151–225,328–495`; `stdlib/encoding/{hex,base64}.void` | KMP search, geometric byte construction and checked scalar accesses; independently owned outputs. No evidence justifies replacing KMP with another search algorithm. |
| JSON | `stdlib/json.void:499–602,699–770,912,1139` | Ordered DOM construction, explicit decoded-key duplicate checks, metering, validation/clone and serialization. The duplicate scan can be quadratic for wide objects, but the other linear text algorithms must not be mislabeled quadratic. Fresh clone/conversion work has failure-order and budget semantics. |
| Strings | `src/runtime/abi.cpp:209–241`; `coordination.cpp:244–260`; `array.cpp` string conversion | Immutable concatenation produces independent strings and copies content. Repeated growing concatenation can copy a cumulative quadratic number of bytes. Fused join/encoding is intentionally not automatically enabled by this audit. |

Repeated validation is not automatically redundant: root tokens must reject expired or reused Frames; native borrows and published results have different authority/lifetime boundaries; layout/header checks protect type and extent; pre-poll admission and post-poll publication checks cannot be merged without a state-transition proof. No check was removed during measurement.

### Counts from successful O2 observation invocations

These are representative first timed counts after warmup in the same observation graph; globals and prebuilt fixtures deliberately remain rooted. Counts are not production time percentages. GC epochs can differ by one with the allocation budget's starting position; all raw samples are retained.

| Workload | Native allocation requests | Managed constructions | Collections | Exact root-offset comparisons |
| --- | ---: | ---: | ---: | ---: |
| Array 4096 | 81 | 12 | 0 | 73,780 |
| Sort 1024 | 278 | 42 | 0 | 2,425,497 |
| Dictionary 1024 | 1,161 | 19 | 0 | 27,693 |
| Text 1024 | 74,072 | 5,233 | 20 | 1,776,775 |
| JSON 128, parse and stringify | 65,690 | 2,495 | 10 | 4,082,612 |
| Parse-only prepared 128-field text | 39,673 | 915 | 4 | 3,472,608 |
| Stringify-only prepared DOM | 24,226 | 1,580 | 6 | 610,010 |
| Indexed access to prepared array | 0 | 0 | 0 | 204,844 |
| 512 growing string concatenations | 8,300 | 1,024 | 4 | 123,961 |

The prepared-array traversal still costs **1.736–1.802 ms** in the two observation-off blocks with no new allocation and no GC. The prepared dictionary's 1,024 lookups cost **8.139–8.464 ms**; insertion-only costs **6.441–6.598 ms** in a different invocation. This is strong evidence for checked-operation overhead independent of growth/collection, but not a disjoint split of the original dictionary workload.

Text component medians in two observation-off blocks: split **21.735–22.657 ms**, join **15.045–15.871 ms**, replace **10.769–11.132 ms**, repeat **4.004–4.225 ms**, hex **8.644–8.966 ms**, Base64 **6.211–6.254 ms**. A 4,096-byte `to_utf8` is **0.024–0.027 ms**, and `from_utf8` **0.034 ms**. An early-match KMP probe is **0.058–0.066 ms**, not a worst-case search measurement. These independently prepared invocations must not be added to reconstruct the original text run.

Parse-only is **33.664–33.868 ms**, prepared-DOM stringify **14.969–15.534 ms**. These local boundaries show that both sides matter; they do not isolate duplicate checking from all parser work. The observation-off host includes the allocation wrapper and a larger rooted fixture graph; its timings are not substituted for the primary CLI medians.

## 4. Tested root-search correction: STOP

Only a disposable copy of `src/runtime/coordination.cpp` changed:

```cpp
const auto* end = descriptor.offsets + descriptor.count;
const auto* found = std::find(descriptor.offsets, end, offset);
if (found != end)
    return {f, i, static_cast<std::size_t>(found - descriptor.offsets)};
```

This replaces the existing inner `for` loop only. It preserves unsorted and duplicate offsets, first matching ordinal, frame/slot traversal order, containment checks and borrowed valid descriptor-array requirements. No alias shortcut, root cache, allocation, new safepoint or ABI change was introduced. The local libstdc++ implementation unrolls ordinary random-access search; this is a native helper experiment, not a change to VOID's SIMD policy. [GNU implementation reference](https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/latest-doxygen/a00431_source.html).

Machine code confirms `Runtime::locate_root` grows **419 → 540 bytes**. The maintained root fixture shows why counts alone were insufficient: packed 128-word last lookup **83.0 → 66.6 ns**, but packed 16,384-word random lookup **2,531 → 3,611 ns**; 1,024 separate-slot last lookup **1,376 → 1,480 ns**. First-hit and reused-token paths are nearly unchanged. Different hit distributions and changed code layout matter. No blanket search improvement is established.

Focused CTests **5/5 Release and 5/5 fatal ASan/UBSan with leak detection** passed: `runtime_coordination`, `gc_buffers`, `gc_schemas`, `runtime_failures`, `runtime_invariants`. These cover root/frame expiry and reuse, descriptor mutations, unsorted/duplicate offsets, ownership, collection/rendezvous and failure paths. The maintained lookup fixture ran in both orders for both variants. Native Windows Debug/Release was not run. Even without that missing qualification, the Linux performance evidence rejects integration. No broad test campaign or further search variations are warranted.

## 5. Managed allocation and escape-analysis evidence

The current allocation path is `backend.cpp:1218–1257` → `vd_gc_construct[_typed]` / `vd_gc_allocate` → `Runtime::construct_into` (`coordination.cpp:274–303`) → `Heap::allocate_validated` (`heap.cpp:19–29`). It validates roots, alias safety, layout and references, checks the allocation budget, cooperates with collection, acquires the Runtime mutex, allocates/zeros storage, registers the object and publishes to the root only after fallible preparation succeeds.

There is **no VOID managed per-thread allocation cache, page/size-class pool or bump allocator** in these paths. TLS Runtime/thread pointers are execution context, not an allocation cache. Native `operator new` may benefit from the host allocator's own reuse; one `new` must not be described as one OS call. Existing zero-layout/metadata caches and geometric buffers are real and remain in place.

### Executable allocation-elimination probes

Four 1,000-iteration source fixtures ran normally and with O2, three measured invocations each after warmup. Their separately linked AOT applications also returned the expected result. The private backend observer only prints post-O2 IR; for all four fixtures its emitted AOT object is **byte-identical to the production compiler's emitted object**.

| Probe | Expected result | Managed constructions, normal and O2 | Representative O2 native allocations / collections | Emitted evidence |
| --- | ---: | ---: | --- | --- |
| Local managed `box`, read its field | 46,995 | 1,000 | 2,024 / 4 | `vd_gc_construct_typed` remains in loop, JIT and AOT |
| Local managed `box`, never read | 1,000 | 1,000 | 2,016 / 4 | Construction remains even when unused |
| Explicit `value struct box` | 46,995 | 0 | 0 / 0 | Entry computation promoted to scalar SSA; no managed construction |
| Fresh one-element growable list each iteration | 46,995 | 2,000 | 10,064 / 8 | Checked list creation/read calls remain; header and backing allocations counted |

The frontend's `ValidatedAnalysis::lower` (`compiler.cpp:7523`) retains/checks the graph; no general escape-analysis transformation was found. The LLVM backend runs the standard O2 pipeline (`backend.cpp:86–107`), with SLP disabled and accepted AOT target analysis. LLVM SROA/mem2reg optimize eligible stack homes and scalar values; they do not understand VOID's opaque checked GC allocator and publication protocol automatically. [LLVM pass reference](https://llvm.org/docs/Passes.html#sroa-scalar-replacement-of-aggregates), [LLVM GC integration requirements](https://llvm.org/docs/GarbageCollection.html).

**Important existing exception:** `frontend/compiler.cpp:4555–4583,5687–5740` and `src/ir/local_cleanup.inc` already recognize restricted synchronous cleanup patterns and eliminate generated capture/handler/cell machinery using rooted local cleanup. That is VOID-specific allocation avoidance for a proven pattern, not a general nonescaping-managed-object pass. The managed resource/guard objects can still exist. Declaring all allocation elimination absent would be incorrect.

These counts establish an opportunity, not a promised speedup or the number of provably nonescaping G31 objects. Returned split fields, DOM nodes, backing arrays, callbacks and live resources have different escape/identity requirements. Do not assume all 5,233 text or 2,495 JSON allocations can move to the stack. Any future proof must preserve constructor effects, alias identity, readonly, publication/failure ordering, precise references, deinit/native ownership, async captures, reload/code leases and real 1 MiB stacks.

### Allocation cost probe

A native fixture uses the current Runtime, one registered root, an 8-byte scalar object and the normal 256-allocation collection budget. Five samples per process, balanced observation-off and sampled runs, 20,000 complete constructions per sample. Native threads are initialized before the uncontended-lock control. Current uninstrumented construction is **193.3 ns/object median** including triggered GC; the ordinary 8-byte `new`/zero/delete control is **9.1 ns/op**. These are different lifetimes and scopes, not subtractable stages.

Standalone current root-word admission is **32.5 ns**, frame entry/leave **15.9 ns**, already-fast G31 poll **5.1 ns**, uncontended mutex control **15.9 ns**. These tiny single-root native probes exclude the full typed ABI, large catalogs and source error handling. They do not predict source-language construction latency or multi-thread contention.

One-in-64 observation inside the actual construction path measures raw interval medians: root lookup **30.4 ns**, layout/reference-map validation **30.6 ns**, each mutex acquisition **31.5 ns**, descriptor validation **28.2 ns**, each alias check **28.7 ns**, poll **31.2 ns**, underlying native allocation **50.2 ns**, zeroing **26.9 ns**, object-map registration **69.2 ns**. An empty sampled span costs **26.7 ns**; values near that floor are unresolved, not free or independently removable costs. Do not subtract medians or add these values as an exact production breakdown. Instrumentation-off copied Runtime construction is **210.5 ns**, sampled construction **217.5 ns**: code layout and measurement already perturb the 193.3 ns control.

This measures registration/native allocation and exposes the precision limit for small validation/lock costs. It does not demonstrate allocator contention, nor prove that a new TLAB would dominate. Go's allocator uses per-P caches and size-class spans, a mature reference with substantially different GC/ownership machinery; it is not a drop-in change. [Go allocator source](https://go.dev/src/runtime/malloc.go).

## 6. Release specialization, inlining and SIMD

Generated AOT IR contains concrete `collections.sorted<i32>` and `numeric_less<i32>` functions: specialization already works. The O2 compare-loop probe resolves a constant function argument to a direct `numeric_less` call; a generic callable switch is not inevitably paid. The small callee nevertheless retains managed frame/poll/worker-checkpoint calls in the coordinated graph. A plain explicit-value entry was inlined into AOT `main` and promoted to SSA. The standard optimizer is present and effective where operations permit it.

In the observation-off O2 host, 65,536 generic numeric callback comparisons take **8.780–9.035 ms**, versus **3.594–3.789 ms** for inline comparisons, with zero allocations/collections in both. This measures the callback boundary **plus** its coordination/effect/checkpoint machinery, not bare CPU call/return latency. It cannot be extrapolated directly to a sort with a different comparator count. Sorting's checked element operations remain significant regardless.

[The accepted AOT-only SIMD qualification](AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md) remains authoritative. `finish` receives the existing generic deployment TargetMachine for AOT and no analysis machine for JIT; SLP stays false. Accepted guarded integer-loop vectorization remains implemented. Checked element calls, poll/checkpoint side effects and alias/error ordering still constrain other loops. G31 made polling cheaper without removing its call-side effects or providing a new legality proof. No JIT target-analysis candidate, fast-math, unchecked array path, CPU-feature policy or cache-identity change was tested here. [LLVM vectorizer reference](https://llvm.org/docs/Vectorizers.html).

## 7. FILE200 attribution extension

The G31 suite's durable I/O case is a prepared-write control, **not full logger FILE200**. This extension therefore reuses the original `tests/log/benchmark.void` command 11 and the original host's byte/write/flush/heap/handle oracles, with production allocation budget 256. Command 18 is its separately executed prepared-record direct-write control. Source setup, 200 synchronous acknowledgements, 200 actual transfers, 15,000 exact bytes, close and one mandatory real `fsync` remain inside the original execution timer; compilation, fixture cleanup and oracle reading remain outside. Linux XFS results are local evidence, not a portable durability-latency promise. No buffering, write elision or fsync substitution occurred.

### Current timing and observer cost

The fresh accepted-G31-I Runtime gives full FILE200 O2 JIT **12.097 ms wall / 6.804 ms process CPU median** across two primary blocks; original normal JIT **12.995 / 6.998 ms** in one control block. Prepared-record direct writes are **6.783 / 1.367 ms** in O2. These are current observations, not a matched causal comparison with the old 17.915 ms result. FILE200 AOT attribution was not rerun in this extension; primary G32 independent AOT qualification is the separate G31 suite.

The observer-linked FILE200 with counters off is **13.596 ms**; counts-only **13.613 ms**; Runtime phase clocks **15.383 ms**. Adding explicit test-only LLVM hooks at `_prepare` and `logger::method::_deliver` yields **14.579 ms** with observation off and **15.643 ms** with nested clocks. Code/allocator-wrapper layout and host/fsync variability matter. Thus the following exclusive table describes one actual **instrumented** execution; it is not a claimed breakdown of the 12.097 ms production median.

Both full and prepared workloads retain all original successful-output checks. Function-boundary hooks cover the two existing synchronous functions only, including their error returns, and use the same nested timer implementation. Their 200/200 call counts were checked explicitly. They are measurement-only symbols, not a proposed Runtime API.

### Exact counts, including acquisition and close

| Observation | Complete FILE200 | Prepared direct-write control |
| --- | ---: | ---: |
| Managed construction attempts, successful | 2,899 | 425 |
| Native C++ allocation requests | 12,827 | 2,024 |
| Cumulative requested native bytes | 806,589 | 224,132 |
| Managed collections | 11 | 1 |
| Collector frame-root words inspected | 5,969 | 337 |
| Exact root-offset comparisons | 214,249 | 4,348 |
| Checked scalar-array element reads counted | 0 | 0 |
| Native write events / bytes | 200 / 15,000 | 200 / 15,000 |
| Final real `fsync` | 1 | 1 |

Complete FILE200 construction identities: **1,853 strings**, **204 buffers**, **210 scalar list headers**, **203 logger guards**, **203 I/O leases**, **202 receipts**, plus acquisition/task/error/catalog/setup objects. The ordinary successful path still creates four I/O error objects in setup/native results; this does not mean four failed writes. All final bytes and return acknowledgements passed.

There are **207 string joins**, **201 UTF-8 encodes**, **200 checked byte snapshots**, **2,693 `construct_into` calls**, **31,756 Runtime polls**, and **13,686 root timing-scope calls**. Root-scope counts include nested `root_word`/`locate_root` calls and are not 13,686 unique registrations. Compiler frame publication and other uninstrumented checks also exist; no claim is made that this counts every root operation. The 5,969 collector-root-word count is a different metric.

Only **nine generic native reply constructions**, not 200, remain. The accepted compact write-success path and dense snapshot are present. Likewise, only a small number of acquisition cleanup cells/captures remain, rather than resurrected per-record generated cleanup nodes. This confirms earlier repairs instead of proposing them again. The logger still creates private guards/receipts/leases through `stdlib/log.void:133,185,426,607` and `stdlib/io.void:379,577`; specialized cleanup elimination does not make those resource-authority objects disappear.

### One disjoint, exclusive instrumented execution

| Category | Exclusive ms | Interpretation |
| --- | ---: | --- |
| Record preparation source remainder | 2.325249 | `_prepare`, excluding all timed descendants |
| String joining | 0.132269 | Shared Runtime join, excluding timed managed/root/GC children |
| UTF-8 encode | 0.475369 | Shared encoder, excluding timed children |
| Managed construction | 1.567811 | Root/layout/publication path, excluding other timed child scopes |
| Buffer allocation | 0.039880 | Private buffer allocation path |
| Heap collection | 0.620867 | Heap tracing/reclamation; Runtime rendezvous/root gathering can remain in parent scopes |
| Root authority lookup | 0.921210 | Nested root scopes counted exclusively |
| Runtime polling | 0.856866 | Highly sensitive to per-call timer overhead |
| Checked write snapshots | 0.194869 | Exactly 200 independent snapshots |
| Generic native replies | 0.079100 | Nine calls; compact write success is not charged as a generic reply |
| Delivery source remainder | 1.084486 | `_deliver`, excluding timed descendants |
| Native writes | 0.529247 | 200 real writes |
| Durable synchronization | 5.794959 | One real final fsync |
| Remaining acquisition/close/coordination and observer work | 1.050734 | Outside the named scopes |
| **Complete observed execution** | **15.672916** | Sum from this one sample, not independently selected medians |

Across six fully scoped samples, preparation inclusive median is **5.262 ms** and delivery inclusive median **2.810 ms**. These overlap the nested allocation/root/encode/write categories and **must not be added to them**. Final fsync median in those same samples is **5.674 ms**. Full raw observations retain fsync variability and both observation-off controls. Poll-clock exclusive time is roughly 0.887 ms across ~32,000 calls; the native empty-span probe's clock floor explains why that cannot be advertised as removable poll work.

**What is demonstrated:** substantial short-lived construction and frequent checked crossings remain; prepared writes have much less CPU work; final durability still consumes several milliseconds. Heap/handle state returns to the original post-collection values, so the counted temporary population is not evidence of a leak. Short-lived does not mean proven nonescaping: receipts cross helper calls, guards/leases control file ownership and cleanup, and records remain rooted through native snapshots.

**What is not demonstrated:** that a particular root/poll is redundant, that removing a managed allocation preserves native release/fault ordering, that a different allocator recovers the whole overhead, or that source-scope instrumentation estimates uninstrumented costs exactly. Do not promote 2,899 counted allocations into a speedup promise. The next allocation question is an exact use/lifetime proof for a specific private type, with original error and resource behavior retained—not a generic escape-analysis project. No FILE200 optimization is recommended by this extension.

## VOID 2.0 Language Bible — Unfinished Performance Opportunities

The [Language Bible](../LANGUAGE_BIBLE.md) contains design direction and historical status notes. Its §54 stages are architectural groupings, not authorization for a new optimization system. Current source and later acceptance records take precedence. “Not demonstrated” means no implementation/proof established in this audit; it is not an assertion about every LLVM transformation or every program.

| Original Bible section / optimization | Current status and production evidence | Actual observations; LLVM contribution | Risk and smallest safe next experiment | Recommendation |
| --- | --- | --- | --- | --- |
| §12, §16, §36: nonescaping managed allocation avoidance | **Not demonstrated for general managed objects. Partially implemented allocation avoidance** through restricted local-cleanup lowering (`compiler.cpp:4555–4583`) and explicit inline values. | Local and unused managed probes still allocate 1,000 objects normally/O2/AOT; explicit value probe allocates zero. Standard LLVM SSA promotion explains the latter, not managed escape analysis. Thousands of text/JSON constructions provide a candidate population, not a proven removable set. | High: identity, references, observable constructor/failure effects, async/native retention, 1 MiB stack and reload. Start with an allocation-site/escape census of one short-lived internal type and a hand-derived proof; no pass implementation yet. | **Investigate narrowly; defer a general pass.** |
| §37, §37.2: per-thread caches, reusable pages/size classes | **Deferred / absent in the current managed allocator.** `heap.cpp:19–29` uses native allocation, zeroing and `objects_.emplace`; `coordination.cpp:274–303` coordinates/locks/publication. Existing native allocator reuse and metadata caches are not a VOID TLAB. | Full native construction 193.3 ns; immediate native new/zero/delete control 9.1 ns. Sampled map registration is measurable; most validation subintervals are close to clock resolution. There is no allocator-contention result. LLVM cannot replace opaque Runtime allocation with a local bump allocation. | High: ownership, allocation failure, object-map authority, collection handoff, fragmentation and teardown. Next evidence would need typed allocation-site/size/lifetime distributions and contention before choosing a pool; this audit does not establish a pool win. | **Defer allocator redesign.** |
| §37.1: precise roots and liveness | **Implemented correctness; partially optimized.** Packed homes (`backend.cpp:692–710,833ff`), frame identities and validated tokens (`coordination.cpp`, `buffer.cpp`), and prior metadata reuse are accepted. Many homes share function lifetime; no general last-use root shortening was found. | Millions of exact offset comparisons remain; prepared traversal allocates/collects nothing yet costs ~1.8 ms. LLVM cannot erase addresses intentionally published to the Runtime. Function-duration roots can retain old values; this audit does not quantify the benefit of earlier nulling. | High for lifetime changes. Reuse the preserved frame/root fixture and count root positions and slot lifetime before changing lookup/liveness. The tested `std::find` correction failed whole-workload qualification. | **Leave production unchanged; investigate exact boundary costs.** |
| §37 performance priority: cheap safepoints | **Implemented G31 fast poll**, preserving managed-state validation and locked request recheck/parking. Loop and worker checkpoints remain. | Standalone current no-request poll ~5.1 ns. FILE200 still crosses it tens of thousands of times; timing every poll largely measures observer overhead. Ordinary LLVM cannot discard an externally visible synchronization call. | High for elimination/coalescing: bounded rendezvous latency, cancellation and failure order. Any future experiment needs explicit safepoint-equivalence proof and existing race controls, not deletion based on call counts. | **Leave unchanged.** No evidence for another G31-style obvious duplicate. |
| §31, §40.2, §54 stage 9: specialization and release inlining | **Implemented concrete specialization; partial inlining.** `GENERICS.md`, frontend specialization, backend O2 pipeline and captured IR. Live-code boundaries remain intentional. | Concrete `sorted<i32>`/`numeric_less<i32>` exist; constant callback is devirtualized to a direct call; scalar value body is inlined into AOT main. Coordinated numeric helper still has frame/poll/checkpoint calls. Callback-vs-inline loop difference is measured but is not bare dispatch time. | Medium/high: effects, cancellation, reload and code size. Inspect a small pure helper's entire call/coordination envelope before a bounded proof-preserving experiment. Do not invent an overload or specialization framework. | **Investigate the envelope only; retain existing LLVM optimization.** |
| §54 stage 9: SIMD | **Partially implemented, accepted AOT-only.** Current `finish` passes the generic AOT TargetMachine; JIT analysis remains unchanged; SLP false. | Earlier accepted IR/instruction evidence proves eligible integer-loop vectorization. G32 collection loops still contain checked calls and effect barriers. Faster poll does not remove them. LLVM already supplies legal loop vectorization. | High for checked-loop transformations or FP changes. Use the existing SIMD fixtures later with identical checked/error semantics and generic target policy. No new SIMD experiment in G32. | **Leave accepted policy unchanged; defer other opportunities to that track.** |
| §37.3, §56: GC throughput/pause tuning | **Precise coordinated mark/sweep implemented; tuning partial.** `coordination.cpp:143–197`, `heap.cpp` collector and default allocation-budget mechanism. | Sort and array count probes collect zero times; text ~20, JSON ~10, FILE200 11. Aggregate observed GC work is measured, but production p95/p99 pauses, fragmentation and concurrent contention are not established here. LLVM does not supply the collector. | High for collector changes. Allocation reduction/cost attribution precedes changing collector generations or thresholds. A bounded pause trace with observation-off control is the smallest next GC-specific measurement if pause behavior changes an engineering decision. | **Defer generational/concurrent GC and threshold tuning.** |
| §32 arrays, §33 dictionaries, §8 strings: efficient containers/representation | **Implemented core strategies; partial constant-cost optimization.** Contiguous geometric arrays, dictionary hashing, KMP text search, dense snapshots and existing schema reuse remain present. | Array has 12 managed constructions, sort 42; text/JSON have much larger intermediate construction. Repeated immutable concatenation is a real cumulative-copy risk; KMP/merge sort are not incorrectly quadratic. LLVM does not fuse opaque checked calls automatically. | Medium/high for transactional checked-operation reuse; high for representation changes. Next bounded diagnostic should distinguish metadata validation/native scratch allocation from actual element work on one existing checked operation. Preserve catalog admission, root identity and error precedence. | **Investigate shared checked boundaries; do not replace algorithms.** |
| §37/§40 performance direction, later rejected alternatives | **Rejected:** prior per-frame root location cache and indexed iterator; JIT target-aware SIMD candidate; current G32 `std::find` candidate. | Prior cache's whole-workload regressions are recorded in `INTERNAL_PERFORMANCE_REPAIR_RESULT.md`; current candidate's negative rows are above. A proposal in the Bible does not override those results. | Reopening requires materially new evidence. Do not rebuild an earlier cache or broaden it to hide these measurements. | **Leave rejected.** |
| §56: comparative measurements and memory evidence | **Partially implemented**, expanded here with matched latest-baseline runs, cold/warm separation, native counts and evidence retention. | Historical Go comparisons retained; current full-workload results and counter controls available. Tiny native intervals remain at measurement floor; no universal speedup or production tail-latency claim. | Low for existing bounded probes; higher for instrumentation that changes optimization/layout. Preserve all unfavorable runs and separate attribution from baseline timing. | **Continue evidence-driven qualification, without new release gates.** |

## 8. Single engineering handoff and stop conditions

**HOLD implementation of the tested root-search change and of new escape-analysis, allocator or collector systems.** There is no demonstrated small correction from this cohort that can be sent to Keyboard as a qualified performance repair.

The next justified action, if Boss wants engineering to continue, is one bounded shared-boundary attribution assignment using the retained fixtures: attribute the current checked scalar read and dictionary lookup's metadata/native-scratch work, with immutable-catalog identity and exact root authority held constant. Compare an existing-token native path against the source ABI path; keep per-call error/alias/version checks intact. Extend the same construction-site accounting to text/JSON and the counted logger guards/leases rather than starting four independent redesigns. A result must identify demonstrably duplicated work before any patch is approved. It must not assume that an expensive validation can simply be removed or that any metadata admission is permanently immutable.

For a later justified correction, reuse this exact baseline and complete workload cohort; require normal/O2 JIT and independent AOT output equivalence, existing allocation-failure/root-expiry/alias/version tests, fatal sanitizer and collection-race controls, and matched whole-workload measurements. Preserve degraded and observation-off results. Stop if semantics/failure order require a new Runtime contract, if representative regressions persist, or if measured savings are indistinguishable from variability. Native Windows 10 Debug/Release qualification is still required before any cross-platform Runtime integration recommendation. None was attempted here because the Linux candidate was rejected.

Already repaired behavior stays repaired: G31 poll fast return; compact synchronous I/O replies; dense byte snapshots; generated local cleanup; geometric collection growth; checked root tokens and metadata reuse. No async retention, LSP, licensing, packaging, HTTP, SIMD-policy or unrelated repair was reopened.

G29's Linux ABI floor, macOS native acceptance, VSIX/npm security review, public VOIDUP trust/distribution, supported-platform declarations and final Boss-controlled comprehensive CI remain unchanged. Performance comparisons neither create nor close release gates. FILE200's historical target is not promoted to a mandatory release blocker by this report.

## 9. Qualification, limitations and evidence

- Primary workload oracle checks: **1,008/1,008 passed**, all 14 cases in normal/O2 JIT and linked AOT, matching original G31 source and oracles.
- Root candidate: **5/5 Release and 5/5 fatal ASan/UBSan CTests passed**; maintained native root fixture completed in both orders. Performance rejected; Windows unqualified.
- Escape probes: four sources, normal/O2 execution, production-identical emitted AOT objects and four independently linked AOT application executions passed. No source-language allocation elimination was implemented.
- Attribution probes: source/component checksums, managed counts, bounded allocator samples and original logger byte/write/flush/heap/handle oracles passed as recorded. These are research probes, not new regression test infrastructure or a full cross-platform acceptance claim.
- No ptrace, hosted CI, production changes, staging, commits, branches or pushes.

Authoring/instrumentation failures are retained: an initially generated component fixture shadowed globals and used unescaped interpolation braces; the first escape-host link accidentally duplicated an allocation object/ordered a backend object incorrectly; logger instrumentation initially omitted `const` in a source marker and initially matched preparation but not the delivery method's debug display name. These were probe construction failures, not product-crash diagnoses. Original outputs/failed fixture copies are preserved. No abnormal VOID execution occurred in the final measured cohorts, and no historical incident was erased or called repaired by a successful retry.

The archive contains exact final fixtures and selected original G31 inputs/oracles, source overlays and diffs, replay scripts, source/binary identities, raw favorable/unfavorable observations, scope/counter controls, optimized IR and machine-code receipts, focused qualification results and failed probe evidence. Routine build trees, binaries, link caches and SDK copies are excluded. The external archive index records verification; task-owned outputs alone are removed after hash/CRC/extraction checks.

Archive: `/home/shmellyorc/Documents/Void.Language-archives/g32-shared-runtime-performance-attribution-2026-11-13.zip`.

- SHA-256: `6dfab2928d05a58e5023390e1b3358938cdfde782c21cd0693dd70dc612a70e3`.
- **548 selected files**, 28,830,293 uncompressed bytes; **2,112,851-byte ZIP**, DEFLATE level 9.
- CRC test, actual extraction SHA checks, source rehash and original production-source identity checks all passed. Restoration instructions are `REPLAY.md`; exact source/member hashes are `_ARCHIVE_MANIFEST.json`.
- Task-owned Linux root `/home/shmellyorc/.local/state/void-g32r-tzrlj5hw` removed after verification: 18,815 regular files, 1,319,026,462 logical bytes, 1,378,553,856 allocated bytes inventoried before archive creation. The external archive index and `g32-cleanup-verification-2026-11-13.json` retain the receipt.
- Linux free bytes: task start **362,141,851,648**; immediately before deletion **360,629,129,216**; after deletion **362,017,566,720**. Observed filesystem-wide gain **1,388,437,504 bytes** (1.293 GiB); concurrent desktop activity can affect that delta. Active builds/toolchains and historical archives were untouched.
- Windows was not accessed; no Windows scratch, disk-space observation or native qualification is claimed.
- HEAD remains `0407f83f9d6dd660b1503df1e68cdcee9b34b77b`. The sole repository change is this untracked/uncommitted report. Tracked and staged diffs are empty. External archive/index/cleanup receipt updates are research evidence only.

**Answer to the mission:** the strongest shared opportunity after G31 is reducing repeated checked-operation and temporary-construction overhead with proof of authority and lifetime preservation. General managed allocation elimination is unfinished, but its payoff on these workloads has not been proved. The smallest tested correction did not qualify. **No production optimization is recommended from this investigation; retain the evidence and pursue only the identified narrow attribution if authorized.**
