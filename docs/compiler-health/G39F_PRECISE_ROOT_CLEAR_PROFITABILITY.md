# G39-F — precise root-clear profitability correction

Accepted base: `d1fc214f17715d464af5fc2d6cb4177258f9eb8f` (G38). Review worktree: `/home/shmellyorc/Documents/Void.Language-G39`, branch `codex/g39-managed-lifetimes`. Qualified G39 backend: `c832db0f6ccf186b2f6d6f8fcb4e17b7f1ac73d9dfa73d869fa218bab94e2441`. Final G39-F backend: `288a7d16ede721f28291fee407e6c4df1e5862d1cc9d07b0097a9d1cbfd6ef44`. LLVM/Clang/LLD **23.1.4**.

**Recommendation: INTEGRATE for Golden Wrench review, with the measured tradeoffs below made explicit for the owner decision.** The correction reduces executable clearing work while preserving every measured G39 construction and GC counter. It fixes the optimized retirement control and removes public-helper clearing back to the accepted G38 image. It does not make every workload faster: normal loop-carried remains +4.8% versus G38; bundled and JIT context costs remain. Boss controls staging/commits/pushes/hosted CI; none occurred.

## Implemented source and preserved capability

Only `src/backend/llvm/backend.cpp` changes beyond the qualified G39 compiler. The complete candidate retains G39's direct reference-containing temporary elimination, block-local payload/child-copy retirement, readonly-field current-Heap provenance from completed validated owners, and bounded branching private/generic helpers. It retains G35's indexed alias/must-CFG ownership proof, factory freshness versus root-transfer distinction, 256-byte object/16-KiB function payload limits, G31/G34 polling/reuse/pacing, G36 collection improvements and G38 bounded AOT SIMD. G37-F Runtime/ABI changes are absent.

The correction postprocesses the existing retirement plan. It seeds retained construction/allocation, explicit collection, and verified fresh private factory calls as collection drivers. One reverse reachability walk uses the existing predecessor graph, including backedges. A reverse instruction walk distinguishes drivers after each proposed retirement. Polls alone are not drivers: `Runtime::poll()` services an existing external request; allocation checkpoints and explicit collection can initiate collection. A reachable driver is a conservative potential benefit, not a guarantee of collection or a proof that every copy remains nonzero.

Copy clears are batched only through intervals without a safepoint. Calls, polls, retained allocation/collection, block boundaries and mandatory payload retirement stop batching. Lowering groups only adjacent pointer words in existing Alloca homes, using LLVM's base/constant-offset query and the target DataLayout; a contiguous run uses a zero memset, an isolated/unknown address keeps its scalar store. No root homes, Slot geometry, scanning protocol, Runtime, public header, allocator, cache or ABI entry points change.

| Classification | Final handling |
|---|---|
| Mandatory payload-field lifetime clear | Always retained at its G39 boundary |
| Copy retirement in a function with payload retirement | Conservatively retains the G39 safety floor; batching preserves all safepoint observations |
| Optional final-use copy with reachable driver, including a later loop instance | Retained; adjacent stores may combine |
| Copy with no reachable allocation/explicit collection/fresh-factory driver before function exit | Clear omitted; ordinary root lifetime retained |
| Unknown/public/native/virtual/callback/async/worker/resource/cleanup retention | Existing G39 eligibility and conservative guards remain |

An extracted child/result reaches its own authoritative existing home before any owner payload clear. Surviving child aliases and returns retain their independent roots. Batching delays dead-copy retirement only inside an unobservable interval; it cannot expose an earlier stale reference or cross an implicit block-entry poll/root-home reuse. Managed stack-owner addresses never become heap references. Constructor ordering, identity/mutations, readonly and nullable authority, checked errors, native interaction, cleanup, reload/callback and cancellation behavior retain G39's proof. Only an eliminated construction's own OOM possibility can disappear.

## Rejected first cutoff and correction

