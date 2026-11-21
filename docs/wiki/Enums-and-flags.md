# Name alternatives and combine flags

Use an enum when a value belongs to a named set of alternatives. A tagged enum
can attach data to each case:

```void
enum download:
    waiting
    complete(bytes: i32)

func main():
    var state = download.complete(120)
    if state is download.complete(var received):
        print(received)
```

This prints `120`. Pattern matching binds the payload only on the matching path.
[Results and options](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values) use the same style of handling.

## Preserve an exact numeric value

```void
enum status(i32):
    ready = 1
    failed = 7

func main():
    print(status.ready.name, status.ready.value)
    var raw: i32 = 99
    if raw as? status is None:
        print("unknown status")
```

It prints `ready 1`, then `unknown status`. Numeric enums are nominal: an integer
or another enum is not silently accepted. `as?` checks representability and declared
membership; `as` fails on an invalid conversion. `.name` gives text and `.value`
the exact backing integer. Use those explicitly for display and native boundaries.

## Combine flags

```void
flags enum access(u8):
    none = 0
    read
    write
    read_write = read | write

func main():
    var permissions = access.read | access.write
    print(permissions.has(access.read_write))
    print(permissions.without(access.write).has(access.write))
```

This prints `true`, then `false`. Flags preserve all bits in their backing width,
including unnamed combinations and unknown bits. Ordinary numeric enums remain
closed sets. `has` tests all requested bits; `has_any` tests whether any are set.

The [raylib guide](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib#typed-input-and-raw-key-codes) shows why a
native API sometimes needs both a closed named enum and a raw integer operation.
A failed enum cast does not prove that an open provider code is unsupported.

Next: [Errors and nullable values](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values).
Precise numeric, flags and tagged semantics: [Enums](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ENUMS.md).
