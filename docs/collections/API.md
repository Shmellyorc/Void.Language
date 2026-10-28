# Collections public API

`use collections as c` provides ordinary functions over `[t]` growable lists and
`{k:v}` ordered dictionaries. Fixed callers explicitly slice. All 24 functions
and error types are documented in the installed source, so compiler-owned hover,
completion, signatures and navigation share the implementation metadata.

```void
func reversed<t>(const items: [t]) -> [t]
func numeric_less<t>(const left: t, const right: t) -> bool
func text_less(const left: string, const right: string) -> bool
func sorted<t>(const items: [t], less: func(const t, const t) -> bool, descending: bool = false) -> [t]
func is_sorted<t>(const items: [t], less: func(const t, const t) -> bool, descending: bool = false) -> bool
func lower_bound<t>(const items: [t], const value: t, less: func(const t, const t) -> bool, descending: bool = false) -> i64
func upper_bound<t>(const items: [t], const value: t, less: func(const t, const t) -> bool, descending: bool = false) -> i64
func binary_search<t>(const items: [t], const value: t, less: func(const t, const t) -> bool, descending: bool = false) -> result<i64, error>
func find<t>(const items: [t], predicate: func(const t) -> bool) -> result<t, error>
func find_index<t>(const items: [t], predicate: func(const t) -> bool) -> result<i64, error>
func index_of<t>(const items: [t], const value: t) -> result<i64, error>
func contains<t>(const items: [t], const value: t) -> bool
func any<t>(const items: [t], predicate: func(const t) -> bool) -> bool
func all<t>(const items: [t], predicate: func(const t) -> bool) -> bool
func count_if<t>(const items: [t], predicate: func(const t) -> bool) -> i64
func filter<t>(const items: [t], predicate: func(const t) -> bool) -> [t]
func map<t, u>(const items: [t], transform: func(const t) -> u) -> [u]
func fold<t, a>(const items: [t], initial: a, combine: func(a, const t) -> a) -> a
func min<t>(const items: [t], less: func(const t, const t) -> bool) -> result<t, error>
func max<t>(const items: [t], less: func(const t, const t) -> bool) -> result<t, error>
func sum<t>(const items: [t]) -> t
func keys<k, v>(const items: {k: v}) -> [k]
func values<k, v>(const items: {k: v}) -> [v]
func copy_dictionary<k, v>(const items: {k: v}) -> {k: v}
```

The only default is `descending = false`. Supply a strict weak ordering explicitly;
generic callback references need concrete arguments (`c.numeric_less<i32>`), and
lambda parameters need written types. Map needs explicit `<T,U>` or an already
typed transform. A destination annotation alone does not infer U.

All returned outer lists/dictionaries are fresh. `sorted` is stable and snapshots
the original outer list before invoking a comparator. Descending swaps comparison
arguments, preserving tie order. Referenced children retain shared identity and
readonly input-derived authority; fixed/value children copy normally. A copy does
not grant mutable access to readonly referents. `map` follows exact callback-result
authority and may return fresh or independently held mutable references.

Linear operations use ordinary live iteration, captured current candidates and
left-to-right callbacks. Structural mutation fails on next access/completion;
immediate return may succeed after mutation. Ordered searches instead check one
captured structural version after every comparator, even final equivalence probes.
They use logarithmic probes without cloning or a sortedness prepass. Nonstructural
ordering-key changes violate the comparator precondition and are not guaranteed
to be detected. Invalid comparators cannot promise ordering results, but internal
loops/indexes stay bounded and checked.

`numeric_less` accepts exactly the ten numeric primitives. Ascending puts equivalent
NaNs last; descending puts them first. Signed zeros are equivalent; infinities
retain natural order. Integer ordering never subtracts endpoints. Ordinary equality
and arithmetic are unchanged. `sum` preserves same-type wrapping/IEEE left-fold
addition and returns typed zero for empty input. `min`/`max` keep the first tie.
`text_less` compares unsigned UTF-8 bytes, including NUL, with no normalization or
locale processing; its owned conversions can allocate on each comparison.

`index_of`/`contains` use existing identity/scalar/custom equality, including its
effects. Unsupported collection/value equality diagnoses at specialization.
Public operators on exact substituted public nominal owners follow the separately
[approved generic rule](GENERIC_OPERATORS.md); private/internal access remains
unchanged. Dictionary supported-key restrictions and insertion order are retained.

`error` extends `error.error` with `operation: string` and `kind: error_kind`, plus
inherited `message`, `path`, and `code`. `not_found` has code 1; `empty_collection`
has code 2; path is empty. Find/find_index/index_of/binary_search return not_found;
min/max return empty_collection for empty input. Checked Runtime/allocation/callback
failures use the existing failure channel, preserving completed user effects.

See [the exact behavior contract](DESIGN.md), [runnable example](../../examples/collections_stdlib/main.void),
[implementation](IMPLEMENTATION.md) and [review handoff](HANDOFF.md).
