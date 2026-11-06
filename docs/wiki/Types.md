# Choose and convert types

_Draft for Boss approval; repository documentation is canonical._

VOID has exact-width signed/unsigned integers, `f32`/`f64`, booleans and immutable
UTF-8 strings. Local inference does not remove static checking. Use explicit
conversions when crossing numeric types:

```void
func entry(minutes: i32) -> i32:
    var seconds: i64 = (minutes as i64) * 60
    return seconds as i32
```

Save as `types.void`, then run:

```sh
voidc check types.void
voidc run types.void 2
# 120
```

The numeric `entry` adapter prints the result and exits zero.

The example widens minutes to i64 before multiplying. Narrowing/conversion rules
and failures depend on the source and target types; do not infer them from C casts.
Value structs copy their fields; ordinary managed structs share identity.

Canonical contracts: [numeric types](https://github.com/Shmellyorc/Void.Language/blob/main/docs/NUMERIC_TYPES.md), [value semantics](https://github.com/Shmellyorc/Void.Language/blob/main/docs/VALUE_SEMANTICS.md), [strings](https://github.com/Shmellyorc/Void.Language/blob/main/docs/STRINGS.md).
