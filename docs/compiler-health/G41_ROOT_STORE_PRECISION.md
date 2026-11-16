# G41 — root-store precision and managed lifetimes

Accepted baseline: `9f00377236a1080ec4ebdc6252f3721843dd2f94` (G40), verified remote main. Isolated worktree `/home/shmellyorc/Documents/Void.Language-G41`, branch `codex/g41-root-store-precision`. LLVM/Clang/LLD 23.1.4. Qualified backend SHA-256: `ac7da6a3d17642cbc533526cc06e3df5b10cbd1a7850fbf8632084ea57aed377`.

**Decision: INTEGRATE the qualified A+C candidate for Golden Wrench review, with the costs below. Priority B was implemented and measured, then excluded from production.** No staging, commit, push, history rewrite or hosted CI. Boss controls integration. This report does not claim every workload improves.

## Production changes and proof boundaries

**A: defining-null stores and existing retirement clears share one physical-home must-proof.** G40's source-local null analysis is replaced, rather than run in parallel. The existing indexed predecessor graph survives until LLVM homes are assigned. Existing locals and SSA copy homes are identified by exact Alloca base and constant DataLayout offset, including SSA homes reused across blocks. Every pointer-shaped SSA definition invalidates that physical word unless its managed value is proved null. Incoming managed parameters remain unknown; no type establishes nullness or current-Heap membership.

The initial facts come from the lowerer's actual zero initialization: instruction homes and ordinary nonparameter managed locals. Constants and loads of definitely-null locals propagate null. Source stores update facts, but their emitted stores, evaluation, readonly checks and ordering remain untouched. A defining GC-copy store is omitted only when both its SSA value and the actual destination word are already null. Retirement vectors lose only clears of words already proved zero. Previously completed retirement can establish zero for a later shared-home definition. Mandatory payload-field writes/retirement are never pruned by copy facts.

Predecessor intersection includes backedges. The greatest fixed point is capped at 256 blocks, 256 physical words and 16,384 block/word pairs. Above budget only writes in the current block establish facts. This is an already-zero proof; general overwrite-before-observation, branch-condition refinement and payload-value propagation are not implemented. It uses G39's existing synchronous known-root-transfer guard. Receiver/initializer/cleanup, worker/closure/virtual, unknown/public/native/callback/async/lifecycle interactions retain conservative behavior. Calls allowed by that guard cannot write arbitrary caller copy homes; known allocation outputs are invalidated by their defining instruction. The accepted nonmoving collector reads homes across checkpoints without rewriting them.

**C: one nested diamond and an exact common retirement boundary.** G40's four-block shape admits one additional nested diamond in one arm. Inner arms must jump to a distinct common tail, which jumps to the outer join; the other outer arm jumps directly there. At most one empty outer-join forwarder may lead to a distinct nonempty retirement block. All owner uses must lie in the resulting at-most-eight-block region, with no pre-birth use. There is no general postdominator or heap-graph analysis.

The existing G35 single-origin, indexed alias and CFG must-analysis remains authoritative. It resets at birth on every construction epoch, rejects rebinding/side-entry paths that cannot establish the current alias, and rejects every escaping/retaining owner use. Nullable conversions preserve the same nominal type and readonly authority. Internal shape vertices are distinct; the shape adds no internal cycle. Payload retirement stays after the last owner use in the chosen block, or after its first instruction if final uses were in the arms. Both entry polls/checkpoints of an empty forwarder and its destination remain. Extracted children have independent existing homes before owner fields clear. No stack-owner address becomes a managed Heap reference, and an earlier loop instance cannot survive backing-storage reuse through an accepted owner alias.

G33–G35 escape/ownership, G34 reuse/Slots/polls/pacing, G36 collections, G38 bounded runtime-free AOT SIMD, G39/F lifetimes/profitability/batching and G40 provenance/deterministic emission remain. Runtime, public headers, ABI, allocator, collector, root authority and collection policy are unchanged. The frozen baseline and candidate Core/Runtime archives match byte for byte. G38's finish pipeline and AOT entry-point suffix are byte-identical. G37/G37-F remains separate and performance-held.

## Incremental implementation, rejected B and corrections

