# Copy values, share objects and close resources

Copying a value and sharing an object are both useful. Choose intentionally when
you design a [struct](https://github.com/Shmellyorc/Void.Language/wiki/Structs-and-initialization) or a [collection](https://github.com/Shmellyorc/Void.Language/wiki/Collections).

```void
value struct point:
    x: i32

struct counter:
    total: i32

func main():
    var original = point(x = 2)
    var copied = original
    copied.x += 10
    var shared = counter(total = 0)
    var alias = shared
    alias.total += 5
    print(original.x, copied.x, shared.total)
```

This prints `2 12 5`. `copied` has its own value storage. `shared` and `alias`
refer to the same managed counter. Lists and dictionaries also retain shared
identity on assignment; fixed arrays copy their own value storage.

## Readonly access follows references

A `const` binding preserves deep readonly authority through referenced children.
Copying a scalar or a value's own fields can produce independent writable storage;
it does not grant writable access to a readonly referenced child. Copying a
reference field preserves that object's identity and permitted access.

## Close native resources

Garbage collection keeps reachable managed data alive. It does not promise timely
closure of files, native images, callbacks or other external resources.

```void
use io as io

func show_open() -> result<bool, io.error>:
    with io.open("notes.txt")? as handle:
        print("opened")
    return Ok(true)

func main():
    if show_open() is Err(var failure):
        print("could not open")
```

Create `notes.txt` in the working directory. On successful open it prints
`opened`; `with` closes the handle when its body ends. A failed open propagates
through `?` and the caller prints `could not open`. Resource wrappers can share close authority;
a copied wrapper is not a new independent native resource. For fallible reads and
cleanup outcomes, follow the specific API's result contract in
[the standard-library guide](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library#read-a-file).

Use `with` for types with scoped `deinit` cleanup, and `defer` for explicit calls.
Cleanup still applies when you return or propagate an error. Task cancellation
must drain owned native work before resource destruction; see [Async](https://github.com/Shmellyorc/Void.Language/wiki/Async).
The [raylib image example](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib#a-first-native-operation) shows an
explicit native unload.

Next: [Standard library](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library).
Precise rules: [Value semantics](https://github.com/Shmellyorc/Void.Language/blob/main/docs/VALUE_SEMANTICS.md),
[Managed structs](https://github.com/Shmellyorc/Void.Language/blob/main/docs/MANAGED_STRUCTS.md), [Resource lifecycle](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ADVANCED_LANGUAGE.md),
[GC Runtime](https://github.com/Shmellyorc/Void.Language/blob/main/docs/GC_RUNTIME.md).
