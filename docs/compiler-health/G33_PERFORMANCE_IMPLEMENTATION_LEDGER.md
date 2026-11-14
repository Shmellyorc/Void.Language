# G33 — Performance implementation ledger

Baseline `0407f83f9d6dd660b1503df1e68cdcee9b34b77b`. **G33-I: Experiment A integrated; production qualification complete on Linux and Windows 10 under explicit owner authorization.** See [managed allocation integration](G33_MANAGED_ALLOCATION_INTEGRATION.md). G32/G32-A findings remain authoritative. The G33-X campaign notes below are historical experimental evidence.

## Current integration ownership

Only A is in production source: `block_allocation_elision` in `src/backend/llvm/backend.cpp` owns the exact archived block-local scalar proof. Construction uses existing staging; eligible aliases have no GC root homes. The archived predicates and lowering are verified equal after helper extraction and formatting. Eligibility remains 256 bytes/object and 16 KiB/function, with no broader escape promise. Maintained JIT and independently linked normal/optimized AOT tests live in `tests/allocation_elimination`; count, alias, effects, numeric, initializer, fault, root, loop-instance and storage-boundary controls accompany integration.

B, C, D1/D2, E and the combined candidate remain experiments. Their exact sources, failures and next actions are preserved in `/home/shmellyorc/Documents/Void.Language-archives/g33-performance-foundations-experiments-2026-11-13.zip` (SHA-256 `e550da875ee3efeca15962ca603eee5b8f6a0ba58966280d0b99dbfed2c41a33`), under `linux/overlays/<variant>` and `linux/evidence/<variant>.patch`. This archive was verified again for G33-I; it is retained unchanged. No Runtime, GC, pacing, pooling, poll or empty-frame experiment is integrated here.

Broader escape analysis remains **NOT YET IMPLEMENTED**: reference-containing objects, interprocedural retention/return effects, unsupported aliases, identity, native/callback and async escape, cleanup and cross-edge lifetimes require their own proofs and qualification. A's per-candidate conservative scan remains a scaling consideration; these bounds are not authorization to generalize it.

## Historical G33-X campaign

Task workspace: `/home/shmellyorc/.local/state/void-g33x-_nnnfykx`. Build driver: `scripts/build.py <variant> <release|strict> [targets...]`; read-only bwrap overlays from `overlays/<variant>/`. Measured binaries must use matching headers/Runtime. Builds and measurements must not overlap. Record failed fixtures exactly before changes. G31 corpus/oracles and G32 escape fixtures were restored from the verified G32 archive.

## Owner decision

Boss/Golden Wrench approved Option 1 for A: a proven-eliminated allocation may eliminate only its own OOM possibility. No dummy allocation is required. Preserve remaining failures, constructor effects/order, cleanup, identity, aliasing and roots. The separate replacement-GC discussion is on hold; E remains the authorized bounded pacing experiment, not a new collector.

The final report is [G33 performance experiments](G33_PERFORMANCE_EXPERIMENTS.md). The following disposition table is authoritative; progress notes below remain as historical investigation evidence. The task workspace is disposable and restored/replayed from the verified archive.

## Complete feature ledger

