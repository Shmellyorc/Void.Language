# UDP datagrams

Import `net/udp` and `net/address`. `bind(local)` acquires an owned
socket in one explicit family. Port zero asks the operating system to
allocate a port. Use `local_endpoint()` to retrieve it.

`send_to(bytes, remote, timeout_ms = 30000)` accepts one complete
datagram or a checked failure. IPv4 admits at most 65,507 payload
bytes and IPv6 admits at most 65,527. The network path can impose a
smaller limit. The destination family must match the socket. Sending
an empty list transmits one zero-length datagram. Successful send
indicates local acceptance. Delivery, ordering and duplication remain
properties of the application protocol.

`receive_from(max_count = 65527, timeout_ms = 30000)` consumes one
datagram and returns its bytes and typed sender. A maximum of zero
accepts only an empty datagram. If a datagram exceeds the requested
capacity, the socket discards that message and returns
`message_too_large`. The error records consumption and the sender
and size where known. The next receive starts with the next
datagram. No truncated payload is returned as a success.

`bind_async`, `send_to_async`, `receive_from_async` and
`close_async` share the synchronous operations' semantics.
A socket admits one send and one receive concurrently. A second
operation in either direction returns `busy`. Positive deadlines
are monotonic and capped at 24 hours. A zero transfer timeout
disables the deadline. Cancellation may leave one datagram effect.

`close()` wakes pending operations and releases the native socket.
Use `with` for scope cleanup. IPv6 sockets are explicitly IPv6-only.
Binds are exclusive. Multicast, broadcast enablement and connected
UDP are outside the unicast interface.

[Loopback example](../../../examples/net_sockets/udp_loopback.void) · [Networking errors](../error/API.md)
