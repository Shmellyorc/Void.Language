# Build data with structs

A struct groups related data. Choose a `value struct` when copying should create
independent value storage, or an ordinary managed `struct` when aliases should
refer to one object. [Managed memory](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory) shows the difference.

## Construct a value

```void
value struct position:
    x: i32
    y: i32

func main():
    var start = position(x = 2, y = 3)
    print(start.x, start.y)
```

It prints `2 3`. Without a custom initializer, named fields provide the values;
omitted fields use their permitted defaults. Use `position(...)`, without `new`.
Required nonnullable managed references and function values need initialization.

## Validate construction

A custom `init` defines the constructor's parameters and can reject an input with
an ordinary [result](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values):

```void
value struct ticket_count:
    amount: i32

func (self ticket_count) init(requested: i32) -> result<bool, string>:
    if requested < 1:
        return Err("at least one ticket is required")
    self.amount = requested
    return Ok(true)

func main():
    if ticket_count(3) is Ok(var valid):
        print(valid.amount)
    if ticket_count(0) is Err(var message):
        print(message)
```

It prints `3`, then `at least one ticket is required`. A fallible constructor
returns `result<ticket_count, string>` at the call site. Success is the `Ok` case;
`Ok(true)` is the convention, and `Ok(false)` is also successful. Failed construction
does not hand out a partially built object. Copies do not rerun the initializer.

## Grow the design when needed

Add [receiver methods](https://github.com/Shmellyorc/Void.Language/wiki/Functions#put-behavior-beside-data) for instance behavior,
[static methods](https://github.com/Shmellyorc/Void.Language/wiki/Static-members) for type-level operations, and
[interfaces](https://github.com/Shmellyorc/Void.Language/wiki/Generics-and-interfaces) for shared contracts. Properties expose
controlled access with `get:` and/or `set:`. Composition with `using` embeds stored
components and promotes members one hop; managed inheritance uses `extends`.
Follow the detailed contracts before depending on construction order, promotion
or overriding behavior.

Next: [Enums and flags](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags).
Precise rules: [Initialization](https://github.com/Shmellyorc/Void.Language/blob/main/docs/INITIALIZATION.md),
[properties](https://github.com/Shmellyorc/Void.Language/blob/main/docs/PROPERTIES.md), [composition](https://github.com/Shmellyorc/Void.Language/blob/main/docs/COMPOSITION.md),
[inheritance](https://github.com/Shmellyorc/Void.Language/blob/main/docs/INHERITANCE.md).