| Bible capability | Experiment / source owner | Exact status | Next action | Integration |
| --- | --- | --- | --- | --- |
| §36, §37 allocation elimination | A: scalar-only block-contained managed allocation/root-home elimination | INTEGRATED; QUALIFIED ON LINUX / WINDOWS 10 | Maintain exact eligibility/count/fault/root negatives; broader escape analysis remains separate | Owner-authorized A only; Release 19, fatal strict 17, Windows Debug/Release 16 each; small JIT controls documented |
| §37 reusable small allocation | B: bounded Heap-owned size-class reuse under current lock | WORKING EXPERIMENT; NEEDS CORRECTION for integration | Resolve whole-workload layout regression and sanitizer poisoning; measure 64-block cap versus 256-allocation budget before TLS/pages | Not ready; native mixed win, small/threaded limitations retained |
| §37.1 precise root liveness | C: clear block-local SSA roots at synchronous edges | WORKING EXPERIMENT; broad benefit not demonstrated | Target one proven local last-use/descriptor reduction; retain root authority and negative tests | Not ready; nonzero visits fall, descriptor visits unchanged |
| §40.2 release call optimization | D1 empty frames; D2 same plus cold poll split | WORKING EXPERIMENT; measured tradeoffs remain | Owner review of call gains versus AOT-array/async regressions; retain every poll, cache/native/reload proof | D2/combined qualified on Linux strict and Windows, not blanket integration GO |
| §37 throughput/pauses | E: bounded live-object source pacing, fixed explicit host budgets | WORKING EXPERIMENT; NEEDS CORRECTION for default policy | Add evidence-backed byte/live-memory bound and meaningful pause sampling; retain both trigger owners/failure rollback | Not ready; fewer collections, more memory and longer pauses |
| §37 thread-local caches | B chooses shared Heap-owned reuse under existing lock first | NOT YET IMPLEMENTED as thread-local caching | Measure contention before adding per-thread lifetime/flush protocols | No promise of TLS allocator |
| §37 reusable pages / large objects | B uses bounded freed blocks rather than a new page allocator; large allocations unchanged | NOT YET IMPLEMENTED as page/bump allocator | Record whether native allocation or registry work dominates after B | No additional allocator prototype unless justified |
| §37 advanced/concurrent/generational GC | Existing precise nonmoving coordinated mark/sweep remains | NOT YET IMPLEMENTED; outside authorized replacement scope | Use A–E pause/CPU data to identify prerequisites only | No replacement collector |
| §56 / optimization plan specialization | Existing concrete specialization and LLVM O2 remain; D examines actual retained boundaries | PARTIALLY IMPLEMENTED in production | Separate unchanged LLVM promotion/inlining from new VOID changes | Existing behavior retained |
| Automatic SIMD | Accepted AOT-only policy and disabled SLP unchanged | IMPLEMENTED WITH APPROVED BOUNDARIES | Regression controls only | No SIMD changes |

## Qualification and measurements

Fresh accepted Runtime identity and independent Release A–E builds are verified. Pending expanded Release/strict tests, normal/O2 JIT and independently linked AOT, allocation/root/pause observations, representative Go references and matched workload comparisons. A has now passed native Windows 10 Debug/Release functional qualification; D2 is running there. Native Windows 10 Debug/Release is mandatory before any cross-platform integration recommendation; macOS acceptance requires native evidence. No hosted CI.

Use classifications WORKING EXPERIMENT / NEEDS CORRECTION / NOT YET IMPLEMENTED / REJECTED WITH EVIDENCE. A failed first candidate does not remove its feature from this ledger. Preserve exact patch, activation proof, failure, bounded repair and remaining work for each variant.

## Continuation / storage

Fresh baseline built in `current-release`; Runtime SHA-256 `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc` matches accepted G31-I. Independent Release A–E builds have completed. Focused Release checks passed 7/7 for A, corrected B, C, D and corrected E; broader qualification and matched performance remain pending. Exact source overlays and preliminary patches are under `overlays/A` … `overlays/E` and `evidence/{A,B,C,D,E}.patch`. Initial source/Git/free-space receipt: `evidence/start.json`. Prior report hashes: `evidence/previous-reports.json`. Final ZIP/manifest/hash verification and task-only cleanup are pending. Protect active builds, SDKs, providers and older archives. No staging, commits, pushes, CI or production edits.

## Complexity encountered — additional Boss directive

Record complexity in code actually inspected/overlaid; distinguish measured cost from mechanism-only risk. Do not turn incidental findings into unrelated implementation.

