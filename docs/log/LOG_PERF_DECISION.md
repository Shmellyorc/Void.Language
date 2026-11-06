# Logging performance release gate decision memorandum

## Current Boss disposition — 5 November 2026

The refined logging patch is already integrated with the shared internal repairs.
[Consolidated qualification and review handoff](../compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md)
is the active status. **The historical FILE200 strict 10 ms aim is not a
Boss-mandated release requirement or an automatic release blocker. It is not
claimed passed.** Retained improvements, regressions and real durability are
reported with their original measurement cohorts. Release authority remains Boss's.

The dated memorandum/handoff below is historical evidence, including its former
release-HOLD wording; that requirement interpretation is superseded above.
Original samples, rejected candidates and failures remain intact.

4 November 2026 · Owner-authorized documentation handoff.

**LOG-PERF OPEN / release HOLD.** Preserve the original
**file200 <10 ms** requirement. The least additional work is an owner decision
on further investment or a separate requirement disposition. Existing evidence
identifies costs but establishes no specific narrow repair likely to close the
gate. It does not prove the requirement impossible.

## Performance acceptance

Accepted production: `11e95f4a`. These are Release LLVM 23.1.4, JIT O2, GC=256,
actual soft/hard 1 MiB stack results on the retained Linux host. Later production
controls are separate observations; rejected-candidate results do not qualify production.

| Gate or criterion | Accepted result | Latest production control | Status |
| --- | --- | --- | --- |
| File 64 B ×200, **<10 ms**, including acquisition/flush/close | 21.390 ms | **20.091 ms**; six samples, range 19.884–24.931 | **FAIL** |
| Heavy 1 KiB ×100 complete framing, **≤10 ms** | **8.603 ms** | 7.408 ms | **PASS**, bounded host/mode scope |
| Heavy 1 KiB ×100 full console, **≤20 ms** | **18.501 ms** | 18.536 ms | **PASS**, bounded host/mode scope |
| Approximately 2–3× prepared direct file I/O aim | 1.75× | 2.06× (20.091 / 9.738 ms) | **Meets relative aim**; does not replace <10 ms |
| Exact bytes, budgets, sink/error behavior, collected-resource restoration | Accepted targeted checks pass | Bounded benchmark oracles pass | **PASS within retained coverage** |
| Tail latency, worst pauses, full native peak live memory, broader platform performance | Not established | Not established | **UNKNOWN**; no new numeric threshold inferred |

