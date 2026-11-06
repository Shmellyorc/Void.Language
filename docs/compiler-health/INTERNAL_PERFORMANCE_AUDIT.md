# Internal performance audit and engineering handoff

Research for Golden Wrench review, 5 November 2026. No implementation authorization is implied.

## Recommendation

Undertake one shared checked-access and tooling-storage repair. Prioritize repeated registered-root searches and transient collection metadata, then remove the tooling index's duplicate immutable records. Validate their combined effect on text, collections and JSON execution and on editor analysis. Keep the existing algorithms, collector, scheduler, source semantics and complete diagnostics.

There is compelling evidence of expensive library workloads and independently measured avoidable Runtime work. There is **not** an exclusive profile proving how much of the historical text or JSON time belongs to each Runtime operation. Do not turn the observations below into a promised speedup. No new timing gate or release hold is proposed.

The integrated logging refinement is separate work awaiting integration review. Its successful cleanup and snapshot changes are dependencies to reconcile, not another implementation assignment here. Its rejected automatic string/UTF-8 fusion must stay disabled. In particular, this report does not authorize another logger encoder, another scheduler or a new GC.

## Scope and provenance

This audit inspected the current working-tree production files, historical receipts and the isolated logging refinement. It did not run Git commands, build or execute programs, launch benchmarks, modify production or trigger CI. Only this report was added. Existing observations suffice to justify the shared repairs; missing attribution is explicitly assigned to a bounded measurement inside the engineering work, not another research mission.

The text, collections, JSON and Stage 7 latency tables predate the LLVM 23.1.4 migration. They establish historical user impact, not today's elapsed times. Current source still contains the specific access/conversion and graph-analysis paths identified below. The compiler-stabilization measurements supersede the original generated-DTO compilation findings. Historical failures and contradictory or slower measurements must remain intact.

Source line references describe the inspected working tree. Selected SHA-256 identities are recorded at the end so integration can distinguish it from the isolated logging candidate or later source.

## Priority by demonstrated user impact

| Rank | User-visible cost | Evidence and qualification | Engineering disposition |
|---|---|---|---|
| 1 | Bulk text/codecs and collection sorting take seconds on modest inputs | Text's 65,536-byte multi-operation workload was 6,687.960 ms Release O2. Sorting 16,384 numeric elements was 2,265.4–3,125.47 ms across distributions. Historical LLVM 19 runs, shared host, no exclusive CPU attribution. | Repair common checked-access overhead first. Preserve KMP, geometric growth, stable sorting and callbacks. |
| 2 | Large-graph edits and contextual assistance take hundreds of milliseconds | Stage 7 large-graph valid edit median 271.76 ms, completion 191.78 ms, signature help 186.72 ms. Idle RSS 129,584 KiB. Historical pipe measurements, not keystroke-to-screen latency. | Remove proven index duplication. Characterize remaining discovery/check/index phases before selecting semantic-incremental work. |
| 3 | Small JSON objects have substantial execution overhead | Historical 1,445-byte, 128-member parse totals 2,736.01 ms over 20 iterations, or 136.8005 ms/parse. Stringify totals 526.67 ms, or 26.3335 ms/call including DOM conversion. | Use JSON as a primary shared-Runtime acceptance workload. Preserve ordered duplicate checks and metering. Do not delete the clone by inspection alone. |
| 4 | Root lookup and metadata allocation amplify library loops | Root position sweep and allocation-count fixture independently demonstrate search and metadata churn. These are mechanisms underlying ranks 1 and 3, not additional end-to-end times. | Highest implementation priority because the remedy is reusable and comparatively bounded. |
| 5 | Repeated immutable concatenation and indexed Unicode scans have quadratic patterns | Confirmed source algorithms, but no current end-to-end timing for a representative concatenation workload. | Document and measure only if needed. No rope representation, general lifetime pass or builder rewrite in this assignment. |
| 6 | Long-lived async scopes still have some linear bookkeeping | Task lookup is already indexed and receive/drain use live-task lists. The repaired sweep short-circuit is present. No new consequential sequential-throughput defect is established. | No scheduler modification. Keep retention/ownership regressions in combined validation. |

