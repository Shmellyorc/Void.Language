# Shared internal performance repair — local result

> Publication note: this commit keeps selected text evidence. Full local archives,
> binaries, candidate trees and disposable runs remain excluded and preserved;
> see the [artifact scope and original-document hashes](evidence/integrated-performance/LOCAL_ARTIFACTS.md).
> Measurements and conclusions below are unchanged; artifact links are portable.

5 November 2026. **Final recommendation: RETAIN the checked-token/root search, Runtime metadata and direct-cursor ToolingIndex repairs for integration review. The per-Frame positive-location cache was removed.** This is an engineering retention recommendation, not production or cross-platform release acceptance.

The unchanged complete LLVM 23 workloads improve: 64 KiB O2 text/codecs **16.85% wall / 16.92% CPU**, 16,384 mixed stable sorting **42.93% wall / 42.94% CPU**, wide128 parsing **41.84% wall** and stringify **23.33% wall**. Index-owned immutable records fall from **9,835 to zero** in the large graph. Editor latency is not universally faster; rename/formatting and HTTP controls retain regressions documented below.

## 1. Actual changes and integration base

The tracked starting tree was clean at `f3cf240e0b14240d9c7486c8882842bfa7043c63`. The audit and logging investigation documents/experiments were already untracked. The entire audit was read before code changes. The refined logging candidate was separate from production. Its approved **19-file** delta was applied cleanly before capturing the measurement baseline; its original directory, patches and evidence were preserved. The baseline therefore already contains cleanup improvements, checked dense snapshots, compact synchronous I/O success, original V1 formatting and disabled automatic join/UTF-8 fusion. Their earlier V1-to-refinement gains are not counted here.

New production changes relative to that integrated baseline are exactly: `src/frontend/tooling.cpp`, `src/runtime/coordination.cpp`, `src/runtime/buffer.cpp`, `src/runtime/list.cpp`, `src/runtime/array.cpp`, `src/runtime/dictionary.cpp`, `src/backend/llvm/backend.cpp`, `include/void/tooling.hpp`, `include/void/runtime.hpp`. [Internal-only patch](../../experiments/internal-performance/internal-repair.patch), [integrated baseline identities](../../experiments/internal-performance/evidence/integrated-baseline-identity.json), [retained identities](../../experiments/internal-performance/evidence/candidate-source-identity.json).

- Root output lookup and tokens share checked registration locations. Tokens carry Frame lifetime identity, Slot/offset ordinals and the originating Slot descriptor. Every use verifies attachment/Managed state, Runtime/thread ownership, an active matching Frame lifetime, current descriptor/bounds/address and current heap membership. Values are re-read after safepoints. Private-builder exclusion and heap output-alias checks remain at their original boundaries. Full newest-first search remains; containment only filters impossible Slots and never authorizes a word without its exact offset.
- Frame lifetimes use a nonwrapping atomic identity. Same-address placement-new reuse rejects old tokens. No heap allocation, address registry or positive/negative cache is attached to a Frame. Backend object-cache identity explicitly includes checked-root ABI identity and Frame size; actual compiler/link fingerprints remain authoritative.
- Scalar identities use fixed string views and transparent existing catalog lookup. Scalar/composite headers share validation. Runtime-owned derived metadata reuses only canonical default zero-capacity layouts, with at most one entry per admitted schema; dictionary headers share that entry. Native descriptors are rechecked on every call. Nonempty capacities remain uncached, existing buffer-owned layout reuse remains, and exact schema/nominal identity is preserved. Optional map insertion alone may fail and return the successfully constructed uncached layout. Catalog admission is additive and keeps equivalent admitted pointers; caches contain data-only schemas/layouts/descriptors, no payloads, native ABI pointers, code pointers or generation leases.
- ToolingIndex uses two immutable analysis-backed ranges instead of two owning record vectors. Views **and iterators** own the analysis; query optionals/vectors remain owning copies. A direct pointer cursor crosses the ordinary/editor boundary once. Order, duplicates, source identity and positions remain unchanged. No incremental checker, assistance cache or editor scheduler was added.

