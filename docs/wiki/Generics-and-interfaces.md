# Reuse algorithms and describe capabilities

A generic function can preserve the caller's type without duplicating a body:

```void
func identity<t>(value: t) -> t:
    return value

func main():
    print(identity(7))
    print(identity("hello"))
```

This prints `7`, then `hello`. The compiler infers `t` from each argument.
When inference has no concrete input, supply a type explicitly, such as
`identity<i32>(7)`. Specializations preserve the exact type and its authority.

## Describe the behavior a caller needs

An interface lists required methods. A managed type with matching accessible
signatures conforms structurally; no `implements` clause is needed:

```void
interface reader<t>:
    func read() -> t

struct cell:
    value: i32

func (self cell) read() -> i32:
    return self.value

func read_one<t>(const input: reader<t>) -> t:
    return input.read()

func main():
    var source = cell(value = 7)
    const view: reader<i32> = source
    print(read_one<i32>(view))
```

This prints `7`. `reader<i32>` describes the method result, while `const input`
borrows readonly access. The interface declaration is nominal; conformance comes
from accessible exact method signatures. Names of parameters do not select a
match; parameter types/order, result and readonly authority do.

## Know the boundaries

Generic structs, enums and interfaces use the same type-parameter idea.
Generic bodies are checked when specialized, so a diagnostic can identify both
the declaration and its requesting call. Constraints express needed members;
they do not create ordinary function overloads or runtime conversions.

Interfaces have bodyless synchronous requirements. General standalone value
boxing/downcasting and async interface dispatch are unavailable. Use concrete
[custom iterator state](https://github.com/Shmellyorc/Void.Language/wiki/Iteration) and ordinary
[async functions](https://github.com/Shmellyorc/Void.Language/wiki/Async) under their implemented contracts.

Next: [Static members](https://github.com/Shmellyorc/Void.Language/wiki/Static-members) or [Collections](https://github.com/Shmellyorc/Void.Language/wiki/Collections).
Precise rules: [Generics](https://github.com/Shmellyorc/Void.Language/blob/main/docs/GENERICS.md), [Interfaces](https://github.com/Shmellyorc/Void.Language/blob/main/docs/INTERFACES.md).
