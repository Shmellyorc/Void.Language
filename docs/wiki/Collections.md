# Store and work through collections

Choose a list for a sequence that grows, a fixed array for a known length, or a
dictionary to find a value by key. For traversal rules, follow [Iteration](https://github.com/Shmellyorc/Void.Language/wiki/Iteration).

## Grow a list

```void
func main():
    var scores: [i32] = [10, 20]
    scores.push(5)
    var total = 0
    for score in scores:
        total += score
    print(total)
```

It prints `35`. Lists share their outer identity on assignment. If two bindings
refer to one list, changing its contents is visible through both. This differs
from copied [value structs and fixed arrays](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory).

## Copy a fixed array, look up a dictionary

```void
func main():
    var original: [i32, 3] = [1, 2, 3]
    var copied = original
    copied[0] = 9
    print(original[0], copied[0])
    var prices: {string: i32} = {"apple": 3, "pear": 4}
    print(prices["pear"])
    for price, fruit in prices:
        print(fruit, price)
```

It prints `1 9`, then `4`, followed by `apple 3` and `pear 4`. Fixed arrays copy
their value storage. Dictionaries share identity and preserve insertion order;
the loop binds the **value first, key second**. Use supported keys such as integers,
strings and eligible data-only enums/value structs. Floats are not dictionary keys.

## Avoid surprises while traversing

Indexing is checked. Use documented safe access when absence is expected rather
than depending on a failing lookup. An explicit slice creates a new outer list;
referenced elements retain their ordinary sharing rules.

Changing a list or dictionary's structure during live traversal invalidates the
iteration. Do not assume a reverse loop makes removal safe. Element replacement
and structural changes have different rules; see the precise collection guides.

For sorting, searching, mapping and filtering, use the
[`collections` standard module](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library). Content equality is supported
for eligible matching collection types; dictionary equality compares logical
contents independently of insertion order.

Next: [Iteration](https://github.com/Shmellyorc/Void.Language/wiki/Iteration).
Precise APIs: [Arrays and lists](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ARRAY_OPERATIONS.md),
[fixed arrays](https://github.com/Shmellyorc/Void.Language/blob/main/docs/FIXED_ARRAYS.md), [dictionaries](https://github.com/Shmellyorc/Void.Language/blob/main/docs/DICTIONARIES.md),
[equality and hashing](https://github.com/Shmellyorc/Void.Language/blob/main/docs/COLLECTION_EQUALITY.md).
