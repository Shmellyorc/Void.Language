# Stage 8 Sprint 2 — ordinary calls and numeric prelude

20 October 2026. **IMPLEMENTED / LOCALLY VERIFIED: 429/429 suites; Wrench review pending. Not hosted locked.**

## 20 October 2026 — Stage 8 Sprint 2 owner-approved semantics

Boss explicitly approved all three decisions during Sprint 2 engineering:

- Ordinary functions and methods share initializer binding: positional prefix,
  then named arguments in any order; reject unknown names, duplicate bindings and
  positional arguments after named ones. Required parameters precede defaults.
  Defaults are pure compile-time expressions evaluated in the declaration's module.
  Omitted parameter annotations may be inferred only from concrete, unambiguous
  defaults; generic parameter annotations remain explicit. Written evaluation
  order, readonly capabilities and existing construction behavior must survive.
- Only numeric `min_value`, `max_value`, `abs()` and `clamp()` are admitted through
  a compiler-owned numeric prelude, without explicit imports or user extensions.
  `T.operation(value)` and `value.operation()` resolve one canonical implementation.
- Signed-minimum `abs` fails with a checked error. Float `abs` clears the sign,
  including negative zero. Integer limits are exact; float `min_value`/`max_value`
  are the most negative/positive finite values. Clamp bounds are inclusive;
  reversed bounds or either NaN bound fail. Validate bounds **before** propagating
  NaN input. Infinite bounds are permitted; in-range signed zero retains its sign.

The implementation below follows these approved semantics; executed acceptance
is recorded separately in [the evidence](stage8/SPRINT2_ACCEPTANCE.md). Sprint 1
is owner-locally accepted; preserve CI #82 repair. Hosted CI is deferred to the
end of Stage 8 unless Boss explicitly authorizes it sooner. No hosted CI request,
Git mutation or Sprint 3 progression is authorized. Root README stays unchanged.

## Engineering and acceptance

Reuse the existing initializer binder, typed numeric kernel, argument snapshots,
capability analysis, IR validation and LLVM 19.1.7 lowering. Keep parameter binding
separate from receiver storage paths. Required verification covers constructor
parity, evaluation order, generic inference, readonly and construction capabilities,
constant/runtime boundaries, all ten numeric types, normal/O2 JIT and linked AOT,
reload contracts and compiler-owned editor assistance. Sprints 3–6 remain future work.

## Ordinary call implementation

Function declarations accept `name: type = pure_default`, or `name = pure_default`
when the existing expression typer determines a concrete type. Unsuffixed numbers
retain the existing i32/f32 defaults and range checks; `null` and empty/untyped
expressions do not invent a parameter type. Parameters in generic functions and
method-specific generics retain explicit annotations. Dependent generic defaults
are checked with their concrete specialization; a separate pure-default type
context admits `t.max_value` without exposing runtime parameters or receivers; required/default ordering and
header names are checked even for unused templates.

The initializer mapper is factored into `argument_mapping` and reused for
ordinary calls and function/method generic inference. No overload set or second
binder exists. Supplied expressions remain in written order, followed by omitted
pure defaults. Lowering snapshots those values before placing them in formal
parameter order. Receiver storage paths and argument indexes are separate records;
readonly/reference effects, staged-construction masks, native callback parameters
and async worker wrappers consume the same mapping. Revisited typed calls retain
their binding instead of appending defaults again.

Module constants and pure init-free value, string, collection and function-value
templates use the existing compile-time evaluator and materialization rules.
Ordinary runtime function calls and parameter/receiver references are unavailable
in defaults. All defaults on concrete declarations are checked, including unused
ones. Numeric prelude operations are pure and can appear in constant/default
expressions; an invalid reached operation diagnoses at compile time there.

Defaults bind to the statically selected declaration before ordinary virtual
dispatch; an override receives the already bound argument vector.

Parameter names and exact default syntax participate in the existing source
compatibility contract. Default/name changes require restart; otherwise compatible
body edits retain old-generation leases. Named labels on ordinary concrete calls
navigate to parameter declarations and participate in guarded parameter rename.
Existing readonly, imported-source, version, graph and stale-edit checks remain.

