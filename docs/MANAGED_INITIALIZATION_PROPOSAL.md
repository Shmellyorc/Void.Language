# Phase 17: managed custom initialization proposal

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

**Status: owner approved S1 + MI1–MI7; implemented locally.**
The owner declares Phase 16 locked and reports Actions run #28 passed all seven
jobs, 35 suites per configuration. Approval explicitly includes replacement of
the obsolete valid-managed-init deferral assertion. The sections below retain
the architectural alternatives and approval rationale; their earlier requests
are fulfilled by this approval. [MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md)
describes implementation; WORK_LOG.md records actual validation and hosted gates.
No P3/P4 relaxation or new source feature outside the approved boundary is implied.

## 1. Recommendation and owner decisions

Recommend **S1: staged construction views, followed by the existing complete-object
commit**. Evaluate arguments, prepare private field storage, run `init` through a
compiler-checked construction receiver, then create the managed object with
`gc_construct` / `Runtime::construct_into`. Until commit there is no parent managed
reference or managed identity. Precise roots contain only null or already complete
objects referenced by the staged fields and arguments.

Keep the Bible's `func (n node) init(...)` / `node(...)` surface, one owner-module
initializer, exact typing, optional pure defaults and once-only invocation. Infer
safe helper use without new ownership syntax. This is a bounded construction
capability, not an ordinary reference disguised as stack storage.

Approval is requested for the **coherent S1 package**, particularly MI1–MI3 and MI6:

| Decision | Recommended contract | Alternative / consequence |
| --- | --- | --- |
| MI1: identity/publication | Receiver denotes private initialization storage; real reference identity starts only after successful init and commit. No precommit identity comparison, self-link or whole-self escape | A real privately rooted identity requires explicit P4 relaxation; symbolic self-links/group commit require a larger design |
| MI2: receiver/helper capability | Permit field operations, local construction-view aliases and proven nonescaping source helper/method calls. No whole-view returns, casts/injection to references, retention, constructor arguments, identity use or whole-view rebinding in this slice | Restrict all helpers/aliases for less work but poor ergonomics; broader borrowed returns/rebinding need more capability-flow machinery |
| MI3: required references | Direct non-nullable reference fields may start unavailable in staging; require first assignment before read and on every successful init exit. Before passing a whole construction view to a helper/method, all required fields must be ready | Requiring every field to have a default makes many useful managed init declarations impossible; automatic child allocation changes P16 defaults; field-sensitive helper initialization summaries are a later expansion |
| MI4: binding/default construction | Init-bearing managed types bind constructor arguments exclusively to initializer parameters using C1 order/default rules. No-init named-field construction stays unchanged. Managed `var n: node` still requires an explicit initializer; `var n: node?` remains null | Implicit `node()` for uninitialized non-nullable bindings would change P16 and introduce hidden allocation; it is not proposed |
| MI5: nesting/readonly | Keep established value-field default construction, nullable null defaults and P3 views. Explicit managed children finish before use; no implicit recursive managed allocation. Fresh construction views are mutable until result qualification | Extending definite initialization to required value children or adding readonly heap fields is separate work |
| MI6: failure/effects | Body completes before final parent allocation. On body/commit failure publish no parent and preserve the assignment destination reference; earlier effects on existing complete objects are not rolled back | Transactional side effects, pre-reserved identity, guaranteed allocation before body or implicit retries require additional contracts |
| MI7: compatibility/tests | Extend C1's conservative init/helper/default reload contract and analysis/tooling metadata. Replace only the obsolete valid-managed-init deferral assertion with positive execution coverage after approval | No migration/reinitialization, automatic restart or weakened lifecycle/failure assertions |

MI3 is a deliberate managed-only extension, **not** a retroactive change to Phase
14's requirement that value fields be constructed before the initializer body.
MI1/MI2 are real restrictions on whole-self use during construction. They are not
permanent bans on future self-link or borrowed-return support. If ordinary reference
identity/self-linking inside init is required now, do not implement S1 as a hidden
substitute: select and approve a different publication architecture first.

## 2. Verified baseline and concrete gaps

The Language Bible §7 requires valid initialization for non-nullable references;
§16 establishes receiver init, valid fields before use, once-only successful
construction and one initializer. Constructor failure rewriting remains open.
Phase 14 C1 resolves arguments/visibility/order/reload for **value** structs;
Phase 16 P3/P4 resolves readonly and complete publication for the managed subset.
None authorizes using an incomplete managed reference as an ordinary init receiver.
Inheritance, `super.init`, strings and fallible result examples in the Bible are
broader design examples, not current facilities to assume available.

