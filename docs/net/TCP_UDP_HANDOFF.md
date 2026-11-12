# TCP/UDP standard library — architecture handoff

6 November 2026. Research and proposed contract only. Stop for Golden Wrench/Boss
review; no production implementation, Git action, tests/builds or hosted CI were
performed for this investigation.

**Recommendation: GO for one bounded TCP/UDP engineering mission after approval
of the observable policies below.** Use ordinary source facades under `net/tcp`,
`net/udp`, `net/address` and `net/error`, backed by nonblocking native sockets and
the existing external-completion/Runtime resource machinery. No new language
syntax, raw handles, public event loop, scheduler, HTTP implementation or TLS API.

## 1. Verified foundation and actual dependencies

| Current source/evidence | Reusable behavior / remaining gap |
|---|---|
| `stdlib/net/` contains `http.void`; [HTTP acceptance](http/ACCEPTANCE.md) | HTTP/HTTPS accepted; no general public TCP listener, TCP stream or UDP resource. Public HTTP support does not imply raw socket support. |
| [Original stdlib handoff §12](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md) | Separate `net/http`, `net/tcp`, `net/udp`; byte-array interoperability with IO; explicit async variants. Historical examples are direction, not existing declarations or evidence of overload support. |
| `include/void/native_resource.hpp`; `src/runtime/native_resource.cpp` | One Runtime resource ledger, owner-thread checks, rooted wrappers, pre-wait stop notification, quiesce then close. Reuse it; GC does not replace explicit native cleanup. |
| `include/void/async.hpp:39–99`; `src/runtime/async.cpp:233–335,734+` | `ExternalNativeJob`, native-only completion tickets, owner publication, cancellation and acquisition abandonment already exist. No source Task/Scope/managed pointer belongs on a socket polling thread. |
| `src/runtime/native_work.cpp:80–100` | Shared worker count is finite. Blocking accept/read/DNS on each worker can consume the entire pool and prevent the peer work needed to finish. Do not implement async sockets that way. |
| `src/runtime/http.cpp:243,1184–1326`; `http_resource.cpp:20–37,183+` | Existing model: Runtime-owned native provider thread, bounded polling, native result ownership and external completion. Reuse that pattern and interfaces; do not route raw sockets through HTTP requests or copy HTTP's whole implementation. |
| `src/runtime/filesystem_abi.cpp:41–130,143+`; `stdlib/io.void` | Checked identities, reservations, independent byte snapshots, native execution, owner-rooted replies, explicit close and recoverable progress errors. Sockets need separate read/write reservations rather than a file's single cursor reservation. |
| `src/runtime/coordination.cpp:351–394` | Runtime shutdown signals native resources before waiting for jobs. Socket/DNS stop must unblock completion without requiring source code to run. |
| `third_party/http-versions.json`; local pinned c-ares 1.34.9 sources | A cross-platform asynchronous resolver is already linked. No new DNS library is needed. c-ares does not imply full OS NSS/Bonjour behavior. |
| `src/runtime/http_provider.cpp:45–73`; `third_party/curl/lib/easy.c:167`, `lib/vdns/asyn-ares.c:95,112` | Existing shared provider lease owns curl's process initialization, including Winsock/c-ares. New independent init/cleanup calls would need coordination. |
| `src/frontend/http.inc`, `src/ir/module.cpp:1118–1120,1506–1508`, `src/backend/llvm/http.inc` | Existing compiler-owned native operation declarations, effect/worker checks, rooted ABI lowering and LSP metadata. Extend this pattern for sockets, not a separate semantic engine. |

The inspected local c-ares `src/lib/ares_library_init.c:142–176` uses a plain
process initialization count; its documentation explicitly calls initialization
non-thread-safe. This is a **new-integration risk**, not proof of a current HTTP
failure. Smallest initial solution: socket service holds the existing shared
`http::ProviderLease` for its complete lifetime, including every resolver channel.
It must not additionally call `ares_library_init/cleanup` or `WSAStartup/WSACleanup`.
The lease's private name can be made protocol-neutral without changing behavior.
This shares existing dependency initialization only: no curl client, transfer or
HTTP driver is created. It retains the current HTTP dependency closure, including
its feature checks; a separately buildable sockets-only Runtime is not promised.

