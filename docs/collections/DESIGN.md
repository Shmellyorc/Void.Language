> Current authorization: Boss approved the complete contract on 27 October 2026.
> Copy-returning transforms, NaNs-last ordering and both mutation refinements are
> settled. Original proposal wording below is historical. See
> [engineering status](IMPLEMENTATION.md) and the separately approved
> [generic operator amendment](GENERIC_OPERATORS.md). Local engineering is complete; independent review remains open. See [handoff](HANDOFF.md).

# Collections: proposed API and observable behavior

27 October 2026. Baseline **`bb7244ea029623ddc83acfc163f888a261379051`**.
**Research proposal, not implementation or Boss approval.**
[Source findings](RESEARCH.md) · [one engineering sprint](ROADMAP.md)

## Recommended scope

One ordinary `use collections` algorithm module over existing `[t]` and `{k:v}`.
Keep language indexing, slicing, iteration, get/pop, insertion/removal and
dictionary operations authoritative. Add stable copy sorting, ordered searching,
linear queries, eager transformations, reduction and dictionary extraction/copy.
No new containers, lazy queries, reflection, generic overload system or collection
storage engine.

The proposed complete initial surface has **24 functions**. All are module
functions; no builtin `.sort()` method or extension-method machinery is implied.
The historical handoff favors mutating `items.sort()` and a Boolean comparator.
Preserve the Boolean direction. **Copy-returning sorting rather than the favored
in-place convenience needs Boss's explicit scope decision**; this document does
not silently count an alias-preserving sort as delivered.

## Exact public signatures

These are proposed headers, not implemented source:

```void
func reversed<t>(const items: [t]) -> [t]
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

func numeric_less<t>(const left: t, const right: t) -> bool
func text_less(const left: string, const right: string) -> bool
```

Only `descending` has a default. `reverse` remains a reserved iteration keyword,
not a legal parameter name. Ordering callbacks are required: there is no
unsupported generic default function, overload dispatch or implicit comparer
class. Callback input types follow the anchored element type. Use `numeric_less`
for numbers, `text_less` for strings, or an ordinary named function, closure,
bound method or compatible callable struct with the exact signature.
Generic function references still need explicit specialization, for example
`numeric_less<i32>`. Inline callback parameter types must be written; untyped
`func(a,b)` is not currently supported. Do not promise the illustrative handoff's
lambda inference before a separately approved shared-language enhancement.

### Supported types and generic limits

- List functions accept `[t]`. Fixed `[t,n]` callers use an explicit slice such
  as `fixed[0..<fixed.count]`; this creates an independent growable value under
  existing rules. There is no numeric generic extent parameter or implicit
  fixed-to-list conversion. Fixed arguments are never mutated through a hidden
  alias. Direct builtin loops/get/slices still support fixed arrays.
- Elements may be any currently valid collection element, including required
  managed references, nullable values, interfaces, fixed children, value structs,
  results/enums and functions, subject to each operation's actual callback/type
  requirements. No default `t()` or zero initialization is assumed for arbitrary t.
- `index_of`/`contains` require existing source `==` on t under readonly authority.
  They use its exact identity/value/custom-operator semantics. Do not invent
  structural list/dictionary equality or custom hashing. Unsupported equality
  diagnoses at specialization, even when runtime input might be empty. Predicate
  find/any remains available for types without that equality.
- `sum` and `numeric_less` specialize only for the ten numeric primitives using
  existing numeric source operations/context checks, not a new numeric interface.
  Bool/string/collection input is not numeric. Min/max instead use the supplied
  comparator and can select any valid element type.
- `map`'s output u is not inferred from the destination annotation or a bare
  unbound named/inline callback today. Use explicit `<T,U>` or a previously typed
  `func(const T)->U` variable. Fold's items and initial value anchor t and a.
  Do not promise an inference repair or editor-only inference shortcut.

## Copies, identity and readonly authority

Every returned outer list/dictionary is fresh, including empty/singleton inputs.
Fixed children and value fields copy with existing independent value semantics;
managed/list/dictionary referents keep shared identities. No deep object-graph
clone, native-resource duplication or automatic source deinit occurs.

