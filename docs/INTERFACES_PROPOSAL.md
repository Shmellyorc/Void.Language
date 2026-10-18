# Phase 21: structural interfaces

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

**Status: IF1–IF8 owner-approved and implemented locally; six Linux configurations passed 55/55. Hosted seven-job verification pending.**
The owner locks Phase 20 at hosted run #35, all seven configurations, 51/51 suites.
The investigation below remains the architectural rationale. INTERFACES.md describes
the implementation; WORK_LOG.md records actual validation. Examples marked as
investigation reproductions remain unchanged-baseline evidence.

## Authority and initial boundary

The Bible, Section 13, already establishes implicit structural conformance,
polymorphic behaviour, no mandatory `implements`, no injected storage or default
implementations, and conformance through unambiguous promoted methods. Preserve:

- `extends`: I am this type; managed single inheritance and identity-preserving views.
- `using`: I have this functionality; actual stored composition, not subtyping.
- `interface`: I support this behaviour; checked accessible methods, not inheritance.

Recommend a complete first slice of **managed-backed interface views**: declaration,
structural binding, parameters/results/locals/fields, nullable views, readonly,
identity, ordinary/virtual/promoted method calls and JIT/AOT/reload/tooling.
Managed owners may satisfy requirements through embedded value methods operating
on their original storage. Standalone value-to-interface erasure is deferred, with
a specific diagnostic, rather than silently boxed or permanently prohibited.
Reference-containing value structs may store interface views and copy their bits,
preserving the managed target's identity and reference-leaf capabilities.

No generics, interface inheritance, default bodies, extension methods, properties,
operator requirements, overloads, downcasts, `is` execution, managed FFI, new entry
policy or new lifetime/ownership syntax belongs to this slice.

## Owner-approved decisions

The owner explicitly approved IF1–IF8 for Phase 21 implementation.

| Decision | Recommended Phase 21 contract | Alternative / consequence |
| --- | --- | --- |
| IF1 — declarations/signatures | Owner-module nominal interface declarations with bodyless `func` requirements; exact ordered argument/result types and const-formal qualifications. No explicit concrete receiver in a requirement. Nonempty, no duplicate names/defaults/lifecycle requirements. | Overloads, defaults, `Self`, marker interfaces or requirement composition add separate rules; defer rather than guess. |
| IF2 — access and binding | Resolve each name with existing static-view member selection and normal access/import rules **at conversion**. Capture its original owner and route. An explicitly accessible factory may expose that behaviour through an accessible interface, including a private implementation it is permitted to call. Consumers import the interface, not hidden implementation owners. | Requiring consumers to import every hidden implementation defeats erasure. Globally ignoring access or resolving in the interface's module breaks existing ownership. Rejecting private implementations entirely is simpler but restricts normal factory abstraction. |
| IF3 — conversions | Checked compile-time structural conversion from a completed managed concrete view to an interface in assignment/argument/return contexts and explicit `as`. Nullable lifting preserves null; readonly never strengthens. Distinct interface declarations remain nominal and interface-to-interface conversions/downcasts are deferred. | Width conversions between interfaces need independent requirement/capability/provenance rules. Dynamic discovery from a nonconforming static base would introduce a checked runtime cast. |
| IF4 — identity and binding | Copyable reference view containing complete object reference plus immutable, code-independent binding witness. Capture the converting **static view**, not merely the dynamic class. Same-interface equality compares object identity, not witness identity; null is canonical. | One pointer cannot preserve different Base/Derived bindings. Code-pointer witnesses pin retired code or dangle. Most-derived rebinding would change ordinary static lookup. |
| IF5 — value boundary | Managed-backed erasure only now; inline value components of a managed owner remain usable without boxing/copy-back. Standalone value types may match the method shape but cannot yet become runtime interface values. | Boxing on conversion plus aliasing on copy changes value semantics; copying boxes adds hidden allocation/failure. Borrowed stack erasure needs escape rules. Unboxed erased value storage needs dynamic root/layout/copy design. None is approved implicitly. |
| IF6 — capabilities/effects | Reuse inferred effects, with a fixed conservative summary **per interface requirement** across eligible bindings and applicable virtual targets in the complete project. Rebase hidden receiver paths to abstract interface-owner provenance. No new permission syntax or runtime tags. | Explicit readonly requirement syntax might improve precision but is new language design. Per-instance/runtime permissions violate P3/RV. Flow-dependent permissions are outside this phase. |
| IF7 — selection/construction | Prepare and root the actual selected receiver before arguments; borrow inline storage, snapshot managed components. Forbid interface conversion/dispatch of unfinished S1 views and aliases. Completed children may convert/call normally. Extend existing required-reference/default/publication rules to interface leaves. | Late projection violates U1–U7. Temporary static dispatch or erasure of unfinished self violates I6/S1/P4. |
| IF8 — metadata/reload | Extend the existing Runtime metadata admission and generation dispatch plans with immutable binding data; no object/interface-held JIT pointers. Requirement/binding/effect/layout changes require restart; compatible bodies retain whole-generation leases. | Separate interface runtime, mutable witness patch cells or object-held generation leases duplicate existing machinery or conflate code/data lifetime. |

