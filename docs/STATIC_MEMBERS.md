# Static fields and methods

Static members belong to a nominal struct type. Access them through that type,
without constructing an instance. Managed `struct` and `value struct` declarations
support the same static syntax.

```void
struct game_settings:
    static max_players: i32 = 4
    private static debug_mode = false
    static const title = "VOID"

## Restore application settings.
## @return No value; restores the shared settings.
static func game_settings.reset():
    game_settings.max_players = 4
    game_settings.debug_mode = false
```

A static field is declared inside its type with `static name: type = expression`.
The type annotation may be omitted when the initializer determines it. Without an
initializer, the existing safe default rules apply: integers start at zero,
nullable values at null, collections at their normal empty/default values, and
required references/callables need an initializer. `static const name` requires an
initializer and has normal deep readonly authority. It is runtime initialized,
not a module compile-time constant. Rebinding it or mutating a referenced object
through it is rejected. Copying a fixed/value payload keeps the existing snapshot
and readonly-reference rules.

Static fields are excluded from instance layouts, constructor fields and instance
member completion. They may contain the ordinary types allowed in runtime globals,
including managed graphs, strings, nullable values, collections and function
values. Existing explicit resource cleanup rules still apply.

## Methods and lookup

Declare a method outside the type with `static func owner.name(parameters)` and an
optional result annotation. The declaration names an unqualified owner in the same
source module. There is no receiver parameter, implicit `self`, virtual dispatch
or instance allocation. Named/default arguments, returns, method-specific generic
parameters and supported function values follow ordinary function rules. Stored
callable static fields can also be invoked normally. Async static declarations
use `static async func` and retain the existing explicit scheduler-root adapter
contract; this does not add an async CLI entry convention.

```void
value struct vector2:
    x: f32
    y: f32

## Construct the origin without an existing vector.
## @return A vector with both coordinates set to zero.
static func vector2.zero() -> vector2:
    return vector2(x = 0, y = 0)
```

Visibility precedes `static`: public is the default; `private static` and
`internal static` are supported. A private static member is accessible in its
own type's methods/initializers and their nested closures, rather than arbitrary
free functions in the same file. Internal members retain the library-domain and
direct-import rules. Imported types use their ordinary qualified names, such as
`game.game_settings.reset()`. Aliases of one type address the same state.

Static and instance access remain distinct. An instance cannot select a static
member; a type cannot select an instance member. Conflicting directly declared
field/method names are rejected; there are no overloads or implicit hiding.
Static lookup selects the written nominal owner, without inherited/promoted
static aliases. Use the declaring base type explicitly for its static members.
Methods on imported types, static lifecycle methods, static virtual methods,
static events/using fields, static classes and static properties are not added.

## Generic owners

Every concrete generic specialization has its own static cells. For example,
`storage<i32>.item` and `storage<string>.item` are independent. All aliases of
`storage<i32>` share that specialization's cell.

```void
value struct storage<t>:
    static item: t?

## Read the payload for this concrete owner.
## @return The stored payload, or null before a value is stored.
static func storage<t>.read() -> t?:
    return storage<t>.item
```

A generic-owner method binds the owner's declared parameter names in order.
Method-specific parameters must use different names. Concrete arguments are
required when selecting the type; there is no unspecialized shared generic cell.
Unused generic declarations do not invent specializations. Concrete fields and
method signatures use the existing specialization and ownership machinery.

## Initialization, GC, concurrency and reload

Static fields reuse [runtime globals](RUNTIME_GLOBALS.md), including their guards,
precise rooted cells, failure state and Runtime lifetime. They initialize eagerly
before the entry function, including fields of reachable nongeneric types that
are never instantiated. Each concrete generic type discovered by compilation is
initialized once per application Runtime. Checking, tooling and cache preparation
do not execute initializers.

Module initialization retains the existing dependency/SCC order. Within a module,
module globals and static fields initialize together in source declaration order.
When multiple concrete specializations share a declaration site, canonical nominal
identity breaks the tie. There is no lazy type constructor, first-use lock or
implicit zero/partial read. Accessing an unpublished cell reports the existing
source-located runtime failure. Static fields are unavailable to compile-time
constants, instance field defaults and parameter defaults.

Reads and writes belong to the application owner thread. Owner async activations
may use them with normal interleaving. Worker-reachable shared-state accesses are
rejected, including transitive accesses through static helpers; pure static
functions can operate on safe worker snapshots. Static state is not implicitly
atomic or thread-local. Native authorization and ABI rules do not change.

Independent compilations/cache hits receive independent Runtime state. Compatible
body-only reload preserves cells and does not replay initializers. Changes to
static identity, type, visibility, readonly authority, initialization/order or
frozen initializer dependencies require restart through the existing compatibility
contract. Runtime shutdown releases persistent roots after normal resource drains;
the collector does not invoke source destructors.

## Editor support

The compiler publishes static fields/methods with their original source identities,
type owner, documentation and static semantic modifier. Hover, completion,
parameter help, definition and references use the same binding as compilation.
Generic declarations show symbolic types; selected usages show concrete readable
types. Private/instance members are excluded from external type completion. Unsaved
edits use the existing revision, cancellation and publication-freshness contracts.

See the [runnable example](../examples/learn/static_members.void) and
[G80 engineering report](projects/G80_STATIC_MEMBERS.md) for evidence and native
platform qualification limits.