| Inspected implementation | What works now | Phase 17 gap |
| --- | --- | --- |
| `src/frontend/compiler.cpp`: `call_arguments`, `construction_fields`, `construction`, `initializer_fields` | One initializer model, C1 parameter binding/default checking, source-order snapshots and recursive value-field plans | Managed init declarations explicitly diagnose deferred; availability masks and construction views do not exist |
| Same file: `Lowering::expression`, place selection/read/write | Value construction prepares a local aggregate then calls marked init through a Place; managed construction snapshots fields then immediately emits gc_construct | Managed branch does not run init; an ordinary managed local holds a reference, not field storage |
| `include/void/ir.hpp`, `src/ir/module.cpp` | Shared nominal family/nullable/access model, Places, initialization call marker, cycle-safe type ownership, managed ops and IR validation | No construction-storage/passing category, scope token or per-field readiness proof. Managed nominals are rightly rejected as source inline aggregate values |
| `src/frontend/capabilities.inc` | Readonly, mutation, retention and returned-alias origins across source calls | Field reads preserve receiver origins for deep readonly; these summaries cannot distinguish escaping the parent from safely returning an already complete child. They also do not prohibit identity use or receiver rebinding |
| `src/backend/llvm/backend.cpp` | Managed receivers pass rooted pointers; value receivers alias Places; root offsets flatten actual storage; shared O2/JIT/AOT and module-owned DataLayout | A staged receiver needs payload field storage/root offsets, not a managed-pointer local or an unrooted byte alloca |
| `src/runtime/coordination.cpp`: `Runtime::construct_into` | Validate precise output root/references; collect/poll before commit lock; allocate/copy; publish output last | Already suitable as final commit. It must not invoke init, acquire user callbacks under its lock or receive a partial-parent reference |
| `src/runtime/heap.cpp`, `src/runtime/abi.cpp` | Fallible allocation/map work precedes publication; checked failures, bootstrap, attachment, frame cleanup | No pending-object registry, construction heap or identity reservation exists; none is needed for S1 |
| Analysis/tooling/reload and `tests/managed`, `tests/frontend/initialization.cpp` | Constructor dependencies, readonly effects, live-object reload, source/AOT roots, failure injection and layout retirement | Need construction-capability/readiness metadata, call-site diagnostics and actual managed init tests |

A nominal managed Type cannot simply be changed to aggregate to reuse value init:
that defeats IR's family invariant and loses managed identity/capability semantics.
Nor can one call existing ordinary managed methods with a pointer to staging bytes:
the present ABI, field lowering and root interpretation assume a complete heap object.

Focused unchanged-binary baseline on both installed and complete official LLVM
19.1.7 Debug trees passed 10/10 selected suites each. WORK_LOG.md records the exact
selection and timings. This proves current foundations still work, not S1's proposed
construction proof. No Phase 17 sanitizer/platform implementation claim is made.

## 3. Architectural alternatives

| Alternative | Identity, roots and failure behavior | Assessment |
| --- | --- | --- |
| **S1: staged field storage + checked construction views** | Parent does not exist during init; complete child roots survive calls/GC. Final allocation can fail after body effects. Self identity/escape unavailable | Recommended: preserves literal P4 and reuses the current commit/runtime, with explicit capability restrictions and shared lowering work |
| **H1: allocate a real object with private construction roots** | Stable self supports ordinary pointer identity and potentially self-links; root scanner sees an unfinished object. Needs construction-state, escape checking, valid reference slots and failure cleanup | Coherent only with owner-approved P4 revision permitting private incomplete-object roots. Private is not the same as complete; not authorized by P16 |
| **R1: symbolic construction identities/relocations and atomic graph commit** | Stage self/child-to-parent links symbolically, allocate an entire pending graph, patch links and publish after all allocations succeed | Potential strict-P4 solution for self-cycles, but requires symbolic-reference operations, group readiness, rollback and batch publication. Not a small extension of construct_into; defer unless self-links are essential now |
| Allocate safe defaults, then call init on the object | Fields may be memory-safe, but custom construction has not completed; GC roots or aliases can expose it before body success | Does not satisfy P4 by renaming zero/default-filled objects complete |
| Disable collection, hold the heap lock or force all initializer calls inline | Avoids some rendezvous points but not escape/failure, and unbounded/nested init can allocate, loop or call other code | Reject: conflicts with automatic GC, lock/state invariants and O0 behavior |
| Pending heap/pinning list outside normal roots | Keeps unfinished objects alive through a second retention path while children are separately tracked | An incomplete-object retention loophole, not preservation of literal P4; adds another lifetime subsystem |

