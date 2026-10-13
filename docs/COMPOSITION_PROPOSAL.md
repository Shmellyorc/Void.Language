# Phase 19 proposal: using composition and member promotion

Status: **U1–U7 approved for implementation by the owner, including named and
unnamed stored composition.** The original investigation/review contract follows;
[COMPOSITION.md](COMPOSITION.md) describes implemented behavior. Verification and
remaining hosted gates are recorded in WORK_LOG.md. Approval does not expand the
one-hop boundary, method ownership, initialization or readonly contracts.
Phase 18 is locked at owner-reported run #30, attempt #2, all seven jobs 43/43.

## Approved U1–U7 contract

| Decision | Revised contract |
|---|---|
| U1 — declarations | Allow multiple named `using name: type` fields and unnamed `using type` shorthand in both managed and value structs. `using movement` stores an ordinary field named `movement`, accessible as `object.movement`. Both forms promote members identically. Duplicate stored names, including implicit names, diagnose; explicit names distinguish repeated embeddings. Targets remain nonnullable nominal managed/value structs with ordinary pure field-default semantics. |
| U2 — lookup | One member namespace for selection: a direct field or owner method wins over promoted members. Otherwise require exactly one accessible promoted candidate. Overlapping member names are legal in the containing declaration; ambiguous access diagnoses at compile time. Do not choose by member types, argument signatures, expected result types or field-versus-method syntax. |
| U3 — depth | Include one-hop promotion initially: only direct ordinary fields/methods of each embedded type. Explicit qualification can continue through arbitrarily nested valid compositions under existing expression limits. Defer automatic transitive promotion. |
| U4 — ownership/access | Keep declarations owned by their original type/module, current type/field visibility and direct-owner method import rules. Promotion grants no new access or transitive method export. |
| U5 — construction | A using field is an ordinary named stored field for defaults, constructor labels, ordering, C1/S1 readiness and P4 commit. Promoted names are not constructor labels and lifecycle methods are never promoted. |
| U6 — semantic preservation | Elaborate promotion to existing typed qualified paths before capability/construction analysis and lowering; preserve receiver selection, P3/RV leaf profiles, effects, precise roots and complete publication. |
| U7 — tooling/reload | Record the using marker, original member identity and embedding dependencies in shared analysis. Restart conservatively for changed composition/member-resolution contracts; retain ordinary compatible body reload and generation leases. |

The owner approved the complete revised U1–U7 contract for Phase 19 implementation.
All acceptance proofs below remain required. No new ownership syntax, dispatch runtime or
representation is needed.

`extends` means “I am this type”; `using` means “I have this functionality”;
`interface` means “I support this behavior”. Composition creates neither subtyping,
multiple inheritance nor implicit conversion. Inheritance, virtual dispatch,
interface implementation and `has` are entirely outside this phase.

## Investigation: current implementation and concrete gaps

Reviewed LANGUAGE_BIBLE.md, DEVELOPMENT_STRATEGY.md, VALUE_STRUCTS.md,
INITIALIZATION.md, RECEIVER_METHODS.md, MANAGED_STRUCTS.md,
MANAGED_INITIALIZATION.md, REFERENCE_VALUE_STRUCTS.md, GC_RUNTIME.md, KNOWN_ISSUES.md
and the recent WORK_LOG.md evidence. Historical phase-specific deferrals in older
guides are superseded by the Phase 16–18 guides; they are not current prohibitions.

Relevant actual code:

- `src/frontend/compiler.cpp`: Parser's structure branch records each field as a
  Statement with annotation/default syntax; there is no using marker. Structure
  layout resolves nominal fields in declaration order, detects inline recursion,
  and enforces depth 128 and 65536 expanded-field limits. Method registration
  rejects duplicate owner methods and direct field/method collisions.
- `Semantics::field_expression`, assignment checking and `Semantics::method`
  independently search direct fields/methods. Method identity is
  `type_identity::method::name`, with explicit owner-module accessibility checks.
  There is no promotion candidate or path resolver. Fields do not currently have
  independent parsed visibility modifiers; analyzed fields inherit type visibility.
