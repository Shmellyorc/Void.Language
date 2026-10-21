# Stage 8 Sprint 5 — completed execution-frame retention

Status: **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending; not hosted locked.**
Baseline: `726b34b658f0d939b1da065acb4e9c735ddfa39d`.
[Contract and ownership proof](../STAGE8_ASYNC_RETENTION.md).

## Evidence-driven scope

This sprint delivers a small internal implementation change, not measurement-only
deferral. A single long-lived root runs four increasing batches of 0/8/32/128
children. A 64-field inline value creates meaningful execution frames; async
children additionally allocate referenced fixed-array data. Each task is awaited
twice, with forced GC during a live child, after awaited batches, at group exits,
and after final root drain. A second workload exercises value-only workers.

The identical observer/source is compiled against the original scheduler read
from the baseline commit and current scheduler. It uses existing Runtime GC and
catalog diagnostics; a constant-size function-shape map tracks actual emitted
frame/task extents without retaining a per-allocation measurement registry.
The original scheduler is optional local test control, not a normal-build Git
history dependency. No source task handle, result, cancellation or ownership API
changed. No scheduler, collector, task ceiling or ownership registry was added.

## Canonical before/after measurements

Release host, Linux x86-64, LLVM 19.1.7; 32 successful isolated processes cover
both scheduler variants, both workloads, normal/O2 JIT and all four batch sizes.
Numbers below are after the fourth batch and forced GC, before root drain.

| Workload / metric | Original | Current |
| --- | ---: | ---: |
| Async completed children, including sentinel | 513 | 513 |
| Async total native task records | 514 | 514 |
| Async rooted execution frames | 514 | 1 |
| Async rooted frame payload bytes | 1,262,656 | 6,832 |
| Async reachable managed payload bytes | 34,890,840 | 10,944 |
| Async reachable managed objects | 3,593 | 515 |
| Async native Task record bytes (excluding allocator overhead) | 419,424 | 419,424 |
| Async RSS in the selected normal-JIT pair | 84,533,248 | 52,637,696 |
| Worker completed children | 512 | 512 |
| Worker rooted execution frames | 513 | 1 |
| Worker rooted frame payload bytes | 549,984 | 5,216 |
| Worker reachable managed payload bytes | 554,088 | 9,320 |
| Worker reachable managed objects | 1,026 | 514 |
| Worker native Task record bytes | 418,608 | 418,608 |
| Worker RSS in the selected normal-JIT pair | 58,564,608 | 57,950,208 |

Both normal/O2 modes produced the same managed payload/frame counts. Task/frame
allocation totals are unchanged: 514 async allocations (including root/sentinel),
513 worker allocations. The improvement retires dead execution storage rather
than suppressing task creation. Snapshots before forced GC distinguish allocated
unreachable storage from post-GC reachable payload; reclamation remains ordinary GC.

The full [measurement CSV](sprint5-measurements.csv) preserves every phase, size,
mode and variant, including RSS, cumulative allocations, frame bytes, Task/Work/
group storage, capacities, catalog counts, Handle registration and generation
observations. [Manifest](sprint5-measurement-manifest.json) records scope/reproduction.
Raw canonical JSON and stdout/stderr remain in `build/stage8-sprint5/final-paired/`.

## What is still retained

Task/result records and two registered Handle objects per record remain until
root teardown. Completed frame Handle values are null; the Handle registrations
and vector capacity still exist. At the largest async checkpoint, registered
Handles remain 1,028 and native Task record bytes remain 419,424 in both variants.
Returned i32 result objects remain rooted for repeated awaits; failures and
cancellation retain owned outcomes and creation-order state. Native Work records
and explicit Group records retain their existing lifetimes.

The settled Runtime catalog remains four nominal types, one schema and zero
witnesses in these workloads; the schema is admitted lazily at first array use.
One native JIT generation remains alive with three active-lease shared references
including the observer's temporary copy. This reference count is diagnostic, not
an ABI promise; existing reload tests prove old-generation protection/retirement.
AOT code is statically linked and has no corresponding JIT generation count.