## 2. Redundant work removed and memory cost

Reused tokens no longer repeat full output lookup plus an originating-Frame offset scan. Scalar reads no longer construct transient admitted-schema strings. Zero-capacity metadata and dictionary identity/header work are reused, and exact reference-offset comparisons no longer allocate temporary vectors. Index construction no longer copies ordinary/editor declaration/reference populations.

| Native representation | Baseline bytes | Retained bytes |
| --- | ---: | ---: |
| Runtime | 872 | 920 |
| Frame | 32 | 40 |
| RegisteredRootWord | 32 | 88 |
| ToolingIndex object | 256 | 208 |

The Frame increase is **8 bytes**, or 512 bytes for 64 simultaneous Frames. Tokens add 56 bytes. This does not establish an abstract recursion capacity: full programs were executed with **soft and hard 1,048,576-byte stacks**. Frame entry/leave still makes zero allocation requests. A cold zero-layout cache entry retains **144 requested native bytes** after the caller releases its layout, versus zero in the baseline, plus the Runtime's 48-byte map object. Destruction of the Runtime releases those bytes. This bounded retention trades one initial map allocation for repeated layout reuse; arbitrary capacities are not retained.

| Root/metadata fixture, warmed operation | Baseline ns | Retained ns | C++ new requests/op, baseline / retained |
| --- | ---: | ---: | ---: |
| Enter/leave 8-Slot Frame | 27.15 | 23.82 | 0 / 0 |
| Fresh last-root token, 1,024 Slots | 1458.42 | 1370.96 | 0 / 0 |
| Reused last-root token + empty scalar layout, 16,384 Slots | 45862.71 | 126.23 | 3 / 0 |
| Same query, 16,384 packed offsets | 18169.67 | 125.37 | 3 / 0 |
| Random roots, 16,384 Slots | 11577.15 | 10777.65 | 0 / 0 |
| Random roots, 16,384 packed offsets | 4283.80 | 2527.02 | 0 / 0 |
| Fresh outer token through 64 Frames | 203.33 | 125.08 | 0 / 0 |
| Scalar default zero-extent layout | 171.31 | 100.98 | 3 / 0 |
| Scalar capacity-16 layout | 169.00 | 124.59 | 3 / 2 |
| Ordinary fixed scalar read, 128-buffer heap | 288.64 | 204.48 | 1 / 0 |

These micro queries are mechanisms, not application speedup claims. Reused-token rows also acquire an empty scalar layout. Raw five-sample timing/CPU rows and separate allocation observations are retained in each `E/F/G/H-*-root` stdout/stderr file.

## 3. Correctness and local qualification

The final 40-byte Frame implementation passed **63 selected Release tests**, **16 strict ASan+UBSan native/host tests**, and **five Debug Runtime tests**, all launched with real 1 MiB soft/hard limits. Following the iterator refinement, **eight Release and eight strict tooling/LSP tests** passed again. Runtime/core/backend/cache-identity archives are byte-identical across that iterator-only change, so their qualified execution measurements were retained rather than rerun. [Final qualification receipts](../../experiments/internal-performance/evidence/validation-complete.json), [cursor archive equivalence](../../experiments/internal-performance/evidence/cursor-runtime-archive-equivalence.json).

Coverage includes normal/O2 JIT and independently emitted/linked normal/O2 AOT; GC/root/schema invariants; nested, overlapping, expired and placement-new Frame reuse; foreign Runtime/thread; unsorted/duplicate offsets and changed descriptors; heap/interior/private aliases; forced collection, allocation-failure sweeps and successful retries; every scalar encoding and canonical zero-layout insertion failure; dictionary/version/reader/nominal/reload checks; original text byte/codec oracles, ownership and depth; stable sort/callback/mutation/readonly/fixed-value independence; JSON grammar/numbers/schema/bounds/ordered metering/ownership; async source/worker/native-job cancellation, repeated await, quiescence, retention and balanced handles; old analysis/view/iterator lifetime, empty ranges, UTF-16, stale acceptance, overlays, refresh, rename and formatting. Existing validation and allocation budgets remain intact.

