# Enums and flags

Enums have nominal identity: two declarations remain different types even when
all their names, payloads or integer values coincide. There is no implicit
integer/enum conversion. `is` tests cases and binds payloads; `match` dispatches
across cases. Both remain useful alongside equality.

## Tagged enums and generics

```void
## A possibly absent typed reading.
enum reading<t>:
    ## No reading is available.
    missing
    ## A reading with its original type.
    ## @param data The measurement.
    present(data: t)

## Read a measurement, or a supplied fallback.
## @param input The measurement to inspect.
## @param fallback Value returned when no measurement is present.
## @return The measurement or fallback.
func read<t>(input: reading<t>, fallback: t) -> t:
    match input:
        reading.present(var data):
            return data
        reading.missing:
            return fallback
```

`reading<i32>.present(7)` selects an explicit specialization;
`reading.present(7)` infers `i32` from the payload. A payload-free case needs
explicit type arguments unless its match subject supplies the concrete type.
Whole-value patterns such as `reading<i32>(var whole)` bind the entire enum.
Numeric and flags members use the same concrete spelling in patterns as in
expressions, for example `code<i32>.ok`; a matching subject can also supply the
generic arguments in `code.ok`. Unrelated specializations remain distinct, and
nongeneric enum patterns reject type arguments.
Specialization, generic functions, nested payloads, lists and dictionaries use
VOID's existing generic machinery. Type arguments do not disappear from symbol
identity when editor labels shorten their display.

`==` compares the same case and its active payloads, in declaration order.
Different cases compare unequal. Payload-free matching cases compare equal.
`!=` negates equality. Both operands evaluate once; payload comparisons
short-circuit. Numbers retain their ordinary IEEE/integer rules, strings compare
contents, managed references retain identity, and applicable custom payload
operators retain their semantics. An explicitly declared enum operator takes
precedence. If a payload lacks equality, comparison is diagnosed at the requesting
operator; the compiler does not compare padding, inactive payloads or pointers in
place of unsupported value equality. Constant comparisons support pure built-in
payload equality, including named and nullable function-value identity. This
compares function values without calling them. User function calls and custom
operator calls remain excluded from compile-time constants.

## Numeric-backed enums and conversion

```void
## Sparse protocol levels.
enum level(i8):
    low = -2
    high = 7
```

All eight fixed-width integer backing types are supported. Ordinary numeric enums
require unique, explicit decimal discriminants that fit the backing type.
`raw as? level` returns `Some(member)` only when the mathematical value fits and
is declared, otherwise `None`. `raw as level` performs the same checks and raises
a source-located runtime conversion failure on rejection. Invalid constant casts
are compile-time diagnostics.

`level.low.value` extracts the exact backing type (`i8` here).
`level.low as i32` performs checked integer conversion, as does `as? i32` with an
option result. Narrowing must fit. Distinct enums do not cast directly to each
other: extract an integer and explicitly validate it for the target enum.
Numeric enum arithmetic and implicit conversions remain unsupported.

## Flags

```void
## Permissions stored in eight bits.
flags enum permission(u8):
    none = 0
    read
    write
    read_write = read | write
    execute

## Exercise immutable and explicit mutable flag operations.
## @return Zero after selecting read and write.
func main() -> i32:
    var allowed = permission.read | permission.write
    allowed |= permission.execute
    allowed &= ~permission.execute
    if allowed.has(permission.read_write):
        return 0
    return 1
```

Omitted backing types default to `u32`. Omitted flag values receive the next
power of two above previously used bits; explicit zero consumes no bit. Explicit
flag discriminants can combine earlier members with `&`, `|`, `^`, `~` and
parentheses. Duplicate values and exhausted backing widths diagnose. A signed
backing type still constrains explicit decimal values; bitwise complement uses
its full width, including the sign bit.

`&`, `|`, `^` and `~` preserve the exact integer or flags type. Binary operands
must have the same type; ordinary numeric enums, bools and floats do not acquire
bitwise operations. `&=`, `|=` and `^=` mutate an explicitly mutable target.
G81 adds general integer shifts, but enums remain excluded as shift values and
counts. Use `.value` explicitly. No custom bitwise operator protocol is introduced.
From weaker to stronger precedence: `or`, `and`, comparisons/`is`, `|`, `^`, `&`,
integer shifts, addition, multiplication, conversions. Parenthesize masks used in comparisons
when that improves readability.

All backing-width bit patterns are valid flags values, including unnamed
combinations and unknown bits. Checked integer conversions enforce numeric
bounds, rather than filtering to declared members. A flags match needs a catchall
to cover unnamed values; listing every declared member is insufficient.

## Names, values and flag helpers

| Expression | Result |
| --- | --- |
| `value.name` | Declared member/case name; unnamed flags use decimal backing text. |
| `value.value` | Exact backing integer, for numeric/flags enums. |
| `value.is_defined()` | Whether the value is exactly a declared member/case. |
| `Enum.values()` | List of declared numeric or payload-free cases, in declaration order. |
| `bits.has(mask)` | All mask bits are present; zero returns true. |
| `bits.has_any(mask)` | Any mask bit is present; zero returns false. |
| `bits.with(mask)` | New value with mask bits set. |
| `bits.without(mask)` | New value with mask bits cleared. |
| `bits.toggled(mask)` | New value with mask bits toggled. |

Flag helpers require the same flags enum for `mask` and leave their receiver
unchanged. `values()` does not invent payloads or enumerate every bit combination.
Existing declared members/methods take precedence over convenience names.
Compiler-owned hover, completion and signature help describe these operations,
including `mask` parameter and return documentation.

## Dictionary keys and ownership

Numeric enums and flags are eligible keys. Ordinary tagged enums are eligible
when every payload satisfies the existing dictionary-key rules: integer, bool,
string, managed identity or recursively eligible value fields/enums. The tag is
part of the key, so different cases remain distinct even for zero/empty payloads.
Inactive storage is canonical zero; the existing key schema/hash machinery is
reused. Floats, collections, interfaces, custom-equality owners/payloads and the
canonical `option`/`result` types do not become keys. See [dictionaries](DICTIONARIES.md).

Enum assignment copies its value and retains existing child ownership semantics.
Dictionary assignment shares dictionary identity. Managed payloads preserve their
reference identities and GC roots. This feature does not change native ABI or
FFI authorization.
