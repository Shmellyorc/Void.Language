# JSON API

27 October 2026. The complete public surface is implemented and locally verified
on Linux Debug, Release and strict sanitizer profiles. See [the handoff](HANDOFF.md),
[implementation status](IMPLEMENTATION.md) and the
[approved contract](DESIGN.md) for acceptance obligations and exact budgets.

Use `use json as json` to keep the module's public type `value` out of unqualified
scope when using a loop binder also named `value`. Boss approved this explicit-alias route on 27 October 2026.
The existing grammar and no-shadowing rule remain unchanged.

```void
use json as json

func load() -> result<i32, json.error>:
    var levels = json.parse_as<[i32]>("[10,20,30]")?
    for value, index in levels:
        print("{index}: {value}")
    var stats: {string: i32} = {"hp": 100}
    var saved = json.stringify(stats)?
    var loaded = json.parse_as<{string: i32}>(saved)?
    return Ok(loaded["hp"])
```

[Executable save/data example](../../examples/json/save.void) also demonstrates
DTOs, fixed arrays, enums, exact u64 values and ordinary error handling. The [validated factory example](../../examples/json/validated.void) maps private invariant-bearing data through an explicit DTO and factory. Literal
object braces inside VOID strings must be doubled; JSON text itself uses ordinary
braces. VOID interpolation rules are unchanged.

| Function | Behavior |
|---|---|
| `parse(text, budget = limits())` | Strict parse to `result<value,error>` |
| `null_value()` | Fresh genuine JSON null node |
| `parse_as<t>(text, budget = limits(), unknown = unknown_fields.reject)` | Parse and decode with one shared budget |
| `to_value<t>(const input, budget = limits())` | Fresh independent JSON tree |
| `from_value<t>(const node, budget = limits(), unknown = unknown_fields.reject)` | Fresh writable typed data |
| `stringify<t>(const input, budget = limits())` | Compact ordered UTF-8 JSON using the one source writer |

`to_value` and `stringify` infer input types. Return-only `t` stays explicit.
Bind a propagated result to a local before passing it to an inferred generic call,
as in `var data = json.parse_as<[i32]>(text)?` then `json.stringify(data)`.
The existing frontend can infer the result wrapper for a nested `?` expression;
the frozen unrelated generic reproduction is recorded in [HANDOFF.md](HANDOFF.md).
`value` exposes a readonly `kind` property and these result-returning observations:

| Member | Success payload |
|---|---|
| `get(name: string)` | Existing object child identity; missing is an error |
| `at(index: i64)` | Existing array child, including safe negative indexes |
| `as_string()` / `as_bool()` | Exact matching kind without coercion |
| `as_number<t>()` | One of the ten numeric primitives at its declared precision |
| `number_text()` | Exact validated original number token |
| `as_array()` | Independent outer list retaining child identity and authority |
| `keys()` | Independent names in insertion/document order |
| `length()` | Array or object count |

Nodes have private construction and payloads. Const observations, forwarding,
`Ok`, `?`, slicing and enumeration retain readonly authority. Public mutation,
patching, reflection, arbitrary constructor hooks and automatic private-data
conversion are absent from this contract. Edit an application DTO/list/map, then
convert it to a fresh tree.

Numbers retain spelling and precision, including integers beyond 2^53 and tokens
such as `1e400`. Integer extraction requires an exact integral mathematical value;
`1.00e2` becomes 100, while `1e-1` fails. Unsigned negative zero becomes zero.
Float extraction rounds directly at f32/f64 precision, accepting finite subnormals
and signed zero while rejecting overflow/nonzero underflow. Typed NaN/infinity
serialization fails. Typed finite numbers use matching-precision shortest display.

Automatic schemas admit primitives, payload-free enums (exact unqualified case
names), growable/fixed arrays, string-key dictionaries, managed nullables and plain
public stored-data structs. Concrete generics, base fields first, nested `using`
components and public const fields follow ordinary construction. Every stored
field must be supported and public. Custom init/deinit, private/internal storage,
property-only schemas, payload enums, results, callables, interfaces, events,
tasks and native resources diagnose at specialization, even on an unvisited path.
Use an explicit public DTO and the application's existing factory for invariants.
Nullable type syntax currently applies to managed references. Optional scalar,
string or list data needs a nullable managed wrapper or explicit application
mapping through a DTO/dynamic node.

Nonnullable fields are required even when VOID declares defaults. Missing nullable
managed fields become null. Explicit JSON null is admitted only by a nullable
managed target or `json.value`. Unknown names reject in input order by default;
`unknown_fields.ignore` still parses all values, detects decoded duplicates and
charges parse budgets. Fields then decode in effective declaration order.

Errors derive from the ordinary core error. `kind` is one of the thirteen documented
categories, `code` is its one-based declaration index, `offset` is an original
zero-based UTF-8 byte offset (or -1 for converted data), and `path` is a relative
JSON Pointer. Missing, explicit null, wrong kind, malformed syntax and resource
exhaustion are distinct. Actual OOM/native Runtime failures keep the existing
checked Runtime failure channel.

`limits` defaults are input/output 16 MiB, depth 64, 262144 nodes, individual
strings/keys 8 MiB, number tokens 1024 bytes, 4096 object members and 67108864
conservative work units. Limits must be positive and depth at most 128. Compound
operations share counters. Logical counters are not an RSS or wall-time quota.
Cycles and excessively deep acyclic data both fail as `limit_exceeded` through
bounded iterative traversal. Output is deterministic for ordered inputs; it is
not sorted cryptographic canonical JSON.

Linux verification includes relocated installed execution and independently linked
AOT in both optimization modes. Native Windows/macOS and hosted acceptance remain
unverified; independent Golden Wrench source review is the next gate.
