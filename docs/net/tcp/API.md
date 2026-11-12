# TCP listeners and byte streams

Import `net/tcp` and `net/address`. Every operation returns a checked
result. Wrap owned listeners and streams in `with` for scope cleanup,
or call `close` explicitly to inspect its result. Aliases share one
resource and closed state. Each accepted stream owns its own native
socket and can outlive its listener.

| API | Behavior |
| --- | --- |
| `connect_endpoint(remote, options)` | Connect to a numeric endpoint with a positive deadline. |
| `connect(host, port, options)` | Resolve and try candidates sequentially within one overall deadline. |
| `listen(local, options)` | Bind an explicit numeric endpoint and listen. Port zero requests a native ephemeral port. |
| `listener.accept(timeout_ms)` | Accept a separately owned stream. Default timeout is zero for an indefinite wait. |
| `local_endpoint()`, `stream.remote_endpoint()` | Report the bound address or the connected peer. |
| `stream.read_bytes(max_count, timeout_ms)` | Return an owned byte chunk up to the positive count or a successful empty EOF. |
| `stream.write_bytes(bytes, timeout_ms)` | Snapshot bytes and send the whole input or report exact local prefix progress. |
| `stream.shutdown_write()` | Send FIN after accepted bytes and preserve receiving. |
| `close()` | Release the native socket and wake pending operations. |

Factories, accept, read, write, half-close and close have explicit
`_async` variants. Endpoint queries use the owned address metadata.
`connect_options` selects an address family, an overall 30,000 ms
default and a 3,000 ms per-candidate default. Numeric connect uses the
overall deadline. Hostname fallback uses sequential attempts.
`listen_options` defaults to a backlog of 128 and no-delay for
accepted streams. Supported backlogs are 1 through 4,096.

TCP read and write default to 30,000 ms, with a default read size of
65,536 bytes. The per-operation transfer limit is 4 MiB. Accept and
data waits allow zero for an indefinite wait. Positive deadlines are
monotonic and capped at 24 hours. Progress does not renew a deadline.

One reader and one writer can be pending on a stream. A second
same-direction operation returns `busy`. One accept can be pending on
a listener. Half-close uses the write reservation and is idempotent.
It leaves reads usable and later writes return `write_closed`.
Repeated explicit close succeeds after native closure. A concurrently
reserved close can return `busy`.

TCP preserves byte order. Reads may split or combine writes. Successful
writes acknowledge local acceptance. The peer protocol must confirm
its own receipt or processing. A final empty read reports EOF after
buffered bytes have drained. A reset is a checked failure. On write
timeout, use the returned progress to avoid resending accepted bytes.

Cancellation of an admitted data operation closes that stream before
the task drains. Cancellation of accept keeps the listener usable and
closes any unpublished stream. Both synchronous and async APIs use
native readiness. Async operations leave the source executor available
for peers and other tasks.

Programs with an async entry use the existing native host or linked
async adapter. The echo examples use conventional synchronous CLI
entries and explicit loopback addresses.

[Echo server](../../../examples/net_sockets/tcp_echo_server.void) · [Echo client](../../../examples/net_sockets/tcp_echo_client.void) · [Networking errors](../error/API.md)