A was implemented first: 57 maintained patterns, three focused suites, normal/O2 JIT and independent normal/optimized AOT; all 228 observer objects matched ordinary emission. B followed with 59 patterns and all 236 emission comparisons, then matched timings and an A/B isolation rebuild. The isolation emitter reproduced all 114 saved A objects before measuring new B witnesses. Stage-a/, stage-b/ and b-isolation/ preserve exact code, IR, native objects and every timing.

The first safe B draft could re-zero long known-null gaps between dirty endpoints, expanding 16 required bytes toward 2 KiB under the word cap. It was rejected before throughput qualification. A corrected B bridged at most one exact already-zero copy word per Alloca base/event, never payload words or unknown live gaps, and moved no store across an instruction, block or safepoint. It preserved all eight observed counters in every A/B mode/pattern sample. Nevertheless, extra writes and machinery were not justified by robust throughput gains. It is excluded from the final backend, including its gap tables and address vectors. No custom vector emitter, second optimizer, alignment/padding/link-order workaround or benchmark predicate was added.

| Rejected B versus A | IR scalar nulls | IR fills | Immediate-zero stack opcodes | Entry bytes |
|---|---|---|---|---|
| nested | 2 → 1 | 7 → 7 | 7 → 5 | 1025 → 1022 |
| direct_nullable | 3 → 1 | 3 → 4 | 2 → 2 | 471 → 471 |
| root_null_mixed | 6 → 4 | 5 → 6 | 8 → 7 | 927 → 927 |
| root_null_backedge | 6 → 4 | 5 → 6 | 11 → 10 | 829 → 829 |
| root_zero_bridge | 11 → 9 | 5 → 5 | 15 → 13 | 1097 → 1087 |
| root_zero_live_gap | 4 → 4 | 7 → 7 | 8 → 8 | 1226 → 1226 |

B-specific six balanced blocks × five samples compare natural G40/A/B standalone images. Negative means less elapsed time. Identical A/B images are controls, so their variation is not a B speedup.

| B/A control | Mode | B/A elapsed | B/G40 elapsed |
|---|---|---|---|
| nested | normal | -0.9% | -1.4% |
| nested | optimized | -1.0% | -0.9% |
| direct_nullable | normal | +6.5% | +20.4% |
| direct_nullable | optimized | -0.2% | +3.6% |
| root_null_mixed | normal | +1.3% | +0.6% |
| root_null_mixed | optimized | +2.0% | -1.0% |
| root_null_backedge | normal | +3.1% | +0.4% |
| root_null_backedge | optimized | +0.6% | +2.7% |
| root_zero_bridge | normal | +0.4% | +5.1% |
| root_zero_bridge | optimized | +4.2% | +0.6% |
| root_retirement | normal | -2.7% | -2.6% |
| root_retirement | optimized | -0.4% | +3.3% |
| root_zero_live_gap | normal | +0.7% | -2.0% |
| root_zero_live_gap | optimized | -2.9% | +1.5% |

The first C witness remained heap allocated because its outer join was empty; exact construction assertions stopped qualification (2,000 versus expected 1,000). The bounded single-forwarder rule corrected activation without weakening ownership checks. A new incoming-owner fixture initially attempted mutable publication from a readonly child; it was corrected to a const child field, preserving authority. B's first AOT setup omitted new names from CMake's explicit fixture list: JIT passed, normal AOT failed to link, and the old optimized binary was not accepted as qualification. The explicit list and stop-on-failure runner fixed setup.

One accepted-G40 emitter process initially failed with empty stderr and an unrecorded exit status. Its exact-command retry and 20 repeats passed; subsequent harnesses record subprocess exit status. No cause is invented. The complete final 256 ordinary/observer object comparisons pass. Separate literal-defining-store instrumentation reproduces all 128 baseline objects. Different output-path lengths are checked separately.

## Activated cases, exact construction counts and native output

At n=20,000, source construction evaluations are independent domain counts. Root bridge/live-gap each has one static constructor site in a factory called four times per iteration. Nested witnesses have two sites; incoming-owner has three. Actual constructions agree in normal/O2 JIT and both independently linked AOT modes, all three observer samples.

