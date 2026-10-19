# Advanced language: Stage 4

Stage 4 extends the production frontend, typed IR, shared LLVM 19.1.7 JIT/AOT
lowerer and coordinated Runtime. Properties and callable structs remain as
specified in [PROPERTIES.md](PROPERTIES.md). **Stage 4 is hosted-verified and LOCKED** at `483e67e407b65674bdc82d44767f29bd83d73ae8`, [CI #68](https://github.com/Shmellyorc/Void.Language/actions/runs/39174408997): seven successful configurations, 80/80 suites each (560/560 total).
Exact local validation and remaining boundaries are recorded in [WORK_LOG.md](WORK_LOG.md).

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
custom collection equality/hash, arbitrary native callback/managed FFI, async and
Stage 5 watching/persistent caching remain outside this implementation. Callback
signatures cannot discard returned readonly reference authority.

Dictionary `.get()` and list `.pop()` still await their approved error types and
signatures, even though `result` now exists. Existing conservative root lifetimes
and the documented deep valid receiver-chain lowering stack limitation remain.

`patterns_source`/`patterns_cli` and `advanced_source`/`advanced_cli` exercise real
programs in `examples/patterns` and `examples/advanced`: normal/O2 JIT, independently
linked AOT, 2 MiB fixed captures, collection/closure cycles, failure injection,
transactional publication, scope cleanup, callback identities, generics and reload.
Existing semantic-depth tests also cover nested closure/defer recovery on 1 MiB.
