# Compiler-health issue inventory

**STATUS: COMPILER STABILIZATION COMPLETED / STANDARD-LIBRARY DEVELOPMENT HOLD LIFTED BY BOSS.**

Boss authorized this change after local acceptance of the await/propagation
syntax. See the [syntax delivery and current authorization](../async-await-syntax/HANDOFF.md).
Normal VOID development may resume under the existing Boss approval, Research, Wrench review and Keyboard implementation workflow. This lifts the special stabilization freeze; it does not approve unspecified modules or unrelated features. No new standard-library development was performed in this syntax sprint.

H1–H7 remain unresolved historical observations. Independent review and hosted
acceptance are not inferred from the lifted hold.

## Historical stabilization record — original hold superseded above

**STATUS: NEW STANDARD-LIBRARY DEVELOPMENT PAUSED — COMPILER STABILIZATION IN PROGRESS.**

Boss authorized the full implementation sprint on 27 October 2026. Current
[engineering and measured candidate status](stabilization/IMPLEMENTATION.md)
supersedes the research-only authorization boundary below. The baseline issue
and historical incident inventory is retained; no independent acceptance or
historical crash repair is inferred from passing candidates.

27 October 2026. Baseline **`1d4ad15f5215838d209e48bb47ac7c3f52b05371`**.
Research only. New standard-library work is on hold. Historical evidence is
retained; a passing retry does not prove a repair. [Report](REPORT.md),
[measurements](BENCHMARKS.md), [engineering plan](ROADMAP.md).

All 605 baseline lines of KNOWN_ISSUES were reviewed alongside current source
and newer acceptance records. The classifications below supersede dated status
claims for this audit; they do not reopen every intentional language exclusion.
Original line ranges refer to the baseline, before its new disposition banner.

## Stabilization implementation status

The retained research table below describes the frozen baseline. Implementation
and local verification are complete; independent Golden Wrench review remains
pending. See the [handoff](stabilization/HANDOFF.md),
[measurements](stabilization/BENCHMARKS.md) and
[audit](stabilization/AUDIT.md). No historical crash repair is inferred.

| ID | Current local disposition |
|---|---|
| C1/C2 | Repaired shared result/collection/async/await/spawn/choice hints; exact nested JSON and generic fixtures pass, including order, error, cleanup, readonly and LSP checks. |
| C3 | Repaired rejected-chain Debug stack exhaustion through outlined hints and borrowed checking contexts; original depth/recovery expectations pass at 1 MiB. |
| P1 | Exponential revisit removed: identical left-chain check median 1.518449→0.008038 seconds. |
| P2 | Compact generated code, pointer-home packing and descriptor bulk initialization meet the local objective: DTO128 optimized uncached 8.149–8.609 seconds, versus 297.021-second frozen replay and 309.755-second research result. Full ladder/profile/JIT/AOT evidence is retained. |
| P3 | CLI entry emission after full checking reduces unused JSON import 10.307→0.227 seconds, pre-O2 functions 64→2. Globals, dispatch, callbacks, cleanup and async targets have explicit preservation checks; embedding/raw emit retain all exports. |
| P4 | Existing native cache is automatic for optimized CLI runs; no-IR-O2 is separate, `--no-cache` bypasses, legacy `--cache` retains its behavior. Source/import/actual compiler changes, unsafe/corrupt/concurrent cases and fresh state are verified. |

An additional confirmed defect, **C3**, is rejected deep receiver chains
overflowing Debug's 1 MiB stack before the semantic guard. The exact baseline
also reproduces it. Outlined hint handlers and borrowed checking contexts pass
the original depth/recovery suite without changing its limit or expectations.
This concerns rejected chains and is distinct from the historically repaired
legal-chain lowering and H1–H7.

## Frozen baseline defects

