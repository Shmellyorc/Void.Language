# Define and call functions

Give an operation a name, typed parameters and an optional result type:

```void
func subtotal(unit_price: i32 = 12, quantity: i32 = 1) -> i32:
    return unit_price * quantity

func main():
    print(subtotal(quantity = 3))
```

This prints `36`. The call supplies `quantity` and uses the default `unit_price`.
Omit `-> ...` for a function with no result. Keep required parameters before
parameters with defaults.

## Call with useful names

Positional arguments come first; named arguments can follow in any order.
For example, `subtotal(10, quantity = 2)` and
`subtotal(quantity = 2, unit_price = 10)` both calculate 20. Supplied expressions
evaluate once in written order before they bind to parameter order.

Functions do not form ordinary overload sets. Give distinct operations distinct
names. Use [generics](https://github.com/Shmellyorc/Void.Language/wiki/Generics-and-interfaces) to express one operation over
multiple types without duplicating its implementation.

## Put behavior beside data

A receiver method operates on a [struct](https://github.com/Shmellyorc/Void.Language/wiki/Structs-and-initialization):

```void
value struct counter:
    total: i32

func (self counter) add(amount: i32):
    self.total += amount

func main():
    var clicks = counter(total = 1)
    clicks.add(2)
    print(clicks.total)
```

This prints `3`. The writable `clicks` binding lets the method mutate its value.
Managed receiver identity and readonly authority are covered in
[Managed memory](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory). Methods that belong to a type rather than an
instance use [static members](https://github.com/Shmellyorc/Void.Language/wiki/Static-members).

Functions can also be stored as callable values. Closures capture their required
state under the [callable contract](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ADVANCED_LANGUAGE.md); async callable
values have additional restrictions. For suspension and task lifetimes, continue
to [Async](https://github.com/Shmellyorc/Void.Language/wiki/Async).

Next: [Structs and construction](https://github.com/Shmellyorc/Void.Language/wiki/Structs-and-initialization).
Precise calls: [Functions and calls](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#functions-and-calls).