The original logging JIT host passed **1,690 checks** in Release and strict modes. Independently linked normal/O2 logging AOT probes passed **828 checks each** in both modes. The text ownership host passed **17,834 checks** in strict mode. The full source execution scripts perform actual JIT, O2 and independent AOT comparisons; results are not multiplied into a synthetic suite count.

Preserved failures are not hidden: the first combined Release run invoked an old text IR audit executable and failed; rebuilding that host made the unchanged test pass, and the final complete Release run is 63/63. The initial source inventory was enumerated before applying the approved logging patch and omitted its newly added `src/ir/local_cleanup.inc` from the first source snapshot, not from the actual baseline build. The complete snapshot restores the byte-identical file from the untouched, manifest-verified logging candidate; both inventories/archives and the correction receipt remain. Earlier measurement packaging rejected a relocated Runtime absent from its inventory. The old query fixture lacked now-required active-parameter documentation; its isolated counterpart adds only documentation and the corresponding declaration line adjustment. Attribution helper compile/launch corrections, a wrongly invoked invariant executable, and all failed attempts remain in command/driver logs. No production validation was weakened to make these pass.

One original final-editor baseline attempt exited unexpectedly during startup (`EOFError`); that original harness did not retain the child's exit/stderr, so the cause is unresolved. One predeclared preserving replay and one candidate diagnostic both completed; captured fixtures, limits, per-action checkpoints and successful child exits are retained. No retries were selected by elapsed time. The failed legacy invocation had not emitted its partial in-memory timing arrays; those values are unrecoverable and were not fabricated or used. Its complete emitted stdout/stderr is preserved. The preserving replay checkpoints each completed action. The baseline replay's startup interval includes diagnostic fixture copying; **no startup speedup is claimed**. Edit/query intervals keep their original timed bodies. This unexplained baseline failure remains a qualification limitation, not evidence of a candidate correctness failure.

Strict builds use Clang/LLVM/LLD **23.1.4**, RelWithDebInfo `-O2 -g`, ASan/UBSan, leak detection and halt/abort on errors; Release uses the actual `-O3 -DNDEBUG` recipes. Debug coverage is the native Runtime tests, not a full Debug compiler/editor matrix. Generated source code and third-party LLVM are not fully sanitizer-instrumented. Native Windows/macOS execution and hosted CI were not performed.

## 4. Matched whole-workload results

Original workload algorithms, inputs, checks, callback counts and timer boundaries were retained. Runtime cohorts are baseline/retained/retained/baseline (**E/F/G/H**). Each text, collection, JSON and logging cell has three observations per block, six per variant. JSON compiles once per block and runs the unchanged four-corpus native driver three times, each timing 20 parse and 20 stringify calls after its original warmups. Compilation stays outside execution timers. The final editor-only direct-cursor cohort is **I/J/K/L**, with three observations per block; the last baseline edit block uses the documented preserving replay. Unaffected Runtime/compiler/backend artifacts are byte-identical. [Selection/provenance](../../experiments/internal-performance/evidence/final-measurement-selection.json), [all 1,356 retained raw observations](../../experiments/internal-performance/evidence/final-whole-workload-raw.json), [all 86 cells](../../experiments/internal-performance/evidence/final-whole-workload-summary.csv). Earlier superseded samples and regressions remain separate.

| Workload | Baseline wall ms | Retained wall ms | Change | Baseline / retained CPU ms |
| --- | ---: | ---: | ---: | ---: |
| Text/codecs O2, 1,024 input bytes | 18.920 | 12.948 | -31.56% | 18.840 / 12.907 |
| Text/codecs O2, 4,096 input bytes | 79.912 | 52.695 | -34.06% | 79.453 / 52.449 |
| Text/codecs O2, 16,384 input bytes | 350.780 | 257.490 | -26.60% | 348.800 / 256.164 |
| Text/codecs O2, 65,536 input bytes | 2237.200 | 1860.165 | -16.85% | 2225.060 / 1848.645 |

