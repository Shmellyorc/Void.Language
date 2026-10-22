# Stage 10 operator overloading

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


**Approved for implementation; Stage 10 acceptance remains open.** The Bible's owner-contained operator direction
is approved in principle, but exact signatures/resolution are not implemented.
This proposal settles those details without changing ownership or coercion rules.
[Main roadmap](../STAGE10_DESIGN.md).

## Complete approved surface

| Family | Declarations / behavior |
|---|---|
| Binary arithmetic | `+`, `-`, `*`, `/`, `%` |
| Unary arithmetic | `+`, `-` (explicit B3 recommendation within the arithmetic family) |
| Equality | Declare `==`; `!=` is its compiler-derived Boolean negation using the same selected call |
| Ordering | Optional independent `<`, `>`, `<=`, `>=`, each returns bool |
| Compound assignment | `+=`, `-=`, `*=`, `/=`, `%=` reuse selected arithmetic plus ordinary assignment |

Types need not implement every arithmetic/order operation. Order operators are
independent because `not (a < b)` is not generally `a >= b` for partial orders;
do not synthesize ordering from equality or a hidden three-way comparison.
Equality reflexivity/symmetry/transitivity remain a programmer contract, not a
theorem proved by the compiler. Derived `!=` guarantees only complementarity.

No independently declared compound operators, assignment, conversions, Boolean
short-circuit operators, `not`, `is`, `as`, `has`, indexing/call, shifts,
bitwise operators or increment/decrement are added to this approved family.
Existing builtin versions remain where already supported. Callable structs keep
their existing `call()` mechanism. This is an explicit scope boundary, not a
proposal to leave part of the approved arithmetic/equality/order set unfinished.

