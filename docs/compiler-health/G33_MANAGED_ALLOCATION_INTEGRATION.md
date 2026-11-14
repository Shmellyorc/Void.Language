# G33-I — managed allocation elimination

Starting commit: `0407f83f9d6dd660b1503df1e68cdcee9b34b77b` (accepted G31-I). Owner authorization: controlled production integration of Experiment A only. **A integrated and qualified on Linux Release/fatal strict and native Windows 10 Debug/Release, 2026-11-14.** No staging, commits, pushes or hosted CI.

## Implementation and proof

`src/backend/llvm/backend.cpp` now uses the archived G33-X A mechanism. `block_allocation_elision` owns the proof. Existing constructor staging becomes the object storage; eligible construction and local/load alias root homes are omitted. Ineligible objects retain the original managed allocation/publication/failure path. No Runtime/header/allocator/GC/protocol changes and no B, C, D2 or E integration.

The Research archive SHA-256 is `e550da875ee3efeca15962ca603eee5b8f6a0ba58966280d0b99dbfed2c41a33`. Its `linux/evidence/A.patch` SHA-256 is `64fce93b1edf27a1a8a905d689b1fe61a1c33f8340aec1fdaa60a3e1142288de`. Selected recovered members were checked against the archive manifest. The archived proof predicates/scan and lowering are verified token-equal after helper extraction and formatting. Other experiments' patches were reverified and remain in the original archive.

Eligibility remains concrete `gc_construct`, scalar-only fields, one synchronous block, exact-typed private local aliases, at most 256 layout bytes/object and 16,384 accepted layout bytes/function. Native resources, tasks, callable/interface/inherited objects, construction capabilities, async/receiver/initializer functions and functions containing cleanup/release operations are excluded. No reference graph or interprocedural escape inference is added.

The forward scan checks arguments, operands and receivers; only local stores/loads and scalar field access can use the reference. A whole-function local scan rejects earlier and cross-block accesses, replacement aliases and receiver uses. Branch/return publication is rejected. Thus the same entry-block staging slot can be reused on loop revisits without exposing a prior instance. Unsupported pointer, identity, capture, native/callback, collection, method and cross-edge aliases retain allocation. Typed IR validation precedes the proof.

Constructor zeroing, alignment, field evaluation/stores, initializer effects and completeness guards remain before the bypass. Only the eliminated allocation's own OOM possibility disappears. Remaining array allocation, arithmetic, cleanup, native and startup failures remain. Stack payloads contain no references, and no accepted address is published as a heap/root reference. Existing unrelated roots, frame entry/leave, polls, worker checkpoints and failure exits remain. The detailed review is retained with the evidence.

Research's conservative per-candidate scans and ordered sets remain. The proof can reject harmless operand-index coincidences. The storage caps and representative compilation timings do not establish arbitrary large-function scalability. Broader escape analysis remains separate work in the [implementation ledger](G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md).

## Maintained qualification

`tests/allocation_elimination` carries Research's local/direct/alias/effects/numeric/initializer/escape/reference/cross-edge/mixed fixtures. A shared harness checks exact object-construction counts, outputs, forced collection, root descriptors/publication, OOM, reclamation and recovery in normal/O2 JIT and independently emitted normal/optimized AOT objects linked only to Runtime. Additional JIT controls cover identity, field publication, captured closures, alias replacement, previous loop instances, the 256-byte and 16 KiB limits, and remaining division failure. The mixed case independently injects a remaining array failure.

The probe counts `vd_gc_construct[_typed]`, not buffer allocations. For 1,000 iterations, baseline local/direct/alias/effects/numeric/initializer each construct 1,000 objects; integration constructs zero. Escape/reference/cross-edge still construct 1,000. Mixed object constructions fall 1,000→0 while array allocation and its failure remain. Research's broader managed-allocation observer counts (60,000→40,000 in the 20,000-iteration mixed fixture) remain historical evidence, not new probe measurements.