- `method_expression`, `Lowering::place`, `select_place`, `read_place` and
  `write_place` already support nested receiver storage and selected managed-owner
  snapshots. A reference crossing selects/root-saves its object; it must not be
  replaced with an extracted inline receiver copy or delayed copy-back.
- `src/frontend/capabilities.inc` projects effects/provenance through typed field
  indexes and receiver paths. `construction.inc` proves staged-view readiness
  through physical direct fields. A promoted spelling cannot bypass these paths.
- `include/void/ir.hpp` has one nominal Type, ordered ValueType fields, reference
  leaf profiles, Place paths and ordinary aggregate/managed field operations.
  `src/ir/module.cpp` and `reference_capabilities.inc` independently verify paths,
  signatures, capabilities, construction and publication. There is no IR concept
  of inheritance, promotion or an embedding pointer adjustment.
- `src/backend/llvm/backend.cpp` uses target DataLayout for actual fields, roots,
  descriptors and debug types. Logical reference paths flatten inline nesting and
  stop at managed references. Runtime Frame/Slot roots, the heap's descriptors and
  `construct_into` already cover these ordinary storage shapes.
- Shared analysis generation in `compiler.cpp` records field/method declarations
  and reference spans; assignment navigation currently assumes written fields and
  physical indexes correspond one-to-one. `tooling.cpp` compares declaration and
  capability/construction contracts. Both need explicit promotion metadata rather
  than synthesizing methods or inferring resolution from spelling later.
- Existing tests in `tests/reference_values`, `tests/managed`, frontend receiver
  tests and LSP protocol checks exercise qualified nested mutation, capabilities,
  staged initialization and owner-root selection. They are useful foundations,
  not evidence that promotion already works.

The central missing abstraction is a shared **member selection result** containing
original declaration identity, ordered physical field path, member kind and access
information. Introduce it in the frontend's existing semantic model, shared by
reads, writes and receiver calls. Do not add a second type checker.

## Syntax, ownership and access

The named form uses the current field declaration with a `using` marker, the
existing name, annotation and optional pure default. The unnamed form omits the
explicit name and colon, deriving the stored field name from the type name:

```swift
struct player:
    using motion: movement
    using combat
```

Here `motion` and `combat` are real stored fields. `using movement` would instead
store a field named `movement`, accessible through `object.movement`. The shorthand
is named physical storage after elaboration, not anonymous storage or method-only
reuse. Both forms use the same type resolution, defaults, constructor field labels,
qualified access and promotion. Imported types retain their ordinary nominal
identity; a module qualifier is not part of the stored field name.

Use `using` contextually in field declarations (a marker before a field name),
without confusing the existing module import keyword `use`. Preserve ordinary
identifier uses, including an existing `using: movement` field without a marker;
Both `using motion: movement` and `using movement` carry an explicit type; no
inferred using type or function-body using statement is introduced. Duplicate
stored field names and direct field/method collisions retain their diagnostics.
Two `using movement` declarations produce the same stored name and must diagnose
at declaration time, as does a collision with an ordinary field named movement.
Use `using left: movement` and `using right: movement` for distinct embeddings.
This duplicate-storage error is separate from legal overlapping promoted names. Marking a field does not rename its type or adopt its methods.
Both managed and value owners can compose either kind; actual field semantics
follow the declared type, not the marker. Primitive and nullable using types
should diagnose with a hint to use an ordinary field instead. Nullable storage
remains available as an ordinary field with explicit checked unwrap; promotion
must not silently insert runtime unwrap or conditional dispatch.

No new field visibility syntax is proposed. Respect current supported type/field
access rules. Embedded methods retain owner-file private restrictions and existing
explicit import/root-owner rules. In particular, importing a wrapper does not
implicitly export methods from a transitively reachable embedded module. A caller
may need to import that method's owner too, just as for qualified access today.
Private candidates are not exposed as public promoted methods. When all candidates
are inaccessible, issue the existing visibility/import diagnostic rather than
claiming the member is absent. Available public candidates may be selected without
private candidates creating a public ambiguity. Related locations must not serve
as a bypass of access checks.

