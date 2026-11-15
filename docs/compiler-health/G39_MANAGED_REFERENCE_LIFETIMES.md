# G39 — managed reference temporaries and precise lifetimes

Starting commit: `d1fc214f17715d464af5fc2d6cb4177258f9eb8f` (accepted G38). Review tree: `/home/shmellyorc/Documents/Void.Language-G39`, branch `codex/g39-managed-lifetimes`. **Implemented and correctness-qualified on Linux and native Windows 10. Broad performance acceptance remains held:** new allocation-elimination cases improve substantially, while earlier root clearing has unfavorable control results. Golden Wrench reviews integration; Boss controls Git. Nothing is staged, committed, pushed or dispatched to hosted CI.

The original `main` was at `97851c92`, with different G37-F contents. It was left untouched. This worktree starts at the exact requested commit and imports no G37/G37-F Runtime or ABI change. LLVM/Clang/LLD remain **23.1.4**. G31 polling, G33 scalar elimination, G34 reuse/root representation/pacing, G35 indexed ownership and CFG epochs, G36 checks and G38 bounded AOT SIMD remain in place.

## 1. Implemented compiler changes and new eligibility

Only production file `src/backend/llvm/backend.cpp` changes. Tests/CMake and this report/ledger accompany it; Runtime and public headers are unchanged.

- A reference-bearing `gc_construct` can now be eliminated without an immediate owning-local publication when its indexed uses have no local publication and remain in its construction block. The existing alias/escape proof still rejects returns of the owner, publication, captures, identity inspection, unsupported calls and rebinding. Backing storage remains at function entry, bounded by **256 bytes/object and 16 KiB/function**.
- A block-local owner's payload child words retire after its final indexed alias/use. Existing G35 owners with cross-block uses retain the accepted function-duration payload lifetime.
- Ordinary managed SSA and local **copies** can retire earlier in a bounded synchronous function. SSA values are block-local; returned SSA values keep their homes through frame exit. A local must have all uses in one block and every store before every load. Receiver places, loop-carried read-before-store and cross-block homes fall back. Parameters keep their independent parameter roots. Cleanup/release, async, worker/closure and unknown/native/public/callback operations preserve conservative homes.
- A readonly child read can establish current-heap provenance only from a completed `gc_construct`, directly or through a local assigned exclusively such constructions. `gc_allocate`, global cells, arbitrary owner aliases, mutable fields, factory-derived owner fields and incoming owners do not gain that stronger field guarantee. Uncertainty propagates through the existing use index, including readonly field reads.
- Private helper support grows from one block to **at most eight blocks / 64 total instructions**, still with one non-null ordinary managed formal, scalar formals, scalar/void result, verified nonretention and scalar-only mutations. Alias bookkeeping rejects a prior unborrowed store/load even when an alias is discovered in a later block. Selected construction-view clones are independently revalidated; original functions, symbols, signatures, reload identities and dispatch remain.
- Fresh private factories retain G35's current-heap result proof. A separate flag in that existing summary proves safe root transfers from the factory's bounded body. Freshness alone does not authorize earlier caller-root retirement. Unknown calls, native/callback operations and reference-valued field writes fail that transfer proof.

Newly eliminated cases include direct field-consumed reference holders, direct nullable holders, direct outer holders over heap-backed nested references, readonly-field-derived children from validated local owners, and branching private read/mutation helpers. A helper argument can already receive a hidden owning-local publication in G35: `direct_helper`, `direct_effects` and `root_retirement` are important lifetime controls, **not newly removed G38 allocations**.

## 2. Root authority, identity and failure proof

The existing packed local `Runtime::Slot` contains each eliminated payload's exact target-layout child offsets. Stack owner addresses are omitted from managed-pointer homes; every child is an actual current-heap reference or null. No second root manager, scanning protocol, collector, allocator, cache or public ABI entry is added.