| Host/profile | Baseline | Integrated | Evidence |
| --- | --- | --- | --- |
| Linux Release | 13/13 | 19/19 distinct tests | Maintained 13 plus allocation JIT/AOT, advanced cleanup, and two cache-process tests |
| Linux fatal ASan/UBSan + leaks | Reused verified G33-X baseline evidence | 17/17 fresh | Maintained 13 plus allocation JIT/AOT and advanced cleanup |
| Native Windows 10 Release | 13/13 fresh | 16/16 fresh | Maintained 13 plus allocation JIT/AOT |
| Native Windows 10 Debug | 13/13 fresh | 16/16 fresh | Maintained 13 plus allocation JIT/AOT |

Linux runs enforce a real 1 MiB stack; worker controls use CPUs 2–5, other timings CPU 2. Strict uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Windows is build 19045, x64 MSVC 14.44.35207, SDK 10.0.19041.0 and physical LLVM/LLD 23.1.4, with the existing verified Release/Debug providers. Twelve inspected PE runners reserve exactly 1,048,576 bytes and commit 4,096 bytes. Windows final source hashes match all 16 integrated source/build/test paths. No hosted CI or macOS qualification claim.

The maintained selections include Runtime coordination/generated code/callbacks/failures/invariants, GC buffers/schemas, managed source, native workers and async source/parallel/lifecycle/external work. The benchmark corpus adds numeric, collections, sort, dictionary, text/encoding, JSON and checked I/O applications. Every oracle passed in all modes. Callback closures, alias visibility, signed-zero/NaN/wraparound, initializer effects, cleanup and remaining faults retain their results.

## Emitted code and identities

The fresh Release Runtime is SHA-256 `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`, exactly accepted G31-I. No Runtime source changed. Normal JIT, O2 JIT and AOT IR each change the eligible construction call count 1→0. Escaping IR is byte-identical in every captured mode. Root inspection never publishes a staging pointer as a heap root. Frame entry, polls and worker checkpoints remain; the two disappearing static frame-leave sites belong to removed allocation-failure exits, not D's empty-frame omission.

IR-observer object output is byte-identical to ordinary production emission. The independently linked new AOT consumers have no LLVM/ORC dependency. The complete G31 executable is byte-identical between variants (SHA-256 `cd151fb343b853eb2a9e4bf1f075804edaf62277908216e67a84e21faf4b0c00`); four recompile/relinks per variant reproduce it. This corpus has no eligible A site. Its timing movements are controls, not allocation-elimination gains.

Eligible object size is 6,888→5,608 bytes; escaping object size remains 8,960 bytes. Whole compiler file size is 80,553,408→80,560,792 bytes (+7,384); these are whole-file sizes. Exact identities, IR, disassembly and linkage receipts are archived.

## Matched execution

Four alternating blocks, three warm samples/case/variant (12 samples each), with compilation excluded from compute timers. Source probes use 20,000 iterations, except 1,000 sequential async tasks. Independent AOT timers bracket compute. All raw CPU, wall, RSS, collections and output records are retained; medians are local observations, not confidence intervals. Builds completed before measurements.

| Source case | Normal JIT ms | O2 JIT ms | AOT ms |
| --- | --- | --- | --- |
| async-tasks | 1.647→1.675 (+1.7%) | 1.679→1.637 (-2.5%) | — |
| calls | 1.369→1.359 (-0.7%) | 1.333→1.361 (+2.1%) | 1.446→1.421 (-1.7%) |
| closures | 1.669→1.656 (-0.7%) | 1.645→1.560 (-5.2%) | — |
| managed-across-edge | 6.494→6.468 (-0.4%) | 6.656→6.590 (-1.0%) | — |
| managed-escape | 7.309→7.414 (+1.4%) | 7.099→7.254 (+2.2%) | 9.618→9.489 (-1.3%) |
| managed-local | 6.101→0.747 (-87.7%) | 6.153→0.746 (-87.9%) | 8.095→0.661 (-91.8%) |
| managed-reference | 8.563→8.580 (+0.2%) | 8.552→8.359 (-2.3%) | — |
| managed-survives-allocation | 47.288→40.834 (-13.6%) | 47.570→40.434 (-15.0%) | 52.123→43.640 (-16.3%) |
| workers | 0.876→0.836 (-4.6%) | 0.879→0.868 (-1.3%) | — |

