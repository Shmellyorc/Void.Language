# Random public API audit

The canonical source contains **66 public top-level declarations**, including
**62 ordinary function/method declarations**. Numeric direct-call adaptation
expands the 40 numeric declarations into 110 supported numeric call forms.
All declarations, enum cases and public error fields have `##` documentation;
written parameters and return values use accepted `@param` / `@return` tags.
Private bounded implementations carry the active-parameter docs used by the
compiler-owned arity signature help. No public below/between names exist.

## Numeric families

For each of i8/u8/i16/u16/i32/u32/i64/u64/f32/f64:

| Owner | Plain forms | Recoverable forms |
|---|---|---|
| Module | `rand_T()` / `rand_T(maximum)` / `rand_T(lower, upper)` | `try_rand_T()` / `try_rand_T(maximum)` / `try_rand_T(lower, upper)` |
| Mutable generator | Same three `rand_T` forms | `try_rand_T(maximum)` / `try_rand_T(lower, upper)` |

Zero-argument module function values retain their single actual signature.
Bounds use exact T and ordinary named argument binding. Integer unit-width
intervals consume no word. Unit floats extract 24/53 high bits.

## Source declaration inventory

| Source line | Public declaration |
|---|---|
| 5 | `const algorithm_version: i32 = 1` |
| 8 | `enum error_kind` |
| 19 | `struct error extends core.error` |
| 40 | `value struct generator` |
| 53 | `func (self generator) init(seed: u64)` |
| 65 | `func (self generator) seed(value: u64)` |
| 141 | `func (self generator) boolean() -> bool` |
| 148 | `func (self generator) rand_range<t>(lower: t, upper: t) -> t` |
| 155 | `func (self generator) try_rand_range<t>(lower: t, upper: t) -> result<t, error>` |
| 162 | `func (self generator) rand_i8() -> i8` |
| 168 | `func (self generator) try_rand_i8(maximum: i8) -> result<i8, error>` |
| 195 | `func (self generator) rand_u8() -> u8` |
| 201 | `func (self generator) try_rand_u8(maximum: u8) -> result<u8, error>` |
| 228 | `func (self generator) rand_i16() -> i16` |
| 234 | `func (self generator) try_rand_i16(maximum: i16) -> result<i16, error>` |
| 261 | `func (self generator) rand_u16() -> u16` |
| 267 | `func (self generator) try_rand_u16(maximum: u16) -> result<u16, error>` |
| 294 | `func (self generator) rand_i32() -> i32` |
| 300 | `func (self generator) try_rand_i32(maximum: i32) -> result<i32, error>` |
| 327 | `func (self generator) rand_u32() -> u32` |
| 333 | `func (self generator) try_rand_u32(maximum: u32) -> result<u32, error>` |
| 360 | `func (self generator) rand_i64() -> i64` |
| 366 | `func (self generator) try_rand_i64(maximum: i64) -> result<i64, error>` |
| 393 | `func (self generator) rand_u64() -> u64` |
| 399 | `func (self generator) try_rand_u64(maximum: u64) -> result<u64, error>` |
| 426 | `func (self generator) rand_f32() -> f32` |
| 432 | `func (self generator) try_rand_f32(maximum: f32) -> result<f32, error>` |
| 469 | `func (self generator) rand_f64() -> f64` |
| 475 | `func (self generator) try_rand_f64(maximum: f64) -> result<f64, error>` |
| 524 | `func (self generator) choose<t>(const items: [t]) -> t` |
| 531 | `func (self generator) try_choose<t>(const items: [t]) -> result<t, error>` |
| 537 | `func (self generator) shuffle<t>(items: [t])` |
| 550 | `func (self generator) shuffled<t>(const items: [t]) -> [t]` |
| 567 | `func seed(value: u64)` |
| 573 | `func from_entropy() -> result<generator, error>` |
| 583 | `func try_initialize() -> result<bool, error>` |
| 593 | `func boolean() -> bool` |
| 598 | `func try_boolean() -> result<bool, error>` |
| 606 | `func rand_range<t>(lower: t, upper: t) -> t` |
| 613 | `func try_rand_range<t>(lower: t, upper: t) -> result<t, error>` |
| 624 | `func rand_i8() -> i8` |
| 630 | `func try_rand_i8() -> result<i8, error>` |
| 668 | `func rand_u8() -> u8` |
| 674 | `func try_rand_u8() -> result<u8, error>` |
| 712 | `func rand_i16() -> i16` |
| 718 | `func try_rand_i16() -> result<i16, error>` |
| 756 | `func rand_u16() -> u16` |
| 762 | `func try_rand_u16() -> result<u16, error>` |
| 800 | `func rand_i32() -> i32` |
| 806 | `func try_rand_i32() -> result<i32, error>` |
| 844 | `func rand_u32() -> u32` |
| 850 | `func try_rand_u32() -> result<u32, error>` |
| 888 | `func rand_i64() -> i64` |
| 894 | `func try_rand_i64() -> result<i64, error>` |
| 932 | `func rand_u64() -> u64` |
| 938 | `func try_rand_u64() -> result<u64, error>` |
| 976 | `func rand_f32() -> f32` |
| 982 | `func try_rand_f32() -> result<f32, error>` |
| 1018 | `func rand_f64() -> f64` |
| 1024 | `func try_rand_f64() -> result<f64, error>` |
| 1059 | `func choose<t>(const items: [t]) -> t` |
| 1066 | `func try_choose<t>(const items: [t]) -> result<t, error>` |
| 1072 | `func shuffle<t>(items: [t])` |
| 1078 | `func try_shuffle<t>(items: [t]) -> result<bool, error>` |
| 1087 | `func shuffled<t>(const items: [t]) -> [t]` |
| 1095 | `func try_shuffled<t>(const items: [t]) -> result<[t], error>` |

No public numeric generic overload machinery exists. `rand_range<t>` accepts
only integer primitives. Collections take growable lists; fixed callers slice.
Readonly children stay readonly through const-input choose/shuffled. Entropy
errors retain native codes; invalid arguments and no-draw defaults skip seeding.

Independent oracle, all-mode execution, frontend/IR, lifecycle/GC/fault/entropy,
LSP, installation, stack and strict profile receipts are indexed in
[IMPLEMENTATION.md](IMPLEMENTATION.md).