Function values have positional, fully supplied signatures without source parameter
names/default policy. Interface requirements still declare required signatures;
events and the pre-existing collection intrinsics retain their reviewed positional
contracts. This sprint does not add overloads, general runtime defaults or new
collection APIs. Constructors without init retain named-field construction.

## Numeric prelude implementation

The compiler-owned registry exposes exactly four members for i8/u8/i16/u16/i32/u32,
i64/u64/f32/f64. `bool`, strings, nominal structs and collections do not acquire
numeric members. No import, runtime type object, boxing, user extension, provider
search or import-order-dependent selection is introduced.

- `T.min_value` and `T.max_value` are immutable exact typed constants.
- `T.abs(value: T) -> T` and `value.abs() -> T` share one canonical callable.
- `T.clamp(value: T, lower: T, upper: T) -> T` and
  `value.clamp(lower: T, upper: T) -> T` share one canonical callable.

Typed operands retain their types; only the existing contextual literal rules
adapt unsuffixed literals. Named numeric arguments use the same binder. A receiver
is typed and evaluated once. Chaining works through ordinary postfix resolution.
Unsigned abs is identity; signed-minimum abs raises a checked, source-located
failure. Float abs clears the sign bit, including negative zero and NaNs, without
rounding. Clamp validates both bounds and their order before selecting any result;
it propagates NaN input only after that validation. Bounds are inclusive, infinities
are permitted and an in-range signed zero is returned unchanged. No epsilon,
saturation of abs, endpoint reordering or implicit widening is added.

Exact limits and constant behavior use the shared APInt/APFloat numeric kernel.
The type spelling in `$numeric::<type>::<operation>` is derived from the exact
primitive Type::kind, distinguishing equal-width signed, unsigned and float types.
Both call forms name that same identity. Lowering emits one private typed IR
function per used canonical callable, using existing calls, arithmetic, comparisons,
branches and checked panic propagation. These bodies use the ordinary source
failure ABI, so errors unwind through the existing caller/cleanup path.

One private, width-preserving **numeric bitcast** IR operation maps directly to
LLVM bitcast. Validation rejects nonnumeric or width-changing forms. It has no
source syntax and exposes no reference/pointer conversion. Float abs uses this
representation operation and unsigned remainder by the sign-bit power of two to
clear exactly the IEEE sign bit; no algorithm-specific abs/clamp opcode, runtime
provider or duplicate receiver implementation was added. LLVM 19.1.7 and existing
strict floating arithmetic remain authoritative for normal/O2 JIT and AOT.

## Editor assistance and authority

The compiler registry supplies numeric completion, documentation, exact typed hover
and bound/unbound signatures. Builtin declaration snapshots have a stable compiler
identity and are explicitly marked as metadata without a filesystem definition.
Definition requests for builtin members return null; references point to actual
user calls. Synthetic declarations are omitted from document/workspace symbols and
cannot be renamed or opened as writable project sources. Ordinary function and
parameter navigation continues to use real source records. Rename revalidation
matches immutable builtin identities exactly, preserving its checks on every real
written binding. No editor contains its own numeric catalog.

## Scope and verification boundary

Private/declared-readonly fields were unnecessary and were not implemented.
Sprints 3–6 remain future work. Sprint 1 discovery, source/VFS admission and CI #82
Windows path repair are preserved. Hosted verification remains deferred until the
end of Stage 8; actual Windows/macOS execution is not inferred from local Linux
results. [The acceptance record](stage8/SPRINT2_ACCEPTANCE.md) distinguishes focused,
consolidated, small-stack and pending platform evidence. Stop for Wrench review.

## Recursive-frame repair evidence

During consolidated local verification, the new numeric lookup temporaries
reproduced a 1 MiB Debug overflow on deep nominal receiver chains. Numeric-only
Type/Function work is now outlined into cold helpers; the existing 256-level
semantic budget and all stack reserves remain unchanged. Valid nominal and numeric
chains through 124 calls pass; 400-call chains diagnose nesting and still recover
the later independent error. [Measured Debug frame allocations](stage8/sprint2-stack-frames.json)
record the Linux Clang prologue evidence; it is not a Windows execution claim.
