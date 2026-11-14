# G33-X — performance foundations: experimental engineering handoff

**Baseline:** `0407f83f9d6dd660b1503df1e68cdcee9b34b77b`; LLVM/Clang/LLD **23.1.4**. **Status: experimental campaign complete; LIMITED GO for A; no production integration authorization.**

A–E are implemented in isolated source overlays. Production remains unchanged. The two strongest targeted results are real scalar managed-allocation elimination (A) and empty-frame omission with a smaller common poll path (D2). Their single combined experiment passes focused qualification and has measured benefits and retained regressions. B/C/E have useful mechanism evidence but are not default-integration recommendations. This is not a claim that VOID broadly matches Go or that a replacement collector is needed.

The durable [implementation ledger](G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md) retains every Bible feature, negative result and next action. Exact patches, source identities, executable identities, inputs, oracles, raw observations and replay scripts are retained in the evidence archive described at closeout.

## Evidence and boundaries

- Fresh baseline Runtime SHA-256: `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`, matching accepted G31-I/G32. All compilers use matching Runtime/header builds. G32's rejected root-search candidate is not reused.
- Four balanced alternating blocks per independent comparison, three warm samples per case, normal/O2 JIT and independently linked AOT. Compilation/linking are outside source execution timers. No samples are discarded. Cold launch and compile/link measurements are separate.
- Linux execution uses CPU 2 and a real 1 MiB native stack; worker/race fixtures use CPUs 2–5. Build groups were stopped or completed before primary timings. Native Windows qualification runs outside primary Linux timing. Machine-local results are not platform-wide speedup promises.
- Managed/allocator observations run separately with observer-off controls. Counts and GC interval samples do not establish exclusive cost of unrelated operations. Requested-memory peaks cover an observed thread/epoch; cross-thread frees and cached storage allocated before observation prevent equating them with global heap peaks/RSS. Process RSS includes compiler/JIT/startup state.
- Every abnormal test/compiler receipt is retained. Setup failures are distinguished from compiler failures; retries never erase originals. ASan/UBSan are fatal with leak detection; B/E also have fatal TSan controls. No hosted CI, production changes or Git mutations.

## Implementations and verified mechanisms

| Experiment | Exact source owner | Implemented behavior | Proven limit / next engineering work |
| --- | --- | --- | --- |
| A | `src/backend/llvm/backend.cpp`, construction/root-home lowering | Bounded block-local escape proof for scalar managed objects; reuse existing construction staging; omit eligible heap publication/root homes | No reference graphs, interprocedural escape, async/native/callable identity, cross-edge aliases or cleanup-bearing functions. Bound 256 bytes/object, 16 KiB/function. Review proof as a named helper and add maintained negative tests before integration. |
| B | `include/void/{heap,runtime}.hpp`, `src/runtime/heap.cpp` | Runtime-owned 16-byte size classes through 256 bytes; at most 64 cached blocks/class, 139,264 payload bytes; current registry and lock remain | No TLS allocator/page engine. Payload reuse leaves registry allocation and authority validation. Preserve sanitizer poisoning diagnostics and resolve broad layout-sensitive regression before integration. |
| C | `src/backend/llvm/backend.cpp`, outgoing synchronous CFG edges | Null dead block-local instruction homes; preserve locals, parameters, descriptors and async homes | Less nonzero retention/marking, same descriptor scan count. Next: a specific proven local last-use case, not a root cache or collector rewrite. |
| D1 | `src/backend/llvm/backend.cpp`, frame enter/exit | Omit only empty root frames outside special lifecycle scopes | Every poll/checkpoint remains. Dynamic 1,001→1 frame enters/leaves in the 1,000-call fixture; 5,003 polls unchanged. |
| D2 | D1 plus `src/runtime/coordination.cpp:136` | Move locked poll wait into a noinline cold helper, retaining validation, acquire load, lock, recheck and park | No new Runtime/header/authority contract. Common machine path saves one register instead of four plus stack space. Small broad shifts remain; targeted call benefit is established. |
| E | `runtime.hpp`, `coordination.cpp`, `buffer.cpp`, backend/JIT and ABI application constructors | Successful collection sets source pacing to max(base, min(16×base, 2×live objects)); explicit host budgets stay fixed | Object count is not a byte bound. Higher retention and pause tails are demonstrated. Needs bounded byte/live-memory policy evidence before a default change; no replacement collector. |