Lowering stores an extracted child/result into its existing root home **before** clearing the owner's child fields. A selected helper has returned and its checked failure has been handled before retirement. Stores are not split by a safepoint. Child aliases that survive the owner keep their own roots; allocation eligibility and child lifetime are separate proofs. Return values retain their SSA roots through the existing return handoff. Failure paths retain the existing frame/cleanup path.

G35's must-dataflow resets each construction epoch and rejects access to a previous loop instance after backing reuse. Local-copy retirement additionally rejects read-before-store loop homes. Publishing a child into any owner still rejects that child's own stack elimination, so this does not create stack graphs or managed heap references to stack owners.

The original ordered initializer evaluation, field guards/stores, readonly authority, nullable checks and remaining arithmetic/collection errors stay. The accepted OOM contract remains: **only the eliminated construction's own OOM may disappear**; remaining child/buffer allocations, effects, failures, recovery and cleanup stay exercised.

The no-extra-root visibility probe captures a raw child address, forces collection before its required use, then checks `Runtime::contains`. Across Release baseline/candidate and fatal-sanitizer candidate, normal/O2 JIT and independent normal/optimized AOT, it performs **24,000 required-lifetime collection checks** with 1 MiB stacks. Required child payload stays **4 bytes**; the nested-use baseline retains **12 bytes** (holder + child), versus **4 bytes** in G39. The retirement witness separately requires zero heap objects before each next independent child birth. Clearing the payload alone failed that witness because hidden operand-copy locals still retained the child; indexed local/SSA copy retirement makes it pass.

## 3. Exact construction, native-request and GC evidence

Below: 20,000 iterations, observer-on optimized independent AOT, medians of three samples. “Source” counts executed struct-construction expressions, independently of elimination. Global-cell initialization is outside warmed samples. `outlined_array` is a separate control: zero struct expressions/constructor-ABI calls but **two retained managed buffers**. Raw source counts cover all 45 fixtures.

| Pattern | Source | G38 heap | G39 heap | G38 → G39 native C++ requests | G38 → G39 collections |
| --- | ---: | ---: | ---: | ---: | ---: |
| direct_reference | 40,000 | 40,000 | 20,000 | 41,560 → 0 | 156 → 78 |
| direct_factory | 40,000 | 40,000 | 20,000 | 41,248 → 0 | 156 → 78 |
| direct_nested | 60,000 | 60,000 | 40,000 | 82,652 → 40,000 | 234 → 156 |
| direct_nullable | 20,000 | 20,000 | 0 | 40,156 → 0 | 78 → 0 |
| readonly_field | 60,000 | 40,000 | 20,000 | 41,560 → 0 | 156 → 78 |
| helper_branch | 40,000 | 40,000 | 20,000 | 41,248 → 0 | 156 → 78 |
| helper_branch_write | 20,000 | 20,000 | 0 | 624 → 0 | 78 → 0 |
| root_retirement | 40,000 | 20,000 | 20,000 | 468 → 0 | 78 → 78 |
| child_survives | 40,000 | 40,000 | 20,000 | 41,248 → 156 | 156 → 78 |
| direct_identity | 60,000 | 60,000 | 60,000 | 82,652 → 80,936 | 234 → 234 |
| readonly_incoming | 60,000 | 60,000 | 60,000 | 82,340 → 82,184 | 234 → 234 |
| mutable_field | 60,000 | 40,000 | 40,000 | 41,560 → 40,000 | 156 → 156 |
| factory_field | 60,000 | 60,000 | 60,000 | 82,184 → 80,468 | 234 → 234 |
| loop_carried | 20,001 | 20,001 | 20,001 | 624 → 156 | 78 → 78 |

Zero warmed native requests do **not** mean zero heap constructions: retained children can use G34's existing reuse cache. Native-request counters cover the observed thread's C++ allocation family, not C/provider allocation or whole-process memory. All new-eligibility count fixtures are synchronous. No cross-thread total is inferred from thread-local observers.

