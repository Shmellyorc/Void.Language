# Logging hot-path architecture review — 28 October 2026

## Architecture approved; implementation blocked by reproduced crash

Boss approved `text.translate_scalars` with mandatory refinements. That approval
remains valid. Its section 1 also requires stopping if the experimental SIGSEGV
cannot be isolated sufficiently to exclude a relevant production defect.

The original executable and all 61 captured source files were hash-verified.
The crash **reproduced on the third original-command O2 run**, exit -11, with
empty stdout/stderr. The new original-binary incident is `incident-tg6xt8tp`;
`incident-vs4b96zy` remains unchanged. Two phase-instrumented O2 runs, one normal
run, five original-binary signal-preload runs, strict ASan/UBSan/leak normal/O2,
and three preload runs without unwinder prewarm all passed. Across all 16
invocations: **15 passes, one signal**. None of the diagnostic variants captured
a failing stack. Passing controls do not resolve the original defect.

The fault remains unclassified among harness, generated prototype code and
compiler/Runtime; a relevant production correctness defect cannot be excluded.
**Implementation stops under the explicit section 1 condition.** No production
or repository test source changed; the suspect flat builder was only diagnostic
input. The approved text operation and logger integration have not started.
No new full correctness matrix or performance acceptance is claimed. Performance
gate **OPEN**, release **HOLD**.