`const` input protects the whole reachable input authority through this path.
Copying to a mutable outer result does **not** thaw referenced children, even if
the caller originally held a mutable input. Numeric/fixed independent value
storage can be changed according to existing copy rules. Readonly result payloads
from find/min/max retain authority through Ok patterns, calls and generic use.

Callback returns obey ordinary exact callable-result authority. Map may return
scalars, independent values, fresh references or independently held mutable
aliases when its signature permits them. It cannot launder a readonly input
alias into a mutable reference result. Current function-value types cannot spell
that readonly-return projection; `-> const node` is not supported. Use filter,
find or copy operations for retaining input identities. A returned result/function
value as u is ordinary data, not automatic error propagation or async execution.

Build same-element result lists from `items[0..<0]`, which carries the input
element authority; a bare mutable `[t]=[]` followed by pushes can reject readonly
managed inputs. Dictionary builders can seed their output profile from the first
entry and return a typed empty result for empty input. These are source-level
implementation details, not new public syntax or relaxed authority checks.

## Traversal, callbacks and reentrancy

All operations are eager and synchronous. Evaluate call arguments once in normal
left-to-right source order. Capture the selected input identity; rebinding the
caller variable does not redirect it. Callbacks run under the existing checked
function ABI/GC roots/generation lease, with no Runtime native comparator trampoline.

**Linear operations use ordinary live `for` semantics**, not an automatic outer
snapshot: find/find_index/index_of/contains, any/all/count_if, filter/map/fold,
min/max/sum and is_sorted. Traverse left-to-right. An indexed replacement through
another mutable alias can be seen when that later position is read. Structural
push/remove/insert/nonempty-clear invalidates the iterator at its **next access
or completion**, including remove-plus-push restoring the same count.

Capture the current candidate before invoking its callback. Find/filter/extrema
use that captured candidate afterwards, not a reread of items[index]. Replacing
the current scalar/fixed/value element during the callback does not replace the
already captured value; referenced children retain ordinary shared identity.

An immediate return still skips future iterator access. Thus a predicate that
pushes an element and returns true may let any/find return successfully; all can
similarly return false immediately. Count_if, filter/map or another continuing
traversal detects invalidation when it resumes.
Do not add a hidden final check to early-returning loops or claim every callback
mutation is rejected. This is the approved Language Bible iteration rule.

Callbacks may change their own captured state or reenter algorithms. Previously
completed effects remain visible on success/failure. Readonly input does not
freeze a separate mutable captured alias. Algorithms neither roll back user
effects nor grant new concurrent-sharing/worker permissions. Existing transitive
worker/indirect-call restrictions determine admission.

Sorted uses a different, explicit boundary: **complete an outer input slice
before its first comparison**. Comparisons and result construction use that
private snapshot. Changes to the original outer list through captured aliases
remain visible there and are never overwritten by sorted. Referenced children
are still shared, so changing ordering-relevant fields can violate the ordering
contract; outer snapshotting is not immutable object-graph isolation.

## Ordering and stable sorting

`less(a,b)` means a precedes b. Require a consistent strict weak ordering during
the operation: irreflexive, asymmetric and transitive, with transitive equivalence
`not less(a,b) and not less(b,a)`. Do not mutate ordering-relevant state through
another alias. Invalid comparators cannot promise sortedness/search correctness;
the implementation must still use bounded loops and checked indexing, without
unsafe memory access or internal infinite loops. A callback that itself never
returns is outside an algorithm termination guarantee. No quadratic validation
pass or comparator-validation framework.

`sorted` is stable: equivalent elements retain original snapshot order.
`descending=true` reverses comparator direction by swapping arguments, **not** by
reversing the final ascending result, which would reverse equal groups.
`is_sorted` checks adjacent pairs for an inversion, returns true for zero/one
element and stops at the first inversion; it does not prove comparator validity.

