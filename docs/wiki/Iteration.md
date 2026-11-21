# Traverse built-in and custom sources

Use `for` for a range, list, array or dictionary:

```void
func main():
    var total = 0
    for number in 1..3:
        total += number
    print(total)
    for item, index in ["a", "b"]:
        print(index, item)
```

It prints `6`, then `0 a` and `1 b`. `..` includes the final endpoint; `..<`
excludes it. The second binder follows the item and supplies the index or key.
Read [Collections](https://github.com/Shmellyorc/Void.Language/wiki/Collections) before structurally changing a source during a loop.

## Make your own source iterable

A source exposes `iterator()`; its state exposes `next() -> option<T>`:

```void
value struct countdown:
    start: i32

value struct countdown_state:
    remaining: i32

func (self countdown) iterator() -> countdown_state:
    return countdown_state(remaining = self.start)

func (self countdown_state) next() -> option<i32>:
    if self.remaining < 1:
        return None
    var current = self.remaining
    self.remaining -= 1
    return Some(current)

func main():
    for number in countdown(start = 3):
        print(number)
```

This prints `3`, `2`, `1`. `Some` supplies the next item and `None` finishes.
The loop acquires state once. Each new loop can obtain its own cursor.
Implement `index()` when you want a second binder and `reverse_iterator()` when
your source supports reverse traversal; the compiler does not invent either.

Managed iterator state can have scoped `deinit` cleanup. Custom sources define
their own mutation policy; wrapping a list does not automatically give a custom
iterator every built-in mutation check. See [resource ownership](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory).

## Await the next item

An asynchronous source uses `async_iterator()` and a managed state's
`async next() -> option<T>`. Here is a complete adapter entry example:

```void
struct number_source:
    count: i32 = 3

struct number_state:
    current: i32
    limit: i32

func (self number_source) async_iterator() -> number_state:
    return number_state(limit = self.count)

async func (self number_state) next() -> option<i32>:
    if self.current >= self.limit:
        return None
    self.current += 1
    return Some(self.current)

async func entry(input: i32) -> i32:
    var total = 0
    for await number in number_source():
        total += number
    return total
```

This sums to `6`. Save as `async-iteration.void` and use `voidc check` for source
checking. Execution needs the explicit [async host](https://github.com/Shmellyorc/Void.Language/wiki/Async#check-and-execute-the-example);
ordinary CLI run/build does not host this entry. The loop awaits each advancement
before starting the next. Built-in lists do not become async sources just because
you write `for await`. No generator or `yield` facility is implied.

Next: [Async and task ownership](https://github.com/Shmellyorc/Void.Language/wiki/Async).
Precise protocol and cancellation rules: [Custom iteration](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CUSTOM_ITERATION.md).
