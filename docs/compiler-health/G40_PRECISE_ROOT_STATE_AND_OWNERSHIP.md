# G40 — precise root state and managed ownership

Accepted base: `793de66cb8cb6fcbb60e85b2ee0fb2a285a13c92` (G39/G39-F), verified remote main at task start. Isolated worktree `/home/shmellyorc/Documents/Void.Language-G40`, branch `codex/g40-root-state`. Final backend SHA-256 `839e729bdb6476d40e3d6b68fa16fb4a8b35af4c4a0e22d5c4efe58713803856`. LLVM/Clang/LLD 23.1.4. **Decision: INTEGRATE as a qualified candidate for Golden Wrench review, retaining the explicit tradeoffs below.** Boss controls Git integration; no stage, commit, push, history rewrite or hosted CI occurred.

## Implemented capability and architecture

Priority A removes redundant copy-home clears using the existing indexed SSA/local uses and retirement plan. A managed SSA constant null, or a load from a definitely-null local, has already written null to its own existing home before retirement. Local facts use predecessor intersection, including loop backedges, with unknown incoming parameters and the lowerer's actual zero-initialized nonparameter homes. Transfer follows only source constant/load/store facts; it never infers null from managed type or arbitrary factories. A greatest fixed point is bounded to 256 blocks, 256 tracked locals and 16,384 block/local pairs. Above that budget only facts written within the current block are used. No branch-condition refinement or general physical-home overwrite analysis is claimed.

The proof applies only under G39's existing synchronous known-root-transfer guard. Receiver/by-reference, unknown/public/native/virtual/callback/worker/async/resource/cleanup cases keep the accepted conservative behavior. Returns keep their existing handoff homes. Failure branches leave the Frame rather than rejoining a successful source path; cleanup/initializer/receiver functions remain excluded. **Payload-field clears are never removed by copy-null facts.** A child may need its own root after its parent allocation disappears.

Priority B admits same-nominal checked nullable conversions and nullable local aliases through the existing use index. New eligibility is restricted to a reconverging four-block diamond: birth block, two direct arms and a nonempty common join; every owner use lies in those blocks. G35's single-origin/must-alias proof still resets at each construction epoch and rejects pre-birth loads, rebinding and old loop instances. New worker/closure/virtual cases remain excluded. Nullable conversions retain checked semantics; no dynamic cast/upcast/native-pointer flow is added. SSA stays block-local; indexed locals carry source temporaries over CFG edges.

The payload retires at the last owner use in the join, or after its first instruction if the final uses were in the arms. Join-entry polling therefore retains conservative visibility. The extracted child/result is rooted before payload retirement. Existing eligible owning-local diamonds also gain this earlier boundary. No payload can survive its backing storage or publish a stack-owner address into the managed Heap. Every path to a new birth passes the bounded join; aliases cannot observe an earlier epoch after reuse.

Priority C extends the existing completed-construction flag through indexed local store/load dependencies. Seeds are completed `gc_construct` origins only. A least fixed point visits each dependency once; all stores must be completed origins. Incoming values, arbitrary factories, split allocation/initialization, uncertain merges and self-supporting cycles cannot bless themselves. Multiple different validated owners may establish current-Heap readonly-child provenance even when their own multiple-origin identities prevent allocation elimination. Mutable fields remain conservative. Freshness, ownership and nonescape remain separate.

G33–G35 allocation/CFG foundations, G34 reuse/root authority/polls/pacing, G36 collections, G38 bounded runtime-free AOT SIMD and G39/F capabilities remain. Runtime/public headers and ABI are unchanged. Core/Runtime Release archives match the frozen baseline byte for byte. The accepted G38 finish pipeline and AOT-entry-point suffix are byte-identical. G37/G37-F remains separate and performance-held. No collector, allocator, cache, root manager, public ABI or whole-program framework was introduced.

## Incremental proof, rejected emission and exact controls

A was implemented first and passes three focused suites covering 50 fixtures in normal/O2 JIT and independent normal/optimized AOT. Its 200 observer objects equal ordinary emission. B then passes all 53 fixtures in the same modes; C expands to 57. Stage-a/ and stage-b/ preserve exact source and qualification.