## Lookup contract and ambiguity

1. Resolve receiver nominal type and existing nullable restrictions.
2. A directly declared member with the spelling wins, including a using field's
   own name. If inaccessible, report access failure; do not fall back to embedding.
3. Otherwise inspect direct non-lifecycle fields/methods of every using field's
   type. Filter by existing accessibility at the call site.
4. Zero accessible candidates reports unknown/inaccessible member as appropriate;
   one resolves; two or more diagnose ambiguity with qualified-path suggestions
   and related declaration/embedding locations.
5. Check whether the selected member supports the requested operation and perform
   normal argument/type/capability checks. Do not retry another candidate on error.

Field/method ambiguity counts even in a call or assignment. No overload selection,
“first field wins”, shortest-path rule, or signature-based fallback is introduced.
Neither member types nor argument signatures nor expected result types may guess
the intended component. A field `x: i32` in one component and `x: f32` in another
still make promoted `object.x` ambiguous, even in an i32 context. Multiple
components may legally declare those names: only the ambiguous access diagnoses
at compile time, not the containing struct declaration. Explicit qualification
selects the intended component, subject to ordinary visibility and typing.
Embedding the same nominal type twice produces two distinct paths, even if managed
fields happen to point at the same object at runtime. Ambiguity is diagnosed on
use, not merely because a type has duplicate promoted names: qualified access is
valid. Direct declarations can deliberately hide promoted members; explicit paths
still reach them.

U3 deliberately defers recursive automatic promotion. For `outer.middle.inner`,
`outer.middle.member` can promote from middle's direct using child, but `outer.member`
cannot search through both using edges. Qualification composes the same resolver
at each written access. Managed using cycles remain legal finite reference graphs;
one-hop lookup cannot recurse through them. Infinite inline layouts remain invalid.
This restriction is a phase boundary, not a permanent language prohibition.

## Approved source examples and expected results

A complete acceptance program for the proposed feature:

```swift
value struct movement:
    x: i32

func (m movement) move(amount: i32):
    m.x += amount

struct combat:
    health: i32

func (c combat) hit(amount: i32):
    c.health -= amount

struct player:
    using motion: movement
    using combat: combat

func entry(value: i32) -> i32:
    var shared = combat(health = 100)
    var p = player(motion = movement(x = value), combat = shared)
    p.move(2)
    p.motion.move(3)
    p.hit(4)
    return p.x + p.combat.health
```

Input 10 should return 111 in normal/O2 JIT and linked AOT. The promoted and qualified operations now use the same implementation paths.

Value-copy versus shared target identity, using the movement/combat declarations:

```swift
value struct kit:
    using motion: movement
    using combat: combat

func entry(value: i32) -> i32:
    var a = kit(motion = movement(x = 1), combat = combat(health = 100))
    var b = a
    b.move(2)
    b.hit(4)
    return a.x + b.x + a.health
```

Expected 100: independent x values 1 and 3, shared health 96. Conversely, copying
a managed player reference aliases the player, including its inline motion field.

Readonly example, inside the preceding kit program:

```swift
const view = a
var copy = view
copy.move(1)          # Valid: copied independent scalar/value storage.
copy.hit(1)           # Reject: embedded combat reference remains readonly.
```

A const managed player also forbids mutating its embedded motion storage. Explicit
`copy.combat.hit(1)` and promoted `copy.hit(1)` must have identical authority checks.
An independent mutable alias to combat remains valid. Replacing reference storage,
passing/returning mixed profiles and mutable-formal requirements retain RV1–RV7;
no whole-value readonly bit may replace reference-leaf tracking.

Ambiguity and deliberate qualification:

