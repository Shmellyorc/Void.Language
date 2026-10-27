# JSON contract

27 October 2026. Baseline `194603ea00eb38aff72b7d6e950865b3a23df92a`.
**Historical research-stage status:** proposal for Golden Wrench/Boss review.
The subsequent Boss authorization and current implementation are recorded below.
[Feasibility and evidence](RESEARCH.md); [complete engineering mission](ROADMAP.md).

Implementation authorization, 27 October 2026: Boss approved all three decision
bundles and this contract. Boss subsequently selected explicit `use json as json`
for examples with a loop binder named `value`, preserving the existing no-shadowing
rule. Earlier proposal/approval wording below is historical. See
[the active engineering record](IMPLEMENTATION.md).

## 1. Public surface

One `use json` module. All declarations use existing VOID grammar. Names below
are recommendations, not an approved overload or reflection feature.

```void
func parse(text: string, budget: limits = limits()) -> result<value, error>
func parse_as<t>(text: string, budget: limits = limits(), unknown: unknown_fields = unknown_fields.reject) -> result<t, error>
func from_value<t>(const node: value, budget: limits = limits(), unknown: unknown_fields = unknown_fields.reject) -> result<t, error>
func to_value<t>(const input: t, budget: limits = limits()) -> result<value, error>
func stringify<t>(const input: t, budget: limits = limits()) -> result<string, error>
func null_value() -> value
```

`parse` returns a dynamic value, `parse_as<t>` parses then uses the same typed
conversion as `from_value<t>`, and `stringify` uses the same tree writer for typed
and dynamic inputs. Type inference works for inputs to `to_value`/`stringify`;
return-only `t` remains explicit. No `parse<t>`/`parse` overload pair, new syntax,
global default generic parameter, implicit expected-return inference or Random-like
call adapter is needed. Imported aliases and function references remain ordinary.

`from_value`/`to_value` are useful conversion boundaries, not second parsers.
They let applications map validated private types through plain DTOs without
reparsing text. `null_value()` constructs the genuine null node; it is distinct
from a nullable VOID reference. No `try_get` family is added.

Example, within ordinary result-returning application functions:

```void
use json

struct player_data:
    name: string
    score: i64

func load(text: string) -> result<player_data, json.error>:
    return Ok(json.parse_as<player_data>(text)?)

func read_name(text: string) -> result<string, json.error>:
    var root = json.parse(text)?
    var player = root.get("player")?
    var name = player.get("name")?
    return Ok(name.as_string()?)
```

Examples in VOID string
literals must double literal braces, e.g. `"{{\"score\":7}}"`. JSON input itself
does not double braces. No source-language string/escape rule changes.

### Dynamic observation

`value` is a managed type with private construction/payload. Public `kind` is a
readonly property returning the payload-free enum `value_kind` with cases
`null_value`, `boolean`, `number`, `text`, `array`, `object`.

| Declaration on `value` | Result and exact meaning |
|---|---|
| `get(name: string) -> result<value,error>` | Object only. Decoded case-sensitive exact key; absent yields `missing_property`, present null yields `Ok(null-node)` |
| `at(index: i64) -> result<value,error>` | Array only. VOID-style negative indexing, normalized safely; invalid index yields `index_out_of_range` |
| `as_string() -> result<string,error>` | String only, including empty/NUL-containing strings |
| `as_bool() -> result<bool,error>` | Boolean only; no truthiness |
| `as_number<t>() -> result<t,error>` | Number only; exactly one of the ten numeric primitives; conversion policy in section 4 |
| `number_text() -> result<string,error>` | Number only, exact retained validated lexeme |
| `as_array() -> result<[value],error>` | Array only; independent outer list, shared node identities with readonly authority retained |
| `keys() -> result<[string],error>` | Object only; independent key snapshot in document/construction order |
| `length() -> result<i64,error>` | Array/object only; element/member count, never string scalar/byte count |

Wrong receiver/JSON value kind produces `wrong_type`; accessors do not reinterpret
text, booleans or null as numbers. Bad `as_number<t>` type arguments are compiler
diagnostics, not runtime errors. Node access through a const root is supported;
returned aliases may not acquire mutable authority. The reproduced source/IR
discrepancy in RESEARCH is a prerequisite to establishing this guarantee.