| Workload | Baseline wall ms | Retained wall ms | Change | Baseline / retained CPU ms |
| --- | ---: | ---: | ---: | ---: |
| Stable i32 sort, 16,384 ordered | 806.969 | 493.637 | -38.83% | 802.979 / 490.884 |
| Stable i32 sort, 16,384 reverse | 819.308 | 511.252 | -37.60% | 815.130 / 508.992 |
| Stable i32 sort, 16,384 duplicates | 1039.130 | 596.379 | -42.61% | 1035.020 / 593.409 |
| Stable i32 sort, 16,384 mixed | 1024.335 | 584.589 | -42.93% | 1020.095 / 582.026 |
| 32 strings, original text comparator | 11.126 | 7.728 | -30.53% | 11.056 / 7.691 |
| Four independent 256 KiB fixed values | 449.203 | 439.120 | -2.24% | 447.090 / 437.322 |
| Dictionary keys, 1,024 entries ×3 | 9.801 | 6.488 | -33.80% | 9.759 / 6.460 |
| Dictionary values, 1,024 entries ×3 | 9.815 | 6.532 | -33.45% | 9.775 / 6.489 |
| Dictionary copy, 1,024 entries ×3 | 16.482 | 11.675 | -29.17% | 16.407 / 11.614 |

| Workload | Baseline wall ms | Retained wall ms | Change | Baseline / retained CPU ms |
| --- | ---: | ---: | ---: | ---: |
| save, parse ×20 | 28.234 | 17.876 | -36.69% | not separately timed by original driver |
| save, stringify ×20 | 29.720 | 21.797 | -26.66% | not separately timed by original driver |
| config, parse ×20 | 19.795 | 12.715 | -35.77% | not separately timed by original driver |
| config, stringify ×20 | 22.441 | 16.541 | -26.29% | not separately timed by original driver |
| deep64, parse ×20 | 96.785 | 62.802 | -35.11% | not separately timed by original driver |
| deep64, stringify ×20 | 151.854 | 114.294 | -24.73% | not separately timed by original driver |
| wide128, parse ×20 | 1144.937 | 665.942 | -41.84% | not separately timed by original driver |
| wide128, stringify ×20 | 332.735 | 255.116 | -23.33% | not separately timed by original driver |

JSON rows are totals for 20 calls; wide128 is **57.247 → 33.297 ms/parse** and **16.637 → 12.756 ms/stringify**. Stringify still clones before writing. The complete four-corpus native process, including its original warmups/printing/startup, has median **1,922.549 → 1,234.944 ms wall**, **1,910.983 → 1,224.157 ms CPU**. This process CPU is not a per-corpus CPU attribution.

| Workload | Baseline wall ms | Retained wall ms | Change | Baseline / retained CPU ms |
| --- | ---: | ---: | ---: | ---: |
| Large graph valid edit + diagnostics | 295.154 | 288.810 | -2.15% | not separately timed by original driver |
| Large graph invalid edit + diagnostics | 200.371 | 199.986 | -0.19% | not separately timed by original driver |
| Large graph repair + diagnostics | 300.761 | 292.517 | -2.74% | not separately timed by original driver |
| Large graph completion | 192.141 | 185.954 | -3.22% | not separately timed by original driver |
| Large graph signature help | 189.178 | 187.647 | -0.81% | not separately timed by original driver |
| Large graph rename preflight | 484.445 | 501.930 | +3.61% | not separately timed by original driver |
| Large graph formatting preflight | 287.251 | 282.603 | -1.62% | not separately timed by original driver |

Large valid-edit wall changes only **−2.15%**; large rename remains **+3.61%**, and medium rename/formatting are **+5.86% / +6.37%**. Thus record reuse is a storage/retention repair with limited responsiveness benefit, not whole-graph incrementality. Complete parsing/checking and guarded rewrite preflights remain.

Compilation is retained separately in [compile samples](../../experiments/internal-performance/evidence/final-compile-samples.json), original collection-host `compile_wall/compile_cpu`, logging `compile_ns`, JSON compile command wall/CPU and Stage 7 CLI-check rows. The original text host does not separately expose its analysis/JIT compilation interval; no text compilation speedup is inferred by subtracting execution from process time. No elapsed-time claim uses historical LLVM 19 receipts.