Evidence sources are [text measurements](../text-encoding/evidence/PERFORMANCE.md), [collections measurements](../collections/BENCHMARKS.md), [JSON handoff](../json/HANDOFF.md), [Stage 7 acceptance](../stage7/ACCEPTANCE.md), and the [stabilization Runtime audit](stabilization/AUDIT.md). These workloads must not be added together into a fictitious application budget.

## 1. Text and collections

### Confirmed execution path

[The existing workload](../../tests/text_encoding/performance.void) repeats `a雪`, splits on `雪`, joins, replaces, hex-encodes and Base64-encodes. At 65,536 input bytes it requests 16,384 repetitions and returns 16,385 split fields, including the empty final field. Those independent string results are required output, not disposable overhead. The runner excludes compilation but times invocation and its checks. It records medians from three invocations, not retained individual timing samples.

The actual source supports the performance document's algorithm analysis:

| Source operation | Responsible source | Consequence |
|---|---|---|
| KMP prefix table and search | `stdlib/text.void:151`, `:168` | Linear input/needle work. Split and replacement advance their cursor; they do not restart at byte zero. |
| Construct each split field | `text.void:315` `_part` | Creates a byte slice, converts that slice through `string.from_utf8`, then appends a string. The byte slice/native snapshot are intermediate representations; the returned string is required. |
| Append a chunk | `text.void:385` `_append` | One checked indexed read and checked push per byte. Geometric list growth avoids accumulated-buffer quadratic copying, but does not make each source operation cheap. |
| Join | `text.void:406` | Performs a correct aggregate-size preflight, then creates UTF-8 lists for the separator and every item, appends bytes and decodes the completed list. |
| Repeat/replace | `text.void:437`, `:485` | Reuse `_append`, with geometric output and bounded sizes. |
| Hex/Base64 | `stdlib/encoding/hex.void:68`, `stdlib/encoding/base64.void:125` | Checked scalar reads, table accesses and pushes, then owned string conversion. No accumulated-string concatenation in these loops. |
| Stable sort | `stdlib/collections.void:66` | Bottom-up merge of index arrays, an initial complete outer snapshot and final value copies. It is O(n log n) comparisons, without recursion or per-pass payload snapshots. |
| Text comparator | `collections.void:46` `text_less` | Allocates two owned UTF-8 lists for each comparison. This is a concrete avoidable representation boundary, but its isolated cost has not been measured. |

The checked scalar path is `fixed_abi.cpp` → `root_word` → `list_read_index`/`list_header` or fixed-array validation → checked backing inspection. Writes additionally poll and preserve reader/version/publication checks. See `src/runtime/fixed_abi.cpp:107–127`, `list.cpp:32–65`, `array.cpp:16–29,137–165`, and `buffer.cpp:201–216`.

`array_insert` already doubles capacity and writes directly into available capacity. `list_write_bytes` already has checked in-capacity and geometric-growth paths. These are repaired behavior. Replacing them with another memory stream or exposing mutable backing is not justified.

The collections timing controls also matter. For 1,024 items, filter was 1.7985 ms versus ordinary-loop 1.7134 ms, map 2.0466 versus 1.9884, fold 1.3326 versus 1.1727. The controls omit callback increments, so these differences are not pure callback-dispatch costs. They do not support blaming callbacks for seconds of sorting. The four-value, 256 KiB fixed-value sort measured 453.439 ms, but independent fixed-value copies are language semantics. Removing those copies or changing comparator authority would be a behavior change.

### Attribution that can and cannot be made

Source proves scalar accesses, intermediate arrays, geometric copies, managed construction, root checks and potential collection. It does not assign milliseconds to them in the 6.688-second receipt. The intermediate slice and encoding paths are linear byte work, and producing thousands of independent results adds per-result overhead. Superlinear total timing alone does not prove a quadratic source algorithm or a collector defect.

The proposed shared metadata/root repair targets the repeated checks' search and allocation overhead, while retaining the checks. The logging candidate's existing bounded bulk `byte_array_bytes` snapshot should also be inherited when approved, because `string_decode_into` uses the same helper. Do not implement that copy repair twice or count its benefit as a new finding here.

A direct immutable UTF-8 comparator could eventually eliminate `text_less`'s two conversions while preserving unsigned-byte lexicographic order, NUL and prefix rules. There is currently only `vd_string_equal`, not an existing general ordering helper. A new private binding therefore has a real compiler/ABI cost. Keep it outside the default repair unless the bounded text-sort attribution identifies it as consequential after the shared changes. No changes to general comparator evaluation, fixed-value copies, snapshot timing or mutation detection are authorized by that observation.

