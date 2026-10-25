# Stage 10 shared display, formatting and interpolation

## Current acceptance checkpoint — 24 October 2026

Implementation, examples and executed coverage are reconciled in
[final acceptance](ACCEPTANCE.md). Step 43 is complete; Step 44 review and required
native evidence remain open. Stage 10 is not locked. Earlier dated research and
progress statements below retain their historical meaning.

## Boss authorization — 21 October 2026

Boss approved implementation of the entire 44-step Stage 10 mission, B1–B4,
and the Wrench amendments. This supersedes the proposal-only authorization
statements below; it does not claim implementation or acceptance.

- One heterogeneous print and shared display/format kernel; exact numeric
  ties-to-even rounding, checked allocation, no arbitrary width/precision cap.
- Declaring-owner field privacy and deep const paths; direct own-initializer
  replacement grant only; existing method/property privacy remains unchanged.
- Owner-contained exact operators include binary `%` and derived `%=` in addition
  to the other arithmetic, unary, equality and ordering families. Floating builtin
  remainder remains prohibited. Independently declared `!=` remains prohibited.
- Approved verified pinned Ryu/date and bundled versioned IANA rules; recursive
  custom-equality dictionary-key exclusion remains required.
- A standalone CR completes a line immediately. Pending CRLF continuation belongs
  to logical line reads; mixed raw reads retain byte ownership.
- Cursor queries require an accessible owned response channel. Redirected stdin,
  unavailable ownership or responses return a recoverable error; no hidden tty.
- Preserve Stage 9 Windows corrections, LLVM 19.1.7, ownership and GC. No Git
  mutation, hosted CI dispatch, publication or root README edit is authorized.


**Approved for implementation; Stage 10 acceptance remains open.** The public behavior below is the implemented contract;
executed coverage and exclusions are recorded in final acceptance. [Main roadmap](../STAGE10_DESIGN.md).

## One implementation and three uses

Use one type-directed display/format operation with one owned UTF-8 builder. Its
consumers are console record construction, interpolated strings and ordinary
conversion. All ten numeric types, bool, strings, nominal values, managed values,
collections, enums/results and temporal values participate. No conversion is an
implicit numeric/string coercion; existing `string + string` remains unchanged.

| Source operation | Contract |
|---|---|
| `string(value)` | Default display; string in, identical text out; checked allocation/hook failure |
| `format(value, spec: string)` | `result<string,string>`; recoverable invalid spec/custom rejection |
| `"Score: {score:000}"` | Literal spec type-checked during analysis; checked Runtime failure only for dynamic hook/resource failure |
| `print("Score:", score)` | Same default display, then console record write |

`format` uses existing `result<T,string>` conventions rather than introducing a
format exception hierarchy. Invalid dynamic specs return `Err` with the token
offset and reason. Allocation/representability failures retain RuntimeFailure.
For a custom pattern, compile-time checking can validate structure and hook
availability, but **cannot execute arbitrary source hooks or prove their domain**;
a hook rejection becomes a source-located checked failure in interpolation.

Compiler-recognized prelude names have ordinary owned analysis/documentation
records. Locally declared/imported names follow the existing name-collision rules;
there is no new runtime scope lookup. Member `string()` is distinct from the
prelude conversion call. `string.from_utf8` and immutable string behavior remain.

## Default display table

| Type | Text |
|---|---|
| string | Exact content, no quotes or normalization |
| bool | `true`, `false` |
| signed/unsigned integer | Base-ten magnitude with leading minus only for negative signed values |
| f32/f64 | Shortest decimal that round-trips to the same declared binary type; invariant decimal point; `-0`, `nan`, `inf`, `-inf` preserved/canonicalized as specified |
| nullable reference / null | `null` or the non-null value's display, preserving readonly authority |
| value/managed struct without hook | `<qualified_type_name>`; no field traversal, addresses, object IDs or private data |
| enum | `qualified_type.case` plus displayed payloads in parentheses, in field order |
| result | `Ok(value)` / `Err(error)` using the contained value's display |
| fixed/growable array | `[a, b]` in element order |
| dictionary | `{key: value}` in existing insertion order; no sorting or custom key comparison |
| callable / native resource / task | Stable opaque `<callable>`, `<resource>`, `<task>`; no payload dereference or native/code address |
| interface view | Opaque `<interface_name>`; no unapproved reflection/dynamic concrete hook discovery |
| construction view / prepared receiver / no-result expression | Not displayable; ordinary construction/escape checks diagnose |
| temporal value | Explicit ISO-style defaults defined below |