## 5. Allocation, search, collection and LSP ownership measurements

Separate disposable observation builds record all four ABBA blocks, two observations inside each invocation: **128 rows across 16 execution cases**, plus 12 standalone tooling observations and valid/invalid/repaired editor phase records. They do not supply the uninstrumented execution times above. Counts are root-thread C++ new-family requests, including managed payload allocation requests, not all malloc/system allocation or absolute RSS. Managed constructions and heap snapshots are separate. Half counts are medians of four observations, not fractional events.

| Observed complete operation | C++ new requests, baseline / retained | Requested bytes, baseline / retained | Managed constructions | GC calls | Fallback offset comparisons, baseline / retained |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 KiB text/codecs | 6,612,625.0 / 4,214,910.0 | 337,794,715.0 / 262,253,167.0 | 82,057 / 82,057 | 320.5 / 320.5 | 168,961,375.0 / 28,405,972.0 |
| 16,384 mixed stable sort | 1,769,701.5 / 308.5 | 55,586,010.0 / 734,849.0 | 48 / 48 | 0.0 / 0.0 | 334,463,974.0 / 55,193,959.0 |
| 1,024 dictionary keys ×3 | 58,835.0 / 254.0 | 2,345,956.0 / 36,784.0 | 40 / 40 | 0.0 / 0.0 | 627,314.0 / 141,838.0 |
| wide128 parse ×20 | 2,139,193.5 / 729,653.5 | 96,117,320.0 / 46,484,700.0 | 20,660 / 20,660 | 81.0 / 81.0 | 647,973,280.0 / 69,640,160.0 |
| wide128 stringify ×20 | 893,668.5 / 363,648.5 | 37,708,108.0 / 19,844,588.0 | 31,600 / 31,600 | 123.5 / 123.5 | 81,735,280.0 / 12,366,640.0 |

All 16 matched attributed cases retain equal managed-construction counts, scalar accesses, selected UTF-8 staging bytes, GC counts and Frame root-word scans. For 64 KiB text, source scalar reads remain **972,128** and selected UTF-8 staging bytes **278,626**. Native request reductions therefore do not imply deleted semantic results, fixed-value copies, JSON clones, validation or collection work. Layout-construction counts also include stack-only expected geometry checks, which intentionally remain. Frame/token direct checks are counted separately from fallback offset comparisons; there are **zero positive-cache hits** in the retained implementation.

| Separate instrumented CPU interval | Baseline exclusive ms | Retained exclusive ms |
| --- | ---: | ---: |
| 64 KiB text: registration checks/search | 5461.111 | 2729.604 |
| 64 KiB text: metadata acquisition/validation | 5521.693 | 4845.197 |
| 64 KiB text: selected native payload boundaries | 1430.040 | 1447.093 |
| 64 KiB text: GC/rendezvous/root enumeration | 1476.867 | 1495.265 |
| wide128 parse ×20: duplicate-loop source instructions | 7600.776 | 7861.030 |
| wide128 stringify ×20: clone body | 188.960 | 238.699 |

These are **exclusive observer-build CPU intervals**; child time is charged once, including nested GC scopes. They are deliberately not production milliseconds or additive components of the uninstrumented medians. In particular, the JSON duplicate-instruction observer makes **10,823,680 timed entries** for wide128 parse×20 and heavily perturbs execution: observer total CPU is about **20.447 → 18.450 seconds**, while plain parse×20 is 1.145 → 0.666 seconds. Its exclusive duplicate/clone interval increases despite lower inclusive checked-access cost; no production duplicate-check or clone percentage is claimed. The original **8,128 comparisons / 24,384 explicit charges per wide object** and clone-before-write error ordering remain. Raw counters and inclusive/exclusive CPU are available in [attribution summary](../../experiments/internal-performance/evidence/attribution-summary.json).