Recommend iterative bottom-up merge sorting of i64 permutation indices against
the private snapshot, then materialize one correctly profiled output. Choose the
left run unless the right element precedes it. Two reusable index buffers avoid
per-pass list allocation and recursion. This bounds index work to O(n log n)
and workspace to O(n), with constant algorithm call depth. Comparing T can still
copy fixed/value payloads; no logarithmic-byte or allocation-free guarantee.
Checked width/midpoint arithmetic must avoid i64 overflow at theoretical limits.

`numeric_less` recommends **NaNs last in ascending order**, equivalent to one
another, with signed zeros equivalent. Otherwise use ordinary numeric `<`;
infinities retain their order, integer comparison never subtracts endpoints.
Reverse order consequently puts NaNs first while preserving equivalent order.
This is not IEEE totalOrder or NaN-payload ordering. It is a proposed policy
requiring Boss approval, not a change to ordinary VOID comparison operators.

`text_less` compares unsigned UTF-8 bytes lexicographically; a proper prefix
precedes the longer string. Strict UTF-8 makes this scalar-code-point order,
without locale, normalization, case folding or grapheme order. NUL is ordinary
data. An ordinary-source implementation can use to_utf8; those owned conversions
may allocate on each comparison and must be measured, not described as zero-cost.

## Ordered search

Input must already be sorted under the same less/descending direction, and its
ordering-relevant values must remain stable for the call. No O(n) sortedness
validation or full-input snapshot is performed by each binary query.

- lower_bound returns the first i64 position where an element does not precede
  value; upper_bound returns the first where value precedes an element.
  Both return positions in `[0,count]`, including zero for empty input.
- binary_search returns the **first comparator-equivalent** position or
  Err(not_found). Equivalence uses the comparator, not `==`: numeric_less makes
  NaNs equivalent even though ordinary NaN equality is false; signed zeros also
  form one equivalent range.
- Capture input/version once; check its structural version immediately after
  **every** comparator, including final equivalence checks, before using its
  answer or returning. Callback structural mutation raises the existing checked
  list-modified Runtime failure, even if count is restored. Callback checked
  failure propagates before a post-call check can execute.
- Reuse existing list_version/list_check IR through two canonical-private source
  adapters. No new Runtime operation, content revision or public version property.
  Indexed replacement does not change that structural version. Mutation of
  ordering-relevant values violates the search precondition; do not claim it is
  detected, atomic or thread-safe. All indexing/control remains checked/bounded.

Each search performs O(log n) probes/comparisons plus existing element-copy,
callback and guard costs. Midpoint is low + (high-low)/2. Returning an index is
an observation during the operation, not a stable handle into future mutations.

## Linear queries, transformations and aggregation

| Functions | Exact behavior |
|---|---|
| find / find_index | First predicate match, otherwise Err(not_found). Find returns the selected value/reference with its readonly profile. |
| index_of / contains | First existing-language equality match; index_of returns Err(not_found), contains returns false. NaN follows ordinary equality here. |
| any / all | Short-circuit at first true / first false. Empty input is false / true. |
| count_if | Invoke predicate once per visited element; return i64 match count, zero for empty. |
| filter | Predicate once per visited item, left-to-right; keep matching values in traversal order in a fresh outer list. |
| map | Transform once per visited item, left-to-right; append each returned u in order to fresh storage. No eager extra call for type inference or empty input. |
| fold | Left fold: combine(accumulator,item), initialized from explicit initial. Empty returns initial under existing value/reference semantics. The accumulator is deliberately ordinary mutable authority; reference initial values may be changed by combine and are not rolled back. |
| min / max | First equivalent minimum / maximum under less. Empty is Err(empty_collection). No descending parameter or hidden numeric order. |
| sum | Same-type left-to-right addition starting with typed zero. Empty returns typed zero; integer wrapping and float IEEE/NaN/signed-zero behavior are existing language arithmetic. No widening, saturation, compensated summation, reassociation or parallel reduction. |
| reversed | Fresh outer result in reverse order, including a fresh empty result. Use existing slices; no mutation of the input or iterator-version change. |
| keys / values | Fresh list in dictionary insertion order, retaining key/value copy and readonly rules. |
| copy_dictionary | Fresh dictionary identity with identical insertion order and existing key equality/hash policy; nested identities remain shared with readonly authority. |