```swift
value struct ambiguous:
    using left: movement
    using right: movement

# In a function:
var p = ambiguous()
p.x = 3              # Reject: left.x and right.x are both candidates.
p.move(1)            # Reject: left.move and right.move are both candidates.
p.left.move(1)       # Valid, updates only left storage.
```

Unnamed shorthand and distinct explicit embeddings:

```swift
struct shorthand_player:
    using movement
    using combat

struct two_motions:
    using left: movement
    using right: movement

# In a function:
var p = shorthand_player(combat = combat(health = 100))
p.movement.move(1)    # Qualified access to the real implicit field.
p.move(2)             # Same receiver selected through promotion.
```

`struct duplicate` containing two `using movement` fields must diagnose duplicate
stored name `movement`; `two_motions` is legal but `value.move(1)` is ambiguous.
With two components defining `x: i32` and `x: f32`, `var n: i32 = value.x` is still
ambiguous. `var n: i32 = value.left.x` selects left explicitly and then undergoes
normal type checking; an incompatible qualified field diagnoses a type mismatch
rather than selecting right. Likewise, differing method parameters/results do not
resolve `value.method(...)` without a unique component.

Additional rejected cases and the intended explanations:

```swift
struct invalid:
    using number: i32       # A using target must be a supported nominal struct.
    using maybe: combat?    # Nullable promotion cannot implicitly dereference.

# With player p:
var m: movement = p         # No composition conversion/subtyping.
p.init()                   # Direct lifecycle invocation remains invalid.
player(x = 1)              # x is not a stored player constructor field label.
```

These are separate negative fixtures, not a single valid program. Lifecycle names
must be rejected before promotion, including calls through qualified fields.
A failed constructor continues to leave assignment destinations unchanged.

## Initialization and execution invariants

A using field occupies exactly one existing physical field position. Marking it
adds neither a hidden object nor duplicate storage. Supplied constructor arguments
run once in written order. Omitted fields prepare in declaration order using the
existing safe/pure defaults and nested value construction; using never reorders
fields or reruns init. No-init construction accepts the stored field name (`motion`),
not promoted names (`x`). With custom init, only initializer parameter labels bind.

C1 remains all-fields-valid before value init. A using managed field is still a
required reference unless explicitly complete/default-valid under existing rules;
there is no automatic managed allocation. S1 required direct managed fields can be
assigned as whole fields in a staged initializer; a promoted subfield write or
method call first reads that child and therefore requires it ready. Helpers cannot
sneak first initialization through promotion. Nested value fields must already be
valid under C1. The parent receives identity only after successful P4 final commit;
no incomplete parent/root/alias is introduced. Pure all-null templates versus
runtime readonly views retain RV5; using grants no purity exemption.

Lower promoted expressions to ordinary typed field-expression chains before
capability/effect and S1 analysis. Lower assignment paths to physical indexes using
the same resolver and preserve written spans separately; do not assume one token
per index. Resolve promoted method receiver to the embedded place, never an ordinary
argument copy. Select receiver/target before arguments/RHS; snapshot at each managed
reference crossing exactly as qualified selection does. Keep original selected
heap owners rooted across allocation, calls and safepoints; an interior address is
insufficient. For inline selected storage, argument-side replacement retains current
qualified semantics, without stale snapshot write-back or overwriting sibling edits.

Embedded reference profiles, construction aliases, helper contexts and inferred
method effects must see the expanded paths. Returning a promoted alias preserves
its leaf provenance. S1 context bounds remain 1024; promotion adds no helper
specialization axis. Existing failure cleanup and protected outputs apply unchanged.

## Representation, tooling and compatibility

Keep the using marker in source/shared analyzed field metadata. Native fields,
nominal identity, receiver signatures and runtime descriptors remain unchanged.
Do not flatten promoted fields into the type layout or generate forwarding methods.
No new IR op, LLVM storage type, allocator, GC roots, runtime permission tags or
external aggregate ABI is recommended. Existing independent IR checks must still
reject bad physical paths/capability claims regardless of source resolution.