The first candidate used only later same-block drivers. Functional/native/sanitizer and visibility tests passed, but measurements disproved its profitability classification: alias-rebind marking **156→780**, native requests **312→1,248**; loop-carried marking **78→390**, requests **156→624**. A later loop allocation was a real observation opportunity. That candidate is rejected, not a performance success. Its exact backend (`bc354f11…`), complete measurements, receipts, IR/native evidence and rejection memo are retained under `attempt1/` in the archive. The final shared CFG reachability correction restores those useful clears. No allocation-elision eligibility was reverted.

## Emitted clearing work

These are exact whole-module LLVM instruction counts, `scalar pointer-null stores / zero memsets`; unchanged initialization and source-null stores are included, so the delta is the meaningful clear-work comparison. A memset is not necessarily one native instruction. Both pre-pass and final emitted IR and selected disassembly are retained. Constructor call-site counts agree for all 90 mode/pattern pairs; all 180 IR-observer objects equal ordinary emission byte for byte.

| Pattern | G39 before LLVM | F before LLVM | G39 optimized | F optimized |
|---|---|---|---|---|
| root_retirement | 21 / 8 | 9 / 11 | 8 / 3 | 3 / 5 |
| alias_rebind | 6 / 4 | 1 / 6 | 6 / 2 | 1 / 4 |
| loop_carried | 5 / 4 | 2 / 5 | 5 / 2 | 2 / 3 |
| public_helper | 2 / 5 | 0 / 5 | 4 / 4 | 0 / 4 |
| helper_block_limit | 7 / 5 | 0 / 5 | 7 / 1 | 0 / 1 |
| direct_reference | 9 / 4 | 2 / 6 | 8 / 3 | 2 / 4 |
| readonly_field | 14 / 5 | 4 / 9 | 13 / 2 | 3 / 6 |

For example, optimized retirement changes eight scalar null stores/three zero fills to three/five. Its hot retirement sequence uses adjacent vector-width zero stores instead of independent word clears. Alias rebinding retains the clears justified by its later allocations; loop-carried retirement remains active. Public-helper standalone images are byte-identical to G38 in both AOT modes. Remaining linked-bundle movement is not evidence of additional helper work. Every pre-LLVM poll call count is preserved.

## Exact allocation, root and collection evidence

The independent arithmetic/domain oracles and all 45 source patterns run at n=20,000 in normal/O2 JIT and independently linked normal/optimized AOT. Every one of the 180 construction comparisons agrees with qualified G39. Further, all three samples of **eight counters across all 180 comparisons** exactly match G39: constructions, native C++ requests, root words, Slot visits, occupied roots, marking, collections and allocated payload after call. Clear batching changes executable work without sacrificing useful measured retirement.

| Pattern | Nominal source constructions | G38 heap constructions | G39 | G39-F | G39→F native requests |
|---|---|---|---|---|---|
| direct_reference | 40000 | 40000 | 20000 | 20000 | 0 → 0 |
| direct_nested | 60000 | 60000 | 40000 | 40000 | 40000 → 40000 |
| direct_nullable | 20000 | 20000 | 0 | 0 | 0 → 0 |
| readonly_field | 40000 | 40000 | 20000 | 20000 | 0 → 0 |
| helper_branch_write | 20000 | 20000 | 0 | 0 | 0 → 0 |
| root_retirement | 40000 | 20000 | 20000 | 20000 | 0 → 0 |
| alias_rebind | 40000 | 40000 | 40000 | 40000 | 312 → 312 |
| loop_carried | 20001 | 20001 | 20001 | 20001 | 156 → 156 |
| public_helper | 20000 | 20000 | 20000 | 20000 | 624 → 624 |
| direct_identity | 60000 | 60000 | 60000 | 60000 | 80936 → 80936 |
| direct_incoming | 40000 | 40000 | 40000 | 40000 | 40936 → 40936 |
| child_survives | 40000 | 40000 | 20000 | 20000 | 156 → 156 |

Root/GC values below are medians of the three optimized-AOT observer samples; all other modes and each raw sample are retained. Slot visits count descriptor visits, not the number of Frames. Post-call payload means allocated bytes awaiting collection, not proven reachable bytes.