The owner explicitly approved IF2 factory exposure, IF3 conversion boundaries, IF4
static-view-sensitive binding, IF5 deferred value erasure and IF6 conservative
readonly precision. That approval does not settle future value-interface ABI or boxing.

## Investigation baseline before Phase 21 implementation

| Existing component | Verified from implementation | Concrete Phase 21 gap |
| --- | --- | --- |
| Frontend (`src/frontend/compiler.cpp`, `inheritance.inc`) | One parser/semantic model; nominal owner imports; direct/static-ancestry precedence; accessible one-hop promotion; typed receiver paths. `Structure` is explicitly move-only after run #34. | No interface declaration/requirement AST, name category, structural proof or interface call expression. Do not encode requirements as ordinary function bodies. |
| Type/IR (`include/void/ir.hpp`, `src/ir/module.cpp`) | Owned nominal arena with weak edges; managed/value distinction; `upcast`, `virtual_call`, prefix/stage checks; independently validated virtual targets. | No erased-interface kind, witness binding, pair operations, abstract requirement signatures or preparation token. Current nullable/reference checks often explicitly test `managed_ref`. IR fields retain owners but not the frontend using marker; native imports are not the source access graph. |
| Effects (`reference_capabilities.hpp`, frontend/IR capability analysis) | Logical leaf origins, readonly profiles, retention/mutable requirements and virtual-family fixed point. | Interface receiver shapes differ between bindings; summaries need sound route rebasing and interface call edges, independently checked in IR. |
| LLVM (`src/backend/llvm/backend.cpp`) | One lowerer, field-path mapper, Frame/Slot homes, checked calls, target DataLayout/debug layout, generation-owned exact-key virtual switches. | No interface pair layout/root offsets or runtime-selected promoted receiver preparation. A virtual call's receiver has already been statically elaborated. |
| Runtime (`runtime.hpp`, `coordination.cpp`, `heap.hpp`, `abi.cpp`) | One heap/coordinator; immutable nominal data; transactional NativeSafe admission; guarded dynamic type queries; complete publication. `NominalDescriptor` contains identity/base/shape/numeric layout, no methods/code. | No stable binding record or pair validation. Existing nominal records are heap-layout records; do not fabricate a dummy allocatable class for a witness. |
| Analysis/reload/LSP (`frontend.hpp`, tooling/LSP, backend contracts) | Shared immutable successful analysis, original owners, implicit dependency edges, base/slot/override facts, exact compatibility and generation leases. | Interface requirements, conversion evidence, candidate dependencies and call navigation must be added to those same records/contracts. |

