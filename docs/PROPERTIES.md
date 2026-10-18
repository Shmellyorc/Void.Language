# Properties and callable structs

The initial properties/callable-struct delivery passed 76/76 Linux Debug, Release
and strict sanitizer suites. The Stage 4 continuation adds the remaining systems;
see [ADVANCED_LANGUAGE.md](ADVANCED_LANGUAGE.md) and the latest WORK_LOG entry for
combined validation. Stage 3 remains locked at cd95e373 / CI #65; hosted Stage 4
acceptance is pending.

## Properties

```typescript
struct player:
    backing: i32 = 100

property (p player) health: i32:
    get:
        return p.backing
    set(value):
        if value < 0:
            p.backing = 0
        else:
            p.backing = value
```

Read `p.health`; assign `p.health = value` or use an ordinary numeric/string
compound assignment. A getter is required; a setter is optional. Getters cannot
mutate their receiver, including through helper calls or referenced children.
Accessors use ordinary method visibility, owning-module, type/effect and deep
readonly checks. Property and field/method names cannot conflict. Calling a
non-callable property with parentheses is an error. Properties cannot satisfy
ordinary interface method requirements.

Managed/value and concrete generic receiver types are supported, with normal
inheritance and one-hop `using` member selection. Properties carry all existing
source value types. Fixed-array/value results copy independently; list/dictionary
and managed results retain identity. A getter-only property can expose a mutable
reference/list/dictionary when its receiver authority permits it; replacing that
property still requires its setter. Readonly receiver paths retain deep readonly.

Receiver selection occurs before the RHS. Simple assignment calls the setter
without calling the getter. Compound assignment reads once before evaluating the
RHS, then calls the setter. Nested value/array mutation reads the property value
once and commits the changed value through its setter. Crossing a managed/list/
dictionary reference mutates that referent without replacing its owning property.
RHS rebinding does not redirect a captured managed receiver. Ordinary failure and
allocation cleanup, precise roots and transactional value publication remain in
force; property methods do not roll back earlier user-visible side effects.

Accessors lower to ordinary typed-IR method calls with the existing LLVM ABI,
GC and JIT/AOT paths. There is no property Runtime, registry or native accessor
ABI. Tooling exposes source references to the accessor declarations. Getter/setter
body changes can reload; removing a setter, changing its type or changing a
property into a method changes the compatibility contract and requires restart.

## Callable structs

```typescript
value struct multiplier:
    amount: i32
func (m multiplier) call(value: i32) -> i32:
    return m.amount * value

func apply<t>(action: t, value: i32) -> i32:
    return action(value)
```

Instances can be invoked using `object(value)`, including selected fields,
properties, indexed elements and returned temporaries. The ordinary `call` method
supplies visibility, argument, result, readonly and receiver-effect checking.
Generic functions can receive these concrete callables. Managed callable aliases
share identity; value callable assignment/arguments retain ordinary independent
copy semantics. Mutating value callables use ordinary local/field storage, or commit a computed
property value through its setter. Managed indexed callables keep their identity.
Mutating inline value receivers selected from arrays/dictionaries remain rejected:
copy-back could overwrite mutations performed through another alias during the
callee. Extract a value, mutate that independent local and explicitly replace the
element instead. This also applies to mutating value-property setters on indexed
inline elements; getter reads are allowed. The existing array rejection regression
is preserved. No escaping mutable element borrow is introduced.

The continuation adds source function values, anonymous/escaping closures, method
callbacks, events, enums/results/patterns and resource cleanup through the same
pipeline; see [ADVANCED_LANGUAGE.md](ADVANCED_LANGUAGE.md). Existing native
callback/quiescence machinery remains unchanged. Dictionary `.get()` and list
`.pop()` still await their approved error types/signatures.

## Regression coverage

Examples are in `examples/properties`: main, generic, order, references, methods,
callable, selection, imports, failure and receiver. `properties_source` checks normal/O2 execution,
GC pressure, precise roots, allocation failure/cleanup/retry, readonly/invalid
source, visibility and accessor reload/old code leases. `properties_cli` checks
all ten fixtures through normal/O2 JIT and separately linked AOT (44 successful
executions and 12 checked failures).
The references fixture includes a 2 MiB heap-backed fixed array. Exact consolidated
commands/results and remaining limitations are recorded in WORK_LOG.md.
