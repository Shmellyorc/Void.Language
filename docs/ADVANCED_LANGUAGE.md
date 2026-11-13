# Advanced language: Stage 4

## Stage 8 Sprint 3 — current extension

The owner-approved [text/collection contract](STAGE8_TEXT_COLLECTIONS.md) supersedes
the dated get/pop/text-inspection exclusions below. Sprint 3 is locally verified;
[acceptance evidence](stage8/SPRINT3_ACCEPTANCE.md) records exact results and limits.
Wrench review remains pending. Public policies are approved
in [DECISIONS.md](DECISIONS.md); hosted CI remains deferred to Stage 8 end.


Historical Stage 4 used LLVM 19.1.7 with the production frontend, typed IR,
shared JIT/AOT lowerer and coordinated Runtime. Current operations use
[LLVM/Clang/LLD 23.1.4 exclusively](SOURCE_COMPILER.md#active-toolchain).
Properties and callable structs remain as
specified in [PROPERTIES.md](PROPERTIES.md). **Stage 4 is hosted-verified and LOCKED** at `483e67e407b65674bdc82d44767f29bd83d73ae8`, [CI #68](https://github.com/Shmellyorc/Void.Language/actions/runs/39174408997): seven successful configurations, 80/80 suites each (560/560 total).
Exact local validation and remaining boundaries are recorded in [WORK_LOG.md](WORK_LOG.md).

Stage 6 Delivery 1 composes these synchronous contracts with native async
activation; see [ASYNC.md](ASYNC.md) for the implemented subset and acceptance.

## Enums, results and patterns

```typescript
enum response:
    accepted(number: i32)
    rejected

func checked(number: i32) -> result<response, string>:
    if number < 0:
        return Err("negative")
    return Ok(response.accepted(number))

func use(number: i32) -> result<i32, string>:
    var item = checked(number)?
    return match item:
        response.accepted(var value): Ok(value)
        response.rejected: Err("rejected")
```

`Ok` and `Err` need a contextual `result<t, e>` type. `?` propagates the exact
error type and runs registered cleanup. Results and enums require explicit
variant initialization, including required struct fields and nonempty fixed
arrays. Required managed fields may be initialized by `init`; the existing C1
rule still requires complete value fields before value `init` begins.

One pattern checker/lowerer serves `is`, statement `match` and expression `match`:

- `_` ignores a value; `var name` creates a success-scoped binding without
  shadowing; an ordinary name compares its existing value.
- Value expressions, ascending/descending inclusive/exclusive integer ranges,
  scalar/value/managed/interface type patterns, named struct fields, enum/result
  payloads and nullable `Ok(pattern)` are supported.
- `[first, ..var rest]` makes an independent rest list. Dictionary patterns match
  the listed keys and permit additional keys. Nested patterns compose normally.
- `and` exposes successful captures to its right operand and successful body;
  captures do not escape `or`, `not`, failed arms or the enclosing scope.
- Incomplete matches warn. An unmatched statement does nothing; an unmatched
  expression raises a source-located runtime failure. Exhaustive matches do not
  warn. `Ok(null)` remains distinct from `Err`.

Fixed children copy independently; managed/list/dictionary children retain
identity and readonly authority. Inactive variant fields are zero and traced
precisely. The lowerer validates/extracts only the active payload.

In the editor, `response.` offers the declared variants in an expression or
an arm of `match item`, including while the arm's colon is still missing.
The editor reports the written type and completed member as separate semantic
roles. Payload constructors still require their arguments, and payload patterns
still require the correct subpatterns. [Editor behavior](EDITOR_INTEGRATION.md#enum-member-completion)

### Enum case tests and imports

Use `is` to test one enum case and `match` to dispatch across cases.
Ordinary tagged enums, including payload-free `console.key`, have no
built-in `==` or `!=` comparison. An enum pattern can also bind a case's payload.

```void
use console

func is_up(pressed: console.key_event) -> bool:
    return pressed.key is console.key.up

func direction(pressed: console.key_event) -> string:
    match pressed.key:
        console.key.up:
            return "w"
        _:
            return ""
```

With `use console`, `key.up` also resolves through the imported type name.
With `use console as term`, spell the qualified case `term.key.up`.
An exhaustive enum match can list every case explicitly. Use a final `_:`
arm to handle all remaining values. A return after a partial statement match
leaves the match itself incomplete and can still produce warning `VOID2021`.
[Adventure source and diagnostic probes](developer-experience/G17_ASCII_ADVENTURE.md#language-and-diagnostic-findings)

Built-in `==` and `!=` support compatible numeric and Boolean operands, string
contents, managed reference identity, and values of the same numeric-backed enum
type. A nominal type can also supply an applicable `operator ==`. An ordinary
tagged enum stores a case and any associated payload. It has no built-in rule
for equality of that data, even if all its cases are payload-free.

Use `not (pressed is console.key.up)` to exclude the `up` case. To compare two
runtime enum values, decide which cases and payloads count as equal. This
example binds both payloads and compares their numbers explicitly.

```void
enum notice:
    idle
    done(amount: i32)

func same_notice(left: notice, right: notice) -> bool:
    match left:
        notice.idle:
            return right is notice.idle
        notice.done(var amount):
            if right is notice.done(var other_amount):
                return amount == other_amount
            return false

func main() -> i32:
    if same_notice(notice.done(7), notice.done(7)):
        return 0
    return 1
```

A literal payload pattern such as `notice.done(7)` tests both the case and the
payload. A pattern that binds or discards a payload leaves its comparison to
the surrounding code. Arbitrary runtime enum expressions cannot always be
substituted for patterns. [Diagnostic guidance](DIAGNOSTICS.md#tagged-enum-equality)

## Multiple return values

```python
func position() -> (x: f32, y: f32):
    return 10.0, 20.0

func validate(data: string) -> (bool, string):
    return data != "", data

func main():
    var x, y = position()
    const valid, message = validate("ready")
    print(x, y, valid, message)
```

Components are checked against their declared types. A named list requires
unique labels for every component. These labels appear in editor signatures.
Return expressions and destructured bindings follow declaration order.
The local bindings appear together after the call succeeds and follow the
ordinary scope, shadowing and readonly rules.

Generic calls and receiver methods, including virtual and interface dispatch,
use the same typed return convention. The private ABI uses the ordered types
and preserves component reference authority across return and extraction.
Use a value struct for a grouped value that needs storage or independent
access. Multiple returns have no discard syntax, assignment destructuring,
tuple literals or indexing. Function values and closures retain their
single-result ABI, as do async functions, operators and native bindings.
The [TestGame example](../examples/multiple_returns/test_game.void) uses
multiple returns to handle a fallible parse and validate its range.

## Function values and closures

```typescript
func counter(start: i32) -> func() -> i32:
    var number = start
    return func() -> i32:
        number += 1
        return number
```

Functions can be stored, passed, returned and invoked with exact positional
signatures. Function types support const parameters, nesting and nullable values
(use `(func(i32) -> i32)?` when needed to disambiguate the return type).
Nullable values default to null and fail safely if invoked while null; `Ok(var f)`
or a checked conversion supplies a nonnullable view.

Named functions, concrete generic function references such as `identity<i32>`,
method references, interface methods and compatible callable structs are supported.
Generic methods accept explicit arguments or infer them from a contextual callback
signature using the existing specialization checker. No polymorphic runtime
function representation is introduced.

Captured local bindings share ordinary managed cells, including across escaping
closures, copies and nested closures. Loop bindings get a fresh cell per reached
binding. Captured value receivers use independent snapshots shared by copies of
that closure; managed receivers retain object identity. Bound value callbacks own
independent receiver storage. Managed method callback identities survive assignment,
readonly aliases, inherited/virtual views and compatible reload.

Cells and environments are ordinary precisely traced Runtime objects. Callables
carry a stable function discriminator and a managed environment, with ordinary
LLVM native dispatch under the caller's generation lease. Heap metadata contains
no executable addresses. Capture layout/authority changes require restart;
compatible body changes preserve existing leased-generation behavior.

## Deterministic cleanup

```typescript
func work(resource: file):
    defer resource.flush()
    defer {
        resource.record_status()
    }
```

Reached defers register transactionally and run LIFO on normal return, `?` and
language-managed runtime failure. Deferred calls evaluate receiver/arguments at
registration; deferred blocks read shared captures during cleanup. Calls retain
ordinary statement result-discard behavior; recoverable errors can be handled
inside a deferred block. Direct return and `?` cannot leave deferred code;
locally valid loops, break/continue and nested cleanup are supported.

A failed registration leaves earlier registrations intact. Applicable cleanup
continues after a cleanup failure; the original failure remains primary and
additional failures use the existing bounded diagnostic trace.

`with expression as name:` acquires one nonnullable managed resource and calls its
`deinit` at scope exit, including break, continue, return, propagation and runtime
failure. The cleanup storage is prepared before acquisition, so an allocation
failure after acquisition cannot lose that resource.

`deinit` is managed-only, takes no arguments and has no result. Explicit/deferred
calls and `with` share a synchronized per-object one-shot state, marked before the
body runs. Virtual overrides and explicit `super.deinit()` chaining use the existing
method machinery. GC collection never invokes `deinit`.

## Events

```typescript
struct source:
    event changed: func(i32)

func (s source) notify(number: i32):
    s.changed.emit(number)
```

Events start empty and use the existing growable buffer infrastructure. `+=`
subscribes strongly, duplicates are legal, and `-=` removes the latest matching
callback. Only owner methods emit; subscriber storage cannot escape through an
ordinary field read. Readonly views cannot alter subscriptions.

Emission evaluates arguments once, snapshots subscribers, then invokes them
synchronously in order. Nested emission and subscription changes affect subsequent
snapshots; an existing snapshot remains intact. Runtime failure stops later
handlers and enters ordinary function cleanup. Handlers have no result.

## Boundaries and regression coverage

Module-time pattern execution and bound receiver allocation are not supported;
named function constants are supported. Result success/error types must be existing
value types; no new unit/void syntax is introduced. Generic enum declarations,
custom collection equality/hash, arbitrary native callback/managed FFI and async
remain outside this implementation. Stage 5 caching was delivered separately and
is hosted-locked at CI #70; automatic watching remains future work. Callback
signatures cannot discard returned readonly reference authority.

Stage 8 Sprint 3 supplies the approved string-error signatures for dictionary/list/
fixed-array get and list pop. Conservative root lifetimes remain. The old deep valid
receiver-chain stack limitation was repaired in Stage 7/Sprint 2; unchanged 1 MiB
gates remain mandatory.

`patterns_source`/`patterns_cli` and `advanced_source`/`advanced_cli` exercise real
programs in `examples/patterns` and `examples/advanced`: normal/O2 JIT, independently
linked AOT, 2 MiB fixed captures, collection/closure cycles, failure injection,
transactional publication, scope cleanup, callback identities, generics and reload.
Existing semantic-depth tests also cover nested closure/defer recovery on 1 MiB.