Reviewed the Bible/Strategy, DECISIONS/KNOWN_ISSUES/WORK_LOG and Phase 16–20 guides,
including C1 initialization, S1 views, P3/P4, RV1–RV7, composition and inheritance.
Older phase-specific exclusions in historical guides do not supersede implemented
Phase 20 behaviour. GC coordination/native-provider quiescence remain authoritative;
this proposal does not declare the experimental runtime production-ready.

## Source signatures and structural proof

Use the Bible's indentation and name-first parameters:

```swift
interface readable:
    func read() -> i32

interface damageable:
    func take_damage(amount: i32)
```

`public` (default), `internal` and file-private declarations follow current nominal
ownership/import semantics. Requirements inherit interface visibility; no separate
private/protected requirement namespace. No fields, initializer, receiver clause,
body, `virtual`/`override`, default parameter or overload inside an interface.
Use existing positional method arguments. Parameter names document/navigation-label
the contract, but matching is by order/type/const qualification, not spelling.
No-result requirements use the existing absent return annotation, not a new `void`.
Signature references resolve in the declaring interface module with existing access.
Imported interface names use existing `use`/alias/qualified-type rules.
Public requirement signatures must not expose less-visible types; this is part of
IF1, not authority to retrofit unrelated existing declarations. Requirements are
the only accessible members of an interface view: erasure does not expose concrete
fields, extra methods, lifecycle calls or a `super` receiver. Parse interface in its
declaration context without silently imposing a broader keyword-reservation policy.

Interfaces have nominal declaration identity: two identically shaped interfaces
are different types. **Conformance is structural, interface identity is nominal.**
No extra `implements` statement, opt-in declaration or implicit implementation body.

At a required conversion, for each requirement:

1. Use the ordinary member selector on the source's static concrete view and the
   converter's module. Real fields/methods on the ancestry precede using candidates.
2. A winning field is not a callable requirement; do not fall back to promotion.
   Missing, inaccessible or ambiguous selection fails at that conversion, not at
   an otherwise valid struct declaration. An unrelated bad conversion must not
   hide independent diagnostics.
3. Match exact declared argument count/order/types/nullability/const qualifications
   and result nominal type. No numeric promotion, overload search, covariance or
   signature-based choice between promoted routes. Inferred result capabilities
   are handled by the effect pass, not guessed from nominal matching alone.
4. Exclude the implicit receiver from explicit argument matching. Its actual
   concrete type must match the selected place/ancestor; it need not equal the
   interface name. Inline component receivers use their existing by-place contract.
5. Record the original member, access evidence, static view, physical path and
   direct symbol or virtual slot. Never synthesize source forwarding methods or
   relabel the method's module owner.

For example, `func clone() -> readable` requires a method declared to return
readable. A method declared to return its concrete class is not a covariant match;
its body can use the ordinary approved concrete-to-interface conversion if its
declared result is readable. Signature-first analysis handles recursive interface
reference signatures; weak arena edges prevent metadata cycles. Inline recursion
and physical-depth bounds remain unchanged.

Visibility proof is contextual. Existing selection filters inaccessible promoted
candidates; importing a different component can change selection/ambiguity. Capture
the proven route, not a later dynamic name lookup. Identical proofs deduplicate by
exact structural binding keys; different eligible routes remain distinct witnesses.
Checking only matching signatures must never resolve an ambiguous accessible name.
A private factory may intentionally export an already checked callable view; an
outside caller cannot create the same view by bypassing private/import checks.

## Conversion, identity and examples

Proposed working source after approval/implementation:

```swift
interface damageable:
    func take_damage(amount: i32)

struct enemy:
    health: i32 = 50

func (e enemy) take_damage(amount: i32):
    e.health -= amount

func attack(target: damageable):
    target.take_damage(10)

func entry(value: i32) -> i32:
    var e = enemy()
    var view: damageable = e
    var alias = view
    attack(alias)
    return e.health
```

Expected 40: no allocation/copy of enemy during conversion or interface assignment.
An explicit `e as damageable` expresses the same compile-time proof. A typed
assignment, argument or return inserts a visible-in-analysis checked structural
conversion, not a user-defined hidden conversion or a runtime search for methods.