## 2. Proposed public declarations

These are declaration sketches using existing VOID types/default arguments, not
compilable implementation files. Factories have distinct names; no overload
system, implicit host/endpoint conversion or unchecked native constructor is added.
All IO resources are opaque managed structs with private construction. Async
functions return the existing source task representation automatically.

### Shared address and errors

`net/error` imports the existing core `error`. Its error carries `kind`,
`operation`, `phase`, `native_domain` (`posix`, `winsock`, `cares`, or empty),
`native_code`, `cleanup_code`, `bytes_transferred`, `partial: [u8]`,
`peer: string`, `datagram_consumed: bool` and `datagram_size: i64` (-1 if unknown).
Do not force socket errors into filesystem `io.error` categories.

Kinds: `invalid_argument`, `invalid_address`, `name_not_found`, `resolve_failed`,
`address_in_use`, `address_not_available`, `permission_denied`,
`connection_refused`, `connection_reset`, `network_unreachable`,
`host_unreachable`, `timed_out`, `closed`, `write_closed`, `busy`,
`message_too_large`, `limit_exceeded`, `unsupported`, `io_failure`.
OS-specific detail remains available; equivalent failures need not have identical
native numbers. There is no ordinary `cancelled` result substituting for VOID's
structured task cancellation.

```text
# net/address
enum family:
    any
    v4
    v6

# ip_address: value struct with private canonical bytes, actual v4/v6 family
# and u32 scope index. No mutable sockaddr, flow-info or native pointer.
value struct endpoint:
    ip: ip_address
    port: u16

value struct resolve_options:
    family: family = family.any
    timeout_ms: i64 = 5000

func parse_ip(text: string) -> result<ip_address, net_error.error>
func format_ip(value: ip_address) -> string
func format_endpoint(value: endpoint) -> string
func any_v4() -> ip_address
func any_v6() -> ip_address
func loopback_v4() -> ip_address
func loopback_v6() -> ip_address
func resolve(host: string, port: u16, options: resolve_options = resolve_options()) -> result<[endpoint], net_error.error>
async func resolve_async(host: string, port: u16, options: resolve_options = resolve_options()) -> result<[endpoint], net_error.error>
```

`parse_ip` is numeric only: strict dotted IPv4 or IPv6, optionally a numeric
`%scope` on IPv6; no DNS, brackets, embedded port, octal shorthand or URL syntax.
Endpoint formatting uses brackets for IPv6 and numeric scope. Ports are host-order
u16 values; conversion to network byte order stays native. Port zero is valid for
binding/ephemeral selection but invalid for a remote connect/send destination.
Scopes are host-local indices, not portable interface identities; link-local
operations require an appropriate nonzero scope. Interface-name discovery is
outside the first API. Reject IPv4-mapped IPv6 literals initially rather than
silently creating mixed-family sockets; ordinary IPv4 and IPv6 remain supported.

Resolve accepts numeric literals through the same parser, or bounded ASCII DNS/
ACE names with an optional final dot. Reject invalid numeric-looking literals,
NUL, URLs and unencoded Unicode hostnames rather than guessing their meaning.
Initial lookup covers c-ares configured DNS and hosts-file behavior, not arbitrary
NSS plugins, mDNS/Bonjour, service-name lookup, SRV or reverse DNS. No environment
override of the hosts-file path. Return distinct endpoints in provider order,
deduplicated by numeric family/address/scope/port; no addresses is `name_not_found`.
Do not imply that DNS authenticates a peer. Public resolver output has a finite
cap (propose 32 endpoints per lookup) and over-limit answers fail instead of
silently dropping addresses.

### TCP