The first combined emission comparison exposed a reproducibility defect in inherited batched clearing: iterating a map keyed by LLVM allocation addresses selected different clear order/stack layouts for different output-path lengths. Exact pre-correction source, ordinary/observer objects, native differences and same-path reproductions are retained. Final emission visits bases in first-seen retirement-plan order and sorts words by offset only. This changes neither root geometry nor boundaries. Different short/long output paths now agree, and **all 228 final observation objects equal ordinary emission**. One separate emitter subprocess failed with empty stderr before a successful exact-command retry; its original exit status was not captured. The log is retained without inventing a cause. Twenty repeated normal/optimized context-control emissions and the final complete comparison pass.

Fixture corrections are retained: nullable fields require explicit checked unwrapping; a leading parenthesized expression is not a writable statement place; the mixed-origin oracle was computed independently as 27,005 with 3,000 source constructions at n=1,000. An early test launch omitted the two new foreign executables; explicit targets corrected that setup failure without weakening assertions.

Maintained new controls are `root_null_straight`, `root_null_join`, `root_null_mixed`, `root_null_backedge`, `root_null_alias`, `cross_nullable`, `cross_child_survives`, `cross_mixed_origin`, `readonly_alias`, `readonly_alias_join`, `readonly_alias_incoming`, `readonly_alias_mutable`. All 45 inherited fixtures and their assertions remain. Mixed join/backedge/read-before-store, alias rebinding, identity, escaping/captured/public/native/async, nullable, return, error/cleanup, unknown factory and mutable/current-Heap negatives remain.

## Allocation and emitted-call evidence

Independent source/domain oracles use n=20,000; baseline means accepted G39-F and candidate means G40. Counts agree across normal/O2 JIT and independently linked normal/optimized AOT, all three observer samples. Nominal source expressions include conditional execution; source counts are not inferred from either compiler's result.

| Pattern | Nominal source expressions | F39 heap | G40 heap | Static constructor calls | Native requests F39→G40 |
|---|---|---|---|---|---|
| root_null_straight | 40000 | 20000 | 20000 | 1 → 1 | 0 → 0 |
| root_null_join | 40000 | 20000 | 20000 | 1 → 1 | 0 → 0 |
| root_null_mixed | 50000 | 30000 | 30000 | 2 → 2 | 156 → 156 |
| root_null_backedge | 20002 | 20001 | 20001 | 2 → 2 | 156 → 156 |
| root_null_alias | 60000 | 40000 | 40000 | 2 → 2 | 0 → 0 |
| cross_nullable | 40000 | 40000 | 20000 | 2 → 1 | 40312 → 0 |
| cross_child_survives | 40000 | 40000 | 20000 | 2 → 1 | 41248 → 624 |
| cross_mixed_origin | 60000 | 60000 | 60000 | 4 → 4 | 60468 → 60468 |
| readonly_alias | 60000 | 40000 | 20000 | 2 → 1 | 40000 → 0 |
| readonly_alias_join | 80000 | 80000 | 60000 | 5 → 4 | 100624 → 60468 |
| readonly_alias_incoming | 60000 | 60000 | 60000 | 3 → 3 | 82184 → 82184 |
| readonly_alias_mutable | 60000 | 40000 | 40000 | 2 → 2 | 40000 → 40000 |

The joined nullable owners and readonly aliases remove one additional construction per iteration. Mixed-origin owners remain allocated; readonly provenance can remove their fresh consumer without removing the owners themselves. Foreign-Heap aliases retain exactly `prepared reference belongs to another heap` and the original validation phase; normal/optimized native controls require termination status 86. Cold injected allocation faults/recovery and remaining null/arithmetic failures pass. Only the eliminated allocation's own OOM possibility may disappear.

## Root work and required child visibility

Counts below are whole-module optimized-IR scalar pointer-null stores/zero memsets, including unchanged initialization and source-null writes. A zero memset may lower to several native stores. Stage-A counts independently isolate null pruning. Removing redundant high-level clears can fragment a zero run or expose earlier stores to LLVM; it is not a promise of fewer final instructions in every case.

| Pattern | F39 scalar/fills | G40 scalar/fills | Whole-module zero bytes |
|---|---|---|---|
| root_retirement | 3 / 5 | 5 / 5 | 168 → 168 |
| root_null_straight | 2 / 6 | 5 / 5 | 176 → 176 |
| root_null_join | 7 / 5 | 6 / 4 | 192 → 168 |
| root_null_mixed | 6 / 6 | 9 / 5 | 200 → 208 |
| root_null_backedge | 6 / 6 | 8 / 5 | 200 → 200 |
| root_null_alias | 4 / 8 | 7 / 5 | 248 → 216 |
| cross_nullable | 2 / 8 | 4 / 6 | 288 → 184 |
| cross_child_survives | 2 / 2 | 3 / 2 | 88 → 88 |
| readonly_alias | 1 / 7 | 3 / 6 | 208 → 208 |
| readonly_alias_join | 1 / 11 | 2 / 10 | 296 → 296 |
| loop_carried | 2 / 3 | 2 / 3 | 80 → 80 |