A really removes managed allocation, not a value-struct substitution or merely LLVM scalar promotion. For 20,000 local constructions, observed managed constructions/native requests/collections change **20,000/40,468/78 → 0/0/0**. Aliases, initializer side effects and mixed numeric edge cases retain outputs. Escaping, reference-containing and cross-edge objects remain allocated. The owner explicitly approved disappearance of OOM caused solely by an eliminated allocation. Remaining allocation failures and constructor effects are preserved and fault-tested.

A's normal/O2/AOT IR removes the eligible allocation call; escaping IR is byte-identical. The complete G31 AOT executable is byte-identical between A and baseline: that corpus contains no eligible A sites. Small A changes there are controls/noise, not allocation-elimination gains. Static frame call sites and actual dynamic frame counts are separate receipts. **The ordinary LLVM O2 pipeline already inlines the tiny `plus` helper in the baseline probe.** Its empty-frame enter/leave effects remain inside the optimized loop. D removes those compiler-generated effects; this is not evidence that ordinary inlining was absent or that a new inliner is needed.

## Managed-source execution

Each O2 row below uses 20,000 loop iterations except 1,000 sequential async tasks; 12 uninstrumented samples/variant. Baseline and candidates use the same source/oracle. Lower is faster.

| O2 source probe (ms) | Baseline | A | B | C | D1 | D2 | E |
| --- | --- | --- | --- | --- | --- | --- | --- |
| managed-local | 6.254 | 0.767 | 6.143 | 6.287 | 6.198 | 6.160 | 6.478 |
| managed-escape | 7.284 | 7.320 | 7.224 | 7.117 | 7.249 | 7.199 | 7.522 |
| managed-reference | 8.790 | 8.273 | 8.326 | 8.602 | 8.762 | 8.695 | 8.622 |
| managed-across-edge | 6.674 | 6.912 | 6.388 | 6.639 | 6.441 | 6.824 | 6.671 |
| managed-survives-allocation | 49.064 | 41.479 | 45.972 | 47.259 | 47.174 | 47.777 | 48.397 |
| closures | 1.662 | 1.603 | 1.594 | 1.624 | 1.651 | 1.574 | 1.603 |
| calls | 1.383 | 1.363 | 1.390 | 1.342 | 0.941 | 0.924 | 1.358 |
| workers | 0.864 | 0.862 | 0.822 | 0.863 | 0.810 | 0.843 | 0.836 |
| async-tasks | 1.696 | 1.731 | 1.729 | 1.667 | 1.698 | 1.721 | 1.706 |

A's mixed temporary-array probe removes only the eligible object allocations: managed constructions fall 60,000→40,000, not zero. D2's call-heavy reduction is associated with removing unnecessary empty frames; the poll cold split corrects D1's larger unrelated arithmetic regression without proving which microarchitectural placement effect caused it. Closures/tasks/worker results remain controls; none imply removing cancellation or lifetime checks.

## Whole G31 corpus

Percent change is against each candidate's own matched baseline (negative is faster). These are local medians, not statistical confidence intervals. Every exact sample/range is in `warm-summary.json` and raw JSON. A is omitted here because no eligible site exists in its byte-identical AOT corpus.

| Mode | Case | B | C | D1 | D2 | E |
| --- | --- | --- | --- | --- | --- | --- |
| aot | integer/4000000 | +7.7% | +6.1% | +6.3% | -1.3% | +1.0% |
| aot | dependent/1000000 | +7.3% | +5.8% | +6.8% | -0.6% | +3.2% |
| aot | float/1000000 | +7.1% | +7.2% | +6.4% | +1.1% | +2.4% |
| aot | array/4096 | +0.2% | +0.9% | +0.2% | +2.9% | -4.9% |
| aot | sort/1024 | -1.2% | +0.0% | -1.1% | -0.4% | -1.7% |
| aot | dict/1024 | -0.6% | +1.7% | -2.4% | +3.0% | +0.2% |
| aot | text/1024 | -5.5% | -0.4% | -0.6% | -1.3% | -5.9% |
| aot | json/128 | -0.5% | +0.1% | +3.1% | -1.7% | +2.3% |
| aot | io/200 | -0.5% | -0.5% | -2.8% | +0.7% | -0.0% |
| jit-O2 | integer/4000000 | +0.7% | -2.0% | -0.0% | +1.4% | +3.5% |
| jit-O2 | dependent/1000000 | +0.1% | -1.5% | +0.8% | +2.3% | +5.2% |
| jit-O2 | float/1000000 | -2.6% | -3.7% | -3.1% | +0.4% | +2.1% |
| jit-O2 | array/4096 | -0.0% | -2.6% | -2.6% | +0.4% | -0.5% |
| jit-O2 | sort/1024 | +1.6% | +0.3% | +1.1% | -1.5% | -0.6% |
| jit-O2 | dict/1024 | -1.0% | +0.1% | -0.4% | +1.5% | +0.1% |
| jit-O2 | text/1024 | -4.0% | -1.3% | +0.4% | -2.8% | -5.4% |
| jit-O2 | json/128 | +0.1% | -2.5% | -1.0% | -4.1% | -1.9% |
| jit-O2 | io/200 | -3.1% | -2.4% | +0.8% | -3.6% | +0.0% |

