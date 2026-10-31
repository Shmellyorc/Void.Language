# HTTP Runtime foundations: working-tree engineering

30 October 2026, LLVM/Clang/LLD 23.1.4; baseline `15f7868b`.
This records the initial foundation experiments. Subsequent approved curl/TLS
integration, native Task/group retirement, Runtime worker lifetime and real public
HTTP/source tests supersede its dated unfinished-limit statements; see
[the current record](CURRENT_ENGINEERING.md). Original experiments and failures
remain intact below. This file is not a full HTTP acceptance claim.

## External completion and scoped lifetime

`NativeCompletion` and `ExternalNativeJob` extend the existing native Work path.
The provider signals a native ticket; the existing owner executor receives it,
publishes rooted managed data, applies failure/cancellation policy and drains the
same parent/group relationships. External operations never enter the source-worker
queue. Source-worker native jobs retain their existing execute path.

The completion state owns native diagnostic text and a shared mutex/condition
notification object. It retains no raw Scope, Task, Runtime, managed payload or
source/JIT callback pointer. An atomic pending/completing/quiescent transition
admits exactly one completion. Later attempts return false, including attempts
through a retained ticket after the root adapter has drained. The owner acquires
the completion before publication. Provider completion is permitted only after
its callbacks and native result access are quiescent; it cannot just mean an
abort request was queued. The actual HTTP provider must still establish that
condition before calling complete.

Cancellation forwards once to the native provider without blocking the owner.
Publication stays on the owner with existing Runtime roots; allocation failure
uses the existing checked failure path. Acquired native results abandon through a
separate preallocated external cleanup ticket. A late original ticket cannot
complete that cleanup. Abandonment is shielded from subsequent cancellation,
matching the worker native-job path; real cleanup failures remain additional
diagnostics without replacing an earlier publication failure. Emergency adapter
unwind cancels/drains external work and cleanup before native records disappear.

The optional private `activation_lifetime` submission associates an immediately
acquired streaming resource's native lifetime with its current activation/group.
Normal exit also cancels/drains this private lifetime operation; a forgotten
transfer with no user read task cannot evade structured ownership. This adds no
public task syntax or detach API. A future transfer factory must register that
lifetime before committing acquisition. The HTTP factory/driver integration is
not implemented yet.

`NativeResource::request_stop()` signals provider shutdown before Runtime close
waits for `native_jobs == 0`. The hook is nonblocking and cannot allocate managed
values, invoke source or reenter the ledger. Close/drain still runs through the
existing ledger after the wait. Provider admission/native-job accounting remains
the responsibility of the Runtime-owned resource, as for existing filesystem
jobs; a completion ticket alone neither starts a thread nor owns Runtime lifetime.
The test proves pre-wait notification with an intentionally nonpublishing owner.

The driver can therefore complete without entering the managed executor or
waiting for source production on a source worker. Existing generation validation,
root invocation code lease, structured failure order, native reservations and
owner-only publication remain authoritative. No replacement scheduler or GC was
introduced. Curl process lifetime and a real Runtime HTTP driver remain future
work in this same mission.

## Completed result ownership

The scheduler releases a completed child result's persistent Handle after its
resume returns, or after native/worker completion is acquired and publication
finishes. Source task handles are already ordinary precise references: retained
handles keep the same result object alive and preserve repeated awaits and fixed
value copying. Dropping the scheduler Handle does not collect a reachable result.
The adapter's root result remains rooted through its final host copy.

Uncommitted native acquisitions keep their result/Work until commit or completed
abandonment. Once quiescent and committed, Work is destroyed too, releasing native
snapshots that may duplicate large managed result bytes. Empty scheduler roots
unregister their existing Runtime Handle through optional storage; there is no
weak-handle mechanism, finalizer, new collector or source task representation.

Native ordering/diagnostic records remain alive through root drain. When GC
reuses an unreachable result's address, registration replaces only a finished,
unrooted record's lookup entry. A live source handle prevents that reclamation.
The old native record still preserves parent/group/failure ordering; no stale
managed result address is dereferenced during drain. Existing source/IR task
opacity and generation checks are unchanged.

**Memory limit:** native Task/group records and collection capacities still grow
inside a long-lived root. This repair retires unreachable result payloads,
completed Work and persistent Handle registrations; it is not constant-memory
task scheduling or a whole-Runtime/RSS bound. Genuine HTTP large-transfer/pull
testing and provider/native admission accounting remain outstanding. The HTTP
module must not be described as memory-bounded from these foundation tests.

## Actual focused verification

