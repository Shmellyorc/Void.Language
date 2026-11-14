# G32-A — Checked Runtime boundary attribution

13 November 2026. Baseline `0407f83f9d6dd660b1503df1e68cdcee9b34b77b`; accepted G31-I behavior retained. **HOLD the disposable correction.** A redundant scalar-list schema lookup is proven, and removing it improves prepared-array reads. The same candidate has consistent AOT arithmetic-control regressions. It is not an acceptable integration recommendation from this evidence. This is a candidate verdict, not a new product release hold.

No production changes, staging, commits, branches, hosted CI, root cache, indexed iterator or new authority system. The [G32 report](G32_SHARED_RUNTIME_PERFORMANCE_ATTRIBUTION.md), including its rejected root-search experiment, remains unchanged. Golden Wrench reviews; Boss controls implementation and integration.

## 1. Exact repeated cost and ownership proof

`src/runtime/list.cpp:31–39`, `Runtime::list_header(RegisteredRootWord, ScalarEncoding)`, retrieves the scalar schema from **the current** `types_->schemas` at line 37. After root validation it calls `array.cpp:20–31`, `list_header(void*, ElementSchemaRef, string_view)`. That helper looks up the same schema identity and compares the same shared pointer again at line 22. This is one redundant catalog-membership lookup per scalar-list **count, version or indexed read**, including the extra null-catalog guard. It is not a duplicated root check or a claim that all collection validation is redundant.

The proof is local to one operation:

- All scalar helper callers (`list.cpp:42–48`) hold `mutex_` across both lookups, root validation and header inspection. `reference_source` does not poll, unlock, invoke native callbacks or change the catalog.
- Admission keys and schema identities correspond (`coordination.cpp:560–566`); admitted schemas are immutable. Catalog publication requires the same mutex. Another admission cannot invalidate the first lookup while this lock is held.
- `reference_source` still verifies Runtime/thread ownership, linked frame and nonreused frame identity, public-root authority, exact registration and descriptor, and the **current** rooted object (`buffer.cpp:234–246`). A token can survive a valid root rebinding; it cannot freeze that referent.
- Header type, backing completeness, schema, extent/capacity and selected-write version checks remain. No authority is retained across a safepoint or subsequent operation. Reads retain their current polling behavior; no safepoint is removed.

### Disposable correction tested

Only three files were overlaid read-only into private builds; the checkout was never patched:

1. `include/void/runtime.hpp:247`: declare one private `admitted_list_header` helper with a locked/current-catalog-membership precondition. No new field, public API, token or cache.
2. `src/runtime/array.cpp:20`: keep the original general helper's catalog checks, then delegate its unchanged header/backing validation body to that helper.
3. `src/runtime/list.cpp:39`: after the already successful current-catalog lookup and unchanged root validation, call the admitted helper directly.

The exact patch is archived as `evidence/candidate.patch`. Generic/schema-taking callers retain full catalog validation. Scalar writes, growth, dictionary algorithms, root discovery, error translation and synchronization are unchanged. Missing metadata precedes invalid encoding as before; invalid roots still precede invalid headers; bounds remain after header validation. A foreign-schema failure at the removed second check is unreachable for this locked scalar caller, not a newly suppressed legal failure.

The risk is maintaining this private precondition and the effect of extracting a shared hot helper on generated code/layout. Runtime, Frame and token sizes remain **1008 / 40 / 88 bytes** in both Release builds. This proof does not justify passing an old schema after unlocking or caching a borrowed native descriptor.

## 2. Source ABI versus existing validated paths

The public scalar ABI `fixed_abi.cpp:116–123` validates signedness, discovers a registered root and calls the checked Runtime read. The existing native token path skips repeated **discovery** but retains `reference_source` validation. Fixed-array ABI `fixed_abi.cpp:86–90` also obtains its layout; the native fixed-array control reuses both token and layout. Their difference is not exclusively root-search time.

Dictionary ABI `fixed_abi.cpp:144` discovers the root before `dictionary_read`. The existing token entry still runs `dictionary_layout` (`dictionary.cpp:24–47`), native-field/layout validation, key-policy validation, poll, root/header validation, hash/bucket search, output-authority check and copy (`dictionary.cpp:76–78`). Existing zero-capacity layouts and dictionary header metadata already reuse `collection_metadata_`; warmed scalar lookups need no DataCursor scratch allocation.

**Why dictionary validation cannot simply be hoisted:** the native descriptor is borrowed, not an immutable authority token. A disposable contract probe mutates a field at the same descriptor address; the next operation correctly rejects it, and restoration permits retry. A second probe admits a new forbidden-key policy into the Runtime catalog; an unchanged root token and key schema then correctly reject the next lookup. Root authority proves neither descriptor immutability nor unchanged key policy. `dictionary.cpp:13–22` scans the current nominal policy and schema leaves; this can be expensive, but skipping it on token identity alone is wrong. A same-catalog optimization would require its own proof; none is proposed or tested here.

