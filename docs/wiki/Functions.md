# Define and call functions

_Draft for Boss approval; repository documentation is canonical._

Functions can have typed parameters, default values and named arguments:

```void
func subtotal(unit_price: i32 = 12, quantity: i32 = 1) -> i32:
    return unit_price * quantity

func entry(quantity: i32) -> i32:
    return subtotal(
        quantity = quantity,
    )
```

Save as `functions.void`, then run:

```sh
voidc check functions.void
voidc run functions.void 3
# 36
```

The numeric `entry` adapter prints the result and exits zero.

`quantity` is 3 while `unit_price` uses its default 12. The trailing comma and
multiline call are supported. This tutorial uses the legacy numeric entry adapter;
ordinary application `main` is synchronous and takes no CLI numeric input.

For receiver methods, generics and callable behavior, follow [methods](https://github.com/Shmellyorc/Void.Language/blob/main/docs/RECEIVER_METHODS.md), [generics](https://github.com/Shmellyorc/Void.Language/blob/main/docs/GENERICS.md) and [advanced language features](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ADVANCED_LANGUAGE.md).