| New maintained pattern | Static sites | Source evaluations | G40 heap | G41 heap | Native requests | Static constructor ABI calls |
|---|---|---|---|---|---|---|
| root_zero_bridge | 1 | 80000 | 80000 | 80000 | 0 → 0 | 1 → 1 |
| root_zero_live_gap | 1 | 80000 | 80000 | 80000 | 0 → 0 | 1 → 1 |
| cross_nested_diamond | 2 | 40000 | 40000 | 20000 | 40312 → 0 | 2 → 1 |
| cross_nested_child | 2 | 40000 | 40000 | 20000 | 41248 → 624 | 2 → 1 |
| cross_nested_escape | 2 | 40000 | 40000 | 40000 | 41248 → 41248 | 2 → 2 |
| cross_deep_diamond | 2 | 40000 | 40000 | 40000 | 40312 → 40312 | 2 → 2 |
| cross_nested_incoming | 3 | 60000 | 60000 | 60000 | 82184 → 82184 | 3 → 3 |

The two nested positives eliminate only the holder. Public escape, deeper-than-supported CFG and incoming-owner children retain heap construction. Foreign incoming children still fail with exactly `prepared reference belongs to another heap`, status 86, in both independent AOT modes. Construction/initializer effects, child OOM, checked errors, fault recovery, identity, aliases, readonly and cleanup remain. Only an eliminated allocation's own OOM possibility may disappear.

The following table separates internal defining-home writes from all-module optimized IR. Literal defining counts exclude source assignments and payload stores; candidate proven-null counts also include definitely-null loads. All-module null stores/fills include unchanged initialization and payload work. Native literal-zero memory stores count decoded entry instructions, including unchanged scalar/payload initialization. The conservative vector count and complete selected disassembly are archived; these columns are static instruction counts, not dynamic execution counts or throughput claims.

| Optimized pattern | Literal defining stores | Proven-null definitions omitted/total | IR scalar/fills | Native literal zero stores | Native entry bytes |
|---|---|---|---|---|---|
| root_retirement | 2 → 0 | 4/4 | 5/5 → 3/4 | 8 → 8 | 1137 → 1105 |
| root_null_straight | 2 → 0 | 4/4 | 5/5 → 1/5 | 5 → 5 | 673 → 657 |
| root_null_join | 4 → 0 | 6/6 | 6/4 → 3/4 | 10 → 7 | 785 → 753 |
| root_null_mixed | 3 → 0 | 3/3 | 9/5 → 6/5 | 11 → 10 | 927 → 927 |
| root_null_backedge | 2 → 0 | 2/2 | 8/5 → 6/5 | 12 → 12 | 829 → 829 |
| root_null_alias | 2 → 0 | 5/5 | 7/5 → 2/6 | 6 → 5 | 879 → 863 |
| root_zero_bridge | 3 → 0 | 3/3 | 14/5 → 11/5 | 18 → 15 | 1133 → 1097 |
| root_zero_live_gap | 0 → 0 | 0/0 | 4/7 → 4/7 | 7 → 7 | 1226 → 1226 |
| direct_nullable | 2 → 0 | 4/4 | 6/3 → 3/3 | 4 → 2 | 487 → 471 |
| mutable_field | 0 → 0 | 0/0 | 1/7 → 1/7 | 5 → 5 | 798 → 798 |
| loop_carried | 0 → 0 | 0/0 | 2/3 → 2/3 | 4 → 4 | 625 → 625 |
| cross_nested_diamond | 0 → 0 | 0/0 | 0/7 → 3/4 | 4 → 7 | 1057 → 880 |
| cross_nested_child | 1 → 1 | 0/0 | 2/2 → 3/2 | 4 → 4 | 1044 → 842 |

Actual native changes are retained, not inferred from IR alone. The null-join entry loses three immediate zero stores (9→6). Retirement-only removes two vector-zero stores in its empty branch while immediate-zero totals alone stay unchanged; its optimized IR zero bytes fall 168→136. Instruction scheduling, frame layout and zero-run fragmentation can offset fewer writes. The final candidate preserves the original G39-F deterministic clear-group emitter; B's extra-zero bridge is absent.

## Roots, GC marking, pauses and child handoff