C# demonstrates owner-contained unary/binary declarations and operand-owner
requirements, but current C# documentation includes newer compound-assignment
features VOID does not need. Do not copy those newer semantics.
[C# operators](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/operators/operator-overloading).

## Declaration syntax and validation

```python
value struct point:
    x: i32
    y: i32

    operator +(const left: point, const right: point) -> point:
        return point(x = left.x + right.x, y = left.y + right.y)

    operator ==(const left: point, const right: point) -> bool:
        return left.x == right.x and left.y == right.y
```

The operator is a function body **inside** its nominal owner's declaration, as
the Bible requests. It is implicitly a static operation with explicit operands;
no `static`, receiver alias or new source `this`. Reuse ordinary parameter/body/
return parsing and lowering after a small operator-header parser. The source
declaration remains owner-contained even if compiler storage uses a normal Function.

One unary operand or two binary operands are required. At least one operand must
be the exact declaring nominal type, or its nullable form for managed references.
For generic owners, it must be the owner's matching instantiated type. Other
operands may be existing concrete source value types. No unconstrained bare type
parameter can satisfy ownership. No operator definition on primitives, foreign
types or interfaces; no operator-specific generic parameters, defaults, named
operand call syntax, async, virtual/override or independently mutable operands.

Operands use explicit `const`, so readonly/reference authority is never silently
strengthened. Value operands are ordinary independent snapshots; their local
bindings are readonly. Managed operands retain identity with readonly access.
Arithmetic returns one explicit existing storable value type; it need not be the
owner (e.g. a dot product can return f64). Equality/order must return exactly bool.
No-result, unfinished construction views or prepared receiver types are invalid.
No overload ranking by result type. Body returns/copies/alias provenance use the
ordinary checker; returning an existing readonly managed operand cannot silently
produce a mutable reference. A freshly constructed result can retain its normal
fresh authority.

Visibility defaults to public; internal uses existing library/project boundaries;
private is visible inside the declaring owner only for these new owner-contained
operator declarations. This does not change historical module-private receiver
methods. Operators can access that owner's private fields. Nested closures retain
ordinary lexical field access, never constructor grants. Explicit declaration of
`!=` diagnoses and points to `==`; the derived symbol/reference belongs to the
equality declaration.

Allow a bounded-by-source set of exact operand tuples per owner/operator/arity;
duplicate tuples diagnose regardless of result type, const spelling or visibility.
This is a specific operator table, not a new ordinary function-overload system.
Do not add arbitrary candidate truncation; use existing analysis resource failures
if actual compiler bounds are exceeded.

## Resolution algorithm

1. Preserve source precedence/associativity and type both operands using existing
   contextual literal rules. Static nonliteral operand types are authoritative.
2. Gather declarations from the direct nominal owners of the operand types only
   (one owner for unary). Deduplicate by stable declaration identity. Include
   nullable owner's declarations when the static type is nullable.
3. Keep exact operand nominal/type/nullability tuples. No numeric widening,
   managed ancestor conversion, boxing, user conversion, reversed-operand retry,
   commutative rewrite, expected-result ranking or runtime concrete-type search.
4. Check owner imports/visibility and const capability acceptance. An otherwise
   exact hidden declaration reports inaccessible; do not silently fall back to
   builtin identity or a component. Multiple exact accessible declarations are
   ambiguous and list both owners. Visibility does not rank two valid candidates.
5. One candidate becomes an ordinary resolved typed call. Record that target and
   explicit operand spans in the normal reference/dependency graph.
6. If no matching custom declaration exists, apply existing builtin rules unchanged.
   If neither exists, report unsupported operator/operand types. A custom declaration
   with unrelated parameter types does not suppress a valid builtin operation.

Unsuffixed literals do **not** search all operator signatures for a convenient
type. Use their current i32/f32 defaults or an existing numeric operand anchor;
the assignment's expected result cannot select a custom signature. A mixed
`point + 1` therefore matches a point/i32 declaration; point/i64 requires an
explicit cast/typed argument. This avoids adding implicit coercion by overload.
`null` can be contextually injected only using the existing nullable rule when a
single concrete opposite operand supplies that owner; two null literals do not
create an owner/candidate set. Matching nullable custom operators receive null
normally and must handle it. There is no automatic lifted call to a nonnullable
operator. Otherwise current builtin null/managed identity comparisons remain.

Operators are not inherited or promoted through `using`. A derived type gains
no base private access and no base operator solely through ancestry. Explicit
existing casts can choose a base static type, after which its operator may match.
No dynamic virtual dispatch is inserted for operators. Interfaces do not acquire
static operator requirements or boxing; generic functions resolve concrete operators
when their bodies specialize, using existing generic type checking. Unused generic
bodies retain the current deferred body-validation contract.

Owner import availability follows the same direct original-owner requirement as
ordinary methods. Do not expose operators solely through transitive reachability.
No global operator registry, per-editor candidate list or runtime patch table.

## Evaluation, effects and compound assignments

Evaluate left then right once, capture values at their own expression positions,
then call. Preserve value copies and managed identity, roots, return slots and
checked failures. The `!=` derived operation invokes `==` exactly once, then negates
its bool; it does not evaluate operands again. Operator bodies use existing
arithmetic behavior (wrapping integers, checked division, IEEE floats) unless the
source explicitly chooses different arithmetic. No extra saturation/checking mode.

Const operand formals reject mutation/retention requiring mutable authority through
those operands. Effects from unrelated I/O, allocations or calls remain ordinary
effects; an operator is not assumed pure because of its glyph. Worker reachability,
cleanup restrictions and construction readiness use the existing effect graph.
Runtime failure preserves source operation/callee frames and reached cleanup.
User code side effects are not rolled back. Pure module constants cannot invoke
user operator bodies: no new compile-time execution engine is introduced.

For `place += rhs`, capture the existing place/owner/index exactly once, obtain
its old value once, evaluate RHS once, call `+`, then use the existing assignment
commit only if the result fits that place and the call succeeds. This must reuse
the current selected-place/property/index/version machinery. Properties call one
getter before RHS and one setter after success. Readonly paths still reject;
no setter is synthesized. Indexed inline values use ordinary explicit read/copy/
replacement semantics, not an escaping mutable receiver borrow. Keep captured
managed owners rooted even when RHS rebinds the original variable. Preserve
structural-version failures for invalidated collection places rather than writing
to a different element after a mutation.

Builtins keep their current semantics. Managed custom arithmetic does not imply
clone-on-call; only explicit construction produces a new identity. Managed builtin
equality stays identity when no exact custom operator matches. Value structs gain
no automatic structural expression equality merely because operators now exist.
Type/value patterns keep current pattern comparison semantics; they do not execute
user equality hooks or gain side effects as an accidental consequence.

## Equality, hashing and dictionaries

Current dictionary eligibility and equality/hash computation are data-only Runtime
metadata, with scalar/string/managed identity and recursively eligible value keys.
DECISIONS.md explicitly leaves **custom-equality keys deferred**. Do not install
JIT callbacks in descriptors or infer a hash from arbitrary custom equality.

Recommended B4 rule: a type defining custom `==`, or a value key recursively
containing such a type, is ineligible as a dictionary key. This includes managed
key types with custom equality, whose existing identity hash would otherwise
disagree with a user expectation of equality. Adding equality can therefore make
new code using that key fail with an explicit diagnostic. No existing source
could declare such an operator before this stage, so old eligible-key programs
keep their behavior. Ordinary values containing custom-equality types remain
usable as dictionary **values**.

Check the exclusion at concrete specialization and independently in IR/native
dictionary admission, including private fields and nested value/fixed key shapes
already allowed by the key contract. The diagnostic points to the equality
declaration and key use and recommends an explicit existing scalar/string/value
key projection. Do not suggest disabling equality tests or replacing hashing with
linear scans. There is no new generic `hash()` protocol in Stage 10. A future key
protocol would need its own approved equality/hash and generation-lifetime design.

## Metadata, reload, editor and lowering

Stable identities include owner, glyph, arity and exact parameter types, encoded
injectively like existing owned native symbols. Analyzed declarations expose the
operator token span, owner, visibility, operand/result types, readonly/effect
contract and documentation. Every selected glyph use is a reference to the actual
owner definition; derived `!=` navigates to `==`. Include operator-set changes and
selection results in the same conservative invalidation/compatibility mechanism.
Signature/visibility/effect/operator-set changes require restart; compatible body
changes follow existing whole-generation leases and constructor dependency closure.
Cache identity includes compiler/formatter ABI changes as usual.

No dedicated LLVM operator backend is required. Resolved operators become ordinary
calls, and derived inequality becomes Boolean negation. The IR validator checks
declaration shape, owner access, selected exact signature, reference effects and
construction rules; a frontend-provided target string is insufficient proof.
Current JIT/O2/AOT roots, snapshots and failure channels remain the only machinery.

Compiler-owned tooling shows declaration signatures, private/const annotations and
selected operator documentation, navigates glyphs and finds references. Operator
glyph rename is rejected (changing a glyph changes semantics); parameter/type/field
renames continue through existing guarded paths. `voidls` never executes an operator
to resolve hover. Source formatting retains precedence and cannot change a custom
operation by rewriting operand order. See [TEST_PLAN.md](TEST_PLAN.md).
