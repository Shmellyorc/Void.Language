# Standard library

`use error as core` exposes the three-field managed `core.error` base. Every
installed module uses a derived public recoverable error; result error types
remain exact. `use convert` provides checked numeric/Boolean/payload-free-enum
parsing, ordinary `to_string`, checked `unwrap` and eager `unwrap_or`.
`use console` adds ordinary `readline(prompt = "")` and `read(max_scalars = 1)`
convenience over the checked console queue. See the
[current combined contract](../docs/combined/CONTRACT.md) for grammars, errors,
ownership, console coverage and performance guarantees.

`use io/memory` provides an owned `memory.stream(bytes = [], order = ...)` with a
shared cursor, independent `to_bytes()` snapshots, bounded seeking, up-to/exact
byte reads, numeric `read<t>()`/`write(value)` through the binary codec, strict raw
UTF-8 and u32-length-prefixed Pascal strings. Failed operations preserve the old
state. Writes reuse geometrically grown private backing; see [the contract](../docs/memory/CONTRACT.md) and
[local implementation evidence](../docs/memory/IMPLEMENTATION.md) for exact costs,
tests and review limits.

`use encoding/binary` provides `encode(value, order)` and `decode<t>(bytes, order)`
for all ten fixed-width numeric primitives. Little endian is the default; explicit
big endian is supported. Encoding returns fresh owned bytes, decoding requires an
exact width and returns a recoverable managed error with
`kind == error_kind.wrong_width`, and IEEE bits are preserved.
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
Time-zone rules are embedded in linked Runtime bytes. Stage 10 and Stage 11 locks
remain separate from this standard-library sprint.