Root words and Slot visits are accumulated scan work, distinct from occupied/nonzero roots, static descriptor counts and marking visits. Optimized-AOT medians follow. Post-call payload includes dead objects awaiting collection and is not a live-set measurement. Native request counts are observed thread/epoch requests, not global threaded RSS or a universal malloc count.

| Pattern | G40 words/Slots/nonzero/mark/GC | G41 same counters | Post-call payload B |
|---|---|---|---|
| root_retirement | 780 / 234 / 0 / 0 / 78 | 780 / 234 / 0 / 0 / 78 | 384 → 384 |
| root_null_join | 702 / 156 / 0 / 0 / 78 | 702 / 156 / 0 / 0 / 78 | 384 → 384 |
| nested | 2340 / 312 / 156 / 312 / 156 | 2340 / 312 / 0 / 0 / 156 | 1164 → 1152 |
| reference_effects | 3432 / 624 / 3276 / 3432 / 156 | 3432 / 624 / 2964 / 3120 / 156 | 532 → 532 |
| root_zero_live_gap | 4680 / 936 / 0 / 0 / 312 | 4680 / 936 / 0 / 0 / 312 | 1024 → 1024 |
| cross_nested_diamond | 1092 / 312 / 156 / 312 / 156 | 468 / 156 / 0 / 0 / 78 | 1164 → 384 |
| cross_nested_child | 1404 / 312 / 1248 / 1404 / 156 | 624 / 156 / 468 / 468 / 78 | 1164 → 388 |
| cross_nested_escape | 1248 / 312 / 1248 / 1560 / 156 | 1248 / 312 / 1248 / 1560 / 156 | 1176 → 1176 |
| cross_deep_diamond | 1092 / 312 / 156 / 312 / 156 | 1092 / 312 / 156 / 312 / 156 | 1164 → 1164 |
| cross_nested_incoming | 3042 / 702 / 2184 / 2418 / 234 | 3042 / 702 / 2184 / 2418 / 234 | 872 → 872 |

All A and A+B observer comparisons preserved eight construction/allocation/root/mark/collection/payload counters exactly (228 and 236 mode-pattern pairs, three samples each). In final A+C, differences occur only in the two newly eliminated owners and earlier payload retirement for existing `nested`/`reference_effects`; construction counts change only for the two new positives. Exact field/sample differences remain in counter-comparison.json.

All 216,000 raw-address forced-collection checks pass under 1 MiB stacks. TLS raw pointers add no root. Inherited witnesses contribute 120,000 checks. The new nested-child witness contributes 96,000, with 8,000 post-birth polls per 1,000-iteration invocation (7,000 ordinary-path polls plus two extra polls for each of 500 nested arms), including after parent retirement. G40 shows 2,000 constructions and exactly 12 live payload bytes; G41 shows 1,000 constructions and exactly 4 bytes, throughout these checks. Release baseline/candidate and fatal-sanitizer candidate cover normal/O2 JIT and both independent AOT modes. Child roots remain independent; no claim of universally minimal child-copy lifetime is made.

Complete request-to-release and stopped-world samples remain separate from throughput. Descriptive optimized-AOT stopped µs median/p99/max follow. Finite-sample p99 is not a population tail guarantee.

| Pattern | G40 stopped µs med/p99/max | G41 same |
|---|---|---|
| root_retirement | 7.18 / 15.36 / 17.25 | 8.83 / 26.60 / 27.19 |
| root_null_join | 7.22 / 12.36 / 18.24 | 7.17 / 12.11 / 16.88 |
| loop_carried | 7.17 / 10.52 / 11.90 | 7.01 / 9.63 / 11.84 |
| nested | 7.41 / 13.51 / 14.99 | 7.58 / 13.21 / 32.20 |
| cross_nested_diamond | 7.37 / 15.24 / 39.97 | 7.05 / 13.90 / 20.65 |
| cross_nested_child | 7.60 / 11.09 / 19.64 | 7.28 / 13.03 / 26.06 |
| cross_nested_incoming | 7.68 / 12.03 / 16.94 | 7.53 / 13.89 / 19.72 |

## Linux, sanitizers and native Windows