| Source/target | Recommendation |
| --- | --- |
| Completed T → I / I? | Requires static T's accessible structural proof; preserve owner identity/authority. |
| T? → I? | Prove underlying T; null yields the canonical null pair. |
| T? → I | Unwrap to T with existing checked `as` first; no implicit nonnull strengthening. |
| I → same I?; I? → same I | Lift normally; unwrap with checked `as`, reporting null at source. |
| Contextual null → I? | Both pair fields zero; safe nullable default. |
| Distinct I → J, I → concrete T | Deferred even if shapes overlap; no silent width/downcast. |
| Construction view → I, standalone value → I | Diagnose the specific unsupported/unsafe boundary. |

A Base variable containing Derived must satisfy requirements through **Base's**
accessible method set. Derived-only methods do not make a statically nonconforming
Base convert. If Base qualifies, retain its selected nonvirtual/promoted route;
virtual slots still select the appropriate dynamic override. A Derived conversion
may legitimately select a different route. Interface assignment does not rebind it.

Same-interface equality ignores witness differences and compares complete owner
identity; either nullable operand may be null. Initially compare only the same
interface nominal type (or contextual null); compare concrete views using existing
concrete rules or explicitly convert both to one approved interface. Distinct
interface/concrete equality and `is` are separate future type-test design.
Interfaces have no constructor: nonnullable variables/fields require a valid view;
nullable ones default to null. There is no `I()` object, inferred allocation or
claim that storage placement determines source identity.

Proposed embedded value use, without boxing:

```swift
interface movable:
    func move(amount: i32)

value struct movement:
    x: i32
func (m movement) move(amount: i32):
    m.x += amount

struct actor:
    using motion: movement

func advance(target: movable):
    target.move(3)
```

`advance(actor(motion = movement()))` mutates that actor's physical motion storage.
`advance(movement())` diagnoses deferred standalone value erasure, even though its
method shape matches. Ordinary value copies and statically typed calls still work.

Rejected examples/diagnostic intent:

- Two accessible using components both offer read: conversion to readable is
  ambiguous, regardless of differing arguments/results. Explicitly qualify a
  component and convert that completed managed component, or keep a typed value
  call; never generate a guess. Merely declaring the owner remains legal.
- Direct `read: i32` plus promoted read(): direct field wins; not callable.
- Required `read() -> i32`, actual `read() -> f32`: exact-result mismatch, not cast.
- `const view = enemy() as damageable; var copy = view; copy.take_damage(1)`:
  readonly violation; copying/annotation does not restore mutable authority.
- Converting init's self/alias, passing it to an interface helper or returning it:
  prohibited unfinished identity/escape, even if all fields happen to be ready.
- Calling on I? without unwrap, storing readonly I into a mutable I field/formal,
  duplicate requirement, malformed/bodyful requirement or direct lifecycle call:
  ordinary precise source diagnostics, not raw LLVM errors.

## Why a binding witness is necessary

Two focused reproductions ran through the unchanged Phase 20 compiler on both
LLVM 19.1.7 SDKs, normal/O2 (eight executions). These are **not interface tests**.

1. Base embeds an inline meter with read(); Derived adds a real nonvirtual read().
   Base.read returns 7, Derived.read returns 20, even for the same object. The
   combined result is 720. Both choices are legal under static ancestry precedence.
   A dynamic-class-only interface lookup cannot preserve both converting views.
2. A managed wrapper promotes child.sample(); its argument replaces child with a
   new engine. The call observes the original selected child, then the owner holds
   the replacement. The combined result is 1190, not late-projection result 9190.

Recommend a private **two-pointer view** `{complete_object, binding_witness}`.
The witness is immutable runtime-owned data, not a vtable or a managed heap object.
It contains exact interface/static-view identities and original method routes,
signature/access/compatibility data; no callable addresses, LLVM/AST pointers,
generation-local indexes or code-generation lease. A witness is about binding,
not per-instance readonly permission. Target layout is LLVM DataLayout/C++ ABI
checked, not an arbitrary external C ABI commitment.