| Area | Current source observation | Evidence status / experiment | Follow-up evidence needed |
| --- | --- | --- | --- |
| Managed publication | `construct_into` validates root/output before preparation and again after a possible collection; Heap separately registers object metadata | Required state-boundary checks, not all duplicate; B changes storage reuse only | Count remaining native registry allocations and source impact |
| Managed allocator | Payload allocation plus unordered-map node allocation/registration; cached payload alone cannot eliminate node work | Mechanism confirmed; B measurements pending | Native requests, mixed-size controls, source results, contention |
| Root homes | Function-duration packed SSA homes retain references after block-local SSA dies; precise descriptor still covers every home | C directly tests shorter lifetime, without changing lookup authority | GC live payload/mark work versus extra clear stores |
| Managed call boundary | Empty frames still consume frame setup/identity reservation and teardown, even with no roots | D directly tests frame omission; polls/checkpoints remain | Call-heavy source timings, emitted code, cancellation/reload |
| Metadata checks | Current array/dictionary metadata and root checks are substantial; G32-A's duplicated scalar helper correction regressed unrelated AOT controls | Historical measured evidence retained; no unchanged revival | Avoid attributing linkage/layout regressions to missing safety checks |
| GC pacing | Allocation-count budget ignores live-object count; blindly adapting explicit host budgets broke forced-collection tests | E v1 failure reproduced; E v2 separates source policy from explicit fixed host budget | Frequency, live/retained memory, pauses, contention and error rollback |
| Backend maintenance | Root-home construction, lifecycle exit branches and operation lowering share a large emitter; small source changes can affect many paths | Engineering complexity confirmed, runtime cost not implied by file size | Qualification and emitted-code inspection; no speculative emitter rewrite |

## Verified activation and preserved failures (interim)

- A: the scalar-only managed fixture executes 1,000 constructions in baseline and zero in the experiment, with identical scalar output. Alias mutation and field evaluation effects are preserved. Escaping, reference-containing and across-edge values remain heap allocated. Emitted normal/O2/AOT IR removes the eligible allocation call; escaping IR is byte-identical. Separate forced-collection/fault controls pass in normal/O2 modes. This is a narrow prototype, not production acceptance or complete escape analysis.
- D: the call-heavy fixture's optimized IR has three frame-enter and three frame-leave call sites in baseline versus one each with D. Poll/checkpoint call sites remain eight each. Static call-site counts are not dynamic timing measurements. Disassembled AOT objects and byte equality between IR-observer and ordinary compiler emission are retained.
- B v1 failed two standalone-Heap compatibility checks (5/7). The corrected version opts only Runtime-owned heaps into storage reuse and retains standalone Heap behavior; 7/7 pass. Dedicated pooled-allocation rollback and reuse controls remain required. Original failures and exact v1 sources are preserved.
- E v1 changed explicit `Runtime(1)` collection behavior and reproduced retained-cycle failure. The correction retains fixed explicit budgets and separately opts ordinary JIT/AOT applications into live-object pacing; 7/7 pass. Original failure and source are preserved.
- No throughput benefit is inferred from these correctness/count runs. They overlapped preparation builds. Matched uninstrumented comparisons are still pending.

The incidental complexity inventory above will retain exact source locations, observed work, required safety purpose, and measured impact where established. Large source files, repeated-looking checks and high-level abstraction alone are not proof of a bottleneck.

## Additional complexity findings from active experiments