Final Linux Release **86/86**, fatal ASan/UBSan/LeakSanitizer **86/86**, no suppressions or weakened negative assertions. Fatal TSan **64 patterns × two independent AOT modes**, plus coordination, races and allocator controls, passes. All source results, construction counts, precise homes/commits, injected cold OOM and recovery, foreign Heap, failure ordering, callback/reload, worker/async/cancellation, collections/text/JSON/I/O and G38 numeric/SIMD controls pass. Separate stack runs enforce 1,048,576 bytes.

Native Windows 10 build 19045, x64 VS2022/MSVC 14.44.35207, SDK 10.0.19041.0 and LLVM/Clang/LLD 23.1.4: **Release 71/71, Debug 71/71**. All 3,313 compiler/test inputs match before and after qualification. Seven PE binaries per profile reserve exactly 1 MiB. Twenty-eight selected native objects and disassemblies, final binary hashes, tool/provider identities and XML receipts are preserved. This is Windows correctness qualification, not a Windows throughput or macOS acceptance claim.

The first fatal build encountered a **Clang SDK code-generation crash (exit 139)** in Branch Probability Basic Block Placement/BranchFolder while compiling unchanged src/frontend/compiler.cpp, before tests could run. Its stack, preprocessed reproducer, shell command and source hash are retained. A retry from CMake compile_commands.json succeeded in 183.94s with identical source, code-generation and sanitizer options; it omitted only Ninja dependency-file flags -MD/-MT/-MF. Ninja resumed the remaining build, then the complete 86 fatal tests passed. No SDK update, pass disabling, source padding or sanitizer suppression was used, and no cause is invented.

A compiler-phase smoke fixture initially lacked any managed Runtime; the managed-only measurement host dereferenced its absent runtime_owner after warm invocation. Ordinary G40/G41 and phase observers reproduced this harness failure. The corrected managed-box smoke passes; actual scaling/throughput fixtures are coordinated. It is retained separately from compiler or generated-code findings.

## Observer-off matched performance and adverse results

All measurement-binary preparation and qualification finishes before final throughput; scaling compilation runs afterward with no overlap. Separate matched observer binaries collect counts and GC intervals. Full 64-pattern timing uses four balanced blocks × three samples across normal/O2 JIT and normal/optimized independent AOT. Natural standalone images use six balanced blocks × five samples. Independent arithmetic/domain outputs check every run. The host is shared; unrelated applications are not stopped. No unchanged-code timing variation is called an optimization.

| Standalone G41/G40 | Normal AOT | Optimized AOT |
|---|---|---|
| root_retirement | -2.1% | +7.1% |
| root_null_straight | +1.5% | -0.6% |
| root_null_join | +2.5% | -5.1% |
| root_null_mixed | -0.7% | +0.3% |
| root_null_backedge | +1.2% | +5.3% |
| root_null_alias | +3.4% | +5.8% |
| root_zero_bridge | +3.4% | -2.6% |
| root_zero_live_gap | +1.1% | +1.8% |
| direct_nullable | +1.0% | +4.2% |
| mutable_field | +0.2% | -0.6% |
| loop_carried | +0.5% | +1.0% |
| cross_nested_diamond | -50.0% | -51.7% |
| cross_nested_child | -51.7% | -53.0% |
| cross_nested_escape | +1.5% | +2.7% |
| cross_deep_diamond | -0.5% | -1.2% |
| cross_nested_incoming | +1.5% | +2.4% |

Largest adverse full-bundle rows are preserved, including small failure/control paths; absolute times prevent misleading percentages.

| Mode | Adverse control | Change | Median ns |
|---|---|---|---|
| jit-normal | outlined_array | +11.4% | 5455 → 6075 |
| jit-normal | direct_null_failure | +7.6% | 395 → 425 |
| jit-normal | direct_helper | +4.9% | 5586992 → 5861012 |
| jit-O2 | branch_alias | +5.9% | 925460 → 979960 |
| jit-O2 | outlined_array | +5.5% | 5370 → 5665 |
| jit-O2 | context_control | +5.1% | 5910670 → 6209349 |
| aot-normal | payload_budget | +14.7% | 3870 → 4440 |
| aot-normal | nested | +10.3% | 10317561 → 11384310 |
| aot-normal | mutable_field | +9.0% | 10081844 → 10989500 |
| aot-optimized | direct_null_failure | +26.8% | 280 → 355 |
| aot-optimized | payload_budget | +13.2% | 2015 → 2280 |
| aot-optimized | direct_incoming | +6.7% | 11501617 → 12275600 |