P4 forbids a **real incomplete managed reference** from entering roots/aliases. It
does not prevent private field storage whose pointer leaves refer only to completed
objects. S1 relies on that distinction explicitly; it does not promise unrestricted
ordinary-reference self semantics. R1 shows why deferred identity is not a proof
that all richer self semantics are impossible, only that they require more design.

## 4. Source contract and evaluation order

Illustrative **proposed**, currently unsupported managed init syntax:

```swift
struct node:
    value: i32

struct account:
    owner: node
    amount: i32

func (a account) normalize():
    if a.amount < 0:
        a.amount = 0

func amount_of(const a: account) -> i32:
    return a.amount

func (a account) init(owner: node, amount: i32 = 0):
    a.owner = owner
    a.amount = amount
    var local_view = a
    local_view.normalize()
    a.amount = amount_of(a)

# account(existing_node, amount = expression) constructs one completed object.
```

An initializer has typed parameters, no result annotation and one owning module.
Empty return or reaching the end succeeds only with every required field ready.
Visibility remains public/internal/file-private in addition to type visibility;
private initializers can be exposed through owner factories. Direct `.init(...)`
and type-qualified lifecycle invocation remain rejected, including inside init.
An unrelated ordinary free function named init keeps its existing meaning.

On a type with init, arguments bind **only** to initializer parameters. Reuse C1:
positional prefix then named arguments; reject positional after named, unknown
labels, duplicate bindings, missing/excess arguments and mismatched exact types.
Required parameters precede defaulted ones. Defaults are typed pure compile-time
expressions in the owner module, with the same numeric rounding/conversion rules.
No implicit promotions, runtime default expressions or overload resolution added.

Construction order:

1. Evaluate supplied arguments once, left-to-right in written order, into rooted
   snapshots. Reordering parameter binding must not reorder evaluation. A failure
   stops before field construction/body/commit; do not touch the destination.
2. Apply omitted pure parameter defaults. Zero all physical reference leaves of
   fresh staging storage before publishing its stack root description.
3. In field declaration order, establish pure/safe defaults and finish nested
   value construction, including existing custom value init. A required value
   child without an available default constructor remains a diagnostic as in C1.
   Nullable references start as null; direct required managed references are
   unavailable under MI3, not source-visible nulls. No managed child is implicitly
   allocated, even when its initializer has optional parameters.
4. Execute the custom parent initializer exactly once through a construction view.
   Every field read and every normal exit obeys MI3. Calls/loops keep ordinary
   coordination and safepoints. Init may explicitly construct complete children.
5. After successful body completion and full readiness, snapshot final fields and
   use the existing complete-object commit. Only success produces a normal managed
   reference and can overwrite the destination or apply its final const view.

Copies, aliases, ordinary parameters/returns and field assignment never rerun init.
Assigning a completed child replaces the reference; it does not reinitialize it.
The parent initializer cannot be invoked twice or delegated as an ordinary method.
There is no body retry on commit failure. A failed attempt may already have run its
body; exactly once means each successful constructor executes its initializer once,
not that every executed body guarantees successful allocation/publication.

`var n: node` remains invalid without an explicit reference initializer; `node()`
executes custom init if all arguments may be omitted. `var n: node?` starts null
and executes no init. Value `var p: point` retains its existing C1 default-construction
behavior. No-init managed types retain P16 named fields and required-reference checks.
Compile-time constants/declared field defaults cannot allocate managed objects or
execute custom init; nullable null constants/defaults remain pure. This introduces
no runtime module initialization or constructor result rewriting.

## 5. Construction-view and helper rules

A construction view has the existing nominal type plus a compiler-only **place
capability and construction-scope origin**. It is not a source managed-reference
value, and no pointer to the future parent exists. The source receiver spelling and
normal type annotations do not change; developers do not write borrow/lifetime tags.