After root drain plus GC, managed object/payload counts are zero in both variants.
Heap bucket/Handle capacity, data-only catalog entries, LLVM/allocator storage and
RSS do not necessarily fall to zero. RSS is process-wide resident pages, includes
observer/LLVM/worker-stack effects, and is not a leak proof or total-memory bound.
Native resume/scratch frames are stack storage; measured persistent native bytes
refer to Task/Work/group records, not an additional native execution-frame heap.
Payload bytes exclude allocator/tracing-map/container overhead. No scheduling
throughput improvement or constant-memory root is claimed. Task-record lookup
and native/result retention remain material future limits.

## Reproduction and permanent checks

Configure a local build with `-DVOID_RETENTION_BASELINE=ON`, then build
`void_retention_baseline` and `void_retention_current`. Ordinary builds leave that
option off and need no baseline commit/history. Run:

```
python3 tests/stage8/sprint5/measure.py \
  --current build/ci-release/void_retention_current \
  --baseline build/ci-release/void_retention_baseline \
  --output build/stage8-sprint5/final-paired
```

Five new CTest suites preserve the growth invariant and normal/O2 linked AOT
parity: `async_retention_sprint5` plus `async_retention_aot_{growth,workers}_{normal,optimized}`.
The observer asserts that active-child frames survive collection, only the parent
frame remains at completed checkpoints, records/results remain, and post-drain
GC reaches zero. The linked executables preserve the legacy printed result 7.
Existing async source/IR/lifecycle/parallel/native integrated suites cover managed
and fixed-value return copies/identity, errors/cancellation, unobserved order,
parent/cleanup precedence, joins, defer/with, held workers, forced GC, Runtime
closing, callback drains and compatible reload/old-generation leases.

Initial focused verification passed those five existing suites but failed five
new observer checks: the observer incorrectly demanded identical catalog counts
before and after legitimate lazy schema admission. It now compares settled group
exits while retaining every raw initial snapshot. AOT validation also now reads
its actual workload label instead of assuming a worker workload. Corrected focused
Release checks passed 5/5; Debug checks passed 5/5. These are test-observer repairs,
not relaxed runtime safety, hidden measurements or additional unique suites.
Preliminary allocation-address bookkeeping was replaced by the constant shape map
before canonical paired measurements; preliminary RSS is not used for claims.

## Final consolidated local acceptance

All builds completed before their corresponding final unfiltered CTest runs.

| Configuration | Final result | Elapsed |
| --- | --- | --- |
| Debug | 153/153; zero failures | 520.47 s |
| Release | 153/153; zero failures | 232.69 s |
| RelWithDebInfo / strict ASan+UBSan+leaks | 153/153; zero failures | 460.39 s |

**459/459 final unique configuration/suite outcomes.** Focused reruns, process
measurements and historical controls are not added to that count. Both full
Debug/Release frontend suites passed with a real 1 MiB stack. Strict acceptance
ran outside ptrace with leak detection and ASan/UBSan halt-on-error enabled; no
assertions, sanitizer categories, depth guards or leak checks were disabled.
Instrumentation covers C++/C compiler/Runtime/provider code; generated LLVM
application instructions are not claimed automatically instrumented.

[Results and log hashes](sprint5-results.json) and [code/test hashes](sprint5-source-hashes.json)
record exact commands, counts, evidence and retained unsuccessful observer checks.
Catalog counts are not a census of every immutable catalog snapshot allocation;
payload sizes exclude native tracing-map/container and allocator overhead.

Windows/macOS and actual hosted verification remain pending at the end-of-Stage-8
gate. No hosted CI was run or requested. Root README and CI #82 repair files retain
their hashes; HEAD remains the starting commit. No commits, pushes, branches,
remote changes, publication or Sprint 6 work occurred. Stop for Wrench review.
