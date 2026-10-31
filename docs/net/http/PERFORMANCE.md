# Local performance and size evidence

Linux x86_64, AMD Ryzen 7 1700X, LLVM/Clang/LLD 23.1.4 Release. Samples were
collected during concurrent local validation. These are observed ranges, not
throughput guarantees or isolated-machine benchmark claims. Stack limits are
1 MiB. Instrumented memory results are separate from Release RSS.

## Small JSON requests and pool behavior

[Opt-in measurement tool](../../../tests/net/http/performance.py) uses independent
Python HTTP/1.1 and pinned h2 peers with TCP_NODELAY, verified local TLS, one
allowed connection per client, 64 sequential warm requests and 16 batches of
four concurrent requests. Actual response versions and exact JSON bytes are
checked. The peer observes 137 small requests sharing a connection, followed by
16 explicit partial-response closes. HTTP/1 needs 16 sockets in total; HTTP/2
retains its one connection and resets individual streams.

| Mode | Cold ms | Warm median / p95 ms | Four-request batch median ms | Partial close median / max ms |
| --- | ---: | ---: | ---: | ---: |
| HTTP/1.1 | 1.279 | 0.321 / 1.398 | 1.174 | 0.099 / 0.130 |
| HTTP/1.1 verified TLS | 4.686 | 0.297 / 0.581 | 0.963 | 0.149 / 0.743 |
| HTTP/2 verified TLS | 33.855 | 0.526 / 0.698 | 1.979 | 0.077 / 0.685 |

Cold time includes peer startup/lazy Python protocol imports and the first
handshake. HTTP/1 batches serialize on their single connection; HTTP/2 uses real
multiplexing. Close time is native abort-to-quiescent completion, not source task
publication latency. [Full measurements](evidence/http-performance-release/measurements.json)
include process CPU, compiler identity, compilation ranges and sizes. An original
run with server Nagle enabled is preserved separately; its 42 ms H2 batch time
was fixture latency, not removed by a production workaround.

An earlier real driver defect polled only the first client's multi even when
later clients had active sockets. Aggregating their documented wait descriptors
reduced H2 warm median from 10.362 ms to 0.518 ms on that native fixture. Native
single-driver ownership, wakeup and fixed admission remain unchanged. The later
measurement table above uses a separate controlled peer.

## Unchanged IO comparison and completed-task cost repair

The same optimized source loop performs 256 asynchronous 64 KiB writes to an
existing IO file (16 MiB) with one worker. JIT compilation is excluded. The
baseline is an actual disposable build of commit
`15f7868b0af177f8f7d49c548392c79c7bd921d2`, not a reconstructed old algorithm linked
against incompatible current headers. Its archive/harness identity is in
[the comparison summary](evidence/http-io-comparison-summary.json). Git archive
was read without checkout, branch/index/history mutation or root README edits.

| Unprofiled loop | CPU median seconds | CPU range seconds | Wall median seconds |
| --- | ---: | ---: | ---: |
| Baseline, no HTTP import (9 runs) | 0.837 | 0.788–0.912 | 0.853 |
| Initial current Runtime, same pure IO (9 runs) | 1.282 | 1.214–1.398 | 1.299 |
| Current after retention-check short-circuit (9 runs) | 0.890 | 0.858–0.996 | 0.901 |
| Current HTTP-capable comparison, absent/idle/paused (9 runs) | 0.837 | 0.796–0.859 | 0.843 |

The initial regression was real. User-space sampling identified
`Scope::sweep_completed`: every task/group retention check continued scanning
dependencies after a live root, pending child or other reason had already
established retention. Short-circuiting those read-only searches preserves every
keep/retire decision and avoids the redundant quadratic walk. Collection epochs,
precise roots, repeated await, ordering and generation leases are unchanged.
Kernel sampling was denied; those original diagnostics remain retained. The
successful gprofng sampling runs use only isolated measurement processes; no
host performance-security setting or production signal/allocator policy changed.

The HTTP-capable run's absent/idle/paused wall medians are 0.812/0.843/0.864
seconds. A paused 512 MiB response exercises backpressure while the same worker
completes all file writes. This proves progress for this workload; it does not
promise disk/network throughput under every host load. All original runs,
including slower concurrent-load samples, remain in evidence.

## Sustained bodies and package cost

The final public native bulk run downloads 2,147,483,648 bytes and uploads
4,294,967,297 bytes from a sparse file, with independent count and SHA-256
`153014c024b3db9a126b40397ded44c0b708efde6f5c89f22407e4089160850a`.
[Receipt](evidence/http-reaping-public-bulk-4GiB-release/commands.json) separates source
checking, async object emission, native linking and execution, with RSS samples.
Its emitted object/executable are 2,434,192/10,106,360 bytes. The final run takes 6.494 seconds execution, 8.56 process CPU seconds and
10,580–11,200 KiB steady RSS. The earlier identical logical workload took 5.983
seconds, 7.5 CPU seconds and 10,496–11,028 KiB; both receipts remain available.

The public source 2 GiB async-pull proof runs normal/O2 JIT and independently
linked normal/O2 AOT. [Final receipt](evidence/http-reaping-source-2GiB-release/commands.json)
includes exact source/compiler identities, forced-GC Task/Work/Handle observations
and per-process RSS. Holding the first task demonstrates repeatable await while
later results and native records retire. Native bulk and source loops establish
different memory properties; neither is a total-process RSS ceiling.

The unused-import comparison executes check/JIT/build/native in both optimization
modes three times. Its ordinary native executable is 1,844,104–1,844,144 bytes;
adding an unused HTTP import produces 1,946,448–1,952,032 bytes. No network operation
is invoked. Linux Release dependency regular files total about 24.6 MB, separately
from compiler/runtime artifacts. [Raw size/compilation measurements](evidence/http-performance-release/measurements.json)
preserve actual samples. No Windows/macOS or machine-independent speed claim is made.
