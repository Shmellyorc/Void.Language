# Numeric fixed arrays

Order 3 implements real source `[T, n]` arrays through the existing typed IR,
shared LLVM 19.1.7 lowerer, ORC JIT, ordinary AOT writer and GC Runtime. This
feature is enabled in default builds; `VOID_LLVM_RESEARCH` is not required.

```typescript
func entry(value: i32) -> i32:
    var numbers: [i32, 3] = [10, 20, 30]
    numbers[1] = value
    return numbers[0] + numbers[1]
```

```sh
build/debug/voidc run examples/fixed_arrays/main.void 7 --no-opt
build/debug/voidc run examples/fixed_arrays/main.void 7
build/debug/voidc emit examples/fixed_arrays/main.void -o main.o
```

The program returns 17. Link generated objects with the existing `void_runtime`
CMake target and the host C++ toolchain, as the fixed-array CLI tests do.

Supported elements are i8/i16/i32/i64, u8/u16/u32/u64, f32/f64 and bool. Literals
require fixed-array context and exactly n elements. Defaults initialize all
numeric/bool elements to zero/false; zero-length arrays and contextual `[]` work.
Array constants, value-struct fields and managed-struct fields use the same path.
Existing source readonly/type checks remain in force.

Assignments, argument evaluation and returns create independent array storage.
Argument snapshots finish before later arguments run. Selected writes evaluate
owner/index once, check bounds before the RHS, snapshot compound old values,
and reacquire current field storage after RHS effects. Unrelated RHS writes are
preserved. Every integer type is a valid index, with `-1` last and `-n` first.
Out-of-range indexes, signed minimum and unsigned maximum fail safely through
the existing source diagnostic/failure channel. Floating and bool indexes reject
at source checking.

Numeric bits, including signed narrow integers, floating NaN and negative zero,
are preserved by scalar bit transport. Array handles and handles embedded in
value/managed fields are included in precise Frame/Slot and heap metadata.
Registered copy staging prevents partially copied aggregate values from becoming
visible. Checked helper failures leave frames normally and preserve existing
result values. Runtime leases, polling and copy transactions are reused.

Payloads live in existing GC buffers. Native values and arguments contain handles;
LLVM handles their ordinary ABI transport. No payload-sized native alloca or
64 KiB production limit is introduced. Chunk quotas bound work between safepoints,
not the maximum array size. Tests execute 2 MiB arrays through normal/O2 JIT and
linked AOT under a 1 MiB stack.

The initial implementation uses eager copies, including functional indexed
updates and rooted staging. It can make redundant copies and retain instruction
temporaries until function exit. No zero-copy const-formal borrowing, performance
claim or unrelated optimization is included in this order.

Nested array elements, struct/string/reference elements, growable arrays,
array-specific iteration/ranges and `.count` are not implemented by this order.
Unsupported native types still fail the whole compilation explicitly. Existing
scalar loops and function calls can manipulate fixed arrays normally.

Focused tests are `fixed_arrays_source` (forced GC, roots, failure injection and
publication), `fixed_arrays_cli` (real source normal/O2 JIT and linked AOT), and
`arrays_foundation` (analysis/typed IR/negative cases). See WORK_LOG.md for exact
local commands/results and platform limitations. Orders 1 and 2 remain intact;
research certificates are optional and do not control source-array acceptance.

## Order 4 Runtime efficiency

Measured root-token lookup previously traversed the same Frame/Slot list twice,
and source-array access constructed a fresh capacity layout on every operation.
Lookup now returns its matching Frame directly and stops membership scans when
found, retaining originating-frame lifetime and exact-slot checks. Array
read/write/copy helpers reuse the source buffer's immutable layout, checking the
same admitted schema, default packing, extent and complete publication state.
Empty arrays still use the ordinary layout factory. Payload copies remain
independent; metadata sharing does not share mutable array storage.

Untyped publication now first checks two conservative address bounds held by the
existing Heap. Writes provably outside all buffer storage skip the object-table
scan. Potential overlaps still use the original precise alias/read-lease checks,
including pointer-sized writes beginning just before a buffer. Bounds widen only
after successful adoption; stale bounds after collection can cause extra scanning
but cannot hide an alias. The next adoption resets bounds after an empty buffer
heap. No allocation registry, global cache or collector change was introduced.

Local Release measurements showed source reads with 1,024 roots falling from
4.15 us to 0.282 us; a 5,000-update real source program fell from 62.68 ms to
41.54 ms with optimized AOT. The existing 2 MiB example showed no material timing
change. These are host-specific measurements, not performance guarantees.
Frame entry/exit allocates no heap storage. Root storage reuse/lifetimes and eager
payload copies were left unchanged. Worst-case root scans and potential-alias
scans remain linear, and schema-key lookup still allocates one small string per
source access. See the Order 4 WORK_LOG entry for reproducible commands, baseline,
allocation counts, focused tests and measurement limitations.