| ID | Defect and scope | Evidence / disposition |
|---|---|---|
| C1 | Generic inference uses a result wrapper for a propagated argument, including nested collection/result forms | Existing [reduction](../../tests/json/reductions/generic_nested_propagation.void); `anchor` fallback [compiler.cpp:2147–2220](../../src/frontend/compiler.cpp), [generic inference:65–70](../../src/frontend/generic_inference.inc). Fresh independent check probes reproduce it. A temporary local/explicit generic is a workaround, not the repair. |
| C2 | Related hints misrepresent await, async calls and conditional expressions | Existing task plus inferred await fails; direct async-call arguments infer the payload rather than the task. Immediate-call await currently passes by cancellation of two wrong hints. A choice argument can infer the condition bool. Exact cases and diagnostics are retained in the inference evidence. Repair the shared hint boundary coherently. |
| P1 | Exponentially repeated frontend arithmetic hint traversal | `operator_anchor` obtains child hints, returns none for primitive operators, then `anchor` repeats the walk. A tiny 20-addition left chain checks in 1.514 s versus 0.00687 s balanced; LLVM is not involved. [Source:2107–2119,2218–2220](../../src/frontend/compiler.cpp). |
| P2 | Severe real compilation latency for generated JSON DTO workloads | Historical 128-field O0 246.61 s / shapes O2 148.76 s; new unchanged-CLI ladder and LLVM phase measurements in BENCHMARKS. Raising a timeout to 600 s is not a performance repair. No crash inference follows from a timeout. |

## Historical unresolved observations

These require causal evidence from preserved/new captures or an independently
demonstrated faulty invariant before attributing a root cause or changing
production behavior. They are separate incident families.