| Pattern | Root words | GC Slot visits | Nonzero root words | Mark pending visits | Allocated heap payload after call |
| --- | ---: | ---: | ---: | ---: | ---: |
| direct_reference | 1,404 → 702 | 312 → 156 | 1,404 → 0 | 1,560 → 0 | 1,164 → 384 |
| root_retirement | 780 → 780 | 234 → 234 | 234 → 0 | 234 → 0 | 388 → 384 |
| readonly_field | 2,028 → 1,014 | 312 → 156 | 2,028 → 0 | 2,184 → 0 | 1,164 → 384 |
| child_survives | 1,404 → 702 | 468 → 234 | 1,248 → 78 | 1,404 → 78 | 1,164 → 388 |

The retirement-only case leaves descriptor words, Slot visits and collection frequency unchanged. It removes occupied words and marking work, not root-search capacity. Post-call allocated payload includes garbage awaiting collection; it is not a reachable-set peak. The forced-collection visibility/retirement probes provide the exact live-set boundaries above. Requested-memory observations are epoch-local; process RSS is reported separately.

All **180** baseline/candidate × normal/optimized observation-emitter objects are byte-identical to ordinary emission. LLVM pre/post IR and selected native disassembly are retained. Direct-reference constructor call sites fall 2→1; direct-nullable 1→0; readonly-field 2→1; branching scalar mutation 1→0. Controls retain construction and identity behavior. IR/probe evidence agrees on real activation.

## 4. Correctness and native qualification

- Baseline G38 focused escape suite: **5/5**. Final G39 Linux Release **82/82** and fatal ASan/UBSan/LeakSanitizer **82/82**, no suppressions or relaxed negatives. The maintained 45-pattern suite runs normal/O2 JIT and independently linked normal/optimized AOT, with exact results/counts, forced GC, cold allocation faults, recovery, remaining null/division failure, initializer ordering, nullable/nested/replaced references, object/function payload bounds, identities, alias rebinding, loop-carried references, readonly authority and helper bounds.
- Foreign-child controls cover the original incoming child, a direct incoming temporary, and a readonly child read through a foreign owner. All three require exact `prepared reference belongs to another heap` rejection in both independent AOT modes, including fatal sanitizer qualification. Incoming managed type is never treated as heap affinity.
- Relevant fatal TSan: Runtime coordination and checked collections **2/2**; all **45 patterns in both independent AOT modes**, plus allocator pressure and catalog/GC races. The TSan fixture leaves allocation replacement to compiler-rt; cold fault injection remains covered by Release and fatal ASan.
- Linux focused JIT/AOT suites also pass explicit **1,048,576-byte stacks**. The raw-address visibility probe uses that same limit in Release/fatal sanitizers. It catches lost roots even when cached payload contents might still look correct.
- Final native Windows **10 build 19045**, x64 VS2022 toolset **14.44.35207**, SDK **10.0.19041.0**, LLVM/Clang/LLD **23.1.4**: **Debug 67/67, Release 67/67**. Normal/O2 JIT and independent normal/optimized AOT, forced-GC/failure/foreign/cleanup/callback/reload/worker/async cancellation controls pass. Seven PE stack headers per profile retain **1 MiB**. Final Linux/Windows source exports match across **3,294 files**; protected installed providers/SDK files remain unchanged.
- The first Windows Release attempt passed 62/67; five Python fixtures could not import `test_output` through the task's provider-tools junction. Setting the task's `PYTHONPATH` to its packaged tools fixed the environment; the full final 67-test selections passed. No test assertion was weakened.

## 5. Observer-off performance

Four alternating balanced blocks, three warm samples/cell; independent arithmetic/domain oracles. Normal/O2 JIT and normal/optimized independent AOT are separate. No task compilation or qualification overlaps throughput sampling. Observer binaries/counts are a separate family. Raw timings, CPU/memory snapshots, all unfavorable cells and code identities are retained.

Percent changes below are elapsed-time changes (negative is faster), not a claim about arbitrary applications.