New allocation patterns and inherited controls across all modes follow.

| Full bundle G41/G40 | JIT normal | JIT O2 | AOT normal | AOT optimized |
|---|---|---|---|---|
| direct_reference | -1.1% | -1.2% | +1.8% | -1.7% |
| helper_branch_write | -1.9% | -1.7% | -5.9% | -2.3% |
| root_retirement | +2.2% | -2.8% | +5.3% | +6.0% |
| root_null_join | -2.3% | -1.2% | +3.2% | +3.6% |
| mutable_field | +3.9% | -0.7% | +9.0% | +0.6% |
| loop_carried | +1.5% | -2.3% | +6.6% | -0.0% |
| cross_nested_diamond | -53.5% | -53.4% | -51.4% | -52.3% |
| cross_nested_child | -49.6% | -54.6% | -50.4% | -51.9% |
| cross_nested_escape | -2.8% | -0.6% | +3.2% | -1.0% |
| cross_deep_diamond | +0.3% | +3.7% | +6.4% | +0.6% |
| cross_nested_incoming | +1.7% | +3.0% | +6.4% | +3.5% |

Inherited loop/native context controls additionally link the exact archived G38 loop object and rebuild exact qualified G39 backend c832db0f… with the same Core/Runtime/driver. These matched observations do not rewrite historical G40 null-join +5.8%, bundled mutable-field +14.3%, 500-site lowering +7.4%, loop +8.6% versus G38, or older G39-F loop +4.8%/worker O2 JIT +14.2%.

| Control | G41/G40 | G41/G38 loop or G39 managed |
|---|---|---|
| loop_carried normal | +0.5% | +4.4% |
| loop_carried optimized | +1.0% | -0.9% |
| calls jit-normal | +0.9% | -0.9% |
| workers jit-normal | +1.6% | -0.1% |
| async-tasks jit-normal | -2.7% | +3.2% |
| calls jit-O2 | +0.3% | -1.1% |
| workers jit-O2 | -1.6% | -1.5% |
| async-tasks jit-O2 | +3.2% | -0.8% |

Representative whole-source normal/O2 JIT and independently linked normal/optimized AOT cover arithmetic, arrays/sorting/dictionaries, Unicode/text/JSON and native I/O.

| Representative G41/G40 | JIT normal | JIT O2 | AOT normal | AOT optimized |
|---|---|---|---|---|
| array-1024 | -1.8% | -0.9% | +1.2% | +4.1% |
| array-4096 | -1.7% | -1.2% | +0.9% | +2.2% |
| dependent-1000000 | +0.6% | -0.6% | +0.3% | -0.9% |
| dict-1024 | +5.7% | +1.9% | +2.0% | +0.8% |
| float-1000000 | -1.5% | -1.8% | +0.1% | -0.5% |
| integer-1000000 | +0.7% | +2.7% | +1.1% | -0.6% |
| integer-4000000 | +0.3% | +0.1% | -0.6% | -1.5% |
| io-200 | +0.1% | +0.9% | +1.8% | +0.3% |
| json-128 | -0.3% | -3.0% | +0.8% | -1.7% |
| json-32 | +1.2% | -3.8% | +0.9% | -2.1% |
| sort-1024 | +1.0% | +0.0% | +1.3% | +4.4% |
| sort-256 | +4.5% | -0.9% | +1.7% | +2.5% |
| text-1024 | +2.2% | +0.3% | +1.8% | -1.1% |
| text-256 | +1.1% | +0.7% | +2.8% | +0.1% |

Both full representative AOT executables are byte-identical to their G40 counterpart; their timing changes are controls. Managed/closure/worker/async source controls follow; exact per-object identities are archived.