For navigation, the written promoted member token targets the original member
field/method declaration. The written using field name targets its own declaration;
its annotation targets the embedded nominal type. Implicit embedding edges drive
dependencies, not fabricated source spans. Track the using marker/path in analyzed
metadata and comparisons. Update assignment references, which currently rely on a
one-to-one written-name/index relationship. Related ambiguity diagnostics show each
qualified route. Invalid unsaved composition must suppress stale navigation until
repaired through existing validated sessions. No new LSP completion/hover/rename
feature is needed; any editor highlighting addition is presentation only.

Adding/removing/changing a using marker can change resolution without changing bytes:
it must invalidate analysis and conservatively require restart. Include ordered
using fields, resolved target identities, access/import dependencies and member
selection contracts in compatibility. Adding a new embedded member can change an
outer selection/ambiguity; analyze consumers again. Method effects/capabilities,
layout/default/init policy and conservative initializer dependency rules retain
current checks. Compatible ordinary body edits retain hot reload and old-generation
leases. Managed reachability remains independent of code generation lifetime.
Debug metadata describes actual stored fields, not synthetic promoted duplicates.

## Alternatives reviewed and deferred design questions

- **Recursive automatic promotion now:** more ergonomic deep wrappers, but adds
  cycle handling, path explosion/bounds and precedence questions. Deferring it
  avoids prematurely choosing shortest-path semantics. If approved in a later phase,
  design a bounded, iterative traversal with distinct-path ambiguity and explicit
  cyclic-promotion policy before implementation; do not reuse inline-layout recursion.
- **Re-export embedded methods through the wrapper:** reduces imports, but changes
  current method visibility/export contracts. Recommend existing imports first;
  explicit owner wrapper methods can expose chosen behavior today.
- **Kind-specific or signature-based lookup:** could avoid some ambiguities but
  makes lookup change with operation context. Recommend one namespace/direct-first.
- **Nullable using:** explicit qualified nullable fields already work. Implicit
  unwrap or optional dispatch would introduce new behavior; defer them.
- **Method-only reuse without embedded state:** separate deferred design question.
  Neither form of using is a trait/mixin or injects implementations without storage.
  No Phase 19 method-only syntax or behavior is proposed.
- **Managed owners only:** smaller syntax surface but unnecessarily excludes valid
  value composition. Recommend both existing struct kinds with ordinary semantics.

The owner has approved U1–U7, including both syntactic forms, one-hop promotion,
direct precedence across member kinds, accessibility filtering/import requirements
and nonnullable targets. No further routine design approval is required within
this boundary. Transitive promotion,
interface/virtual dispatch integration and method-only reuse remain deferred. No architectural contradiction was found in qualified-path reuse;
implementation must stop for review if equivalence cannot preserve C1/S1/P3/P4/RV.

## Approved implementation sequence

1. Parse named/unnamed fields into the same stored-field model with explicit or
   derived names and source spans; add field marker and shared analyzed composition
   metadata, duplicate implicit-name checks, grammar/recovery and
   unsupported-target diagnostics. Preserve normal named field construction.
2. Implement one bounded shared selector with direct precedence, candidate paths,
   visibility and ambiguity diagnostics; elaborate reads/writes/calls once.
3. Feed resolved paths through current capabilities/effects, construction readiness
   and Lowering places; independently verify IR and selected-owner root behavior.
4. Add navigation/dependency/reload comparisons and invalid-edit repair regressions.
5. Add real source fixtures, native JIT/AOT and GC stress/negative IR coverage; update
   implementation guides only once behavior is implemented and verified.

## Acceptance and verification gates

Preserve every existing **43-suite** assertion. Add focused promotion tests rather
than replacing existing qualified-access proofs. Required coverage:

- Both owner and field kinds; named/unnamed parity, implicit qualified access and
  constructor labels, duplicate implicit names and implicit/explicit collisions,
  repeated types with explicit names; multiple fields, imports/aliases,
  private/inaccessible methods, missing imports, direct field/method precedence,
  field-vs-method conflicts, i32/f32 field ambiguity, method argument/result-type
  non-disambiguation and repeated same-type paths. Overlapping promoted declarations
  must succeed until ambiguous access is attempted. Verify exact related
  locations, ambiguity hints and parser recovery with accurate source spans.