- **Allocation pipeline, mechanism confirmed:** `src/runtime/coordination.cpp:274` validates output authority, prepares the reference map, checks the collection budget, polls, validates references/descriptors and publishes under the Runtime lock. `src/runtime/heap.cpp:19` then allocates payload and inserts a registry node. The pre/post-safepoint validations protect different states; neither B nor E removes them. B can avoid some payload requests but leaves registry nodes and reference-map preparation. Timing attribution remains pending.
- **Two collection-trigger owners:** `src/runtime/coordination.cpp:288` and `src/runtime/buffer.cpp:124` independently check the allocation limit. E's first corrected policy covered only the former; source review caught this before matched performance. The current E overlay applies the same threshold to both, with fixed explicit budgets unchanged. This is a maintenance hazard demonstrated by the prototype, not evidence that two checks on different allocation paths are redundant.
- **Empty-frame work, emitted-code proof:** `src/runtime/coordination.cpp:411` allocates a process-wide nonwrapping frame identity with atomic compare/exchange, then links/unlinks the frame even for zero descriptors. D omits only frames with no roots and no special lifecycle scopes. The actual dynamic benefit is pending measurement; the identity mechanism remains necessary for nonempty frames.
- **Prototype A compilation complexity:** the new local proof scans later instructions and local uses across the function per eligible construction, using ordered sets. Its conservative scope is intentional, but analysis cost can grow with constructor sites and function size. Compilation receipts must determine impact before a production-quality implementation; no speedup is inferred from removing heap calls alone.
- **Reuse/sanitizer visibility:** B retains freed payload inside the Heap, so ordinary allocator poisoning cannot detect every erroneous access to cached storage. Precise-root/ownership controls still apply. A production pool would need a deliberate sanitizer poisoning strategy before claiming the same stale-pointer diagnostic coverage as individual native frees. Current ASan results qualify exercised behavior, not every stale-pointer case.
- **Pacing units:** E is bounded in object counts, not payload bytes. Mixed/large-object and memory observations are essential; a count cap alone is not a universal resident-memory bound. This is a candidate limitation, not justification for a new collector.

Windows access was recovered using the existing interactive keyboard path after a display wake. Native build 19045, x64 VS2022/MSVC 14.44.35207, SDK 10.0.19041.0 and LLVM/LLD 23.1.4 were reverified. Baseline qualification is running in a new private NTFS task root. No credentials, existing jobs, VM settings or security controls were changed.

## First matched comparisons and bounded D refinement

Four alternating blocks, three warm samples/case, normal/O2 JIT and independently linked AOT now cover A–D individually. All oracle checks passed. Build groups were explicitly suspended during these measurements; process-state receipts show every member stopped. Native Windows baseline 13/13 passes in each profile completed before timing. Raw observations, including slow samples, are retained.

- A's complete G31 AOT executable is **byte-identical** to baseline: this corpus does not exercise the new eligible managed construction. Its small timing movements are controls, not an allocation-elimination speedup. Dedicated managed-source timings remain pending; zero-allocation activation is already proven.
- B's text median improved about 3–5.5%, while allocation-free AOT integer/float controls regressed about 7–8.5%. The allocator cannot explain this by doing extra allocations inside those loops. The complete entry object is identical, while Runtime layout and linked hot-path addresses differ. Exact manual relinking reproduced the original executable hashes.
- Diagnostic `-falign-functions=64` controls on the same three Runtime translation units changed baseline arithmetic from about 123.4 to 134.8 ms, and B from about 133.8 to 131.3 ms. The relative ranking flips by worsening baseline. **This is code-placement sensitivity evidence, not a repair or proof of a specific CPU cache/branch-predictor cause.** Alignment is not recommended for integration.
- C/D also show unfavorable AOT arithmetic movement; their archive/code/placement evidence must remain separate from root-clearing or frame-elision effects. Their remaining representative results are mixed/small. No improvement is silently accepted from these results.
- D2 preserves D1 and factors only `Runtime::poll`'s rare locked wait into a noinline immediately invoked local lambda. Validation, acquire load, locked recheck, park and failure ordering are unchanged; no new header, layout or public API is introduced. Actual machine code changes the common path from four saved registers plus stack space to one saved register and a short conditional tail jump. **Throughput and complete qualification pending.** D1 remains preserved as a separate experiment.

A further confirmed code-generation observation is the optional TLS-initializer guard in `vd_gc_poll`: `src/runtime/abi.cpp:108` accesses the raw TLS pointer declared at `include/void/runtime.hpp:221`, whose null initializer is in another translation unit (`src/runtime/coordination.cpp:17`). The current disassembly retains a weak initializer probe. Explicit C++ constant-initialization declarations may be worth a later narrow experiment, but no speedup or portable correction is claimed and this mission has not modified those declarations.