No public node setters, mutable payload property, implicit null indexing or
in-place patch API. Applications can edit a DTO/dictionary/list, then call
`to_value` or `stringify`. This module is a complete parse/query/convert/write
facility, not a JSON editor, schema language or patch engine.

## 2. Ordinary dynamic representation

Use an internal tagged enum with six variants, stored in one managed `value`:

| Variant | Private payload |
|---|---|
| null | None |
| boolean | bool |
| number | Validated original numeric string |
| text | Decoded immutable string |
| array | Ordinary `[value]` |
| object | Ordered `[_member]`, where `_member` is a private value struct holding string name and managed child value |

Each node also retains its original start byte offset, or -1 for programmatically
converted nodes. JSON member names never become source identifiers. A private
owner `init` initializes private fields; module-level helpers must call owner
methods rather than acquiring special private-field permission. All arrays are
private during construction. Commit the completed container once and retain no
mutable builder alias after publication. Methods expose observations only.

Managed/list edges break recursive value layout. There is no inline enum whose
payload directly contains itself, boxed arbitrary VOID value, raw native pointer,
new GC representation, lazy text view or borrowed input buffer. Strings/numeric
tokens are owned; retaining one child need not retain the complete original input.

Object member lists preserve source order and allow explicitly metered duplicate
checks. This trades average constant-time dictionary lookup for simple bounded
linear lookup and avoids the Runtime's unkeyed hash table for untrusted names.
No sorting/index framework is required. See section 7 for work charging and
conversion to ordinary typed dictionaries.

`to_value` produces a fresh independent tree, including when given a `json.value`.
This is necessary when readonly input nodes would otherwise be installed into
mutable managed backing. Immutable strings may share bytes. `from_value<value>`
also copies into a fresh tree, preserving lexemes and stored offsets. No capability
exception is granted because the public API happens to have no setters.
`stringify<value>` may observe the tree directly and avoid this clone; both paths
use the same writer and error rules.

## 3. Parser and Unicode behavior