### Uninstrumented operation timings

Linux x64 Ryzen 7 1700X, LLVM/Clang/LLD 23.1.4. Four balanced current/candidate blocks, five samples per process: 20 samples per cell. Native loops perform 16,384 operations per sample, with clocks outside the loop. Each packed frame has 3 or 128 root words; the selected list is last. Tables show medians in **ns per complete operation**, not additive exclusive phases.

| Operation | Roots | Current ns | Candidate ns | Change |
| --- | ---: | ---: | ---: | ---: |
| `root_admission` | 3 | 30.4 | 29.2 | -3.9% |
| `root_admission` | 128 | 84.0 | 84.1 | +0.1% |
| `list_source_abi` | 3 | 303.4 | 256.5 | -15.5% |
| `list_source_abi` | 128 | 372.9 | 331.1 | -11.2% |
| `list_existing_token` | 3 | 278.8 | 223.7 | -19.7% |
| `list_existing_token` | 128 | 280.3 | 234.9 | -16.2% |
| `fixed_source_abi` | 3 | 260.8 | 257.9 | -1.1% |
| `fixed_source_abi` | 128 | 317.8 | 319.8 | +0.6% |
| `fixed_existing_token_layout` | 3 | 80.3 | 78.3 | -2.6% |
| `fixed_existing_token_layout` | 128 | 80.0 | 80.0 | +0.1% |
| `dictionary_source_abi` | 3 | 2642.3 | 2625.7 | -0.6% |
| `dictionary_source_abi` | 128 | 2728.6 | 2736.6 | +0.3% |
| `dictionary_existing_token` | 3 | 2610.0 | 2612.1 | +0.1% |
| `dictionary_existing_token` | 128 | 2594.9 | 2604.0 | +0.3% |
| `dictionary_count_existing_token` | 3 | 2514.4 | 2477.1 | -1.5% |
| `dictionary_count_existing_token` | 128 | 2516.8 | 2512.7 | -0.2% |
| `native_key_metadata` | 3 | 173.6 | 175.0 | +0.8% |
| `native_key_metadata` | 128 | 176.1 | 173.6 | -1.4% |
| `native_entry_metadata` | 3 | 158.4 | 157.4 | -0.6% |
| `native_entry_metadata` | 128 | 159.4 | 156.9 | -1.6% |

The scalar ABI improvement is about **42–47 ns/read** in these controls, with existing-token improvements about **45–55 ns/read**. This is the controlled net effect of the candidate, including possible code-layout effects; it is not an exclusive stopwatch measurement of the removed map lookup. All four block comparisons improve scalar ABI reads: **8.3–20.2%**, depending on root count/block. Fixed-array and dictionary controls do not demonstrate a comparable improvement.

Dictionary count through an existing token costs roughly 2.5 µs even without key hashing/search/output copying; dictionary read is roughly 2.6 µs. This supports the importance of their shared metadata/header boundary, not a subtractable attribution of the policy scan alone. Entry and key metadata controls individually cost roughly 0.16–0.18 µs and overlap the full path. The prepared source graph contains **196 nominal types / 9 schemas** before custom fixtures; these costs are catalog/shape dependent. Native dictionary fixtures reuse the compiler's canonical i32/i32 entry identity rather than an artificially short schema key.

All counted warmed read/metadata batches performed **zero C++ allocation requests**. Counts ran separately from timed loops: 1,024 native operations or one prepared source invocation per count batch. This rules out native scratch allocation as the immediate explanation for these particular scalar reads, not for compound-key operations or collection growth. The raw scalar-memory reference is retained (~0.5 ns/op) only as a lower-level control; it has none of VOID's authority/error contracts and is not an equivalent implementation.

### Existing prepared source fixtures

The unchanged archived G32 fixture is compiled normally by the O2 JIT; setup and JIT compilation are outside traversal timers. Four invocations per sample, five samples per process, four balanced blocks. The host's allocation wrapper is inactive during timing and allocates nothing in these warmed paths; the separate complete-workload comparison below uses ordinary compiler/application executables without that wrapper.

| Complete prepared traversal | Current ms | Candidate ms | Change |
| --- | ---: | ---: | ---: |
| source_prepared_array_4096 | 1.800596 | 1.630853 | -9.4% |
| source_prepared_dictionary_1024 | 8.269634 | 8.308300 | +0.5% |

Array traversal improves in all four paired blocks (6.9–10.4%), about **0.170 ms per 4,096 reads** overall. Dictionary traversal is essentially unchanged (+0.5% median; block changes have both signs). Exact source checksums remain 256,528 and 1,571,328. Prior G32 found zero managed allocations/collections in these prepared traversals; this experiment separately confirms zero native allocation requests, without claiming new collector counters were installed.