Occupied roots, fixed root/Slot scan work, marking and collection frequency are separate quantities. Optimized-AOT medians below retain all raw samples/modes. Allocated payload after call includes dead objects awaiting collection and is not a live-set measurement.

| Pattern | F39 words/Slots/nonzero/mark/GC | G40 same counters | Post-call payload B |
|---|---|---|---|
| root_retirement | 780/234/0/0/78 | 780/234/0/0/78 | 384 → 384 |
| root_null_join | 702/156/0/0/78 | 702/156/0/0/78 | 384 → 384 |
| root_null_mixed | 1053/234/39/39/117 | 1053/234/39/39/117 | 576 → 576 |
| root_null_backedge | 702/156/78/78/78 | 702/156/78/78/78 | 400 → 400 |
| cross_nullable | 2496/312/156/312/156 | 624/156/0/0/78 | 1164 → 384 |
| cross_child_survives | 1404/312/1248/1404/156 | 624/156/468/468/78 | 1164 → 388 |
| readonly_alias | 2028/312/0/0/156 | 1014/156/0/0/78 | 1152 → 384 |
| readonly_alias_join | 4680/624/312/624/312 | 3510/468/234/468/234 | 1676 → 780 |
| loop_carried | 390/156/78/78/78 | 390/156/78/78/78 | 400 → 400 |

All 32 null-state/alias/loop/retirement mode-pattern comparisons agree exactly with G39-F on all eight construction, native-allocation, root-search, occupancy, marking, collection and payload counters, across all three observer samples. This is evidence that useful accepted retention boundaries remain intact; clearing an already-null home need not change occupancy.

The inherited 24,000 raw-address forced-collection checks are repeated. Two additional archived generated witnesses add 96,000 checks: cross-block child survival is checked through all 7,000 post-birth polls per 1,000 iterations, including after parent retirement; readonly-alias ownership is checked at 1,000 required helper polls. Raw TLS addresses add no root. Release baseline/candidate and fatal-sanitizer candidate cover both JIT modes and independent AOT under 1 MiB. **All 120,000 checks pass.** New witnesses show precisely 12→4 live payload bytes: the child stays visible after parent elimination, while no Heap owner remains. Maintained 57-fixture tests additionally force collection at construction/poll boundaries and validate every precise home/commit.

## Final Linux and native qualification

Final Linux Release **84/84**, fatal ASan/UBSan/LeakSanitizer **84/84** with halt/abort/leak detection and no suppressions. Relevant fatal TSan **57 patterns×two independent AOT modes**, coordination plus race/allocator controls, 1 MiB runs, pass. Normal/O2 JIT and independent AOT, exact construction/result/fault/ownership/Frame assertions, callback/reload/worker/async/cancellation, collection/dictionary/text/JSON/native-I/O, cache and G38 numeric/SIMD controls pass. Stack runners separately enforce 1,048,576 bytes.

Native Windows 10 build 19045, x64 VS2022/MSVC 14.44.35207, SDK 10.0.19041.0, LLVM 23.1.4: **Release 69/69, Debug 69/69**. Independent normal/optimized AOT and normal/O2 JIT, two new exact-diagnostic foreign controls and preserved failure/cleanup/reload/worker/async controls pass. Seven PE executables per profile retain 1 MiB stack reserve. Every exported compiler/test input and installed protected provider/tool identity matches. Native functional qualification is not a Windows throughput or macOS acceptance claim.

## Matched observer-off performance

All measurement-binary preparation and qualification completes before throughput; scaling compilation runs afterward. No task compiler runs concurrently with throughput sampling. Four balanced blocks with three samples cover all 57 patterns in normal/O2 JIT and independent normal/optimized AOT. Six balanced standalone blocks use five samples and natural linking. Separate observer binaries count allocations/root work and full GC intervals. The host is shared; unrelated user processes are not stopped. Retain adverse samples and unchanged-image controls; no padding, forced alignment, special linker ordering or benchmark-specific compiler predicate is used.