No `-1` missing-index sentinel: -1 is an existing valid negative index. No second
list copy helper: `items[0..<items.count]` already is the complete copy operation.
Dictionary assignment still shares identity; only copy_dictionary creates another
outer dictionary here.

## Working-style examples

Proposed API usage, not execution evidence:

```void
use collections

func positive(const value: i32) -> bool:
    return value > 0

func doubled(const value: i32) -> i32:
    return value * 2

func add(total: i32, const value: i32) -> i32:
    return total + value

func main():
    var numbers = [3, 1, 2, 1]
    var ordered = collections.sorted(numbers, collections.numeric_less<i32>)
    var selected = collections.filter(numbers, positive)
    var transformed = collections.map<i32, i32>(numbers, doubled)
    var total = collections.fold(numbers, 0, add)
    if collections.binary_search(ordered, 1, collections.numeric_less<i32>) is Ok(var index):
        print(index, total)
    var names = collections.sorted(["player", "enemy"], collections.text_less)
    var fixed: [i32, 3] = [7, 8, 9]
    var fixed_total = collections.sum(fixed[0..<fixed.count])
    var scores = {"player": 10, "enemy": 5}
    var independent = collections.copy_dictionary(scores)
    independent["player"] = 20
```

`numbers` and aliases of its original list remain in original order; rebinding
numbers to ordered would replace only that variable's binding. Both dictionaries
have independent outer identity. Managed input-derived result leaves would remain
readonly; the numeric example does not imply a general mutable deep clone.

## Errors and allocation safety

```void
use error as core

enum error_kind:
    not_found
    empty_collection

struct error extends core.error:
    operation: string
    kind: error_kind
```

Inherited code is stable 1 for not_found, 2 for empty_collection; path is empty.
Messages identify the operation, not arbitrary element contents. Missing/empty
outcomes are recoverable as specified. No fabricated comparator-invalid Err,
catchable OOM or implicit conversion between collections.error and other error
types. `?` retains exact error-type matching.

Invalid indexing, iterator modification, callback Runtime failure, allocation or
representability failure use existing source-located checked failure/unwind.
Every new outer result remains private until successful return; no incomplete
value or partially built result reaches the destination. Input is never mutated
by these algorithms, but already completed callback/custom-equality/accumulator
effects remain. Allocation failure is not general transactional rollback.

## Owner decisions and intentional exclusions

1. **Initial mutation scope:** recommend accepting the complete copy-returning
   transform API above, with existing builtin mutations retained. This avoids a
   new commit/revision mechanism and data-loss-prone multi-write promises. Boss's
   favored alias-preserving in-place sort/reverse/remove-if is a meaningful
   alternative scope, not implemented by rebinding a local variable. If it is
   required now, approve its failure/mutation guarantees and corresponding narrow
   architecture before engineering; do not ship a partial imitation or quietly
   defer an approved API.
   An explicitly nontransactional source implementation can reuse current writes,
   like shuffle; failure between element assignments may leave a valid list that
   is no longer the original permutation. Only stronger rejection of all target
   content mutation plus atomic failure behavior requires additional change
   tracking/prepared publication. Do not imply every in-place option needs it.
2. **Natural numeric order:** approve NaNs-last numeric_less with equivalent
   signed zeros, or specify a different policy. Custom comparators remain explicit.

Other questions follow existing contracts: fixed callers slice, exact generic
inference/readonly rules apply, callbacks are synchronous and errors use the
current ABI. Do not force additional owner choices for those requirements.

Optional conveniences outside this proposed initial inventory are cached-key
sort_by, last-find/partition/zip/group/join/distinct/set families, reduce-without-
seed, average/product aliases, lazy query chains, parallel algorithms, custom
iterators and new queue/heap/container types. Fold and ordinary source loops cover
custom aggregation; dictionary operations and random shuffle keep their current
owners. This is a complete bounded algorithm surface, not the entire collection
ecosystem. **Stop for Golden Wrench review and Boss contract approval.**