| Pattern (G39=F) | Root words / Slots | Nonzero / mark visits | Collections | Post-call payload B |
|---|---|---|---|---|
| root_retirement | 780 / 234 | 0 / 0 | 78 | 384 |
| alias_rebind | 1092 / 312 | 156 / 156 | 156 | 772 |
| loop_carried | 390 / 156 | 78 / 78 | 78 | 400 |
| public_helper | 390 / 156 | 390 / 390 | 78 | 388 |
| direct_reference | 702 / 156 | 0 / 0 | 78 | 384 |
| readonly_field | 1014 / 156 | 0 / 0 | 78 | 384 |

Against G38, retirement-only retains G39's 780 root words/234 Slot visits/78 collections, while occupied roots/marking remain 234→0 and native requests 468→0. Scanning geometry is not reduced. The final raw-address witness performs **24,000 forced collections**, without adding a root, across Release G39/F and fatal-sanitizer F, normal/O2 JIT and independent AOT. Both patterns retain precisely 4 live payload bytes through every required observation. The three-instance retirement fixture proves prior children disappear before the next child construction. Child allocation and remaining null/arithmetic failure, cold injected OOM/recovery and foreign-child checks pass.

Current-Heap eligibility is unchanged: completed local owner validation/readonly-field proof is required. Incoming, mutable-field, unknown-factory, multiple-origin, identity-observed, escaping/captured/callback/native/async/loop-carried and cleanup controls retain their G39 fallbacks. Foreign children retain the exact rejection path; a managed type is never treated as evidence of current Heap ownership.

## Native and sanitizer qualification

Final corrected source passes Linux Release **22/22** and fatal ASan/UBSan/LeakSanitizer **22/22**, plus debug/automatic cross-process cache controls **2/2 per profile**. Fatal TSan repeats all **45 patterns × two independent AOT modes**, coordination plus race/allocator controls; generated cases/race controls run under 1 MiB. Release and strict normal/O2 JIT and independent AOT/managed runners also pass explicit 1-MiB stack invocations.

Native Windows **10 build19045**, x64 VS2022/MSVC14.44.35207, SDK10.0.19041.0, LLVM23.1.4: final Release **22/22**, Debug **22/22**, independently rebuilt and linked, normal/O2 JIT/AOT, foreign/failure/resource/callback/reload/worker/async controls. Seven PE headers per profile verify 1,048,576-byte stack reserve. All 3,294 native compiler/test inputs match their exported identities; installed providers/SDKs remain unchanged. Windows performance and macOS qualification are not inferred.

The original G39 Linux82/82 and Windows67/67 qualifications remain archived; they are reused as prior coverage only where source/emission identities justify it, not relabeled as full G39-F reruns. Runtime/Core Release archives remain byte-identical to G38/G39. Native and Linux receipts identify this exact final backend.

## Balanced observer-off performance

All task compilation/qualification finishes before throughput sampling. Six balanced standalone blocks cover three naturally linked variants, five samples/cell, independent oracles, n=20,000. Four matched blocks/three samples cover the full 45-pattern normal/O2 JIT and independent AOT corpus. No padding, forced alignment or special linker ordering is used. The earlier recorded G39 +6–9% regressions are preserved in its archive and `legacy-results`; matched G39 baselines move with context and are reported rather than assumed to repeat those percentages.

| Standalone | F/G39 normal | F/G39 optimized | F/G38 normal | F/G38 optimized |
|---|---|---|---|---|
| root_retirement | +2.1% | -7.3% | +0.3% | -4.7% |
| alias_rebind | +1.4% | -1.6% | -1.1% | +2.6% |
| public_helper | +2.2% | -4.7% | +2.5% | +2.4% |
| loop_carried | +2.7% | +2.2% | +4.8% | +2.1% |
| incoming_rebind | +2.7% | -1.3% | +2.2% | -2.5% |
| helper_block_limit | -1.9% | -3.9% | -2.5% | +0.6% |
| factory_field | -1.3% | -0.1% | -2.9% | +5.6% |
| direct_incoming | +1.3% | +0.8% | +1.1% | +1.2% |
| direct_reference | -3.5% | +1.0% | -56.9% | -55.1% |
| readonly_field | -2.2% | -3.2% | -55.5% | -56.8% |
| helper_branch_write | +8.2% | +1.0% | -77.8% | -80.3% |
| direct_nested | -5.0% | -5.0% | -40.1% | -39.0% |
| child_survives | +0.4% | -1.0% | -43.6% | -46.1% |

