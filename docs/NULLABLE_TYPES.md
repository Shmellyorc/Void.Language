# Nullable types

Non-nullable types are the default. Add `?` to opt into a value that can contain
its ordinary payload or `null`:

```void
var health: i32 = 100
var optional_health: i32? = null
var nickname: string? = "Boss"
var items: [string]? = null
```

G79 supports all fixed-width numeric primitives, `bool`, strings, ordinary value
structs, tagged/numeric/flags enums, fixed arrays, growable lists and dictionaries.
Existing nullable managed references, interfaces and function values keep their
representation and behavior. Nullable types work in locals, runtime globals,
constant expressions, fields, parameters, return values, collections and concrete
generic specializations. A payload must itself be an ordinary supported value;
private construction views and multiple-return packs are not nullable payloads.
Native C signatures do not acquire new layouts or automatic nullable conversions.
Inline value containment must remain finite: `value struct node` with a `node?`
field is invalid. A growable list or managed-reference edge still permits valid
recursive graphs, including lists of nullable value elements.

## Presence and defaults

`T` lifts to the same `T?` at ordinary assignment, argument and return boundaries.
This does not convert unrelated payload types or allow `null` in `T`. A bare
`null` needs an expected nullable type; `var value = null` is ambiguous and rejected.
Omitted nullable locals and fields default to null without constructing a payload.
Fixed arrays of nullable elements also default to null elements.

`0`, `false`, `""`, an empty list and an empty dictionary are present values. They
are never used as an absence sentinel. Null is a separate state even for strings,
whose existing empty value internally uses a zero root word.

## Checking and extracting

Use `== null` / `!= null`, or a presence pattern:

```void
func score(value: i32?) -> i32:
    if value is Ok(var present):
        return present
    return 0
```

`Ok(pattern)` matches a present payload and then applies the existing pattern
checker to `T`. A null value does not match it. `match` supports `null` and `Ok`
arms, including nested patterns such as `Ok(Ok(null))` for a nullable successful
result containing null. `Err` is a result alternative, never nullable absence.
Nullable bools are not implicit conditions; unwrap or match them explicitly.
Null checks do not introduce implicit dereferences or general flow narrowing.

`value as T` extracts the same payload type with a checked failure when absent.
The failure follows the existing source-located runtime-error path, with exit 1
for a compiler-run application. A null extraction in an evaluated module constant
is a compile-time error. It never reads inactive payload storage. Conversion to
a different numeric type is a separate explicit operation after extraction.

`value as? T` safely extracts to canonical `option<T>`: `Some(payload)` when
present, `None` when absent. The operand runs once. This works for nullable value,
managed, interface and callable families. Existing integer range/discriminant
`as?` conversions retain their behavior.

```void
var nickname: string? = ""
if (nickname as? string) is Some(var text):
    print(text) # An empty string is still present.
```

Nullability is not an implicit conversion to `option<T>` or `result<T,E>`.
`option<T>` now admits ordinary supported payloads, including nullable values;
`Some(null)` remains distinct from `None`. A nullable result remains distinct
from a result whose success payload is nullable. Result `?` propagation keeps
its existing contract; it does not silently propagate nullable absence.

Equality with null is available for every nullable payload. Comparing two
nullable values uses the payload's existing equality: null equals null, one null
is unequal to one present value, and two present values compare their payloads.
Unsupported payload equality still diagnoses. No collection-content or unrelated
value-struct equality is invented. Numeric NaN and signed-zero equality retain
their existing semantics. Arithmetic, ordering, bitwise operations and member
access require extraction first; no lifted arithmetic or `??` is introduced.
Eligible nullable dictionary keys reuse existing key eligibility and hashing;
null and a present zero are distinct keys. Unsupported key payloads stay rejected.

## Printing and diagnostics

`print(value)`, `string(value)` and interpolation display `null` when a nullable
value is absent. A present value uses its payload's ordinary display, including
custom string hooks. Zero prints `0`, false prints `false`, and a present empty
string contributes empty text. Nullable elements inside collections use the same
rules; private presence tags and storage names are never printed. This does not
change ordinary enum, option or result display conventions.

Readonly errors name logical source paths such as `carrier?.child`. The private
presence wrapper is not a user-selectable field. Editor primary and related
messages use readable names while preserving diagnostic codes, source locations
and authority checks.

## Generics, ownership and presentation

`t?` in a generic declaration specializes using the same existing machinery as
`t`. Inference can obtain the payload type from an ordinary or nullable argument.
An unconstrained `t` can itself be nullable. Applying `t?` when the concrete `t`
is already nullable retains that nullable type, rather than nesting presence bits.
A second written `?` suffix is invalid. Generic constraints retain their existing
non-nullable underlying-type requirements.

Nullable wrappers preserve ordinary copy behavior: scalar/value payloads copy as
values, fixed arrays retain snapshot behavior, and lists/dictionaries/managed
references retain shared identity. Readonly reference authority survives wrapping,
copying, return, `as`, `as?` and pattern extraction. Nullable value storage reuses
VOID's private tagged sums with zeroed inactive fields and precise GC descriptors.
Reference/interface/callable nullable layouts remain unchanged. There is no new
Runtime ownership model or public native ABI.

Editor hover, completion details and signature help show `i32?`, `string?`,
`[string]?` and `player?`. Generic declarations show symbolic `t?`; concrete calls
show their specialization. Internal nullable storage names are not public APIs.
Use `(func(i32) -> i32)?` for an optional function returning a required integer;
`func(i32) -> i32?` is a required function returning an optional integer.

See the runnable [nullable example](../examples/learn/nullable.void),
[advanced patterns](ADVANCED_LANGUAGE.md), [numeric conversions](NUMERIC_TYPES.md)
and [G79 engineering evidence](projects/G79_NULLABLE_COMPLETENESS.md).