| Standalone G40/F39 | Normal AOT | Optimized AOT |
|---|---|---|
| root_retirement | -0.1% | +0.8% |
| root_null_straight | +1.8% | +0.6% |
| root_null_join | -1.6% | +5.8% |
| root_null_mixed | +1.0% | -0.4% |
| root_null_backedge | +0.6% | +1.1% |
| root_null_alias | -1.0% | -3.2% |
| cross_nullable | -49.6% | -52.9% |
| cross_child_survives | -53.4% | -53.6% |
| cross_mixed_origin | +2.0% | -1.9% |
| readonly_alias | -57.0% | -59.4% |
| readonly_alias_join | -33.1% | -30.4% |
| readonly_alias_incoming | +0.6% | -2.8% |
| readonly_alias_mutable | +0.9% | -3.2% |
| loop_carried | +2.8% | +2.1% |
| alias_rebind | +0.5% | -0.1% |
| public_helper | -1.3% | -1.8% |

Full-bundle timing includes all 57 patterns. Newly eligible patterns and inherited G35/G39 controls follow; negative percentages mean less elapsed time.

| Full-bundle G40/F39 | JIT normal | JIT O2 | AOT normal | AOT optimized |
|---|---|---|---|---|
| direct_reference | +0.2% | -2.5% | +1.0% | +0.7% |
| helper_branch_write | -4.7% | -6.5% | +1.3% | +5.3% |
| root_retirement | +0.9% | -2.1% | +0.2% | +4.2% |
| root_null_straight | -0.8% | +2.6% | +3.2% | +1.0% |
| root_null_join | -0.9% | -7.0% | +0.7% | +2.6% |
| root_null_mixed | -2.8% | -1.5% | +3.2% | +2.5% |
| root_null_backedge | -0.9% | +0.4% | -1.2% | +3.1% |
| root_null_alias | -2.3% | -1.6% | +4.7% | +1.1% |
| cross_nullable | -53.2% | -54.9% | -50.2% | -52.1% |
| cross_child_survives | -53.9% | -54.6% | -51.9% | -53.9% |
| cross_mixed_origin | -0.7% | -2.7% | +5.6% | +0.7% |
| readonly_alias | -57.7% | -58.7% | -54.4% | -55.0% |
| readonly_alias_join | -31.5% | -30.7% | -29.1% | -30.8% |
| readonly_alias_incoming | -2.2% | +0.6% | +1.9% | +5.7% |
| readonly_alias_mutable | -3.0% | -2.6% | +9.7% | +4.5% |
| loop_carried | -1.4% | -4.0% | +3.8% | +5.5% |

Largest adverse full-bundle rows per mode are explicit below; full positive and negative data remain archived. Changes to code placement alone are not source-work improvements.

| Mode | Largest adverse G40/F39 rows |
|---|---|
| jit-normal | direct_null_failure +2.4%; payload_budget +1.9%; outlined_array +1.7% |
| jit-O2 | payload_budget +9.6%; branch_alias +3.5%; direct_public +3.2% |
| aot-normal | readonly_alias_mutable +9.7%; direct_nested +8.6%; direct_null_failure +6.5% |
| aot-optimized | mutable_field +14.3%; null_failure +10.4%; branch_alias +8.2% |

The inherited normal-loop and worker costs are rechecked without attempting another placement audit. The loop standalone additionally links the exact archived G38 object with the same Runtime/driver; worker/calls/async JIT controls independently rebuild exact qualified G39 backend c832db0f… against the same unchanged Core/Runtime and driver.

| Inherited control | G40/F39 | G40/G38 loop or G39 managed |
|---|---|---|
| loop_carried normal | +2.8% | +8.6% |
| loop_carried optimized | +2.1% | -2.2% |
| calls jit-normal | -7.7% | -1.9% |
| workers jit-normal | -3.2% | -3.9% |
| async-tasks jit-normal | -0.0% | -4.9% |
| calls jit-O2 | -7.2% | +0.8% |
| workers jit-O2 | -5.3% | +2.6% |
| async-tasks jit-O2 | -3.3% | -3.2% |

Broader representative elapsed-time deltas use normal/O2 whole-program JIT and independently linked optimized AOT. Normal AOT broader runtime behavior is covered by the focused/native and separate GC suites; this table does not imply a second normal-AOT representative throughput image.