```text
# net/tcp; imports net/address and net/error as address / net_error
value struct connect_options:
    family: address.family = address.family.any
    timeout_ms: i64 = 30000
    attempt_timeout_ms: i64 = 3000
    no_delay: bool = true

value struct listen_options:
    backlog: i32 = 128
    no_delay: bool = true             # policy for accepted streams

struct chunk:
    bytes: [u8]
    eof: bool

func connect(host: string, port: u16, options: connect_options = connect_options()) -> result<stream, net_error.error>
func connect_endpoint(remote: address.endpoint, options: connect_options = connect_options()) -> result<stream, net_error.error>
func listen(local: address.endpoint, options: listen_options = listen_options()) -> result<listener, net_error.error>
func (self listener) accept(timeout_ms: i64 = 0) -> result<stream, net_error.error>
func (self listener) local_endpoint() -> result<address.endpoint, net_error.error>
func (self stream) local_endpoint() -> result<address.endpoint, net_error.error>
func (self stream) remote_endpoint() -> result<address.endpoint, net_error.error>
func (self stream) write_bytes(const bytes: [u8], timeout_ms: i64 = 30000) -> result<i64, net_error.error>
func (self stream) read_bytes(max_count: i64 = 65536, timeout_ms: i64 = 30000) -> result<chunk, net_error.error>
func (self stream) shutdown_write() -> result<bool, net_error.error>
func (self stream) close() -> result<bool, net_error.error>
func (self listener) close() -> result<bool, net_error.error>
```

Provide explicit `connect_async`, `connect_endpoint_async`, `listen_async`,
`accept_async`, `write_bytes_async`, `read_bytes_async`, `shutdown_write_async`
and both receiver forms of `close_async`, with the same arguments/results.
Endpoint queries inspect owned metadata and need no async twin. Each resource has
`deinit` following existing `with` cleanup; explicit close reports errors directly.

Semantics:

