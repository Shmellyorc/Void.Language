# Native ordered dictionaries — Stage 3

Current status and hosted evidence live in
[DEVELOPMENT_STRATEGY.md](DEVELOPMENT_STRATEGY.md#current-implementation-status).
The owner approved this bounded contract on 18 October 2026. It extends the existing
frontend, typed IR, LLVM 19.1.7 JIT/AOT lowerer and coordinated Runtime.

```typescript
func entry(value: i32) -> i32:
    var scores: {string: i32} = {"first": 1, "second": 2}
    scores["first"] = value
    scores.remove("second")
    scores["second"] = 3
    for amount, name in reverse scores:
        if name == "first":
            return amount
    return 0
```

## Source contract

- `{K: V}` declares a dictionary type; an omitted initializer creates an empty
  dictionary. `{key: value, ...}` literals infer concrete key/value types where
  unambiguous or accept a typed context. Empty literals require context. Multiline
  dictionary literals work. Bare identifiers are ordinary expressions, never
  implicit strings; `name = value` entries are rejected.
- Evaluate entries left to right, each key before its value, exactly once. Later
  duplicate keys replace the value without moving the first insertion position.
  Literal storage remains private until construction succeeds.
- Keys support all eight exact-width integer types, bool, immutable strings, managed
  structs including nullable references, and value structs whose fields recursively
  support these rules. Managed keys compare/hash by identity. Strings compare exact
  UTF-8 bytes, including NUL, with no normalization. Value keys compare/hash fields,
  ignoring native padding. Copying a value key preserves its stored value when the
  original variable changes; referenced object identities remain shared.
- Float, interface, array/list/dictionary keys and value structs containing those
  fields reject. Enum/custom equality and floating-point key policy are future
  design. Hash values/algorithm are private implementation details; no stable
  cross-launch hash or adversarial-input randomization guarantee is made.
- `d[key]` performs checked lookup and raises a source-located runtime error if
  missing. `d[key] = value` inserts or replaces. Compound assignment requires an
  existing key and retains ordinary numeric/string rules. Receiver/key capture
  precedes value evaluation; a structural change during the value invalidates the
  pending write. Rebinding cannot redirect it. Earlier side effects remain visible.
- `.contains(key)` returns bool; `.remove(key)` returns whether a key was removed;
  `.clear()` returns nothing; readonly `.count` is i64. Removing an absent key and
  clearing an empty dictionary are no-ops. Clear preserves identity/capacity and
  releases removed key/value references from the traced live prefix.
- Dictionary assignment/arguments/returns share identity. Fixed child arrays and
  value fields retain independent value copies; managed/list/dictionary children
  share their existing reference identities. Deep readonly authority follows keys,
  values, aliases, calls, generic instances and loop bindings. Readonly dictionaries
  permit observation, not mutation of embedded mutable referents.
- `for value, key in d:` and `for value, key in reverse d:` traverse insertion order
  or its reverse; the key binding is optional. A new key, successful removal or
  nonempty clear increments structural version. Replacement preserves order/version.
  Delete/reinsert moves the key to the end. Detect structural invalidation at the
  next access/completion; immediate break/return remains allowed. Nonstructural
  replacement is observed by subsequent accesses. No reversed temporary collection.
- Pure dictionary literals can be owned module/field/parameter default templates;
  materialization creates fresh storage using the existing template mechanism.
  Dictionary lookup/count/methods require runtime evaluation. `.get()` remains
  deliberately deferred until result/Ok exists; no incompatible substitute is added.

## Storage and correctness

One ordinary managed header owns an ordered entry buffer and a scalar hash-index
buffer from the existing typed-buffer infrastructure. Open addressing at a bounded
load factor uses the stored hash plus exact key equality, including collisions.
Updating a key changes its value in place. In-capacity insertion writes one new
entry/bucket; growth privately builds new buffers with BufferBuilder/read leases,
then revalidates the selected version/backing before publishing. Deletion compacts
entries and rebuilds the scalar index to preserve insertion order. Clear zeros live
entry storage and the index under the existing Runtime lock.

The shared LLVM DataLayout describes entry/key fields through existing native array
metadata. Dictionary metadata contains data only, never executable equality callbacks
or source/JIT pointers. Field hashing omits padding and uses Runtime-owned string
bytes. No second allocator, collector, ownership registry or global cache is added.

Captured inputs, whole native entry staging, old/new buffers and private results use
precise Frame/Slot roots across allocations/safepoints. A failed growth/construction
cannot publish partial dictionary state. Locked updates/removals/clear perform no
fallible allocations after mutation begins. Existing coordination, failure cleanup,
generation leases and runtime-owned metadata lifetimes remain authoritative.
Capacity/count arithmetic checks actual representability, with no arbitrary 64 KiB
array-byte or native-stack payload ceiling.

Deletion is O(n) because insertion order is compacted; retained capacity and conservative
function-lifetime roots follow the existing collection behavior. No speculative
allocator/collector performance changes were made. General dictionary cloning/views,
result/pattern syntax and unrelated collection features are outside this foundation.

## Verification

`dictionaries_source` covers normal/O2 source programs, all key families, generic
keys/values, nested/interface/nullable values, cyclic graphs, fixed child copies,
shared aliases, readonly rejection, evaluation/capture order, collision chains,
iteration versions, allocation-failure rollback/retry, GC, hot reload and retirement.
`dictionaries_cli` checks independently linked AOT alongside both JIT modes. The large
fixture uses 2 MiB fixed payloads in nested collections. Native Runtime sweeps separately
exercise construction, growth, in-capacity insertion, replacement, removal, clear and
queries; allocation-free paths are not forced to fail. Exact final evidence and
commands belong in the latest Stage 3 WORK_LOG entry.