The I/O control retains 200 checked writes, 15,000 exact bytes, final real durable flush, close and output verification. It is the G31 prepared-byte I/O control, **not the complete historical logger FILE200 workload**. No logging threshold or contract was altered.

### Regression attribution, including unfavorable controls

B's AOT integer control repeated roughly 7–8.5% slower even though its emitted arithmetic object is identical and the timed loop allocates nothing. Runtime layout moves the request field from offset `0x3b9` to `0x481`, and linked hot-function placement changes. Exact entry-object manual relinking reproduced original executable hashes. Diagnostic function alignment flipped the relative ranking by worsening baseline (123.368→134.838 ms), not by demonstrating a reliable repair; B moved 133.810→131.263 ms. This supports code-placement sensitivity, not a confirmed instruction-cache/branch-predictor root cause. Alignment is not recommended.

D1/C also retain unfavorable AOT movements. D2's smaller poll prologue is verified in disassembly; the observed broad effect is mostly small and mixed. No benefit is assigned to register-save removal independently of layout. The report does not silently promote every microbenchmark win.

## Allocation, roots, memory and GC

The native allocator fixture executes 20,000 constructions (1,024 × 64 KiB for large objects), verifies payloads, ownership and final reclamation, and includes 512 retained roots and four-thread contention. Twenty uninstrumented samples/case/variant; CPU includes all process threads.

| Native case | Baseline wall/CPU ms | B wall/CPU ms | E wall/CPU ms |
| --- | --- | --- | --- |
| small | 3.880 / 3.821 | 4.018 / 3.951 | 4.139 / 4.069 |
| mixed | 4.967 / 4.876 | 3.782 / 3.717 | 5.131 / 5.040 |
| large | 11.189 / 10.890 | 10.942 / 10.678 | 11.067 / 10.822 |
| retained | 7.209 / 7.099 | 7.075 / 6.960 | 6.299 / 6.199 |
| threads | 18.567 / 37.719 | 19.396 / 45.581 | 18.707 / 42.735 |

B's mixed-size benefit is real in this fixture, but small allocations do not improve and threaded CPU rises. B avoids 4,992 of 40,468 native requests in the managed-local observer, not all allocation work. The 64-block/class cap versus a 256-allocation collection budget limits scalar-class reuse to roughly one quarter of each batch; the observed 4,992 hits demonstrate this. A capacity-versus-memory experiment is a concrete smaller next step than inventing TLS/page allocation, but no larger cache is approved or measured here. Its first prototype enabled caching for standalone Heap and failed two existing failure/retention tests; corrected Runtime-only opt-in passes. Cached blocks weaken ordinary native-free ASan poisoning unless explicit poisoning is added; current exercised sanitizer passes do not prove every stale-pointer diagnostic.

C changes text nonzero root visits 1,450→914 and JSON 2,331→971, while root-word visits remain **7,226 / 8,889** and collections remain **20 / 10**. Text mark visits change 44,566→44,030, JSON 21,386→20,026. This proves activation but not a consequential whole-workload improvement. Descriptor scanning and still-live locals remain costs.

E's retained-set native workload changes collections **78→20** and final live payload **8,704→20,992 bytes**. The independent observed retained-set GC distribution is:

| Variant | GC events | Median μs | p95 μs | p99 μs | Max μs |
| --- | --- | --- | --- | --- | --- |
| current | 390 | 26.450 | 30.160 | 42.849 | 66.759 |
| B | 390 | 26.895 | 31.090 | 44.560 | 56.470 |
| E | 100 | 64.245 | 96.879 | 100.359 | 102.029 |