The large immutable graph contains **6,507 declarations and 3,328 references**. Index-owned copies are **6,507/3,328 → 0/0**, checked against backing record addresses. Index-only retained requested bytes are **10,311,792 → 1,059,178** (−89.73%); peak new requested bytes **11,850,100 → 1,059,690**; construction requests **36,952 → 23,642**. The existing analysis still owns every record and the necessary index maps remain. The direct-cursor change does not alter those constructor paths or linked archives.

Uninstrumented large-graph edit-cohort idle RSS medians are **124,558 → 129,008 KiB** (a regression), while process HWM is **141,124 → 129,008 KiB**. After query cohorts, idle RSS is **134,058 → 126,058 KiB**, HWM **147,322 → 128,238 KiB**. RSS includes allocator/graph/history effects and is not interchangeable with index-owned bytes. No universal resident-memory reduction is claimed.

One separate large valid-edit phase observation (ms exclusive CPU) is discovery/read **8.449 → 8.165**, parse **32.737 → 33.008**, check **193.642 → 198.900**, analysis metadata **27.615 → 31.892**, index **10.541 → 5.822**, invalidation **6.008 → 7.231**, serialization **0.144 → 0.156**. The remaining orchestration interval is retained as well. This identifies substantial checking cost and a smaller index phase; it does not authorize a new checker or scheduler.

## 6. Rejected candidates, regressions and residual work

The first four-entry checked-location cache occupied **112-byte Frames** and regressed random/deep controls. A checked containment filter and skipping empty cache entries repaired much of that mechanism cost, but the bounded cache-on/off ABBA whole-workload control still slowed **every text size/mode by approximately 3–6%** and the large fixed-value control by 2.92%. It helped wide128 parsing by 12.58%; that did not justify retaining its representative text regressions. The entire Frame cache was removed, not enlarged or selected by an arbitrary workload threshold. [All cache control rows/statistics](../../experiments/internal-performance/evidence/cache-control-summary.json) and both discarded source/binary variants remain.

The first two-segment view used indexed dereferencing and enlarged large rename preflight by **21.71%**. It was replaced with the direct cursor, and explicit ordering/end/empty-view checks were added. All first-view samples remain in `indexed-view-*`; the final residual query changes are reported, not trimmed away.

| Final matched cell with wall increase above 2% (reporting filter only) | Baseline / retained ms | Change |
| --- | ---: | ---: |
| lsp/editor/large/definition_roundtrip | 0.057 / 0.061 | +8.85% |
| lsp/queries/small/hover | 0.085 / 0.099 | +15.20% |
| lsp/queries/medium/completion | 30.398 / 31.377 | +3.22% |
| lsp/queries/medium/signatureHelp | 30.441 / 31.602 | +3.81% |
| lsp/queries/medium/rename_preflight | 65.300 / 69.127 | +5.86% |
| lsp/queries/medium/formatting_preflight | 49.153 / 52.284 | +6.37% |
| lsp/queries/large/rename_preflight | 484.445 / 501.930 | +3.61% |

The 2% display cutoff is only to keep this table readable; **it is not an acceptance threshold**. Every smaller increase, sample, range and compile observation remains in the complete JSON/CSV. Retention is based on actual work removed, correctness, whole text/collections/JSON gains and the large index-ownership reduction, with the remaining latency and I/O tradeoffs explicit.

The original one-worker 256×64 KiB async I/O control runs HTTP absent, idle and paused, including the first client initialization. Every invocation writes the full 16 MiB and drains managed objects after collection. Its original ABBA had a slower idle-HTTP candidate block. Exactly one predeclared same-core ABBA diagnostic was added; cohorts are **not pooled** and no slow sample or cold initialization is discarded.

