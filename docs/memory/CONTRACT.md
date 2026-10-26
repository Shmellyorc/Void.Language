> **26 October 2026 combined-sprint update:** Public failures now use managed
> errors derived from `error.error`; inspect `reason.kind` using `error_kind`.
> [The current combined contract](../combined/CONTRACT.md) and
> [implementation/evidence](../combined/IMPLEMENTATION.md) supersede the earlier
> source-protection and implementation-cost statements below. The original
> sprint receipt and source hashes remain historical evidence.

# io/memory — Sprint 2 contract and capability audit

Boss authorized this module on 26 October 2026 after independent Wrench source
review of encoding/binary at `64c485c5beb5df519e998fdb838a640fd9758376`.
This sprint implements only memory streams. Binary source, console, root README,
locked language/tooling contracts, Git and hosted CI remain outside its mutations.

## 1. Capability audit

Existing source managed structs provide stable shared identity; private fields and
compiler-inferred readonly effects prevent outside buffer/cursor mutation. Custom
managed init, empty array defaults, const byte-array parameters, independent numeric
slices and concrete generic receiver methods were checked with the current compiler.
Array growth, GC Frame/Slot roots, allocation failure unwinding and managed field
publication already exist. String `to_utf8()` creates owned bytes;
`string.from_utf8()` strictly validates and returns a recoverable result. Binary
encode/decode already support all ten numeric types and both byte orders.

No new language syntax, intrinsic, IR opcode, Runtime ABI, native provider,
collection, GC or allocator is required. There is no numeric sizeof operation in
source: numeric read width is obtained from `binary.encode<t>(0).count`, retaining
one authority for supported numeric families and widths. No numeric bit conversion
or UTF-8 validator is copied into this module.

## 2. Public API

Import `use io/memory`; the module binds as `memory` (explicit aliases also work).
Import `encoding/binary` when naming its canonical `byte_order`; there is no
second endian enum or source reexport mechanism.

* `error`: `invalid_range`, `unexpected_eof`, `invalid_utf8`, `limit_exceeded`.
* `stream(const bytes: [u8] = [], order: binary.byte_order = binary.byte_order.little)`:
  one managed read/write stream, initially at zero, with an independent copy of
  bytes. Constructor is infallible in the domain-error sense; allocation failures
  use RuntimeFailure. The empty default is an ordinary supported array default.
* Readonly properties: `position: i64`, `length: i64`, `remaining: i64`, `eof: bool`,
  `order: binary.byte_order`. Byte order is chosen at construction for that stream.
* `seek(position: i64) -> result<i64, error>`: absolute positions 0..length inclusive;
  success returns the new position. Negative, beyond-end and overflowed caller
  positions are invalid_range; no negative-index or sparse-gap convention applies.
* `to_bytes() -> [u8]`: independent writable snapshot of the entire logical buffer,
  independent of position and future stream writes. Does not move the cursor.
* `read_bytes(max_count: i64) -> result<[u8], error>`: up to max_count bytes, advances
  by the returned count. Negative is invalid_range; at EOF returns empty Ok.
  Zero returns empty Ok without moving. `eof` explicitly reports current exhaustion;
  an empty result also occurs for a zero-count request before EOF.
* `read_exact(count: i64) -> result<[u8], error>`: exactly count bytes or
  unexpected_eof; negative is invalid_range. Zero succeeds, including at EOF.
* `write_bytes(const bytes: [u8]) -> result<i64, error>`: overwrite at the cursor,
  extend at the end as needed, preserve an existing tail, return the bytes written.
  Empty input returns zero without changing position or contents. Checked i64
  position/growth overflow is invalid_range; allocation/physical representability
  limits retain RuntimeFailure rather than becoming EOF or a fake size cap.
* `read<t>() -> result<t, error>` / `write<t>(value: t) -> result<i64, error>`:
  exactly i8/u8/i16/u16/i32/u32/i64/u64/f32/f64, using the stream's byte order and
  binary.encode/decode. Write infers t; read needs an explicit numeric type.
  Preserve all two's-complement and IEEE bits, including signaling NaN payloads.
* `read_string(count: i64) -> result<string, error>`: exact UTF-8 byte count,
  strict validation, invalid_range/unexpected_eof/invalid_utf8 as appropriate.
* `write_string(value: string) -> result<i64, error>`: raw UTF-8 bytes, with no
  prefix, newline, terminator, BOM insertion or normalization; returns byte count.
* `read_pascal_string(max_bytes: i64 = i64.max_value) -> result<string, error>`:
  u32 byte-length prefix in stream order, then that many strict UTF-8 bytes.
  Negative max is invalid_range; prefix greater than max is limit_exceeded;
  incomplete prefix/payload is unexpected_eof. The limit is checked before payload
  slicing/allocation. Empty strings use a zero prefix and no payload bytes.
* `write_pascal_string(value: string) -> result<i64, error>`: check UTF-8 byte length
  fits u32 before narrowing; write the prefix and payload as one operation. Return
  total bytes, including four prefix bytes. An unrepresentable u32 length is
  invalid_range. Embedded NUL is data, not a terminator.

## 3. Failure, ownership and publication

Every recoverable error leaves cursor and contents unchanged. Reads validate and
allocate their output before advancing. Pascal reads validate the entire frame
before advancing past either prefix or payload. Validation precedence is explicit:
negative arguments, prefix availability, caller limit, payload availability, UTF-8.

Writes stage an independent replacement buffer, overwrite/extend that private copy,
then publish one completed private state reference. State holds immutable bytes,
position and order. Stream aliases retain the public stream identity across state
replacement. Seeks/read advances share the private immutable bytes and publish a
new state; snapshots and constructor inputs are always independently copied.
Existing managed publication provides a single commit point: Runtime allocation
failures preserve the old visible state and ordinary roots unwind safely. There
are no native resources, close/deinit protocol or borrowed GC payloads.

This deliberately simple source implementation makes writes linear in the entire
buffer size and stages an additional full copy; repeated small writes can be
quadratic. Reads/seeks allocate a small state object, and numeric reads additionally
encode a zero solely to obtain width. These are disclosed costs, not an alternate
buffer engine. Optimizing them should follow measurements and separate review,
without weakening state/error semantics or duplicating the codec.

A const stream permits observational properties/to_bytes, but cannot seek, read or
write because those change shared state. Const input byte arrays are accepted and
copied. There is no cross-worker synchronization or borrowed-buffer ownership mode.
There are no unresolved language-feature decisions or speculative public overloads.