Extend the existing Runtime metadata snapshot/admission with nonallocatable witness
records. Validate/intern class and witness records as one atomic NativeSafe batch,
with all allocation/validation before publication and unchanged data on failure.
Do not masquerade witnesses as nominal heap classes, create a separate runtime
service or store `shared_ptr` with C++ destruction obligations in source payloads.
Views contain a raw immutable data pointer; the existing Runtime owns its lifetime.
Compatible admissions reuse exact witness identities. Catalog lifetime extends
through heap/handle/callback drain and is independent of ORC generation retirement.
Body-only reloads must not accumulate distinct metadata for identical bindings.

The generation maps validated exact witness keys to its shared dispatch plans.
It also verifies the complete object's dynamic type is compatible with the recorded
source view. For a nonvirtual route, call its selected original symbol; erasure does
not make that ordinary method virtual. For a virtual route, use the existing slot
planner/dispatcher on the selected receiver, including a managed component's own
dynamic identity. No object-held JIT pointers, mutable per-method patch cells,
hash-only identity comparisons or global AOT code registry.

Witness lookup must check registry membership before dereferencing an untrusted
pointer in internal negative tests. Unknown/incompatible binding or catalog gets
the existing checked failure before method access; no most-derived fallback.
Invalid GC root/state corruption retains existing invariant handling, not a new
recoverable policy. Canonical null is `{null, null}`; mixed/null-invalid pairs are
rejected. Ordinary source cannot manufacture metadata pointers.

## Receiver preparation and common dispatch

Conversion captures the **route**, not a particular component instance forever.
Each call evaluates its interface receiver once, then prepares that route before
evaluating ordinary arguments left to right:

1. Keep the complete owner in its existing root home; validate witness/domain.
2. Follow the verified physical path with the same field mapper used today.
   Snapshot/root a selected managed component. Borrow the selected inline place
   without copying it, retaining its actual heap owner separately.
3. Evaluate and root argument snapshots. Replacing a managed field cannot redirect
   the already selected target; replacing inline storage is observed through its
   existing selected address, as for current qualified/promoted calls.
4. Invoke the already prepared direct/virtual binding and transfer rooted result
   through existing checked-call/failure cleanup. A virtual implementation still
   follows Phase 20's exact slot contract.

A compiler-private prepared receiver/token is allowed only within its generation's
active call frame: no source storage, field, return, callback escape or constructor
argument. Its selector can be generation-local because it cannot outlive that
frame/lease; an interface view's witness cannot. Dominance, expected signature,
selected-owner liveness and no use-after-cleanup must be independently IR checked.
GC roots retain owner/component references; an interior address is never a root.

This is a generalization of existing receiver preparation and generation-owned
guarded dispatch, not an independently implemented interface dispatcher/type checker.
The shared dispatch plan describes original member, path and direct/virtual target;
frontend, independent IR validation and LLVM consume the same canonical plans.
Generated selection/adapter blocks use ordinary call/virtual-call lowering and
failure channel, not duplicate method bodies or a forwarding-method subsystem.

Publish the minimal logical lookup facts needed to verify a binding in the shared
IR: original declaration owners/access, source module/root/import edges, physical
fields' using markers and interface requirements. Extract a shared binding selector
from existing member-selection rules; source diagnostics wrap it and IR validation
rechecks its result. A route with the right types is insufficient if a direct field
blocks it, a component is not using, promotion is two hops, access is absent or an
accessible competing route makes it ambiguous. Do not trust a frontend boolean
"conforms" or perform an independent alternative name-resolution algorithm.
These logical facts do not change physical layout or add GC responsibilities.
The runtime checks metadata identity/admission/domain, not source import/type rules;
generation guards use the verified compiled path, not arbitrary runtime offsets.

## Readonly and effect inference