| Cohort / HTTP state | Baseline wall / CPU ms | Retained wall / CPU ms | Wall / CPU change |
| --- | ---: | ---: | ---: |
| Original ABBA, absent | 65.237 / 63.374 | 63.104 / 61.826 | -3.27% / -2.44% |
| Original ABBA, idle | 65.093 / 63.682 | 87.332 / 81.995 | +34.16% / +28.76% |
| Original ABBA, paused | 68.643 / 65.575 | 71.947 / 65.139 | +4.81% / -0.66% |
| Separate fixed-core ABBA, absent | 61.806 / 60.584 | 62.541 / 61.277 | +1.19% / +1.14% |
| Separate fixed-core ABBA, idle | 76.770 / 71.337 | 75.108 / 64.002 | -2.17% / -10.28% |
| Separate fixed-core ABBA, paused | 69.307 / 64.322 | 75.338 / 65.225 | +8.70% / +1.41% |

The original idle-HTTP aggregate regression is real in that cohort and is driven by candidate block 2; candidate block 1 was close to the baseline. The fixed-core diagnostic does not reproduce that idle CPU regression. Paused wall remains slower in both cohorts, and some CPU controls also regress. No exclusive cause is established and no async sweep/scheduler change is made or claimed. These controls limit any claim of universal throughput improvement.

## 7. Refined logging compatibility and real durability

| Workload | Baseline wall ms | Retained wall ms | Change | Baseline / retained CPU ms |
| --- | ---: | ---: | ---: | ---: |
| Original FILE200, command 11 | 16.265 | 14.015 | -13.83% | 8.963 / 8.358 |
| Small console, command 10 | 5.937 | 5.595 | -5.77% | 5.909 / 5.570 |
| 100 × 1 KiB file, command 12 | 14.407 | 14.305 | -0.71% | 7.929 / 7.856 |
| 3 × 65,500-byte file, command 13 | 10.151 | 9.949 | -1.99% | 3.186 / 3.124 |

Both variants already contain the approved logging candidate. This table measures only the additional shared internal repair. Original bytes, record order, individual acknowledgements, preparation/flush/close and real final durability remain inside the original timer. Every measured FILE200 row verifies **200 writes, 15,000 bytes, one flush**, baseline handle restoration and post-GC heap restoration.

A separate native syscall-wrap control executes one original warmup plus one FILE200 measurement and records **400 actual native writes, 30,000 bytes and two real `fsync` calls**. This is deliberately outside the primary timings. The current IR proof confirms **two ordinary joins, one ordinary encoding, zero fused calls**, **18 local cleanups**, one compact write and three rejected malformed private operations. No final sync, payload ownership, cancellation/quiescence, V1 formatting or typed/partial error contract was weakened. [Durability proof](../../experiments/internal-performance/evidence/durability-proof.stderr), [IR proof](../../experiments/internal-performance/evidence/logging-ir-retained.stdout), [protected original evidence check](../../experiments/internal-performance/evidence/protected-originals-check.json).

## 8. Final recommendation and retained evidence

**RETAIN** the simplified checked-token/root fallback, admitted metadata reuse and direct-cursor storage repair for review. **Do not retain the Frame location cache or indexed iterator.** The decision rests on complete LLVM 23 execution gains and direct ownership/allocation evidence, with unchanged semantic algorithms and qualified failure behavior. It does not assert that all queries or HTTP modes are faster, that instrumentation measures production phase shares, or that local checks grant release acceptance.

No JSON clone deletion, duplicate-policy/metering change, sort/comparator rewrite, string representation change, collector/scheduler change or automatic UTF-8 fusion was introduced. No cloning, staging, commit, push, branch, hosted CI or root README change was performed. Original logging evidence, including its rejected experiments, remains intact; **769 protected refinement artifacts** match their manifest, including its logical report/test aliases.

The sprint artifacts under [experiments/internal-performance](../../experiments/internal-performance/) retain initial/integrated/discarded/final source snapshots, actual command/status/stack records, every raw emitted sample and failure, separately linked AOT artifacts in the build trees, private observation archives, matching source/library/executable identities and reproduction notes. [Commands](../../experiments/internal-performance/evidence/commands.jsonl), [final sample selection](../../experiments/internal-performance/evidence/final-measurement-selection.json), [native sizes](../../experiments/internal-performance/evidence/sizes-candidate.stdout). There is no arbitrary timing release gate and no erasure of unfavorable evidence.
