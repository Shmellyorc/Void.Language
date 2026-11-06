# Grow and traverse a list

_Draft for Boss approval; repository documentation is canonical._

`[i32]` is a growable list of i32 values:

```void
func entry(latest_score: i32) -> i32:
    var scores: [i32] = [10, 20]
    scores.push(latest_score)
    var total = 0
    for score in scores:
        total += score
    return total
```

Save as `collections.void`, then run:

```sh
voidc check collections.void
voidc run collections.void 5
# 35
```

The numeric `entry` adapter prints the result and exits zero.

The output is 10 + 20 + 5. Fixed arrays, lists and ordered dictionaries have
different contracts. Bounds, mutation and recoverable failure behavior matter;
do not substitute an unchecked native buffer for a collection.

Canonical APIs: [fixed arrays](https://github.com/Shmellyorc/Void.Language/blob/main/docs/FIXED_ARRAYS.md), [lists](https://github.com/Shmellyorc/Void.Language/blob/main/docs/GROWABLE_ARRAYS.md), [dictionaries](https://github.com/Shmellyorc/Void.Language/blob/main/docs/DICTIONARIES.md), [collection helpers](https://github.com/Shmellyorc/Void.Language/blob/main/docs/collections/API.md).