- `connect(host,port)` resolves then tries candidates sequentially, each with the
  smaller of remaining total time and `attempt_timeout_ms`; total includes DNS.
  Close failed candidates before trying the next. Numeric connect performs no
  DNS, uses the total timeout and rejects a conflicting family option. Return
  first success; if all attempts fail before the deadline, preserve the last
  attempted endpoint/native error and an attempted-count diagnostic. Never retry
  application bytes. This bounded simple fallback is **not Happy Eyeballs**;
  an unreachable first address can delay fallback by up to its attempt budget.
  [RFC 8305](https://www.rfc-editor.org/rfc/rfc8305.html) explains the concurrency
  alternative; adding a connection-racing policy is not necessary for this first
  contract and should not be claimed implemented.
- Listen requires an explicit numeric local endpoint. No implicit wildcard or
  hostname bind. Backlog is positive, bounded (recommend 1..4096), and subject to
  OS limits; it is not an application connection quota. Accept exposes the actual
  local/remote endpoints. Closing a listener does not close already accepted
  streams.
- `write_bytes` snapshots input once, then completes all bytes or returns an error
  with the exact locally accepted prefix count. Writes are not network messages.
  A successful send does not prove peer receipt or processing. No flush method:
  `io.flush` durability has no socket equivalent.
- `read_bytes` returns available data up to a positive bound; one write may require
  many reads and many writes may appear in one read. `eof=true` only on a later
  zero-byte native receive after queued bytes are exhausted. EOF is successful,
  repeatable and distinct from reset; an empty successful non-EOF chunk is not
  returned. No read-ahead beyond the requested bound. Users frame protocols using
  ordinary byte arrays/`encoding/binary`/`io/memory`, not TCP packet assumptions.
- A zero-byte write validates the live writable stream and succeeds with zero;
  it sends no wire event. A nonpositive read bound fails before IO. No exact-read,
  text-line or serialization variants are needed in this initial socket API.
- `shutdown_write` sends native FIN after already accepted output, does not wait
  for a peer acknowledgement, and leaves receiving available. It is idempotent,
  rejects an active writer with `busy`, and later writes return `write_closed`.
  There is no public linger/abortive-close option. Close is not a delivery guarantee.

### UDP

```text
# net/udp
struct datagram:
    bytes: [u8]
    peer: address.endpoint

func bind(local: address.endpoint) -> result<socket, net_error.error>
func (self socket) local_endpoint() -> result<address.endpoint, net_error.error>
func (self socket) send_to(const bytes: [u8], remote: address.endpoint, timeout_ms: i64 = 30000) -> result<i64, net_error.error>
func (self socket) receive_from(max_count: i64 = 65527, timeout_ms: i64 = 30000) -> result<datagram, net_error.error>
func (self socket) close() -> result<bool, net_error.error>
```

Add `bind_async`, `send_to_async`, `receive_from_async`, `close_async` and `deinit`.
One socket has one explicit family; destination family must match. Resolve names
separately, not on every datagram. Bind port zero selects an ephemeral local port.

One send means exactly one datagram, including an **actual zero-length datagram**.
No chunking or automatic retries. IPv4 payloads above 65507 and IPv6 payloads above
65527 fail before transmission; smaller packets can still exceed path/interface
limits. Jumbo UDP is excluded. Large UDP can fragment or fail; examples should
use modest payloads, not advertise the maximum as a reliable network MTU.
Success means local acceptance of the whole datagram, never reliable delivery.
Loss, duplication and reordering remain application concerns.

Receive consumes exactly one message. Empty payload is valid, never EOF.
Recommended portable oversized-message policy: receive into a bounded native
65536-byte scratch allocation, then compare with `max_count` (0..65527). If too
large, **discard that datagram and return `message_too_large`**, with peer,
`datagram_consumed=true`, known size and consumed-byte count; no misleading
successful prefix. A native truncation beyond the supported bound reports unknown
size when necessary. Do not use the error count as a retry offset. The next call
receives the next datagram. Zero receive capacity deliberately accepts an empty
datagram and rejects a nonempty one after consumption.

This avoids relying on identical truncation reporting: Linux has `MSG_TRUNC`,
whereas [Winsock recvfrom](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recvfrom)
reports `WSAEMSGSIZE` and discards excess data. Never expose platform-truncated
data as a complete application packet.

## 3. Deadlines, cancellation and failure precedence

Relative timeout values use a monotonic native clock and one absolute deadline
captured at admission. Positive values are capped/documented (recommend at most
24 hours); zero disables the deadline for accept/read/write/datagram waits.
Connect/resolve budgets must be positive; reject negatives/overflow before IO.
Time includes provider queue time; partial progress does not reset it. No
real-time return guarantee: OS scheduling, native setup and cleanup may delay
reporting. Recompute remaining time after EINTR/readiness wakeups.

Input/type/range validation, resource identity/state and same-direction reservation
precede payload snapshot/native admission. Establish this order in one operation
contract table for sync/async; a failed snapshot/admission performs no IO. The
provider serializes completion decisions: a successful syscall may win its race
with a subsequently observed deadline; already accepted bytes cannot be undone.

| Event | Observable result and resource state |
|---|---|
| TCP write deadline/native failure | Typed error with accepted prefix count; timeout alone leaves stream usable, fatal connection error makes it terminal. Caller must not resend the prefix. |
| TCP read deadline before data | Typed `timed_out`, zero progress, stream usable. Positive receive completes that read; later EOF/reset belongs to a later read. |
| UDP timeout before a syscall succeeds | No datagram consumed/sent; typed timeout. Native ICMP errors may reflect an earlier send and are not delivery receipts. |
| Explicit resource close | Stop admission, wake pending operations, drain native access, close once. Pending operations not already completed receive `closed`; racing completed operations can still report their actual result. |
| Source task cancellation | Existing structured cancellation, **not `Err(cancelled)`**. Drain provider work before task/frame release. No promise of zero external effects or a returned progress count. |

**Recommended conservative TCP cancellation rule:** cancellation of an admitted
TCP read/write closes that stream, including its opposite pending direction.
Cancellation before admission has no socket effect. This avoids silently reusing
a byte stream after consumed/sent data whose result was suppressed by task
cancellation. It also covers completion-before-owner-publication races. It is a
genuine policy choice requiring Boss approval. Operation timeouts remain recoverable
results and do not use this rule.

Cancelling a pending accept does not close the listener; a newly accepted but
unpublished stream is closed. Cancelling connect closes all provisional sockets.
Cancelling UDP may have consumed or sent one entire message; discard any unpublished
received message and leave the socket usable. Cancelling a resolver affects only
that lookup. There is no promise that retrying a cancelled UDP send cannot duplicate
a message.

Managed allocation failure retains existing Runtime failure semantics, not a
guaranteed recoverable out-of-memory error. If TCP progress cannot be published,
terminalize the affected stream. An accepted/connected handle whose wrapper
publication fails must be closed. UDP publication failure may discard one datagram.
No operation can promise rollback of network effects.

## 4. Runtime architecture — one source scheduler

Use one lazily created **native socket service per Runtime**, owned in the existing
ledger. It owns nonblocking TCP/UDP descriptors, DNS channels, a bounded command
queue, readiness registrations and deadlines. One native thread performs native
socket work; no threads per connection, source callbacks or managed allocation.
It completes existing `NativeCompletion` tickets; it is an IO provider, not a
second source event scheduler. Keep the accepted HTTP transport/driver separate.

Start with `poll` on Linux/macOS and `WSAPoll` on Windows. Watch readiness only
for pending operations; unwatched idle connections do not cause a busy loop.
Reuse the HTTP driver's bounded-poll approach: idle with no active work waits on
a native condition variable; active polling waits at most the nearest deadline
or 10 ms so command/cancel processing does not depend on peer traffic. This is
an implementation starting point, not a latency promise. No epoll/kqueue/IOCP
framework until measurements show the bounded poll design insufficient.
[WSAPoll](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll)
supports socket readiness; do not treat Win32 HANDLEs or Unix pipe descriptors as
interchangeable with SOCKETs. Native performance qualification must include idle
CPU and cancellation responsiveness.

Readiness is only permission to attempt a nonblocking syscall. Handle EAGAIN/
WSAEWOULDBLOCK by rearming, and connect completion with `SO_ERROR`, not writability
alone. Accepted descriptors must explicitly acquire nonblocking/non-inheritable
state. If a platform cannot atomically create that state, serialize the setup
appropriately against process launch; never expose the descriptor during setup.
All actual close/unregister actions occur on this service, with generation-tagged
native operation identities so reused descriptor integers cannot receive stale
completions. Do not allocate a second source ownership registry.

Reservations: one TCP reader and one writer concurrently; one listener accept;
one UDP receive and one send. Another same-direction operation returns `busy`
rather than entering an unbounded implicit queue. Close has priority over new
work and can drain active reservations. Source async tasks may share aliases on
the owning executor; source worker functions may not perform socket/DNS IO.
Closed aliases stay closed. Readonly views cannot mutate/consume a socket. Pure
address values may be copied; enforce native-call effects through existing IR checks.

Sync functions submit the same native operation then wait in `Runtime::Native`;
async functions use `async_submit_external`. A synchronous operation called from
an async activation blocks that executor, even though GC can cooperate: document
using `_async` when other source tasks must progress. Driver completions must never
depend on that executor being available.

Reuse existing acquisition `acquired`/`publish`/`commit`/`abandon_external` hooks.
Connection/accept acquisition keeps a provisional native owner until source
publication/commit. For existing-stream progress, retain a cleanup obligation until
successful publication, then clear it; publication failure invokes native cleanup.
Do not copy HTTP CompletionJob's no-acquisition/no-op abandonment unchanged.
`src/runtime/async.cpp:285–316` can suppress publication on cancellation or turn
allocation failure into task failure; new adapters must cover both paths.

Requests own native snapshots and resource references; owner-side reply building
uses registered frames/roots. Tickets carry native completion state only. Receive
arrays must not alias reusable native scratch storage. Repeated awaits preserve
existing task-result identity and language copying rules; they do not perform
another receive or promise a new deep copy. JIT code
leases and task generation remain existing Runtime authority. `request_stop`
only signals; quiesce drains callbacks/jobs, then close releases sockets and DNS,
then the shared provider lease. Shutdown must succeed with pending infinite
accept/read and no remote activity. Explicit close is idempotent; forgotten
wrappers remain Runtime-owned until shutdown, not GC-finalized.

Initial proposed admission bounds: 1024 live/provisional socket resources, 32
concurrent lookups, 2048 operations and 64 MiB outstanding native payload/scratch
bytes per Runtime; TCP per-operation input/read limit 4 MiB. DNS descriptors also
need a separate bounded count under the same service accounting. Enforce every
limit before uncontrolled growth, reserve cleanup capacity, and count pending
results until publication/abandonment. These are proposed resource limits to
validate, not performance gates or claims of total-process RSS bounds. Existing
managed copies, c-ares internals and kernel socket buffers need separate accounting.

### DNS integration

Use the already pinned c-ares 1.34.9 with **one channel per concurrent lookup**.
That avoids cancelling unrelated requests: [ares_cancel](https://c-ares.org/docs/ares_cancel.html)
cancels all queries on a channel. No extra c-ares event thread. Feed its socket-state
changes and [process_fds](https://c-ares.org/docs/ares_process_fds.html) timeouts
through the native socket service. Use numeric service ports and `ARES_AI_NOSORT`
to avoid c-ares address-sorting connection probes. Snapshot/copy bounded numeric
results and free provider data; callbacks may fire immediately during submission
or cancellation/destruction, so establish request ownership before calling in.
Do not publish managed results from these callbacks.

Use the existing pinned c-ares headers/link dependency in the private Runtime
build. Do not upgrade the provider or silently rely on an ambient system resolver
library. Its [initialization contract](https://c-ares.org/docs/ares_library_init.html)
and [lookup options](https://c-ares.org/docs/ares_getaddrinfo.html) support the shared
lease and explicit external-readiness design; verify against the pinned local
1.34.9 source when implementing, rather than assuming newer documentation adds
available options.

This first version adds no shared DNS cache; explicit resolve lets an application
reuse endpoints. System DNS/hosts configuration follows the pinned provider's
behavior, not a promise of complete OS resolver equivalence. Tests must isolate
resolver configuration and cover cancellation of one lookup while another succeeds.

## 5. Platform and protocol boundaries

| Boundary | Required implementation policy |
|---|---|
| IPv4/IPv6 | Set `IPV6_V6ONLY=1` explicitly. Use two explicit listeners/sockets for both families; do not inherit divergent OS dual-stack defaults. Native Windows [IPv6 option documentation](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ipv6-socket-options) documents its default. |
| Bind ownership | No `SO_REUSEPORT` or UDP address sharing. Use Windows `SO_EXCLUSIVEADDRUSE`; initially leave Unix `SO_REUSEADDR` off. Rapid TCP restart may therefore get `address_in_use` during TIME_WAIT. Do not map Windows `SO_REUSEADDR` to a supposedly safe portable restart option; [Microsoft documents the different sharing risks](https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse). |
| Broken peer / SIGPIPE | Linux uses per-send `MSG_NOSIGNAL`; macOS uses per-socket `SO_NOSIGPIPE`. Do not globally change the embedding process's signal policy. See [send](https://man7.org/linux/man-pages/man2/send.2.html) and Apple's [socket-option reference](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/setsockopt.2.html). |
| TCP close / EOF | Use platform close/shutdown, disable application-configured positive linger, preserve cleanup errors. Do not blindly retry close on an integer that may already have been released/reused. [recv](https://man7.org/linux/man-pages/man2/recv.2.html) distinguishes stream EOF from datagrams. |
| Socket identity | Winsock SOCKET is not a POSIX fd or an `io.file`. No wrapping sockets as seekable files, inheritance, native handle extraction or arbitrary socket-option integers. |
| Async native calls | Prefer nonblocking operations/readiness on all three hosts. Do not combine overlapped Winsock ownership with POSIX-style close assumptions in this first implementation. |
| UDP errors | Surface native ICMP/reset/no-buffer/message-size errors; availability and timing differ. Do not claim delivery confirmation or disable Windows UDP errors silently. |
| Security / deployment | Ordinary OS firewall, permission and routing rules apply. TCP/UDP are plaintext, unauthenticated transports; no implied TLS, HTTP proxy routing or certificate checks. Existing HTTP retains its own secure path. |

No initial Unix-domain sockets, raw IP, ancillary fd passing, custom flags,
connected-UDP API, multicast membership, broadcast-enable API, interface discovery,
keepalive tuning or TLS-over-stream. Reject multicast send targets in this unicast
contract. Directed IPv4 broadcast may be rejected by the OS with broadcast sending
disabled; ordinary binds may still receive delivered broadcast traffic. No stronger
packet-filtering guarantee is implied. These exclusions are explicit scope choices,
not features described as already complete.

## 6. Practical examples and traps

Prospective examples below require the proposed modules; they were not compiled.
Use existing `with`, `?` and byte arrays, and numeric loopback binds by default.

```text
use net/address
use net/tcp
use net/error as net_error

func serve_once() -> result<i64, net_error.error>:
    var local = address.endpoint(ip = address.loopback_v4(), port = 7777)
    with tcp.listen(local)? as server:
        with server.accept(timeout_ms = 10000)? as peer:
            var total: i64 = 0
            for true:
                var block = peer.read_bytes()?
                if block.eof:
                    peer.close()?
                    server.close()?
                    return Ok(total)
                total += peer.write_bytes(block.bytes)?

async func duplex(peer: tcp.stream, const packet: [u8]) -> result<tcp.chunk, net_error.error>:
    var incoming = peer.read_bytes_async()
    var outgoing = peer.write_bytes_async(packet)
    var sent = (await outgoing)?
    return await incoming
```

A sync client connects, writes a small payload, calls `shutdown_write`, then reads
until EOF; it must concatenate/interpret bytes rather than assume the first read
is the full echo. Bounded length-prefixed game messages can use `encoding/binary`
and `io/memory`, validating the length before allocation. No duplicate socket
serialization framework is needed.

```text
use net/address
use net/udp
use net/error as net_error

func echo_packet() -> result<i64, net_error.error>:
    var local = address.endpoint(ip = address.loopback_v4(), port = 7778)
    with udp.bind(local)? as server:
        var packet = server.receive_from(max_count = 1200, timeout_ms = 10000)?
        var sent = server.send_to(packet.bytes, packet.peer)?
        server.close()?
        return Ok(sent)
```

The UDP client binds loopback port zero, sends to the explicit server endpoint,
then receives with a deadline. An empty datagram must still receive an empty echo.
Test oversized input followed by a valid packet to prove that truncation cannot
merge messages. Do not build an unauthenticated Internet amplification service
from this local illustrative echo; production protocol admission/rate policy
belongs to the application.

Async examples need the existing explicit async host/AOT adapter, as current
`examples/net_http/streaming_async.void` does. Do not quietly expand application
entry syntax or claim ordinary CLI async-main hosting in this networking sprint.

## 7. One engineering mission and acceptance

Implement shared operation/error/address contracts and native resource service;
add TCP/UDP sync operations and source facades; then bind those same operations
to existing external async completion and acquisition cleanup. Integrate c-ares
through the shared lease, followed by source examples and native qualification.
These are dependencies within one mission, not separate research approvals.

Expected components: `stdlib/net/{address,error,tcp,udp}.void`; a narrow native
socket provider/ABI and source-operation table; existing Runtime ledger and
external-job adapters; frontend capabilities/effects/IR validation and LLVM native
call lowering patterned after HTTP/IO. Extend install discovery, native resource
inventories, ABI/cache fingerprints and docs/LSP tests. Keep reply layouts bounded
and operation-specific rather than constructing unused large generic response
graphs. No new LLVM instruction, unsafe source cast or scheduler replacement.

Reuse `tests/async/external.cpp`, `tests/io/native_jobs.cpp`, HTTP local peers,
source-host/AOT adapters and `tools/test_platform.py` stack checks. Acceptance:

- Numeric address round trips, invalid formats, scope/port boundaries, both
  families and ephemeral ports; DNS NXDOMAIN/no-data/failure/deadline, multiple
  candidates, fallback, hosts behavior, and cancellation isolation using a local
  controlled resolver. No public Internet dependency for deterministic tests.
- TCP connect/listen/accept, complete writes and arbitrary read splitting,
  concurrent read/write, same-direction `busy`, partial send/deadline, graceful
  EOF/half-close versus reset, no SIGPIPE process termination, failed bind and
  accepted-stream survival after listener close. Force actual backpressure.
- UDP zero/ordinary/maximum payloads, oversized local validation, receive truncation
  with subsequent message intact, exact peer, family mismatch, non-EOF empty
  message, OS asynchronous errors and no silent retry/fragmentation by the library.
- Deterministic scheduling hooks around admission, native progress, completion,
  owner publication, task cancel, result-allocation failure, close and Runtime
  shutdown. Verify the proposed TCP cancellation-close rule and UDP consumption
  limits, repeated awaits, exact cleanup, descriptor reuse and stale completions.
- Force GC while operations wait; mutate original send arrays after submission;
  exercise aliases, readonly violations, source worker diagnostics, generation
  reload and cache hits/misses. No dangling pointers, foreign-Runtime resources,
  leaked accepted sockets or native buffers after quiescence.
- Run with one source worker and more pending accepts/reads than the worker count;
  peers must still progress and shutdown must drain. Simultaneous HTTP, TCP, UDP
  and DNS across multiple Runtimes must preserve shared provider lifetime.
- Normal/O2 JIT and independently linked AOT, Debug/Release/Linux strict sanitizers,
  real 1 MiB main/provider-thread stack validation and public-source installed
  modules. Record local loopback throughput/latency, idle CPU, pending-buffer and
  descriptor plateaus, cancellation latency and O(N) poll cost at representative
  1/32/128 connections; no invented speedup or release threshold.
- Native Linux x64, Windows x64 and macOS ARM64: actual bind flags, IPv6, error
  codes, scopes, noninheritance, shutdown and provider initialization. IPv6 disabled
  or unavailable infrastructure must be reported, not counted as passing IPv6.
  Host-controlled faults may need adapters; do not claim native parity from mocks.

Existing seven-job HTTP acceptance is a regression baseline, not socket acceptance.
Run focused existing HTTP/IO/async checks when shared initialization or ABI changes;
Boss controls any later hosted runs. Preserve exact `_crash.void` inputs and incident
evidence for every abnormal compiler/runner exit; retries do not erase failures.

**Owner decisions:** approve the conservative admitted-TCP-cancellation closure
rule; the explicit-family/no-port-sharing and resolver scope (including initial
sequential fallback); and the first unicast feature boundary/default resource
limits. Recommended answers are the policies specified above. No decision is
needed to preserve existing roots, error ordering, worker restrictions or HTTP.
Stop for Golden Wrench/Boss review before engineering.

## Batch 4 contract review

The authorized implementation starts at `77986362acf8b40d52aacbbdb324665b4484155b`.
The G04 worker adapter runs one operation until completion. A pending
socket read or accept could therefore occupy the only worker needed by
its peer. Sockets use `ExternalNativeJob` and its two completion tickets
instead. A native-only poll service per Runtime handles readiness, deadlines
and c-ares channels. Synchronous calls use the same service and wait in
`Runtime::Native`. The service holds the existing `http::ProviderLease`
through socket and resolver shutdown. HTTP and its initialization remain
separate from the socket operation queue.

The first supported targets are Linux x64 and Windows x64. Numeric
addresses store a private canonical representation, family and numeric
scope. The native boundary reconstructs validated binary socket addresses.
Network operations enforce explicit family, exclusive binds, unicast
destinations and the proposed sequential fallback. One c-ares channel
per lookup isolates cancellation and uses the already pinned headers and
library. DNS answers contain at most 32 distinct endpoints.

The service admits at most 1,024 native sockets, 2,048 ordinary operations,
32 lookups and 64 MiB of retained native payload and receive reservations
per Runtime. TCP transfers allow at most 4 MiB per operation. A positive
deadline is limited to 24 hours. Accept, TCP data and datagram operations
accept zero for no deadline. Direction reservations precede input copies.
A reserved close path and the original completion tickets retire native
state even when ordinary admission is full. The provider holds native
copies only. The executor roots wrappers and materializes results.

Cancellation of an admitted TCP data operation closes its stream before
retirement, including completion races. The same cleanup applies if
managed publication fails. Accept or connect abandonment closes an
unpublished stream. UDP cancellation retires the operation and can leave
one send or receive effect. Oversized UDP receives consume one message
and return its known size and sender without a truncated success.