| Pattern | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| direct_reference | -58.1% | -58.3% | -57.4% | -55.8% |
| direct_factory | -55.1% | -51.8% | -51.6% | -51.7% |
| direct_nested | -34.7% | -33.2% | -37.9% | -36.9% |
| direct_nullable | -86.9% | -87.1% | -87.4% | -86.7% |
| readonly_field | -54.6% | -56.5% | -54.0% | -52.9% |
| helper_branch | -51.6% | -51.8% | -51.6% | -49.9% |
| helper_branch_write | -79.9% | -83.3% | -81.4% | -82.9% |
| root_retirement | -0.2% | -0.1% | +0.4% | +0.0% |
| child_survives | -46.1% | -45.2% | -45.8% | -44.6% |
| alias_rebind | +9.3% | +4.5% | +1.3% | +2.4% |
| loop_carried | +7.4% | +0.0% | +0.7% | +1.2% |
| public_helper | +5.5% | -1.4% | -2.6% | +2.5% |
| factory_field | +3.7% | +3.1% | -2.4% | +5.9% |

The retirement-only case is effectively flat in the main bundle despite materially less marking. Earlier clearing adds stores; fewer nonzero roots is not automatically faster.

The normal-AOT helper-limit bundle cell regressed **12.2%**. Six balanced blocks/five samples in naturally linked single-fixture images reduce that cell to **+0.1%**, but other adverse results remain:

| Standalone control | Normal AOT | Optimized AOT |
| --- | ---: | ---: |
| helper_block_limit | +0.1% | +3.1% |
| alias_rebind | -1.4% | +9.0% |
| loop_carried | +6.5% | -3.6% |
| public_helper | -1.1% | +7.6% |
| factory_field | +1.3% | +6.0% |
| direct_incoming | +2.3% | -0.2% |
| root_retirement | +6.0% | +6.8% |
| incoming_rebind | +3.5% | -1.7% |

`incoming_rebind` standalone images are byte-identical between variants: their +3.5%/−1.7% variation is not a compiler improvement/regression. Other controls change code and still show costs up to about **9%**. Standalone retirement-only costs about **6–7%**. No padding, forced alignment, suppression or benchmark-only optimization is introduced. These results keep general performance acceptance held; they are not erased by large positive cases.

The representative G31 arithmetic (integer/float/dependent), arrays, dictionaries, sorting, text, JSON and native-I/O optimized AOT executable is **byte-identical** (`efb6850f53116cc51228527675fe81e3b853444da21996a2815f74e9affe6bb4`). Four balanced blocks pass all 42 output rows per run in AOT and normal/O2 JIT. AOT medians move −2.4%…+1.1%; no speedup is attributed to unchanged code. Calls, closures, workers and async-task AOT objects are also byte-identical in both modes. Their raw mode-specific timing movements remain controls, not G39 work elimination. JIT host/context variation remains an inherited open issue.

## 6. GC intervals and representative memory work

Timer instrumentation begins once every mutator is safe and ends after coordination-lock release, covering root enumeration, marking, sweep, accounting and resumption. These are **observer-on full stopped-world intervals**, not isolated `Heap::collect` timings or observer-off throughput. Counters/allocation hooks perturb observed costs. No-GC cases have no pause distribution.

Optimized independent AOT, three observation samples:

| Pattern | Events G38/G39 | Median stopped µs | p99 stopped µs | Max stopped µs |
| --- | ---: | ---: | ---: | ---: |
| direct_reference | 468/234 | 7.60 → 7.10 | 14.79 → 11.15 | 20.65 → 12.64 |
| root_retirement | 234/234 | 7.32 → 7.08 | 13.61 → 12.49 | 27.66 → 60.88 |
| readonly_field | 468/234 | 7.70 → 6.91 | 10.90 → 7.25 | 15.57 → 8.84 |
| child_survives | 468/234 | 7.59 → 7.13 | 10.56 → 8.96 | 15.97 → 14.26 |
| incoming_rebind | 703/703 | 7.79 → 7.74 | 8.66 → 8.30 | 18.03 → 14.43 |