- One-hop boundary and nested qualified access, finite managed using cycles and
  rejected inline recursive layouts; preserve existing depth/resource diagnostics
  and 1 MiB semantic tests. No recursive candidate expansion bypasses limits.
- Compare each promoted program with its explicit-path counterpart: inline copies,
  managed aliases, original receiver mutation, whole nested replacement, side-effect
  argument/RHS selection, sibling edits, methods on allowed temporaries and failure
  cleanup. Distinct expected results must catch accidental receiver copying.
- P3/RV tests for const views, independent mutable aliases, per-leaf mixed profiles,
  field replacement/assignment, casts, helper parameters/returns, effect SCCs and
  nullable checked access. Neither promoted nor qualified storage can launder authority.
- C1 nested defaults/order/custom init, required-child boundaries; S1 reads/calls
  before readiness, whole-child assignment, safe helpers, no escaping construction
  view, once-only init, direct lifecycle rejection and failed-publication protection.
- Precise GC with no unrelated host roots: live selected owners and nested leaves
  across receiver/argument allocation, calls/returns, loops and temporaries in normal
  and O2 JIT and separately linked AOT. Force budget-one collection, cycles,
  deterministic overlap, allocation/rendezvous failure, output protection/recovery.
  Preserve no-sweep failed rendezvous and callback/provider-quiescence proofs.
- Independent invalid IR for wrong receiver paths/types, invalid capability writes,
  unready staged reads, invalid commits and readonly escapes. No source-only flags
  may authorize otherwise unsafe IR.
- Imported declaration navigation and references for promoted reads/writes/calls,
  ambiguity diagnostics, unsaved invalidation/repair, composition-only restart,
  effect changes and compatible body reload. Real extension unit/editor integration
  and VSIX packaging remain required at implementation acceptance.

Final implementation gates: both LLVM **19.1.7** SDKs in Debug, Release and strict
ASan/UBSan with leak detection (six Linux configurations), all existing suites plus
new suites, repeated deterministic GC stress, 1 MiB stack regressions, normal/O2
JIT and runtime-only separately linked AOT. Preserve the seven hosted jobs and
Debug editor gates. Hosted Windows/macOS results must be recorded after owner push;
Linux success cannot substitute. No compiler/runtime/workflow changes are needed
during the original design-only investigation.

## Actual investigation verification

Existing Debug test inventories enumerate 43 suites. Focused unchanged-baseline
checks use both installed and official LLVM 19.1.7 Debug builds, selecting
`managed_initialization_source`, `reference_values_source`, `lsp_protocol` and
`receiver_methods_cli`. Results are recorded in WORK_LOG.md. These exercise reused
mechanisms, not Phase 19 syntax or implementation. No full build matrix was run.


## Phase 19 implementation and acceptance

The approved five-stage sequence is complete locally. Both stored forms, shared
member selection/path elaboration, capability/readiness reuse, analysis/navigation
and compatibility metadata, diagnostics and native integration coverage are
implemented. COMPOSITION.md is the implementation guide. The original baseline
checks above remain investigation evidence, not implementation proof.

Final implementation acceptance: 47/47 CTest suites in each of six LLVM 19.1.7
configurations (both SDKs, Debug/Release/strict sanitizers with leak detection),
446 additional repeated/stress/1 MiB-stack CTest executions, native normal/O2 JIT
and linked AOT, real LSP and VS Code integration, extension tests and VSIX packaging
passed. Exact commands, timings and the sandbox LeakSanitizer limitation/recovery
are recorded in WORK_LOG.md and build/phase19-verification/summary.json.
All 43 previous suites/assertions remain. No hosted Windows/macOS or VSCodium
execution is inferred; all seven hosted jobs remain the next acceptance gate.
