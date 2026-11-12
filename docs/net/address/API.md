# Numeric addresses and name resolution

Import `net/address`. An `ip_address` owns a validated numeric address.
An `endpoint` pairs it with a host-order `u16` port.

| API | Behavior |
| --- | --- |
| `parse_ip(value)` | Strict dotted-decimal IPv4 or IPv6, optionally with an IPv6 numeric `%scope`. Returns `result<ip_address, net_error.error>`. |
| `format_ip(value)` | Canonical address without a port. Preserves a nonzero numeric scope. |
| `format_endpoint(value)` | Numeric address and port, with brackets around IPv6. |
| `ip.address_family()`, `ip.scope_index()` | Concrete `family.v4` or `family.v6` and host-local `u32` interface index. |
| `any_v4()`, `any_v6()`, `loopback_v4()`, `loopback_v6()` | Explicit wildcard and loopback values. |
| `resolve(host, port, options)`, `resolve_async(host, port, options)` | At most 32 distinct numeric endpoints in provider order. Accepts `family.any`, `family.v4` or `family.v6`. The default deadline is 5,000 ms. |

Parsing uses the same IPv4 decimal grammar on both hosts. Leading-zero
IPv4 components, shorthand, brackets, ports, mapped IPv6 addresses and
interface names are rejected. The `%scope` suffix uses a decimal
32-bit index. A zero scope leaves the interface unspecified. Port zero
is suitable for a bind and is invalid for remote connect or send.

Resolution accepts numeric literals and ASCII DNS names including
already encoded ACE labels. A final dot is allowed. Hostnames have at
most 253 characters excluding the final dot and labels have at most
63. The pinned c-ares provider reads the host's DNS and hosts-file
configuration. Numeric literals require no DNS query. Name service
plugins, mDNS, reverse lookup, service-name resolution and automatic IDN
encoding remain outside this interface. An empty provider result returns
`name_not_found`. A result with more than 32 distinct addresses returns
`limit_exceeded`.

Resolve deadlines must be positive and at most 24 hours. Async
cancellation retires that lookup's c-ares channel through the existing
task outcome. DNS names and addresses carry no authentication.

[Address example](../../../examples/net_sockets/addresses.void) · [Networking errors](../error/API.md)
