# Combined standard-library sprint — 26 October 2026

Boss authorized standard errors, console completeness, conversions and measured
memory-write optimization together. This supersedes the earlier console/binary
source protection only for these migrations. Root README, hosted CI, branches,
commits and pushes remain outside this sprint. Golden Wrench review is separate.

## Installed modules and error identity

| Installed source | Public failure payload | Canonical base | Additional fields |
| --- | --- | --- | --- |
| `error.void` | `error.error` | canonical managed base | exactly message:string, path:string, code:i32 |
| `console.void` | `console.io_error` | `error.error` | operation, stream, kind, os_code, bytes_transferred, partial, cleanup_code, partial_utf16_units |
| `encoding/binary.void` | `binary.error` | `error.error` | kind:binary.error_kind |
| `io/memory.void` | `memory.error` | `error.error` | kind:memory.error_kind |
| `time.void` | `time.error` | `error.error` | operation:string |
| `convert.void` | `convert.error` | `error.error` | kind:convert.error_kind, input:string |

`use error as core` gives unambiguous `core.error`. Existing init-free named
construction supplies ordinary empty strings and zero for omitted base fields;
empty messages remain legal. `path` is a relevant resource/location, empty if none.
There is no global code registry or exception framework. Ordinary object upcasts
share managed identity. Results retain exact error types: `?` diagnoses unequal E
at the source rather than relying on a later IR rejection. Explicit object upcasts
remain valid. No implicit result widening or string-to-error conversion exists.

Binary wrong-width has code 1. Memory range/EOF/UTF-8/limit categories have codes
1/2/3/4. Console retains its portable kind number as code, separate from full-width
native os_code. Time currently leaves code unspecified (zero) and operation `time`
or `monotonic`, preserving its original detailed native message. Convert grammar
and range failures have codes 1/2. All five types inherit the three base fields,
without duplicate fields. Error-kind enums remain ordinary categorization enums.

Time's private kernels and `_checked<t>` bridge retain internal string results.
Public factories, arithmetic, clocks and explicit `value.format(spec)` return
managed temporal errors. Built-in `format(value, spec)` keeps its accepted
`result<string,string>` display protocol: one compiler-owned canonical temporal
adapter extracts the error message explicitly. Default string hooks use that same
bridge and checked formatter failure. Arbitrary user display hooks retain their
exact accepted signatures. Runtime failures and cancellation stay distinct from Err.

## Console

Use `use console` for ordinary unqualified `readline` and `read`; an explicit
module alias provides qualified calls. These are normal source declarations under
the existing import/name-collision rules, with compiler-owned help and navigation.

* `readline(prompt:string="")->string`: write the exact nonempty prompt completely,
  acknowledge a serialized flush, then read one logical line without LF/CRLF/CR.
  Prompt write/flush failure returns empty without consuming input.
* `read(max_scalars:i64=1)->string`: up to the requested Unicode scalar count.
  Zero yields empty; negative counts, EOF and recoverable read failures yield empty.
* Empty entered lines also yield empty. These helpers deliberately discard detailed
  failures. Allocation/Runtime failures retain checked failure semantics.

Both helpers use the existing single queue and strict UTF-8 decoder. Existing
checked char/text/line/byte reads, EOF variants, timeout and partial progress,
stdout/stderr, print/write/flush, terminal capabilities/dimensions/cursor/visibility,
16 colors/reset/clear/title/bell, key metadata/availability and scoped mode cleanup
remain available. No second input path or native terminal implementation is added.
All public console declarations, enum payloads, explicit parameters and meaningful
returns carry compiler-owned documentation.

## Convert

`use convert` exposes `parse<t>`, `to_string<t>`, `unwrap<t,e>` and
`unwrap_or<t,e>`, plus managed `error` and `error_kind`.

`parse<t>(text:string)->result<t,error>` accepts exactly the eight integer
primitives, f32/f64, bool and closed payload-free enums. Nullable, collection,
managed/value object, result, callable and payload-bearing enum targets fail
specialization. There is no runtime type reflection.

* Text is consumed completely. No leading/trailing/internal whitespace, separators,
  radix prefixes, numeric type suffixes, embedded NUL or trailing junk is accepted.
* Integers use decimal digits with optional leading + or -. Leading zeros are
  accepted. Unsigned negative spelling (including -0) is a range error. All exact
  signed and unsigned minima/maxima are supported. Overflow is recoverable.
* Floats accept finite decimal significands with digits on either side of an
  optional dot (at least one digit), optional sign, and optional e/E exponent with
  optional sign and required digits. Hex, NaN and infinity spellings are rejected.
  Conversion uses locale-independent `from_chars` into the actual target width:
  correctly rounded target precision, without a double intermediate for f32.
  Overflow and underflow outside the target range, including nonzero rounded to
  zero, return range errors. Representable subnormals and negative zero survive.
  Decimal-to-binary rounding is explicit, not an exact-decimal promise.
* Bool accepts exactly `true` and `false`, case-sensitive.
* Enums accept exact case names or the canonical qualified name produced by the
  existing display formatter. Names are case-sensitive. A closed compiler-emitted
  case table reuses existing enum metadata and introduces no reflection API.
* Invalid grammar wins over range when both could apply. Errors retain original
  text, a useful message, an empty path and the category/code above.

`to_string<t>(const value:t)->string` delegates to existing `string(value)`;
it supports the same displayable types and hooks, not another formatter. Enum
output round-trips through parse of the same payload-free enum. Existing opaque
object displays and nonfinite float displays do not acquire parsing promises.
Ordinary generic-definition visibility applies: the helper's defining module does
not inherit its caller's imports. A custom hook outside that module's visible
imports retains the ordinary opaque display. Use `string(value)` in the caller
when its imports provide the desired custom hook. No implicit hook visibility or
generic lookup rule is added by this sprint.

`unwrap<t,e>(outcome:result<t,e>)->t` preserves successful payload type, GC roots,
identity and readonly provenance. Err raises the existing checked Runtime failure
with source location and a diagnostic directing the caller to inspect/use unwrap_or.
It does not auto-display arbitrary errors or introduce exception conversions.
`unwrap_or<t,e>(outcome,fallback:t)->t` returns Ok's payload or fallback. Both written
arguments evaluate normally once, left to right; fallback is eager even on success.
Byte/text and numeric wire conversion remain existing string UTF-8 and binary APIs.

## Transactional memory growth

Public stream methods, readonly rules, cursor aliases, checked seeks/overwrites,
UTF-8/Pascal/endian semantics and independent constructor/read/snapshot ownership
are preserved. Error categories now live in managed `error.kind`.

Each stream privately owns one existing Runtime u8 list. Source allocates its next
state/cursor before asking a canonical-module-only bulk operation to write bytes.
The Runtime checks roots, schemas, extents, leases and version before mutation.
In-capacity writes use one memmove. Growth reserves max(required, twice capacity)
with the existing typed BufferBuilder/GC/quota allocator, copies only the old
prefix, incoming bytes and retained tail, then publishes backing/count together.
No second allocator or buffer abstraction exists. Source performs only the proven
managed reference store and result construction after success; it performs no
fallible count lookup, allocation or safepoint after byte publication.

OOM/domain failures leave bytes and position unchanged. Inputs remain readonly;
private buffers cannot escape through snapshots. Retained capacity can exceed
logical length. Repeated appends perform amortized linear total byte copying;
individual growth still copies existing data. Reads/seeks still allocate a small
state; numeric reads still obtain width through binary.encode<t>(0). These costs
and ordinary single-owner/no-worker-synchronization rules remain unchanged.