The real managed-local gain accompanies 1,000→0 observed object constructions and removed allocation calls. The mixed case improves while retaining its array allocations/failures. Small movements in noneligible cases are not claimed as optimization benefits.

### G31 corpus controls

Percent change (negative is faster). The AOT executable is identical.

| Case | Normal JIT | O2 JIT | AOT |
| --- | --- | --- | --- |
| array/1024 | -1.0% | +4.2% | -3.4% |
| array/4096 | -1.3% | +4.4% | -2.5% |
| dependent/1000000 | +2.2% | -0.9% | +0.1% |
| dict/1024 | +0.1% | +1.2% | -0.0% |
| float/1000000 | +1.4% | -2.4% | -0.1% |
| integer/1000000 | +1.8% | -0.3% | -0.3% |
| integer/4000000 | +1.7% | -0.4% | -0.1% |
| io/200 | +0.1% | +2.2% | +0.3% |
| json/128 | +1.1% | -0.0% | +0.3% |
| json/32 | +2.4% | -0.3% | +1.1% |
| sort/1024 | +0.6% | +2.6% | -0.7% |
| sort/256 | +0.2% | +1.3% | -0.7% |
| text/1024 | -1.4% | +3.5% | -1.3% |
| text/256 | -4.4% | +3.1% | -1.3% |

### Regression investigation

Primary O2 array/4096 moved +4.4%, text/1024 +3.5% and sort/1024 +2.6%. These unfavorable results are retained. A bounded second four-block comparison, using the same binaries and oracles, produced:

| O2 control | Baseline ms | Integrated ms | Change |
| --- | --- | --- | --- |
| array/1024 | 1.250 | 1.286 | +2.8% |
| array/4096 | 4.943 | 5.018 | +1.5% |
| text/256 | 14.554 | 14.875 | +2.2% |
| text/1024 | 61.069 | 61.972 | +1.5% |
| sort/1024 | 31.586 | 32.800 | +3.8% |

The complete optimized G31 JIT IR is byte-identical (SHA-256 `4141d4938861ed16b0d2aa91229c94bf6f28005ebc4d4fd3322048d743add5b3`); worker and async-task optimized IR are also byte-identical. The Runtime archive is unchanged, but native Runtime ABI addresses in the two JIT host images move by `0x1b40` because their compiler code differs. Compilation also has a different temporary allocation history. These are concrete remaining placement/process-state differences; the specific cache, allocator or scheduling cause of the small JIT shifts is **UNKNOWN**. Identical IR does not establish zero timing cost in every JIT process. No eligibility, Runtime policy, alignment or protocol was changed to hide the result.

The primary O2 worker median is 0.879→0.868 ms, but the first integrated process has three 2.57–2.61 ms samples. Later blocks are around 0.8–0.9 ms, with another 1.62 ms sample. All are retained; a median alone does not describe that variability. No broad worker speedup is claimed. The source/IR/functional controls establish unchanged semantics, not a guarantee against machine-local timing variation.

The integration provides the large targeted allocation-elimination gain with small residual unfavorable JIT controls (1.5–3.8% on the bounded repeat), no G31 AOT code change and no semantic regressions. This is a bounded A qualification, not a claim of universal performance improvement.

## Compilation and startup

Four fresh processes/profile/variant. Cold AOT includes bootstrap; cold JIT includes frontend/LLVM compilation with disk cache disabled. Compile+link is separate from compute timing.

| Operation | Baseline ms | Integrated ms | Change |
| --- | --- | --- | --- |
| aot | 5.043 | 4.968 | -1.5% |
| jit-normal | 3046.958 | 3056.171 | +0.3% |
| jit-O2 | 3753.060 | 3748.067 | -0.1% |
| compile+link | 3609.473 | 3620.356 | +0.3% |