Detailed escalation and raw evidence:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/approved-crash-classification/ESCALATION.md`.
That directory also contains bounded plans, exact commands/hashes, all receipts,
a separate raw index and `review/void-log-approved-crash-review.tar.gz`.
No ptrace or Git mutations were used. Historical evidence and the previous
263/263 result remain unchanged. The next decision concerns a bounded causal
crash investigation, not another approval of the text architecture.

### Mandatory refinements retained for the eventual implementation

- Keep the exact approved source signature and nonrecursive scalar substitution;
  no callbacks, persistent native plans, locale interpretation or normalization.
- Use existing `text.error_kind`: `invalid_argument` for unequal arrays, invalid
  scalar keys or duplicates; `invalid_range` for negative `max_bytes`;
  `limit_exceeded` for encoded output exceeding the budget. Validate every rule,
  including for empty input, and enforce the unchanged-result budget. Allocation,
  root and invariant failures retain established Runtime failure behavior.
- Use the canonical standard-library private-operation pattern with exact typed
  evaluation/binding, `/@stdlib/text.void` authorization, and independent typed IR
  validation of arguments/results and provenance. Preserve registered roots,
  checked schemas/references, stable leases, native-owned snapshots, bounded
  cooperation, efficient rule lookup, overflow checks and atomic publication.
- Verify private rule-table retention through existing per-application Runtime
  globals; do not assume managed compile-time array constants or rebuild the
  logger's table for every record. Keep escape policy in VOID source, and measure
  initialization, validation, snapshotting and transformation separately.

The narrow scalar-key optimization remains optional and independently validated.
A new string lifecycle/lease contract still requires separate architecture review.
All frozen performance screens and the full final validation matrix remain in
force. These obligations have not been implemented or validated while the crash
precondition remains unmet.

## Prior architecture checkpoint and proposal (preserved)

**STOP for Golden Wrench architecture review. Performance gate OPEN; release HOLD.**
Baseline: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
Both remaining paths have been investigated. Neither is repaired to acceptance.
No production or repository test source was changed in this continuation; all
candidate implementations are external, test-only overlays. The previous
263/263 selected-test result belongs to the preserved baseline.

Boss's current directive says: “If a significant new shared Runtime contract
becomes necessary, stop for Golden Wrench architecture review before implementing
that contract.” This checkpoint follows that instruction. The proposal below is
not implemented, and its performance is not established by the native control.

## Evidence and decision

The current source/executable freeze and original failures are retained at:

`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/`

`freeze.json`, `historical-evidence-hashes.json`, `summary.json`, `targets.json`
and `EVIDENCE_INDEX.md` identify the data. The compact source-and-review archive
is indexed there separately; executable files and raw receipts are not added to
Git. The protected README and historical evidence are unchanged.

### Ordinary text

The original small-file workload reproduced at **17.323 ms / 200 records**,
versus **2.981 ms** for prepared-byte direct file output: **5.81×**. That direct
control prepares the complete record once; it does not perform encoding and
framing for every call.

For 64-byte ASCII, the first isolated component repeat measured quote/frame/full
console at **8.875 / 11.434 / 15.046 ms**. A scalar-read-only schema-key overlay
replaced temporary `std::string` keys with bounded immutable `string_view` keys.
It retained the same Runtime-owned metadata catalog and every root, nominal,
bounds and backing validation. Quote time became **7.425 ms**, frame **10.063 ms**,
and full console **13.477 ms**. Instrumented quote allocations fell from
**16,222 to 3,222**; the **12,800 checked reads, 13,200 header/root checks and
85,817 Runtime polls** did not disappear. These count comparisons use the same
independent AOT O2 instrumentation mode; timing comparisons use JIT O2.

The mechanism is specific: every `source_bytes[at]` lowers through the private
`vd_list_read_index` ABI, obtains registered-root authority, locks the Runtime,
normalizes signed indexes, resolves scalar metadata, validates nominal header and
backing, checks the bound and reads one scalar. Source call/loop safepoints remain.
The previous sprint already fused count/index checking; it did not make iteration
a bulk operation. Generating the long schema key adds a heap allocation per scalar
lookup. Removing that allocation helps, but cannot remove the remaining operation
count. For the expanded ordinary frame, both-key views reduce allocations from
24,522 to 10,522 while retaining 12,800 reads and 13,600 root lookups.

Existing string indexing counts/scans Unicode and constructs a substring;
`text` search/replace/join also traverse checked byte arrays. `string_bytes` is a
Managed-only internal borrow, not a source-level scan primitive. Caching raw
pointers in generated code, dropping polls or trusting a logging array would
change the safety contract and is not a valid repair.

### Escaped text

The current 1-KiB heavy-control frame / 100 records measured **926.213 ms** in the
expanded harness. Its **77,100 source managed constructions**, **76,900 bulk
writes**, **13,100 slices**, **785,600 root lookups**, **3,201,617 Runtime polls**
and **2,509,954 native allocations** explain the cost. Each escape creates a tiny
replacement array; unchanged spans create slices; each stream write constructs a
new `_state` to publish bytes and cursor atomically. The transactional state cannot
simply be deleted or replaced by separately published fields.

The source-only flat builder eliminates these per-escape objects and stream
writes, using one output byte array with checked pushes. Alone, it measured
**399.606 ms** for the original heavy frame. With both read and write schema-key
views, expanded heavy framing measured **343.877 ms** and full console **355.783 ms**.
Native allocations fell to **14,336**, with **zero source managed constructions**,
but **217,600 checked pushes, 268,800 reads, 488,000 root lookups and 2,456,517
Runtime polls** remained. Hook-observed copied bytes fell from 23,750,112 to
6,731,632; these hooks do not measure all hardware memory traffic.

This candidate also regresses occasional-escape input: 1-KiB framing rises from
**138.957 to 181.101 ms**, and near-budget framing from **245.763 to 326.894 ms**.
Copying ordinary runs one checked byte at a time loses the existing bulk-copy
advantage. It is not selected for production. Schema views alone do not materially
solve heavy framing either. A hybrid could reduce this particular regression,
but still retains checked traversal and per-replacement calls; the measurements
do not establish that it can meet either acceptance screen.

## Explicit engineering screens

These screens are recorded in `targets.json` before selecting any production
repair or implementing a shared capability. They are targets, not achieved results.

| Workload | Required screen |
|---|---|
| Small file, 64 B × 200 | Less than 10 ms; aim for approximately 2–3× the contemporaneous prepared-byte I/O floor |
| Heavy controls, 1 KiB × 100, complete framing | At most 10 ms; 97.66 ns/input byte, 45.72 ns/complete-output byte |
| Same heavy workload, complete console logging | At most 20 ms; 195.31 ns/input byte, 91.45 ns/complete-output byte |

The heavy workload contains 102,400 input bytes and 218,700 complete output bytes.
The expanded baseline controls measured **1.976 ms** for native UTF-8 decoding,
exact escaping and complete framing, **7.926 ms** for prepared-byte console I/O,
and **949.305 ms** for the source `_frame` followed by direct console output.
The last control performs the same source encoding/framing work on every call.
The native control is an algorithmic lower bound: it excludes registered roots,
managed allocation and Runtime publication. It also checks complete bytes inside
its timed loop. It cannot demonstrate a safe implementation's attainable latency.
Do not add independently timed controls and call the sum an end-to-end result.
All smaller/larger and Unicode cases must retain exact bytes, report scaling, and
show no material unexplained regressions before accepting a repair.

## Proposed smallest shared capability

Review a **general, bounded Unicode-scalar substitution operation in `text`**.
A possible source shape, subject to architecture approval, is:

```text
text.translate_scalars(value: string, const keys: [i32],
    const replacements: [string], max_bytes: i64) -> result<string, text.error>