Negative means less region time. Major G39 gains survive: direct-reference about55–57%, readonly-field55–57%, branching mutation78–80%, nested references about the retained three-way values above. Optimized retirement improves7.3% versus G39 and4.7% versus G38; normal retirement is +0.3% versus G38. Useful loop retirement still costs +4.8% normal/+2.1% optimized versus G38. Alias optimized is +2.6% versus G38 despite fewer clear instructions and retained GC benefits. These are owner tradeoffs, not silently resolved costs.

The linked bundle has different context. The following are its largest adverse F/G39 rows per mode; full favorable and unfavorable rows remain in `throughput-summary.json`. They do not erase standalone results or establish a microarchitectural cause.

| Mode | Largest adverse bundle rows |
|---|---|
| jit-normal | helper_write +8.8%; direct_factory +8.4%; nullable +8.4% |
| jit-O2 | nullable +9.9%; direct_null_failure +9.2%; nested +7.4% |
| aot-normal | branch_birth +10.4%; branch_alias +6.7%; helper_read +6.7% |
| aot-optimized | helper_block_limit +8.0%; public_helper +7.5%; helper_branch_rebind +4.7% |

Representative integer/float/dependent arithmetic, arrays, dictionaries, sorting, text/JSON and native I/O pass all independent oracles in normal/O2 JIT and AOT. The complete representative optimized AOT image is byte-identical G39/F (`8a7b30f1…`); its variation is a control. Largest representative row is app-normal float-1000000 +9.3%. Managed calls/closures/workers/async plus allocation/reference controls pass; calls/closures/workers/async AOT objects remain identical. Largest managed row is jit-O2 workers +14.2%; this JIT observation remains unresolved, and identical AOT objects alone do not prove a JIT performance cause. No speedup is claimed for unchanged code. Historical G34 array/async/GC-tail, G35 JIT-control, G36 numeric/worker/JSON-tail, G37-F and G38 compilation/JIT-context holds remain visible.

## Compilation, startup, memory and GC intervals

100/500/2,000 are total source construction expressions (owner+child pairs), not only owners. Three balanced compilation blocks measure frontend analysis, LLVM lowering/JIT, process CPU/wall and wait4 peak RSS. The final CFG/clear scheduling adds cost; it does not improve this scaling control.

| Total sites | Lower/JIT ms G39→F | Change | Peak RSS KiB G39→F |
|---|---|---|---|
| 100 | 101.19 → 102.16 | +1.0% | 50564 → 50952 |
| 500 | 527.64 → 555.30 | +5.2% | 67696 → 68504 |
| 2000 | 4356.89 → 4521.92 | +3.8% | 137868 → 139904 |

Warm-filesystem startup uses20 balanced blocks, n=0, separate processes. These are not cold-disk or deployment startup guarantees.

| Startup | Wall ms G39→F | Peak RSS KiB G39→F |
|---|---|---|
| aot-normal | 2.434 → 2.528 | 14788 → 14788 |
| aot-optimized | 2.418 → 2.413 | 14788 → 14788 |
| jit-O2 | 22.332 → 21.453 | 47182 → 47092 |
| jit-normal | 20.191 → 19.543 | 45108 → 45000 |
| representative-aot | 3.242 → 3.177 | 14788 → 14788 |

Complete request-to-release latency and full stopped-world intervals are recorded separately, observer-on. Descriptive stopped µs median/p99/max below use all three optimized-AOT samples; p99 is an order statistic, not a population tail guarantee.

