# Standard library

`use io/memory` provides an owned `memory.stream(bytes = [], order = ...)` with a
shared cursor, independent `to_bytes()` snapshots, bounded seeking, up-to/exact
byte reads, numeric `read<t>()`/`write(value)` through the binary codec, strict raw
UTF-8 and u32-length-prefixed Pascal strings. Failed operations preserve the old
state. Writes stage a full copy; see [the contract](../docs/memory/CONTRACT.md) and
[local implementation evidence](../docs/memory/IMPLEMENTATION.md) for exact costs,
tests and review limits.

`use encoding/binary` provides `encode(value, order)` and `decode<t>(bytes, order)`
for all ten fixed-width numeric primitives. Little endian is the default; explicit
big endian is supported. Encoding returns fresh owned bytes, decoding requires an
exact width and returns recoverable `error.wrong_width`, and IEEE bits are preserved.
See [the binary contract](../docs/binary/CONTRACT.md) and
[implementation evidence](../docs/binary/IMPLEMENTATION.md).

`use time` provides validated Gregorian dates, times, local/offset/zoned values,
full-range seconds-based instants and durations, checked arithmetic, bundled IANA
rules and UTC/monotonic clocks. Components are private const fields with public
read-only properties and recoverable factories. Formatting uses the shared Runtime
kernel through ordinary source display/format hooks. Clock precision queries report
native resolution in nanoseconds, not UTC accuracy. Monotonic samples belong to
one Runtime context; compare/elapsed reject mismatched contexts.

The compiler owns only narrow checked native operations; library policies and
public values use ordinary VOID source. Installed CLI/LSP tools discover matching
sources under `share/void/stdlib`, with `VOID_STDLIB_ROOT` as an explicit override.
Time-zone rules are embedded in linked Runtime bytes. Stage 10 remains incomplete;
see `docs/stage10/IMPLEMENTATION.md` for exact acceptance and remaining systems.