| Check | Actual evidence |
| --- | --- |
| Release adjacent CTests | [10/10](evidence/runtime-foundation-release-final-tests.log): native jobs/external completion, IO async/acquisition, async source/parallel/lifecycle, normal/O2 independently linked activation AOT, source retention |
| Debug native CTests | [2/2](evidence/runtime-foundation-debug-final-tests.log) before the additional cleanup-shield case; subsequent native receipt below covers that added case |
| Strict native CTests | [2/2](evidence/runtime-foundation-strict-final-tests.log) with ASan/UBSan/leaks before the additional cleanup-shield case; subsequent strict native receipt covers that case |
| Final external/IO native process stack checks | [Six runs](evidence/runtime-foundation-stack.json), actual child soft/hard 1 MiB limits in Release, Debug and strict; IO's seven existing scenarios retained |
| Additional cancellation during abandonment | [All three profiles](evidence/runtime-foundation-cleanup-shield.json), ten external lifecycle cases, pre-wait shutdown and 128 MiB pull regression, actual child soft/hard 1 MiB limits |
| Source result/record distinction | [Full 16 process measurements](evidence/retention-source-results.json) and [summary](evidence/retention-source-summary.json), normal/O2; source repeated awaits and child-running GC retained |

The native pull fixture performs 128 one-MiB result operations while retaining
the first task handle. Forced GC after every pull reports **1,048,640..1,048,640
managed payload bytes**, at most **two persistent Handles**, and zero retained
native result-buffer bytes after each publication. The first result remains
awaitable/readable throughout. Root drain and subsequent GC leave zero objects.
This is a native task-result fixture, not a network transfer or HTTP acceptance.

The ordinary source `growth` fixture at four batches of 128 retains 514 native
Task records but only 6,856 managed payload bytes after GC in its normal run,
with two persistent Handles. Source `workers` retains 513 Task records and 5,232
managed payload bytes in its optimized run. Each current Task record is 952
bytes on this Linux build; records alone therefore reach 489,328 / 488,376 bytes,
plus vector/index/group/allocator overhead. Those costs and measured RSS remain
visible rather than being mislabelled constant memory.

Strict tests instrument the C++ Runtime and fixtures with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No sanitizer flags/assertions
were weakened. The source/JIT/O2/AOT checks above are Release checks; they are not
fresh Debug/strict JIT/AOT, native Windows/macOS or full HTTP acceptance. Child
process stack limits were queried; separate per-thread/provider stack probes and
actual transport stacks remain part of final integration.

## Preserved failures and corrections

- The first new-target build reported no make rule before CMake regeneration;
  [original log](evidence/runtime-foundation-release-build-first.log) retained.
- The retention observer includes the Runtime implementation after frontend
  headers. Its first updated build exposed `vd::Diagnostic` shadowing the internal
  diagnostic in `NativeCompletion::State`; explicit `::Diagnostic` fixes the
  lookup. [Original compiler errors](evidence/runtime-foundation-retention-observer-build.log)
  and [repaired build](evidence/runtime-foundation-retention-observer-build-fixed.log)
  are retained. These are ordinary compiler errors, not a process crash.
- A subsequent measurement accidentally invoked the old observer binary after
  that failed build. [Those observations are explicitly excluded](evidence/retention-first/EXCLUDED.txt),
  with its old binary hash retained; they do not count as new acceptance.
- The rebuilt observer then [failed its historical result-retention assertion](evidence/runtime-foundation-retention-current-first.log):
  it expected one live result for every historical native Task. The current
  observer separately reports scheduler-rooted results, generated frame reference
  counts and native records. Assertions bound live scalar results using actual
  generated root metadata and require settled payload/Handle plateaus, repeated
  awaits and zero post-drain objects. Native record growth remains checked.
- Source audit found that the new external cancellation path needed the same
  abandonment shielding as existing worker jobs. The corrected guard and an
  additional cancellation-during-cleanup fixture pass all three native profiles;
  no claim of a historical compiler/optimizer repair follows.

No abnormal compiler/runner signal or new core occurred in these checks. The
original logging/LLVM incident evidence, performance OPEN and release HOLD remain
unchanged. No Git/hosted CI/publication/root README action was performed.

## Reproduction and remaining work

Configure the existing LLVM 23.1.4 build directories normally, then build
`void_async_external_tests` and `void_io_native_jobs`. Run CTest selectors
`^(async_external|io_native_jobs)$`; the exact build/test logs are retained above.
The stack receipts use existing `tools/test_platform.py::small_stack` and
`report_stack`, with each child binary hash and output captured.

The full mission still needs dependency/link/install closure, curl global
lifetime, actual URL and HTTP compiler/source facades, a real native driver,
transport policy/streaming/bulk IO, complete error/tooling/examples, real h1/h2/TLS
and large-transfer tests, allocation/race/native platform acceptance and measured
provider memory/performance. None is inferred from this foundation. The exact
curl correction is now directly vendored and selected by CMake under Boss's
subsequent filesystem directive; see [current source/build evidence](VENDORED_PROVIDER.md).
Independent Wrench review remains separate from that authorized implementation.

[Current source/binary hashes and verification summary](evidence/runtime-foundation-verification.json)
identify this working-tree checkpoint; the earlier provider-only verification
snapshot remains preserved separately.