| Use of a construction view `n` | S1 behavior |
| --- | --- |
| Read/write ready fields; compound arithmetic; if/for; nested value receiver methods | Allowed with existing typing/readonly rules and readiness checks |
| `var alias = n`, `const view = n`; access through these local aliases | Allowed as same-stage views, not independent values or managed references. Const remains deep readonly; aliases cannot escape their construction scope |
| Rebind `n` or one of its construction aliases to another reference/view | Reject in this slice; no hidden copy-commit or replacement of the intended object. Ordinary methods on completed references retain P16 local-rebinding semantics |
| Pass `n` to a known source method/helper | Allowed only when all required fields are ready and a construction-compatible body/call-chain proof exists for that parameter |
| Helper reads/returns/stores `n.child` where child is an already complete object | Allowed under P3; a child reference is not the incomplete parent. Numeric/reference-free value copies are ordinary independent values |
| Whole-view return, including helper identity-return even if caller discards it | Reject; no construction-view result ABI in this slice |
| `n == n`, comparison to another reference, checked cast/nullable injection of whole `n` | Reject: these require an ordinary reference/identity; field-reference comparisons/unwraps retain normal semantics |
| `n.next = n`; storing `n` in any existing/new object, callback, handle or collection | Reject; neither a self-reference nor an outward reference to the unfinished parent exists |
| Pass whole `n` as a constructor argument | Reject in this slice; constructor arguments are completed values. Nested construction from scalar/complete child fields works |
| Pass whole view to opaque/native/indirect calls or capture it | Reject unless a future approved capability-aware boundary proves safety; no source FFI/callback/closure features are added |

Fully ready **fields** do not mean a completed **constructor**. Even after the
readiness barrier, helpers must remain within the construction-view protocol until
the initializer returns and commit succeeds. They cannot make it publishable early.

Construction-compatible helpers may mutate staged fields, call other compatible
helpers (including recursive direct call graphs), allocate ordinary completed
objects and return scalars/reference-free values or completed child references.
Normal source helpers/methods need no new annotation. A helper valid for ordinary
references may be ineligible during construction because it compares identity,
returns its receiver, rebinds the borrowed parameter or retains the parent. Report
at the constructor-context call with a related location at the prohibited use;
do not globally outlaw the ordinary helper or silently choose a heap path.

The construction proof must distinguish **whole-stage provenance** from existing
readonly origins. Reading a managed child removes whole-stage provenance but keeps
readonly qualification; projecting an inline field retains its place owner, while
copying a reference-free value removes the borrow. Existing retained/returned-origin
sets alone are too coarse to prove this. Extend the same typed-AST capability/effect
analysis with the required construction-use facts; no second nominal type checker.
Unknown effects fail closed. Local aliases preserve the stage origin and capability; an explicit nominal type
annotation cannot turn the view into an ordinary reference.

Body variants are private compiler instantiations for reachable parameter/receiver
passing categories (normal reference versus construction place), not source overloads.
Several arguments may alias the same staging place. Use one validated source body,
one shared IR/projector and one LLVM lowerer; do not hand-write constructor versions,
require optimization/inlining, or introduce a runtime tagged-reference representation.
Key contexts by the declaration and passing-category mask, not dynamic construction
instances. Compute compatible contexts to a finite fixed point, handle recursive SCCs,
materialize only reachable variants and diagnose bounded analysis/instantiation
resource exhaustion. Returning a construction borrow and mixed borrow/reference
rebinding are explicitly deferred, rather than optimized away by assumption.

## 6. Field availability and readonly

At body entry numeric/bool, nullable reference and successfully default-constructed
value fields are ready. Direct non-nullable reference fields without a pure valid
value are not. Their physical null slots exist only for safe root scanning.

Track definite readiness for these required fields over the existing CFG: a typed
whole-field assignment from a completed non-nullable reference marks ready; reads,
compound assignments and dereferenced subfield writes require readiness first.
Branch joins intersect readiness sets. Zero-trip loops cannot establish a guaranteed
assignment; break/continue/early-return paths need their actual flow. Check every
successful init exit. Arithmetic/conversion/allocation failure exits need not finish
fields, but must use existing cleanup and never commit. No unchecked LLVM load of
an unavailable field or poison/undef stand-in is permitted.

Creation of a local construction alias does not read fields. Its direct field
assignment/read updates/tests the same stage readiness. Calls using a whole view
require **all required fields ready**, even if a callee apparently needs only one.
Thus Phase 17 does not need field-sensitive interprocedural definite-write summaries:
a helper cannot be relied on to fill the first required field. After the barrier,
compatible helper writes preserve typed validity. More permissive readiness-aware
helpers can follow deliberately; this conservative boundary needs MI3 approval.

