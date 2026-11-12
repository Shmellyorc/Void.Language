# Checked networking errors

Import `net/error as net_error` for the shared error type. TCP, UDP,
address parsing and resolution return `result<T, net_error.error>`.

`kind` is the portable category. `operation` and `phase` identify the
failed API and native stage. `native_domain` and `native_code` preserve
POSIX, Winsock or c-ares diagnostics where the provider supplied them.
A portable validation or deadline can have a zero native code.
`cleanup_code` retains a separate native close failure.

`bytes_transferred` is the exact locally accepted TCP write prefix
or the known byte count of a consumed UDP message. A timeout leaves
an otherwise healthy stream usable. `connection_reset` is distinct
from TCP EOF. When hostname connect fails, `peer` and
`attempted_count` describe the last attempted endpoint and the number
of attempts.

An oversized received UDP datagram sets `datagram_consumed`. It
reports the numeric sender in `peer` when available and the known
size in `datagram_size`. A size of `-1` means unknown or inapplicable.
The error's `partial` list is empty when a datagram has been discarded.

Structured task cancellation uses the existing cancellation outcome.
A canceled TCP data task can have affected the peer before the stream
is closed. UDP cancellation can have sent or consumed one datagram.

[Address and DNS](../address/API.md) · [TCP](../tcp/API.md) · [UDP](../udp/API.md)