| Managed G41/G40 | JIT normal | JIT O2 | AOT normal | AOT optimized |
|---|---|---|---|---|
| async-tasks | -4.2% | -0.6% | -5.1% | -3.0% |
| calls | +1.5% | -0.9% | -6.9% | +1.9% |
| closures | +3.3% | -3.5% | +1.3% | +6.1% |
| managed-across-edge | -1.0% | +0.1% | -4.5% | +1.9% |
| managed-escape | -1.2% | +0.3% | -2.3% | +0.7% |
| managed-local | +2.3% | -0.2% | -5.6% | -0.1% |
| managed-reference | +0.8% | +0.2% | -3.2% | -1.4% |
| managed-survives-allocation | -1.6% | +2.9% | -1.6% | +3.4% |
| workers | -1.4% | -0.3% | +5.5% | -4.8% |

## Compiler scaling, startup and memory

100/500/2,000 denote total source constructor sites/evaluations in the generated single-block fixture (owner plus child). Three balanced blocks record ordinary frontend analysis, combined lower/JIT, process CPU/wall and wait4 peak RSS. Separate compiler-only phase observers break out source IR/index, physical-home plan, LLVM lowering/O2 and JIT materialization; they are never used for throughput. Moving G40's null proof to physical-home planning means source-index plus home-plan time must be compared together, rather than claiming the relocated work disappeared.

| Sites | Frontend ms | Lower/JIT ms | Change | Peak RSS KiB | Process wall s |
|---|---|---|---|---|---|
| 100 | 3.76 → 3.82 | 96.87 → 103.43 | +6.8% | 51036 → 50464 | 0.109 → 0.115 |
| 500 | 17.04 → 17.16 | 537.43 → 538.80 | +0.3% | 68832 → 68508 | 0.565 → 0.565 |
| 2000 | 73.66 → 74.15 | 4648.01 → 4547.53 | -2.2% | 140556 → 140412 | 4.750 → 4.654 |

| Sites | Variant | Source IR ms | Index ms | Home proof ms | LLVM lower ms | LLVM O2 ms | JIT materialize ms |
|---|---|---|---|---|---|---|---|
| 100 | baseline | 5.43 | 0.57 | 0.00 | 9.40 | 20.47 | 61.33 |
| 100 | candidate | 5.29 | 0.36 | 0.30 | 9.17 | 19.67 | 58.99 |
| 500 | baseline | 26.46 | 2.47 | 0.00 | 45.11 | 176.28 | 275.74 |
| 500 | candidate | 27.93 | 1.95 | 1.66 | 46.09 | 173.13 | 264.03 |
| 2000 | baseline | 112.82 | 11.51 | 0.00 | 182.22 | 2726.98 | 1391.25 |
| 2000 | candidate | 114.28 | 10.57 | 7.58 | 193.71 | 2800.88 | 1428.58 |

A separate 100/500/2,000-site nested-CFG stress cohort exposes the additional applications of the existing G35 CFG must-proof. It uses three balanced blocks with independent result checks. The new shape check is constant-size; total compiler work is not claimed linear. Source/index and end-to-end superlinear costs remain visible.

| Nested sites | Existing index/must proof ms | Process wall s | Change | Peak RSS KiB |
|---|---|---|---|---|
| 100 | 1.57 → 4.32 | 0.730 → 0.631 | -13.5% | 76164 → 76832 |
| 500 | 12.01 → 87.13 | 16.683 → 14.439 | -13.4% | 695144 → 695148 |
| 2000 | 62.97 → 2309.81 | 480.847 → 470.707 | -2.1% | 10309828 → 10296976 |

| Nested sites | Variant | Source IR s | Index s | Home proof s | LLVM lower s | LLVM O2 s | JIT materialize s |
|---|---|---|---|---|---|---|---|
| 100 | baseline | 0.107 | 0.002 | 0.000 | 0.103 | 0.130 | 0.269 |
| 100 | candidate | 0.107 | 0.004 | 0.001 | 0.104 | 0.111 | 0.187 |
| 500 | baseline | 2.837 | 0.012 | 0.000 | 2.521 | 3.087 | 5.237 |
| 500 | candidate | 2.789 | 0.087 | 0.004 | 2.645 | 2.667 | 3.416 |
| 2000 | baseline | 112.371 | 0.063 | 0.000 | 105.903 | 89.918 | 59.355 |
| 2000 | candidate | 114.859 | 2.310 | 0.017 | 109.725 | 94.353 | 39.642 |