A const destination does not make fresh init storage readonly; result qualification
occurs only after commit, as for value construction. Const construction aliases and
const helper parameters remain readonly during the body, however. Readonly reference
arguments cannot be stored into mutable fields or laundered through casts/returns.
Independent mutable aliases to completed children remain valid. No object freeze,
readonly-to-mutable exception, readonly heap field or general borrowing syntax added.

## 7. IR, LLVM, root and commit invariants

Extend Place/storage/passing metadata with a construction scope and nominal payload
layout. Do not make managed nominals source aggregate values or represent a staging
pointer as managed_ref. Ordinary reference ABI and the existing value Place path
remain intact. Generalize field projection over heap, inline and staging places;
keep selection-before-arguments/RHS, selected-owner lifetime and unrelated-field
mutation behavior from P7. Crossing a staged reference field selects a completed
heap owner and snapshots/roots it normally before subsequent effects.

IR validation must independently enforce:

- Staging storage is created/reset in a valid scope; all pointer leaves are null
  before first frame publication and valid complete references at safepoints.
- Reads require readiness; no whole-stage managed load, identity operation, heap
  store, return or non-compatible call can consume a construction place.
- Construction aliases cannot outlive the scope or become ordinary reference slots.
  Calls use the correct private passing category, including multiple aliases.
- Only constructor-marked invocation can execute init on fresh staging storage;
  successful init proves complete fields; commit follows it once. Failure paths
  cannot commit. Reused loop storage starts a fresh construction with reset state.
- Ordinary legacy allocation proofs remain internal and cannot replace this path.

Shared LLVM lowering maps a construction parameter to payload storage, rather than
the existing pointer local. Its root description flattens **reference fields of that
payload**, not a single pointer to the staging bytes. Register actual mutable field
storage through the existing Frame/Slot ABI; aliases must not root obsolete copies.
Root entries may refer to stack storage, but every non-null pointer leaf denotes a
complete existing heap object. The future parent is never among those leaves.
Arguments, nested expression temporaries and completed child returns retain the
existing root/handoff rules across unoptimized and O2 code.

Use existing LLVM 19.1.7 allocas, target layout, opaque runtime frame calls and shared
optimization pipeline. Preserve module-owned DataLayout/debug layout fixes, ABI
size/offset checks, alignment and resource limits. Do not add statepoints/stack-map
machinery, GC suppression, blanket volatile, false memory attributes, early lifetime
ends, an all-inlining requirement or another root registry.

After init, the existing gc_construct final-field operands can be loaded from
proven-ready staging storage and committed unchanged. Its LLVM payload copy is
acceptable initially; a new commit opcode/allocator is not required just to remove
a copy. Stage/argument roots remain valid through any collection in construct_into.
Under the existing lock, Heap completes fallible allocation/map work, copies the
finished payload and publishes the result root last. No user code/safepoint/throwing
bookkeeping occurs within that final publication interval. A future optimization
must prove the same contract rather than create a second construction path.

## 8. Failure, coordination, native boundaries and code lifetime

On argument, nested value init, explicit child construction, body arithmetic/cast,
collection/rendezvous or final allocation failure: do not run later steps, publish
a parent, overwrite the destination reference or execute the body again. Existing
checked RuntimeFailure/source traces and normal/failure frame unlinking apply.
No C++ exception crosses generated code. Staging roots leave through normal cleanup;
unescaped completed children become collectible. No partially constructed parent
needs heap rollback because it was never allocated before commit.

Effects on already complete objects and logging/scalar data are not transactional.
For example, an init may mutate a supplied mutable child, then fail parent commit;
that child mutation remains. Preserving a destination reference does not undo
mutations performed through an argument alias to its old object. A completed child
published elsewhere remains a valid completed object even if parent construction
fails. No incomplete parent identity or staging address may accompany those effects.
Future fallible constructors/resource cleanup require a new approved contract; no
result, exceptions, destructor, finalizer, deinit or automatic rollback added here.

All generated constructor/helper variants participate in normal coordination,
including numeric loops/recursion. Never park while holding a collector-required
lock. Automatic budgets may collect during nested calls or before parent commit;
failed/timed-out rendezvous never sweeps and follows the existing failure protocol.
No collector lock is held while running initializer/user code.

