# Binary numeric encoding — Sprint 1

Boss authorized only `encoding/binary` on 26 October 2026. Stage 11 and the
documentation/hover lock remain unchanged. Independent Wrench review and
Boss-controlled cross-platform acceptance remain separate from implementation.

## Capability audit and design

The numeric contract and `src/numeric.cpp` support i8/u8, i16/u16, i32/u32,
i64/u64 and IEEE f32/f64. Numeric `as` is checked value conversion, not bit
reinterpretation. General source bitwise operators and compile-time numeric type
branches are not available. Ordinary function overloading is not available.
Existing generic specialization reparses bodies with concrete type bindings.
Results, enum patterns, const array parameters, list indexing/push and source
loops are sufficient for the codec. Existing typed IR `bitcast` validates equal
numeric widths and LLVM lowers it without arithmetic. No new IR operation,
Runtime ABI, native provider, GC, allocator or public language syntax is needed.

Three private compiler operations, recognized only inside the canonical
`/@stdlib/encoding/binary.void` module, supply numeric width, bits to u64, and
u64 bits to a concrete numeric type. Unsupported types fail specialization.
Narrowing uses the existing checked unsigned conversion before the equal-width
bitcast. Source code owns all byte order, buffer validation and codec loops.
Future stream adapters can call these same source functions on exact byte chunks.

## Complete public surface

`use encoding/binary` binds the module as `binary`; an explicit alias is allowed.

* `byte_order`: `little` (least significant byte first), `big` (most significant
  byte first).
* `error`: `wrong_width`, returned for both short and oversized inputs.
* `encode<t>(value: t, order: byte_order = byte_order.little) -> [u8]`:
  infer t from the value or supply it explicitly. Returns a fresh owned array of
  exactly sizeof(t) bytes. Does not numerically convert the value.
* `decode<t>(const bytes: [u8], order: byte_order = byte_order.little)
  -> result<t, error>`: t must be explicit, because it occurs only in the result.
  Accepts exactly sizeof(t) bytes; returns Err(error.wrong_width) otherwise.
  Reads without modifying or retaining the input. No offset, trailing bytes,
  partial reads or cursor semantics are implicit.

Both functions accept exactly the ten numeric primitives above. Bool, enums,
strings, arrays and structs are rejected at compilation, including nullable and
non-numeric generic substitutions. Integer representations are two's complement;
all bit patterns are valid. Every IEEE pattern is valid, including subnormals,
signed zeros, infinities, quiet/signaling NaNs and their sign/payload bits.
The codec performs no floating arithmetic and does not normalize NaNs.
There are no malformed exact-width numeric representations; length is the only
recoverable data error. Allocation/representability failures retain the existing
RuntimeFailure policy, as with ordinary source arrays.

Little endian is the portable default recommended by STAGE11_RESEARCH.md;
big endian is explicit. Neither depends on CPU endianness. Bytes encode numeric
representation, not decimal text, string conversion, serialization or parsing.
No typed wrapper family, streaming framework, bool wire format, public bitcast,
offset overload or extra width API is introduced. These choices use existing
language capabilities and the approved direction; no unresolved language feature
is required. Wrench may review the public names and the single wrong_width case.

## Ownership

Encoding builds a fresh `[u8]` through existing Runtime-backed list operations.
All temporaries use ordinary precise roots and failure unwinding. Decoding
validates count before any indexing and assembles at most eight unsigned bytes.
No raw pointer, borrowed payload, host byte layout or native resource is involved.
Previously returned arrays and caller inputs remain independent on later failure.
