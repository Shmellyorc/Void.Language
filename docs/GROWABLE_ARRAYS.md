# Growable arrays

`[T]` lists execute through the production frontend, typed IR, shared LLVM
19.1.7 JIT/AOT lowerer and coordinated Runtime. Research builds are unnecessary.
Elements support all existing numeric types and bool, strings, value structs,
managed/nullable and interface references, and nested fixed/growable arrays.

```typescript
func entry(value: i32) -> i32:
    var numbers: [i32] = []
    numbers.push(10)
    numbers.push(value)
    numbers.push(30)
    var alias = numbers
    alias.push(40)
    var removed = numbers.remove_at(0)
    return numbers.count as i32 + removed
```

Input 7 returns 13. Run with `voidc run examples/growable_arrays/main.void 7`
(default O2) or append `--no-opt`. Emit with the existing `voidc emit` command
and link with `void_runtime`, as the growable-array CLI targets demonstrate.

## Behavior

- Typed `[]`, nonempty literals and omitted initializers create fresh identities.
  `var values = [1, 2]` infers `[i32]`; unannotated `[]` requires a type context.
  Literal expressions evaluate once, left to right. Module constants remain pure
  templates, materialized afresh on use with readonly list authority.
- Assignment, arguments, returns and inline aggregate copies preserve the same
  list identity. Defaults in separate aggregate/managed instances are independent.
  Fixed `[T, n]` arrays retain their independent value-copy behavior.
- `.count` returns i64 for both fixed and growable arrays. `.push(value)` returns
  nothing. `.remove_at(index)` returns the removed element and shifts the suffix.
- Every existing signed/unsigned integer type can index. `-1` is last and `-count`
  is first; empty/out-of-range accesses fail with source locations. Bool/floating
  indexes reject at analysis. Indexed assignment and existing numeric compounds
  are checked before evaluating their RHS.
- Selected writes retain a rooted original header, normalized index, structural
  version and compound old operand. Rebinding during the RHS cannot redirect
  selection. Any intervening push/remove invalidates the write, including a
  remove/push restoring the same count. Nonstructural writes remain visible.
  Earlier RHS side effects are preserved when a selected commit fails.
- Existing readonly leaf/effect analysis applies to list handles, including
  aliases, fields, arguments and returns. Independent IR validation also rejects
  mutation through readonly authority.

## Storage and safety

A list is an ordinary GC object containing backing/count/capacity/version, with
one precise edge to an existing typed GC buffer. No allocator, collector,
ownership registry, private calling convention or certification path was added.
Empty lists need no buffer. Backing grows from capacity 4 geometrically with
checked arithmetic; count is representable as i64 and bytes must fit existing
Runtime layout/address limits. There is no 64 KiB or fixed megabyte ceiling.

Existing Frame/Slot roots protect headers and old/new backing during allocations,
copies and polls. Growth copies through the existing leased BufferBuilder and
publishes the complete replacement only after success and version revalidation.
In-capacity writes/removal update bytes, live prefix and header under the Runtime
lock without intervening safepoints. Failed allocations preserve identity, count,
contents and version; unpublished allocations are reclaimed normally.

Removal is O(count-index), retains capacity and performs its element memmove
under the Runtime lock. Lists currently retain the compiler's existing temporary
root lifetimes. These are implementation costs, not new language restrictions.
This is coordinated GC safety, not a new source-level concurrent collection API.

## Verification and scope

`growable_arrays_source` covers normal/O2 numeric/bool source execution,
aliases/defaults/fields, diagnostics, independent IR negatives, live GC roots,
concurrent collection, allocation-failure sweeps, transactional cleanup and retry.
`growable_arrays_cli` executes real examples through normal/O2 JIT and linked
native AOT, including checked failures and a payload exceeding 2 MiB (4 MiB
backing capacity). The large example also runs under a 1 MiB native stack.
Exact local commands/results are recorded in the growable-array WORK_LOG entry.

The additional `arrays_source` and `arrays_cli` suites cover composite/nested
elements, traversal and ranges described below. List equality, slicing, sorting
and generic collections remain outside this feature. Cross-platform
execution remains for hosted CI; local evidence is Linux LLVM 19.1.7.

## Phase 23: composite elements and traversal

The same typed buffer implementation traces strings, managed/interface references,
inline value fields and nested array handles. Stored interface pairs retain admitted
witness metadata independently of allocating code. Recursive value types through
growable fields are supported; inline infinite-size recursion remains invalid.

List assignment shares identity. Fixed arrays inside a list element are copied
when that element is stored, read, passed or returned; growable fields retain
identity. Required nonnullable references need explicit initialization, except in
zero-length arrays. Defaults run separately for every element, including custom
initializers and fresh empty list fields. Readonly element capabilities survive
aliases, calls, nested copies and iteration. Mutable list element profiles are
invariant, preventing aliases from widening and then restoring mutation authority.

Nested indexed/field writes preserve selected storage and left-to-right evaluation.
Structural modification invalidates a pending list-dependent selection. Selecting
another growable or managed reference captures that identity and ends dependency
on its containing value path. Inline mutating methods on array elements remain
rejected; explicit indexed/field assignment is supported.

```typescript
for item, original_index in items:
    pass
for item, original_index in reverse items:
    pass
for index in 0..<items.count:
    pass
```

Traversal evaluates its collection once, performs no reversed collection allocation,
and supplies an optional original i64 index. Value elements are independent copies;
reference elements retain identity. Replacing an element is nonstructural and is
visible when that position is visited. Rebinding loop variables does not write back
or change traversal. Nested loops, break, continue and return use existing control
flow. Push/remove invalidate forward and reverse iterators at the next access,
**including completion**. Mutation followed immediately by break/return is allowed.
There is no reverse-remove-current exception.

Ranges accept matching i32 or i64 endpoints, evaluated once left to right. `a..b`
includes both endpoints; `a..<b` excludes b. Direction follows endpoint order;
`reverse` reverses that sequence. Equal inclusive endpoints yield one item, equal
exclusive endpoints none. Terminal checks precede stepping, including integer
minimum/maximum boundaries; no range is materialized and no span is subtracted.
Other endpoint types, mixed endpoint widths, steps and range index binders reject.

See examples/arrays and the arrays_source/arrays_cli production suites. Slices,
array equality/search, user iterators, dictionary traversal and additional collection
APIs remain outside this implementation.