Native/callback boundaries can only receive ordinary completed values under current
binding rules, never construction views. Existing provider unregister/quiescence,
admission, NativeSafe freezing and generation leases remain authoritative. A callback
cannot gain a staging reference from a root registration or helper ABI. This does not
claim safety against malicious native raw-memory access; no new such source access
is introduced. S1 requires no provider-specific teardown assumptions or runtime
construction callback mechanism.

Runtime layouts and GC reachability remain independent of ORC generations. Keep the
current runtime owner across compatible reload. All active init/helper variants retain
the same generation lease; successful publication does not release active code early.
AOT uses the same IR/lowering and existing bootstrap/runtime-only link dependency.
No new entry signature, managed public C ABI, code-bearing heap descriptor or runtime
choice per constructor is introduced.

## 9. Analysis, diagnostics and reload

Expose initializer identity/visibility/parameters/default ranges and written/implicit
constructor references through the existing validated model. Add construction-use
summaries and required-field/readiness facts to that model and conservative tooling
invalidation; do not keep a separate LSP-only checker. Source queries retain their
revision/invalid-snapshot rules. Private implementation variants navigate/report the
original source declaration, not synthetic names.

Retain C1 conservative compatibility for initializer presence, signature labels/order/
exact types/default syntax, field layouts/defaults, owner/visibility, init body and
transitive helper/constructor dependencies. Include construction-call eligibility
and readonly effects. Adding/removing init changes the parameter-versus-field binding
contract and requires restart. Changed helper bodies in the initialization closure
require restart even if ordinary ABI is unchanged. Unrelated compatible bodies may
reload. No automatic restart, existing-object migration or rerunning init occurs.

Diagnostics must distinguish unavailable fields from forbidden self escape, unsafe
helper use, readonly failure, bad constructor arguments and ordinary unknown names.
Examples: "Field 'owner' must be initialized before it is read"; "Initializer can
complete without setting 'owner'"; "Cannot publish the construction receiver before
initialization completes"; "This helper requires reference identity during init".
Attach field/parameter/prohibited-helper locations; suggest explicit field assignment,
passing a completed child/scalar or performing self-linking after successful construction.
No raw LLVM errors or speculative fixes such as copying a readonly reference to var.

## 10. Implementation boundary and sequence after approval

1. Record approval and source contracts. Reuse initializer parsing/binding/purity/
   ownership resolution; add construction view and required-reference availability
   to the existing semantic/capability model. Diagnose invalid uses before IR exists.
2. Prove finite helper-context compatibility and readonly propagation, including
   aliases, recursion/imports and distinguishing child references from whole stage.
   Keep the all-fields-ready call barrier; do not add speculative escape optimizations.
3. Extend shared IR Places/passing/storage/readiness verification and constructor
   lifecycle. Lower the same checked bodies to normal/reference or construction
   contexts with one projector/backend; preserve the value path.
4. Implement staged roots and init-before-gc_construct through shared LLVM. Reuse
   Runtime::construct_into, Heap, bootstrap, failures and leases; only change runtime
   code if an evidenced invariant requires it, not to permit partial parent roots.
5. Integrate analysis/LSP/reload contracts and real source examples, then execute
   the acceptance gates below. Update INITIALIZATION/MANAGED_STRUCTS/DECISIONS/
   KNOWN_ISSUES and a managed initialization guide only for approved implemented work.

Deferred: self-links/symbolic references/group commit; borrowed returns and rebinding;
helper-assisted first initialization; required-value-field definite initialization;
reference-containing value structs; runtime field/parameter defaults; ordinary named/
default calls; managed FFI/closures; inheritance/interfaces/generics; strings/collections;
async/source threads; weak references/finalizers/deinit; fallible result constructors,
resource rollback, production allocator changes and new entry/export policy.

## 11. Verification and approval gates

Preserve all 35 suites and every applicable safety assertion. The current
`tests/managed/tests.cpp` diagnostic rejecting `func (n node) init(): n.value = 1`
solely as deferred becomes obsolete if S1 is approved. Request explicit authorization
to replace **that assertion** with positive once-only execution coverage. Keep direct
.init rejection, bad result/lifecycle/visibility, readonly, failure, output protection
and all value initializer/GC/coordination tests; add suites/assertions as needed.
No test changes are made in this design phase.

