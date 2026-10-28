# Proposed `channel` public contract

**Implementation authority, 27 October 2026:** Boss approved this architecture
and settled readonly payloads, reusable clear and one payload type per router.
The proposal wording below is historical. [Current API](API.md) and
[implementation handoff](HANDOFF.md) describe the delivered source and evidence.

27 October 2026. Baseline **`72ee05db7e417645d434b3932bfc1e00ffd4ecb6`**.
**Proposal for Wrench review and Boss approval, not implemented.**
[Source research](RESEARCH.md), [complete engineering scope](ROADMAP.md).

## Public surface

One module, two public managed types and four methods. Signature notation below
omits bodies and is not a new interface/type-alias syntax:

```text
channel.router<t>()
channel.subscription                         opaque identity handle

router<t>.subscribe(topic: string, handler: func(const t)) -> subscription
router<t>.unsubscribe(const token: subscription) -> bool
router<t>.emit(topic: string, const payload: t)
router<t>.clear()
```

Use `var bus = channel.router<damage>()`. A parameterless constructor cannot
infer its payload type; require explicit `<damage>`. Receiver methods obtain t
from the instance; no `subscribe<damage>` or overload set. Public types/operations
are ordinary VOID declarations. No new builtin, type-erasure convention or
callable alias syntax.

A router is a managed reference. Assignment, fields and ordinary parameters
share that instance; separate construction creates independent routing/state.
A global variable is shared application storage, not a module singleton. The
module defines no default router/global registry.
Holding a router reference grants its public publishing capability; there is no
per-topic authorization layer. Its private event is still emitted only by that
event's owner method. Existing public event owner checks remain unchanged.

## Types, topics and payloads

Each instance has one exact payload type t. All its topics accept that type.
Integer widths/conversions, nullability, readonly authority, generic identity,
callback arity and no-result signatures use existing checking. No later emit
establishes/churns a topic's payload type and no unchecked string-to-type cast.

Recommend separate typed routers for unrelated payloads. The same topic string
on `router<damage>` and `router<chat_message>` describes unrelated subscriptions.
An application can explicitly choose one existing closed payload enum and match
its cases; that is ordinary typed data, not heterogeneous channel storage.
Typed-topic identities and a heterogeneous router are outside the initial scope.

Topics compare exact UTF-8 bytes through existing string dictionary rules. Empty
strings and embedded NUL are valid keys. Case, normalization and punctuation are
significant. `/` has no hierarchy, wildcard, filesystem, prefix or broadcast
meaning. Unknown topics emit nothing and are not inserted by emit. There are
no reserved error/control topics or compile-time topic typo diagnostics.

`func(const t)` gives handlers readonly payload access. Scalars retain ordinary
values; value structs/fixed arrays follow existing copy/snapshot rules; managed,
list, dictionary and interface referents retain identity with readonly authority.
No deep clone, serialization, move or implicit cross-thread transfer. Another
existing mutable alias may change the object, including between handlers.
Handlers may update their ordinary captured game state. A handler requiring
mutable payload authority does not match this proposed signature.

This readonly policy needs Boss approval. Choosing mutable `func(t)` instead
would be another coherent contract, with different accepted handlers/payloads;
do not silently accept both through casts or invented overloads.

## Subscription and token semantics

Subscribe appends one strong registration and returns its unique handle.
Registering the same callback twice produces two deliveries and two handles.
Unsubscribe removes exactly the registration belonging to that handle, including
an earlier duplicate; it does not apply built-in event's latest-original-callback
selection to the user's callback.

A copied/const handle identifies the same registration. It contains no callback,
router, topic, native resource or code lease. Retaining it after unsubscribe/clear
cannot retain those objects through the handle. Dropping it does not unsubscribe;
the router retains the connection until explicitly removed or cleared/reclaimed.
A lost handle can be dealt with by clearing the router. No weak subscriptions.

`unsubscribe` returns true when a live matching registration is disconnected;
false for an unrelated router's handle, stale handle or repeated call. Handles
from different payload routers can be passed, but never disconnect foreign
registrations. No numeric ID reuse or ownership hash is involved. Constructor
access should keep handles opaque; even an inert fabricated handle would simply
miss the private identity registry. No public token fields or mutable status.