## 3. Whole-workload regression result — why HOLD

The exact archived G31 source/oracles were reused: 14 workload/size cases, independent AOT, uncached O2 JIT and normal JIT, four balanced forward/reverse blocks, three timed samples per case. **1,008/1,008 timed output checks passed**. Warm timers exclude compilation/linking; file I/O retains all 200 writes, exact 15,000 bytes and final durable flush. This I/O case is a prepared-write control, not full logger FILE200.

Milliseconds, medians of 12 observations. Positive percentage means slower. Normal-JIT full values, per-block deltas, ranges, process CPU/RSS and every raw sample are archived.

| Workload | Current AOT | Candidate AOT | Change | Current O2 JIT | Candidate O2 JIT | Change | Normal JIT change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| array 1,024 | 1.1895 | 1.1926 | +0.3% | 1.2774 | 1.2420 | -2.8% | -5.4% |
| array 4,096 | 4.7217 | 4.7485 | +0.6% | 5.0226 | 4.9331 | -1.8% | -5.4% |
| dependent 1,000,000 | 30.9903 | 33.4621 | +8.0% | 37.2910 | 37.6173 | +0.9% | -2.1% |
| dict 1,024 | 9.9078 | 9.7705 | -1.4% | 12.9474 | 13.1526 | +1.6% | -0.1% |
| float 1,000,000 | 31.0824 | 33.4475 | +7.6% | 37.0956 | 37.3308 | +0.6% | -0.4% |
| integer 1,000,000 | 31.0534 | 34.2772 | +10.4% | 36.5905 | 37.5495 | +2.6% | -0.0% |
| integer 4,000,000 | 124.2935 | 135.1156 | +8.7% | 150.5497 | 150.3070 | -0.2% | -2.5% |
| io 200 | 6.9198 | 6.9532 | +0.5% | 7.1259 | 6.9165 | -2.9% | -2.0% |
| json 32 | 6.6811 | 6.5821 | -1.5% | 7.3632 | 6.9732 | -5.3% | -3.7% |
| json 128 | 42.4683 | 41.9193 | -1.3% | 48.1944 | 45.9771 | -4.6% | -1.8% |
| sort 256 | 6.2490 | 5.9856 | -4.2% | 6.5394 | 6.3753 | -2.5% | -6.5% |
| sort 1,024 | 30.9809 | 29.9889 | -3.2% | 32.0340 | 30.8612 | -3.7% | -6.2% |
| text 256 | 14.4807 | 14.2599 | -1.5% | 15.1416 | 14.7117 | -2.8% | -3.2% |
| text 1,024 | 59.7112 | 58.3383 | -2.3% | 63.6027 | 60.2266 | -5.3% | -4.4% |

Sorting improves across modes; text generally improves. But AOT dependent/float/large-integer controls regress **8.0% / 7.6% / 8.7%**, with all four paired-block medians slower for each. The smaller integer control regresses 10.4% overall and has one large baseline outlier. AOT whole-array traversal is nearly flat (+0.6%), unlike the prepared read-only probe; construction/iteration costs differ. O2 dictionary regresses 1.6%. No unfavorable samples were discarded, and there is no assertion that small deltas establish universal effects.

The arithmetic-loop regression mechanism is **UNKNOWN**. The full emitted suite object is byte-identical, SHA-256 `543984ac64f326f44d0eb42d352ea7b9470953bd20dcd4e050b75cab7c55a561` (2,871,816 bytes); the pure fixture object is also identical. `libvoid_core.a` and `libvoid_llvm.a` match. Runtime coordination, ABI, async and dictionary object files match; list/array objects differ. Linked `Runtime::poll`, `vd_gc_poll` and `vd_worker_checkpoint` addresses move by 224 bytes. Layout/alignment/branch effects are plausible; neither those addresses nor this desktop experiment prove a microarchitectural cause. The correction does not add a map lookup to integer arithmetic. Nevertheless, consistent matched regressions cannot be dismissed as harmless noise or accepted on the strength of the microbenchmark.

This experiment ends here. No second patch, layout tuning, repeated campaign to obtain favorable samples or broader compiler redesign was attempted. The measured result supports keeping G31-I and holding this candidate, rather than declaring the duplicated check unimportant or inventing a net speedup.

## 4. Correctness and failure controls