Additional numeric A proof passes normal/O2 forced collection and fatal sanitizers: mixed u8/i64/u64/f32/f64/bool fields preserve wraparound, signed zero, NaN classification and exact results while removing the eligible object allocations. Exact fixture and negative controls are retained.

## Qualification recovery and completion update

The existing Windows desktop was recovered without a reboot, credentials, new access mechanism or security changes. The apparent pause was not classified as a compiler failure: A subsequently completed Debug `async_source` in 40.34 seconds and the full 13-test selection in 74.12 seconds. Native A Release and Debug both pass all 13 maintained tests, positive/negative allocation proofs, normal/O2 JIT and independently linked AOT. D2 native qualification is in progress. Measurement runs wait for the Windows build/test job to finish.

D2 Linux Release passes the seven base and six expanded maintained tests, both cache-process tests and the existing race fixture. Its derived strict archive passes the same 13 maintained tests with fatal ASan/UBSan and leak detection; its source, headers, archive replacement and consumer identities are recorded. B/C/D/E additionally pass all three async sanitizer controls. E's explicit-host-budget/failure/continued-GC pacing control passes strict. A's mixed numeric and explicit-initializer fixtures pass Release normal/O2 JIT and independent AOT. These results do not establish throughput benefit or production acceptance.

## Scope limits retained for the engineering handoff

- A is a block-local scalar proof, not interprocedural escape analysis. Reference-containing graphs, escaped identities, callable/native objects, async frames and cleanup-bearing functions stay allocated. Constructor effect/alias/numeric controls are separate from the unmodified G31 corpus, which has no eligible A site. Its bounded stack-home reuse must not be generalized to escaping loop instances.
- B reuses payload storage under the existing Runtime mutex. It does not eliminate object-registry nodes, reference-map preparation, publication validation or locking; it is not a TLS allocator. Cached payload bytes and cross-thread observer limitations must be reported separately from native requested-memory peaks.
- C clears dead block-local instruction values, retaining the precise descriptor table and local/parameter lifetimes. It may lower retention/mark work; it does not reduce descriptor scan count or prove general local last-use liveness.
- D/D2 preserve every poll/checkpoint and the existing atomic request policy. D omits only unnecessary empty frames. D2 changes cold-path placement, not the rendezvous protocol. Generic specialization and LLVM inlining remain the existing implementations.
- E changes only automatic source pacing. Explicit native host budgets remain fixed. Its object-count cap does not prove a universal byte-memory limit, and fewer collections can increase retained payload and pause tails. No concurrent/generational collector is implemented or authorized here.

Existing benchmark observers count managed constructions, native allocation requests, root-word visits and GC intervals. Their native-memory peaks cover the observed thread/epoch, not whole-process live memory; cross-thread frees and reused pre-observation storage prevent interpreting these as global heap peaks. Uninstrumented measurements remain the throughput authority.

## Independent measurement decision

Four balanced blocks completed for A–E and D2. In the dedicated 20,000-iteration O2 probe, A lowers managed-local median 6.254→0.767 ms (87.7%) with 20,000→0 actual constructions; the mixed temporary-allocation case improves 49.064→41.479 ms (15.5%), leaving its required arrays allocated. D2 lowers the call-heavy probe 1.383→0.924 ms (33.2%) with every poll retained. These targeted benefits do not imply the same percentage for arbitrary applications.

The single combined experiment will contain **A + D2** (including D1 empty-frame omission). B/C/E remain independent evidence, not silently removed features. B improves some allocation/text workloads but retains repeatable unrelated AOT regression and pooling sanitizer limitations. C demonstrably reduces nonzero root visits but not descriptor scans; broad gains are not established. E reduces text collections 20→1 in the separate observation, with requested-memory peak 234,199→874,641 bytes and one 2,086.889 μs collection rather than 20 collections with 825.881 μs median/1,184.713 μs maximum. A single event is not a p99 estimate. E's useful retained-set throughput effect comes with memory/pause costs, so default-policy integration is not justified by this experiment.

