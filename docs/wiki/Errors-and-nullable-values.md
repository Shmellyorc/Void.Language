# Handle failure and missing values

A failed operation and an absent value are different situations. Use `result<T,E>`
for success or a typed error, and `T?` when the value may simply be absent.

## Handle a result

```void
func positive(value: i32) -> result<i32, string>:
    if value < 1:
        return Err("expected a positive number")
    return Ok(value)

func doubled(value: i32) -> result<i32, string>:
    var checked = positive(value)?
    return Ok(checked * 2)

func main():
    if doubled(7) is Ok(var answer):
        print(answer)
    if doubled(0) is Err(var message):
        print(message)
```

It prints `14`, then `expected a positive number`. `?` unwraps a successful result
or propagates its error through a compatible enclosing result function. It still
runs required cleanup. You can also use `match` to handle both alternatives in one
place. See [Standard library](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library) for input and file operations
that return structured errors.

## Represent absence

```void
func main():
    var selected: i32? = null
    if selected == null:
        print("no selection")
    selected = 0
    if selected is Ok(var present):
        print(present)
```

This prints `no selection`, then `0`. Zero is a present value. `Ok` in this pattern
extracts a nullable payload; it does not convert the nullable value into a result.
A checked extraction with `as` requires presence. `option<T>` has explicit `Some`
and `None` cases, used by [custom iterators](https://github.com/Shmellyorc/Void.Language/wiki/Iteration).

Do not replace every error with a default silently. Decide whether your caller
needs a message, retry, partial progress or cleanup information. Native owners
still require their [documented resource cleanup](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory#close-native-resources).
Runtime failure and task cancellation have their own behavior; they do not become
arbitrary typed `Err` values. See [Async](https://github.com/Shmellyorc/Void.Language/wiki/Async#own-the-work).

Next: [Collections](https://github.com/Shmellyorc/Void.Language/wiki/Collections).
Precise rules: [Nullable values and results](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#nullable-values-and-results).