For shortest floating display, canonicalize the kernel's digits: plain decimal
when decimal exponent is from -4 through 15 inclusive, scientific otherwise;
lowercase `e`, an explicit exponent sign and at least two exponent digits. The
threshold is a spelling policy, not a change to the shortest significant digits.
Finite integral floats may render `1`; their static type is already known. NaN
payload/sign is not a serialization promise. No locale or environment variable
changes output. Default display is human output, not a general serialization ABI.

Nested collection strings remain raw text, so output need not be parseable as a
source literal. This avoids inventing a second debug/serialization grammar.
Use an active-path identity set for cyclic collections and output `<cycle>` at a
cycle edge; shared acyclic references can appear repeatedly. Traverse with a work
stack rather than native recursion proportional to collection depth. Snapshot
collection iteration under existing version/copy rules; an intervening structural
mutation diagnoses rather than reading invalidated storage. Hook recursion is
handled separately below. Resource/size errors never silently truncate output.

## Numeric format grammar

All grammar is invariant ASCII inside a UTF-8 string. Public formatting is
type-aware: an absent spec selects the default. A spec consists of an optional
alignment followed by an optional type pattern:

```text
spec        := alignment | pattern | alignment "|" pattern
alignment   := ("<" | ">" | "^") decimal_width
pattern     := numeric_pattern | numeric_alias | temporal_pattern | custom_pattern
```

Width is a minimum number of Unicode **scalars**, never bytes or terminal cells;
ASCII spaces pad and text is never truncated. `<` left, `>` right, `^` centered
(odd extra space goes on the right). Example `"{score:>8|000}"`. Existing Bible
`<12`/`>12` forms work alone. `|` is reserved only after a valid alignment prefix;
quote it inside a numeric/temporal literal. Dynamic custom format hooks receive
the pattern after this universal alignment envelope. The shared layer pads the
hook's resulting text.

| Pattern element | Exact behavior |
|---|---|
| `0` integer placeholders | Minimum integer digits, leading zero pad; never truncate additional digits |
| `#` integer placeholders | Optional digits; zero still emits one `0`, preventing empty numeric output |
| `.` followed by `0*#*` | Required then optional fractional digits; at least one placeholder after point |
| `0.00` | Exactly two fractional digits, rounded once |
| `0.00#` | Two to three fractional digits; round to three, strip only optional trailing zeros |
| `,` between integer groups | Require groups of three placeholders after the first; group actual digits by three |
| leading `+` | Always show sign; negative zero retains minus |
| leading `-` or no sign marker | Show only a negative sign; do not emit a leading blank for positives |
| trailing `%` | Exact decimal scale by 100 before rounding, append `%` |
| `e+00` / `E+00` suffix | Scientific exponent, always signed, at least two digits; mantissa has one integer placeholder |
| single-quoted literal | Output quoted content; doubled single quote encodes one quote |

Signs precede leading literal text unless that sign is explicitly included in the
literal; e.g. a negative value with `'USD '0.00` displays `-USD 1.50`. The numeric
skeleton contains one contiguous integer/fraction portion; quoted literals may
precede/follow it. Percent precedes any trailing literal. Scientific notation and
percent are not combined in one spec. Unsupported combinations diagnose instead
of being interpreted heuristically.