| Representative G40/F39 | JIT normal | JIT O2 | AOT optimized |
|---|---|---|---|
| array-1024 | -5.7% | -4.8% | -1.1% |
| array-4096 | -5.4% | -5.4% | -0.4% |
| dependent-1000000 | -10.7% | -7.4% | +0.8% |
| dict-1024 | -0.3% | +3.0% | -0.8% |
| float-1000000 | -9.8% | -6.6% | +0.7% |
| integer-1000000 | -9.3% | -9.9% | -0.8% |
| integer-4000000 | -10.2% | -8.1% | +1.5% |
| io-200 | -1.3% | +0.9% | -0.3% |
| json-128 | -3.7% | +0.2% | +2.6% |
| json-32 | -1.7% | -3.1% | +0.5% |
| sort-1024 | +0.8% | +2.1% | +1.3% |
| sort-256 | +0.8% | +1.0% | +1.2% |
| text-1024 | -4.3% | -0.1% | +0.2% |
| text-256 | -1.4% | -1.1% | +1.5% |

The full representative optimized AOT executable is byte-identical (`2d7f17b72d139e0b02bfbec560692f699d2e8867bfe0e2cde28fccda55aa798e`); no arithmetic/collection/text/JSON/I/O source-speedup claim follows from its timing variation. Sixteen of 18 managed AOT objects are byte-identical; exact per-case identities remain archived.

Managed/closure/worker/async whole-source controls follow. Their unchanged-object status is recorded separately; elapsed-time variation alone does not establish reduced compiler work.

| Managed G40/F39 | JIT normal | JIT O2 | AOT normal | AOT optimized |
|---|---|---|---|---|
| async-tasks | +1.2% | +4.5% | -5.7% | -2.0% |
| calls | -6.6% | -8.8% | +5.0% | -0.0% |
| closures | -4.4% | +4.1% | -3.2% | -1.0% |
| managed-across-edge | -2.3% | -5.0% | +1.9% | -2.1% |
| managed-escape | -1.8% | -2.5% | -0.4% | -0.9% |
| managed-local | -9.5% | -7.3% | +3.2% | +1.9% |
| managed-reference | -9.3% | -5.9% | +5.2% | +0.2% |
| managed-survives-allocation | -3.0% | +0.5% | +0.0% | +0.0% |
| workers | -8.3% | -6.3% | +0.6% | -1.0% |

The earlier G39-F +4.8% normal-loop versus G38 and +14.2% worker O2 JIT versus G39 remain historical adverse results; this campaign does not rewrite them. Representative arithmetic/arrays/sort/dictionary/text/JSON/I/O and managed calls/closures/workers/async run normal/O2 JIT and independent AOT with independent outputs. Exact object/image identities and all timing rows are retained. Byte-identical cases are controls, never claimed optimizations. Historical G34–G39 compilation, JIT/native context, startup and GC-tail tradeoffs remain active.

## Compilation, startup, memory and full GC intervals

100/500/2,000 are total source owner+child construction expressions. Three balanced compilation blocks record frontend analysis, lower/JIT, process CPU/wall and wait4 peak RSS. All analysis and memory regressions remain visible.

| Sites | Lower/JIT ms F39→G40 | Change | Peak RSS KiB |
|---|---|---|---|
| 100 | 103.11 → 102.95 | -0.2% | 50332 → 50624 |
| 500 | 547.40 → 588.07 | +7.4% | 68312 → 68696 |
| 2000 | 5262.21 → 5147.81 | -2.2% | 140276 → 140276 |

| Sites | Frontend analysis ms | Process CPU s | Process wall s |
|---|---|---|---|
| 100 | 3.82 → 3.87 | 0.113 → 0.112 | 0.115 → 0.115 |
| 500 | 17.21 → 17.35 | 0.569 → 0.608 | 0.575 → 0.617 |
| 2000 | 73.14 → 76.09 | 5.273 → 5.192 | 5.357 → 5.247 |

Total lower/JIT cost is superlinear over 100/500/2,000 sites in both baseline and candidate; these measurements do not establish linear end-to-end compilation. The 500-site candidate costs +7.4% in lower/JIT, while 2,000 sites measure −2.2% with equal peak RSS. Three blocks on a shared host are descriptive, not an asymptotic or tail guarantee.

Warm-filesystem n=0 startup uses 20 balanced process blocks; these are not cold-disk guarantees.

| Startup | Wall ms F39→G40 | Peak RSS KiB |
|---|---|---|
| aot-normal | 2.673 → 2.558 | 14992 → 14992 |
| aot-optimized | 2.541 → 2.474 | 14992 → 14992 |
| jit-O2 | 23.308 → 23.305 | 47048 → 46892 |
| jit-normal | 21.194 → 21.195 | 44822 → 44788 |
| representative-aot | 3.443 → 3.427 | 14992 → 15056 |