## 2. JSON

The expensive 128-member measurement is dynamic `json.parse`, not compilation of a 128-field DTO. [The measurement driver](../../tests/json/measure.py) compiles before execution, warms parse/stringify, then times 20 calls each. Generated-DTO compilation was separately repaired. Current `src/frontend/json.inc:127–155,191–246` has shared selection/conversion bodies and the existing wide construction closure. Current `ValidatedAnalysis::lower` applies entry reachability after complete checking. Do not reopen the old minutes-long LLVM scheduling diagnosis.

Current costs are concrete:

- `stdlib/json.void:499` `_parse` makes an owned byte input and explicit frame stack. Scalar reads, `take`, whitespace, quoted decoding and numeric scanning repeatedly traverse the shared checked collection path.
- At `:538–544`, each new object name compares against all previous decoded names. A valid 128-member object performs **8,128 comparisons and 24,384 explicit charge calls in this duplicate-check loop alone**. Each comparison charges previous-name bytes, new-name bytes and one comparison in order. This is intentionally bounded quadratic validation. Its share of 136.8 ms is unknown.
- `_writer.byte`, `.append`, `.text`, `.quoted` at `:704–767` produce byte lists through checked source operations and maintain exact output/work limits. `_write` at `:770` uses explicit frames rather than recursion per JSON edge.
- `stringify` at `:1116` calls `_json_to_value` before `_write`. For an existing dynamic value, `src/frontend/json.inc:96` selects `_clone`. `_clone` at `json.void:912` constructs a fresh tree, copies outer arrays/members, charges limits, preserves offsets and traverses paths before writing.
- Typed conversion keeps additional intentional snapshots, declared-field order, unknown-field comparisons and exact conversion errors. The reduced generated shape does not remove these execution costs.

**Recommended treatment.** Optimize the shared access/metadata costs first and retain JSON's semantic algorithms. A dictionary replacing the duplicate scan would change ordered work exhaustion unless the old charging sequence were reproduced. Simply multiplying a total charge changes the failure location/precedence when the budget runs out midway. General dictionary machinery also has nontrivial metadata costs of its own.

