# Channel routing

`use channel` supplies synchronous string-topic routing through VOID's built-in
events. Construct `channel.router<t>()` with an explicit payload type. Each router
accepts exactly that type on every topic. Independently constructed routers have
independent state; assignments, fields and parameters share the managed instance.
Use separate typed routers for unrelated messages.

| Public declaration | Contract |
|---|---|
| `channel.router<t>()` | Empty reusable managed router; no default global registry. |
| `channel.subscription` | Opaque managed identity returned by subscribe; constructor is private. |
| `subscribe(topic: string, handler: func(const t)) -> subscription` | Append one strongly retained synchronous callback; return its unique handle. |
| `unsubscribe(const token: subscription) -> bool` | Disconnect exactly this registration; foreign, stale and repeated handles return false. |
| `emit(topic: string, const payload: t)` | Deliver synchronously in subscription order from an event snapshot. |
| `clear()` | Detach all routing state; later subscribe reuses the same router. |

Topics match exact UTF-8 bytes. Empty strings, embedded NUL and slash are data.
Matching is case-sensitive with no normalization, hierarchy, wildcard or reserved
topic. Unknown emissions deliver nothing and create no routing state. Ordinary
dictionary selection can allocate and fail even when no subscriber matches.

Callbacks receive readonly payload authority. Managed/list/dictionary/interface
references retain identity; value structs and fixed arrays retain existing copy
semantics. Another mutable alias may change a referent between handlers. Captured
application state remains mutable. There is no deep clone, freeze or type erasure.
Async handlers and handlers with results are rejected by existing type checking.

Duplicate callbacks receive distinct handles. Copying a handle preserves its
identity. Dropping a handle does not disconnect. A retained handle contains no
router, topic, callback, native resource or code lease, so it cannot keep those
graphs alive. A router strongly owns its live callbacks until unsubscribe, clear,
or ordinary reclamation of the router graph.

An emission captures its subscriber snapshot before invoking the first callback.
Subscription changes and clear do not change that snapshot. Nested emissions use
current routing state and run synchronously, depth-first. Clear followed by
subscribe creates new state while an old snapshot finishes. Nesting uses native
stack proportional to depth; unbounded recursion has no special guarantee.

There is no terminal close, source deinit, finalizer or automatic native-resource
ownership. Clear/unsubscribe release router ownership; active frames and snapshots
can retain callbacks until they return. The collector never invokes source
destructors. Applications retain responsibility for captured files and other
native resources. Ordinary `defer bus.clear()` is supported.

Use a router synchronously on its owning application thread. Managed storage and
GC coordination grant no concurrent mutation permissions. Runtime global access
keeps its existing owner check. Local routers gain no additional runtime thread
stamp. Owner async activations can create/use a local router across await, or use
an application global. Callable-containing router graphs cannot cross existing
async arguments/results/receivers or worker boundaries. There is no worker queue
or implicit asynchronous delivery.

Allocation and handler failures follow existing checked Runtime diagnostics.
Snapshot preparation failure delivers nothing; a later handler/runtime failure
stops remaining delivery, runs reached cleanup and preserves earlier effects.
Subscribe prepares metadata before its final event append; failed preparation
can leave inert null-wrapper records, which retain no failed callback. After a
successful append it commits directly to its held record and returns the token.
Unsubscribe removes the event wrapper first and clears its record before fallible
index cleanup. Cleanup failure can leave inactive metadata; retry and clear remain
valid. Neither source defer nor dictionary removal is assumed infallible. Repeated
host-driven failures have no bounded metadata-growth promise.

Compatible callback body reload preserves router state and tokens. Active calls
finish under their existing generation lease; later calls select the replacement
body. Signature, capture, specialization, layout and global initialization changes
keep existing restart rules. Cached native objects contain code and immutable
metadata; a new application receives fresh Runtime state and no subscribers.

The source declarations carry `##`, `@param` and `@return` documentation for
compiler-owned completion, hover, signatures, definitions and references. Internal
types and fields remain private. Module discovery uses the normal installed
standard source tree; there is no editor registration or second parser.

Runnable examples: [local duplicate removal](../../examples/channel/local.void)
and [application global](../../examples/channel/global.void), with its
[shared state module](../../examples/channel/state.void).

Cost: emit selects one topic and snapshots its K subscribers, with O(K) snapshot
storage and one wrapper call per callback. It never scans the registration index.
Unsubscribe includes event search, an active-registration scan and dictionary
deletion whose cost depends on retained capacity. Successful removal prunes empty
topics, including after churn. Clear's source body is one nullable reference
store; later collection accounts for reclamation. See the measured performance
and local acceptance limits in [HANDOFF](HANDOFF.md).