This is a strict Unicode-scalar profile of JSON. The interoperability distinction
is documented in [RESEARCH](RESEARCH.md#correctness-research-and-policy-choices).

- Accept exactly one value of any of the six kinds. Permit surrounding ASCII
  space, tab, CR and LF. Reject empty input, comments, trailing commas, extra
  values, missing separators, unquoted keys and trailing non-whitespace.
- Accept raw valid UTF-8 scalars in quoted strings. Reject raw U+0000–U+001F;
  DEL and other valid scalars remain data. Do not use `text.trim`, whose Unicode
  whitespace set is broader than the JSON whitespace set.
- Decode only the JSON escapes `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`
  and exactly four hexadecimal digits after `\u`. A high surrogate requires an
  immediately following low-surrogate `\u` escape; combine with
  `65536 + (high - 55296) * 1024 + low - 56320`. Reject lone/reversed/bad pairs
  and truncated escapes with `invalid_unicode` or `syntax` as applicable.
- Encode decoded scalars to UTF-8 arithmetically using existing byte lists and
  strict `string.from_utf8`. NUL is data. No replacement characters, NFC/case
  folding, grapheme rules, locale or new Unicode database.
- Reject a leading BOM with `syntax`; preserve U+FEFF within a string. Permit
  Unicode noncharacters consistently with VOID strings. UTF-16/UTF-32 byte
  documents are not accepted or guessed.
- `parse` takes a valid VOID string. Invalid UTF-8 bytes must first fail the
  existing `string.from_utf8` / IO text boundary; there is no unchecked byte
  overload or duplicate UTF-8 validator. Malformed `\u` text remains a JSON
  error. Tests must exercise both boundaries and not invent an invalid string.
- Detect duplicate keys **after escape decoding**, before parsing/storing the
  duplicate value. `"a"` and `"\u0061"` collide. Reject even equal values or
  duplicate unknown fields. Distinct normalized-looking strings remain distinct.

Parsing is an iterative byte cursor plus explicit heap frames for array/object
states: expecting value/key, colon, comma or close. A closing container creates
its completed node and attaches it to its parent. A single root followed by EOF
completes the transaction. Do not recursively call the parser per input depth.

## 4. Numbers and precision

Recognize an optional minus, `0` or a nonzero decimal integer part, optional
fraction with at least one digit and optional signed exponent with at least one
digit. Reject `+1`, `01`, `.5`, `1.`, hexadecimal, separators, `nan` and infinity.
Validate this before calling `convert.parse`; its broader grammar is not the
validator. Store the validated token unchanged and bound its byte length.

| Operation | Proposed semantics |
|---|---|
| Dynamic parse/write | Preserve all accepted tokens exactly, including large integers, `1.00`, exponent spelling, `-0` and `1e400`; no eager f64 conversion |
| Integer extraction, all 8 widths | Exact mathematical integral value required. Accept `1.00e2` as 100 and `100e-2` as 1; reject `1.5`/`1e-1` as `out_of_range`. Check signed/unsigned extrema before construction |
| Negative zero to integer | 0, including unsigned targets; normalize zero before invoking unsigned conversion |
| f32/f64 extraction | Round directly to the selected precision using existing invariant conversion; no intermediate f64 for f32. Reject overflow and nonzero underflow-to-zero as `out_of_range`; accept finite representable subnormals and signed zero |
| Typed integer output | Exact decimal digits; all u64/i64 extrema remain numbers, never silently quoted or rounded |
| Typed float output | Reject NaN/±infinity as `non_finite`; use existing matching-precision shortest round-trip display, including negative zero. No locale/grouping/format specifiers |

Normalize integer digit/scale text using bounded source arithmetic: strip zero
digits, combine fractional length and exponent with saturation, verify that any
removed digits are zero, then compare significant magnitude to target limits.
Never allocate exponent-sized padding or truncate via float conversion. Nonzero
values requiring more than 20 integral digits cannot fit supported integer types;
zero remains zero even with an enormous bounded exponent. Feed the final at-most-20
digit magnitude/sign into existing exact-width conversion. The private bridge may
select existing numeric min/max/type facts; no BigInt arithmetic is required.

Conversion of all-zero significands preserves sign for floats without allowing
an enormous exponent to manufacture a range error. Rounding of finite decimals
follows the existing convert contract; verify half-neighbor/underflow behavior in
native profiles. Integer exactness is not a promise that JavaScript consumers
can represent every emitted 64-bit integer. Users requiring wire strings should
model those fields as strings explicitly.

## 5. Typed conversion contract

Admit a finite concrete type graph at specialization time. Unsupported types
produce a source diagnostic with the requesting call and offending field/type
path. Do this independently of whether runtime input takes a particular branch.
There is no dynamic type search, implicit coercion or public reflection API.

| VOID target/source | JSON mapping and boundary |
|---|---|
| bool/string | Corresponding kind only; no conversions from other kinds |
| Numeric primitives | Section 4; declared width retained |
| Payload-free enum | Unqualified exact case-name string. No ordinal numbers or qualified alias names. Enumerate actual cases at compile time; unknown names yield `invalid_enum` |
| `[t]` | Array; fresh list on decode, ordinary per-element mapping |
| `[t,n]` | Array of exactly n elements; mismatch yields `length_mismatch`. Check extent/budget before allocating; encode independently of list generic inference |
| `{string:t}` | Object; fresh ordinary insertion-ordered dictionary on decode. Other key types reject at specialization, including numeric keys |
| Plain value/managed structs | Object; map public stored fields in effective declaration order. Public const fields supplied during fresh named construction, never assigned later |
| Concrete generic data structs | Same mapping after normal specialization, without erasure |
| Managed `t?` | JSON null ↔ null, otherwise map concrete t; no absent-versus-null conflation in the DOM |
| `json.value` | Deep copy of the existing tree for value conversion; direct read is allowed for stringify |
| Data `using` composition | Serialize the actual stored component field as a nested object; do not flatten/promote members or duplicate them |
| Plain managed inheritance | Effective base fields first, then own fields, provided the complete chain meets the same public-data/no-init rules. Detect duplicate JSON field names before generating code |

**A plain data struct** has no custom init/deinit in its relevant construction chain,
no native resource/task/callable/event/interface storage and no inaccessible
stored fields. All stored fields must be supported recursively. Public methods
do not become data. Properties are not automatically invoked or serialized;
the stored data remains the contract. A property-only/private-backed type therefore
requires explicit DTO mapping and does not silently become `{}`. If the struct
has private/internal storage, reject automatic conversion for that whole type,
even when called from its owner; callers must not obtain differing wire schemas
based on lexical privileges.

Concretely, reject a property-bearing type with no public stored-data schema;
an intentionally empty struct with no properties remains an empty JSON object.
Serialization follows the **static concrete T**, including its declared base
fields. A derived object passed through a base-typed reference uses that base
schema; no runtime descendant discovery, discriminator or implicit downcast is
promised. Applications needing polymorphic wire data choose an explicit DTO.

Custom init/deinit is rejected even when it is public or parameterless. No uninitialized
allocation, setter replay, constructor argument guessing or private-field bypass.
Explicit application code validates a public DTO and calls its existing factory;
serialization explicitly extracts its DTO through accessible observations. This
also handles `time`, resources, invariants and alternative wire schemas. It is a
practical supported route, not an undocumented plugin/attribute system.

Enums with payloads, `result`, interfaces/dynamic polymorphism, callable/task/event
handles and arbitrary native resources are outside automatic data conversion.
Their attempted use diagnoses. This does not claim that they are missing VOID
features; their wire representation needs a separately approved contract. No
extension of nullable scalar/list/string or generic `t?` is part of JSON. DTOs can
represent such optional data explicitly with a nullable managed wrapper or a
`json.value`, and application code can enforce their desired interpretation.

### Object schema and failure order

Names are exact, case-sensitive and independent of import aliases. No automatic
camel-case/snake-case, renaming, omitted-zero or serialization attributes.
Nonnullable stored fields are required, including fields with VOID default
initializers. Missing nullable managed fields become null. Explicit null is legal
only for a nullable managed target or `json.value`; a present null never invokes
a default. Empty lists/maps/string and numeric zero remain ordinary values.

Default `unknown_fields.reject` reports the first unknown property in input order.
`unknown_fields.ignore` ignores schema membership only: ignored values must still
parse correctly, be unique and consume the same parse budgets. After the unknown
scan, process declared fields in effective declaration order, checking presence
and converting each. Fail on the first error; do not return a partially initialized
T. Default expressions must not execute to replace missing data.

Each nested object/list/value is fully checked before its parent is constructed.
All decoded managed nodes/collections are new and writable, independent of the
DOM. Aliasing in input JSON has no special meaning. During serialization,
reference cycles eventually yield `limit_exceeded` at the depth/node/work bound;
they cannot produce a successful finite JSON tree. This deliberately uses the
same error as an excessively deep acyclic graph. There is no promise to classify
identity cycles separately. Repeated non-ancestor references are serialized
separately and charged again. No `$id`/`$ref`, global visited-set rejection of
harmless sharing, identity preservation promise or arbitrary downcast.

This avoids inventing collection identity reflection: managed value patterns can
compare identity without user operators, but lists/maps have no public identity
comparison admitted by current IR. Reusing the display Runtime's cycle context
would couple JSON to unrelated display state. A strict finite depth limit and
iterative traversal already provide safe termination with less infrastructure.
If Boss specifically requires a separate cycle diagnostic, review that extra
private identity facility before implementation; do not silently generate `==`
calls that can invoke user operators or invalid collection comparisons.

## 6. Errors and output

`json.error extends core.error` adds `kind: error_kind` and `offset: i64`.
Inherited `message` is human-readable, `code` is a stable module-owned enum-index
mapping beginning at 1, and `path` is a JSON Pointer to the relevant location.
Root path is empty; escape `~` as `~0` and `/` as `~1`, with decimal array indexes.
This is a location convention, not a new pointer-query API.
[JSON Pointer encoding](https://www.rfc-editor.org/rfc/rfc6901#section-3)

Kinds, in code order: `syntax`, `invalid_unicode`, `duplicate_key`,
`missing_property`, `index_out_of_range`, `wrong_type`, `out_of_range`,
`non_finite`, `invalid_enum`, `unknown_property`, `length_mismatch`,
`limit_exceeded`, `invalid_limits`.

Paths are relative to the root of the operation: parse_as uses its document,
from_value uses its supplied subtree, and an accessor uses its receiver. A child
lookup therefore need not retain parent links or the original document path.
Offset is a zero-based original UTF-8 byte position: offending token/escape/key,
or byte_length for unexpected EOF. A missing property uses its object's opening
offset; unknown/duplicate errors use the relevant key's opening quote. Nodes and
member records retain the necessary start offsets. Programmatic values have -1.
Location text does not retain the full source string. A resource error reports
the current location and the first exceeded limit. LSP source positions remain
UTF-16 and independent of JSON data offsets.

Parser-first precedence: validate limits, input byte count, then scan in document
order, charging before work/allocation; key duplication is checked immediately
after decoding a key. Typed validation begins only after a complete successful
parse. During traversal, depth is charged before descending, nodes before
materialization and output bytes before append. Within a step check depth, nodes,
token/string/member limits, work, then output. For `from_value` / `to_value`, use
the corresponding traversal order. Accessors distinguish missing/type errors;
they never return a null sentinel on failure.

Actual OOM, invalid Runtime state and native failures retain existing checked
Runtime failures and source cleanup. They must not be misreported as malformed
JSON or promised recoverable `Err` if the existing infrastructure cannot do so.
Result error types stay exact; applications explicitly translate IO errors when
combining IO and JSON in one result-returning function.

Writer output is compact UTF-8: no BOM, spaces, indentation or final newline.
Keep array order, dynamic object order, dictionary insertion order and DTO
declaration order. Escape quote/backslash and U+0000–U+001F; use short escapes for
BS/FF/LF/CR/tab and lowercase `\u00xx` for the other controls. Leave slash, DEL,
non-ASCII scalars and U+2028/U+2029 unescaped. Do not embed this output into HTML
without the host application's separate escaping. Preserve dynamic number tokens;
typed numbers use section 4. Output is deterministic for the same ordered values,
not sorted/JCS canonical, and data round trips do not preserve whitespace or
original string escape spelling.

## 7. Explicit bounds and work accounting

Proposed ordinary `value struct limits` has these public i64 fields/defaults:

| Field | Default | Counted before doing the work |
|---|---:|---|
| `max_input_bytes` | 16,777,216 | Original parse string bytes, before `to_utf8` copying |
| `max_output_bytes` | 16,777,216 | Encoded JSON bytes, including escaping/structure |
| `max_depth` | 64 | Container/typed traversal nesting; scalar root 0, container root 1 |
| `max_nodes` | 262,144 | Logical JSON values and typed value occurrences materialized across a compound operation |
| `max_string_bytes` | 8,388,608 | Each decoded key/string and each converted source string |
| `max_number_bytes` | 1,024 | Each numeric lexeme, including exponent spelling |
| `max_object_members` | 4,096 | Members in each object / typed dictionary / DTO |
| `max_work` | 67,108,864 | Conservative traversal/byte/comparison/probe units below |

All limits must be positive; max_depth cannot exceed 128. Other limits must fit
existing i64/count/allocation representability and checked arithmetic. Settings
can be reduced for network input or raised explicitly within those constraints.
These are module budgets, not new language-wide collection limits. Empty values
still parse normally; callers need not set a limit to zero to admit them.

A compound `parse_as` shares one context across parsing and decoding; `stringify`
shares one across conversion and writing. Charge occurrences each time a new DOM
node or typed destination value is materialized; this can count a scalar twice
in parse_as. Aliased repeated subtrees count on each traversal. Check a fixed
extent and prospective child count before allocating/pushing. Budget comparisons
use subtract-before-add or checked arithmetic; VOID integer wrap is not overflow
protection. Byte builders check encoded expansion before growth, including six
bytes for escaped controls. No repeated whole-string concatenation loop.

Charge 1 per state transition/value/element, bytes scanned/copied/emitted, and
both operands' byte lengths plus 1 before each key equality comparison. Duplicate
detection scans the ordered member list under this budget. A public `get` scans
at most the admitted object's member count; all parse-produced objects retain
that bound. `get` has no accumulating cross-call budget or persistent mutable
state; repeated explicit application queries remain application-controlled work.

Typed dictionary construction reuses the Runtime, whose internal hash probes
cannot be observed by source JSON. A fresh dictionary has capacity 0; its first
insertion grows to 4, and later growth doubles capacity. It has twice as many
buckets as entry capacity. Maintain that known
capacity in the private fresh builder. For n existing entries, old capacity C
and maximum previous/current key length K, precharge key hashing plus
`2*C*(2*K+1)` for worst-case search/comparisons. Non-growth insertion adds `2*C`
for bucket insertion. At growth to G (4 when C is 0), instead add `2*G*(n+1)` for all reindex
probes and `2*G+n` for bucket clearing/entry copying. Empty search costs zero;
hashing includes the fixed length word. Use checked arithmetic and charge before
the Runtime call. These conservative logical units still need tests against
`dictionary.cpp:44–76`; no timing claim follows. Serialize typed dictionaries by
ordered iteration instead of hash lookup. Do not add keyed hashing or a new map
implementation as JSON work.

Logical budgets are not an RSS/allocator quota. Input copies, decoded text,
source trees, destination data, inactive sum storage and spare capacity all
contribute memory. Failed intermediates may remain rooted until the operation
returns; inspect/clear completed work slots and rely on existing GC. No claim of
universal hard memory or wall-time bounds follows from these counters. Demonstrate
bounded malicious fixtures and actual 1 MiB stack behavior before acceptance.

## 8. Compiler and execution boundary

Two canonical-private hooks in `json.void` may request to-value/from-value
specialization. They select normal generated functions from concrete compiler
types and pass ordinary source context/values. Validate exact canonical VFS
identity, shadowing/binding, supported concrete types and closed operation shape,
following existing convert/Math patterns. They are not public intrinsics or
native parser entry points.

Generated adapters discover a finite graph once, register provisional identities
before recursive edges, then emit per-type heap owner/partial-result stacks and
explicit continuations using existing AST/IR. A decoded destination object is
constructed only when all operands exist; const fields use ordinary initialization.
All accesses pass normal frontend and independent IR visibility/capability checks.
Keep stack lengths/cursors consistent and clear popped reference slots. No new
Runtime metadata tables, ABI object walkers or executable GC descriptors.

Put type admission before cached specialization use. Include concrete types,
capability profiles, direction and semantic policy in adapter identity; library
source/options and compiler version stay in the existing cache contracts. Publish
no generated field references as fake user source references. Diagnostics point
to the requesting call plus original field declaration. Reuse original source
docs and generic signature recovery for editor requests, including incomplete
`parse_as<...>(` and `as_number<...>(`.

No globals, process singleton, OS resources or borrowed buffers. Each operation
has its own source context rooted by ordinary calls. Compatible body-only reload
and generation leases retain existing behavior; field/type/instance-set changes
follow current restart rules. Existing worker transfer restrictions remain.
No JSON promise of shared DOM concurrency, new worker return types or Runtime
state survives a fresh application/JIT/AOT Runtime.

Readonly observation is not a globally atomic snapshot or object freeze. Other
mutable aliases retain the existing language permissions. Generated traversal
uses existing collection version checks and never invokes application getters,
custom constructors, display hooks or equality operators. Concurrent mutation
has only the existing Runtime/iteration guarantees; no new cross-thread DOM
publication or snapshot protocol is introduced.

## 9. Approval boundary

The three decision bundles are listed in [RESEARCH](RESEARCH.md#decisions-for-wrench-and-boss).
Automatic public DTO conversion, distinct parse names, strict duplicates/exact
numbers and the resource defaults require approval. The implementation must not
silently drop typed conversion, diagnostics or readonly support to claim a
dynamic-only prototype complete. If Boss instead chooses explicit source DTO
mapping, rewrite the scope/acceptance first and label that limitation plainly.