All figures above are local measured observations. Count/GC observers are separate from uninstrumented throughput; do not sum observer timings or interpret per-thread requested-memory peaks as global RSS. The original and corrected failures, exact diffs and unmatched-layout negative controls remain retained.

Additional measured complexity: B's 64-block/class cap versus the source allocation budget of 256 permits only about one-quarter reuse for a steady scalar size class; observed 4,992 pool hits remove 4,992 of 40,468 native requests. Registry and publication work remain. A bounded capacity/memory tradeoff would be a smaller next investigation than TLS/page allocation, but was not added to this campaign. C's text observer removes 536 nonzero root visits while retaining all 7,226 descriptor-word visits; native requests fall by only 20. This explains why activation alone does not establish a broad liveness optimization.

Release-inlining reconciliation: baseline optimized `calls` IR already inlines the tiny `plus` helper, but preserves its empty-frame enter/leave effects inside the loop. D removes unnecessary frame effects; it does not introduce ordinary LLVM inlining. Keep this distinction in the Bible §40.2 backlog and do not propose a second inliner from this probe.

Combined A+D2 native Windows Release/Debug now each pass the 13 maintained tests plus positive/negative/fault allocation proofs, mixed numeric and explicit-initializer probes, normal/O2 JIT and independent AOT. Final binary hashes and PE headers verify a 1,048,576-byte stack reserve for `voidc`, Runtime, managed and async runners in both profiles. Linux combined Release 13 and maintained race controls pass; strict backend compilation and combined matched measurements remain in progress. No Windows performance percentage or macOS acceptance is inferred.

## Final disposition and evidence

All A–E prototypes and one combined A+D2 candidate were implemented and measured. A has direct 20,000→0 managed-allocation proof and strong local/mixed-source gains. Combined O2 managed-local 6.022→0.697 ms, calls 1.325→0.868 ms; AOT local 8.184→0.631 ms, calls 1.430→0.783 ms. Combined AOT array/4096 +4.4% and O2 async tasks +3.9% remain unfavorable results. Array-function IR is unchanged; async IR is byte-identical; combined and D2 full G31 AOT executables are byte-identical. Specific microarchitectural cause remains unknown. Recommend A first; D2 requires explicit tradeoff review or a bounded correction, not automatic combined integration.

Baseline/A/D2/combined native Windows each pass 13 tests per Debug and Release profile; A/combined also have allocation/fault/numeric/source/AOT proofs as detailed in the report. A–E Linux Release 13 and strict base+async 10 pass; D2/combined strict 13 pass. B/E TSan, targeted race, cache and fault controls pass. Original B/E failures, all negative timings and setup errors are retained. No macOS acceptance or hosted CI claim.

Archive: `/home/shmellyorc/Documents/Void.Language-archives/g33-performance-foundations-experiments-2026-11-13.zip`. Exact patches, baseline source inputs, identities, IR/disassembly, all benchmark scripts/inputs/oracles, raw outputs, strict/Windows receipts and this ledger are retained. The external archive index/cleanup receipt records verification and task-only removal. No production or Git changes beyond the two new uncommitted G33 research documents.

Verified archive/cleanup complete: ZIP SHA-256 `e550da875ee3efeca15962ca603eee5b8f6a0ba58966280d0b99dbfed2c41a33`, 21,520 selected files, CRC/extracted/source hashes passed. Inventoried task roots removed; observed free-space recovery 16.27 GiB Linux and 2.03 GiB guest. SDK/provider/helper targets and prior archives preserved. The final report contains the post-archive cleanup correction/receipts. Production/staged diffs remain empty; stop for Wrench/Boss review.