These observed intervals include observer/native allocation instrumentation and are not added to raw throughput. E's text observation changes collections 20→1, requested-memory peak 234,199→874,641 bytes, and a 1,184.713 μs baseline maximum to one 2,086.889 μs event. JSON changes 10→1 and peak 177,454→388,421 bytes. **One event cannot estimate a percentile tail.** E remains a working pacing experiment with a demonstrated tradeoff, not a default-policy recommendation. Its first version changed explicit Runtime(1) behavior and failed a retained-cycle test; fixed explicit-host policy and both construction owners are now covered. Failed collection preserves policy/root state and retry succeeds.

## Fresh Go reference and remaining gap

Official Go 1.27.4 was downloaded into task storage with its release SHA verified; no system installation. Four fresh complete-corpus runs preserve the original oracles. Source arithmetic and checksums are matched; different GC, checked boundaries, owning text versus substring behavior, sorting implementations and ordered JSON DOM versus Go map/marshal semantics make several library comparisons informative rather than identical-contract performance claims.

| Case | VOID baseline AOT ms | D2 AOT ms | Go ms |
| --- | --- | --- | --- |
| integer/4000000 | 125.718 | 124.115 | 4.017 |
| array/4096 | 4.754 | 4.894 | 0.019 |
| sort/1024 | 30.442 | 30.311 | 0.043 |
| dict/1024 | 9.654 | 9.943 | 0.078 |
| text/1024 | 59.454 | 58.685 | 0.087 |
| json/128 | 42.568 | 41.834 | 0.118 |
| io/200 | 7.028 | 7.078 | 6.147 |

The dedicated Go pointer-box local/alias/identity examples allocate zero heap objects, including a noinline identity call. Escape diagnostics and machine code are retained. VOID A proves only local block containment and conservatively keeps the identity-call case allocated. This is concrete remaining interprocedural opportunity, not authorization to add an optimizer framework or pretend VOID has caught up. Go's tiny loop also lacks VOID's per-operation Runtime protocols; eliminating allocations alone cannot explain or remove the remaining gap.

## Startup, compilation and code

Four cold invocations/profile/variant and four complete application compile+link runs are separate from timed execution. Cold AOT includes process startup/argument initialization; cold JIT includes frontend/LLVM compilation with cache disabled. Values are wall medians in milliseconds, not isolated LLVM pass costs.

| Variant | Cold AOT | Cold normal JIT | Cold O2 JIT | Compile + link |
| --- | --- | --- | --- | --- |
| current | 4.74 | 3157.40 | 3840.88 | 3694.02 |
| A | 4.84 | 3149.73 | 3863.94 | 3755.99 |
| B | 5.03 | 3139.33 | 3863.10 | 3703.31 |
| C | 5.00 | 3203.30 | 3996.72 | 3833.35 |
| D | 4.72 | 3130.76 | 3863.67 | 3673.01 |
| D2 | 4.90 | 3124.03 | 3847.65 | 3752.19 |
| E | 4.93 | 3175.45 | 3886.47 | 3732.58 |

The emitted managed-local AOT object shrinks 6,864→5,600 bytes with A; the escaping object stays 8,920 bytes. D1's call-probe object shrinks 7,664→7,464 bytes. Full compiler binaries grow 7,272 bytes for A and 248 bytes for D2; these are whole-file sizes, not text-section sizes. Exact identities and code sizes are retained.

A's conservative proof rescans local uses per candidate and uses ordered sets. That is an engineering scaling risk; these corpus compilation medians do not prove large-function scalability. Preserve the 16 KiB/site limits and move reusable use facts into existing typed-IR analysis if a measured large eligible function justifies it. C adds root-clear stores/IR and has the highest compilation median here. No artificial compilation gate is introduced.

## Qualification and failures

- Independent A–E: Linux Release base 7 plus expanded 6 maintained tests; strict base 7 plus async 3. B/E additional fatal TSan coordination and multithread allocation controls pass. A forced-GC/fault proofs, B storage-reuse/rollback controls, E failure/pacing/continued-collection controls pass, with corresponding strict coverage.
- D2: Linux Release 13, derived fatal strict 13, both cache-process controls and maintained race fixture pass. Derived strict replaces only the changed Runtime translation unit; all consumers use unchanged headers/layout and recorded matching archives. No cache-identity proof is claimed from the derived strict fixture.
- Native Windows 10 build 19045.6466: baseline, A and D2 each pass 13 tests in **Release and Debug**, with normal/O2 JIT, linked AOT and 1 MiB PE-stack receipts. A includes allocation-elimination and negative/fault proofs. VS2022 x64 MSVC14.44.35207, SDK10.0.19041.0, physical LLVM23.1.4 and verified profile-matched providers were reused in a verified private NTFS root. B/C/E have no native Windows acceptance or cross-platform integration recommendation.
- Preserved failures: B standalone-Heap compatibility; E explicit-budget retained-cycle failure; intermediate E buffer-trigger omission found during source review; exact-relink wrong emission scope; initial deprecated CLI option; incomplete baseline test-target build. The last two baseline qualification attempts failed because five executables had not all been built, not because a compiler test assertion failed. After all target existence was verified, the baseline recheck passed 7/7; its expanded 6/6 also passed. No tests were disabled.
- No macOS acceptance claim, no hosted CI. No real native library/HTTP campaign was reopened; relevant lifetime/native-worker/callback controls remain the maintained selections.