Grouping example: `#,##0.00` gives `1,250.50` for 1250.5. A trailing comma, repeated
comma, irregular group size, or decimal scaling comma is invalid. Semicolon
positive/negative/zero sections and arbitrary unquoted text are invalid. These
rules avoid .NET's overloaded comma/scaling and section semantics while retaining
familiar digit patterns. .NET is a comparison source, not VOID's rounding or
locale authority. [Custom numeric formats](https://learn.microsoft.com/en-us/dotnet/standard/base-types/custom-numeric-format-strings).

| Bible alias / additional base form | Expansion / rule |
|---|---|
| `F` / `F<n>` | Fixed, default 2 fraction digits |
| `D` / `D<n>` | Integers only; minimum decimal digits, default 1 |
| `N` / `N<n>` | Grouped fixed, default 2 fraction digits |
| `P` / `P<n>` | Percent, default 2 fraction digits |
| `X` / `x` plus optional `<n>` | Integers only, upper/lower hexadecimal, minimum digit count |
| `B` / `b` plus optional `<n>` | Integers only, binary, minimum digit count |
| `E` / `e` plus optional `<n>` | Scientific, default 6 fraction digits, exponent at least 2 digits |

Aliases are conveniences, never mandatory conversion codes. Hex/binary have no
automatic `0x`/`0b` prefix. Signed negatives use the declared width's two's-complement
bits; default width for a negative i8 hex value is two digits, i64 sixteen. Positive
values use minimal digits unless padded. Width beyond the declared negative bit
width zero-pads the already rendered bit pattern; it does not pretend to sign-extend
to a different VOID type. Reject floating hex/binary/D formats. Numeric formats
on bool/string/collections reject; universal alignment still works on them.

## Precision, rounding and resources

Round finite numbers to nearest, ties to even, for **all** numeric patterns.
Use the exact represented integer or IEEE binary value, not its default display
string. Perform percent scaling in decimal/exact integer space and round once at
the requested final precision; never calculate `float_value * 100` in the source
float width and then round that intermediate value. Round before grouping/padding.
Carry can change integer width or scientific exponent. Preserve a negative sign
when a negative value rounds to zero. NaN/infinities ignore digit precision and
grouping, retain canonical tokens, and still accept alignment; percent appends `%`.

Examples: at zero fractional digits 2.5 becomes 2 and 3.5 becomes 4. Binary f64
2.675 is slightly below the decimal midpoint, so `0.00` yields 2.67. A future decimal
type would require its own exact input model; this stage does not invent one.
Modern .NET standard-format rounding and older/custom-format policies differ,
which is why VOID needs one explicit rule.
[Standard numeric formats](https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-numeric-format-strings).

Use the same small LLVM-independent numeric implementation in compiler constant
formatting and Runtime formatting. Recommended dependency: Ryu v2.0, commit
`72e18e8d14d4392a4a3261f37cde248e4fbedbec`, shortest f32/f64 plus fixed/scientific
conversion; retain the chosen upstream license and tests. Verify the pinned
`d2fixed.c` rounding branches, not just README claims. Ryu's fixed f64 formatter
can format f32 after exact widening; shortest f32 must use the f32 algorithm.
[Pinned Ryu](https://github.com/ulfjack/ryu/tree/72e18e8d14d4392a4a3261f37cde248e4fbedbec).

Ryu alone does not implement VOID's grammar or exact percent scaling. The adapter
must expose decimal digits/exponent and retain guard/sticky/tie information.
For exact percent output, shift the decimal point in the exact binary-to-decimal
expansion before applying final rounding; do not multiply and round a shortest
string. A finite f64 needs at most 1074 fractional decimal places for its exact
binary fraction, so a bounded-by-type scratch representation suffices; requested
additional zeros can be appended directly. This is a decimal digit helper, not a
new arbitrary-precision source number type. Signed-minimum magnitudes use unsigned
arithmetic. Compiler APFloat remains useful for existing numeric analysis but is
not a second formatting engine or a Runtime LLVM dependency.

There is no new arbitrary 64 KiB/precision/width language ceiling. Parse counts
with checked arithmetic; reject values unrepresentable in the target's string
extent or size_t before allocation. Respect current 16 MiB decoded constant/result
and 64 MiB unique constant-pool limits for compile-time strings. Dynamic formatting
uses actual string/allocator bounds and checked output lengths. Large requested
padding must not overflow, loop before checking size, or allocate multiple
redundant copies. Literal precision analysis is linear in the actual spec length;
enormous but representable output can still fail allocation through RuntimeFailure.

## Working temporal values, not format-only placeholders

Expose ordinary validated source value types in `use time`. Private readonly
components prevent invalid direct field construction; recoverable factories use
existing results. These types are dependencies of Stage 10 completion, not a later
stdlib promise.

| Type | Representation and required creation/conversion API |
|---|---|
| `date` | year/month/day; `make_date(y,m,d) -> result<date,string>`; proleptic Gregorian years 1..9999 |
| `time_of_day` | hour/minute/second/nanosecond; `make_time(h,m,s,ns)`; seconds 0..59, ns 0..999999999 |
| `local_datetime` | valid date + time; `make_local_datetime(date,time)`; no implied zone |
| `instant` | Unix seconds i64 + normalized ns u32; `make_instant(seconds,ns)`; UTC/POSIX timeline |
| `offset_datetime` | local datetime + signed UTC offset in seconds, absolute offset <24h; `at_offset(instant,offset_seconds)` |
| `duration` | signed seconds i64 + normalized fractional ns; `make_duration(seconds,ns)`, checked add/subtract and instant difference |
| `zone` | immutable IANA identifier/rules-version handle; `find_zone(name) -> result<zone,string>` |
| `zoned_datetime` | instant plus zone value; `in_zone(instant,zone)`; local components and exact historical offset computed from its rules |

Factories other than `find_zone` return the appropriate `result<T,string>` when
validation or representability can fail. A normalized seconds/ns pair uses floor
seconds and ns >= 0: -0.5 seconds is (-1,500000000). Canonical duration formatting
must compute its sign/magnitude without negating i64 minimum. Constructors and
factories never normalize invalid February dates or out-of-range components
silently. Private fields require an explicit init path that prevents a public
default from manufacturing an invalid date; source checker rejects omitted
required validated initialization, using the existing initializer presence rules.

Keep seconds and fractional nanoseconds separate across the native boundary.
Do not squeeze every instant/calendar value into an i64 count of nanoseconds:
that would impose a roughly 292-year range and contradict the calendar contract.
Use checked seconds/day arithmetic for zone/calendar projection, then format the
fraction separately. Public validation is also applied to clock/native conversion
results; source-private fields alone do not validate an OS or dependency result.

Required useful APIs: `now_utc() -> result<instant,string>`, `monotonic_now()` as an
opaque comparable tick value, checked `elapsed(start,end) -> result<duration,string>`,
instant ± duration, instant difference, date/component accessors and explicit
conversion of local datetime using a zone. Date/time assembly from components is
available without reading a clock. No calendar-month-as-fixed-duration arithmetic.
No implicit local zone; no leap-second `:60`; POSIX seconds do not count leap
seconds. Monotonic values cannot be formatted as calendar dates or compared across
different Runtime clock contexts. Clock precision may be coarser than 1 ns and
must be reported rather than fabricating precision.

`resolve_local(local, zone, choice = reject)` returns `result<zoned_datetime,string>`.
An overlap rejects unless the caller selects `earlier` or `later`. A gap always
returns an error; no invisible shift-forward. Explicit offset conversion cannot
invent a geographic zone. Converting an instant outside the supported calendar
range returns an error rather than wrapping. `instant` itself retains the full
seconds representation even when calendar projection is unavailable.

Use pinned Howard Hinnant date v3.0.3, commit
`5bdb7e6f31fac909c090a46dbd9fea27b6e609a4`, only for Gregorian/zone operations needed
here. Use its manually supplied, bundled IANA source data; disable remote download
and auto-install, use no libcurl, and do not depend on whichever OS zone database
happens to be installed. Keep the database immutable for a Runtime context.
IANA rules change. Its official release page reports **2026e**, released
29 September 2026; recommend that concrete data baseline for initial engineering,
record the downloaded archive digest and ship the selected version in install/
native package artifacts. Replacing it later is an explicit dependency update,
not an automatic startup network request. Rules are data, not executable/JIT pointers.
[Pinned date library](https://github.com/HowardHinnant/date/tree/5bdb7e6f31fac909c090a46dbd9fea27b6e609a4),
[zone API](https://howardhinnant.github.io/date/tz.html),
[IANA data](https://data.iana.org/time-zones/tz-link.html).

The selected data release is verified from [IANA's 2026e release](https://www.iana.org/time-zones/releases/2026e);
compatibility with the pinned parser must still be established by loading the
complete data and testing its transitions during engineering.

Resolve data relative to the existing compiler/Runtime installation, not the
project cwd or VFS. Native executables need matching data alongside their install
or a build-selected embedded data artifact. **Recommend embedding the selected
read-only rules payload in the Runtime package/native link**, with one parser
adapter for bytes, so `voidc build` continues to deliver an independently runnable
executable without new sidecar discovery or downloads. The date library's stock
manual path loader is not an in-memory loader: an adaptation of its input-source
boundary is required and must be reviewed/licensed and tested explicitly. Do not
claim setting MANUAL_TZ_DB alone accomplishes embedding. If that adaptation is
larger than a small reader-source change, return the concrete diff/dependency cost
to Boss before choosing a sidecar distribution policy. Zone completeness is not
waived by this dependency risk. Use UTC/fixed-offset functions without loading
zone data; missing/corrupt zone payload returns an error, not guessed rules.

## Temporal patterns and defaults

Parse longest valid token runs, case-sensitive. Unknown alphabetic tokens reject;
single quotes delimit literal text and doubled quotes escape a quote. Punctuation
`- / : . ,` and spaces is literal; `%` has no temporal conversion meaning.
Default names are invariant English, independent of OS locale.

| Tokens | Meaning / applicability |
|---|---|
| `yyyy`, `yy` | Four-digit year / final two digits; date-bearing values |
| `M`, `MM`, `MMM`, `MMMM` | Month number / padded / short / full English name |
| `d`, `dd`, `ddd`, `dddd` | Day of month / padded / short / full weekday |
| `H`, `HH` | 0–23 hour / padded |
| `h`, `hh`, `tt` | 1–12 hour / padded; AM/PM marker; 12-hour patterns require `tt` |
| `m`, `mm`, `s`, `ss` | Minute and second number / padded |
| `f` through nine `f`s | Required leading fractional-second digits; truncate lower ns digits, never round across a date boundary |
| `F` through nine `F`s | Same precision, trim trailing zero digits; literal decimal point remains literal |
| `zzz` | Offset ±HH:MM; reject historical offsets with nonzero seconds rather than losing them |
| `zzzz` | Exact offset ±HH:MM:SS; useful for historical zone rules |
| `VV` | IANA identifier; only zoned values |

Example `"{value:yyyy-MM-dd HH:mm:ss}"` requires a date-and-time-bearing value.
`"{date:MMMM dd, yyyy}"` works with `date`. Date-only values reject time tokens;
time-only values reject dates; local values reject offset/zone tokens. An instant
formats calendar components in UTC; its offset is zero. A zoned value uses its
selected rules snapshot, never machine local time. .NET's understandable patterns
are a useful precedent, but VOID limits them to explicit working value types.
[Custom date/time formats](https://learn.microsoft.com/en-us/dotnet/standard/base-types/custom-date-and-time-format-strings).

Duration patterns are a separate typed grammar: `d`/`dd` total days, `HH` total
hours, `hh` hours modulo 24, `mm` minutes modulo 60, `ss` seconds modulo 60, and
`f/F` fractional seconds as above. One automatic leading minus applies to the
entire magnitude. `HH` cannot occur with `d/dd`; no months, years, AM/PM or zone
tokens. Example `dd:hh:mm:ss.fff` and `HH:mm:ss` are unambiguous. Precision drops
truncate duration magnitude, preserving negative sign, not calendar rounding.

Defaults: date `yyyy-MM-dd`; time `HH:mm:ss` plus a decimal fraction only when
nonzero (up to 9 digits); local datetime combines those with `T`; instant adds `Z`;
offset datetime adds exact ±HH:MM[:SS]; zoned datetime adds `[IANA/name]` after the
offset. Duration default is `[-]days:hh:mm:ss` plus a nonzero fraction. These
defaults are specified human representations; a general parsing API is separate.

## User display and custom formats

Reuse ordinary owner receiver methods:

```python
func (p point) string() -> string:
    return "({p.x}, {p.y})"

func (p point) format(spec: string) -> result<string, string>:
    if spec == "short":
        return Ok("{p.x},{p.y}")
    return Err("unknown point format")
```

An accessible exact `string() -> string` hook overrides opaque nominal default
display. An exact `format(spec: string) -> result<string,string>` handles nonempty
patterns; absent hook means nonempty custom patterns reject. Empty spec always
means default display, not a surprise invocation of format. A type can define
either or both; custom format without string keeps the opaque default.

Hooks must be synchronous and receiver-nonmutating under existing conservative
effects, including referenced children. No implicit thaw of const fields or
managed arguments. A hook may allocate, perform unrelated I/O or fail as an ordinary
function; no new purity claim is inferred from its name. Owner/import/visibility
selection is ordinary shared member selection; ambiguous promoted hooks diagnose.
Virtual overrides use current guarded dispatch; generic hooks check at concrete
specialization. No hook is an interface requirement merely because of its name.

Track active custom hook invocations by hook identity plus managed object identity.
Detect re-entry of the same managed object/hook. Value hooks have no stable object
identity: use a proposed 64-active-hook nesting resource guard per conversion chain,
with an explicit checked recursion-limit failure, rather than treating every
same-type nested value as a cycle. This is a new implementation guard included in
B1 review, not an assertion that the Runtime already has a call-depth guard.
Built-in recursive containers use their separate active identity path/work stack.
A hook formatting unrelated nested values below the guard remains legal. Tracking
is per conversion chain, inherited by nested conversion calls and cleaned on every
failure; no permanent managed-object cache or code pointer in Runtime type metadata.

Rust's Display and Python's `__format__` demonstrate explicit owner customization;
VOID reuses its existing receiver methods instead of adding a trait hierarchy or
dynamic object protocol.
[Rust Display](https://doc.rust-lang.org/std/fmt/trait.Display.html),
[Python data model](https://docs.python.org/3/reference/datamodel.html#object.__format__).

## Complete interpolation grammar and source identity

Keep automatic ordinary strings, `{{`/`}}`, existing escapes and literal Unicode
escaped braces. Hole syntax is `{expression}` or `{expression:spec}`. Delimiters
inside nested strings, parentheses, calls, array/dict literals, generics and
ordinary expression bodies are not hole terminators or format separators.
Use the existing expression parser on original source offsets; only a colon left
after a complete expression introduces the spec. Require parentheses around an
expression whose unparenthesized top-level colon would be ambiguous with a spec
(e.g. an if/match expression). Diagnose with a parenthesized example, not a guessed
split. Nested quoted literals and quoted format literals must be tokenized in
their respective contexts. No decoded-string reparsing or synthetic file identity.

Permit all ordinary expression forms, including calls, indexes, match/if,
anonymous functions when their result is displayable, and multiline expressions
inside supported delimiters. Add `"""..."""` for multiline literal text, using the
same interpolation/escape rules. Literal multiline text preserves physical UTF-8
bytes/line endings; no implicit indentation trimming or newline normalization.
Three quotes terminate only outside holes; use `\"` to escape a quote when needed.
Ordinary quoted literal **text** still cannot contain raw line breaks, but a hole
may contain a properly delimited multiline expression. Comments inside a multiline
hole end at the source line, never consume a closing brace on a later line. A
comment that hides the only closing brace produces an unterminated-hole diagnostic.
Indentation inside actual if/match/function bodies retains ordinary meaning;
continuation indentation does not create a second indentation policy.

Within specs, quote literal braces; no nested replacement-field format language
is added. Dynamic widths/patterns use ordinary expressions calling `format`.
Literal spec validation never evaluates source hooks. Empty holes, unbalanced
braces, malformed quoting, invalid patterns and wrong static types diagnose at
the original hole/spec span. Preserve the existing lexer nesting guard and constant
limits as resource diagnostics, without expanding them into new format ceilings.

Evaluate a hole's expression once, then its conversion once, before the next hole.
Append literal chunks in order. On failure, later holes are not evaluated and no
partial result string is published. Earlier user effects remain. This differs
from print's ordinary call argument-capture phase and is explicit. Root intermediate
managed results through polls and custom calls; value operands retain ordinary
copies, managed operands retain identity. Unfinished self cannot be displayed as
a whole, boxed or passed into a hook as an identity. Existing pure constant rules
permit built-in constant formatting but not arbitrary source hook evaluation.

## Compiler, Runtime and editor integration

Extend existing string-token/typed-AST representation with owned hole spans and
format plans. Lower built-in rendering through one small private same-build ABI;
custom hooks lower as ordinary calls. Append through one checked builder to avoid
repeated full-string concatenation. Publish the final immutable string only after
success using the current GC string allocation/roots. No format plan contains
borrowed AST pointers, GC addresses or executable callbacks.

Share syntax/format validation and numeric kernel code with constant evaluation;
Runtime remains LLVM-free. Runtime ABI failures are noexcept wrappers into the
existing failure channel. Normal/O2 JIT and object emission use the shared LLVM
lowerer and explicit Runtime symbol binding. Include formatter ABI/dependency/
rules versions in existing cache identity. Include custom hook references/effects,
spec tokens and constant syntax in exact compatibility and invalidation.

Composite display is statically lowered for its concrete source type through
ordinary field-independent container iteration and typed element conversion calls.
It does not require a dynamic boxed value, Runtime reflection, or source function
pointer stored in a collection/type descriptor. Default nominal values remain
opaque. A private per-call builder/context token can carry active-path and hook
depth state through generated helper calls; it is not source-storable or retained
by Runtime metadata. This keeps the “one formatting implementation” rule compatible
with data-only GC/dictionary descriptors and ordinary generation leases.

Expose hole expression references, declared hook identities, spec spans and
type-appropriate pattern documentation through compiler-owned analysis. `voidls`
reuses that for completion, hover, signatures and navigation, including invalid
unsaved holes through the existing recovery boundary. Completion may suggest
patterns and real time-zone names from installed metadata; never run hooks,
read clocks or perform I/O to answer LSP requests. Formatting preserves literal
bytes, escape spellings and spec strings. UTF-16 conversion uses the same original
immutable snapshot, including supplementary characters before/inside holes.

## Step 37 dependency compatibility — 22 October 2026

Shared format policy `void-format-v2` names ties-to-even rounding, Unicode scalar
alignment and the full pinned Ryu revision. Temporal policy `void-temporal-v1`
names date v3.0.3/full revision, bundled IANA 2026e and explicit overlap/gap handling.
Both enter source compatibility and the existing native Debug cache key, in addition
to actual archive build digests and complete immutable source snapshots.

Written format specification expressions have owned, trivia-free syntax contracts,
including interpolation, reordered named prelude arguments, direct format hooks,
concatenation and nested expression/pattern dependencies. Module/local constant
syntax keeps the existing exact constant contract. Selected adapters, hooks and
ordinary helpers retain ordinary reference edges and inferred effects. A relevant
specification/selection/effect change requires restart; compatible helper/hook body
edits use the established generation lease. Dynamic specification-producing helper
bodies remain ordinary code; they are not evaluated during dependency analysis.
No executable pointers enter formatting/zone/GC descriptors and no additional
formatting engine/cache is introduced. Final focused evidence is in the Step 37
engineering record; this does not complete composite display or Stage 10 acceptance.


## Step 38 editor integration — 22 October 2026

Compiler EditorProbe offers numeric, temporal and alignment examples in interpolation
format tokens and ordinary format literal arguments. The production validators and
actual operand type filter candidates; no hook executes to answer a query. Selected
custom conversions navigate/reference the ordinary hook already selected in the
compiler adapter. Layout-only specifications navigate to string; dynamic specs
retain both conditional hooks. A format replacement requires an actual colon in
an interpolation hole. Explicit hook calls retain ordinary docs and parameter assistance.
Original expression spans support hover/signature/completion/navigation inside holes,
including multiline/CRLF/Unicode and escaped braces. Installed temporal sources retain
read-only identities. Future console/composite display tooling remains dependent on
those implementations. Focused evidence: engineering/step38-results.json.


## Step 39 conservative source formatting — 22 October 2026

The compiler-owned formatter first parses with the existing frontend. It rewrites
only proven inter-token space gaps and physical code indentation. Comments, blank
lines, literal spelling/decoded bytes, format specifications and LF/CRLF bytes
remain unchanged. Both physical and interpolation-expanded token streams must
retain kind, spelling and decoded contents, including indentation tokens. A changed
opening-line indentation can affect a multiline hole body even when the entire
string token is byte-identical; such a proposed edit is refused. Malformed syntax
also produces no edit. No interpolation mini-parser or formatting grammar is added.

The LSP additionally requires current successful project analysis, reanalyzes the
proposed overlay, compares exact compatibility, verifies idempotence and rechecks
revision/disk state. Semantic diagnostics therefore prevent editor formatting.
The standalone formatting API can normalize syntactically valid erroneous source
while retaining the same errors in their own original-source snapshots. Literal
content and expressions inside strings are never reformatted. Existing native
clients retain their version/changedtick guards. Focused acceptance and limits:
engineering/step39-results.json. Future console/composite integration is still open.

## Step 27 print consumer integration

The source heterogeneous print consumer now captures every supplied expression and
control before rendering through ordinary scalar display or selected nominal hooks.
It uses the existing checked join/builder kernel and writes only a completed record.
Approved borrowed console tokens have the same opaque <resource> display policy as
other native resources, with no payload traversal. Strings retain exact UTF-8/NUL
bytes. Hook failures preserve their reason and run reached cleanup; no outer record
prefix is emitted. Collections/enums/results remain a separate missing composite
display obligation. Exact normal/O2 JIT, linked AOT and strict evidence is in
engineering/step27-results.json; final four-system integration is still open.


## Collection increment implementation — 23 October 2026

Fixed arrays, growable lists and dictionaries now lower statically through the same
conversion dispatcher used by string, format, interpolation and print. A generated
ordinary function drives managed continuation tags and type-specific owner/cursor/
version stacks; dictionary key/value snapshots use ordinary typed storage. Every
container edge is processed iteratively, without native calls proportional to
collection depth. Existing collection reads/version checks and readonly/value-copy
rules remain authoritative; private fields of opaque nominal values are never
traversed. Eligible key admission and data-only dictionary equality/hash are unchanged.

A compiler-private per-call Runtime context holds the existing checked UTF8 Builder
and active identity path. Generated typed roots retain owners; the context stores
no executable metadata, boxed values or managed ownership. Nested conversions see
active ancestors, cycle edges display <cycle>, and shared acyclic values repeat.
Scope cleanup restores the path and builder on normal/failure/cancellation exits
using the ordinary checked LLVM/Runtime call path. No nesting/output limit is added.

Universal alignment remains scalar-based; collection patterns are otherwise empty.
Invalid literal collection specs diagnose; dynamic unsupported specs return Err.
The immutable result is published only after complete construction; print still
assembles all conversions before admitting its outer native output record. Hooks
retain existing effects, visibility, readonly authority and recursion guards.

The formatter policy is void-format-v3;collections-v1, with the same pinned Ryu,
date and bundled IANA versions/licenses. No dependency was added. Focused acceptance
is recorded in engineering/collections-results.json. Enum/result display and their
collection element forms remain the next composite increment. Native Windows/macOS,
collection/editor integration and complete four-system/final acceptance remain open.

## Enum/result increment implementation — 23 October 2026

Enum defaults use the canonical qualified declaring type followed by `.case`.
Payload-bearing cases append `(first, second)` in declared order; empty cases
have no parentheses. Results use `Ok(value)` or `Err(error)`, preserving Ok(null).
Nested strings retain exact raw Unicode/NUL contents. These defaults are shared
by string, format, interpolation and print, including container and sum nesting.

The existing generated typed work stack now includes sum continuations and typed
owner/cursor storage. It checks the tag before selecting a case, reads only active
payloads through existing checked sum_get IR, and uses the existing scoped builder,
active collection path and ordinary leaf hooks. Enum/list cycles remain iterative;
shared acyclic references repeat. Value copies and managed identities keep their
existing semantics. No enum/result Runtime renderer, reflection, boxing, executable
metadata callback, new IR/ABI operation or dependency was introduced.

Universal alignment counts Unicode scalars. Other patterns are rejected by the
same literal-analysis/dynamic-Err rules as collections. Failed display publishes
no immutable result or outer print prefix. Existing source cleanup/frames, GC roots,
worker effects, code leases, dictionary admission and pattern semantics remain.
Concrete generic result/payload/owner specializations use existing syntax; this
increment does not introduce a new generic-enum declaration syntax.

Policy identity is void-format-v4;collections-v1;sum-display-v1. Ordinary hook edges,
existing variant/layout contracts and exact compiler/Runtime/dependency identities
cover reload and native caching. Focused final evidence is tracked in
engineering/sum-display-results.json: focused Linux Debug/strict acceptance passes.
Native-platform/editor/four-system/final-stage acceptance remains separate.

## Composite tooling integration — 23 October 2026

Existing EditorProbe/declaration/reference records cover enum case menus, declared
payload names/docs, constructor parameter help and checked pattern source targets.
Concrete result constructors and patterns expose compiler-owned metadata with
separate readonly-capability identities and contextual Ok/Err menus; builtins have
no invented filesystem definitions. Fixed/list/dictionary/result/enum consumers
retain shared layout-only format suggestions and production invalid-spec validators.
No display evaluation, renderer, reflection or editor semantic resolver is added.
Unicode/CRLF/multiline/nested holes retain original compiler spans. Focused evidence
and remaining platform/final acceptance are in console-tooling-results.json.
