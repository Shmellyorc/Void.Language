# Collection equality and dictionary hashing

G83 implements content `==` and `!=` for matching concrete fixed-array, growable
list and dictionary types. It reuses ordinary element equality and the existing
Runtime dictionary schemas. There is no public hash-code API or custom key comparer.

```void
var a: [i32] = [1, 2, 3]
var b: [i32] = [1, 2, 3]
print(a == b)  # true
print(a != [3, 2, 1])  # true
var first = {"boss": 7, "crew": 3}
var second = {"crew": 3, "boss": 7}
print(first == second)  # true
```

## Content and element rules

- Fixed arrays require the same element type and length; lists require the same
  element type. Fixed and growable arrays do not implicitly convert for equality.
  Sequences compare length, then elements in ascending index order, stopping at
  the first difference. Different list lengths compare unequal.
- Dictionaries require matching key and value types. They compare count, key
  membership and corresponding values; insertion order does not affect the result.
  Value comparisons visit the left dictionary's insertion order and short-circuit.
  Key membership uses the existing data-only schema, not a new source operator.
- Elements use existing scalar, string, callable identity, managed/interface
  identity, eligible enum, nullable and custom `operator ==` semantics. Nested
  collections and concrete generic specializations use these same rules. A plain
  value struct still needs supported equality, such as an explicit operator.
  Unsupported elements diagnose; empty collections do not bypass type checking.
- `!=` negates the same content comparison. Both operand expressions evaluate
  once in source order. Float NaN and signed zero retain ordinary IEEE equality;
  comparing a handle with itself does not bypass element comparisons or effects.

`T?` presence is distinct from contents: null differs from an empty present
collection, zero, false or an empty string. Existing nullable lifting and checked
extraction apply. `option<t>` and `result<t,e>` retain their separate contracts;
G83 does not add general equality to otherwise unsupported payload families.

## Ownership, aliases and mutation

Lists and dictionaries still share handles on assignment. Equality observes their
current contents without deep-copying managed objects or changing object identity.
Fixed arrays retain ordinary value-argument snapshot semantics. Comparisons borrow
readonly views, preserve GC roots and cannot grant mutation authority to operators.
An explicitly written custom operator can have its normal side effects through
other permitted paths; equality is not a transaction.

Structural mutation of either compared list/dictionary during an element callback
is a checked, source-located Runtime failure, even if that callback returns false.
Nonstructural replacement follows the existing collection iteration/access rules.
No concurrent snapshot guarantee is introduced; worker/global restrictions remain.
No alias identity shortcut changes these rules.

Recursive structural shapes, for example an enum whose payload is a list of that
same enum, are diagnosed when collection equality is requested. Managed references
are identity leaves, so cycles inside managed objects do not traverse their graphs.
An explicitly defined custom equality operator remains responsible for its own
recursion. G83 does not introduce graph bisimulation or a visited-object table.

## Constants

Pure built-in collection comparisons are available in compile-time constants,
including nested collections, active enum payloads, nullable presence wrappers and
callable identity. Dictionary literals use the same last-write-wins duplicate-key
rule as Runtime construction; order is irrelevant. Repeated fixed defaults need
not be expanded for comparison. Custom equality calls and managed allocation
remain excluded from compile-time evaluation. All-null nullable identity leaves
are supported; this does not permit managed object creation in constants.

## Keys and hashes

Content equality does **not** make collections valid dictionary keys. Existing
eligible keys are exact-width integers, bool, byte-exact UTF-8 strings (including
NUL), managed identity references, eligible numeric/flags/tagged enums and value
structs with recursively supported stored fields. Eligible nullable value wrappers
preserve presence, so null and zero are distinct keys. Value keys are snapshots;
managed referents contribute identity rather than mutable fields.

Floats, collections (including fixed arrays), interfaces, canonical option/result,
and custom-equality owners or stored custom-equality leaves remain rejected. No
implicit conversion, padding comparison, mutable collection hashing or custom hash
callback is added. These restrictions maintain equal-key/equal-hash coherence.

Runtime schemas, collision handling and hashes are unchanged. A collision never
establishes equality: lookups compare the full key. Hash algorithms remain private,
with no stable cross-process or adversarial-input randomization guarantee. Ordinary
value equality and dictionary structural-key equality remain distinct contracts.

See [dictionaries](DICTIONARIES.md), [operators](stage10/OPERATORS.md),
[the runnable example](../examples/learn/collection_equality.void) and
[G83 implementation and qualification](projects/G83_COLLECTION_EQUALITY_HASHING.md).