```

This is a declarative finite substitution map, with no logger, sink, timestamp,
severity or escape policy in the Runtime. Each original scalar is replaced once;
replacement text is never recursively transformed. Empty replacement strings are
permitted. Unmapped scalar bytes remain unchanged, without normalization. Logging
would provide its exact current escape table in source, and continue to own framing,
UTC formatting, admission, byte budgeting and delivery.

This proposal needs no persistent native handle, compiled-plan cache, callback,
new scheduler or collector. Keys and replacements may be held as private source
constants. Initial implementation should snapshot/validate them per invocation,
using checked bulk traversal internally; introducing an immutable compiled plan
would be another decision, not an implicit expansion of this approval.

### Ownership and authority

- All managed inputs and the output are registered roots. Validate Runtime/thread/
  frame ownership, actual array schemas, nominal types, live extents, backing
  completeness, reference validity and output authority before reading data.
- Require equal key/replacement counts, valid Unicode scalar keys excluding
  surrogates, no duplicate keys and nonnegative `max_bytes`. Validate all rules
  even if the input would not use them. Invalid arguments use the existing typed
  text error boundary; allocation/Runtime failures retain the existing failure ABI.
- Use existing rooted snapshot/lease and buffer validation machinery for rule
  arrays. Acquire stable snapshots in a defined order and retain leases until the
  native-owned rule data is complete. Freeze structural mutation through the
  existing lease mechanism; do not infer immutability from a source `const` alone.
  Referenced replacement strings are immutable and remain traced through the
  rooted snapshot until copied. No managed pointer is retained in native-owned
  rule data, and no pointer is exposed to generated code.
- Validate and copy the input string through its registered root in quota-sized
  chunks, reacquiring the checked borrow after every safepoint. Never retain a
  `string_view`, element pointer or layout-dependent address across a poll or a
  Managed/Native transition. Existing metadata references remain Runtime-owned;
  no process-global cache of Runtime metadata pointers is introduced.

### Bounded work, GC and publication

- Reuse `BufferWorkQuota` conventions for bytes/node work in snapshotting and
  traversal. Safepoints occur between bounded chunks, with roots and leases live.
  Input/rule snapshots and transformation workspace are RAII-owned native data.
  A NativeSafe region may access only these owned snapshots. Return to Managed
  with the established coordination protocol before touching managed roots.
- Count exact UTF-8 output size with checked subtraction before appending or
  allocating the final extent. Use a counting pass followed by one native bulk
  assembly; avoid a managed read/push per scalar. Native rule sorting/indexing
  must also bound cooperation work for large caller-supplied rule sets.
- Do not allocate `max_bytes` eagerly. Space is bounded by validated input,
  rules and actual output sizes, with checked `size_t`/`i64` conversions and
  header arithmetic. No new hidden 64-KiB or 64-MiB output cap: logging callers
  already may explicitly raise their record budget.
- Publish only a complete valid immutable string through the existing checked
  string-construction boundary. An unchanged result may reuse the original
  rooted string after rule/budget validation. Stage aliases before output
  publication. Input arrays/strings are never mutated; no sink is involved.
- Ordinary argument/limit failure and every injected allocation failure leave
  the output unpublished, release temporary roots/leases/storage, and preserve
  caller-visible input. Final publication must follow the existing constructor's
  failure semantics, not add a second partially committed state.
- Existing string construction has its own staging and copy behavior. Reusing it
  does not justify claiming a new hard pause bound for arbitrarily large strings.
  Review that boundary explicitly; if meeting the approved work quota requires a
  new string-builder/lease contract, return to review before implementing it.

These are proposed obligations, not claims that today's public/source facilities
already provide the operation. The native benchmark implements only a reference
encoder and is not an implementation or safety proof of this contract.

## Review decision and remaining work

Approve, revise or reject this general substitution boundary before implementation.
The small scalar-key optimization can then be considered together with the final
formatter, with one shared key definition for every scalar encoding; it should
not be shipped as a claim that both paths were repaired.

After approval: implement and measure each change independently; add arbitrary
(non-logging) substitution, malformed root/schema, negative/unsigned indexing,
readonly/lease invalidation, structural mutation, aliased outputs, concurrent GC,
forced-GC and allocation-failure tests. Preserve every current escape/UTF-8/UTC/
budget oracle and lifecycle/failure ordering test. Run Debug, Release, strict
ASan/UBSan/leak checks, normal/O2 JIT and independently emitted/linked AOT with
actual hard 1 MiB stacks, plus relevant array/buffer/reference, async, rotation,
partial-write, cache/reload, tooling and installed-execution coverage. Recheck
100/250/500/1,000 direct and logging operations. Native Windows/macOS remains
pending. Then return for independent acceptance review; do not close the
performance gate based on a percentage improvement.

No new complete correctness matrix is claimed at this architecture checkpoint.
The baseline's 263/263 result is preserved; the external prototype runs are JIT O2
byte/count checks and component measurements, with an additional AOT O2 schema
instrumentation experiment. They do not validate a new Runtime contract.