## Dispatch and mutation

Emit evaluates its topic and payload expressions once under normal call ordering.
After lookup, the existing owner event captures its subscriber snapshot, then
invokes it synchronously in subscription order. Handler returns are absent;
recoverable errors must be handled inside handlers. Async function values are
not accepted and no delivery is automatically queued/scheduled.

| Operation while a handler runs | Current snapshot | Nested/later emission |
|---|---|---|
| Subscribe C | C is absent. | C is present, after existing live subscribers. |
| Unsubscribe B/self | Already selected callbacks still run. | Removed callback is absent. |
| Clear | Already selected callbacks still run. | Router is empty until new subscriptions. |
| Clear then subscribe C | Old snapshot completes. | New independent state contains C. |
| Nested emit | Suspends outer handler until nested dispatch returns. | Takes a fresh snapshot of current routing state. |
| Checked Runtime failure | Stops later callbacks; ordinary cleanup runs. | No automatic retry or rollback of earlier effects. |

Thus reentrancy is depth-first, with no pending-mutation queue, dispatch counter
or changed event semantics. A unique subscription wrapper captures its handler
**directly**, not through mutable registration state. Caller and snapshot roots
keep selected callbacks/captures safe after unsubscribe or clear.

Same-thread synchronous recursion can still exhaust a finite native stack.
The initial module does not add a recursion limiter or asynchronous trampoline.
Subscriber iteration is a loop; stack depth grows with nested emissions, not
subscriber count. Actual 1 MiB bounded nesting must be tested.

## Clear, ownership and failure

`clear()` detaches the entire private nullable state reference. It is reusable:
subsequent subscribe lazily creates fresh state. Repeated clear is harmless.
All old handles become inactive for this router. Active emissions may retain old
state/snapshots until return; GC/ordinary frame lifetime determines reclamation.
Clear does not invoke subscriber callbacks or deinitialize their captured resources.

Recommend no terminal `close`, no router/token `deinit`, and no automatic
unsubscribe/finalizer. These are pure managed data, not native resources.
`with bus` is consequently not part of the API. An application's resource owner
may explicitly call `bus.clear()` from its own cleanup; ordinary `defer bus.clear()`
is also available. Cleanup registration follows existing defer failure semantics.
Native files/textures captured by subscribers remain the application's responsibility.
A terminal-disposal contract would require an explicit closed state/error policy;
this is an owner decision, not silently added to the initial API.

Allocation, snapshot preparation, callback panic and native failures follow
VOID's existing checked Runtime failure/diagnostic model. There is no channel
error hierarchy, swallowed error, catch-and-continue or partial-delivery count.
Snapshot allocation failure delivers nothing; handler failure may follow earlier
successful handlers. Subscription changes already made by handlers persist.
Dictionary selection can allocate temporary Runtime layouts, and source `get`
may perform contains plus get. Unknown-topic no-op is not an allocation-free or
infallible promise; only routing state must remain untouched.

### Required internal failure invariants

Use one private nullable state containing topic and token-registration indexes.
Each registration's wrapper is nullable: null means inert preparation/removal
metadata; nonnull means installed. The private type's commit fields can be
module-accessible so the public router method performs direct stores.

1. Prepare handle, unique wrapper, bucket/registration and state/index storage.
   Keep the wrapper local; pending indexed metadata contains **null** wrapper,
   never the original callback. String/managed-key operations invoke no user code.
2. Insert metadata first. Event `+=` is the final fallible publication. After
   success, directly store wrapper in the held record and return the held handle.
   No helper, dictionary operation, new defer, cleanup call or copying allocation
   may intervene after append. Do not register rollback defers in this method.
3. A failed append leaves event storage unchanged. Earlier metadata may remain
   inert, but must retain no failed handler and dispatch no unreturned subscription.
   Strong all-metadata rollback under sustained OOM is not promised.
