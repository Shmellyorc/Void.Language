# Choose and convert types

Types describe the values your program can hold. Start with `i32` for ordinary
counts, `bool` for decisions and `string` for text; use a specific width when a
file format or native API requires it.

| Kind | Types |
| --- | --- |
| Signed integers | `i8`, `i16`, `i32`, `i64` |
| Unsigned integers | `u8`, `u16`, `u32`, `u64` |
| Floating point | `f32`, `f64` |
| Boolean and text | `bool`, `string` |

## Convert deliberately

```void
func main():
    var minutes: i32 = 2
    var seconds: i64 = (minutes as i64) * 60
    print(seconds)
```

This prints `120`. `as` converts explicitly; numeric mixing and narrowing follow
checked source/target rules. A cast from text is not a number parser. Use
[`convert.parse`](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library#parse-input) to read a number from text.

Ordinary unsuffixed literals default to `i32` or `f32` without another exact type
from context. Boolean values are not numeric truth values.

## Numbers and bits

Integer arithmetic has fixed width. Signed `MIN / -1` yields `MIN`, and
`MIN % -1` yields zero; integer division or remainder by zero fails. Shift counts
are normalized modulo the left operand's width. Signed right shift is arithmetic;
unsigned right shift is logical. See the [numeric contract](https://github.com/Shmellyorc/Void.Language/blob/main/docs/NUMERIC_TYPES.md)
for conversion bounds, floating point and overflow details.

`&`, `|`, `^`, `~`, `<<` and `>>` operate on integers. For named bit sets, use
[flags enums](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags#combine-flags) rather than hand-maintained masks.

## Text and structured values

Strings contain validated UTF-8. `"Hello, {name}!"` interpolates a value with a
supported display implementation. Byte lengths and Unicode-scalar positions are
different concepts; use the [text library](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library) for text operations.

Use [structs](https://github.com/Shmellyorc/Void.Language/wiki/Structs-and-initialization) for records, [enums](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags)
for alternatives and [collections](https://github.com/Shmellyorc/Void.Language/wiki/Collections) for groups of values.
A nullable type such as `i32?` adds absence without using a special integer;
see [Errors and nullable values](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values).

Next: [Functions](https://github.com/Shmellyorc/Void.Language/wiki/Functions).
Precise rules: [Numbers and operators](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#numbers-and-operators).
