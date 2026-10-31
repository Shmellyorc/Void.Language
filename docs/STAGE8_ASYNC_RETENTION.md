# Stage 8 Sprint 5 — completed execution-frame retirement

Subsequent 30 October 2026 HTTP engineering adds
[completed result/Work/Handle retirement](net/http/RUNTIME_FOUNDATION.md) using
the existing task and precise-root model. Authorized HTTP continuation also adds
quiescent native Task/group record retirement, with focused measurements linked
in [the current HTTP record](net/http/CURRENT_ENGINEERING.md). The original Sprint 5
scope, measurements and remaining-limit statements below are historical. No
whole-Runtime RSS ceiling is claimed.

Status: implemented and locally verified; Wrench review pending; not hosted locked.
Baseline: `726b34b658f0d939b1da065acb4e9c735ddfa39d`. Sprint 5 only is authorized;
Sprint 6 and the hosted gate remain deferred. Root README stays unchanged.

## Measurement and bounded decision

The reproducible observer compiles the same workload/harness against the original
scheduler (read from the recorded commit) and current scheduler. Historical control
builds require the explicit local `VOID_RETENTION_BASELINE=ON` option; ordinary and
shallow/source-distribution builds need no historical Git access.

In the baseline, four batches of 128 awaited async children plus a sentinel retained
514 task records and 514 execution frames. After forced GC, live managed payload
was 34,890,840 bytes. The unchanged workload with frame retirement retains the same
records/results, one live parent execution frame and 10,944 managed payload bytes.
These canonical Release measurements justify the narrow change.
[Final acceptance](stage8/SPRINT5_ACCEPTANCE.md) records the complete paired data,
459/459 local suites and both 1 MiB frontend gates.

The observer separates completed native records, Work/group records and vector
capacity, cumulative managed frame allocations, rooted frame bytes, actual live
managed payload after GC, Runtime catalog counts, persistent Handle registration
and capacity, native generations/active lease references, and RSS. Execution frames
are managed allocations; native resume/scratch frames use stack storage and are
not separately persistent native heap frames. RSS includes LLVM, allocator caches,
worker stacks and observation overhead, and is not a live-payload bound. A constant
shape-size map avoids retaining a per-allocation observation registry.

## Ownership proof and implementation

A task result is a separate managed object, rooted by `Task::task`. Its descriptor
traces returned managed references/fixed values. Completed diagnostics own their
text. Repeated awaits read this result/outcome and the native Task record, not the
execution frame. Records and their creation order, group links, cancellation and
observed flags therefore remain unchanged until the owning root drains.

A completed execution frame can lose its persistent root at either of two points:

1. On the owning executor, **after the native resume call returns** and its state
   is finished. This includes checked error/cancellation exits. Generated return
   lowering may still access its frame after reporting completion; the completion
   hook itself must not release it. Defers, `with`, child joins and scratch-root
   removal have finished before the resume returns.
2. For a worker, **after the executor acquires its done publication**. The worker
   publishes only after resume, generated cleanup/root removal, and diagnostic
   copying return. It performs no subsequent execution-frame access. The executor
   copies outcome/status before clearing the frame Handle.

The change clears only `Task::frame`; actual storage and frame-only referents are
reclaimed by the existing precise GC. Prepared, queued, running, waiting and
joining frames remain rooted. Task/result records, diagnostics, native Work records,
callbacks/resources and the enclosing generation lease are not retired early.
The retained-record scans remain linear per scheduler lookup; no scheduling
throughput improvement is claimed. No scheduler replacement, extra collector, new task handle, public cancellation
API, ownership registry or task/memory ceiling is introduced.

## Preserved limits

Native Task/Work records, managed result objects and registered null Handle slots
still scale with total tasks created inside a root. Explicit groups and allocator
capacities also remain until their existing teardown points. This is **not** a
constant-memory scheduler or a general task-record retirement solution. Result
identity, repeated-await copying/errors/cancellation and creation-order unobserved
failures prohibit indiscriminate record deletion. Broader handle/record ownership
changes require owner review before implementation.

Runtime catalog entries are data-only and may remain after root drain/GC; bucket
and Handle vector capacity can also remain reusable. Native code is protected by
the existing root invocation lease; native callback registrations may additionally
retain their own generation. Frame retirement does not change either boundary.

The host-only `Runtime::memory_snapshot()` observes the existing Heap/catalog/root
registries under the Runtime mutex. `Heap::memory_snapshot()` sums admitted payload
extents and reports existing buckets/root capacity. These diagnostics allocate no
parallel ownership registry and expose no source-language task API.
