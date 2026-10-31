# Native file producer lifetime decision

30 October 2026. Boss approved the recommended existing-pool lifetime extension
through the in-chat review response. Implementation and focused verification are
in progress. The approved API includes synchronous `client.start` with
multipart file parts. It must return without waiting for headers or upload consumption, as specified
by DESIGN; native progress may overlap acquisition.
Response and upload may overlap. File parts retain existing exclusive IO claims;
no source or blocking file callback may run on the transport driver.

## Concrete existing limitation

`src/runtime/async.cpp` places `workers`, the worker queue and `worker_loop` inside
the per-root `Scope`. `ensure_workers` creates them for that root, and `~Scope`
joins them. `async_submit_native` rejects submission outside an active async body.
Synchronous IO runs its native kernel directly in NativeSafe state instead.

A synchronous multipart `start` can run outside any async root. Its file producer
must continue after `start` returns. A short-lived private async root would drain
the producer before returning, violating immediate acquisition and potentially
deadlocking behind an unread response. Deferring production until a later sync
response/read call has the same problem: the short-lived root cannot return early
while its producer is still active. Reading the file on the transport driver can
block other transfers, including the file's own source producer.

This is an ownership gap in the existing worker infrastructure, not permission
to remove file parts, weaken sync start, spool whole files or add source callbacks.

## Recommended extension of the existing pool

Promote the **existing** worker pool to Runtime lifetime. Retain one pool, the
existing worker limit, source scheduling, task/generation ownership, precise roots,
file kernel and owner publication. Do not create a second pool or source scheduler.

The current per-root Scope continues to own and drain its source tasks, native
Work, failure ordering, activations and generation lease. Shared worker threads
execute the same admitted Work and report completion to that Scope's native wake
state. A Scope cannot disappear while one of its Work records is running, even
though idle threads can survive into the next root.

Add a private native-only file-producer submission to this same queue. Its owned
payload contains file borrows, fixed native buffers, cancellation and driver
operations. It contains no generated function pointer or unrooted managed value.
Its Runtime ledger owner holds its lifetime and native-job admission; completion
and rollback are quiescent before file claims are released. The network driver
continues to receive only owned native bytes and native completion tickets.

Runtime shutdown must signal both transport and Runtime-owned file producers
before waiting for native jobs. It rejects new native-only admissions, cancels
those producers at existing checked boundaries and drains entered syscalls.
Already admitted source Scope Work retains its existing contract: Runtime.close
may return while that source Work is still admitted. The Scope continues to drain
its Work and code lease; the pool joins after that Scope leaves, or during idle
Runtime shutdown, before Runtime storage is destroyed. Source task/group semantics and generation leases
remain unchanged. No new GC representation or ownership registry is needed.

## Review/verification obligations

- Preserve all existing worker source tests, one-worker scheduling, source task
  cancellation and parent/group failure ordering.
- Verify repeated roots and compatible generation reload with idle persistent
  workers, retaining each running root's existing code lease.
- Verify sync multipart start outside an async body, early response, file producer
  backpressure, mixed byte/file parts, repeated file parts and exact cursor/count
  behavior. Concurrent file operations must retain existing busy/readonly rules.
- Verify allocation/queue admission rollback, rejected publication, Runtime
  shutdown while queued/running, and no transport callback after detach.
- Run Debug, Release, full strict sanitizers and 1 MiB stacks. Preserve evidence
  for any native syscall cancellation limit inherited from IO.

The native HTTP kernel and Runtime adapter currently work without this extension
for byte/text/stream bodies and explicit native bulk calls. Actual single-worker
file production inside an async root already passes. The existing worker dispatch has now moved to `src/runtime/native_work.cpp`,
with Scope-owned Work and Runtime-owned native-only cancellation. Source Work
remains scoped and is not cancelled by Runtime.close. Dedicated repeated-root, native-only, reconfiguration and shutdown
probes are under verification. Source HTTP integration and file multipart remain
unfinished.
No narrowed public contract has been selected.


## Native queue continuation implementation

The approved Runtime-owned pool now supports native-only bounded IO steps that
yield while a provider operation is pending. The completion holds the same pure
native wake state; it contains no Runtime, Task, Scope or generated pointer.
`queue_ready` defaults true and `finished` defaults true for source Work, so
ordinary source scheduling remains the existing single-run Scope-owned path.
Native file items rejoin the same queue until complete; they create no pool or
source scheduler. Cleanup has a preallocated ticket, and Runtime native-job
admission spans all steps and entered syscalls.

This is necessary for a one-worker peer dependency, not merely throughput: a
32 MiB upload blocked on a peer that waits for a second file request must let the
second producer run. The Release native fixture proves that case and simultaneous
multipart input / file download. The public source facade also proves native file
borrows remain busy after producer EOF until transfer finish. Current receipts
and retained integration failures are in CURRENT_ENGINEERING.md. Expanded strict
and native platform acceptance remain outstanding.


Queued Runtime-owned native items also retire without requiring a free worker:
stop first removes unentered native items, makes all producers quiescent, then
drains dependent cleanup. Existing Scope-owned source Work stays queued/running.
This preserves Runtime.close returning while already admitted source Work remains
blocked until that call returns. Running native syscalls still drain on their
worker; no asynchronous syscall termination is claimed. Delayed HTTP cleanup
conditionally erases its own ledger resource, preserving any newer resource that
reuses the same managed address. Focused regression coverage is in progress.