| ID | Observation | What is established / what remains unknown |
|---|---|---|
| H1 | LLVM 19.1.7 DSE SIGSEGV | [Original record and preopt IR](../stage11/OPTIMIZER_FAILURE.md), [backtrace](../stage11/evidence/llvm-dse-backtrace.txt): crash in `CallBase::getMemoryEffects` during O2. Pre-pass verification passed. Same binary later passed; serialized IR passes standalone opt. This identifies a compiler phase, not whether invalid in-process state or LLVM caused it. Stage 11 lock explicitly does not claim a fix. |
| H2 | Release async filesystem runner SIGSEGV | [Independent investigation](../io/SIGSEGV_RESEARCH.md), [initial log](../io/evidence/release-initial-io.txt), [bounded phase captures](../io/PHASE_CAPTURE_2026-10-26.md). No original fault PC/core/backtrace, exact original temporary source or failing binary hash. The reconstructed workload is a candidate, not verified original bytes. Five phase captures passed; their authorized budget ended. No workers/GC/JIT cause established. |
| H3 | Stage 9 two preliminary process faults | [Acceptance, reliability observations](../stage9/ACCEPTANCE.md): Debug async emitter and numeric process segfaults. Focused/baseline/current/debugger and later strict controls passed. No demonstrated cause/repair; do not fold them into H1/H2. |
| H4 | Pinned Clang BDCE/DemandedBits build crash | [Classification and original reproducer links](../text-encoding/follow-up/verification/crash-classification.md): Clang 19.1.7 compiling `conversion.cpp`, not an unsupported system Clang. Same source/flags later passed. A compiler-build incident, not source-program execution. |
| H5 | LLD/LLVM optimizer stack while linking SDK objects | [Stage 10 record](../stage10/IMPLEMENTATION.md): GVN/AnalyzeLoadAvailability while processing `IRTranslator.cpp.o`; enclosing build returned zero. Earlier [KNOWN_ISSUES toolchain observation](../KNOWN_ISSUES.md#toolchain-observations) has a separate GlobalISel/machine-scheduler stack. Preserve raw diagnostics without inventing a nonzero exit or merging incident identities. |
| H6 | Parallel ThinLTO cache temporary-object rename/link failure | [Stage 11 acceptance](../stage11/ACCEPTANCE.md). Serial retry passed. Build/link cache observation, independent of DSE and application caches; no inferred DSE repair. |
| H7 | Stage 10 Step 37 publication/default-root anomalies | [Original implementation history](../stage10/IMPLEMENTATION.md). Did not reproduce in controlled frozen-library reruns. Keep as historical uncertainty; do not treat the current JSON root workload as proof of the same cause. |

This investigation does not restart an indefinite crash hunt. No ptrace/debugger
was used. If a new signal occurs, preserve the exact source, binary hashes, output
and phase first. Boss requires stopping **before** ptrace use; debugger collection
is a separate owner-authorized next action, not a prerequisite to the measurements
reported here.

## Verified repairs: exclude from new repair engineering

| Historical issue | Newer source/evidence | Current disposition |
|---|---|---|
| CI88/89 Windows names/manifests/deep native output | [CI95 machine-readable acceptance](../stage9/CI95_WINDOWS_ACCEPTANCE.json): Windows Debug 160/160 and Release 160/160. Existing same-parent publication/short private link staging/security retained. | Repaired and natively verified for the accepted scope. Physical second-volume transfer remains an accepted exclusion. |
| Deep valid receiver-chain small-stack lowering | Stage 7 acceptance already supersedes the old open entry; the old file even contradicts itself at baseline lines 121–123 versus 230–239. | Repaired. Preserve the original issue and 1 MiB regression, not a new active bug. |
| Stage 10 Release result-handler stack regression | [Implementation record](../stage10/IMPLEMENTATION.md): dispatcher reservation reduced 4152→1088 bytes with unchanged optimization. | Repaired under focused stack gates. Does not explain H1/H2. |
| Text-encoding Debug anchor stack regression | [Independent crash classification](../text-encoding/follow-up/verification/crash-classification.md), current cold `name_anchor` / text-case helpers in compiler source. | Repaired with exact original evidence and subsequent checks. The new arithmetic work explosion P1 is distinct from that stack repair. |
| JSON readonly result/capture/pattern failures from research | [JSON handoff](../json/HANDOFF.md), [readonly regressions](../../tests/json/readonly.py): 47 process checks/profile, four-mode positives and authority negatives. | Locally repaired and verified in effective 93/93 JSON/shared groups; no native-platform lock inferred. |
| JSON invalid-pattern recovery abort | [RAII restore in compiler.cpp:2258–2267](../../src/frontend/compiler.cpp), retained reduction and diagnostic regression. | Repaired: ordinary source diagnostic replaces abort. |
| LSan process-inspection sandbox failures | [Math handoff](../math/HANDOFF.md), [Random incidents](../random/INCIDENTS.md): identical strict flags/binaries passed outside the sandbox. | Environment-specific failures, preserved; not established compiler memory defects. Do not disable leak checking. |
| Stale build/source/provenance races; malformed test flags; synthetic crashes | [JSON handoff](../json/HANDOFF.md), Math/Random incident ledgers and preservation tests. | Distinguish setup/provenance errors and deliberate self-signals from actual product failures. Frozen replacements do not erase originals. |

Source details of the JSON repairs: growable capture/enumeration uses checked
ordinary storage ([compiler.cpp:4038–4043,4725](../../src/frontend/compiler.cpp));
pattern/narrowed authority follows inferred facts
([capabilities.inc:183–198](../../src/frontend/capabilities.inc)); comparison RHS
keeps its own root authority
([pattern_lowering.inc:21–25](../../src/frontend/pattern_lowering.inc)). The Runtime,
independent IR validator and LLVM backend were not modified for those repairs.

## Current intentional boundaries versus measured concerns

| Area | Intentional boundary | Consequential performance implication |
|---|---|---|
| Native-object cache | Automatic O2 and separate no-IR-O2 objects use the existing store; Windows persistence safely unavailable. Per-project cache has no automatic eviction and corrupt candidates remain misses. | A cache hit still performs source discovery/analysis, key hashing, object load/metadata admission and execution. Cold compilation is measured independently. |
| Rooting/GC | Precise coordinated nonmoving GC; function-duration conservative roots; no ASan instrumentation of every generated instruction. | Root initialization, registration validation, repeated root membership scans and collection traverse large root sets. Measure before changing checks; root liveness/slot reuse is an engineering task, not a correctness waiver. |
| Async ownership | Completed execution-frame roots retire, but Task/result/Work records and null Handle registrations remain until root drain. | [Retention contract](../STAGE8_ASYNC_RETENTION.md), [async.cpp](../../src/runtime/async.cpp): linear find/receive/pending scans can make long-lived sequential-task workloads grow in memory/work. Source-supported concern; this audit does not claim a newly measured asymptotic defect. |
| Editor analysis | Compiler-owned full-graph analysis, not incremental per-function compilation. | [LSP:227–229,516](../../src/lsp/main.cpp) reanalyzes the graph and may flush synchronously. Save/watch/refresh handlers already exist at 609–623; old restart-only wording is obsolete. Compiler latency affects editor usability. |
| JSON source algorithms | Metered ordered duplicate/schema comparisons; stringify currently converts even a dynamic value into a fresh DOM. | Actual native throughput is separate from compilation; bounded quadratic work and fresh allocation are explicit opportunities, not authority for a second parser or weaker limits. |
| Strings/collections | Immutable strings; checked per-element collections; no new source reserve API. | [Text performance evidence](../text-encoding/evidence/PERFORMANCE.md): 65,536-byte multi-operation Release O2 workload median 6.688 s, Debug O2 45.408 s. Algorithms avoid repeated accumulated-string copying; Runtime costs need attribution. Historical samples shared the host with other work. |
| Native/ABI/reload | Existing aggregate ABI, trusted bindings, generation leases, explicit Runtime ownership and conservative restart rules. | Not missing feature requests for stabilization. No arbitrary FFI, concurrent collector, package manager or second compiler is proposed. |
| Language exclusions | General overload sets, broad nullable/generic forms, richer interface/enum/iterator features and extra dictionary key families remain bounded by current contracts. | Intentional limitations, not defects to add to this repair sprint. Existing wrapping arithmetic/checked casts are settled semantics. |

## Reconciliation of the historical known-issues file

| Baseline section/range | Current interpretation |
|---|---|
| CI88/89 entries, lines 3–34 | Superseded by CI95 native acceptance; original failure details retained. |
| Stage 7/8/9 authorization and planning, 55–158 | Dated workflow checkpoints, superseded by later implementation/locks; they do not prohibit the current Boss-authorized investigation. |
| Delivery 5 / boundaries, 160–197 | Preserve actual resource/task/cache/root limits. Later result get/pop, text, formatting, runtime globals and editor capabilities supersede missing-feature claims. |
| Phase 23 / receiver chain, 203–239 | Large-array and chain blockers repaired under later accepted source lowering; no arbitrary small array ceiling reinstated. |
| Phase 22 / Phase 21 object model, 241–341 | Later strings, private/readonly fields, calls and constructors are implemented. Remaining richer object-model boundaries follow their current contracts. |
| Compiler scope, 364–426 | Main discovery, explicit entry overrides, closures, async, globals, collections, warnings, multiline support and native executable linking exist. VFS identity/security and intentionally bounded identifiers/native ABI remain current. |
| Managed memory/reload, 428–474 | Ownership, exact roots, checked returns, generation leases and native lifetime boundaries still apply; historical absence of later async activation does not. |
| Toolchain, 476–495 | LLVM 19.1.7/no-RTTI/header warnings and unresolved diagnostic stacks remain accurately scoped; CURL warning and sandbox LSan are not source-runtime defects. |
| Tooling/editor, 497–584 | Old phase exclusions are historical. Current compiler-owned completion/hover/signatures/navigation/rename/formatting, discovery, save/watch/refresh and supported workspace handling exist; full incremental compilation and native client exclusions stay separate. |
| Composition, 587–605 | Approved one-hop stored composition and existing conflicts/access rules remain intentional. No new mixin/transitive-promotion feature is authorized. |

Stage 10, Stage 11 and documentation/hover locks remain their respective accepted
historical scopes. None closes an unexplained incident without causal evidence.
The current internal index still contains stale JSON research-only wording;
the actual [JSON handoff](../json/HANDOFF.md) and production source govern this
audit. This report does not issue a new acceptance lock.