Every full request-to-release latency and stopped-world interval is retained in separate observer campaigns, including representative collection/text/JSON/I/O controls. Descriptive optimized-AOT stopped µs median/p99/max follow; finite-sample p99 is not a population tail guarantee. Native requested-memory counters are per observed thread/epoch, not whole-process RSS or global threaded allocation peaks.

| Pattern | F39 stopped µs med/p99/max | G40 same |
|---|---|---|
| root_retirement | 7.29 / 17.17 / 17.50 | 7.01 / 12.18 / 14.80 |
| root_null_join | 7.32 / 14.70 / 18.17 | 7.07 / 13.87 / 14.51 |
| cross_nullable | 8.09 / 14.92 / 17.03 | 7.09 / 13.64 / 16.94 |
| cross_child_survives | 7.67 / 23.87 / 65.57 | 7.28 / 14.61 / 27.46 |
| readonly_alias | 7.50 / 14.64 / 15.47 | 6.94 / 8.74 / 10.07 |
| readonly_alias_join | 8.07 / 15.91 / 23.26 | 7.49 / 17.09 / 23.36 |
| loop_carried | 7.01 / 10.87 / 11.42 | 6.86 / 10.93 / 28.08 |

## Complexity and limitations

The backend diff adds 133 lines and removes 16 lines (net 117); it replaces the direct-only completion test and address-ordered emission rather than keeping parallel implementations. The null proof reuses existing use/predecessor indices, adds a capped must-null local lattice and per-block SSA facts, then prunes only the existing copy retirement vectors. Above its state budget it falls back to within-block writes. The ownership extension adds transparent same-class alias edges and a four-block shape check over indexed candidate uses; the existing G35 epoch/CFG proof remains authoritative. Readonly completion replaces the direct-only local-store check with linear indexed store dependencies and a least fixed point, avoiding per-candidate function rescans. Deterministic clear grouping adds one transient first-seen base list; no Runtime metadata grows. These are extensions to existing machinery, not parallel escape/root frameworks.

General overwrite-before-observation proofs, branch-condition refinement, arbitrary postdominators, larger/irreducible temporary CFGs, nullable ownership merges with unknown origins, private factory completion/retention, mutable fields, incoming/foreign ownership, escaping identities, callbacks/native/async captures and cleanup scopes remain conservative. Known-null pruning does not guarantee fewer optimized stores: exposing earlier source-null stores and splitting runs can worsen final instruction counts; emitted/timing adverse rows above are retained. Larger root-state lattices are not inferred from this capped implementation.

Outstanding Language Bible objectives remain: broader escape and direct-reference lifetimes, stronger current-Heap provenance, interprocedural retention, checked collections, dictionary/sorting, text/Unicode/JSON materialization, SIMD beyond G38, native/JIT tradeoffs, compilation/RSS, GC-tail/startup and future macOS/release qualification. G37-F remains a separate held experiment. Completed milestones and rejected proofs remain historical records, not overwritten current-source status.

## Archive, exact inventory and next action

Evidence archive: `/home/shmellyorc/Documents/Void.Language-archives/g40-precise-root-state-ownership-2026-11-15.zip`. DEFLATE 9, CRC, every SHA-256 manifest entry and exact source reconstruction from accepted 793de66c preimages plus patch are verified before task-only cleanup. Exact retained-file count, compressed bytes, archive SHA, final source/path hashes and separate observed host/guest reclamation are in sibling verification/closeout/cleanup/git-inventory receipts; an archive cannot contain its own digest. Installed SDK/providers, original G37-F/G38/G39 worktrees and previous archives remain protected. No VDI compaction is inferred from guest free space.

Final source inventory: 17 files — CMakeLists, backend, escape-case catalogue, cumulative ledger, this report and 12 maintained source fixtures. The exact path/hash inventory is archived and linked by closeout. HEAD remains 793de66cb8cb6fcbb60e85b2ee0fb2a285a13c92 on codex/g40-root-state; nothing is staged or committed. Original G39/G39-F reports and all prior fixtures remain intact. Golden Wrench reviews the candidate; Boss owns integration/push/hosted CI.

Next concrete implementation: use the existing retirement/home information to prove redundant *defining* null stores or avoid zero-run fragmentation, beginning with the emitted retirement-only case; preserve mandatory payload boundaries, loop-useful clears and the qualified nullable/readonly capabilities. This is proposed future work, not another change hidden in G40. General factory/complex-CFG expansion remains a separate proof obligation.