| Pattern | G39 stopped µs med/p99/max | F stopped µs med/p99/max |
|---|---|---|
| root_retirement | 7.06 / 11.96 / 13.27 | 7.10 / 12.34 / 19.31 |
| alias_rebind | 7.06 / 9.36 / 21.38 | 7.00 / 14.82 / 22.20 |
| loop_carried | 7.08 / 12.44 / 17.60 | 6.96 / 9.36 / 11.25 |
| public_helper | 7.23 / 9.19 / 30.51 | 7.21 / 13.44 / 16.89 |

Separate representative array/sort/dictionary/text/JSON/I/O observers retain constructions, native requests, occupied roots, marking, root/Slot scans, allocated payload and every full interval. Their final GC samples were replayed in isolation after the throughput campaign: a cache CLI prerequisite had overlapped the first representative GC campaign. Those contaminated intervals are retained under `setup-failures` and excluded from final GC claims. The initial cache-test missing-executable setup failure is also retained; building the task CLI yielded2/2 in each profile. Native requested-memory counters are per observed thread/epoch, not whole-process live memory or global threaded peaks. Failed-allocation recovery remains a semantic qualification, not a throughput inference.

## Complexity and remaining work

The correction introduces three shared bounded instruction passes, one worklist over the existing CFG predecessor edges, two temporary block-bit vectors and a worklist, plus transient grouping/sorting of clear addresses at emission. Ordered retirement/factory lookups remain. It never rescans the whole function per allocation candidate. It removes scalar clear instructions/empty retirement events, not source analyses or GC metadata. The CFG facts are necessary: the rejected same-block shortcut measurably lost useful GC retirement. The additional500-site +5.2% and2,000-site +3.8% lowering costs and roughly0.8/2.0MiB peak RSS increases remain adverse results.

The function-wide payload safety floor is conservative. Known-null/overwritten copy homes and cross-edge overwrite precision can leave redundant stores; a reachable driver may be on only one feasible path or occur after a home overwrite. This mission does not add another last-use, root or escape framework to resolve those cases. It does not claim all clears are minimal or universally faster.

Still active: broader escape analysis beyond G39; general direct-reference CFG lifetimes; incoming/mutable/factory/alias current-Heap provenance; interprocedural retention; checked collections; dictionary/sorting; text/Unicode/JSON materialization; SIMD beyond G38; native/JIT context tradeoffs; GC-tail and startup; future native macOS/release qualification. The original G39 report remains historical qualified evidence, not an assertion that its previous profitability hold was resolved without costs.

## Reproducibility, inventory and handoff

Evidence: `/home/shmellyorc/Documents/Void.Language-archives/g39f-root-clear-profitability-2026-11-15.zip`. DEFLATE9, CRC, every SHA256 manifest entry, and reconstruction of all source postimages from accepted G38 preimages plus the complete patch are verified before task-only cleanup. Exact archive bytes, retained-file count, archive SHA256, final Git inventory and separately observed host/guest reclamation are in sibling `-verification.json`, `-closeout.json`, `-cleanup.json` and `-git-inventory.json` receipts. An archive cannot contain its own digest; those receipts are authoritative. Replay scripts, independent oracles, raw results, IR/native objects, rejected cutoff and significant setup corrections are retained. Installed SDK/providers and original G37-F/G38/G39 worktrees/archives remain protected; guest free-space recovery is not host VDI compaction.

Final source inventory is **29 files: eight tracked modifications plus21 new files**, no staged entries: CMakeLists, permanent ledger, backend, five escape-test harness files, original G39 report, this G39-F report and19 G39 fixtures. F adds no fixture or ABI surface; all G39 fixtures are preserved byte for byte. The exact path/hash list and raw status are in the archive inventory/companion receipt. HEAD/branch stay as stated above. The ledger keeps one copy of the previously duplicated identical G39 entry, adds G39-F and preserves every unfinished objective/history.

**Next action:** Golden Wrench reviews this exact compiler-only candidate and its explicit loop/JIT/compilation tradeoffs; Boss retains all Git/hosted-CI authority. The next narrow implementation is an indexed proof of already-null SSA/local homes, removing only proven-zero copy clears while preserving CFG-useful and mandatory payload retirement. No further tuning cycle, padding or alignment workaround is part of G39-F.
