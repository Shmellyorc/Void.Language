# Expanded array operations (Phase 25)

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

The owner approved the following bounded contract on 18 October 2026. These
operations use the existing frontend, typed IR, LLVM 19.1.7 JIT/AOT lowerer,
typed buffers and coordinated Runtime. No research build is required.

```typescript
func entry(value: i32) -> i32:
    var items = [10, 30]
    items.insert(1, value)
    var copied = items[-1..0]   # [30, value, 10], independent outer list
    items.clear()
    return copied[1]
```

Input 7 returns 7. All existing numeric/bool, string, managed/nullable/interface,
value-struct, nested-array and concrete generic element types are supported.

## Insertion and clearing

`items.insert(index, value)` returns nothing and inserts before the selected
element. Valid nonnegative positions are 0 through count; count appends. Negative
positions use existing element normalization: -1 inserts before the last element,
-count before the first. An empty list accepts only 0. Existing integer index
types are accepted; bool and floating indexes reject.

The receiver and index are evaluated once, in that order. The insertion boundary
is checked before evaluating value. The selected list identity, normalized index
and structural version survive value evaluation. Rebinding does not redirect the
insertion; any structural change during value evaluation invalidates it. Earlier
value/index side effects remain visible when the selected insertion fails.

`items.clear()` returns nothing, preserves identity and capacity, and removes all
live elements. Aliases observe the empty list. Vacated reference words are zeroed
and the buffer live prefix becomes zero, so the collector no longer retains removed
elements through that backing. Existing compiler temporaries can retain their
ordinary roots until function exit; no liveness optimizer is introduced.

A successful insertion increments the structural version once. A nonempty clear
increments it once; clearing an already empty list is a no-op. Both structural
operations invalidate active forward/reverse iterators at the next access,
including completion, under the Phase 23 contract. Immediate break/return remains
allowed. Nonstructural element replacement retains its existing behavior.

## Slices

`source[first..last]` includes last; `source[first..<last]` excludes it. Both fixed
and growable sources produce a fresh growable `[T]`, including empty slices.
Both endpoints are required initially. Each endpoint accepts any existing integer
index type, independently; bool/floating endpoints reject. This does not expand
loop-range endpoint types.

Negative endpoints normalize using the source count before direction is chosen.
Direction follows the normalized endpoint order, so `[3..1]` selects 3, 2, 1 and
`[3..<1]` selects 3, 2. Inclusive endpoints must select elements. An exclusive end
may equal count. Equal exclusive boundaries produce an empty list, including
`empty[0..<0]` and `items[items.count..<items.count]`; every nonempty slice must
start at a valid element. Invalid bounds fail with source locations; no clamping,
wrapping or implicit endpoint is introduced.

The receiver, first endpoint and last endpoint execute once, left to right.
Bounds are checked afterwards against the captured collection. Fixed sources
retain their ordinary value snapshot before endpoint effects; growable sources
retain the selected identity and copy its current contents after endpoint effects.
Rebinding the source place cannot redirect the operation.

The outer collection is independent. Fixed children copy recursively, value structs
copy their value fields, managed/list elements preserve referenced identity and
strings preserve immutable content. Readonly authority of embedded referents is
preserved through slices, aliases, calls and generic specializations. Copying a
readonly numeric array produces writable independent numeric storage; it does
not grant mutable authority to referenced objects/lists.

## Storage and failure handling

Push and insertion share one Runtime implementation: in-capacity insertion shifts
under the existing lock; growth uses the existing leased BufferBuilder to stage
prefix, inserted element and suffix, then commits after version revalidation.
Clear updates bytes/live prefix/version under the same lock without intervening
safepoints. Slicing uses leased buffer copies into private fresh storage, followed
by the existing fixed-child copy lowering. Deep-child copy failures cannot publish
a partial slice to the destination source variable.

Precise Frame/Slot roots protect captured receivers, prepared elements, old/new
backing and private results across allocation and collection. Allocation failure
preserves the original list/slice destination, and ordinary cleanup reclaims
unpublished storage. No ownership registry, collector, allocator or machine ABI
was added. Checked address/count representability and existing analysis bounds
apply; no array-byte or native-stack payload ceiling is introduced.

## Deliberately deferred

The owner approved deferring `.pop()` until the illustrated `result`/`Ok` contract
exists. `.pop()` is not replaced with an unchecked/incompatible operation; use
checked `.remove_at(-1)` when a runtime error on empty is appropriate. Missing
slice endpoints, slice assignment/views, string slicing, dictionaries, sorting,
general equality/search and future result/pattern features remain outside Phase 25.

## Verification

`collections_source` covers real normal/O2 programs, precise roots, publication,
readonly diagnostics, generic/nested elements, endpoint/insertion order, alias
capture, fixed snapshots, iterator invalidation, reference reclamation, allocation
failure/rollback/retry and hot reload with an old generation lease. Native sweeps
separately cover growth, in-capacity insertion and slicing without requiring an
allocation-free operation to fail. `collections_cli` executes real programs via
normal/O2 JIT and independently linked AOT. Large cases exceed 2 MiB and run under
the existing 1 MiB stack checks. Exact commands and results are in WORK_LOG.md.