## Combined candidate and integration decision

**Combined A + D2: WORKING EXPERIMENT; qualification complete.** Linux Release 13, fatal strict 13, allocation/fault/numeric/initializer proofs, maintained race and both cache controls pass. Windows 10 Release and Debug each pass 13 maintained tests plus allocation/fault and normal/O2/AOT probes. All four principal compiler/Runtime/managed/async PE runners have verified 1 MiB stacks. Strict derives from unchanged public headers/consumer objects, rebuilt combined backend and independently qualified D2 Runtime; exact identities are retained.

Four fresh matched blocks, 12 source samples/case. Compilation is excluded from these invocation timings:

| O2 source case | Baseline ms | Combined ms | Change | Baseline / combined CPU ms |
| --- | --- | --- | --- | --- |
| managed-local | 6.022 | 0.697 | -88.4% | 5.963 / 0.694 |
| managed-escape | 7.169 | 6.961 | -2.9% | 7.086 / 6.897 |
| managed-reference | 8.651 | 8.536 | -1.3% | 8.558 / 8.430 |
| managed-across-edge | 6.880 | 6.548 | -4.8% | 6.822 / 6.495 |
| managed-survives-allocation | 49.066 | 40.658 | -17.1% | 48.419 / 40.103 |
| closures | 1.629 | 1.591 | -2.4% | 1.621 / 1.579 |
| calls | 1.325 | 0.868 | -34.5% | 1.324 / 0.864 |
| workers | 0.861 | 0.782 | -9.2% | 1.664 / 1.488 |
| async-tasks | 1.654 | 1.718 | +3.9% | 1.644 / 1.700 |

Independently linked application probes, with timer calls outside each compute operation (same 20,000 iterations, warmup and 12 samples):

| AOT source case | Baseline ms | A ms | D1 ms | D2 ms | Combined ms |
| --- | --- | --- | --- | --- | --- |
| managed-local | 8.184 | 0.684 | 8.439 | 8.007 | 0.631 |
| managed-escape | 9.873 | 9.508 | 9.719 | 9.519 | 9.430 |
| managed-survives-allocation | 52.305 | 43.414 | 53.058 | 50.977 | 43.327 |
| calls | 1.430 | 1.395 | 0.821 | 0.795 | 0.783 |

Combined managed-local observations retain zero managed constructions/native requests/collections. Escape/reference negative cases retain allocation and injected failure. The JIT managed-local process peak RSS median changes 46,524→46,070 KiB; mixed temporary allocation changes 46,784→46,894 KiB. These include compiler/JIT state and are not evidence that total process memory vanishes when the local object is eliminated. Complete raw counts/CPU/RSS/pause observations are archived.

**Unfavorable outcomes remain:** combined AOT array/4096 is 4.731→4.938 ms (+4.4%); paired block medians are +0.7%, +5.4%, +4.0%, +3.9%. O2 async tasks are 1.654→1.718 ms (+3.9%). The generated array function's IR is identical after normalizing debug/profile IDs, and optimized async-task IR is byte-identical in baseline/D1/combined. The entire combined G31 executable is byte-identical to D2 (`97205d8111d78f66045994b2e27a465e04ae0828253e4a2c837f510f4bf0b4eb`); exact entry-object relinking reproduces it. Thus A introduced no new G31 AOT code interaction. Runtime poll implementation and linked/JIT code placement remain differences. A specific microarchitectural cause is **UNKNOWN**; shorter source/common-path code does not guarantee faster every workload. No second combined candidate or profiling campaign is invented to erase the result.

Matched combined cold AOT is 4.828→4.940 ms, normal JIT 3,143.015→3,176.464 ms, O2 JIT 3,852.475→3,871.496 ms, compile+link 3,720.040→3,722.513 ms. These are four-process medians, not a guarantee of zero analysis cost. Full uninstrumented normal-JIT and whole-corpus samples/ranges are retained.

