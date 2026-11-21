# Put shared operations on a type

A static member belongs to a type. Use it for a named factory or settings shared
by instances; use a [receiver method](https://github.com/Shmellyorc/Void.Language/wiki/Functions#put-behavior-beside-data) when an
operation needs a particular instance.

```void
value struct position:
    x: i32
    y: i32
    static const label = "origin"

static func position.zero() -> position:
    return position(x = 0, y = 0)

func main():
    var start = position.zero()
    print(position.label, start.x, start.y)
```

This prints `origin 0 0`. The static field is declared inside the struct; the
static method is declared outside it with `static func owner.name(...)`.
Select both through `position`, rather than through `start`.

## Understand shared state

Static fields do not become instance fields or constructor arguments. They follow
runtime initialization rules and initialize before the application entry. A
`static const` field is readonly runtime state, not a module compile-time constant.
Checking a source file does not run its initializers.

Each concrete [generic specialization](https://github.com/Shmellyorc/Void.Language/wiki/Generics-and-interfaces) has its own
static storage. Aliases of the same specialization share it. Static state retains
ordinary managed roots and resource cleanup obligations; there is no implicit
lock, lazy type constructor or permission for worker tasks to access globals.
See [Async](https://github.com/Shmellyorc/Void.Language/wiki/Async#own-the-work) before introducing concurrent state.

Next: [Projects and modules](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules).
Precise rules: [Static members](https://github.com/Shmellyorc/Void.Language/blob/main/docs/STATIC_MEMBERS.md),
[Runtime globals](https://github.com/Shmellyorc/Void.Language/blob/main/docs/RUNTIME_GLOBALS.md).