4. Unsubscribe finds the held record and removes its event wrapper first. Then
   directly null the record's wrapper, before fallible metadata deletion/pruning.
   Failure cannot lose the lookup record of a still-live callback. A later retry
   may clean inactive metadata and returns false once the callback is already gone.
5. Remove empty topic indexes after scanning remaining **active** registrations,
   outside the scan. No duplicate subscriber counter/list. Metadata left by a
   checked failure can remain until clear; repeated host-driven failing retries
   are not promised bounded metadata growth.
6. Clear's body is one nullable reference store, with no dictionary clear/remove,
   allocation of replacement maps, callback iteration or source destructor.

These are implementation obligations backed by existing primitive behavior,
not completed fault-injection evidence. Inspect typed IR and test persistent
state across failures. A failed call still reports ordinary Runtime failure;
there is no successful subscription result for an unreturned token.

## Working-style examples

These target the proposed module. The module does not yet exist; only the
underlying language compositions were executed during research.

### Application-wide instance in an ordinary module

```void
# messages.void
struct damage:
    amount: i32
```

```void
# state.void
use channel
use "messages.void" as messages
var combat = channel.router<messages.damage>()
```

```void
# main.void
use channel
use "state.void" as state
use "messages.void" as messages

func report(const info: messages.damage):
    print("Damage:", info.amount)

func publish(bus: channel.router<messages.damage>, amount: i32):
    bus.emit("player/damaged", messages.damage(amount = amount))

func main():
    defer state.combat.clear()
    const connection = state.combat.subscribe("player/damaged", report)
    publish(state.combat, 25)
    print(state.combat.unsubscribe(connection))
```

Each file imports the modules needed by its annotations; imports do not re-export
an alias. Keeping imports explicit avoids relying on special root visibility.
Any other importing game module sees the
same `state.combat` instance. The router may instead be a required managed field,
for example `notifications: channel.router<i32>`, supplied during construction.

### Local independent routers and exact duplicate removal

```void
use channel

func observe(const amount: i32):
    print(amount)

func main():
    var local = channel.router<i32>()
    var other = channel.router<i32>()
    const first = local.subscribe("score", observe)
    const second = local.subscribe("score", observe)
    local.emit("score", 7)                 # Prints twice.
    print(other.unsubscribe(first))         # false
    print(local.unsubscribe(first))         # true; second remains.
    print(local.unsubscribe(first))         # false
    local.emit("score", 8)                  # Prints once.
    local.clear()
    print(local.unsubscribe(second))        # false
    local.emit("score", 9)                  # No output.
```

### Nested dispatch with removal and addition

```void
use channel

func main():
    var bus = channel.router<i32>()
    var second_handle: channel.subscription? = null
    var second = func(const number: i32):
        print("B", number)
    var third = func(const number: i32):
        print("C", number)
    var first = func(const number: i32):
        print("A", number)
        if number == 1:
            if second_handle is Ok(var connected):
                bus.unsubscribe(connected)
            bus.subscribe("tick", third)
            bus.emit("tick", 2)
    const first_handle = bus.subscribe("tick", first)
    second_handle = bus.subscribe("tick", second)
    bus.emit("tick", 1)
    bus.clear()
```

Output order: `A 1`, `A 2`, `C 2`, `B 1`. The discarded third handle does not
cancel its registration. The outer snapshot still owns B. The underlying generic
event composition was executed with the equivalent `AACB` trace.

## Approval decisions

1. **Readonly message parameters:** recommend `func(const t)` so callers can emit
   readonly payloads without granting mutation authority. Alternative: mutable
   `func(t)` with the ordinary event payload contract. This must be explicit.
2. **Lifecycle:** recommend reusable clear plus GC, with ordinary Runtime failure
   and the valid inert-metadata recovery contract above. No terminal disposal,
   native ownership or new recoverable channel errors. If Boss wants a terminal
   close/RAII connection instead, revise ownership and failure behavior before
   engineering; do not bolt it onto an identity-only handle.

The exact public surface and token architecture remain for Wrench review/Boss
approval. Synchronous snapshots, existing ownership/worker/global rules, LLVM
19.1.7 and preserving public event behavior are settled mission constraints.
