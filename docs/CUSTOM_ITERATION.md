# Custom iteration — G85

Developer-defined collections participate in ordinary `for` loops through checked
receiver methods. No registration, new keyword or built-in collection identifier
is required. [Runnable generic example](../examples/custom_iteration/main.void):

```void
var tiles = tile_collection(values = [10, 20, 30])
for tile in tiles:
    print(tile)
for tile, index in reverse tiles:
    print(tile, index)
```

## Protocol

| Operation | Required contract |
| --- | --- |
| `collection.iterator()` | Forward factory returning a non-nullable concrete value or managed iterator struct |
| `collection.reverse_iterator()` | Explicit reverse factory with the same state requirements |
| `state.next()` | Returns exactly `option<T>`; `Some(value)` yields one `T`, and `None` completes traversal |
| `state.index()` | Required only for a second binder; returns the index/key belonging to the value just yielded |
| `state.deinit()` | When present on managed state, receives automatic scoped cleanup |

Protocol operations are ordinary accessible synchronous receiver methods with no
explicit parameters, including no defaulted parameters. Owner generic parameters
specialize normally; the loop does not infer independent method type arguments.
Properties, callable fields, static functions, Task results and nullable or erased
iterator state cannot substitute for these methods. Unused ordinary methods are
not special declarations: protocol shapes are checked when a loop uses them.

The source expression and selected factory execute once. Each iteration calls
`next()` once. After `Some`, the compiler copies the payload into the item binder,
then calls `index()` once if requested, then executes the body. No index call or
body executes after `None`. The loop never calls `next()` again after that first
`None`. Implementations should keep exhaustion terminal until explicitly reset.
The loop's `continue` advances by returning to `next()`; `break` leaves traversal.

The second binder has the exact returned type of `index()`. It may be an integer,
string key or another ordinary storable value. The compiler supplies no ordinal
counter. Implementers define that key's meaning in their API. A reverse index
normally identifies the original collection position, as in the runnable example.
Forward and reverse factories are independently selected capabilities; each loop
infers its item/key types from the selected iterator. There is no synthesized
reverse traversal, full-collection copy, or requirement to implement reverse when
only forward traversal makes sense.

## Ownership and failures

The loop owns one private state binding. Value state uses ordinary value-copy
semantics, with mutable cursor storage for receiver calls. Managed state preserves
its managed identity. Implementers retain all managed collection/storage references
needed by their state. Collections intended for nested or repeated traversal
should create independent cursor state; returning an existing managed cursor keeps
its existing aliases and shared progress. The compiler does not clone it.

A managed iterator with `deinit()` uses the existing `with` machinery. It is
activated only after successful acquisition and cleaned once on completion,
`break`, return, checked Runtime failure and error propagation. `continue` keeps
iterator state active while cleaning any scopes exited in the body. Nested
iterators unwind from the inside out. Failure while creating state activates no
state cleanup. Failure during cleanup follows ordinary `with` error behavior.
Inaccessible or ambiguous destructors diagnose instead of silently omitting cleanup.
Managed state without a destructor requires no implicit resource action. Value
`deinit` remains unsupported by VOID; resource-owning iterators use managed state.
Existing `defer` registrations retain their function-exit timing and order.

Each yielded value has the ordinary representation and ownership of `T`: inline
values are independent copies, managed/interface values preserve rooted identity,
and collection fields retain their established copy/alias rules. Item/key values
can escape the loop using ordinary assignments or returns. No reference into an
iterator's inline stack storage is exposed. Replacing iterator fields or releasing
its local roots cannot invalidate separately rooted managed values. Implementers
must still respect the ordinary lifetime of external resources they explicitly
close. Precise GC roots, readonly provenance, generation leases and resource
ownership remain the existing language contracts. Dead temporary roots can remain
until function exit; this feature adds no root-liveness optimizer.

Factories and `next` do not have an implicit typed-error channel. Handle fallible
collection acquisition before iteration, using existing result handling or `?`.
An iterator can yield `option<result<T, E>>`; the body then handles or propagates
that result normally. [The checked collection example](../examples/custom_iteration/checked.void)
uses this to report structural invalidation without adding exceptions.
Synchronous protocol methods can run in an async function and survive body
suspension through its existing state/cleanup machinery. Async factories and
async `next` are not an asynchronous iteration protocol.

## Mutation and readonly authority

Custom implementations define and enforce their own structural mutation,
aliasing and concurrency policy. A collection that wraps a list does not
implicitly acquire the built-in list loop's saved-version check. A typical design
captures a revision in its iterator and checks it on every `next()`, including the
completion attempt, before reading storage. Resource-owning, versioned or
synchronized state should expose the relevant policy in its API. Source workers,
readonly views and managed aliases do not gain new permissions through iteration.
The checked example demonstrates revision validation, including a mutation after
the last successful value.

Built-in arrays, lists, dictionaries and ranges retain their existing lowering.
List/dictionary structural mutation still invalidates at the next access,
including completion. Immediate `break`/return retains its existing behavior.
Reverse remove-current remains subject to the same invalidation check; G85 adds
no exception. Element replacement, fixed-array views and dictionary insertion
order retain their established contracts.

## Generics, interfaces and editors

Concrete generic collections and iterator structs use normal specialization,
argument inference, owner visibility and resource bounds. Structural interface
constraints can describe iterable collection methods, including specialized
`iterator()` results. A managed collection can also be used through an interface
whose factory returns a concrete iterator. See the maintained
[interface/constraint example](../tests/custom_iteration/imported/main.void).

The returned iterator must be concrete: interface-erased iterator state cannot
hide a required destructor, since lifecycle requirements are not interface members
in VOID. This does not change interface boxing or introduce associated types.
Ordinary ancestry and one-hop composition method selection remain authoritative.
Import the original owners needed for member access; the loop grants no private
or transitive-import authority. Readonly effect/provenance analysis includes all
factory, advancement, key and cleanup calls.

`voidls` uses the same typed calls and pattern binders for hover, completion,
signatures, navigation, references and semantic highlighting. Compiler-private
state bindings are hidden from editor declarations. Parameterless method calls
now provide their zero-parameter signatures without an invented active parameter.
Incomplete loop headers and invalid protocol edits retain diagnostic recovery.
See [G85 qualification](projects/G85_CUSTOM_ITERATION.md) for actual editor and
platform evidence; implementation is separate from independent acceptance.