Accepted file200 also fails in normal JIT and normal/O2 AOT; latest controls
cover JIT O2. [Accepted performance](BOUNDED_FRAMING_RESULT.md#performance-comparison)
and [raw criteria](evidence/llvm23-framing-20261104/results.json) retain their scope.

## Findings and uncertainties

**CPU-side work:** Latest production file200 uses 14.399 ms process CPU; earlier
write-only controls take about 14–15 ms wall time before flush/close. One disjoint
reduced-timer write-only sample totals 14.026 ms, including translation 4.934,
GC 0.794, byte snapshot 0.846, checked I/O reply 0.888 and native writes 0.516 ms.
Its unresolved remainder is 5.670 ms, plus 0.378 ms other measured scopes.
These are elapsed scopes, not pure CPU attribution.

Translation already returns 200 aliases after validating all 81 rules per call.
Preparation/disposal dominates that operation. Allocator, validation, root,
mutex, copying and tree costs are not independently resolved; allocation/lock
counts do not establish allocator time or contention.

**Filesystem synchronization:** One profiled end-to-end sample totals 19.873 ms,
including native flush 5.481 ms and native close 0.0075 ms. Flush spans
5.481–8.615 ms in that run. Logger close calls the required `fsync` before closing.
These function timings do not isolate device latency or explain the storage cause.
Removing flush changes durability and leaves substantial CPU work.

**Variation:** Latest file200 control-block medians are 20.160/20.018 ms; an earlier
control median was 26.806 ms. Direct-I/O medians have crossed 10 ms in both
directions: there is no proven universal storage floor. Active-desktop samples
lack confidence intervals, and detailed timers perturb execution. Independent
medians or different runs cannot be combined into an exclusive breakdown.
[Attribution and receipts](SMALL_FILE_INVESTIGATION.md) preserve those distinctions.

## Rejected experiment

**Allocator experiment: REJECTED.**

The per-invocation map allocator reduced allocations **16,200 → 200**, but full
file200 improved only **20.091 → 19.485 ms: 0.605 ms / 3.0%**. Requested map-storage
bytes increased **1,166,400 → 1,641,600: 40.7%**; this is traffic, not peak live memory.
Quote200 improved 16.0%, with no observed heavy-path regression. The small
end-to-end gain triggered the approved discard/stop rule; correctness was not
the rejection reason. Production was restored exactly.

The [rejected patch](evidence/small-file-20261104/rule-allocation-candidate.patch)
and [record](SMALL_FILE_INVESTIGATION.md#bounded-rule-allocation-experiment--4-november-2026-discarded)
remain preserved. Targeted ownership, logging and native allocator sanitizer
checks passed. Broader strict setup failed because its older frontend lacks
`_translate_scalars`; that preserved failure is not a strict-suite pass.

## Choices and recommended action

| Choice | Benefit | Tradeoff |
| --- | --- | --- |
| Retain <10 ms and HOLD | Preserves the contract; no engineering work now | Release blocker remains |
| Separately authorize a narrow repair investigation | May identify removable CPU work | No demonstrated mechanism supplies the roughly 10 ms end-to-end reduction needed; benefit/effort unknown |
| Separate owner requirement or exception decision | Explicitly settles acceptable latency and scope | Changes acceptance or accepts risk; cannot count as passing original <10 ms |

The byte-snapshot proposal targets roughly 0.85 ms; achievable savings would be
smaller and cannot alone close this gap. More allocator variants or output-builder
work lack justification here. Any further repair proposal needs a named mechanism,
a credible full-workload benefit hypothesis, preserved ownership/failure semantics
and a bounded review plan before implementation.

**Least-work next action:** Golden Wrench takes this memorandum to the requirement
owner (Boss): retain <10 ms and HOLD pending separately scoped work, or explicitly
disposition the requirement. That decision needs no new benchmark or infrastructure
work. The current miss does not imply that relaxation is necessary.

## Evidence required to change blocker status

An engineering **PASS** requires Golden Wrench review of:

1. The original whole file200 **JIT O2 median strictly <10 ms**, preserving exact
   bytes, 200 writes, acquisition, required flush/close, budgets and errors.
   Use the retained warmup/three-sample protocol with repeated contemporaneous
   control blocks; retain every sample. Report normal/O2 JIT and AOT separately
   for the existing qualification scope, without averaging modes or platforms.
2. Same-host/build heavy framing **≤10 ms** and full console **≤20 ms**, plus
   relevant byte/count, ownership/GC/failure and sink/error checks for any changed
   path. Compatible validation artifacts are required to claim a profile passed.
3. Source/binary/toolchain hashes, commands/environment, actual stack limits,
   GC budget, raw wall/CPU samples and failures. Exclude compilation; separate
   counters from acceptance timing. Controls must distinguish improvement from
   variation. Phase-only and direct-I/O results cannot replace file200.

Alternatively, an **explicit dated owner decision** must identify the revised
threshold or exception, workload, durability/host/mode scope, rationale and risks.
Record revised acceptance or a waiver separately; retain the original <10 ms
**FAIL** and rejected experiments. No such decision is currently recorded.

**LOG-PERF: HOLD / OPEN.** HTTP/HTTPS and accepted installation qualification stay
unchanged. Historical failures remain preserved. This review authorizes no new
implementation, experiment or regression campaign. Boss authorized committing and
pushing only this memorandum, the logging handoff and the blocker register with
`[skip ci]`. Supporting investigation reports, receipts and the rejected patch
remain preserved locally outside this three-file commit; their relative links
resolve in the working checkout. This authorization does not waive LOG-PERF.
Stop after the documentation handoff.