These measurements do not isolate proof-analysis cost or establish large-function scalability. The 65 × 256-byte maintained boundary fixture verifies exactly one remaining heap construction after the 16 KiB budget, under the same 1 MiB native stack limit.

## Failures retained and corrected

The new exact-count tests caught stale baseline backend reuse caused by overlay/source timestamps; the backend and every consumer were rebuilt before accepted measurements. New fixture setup corrections covered buffer-versus-object count ownership, source array diagnostics, forbidden boolean-to-numeric conversion, three captured-object constructions (box/cell/environment), and the reserved `f32` field spelling. Windows setup retained and corrected the developer-environment launcher, missing packet LICENSE and known-overlay/junction resume checks; its first added JIT test used an earlier fixture executable and was rebuilt from the final source. Initial source, failed outputs, corrections and successful replay are retained. No compiler eligibility or source semantics was changed in response. No tests were disabled or samples discarded.


## Evidence and storage

Evidence archive: `/home/shmellyorc/Documents/Void.Language-archives/g33-managed-allocation-integration-2026-11-14.zip`. The exact source diff is also retained as `g33-managed-allocation-integration-2026-11-14.patch` beside it. The ZIP retains inputs/oracles, compact build recipes, identities, essential raw results, IR/disassembly, failures, proof review and replay instructions. Routine object files, caches, SDK/provider copies and complete build trees are excluded. The original G33-X archive and active toolchains/providers remain intact.

This is the pre-cleanup archive snapshot. DEFLATE-9 manifest/CRC/member-SHA/source-rehash verification precedes final task-only cleanup; the external verification and cleanup receipts and the local closeout below record the resulting count, size and observed disk recovery.

## Git inventory

The original uncommitted reports remain part of this handoff:

- `docs/compiler-health/G32_CHECKED_BOUNDARY_ATTRIBUTION.md`
- `docs/compiler-health/G32_SHARED_RUNTIME_PERFORMANCE_ATTRIBUTION.md`
- `docs/compiler-health/G33_PERFORMANCE_EXPERIMENTS.md`
- `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md` (updated for integration)

Production changes are `CMakeLists.txt`, `src/backend/llvm/backend.cpp` and the new `tests/allocation_elimination` files. This report is also new. The exact final status and hashes are retained with the archive; nothing is staged or committed.

## Verified cleanup closeout

Retained **230 selected evidence files** (231 ZIP members including the manifest); ZIP **1,783,836 bytes**, DEFLATE-9. SHA-256 `a26e43ec8ad170233b776e4826ac6a3f52c604a3c075d0f6878204ae9cfad5b1`. CRC, extracted-member SHA-256 and source rehash passed before cleanup. The ZIP contains the pre-cleanup report snapshot; this closeout and external receipts record subsequent cleanup.

Observed free-space recovery: **3.56 GiB Linux** (3,827,113,984 bytes), **35.9 MiB Windows** (37,613,568 bytes) from its final source root. Windows Release/Debug build groups had already been removed after their identities/results were recorded. The filesystems are separate; these values are not summed. Linux task root and transport and the Windows private task root are absent, with no remaining open consumers found before deletion. Critical active LLVM/toolchain/provider hashes and the original Windows helper were checked unchanged. The original G33-X archive remains unchanged.

The ZIP has nine small external sidecars: exact source patch, ZIP hash, verification, pre-cleanup/cleanup/Windows cleanup receipts, the two cleanup scripts and final Git/closeout receipt. Their identities are indexed in `archive-index.json`. Routine intermediate/cache/build files were not retained. Exact final source/Git identities and prior-report preservation are in `g33-managed-allocation-integration-2026-11-14-final.json`.

A is integrated and qualified within the stated scope. Small unfavorable JIT controls and the unresolved timing cause remain documented. B/C/D2/E and broader escape-analysis work remain recoverable and active in the ledger. No staging, commits, pushes or CI dispatch occurred.