Warm-filesystem n=0 startup uses 20 balanced process blocks; it is not a cold-disk guarantee.

| Startup | Wall ms G40→G41 | Peak RSS KiB |
|---|---|---|
| aot-normal | 2.416 → 2.393 | 15476 → 15476 |
| aot-optimized | 2.310 → 2.268 | 15540 → 15476 |
| jit-O2 | 22.300 → 21.608 | 47116 → 47030 |
| jit-normal | 20.236 → 19.715 | 44886 → 44978 |
| representative-aot | 3.154 → 3.064 | 15604 → 15604 |
| representative-aot-normal | 3.228 → 3.152 | 15604 → 15604 |

## Complexity, remaining objectives and next action

The backend diff adds 127 lines and removes 63 (net 64). G40's separate source-local null pass is removed; one shared physical-home representation takes its place. It adds exact home-key lookup, per-instruction home IDs, capped must-null states and an omission set consumed by the existing emitter. Predecessors are reused from the existing function-wide index. No per-candidate full instruction rescans are added. C adds constant-size shape checks and reuses the existing global CFG ownership proof, increasing the number of its applications; the nested scaling cohort measures that cost. B's extra-zero plan/address tables and 40-added/one-removed-line extension are absent from production. Runtime metadata and authority do not grow.

Unsupported cases stay conservative: deeper/irreducible CFGs, unknown joins/rebinding, arbitrary postdominators/heap graphs, incoming or mutable unknown owners, general private-factory/interprocedural retention, native/callback/worker/async/resource captures and cleanup. General defining-store overwrite proofs and profitable no-extra-write batching remain open. Smaller IR, fewer stores or roots alone is not a speed guarantee.

Outstanding Language Bible work remains broader escape and direct-reference lifetimes, current-Heap provenance and interprocedural retention, checked collections, dictionary/sort costs, text/Unicode/JSON materialization, SIMD beyond G38, native/JIT context, compiler/index/RSS scaling, GC-tail/startup and future macOS/release qualification. Historical accepted milestones, performance-held G37-F and all rejected experiments remain intact. The next proposed implementation is to fold already-required defining-null writes into retirement fills within one proven observation interval without adding zero bytes, beginning with the optimized retirement-only regression. It must prove alias/failure boundaries and improve final native work before expansion; it is not implemented here.

## Archive, source inventory and task-only cleanup

Archive: `/home/shmellyorc/Documents/Void.Language-archives/g41-root-store-precision-2026-11-15.zip`. DEFLATE 9, CRC, every SHA-256 manifest entry and restoration of exact final postimages from accepted 9f003772 preimages plus patch are verified before task-only cleanup. Sibling verification, git-inventory, cleanup and closeout receipts hold the archive hash/bytes/count, final file hashes, exact Git status and separate observed host/guest reclamation. An archive cannot contain its own digest. No VDI compaction is inferred from guest free space.

Final inventory is exactly 12 files:

```text
CMakeLists.txt
docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md
docs/compiler-health/G41_ROOT_STORE_PRECISION.md
src/backend/llvm/backend.cpp
tests/escape_analysis/cases.hpp
tests/escape_analysis/fixtures/cross_deep_diamond.void
tests/escape_analysis/fixtures/cross_nested_child.void
tests/escape_analysis/fixtures/cross_nested_diamond.void
tests/escape_analysis/fixtures/cross_nested_escape.void
tests/escape_analysis/fixtures/cross_nested_incoming.void
tests/escape_analysis/fixtures/root_zero_bridge.void
tests/escape_analysis/fixtures/root_zero_live_gap.void
```

Compiler/test postimages match qualification; only report/ledger documentation is written afterward. HEAD remains 9f00377236a1080ec4ebdc6252f3721843dd2f94 on codex/g41-root-store-precision, with no staged files or commits. SDK/providers, protected G37-F/G38/G39/G40 worktrees, G40 integration preparation and every older evidence archive are unchanged. Cleanup removes only the three G41 build groups, evidence scratch/transport and temporary SDK reuse link after verification.