An interface value has one logical managed reference leaf: its complete owner.
The metadata pointer is neither authority nor another GC leaf. Nested carriers
retain RV leaf paths, const qualification and origin facts. Var copies permit
independent carrier storage changes, never mutable authority to readonly referents.
Existing mutable binding/field capacities and plain/const parameter rules remain.
Same-interface conversion/casts/null handling cannot launder authority.

Nominal matching precedes a monotone effect fixed point over ordinary methods,
virtual families, interface bindings and calls. Per requirement, union all eligible
binding effects in the complete validated project's module/access contexts,
deduplicating identical maps. Include possible virtual overrides, hidden managed
components, inline writes, transitive helpers, retention, mutable-boundary needs and
per-result-leaf provenance. Rebase concrete receiver/field origins onto abstract
interface-owner authority; explicit argument leaves keep their own origins.
Fresh independently allocated returned objects retain independent authority when
proven; potentially receiver-derived returns cannot become mutable through erasure.
IR recomputes these constraints rather than trusting frontend effect labels.

**Precision tradeoff:** an unrelated eligible implementation that mutates read()
can make read() unavailable through every readonly readable view in this project.
Summaries are per requirement, so a separate mutating move() does not automatically
poison a proven reader. This follows Phase 20's conservative family approach, but
the wider structural family is a new owner decision. Plain interface function
formals still require mutable leaves even if the current body does not mutate;
use existing `const target: readable` to accept readonly views.

An interface declaration need not already have a provider. Empty provider sets must
not vacuously certify readonly calls or invent mutable result authority: retain a
conservative unknown-call summary and guard any impossible runtime binding. Adding
a provider changes the interface universe/effect contract and requires reanalysis/
restart. Recursive call cycles likewise cannot certify safety from unfinished work.

Bindings are keyed by finite declarations/static views/resolved routes, not runtime
instances or permission masks. Compute eligible conformance maps without recursively
specializing bodies. Resource-limit diagnostics must bound witness/effect work and
preserve semantic depth/1 MiB checks; never truncate candidates to change ambiguity
or infer an optimistic effect from partial work. Implementation must document the
measured budget before acceptance, using the existing diagnostic/resource mechanism.

## Roots, construction and failure

Extend the existing nominal arena/Type/FieldType with an explicit interface-reference
category and requirement records; do not represent an interface as an empty value
struct or pretend it has a class base/layout. Interface names remain in the existing
resolver and successful-analysis ownership model. Refactor shared reference-category
checks deliberately, preserving legacy untyped internal IR proofs.

Each pair's object word is traced exactly, including nested value fields, heap
fields, parameters, locals, temporaries, prepared receivers and returns. Witness
data is not scanned. Use existing Frame/Slot homes and numeric Heap root offsets,
module-owned target layout and safepoints. Both pair fields are initialized before
the first parameter poll or successful publication; tuple stores have no intervening
safepoint. Zero whole homes before registration. Optimized code must retain actual
object words in escaped storage. No secondary root registry/statepoints, conservative
scan, volatile workaround or optimizer disabling.

Type classification must extend recursive nonnull validation to interface leaves,
including pair validity and witness/domain admission before complete-object commit.
Nullable interface fields default to canonical null; required interface fields have
no implicit object construction. S1 definite initialization covers direct required
interface fields; C1 still requires every value field valid before value init.
No hidden constructor allocation or unavailable nested value extension. Pure
all-null templates follow RV5; live witnesses/objects/conversions/init cannot appear
in pure declared field defaults or module constant evaluation. Runtime readonly
null views remain readonly despite zero bits.

Never erase unfinished self/aliases into interfaces, including through aggregates
or helpers. Completed child interface calls may run during init and participate in
the constructor dependency closure. One base-to-derived private stage and one P4
commit remain authoritative. Conversion uses already admitted metadata and complete
objects; no allocation or initializer rerun is inherent in interface assignment.
Arithmetic/argument/lookup/admission/collection/allocation failures skip later work,
preserve output/destination protection and unlink frames through the existing channel.
Already performed effects on complete objects are not rolled back. A failed/timed-out
rendezvous never sweeps. Native calls/callbacks retain current Attachment/Managed/
Native and provider-quiescence rules; no public interface FFI is added.