Direct serialization of immutable DOMs is an established alternative. [serde_json's `Serialize for Value`](https://raw.githubusercontent.com/serde-rs/json/v1.0.145/src/value/ser.rs) traverses a borrowed value and delegates arrays/maps to the serializer. That establishes feasibility, not compatibility with VOID's conversion-before-writing budget contract.

For VOID, skipping `_clone` would require a read-only preflight with exactly the clone's node/depth/work/key/number/path/offset checks, followed by the existing writer using the same remaining context. Writing immediately would interleave output-limit errors ahead of existing conversion errors. Public `to_value` and `from_value` must continue producing fresh trees. Factoring a shared traversal and selecting an exact `json.value` stringify route also needs compiler-supported specialization, not an invented generic type test. This is a medium-to-high-complexity option with no measured clone-only saving. **Do not include clone deletion or a new native JSON engine in the selected engineering repair.**

## 3. Registered roots and Runtime metadata

### Strongest measured shared mechanism

`Runtime::require_output`, `src/runtime/coordination.cpp:198–205`, scans every frame, Slot and reference offset until it finds the exact address. `root_word` calls it through `require_buffer_output`. `reference_source`, `buffer.cpp:205–216`, checks the live frame chain, performs root lookup again and scans the originating frame again. `BufferBuilder::write_root` repeats validation around polling. The authority checks are necessary; repeatedly rediscovering the same registration is not inherently necessary.

The preserved first/last-root experiment has 5,000 queries per row. In the stabilization replay, 16,384 single-word Slots took 0.453967 seconds for last-slot fresh-token queries, versus 0.001094 seconds for first-slot queries. Reused last-slot tokens still took 0.310106 seconds. An earlier sweep recorded roughly 254 ns versus 82,879 ns per operation at one versus 16,384 last-position roots. These are different receipts, not matched speedups. Root width, position and source searches establish the mechanism without GC or LLVM being involved.

The existing [root/metadata fixture](../../tests/runtime/root_lookup_measure.cpp) separately measured:

- Frame entry/leave at 1,024 Slots around 1.506 µs, with zero allocations.
- Last-root token lookup around 1.071 µs at that width.
- Scalar layout acquisition around 175 ns with three C++ allocation requests/103 bytes.
- Ordinary fixed-array read around 308 ns with one allocation/31 bytes, versus direct fixed-layout read around 106 ns and zero allocations.

The current `fixed_scalar_layout(source, ...)` already reuses a nonempty buffer's immutable layout (`buffer.cpp:382–403`). The remaining scalar-key string construction is visible in `fixed_scalar_schema` and `list_header(ScalarEncoding)`. Do not claim the already removed per-read layout allocation remains on every fixed-array read.

Further metadata churn remains in `native_array_layout` (`array.cpp:30–77`), `buffer_layout` (`buffer.cpp:83–85`) and `dictionary_layout` (`dictionary.cpp:24–35`). Even admitted zero-extent geometry is repeatedly materialized. Dictionary count/read/enumeration enter `dictionary_layout`; temporary identity strings and the temporary reference-offset comparison vector are avoidable. Schema, nominal identity, field geometry, bounds and reader/version validation are not.

### Selected checked-access design

Use one registration-location helper shared by output lookup and tokens. Its result is a frame identity plus Slot/offset ordinals, never a cached managed referent.

Start with a small fixed positive-location cache per Frame, with a fixed small number of entries such as four. Traverse active frames in the existing newest-first order. Before accepting a cached entry, check the current Slot count, offset count, storage extent and exact computed word address. On any miss or changed descriptor, run the original search. Never cache negative results or accept mere containment in a frame's storage. Keep private-builder exclusion and heap-output-alias validation at their existing boundaries.

Carry the verified ordinals in `RegisteredRootWord` so its originating-frame membership check can validate the exact current descriptor directly. The active frame and thread/Runtime checks still apply. Give frames a lifetime identity that cannot silently wrap or be confused by stack-address reuse, and reject tokens from expired lifetimes. Re-read the word and verify the resulting heap object on every use, including after a safepoint. This is a lookup accelerator, not permission to retain an unchecked pointer across polling.

This choice avoids a heap allocation on every Frame entry and an unbounded global address registry. It improves repeated access to a small hot set. It **does not** promise constant-time arbitrary root lookup or remove scanning of deep frame chains. Mixed/random-root and nested-frame controls are mandatory so a last-slot microbenchmark cannot conceal cache thrashing. Additional Frame size must be accounted for at the real 1 MiB stack limit. If it loses on representative source execution, drop the cache rather than enlarge it indefinitely. The original checked search remains the fallback.

For metadata, use fixed scalar identity string views with the existing transparent catalog lookup. Preserve enum/geometry validation. Share the checked header routine so scalar and composite paths do not diverge. Reuse canonical zero-extent layouts by exact admitted schema identity within the Runtime/catalog lifetime. Do not cache arbitrary capacities, caller-owned `NativeArrayElement*` addresses, JIT memory or a previous Runtime's proof. Recheck incoming native field metadata, preserving malformed-ABI rejection. Nonempty fixed buffers retain their already implemented layout reuse.

Keep optional cache construction before publication and safely fall back to uncached construction if only cache insertion fails. Avoid a reference cycle from a schema to a layout that owns that same schema. Catalog replacement/admission must either rebuild or invalidate cached derived metadata atomically. No managed object, source callback or code-generation lease belongs in this cache.

Affected components are `include/void/runtime.hpp`, `include/void/heap.hpp` if catalog-owned layout storage is selected, `src/runtime/coordination.cpp`, `buffer.cpp`, `list.cpp`, `array.cpp`, `dictionary.cpp`, and relevant private ABI/frame sizing/cache identity checks. Do not weaken `fixed_abi.cpp` failure conversion. Complexity is medium, with high-consequence lifetime mistakes despite modest code volume.

The existing `RuntimeMetadata` immutable catalog, buffer-owned layouts and indexed heap membership are the primary reusable infrastructure. [.NET's RuntimeType cache](https://raw.githubusercontent.com/dotnet/runtime/v9.0.0/src/coreclr/System.Private.CoreLib/src/System/RuntimeType.CoreCLR.cs) provides an upstream example of associating reusable metadata with its owning type. It does not justify bypassing VOID's catalog or native descriptor checks.

### GC conclusion

The stabilization fixture measured idle polls around 11–12 ns, empty collection at 4,096 null words around 11.68 µs, and collection retaining 4,096 simple objects around 84.66 µs. These are controlled micro workloads, not a text/JSON GC profile. A DTO128 helper invocation made 391,906 root-thread C++ allocations, which is allocation churn rather than retained managed payload. The shared repair should remove repeated metadata construction before changing the collector.

`coordination.cpp:165–186` builds the actual root list and `Heap::collect` performs required tracing. A reusable scratch vector might save native allocations, but has no demonstrated dominant end-to-end cost here and introduces retained capacity and failure-path concerns. No collector redesign, changed allocation budget, reduced polling or blanket root-liveness pass is justified.

## 4. Async retention

The historical Stage 8 document explicitly points to later HTTP changes. Current source verifies those changes:

- `Scope::task_index` at `async.cpp:184` and `Scope::find` at `:485` replace task-address linear search.
- `receive`, `work_pending` and executor progress use the live-task list, not every completed task.
- `release_result_root` at `:416` retires Work and execution-frame roots after quiescence, keeping the adapter result rooted through host copying.
- `sweep_completed` at `:425` runs on collection epoch/change and short-circuits dependency searches once retention is established. Handle removal is indexed swap-pop (`coordination.cpp:405`), not a vector erase shifting all history.

[HTTP's preserved comparison](../net/http/PERFORMANCE.md) measured the 256-write pure-I/O regression at 1.282 seconds CPU, then 0.890 seconds after the short-circuit, with the later absent/idle/paused comparison at 0.837 seconds. These separate cohorts are not universal speedups. They directly contradict treating the old unconditional sweep as a current defect.

Remaining loops inspect live workers, child/group relationships, dependency cycles, creation-order failures and collectible native records. Retirement may search a sibling predecessor and sweep dependencies. Those costs can grow with retained task graphs, but there is no fresh long-lived sequential bottleneck demonstrated here. Keeping a source task alive intentionally preserves its result and repeated-await behavior. Unobserved failures and dependent/group records also have necessary lifetimes.

**No async production repair is justified.** Reuse the existing retention observer, HTTP pure-I/O comparison and repeated-await/address-reuse tests as regression controls for shared Runtime changes. Only a new measured workload with large retained history would justify redesigning the remaining searches.

## 5. Immutable strings

`concat_into` at `coordination.cpp:233` obtains both byte views and uses `string_parts_into`. That helper validates the UTF-8 inputs, allocates a complete native header/byte vector, copies both operands, then calls `construct_into`. String construction validates the complete payload and publishes a new managed leaf. The native scratch exists to own bytes before allocation/polling, preserve failure atomicity and avoid managed borrows crossing a collection boundary.

Repeated `s += chunk` with a fixed-size chunk copies prefixes of lengths k, 2k, …, nk, for Θ(kn²) cumulative byte work. A long unrolled binary concatenation chain has the same risk. This is separate from `text.repeat/join/replace`, whose source uses geometric byte lists. Interpolation already has `vd_string_join` (`abi.cpp:200`) using a native Builder; the rejected logging fusion is not evidence that interpolation is quadratic.

`string_count(..., false)` scans UTF-8 scalars; byte length is constant-time. `string_index_into` (`array.cpp:300`) counts scalars and scans to the selected scalar. Repeated indexed access across an entire long string is another quadratic-work pattern. Exact Unicode scalar indexing, negative indices and fresh scalar-string results remain required. The reviewed text/codec workload uses byte lists rather than that indexing pattern.

The backend's packed instruction homes remain function-duration roots (`backend.cpp:732–774`). Dead intermediate values can remain live until return, as [the string contract](../STRINGS.md) documents. A loop reuses its static instruction homes, so it does **not** automatically retain every previous iteration's string. Unrolled distinct expression sites can retain multiple large prefixes. Root retention, cumulative copying and allocator RSS are different measurements.

[Go's Builder](https://raw.githubusercontent.com/golang/go/go1.24.0/src/strings/builder.go) uses geometric byte storage and enforces noncopying of an initialized builder. Its final string shares backing through a trusted unsafe conversion. VOID can learn from avoiding repeated prefix construction, but cannot copy that aliasing rule onto independent writable lists. [Go's Join](https://raw.githubusercontent.com/golang/go/go1.24.0/src/strings/strings.go) also preflights output and grows the destination. VOID already uses aggregate preflight and geometric accumulation at source level.

No new string representation or broad lifetime pass belongs in the selected work. First remove repeated checked-access overhead. Any later literal-chain fusion must preserve intermediate overflow/allocation failures and operand evaluation before it can claim equivalence. The pending logging refinement explicitly restored ordinary join/encode because the fused path regressed important inputs.

## 6. Editor responsiveness and retained memory

`src/lsp/main.cpp:659` still drains bounded ordered edits before analysis, preserving request barriers. The repaired 20-edit burst fell from 4,800.57 ms and 660 notifications to 277.77 ms and 33 notifications. This repair is present and is not reopened.

Remaining work is visible in current source:

1. `ToolingSession::analyze`, `tooling.cpp:240`, calls `analyze_project` for each selected graph. `SourceGraph::load`, `compiler.cpp:5188–5228`, resolves, reads, parses and traverses imports. Semantic analysis remains whole-graph.
2. `ToolingSession::accept` computes `plan_invalidation` **after** the new analysis. Its comparison is useful authoritative metadata, but not an incremental checker. It cannot simply be used to skip checking on the next edit without a dependency proof.
3. `editor_assistance`, `compiler.cpp:6604–6669`, creates a cursor-repaired source and invokes the real compiler. Completion/signature-help requests repeat graph work. Rename and formatting deliberately revalidate their proposed graph and disk state (`main.cpp:369–470`).
4. `ToolingIndex::ToolingIndex`, `tooling.cpp:7–38`, retains its `ValidatedAnalysis` and also copies all ordinary/editor declaration and reference records into two additional vectors. These records contain strings and owning source metadata. The duplication is directly confirmed, but its share of the historical RSS increase is unmeasured.
5. `ValidatedAnalysis::lower_` owns the validated graph for later lowering. Session storage keeps the latest result and last successful result. They coincide on success; an invalid edit can intentionally keep a preceding successful graph. This is not proof of an unbounded graph leak.

**Selected repair:** make ToolingIndex index immutable records owned by its existing analysis. Use a lightweight combined view over ordinary/editor vectors, preserving order, identifiers and duplicate rules. Keep owning copies at outward query boundaries where callers retain results. Change `include/void/tooling.hpp`, `src/frontend/tooling.cpp` and the few consumers that assume a contiguous vector. Existing `shared_ptr<const ValidatedAnalysis>` supplies lifetime ownership. No raw pointer may outlive that owner. This is low-to-medium complexity with a directly demonstrable eliminated copy population, although the wall/RSS benefit remains unknown.

A bounded exact-snapshot assistance cache could avoid identical completion/signature probes, but it does not repair the first edit or a moving cursor. It would need disk-current checks, module/cursor/probe identity and refresh invalidation. Do not sell it as a single-edit latency solution.

Full semantic incrementality, dropping the retained graph, or caching checked mutable Units are not selected. Units are annotated/mutated by graph checking, so caching them as if they were immutable parsed syntax is unsafe. The appropriate later architecture would separate immutable syntax from semantic state and validate affected dependencies, including root-wide names, generics, constants, import changes and invalid graphs. Its complexity exceeds the available phase evidence.

[clangd's source](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/clang-tools-extra/clangd/TUScheduler.cpp#L150) demonstrates bounded AST ownership with `ASTCache` and explicit retention limits. Its [design](https://clangd.llvm.org/design/code) also separates AST/preamble reuse and request scheduling. This is an architectural reference, not a proposal to copy clangd's scheduler or allow stale VOID diagnostics. The source reference is explicitly the inspected 19.1.7 implementation, not a claim about an unverified 23.1.4 clangd file.

## One Keyboard Clanker assignment

After Golden Wrench review and Boss approval, implement **shared checked-access and immutable tooling-storage reuse** as one coherent development assignment. Do not create six module repair projects.

### Work to execute

1. Freeze the actual integration base and reconcile the approved logging cleanup/snapshot/private-write changes. Preserve its failed fusion experiments and do not re-enable fusion. Rebuild the existing measurement hosts against the actual headers/Runtime so old binaries cannot masquerade as the current baseline.
2. Capture one bounded current baseline with the existing root/metadata fixture, text workload, collections benchmark, JSON timing driver and Stage 7 graph/query harness. Preserve compiler/native-library/source identities, optimization, stack and GC settings. This is initial engineering characterization, not a separate research deliverable.
3. Implement the shared positive registration-location reuse and direct checked token ordinals, with frame-lifetime validation and the original search fallback. Preserve frame enumeration for GC and every heap/private-buffer check. Measure frame size as well as lookup time.
4. Remove transient scalar identity strings, reuse canonical admitted zero-extent layouts and remove avoidable temporary metadata comparison containers. Keep native metadata validation and errors in their original order. Do not add a process-global type cache or cache arbitrary capacities.
5. Replace ToolingIndex's duplicate declaration/reference ownership with immutable views into its owned analysis. Preserve outward API lifetimes and exact query results. Leave graph checking and source security preflights intact.
6. Run focused correctness throughout, then matched complete-workload comparisons and one combined review. Text, JSON and collections are primary beneficiaries of the shared Runtime work; they need not receive production edits to count as addressed. Async and strings remain compatibility controls. If shared changes do not materially improve a workload, report the residual cost instead of forcing an unrelated rewrite.

### Bounded attribution, only where it changes the decision

Reuse the existing host callbacks/counters and disposable measurement-host boundaries. Do not add permanent production instrumentation or trace the user's desktop processes.

For text, split invocation into repeat, split, join, replace, hex and Base64 intervals in a test-only counterpart, checking the exact original outputs. In a separate attributed run, count native allocation requests, managed constructions, collections/root words, scalar accesses, metadata acquisitions/cache misses, root-search examined offsets and bytes copied. Use exclusive CPU intervals for metadata lookup, registration search, payload work and GC. Attribute child time once. Compare against the unchanged complete workload with observation off.

For JSON, use the existing four corpora. Separate parse from stringify as today, then measure duplicate checking and clone-before-write in the observation build only. Preserve all charges. No clone removal is approved by a measurement showing that clones cost something. For LSP, split one edit into graph discovery/read, parse/check, analysis metadata/index construction, invalidation comparison and response serialization. Record live owned record counts and idle/peak RSS separately. The result determines how much the selected index-storage change actually addresses; it does not automatically authorize incremental semantics.

Use existing small/medium/large sizes and a bounded alternating baseline/candidate schedule, such as ABBA with three observations per cell, keeping every raw sample and failed attempt. Exclude compilation only for execution workloads, and report compilation separately. No retries until a favorable median, no comparison of unrelated historical medians. Measurements are evidence for retain/revert decisions, not newly invented release gates.

### Combined correctness and compatibility

| Boundary | Focused verification |
|---|---|
| Root authority | Existing `runtime_coordination`, `gc_buffers`, `runtime_failures`, `runtime_invariants` and schema tests. Add cached-hit/miss, unsorted/duplicate offsets, changed descriptors, nested overlapping registration, foreign Runtime/thread, expired token, same-address frame reuse, private builder, leased interior alias and allocation-failure cases. |
| Lookup performance | Extend `tests/runtime/root_lookup_measure.cpp` with packed/many-Slot first/last/random/hot-set queries, shallow/deep frames and nested temporary roots. Count examined offsets and all allocations. Include Frame entry/leave to expose costs shifted into setup. |
| Metadata | Every scalar encoding, empty/nonempty fixed and growable arrays, invalid native field/layout inputs, nominal identity, dictionary ordering/version/reader checks, fresh Runtime and catalog admission/reload. No metadata cache retaining executable code or source objects. |
| Library execution | Existing text/codecs oracle, depth/ownership/large inputs; collections stable ordering, exact callback count/evaluation, callback mutation/reentrancy, readonly and fixed-value independence; JSON grammar, numbers, schema, bounds, metering and ownership. Preserve exact typed errors, offsets and first-error order. |
| GC/lifetimes | Forced GC and managed/native allocation failures, post-failure retry, balanced roots/handles, no partial publication. Keep allocation budgets unchanged. C++ allocation counts and managed allocation counts remain separate. |
| Async | Existing source/worker/external/native-job retention, repeated await, cancellation and quiescence tests. No repaired HTTP sweep regression or Task/Work/Handle growth introduced by metadata/root reuse. |
| Tooling | Existing tooling/LSP graph and editor tests, unsaved overlays, invalid/repaired source, UTF-16, stale accept rejection, disk refresh, removal/rename, old snapshot lifetime and exact completion/hover/reference results. Repeated valid/invalid edits must not accumulate index-owned records. |
| Execution modes | Normal/O2 JIT and independently linked AOT; relevant Debug, Release and strict sanitizer coverage; actual hard/soft 1 MiB stacks. Update private ABI/cache identities if Frame/token/backend layout changes. Preserve reload/generation contracts. |
| Integration | Reconcile the pending logging candidate and run its focused behavior/performance controls once against the integrated result. Preserve 200 acknowledgements, bytes, ordering, partial errors and final durability. Native Windows/macOS validation remains explicit where this local Linux work cannot establish it. Boss controls hosted CI. |

Acceptance requires correct outputs and failure semantics, demonstrated removal of the selected repeated searches/allocations or duplicate records, and matched results showing where complete workloads improve or regress. Do not require an invented universal percentage. Revert any component whose overhead outweighs its benefit, preserve its measurements, and retain independently justified components. No release decision is delegated to a benchmark.

### Explicit exclusions and remaining uncertainty

- No changes to `io/memory` growth, queued-edit collapsing, repaired HTTP sweep, generated DTO stabilization, logging fusion or final synchronization.
- No JSON duplicate-policy/work-budget change or direct-DOM stringify shortcut.
- No dropped precise roots, unregistered borrowed spans, unbounded cache, reduced collection frequency or broad compiler liveness framework.
- No sort engine replacement, fixed-array aliasing, callback reordering or hidden list mutation.
- No claim that 6.688 seconds, 136.8 ms or 271.76 ms is the current LLVM 23 production timing. No asserted percentage attributable to GC, roots or copies without exclusive measurements.
- No evidence currently justifies a new async scheduler repair, universal immutable-string representation change or whole-graph incremental checker.

The largest unresolved question is the **current end-to-end share of shared Runtime access overhead**. The proposed bounded engineering characterization answers that while implementing the justified common repairs. It must not become a chain of fresh research assignments or an excuse to erase unfavorable results.

## Inspected production identities

| File | SHA-256 |
|---|---|
| `src/runtime/coordination.cpp` | `5f88462fd43ebaec4a5be6fbac439ade66fb92e5a3dca31b6cf84ce441718e4c` |
| `src/runtime/buffer.cpp` | `1efdcbc0c981307fbf5ea4236838f7b2d4f552c2e623586e77d8d8f056e939fc` |
| `src/runtime/array.cpp` | `807848a104157e696614e1ebf01ea8191a5937976cedf32155208ae3ede5d4b8` |
| `src/runtime/list.cpp` | `d77090772e5c6b74ef32ebc9ac9e56e5b183b2975b4b591062386abafefba1e8` |
| `src/runtime/dictionary.cpp` | `baeebadbe9dea5664740fde3e0c5c44b7977dd0242f69fd7b98e137e1fe3b80f` |
| `src/runtime/async.cpp` | `e29616fc3c69306f700978052d2b8b1dc3bc1f6b286b87c62e54126f6b4974d0` |
| `src/frontend/compiler.cpp` | `d82300523b87aa6e7a1a5d050372393448c008b760853faf5faa3b3885db4d4c` |
| `src/frontend/json.inc` | `e7b9fbf95b897b386417d1b12abe4351e7036032c8d6da57eec5668d7aaacb22` |
| `src/frontend/tooling.cpp` | `7bae9162ea7aab48d88447e978f8d309e3779f6149c276de3f42ac3906d8fdbb` |
| `src/backend/llvm/backend.cpp` | `a147a2e22641ddb754d37cd369b1fe52fa8a77c260b76a5a9db5df07335bf493` |
| `stdlib/text.void` | `cf30884f16703ff8dad4f80ab13618633d90a399212fddf3077ed393c7603e19` |
| `stdlib/json.void` | `94ab0a6fb8856ddd20209727341a8336f048fdf430fa3dace045d8eb44340857` |
| `stdlib/collections.void` | `72c5fd2cbcacb26cf656d64f35ea16dfbc2b577ce6faa7b5ca67d32fb4fe0fa4` |
| `src/lsp/main.cpp` | `e725f9a854a930a367390bcd5597227ee4eca6f969e8907a9f4eb6bf6a0158c2` |

Stop for Golden Wrench review and Boss authorization. No integration or release action was performed.