| Gate | Required real-source evidence, normal and O2 JIT plus separately linked AOT |
| --- | --- |
| Constructor surface | Typed positional/named/default arguments and exact source order; missing/extra/duplicate/unknown/wrong types; initializer visibility/imports; parameter-only binding; no-init named fields unchanged |
| Field/body order | All ten numeric types/bool/nested value fields; value child init before parent body; explicit managed children; optional refs null; no unintended child/default allocation; body once; copies/aliases never rerun it |
| Required fields | Valid first assignment, alias writes, branch intersections, zero-trip loops, break/continue/early returns, read/subfield/compound access before assignment, missing normal-exit initialization and full-readiness helper barrier |
| Construction helpers | Mutable/readonly local views; safe imported/recursive helpers and multiple aliases; completed child return/retention; unsafe identity/whole-self returns/rebinding/casts/nullable injection/store/self-link/constructor/native/callback escape rejection with related spans |
| P3 | Const destination init succeeds then readonly; const borrow cannot mutate; readonly argument cannot be retained; casts/helper returns cannot thaw views; independent mutable child aliases and reference-free copies still work |
| Precise roots/P4 | Force collection at argument/default/body/helper/loop and commit boundaries. Observe registered pointer leaves are null/complete heap refs; parent identity/allocation appears only after successful body; current stage field writes, child temporaries and return/argument handoffs survive real O2 collection |
| Failure/publication | Inject body/default/argument/nested child failures and commit OOM/descriptor/map/collection failures; no parent/result publication or destination replacement; balanced frames/threads; no sweep after timeout; recovery succeeds; distinguish retained prior side effects from rollback |
| Real execution boundary | Actual CLI-emitted bootstrap executables and private compiled-source runtime harnesses; runtime-only native link, startup/shutdown failures, live completed objects outliving code retirement |
| Compatibility/tooling | Added/removed/changed init/defaults/helper safety/body/layout/readonly require restart; unrelated body reload succeeds; live old objects unchanged; constructor/field/method definition/references and unsaved diagnostic repair; no stale queries |
| Platforms/resources | Both SDKs' Debug/Release/strict sanitizers, LLVM 19.1.7/RTTI policy and leak detection; deterministic repeated GC overlap/stress, 1 MiB semantic checks, retired-context/mixed-layout regressions; editor unit/real integration/VSIX |

Extend the existing test-only ABI observation/failure probes where needed, forwarding
to the actual runtime. Capturing Frame/Slot registrations can inspect actual root
leaves at controlled polls, without adding a production stage heap or suppressing GC.
Do not reconstruct source IR or use only output numbers as proof of root/publication
safety. Pauses must overlap deterministically; no sleeps/iteration inflation as proof.

Hosted acceptance remains all seven runners: Ubuntu Debug/Release/Sanitizers,
Windows Debug/Release and macOS ARM64 Debug/Release, each with the full inventory;
all three Debug editor/package runs and Linux small-stack regression remain. Preserve
LLVM 19.1.7, strict ASan/UBSan/leaks, RTTI correction, the 90-minute timeout and current
workflow. Linux evidence cannot certify Windows ABI/linkage or ARM64 layout/debug/O2.

**S1 and MI1–MI7 are approved.** During
implementation, stop again if the proof needs an incomplete parent root, construction
view escape, invalid field read, unsafe opaque helper fallback, lost readonly
capability, unbounded specialization, a second compiler/runtime or weakened GC/code
lease contract. Do not resolve such a contradiction by silently selecting H1/R1.


## 12. Implementation record

The existing typed-AST capability pass is followed by a bounded construction-use/
readiness worklist. Shared validated declarations retain context masks and required
field names. Private IR construction capability, storage origins and stage_begin
express the same places; an independent CFG proof validates their lifecycle.
Shared LLVM lowering roots actual staged field storage and commits through the
unchanged Runtime::construct_into only after the marked initializer succeeds.
Helper variants are finite declaration/mask contexts (1024/project), reuse normal
lowering and preserve original source/debug names. No runtime implementation or
seven-runner workflow changes were needed. Four additive source/JIT/AOT/CLI suites
bring the inventory to 39. The only replaced historical assertion is the explicitly
approved valid-init feature-deferral rejection; all other lifecycle/readiness/
readonly/failure/native/GC assertions remain. Implementation guide and work log
record current limitations and actual acceptance evidence.
