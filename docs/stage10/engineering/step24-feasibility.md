# Step 24: native cancellation policy blocker

22 October 2026. **Historical blocker, resolved by Boss's later contract revision.**
Boss approved prompt queue/readiness cancellation with best-effort cancellation of
entered uncancellable syscalls and retained lifetime through actual completion.
Step 24 is now implemented and focused-tested under that corrected contract; see
step24-results.json and IMPLEMENTATION.md. The findings below remain historical,
not an outstanding owner decision or a claim of mandatory io_uring. The metadata
probe gained only no-I/O overrides to stay buildable after Adapter was extended.
The Step 23 Runtime, Adapter and lease implementation is unchanged. This probe
examines a proposed transfer strategy; it does not reveal a transfer defect in
Step 23, which does not perform transfers.

## Reproduced constraint

The approved contract borrows host descriptors, permits independent output
writers, requires exact progress, and requires shutdown to wake/drain requests.
It suggests readiness plus an owned wake pipe for POSIX. Readiness only describes
the observed instant. It does not reserve capacity for the following syscall.

The standalone `step24-posix-feasibility.cpp` uses real pipes and the existing
Runtime/Context/Operation cancellation model:

1. An operation sees POLLOUT on an empty blocking pipe.
2. A barrier permits an independent host writer to fill exactly its capacity.
   No descriptor duplication, replacement, closure or status-flag change occurs.
3. The operation writes **one byte**. Linux /proc thread syscall inspection proves
   that it has entered the blocking write. This rules out fixing the example
   merely by splitting writes into PIPE_BUF chunks or adding a second readiness
   check: another writer can fill capacity after the last check.
4. Runtime.close requests stop. Its native callback signals a separate owned wake
   pipe. The probe observes that readable wake pipe while the request is still in
   the write syscall and close is still waiting on its admitted lease.
5. Only the test's explicit data-pipe read releases the write. Exactly the host
   prefix and final byte are verified; operation and shutdown join; borrowed flags
   are unchanged. No worker is detached and no pending request is abandoned.

A second real probe submits a one-byte POSIX aio_read on a pipe, waits until
libc's worker is demonstrably in the read syscall, then calls targeted
`aio_cancel(fd, &request)`. It returns AIO_NOTCANCELED and remains EINPROGRESS.
The test explicitly writes a byte, waits/reaps the result and verifies progress.
This is an observed libc/Linux limitation, not a universal claim about all AIO
implementations.

Seventeen checks pass in both Debug and strict ASan/UBSan/leak configurations.
Barriers and syscall observation determine ordering; ten-second bounds detect
broken test synchronization, without sleep-based assumptions. The standalone
probe is not added as a passing production-transfer CTest.

## Alternatives investigated and decision needed

Linux `io_uring_setup` with two entries and no special flags succeeds outside the
restricted runner (fd 3, errno 0). Inside that runner it fails with EPERM (1).
The structures come from the installed kernel header, not a guessed layout.
The earlier Python diagnostic observed fd 4 outside restriction; descriptor
numbers naturally depend on live host descriptors. Neither setup result verifies
read/write submission, request cancellation, completion progress or portability.
It is incorrect to claim io_uring is unavailable on this machine, or to infer that
it would be available to every generated VOID application.

**Boss/Wrench decision:** settle the native-request and platform compatibility
policy before production transfers are added. May this adapter require
platform-specific asynchronous requests when readiness cannot guarantee a
nonblocking syscall, and what must happen when the kernel/sandbox cannot provide
the needed cancellation facility? Alternatively, the embedding/wait contract must
be explicitly revised with its precise guarantees. Stronger exclusive output
ownership alone has not been established as sufficient for every supported device
or regular-file wait; it must not be presented as a proved complete solution.

This is a decision about promised behavior and platform support, not a request to
reauthorize Step 24. An implementation cannot silently pick a Linux kernel/runtime
requirement, weaken host interoperability or report an unfinished adapter as an
unsupported platform capability. No contract change or new dependency was adopted.

Unacceptable silent workarounds include changing shared O_NONBLOCK flags, changing
process-global signal handlers, closing borrowed host descriptors, canceling
unrelated host requests, abandoning a live request/buffer, or inventing consumed
byte counts after forced worker cancellation. Native Windows/macOS behavior was
not tested. Windows dedicated-thread/overlapped cancellation still needs actual
adapter integration and platform verification; no Windows failure was inferred
from the POSIX probe.

## Current engineering boundary and continuation

Production sources, CMake, Runtime ABI and dependency versions are unchanged.
Seven existing Runtime suites pass Debug 7/7 (13.03 s) and strict 7/7 (5.80 s),
including the 72 foundation and 14 real endpoint checks from Step 23. These reruns
prove preservation, not transfers. The architecture probe has 17 checks in each
configuration; strict runs include actual leaks outside the ptrace runner.
An initial manual probe link omitted libvoid_native_fixture; the final build uses
both existing archives and the pinned LLVM/Clang 19.1.7 linker. No assertion or
sanitizer check was disabled.

Step 24's actual transfers, complete short writes, EOF, portable/native errors,
partial input, monotonic deadlines, owned-request cancellation, SIGPIPE handling,
real transfer concurrency and GC acceptance are **still unimplemented/unaccepted**.
No source console API or newly linked JIT/O2/AOT acceptance is claimed.

After the decision, the next source change remains the existing Adapter/Operation
in `include/void/console.hpp` and `src/runtime/console.cpp`: explicit outcomes and
native-owned buffers, one deadline through admission/transfers, then the agreed
owned-request platform implementation and original focused Step 24 tests.
Do not advance to Step 25 as though Step 24 passed. Steps 25–31, composite display,
full four-system integration and final Stage 10 acceptance remain open. Previous
Step 23/32–40 records and Step 37's unexplained observations are preserved.

## Primary API evidence

- [poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html): writable readiness
  does not promise an arbitrary-size write will not block; regular files are
  always reported ready. The probe strengthens the concrete writer-race case
  to a one-byte request without depending on arbitrary transfer sizes.
- [pipe(7)](https://man7.org/linux/man-pages/man7/pipe.7.html): blocking pipes wait
  for capacity; O_NONBLOCK belongs to the open file status, and PIPE_BUF atomicity
  does not reserve capacity between poll and write.
- [aio_cancel(3)](https://man7.org/linux/man-pages/man3/aio_cancel.3.html): an
  in-progress request may be uncancelable; AIO_NOTCANCELED is a normal outcome.
- [io_uring_setup(2)](https://man7.org/linux/man-pages/man2/io_uring_setup.2.html):
  creates the request/completion interface; setup success alone is not a transfer
  or cancellation guarantee.
- [Microsoft cancellation guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations):
  request identity, synchronous-thread ownership and driver support matter. A
  cancellation request does not itself establish completion or buffer safety.

Exact commands, logs, hashes, platform limits and preservation:
`step24-results.json`. No Git mutation, hosted CI or public root README edit.