**Recommendation: LIMITED GO for A as the first narrow integration candidate, after Golden Wrench review.** It has direct allocation/code/fault proof, native Windows qualification and no G31 AOT code change. D2 is a useful, qualified call-overhead experiment but is **not an unqualified default-integration recommendation** while these measured tradeoffs remain. Boss/Wrench may explicitly accept that tradeoff or request a bounded correction; this report introduces no new performance gate or release hold. Do not automatically integrate the combined patch merely because its target probes improve. B/C/E remain implemented experiments with exact continuation work below; none is silently deleted from the roadmap.


## Specific engineering continuation

1. Hand Keyboard A as the first bounded integration candidate; retain D2/combined as separately reviewed options with the measured tradeoffs: name/review the escape proof, retain all negative/fault/constructor/stack tests, retain a noinline spelling supported by the pinned Clang targets (Windows clang-cl is exercised here), and keep the unchanged safepoint/request protocol. Do not expand eligibility while integrating. Native macOS and owner-controlled CI remain later acceptance requirements.
2. B: retain the working bounded pool and evidence; resolve repeatable whole-workload/layout effects and sanitizer poisoning before production. Native mixed-size wins do not authorize TLS caches or registry validation removal. Map-node/reference-map costs remain measured counts for a later focused improvement.
3. C: retain demonstrated root-liveness activation; evaluate a concrete local-last-use case with lower descriptor/retention cost, rather than ship general clear stores for mixed small gains. Do not revive root caches/indexed iterators.
4. E: retain fixed host contracts and explicit successful-collection update. A practical next refinement needs a byte/live-memory bound and meaningful pause sampling, not merely a higher allocation count budget. Current evidence does not justify a concurrent/generational collector.
5. Keep the complexity inventory: duplicate trigger ownership in construct_into/BufferBuilder, registry/native request cost, root descriptors versus nonzero liveness, global empty-frame identity allocation, TLS initializer guard and proof-analysis scaling. The TLS guard is a source/disassembly observation only; no new TLS patch or claimed speedup was tested.

## Archive, cleanup and Git

The evidence archive is `/home/shmellyorc/Documents/Void.Language-archives/g33-performance-foundations-experiments-2026-11-13.zip`. The external `archive-index.json` records its SHA-256, CRC/extraction/source rehash checks and task-only cleanup receipt. Build outputs are removed only after verification. This archived report is the pre-cleanup closeout; the final cleanup receipt is retained externally and appended to the local report.

Tracked Git diff is empty. Exact status before archive:

```text
?? docs/compiler-health/G32_CHECKED_BOUNDARY_ATTRIBUTION.md
?? docs/compiler-health/G32_SHARED_RUNTIME_PERFORMANCE_ATTRIBUTION.md
?? docs/compiler-health/G33_PERFORMANCE_EXPERIMENTS.md
?? docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md
```

The first two reports predated this task and retain their recorded hashes. No staging, commits, pushes, branches, CI dispatch or production edits. Boss retains integration and release authority.

## Verified cleanup closeout

Evidence ZIP: **21,520 files**, 28,953,885 bytes; SHA-256 `e550da875ee3efeca15962ca603eee5b8f6a0ba58966280d0b99dbfed2c41a33`. CRC, extracted-member SHA-256 and source rehash checks passed before removal. The ZIP retains the pre-cleanup reports; this section records subsequent cleanup.

Removed only the inventoried G33 Linux root, Windows private NTFS root and task transport. No active file/mapping consumers remained. Observed free space increased **16.27 GiB on Linux** and **2.03 GiB inside the Windows guest**; these are separate filesystems. The original Windows helper junction target, SDKs/providers and previous archives were preserved.

The first Windows cleanup guard stopped before deletion because `test_output.py` was absent from the older helper target. The corrected check uses the established `setup_host_deps.py`; its before/after hash matched. Initial refusal and corrected cleanup script are retained as post-archive sidecars. Cleanup and sidecar hashes are recorded in `/home/shmellyorc/Documents/Void.Language-archives/archive-index.json` and `g33-cleanup-receipt.json`. This was a cleanup-script correction, not a compiler failure.

Final tracked and staged Git diffs are empty; the four untracked documentation paths listed above remain. The original two G32 reports retain their starting hashes. No production implementation, Git staging/commit/push or hosted CI occurred. **Stop for Golden Wrench/Boss review.**