- Candidate Release: **8/8** focused CTests passed — Runtime coordination, buffers, schemas, failures, invariants, growable-array source, array source and dictionary source. The source suites exercise normal/O2 code paths.
- Candidate fatal ASan/UBSan with leak detection: **5/5** Runtime/root/schema/failure/invariant CTests passed. `halt_on_error=1`, ASan `abort_on_error=1`, leak detection enabled.
- Custom contract probe: **21 assertions** passed identically on baseline Release, candidate Release and candidate strict. Diagnostic text and Runtime/Frame/token sizes match. Controls include missing catalog/invalid encoding precedence; negative/minimum/bounds indices; descriptor mutation and frame expiry; null/untracked/wrong-type referents; structural-version invalidation; failed-write preservation; root rebinding; collection; existing ABI failure preservation; signedness precedence; borrowed dictionary metadata mutation/retry; later catalog-policy rejection; unrelated scalar access after admission.
- Native comparison: **960 measured batch rows** passed their value checks, with separately counted zero-allocation batches. Complete source comparison: **1,008 checks** passed; no abnormal source/compiler/runner exit in the final campaign.

No production root, cancellation, GC, catalog publication, lease, allocator, stack or concurrency contract changed. Existing coordination tests exercise managed/native rendezvous; this is not exhaustive schedule exploration or a TSan claim. Measured children and focused checks use CPU 2 affinity and actual soft/hard **1,048,576-byte stacks**. Builds finished before timed campaigns. No Windows/macOS qualification or Debug source campaign was launched; this Linux-rejected candidate does not justify them.

**Preserved harness failure:** the first test-only native allocation counter passed alignment 4 to `posix_memalign`, which requires a power-of-two multiple of pointer size. It failed with `bad_alloc` and LLVM's out-of-memory abort (exit -6). The exact `_crash.void`, failed C++ observer, stdout/stderr and receipt are archived. Normalizing only the observer's alignment to at least `sizeof(void*)` resolved that authoring defect. It is not a VOID or LLVM production repair. [Linux posix_memalign contract](https://man7.org/linux/man-pages/man3/posix_memalign.3.html). The first successful exploratory run used a shorter synthetic dictionary identity and is preserved separately; none of its timings are paired with the final candidate.

## 5. Secondary findings and implementation boundary

G32's FILE200/text/JSON allocation counts remain secondary evidence. Text/JSON construct thousands of objects; prepared scalar traversal does not. Full FILE200 previously counted 2,899 managed constructions, 200 snapshots, 200 transfers and one real `fsync`; its direct-write control is a different scope. This patch removes no logger allocations and no durable synchronization. The current whole-suite text/JSON results above cannot be added to old exclusive logger timings or used to promise FILE200 savings. No logger rerun or architecture change was needed for this boundary-only candidate.

**Engineering handoff:** do not integrate the archived patch. Retain the local proof and regression receipts. There is no justified dictionary cache, weaker root validation, new allocator or new authority API in this assignment. If Boss later authorizes further work on the proven scalar duplication, the necessary evidence is a correction that preserves the same lock/error-order proof **and** avoids the observed complete-workload regressions; this report does not mandate another research stage or claim such a correction exists. General helper extraction can affect unrelated linked hot paths and must not be judged only by indexed-read timing.

## 6. Reproduction, evidence and cleanup

The fresh baseline Runtime SHA-256 is `1850eb63413ee40aa17a7b555aa7c7097f0e96ef482ab4239fa530a57772afcc`, matching accepted G31-I/G32. Disposable candidate Runtime: `bbfe952878a3216a9ae3ef096a5cd5689af39bd7a9c77ee771fad343d9830511`. The stale active build was not measured or overwritten. `evidence/g32a-identities.json` records fresh binaries and source hashes; restored `evidence/identity.json` is explicitly historical G32 provenance, not this run's identity.

Selected archive: `/home/shmellyorc/Documents/Void.Language-archives/g32a-checked-boundary-attribution-2026-11-13.zip`. It retains the patch/three overlay files, probe sources, original G31/G32 fixtures and oracles, build/qualification/measurement drivers, raw observations, summaries, failure preservation, system/identity records and this report. Rebuildable executables, objects, build trees and caches are excluded. The prior G32 ZIP/hash remains untouched.

Reproduce by verifying the external archive index and embedded manifest, extracting into a new private root, then using `scripts/build.py current release`, `scripts/build.py candidate release`, and the recorded target/strict recipes. Candidate builds use read-only bwrap overlays of only the three selected files. `native-build.py`, `contracts-build.py`, `qualify.py`, `boundary-measure.py` and `performance-g32a.py` preserve exact invocation/order. Complete all builds and qualification before measurements. `performance-g32a.py` deliberately omits G32's separate cold/build campaign: compiler-startup optimization is outside this mission. No profiler or per-operation clock is used in primary timing.

Archive CRC, extracted member SHA-256 and rehashed originals are verified before task-owned scratch removal. Final archive hash and cleanup details are recorded in the external `archive-index.json`; the report remains uncommitted. No existing source, tests, build configuration, previous research report or Git staging is changed.