Retirement-only maximum pause is unfavorable (**27.66→60.88 µs**) despite lower median/p99 and no marking. This is retained, not presented as universal GC-tail improvement. Raw full latency and stopped samples accompany all 45 cases/modes. Representative array/sort/dictionary/text/JSON/native-I/O GC controls additionally record constructions, native requests, root/Slot/mark work, allocated payload and complete intervals with independent oracles; source reading and a byte-buffer setup are included in those GC control fixtures. Their code identities and all raw samples are retained. Historical G34 array/async/GC-tail, G35 JIT-control, G36 numeric/worker/JSON-tail and G38 compilation/JIT-context tradeoffs remain active.

All 16 representative GC-control AOT objects (eight workloads × two emission modes) are byte-identical between variants; their median construction/request/root/mark/collection/payload counts also match. The following optimized-AOT counts are therefore preservation controls, not G39 savings.

| Control | Heap constructions | C++ requests | Root words / Slots | Mark pending visits | Collections | Allocated payload after call B |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| array-4096 | 16 | 110 | 37 / 13 | 23 | 1 | 28,980 |
| sort-1024 | 46 | 299 | 84 / 25 | 43 | 1 | 46,500 |
| dict-1024 | 23 | 1,182 | 38 / 14 | 25 | 1 | 57,660 |
| text-256 | 1,385 | 8,381 | 883 / 283 | 1,295 | 5 | 29,968 |
| text-1024 | 5,237 | 37,744 | 2,093 / 666 | 9,507 | 12 | 74,670 |
| json-32 | 674 | 7,479 | 1,364 / 556 | 746 | 3 | 9,844 |
| json-128 | 2,504 | 48,259 | 3,256 / 1,460 | 3,629 | 6 | 30,121 |
| io-200 | 419 | 1,084 | 276 / 104 | 88 | 2 | 2,788 |

Identical-code text/1024 stopped median varies 351.51→239.51 µs and max 985.95→446.56 µs (36 events/variant); JSON/128 median varies 162.49→171.90 µs and max 214.32→222.29 µs (19 events/variant). Neither change is attributed to G39. These small samples do not establish tail quantiles; full raw intervals and sample counts remain available.

## 7. Compilation, startup and complexity

Requested scaling counts **all struct-construction expressions**: 100/500/2,000 total sites, half reference owners and half retained child sites. Three balanced blocks, independent result = site count/2. Kernel `wait4` records per-process CPU/RSS; explicit analysis/lowering/JIT timers separate phases. No unrelated task compilation overlaps. This is compile + JIT + minimal verified execution, not a throughput claim.

| Total sites | G38 → G39 process s | G38 → G39 peak RSS MiB | G38 → G39 lowering/O2 JIT s |
| --- | ---: | ---: | ---: |
| 100 | 0.216 → 0.109 | 51.9 → 49.6 | 0.205 → 0.099 |
| 500 | 0.697 → 0.538 | 80.1 → 66.5 | 0.667 → 0.512 |
| 2000 | 7.591 → 4.498 | 189.6 → 134.4 | 7.492 → 4.401 |

An additional **2,000-owner + 2,000-child = 4,000-total-site** stress case approaches the 16 KiB eliminated-payload cap: **35.16→21.84 s**, **337.5→225.7 MiB**. Earlier progress updates referred to owner-site counts; the requested table above uses total construction sites. Overall LLVM lowering/optimization remains strongly superlinear at large sizes. The unchanged frontend analysis is not claimed faster.

The compiler reuses one function-wide use/local-use/predecessor index and G35's CFG must-dataflow. Added shared facts are one construction-origin bit per local, uncertainty edges for readonly reads, bounded helper alias/taint sets, and one instruction-keyed retirement plan. Candidate walks still follow indexed aliases; there is no per-candidate whole-function instruction rescan. A shared operation predicate separates root-transfer proof from factory freshness. The old immediate-publication check is moved behind the bounded lifetime proof, not duplicated into another escape framework. Backend diff: 269 additions/64 deletions (205 net lines, including helper-body reindent). No Runtime metadata table is added.