## Compatibility and tooling

Signature, visibility/import selection, static binding path/member, witness domain,
interface/heap layout, capability/effect profiles, conformance universe and constructor
dependency changes require restart. Compatible implementation bodies use the same
whole-generation reload rule; virtual mappings call the version owned by the current
lease. Object/witness reachability never retains a code generation. Old active calls
keep old prepared selectors and code; new calls bind the new generation only after
transactional metadata/code publication. AOT emits/adopts the same witness schemas
and guarded plans through the existing bootstrap and runtime-only linking.

Shared analysis records requirement declarations, interface types, selected binding
evidence and implicit candidate/route/override dependency edges. An interface call's
written method navigates its **requirement**, not a guessed runtime implementation.
A concrete written call retains existing original-owner navigation. Conversion
diagnostics relate requirements and candidate/embedding declarations; inherited and
promoted methods retain their true modules. Conformance dependencies invalidate on
ambiguity/access/signature/effect edits even without a direct written call.
LSP retains current stale-query suppression and owned snapshots; do not add a second
parser or completion/hover feature by implication. Contextual interface highlighting
can be added during implementation using existing colours and strict editor tests.
Debug pair/field metadata uses the same target layout without exporting metadata
pointers as an application API.

Pair size/alignment checks do not prove a C++ aggregate calling convention. Source
callers/callees use the same private LLVM ABI; the entry adapter remains i32(i32).
Native integration harnesses must use existing pointer/scalar test boundaries or
an explicitly checked private bridge, not assume `extern C` with a C++ two-pointer
struct matches LLVM's internal aggregate ABI on Windows or ARM64. An old holder
whose interface field contains an old witness is a useful cross-generation proof
through the existing managed-pointer harness boundary, without new public exports.

## Alternatives considered

| Approach | Assessment |
| --- | --- |
| Single object pointer plus global dynamic-class/interface map | Cheap but loses static-view/access binding and can redirect promoted/nonvirtual methods. Reject for the approved existing language. |
| Object pointer plus generation-owned vtable pointer | Conventional fast dispatch, but persistent fields/returns can outlive code. Pinning generations through values violates locked lifetime separation; mutable patching creates another reload mechanism. Reject. |
| Object pointer plus stable data witness, generation-owned guarded plans | Recommended. Two words, an exact binding lookup and prepared receiver path; correctness reuses current metadata/dispatch/root architecture. Optimizations require later evidence. |
| Generic specialization only | Useful future value path but not general runtime interface fields/returns/heterogeneous identity. Generics are not implemented; do not invent restricted borrowing or hidden unbounded specialization now. |
| Box every standalone value | Reuses Heap allocation but needs copy/alias/identity/hidden failure rules that are not approved. Defer, especially where specialization could avoid boxing. |
| Unboxed erased value buffer | Potential future genuine value semantics, but dynamic size/alignment/reference offsets, copy contracts and optimizer-safe roots need separate design. No conservative scanning fallback. |

## Implementation sequence after approval

1. Add declaration/type/requirement ownership and recovery. Header matching uses
   the existing resolver/member selector. Add contextual signatures/access/binding
   diagnostics before changing code generation; preserve move-only AST contracts.
2. Add canonical binding plans and conservative effect/capability fixed point;
   record shared analysis/navigation/dependencies and exact compatibility. Include
   static-view/import cases and bounded recursive signatures/calls.
3. Extend shared IR with interface pack/lift/unwrap/identity, prepared receiver and
   invoke operations/contracts. Independently prove pair types, binding signatures,
   routes, readonly/provenance, dominance/liveness and construction restrictions.
4. Extend the existing Runtime metadata transaction with stable witnesses and
   membership/domain validation; prove admission rollback, lifetime and lock/epoch
   rules before publishing executable source. No second allocator/registry of roots.
