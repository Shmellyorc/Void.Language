# Phase 13: receiver methods and initialization decision gate

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

Status: the owner approved **M1 + I1**. I2 and M2 below remain unapproved
alternatives, not implemented language behavior. See RECEIVER_METHODS.md for
the implemented scope. The owner reports Phase 12 commit `acf7355`
passed hosted run #18 on Windows, Linux and macOS.

## Agreed foundation

Use `func (p point) move(dx: i32, dy: i32):` and ordinary `value.method(...)`
calls. Preserve nominal value types, copies for ordinary arguments/assignment/
returns, visibility, source diagnostics and the shared validated AST/IR/LLVM path.
Infer receiver mutation from validated writes and transitive method calls, with
finite graph traversal for recursive methods. A body change that changes this
effect is a contract change and requires restart, even with an unchanged signature.

Mutable storage must be updated in place, including a selected nested field.
Implement an internal typed receiver-place operand rather than copying out and
writing back after the call: delayed writeback could lose argument-side mutations
or mishandle failure propagation. Source programs gain no pointer/reference syntax.
Readonly receivers permit only methods proven not to mutate their receiver.

The proposed bounded receiver contract permits non-mutating calls on temporaries;
mutating calls require a mutable local/parameter or nested field rooted there.
Evaluate the receiver once before arguments, then arguments once left to right.
For an addressable receiver, select its storage before evaluating arguments; the
method observes that storage after argument effects. Thus `p.inspect(p.change())`
observes the updated `p`. A temporary is materialized once in owned local storage.
Ordinary aggregate arguments remain value copies evaluated at their source position.

## Decision 1: ownership and method lookup

The Bible establishes receiver syntax but does not specify extension-method
coherence, competing declarations or method scope for foreign-owned types.

**M1 — Owner-module methods (recommended for this phase).** A receiver method must
be declared in the same source module/file as its nominal type. The pair of nominal
type identity and method name uniquely identifies the method; no overloading.
Calls work from the owning module, from other modules for project-root types,
and through direct imports of the owner, including aliased imports. Visibility
follows existing public/internal/file-private rules. An unrelated reachable module
does not add methods merely because it was compiled. This supports methods on
imported types while deferring foreign-type extension declarations, without
permanently prohibiting them. Fields and methods with the same name are rejected.

**M2 — Imported extension methods now.** Permit declarations for visible foreign
types and search the caller's own/root/direct-import method scopes. Competing
bindings must diagnose ambiguity independently of import order. This expands the
public coherence model and needs explicit rules for owner-method versus extension
precedence and collision handling. Those rules are not established by the Bible.

## Decision 2: custom initialization

Bible section 16 explicitly leaves named-field/custom-init interaction open.
Two bounded routes are available; neither silently locks parameterized
construction, runtime field defaults or overload resolution.

**I1 — Receiver methods plus initializer groundwork (safest minimal scope).**
Complete methods, receiver-effect analysis, storage lowering, tooling and tests.
Keep Phase 12 construction unchanged. Reserve/detect receiver `init` as lifecycle
syntax and diagnose unsupported custom initialization rather than allowing it to
run as an ordinary method. Record initializer identity/effect and compatibility
requirements in the design, but do not expose a partly functioning constructor.
Public initializer execution waits for a separate approved construction contract.
The Phase 13 request expressly permits this route.

**I2 — Add simple zero-argument initialization now.** Proposed complete contract:

1. At most one receiver `init()` per type, with no explicit parameters or result.
   Parameterized/positional construction remains deferred, not prohibited forever.
2. Start with Phase 12's recursive pure field defaults. Evaluate explicit named
   fields once in written order and install them, then invoke the outer type's
   initializer exactly once. The initializer can inspect and overwrite named
   fields. This order is observable and needs approval.
3. `var p: point` is default construction and runs `point`'s initializer. Copies,
   ordinary parameter passing and returns never rerun initialization.
4. Nested fields supplied by pure defaults are already valid values; they do not
   implicitly run their type's custom initializer. An explicitly evaluated
   `point(...)` constructs that value and runs its own initializer once. This
   avoids initializing an omitted nested value and then discarding it when an
   explicit named value replaces it, but distinguishes safe nested defaults from
   explicit construction. This distinction also needs approval.
5. Construction of an init-bearing type is runtime-only: module constants and
   declared field-default expressions cannot call it, even for a non-mutating
   initializer. Safe defaults without an explicit constructor call remain pure.
6. `.init()` is never an ordinary callable method. Successful construction exposes
   the value only after initializer completion; existing arithmetic failure
   propagation prevents use of a failed result. No generalized fallible constructor
   or managed cleanup system is introduced.
7. Initialization is an intrinsic construction step, not an explicit call subject
   to method visibility. Whether lifecycle declarations themselves accept access
   modifiers should be decided before extending this bounded public surface.
   For I2, reject explicit visibility modifiers on `init` rather than implying
   private-constructor access rules.
8. Initializer identity, signature, receiver effect and exact body token syntax
   participate in the restart contract. Ordinary compatible method body changes
   remain reloadable; initializer body changes conservatively require restart.

An alternative order, defaults → init → named fields, preserves explicit fields as
final overrides but prevents init from observing them. Parameterized initializers
also need a way to distinguish named initializer arguments from named fields.
Neither alternative is selected here. If I2's nested/default/constant rules are not
desired, select I1 and resolve construction as a later coherent design phase.

## Implementation and verification after the gate

The current parser flattens qualified calls, so method calls must retain their
receiver expression until semantic resolution distinguishes module qualification
from value access. Function registration must retain receiver ownership and effect;
the existing small expression handlers and depth guards must remain small.

Extend owned declaration/reference records and exact compatibility data; tools and
LSP consume them directly. Extend typed IR validation and shared LLVM lowering with
an internal receiver place; ordinary parameters remain by value. Preserve source
failure checks and target-native private aggregate layouts. No external C ABI claim.

Keep all 22 suites and add real-source method coverage in normal/O2 JIT and linked
AOT, readonly/temporary/nested mutation, once-only evaluation, imports, recursive
effect graphs, diagnostics, reload and navigation. Run all six installed/official
LLVM 19.1.7 Debug/Release/strict sanitizer matrices, both small-stack Debug checks,
repeated stress, extension units, actual editor integration and local VSIX packaging.