Twenty balanced warm-filesystem startup pairs retain independent output checks:

| Startup path | G38 → G39 wall ms | G38 → G39 peak RSS MiB |
| --- | ---: | ---: |
| aot-normal | 2.34 → 2.29 | 15.0 → 15.0 |
| aot-optimized | 2.00 → 2.04 | 15.0 → 15.0 |
| jit-O2 | 21.81 → 21.01 | 45.9 → 45.9 |
| jit-normal | 19.31 → 19.04 | 44.0 → 43.8 |
| representative-aot | 2.91 → 2.84 | 15.0 → 15.0 |

These are warm-filesystem process starts; JIT rows include compilation. They do not establish cold-boot or release-startup improvement. The representative startup image is identical. Build preparation times overlapped qualification and are retained as receipts only, not performance authority.

## 8. Rejected proofs, limits and next action

Preserved failed evidence includes: payload-only retirement leaving a child live through hidden operand homes; the first overly broad readonly-owner affinity idea, narrowed to completed constructions; and a rejected factory proof that treated fresh return as sufficient for caller-root retirement. The latter is reproduced with the exact rejected backend and a raw liveness control; the final factory guard passes normal/O2 JIT and independent AOT. A mistaken first evidence oracle counted a helper-argument holder as a G38 heap allocation; G35 already eliminated that hidden owning-local case. Corrected counts distinguish that control from genuinely new direct-field eligibility.

Still unqualified: general direct-temporary CFG/branch-join payload retirement, arbitrary mutable-field/incoming/factory-derived owner provenance, multiple-origin owner elimination, stack graphs, broad interprocedural retention, unknown/public/virtual/native/callback/worker/async targets, and cleanup/resource-bearing functions. Conservative fallbacks remain. Parameter and many cross-block child homes can still retain references longer than a future stronger proof might require.

The permanent ledger preserves these objectives plus checked collection specialization, dictionary/sorting costs, text/Unicode/JSON materialization, SIMD beyond G38, native/JIT tradeoffs, GC-tail/startup costs and future macOS/release qualification.

**Specific next implementation:** use the existing use index/CFG facts to gate redundant-root clears on a reachable later safepoint and a useful retention/marking reduction, starting with parameter-derived duplicates and the standalone alias/helper/retirement controls. Requalify the observed 6–9% adverse cases before general default acceptance. Broader provenance should remain independent of that profitability correction.

## 9. Reproducibility, archive, cleanup and Git

Exact source patch/postimages, accepted preimages, source/compiler/tool/provider identities, fixtures/oracles, raw measurements, LLVM/native evidence, significant rejected sources, fatal-sanitizer/native receipts and replay instructions are selected into the DEFLATE-9 archive `/home/shmellyorc/Documents/Void.Language-archives/g39-managed-reference-lifetimes-2026-11-15.zip`. CRC, SHA-256 manifest, extracted hashes and restoration of accepted source plus the patch are verified **before task-only cleanup**. Final retained-file count, ZIP size/hash and separately observed host/guest reclamation are recorded in the [closeout receipt](/home/shmellyorc/Documents/Void.Language-archives/g39-managed-reference-lifetimes-2026-11-15-closeout.json). No original G37-F/G38 worktree/archive, installed SDK/provider or unrelated development file is deleted.

Review changes remain in the isolated worktree. The index is empty; no commits, pushes or hosted CI occur. Final [Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g39-managed-reference-lifetimes-2026-11-15-git-inventory.json) contains every changed file and SHA-256. Production changes consist of the LLVM backend, maintained escape tests/CMake, this report and the permanent ledger. Task build products/transports are removed only after archive/source verification; the review source tree remains.