5. Lower pair storage, exact roots and prepared dispatch through the one LLVM path.
   Integrate existing virtual targets, receiver storage and checked failure handling;
   run normal/O2 JIT and separately linked AOT, not hand-built source substitutes.
6. Complete reload, LSP/editor/debug metadata, stress/resource/cross-platform tests
   and implementation documentation. No artificial partial constructor/boxing policy.

## Acceptance gates

Preserve all 51 suites, Windows AST/CRT corrections, LLVM 19.1.7 and seven CI jobs.
Implementation verification requires both installed/official SDKs across Debug,
Release and strict ASan/UBSan with leak detection, not just this investigation.

- Real source: direct/inherited/virtual/promoted requirements; mixed inline/managed
  components; cross-module private factories and missing owner imports; exact types,
  const formals, recursive nullable interface signatures and no-result/bool/numeric/
  value/reference/interface returns. Nonconforming static Base never gains Derived
  methods dynamically. Same object with distinct valid witnesses retains identity.
- Binding/ordering: Base/Derived route reproduction through actual interface values;
  replacement of managed component/owner during arguments; inline receiver reset;
  once-only receiver/argument evaluation, selected-owner retention and failure order.
- Diagnostics: malformed/duplicate/bodyful/lifecycle requirements, unknown types,
  missing/inaccessible/wrong members, signature/nullability/const mismatches,
  ambiguous promoted names with conflicting types, field precedence, unsafe views,
  unsupported value erasure/downcasts/width conversions and nullable dereference.
- P3/RV: aliases, casts, inferred/annotated locals, mutable binding/heap field stores,
  nested carrier copies, const/plain arguments, returned origins, constructors and
  transitive helpers. An interface witness is never a runtime permission tag.
- Precise GC: sole interface root after concrete factory returns; interface-only
  carrier roots; cycles through interface fields; metadata pointers never scanned;
  allocation/loops/calls/aggregate argument/return/failure paths in normal/O2 JIT
  and real linked AOT. No unrelated host handles masking missing roots.
- IR negatives independent of frontend: forged interface/witness/domain/route/
  signatures/access evidence, non-using/two-hop routes, ignored direct precedence
  or competing candidates, mismatched or null-invalid pairs, metadata-as-root, noncoordinated
  managed execution, readonly laundering, skipped receiver preparation, token
  escape/use-after-cleanup, unavailable interface fields and unfinished parent
  erasure/publication. Preserve legacy fatal root/state corruption contracts.
- Runtime/reload: deterministic collection overlap during prepared calls; admission
  conflict/OOM rollback; timeout/no-sweep recovery; old interface objects/witnesses
  surviving allocating-generation retirement; old active calls versus new bodies;
  constructor target changes/restart and new-code dispatch on old data in AOT.
- Tooling/resources: imported requirement/implementation navigation, ambiguity and
  readonly diagnostics, unsaved invalid/repaired signatures and stale-query rejection;
  bounded witness/effect recursion and 1 MiB semantic checks; real editor process-kill,
  restart/deactivation tests and VSIX packaging. Preserve callback/provider stress.
- Hosted gate: Ubuntu Debug/Release/Sanitizers, Windows Debug/Release, macOS ARM64
  Debug/Release. Linux successes cannot establish the other targets or public ABI.

## Investigation evidence and stop point

Only focused unchanged baselines ran: five suites on each Debug SDK (inheritance,
composition, reference values, managed initialization and native LSP protocol),
5/5 installed in 7.99 s and 5/5 official in 7.53 s. The two reproductions above
passed normal/O2 on both SDKs: eight executions, results 720 and 1190. Local logs
and temporary source are under build/phase21-investigation; they are not interface
implementation proofs. No full matrix, sanitizer/editor rerun or hosted claim was
necessary for this documentation-only investigation.

Stop for owner review of IF1–IF8. No compiler/runtime/editor/test/build/CI changes,
commits, pushes, branches or remote modifications are authorized by this document.
