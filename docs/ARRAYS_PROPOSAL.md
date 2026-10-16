# Phase 23 proposal: typed arrays, indexing and iteration

**Current status, 15 October 2026: owner confirms CI #49 passed all seven jobs,
62/62 suites, locking Stage 2B at `4d25f4f`. Stage 1 was locked by CI #47 and Part
5A by CI #45. Owner/wrench authorize Stage 2C only. Section 26 records the private
rooted byte-buffer implementation and local evidence. Native source arrays,
reference-bearing population, leased/deep copying and production certification
remain disabled. The superseding large-array decision below remains in force.**

> **SUPERSEDING OWNER DECISION: large arrays are a language requirement.**
> Neither `[T, n]` nor `[T]` may have an arbitrary 64 KiB source-level ceiling.
> Valid large-array programs, including megabytes of JSON, text, image and asset
> data, must not be universally rejected by a 65,536-byte per-function policy.
> That threshold may remain in research fixtures, stack investigations, diagnostics
> and internal verification experiments. It is not an approved production policy.
> Large fixed arrays retain value/snapshot semantics. Investigate safe storage,
> including the existing coordinated GC heap, without a second memory manager.
> No alternative representation, changed numerical ceiling or production policy
> reactivation is approved by this documentation correction.

The separate 65,536 expanded-layout guard imposed a source-visible element/shape
ceiling in the Section 21 snapshot. Stage 1 replaces multiplicity accounting on
compact reference-free paths (Section 22). Internal analysis-work protection must
not silently recreate the rejected size limit.
A successful build must include every required function. Unsupported operations or
compilation failures must fail the whole build with a clear diagnostic; no omitted
function, partial output or success fallback may hide failure.

Earlier A1-A10 semantic approvals, original storage measurements and Part 5A CFG,
ABI, stack-depth and cross-platform evidence remain valuable. Historical numerical
approvals and inline-only assumptions in Sections 8-20 are retained as records of
those checkpoints, superseded for future production policy by this decision and
revised A10. Section 21 retains the historical unchanged-code audit; Section 22
records the current bounded foundation. Previously approved array semantics remain
binding. Production storage implementation and activation require later review.

Phase 22 is locked. The owner reports Actions run #39 passed all seven jobs,
59 suites per configuration. The original architecture investigation used clean
commit `95e79ff`; that is historical context, not the current workspace status.
The Language Bible is authoritative. The owner approved the revised A1-A10
recommendations below, excluding unapproved numerical limits and explicitly deferred
features; alternatives retained for context do not authorize a second design. Existing S1-S8 strings and value/reference contracts remain
unchanged. The valid receiver-chain IR-lowering stack issue is separate hardening
work and is not part of this proposal.

## 1. Recommended first useful slice

Implement builtin, homogeneous `[t]` growable lists and `[t, n]` fixed arrays,
checked reads/writes, `count`, `push`, `remove_at`, array traversal in both
directions, and bounded integer ranges through the existing compiler/runtime.
Start proofs with numeric/bool elements, then require string and reference-bearing
coverage before calling Phase 23 complete. Final acceptance also requires both
i32 and i64 ranges, including `0..<items.count` with i64 count; i32-only is an
intermediate checkpoint. Neither source generics nor a general
collection framework is required to type these builtin constructors.

Recommended final element domain: existing ordinary storable types, including
strings, managed/interface references, value structs and nested typed arrays,
subject to finite value layout, checked physical bounds and reviewed analysis-resource protection. Private construction
views and prepared receivers, void, unresolved types and unfinished values are
never elements. Array types are structural; nominal element identity is preserved.
Do not initially make arrays inherit structs, accept `using` on an array itself,
or erase arrays into existing managed-backed interfaces.

The smallest increment is scalar fixed-array literals/indexing plus ranges. That
is an implementation checkpoint, not permission to omit the growable ownership,
reference-element GC and iteration proofs from the proposed Phase 23 endpoint.
Defer slicing, pop/get results, search/equality-based remove, insertion, sorting,
predicates and user iterator protocols. These need additional contracts or future
language machinery. Do not implement string indexing along with array indexing.

### Approved refined decisions

Wrench review supports the A1-A10 architecture; subsequent owner approval authorizes
implementation through P23-1 to P23-6. The selected choices are zero-length fixed arrays,
contextual typing for empty literals initially, homogeneous element capability
profiles, invalidation at next iterator access, and repeated reference metadata
in the existing collector. Fixed-array snapshot semantics are mandatory; borrowing
is only a proved optimization. Nested selections must reacquire storage and
revalidate dependent owners after possible resize or safepoints. Live elements
must always satisfy their source type, including required references; zeroed spare
capacity is private GC storage only. The earlier 256 KiB proposal and later
64 KiB production approval are superseded by the current large-array decision.
Neither is an approved production array-size or per-function compilation ceiling.

## 2. Bible commitments and approved bounded choices

| Area | Existing commitment | Approved Phase 23 choice, subject to explicit proof gates |
| --- | --- | --- |
| Types, Section 32 | `[t]` grows; `[t, n]` has fixed length; inferred literals are growable | Structural type identity, exact literal length and size-expression rules |
| Defaults, Section 7 | Empty growable lists are independent per instance; no invalid defaults | Fresh GC header per default construction, shared absence of backing only |
| Copies, Sections 12/36 | Storage optimization must preserve value/reference semantics | Fixed arrays copy values; growable assignment aliases one stable list identity |
| Readonly, Sections 5/32 and P3/RV | Readonly elements/method access; safe fixed-array const-formal borrowing | Array element authority profiles and proof conditions for borrowing |
| Indexing, Section 32.2 | `0` first, `-1` last; invalid direct indexing raises a runtime error | Complete negative interval, accepted index types, selection/revalidation order |
| Iteration, Section 26 | Element first, optional original index second; reverse does not copy/reorder | Item copies, binder types, evaluation-once and replacement visibility |
| Ranges, Section 26.1 | Inclusive `..`, exclusive `..<`, ascending/descending by endpoints | Equal endpoints, overflow and endpoint evaluation; final Phase 23 must cover i32 and i64 |
| Mutation, Section 34 | No iteration copy; referenced-object field mutation allowed | General invalidation is recommended, not fully locked; exact check timing |
| Reverse deletion | Suggested convenience only | Defer exception; invalidate at next iterator access after structural mutation in either direction |
| APIs, Sections 32/66 | Push/pop preferred; no automatic add alias | Approve `count`, `push(t)`, `remove_at(index) -> t` for this slice |
| Empty literal inference | Bible illustrates `var history = []; history.push("move")` | Defer later-statement inference; initially require expected type for `[]` |
| Slicing, Section 32.2 | Copies into a new list, not a view | Defer all slicing; fixed-array result kind remains OPEN |
| Custom iteration, Section 35 | Collection syntax should extend to user collections | Protocol, next-result and disposal signatures remain OPEN |

The last six rows must not be turned into invented existing behavior. Rejection of
a future-only example must be explained as a bounded first implementation. No
proposal here changes the Bible's already agreed positive examples into permanent
language exclusions.

## 3. Current implementation findings and reusable paths

| Existing implementation | What can be reused | Actual gap |
| --- | --- | --- |
| `src/frontend/compiler.cpp`: lex/type/primary/statement | Snapshot tokens, contextual numeric typing, recovery, scopes, pure constants, ownership checks | Brackets are not tokens; type grammar is scalar/nominal; postfix/assignment places are field-only; iterable loops are expressly rejected |
| `include/void/ir.hpp`, `src/ir/module.cpp` | One Type/FieldType/arena, constants, validator, typed slots, effects and layout checks | No array kinds, structural element/extent metadata, dynamic-index place or array operations |
| `include/void/reference_capabilities.hpp` | Logical leaf provenance, readonly projection, inferred effects, independent IR checks | Paths currently name static fields; dynamic element selection needs a sound summary, not an arbitrary field index |
| `src/backend/llvm/backend.cpp` | Module-owned DataLayout, aggregate layout, fixed Frame/Slot roots, failure blocks, loop polls, JIT/AOT | No source array layout/access/loop lowering; no resize-safe selected array element |
| `include/void/heap.hpp`, `src/runtime/heap.cpp` | One allocator/collector, validated precise references, rollback of partial marking | TypeLayout has a flat reference-offset vector and ordinary/string kinds; allocation size is fixed for each object; no repeated element shape |
| `src/runtime/coordination.cpp`: construct_into/store | Managed-only operations, poll before allocation, prepared payload validation, final root publication | No stable list header/backing transaction or typed array element commit |
| `src/runtime/abi.cpp`, runtime failure channel | noexcept helpers, checked returns, source snapshots, leased failure text, balanced root cleanup | Bounds, version and resize failures need integration, not native exceptions through generated code |
| Shared analysis/tooling/LSP and reload | Owned immutable type/declaration records, exact contracts, revisions, leases | Array structural keys, size-constant dependencies and builtin members need shared records |

`TypeLayout.references` is used both when validating prepared payloads and during
marking/stores. Extending tracing alone would be insufficient. Frame slots likewise
contain fixed byte extents and offsets. `reference_paths(type, true)` includes
immutable strings for GC while authority traversal excludes them. Interface values
trace the object word, not their witness. Preserve these distinctions in arrays.

`ir::Place` currently contains a local slot and static field indexes. It cannot
safely represent a growable element by inserting a runtime index into that vector.
The existing private prepared interface receiver demonstrates rooted selection
before argument evaluation, but its witness/domain semantics must not be reused
as if array elements were interface receivers.

Historical phase guides contain old exclusions/pending-hosting prose. Current
code and subsequent approved contracts supersede those phase-specific statements;
this proposal does not infer that implemented strings or reference values are absent.

## 4. Approved language contracts and explicit remaining proof gates

A1-A10 below form the approved implementation contract as revised. Recommendation
wording is retained from the reviewed proposal; numerical budgets, explicit proof
gates and deferred features remain exceptions, not implicit approvals.

### A1. Types, inference and initialization

- `[t, n]` is a value-semantic type whose identity includes the exact element type
  and evaluated length. `[t]` is a reference-semantic list type. Neither implicitly
  converts to the other; an expected fixed type contextualizes a literal directly.
- Literal elements evaluate once, left to right. A fixed literal supplies exactly
  n elements; no silent truncation, padding or implicit spread. Annotated `[t] = []`
  is valid. Unannotated nonempty literals infer growable lists.
- Expected element types guide literal interpretation as existing numeric rules
  do. Typed elements must agree exactly after already permitted contextual
  conversions; no union, boxing, numeric widening or inferred common interface.
  Mixed typed numeric anchors diagnose. `[null]` needs a nullable element context.
- Initially `[]` requires an expected element type. Do not infer `any`, default it
  to i32 or retrofit its type from an arbitrary later push. Later-statement empty
  inference is explicitly deferred from the Bible's broader examples.
- Fixed extents use integer literals or the existing pure module-constant
  expressions, with imported/root ownership and cycle checks. Reject runtime
  expressions and local runtime const extents initially. The type parser must
  retain the size expression for staged resolution after declaration discovery;
  forward module constants must not be defeated by parse order. Cycles passing
  through extent/type resolution and constant initializers must share dependency
  diagnostics and bounded work, rather than recursing between two evaluators.
- Recommend accepting zero fixed length. Negative/nonintegral extents diagnose;
  zero-size native layout must not be sent to Heap::allocate as an ordinary object.
  Empty fixed arrays have no elements or roots. This zero-length choice needs approval.
- An omitted fixed-array initializer constructs every element using its existing
  safe default/construction rule in index order. Do not memcpy one result of init
  n times. A nonnullable reference element without a supplied value has no default when
  n is nonzero; require the complete literal. Zero length has no required elements. Nested lists receive independent fresh headers.
- List defaults and runtime literals construct a fresh header. Empty has count
  and capacity zero and no backing allocation. Never use one shared mutable empty
  singleton or per-use copy-on-write as an unapproved change in alias semantics.
- Recommend pure array templates for module constants and declared defaults,
  using owned element templates like existing ConstantValue/string templates.
  Evaluation allocates no managed object; runtime materialization stages complete
  elements and fresh lists at each use. Const lists materialize readonly views;
  no runtime module-global mutable singleton is introduced. Calls, live references
  and custom init remain outside pure evaluation. The existing initializer rules
  still govern runtime omitted defaults, including nested allocation and failures.

Examples below describe the proposal, not executable Phase 22 source:

```python
var list = [1, 2, 3]                    # [i32], growable
var empty: [string] = []
const size = 4
var fixed: [i32, size] = [10, 20, 30, 40]
var zeros: [i32, 4]                     # four valid zero elements
# var unknown = []                      # contextual element type required initially
# var wrong: [i32, 4] = [1, 2]           # exact extent required
```

### A2. Ownership, copies and readonly authority

Recommend a stable GC list header pointing to replaceable contiguous GC storage.
`var b = a` for growable lists aliases the same header: push/remove through b is
visible through a. Independent defaults/literals create distinct headers. Fixed
assignment, arguments and returns preserve element-value snapshots; embedded list/managed
references retain target identity, and strings share immutable content. No implicit
deep clone, frozen object or source-visible stack/heap choice is introduced.

A const list is a deep readonly access path. Copying that reference to var cannot
thaw it. Independently held mutable aliases may still change it. A const fixed
array cannot be written through its binding; copying it to var yields independent
writable value storage while preserving readonly authority of embedded referents,
as RV requires. Immutable strings need tracing but no mutable authority.

**Capability design requiring proof:** fixed arrays need a conservative homogeneous
reference-leaf profile across elements. A dynamic read joins all possible element
origins; dynamic writes must fit the destination's established capacity. Do not
pretend an unknown index is element zero. Prefer a compact element-summary step
in the shared logical shape/effects model over O(n) index-sensitive permission
sets. Literal inference can join readonly leaf profiles conservatively, so one
readonly input can constrain that leaf for every element. Explain resulting
rejections; do not manufacture a runtime-dependent permission.

For growable lists, distinguish authority to replace/resize container storage from
authority over references carried by its elements. Profiles are fixed and invariant
under aliasing, writes and calls. A mutable container must never accept a readonly
reference and later expose it as mutable through another alias. Homogeneous
element profiles are the revised recommendation; alternatives considered are:

1. Carry a homogeneous element profile with the container type/facts, retaining
   it through aliases and exact storage boundaries. Recommended for consistency
   with RV; mutable container storage can coexist with readonly referent leaves.
2. Initially allow readonly referents only in wholly readonly lists, rejecting
   mixed profiles at mutable boundaries. Smaller but more restrictive; requires
   explicit approval rather than silently bypassing RV rules.

Use existing origin/effect fixed points and independent IR verification. GC stops
at list header references; authority projection consults the element summary.
Do not recursively expand dynamic lengths or cyclic list element graphs in either
analysis. Nominal field capacities cannot vary by runtime instance. Arrays are
invariant in element type/profile at mutable boundaries; no list covariance from
managed inheritance or structural interfaces.

Const fixed-array formals retain value snapshot semantics. Borrow actual storage
without a full copy only when later arguments and the callee cannot overwrite the
selected value bytes through aliases, and the owner remains rooted/stable. A
readonly formal alone does not prove that: another argument may mutate its owner.
Otherwise copy into an independent snapshot at that argument's ordinary evaluation
point, before evaluating later arguments, or retain a rooted owned temporary for
an rvalue. Neither later-argument writes nor callee writes through another alias
may change the value bytes observed by the const formal. A const formal does not
permit a delayed copy or a live view of changed caller storage.
This optimization does not freeze referenced objects and introduces no public
borrow/ref syntax. Include a simple proved borrow case and alias-conflict fallback
in acceptance, not an unconditional claim that all const arrays are zero-copy.

### A3. Indexing and selected writes

Recommend all existing integer types as index operands, excluding bool/floats.
Literal contexts may choose i64; typed i32/u64 indexes need no lossy source cast.
Perform exact private checked normalization before any narrowing to target size.
Use i64 for public count and optional iteration indices, with runtime count bounded
by INT64_MAX and target allocation arithmetic. This exact-width choice needs approval.

For count n, recommend nonnegative indexes `0 <= i < n`; negative indexes are
valid exactly for `-n <= i < 0` and select `n + i`. Empty accepts none. `-1` is last,
`-n` first. Signed MIN must fail safely when out of range, without negating it in
signed arithmetic. For unsigned u64, check the full value before conversion.
Normalize before forming an address; never compute an out-of-bounds inbounds GEP.

`a[i]` reads an element value: inline values copy, references preserve identity,
string text remains immutable. It does not return an escaping lvalue reference.
Array indexes are independent of future string scalar indexes. Bible Section 8.1
already approves future string scalar indexing returning a one-scalar string,
scalar `.length` and UTF-8 `.byte_length`; those remain unimplemented. Neither
array negative-index bounds nor `.count` decides string index errors or APIs.

For assignment/compound/nested field writes:

1. Evaluate/select the owner and index once, before the RHS, and check bounds.
2. Root the complete owner. A list selection holds owner, normalized index and
   structural version, not a raw backing/interior address across user code.
3. For compounds snapshot the old scalar/string operand before the RHS, as current
   selected-owner assignment does. RHS side effects run once.
4. Revalidate a list's structural version before commit; an RHS push/remove causes
   checked invalidation failure even if length later returns to the old value.
5. Resolve current storage only after validation and commit without user code or
   safepoints between constituent stores. Nested field writes preserve unrelated
   fields changed by the RHS; do not overwrite a stale whole-element snapshot.

For nested selections such as `a[i][j]` or `a[i].values[j]`, retain rooted owners,
evaluated index values and versions for every enclosing resizable storage on which
the selected inline place still depends. After any intervening operation that may
resize or reach a safepoint, reacquire storage from those roots, revalidate the
dependent versions/bounds, and derive fresh addresses before descending or committing.
Do not cache a backing/element address across that boundary, even in nested fixed
arrays. Reacquisition never reevaluates source owner/index expressions or silently
retargets an invalidated selection: a changed dependent version fails through the
checked channel. A nested list reference already read as a value is an independently
rooted selected header; replacing its former outer slot does not redirect it, and
ancestry that no longer backs an inline place is not a spurious invalidation source.

Rebinding a list variable during the RHS does not redirect the selected original
header. Index-expression effects finish before selection. Earlier side effects are
not rolled back after bounds/version/allocation failure. Direct bounds failures in
ordinary functions stay runtime failures, even for literal indexes in dead branches;
invalid pure constant indexing instead diagnoses while evaluating that constant.

### A4. Safe element use without an escaping reference feature

No source ref-return, pointer, slice view or long-lived element borrow in this slice.
A managed reference read from an element can be rooted independently and used by
its existing methods, even if the list later resizes. Its authority is preserved.
An inline element read is a copy; mutating that local copy requires explicit
writeback to change the array.

Recommend initially allowing checked selected element/subfield assignment and
non-inline-mutating methods, but deferring methods that alias an inline array
value element for in-place mutation. Diagnose that case explicitly rather than
mutating an unnoticed copy. Existing methods on independently copied values and
managed referents keep their ordinary semantics.

To support in-place inline-element calls later, a private rooted scoped access
must prevent structural modification through every alias for the whole call,
including argument evaluation, callbacks and failure exits, or provide equivalent
proved stability. A version check only after a call is too late: writes may already
have used a detached buffer. If included in Phase 23, this is a separate proof
checkpoint, not a trivial extension of ir::Place. No raw interior address may be
mistaken for a GC root. Do not add unsafe borrows to meet a completeness claim.

### A5. Array loops and reverse

Recommended execution contract:

- Evaluate the iterable once. Capture the list header or selected fixed-array
  storage; retain the owning object/temporary for the whole loop. Do not make a
  snapshot/reversed collection or reevaluate a function on each iteration.
- Traverse existing order, forward 0 to n-1 or reverse n-1 to 0. Empty does no work;
  reverse empty must not underflow. The optional binder always receives the original
  zero-based index, independent of cursor direction.
- Item bindings receive an ordinary element value each iteration. Scalar/value
  assignment changes that copy, not the collection. Reference items alias their
  targets, subject to readonly authority. Use array[index] for element replacement.
- The optional index is an independent i64 value; rebinding it cannot redirect the
  hidden cursor. Binders are body-scoped and obey existing no-shadowing rules.
- No-copy traversal observes a nonstructural element replacement when that index is
  subsequently fetched. The current copied item is not retroactively rewritten.
- A fixed array embedded in selected growable storage also retains the enclosing
  owner/version, validating it before further access if that outer list resizes.
- Keep existing break/continue/return behavior and coordinated loop polls. Every
  checked failure releases private iterator/access state and ordinary root frames.

### A6. Structural mutation and nested traversal

Recommend fail-fast version checks for growable lists in forward AND reverse loops.
Capture version at entry; check before each fetch and before declaring normal
completion, including the next step after continue. Do not silently end a loop
because a removal shrank count below the cursor. Nested loops have independent
captured versions, even when traversing the same header.

Push and successful remove_at increment a u64 structural version, including when
capacity is unchanged. Never wrap into a previously valid version: exhaustion fails
before mutation. Failed operations do not change count, backing or version.
Replacing an existing element/subfield is nonstructural; mutating fields of an
existing referenced object remains allowed. A nested list is a separate container:
resizing it does not invalidate traversal of the outer list unless the outer is
also structurally changed.

Recommend detecting a body-induced structural change at the next iterator access,
not necessarily rejecting the mutation itself. Thus a mutation followed by break
has no further access and can succeed; another still-active nested iterator will
fail when it resumes. This exact timing is OPEN for approval. An eager active-
iterator mutation ban is a valid alternative, but needs counted lifetime cleanup
on every exit. Do not mix the two models accidentally.

Defer the reverse remove-current exception. Even one duplicate value, two nested
reverse iterators or remove/reinsert can defeat a naive cursor adjustment. Do not
special-case it merely because the example exists in Section 34. Concurrent writes
remain outside the current source threading contract; a version counter is not
synchronization, atomicity or a data-race solution.

### A7. Numeric ranges

Final Phase 23 acceptance requires single-binder i32 AND i64 ranges. An i32-only
implementation is acceptable at an intermediate checkpoint, not at the final gate.
Existing contextual literal typing selects a matching endpoint type, so
`0..<items.count` is an i64 range and its binder is i64. Typed endpoints must match;
a typed i32 endpoint and typed i64 endpoint require an explicit existing conversion,
not implicit widening. Other integer widths, floats, custom steps and two-binder
range metadata remain deferred. This does not define generic iteration.

Evaluate endpoints once, left to right. Preserve all four Bible examples exactly:
0..10 includes 10; 0..<10 stops at 9; 10..0 includes 0; 10..<0 stops at 1.
Recommend equal endpoints yield one item for inclusive and none for exclusive.
Direction follows the initial endpoints. Reverse ranges reverse the SAME included
sequence, not simply swap endpoints (which changes exclusive membership).

Use endpoint termination tests before the next increment/decrement, or equally
proved checked internal arithmetic. Do not compute `end - start`, negate signed
MIN or form a one-past endpoint in the source integer width. In particular, the
inclusive INT64_MIN..INT64_MAX sequence has 2^64 items: do not require its cardinality
to fit i64 or u64. Traversal must remain valid without materializing that count.
For both widths, `MAX..MAX` and inclusive sequences ending at MIN terminate
without overflow; reverse exclusive bounds must be equally safe. No wrapping range
step or LLVM poison. Very large ranges still poll and support break/continue;
they allocate no collection and never pre-expand source/IR by length.

`0..<items.count` snapshots count once. Resizing items inside that numeric loop does
not turn it into a list iterator or update the endpoint; each subsequent `items[i]`
is independently bounds-checked. Version invalidation of an array iterator remains
a distinct contract from traversing integer values.

### A8. Precise GC, resize transactions and publication

The following is the original candidate representation, compiler-private and
subject to target layout. Its fixed-array inline entry is not a requirement that
large values occupy native stack storage. Revised A10 requires investigation and
owner review of large fixed-array storage on the existing GC, without choosing
that architecture here. The growable header/backing and precise-GC obligations
remain; no heap-backed fixed array is implemented or newly approved.

Original candidate diagram:

```text
source [t] word -> stable header { backing reference, count, capacity, version }
                                      |
                                      v
                            contiguous element buffer
source [t, n] -> inline n * element layout in its owning value/place
```

Headers and backing both use the current Heap. No malloc-owned hidden array outside
the collector, header relocation, second tracer, pointer tagging or per-element
managed wrapper. Non-moving objects do not make backing replacement safe by itself.
Empty headers are ordinary complete objects with null backing/count/capacity zero.
Every helper validates count <= capacity, the exact buffer element schema/extent
and runtime ownership before accessing bytes; arbitrary header fields cannot
authorize pointer arithmetic or a forged typed store.

Two options for backing reference maps:

- Expand existing flat numeric offsets for every capacity slot. Least tracer code,
  but O(capacity * reference-leaves) metadata, repeated copies at allocation and
  expensive maps for nested value elements. Acceptable only with an explicit
  modest first-slice bound and measured memory costs.
- Extend TypeLayout with an immutable repeated region: start, stride, capacity,
  and validated per-element reference offsets. Recommended for scalable contiguous
  storage. Keep ordinary/string layouts unchanged and add one shared reference-
  enumeration helper consumed by allocation validation, marking and checked stores.
  This is an extension of the existing collector, not a parallel array collector.

At every source-observable point, all elements in [0, count) are complete, valid
values of the element type. Every nonnullable/required reference leaf there targets
a complete valid object; an unfilled null required-reference element is never a
live source default. This includes nested value/fixed-array elements. Spare slots
in [count, capacity) contain zeroed GC reference positions and are inaccessible to
indexing, iteration, element methods and source initialization reads. Their zeros
are not valid source values of a required-reference type.

Trace all capacity reference positions, with unused positions zero. Do not derive
extent from an untrusted/mutable source count during collection. Give private
buffer slots a nonzero, alignment-correct stride even for zero-sized element
values; source zero-length fixed arrays still contain no observable storage or
identity. Validate this case rather than dividing by zero or allocating a
zero-byte Heap object. Interface elements
trace object words only; strings trace leaf allocations; nested fixed arrays repeat
existing target-relative shapes; nested lists trace their headers. Validate stride,
alignment, region end and every offset with checked multiplication/addition before
allocation. Repeated metadata and type descriptors are copied/runtime-owned,
contain no AST/LLVM/code pointers, and survive allocating-generation retirement.

Resize plan:

1. Root header, old buffer, new element/removed result and all staged values.
   Evaluate source operands once before the runtime commit.
2. Prepare capacity/layout and every fallible allocation/metadata operation before
   replacing the visible buffer. Grow geometrically for amortized O(1) push; exact
   growth factor is private, not an observable source capacity API.
3. Allocate a new zeroed buffer with valid maps using the existing rooted allocation
   path. Source required-reference elements are valid for every live index; unused
   capacity zeros are private storage, never readable source values. A rooted
   unpublished buffer is a complete private storage object, not a half-initialized
   source array: no source live count exposes it until the header commit. Every
   stored pointer remains a valid traced reference throughout preparation. Populate
   and validate every element that will become live before increasing the visible
   count. During remove/shift, publish the reduced count and cleared spare reference
   positions only as part of the same no-safepoint commit; temporary copy states
   never become observable source elements.
4. While Managed, copy staged/live bytes and commit header backing/count/capacity/
   version together without user code, collection, parking or fallible operations
   in between. A collection before or after commit sees a complete graph.
5. In-capacity append/remove uses the same validate-then-infallible-commit discipline.
   Root the removed value before shifting; zero vacated reference slots. Do not
   retain deleted references accidentally through spare capacity.

Do not hold the collector-required mutex while polling/collecting or running user
code. Large no-safepoint copies/zeroing have pause costs: measure them and consider
rooted bounded copying into an unpublished buffer with polls between chunks. Do
not add chunked publication that exposes half-initialized live elements. Failed
resize leaves aliases' old count/content/version intact; staged complete children
and abandoned backing become collectible. User-side effects before the commit are
not transactions. Deterministic allocation and map/trace-failure injection remains
mandatory; OS overcommit cannot be represented as a guaranteed catchable OOM.

### A9. Operations and phase boundary

| Phase 23 minimum recommendation | Reason | Later work |
| --- | --- | --- |
| `.count: i64` on both forms | Iteration/indexing without inventing string length APIs | General properties remain separate |
| `.push(value)` with no result, growable only | Useful append and growth proof; preferred Bible spelling | No automatic add alias |
| `.remove_at(index) -> t`, growable only | Bible example captures removed value; checked shift/root handoff | No shrink policy exposed |
| Checked [] read/write and supported compounds | Reuse typed arithmetic/string rules | No array arithmetic/equality/ordering selected |
| Builtin arrays/ranges in for/reverse | Small complete consumer of collection storage | User iterable protocol after design |

Phase 24 can choose generic specialization, constraints and APIs expressing t/result
without changing these builtin type identities. Phase 25 can add insert, clear,
pop/get with proper result/lookup errors, remove-by-equality, remove_if predicates,
slices, dictionary/hash policy and library queues/builders. Scheduling alone does
not approve those semantics. Ordinary pop/get examples need result/pattern support;
do not return a sentinel or silently invent nullable primitive elements to fake it.

The proposed builtin operations should use the existing shared member catalog with
owned signature/effect information for analysis and tooling. Member names are not
new global reserved identifiers; existing user struct members keep their names. Do not implement a
source generic compiler, duplicate array method checker or a special runtime per t.
An internal iteration plan may distinguish builtin arrays/ranges now and later a
resolved protocol call. Public iterator/next/disposal names and library conformance
remain OPEN; Section 35 forbids turning every future collection into a new hardcoded
compiler family.

### A10. Superseding large-array commitment and production approval gate

**Owner decision, 15 October 2026, supersedes the earlier 65,536-byte production
policy and the proposal to count array multiplicity into a permanent 65,536-shape
ceiling.** Neither fixed nor growable arrays have an arbitrary 64 KiB source-level
size ceiling. The current implementation still has the old gates (Section 21);
this documentation does not remove checks or claim large arrays already compile.

Retain finite-layout/cycle validation, checked arithmetic, parser/semantic work
protection and the valuable target-aware inventory, CFG, ABI, root and stack-depth
proofs. Distinguish actual analysis complexity from repeated element count or
payload bytes. Review frontend, IR, shape walks, defaults/constants and reference
metadata together; do not just rename, raise or move the same size ceiling. Safe
analysis-resource exhaustion must diagnose and fail the complete build, never
truncate types, elements or required functions. No new numerical cap is approved.

Investigate representations capable of large fixed values and growable storage,
including use of VOID's existing coordinated GC heap. Candidate studies may compare
safe inline storage with GC-backed fixed payloads or a combination; no selection,
cutover threshold, copy-on-write contract or heap-array implementation is authorized.
Value copies/snapshots must remain independent as required by existing semantics;
embedded references keep their existing element semantics. Const-formal borrowing
still needs proof, with safe snapshot copying on alias conflict. Heap use must
preserve precise roots, safepoints/reacquisition, complete publication, allocation
failure cleanup, readonly capabilities and reload lifetimes without another allocator
or collector. Source value semantics do not require all payload bytes on the stack.

NativeStoragePlanner's complete target-derived inventories remain useful. Actual
DataLayout/ABI homes, arguments/results, copies, roots, staging, alignment, spills
and call areas still need safe accounting. A future reviewed plan must distinguish
native stack usage from total array payload/storage. It must serve frontend, IR,
LLVM JIT/AOT and shared analysis/LSP without an editor-side calculator. The historical
65,536-byte threshold can remain an explicitly scoped research input; it cannot
reject every otherwise valid large-array program in production. Do not disable
certification checks, alter stack reserves or silently substitute a larger ceiling.

Runtime sizes require checked exact arithmetic, representable count/index and
selected-target address-space limits, plus ordinary allocation-failure handling.
These physical constraints are distinct from an arbitrary public capacity maximum.
String literal/pool limits remain their separate contracts and are not array-payload
limits. Compiler template/default work needs review without eagerly expanding every
repeated element merely to test a bound.

Before production native-array activation, owner/wrench must approve the revised
storage architecture and analysis-resource policy, followed by implementation and
proof of large-array semantics, complete function coverage and atomic failure.
A failed/unsupported required function fails the entire build with a clear diagnostic;
no omitted-function success or partial native delivery is permitted. All current
native-array, lineage, ABI, origin and physical-memory gates remain in force until
resolved. Receiver-chain hardening remains separate.

Structural type keys must include element kind/nominal identity and fixed extent;
capability contracts additionally include homogeneous profiles, effects and borrow/
construction rules. Extend FieldType/arena ownership without strong cycles or
synthetic nominal collisions. A growable reference edge breaks inline layout
recursion; fixed-element inline recursion must still reject infinite layouts.

Exact source compatibility must include arrays everywhere: declarations, fields,
formals/results, nested templates, size-constant references, initialization closure,
implicit builtin effects and interface/virtual exact signatures. Size/type/default/
const/profile changes require restart under current conservative contracts; equal
byte size does not authorize reinterpretation. Do not migrate a live list to a new
element stride. Compatible body-only changes keep old headers/buffers usable by new
code and preserve old-call leases. Arrays carry no executable pointers or code
leases; GC reachability and generation reclamation remain separate.

## 5. Integration plan through the existing compiler

1. Add bracket/range tokens with longest-match handling BEFORE numeric lexing can
   consume `0..10` as a malformed numeric literal. Preserve decimal floats, imports,
   strings/comments, original spans and recovery. Share postfix parsing for calls,
   fields and indexing, including literals/parenthesized results. Generalize the
   existing assignment place model rather than adding a second statement parser.
2. Extend the shared type arena, canonical formatting/equality, defaults and owned
   constant templates. Keep source extent expressions and dependency spans even
   after resolving a numeric extent. Reuse literal typing and pure evaluation;
   validate all elements including unused declarations before lowering.
3. Extend the shared logical shape/capability model once for array elements.
   Preserve independent IR derivation of readonly, origins, constructor readiness
   and write capacities. Prove aliases, joins, stored elements and returned values.
4. Add typed backend-independent operations for fixed construction/access/update,
   list creation/count/push/remove, and private checked selection/version state.
   IR validation derives element types and checks owners/index types, reference
   shapes, scope/lifetime and coordinated-runtime requirements. Negative IR tests
   must reject forged extents, strides, qualifiers and bypassed selected writes.
5. Lower fixed storage with LLVM ArrayType and this module's DataLayout, not n fake
   struct fields. Root maps cover actual fixed storage and reference-bearing result
   homes. A list is a rooted header word. Resolve backing addresses only inside a
   validated no-safepoint access window; no interior pointer can retain an owner.
6. Add array layout/helper operations to the existing runtime ABI and bind them in
   ORC and AOT alike. Numeric-only fixed arrays/ranges need not introduce a
   runtime owner; growable lists and reference/string-bearing arrays must use the
   existing coordination/bootstrap discovery, including parameters and defaults. Existing failure propagation unlinks frames; bounds/version
   errors retain exact access/push/remove spans and source call traces. Static
   failure descriptions are acceptable initially; richer index/count rendering
   must extend the same failure channel, not a new exception subsystem.
7. Lower iterable/range plans into the existing CFG/slot/control-flow machinery.
   Preserve return-path analysis, continue/backedge polls and break cleanup.
   Add no iterator objects or reverse buffers for builtin loops.
8. Extend owned analyzed metadata, builtin member descriptions, size references,
   navigation, hover/type text and invalidation. LSP uses the same checker and
   immutable source/UTF-16 positions, including incomplete bracket/range recovery.
   Builtin members without source declarations must not acquire fabricated source
   spans for navigation. Editor grammar changes are bounded to real syntax. No
   editor-side array type system or implementation-dependent capacity UI.

## 6. Incremental implementation checkpoints after approval

| Step | Deliverable and exit proof |
| --- | --- |
| P23-0 | Historical A1-A10 approval; the subsequent 65536-byte production policy is superseded. Revised large-array storage/resource policy requires owner/wrench review before activation |
| P23-1 | Canonical structural types, finite/default layouts and source diagnostics; scalar fixed literals/access and exact extent negatives through existing analysis/IR |
| P23-2 | Normal/O2 fixed-array JIT/AOT and for/range/reverse CFGs, original indexes, i32 range endpoint safety and readonly const-formal borrow/fallback proofs; i64 may follow before the final gate |
| P23-3 | GC header/buffer proof, scalar list defaults/push/remove, alias identity, version checks and rollback under injected failures |
| P23-4 | String, managed/interface and reference-value elements, nested arrays, capability summaries, complete initialization and budget-one GC at every boundary |
| P23-5 | Selected nested writes, RHS alias/resize effects, nested/reverse iteration, leased reload and old-holder/new-AOT-code proofs; in-place element method borrows only if separately proved |
| P23-6 | Shared tooling/LSP, diagnostic recovery, reviewed resource limits and required i64 range completion (`0..<items.count`); full local acceptance then owner review before owner-controlled hosted CI |

A failed proof can justify reducing a proposed first-slice feature only through
review, not by weakening assertions, excluding a platform or silently changing
agreed semantics. No implementation checkpoint changes the locked Phase 22 contract.

## 7. Focused test plan and eventual acceptance

Preserve all existing 59 suites and their assertions. Add focused arrays coverage
instead of repurposing a passing string/reference test. Exact suite names/counts
are implementation planning, not a claim that these tests exist now.

- **Types/defaults:** inferred/contextual empty/nonempty literals, exact extents,
  forward/imported size constants and cycles; zero/negative/huge n; heterogeneous
  anchors; nonnullable required elements; nested init called once per element;
  independent default lists in two objects and two array elements; pure templates
  with fresh runtime identities and no compile-time allocation.
- **Copy/authority:** fixed independent bytes versus shared referents/list aliases;
  deep const, const-to-var copies, mixed leaf summaries, fixed capacities, joins,
  helper returns/retention, mutating callbacks through aliases and forged IR.
  Prove safe const-formal borrow plus snapshot fallback when a later argument
  writes the selected array or the callee receives another alias to its owner.
  Include a fixed array embedded in a resizable buffer, with resize during later
  argument evaluation. The const formal sees original inline element bytes while
  caller mutations remain visible through their own aliases; referenced object
  identity is shared rather than deep-cloned.
- **Indexing:** first/last, -1/-n, n/-n-1, empty, signed MIN and unsigned MAX;
  exactly-once owner/index/RHS effects, rebinding, failed initial bounds before RHS,
  resize/remove/reinsert during RHS, and preserved unrelated nested fields.
  Exercise nested `a[i][j]` and `a[i].values[j]` with a safepoint or resize between
  selection steps and before commit: reacquire addresses, fail invalid dependent
  selections, and preserve an independently captured nested list header.
  No string indexing or source pointer accidentally becomes accepted.
- **Iteration:** empty/single/many arrays, original reverse indices with duplicate
  values, no reversed allocation/copy, iterable evaluated once, copied item versus
  reference mutation, nonstructural replacement visibility, nested same/different
  owners, continue/break/return/failure cleanup, structural mutation and version
  exhaustion. Test mutation-followed-by-break according to the approved timing.
- **Ranges:** all four Bible examples, equal endpoints, reverse exclusive membership,
  both i32 and i64 min/max/descending boundaries, `0..<items.count` with an i64
  binder, typed-width mismatch diagnostics, endpoints evaluated once, no one-past
  overflow or cardinality subtraction,
  rejection of unsupported types/steps, and long-loop cooperative rendezvous.
- **GC/failure:** actual root-frame-only reachability, strings and two-word interfaces,
  nested value leaves, cycles through list headers, spare-slot clearing and removal
  collectability, capacity growth with budget 1, allocation/map/collection failure
  before every commit, protected old aliases, cross-runtime reference rejection,
  required-reference validity for every newly live element and inaccessible zeroed
  spare capacity before/after push, remove, failed resize and nested construction,
  balanced roots and no half-published S1/C1 parent.
- **Native/reload:** normal/O2 JIT and separately emitted/linked normal/O2 AOT with
  runtime-only dependencies; checked failure source locations/call traces; retain
  arrays after allocating-generation retirement via explicit handles; new AOT code
  accesses old live arrays; active old calls keep generation leases. Changed
  element/fixed-extent/profile/init/const contracts reject replacement.
- **Tooling/resources:** precise malformed bracket/range diagnostics and recovery to
  a later independent error, strict UTF-8 output, imported extent navigation and
  references, unsaved edit/repaired revision queries, type hover, completion of
  approved builtins, unchanged string parsing, bounded shape/template/native frames.

After implementation, require both LLVM 19.1.7 SDKs in Debug, Release and strict
ASan/UBSan with leak detection; focused GC/failure/reload stress; 1 MiB checks on
Debug AND Release; existing and new CLI/JIT/AOT and LSP suites; relevant real editor
integration and VSIX packaging. Distinguish the separate valid receiver-chain
lowering issue from new array regressions and do not claim arbitrary stack safety.
If array acceptance is blocked by the existing lowering issue, report that blocker
and scope a separate hardening task; do not weaken new tests or relabel an array
regression to claim success. New array lowering handlers must avoid increasing
the old recursive dispatcher frame; a necessary broader refactor returns to review.
Then stop for owner review. Only owner-controlled CI with all seven jobs passing,
all 59 preceding suites intact and added array coverage, can lock Phase 23.

> **Historical record notice:** Sections 8-20 retain measurements, approvals and
> restrictions as they stood at each checkpoint. Their 65,536-byte production-policy
> statements and mandatory expanded-array size ceilings are superseded by the owner
> decision above. Research checks remain operational; these records do not authorize
> production reactivation or an alternative storage design.

## 8. Investigation-only verification and review handoff

No compiler/runtime/tests/build/grammar implementation was edited. Six selected
existing Debug suites passed: runtime_coordination, reference_values_source,
strings_source, tooling, analysis and frontend (6/6, 26.97 seconds). CTest inventory
is still 59. Six standalone check probes confirm array types/literals/indexing,
collection iteration and ranges remain unsupported by the current compiler;
each exits 1 with a source diagnostic. Logs are retained under
build/phase23-investigation. These are baseline checks, not Phase 23 acceptance.

The owner has now approved the representation/alias choice, homogeneous element
capability model, indexing/count types, fixed-array snapshot and proved borrow
behavior, next-access invalidation, negative bounds, range choices, zero-length/
contextual-empty policy, templates, builtin API subset and repeated GC metadata.
Numerical storage budgets still require evidence and separate approval. Nullable
arrays, string indexing, reverse deletion, public element references, a generic
iterator protocol and the separate lowering fix remain outside implementation.

## 9. Historical implementation preflight: storage evidence before approval

The owner authorized P23-1 through P23-6, preserving the explicit numerical-budget
approval gate. Before enabling fixed-array native lowering, inspected its real
allocation sites in backend.cpp: local homes, construction stages, parameter root
copies, instruction-result root homes, Runtime::Slot descriptors and Runtime::Frame.
The existing 65536 expanded-shape bound cannot bound their combined native storage.

A retained LLVM 19.1.7 probe models four simultaneously live pointer-array homes
using actual LLVM ArrayType storage, four existing root descriptors and the current
32-byte Frame. Linux executes the real vd_gc_frame_enter/leave/poll ABI and collects
with every frame active. All pointers are null: these are valid nullable-reference
root payloads, not incomplete required-reference source values. The probe is not
Phase 23 source implementation, array construction validation or a public array ABI.

LLVM-reported native frame sizes in bytes (O0 / O2):

| Planned static bytes, including roots/frame | Linux x64 | Windows x64 | macOS ARM64 |
| --- | --- | --- | --- |
| 65536 | 65560 / 65576 | 65592 / 65608 | 65632 / 65616 |
| 131072 | 131096 / 131112 | 131128 / 131144 | 131168 / 131152 |
| 262144 | 262168 / 262184 | 262200 / 262216 | 262240 / 262224 |
| 524288 | 524312 / 524328 | 524344 / 524360 | 524384 / 524368 |

The three target DataLayouts came from Clang 19.1.7. These target compile results
include register/call-frame overhead but are not Windows/macOS execution. Warning
attributes request measurement only; optimization and root liveness remain enabled.
Actual C++ sizeof/alignment checks confirm native Linux Slot and Frame are both
32 bytes, aligned to 8. Cross-target records use the corresponding four-word schema;
complete native runtime ABI validation remains a hosted obligation.

Linux runs use a hard and soft 1048576-byte stack, separately for each executable:

| Planned bytes per frame | Live frames passing at both O0/O2 | First failing tested depth |
| --- | --- | --- |
| 64 KiB | 1, 2, 4, 8 | None through 8 |
| 128 KiB | 1, 2, 4 | 8 |
| 256 KiB | 1, 2 | 4 |
| 512 KiB | 1 | 2 |

Failing child processes exit SIGSEGV at these deliberately oversized call stacks.
Passing processes collect through actual registered roots and finish with balanced
thread/frame cleanup. This is 24 target/mode/layout measurements and 32 Linux
stack runs: 20 successful and 12 expected exhaustion cases. It neither repairs nor
exercises the separate frontend receiver-chain lowering defect.

**Historical recommendation, subsequently approved after run #3: 64 KiB (65536 bytes) per
array-carrying function for total statically reserved storage before optimization.**
This is a candidate resource policy, not an adopted compiler limit or a guarantee
about arbitrary recursion. Count all known local/parameter/result/stage/copy homes,
root descriptors, Frame and target ABI scratch/outgoing copy costs, not merely
one array's payload or its traced leaves. Do not apply a new restriction to existing
array-free functions as an incidental change. Bounds must use target DataLayout
and checked size arithmetic, including zero-sized layouts and alignment.

Before enforcement, a common target-aware storage plan must serve validated
analysis/LSP and shared lowering, and cover actual array parameter/return ABI,
borrow fallback copies and intrinsic call scratch. At that preflight the APIs had no such
shared native-storage plan; P23-1 now prepares the common inventory contract. The hand-generated probe does not prove those missing
parts; the real planner and later six-configuration/native tests must audit them.
Register spills and arbitrary caller depth remain outside a simplistic payload
sum, so the budget cannot be advertised as native-stack safety for every program.

The earlier 256 KiB proposal remains unapproved and is not supported as a default
by this nesting evidence. At this historical preflight, no limit had been adopted
and work stopped for numerical review. Section 10 supersedes that pending status:
64 KiB is now approved, with P23-1 authorized and all target/ABI, readonly,
initialization and later checkpoint proof gates retained.
Evidence and rerunnable scripts are in build/phase23-storage, including measured.json,
target assembly, run.py and measure.py. Full Phase 23 acceptance is still outstanding.

## 10. Targeted manual native stack investigation

Prepared `.github/workflows/phase23-stack-probe.yml`, triggered only through
`workflow_dispatch`, with Ubuntu 24.04 x64, Windows 2022 x64 and macOS ARM64 jobs.
The macOS runner label is `macos-latest`; the driver rejects an x64 host rather
than substituting cross-compilation. LLVM/Clang remains pinned to 19.1.7 through
the existing SDK installer/cache. The ordinary seven-job `ci.yml` is unchanged.
Only the owner may push and dispatch this experiment. A normal CI push trigger
still operates independently of this manually triggered workflow.

Tracked reproduction sources are under `tools/ci/stack_probe/`: generated LLVM
probes, a native worker harness, a standalone CMake subproject and evidence checks.
The subproject builds only the existing six LLVM-independent runtime sources and
the harness, never the compiler project or its 59-suite matrix. Both generated
code and runtime/harness use O0 or O2 as selected. The same four live nullable
pointer-array homes and existing root descriptors/Frame model are retained for
64/128/256 KiB planned storage. Actual native ABI checks precede execution.

Each depth 1/2/4/8/16 executes in an isolated process. POSIX supplies a verified
1 MiB worker-stack mapping, 512 KiB inaccessible low guard, high guard page and
separate 64 KiB alternate signal stack. Windows requests a 1 MiB worker reservation,
verifies its allocation with VirtualQuery, records OS stack limits and requests
64 KiB exception headroom. These controls do not create identical usable headroom
across platforms and are not main-thread RLIMIT_STACK measurements. Thread startup,
TLS, call overhead and platform guard behavior remain platform-specific.

An expected failure requires a fault address in the recorded POSIX guard or actual
Windows EXCEPTION_STACK_OVERFLOW, plus unique stack/ABI evidence and exit 90.
An unrelated fault control for every size/mode must exit 91 and be rejected as
overflow. Unexplained faults, setup errors, missing evidence and timeouts fail the
job. Small cases consuming at most 75% by measured frame size must pass; cases
whose frame sum reaches the full reservation must exhaust; intermediate cases
are reported as boundary observations. These checks do not enforce a source limit.

Actual Linux x64 execution of the final observer-enabled harness:

| Planned KiB | O0 LLVM frame / observed stride bytes | O2 LLVM frame / observed stride bytes | Passing depths, both modes | Verified exhaustion depths, both modes |
| --- | --- | --- | --- | --- |
| 64 | 65560 / 65568 | 65576 / 65584 | 1, 2, 4, 8 | 16 |
| 128 | 131096 / 131104 | 131112 / 131120 | 1, 2, 4 | 8, 16 |
| 256 | 262168 / 262176 | 262184 / 262192 | 1, 2 | 4, 8, 16 |

This is 30 depth cases: 18 successful collections with balanced cleanup and
12 verified guard faults. All six unrelated-fault controls pass. Six evidence
classification tests reject invalid guard addresses, non-stack Windows exceptions,
bare signal/exit claims, missing/malformed evidence and mismatched stack extents.
Observed recursive frame strides are consistent and include call/alignment costs
outside LLVM's frame-size diagnostic. No optimization suppression is used.

Independently inspected [native probe run #3](https://github.com/Shmellyorc/Void.Language/actions/runs/38660215692),
commit `ea8685180750f69b1dc4d182b7ac5940ddc6c9bd`: all three jobs and platform
artifacts pass with LLVM 19.1.7. Each platform passes six harness regressions,
18 normal depth cases, 12 verified overflows and six unrelated-fault controls.
All platforms have the same tested passing/exhaustion depths in the preceding table.
These are native executions, not cross-compilation inference.

| Planned KiB | Linux x64 O0 frame / stride | Linux x64 O2 frame / stride | Windows x64 O0 frame / stride | Windows x64 O2 frame / stride | macOS ARM64 O0 frame / stride | macOS ARM64 O2 frame / stride |
| --- | --- | --- | --- | --- | --- | --- |
| 64 | 65560 / 65568 | 65576 / 65584 | 65592 / 65600 | 65608 / 65616 | 65632 / 65632 | 65616 / 65616 |
| 128 | 131096 / 131104 | 131112 / 131120 | 131128 / 131136 | 131144 / 131152 | 131168 / 131168 | 131152 / 131152 |
| 256 | 262168 / 262176 | 262184 / 262192 | 262200 / 262208 | 262216 / 262224 | 262240 / 262240 | 262224 / 262224 |

All frame/stride values are bytes. Artifacts verify the requested native 1 MiB
worker extents, platform guard/reporting controls, root ABI and successful cleanup.
Linux and macOS pass both lifecycle controls. macOS records the old zero-size
request returning -1 with errno 12/ENOMEM at O0/O2; corrected disable/query/unmap
returns zero. Windows verifies the actual reserved stack and actual stack-overflow
exception. Artifacts retain target/version, source hashes, commands, assembly,
frame diagnostics and per-child JSON/logs. Local copies are under ignored
`build/phase23-probe-run3`; hosted artifacts have 14-day retention.

Boss/wrench approve **65536 bytes of total per-function native static storage**
for array-carrying functions, subject to target DataLayout and real ABI proofs.
Account for parameter/result homes, local and instruction homes, copies, roots,
staging, alignment and known ABI overhead. Native enforcement must use a shared
plan consumed by analysis/tooling and lowering; no separate LSP calculator or
payload-only estimate is authorized. The earlier 256 KiB proposal is not adopted.
The probe does not prove real array calling conventions, reference-bearing arrays,
borrow fallback storage or arbitrary source recursion. Stop if the approved budget
cannot be supported, rather than changing it. Full Phase 23 acceptance remains outstanding.

## 11. P23-1 implementation boundary

P23-1 adds structural array descriptor kinds, element/extent identity keys and
canonical scalar fixed-array instances in the existing type arena. Array descriptors
contain one nonowning element edge, not a vector of synthetic nominal fields.
Source storage currently accepts fixed arrays of existing numeric/bool scalars.
Growable, nested and reference-bearing storage diagnose as deferred to subsequent
checkpoints; the descriptor foundation does not claim those operations work.

Scalar fixed annotations, exact contextual literals, omitted zero defaults,
parameters/results, inline struct fields, value copies and checked reads/updates
flow through the existing parser, semantic analysis and validated IR. Empty fixed
literals need a context; zero extents contain no elements. Numeric contextual
typing retains exact existing conversions and range rules. All existing integer
index widths are accepted; bool/floating indexes reject. The shared checked
normalization handles -1/-n, signed MIN, unsigned MAX and empty arrays without
signed-negation overflow or target-width narrowing. Runtime bounds operations are
retained as checked IR; only pure constant out-of-bounds accesses execute and
diagnose during this checkpoint.

Extent expressions remain owned syntax until declaration discovery. Resolution
reuses pure module constants, root/import visibility, existing dependency states
and diagnostics, including forward references, value-constant fields and cycles
through array annotations. Runtime/local-const extents reject. Layout multiplicity
uses the existing 65536 expanded-shape and 128 depth guards before allocating
default/template vectors. The parser's existing 512 expression-node guard also
covers extent subexpressions inside casts. No new constant-array pool numerical
policy is inferred from string limits; aggregate pool work remains a later reviewed
resource obligation before the broader array template/GC implementation.

Indexed places reuse existing selected-owner storage. Owners/index expressions
evaluate once; index effects finish before reading current array bytes. Selected
writes check bounds before the RHS, snapshot compound operands, then reacquire
current storage for commit so unrelated RHS writes are preserved. Checked access
instructions retain source locations/excerpts. Scalar arrays have no GC/readonly
reference leaves; reference-containing profiles, resize/version selection and
borrow optimization remain later proof gates.

`NativeStoragePlanner` is the shared target-aware inventory contract described in
A10. Its finalizer requires all home categories and checked target alignment/size
arithmetic; a complete array-carrying inventory exceeding 65536 bytes rejects.
Target layouts are supplied explicitly, without sizeof(host) guesses. The actual
LLVM/ABI inventory producer and its shared analysis/tooling target configuration
are not implemented in P23-1. Consequently the common LLVM lowering path explicitly
rejects array types/operations before native execution. This gate applies to JIT
and AOT, preventing accidental native support before budget enforcement is proved.

The new `arrays_foundation` suite validates source/IR positives and negatives,
forged IR extents/index/value/stride metadata, dependency ownership, zero/default
layouts, source evaluation order, integer extrema and incomplete/overflowing storage
inventories. The 59 original suites remain present. P23-2 through P23-6, iterable
loops/ranges, native array lowering, growable storage, GC array maps, indexing of
strings and the unrelated receiver-chain hardening task are not implemented here.

## 12. P23-2 storage proof preflight: stopped before production lowering

Boss/wrench report hosted run #40 passed all seven jobs, 60/60 per configuration;
P23-1 is locked. The P23-2 assignment retains the approved 65536-byte budget and
requires stopping if complete target accounting or const-formal borrow proofs
cannot be established. The storage accounting prerequisite is not yet proved.
P23-2 is **not implemented or accepted** by this investigation.

The current shared backend constructs ordinary value parameters/results directly
as LLVM value types. Extending only its type mapper to LLVM ArrayType would expose
arrays to target aggregate argument/result legalization. DataLayout gives the
inline value's layout, not the whole storage cost of that legalization. The
P23-1 StorageTarget value_layout callback and explicit role checklist alone do
not enumerate machine-generated homes. There is no real inventory producer or
shared analysis/LSP native target context yet. Marking known_abi as accounted
without enumerating those homes would be an unsupported proof claim.

The tracked standalone reproducer is tools/ci/array_abi_probe/probe.py with host.c
and README.md. It models that raw aggregate extension in separately generated
caller/callee modules, with the existing trailing failure-pointer shape. It does
not remove the production backend's array gate or implement a second compiler.
Each array contains i32 scalars. The caller has two explicit homes of one payload
each; the callee has one. External helpers populate, mutate and check all elements.

LLVM 19.1.7 post-prologue/epilogue MIR measurements:

| Target / evidence | Payload bytes | Caller frame normal / O2 | Callee frame normal / O2 |
| --- | ---: | ---: | ---: |
| Linux x64, native | 4096 | 20440 / 20440 | 8200 / 8104 |
| Linux x64, native | 16384 | 81880 / 81880 | 32776 / 20392 |
| Windows x64, compile-only | 4096 | 24616 / 24616 | 8248 / 8152 |
| Windows x64, compile-only | 16384 | 98344 / 98344 | 32824 / 20440 |
| macOS ARM64, compile-only | 4096 | 20464 / 20464 | 8192 / 8096 |
| macOS ARM64, compile-only | 16384 | 81904 / 81920 | 32768 / 20384 |

Normal leaves LLVM IR unoptimized, while O2 uses default<O2>. Both use the current
AOT default machine optimization level, llc -O2, not llc -O0. Target DataLayouts
come from the pinned target compiler. Windows/macOS rows are compile-only, not
hosted results or native verification. This experiment does not reproduce ORC's
host CPU/features or prove its storage configuration matches AOT.

For the 4096-element Linux caller, explicit array homes total 32768 bytes, but the
machine frame is 81880 bytes. MIR additionally shows a 16384-byte hidden result
home, a 32736-byte maximum outgoing call area, scalar spills and callee-saved
storage. These frame categories must be inspected together; summing independent
reported fields can double-count overlapping accounting. The callee has fixed
incoming argument slots outside its reported local frame. No single DataLayout
payload multiplier or fixed overhead measured here is a complete ABI proof for
all numeric/bool elements, zero extents, surrounding value structs, signatures,
call sites, optimization modes and supported targets.

This is an integration hazard in a possible P23-2 extension, not a defect in the
locked P23-1 gate or evidence that the approved budget is unworkable. These modules
omit production root frames, source argument snapshots and construction staging.
Their measured frames are not proposed constants for NativeStoragePlanner. The
older native stack probe remains valid for its own four-home/root-frame experiment;
it did not exercise array-valued parameters or results.

Review recommendation: first prove a lowering inventory that makes array value
homes, argument snapshots and result handoffs explicit. Investigate an internal
address-based representation for new array-carrying values, preserving existing
array-free ABI and independent value snapshots. Compare that against retaining
raw aggregates with target machine-level call/storage auditing. Either approach
must account for all remaining ABI storage, validate its coverage against emitted
machine frames, and supply the same selected triple/DataLayout/CPU/features and
policy to analysis, LSP, JIT and AOT. Retain the approved preoptimization
explicit-home inventory: a smaller optimized frame must not replace that policy.
Machine measurements audit coverage, not authorize omitted homes. The exact
complete accounting mechanism remains unproved; neither approach is silently
adopted here. A const-formal borrow must also prove stable owner storage and no
later-argument/callee alias writes; otherwise
its explicit snapshot belongs in the inventory at ordinary argument evaluation.
Readonly metadata alone is insufficient.

The required proof gate therefore stops production work before enabling ArrayType,
native checked accesses, borrowing or iteration/range CFGs. No budget increase,
heap boxing, stack-reserve change, array-free restriction, receiver-chain repair,
string indexing or P23-3 through P23-6 work is included. Native feature regressions,
JIT/AOT agreement, strict sanitizers and the eventual hosted acceptance remain
required once this prerequisite is resolved. WORK_LOG.md records actual local
checks and artifact paths. Stop for boss/wrench review; no Git/remote mutations.

## 13. P23-2 explicit address-call investigation, 14 October 2026

Investigation only, following wrench review of Section 12. The approved per-function
65536-byte policy is unchanged. No production array lowering, ABI, compiler/runtime,
LSP or workflow has been changed. The reproducible tools/ci/array_abi_probe harness
now compares raw calls and a candidate explicit-address protocol, retaining the
original independently linked validation and assertions.

### 13.1 Candidate protocol and value/failure behavior

For the sampled mutable by-value array parameter/result:

- The caller owns input, an argument snapshot taken at ordinary evaluation time,
  and a distinct private result home. These are three target-sized homes.
- The candidate callee takes plain result/snapshot/failure pointers and returns
  void. It copies the argument snapshot into its own mutable local, then fills
  the result on success. No LLVM aggregate crosses the boundary; no byval/sret
  attributes request hidden copies. Memcpy intrinsics optimize normally.
- Result storage must remain private until the existing failure channel reports
  success. It must not be the final user-visible assignment destination while
  the callee can fail or reenter code that observes that destination.
- The scalar adapter remains i32(i32, ptr). A future internal call descriptor
  should be driven by signature types, not body contents. Adding array locals to
  a function must not change its existing array-free signature, receiver, scalar
  result or failure ABI. Mixed signatures still require accounting for unchanged
  raw non-array aggregate parameters. Virtual/interface dispatch, imports and
  leased reload calls must use the same descriptor, not argument-count guesses.

The C host preserves both original seed checks, checks every returned element and
requires fill/touch/check exactly twice before additional address tests run. The
new tests mutate the original owner during later-argument evaluation and verify
that the earlier snapshot and returned value remain independent. They check event
order, later-argument failure before invocation, callee failure after a partial
result write, suppression of result consumption on failure and one-frame trace
propagation using the existing message/count/32-frame shape. Normal/O2 negative
controls deliberately delay the snapshot or pass the changed original as an
unsafe borrow; all four must exit exactly 1 through the assertions. Signals and
unexplained failures are rejected. This is not production source-diagnostic proof.

### 13.2 Measured frames and the unchanged budget

All measurements use LLVM 19.1.7, target-derived DataLayouts and i32 size/alignment
4/4, witnessed by each target compiler. Normal has no LLVM IR optimization; O2
uses default<O2>. Both use the existing AOT default machine-code level, llc -O2,
with PIC. In this earlier driver, llc uses its empty CPU default; the generic
TargetMachine helper supplies layout/frame-prefix facts. Section 14 explicitly
selects generic for both C++ and independent llc emission and records the one
observed Windows semantic-frame difference. No noinline/volatile attributes,
suppression of optimization, heap boxing or stack-reserve increase are used.
Independent modules naturally preserve the call boundary; Section 13.3 also tests inlining.

Actual machine frame bytes for a 16384-byte array payload:

| Target / evidence | Raw caller normal / O2 | Address caller normal / O2 | Raw callee normal / O2 | Address callee normal / O2 |
| --- | ---: | ---: | ---: | ---: |
| Linux x64, native | 81880 / 81880 | 49192 / 49192 | 32776 / 20392 | 16408 / 16408 |
| Windows x64, compile-only | 98344 / 98344 | 49224 / 49224 | 32824 / 20440 | 16440 / 16440 |
| macOS ARM64, compile-only | 81904 / 81920 | 49200 / 49200 | 32768 / 20384 | 16416 / 16416 |

Raw 4096-element frame measurements above reuse Section 12's frozen artifacts;
the unchanged raw input modules were independently checked byte-for-byte. All
eight historical Linux native runs were replayed, including four 1 MiB runs.
Fresh raw 1024-element code generation at both modes/all three targets matches
all prior frames and retains the original native assertions. Address rows are
new measurements. Windows/macOS are compile-only, not native or hosted results.

For the address caller, the preoptimization homes are 49152 bytes. Its actual
frame also contains outgoing call storage, saved registers/spills and alignment.
At this size, measured non-home frame bytes are 40 Linux, 72 Windows and 48 macOS;
Windows reports a 32-byte outgoing call area already included in its frame.
A tiny LLVM C++ target-facts helper also queries stack alignment and the local-area
offset. For these profiles, the entry return slot outside F is 8 bytes on x64
and 0 on ARM64, consistent with the earlier native frame/stride evidence.
These observations are not proposed fixed allowances for copies/spills. Residuals change at other
extents and signatures.

The address samples at 5440 elements have 65280 bytes of explicit caller homes.
Their aligned inventory-plus-audit charges are 65328 Linux, 65360 Windows and
65328 macOS, all within policy. At 5461 elements, explicit homes total 65532 bytes,
aligned to exactly 65536 before ABI additions. Actual caller frames are 65576,
65608 and 65584 bytes respectively; final charges are 65584, 65616 and 65584.
All three are reported over-budget by the experimental auditor under the unchanged
policy despite a payload-only plan fitting. Normal/O2 agree for these samples. Callee storage is charged to the
callee, not added as a new call-chain limit. Over-budget samples execute only as
investigations; these tools enforce no production limit or maximum array length.

Address verification covers 24 target/extent/mode pairs and 48 principal frame
measurements. Linux independently linked AOT passes 16/16 runs, eight with hard/
soft RLIMIT_STACK=1048576 before exec. This is a main-process limit, not the older
probe's mapped worker stack. Eight additional Linux lli ORC runs pass with generic
CPU/machine O2 and independent IR modules. All 32 actual ORC frame warnings match
the four corresponding AOT/MIR functions per run. ORC uses the inherited process
stack limit. This verifies the configured standalone experiment, not production
VOID's current host-feature ORC configuration or a complete target factory.

### 13.3 Inventory reconciliation and remaining concrete gaps

The pinned-MIR auditor matches each explicit machine home to the preoptimization
inventory and records actual call areas, incoming slots, spills and saved-register
objects. For its supported fixed, nonoverlapping case:

```
P = unrounded original target-sized/aligned home inventory
R = bytes of machine objects matched to planned homes
F = actual machine frame bytes, including call areas/spills/padding
E = entry-area prefix from the supported target FrameLowering profile
candidate charge = align_up(P + F - R + E, target stack alignment)
```

For the pinned nonleaf X86/AArch64 profiles, E is minus getOffsetOfLocalArea,
queried from the selected generic/PIC TargetMachine, not an invented allowance.
Its stack alignment and DataLayout are checked against the target compiler.
This does not establish that recipe for other architectures or frame shapes.
Round only the final sum; frame fields already included in F must not be added
again. P remains charged when O2 eliminates a home. An actual dead-home test keeps
its 16 bytes in the charge: normal retains it, O2 deletes it. Untracked non-spill
objects, ambiguous extents, unsupported incoming areas, overlapping planned homes,
opaque/dynamic stack adjustments, tail calls and unproved realignment receive no
policy verdict. Red-zone or other target storage outside getStackSize also needs
explicit coverage; F-R is not a general-purpose formula for every LLVM function.
The MIR text parser is an investigation tool, not production infrastructure.

The MIR-only checkpoint exposed two gaps. Section 14 now resolves the specific
Windows five-argument ownership case and demonstrates C++ rejection of inlining
without provenance; general ABI and transformation-lineage integration remain open:

1. The five-argument failure callee on Windows has a 120-byte frame and an 8-byte
   incoming slot; its caller has a 264-byte frame with a 40-byte outgoing area.
   The auditor refuses to certify the incoming slot's ownership/extent rather
   than silently omit it or count a guessed universal call area. Address calls
   remove aggregate scalarization but do not remove scalar ABI obligations.
2. Combining the ordinary caller/callee modules and enabling default O2 produces
   an entry frame of 49176 bytes on Linux. The snapshot disappears and a new
   16384-byte home.i appears from the inlined callee. Native validation passes;
   the auditor rejects its missing provenance and issues no budget verdict.
   Production needs a proved replacement/reuse mapping or explicit added storage
   for introduced homes. It must retain the original preoptimization inventory
   and must not disable inlining or accept a smaller optimized frame as the proof.

The nine focused regressions additionally reject actual raw hidden result homes,
omitted or inexact inventories, understated frame sizes, untracked allocations
and unsafe snapshot protocols. Their artifacts include the real optimized modules
and machine records, not only synthetic arithmetic assertions.

### 13.4 Recommended shared production proof boundary

Recommend continuing toward explicit address-based passing of array-carrying
by-value arguments/results, with always-owned snapshots as the correctness path.
It removes the observed aggregate ABI multiplier while leaving existing array-free
calls intact. It is a viable foundation within 65536 bytes for the sampled cases,
not yet a complete production proof or approval to implement it in this turn.

Use one native preflight/emission service for the frontend/tooling and both native
engines. Its context must identify triple, DataLayout, CPU/features, relocation/
code model, target options, LLVM version and IR/machine optimization profile. Do
not reuse the generic measurements for the current host-feature JIT. LSP must
select the same active context and invoke that service, not port the Python
layout arithmetic or introduce another calculator. Exact native dry-run audits
may need caching/cancellation; latency must not become permission to skip checks.

The preoptimization producer must enumerate actual lowering choices with stable
home identities, roles, target extents/alignment and lifetimes. It includes source
locals, argument/result snapshots, instruction copies, roots/Frame/Slot records,
construction staging, call helpers and source failure/native guards. Every role
must be accounted, including those proved empty. Preserve that plan through
optimization. Track allocation deletion, cloning, inlining and storage replacement;
unknown lineage is a failed proof, not an empty category.

Audit the real MachineFunction/FrameInfo from the same emission that will publish
code, after target frame placement and before publishing the object. LLVM 19.1.7
provides getObjectAllocation/Size/Align/Offset, object indices, getStackSize and
getMaxCallFrameSize; TargetPassConfig exposes insertion after
PrologEpilogCodeInserterID. Section 14 verifies a later C++ boundary after all
machine passes, with
post-AsmPrinter and encoded-object checks; the PEI insertion point alone is not
sufficient for final certification. Reconcile complete object extents, fixed
incoming/outgoing ownership, red zones, realignment, saved registers and spills;
prove supported padding/reuse rather than adding a measured constant. Verify
target-generated prologue/epilogue and stack-probe scratch, including any frame
changes after the chosen audit point.

Both AOT and a custom ORC compile callback should use that same audited emitter.
LLJITBuilder's CompileFunctionCreator is the appropriate proposed boundary; IR
transforms happen too early, and an object transform alone has lost AllocaInst/
MachineFrameInfo provenance. Reject unaccounted storage or an over-budget plan
before symbol lookup/publication or final object delivery. Cache attestations only
with matching target/profile, IR/lowering and ABI identities. This integration is
not implemented or claimed proved by invoking lli. Independent IR validation and
ordinary source failure/diagnostic contracts remain mandatory.

### 13.5 Roots, staging, borrowing and review boundary

Scalar i32 samples do not prove managed-owner liveness, mixed value layouts with
existing reference fields, GC root maps, Slot/Frame ABI, complete construction
publication or failure cleanup. Inventory these using the same selected target
layout and existing walkers/runtime contracts. Reference-containing future result
homes need complete zero initialization and registered roots before safepoints,
with private publication only after success. No reference arrays are implemented.

Const-formal optimization remains a separate proof: snapshot first at ordinary
argument evaluation unless later arguments and all callee aliases cannot change
those bytes. A readonly spelling alone does not establish that. Borrowed storage
must retain its owner/root, stable address and lifetime through polls, calls and
failure cleanup. Independent IR verification must enforce the proof, and compatible
reload must not invalidate an already-generated caller's assumptions about alias
writes. Otherwise retain an owned snapshot and charge its copy. No borrow is
implemented or inferred safe from the pointer ABI experiment.

Stop for boss/wrench review. Production integration, inlining provenance, general
ABI-slot ownership and GC/staging/borrow proofs remain open implementation gates.
The numerical limit, all 60 suites, normal CI and P23-1 remain unchanged. No
production arrays, later checkpoints or unrelated hardening have been started.

## 14. P23-2 C++ machine-storage certification proof, 14 October 2026

Focused implementation proof following the approved review of commit 47ebbeb.
The 65536-byte policy and all production code, ABIs and ordinary CI are unchanged.
This section advances the investigation in Section 13; it does not certify VOID's
production emitter or authorize later array checkpoints.

### 14.1 Actual audit boundary and immutable inventory

The new standalone machine_emit.cpp uses the LLVM 19.1.7 C++ SDK and the same
SelectionDAG/legacy machine-code pipeline exposed by LLVMTargetMachine. It freezes
all original static AllocaInst homes before any IR optimization, using the selected
TargetMachine DataLayout, allocation count and alignment. WeakVH retains original
identity without following RAUW replacements. Deleted homes remain charged in P;
a clone, matching name or matching extent is not original-allocation proof.

An early MachineFunction pass records actual call-frame setup sizes. An observation
pass immediately after PrologEpilogCodeInserterID records the placed frame. The
certifying pass runs after TargetPassConfig::addMachinePasses() has added the
entire target machine pipeline, immediately before addAsmPrinter(). The latter
boundary is necessary: PEI precedes late scheduling, placement, outlining, CFI work
and target addPreEmitPass2. The public pipeline sequence was checked against the
pinned [LLVMTargetMachine.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/LLVMTargetMachine.cpp)
and [TargetPassConfig.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/TargetPassConfig.cpp).
No optimizer, inliner or machine pass is disabled.

For every MachineFunction, including originally array-free functions, the auditor
reads live frame objects, exact original-allocation pointers, sizes, alignments,
fixed/incoming slots, spills, offsets, stack size and outgoing area. Exact,
nonoverlapping original homes alone can earn R. Unknown non-spill objects fail;
spills/saved-register storage stays wholly in F. Objects outside the accounted
frame, dynamic/opaque frames, tail calls and unsupported realignment fail. Missing
inventories also fail, including newly generated functions. No source name is an
identity and no measured residual becomes an overhead allowance.

The charge remains C = alignUp(P + F - R + E, A), rounded once. E comes from the
supported target FrameLowering entry prefix (8 x64, 0 ARM64); A is 16. Homes are
not rounded to the final stack alignment individually. Incoming caller-owned
storage is not silently folded into R or added again to the callee's F.

Real lifetime-based coloring merges two 256-byte original homes into one machine
object on all three targets, at both modes. This prototype grants **zero R to the
entire function when lifetime markers indicate possible reuse**, including markers
introduced by optimization. Its P remains 512. This is conservative certification,
not a proof that pointer identity establishes merged-home lineage. Exact reuse
credits remain deferred. Dead-home controls retain their original 16 bytes in P
whether normal preserves the home or O2 deletes it.

Ordinary O2 inlining into originally_array_free introduces a real array home into
an original P=0 function. Its clone receives R=0 and certification fails. Likewise,
combining the original address caller/callee modules retains the caller's original
P=49152 and rejects inlined homes. There is no array-free-function exemption,
renaming workaround or optimization suppression.

### 14.2 Final object checks and publication gate

Objects remain in a private memory buffer. After AsmPrinter, another actual C++
MachineFunction pass compares the frame/object/SP/call snapshot with the late
certificate before LLVM frees the MachineFunction. The prototype then parses the
emitted ELF/COFF/Mach-O object and uses the selected LLVM MC disassembler to compare
each function's encoded stack-pointer writes and call opcode sequence with the
certificate. Function symbols must cover certified functions; Mach-O section-anchor
aliases are accepted only for an identical certified section/address/byte extent.
Undecodable code or an unmatched function fails.

A real final-emission change occurs: x86 ADD64ri32/SUB64ri32 can be encoded as the
imm8 forms. Only these two equivalent encoding families are normalized, and only
with identical decoded register operands and signed immediate values. Other opcode,
operand, frame or stack-effect differences fail. This is an explicit encoding
proof, not permission to ignore late stack changes. The PEI observation also sees
later changes to epilogue instruction sequences in normal x64 semantic samples
(the independent llc trace identifies branch-folder merging three caller epilogues
into one, reducing SP-writing instructions from 20 to 10 without changing F);
therefore PEI alone is not the final certificate boundary.

Deliberate late changes to MachineFrameInfo and SP instructions both fail after
emission and deliver no object. A Linux-only fixture alters the actual encoded
SUB-SP immediate after the post-AsmPrinter check; the MC check independently
rejects it. Omitting a real snapshot from the original inventory fails against the
actual machine allocation. Over-budget and incomplete certificates return 2 and
never create the requested object. Successful delivery uses a private temporary
file and rename after all checks. The certificate binds the final bytes by SHA256;
existing delivery paths are refused to avoid stale-object false positives.

Linux independently links only certified caller/callee objects with the unchanged
C assertions. Standalone lli ORC also loads those already certified objects. This
demonstrates the object delivery gate on two consumers, not production ORC IR
compilation, source diagnostics, executable-memory publication hooks or cache/reload
integration. Production must keep its attestation and emitted buffer together
through the shared emitter and ORC compile callback before object linking or lookup.

### 14.3 Windows five-argument ownership, measured rather than assumed

The exact investigated C signature is void(ptr result, ptr snapshot, i32 later,
i32 mode, ptr failure). Independent caller/callee modules preserve this boundary
at normal and O2. Actual pre-PEI call-frame setup is 40 for checked_roundtrip and
32 for the caller's ordinary register-only helper calls. The late caller contains
a real 8-byte store at outgoing SP+32 immediately preceding that direct call.
The same slot is an 8-byte fixed incoming object at LLVM frame offset 32 in the
callee; TargetFrameLowering resolves it to post-prologue RSP+160 with F=120,
or callee entry SP+40. Thus:

| Interval / coordinate | Ownership |
| --- | --- |
| Caller pre-CALL SP+[0,32) | Caller-owned shadow area |
| Caller pre-CALL SP+[32,40) | Caller-owned fifth argument |
| Callee entry SP+[0,8) | Return address, covered by callee E=8 |
| Callee entry SP+[8,40) | Same caller shadow area |
| Callee entry SP+[40,48) | Same caller fifth argument |

The 40-byte outgoing area is already in caller F. The incoming 8 bytes are outside
callee F; charging them again to the callee would duplicate caller-owned storage.
The callee has its own 32-byte outgoing area for touch, already included in its
120-byte F. The return slot is not shadow space and is not subtracted from F.

The prototype accepts this incoming slot only with the exact supported signature,
target, offset/extent and a previously certified actual caller edge with the
40-byte setup and SP+32 store. The imported local certificate is bound to its
actual delivered caller object hash. Missing proof, shadow-only evidence, missing
store evidence or a stale object hash cannot certify the callee. This resolves
this probe's five-argument ownership, **not** arbitrary Windows signatures,
varargs, aggregates, vector conventions, indirect calls or exported entry points.
A production ABI descriptor and closed call-edge ownership model remain necessary.

### 14.4 Measurements, verification and limits

All results use LLVM 19.1.7, generic CPU, empty explicit feature string, PIC,
CodeGenOptLevel::Default (machine O2), normal IR or target-aware default<O2> IR.
The C++ target DataLayout must equal the target-compiled layout witness. Windows
and macOS results below are **cross-compilation and object decoding only**.

For the original 4096-element, 16384-byte payload, both normal and O2 give:

| Target / evidence | Caller P / F / R / E / C | Callee P / F / R / E / C |
| --- | --- | --- |
| Linux x64, native | 49152 / 49192 / 49152 / 8 / 49200 | 16384 / 16408 / 16384 / 8 / 16416 |
| Windows x64, compile-only | 49152 / 49224 / 49152 / 8 / 49232 | 16384 / 16440 / 16384 / 8 / 16448 |
| macOS ARM64, compile-only | 49152 / 49200 / 49152 / 0 / 49200 | 16384 / 16416 / 16384 / 0 / 16416 |

At 5440 elements, caller C is 65328/65360/65328 for Linux/Windows/macOS;
at 5461 it is 65584/65616/65584 and object delivery is rejected on every target
and mode. The limit remains 65536; these are per-function charges, not call-chain
or allocation limits. The 5461-element caller is not linked or executed by this
certifying experiment.

The five-argument semantic callee F/C is 104/112 Linux, 120/128 Windows and
112/112 macOS. Its caller F/C is 232/240, 280/288 and 240/240 respectively.
The Windows caller F=280 differs from Section 13's empty-CPU llc result of 264:
independent llc -mcpu=generic reproduces 280. CPU selection affects copy selection,
alignment and frame layout. These profiles must not be conflated or turned into
fixed allowances. Real reused-slot functions have P=512, R=0 and F/C of 264/784,
296/816 and 288/800, both modes, with all original homes still charged.

Evidence and exact test counts are recorded in the rolling work log and the
harness README. Source snapshots, input/optimized IR, machine facts, disassembly,
object hashes, native binaries, command statuses and deliberate rejection artifacts
are reproducible from tracked sources. No full compiler matrix, hosted execution,
Windows/macOS native run or ordinary CI change is claimed.

### 14.5 Recommendation and remaining integration obligations

Proceed next, after review, with a shared production storage-plan/audited-emitter
integration checkpoint, using the exact target/profile for both preflight and
emission. Keep explicit-address snapshots/results as the correctness path and
preserve array-free ABI. The proof establishes a usable late MachineFunction and
encoded-object gate for these profiles, with conservative rejection available
where optimization lineage is incomplete. Do not port the MIR parser into LSP.

Before accepting production array code, inventory every real LLVM/ABI home,
parameter/result staging area, instruction copy, native guard and GC Frame/Slot/root
using the actual shared target layout. Prove zero-sized and other scalar layouts,
existing non-array aggregates, calls and frame shapes or reject them explicitly.
Inlining/cloning/merging needs durable transformation-aware lineage, or conservative
no-credit accounting with explicit added homes where those extents are proved.
Unknown origins remain failed certification, including in formerly array-free
functions. Repeated metadata and root walkers must agree with complete publication;
reference-containing arrays remain outside this checkpoint.

Readonly is still insufficient to justify borrowing. Prove no conflicting writes
from later argument evaluation, callee aliases, reentrancy or reload, as well as
owner/root lifetime and stable addresses through safepoints and failure cleanup.
Otherwise take and charge independent snapshots. Existing result failure and
partial-construction publication rules remain mandatory. None of these GC or borrow
proofs is established by the scalar native harness.

Production finalization must fail before AOT delivery or ORC object publication,
including unsupported late transformations and cache/profile mismatches. Extend
the small supported MC encoding checks deliberately; do not replace missing proofs
with overhead constants. Native Windows/macOS integration and actual host-feature
JIT validation remain required later. Stop for boss/wrench review before production
array lowering or any later Phase 23 checkpoint.

## 15. Production audit integration Part 1: staged, activation stopped

Historical first-integration boundary. Section 16 records the subsequent restricted
proofs for these two shapes. Activation remains disabled by owner instruction.

The first compiler-service integration was authorized after Section 14. This
checkpoint is **incomplete**: broader real AOT generation exposed two unproved
existing storage shapes. The attempted activation rejected real objects. Per the
owner's stop instruction, the original AOT emission path was restored without a
certification fallback or an exemption for the offending homes. The staged service
is compiled into void_llvm and exercised through compiler-level fixtures; ordinary
emit_object/emit_runtime_object do not yet use it. No source native-array execution,
ORC/LSP integration, GC completeness or readonly borrowing is claimed.

### 15.1 Staged compiler service and lowering interface

include/void/llvm_storage_audit.hpp and src/backend/llvm/storage_audit.cpp extract
the actual late MachineFunction/MC mechanism into an owned, single-use AOT emitter.
Its factory pins LLVM 19.1.7, native target triple, generic CPU, explicit feature
string, DataLayout, PIC, actual code model, machine Default/O2, factory-owned
TargetOptions defaults and IR optimization identity. Existing production AOT uses
PassBuilder without a TargetMachine for default<O2>; that exact identity is kept,
not silently replaced by the standalone proof's target-aware optimizer.

The lifecycle is freeze original inventory, optimize once, emit into private
memory, late machine audit, post-AsmPrinter comparison, final MC comparison, then
hash-bound temporary-file delivery and rename. Failure leaves an existing output
untouched. The standalone experiment's injected faults and imported JSON ownership
mechanism are not production escape hatches. In particular, unproved Windows
incoming slots in policy-applicable functions still fail; the exact standalone
five-argument witness is not generalized into a production ABI allowance.

LoweringInventory declares each function's array applicability and every original
AllocaInst by distinct identity, StorageRole and HomeKind. Home kinds distinguish
arguments, results, copies, value/root homes, Frame, Slot records, staging and native
guards. Each function must explicitly close all eight existing storage categories,
including empty categories; omitted allocations, missing functions, duplicate
allocation/identity records and foreign allocations fail before optimization.
Actual allocation type, constant count and alignment determine target extents.
The existing shared NativeStoragePlanner now retains unrounded_bytes as well as
its previous rounded total, so the service uses that planner for P rather than
adding another estimator. The 65536-byte final machine policy is unchanged.

The existing shared lower() now accepts an optional inventory sink and records
its real allocation sites with their purposes. Its current native-array guard is
unchanged. Ordinary AOT/JIT currently leave this staged sink unused. Adapter homes
were inventoried during the failed activation experiment; that activation was
withdrawn, not left enabled behind a test-only flag or a permissive exception.
Other unfinished interface work includes stable source-diagnostic association,
complete ABI classification, cache/profile hardening and proving the actual GC
layout/walker and lifetime contracts. Purpose labels alone do not establish them.

The staged auditor keeps P immutable, credits only exact original physical homes,
and grants zero reuse credit when lifetime markers permit coloring. Original
array-call graph closure retains policy applicability in previously array-free
callers. Removed array calls/functions require transformation-lineage proof; unknown
inline allocations remain failed certification. Pure array-free functions receive
no new array budget verdict (C is null), but the attempted emitter still refused
inexact original machine homes. No size exception was introduced for existing ABI.

### 15.2 Actual production activation blockers

The full Debug build reached existing managed CLI object generation and failed
closed on examples/managed_structs/main.void and handoffs.void at normal optimization.
The production module and TargetMachine were the actual public AOT path, not a
cross-target surrogate. Concise original machine evidence is retained in
build/p23-2-audit-activation-blocker.json with the full failure log alongside it.

| Existing home | Original target extent / alignment | Actual machine evidence | Missing proof |
| --- | --- | --- | --- |
| CLI input-parse pointer home | 8 / 8 | main object size 0, alignment 1 | Conditional non-entry alloca becomes dynamic SP storage |
| CLI RuntimeFailure home | 272 / 8 | main object size 0, alignment 1 | Conditional non-entry alloca becomes dynamic SP storage |
| Empty GC Slot array in position.bump | 0 / 8 | Machine object size 1, alignment 8 | Zero logical extent versus addressable physical object |

For main, original P=280 but MachineFrameInfo reports F=40 and two zero-sized
non-spill objects. Actual SP writes include register-to-SP moves and dynamic
adjustments; F alone is not the original homes' physical bound. They are not
eliminated homes and cannot receive zero-size subtraction credit. Establishing a
bound requires actual target stack-adjustment/alignment and CFG/lifetime evidence,
including why conditional allocations execute a bounded number of times. Blindly
substituting P+F or a fixed alignment allowance is not a proof.

For position.bump, P=40 comprises two 4-byte value homes, the empty Slot array,
and a 32-byte Frame home. F=88 contains an actual 1-byte Slot object rather than
the planned zero bytes. That physical byte is not a reason to normalize source
extents, change zero-length language semantics or credit a nonexistent logical
byte. A future extension may conservatively retain actual physical overhead in F
with zero R for the logical empty home, but must prove original identity/target
behavior and address/lifetime safety first. No such extension is adopted here.

Focused compiler regressions reproduce both shapes and require no-object delivery.
They retain the rejected certificates. No production/runtime ABI was changed,
allocas were not hoisted, and neither mismatch was suppressed to make activation
pass. The approved numerical limit was not raised or reinterpreted.

### 15.3 Review boundary and next step

Review the staged service, original-home interface and blocker certificates before
activation. The next necessary proof is narrow: reconcile existing conditional
homes and zero-extent physical homes against actual target lowering, or review a
semantics-preserving lowering change that removes those shapes. Do not adopt
blanket dynamic-frame acceptance, guessed spill/ABI allowances, zero-byte credit,
or a general incoming-slot exception. Preserve independent snapshot, source
failure, root/Slot/Frame, debug and reload contracts while investigating those cases.

Only after that proof should the public AOT path be switched to the shared
certified emitter and all existing native/CLI suites rerun with activation enabled.
Passing tests on the restored original emitter do not certify the staged service
for those programs. Additional target/profile/optimization and compiler negative
checks remain necessary before ORC, tooling or native arrays can advance. Actual
verification results and any unresolved test uncertainty are recorded in WORK_LOG.md.
No CI, commits, pushes, branches or remote changes are part of this checkpoint.

## 16. Conditional and zero-extent storage-shape proof

The two Section 15 mismatches now have narrow, tested solutions in the staged
service. **Public AOT remains on its original emission path.** The explicit
emit_aot_storage_proof entry reuses that path's actual module/adapter producer,
then invokes the service. There is no fallback from failed certification to normal
emission, no source native-array permission, budget change or ORC/LSP integration.

### 16.1 Conditional homes: bounded compiler-owned entry storage

LLVM's pinned SelectionDAGBuilder::visitAlloca selects dynamic allocation for
non-entry homes absent from StaticAllocaMap. That path rounds each allocation to
target stack alignment and emits DYNAMIC_STACKALLOC; repeated block execution can
repeat the allocation. This explains why constant size alone did not make the
original CLI homes static. See the pinned
[SelectionDAGBuilder.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp#L4075-L4128).

The solution does not attempt a general dynamic-stack bound. The shared adapter
producer explicitly identifies its hidden parse-pointer and RuntimeFailure homes
with cli_parse/cli_failure kinds. Only the staged service normalizes those homes.
The public producer still leaves the original allocations where they were.

Before moving an allocation, the service requires the compiler-owned main C
signature, exact pointer/failure shape and count-one allocation, default address
space, original ABI alignment and known_abi role. Reachability must show the
original block is executable and cannot reach itself through any nonempty CFG
path. This handles self-loops, multi-block cycles and irreducible reentry without
assuming a source trip count. Stack save/restore or returns-twice control in the
function rejects the transformation. Variable counts, unexpected types and extra
alignment reject as well. Arbitrary source homes do not acquire this permission.

All inventory validation completes before moves. The same AllocaInst moves to
entry; it is not cloned or replaced. Its type, count, alignment, debug attachments,
all users and original identity remain. No initializer, lifetime marker, load,
store, call, branch or failure check moves. The microfixture explicitly checks
that initializers stay in parse/run. Each original allocation executed at most
once per function invocation and, without stack restoration, remained allocated
until return. Hoisting extends only the hidden ABI allocation's availability;
initialization and use still occur at their original semantic points.

These homes are owned by the adapter, not addressable VOID values. The parse
pointer is borrowed by strtoll, and the failure home is initialized before the
checked entry/metadata invocation. Neither is registered as a managed root or
made visible before its original initialization. Reserving entry storage adds no
source evaluation, publication, lifetime cleanup or failure-channel event. Tests
compare actual CLI success, invalid input and source failure behavior against the
original objects. Recursive call-chain storage remains outside a per-function
policy; the CFG proof concerns one invocation, not a universal recursion bound.

After normalization, the machine certificate requires no variable/opaque frame.
For moved CLI homes it also rejects SP-defining body instructions outside target
FrameSetup/FrameDestroy regions. The exact normal/O2 main frames have only static
prologue/epilogue SP changes. The final object decoder must agree with that late
machine snapshot. Thus every relevant CFG path, including the trace-printing loop,
uses the one measured static frame; there is no repeated dynamic allocation to
estimate or charge using an invented allowance.

For the actual Linux x64 managed main/handoffs adapters:

| IR mode | Original P | Measured static F | Entry E | Own frame F+E |
| --- | ---: | ---: | ---: | ---: |
| Normal | 280 | 312 | 8 | 320 |
| O2 | 280 | 328 | 8 | 336 |

The existing outgoing argument area is inside F. Callee frames and their entry
return slots retain their existing ownership; they are not added to this function's
frame bound. These array-free adapters have C=null, not a new array-budget verdict.
A policy-applicable microfixture has P=280, F=280, R=280, E=8, C=288 at both modes.
No physical residual is promoted into a fixed allowance.

Unmarked conditional homes still fail. The exact original main/handoffs LLVM
snapshots, with no CLI-home declaration, reject while preserving an existing output.
Cyclic/repeated, variable-count, over-aligned, wrong-shape and stack-control cases
also reject before any object can be delivered. General bounded-loop/dynamic
allocation acceptance is not implemented.

### 16.2 Zero-extent homes: original identity and zero subtraction

The cause is explicit in pinned LLVM 19.1.7
[FunctionLoweringInfo.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/SelectionDAG/FunctionLoweringInfo.cpp#L143-L159):
static allocation size is computed from DataLayout and count; a zero result is
raised to one physical byte before CreateStackObject, with the original AllocaInst
retained as provenance. This is backend addressable storage, not a one-element
language array. It is not a general permission to round logical homes upward.

The staged auditor accepts this specific mapping only for the exact surviving
original static allocation, still logically zero in its current LLVM type/count,
a measured one-byte non-spill machine object, sufficient original alignment, unique
provenance and a physical range inside F. It records logical_bytes=0,
physical_bytes=1 and subtraction_credit=0. P and the original LLVM zero-extent type
remain unchanged. The byte and its placement/alignment cost stay in measured F;
no subtraction credit is awarded for the empty home. Unknown/cloned zero homes
remain rejected in policy-applicable code, and dynamic zero-size homes are not
exempted from the global dynamic-frame rejection.

The exact position.bump fixture retains P=40. Its normal/O2 F is 88/72, including
the one-byte empty Slot object. No IR transformation is made for that Slot array.
The root-array type/count stays empty and the Frame/Slot ABI is unchanged. Existing
Frame construction and collector walkers iterate by count; count zero never
reads a Slot record. A direct collector regression nests a zero-count Frame with
null Slot storage above a live parent root, verifies collection preserves that
root, and verifies collection releases it after the parent Frame leaves.
Native managed-cycle, handoff and failure comparisons plus
existing runtime/source regressions verify the exercised behavior. This does not
prove future reference-bearing array storage, GC publication or readonly borrowing.

### 16.3 Actual verification and remaining review gates

Focused compiler cases include the exact managed_structs/main.void and
handoffs.void modules, normal/O2. The reproducible native driver additionally uses
null_failure.void. It independently links public-original and staged-certified
objects against the same runtime. Across Debug and Release, all 192 comparisons
(384 process executions, half with a 1 MiB stack) agree on exit status/stdout/stderr.
Invalid input, i32 range failures and actual source-failure reporting are included.
No failed certificate or source native-array program is executed.

Debug and Release full matrices pass 61/61 with production activation disabled.
Strict ASan/UBSan with leak detection passes the four focused audit/foundation/
runtime-generated/interface suites. Final added exact-source rejection cases pass
in all three builds. WORK_LOG.md records exact times, logs and artifact locations.
The Linux results are not Windows/macOS native execution or hosted CI acceptance.

The earlier Debug interfaces_source segfault remains **unexplained, not fixed or
attributed**. This investigation ran six serial/eight two-worker repetitions before
progress changes, eight serial/24 four-worker repetitions with flushed phase/mode
progress, two serial/eight four-worker final repetitions after making core-limit
inheritance safe for a threaded driver, and six native GDB repetitions with ASLR
explicitly enabled. All 62 controlled runs passed without a signal or core; full matrices and sanitizers also
passed. The tracked runner isolates logs/core destinations and treats every signal
or timeout as failure. No assertions, native optimization, test body or coverage
were removed. If it recurs, the last completed phase and original crash must be
preserved; a successful retry cannot erase it.

Review these restricted proofs before any activation step. The entire AOT fixture
corpus still needs certification-path validation, and new-shape native Windows/
macOS acceptance remains open. General incoming ABI ownership, profile/cache and
source-diagnostic hardening, optimization lineage, full root/staging inventories,
GC safety and borrowing/reload contracts retain their existing gates. Public
emit_object/emit_runtime_object continue to choose the original branch, not the
proof entry. See tools/ci/STORAGE_SHAPE_PROOF.md for tracked reproduction commands.

## 17. Production AOT certification Part 2: corpus and binding gates

15 October 2026. Owner reports hosted CI #41 passed all seven jobs, 61/61 suites
per configuration, locking Section 16. This checkpoint keeps public AOT on its
original path. The approved 65,536-byte policy is unchanged; source native arrays,
ORC/LSP activation and later checkpoints remain blocked.

### 17.1 Exact production producers and fail-closed scope

The explicit proof entry now selects the existing source CLI, architecture adapter
or runtime-library producer, including the existing reload generation suffix.
Fixture-only --storage-proof arguments reuse the original seven family/library
producers, the three edited reload producers and runtime_test::proof(). There is
no second frontend, alternate module builder, environment activation switch or
public CLI activation. Original arguments still choose emit_object or
emit_runtime_object and their unchanged original emission branch.

The tracked aot_certificate_corpus.py driver reads configured integration commands
from CTest JSON and family/program lists from CMake. It captures each successful
ordinary emission immediately, before generated/imported sources can be edited
again. It exercises both profiles for the architecture and edited-source fixtures
that ordinarily emit only O2. All other corpus producers already provide normal
and O2. Source-invalid diagnostic cases retain their original checks and are not
counted as native fixture candidates.

Unknown non-spill machine homes and unproved incoming-object ownership now reject
array-free candidates too. The previous array-free exemption did not constitute a
storage-lineage or general ABI proof. Current certificates retain every unmatched
index, physical extent/alignment, current alloca text or absence of an alloca,
and zero subtraction credit. Dynamic/opaque frames still reject. Every remaining
LLVM definition must have PEI, late and final-emission machine coverage; emitted
function bytes must still match the late SP/call certificate. An actual
available_externally definition with no emitted machine function demonstrates
incomplete-coverage rejection.

A failed attempt must preserve an existing sentinel destination. It cannot fall
back to the public emitter, be linked or execute. Ordinary emissions in the
investigation are separate control objects, never replacements for failed proof
outputs. Array-free acceptance keeps C=null and does not newly impose the array
budget or prove future array argument/result/root/borrow accounting.

### 17.2 Owned profile, inventory and object identity

One single-use service owns the TargetMachine and its actual target triple, generic
CPU, features, DataLayout, relocation/code model, machine optimization and pinned
LLVM 19.1.7 options. Its profile distinguishes verify-only IR from the unchanged
PassBuilder(no-TM) default O2 pipeline. Per-function target-cpu, tune-cpu,
target-features or target-abi attributes that disagree with the owned profile
reject before emission. Target triple/DataLayout are checked again at each boundary.

After the approved CLI-home normalization, freeze records the exact normalized
module SHA-256 and an owned serialization/hash of original home identities,
roles, kinds, target-derived extents/alignment, P and policy applicability.
Optimization rejects intervening caller edits, then records its exact output
module SHA-256. Delivery rejects later edits before instruction selection. The
original allocation WeakVH identities remain independent of these content hashes;
matching hashes do not manufacture missing transformation lineage. A caller's
later changes to its LoweringInventory cannot change the frozen service.

The resulting certificate includes the owned profile, serialized inventory,
both module hashes and the private emitted object SHA-256. The corpus driver
independently checks those hashes and optimization identity against saved IR and
delivered bytes for each accepted candidate. Machine reconciliation, post-printer
comparison and final encoded SP/call checks remain mandatory before delivery.
No serialized certificate import, external cache acceptance or cryptographic
attestation is implemented or claimed. Reuse across modules/profiles is forbidden
by the service lifecycle, not enabled by a hash-only cache.

The corpus also found a staged report lifetime defect: entry_home_moves stored a
StringRef to an original basic-block name. O2 could delete that block before JSON
serialization, leaving invalid UTF-8 in original_block. Freeze now owns that name.
The CLI microfixture checks that both parse/run names survive normal/O2, and corpus
logs/certificates use strict UTF-8 decoding. This reporting correction changes no
source, runtime, public AOT or array semantics. It does not attribute or resolve the
historical interfaces_source crash. That suite's phase/mode/core instrumentation
is preserved.

### 17.3 Confirmed blockers and review boundary

Existing production value-struct functions require incoming stack-object ownership
that the general service has not proved. For value_structs/main.void, choose has
two incoming objects (8 bytes at entry SP+16 and 1 byte at entry SP+8), P=49 and
F=0 in the normal Linux x64 fixture. Their presence is measured; no caller/callee
ownership assumption is awarded. Both normal/O2 main and numeric fixtures now have
focused destination-preserving rejection tests.

The normal numeric entry additionally has a measured 64-byte, align-8 non-spill
object with no LLVM alloca identity (index 47, P=480, F=600). Its current interface
cannot prove this object's origin. O2 also leaves unmatched current allocas in
existing managed functions and inlined receivers, including main wrappers. Current
size, source spelling or similarity to a deleted home cannot prove that lineage.
The service rejects rather than crediting such homes or disabling optimization.
Original lifetime-colored storage reuse still passes its focused fixture with P=512
and R=0. Array-free and policy-applicable inline-clone negatives both reject.

The normal numeric entry's final encoding also changes PUSH64i32:i1 to PUSH64i8:i1.
All other recorded SP operands in that example agree. This equivalence is not in
the proved opcode normalizer, so final-byte certification rejects it as well.
No broad opcode exemption or unproved emission acceptance was added. This is a
separate remaining encoding-proof obligation, not evidence of a source compiler
miscompile.

Stop production activation here. The next proof should establish general incoming
ABI ownership and trace original storage through actual optimizer/codegen changes,
including ABI-generated homes without AllocaInst identities. Exact signed-push
encoding equivalence also needs its own narrowly tested rule before acceptance.
No guessed ABI allowance, new estimator, budget increase, inlining suppression,
readonly/GC shortcut or receiver-chain correction is justified by this corpus.
Full target-native Windows/macOS coverage and future array/root/staging/borrow
contracts remain gates. Historical Section 16 accepted O2 managed observations
must not be read as acceptance under this stricter unknown-home requirement.

### 17.4 Actual corpus results and storage inventory

Native Linux x64, installed LLVM 19.1.7, Debug compiler build. All 15 configured
AOT-related integration scripts pass their original checks. The separate complete
corpus contains 334 staged attempts, 167 at each profile: 247 accept and 87 reject.
These are investigation results, not approval of activation.

| Producer | Normal attempted / accepted | O2 attempted / accepted |
| --- | ---: | ---: |
| Integration source/architecture, including generated edits | 104 / 100 | 104 / 100 |
| Seven families' source CLI programs | 52 / 41 | 52 / 1 |
| Runtime libraries | 7 / 3 | 7 / 0 |
| Edited reload generations | 3 / 1 | 3 / 0 |
| Generated runtime IR | 1 / 1 | 1 / 0 |

Across 1,972 observed machine functions, the complete reports retain every live
frame object, original home purpose/extent, incoming object, call area, SP writer,
post-PEI observation and final encoded comparison. The driver groups 32 combinations
of recorded home kinds, CLI moves, zero extents, lifetime markers, incoming and
unmatched objects. These combinations are an index into the full per-object reports,
not an exhaustive taxonomy of future storage behavior.

Observed original kinds are value, argument, root, Frame, Slot array, staging and
both CLI ABI homes. No evidence here establishes future fixed-array result/copy,
reference-bearing array or borrow inventories. There are 310 CLI-home move records
at function level, 29 functions with exact zero-home mappings, 40 with lifetime
markers, 14 with incoming objects and 309 with unmatched objects. Zero-byte homes
retain zero subtraction credit. There are 24 distinct unmatched physical
size/alignment/alloca-presence combinations, including allocations without any
AllocaInst identity. Full extents and identities remain in each certificate.

No corpus frame has variable/opaque storage after the narrowly approved CLI
normalization. Dynamic/opaque negatives remain in the focused suite. There are
575 functions whose machine snapshot changes after PEI, demonstrating why the late
boundary and final-byte check remain necessary. There are also 395 functions with
local objects below the measured local-frame range, including ordinary Linux
red-zone storage. These array-free reports have C=null; they do not certify a
64 KiB physical bound or general red-zone ownership. Policy-applicable fixtures
continue to reject objects outside their accounted local frame. General target/ABI
integration remains blocked, rather than extending array policy to ordinary code.

Rejection reasons overlap by module: unmatched allocations occur in 82 candidates,
unproved incoming ownership in 14, final encoded SP mismatch in two and final call
mismatch in three. The last category occurs in O2 inheritance stress CLI/library/
reload virtual thunks. The late machine list includes TAILJMPd64, while the final
MC call list contains four CALL64pcrel32 entries and no tail-call marker. SP lists
agree for that thunk. No equivalence between a target tail pseudo and final branch
classification is assumed. A focused actual inheritance-stress fixture retains
this rejection and protects an existing destination.

All 242 accepted CLI candidates are independently linked and compared against
ordinary control objects for six argument cases: 1,452 exact exit/stdout/stderr
comparisons, 2,904 native executions. Four complete native assertion harness pairs
also pass (managed, managed_initialization, inheritance and runtime, all normal).
The other 12 harness configurations are explicitly recorded as rejected/not linked;
a partly accepted library/reload pair is never executed. The corpus does not run
new 1 MiB tests or claim the historical Section 16 small-stack witness for newly
rejected O2 objects. No Windows/macOS native or cross-target execution is claimed.

Evidence and reproduction: tools/ci/STORAGE_SHAPE_PROOF.md and
build/p23-2-part2-corpus-final/{complete.json,records.jsonl,shape-inventory.json}.
The directory retains all 334 reports, original/normalized/optimized IR, rejection
sentinels, accepted objects and native/harness logs. The initial incomplete
build/p23-2-part2-corpus attempt exposed the report-name lifetime bug and is not
an acceptance result. Exact source/ABI failure regressions are restricted to
native Linux x64 observations; other targets retain portable service/scalar tests
without guessed ABI/encoding assertions. The normal public cross-platform fixture
corpus and test assertions remain on their existing paths.

Local compiler validation: full Debug 61/61 (65.83 s), followed by final focused
audit passes in Debug (1.39 s), Release (0.78 s) and strict ASan/UBSan (1.72 s).
The strict audit/foundation/runtime-generated/interface set also passes 4/4
(10.11 s), with leak detection and halt/stack reporting enabled. Final additions
after the broad run were test-only actual-fixture negatives and target guards;
shared product code was unchanged. No full Release/sanitizer/six-configuration
matrix, new hosted acceptance or interface-crash attribution is claimed. Public
activation remains stopped at the general ABI/lineage and final-encoding gates.

## 18. Production AOT certification Part 3: limited lineage proof, ABI gate retained

15 October 2026. Owner reports CI #42 passed all seven jobs, 61/61 suites, locking
Part 2. Public AOT, native-array prohibition, the 65,536-byte policy, original ABI,
ORC/LSP gates and the separate PUSH/tail blockers remain unchanged.

### 18.1 Incoming ownership: real addresses and a missing charging boundary

Native Linux x64 witnesses use the distinct original production AOT control
executables, input 10, with ASLR enabled. GDB breaks at caller entry, the actual
call instruction, callee entry, return and caller cleanup. It verifies the same
argument bytes/addresses on both sides of the call and the machine-reported
alignment for every incoming object. No rejected candidate is executed.

| Original normal control | Caller static F | Additional outgoing storage below F | Incoming objects | Callee-entry offsets |
| --- | ---: | ---: | ---: | --- |
| value_structs/main, choose | 280 | 16 | 2 | +8 (1 byte, align 16), +16 (8 bytes, align 8) |
| value_structs/numeric, exchange | 600 | 144 | 17 | +8 through +136 in 8-byte steps, align 16/8 alternating |

At both calls, caller SP is 16-byte aligned; CALL places the return address eight
bytes below it. RET restores the call-site SP, and caller cleanup restores entry
SP minus static F. Incoming storage is visibly in a caller-created argument area
for these executions. The choose area occupies 16 bytes, not the nine bytes
obtained by summing logical object extents. Numeric's MFI maxCallFrameSize is 136,
while native additional SP use is 144: alignment padding is not a guessed allowance.
These observations do not grant callee subtraction credit or establish a general
caller charge from an incoming offset alone.

Pinned [X86 call lowering](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86ISelLoweringCall.cpp)
creates formal-memory objects from actual CC assignments; separate tail-call,
byval, copy-elision and variadic paths also exist. Pinned
[X86 frame lowering](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86FrameLowering.cpp)
explicitly excludes push sequences from reserved call frames. Thus
hasVarSizedObjects=false does not prove that outgoing arguments are inside static F.
The shared auditor now records reserved_call_frame, has_tail_call and the original
FinalizeISel object inventory. Policy-applicable functions reject nonreserved,
nonzero outgoing call frames until a full bound is proved. Array-free candidates
retain C=null; this does not alter their public ABI or newly impose the array cap.

A focused near-limit negative has P=65,520, F=65,528, R=65,520, E=8, nominal C=65,536,
and a nonreserved 40-byte outgoing call area. It rejects at the new storage gate in
normal/O2, independently of the existing final PUSH encoding rejection. No guessed
rounding term is added to C. No incoming ownership is marked proved on any target.
Windows x64 and macOS ARM64 have no new native ABI witness here. Their general
ownership/charging gates remain closed; earlier restricted proofs are not generalized.

### 18.2 ABI-created storage: cause identified, general identity still missing

In numeric normal, the extra 64-byte/align-8 object is already present at
FinalizeISel, without an AllocaInst. Independently generated pinned MIR shows
LEA stack.47 passed in RDI to exchange, then returned fields loaded from stack.47.
The native original control confirms RDI equals caller entry SP minus 112, the
measured object's address (MFI offset -120 plus E=8), and confirms exchange writes
its expected first and tail fields there. The role of this particular hidden
result buffer is established.

Pinned [SelectionDAGBuilder return demotion](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp)
creates a target-sized stack object and inserts a hidden result pointer when the
return cannot be lowered directly. The object has no compiler AllocaInst identity.
A shape match to a source return type is not a reusable call-site identity proof.
The general auditor has no verified origin event connecting this frame index to
an immutable lowering result home, especially after call transformation/reuse.
It continues to reject that object. No source ABI is changed to eliminate it.

### 18.3 Exact count canonicalization: proved edge, zero subtraction credit

The real production O2 pipeline now has read-only pass instrumentation. Weak
allocation identities prevent a deleted address reused by LLVM from acquiring
an old observation identity. Reports record births, deaths and descriptor changes
at the boundary where they are first observed. These snapshots identify where to
investigate; they do not invent parent/child lineage or prove partitions/merges.

The actual managed main/handoffs mismatches include InstCombine's
[simplifyAllocaArraySize](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp):
it replaces alloca i8,N with alloca [N x i8],1. A live CallbackVH observes the
actual replaceAllUsesWith edge while both instructions exist. Acceptance is limited
to original compiler-declared Frame/roots and staging/staging homes, the actual
InstCombine pass, static same-function/default-address-space allocations, a positive
constant count greater than one, exact byte element/count/type and preserved
alignment. Inalloca/swifterror forms and replacements claimed by another original home
are excluded. Names, copied metadata, matching sizes or after-pass coincidence
cannot supply a replacement edge. An erased/replaced successor cannot retain a
valid WeakVH identity by address reuse.

The immutable P remains unchanged. A late exact matching machine object records
canonical_byte_array_homes with original identity and FI, but receives **zero R**.
All physical storage remains in F, including alignment, spills and staging. This
is conservative reconciliation, not a new optimization or backend lowering.
A 40,000-byte canonical home therefore rejects at C=80,016 with R=0. This deliberate
overcharge is not a new allocation limit; future large staging still needs a
stronger subtraction proof or must fail the unchanged policy.

A reuse fixture keeps two distinct 32-byte original identities, both exact rewrite
edges, one physical colored object, P=64, F=40, R=0 and C=112 on Linux x64 O2.
The original 512-byte reuse assertion remains intact. Missing compiler identities,
wrong role, wrong type, original inline clones, zero-home clones, post-freeze edits,
missing inventory/coverage and over-budget forms still reject. Exact former managed
O2 candidates now have positive production-route tests and an altered-identity
replay of the same normalized module retaining the prior unknown-home rejection
and destination-preservation assertion.

For managed main O2, profile, frozen IR, optimized IR, immutable inventory and
private object hashes match Part 2 exactly. Acceptance changes only because the
new live edge proves the narrow rewrite; no pass, source evaluation, GC operation,
readonly/value semantics, target option or instruction encoding changes.

### 18.4 Remaining proof boundary

Inlining can create homes in a different caller without an original-home identity;
SROA can erase/promote or partition allocas; stack coloring can retain a representative
while rewriting uses of other slots. The observer records actual transformations
and pre/post-machine shapes, but snapshots do not establish each byte-range mapping.
The narrow count rule is not applied to clones or inferred partitions. Unknown
homes still reject, including optimized null_failure inline storage. Existing
source/IR/GC/readonly/reload contracts are not bypassed.

Stop general activation here. Incoming acceptance requires a verified caller/callee
assignment map and complete outgoing-SP bound, including padding, push sequences,
CFG paths and joins, cleanup, tail/byval/variadic cases and target differences.
ABI result storage requires a trustworthy lowering/codegen origin event or a separately
approved bounded producer transformation; FI/extent similarity is insufficient.
Remaining optimization lineage needs equally explicit transformation edges.
No broad ABI calculator, metadata-origin guess, suppression of optimization or new
LLVM fork/ABI change is introduced in this checkpoint. The signed-PUSH and tail-pseudo
encoding proofs remain separate and their rejection logic is unchanged.

### 18.5 Complete corpus rerun and local verification

All 15 original integration scripts pass again. The complete native Linux x64
corpus has 334 attempts, 275 accepted and 59 rejected, compared with 247/87 in
Part 2. All 28 newly accepted candidates are O2 and use the narrow reconciliation.
No normal-profile candidate changes outcome.

| Producer | Normal attempted / accepted | O2 attempted / accepted |
| --- | ---: | ---: |
| Integration source/architecture | 104 / 100 | 104 / 100 |
| Seven families' source CLI programs | 52 / 41 | 52 / 25 |
| Runtime libraries | 7 / 3 | 7 / 2 |
| Edited reload generations | 3 / 1 | 3 / 1 |
| Generated runtime IR | 1 / 1 | 1 / 1 |

All 288 routes sharing the exact source identity with the retained Part 2 corpus
have identical target profile, frozen IR, optimized IR, immutable inventory and
private object hashes. Generated integration paths intentionally differ between
artifact directories and are not included in that cross-run identity comparison.
The corpus driver independently verifies each accepted delivered object and IR/
inventory hashes. There is no fallback for rejected candidates.

Reports contain 1,972 machine functions, 454 proved canonical rewrite events and
447 physical canonical-home observations, all with zero subtraction credit. Some
rewritten allocations are subsequently eliminated. Remaining unmatched storage
contains 200 objects in 80 functions, including 51 objects without AllocaInst
identities. Incoming objects remain unproved in 14 functions. Eleven function
observations have nonreserved/nonzero outgoing areas; corpus functions remain
array-free C=null, while policy-applicable regression fixtures reject this shape.

Rejection reasons overlap: unmatched storage in 54 candidates, incoming ownership
in 14, final encoded SP differences in two and final call differences in three.
The PUSH/tail reasons and counts match Part 2. All 59 rejected outputs retain their
sentinels and are neither linked nor executed. Actual source negatives, rather
than guessed ABI permissions, preserve these boundaries.

The 266 accepted CLI candidates pass 1,596 exact exit/stdout/stderr comparisons,
3,192 native process executions. Seven complete assertion harness pairs pass:
managed normal/O2, managed_initialization normal, inheritance normal, strings O2
and generated runtime normal/O2. The other nine library/reload configurations are
explicitly not linked because at least one required object rejects. These 14
harness executions retain the existing caller, GC, reload and runtime assertions.

The original three-program small-stack driver correctly stops before linking or
executing optimized null_failure. Its directory remains an incomplete negative
experiment. A fresh explicit main/handoffs selection passes all normal/O2 cases:
64 comparisons, 128 executions, including 64 executions under hard/soft 1 MiB stack
limits. The selector changes no default coverage or crash classification. The
full corpus still includes rejected null_failure and its recorded cause.

Full Debug 61/61 passes (69.56 s, four workers); the audit suite passes within that
run (9.04 s). Full Release 61/61 also passes (30.16 s, four workers); the focused
Release audit passes (2.36 s). Strict ASan/UBSan audit,
foundation, generated runtime and interfaces pass 4/4 (16.56 s), with leak detection,
abort/halt and stack reporting enabled outside the ptrace-restricted sandbox.
The existing Debug interface crash capture is untouched; no cause or fix is claimed.

Evidence: build/p23-2-part3-corpus contains complete.json, records.jsonl, all reports,
IR, objects/sentinels, native/harness logs and part3-evidence-summary.json.
build/p23-2-part3-native-witness-verified retains GDB scripts, disassembly, normal-exit
checks, original-control hashes and pointer/alignment observations. Independent
numeric MIR is in build/p23-2-part3-native. Pinned source copies and SHA256SUMS are
in build/p23-2-part3-sources. The successful small-stack subset is in
build/p23-2-part3-small-stack-certified; the earlier incomplete run is preserved
separately. Tracked reproduction is in tools/ci/STORAGE_SHAPE_PROOF.md.

A sanitizer build attempt overlapped editing the fixture translation unit and ended
in Clang SIGBUS after invalid source-byte diagnostics. The log, preprocessed source
and compiler reproduction script are retained in build/p23-2-part3-toolchain-crash.
The stable-source rebuild and strict tests above pass. No formal cause is assigned
to that toolchain failure or to the historical VOID Debug interface crash.

No hosted run, Windows/macOS native execution, complete target ABI proof, ORC/LSP
certification or source native-array execution is claimed. Stop for boss/wrench
review at the remaining ABI, lineage, budget-reconciliation and encoding gates.
No commits, pushes, branches, remote modifications or CI edits.

## 19. Production AOT certification Part 4: outgoing depth and the encoded-CFG gate

15 October 2026. Owner reports CI #43 passed seven jobs, 61/61 suites, locking
Part 3. This checkpoint does not activate public AOT certification or native arrays.
The 65,536-byte limit, immutable inventory, incoming/optimization-lineage rejection,
PUSH/tail encoding gates and interface crash instrumentation are unchanged.

### 19.1 Pinned Linux x64 lowering findings

The installed tools and SDK are LLVM 19.1.7. All eight retained pinned source
checksums in build/p23-2-part3-sources/SHA256SUMS were rechecked successfully.
The relevant target rules are:

- [hasReservedCallFrame](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86FrameLowering.cpp#L63)
  excludes variable homes, push sequences and preallocated calls. The SDK contract
  says a reserved call area is included in the frame. Adding maxCallFrameSize to F
  unconditionally would therefore double-count reserved storage.
- [X86CallFrameOptimization](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86CallFrameOptimization.cpp#L137)
  requires closed, nonnested call sequences in each block for its push conversion.
  Its profitability calculation can keep one stack argument as an SP-relative store
  in a reserved frame while converting longer sequences to pushes. It explicitly
  excludes Windows x64; a Linux rule cannot be generalized to that target.
- [eliminateCallFramePseudoInstr](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86FrameLowering.cpp#L3733)
  rounds a nonreserved call area to stack alignment, subtracts bytes already handled
  by pushes/callee cleanup, and can merge adjacent SP updates. Dead cleanup after a
  nonreturning call can be omitted; minsize paths can use pops. FrameSetup/Destroy
  flags or the largest unrounded pseudo alone cannot recover all physical movement.
- [LowerCall and cleanup](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Target/X86/X86ISelLoweringCall.cpp#L1992)
  distinguish caller/callee-pop conventions, tail calls, byval, inalloca and
  preallocated arguments. hasCalleePopSRet returns false for 64-bit targets;
  ordinary Linux x64 C calls return to their call-site SP. This is not a proof for
  other conventions, variadic/indirect calls or arbitrary generated helper calls.
- Hidden result demotion remains the Section 18 SelectionDAGBuilder storage-origin
  problem. Its buffer is a caller home, separate from the outgoing argument area;
  it must not be charged again as outgoing bytes or accepted from size/FI similarity.
  Tail reuse of incoming storage is not an ordinary net-zero call/return edge.

### 19.2 Narrow machine-CFG observations, with zero certification credit

The existing auditor now emits outgoing_sp_observation. It changes no LLVM IR,
pass order, code generation or delivery decision. A worklist starts at entry depth
zero and requires identical SP depth at every join/backedge. It records every
reachable block, successor, supported SP delta and call depth. Balanced repeated
calls do not accumulate storage. Uncovered blocks, unequal joins/accumulating
cycles, exceptional/address-taken/indirect control, unknown SP writes, dynamic/
realigned/tail frames and unsupported source/machine calls make the observed bound
null. Calls in this observation profile are direct Linux x64 C CALL64pcrel32
instructions with a retained source callee; returns must restore entry SP.
The recognized arithmetic/push/pop subset is used for measurements only, including
signed PUSH opcodes. The final encoding normalizer is unchanged.

For a complete machine observation, let D be the maximum entry-relative SP depth.
O = max(0, D - F) identifies movement below the already accounted static frame.
It does not add a second reserved area, sum alternative branches or accumulate
balanced loop iterations. It excludes callee execution and the transient CALL
return-address push. No incoming object earns credit. Reconciliation would need
C = alignUp(P + F + O - R + E, A), with R still limited to proved bytes within F,
**only after the missing ownership and final-encoding proofs are established**.
This checkpoint does not adopt that equation as a new acceptance rule.

The report explicitly says diagnostic-machine-CFG-only;not-a-delivery-certificate,
final_encoded_cfg_bound=false, reconciliation_credit=0 and incoming_credit=0.
The existing policy rejection for nonreserved, nonzero call areas remains mandatory.
C in such a rejected report is the old nominal local-only value, not a certified
complete charge. Ordinary array-free functions still have C=null. Unknown observer
shapes cannot turn a rejected candidate into an accepted one.

### 19.3 Native witnesses and boundary cases

New normal/O2 compiler fixtures cover reserved areas, alignment, multiple calls,
both branch alternatives, balanced repeated calls, two near-limit sizes, hidden
result homes and tail rejection. Additional negatives cover repeated dynamic
allocations, indirect calls and realignment. All rejected destinations preserve
their owner sentinel. Existing real value_struct/numeric and inheritance negatives,
IR identity/reuse/inlining assertions and source-native-array prohibition remain.

Native witnesses use distinct ordinary-control objects independently compiled from
saved optimized fixture IR, then linked against a separate C caller/callee harness.
They never link staged rejected objects. GDB has ASLR enabled and measures each
caller instruction, stepping over callees. It checks call-site alignment, unchanged
SP after each call returns, balanced function return, exact results/side effects,
and the measured depth against the machine observation. Both branch alternatives
and all three loop iterations execute. The finite native witnesses corroborate
these observations; they cannot substitute for an all-path encoding proof.

| Fixture | Raw maximum call area | Static F normal / O2 | Observed peak D normal / O2 | Additional O |
| --- | ---: | ---: | ---: | ---: |
| Reserved, register arguments | 0 | 40 / 40 | 40 / 40 | 0 |
| Reserved, one stack argument | 8 | 40 / 40 | 40 / 40 | 0 |
| Five stack arguments, padded | 40 | 40 / 40 | 88 / 88 | 48 |
| Two calls with different areas | 40 | 56 / 56 | 104 / 104 | 48 |
| Branch alternatives | 40 | 40 / 40 | 88 / 88 | 48 |
| Three loop iterations | 40 | 56 / 40 | 104 / 88 | 48 |
| Original home 65,456 bytes | 40 | 65,464 / 65,464 | 65,512 / 65,512 | 48 |
| Original home 65,520 bytes | 40 | 65,528 / 65,528 | 65,576 / 65,576 | 48 |
| Hidden 64-byte result home | 0 | 104 / 72 | 104 / 72 | 0 |

For the two near-limit cases, nominal C is 65,472 and 65,536 respectively. Adding
the observed 48 bytes would produce 65,520 and 65,584 after alignment. Both still
reject because the general outgoing proof is incomplete, even where the observed
sum fits. No limit increase, estimated allowance or partial-proof acceptance is
introduced. Hidden result fixtures reject their unmatched ABI home in both profiles,
with zero new provenance/subtraction credit. Tail/dynamic/indirect/realigned forms
have null observation bounds and retain their separate rejection reasons.

All 36 debugger witnesses pass. The separate controls pass 72 ordinary native runs,
36 with hard and soft RLIMIT_STACK=1,048,576, plus the 36 debugger executions.
The first restricted GDB attempt correctly failed with ptrace Operation not
permitted; the final run used permitted ptrace. There is no Windows/macOS native
or cross-target execution claim in this Linux-only checkpoint.

### 19.4 Confirmed stop boundary: encoded CFG and call-site ownership

The current final object check compares a linear list of SP writers and a separate
linear list of call opcodes. It does not bind branch targets, block ranges, call/SP
interleaving or path multiplicity to the late machine CFG. Post-printer frame
comparison does not supply that missing final-byte relationship.

A reproducible **bytes-only, never-executed** negative changes the ordinary branch
control's conditional jump target to function entry/frame setup. The final decoded
SP-writer and call lists stay exactly identical, while a path can repeat allocation
without cleanup. The native compiler did not emit this altered object. The experiment
shows insufficiency of the available certificate information, not a compiler
miscompile or an accepted faulty delivery. No rejected/altered object is executed.
The existing final-SP/call checks are preserved rather than advertised as a CFG proof.

There is also no complete outgoing call-site assignment map: Early currently records
maximum pseudo area per callee name, losing individual sites, ordering and cleanup
ownership. Source callee membership and a matching FI/extent do not prove that map,
especially for generated calls, return demotion and transformed/inlined calls.

Stop acceptance widening and production activation here. Recommended next checkpoint,
subject to architectural review: bind final MC block ranges, conditional/unconditional
branch targets, fallthroughs and ordered call/SP events to the audited CFG, rejecting
unknown relocations, branches, indirect flow, tail encodings and uncovered bytes.
Alternatively, an LLVM-origin emission hook must attest the equivalent mapping through
relaxation and pseudo expansion. In addition, capture trustworthy per-call argument/
cleanup/result-origin assignments before call-frame pseudos disappear and preserve
those identities through transformations. Do not introduce a fork/hook or guess
ownership without review. The separate optimizer lineage, hidden-return origin and
PUSH/tail gates remain open. No claim of completed outgoing certification is made.

### 19.5 Corpus rerun, unchanged decisions and actual verification

The complete staged Linux x64 corpus again has 334 attempts, 275 accepted and
59 rejected. All 334 individual outcomes match Part 3. Every one of the 288 routes
with identical physical source identity also has identical profile, frozen IR,
optimized IR, inventory and private-object hashes. This observer changes report
content only. Generated integration paths differ across artifact directories and
are excluded from that cross-run identity comparison.

| Producer | Normal attempted / accepted | O2 attempted / accepted |
| --- | ---: | ---: |
| Integration source/architecture | 104 / 100 | 104 / 100 |
| Seven families' CLI programs | 52 / 41 | 52 / 25 |
| Runtime libraries | 7 / 3 | 7 / 2 |
| Reload generations | 3 / 1 | 3 / 1 |
| Generated runtime | 1 / 1 | 1 / 1 |

All 15 original integration scripts pass. Accepted CLI objects pass 1,596 exact
exit/stdout/stderr comparisons, 3,192 native executions. The same seven complete
native assertion harness pairs pass as Part 3 (14 executions); the remaining nine
configurations are rejected/not linked. All rejected candidates retain their
sentinels and are never executed. Rejection reasons, overlapping by module, stay
54 unmatched-lineage, 14 incoming-ownership, two final-SP/PUSH and three final-call/
tail. No acceptance gain is claimed or required.

Of 1,972 machine function observations, 1,670 have a complete diagnostic machine
path-depth observation and 302 have a null bound. Unsupported source/machine calls
and tail/non-C shapes account for those null observations; reasons can overlap.
All 11 nonreserved/nonzero-area observations have a measured machine depth, zero
new credit and array-free C=null. These measurements do not certify their modules.
The real value_structs main/numeric original-control debugger witnesses pass again:
F/D/O are 280/296/16 and 600/744/144. Argument addresses/bytes, alignment, hidden
result pointer/fields, return SP, caller cleanup and normal exit checks pass.

Full Debug passes 61/61 (75.24 s; audit 8.81 s). Full Release passes 61/61
(37.54 s; audit 3.04 s). Strict ASan/UBSan audit, foundation, generated runtime
and interfaces pass 4/4 (15.23 s; audit 6.45 s), with leak detection, abort/halt
and stack reporting enabled with permitted ptrace. No full sanitizer matrix or
new hosted acceptance is claimed. Debug interface crash instrumentation is untouched;
this checkpoint neither reproduces nor attributes/fixes the historical crash.

The explicit production managed main/handoffs small-stack selection passes in both
Debug and Release: each build has 64 comparisons and 128 native executions, including
64 executions at hard/soft 1 MiB. Rejected null_failure remains in the full corpus;
it is not silently included as a successful selected program. These checks are in
addition to the new outgoing-control 1 MiB witnesses in Section 19.3.

Evidence is retained under build/p23-2-part4-corpus (complete.json, records.jsonl,
part4-evidence-summary.json, reports, three IR snapshots, objects/sentinels and logs),
build/p23-2-part4-outgoing-final (36 native witnesses, commands, independent controls
and encoded-cfg-gap.json), build/p23-2-part4-production-native and
build/p23-2-part4-small-stack-{debug,release}. Build/CTest logs have the
build/p23-2-part4 prefix. The earlier restricted/incomplete debugger directory is
retained separately; it is not acceptance evidence. Tracked reproduction is in
tools/ci/STORAGE_SHAPE_PROOF.md and tools/ci/aot_outgoing_probe.py.

Stop for boss/wrench architectural review of the encoded-CFG and call-site origin
boundary. No production reconciliation rule was widened across that boundary.
No public emitter, ABI, source semantics, GC/readonly contracts, native array gate,
ORC/LSP path, ordinary CI or unrelated receiver-chain code changed. No commits,
pushes, branches or remote changes.

## 20. Part 5A: research-only final encoded-object CFG verification

Owner reports CI #44 is locked. Reviewed the supplied Research Clanker report
(final machine certification review of 6740b666), Section 19 and the rolling work
log. The hybrid direction is approved, but this checkpoint implements only the
independent encoded-CFG pilot. No instruction labels, emission-origin hooks, ABI
ledger, storage reconciliation, public activation or native arrays are introduced.
The 65,536-byte policy and every existing certification rejection remain intact.

### 20.1 Read-only verifier and supported boundary

AotObjectEmitter::research_encoded_cfg reuses the staged service's exact LLVM
19.1.7 TargetMachine/profile and installed object/MC facilities. Its implementation
is in src/backend/llvm/encoded_cfg.cpp. It reads an existing object; it never emits,
replaces or delivers one. The explicit compiler-fixture --research-cfg entry writes
only a separate research JSON report. deliver() and checkObject() retain their
existing decisions, including the separate signed-PUSH/tail gates.

The bounded profile is Linux x64, little-endian ELF64, ET_REL/ET_EXEC/ET_DYN,
and one explicitly selected function. ObjectFile/computeSymbolSizes establish
section/function extents; overlapping function extents, aliases that make linked
function addresses ambiguous, missing symbols and truncated extents reject.
MCDisassembler must decode every byte into exact instruction intervals. The small
explicit opcode allowlist contains the observed integer/register/memory operations,
NOPs, supported RSP immediate arithmetic, 64-bit pushes/pops, direct calls,
JCC/JMP short/near branches and RET64. Unknown opcodes/SP writers, indirect calls/
branches, unfamiliar returns and failed/soft decoding reject.

MCInstrAnalysis evaluates direct local branch targets. Every target must stay
within the function and land on a decoded instruction boundary. Conditional edges
include the taken target and fallthrough; JMP has no fallthrough; calls require a
continuation; RET terminates. Missing fallthrough, unreachable decoded bytes and
incomplete function coverage reject. The report records intervals, original bytes,
opcodes, branch targets, fallthroughs, successors, ordered events and entry depths.
The selected function is not a whole-image or extra-thunk coverage certificate.

For ET_REL, only exact five-byte direct-call R_X86_64_PLT32/R_X86_64_PC32 operands
with addend -4 are supported. The relocation must cover the call displacement,
not an opcode, branch or other immediate. Unsupported relocation types in the
selected code section, duplicate/unused relevant relocations and relocations crossing
function boundaries reject. Local branches must have fully resolved targets without
relocation. Linked-image calls must resolve to an exact defined function symbol;
unidentified PLT stubs/thunks and indirect edges reject. Other sections' unrelated
relocations are outside this selected-function experiment. Linker transformations
and the full call target/ABI contract remain future proof obligations.

### 20.2 Exact-depth dataflow and machine comparison

A worklist propagates one exact entry-relative SP depth per decoded instruction.
Every join/backedge must agree; growing cycles, negative/overflowing depths,
misaligned call-site SP and unbalanced returns reject. Balanced loops are analyzed
without enumerating iterations. CALL is modeled as net zero across its continuation
under the explicit ordinary Linux x64 C caller-pop contract, excluding callee
execution and the transient return-address push. This is a stated pilot assumption,
not a newly proved per-call ABI ledger. Tail calls remain unsupported.

The existing machine observer now adds layout and ordered_events (SP, call, return)
while preserving its prior sp_events field. The verifier independently constructs
its encoded event graph, retaining successor relationships through non-event
instructions. It compares event order, labels/depths, successor sets, entry set and
peak with a complete same-profile machine observation. Missing or altered evidence
rejects. The comparison is deliberately tied to the current layout/event order;
it does not infer an origin map, accept an arbitrary graph isomorphism or silently
normalize an unfamiliar machine expansion. Non-event instructions still receive
independent encoded boundary, reachability and opcode checks. Their exact machine
instruction attribution remains Part 5B.

Event-free cycles reject conservatively. Traversal uses explicit worklists rather
than recursive path enumeration. Research resource guards limit a selected symbol
to 1 MiB and decoded instructions/machine blocks/events to 4,096. These bounds
limit this pilot's analysis workload; they are not new compiler storage budgets,
array extent limits or approvals of any native frame size.

Every result carries the exact profile and input object SHA-256. supported=true
means only supported intraprocedural CFG/SP analysis under the stated call contract.
The independent flags delivery_credit, abi_ownership_proved and
physical_memory_bound_proved remain false. SP movement does not prove red-zone,
stack-derived memory, probing, realignment, hidden-home ownership or total recursive
call-chain storage. No new O, R or budget credit is granted, even below 65,536 bytes.
Signed PUSH forms can share a measured SP effect here without overriding the old
machine-to-object encoding rejection or claiming their origin transformation proved.

### 20.3 Mandatory mutation and regression scope

The exact retained Part 4 branch original passes supported encoded analysis and
agrees with the fresh ordered machine observation. Its retained never-executed
mutation, with identical flattened SP/call lists, rejects as an inconsistent join/
growing stack cycle. Both original and negative object hashes are independently
checked before/after inspection and against the retained Part 4 metadata.
Neither input is modified, and the negative is never executed.

The existing arrays_storage_audit suite also contains self-contained normal/O2
ELF fixtures. A real emitted conditional function with two direct C call alternatives
passes machine/event comparison. Copy-only branch retargets to frame setup and into
an instruction reject. The original bytes and rejected copies remain unchanged.
These fixtures require no retained build artifacts and do not add a CI workflow.

tools/ci/aot_encoded_cfg_probe.py exercises the retained mandatory pair and ordinary
normal/O2 controls in linked and relocatable forms. Positives cover both branch
paths, reserved/nonreserved areas, padding, multiple calls, loops, near-limit homes
and hidden results. A supported CFG does not rescue the latter candidates' existing
storage-origin/ownership failures. Negatives additionally cover unknown instructions,
indirect flow, unknown SP writes, unbalanced returns, truncated decoding, missing
conditional/ordinary fallthrough, out-of-function/mid-instruction targets, unreachable
bytes, event-free/growing cycles, unequal joins, unsupported/wrong-addend/branch/
boundary relocations, mismatched profiles and missing/altered machine events/edges.
Synthetic wide/narrow signed PUSH cases compare stack effects only; production
encoding acceptance is unchanged. All altered copies are explicitly never executed.

### 20.4 Actual results and review boundary

LLVM/Clang tools and SDK: 19.1.7, native Linux x64 only. The completed adversarial
driver passes 92 checks in each of Debug, Release and strict ASan/UBSan: 60 supported
research cases and 32 required rejections/input-protection checks per build. It
independently disassembles the retained mandatory pair and checks identical flattened
SP/call lists. The original's decoded peak is 88 bytes; the retarget rejects both
with and without machine comparison. The old native control hash remains
E56F290C3DC2902D01099A043F02F78D272487BDF03C2B9BB0E29061318F58A0.
No mutation is executed. The 4,096-instruction boundary passes; 4,097 rejects.

Malformed JSON errors are consumed through LLVM's Expected API before reporting
rejection. Missing SP deltas, machine layout/events/edges and mismatched profiles
reject. Foreign ELF ABI/machine/flags reject. The fixture CLI forbids report paths
that alias object or machine inputs, including canonical/equivalent paths; collision
regressions use disposable copies so a failing guard cannot damage retained evidence.
The inspected originals and all final input hashes are unchanged. These checks add
no serialized-certificate import or publication authority.

Full Debug passes 61/61 (90.96 s; audit 10.58 s); full Release passes 61/61
(33.20 s; audit 2.82 s). After final research-only malformed-input/ABI/report guards,
focused audits pass again, Debug 9.03 s and Release 2.65 s, together with the final
92-case drivers above. Strict audit/foundation/generated-runtime/interfaces pass
4/4 (16.44 s; audit 6.49 s), with ASan leak detection/abort and UBSan halt/stack
reporting enabled with permitted ptrace. The strict 92-case driver uses the same
settings. No full sanitizer or six-configuration matrix is claimed.

The complete staged AOT corpus is unchanged: 334 attempts, 275 accepted / 59 rejected,
all individual outcomes identical to Part 4. The 288 exact source-identity routes
retain identical profile, frozen IR, optimized IR, inventory and private-object hashes.
All 15 original integration scripts pass. Accepted CLI objects pass 1,596 exact
comparisons / 3,192 native runs. Seven complete assertion harness pairs pass
(14 runs); nine configurations remain rejected/not linked. Every rejection retains
its sentinel. Overlapping reasons stay 54 unmatched, 14 incoming, two final-SP/PUSH
and three final-call/tail. No rejected object is linked or executed.

The outgoing native witness driver passes again: 36 debugger executions plus
72 ordinary runs, 36 at hard/soft 1 MiB, ASLR enabled. Debug and Release production
main/handoffs normal/O2 small-stack selections each pass 64 comparisons / 128 native
runs, including 64 at 1 MiB. These corroborate native behavior, not whole-program
certification. The historical interface crash capture is preserved without a new
cause/fix claim. No Windows/macOS execution or portability proof is claimed.

Evidence: build/p23-2-part5a-cfg-{debug,release,sanitizers}-complete retains all
92 reports/cases per build, mandatory disassemblies/flattened equality, input hashes
and never-executed negatives. build/p23-2-part5a-corpus contains complete.json,
records.jsonl, part5a-evidence-summary.json, IR, objects/sentinels and native logs.
Other evidence is in build/p23-2-part5a-outgoing-native and
build/p23-2-part5a-small-stack-{debug,release}; build/CTest logs use the same prefix.
Earlier exploratory/incomplete directories are not acceptance evidence.

Stop for boss/wrench review. Part 5A's bounded encoded CFG/SP milestone is met;
verified emission origins, exact non-event machine attribution, call-site ABI ledger,
stack-memory-region coverage, physical storage reconciliation and final linked-code
publication remain unproved. No later hybrid stage, LLVM hook/fork, acceptance
increase, budget change, public activation, native arrays, ORC/LSP integration,
Git mutation or CI workflow edit is included.

## 21. Superseding array-size decision: unchanged-code audit and activation impact

15 October 2026. This is a documentation-only audit following the owner's explicit
correction. CI #45 is owner-confirmed green, seven jobs / 61 suites each; Part 5A
is locked. Existing guards remain operational until separately reviewed changes.
They must not be mistaken for an approved permanent large-array language policy.

### 21.1 Existing enforcement and resource-sensitive paths

| Location | Actual current behavior | Required follow-up before production activation |
| --- | --- | --- |
| include/void/native_storage.hpp:26 | array_function_storage_budget = 65536 | Scope the old constant to research; production must not apply it as a universal array-program rejection |
| src/ir/native_storage.cpp:31-45 | finish(accounted, true) rejects an aligned inventory over that byte budget; missing categories/overflow/target checks are independent | Keep complete target inventories and arithmetic safety; review production policy/API separation. Current production-source call integration is not enabled |
| src/backend/llvm/storage_audit.cpp:305-393, 717-745, 756-777, 870 | Staged plan.policy applies the 65536-byte C gate and propagates it to callers; freeze uses planner.finish(..., false), while the late machine audit enforces the policy; report advertises the old budget | Preserve staged experiments and all fail-closed proofs; do not reactivate this array/caller flag as universal production policy. Separate native stack accounting from payload placement under a reviewed design |
| src/frontend/compiler.cpp:2428, 2434-2435 | Directly rejects extent >65536 and multiplicity above expanded shape bounds | This is already a source-visible ceiling, not merely a byte or stack guard. Review/rework compact repeated-shape analysis rather than hide or rename the cap |
| src/frontend/compiler.cpp:2483-2491 | Enclosing value layout charges a fixed array's multiplicity into the 65536 expanded-field counter | Review aggregate/nested-array accounting too; changing only the top-level extent check leaves an equivalent barrier |
| src/ir/module.cpp:222-244 | Independent IR inline_layout validation repeats the extent/multiplicity and aggregate expanded-field guard, with separate finite/depth checks | Keep independent validation, finite-type and overflow protections; revise together with frontend so valid large-array IR is not rejected by a copied ceiling |
| src/ir/module.cpp:73-89 | reference_paths has a 65536 aggregate traversal-work guard and 128 path-depth bound; it currently does not traverse fixed arrays | Keep actual work/cycle protection; review future repeated reference/capability shapes without O(n) expansion. Do not describe this aggregate walk as an existing array-byte cap or remove it blindly |
| src/frontend/compiler.cpp:1444, 1575; src/ir/module.cpp:508 | P23-1 defaults allocate per-element ConstantValue fields; literals/IR validate exact argument count | Audit compile-time materialization, default/template work and checked sizes before lifting extent guards. Removing one comparison alone can expose excessive work or allocation failure |
| src/backend/llvm/backend.cpp:101 | All source array native lowering still rejects pending P23-2 planning/review | Preserve this activation gate until the revised architecture, large-array semantics and safety proofs are approved and implemented |
| src/backend/llvm/storage_audit.cpp:855-880; checkObject | Staged delivery checks every remaining definition for machine coverage, verifies final bytes and rejects before touching the destination | Preserve this protection and extend future production whole-build diagnostics/required-function coverage. Never turn an unsupported required function into an omitted-success result |

Line numbers identify the audited snapshot; symbol names remain authoritative after
later edits. The planner's true-budget calls are currently exercised by tests;
the staged emitter freezes with false and performs its separate late C check.
No evidence here says the old byte policy is already active in public native-array
production: source native arrays remain blocked. Conversely, the frontend/IR shape
restriction is active now and cannot be dismissed as research-only.

Tests retaining the old contracts are tests/arrays/foundation.cpp:137-138 (65537
extent negative / 65536 accepted) and :197-203 (planner byte boundary and overhead),
and tests/arrays/storage_audit.cpp's policy/near-limit, canonical/reuse and destination
negatives. Preserve research coverage; a future approved implementation must separate
language acceptance tests from experiment-threshold assertions and add large-array
positives. No test is weakened or rewritten in this task.

tools/ci/array_abi_probe/{audit.py,machine_emit.cpp,test_audit.py,test_machine.py}
contains explicit 65536 research charging/boundary checks. probe.py:57-58 restricts
its probe count to 1..65536; this is a harness range, not language authority.
The stack-probe 64/128/256 KiB sizes and Part 5A's 1 MiB function-extent/4096-instruction
workload guards remain research parameters, not array allocation policies.
The separate src/ir/interfaces.inc:89 discovery-work budget (65536 combinations)
is neither array bytes nor element count and is not changed by this correction.

### 21.2 Review and implementation gates

1. Investigate large fixed-array representation and copying on the existing GC,
   alongside growable storage, without selecting a second memory manager or an
   unreviewed stack/heap cutoff. Preserve value snapshots, reference-element semantics,
   readonly borrowing/fallback, initialization, roots, safepoints, failure and reload.
2. Review multiplicity-aware type/default/reference metadata and analysis-work
   accounting across frontend, IR and shared tooling. Genuine resource exhaustion
   must give a clear whole-build failure. A repeated simple shape must not silently
   inherit a permanent element/payload ceiling labeled as complexity protection.
3. Approve the revised architecture and production resource/stack policy before
   implementation or activation. Do not merely raise the old number or disable
   CFG, ABI, origin, memory-region or platform checks to make a large fixture pass.
4. Implement only after approval, then validate megabyte-scale fixed/growable arrays,
   independent copies/alias conflicts, reference-bearing data, allocation failure,
   target bounds, JIT/AOT, shared analysis/LSP and all required native functions.
   Unsupported operations or any failed required function fail the entire build
   with source diagnostics and no partial delivery. Preserve existing suites and
   eventual seven-job native acceptance; cross-compilation is not execution.

No alternative storage design or changed numerical limit is adopted here. Sections
9-20 remain historical evidence, including failures and measured frames. The staged
certificate and encoded-CFG verifier are unchanged and operational. Stop for owner/
wrench review before further architecture or implementation work.


## 22. Large-array foundation, Stage 1 (locked, CI #47)

The owner reports CI #47 passed all seven jobs, 61/61 suites each, locking Stage 1.
The owner supplied the complete Large-Array Storage Architecture Review and
explicitly authorized Stage 1 only for the implementation recorded here. Its six-stage roadmap supersedes the old
inline-payload implementation sequence where they conflict. Section 21 is the
historical unchanged-code audit, not the current implementation status.

### Implemented analysis and IR contract

- The canonical arena retains one element edge and the exact unsigned 64-bit
  extent, including zero. Source extents must fit the already approved A3 i64
  public-count contract; this representation check is not an array-byte policy.
  No element fields are synthesized. Structural keys,
  nominal element identities, constant dependencies and source spans are retained.
- Reference-free fixed arrays and nested value shapes can be analyzed. Scalar
  literals and indexing retain their existing behavior. Nonscalar explicit literals,
  indexing, growables and reference-bearing elements remain gated. This checkpoint
  does not implement their future runtime operations.
- Pure defaults use an owned compact template: an exact repetition count plus one
  element prototype, or no prototype at zero extent. Nested arrays and pure value
  defaults compose these templates. Repeating a value denotes independent logical
  values; it does not authorize shared mutable runtime storage.
- Existing fixed_array IR gains an explicit repeated-template mode. Independent
  validation recomputes the shape, identity, extent and operand typing. It verifies
  a constant/aggregate/array-only prototype DAG in block SSA order; calls, loads
  and other computations cannot be disguised as pure repeated defaults. Explicit
  literal expressions remain individual ordered instructions, including effects.
- A shared memoized shape walk performs checked unsigned 64-bit logical scalar-byte
  arithmetic. This is a necessary representability lower bound, **not** DataLayout,
  heap allocation size, ABI layout, native-stack inventory or an allocation promise.
  Target alignment, object headers, allocator limits and actual construction bounds
  remain later-stage obligations. Even enormous zero-payload repeated shapes can
  remain representable without allocating per-element compiler metadata.
- The old element-count/multiplicity rejection is removed for these compact paths.
  Finite logical value checks apply even to zero repeats. Existing depth bounds and
  actual analysis/default-recipe work protections remain. The latter constrain real
  graph edges or explicitly represented recipe fields, never repeated payload count.
  Custom-initializer repetition is rejected rather than executing an initializer
  once and incorrectly copying its effects. Effectful construction recipes are deferred.

### Boundaries requiring later review

Stage 1 selects no native representation, array ABI, production stack policy or
GC descriptor. Research recommends existing-GC-owned fixed payloads with independent
copies and stable-header growables; the representation, recursive copy/publication
rules, repeated GC descriptors and native completeness ledger must be locked at
subsequent review checkpoints before their implementation. No copy-on-write,
stack/heap cutoff, second allocator or replacement numerical ceiling is adopted.
Production native array lowering remains rejected. Runtime/collector code, native
storage planner, staged certification, Part 5A verifier and 64 KiB experiments are
unchanged. Passing compact analysis does not certify memory availability or execution.

### Verification

Linux x64, pinned LLVM 19.1.7: Debug and Release full CTest runs both pass
61/61 suites. Strict ASan/UBSan with leak detection passes arrays_foundation,
arrays_storage_audit, frontend, analysis, interfaces_source and runtime_generated
(6/6). The expanded foundation suite also passes under 1 MiB Debug/Release stacks.
The unchanged research auditor remains covered by both full runs and sanitizers.

Repeated 1-byte, 2 MiB and 8 MiB defaults each lower to four IR instructions.
Fresh Debug CLI checks under a 512 MiB address-space cap use about 34 MiB peak RSS,
independent of extent at these sizes. Nested value recipes retain nonzero defaults
without expansion. Source and forged-IR negatives cover representation, finite
cycles, arity, purity and actual recipe-work exhaustion. Normal/O2 native JIT/AOT
attempts on an 8 MiB source still reject; AOT destination sentinels are preserved.
Exact commands, measurements and limitations are in the Stage 1 WORK_LOG entry.
These were the original local results. The owner subsequently confirmed CI #47
passed all seven jobs, locking Stage 1. That is hosted baseline evidence, not
verification of later-stage native arrays. Section 23 records the approved runtime
direction; Section 24 records the bounded Stage 2B implementation.

## 23. Stage 2A approval gate: repeated GC layouts and rooted buffers

15 October 2026. **Owner/wrench approved this architectural direction and its
retained safety gates. Stage 2B is locked; Stage 2C was separately authorized.**
The owner reports CI #47 passed all seven jobs, 61/61 suites, locking Stage 1.
Read the complete owner-supplied Large-Array Storage Architecture Review, especially
Sections 2, 5, 6, 10 and 11, and Section 22 above. This section proposes Stage 2's
runtime primitives. It does not implement them or approve a source array ABI,
heap-backed source arrays, native execution, a size cutoff or production certification.
Historical inline-storage diagrams in A6 describe an earlier proposal, not a new
requirement to put fixed payloads on the stack.

### 23.1 Inspected baseline before Stage 2B

| Existing location | Observed behavior | Stage 2 implication |
| --- | --- | --- |
| include/void/heap.hpp, Heap::Object | TypeLayout owns a flat reference vector; object map owns layout copies | Add compact runtime-owned descriptors without flattening repetitions |
| src/runtime/heap.cpp, allocate | Sort/check offsets, operator new, zero whole payload, emplace with rollback | Preserve this ordinary/string behavior; a new buffer reservation/adoption path must avoid long zeroing under Runtime's mutex |
| Heap::store / collect | Linear offset membership; memcpy pointer words; mark graph before sweep; reset marks on failure | One common compact cursor/lookup interpretation; retain fail-before-sweep and mark rollback |
| coordination.cpp, require_output / Frame | Output must be an exact current-thread Frame root address; Handle is not an allocation output | Use existing small Frame/Slot roots for private buffers, not payload-sized root arrays |
| construct_into | Optional budget collection, poll, lock, validate prepared refs, allocate, whole-payload memcpy, publish root | Do not route large buffers through a payload-sized prepared vector or this long locked copy |
| allocate_into | Poll then allocate/zero under mutex; not the construct_into allocation-budget path | Explicitly integrate new buffer adoption with the existing allocation budget, not a new collector |
| collect / Native | Managed threads park; NativeSafe frame chains stay frozen; invalid precise graphs terminate; bad_alloc returns allocation_failure | Chunked preparation polls; raw reservation/zeroing may be NativeSafe only before it contains managed references |
| admit_metadata / validate_descriptor | NativeSafe transactional catalog, exact runtime-owned descriptor identity, no code pointers | Extend this catalog/lifetime model; do not borrow compiler/LLVM descriptor memory |
| abi.cpp / reload.cpp | Selected errors use RuntimeFailure, invariant failures terminate; invocation keeps generation leases; reload publishes compatible code atomically | No new source failure/ABI semantics or generation-owned tracing callbacks |

The actual heap uses ordinary operator new/delete internally. A temporary reservation
owned by that same Heap allocation transaction is not a second heap or a new source
ownership system. The current collector is stop-the-world; this proposal does not
turn it into a concurrent/incremental collector.

### 23.2 Runtime-owned schema and physical layout

Propose an immutable **ElementSchema** DAG admitted into Runtime's metadata catalog.
Input specs contain node indices and owned strings/data, never arbitrary node/code
pointers. Admission deep-copies, validates acyclic inline edges, canonicalizes and
publishes atomically. Shared DAG children are retained once. Managed reference
identity edges use admitted nominal keys, not recursive inline expansion.

The proposed input record is ElementSchemaSpec{identity, capability_profile,
storage_domain, lowering_layout_key, root_node, nodes}. NodeSpec is a discriminated variant, not a bag
of optional unchecked fields: Scalar{encoding}, Bytes{bytes, alignment},
ManagedRef{target_identity, nullable}, StringRef{}, Interface{interface_identity,
nullable, object_offset, witness_offset, bytes, alignment}, Record{bytes, alignment,
fields[{logical_id, offset, child_node}]}, Repeat{base, stride, extent, child_node,
bytes, alignment}, and the rejected reserved roles. Primitive sizes/alignments must
match the storage domain; record/repeat spans must match checked derived geometry.
Record fields have unique logical IDs and nonoverlapping physical ranges. Unknown
variants/encodings/extra unresolved targets reject. Specs use topologically ordered
child indices; forward/self/cyclic inline indices reject before publication.

Each admitted node has checked byte extent/alignment and one of these roles:

| Node | Physical/semantic data |
| --- | --- |
| Scalar / byte region | Encoding, bytes, alignment; bool validity is explicit; byte regions cannot contain declared references |
| Managed reference | One whole-object pointer word; admitted nominal target/subtype rule and required/nullable policy |
| String reference | One GC object word; null is the existing empty string, otherwise intrinsic string kind |
| Interface value | Object word plus runtime-owned witness word at explicit offsets; only the object word is traced; pair consistency and witness membership are checked |
| Record | Logical field IDs and child offsets; separate sorted physical index; padding is explicit non-reference storage |
| Repeat | One child, exact extent, base/stride; no per-element nodes |
| Owned fixed child / growable header | Reserved semantic roles, rejected by the first Stage 2 value-copy implementation pending later representation contracts |

Reserved owned/growable roles have no chosen source handle width/offset in Stage 2;
reject their admission for executable value operations until that later review.
A reference shape is the projection of this schema, not a separate manually supplied
flat list. Cache has_references, zero_data_complete, copy classification and maximum
atomic write group as derived admission results. Invalid/missing classifications
reject. This does not infer pointer fields from numeric bits. Compiler semantic-type
to runtime-schema correspondence remains an independent later lowering/IR proof;
runtime validation is not protection against arbitrary dishonest embedding code.

A per-allocation **BufferLayout** retains its admitted element schema, domain,
base, stride, capacity, exact payload bytes and alignment. It has no executable
pointer, source AST/TypeArena edge, LLVM object or generation lease. Element schemas
are catalog-owned; capacity-specific layouts are reference-counted by buffers and
users, not interned forever for every historical capacity. Every use rechecks exact
admitted-schema pointer membership in this Runtime, not identity strings alone;
foreign handles remain invalid even if a destroyed Runtime's address is reused.

```text
Heap object-map metadata (outside payload)
    kind=buffer, shared immutable BufferLayout, private/complete,
    sealed valid_prefix, read-lease count, allocation/deallocation alignment

GC payload
    zeroed prefix padding [0, base)
    capacity slots, each stride bytes
    zeroed trailing alignment padding
```

For element footprint S/alignment a, the buffer factory computes:

- stride = checkedAlignUp(max(S, 1), a), including physically empty elements;
- require base % a == 0; payload alignment = a;
- bytes = checkedAlignUp(base + capacity * stride, a), with every intermediate
  operation checked before narrowing to size_t or forming an address;
- capacity fits the approved i64 count contract and the actual host allocation
  representation. Zero capacity uses base=0, bytes=0, a typed empty token and no
  Heap allocation. Zero-payload elements do not imply zero stride for nonempty backing.

No guessed field widths or alignment from Stage 1's logical scalar-byte lower bound.
The storage domain records pointer size/alignment, scalar encoding sizes/alignments,
byte order and runtime storage-format revision. Runtime derives/checks these native
facts without an LLVM dependency. A separate exact lowering_layout_key binds the
schema's declared physical layout identity and reload compatibility; it is not,
by itself, proof that LLVM used that layout. Stage 2 fixtures use an explicit native
C++ fixture key and sizeof/alignof/offsetof. Future LLVM lowering supplies its selected
DataLayout/configuration identity and independently proves the match before admission.
Foreign pointer/encoding/alignment domains reject; equal-size schemas with different
layout keys are not interchangeable. No cross-target code execution is implied by
metadata validation. This specifies internal GC buffer storage, not a source fixed-
array parameter/result representation or public C ABI.

Admission checks power-of-two/supported alignment, offset alignment, nested child
containment, nonoverlapping record storage, stride >= child span, exact checked
repeat end, pointer-word containment, node indices, finite topology and semantic
identity/capability profile. Repetition disjointness follows from the child's
validated region plus stride, without enumerating reference occurrences. Validate
children even for extent zero. Hashes may index canonical data; full identity and
schema comparison decide equality. No payload/element-count complexity ceiling is
introduced. Actual descriptor work/allocation exhaustion fails explicitly.

For new buffer layouts, pointer words meet the selected domain's pointer alignment.
Existing ordinary layouts may have unaligned memcpy-accessed pointer offsets;
they retain that accepted behavior. No new alignment rule retroactively rejects them.
Over-aligned buffer allocation uses the existing Heap's matching aligned new/delete
forms; record the deletion form. Default ordinary/string allocation stays unchanged.
If this alignment/deallocation proof cannot be established, stop that slice rather
than silently weakening alignment or substituting a different allocator.

### 23.3 One interpretation for all GC operations

Propose a validated schema cursor with two operations: stream reference/value events
and seek a byte offset or logical field/index path. Record lookup uses its sorted
physical index; Repeat lookup uses checked quotient/remainder and child descent.
Both operations interpret the same admitted nodes and rules. Explicit cursor storage
is bounded by shape depth, allocated before a fallible operation needs it, not by
capacity. No recursive native call chain or expanded offset vector is required.

| Consumer | Required use |
| --- | --- |
| Allocation/admission | Symbolic region/disjointness checks and cached schema properties, not an O(capacity) metadata expansion |
| Marking | Stream declared object words for the full immutable capacity; skip nulls; skip reference-free subgraphs in O(1) |
| Checked stores | Seek exactly a declared leaf inside valid live storage; validate reference kind/target/nullability and runtime ownership |
| Prepared-payload validation | Stream the same leaves/pairs, validate targets, use memcpy access; never maintain a second offset interpretation |
| Construction/copy | Same cursor identifies scalar regions, reference words and atomic interface groups; data completeness follows the corresponding typed event order |

Legacy flat layouts are an adapter/view over their validated sorted offsets in this
same service. Sort/validate an owned copy once; existing Heap inputs may be unsorted,
so seek must not assume the caller already sorted them. TypeLayout's new buffer
variant is mutually exclusive with ordinary flat references, intrinsic string kind and nominal-object descriptor fields. Reject
mixed/unknown variants; do not attempt an unchecked union of overlapping maps.
Existing construct_into uses the common reference service without changing its old
payload contract. Existing Heap::allocate, Runtime::allocate_into and construct_into
must reject the buffer variant: only the reservation/builder path can create and
seal it. A raw prepared buffer pointer cannot bypass completeness or root checks.
Prepared buffer validation is exercised through typed rooted chunks/copy sources,
not an external payload containing unrooted reference words. New buffer operations
bypass whole-payload staging/copy.

The trace extent is immutable allocation capacity, never a mutable source count.
Private and spare slots contain zero in reference positions. Null there is safe for
tracing but not a complete live required-reference value. GC does not reconstruct
source validity from nullness or from a caller-supplied header count. For a sealed
buffer, a nonnull reference outside valid_prefix is an invariant violation, detected
while streaming the capacity during collection; private partially filled elements
permit nulls. Heap edges to private targets reject, while an internal registered
builder root may name its own private allocation. Geometry interpretation is common;
required-value checks depend explicitly on private/complete state, not a second map.

Marking remains O(actual reference positions + reachable objects), not O(payload
bytes) for byte buffers. Avoid a vector of expanded offsets or one pending entry
per duplicate reference: use streaming cursor/work items and unique queued/marked
objects, with complete mark rollback on any worklist allocation failure. All graph
validation precedes sweep as today. Any queue/mark ordering change needs its own
failure-injection proof; a plausible scan count does not authorize partial sweep.

### 23.4 Proposed C++ API surface

This is the full Stage 2 internal embedding/runtime API design, not a new generated-
code ABI or a claim that every declaration works. Sections 24-26 record implemented
2B admission and the 2C reference-free subset; reference/copy/lease APIs remain
proposals. Opaque handles are runtime-
bound and factory-created;
callers cannot manufacture an admitted handle by constructing shared_ptr<const T>.

```cpp
// Proposed namespace vd declarations; handle constructors remain private.
class BufferBusy final : public std::logic_error {
public: using std::logic_error::logic_error;
};
struct BufferWorkQuota { std::size_t bytes, reference_events, node_events; };

// Proposed additions INSIDE Runtime, alongside existing Frame/Handle declarations:
class BufferBuilder;
class BufferReadLease;
void admit_metadata(std::span<const NominalDescriptor>,
                    std::span<const InterfaceWitness>,
                    std::span<const ElementSchemaSpec>); // NativeSafe/host overload
void admit_buffer_schemas(std::span<const ElementSchemaSpec>); // wrapper over overload
ElementSchemaRef buffer_schema(std::string_view identity) const;
BufferLayoutRef buffer_layout(ElementSchemaRef, std::uint64_t capacity,
                              std::size_t base = 0) const;
RegisteredRootWord root_word(void** slot) const;
void buffer_store_scalar(RegisteredRootWord owner, BufferLayoutRef,
                         std::uint64_t index, SchemaPath, std::uint64_t bits);
void buffer_store_interface(RegisteredRootWord owner, BufferLayoutRef,
                            std::uint64_t index, SchemaPath,
                            const RootedInterfaceSource&);

// Proposed nested class definitions; private storage omitted here.
class Runtime::BufferBuilder {
public:
    BufferBuilder(Runtime&, BufferLayoutRef, std::uint64_t requested_live,
                  BufferWorkQuota);
    ~BufferBuilder() noexcept;
    BufferBuilder(const BufferBuilder&) = delete;
    BufferBuilder& operator=(const BufferBuilder&) = delete;
    BufferBuilder(BufferBuilder&&) = delete;
    BufferBuilder& operator=(BufferBuilder&&) = delete;
    void write_scalar(std::uint64_t bits);
    void write_bytes(std::span<const std::byte> native_input);
    void write_reference(RegisteredRootWord source);
    void write_interface(const RootedInterfaceSource& source);
    void zero_elements(std::uint64_t count);
    void copy_elements(BufferReadLease&, std::uint64_t first, std::uint64_t count);
    void finish_into(void** registered_output);
};
class Runtime::BufferReadLease {
public:
    BufferReadLease(Runtime&, RegisteredRootWord source, BufferLayoutRef expected);
    ~BufferReadLease() noexcept;
    BufferReadLease(const BufferReadLease&) = delete;
    BufferReadLease& operator=(const BufferReadLease&) = delete;
    BufferReadLease(BufferReadLease&&) = delete;
    BufferReadLease& operator=(BufferReadLease&&) = delete;
};
// Existing Runtime::store gains buffer-aware reference-leaf lookup/rules;
// paired interface writes require buffer_store_interface, not an object-word store.
```

The new admission overload prepares nominal types, witnesses and schemas in one
copy/validate/publish catalog transaction; references may resolve against that prepared
catalog. The convenience schema method calls this service with empty other inputs.
Existing admission signatures delegate with no buffer specs. Do not introduce a
second admission registry or publish three separate partial catalogs.

Builders and read leases are Managed-only, thread-affine and own small Frame roots.
Their destructors require the same Managed thread and perform no allocation/poll;
wrong-thread/state/LIFO destruction retains the existing fatal Frame policy.
SchemaPath is an input span of field-ID/repeat-index selections, validated against
the immutable schema on every operation, not a trusted byte offset. Root tokens
are short-lived registration proofs; their Frame must remain linked for the call.
The lease copies the validated source word into its own root before any subsequent
poll, so changing the caller's slot cannot redirect the lease to another buffer.
No mutable raw payload address or persistent generation-owned callback is returned.

Builder calls consume the **next** typed event(s), with a complete-element prefix
and bounded in-element cursor. They do not accept arbitrary out-of-order offsets. Event order is a storage protocol,
not permission to reorder source expressions: later lowering must evaluate named
arguments/constructors in source order, staging rooted values where necessary rather
than introducing payload-sized native temporaries.
Missing, repeated, wrong-width or wrong-kind writes fail before changing progress.
Any throwing builder operation poisons that transaction: only destruction/abort is
then legal, and finish rejects. A failed multi-chunk call may have written private
bytes, so retry means a fresh builder, not replaying the whole call over an unknown
cursor position. Constructor failure unwinds all reservations/roots automatically.
write_bytes accepts byte regions/dense u8 streams, not pointer words or arbitrary
serialized structs. A span is a trusted C++ lifetime/provenance precondition, not
proof that an arbitrary address is outside the GC heap. Managed input must use the
rooted source API; the source compiler must never turn a managed borrow into this
native-input span to bypass rooting. Native input must remain host-owned and valid for the call,
including safepoints; it is not an unrooted managed borrow. Managed sources use
root/lease APIs and are reread after polls. write_scalar validates its declared
encoding: width-masked bits, bool 0/1, and existing floating bit patterns. It is
not a new source numeric conversion.

A root token names a registered slot, not a cached raw referent. Validate membership
before polling and reacquire the value afterward. RootedInterfaceSource also binds
an admitted witness; validate opaque catalog membership before dereferencing it.
A complete-element prefix advances only after every required event passes.
zero_elements explicitly appends all-zero data values only where zero_data_complete
proves required-reference/encoding validity. This is not a language-default operation:
physical layout cannot infer a field default of 7, a required constructor execution,
or a fresh list header. Source default lowering must use the Stage 1 pure template
or explicit ordered construction, with an independent source-construction proof
before Stage 3 activation. Stage 2 fixtures use passive physical value records;
GC/data completeness does not certify that arbitrary user constructors ran.

Work quotas bound population/copy work per Managed critical section and guarantee
polling between chunks. They are work bounds, not worst-case wall-clock guarantees;
allocator calls, Heap map growth and existing stop-the-world marking remain separate
latency costs to measure. Bytes must accommodate the schema's largest atomic group;
all quotas must be positive. They are scheduling parameters, not array-size acceptance limits;
no numerical production default/cutoff is selected at this gate. Large elements can
span chunks. Reference words and interface pairs are never torn across a safepoint;
partially constructed elements remain private with valid references or zeros.

### 23.5 Reservation, roots, completeness and publication

Use the following state machine, all through the existing Heap/Runtime:

1. Validate the admitted layout, requested_live <= capacity and quotas using cached
   geometry. Construct a small null private root and link its existing Frame/Slot.
   Keep the caller's eventual output unchanged. Honor existing allocation-budget
   collection outside the mutex, with all source roots already registered.
2. Enter Runtime::Native with the frozen frame chain. Allocate the shape-sized
   cursor work storage here as well; no managed data is read. A Heap-owned PendingBuffer
   obtains raw storage from the Heap's allocation family and zeroes it outside the
   Runtime mutex. It contains no managed references, is not a tracked object and
   has no observable identity. Collection can proceed during allocation/zeroing;
   no managed pointer/payload is dereferenced in this NativeSafe phase.
3. Reenter Managed through the existing rendezvous, then lock and adopt the fully
   zeroed reservation as a **private buffer** in the Heap map. Complete every fallible
   map operation before relinquishing PendingBuffer ownership. Install the private
   Frame root immediately, without a poll or fallible step between adoption/rooting.
   Adoption updates the existing successful-allocation accounting once.
4. Populate in bounded Managed chunks. All reference targets must already be complete,
   same-runtime objects. No incomplete-to-incomplete heap edge is needed in this slice.
   When Stage 3 enables owned-child buffer roles, children must finish into their own
   roots before becoming parent edges; that role is not activated here. The collector
   can trace every capacity slot safely after any chunk.
5. Maintain an authoritative prefix/cursor in the unforgeable builder. All capacity
   after requested_live stays zero; the final partially prepared element is never
   live. Validate semantic completeness incrementally. Do not rescan a multi-megabyte
   payload under one lock in finish_into.
6. finish_into checks the exact output root, full requested-prefix completeness and
   private object identity; poll before commit, then revalidate under the mutex.
   In one nonfallible/no-safepoint critical section set complete + sealed valid_prefix
   and assign the output root. No user callback, allocation, memcpy of the whole
   payload or descriptor creation occurs in commit. This is publication's linearization
   point with respect to collector/runtime operations, not a new lock-free language API.
7. Drop the private builder root after successful root handoff. On abort clear it and
   unlink the Frame; normal GC reclaims the abandoned graph. An unadopted reservation
   is released immediately with matching delete. Do not wipe a huge aborted payload
   while holding the mutex or free a still-rooted managed allocation manually.

For capacity zero, keep the typed empty layout token and no object. No element writes
are permitted; finish_into publishes null only with requested_live=0. Typed APIs
validate the token so null cannot impersonate an arbitrary nonempty buffer.

Private object state is distinct from GC membership. Collector root/edge validation
uses internal tracked membership; source-facing contains/nominal access, persistent
Handle publication and ordinary stores must not expose a private buffer as a complete
value. Ordinary Runtime::store cannot write private destinations. Only the builder
capability can do so; ordinary stores of private targets reject. Debug object-count
telemetry may include private allocations, but is not source-value publication.

This design deliberately zeroes **all** raw bytes before Heap adoption. It needs no
mutable tracing frontier, per-element initialized bitmap or scan of uninitialized
memory. Private/spare nulls are allowed solely because no source operation can read
them as required values. Sealed valid_prefix is authoritative for buffer live access;
GC still traces capacity. Future growable commits must coordinate this prefix with
header count/version, under separately reviewed Stage 4 rules.

### 23.6 Copy correctness, aliasing and readonly

Proposed Stage 2 copying supports scalar buffers, nested physical value records/repeats and managed/
string/interface reference elements into distinct destination allocations. Managed
references retain referent identity, immutable strings share storage, interface
witnesses remain runtime-owned, and copied value bytes are independent. Padding
has no reference authority. Exact schema/profile matching precedes copying.

A read lease roots and freezes the **buffer's value bytes** for chunked copying.
Readonly source syntax alone is not proof of stability against another alias.
Acquire/release the lease under the Runtime mutex; every buffer writer checks the
lease, including the buffer branch of existing store. Never block a Managed writer
waiting for a lease across a safepoint. A conflicting writer throws BufferBusy
before writing. Lease counts use checked increment/decrement with no wrap/underflow.
This is an internal protocol in Stage 2 fixtures, not a new source concurrency rule.
Later lowering must prove exclusive/stable access or resolve it before entering the primitive.
No raw writable buffer export is offered. Referenced ordinary objects can still
mutate: copying their identity does not promise snapshots of their contents.

copy_elements streams directly between a rooted leased source and a distinct private
destination. It never needs an element-sized native temporary. A private destination
cannot alias its source. Every chunk is checked/committed before polling. The lease
is bounded to preparation and released by RAII on failure. Reacquire the rooted base
and Heap metadata under the mutex for each chunk; do not retain heap-map iterators,
metadata addresses or interior pointers across allocations/polls. A lease on one
buffer is not validation of a future growable header/selection version.
Metadata/capability profiles
are preserved; tracing data does not create authority to thaw managed readonly views.

Future type-directed value copying must use these actions:

| Value component | Required action |
| --- | --- |
| Scalar / trivial byte region | Checked bulk copy into independent destination storage |
| Ordinary managed ref / immutable string / interface | Share valid referent identity and preserve capability/witness |
| Inline value record / repeated physical value shape | Compose actions in logical order, with rooted partial destination |
| Owned fixed-array child | Allocate/clone recursively or transfer uniquely owned complete storage under proof; never shallow-copy its pointer as a shared ref |
| Growable array child | Retain stable header identity when Stage 4 representation exists |

Stage 2 must **reject** reserved owned-fixed/growable actions in generic copying;
calling a byte-copy helper is not an implementation of deep value copying. Stage 3
must supply the approved semantic-to-storage copy plan and ownership proof before
these roles can activate. Nested physical Repeat fixtures exercise descriptor
composition, not approval of inline source fixed-array storage. Recursive cloning
will root both owners and each unfinished child, finish children before linking,
and unwind only private state on failure. Constructor evaluation remains compiler-
ordered; no generation-owned user callback is stored in a GC descriptor.

### 23.7 Failure taxonomy and compatibility

| Condition | Required outcome |
| --- | --- |
| Count/base/stride/alignment/end overflow or size_t narrowing | Checked representation/length failure before allocation/address formation; no output mutation |
| Bad spec/unknown role/foreign domain/runtime/nominal identity | Admission or typed-operation rejection; no catalog/heap/output partial commit |
| Metadata, cursor, raw storage, map-node or clone allocation failure | bad_alloc path; RAII roots/reservation/lease unwind; previous output unchanged |
| Incomplete builder, holes, invalid required reference, torn interface | Reject finish/write; never mark complete or publish; structural/invariant failure is not ordinary OOM |
| Corrupt graph/untracked pointer found by collector | Existing invariant-failure policy; no sweep or successful delivery, not a recoverable allocation success |
| GC work allocation failure during preparation | Existing mark rollback and allocation_failure; valid private graph remains rooted until abort; retry a fresh transaction |
| Writer conflicts with active read lease | Explicit internal conflict, no waiting Managed and no partial write; source policy remains gated |
| Allocation succeeds physically but OS overcommit later kills process | No promise of catchable OOM; deterministic injection proves language/runtime rollback paths independently |

Stage 2 C++ tests use existing exception conventions (invalid_argument for bad API
inputs, length/overflow errors for representation, bad_alloc for allocation, logic
errors for broken internal construction/root protocols). BufferBusy is the specific
internal lease-conflict exception; never silently map it to source OOM.
Future C helpers must translate only approved recoverable cases into the existing
RuntimeFailure channel and never unwind through generated frames; no such ABI is
introduced in Stage 2. Invalid precise roots retain existing fatal policy.

Ordinary/string object bytes, nominal descriptors, null-empty-string convention,
UTF-8 checks, Frame/Slot/Handle ABI and all public C adapters remain unchanged.
The legacy offset adapter must be regression-tested against existing layout/store/GC
behavior. Element/schema ownership is runtime data lifetime: active objects retain
metadata through code-generation retirement. Admission compares complete semantic
identity, physical graph, extent, capability and storage domain, not just equal byte
sizes or a hash. Incompatible changes require the existing restart decision. Retired
JIT code is never needed to trace an old buffer. No reload/GC invariant is delegated
to LLVM, and no production ORC/AOT publication gate is relaxed.

### 23.8 Required proof tests and acceptance

| Area | Positive proof | Negative proof |
| --- | --- | --- |
| Descriptor compactness | Millions of scalar/ref elements, nested repeats; metadata nodes/edges independent of capacity | Expanded offset vector; node/edge cycle, missing node, forged identity/domain rejected |
| Geometry | Derived host stride/base, padding, aligned allocations, nested fields, zero-capacity token, zero-footprint elements | Overflow at each sum/product/align/narrow, bad/nonpower alignment, short stride, overlapping refs/regions, leaf past end, foreign pointer width |
| Shared interpretation | Enumerated offsets agree with seek/store/prepared checks for small independently enumerated fixture oracles | Off-by-one/inside-pointer/padding offset and mixed flat/buffer variants reject |
| Existing objects | All existing ordinary/string/root/inheritance/interface fixtures retain behavior, including allowed legacy unaligned memcpy offsets | Wrong kind, corrupt UTF-8/string header, bad old roots remain rejected/fatal |
| Construction | Collect before reservation, during NativeSafe zeroing, after adoption, between scalar/ref chunks and partial large elements | Private pointer escape, unregistered output, foreign-root token, incomplete child edge, premature finish, null required live ref reject |
| Live versus spare | Source access restricted to sealed prefix; unused capacity zeros trace safely | Count/prefix forgery; live access/store in spare slot; corrupted nonzero reference in sealed unused tail; private source lease reject |
| Copies | Distinct multi-megabyte allocations; mutate either copy, other unchanged; shared referent identity preserved; nested records/ref repeats | Source/destination alias, active-lease write, unregistered/foreign source root, shallow owned-fixed/growable copy action reject |
| Failure injection | Sweep new-allocation failure ordinals across admission, cursor, reservation, adoption and copying; old sentinel/root stable and retry succeeds | Any changed catalog/output, leaked lease/frame, double free, stale requested epoch, partial sweep or published partial value |
| Scheduling | Collector completes while raw allocation/zeroing is NativeSafe; bounded chunk polls; reference-free tracing independent of byte extent | Poll while holding required mutex, NativeSafe managed dereference, oversized atomic group for quota, starvation/deadlock |
| Lifetime | Destroy caller specs, retire allocating generation and collect/use old buffers with retained schemas | Compiler-owned pointer, executable tracing callback, equal-size incompatible schema accepted |
| Native stack | Large runtime buffer fixtures under 1 MiB Debug/Release stacks, bounded cursor/handle homes | Payload-sized stack input/staging/root arrays or hidden deep recursive native work |

Use independently enumerated small-layout oracles, not just two calls to the same
walker. Allocation failure ordinal sweeps must first measure the successful path's
allocation sites, then cover each site; do not rely on one guessed fail_after value.
Exercise cancellation/abort and rollback before a successful retry. Preserve existing
mark-worklist failure tests in tests/runtime/failures.cpp, fatal invariant subprocess
tests, coordinated epochs, interface crash capture and research destination sentinels.

Benchmarks record descriptor bytes, allocation/zero/copy throughput, copied bytes,
peak live memory and collector pause distributions for byte and reference-rich
buffers. No frame-time or OOM guarantee is claimed from average elapsed time. Collection
itself still stops mutators while tracing actual reference locations; permitting
mutators during marking would require write barriers/another architecture decision.

### 23.9 Smallest implementation sequence after approval

1. **2B, schemas/common cursor:** Implement runtime-owned admission, exact domain/
   checked geometry, legacy adapter, seek/stream invariants and independent oracles.
   No new allocation/source path yet. Keep existing objects' behavior and all 61 suites.
2. **2C, private rooted byte buffers:** Same-Heap PendingBuffer reservation, NativeSafe
   zeroing, aligned adoption and existing Frame root handoff; pure byte defaults,
   sealed prefix and nonfallible finish. Prove failure/collection boundaries before
   allowing reference-bearing population.
3. **2D, reference-bearing preparation:** Typed reference/interface events, zero spare
   capacity, required-value completeness, common prepared validation and bounded
   chunk polling. Prove no private escape and preserve failed-collection rollback.
4. **2E, stable-source copying:** Checked read leases and every-writer enforcement,
   distinct destinations, nested physical shapes and explicit unsupported owned-copy
   negatives. Prove alias/conflict/failure behavior; no source-level borrow optimization.
5. **2F, acceptance/review:** Debug/Release, strict sanitizers, failure ordinals, 1 MiB
   stacks, stress and runtime-native Windows x64/macOS ARM64/Linux x64 acceptance
   using existing CI conventions only when separately authorized. Keep all current
   suites, add focused runtime suites, preserve ABI/stack/CFG research, then stop.

The smallest useful first slice is 2B followed by 2C byte buffers; it does not need
source native arrays or a selected array-call ABI. Stop rather than substitute
unproved alignment, source stability, reference completeness or publication rules.
No later stage is authorized by approving a substep.

**Approved architectural direction, implemented through the separately authorized
2B and 2C checkpoints only:**
runtime-owned DAG/cursor; mutually exclusive
buffer layout and typed empty representation; same-Heap NativeSafe zero reservation
then rooted private adoption; prefix/event completeness and sealed publication;
checked nonblocking read-lease protocol for runtime fixtures; reserved owned-copy
rejection pending Stage 3; the incremental review sequence above. Chunk scheduling
numbers, source ABI/owned-handle layout, production stack policy, growable header
commit protocol and source concurrency semantics are not selected here.

The original Stage 2A checkpoint changed documentation only. The subsequent owner
authorizations separately permit 23.9 steps 2B and 2C, recorded below; remaining
implementation steps require their own review. Research checks, CI, Git history and remotes remain
unchanged.


## 24. Stage 2B: compact schemas and shared reference cursor

15 October 2026. Bounded runtime implementation following approved Section 23.9.
This checkpoint adds metadata and reference-map services only. There is no buffer
object kind, allocation/reservation API, private builder, new generated-code ABI,
source array execution or production machine-certificate activation.

### 24.1 Admission, identity and lifetime

`include/void/gc_schema.hpp` defines `ElementSchemaSpec`, the discriminated
`SchemaNode` variants, `StorageDomain`, opaque immutable `ElementSchemaRef`,
`ReferenceShape` and `ReferenceCursor`. Runtime provides the three-input
`admit_metadata(nominals, witnesses, schemas)` overload, `admit_buffer_schemas`,
`buffer_schema(identity)` and `reference_shape(handle)`. Existing admission
signatures delegate with no schemas. The same catalog snapshot/prepare/compare/
publish transaction admits all three metadata classes; a failing allocation,
validation or compatibility check publishes none of them. Concurrent admissions
retry against the latest catalog. Admission remains host/NativeSafe only.

Specs are copied into runtime-owned storage. Child indices must precede parents;
all nodes must be reachable from the root. Admission validates every node,
including children of empty repeats, then shares structurally identical nodes.
Node keys contain local child IDs, never recursively expanded child text.
Compatibility compares the complete rooted structure iteratively, independently
of topological node numbering, including nominal identities, field IDs, nullability,
capability profile, domain and layout key. Equal byte size is insufficient.
Compatible readmission retains the existing canonical handle. Input node/field
ordering is a representation contract; it cannot change the declared field IDs,
offsets or child shapes under an existing identity.

Domain facts come from actual native C++ sizes/alignments and byte order, without
LLVM. All scalar encodings, pointer geometry and storage-format revision must match.
Record fields have unique IDs, aligned contained disjoint nonzero physical spans;
zero-byte fields remain logically selectable without shadowing physical lookup.
Repeats validate positive aligned stride, child span, base, i64 extent and exact
rounded total bytes with checked sums/products/alignment. Zero repeats have zero
base/bytes but still validate their child/stride. Metadata work depends on distinct
nodes/edges, not repetition count. Allocation exhaustion throws through the existing
C++ exception convention; no replacement payload/count cutoff is introduced.
Power-of-two over-alignment can be represented as metadata; allocator support and
matching aligned deletion remain an unproved Stage 2C obligation, not an allocation
capability granted by schema admission.

Managed targets resolve to prepared/admitted nominal descriptors. Interface targets
resolve to prepared/admitted witnesses with the exact interface identity because
the current catalog has no standalone interface-declaration registry. An interface
without any witness is conservatively rejected; this does not invent a new registry
or approve source interface construction. Strong metadata references retain nominal
and witness dependencies. Schemas contain no executable pointers, LLVM objects or
Runtime backpointer. Handles/cursors may retain immutable geometry after Runtime
retirement; this conveys no heap/allocation authority. `Runtime::reference_shape`
requires exact pointer membership in that Runtime's catalog, rejecting equal-key
foreign handles. Callers cannot construct/copy an admitted `ElementSchema` object.

### 24.2 One reference interpretation and legacy compatibility

`ReferenceCursor::next()` streams physical-order `ReferenceEvent`s with offset,
kind, target/nullability and optional interface witness offset. Only an interface's
object word is a trace reference. `ReferenceShape::at(byte_offset)` uses the same
nodes with sorted record lookup and repeat quotient/remainder; only exact reference
word starts match. `path(field-ID/element-index steps)` descends checked logical
selections. Pointer interiors, witnesses, padding, invalid IDs and out-of-range
indices return no reference event. These operations describe reference geometry;
they do not yet validate typed live values or expose scalar-construction events.

The cursor uses an explicit heap work stack bounded by schema depth, reserves before
replacing its state and performs no allocation during `next()`. Reference-free
subgraphs and empty repeats are skipped without visiting each element. Geometry
validation and structural comparison are iterative too, avoiding a native recursive
call chain for deep finite DAGs. Event target string views borrow metadata retained
by the shape/cursor; callers must retain that owner while using the view.

The initial Stage 2B `ReferenceShape::legacy` copied flat offsets into a private
validated DAG record with shared generic reference leaves. The performance followup
in Section 25 replaces this expansion with an owned validated flat map inside the
same service. Its alignment-one compatibility contract
preserves accepted unaligned memcpy offsets and rejects duplicate/overlapping or
out-of-bounds words. The adapter is not admissible as a new typed buffer schema.
Heap objects retain this reference shape. Heap allocation validation, marking,
checked stores, nominal layout normalization and `construct_into` prepared-reference
validation use the shared service. Existing ordinary/string payloads, output-root
publication, UTF-8 checks, null-empty-string convention and Frame/Slot/Handle ABI
remain unchanged. Frame/Slot root registration retains its existing ABI and root
alias rules; it is not reclassified as a new heap element schema.

The collector still validates the graph before sweeping and resets marks after
failure. Its pending-object queue/mark ordering is unchanged. This checkpoint does
not claim the future duplicate-reference queue optimization or chunked preparation,
value completeness, target/nullability checking for buffer stores, private-object
states, read leases or safe publication. No DAG-backed payload can be allocated
through the legacy Heap API.

### 24.3 Focused proof and local validation

`tests/runtime/schemas.cpp` adds `gc_schemas`, preserving the existing 61 suites.
Independent small-layout oracles cover nested records/repeats, physical and logical
lookups, interface object versus witness words and every byte of legacy unaligned
layouts. Million-reference metadata has two nodes/one edge; the cursor streams
without an expanded offset vector or a payload allocation. A billion-byte
reference-free shape is skipped. Tests also cover all scalar domains, empty and
zero-footprint children, duplicate nodes, alternate topological numbering, deep
iterative traversal, detached metadata lifetime and concurrent catalog admission.
Forged domains/identities/targets, reserved roles, cycles/forward edges, unused
nodes, overlap, invalid stride/alignment, every extent arithmetic boundary, foreign
handles and incompatible readmission reject. A measured 48-allocation-site sweep
checks all-or-nothing nominal/witness/schema admission and successful retry;
incompatible readmission also preserves the old catalog. Cursor reserve-failure
rollback preserves its previous traversal. On local Linux x64, requested admission
allocation bytes were 1,886 for one reference versus 1,917 for a million with
identical catalog seeds and type identity. Streaming the million events requested
zero allocations after reset. These are requested metadata bytes measured by the
fixture allocation hook, not resident-memory or payload/GC throughput measurements.

Local Linux x64 LLVM 19.1.7 verification: all 62 suites passed in Debug
(120.66 s) and Release (56.64 s). The final expanded schema fixture passed again
in both configurations. Nine GC/runtime/string/interface suites also passed under
explicit 1 MiB Debug/Release native stack limits (12.66 s / 3.95 s), including
normal/O2 independently linked runtime AOT, generated JIT, callbacks, collection
failure rollback and the 2,048-level iterative schema fixture. The full strict
ASan/UBSan RelWithDebInfo rebuild and all 62 suites passed (146.99 s), with
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Generated build steps and tests
ran outside the desktop ptrace sandbox because LeakSanitizer explicitly rejects
ptrace; leak checks stayed enabled. `git diff --check` and an exact changed-file
audit passed. Native hosted Windows/macOS execution is not claimed. Stage 2B is
complete locally for owner/wrench review; Stage 2C, GC publication changes, source
arrays and ABI/certification activation remain gated.


## 25. Stage 2B followup: ordinary Heap performance audit

15 October 2026. Owner reports CI #48 green, seven jobs and 62/62 suites,
locking the Stage 2B baseline. This focused followup compares `3c69f66` with
`cb77d62` and the local optimization. It does not implement Stage 2C.

### 25.1 Reproducible method

`tools/bench/heap_legacy.py` reads both Git revisions into ignored build snapshots
without changing Git state. It compiles the same ordinary Heap fixture against each
revision and the working sources with Clang 19.1.7 and the existing Release flags:
`-O3 -DNDEBUG -std=c++20 -frtti -Wall -Wextra -Wpedantic`. Only Heap and, where
present, the schema service are linked; no LLVM compiler/backend optimization is
disabled, and no whole-compiler benchmark substitute is used. This is a direct
ordinary Heap audit, not a claim about coordinated Runtime latency or source arrays.

Final command: `python3 tools/bench/heap_legacy.py --output
build/phase23-gc-perf-reviewed --samples 13 --objects 100000 --cpu 0 --label
optimized` (one shell line). All three variants use identical flags and fixture
code. Thirteen process samples rotate variant order on the same allowed logical
CPU after a warm-up process for each variant. Each fresh Heap allocates 100,000
ordinary objects with 0/1/4/16 deliberately unsorted, unaligned reference offsets;
payload bytes are `32 + reference_count * sizeof(void*)`. Nonempty layouts form
a real backwards chain through the first word. Three all-live collections and one
all-dead collection assert the exact object counts. Timings use the normal allocator.

Separate instrumented executables count new calls, requested bytes and retained
requested live bytes. The fixture root vector is allocated outside the measured
phase; payload bytes are subtracted from allocation metadata totals. Hash nodes and
buckets remain included. These are C++ allocation requests, not RSS or malloc size-
class overhead; instrumentation headers are excluded. Instrumented timings are
not used for throughput. Each run records commands, exact commit IDs, source/header
SHA-256 hashes, CPU affinity and all raw samples in `results.json` under the output
directory. Local Linux x64 execution only; no Windows/macOS performance is claimed.

### 25.2 Finding and bounded correction

The original per-allocation legacy DAG construction is materially expensive.
It allocated node variants, summaries, field indexes, canonicalization keys/maps
and control blocks for a shape already supplied as a flat offset list, including
for reference-free objects. The first seven-sample/50,000-object audit reproduced
roughly 5-11x allocation latency and substantially increased collection costs.
The final three-way measurements below reproduce the defect against the locked
Stage 2B snapshot rather than relying on a remembered baseline.

The correction is an owned immutable `LegacyReferenceMap`: validate/sort/copy the
offsets once, retaining the original byte extent and legacy unaligned memcpy rules.
It is the flat mode of the same reference service, not another collector or a
per-Heap/global cache. Heap nodes own this value together with kind and nominal
descriptor; they no longer duplicate the offset vector in a second TypeLayout copy.
Empty maps allocate no offset metadata. Public `ReferenceShape::legacy` and cursors
still retain strong ownership, including temporary/copy/move lifetimes.

Heap and prepared construction alone may create a private, scoped borrowing cursor
over their own validated map. The map remains alive and immutable for traversal;
Heap traversal occurs under its existing exclusion, before sweep, with no user
callback or map mutation. Construction keeps its local map alive across polling
and validation, then moves it into the new node after traversal. Public cursor
operations cannot request this borrow. `next_offset()` projects the same stream
position as `next()`, avoiding unused typed event construction in GC; mixed-call
regressions check this for both flat and DAG shapes. Lookup and prepared validation
use that same validated map/cursor. Typed DAG admission/geometry is unchanged; the
obsolete private legacy-only DAG alignment bypass is removed. No checks, payload
publication, roots, string/nominal behavior, GC queue/mark rollback or native ABI
are relaxed. No registry of retained historical layouts is introduced.

### 25.3 Measured results

Cells below are **parent / locked Stage 2B / optimized**, per object. New-call cells
exclude the 14 common bucket-growth allocations per 100,000-object run; byte cells
include their actual contribution. All counts are from separately instrumented
executables using the exact same fixture.

| Reference words | New calls | Requested metadata bytes | Retained metadata bytes |
| --- | --- | --- | --- |
| 0 | 2 / 11 / 2 | 107.26 / 963.26 / 99.26 | 93.83 / 701.83 / 85.83 |
| 1 | 4 / 18 / 3 | 123.26 / 1499.26 / 107.26 | 101.83 / 909.83 / 93.83 |
| 4 | 4 / 22 / 3 | 171.26 / 1831.26 / 131.26 | 125.83 / 1053.83 / 117.83 |
| 16 | 4 / 26 / 3 | 363.26 / 3153.26 / 227.26 | 221.83 / 1629.83 / 213.83 |

Median nanoseconds per object (same **parent / Stage 2B / optimized** order):

| Reference words | Allocate | All-live collect | All-dead collect |
| --- | --- | --- | --- |
| 0 | 174.6 / 1020.9 / 170.1 | 44.0 / 107.2 / 40.0 | 54.9 / 204.9 / 47.6 |
| 1 | 168.1 / 1363.9 / 155.2 | 66.5 / 330.0 / 70.4 | 74.6 / 406.3 / 73.4 |
| 4 | 167.0 / 1642.7 / 148.3 | 74.9 / 538.6 / 83.7 | 82.0 / 405.7 / 80.3 |
| 16 | 420.5 / 3192.2 / 372.7 | 144.9 / 843.0 / 134.3 | 118.5 / 541.4 / 94.4 |

Allocation throughput in millions of objects/second:

| Reference words | Parent | Stage 2B | Optimized |
| --- | --- | --- | --- |
| 0 | 5.73 | 0.98 | 5.88 |
| 1 | 5.95 | 0.73 | 6.44 |
| 4 | 5.99 | 0.61 | 6.74 |
| 16 | 2.38 | 0.31 | 2.68 |

Retained requested metadata is eight bytes/object below the parent on this native
C++/standard-library layout, versus the original Stage 2B increase of hundreds of
bytes. This is a measurement, not a portable object-size guarantee. All three live
collections request 54 allocations/6,291,432 bytes in parent and optimized runs;
Stage 2B reference-bearing runs request 57/6,291,576 due to cursor work-stack setup.
There is no per-object or per-reference allocation during the optimized mark walk.

Allocation medians improve relative to the parent in these fixtures. GC does not
establish universal performance parity: one-reference live GC is 70.4 versus 66.5
ns/object (+5.7%), and four-reference live GC is 83.7 versus 74.9 (+11.8%). Their
13-sample ranges overlap (64.0-101.6 versus 63.2-92.4, and 78.2-120.0 versus
69.9-117.8 ns/object). Reference-free/16-reference medians improve; these overlapping
local ranges neither prove zero residual cost nor a production frame-time bound.
The scoped flat correction removes the large per-object DAG amplification without
expanding the optimization into collector scheduling, allocator tuning or a cache.

### 25.4 Verification and review boundary

Full local rebuilds and CTest passed: Debug 62/62 (108.73 s), Release 62/62
(53.69 s), strict ASan/UBSan RelWithDebInfo 62/62 (135.02 s). Sanitized build steps
and tests ran outside the ptrace sandbox with leak detection/abort enabled and
UBSan halt/stacktrace enabled. Nine selected GC/runtime/string/interface suites
also passed with explicit 1 MiB Debug/Release stacks (11.73 s / 3.74 s), including
normal/O2 native AOT and generated JIT coverage. Expanded schema tests cover owning
flat cursor temporaries/copies/moves, shared offset/event positions for flat/DAG
shapes, legacy logical paths, caller mutation/alternating maps and every measured
fast-allocation failure site. Existing forged geometry, independent oracles,
million-element compactness, mark rollback, invariant termination, interface crash
capture and research rejection suites are retained. The combined catalog sweep
now measures/covers all 40 successful-path allocation sites rather than the original
48; the change removes real metadata work without reducing the ordinal coverage.
The one/million typed-schema admission requests are 1,878/1,909 bytes, and streaming
still allocates nothing. Existing mark-work failure/recovery covers seven failing
positions. Whitespace, source-hash and changed-file audits passed.

At the end of this Stage 2B audit, Stage 2C and native arrays remained disabled.
The subsequent Stage 2C authorization/evidence is in Section 26. No production
certificate, stack policy, CI workflow, commit,
push, branch or remote modification is part of this audit.


## 26. Stage 2C: private rooted reference-free buffers

15 October 2026. Owner/wrench authorize only Section 23.9 step 2C, after the
owner-reported CI #49 lock of 4d25f4f (seven jobs, 62/62 suites). The implementation
is a C++ runtime service; it is not source-native array execution, a fixed-array
value representation, parameter/result ABI, reference-bearing population or copying.

### 26.1 Exact layout and ownership APIs

`include/void/buffer.hpp` adds runtime-factory-created, immutable `BufferLayoutRef`.
`Runtime::buffer_layout(ElementSchemaRef, uint64_t capacity, size_t base=0,
size_t stride=0)` validates exact admitted schema pointer membership. A zero stride
argument requests the checked natural stride: alignUp(max(schema.bytes,1), alignment).
Explicit stride must contain the child span and meet native alignment; base must
also meet it. Extent is checked alignUp(base + capacity*stride, alignment). Count
fits i64; every product, sum, rounding and target size_t conversion precedes address
formation/allocation. Allocation extents must fit native ptrdiff_t for byte-address
arithmetic. This is target representation protection, not a source size ceiling.

Zero-capacity layouts require base=0, retain valid child/stride metadata and have
bytes=0. The typed empty representation is the retained layout token plus null;
finish publishes null only with requested_live=0 and allocates no Heap object.
Nonempty zero-footprint schemas retain positive stride and real backing; they are
not confused with zero capacity. Capacity layouts are reference-counted by callers
and live buffer metadata, never permanently interned. Equivalence requires the exact
canonical schema pointer and all capacity/base/stride/byte geometry, so independently
created equivalent tokens work while equal-byte foreign/incompatible schemas reject.

The implemented embedding surface is `Runtime::BufferBuilder(Runtime&, layout,
requested_live, BufferWorkQuota)`, `write_scalar(bits)`, `write_bytes(native_span)`,
`zero_elements(count)` and `finish_into(registered_output)`. Quotas are supplied
explicitly, all positive, with bytes large enough for a scalar atomic group. There
is no production numerical default. `Runtime::buffer_byte(object, expected_layout,
element_index, byte_offset)` returns one copied byte from a complete live element,
Managed-only under the mutex, with no poll or raw pointer export. It rejects private,
foreign, incompatible, spare-capacity and out-of-bounds access. This inspection
helper does not create a read lease or managed copying API.

### 26.2 Same-Heap reservation, private roots and publication

`Heap::PendingBuffer` uses the existing new/delete family. Native alignment at or
below default-new alignment uses ordinary new/delete; over-alignment uses matching
align_val_t new/delete. The returned pointer is checked against the required
alignment before zeroing. All raw bytes are zeroed while NativeSafe outside the
collector-required mutex. Reservations contain no managed reference or Heap-map
identity. An exception before adoption releases raw storage with its recorded
layout/alignment; failed map insertion leaves ownership with PendingBuffer.

Builder construction links a small null private root through the existing Frame/
Slot before budget collection or NativeSafe work. Frame and Slot layout/ABI are
unchanged. A separate per-thread private-root registration authorizes exactly that
Frame word and private owner; it does not authorize another root containing the same
pointer. Cursor scratch and buffer metadata are allocated NativeSafe. Reentry uses
Runtime::Native's existing Managed rendezvous. Under the mutex, Heap adoption
completes its fallible map insertion, transfers raw ownership, installs the private
root immediately and increments the existing successful-allocation accounting once.
No poll/allocation/fallible step separates adoption and root installation. Capacity
zero does not consume a successful Heap allocation or trigger allocation-budget work.

Heap nodes retain the validated layout and private-owner/complete/valid-prefix state
in data-only metadata. The existing shared-owner slot is selected by the validated
object-kind tag; ordinary nodes do not gain a second reference map or per-object
allocation. Legacy maps and their optimized cursor remain unchanged. TypeLayout's
reserved buffer kind is rejected by ordinary Heap::allocate, allocate_into and
construct_into; only private adoption creates it. There is one Heap/collector and
one ownership transaction, not a second allocator/GC.

Private membership is internal: source-facing contains, Handle construction/reset,
ordinary stores and inspection cannot expose it as complete. Frame root enumeration
accepts private pointers only in their registered builder word; an ordinary root or
heap edge containing a private pointer is a precise-GC invariant failure before
sweep. A private-buffer counter avoids extra target lookups in ordinary GC when no
private buffer exists. Stage 2C adoption accepts only reference-free schemas; those
objects require no payload scan. Mark-before-sweep, pending queue ordering, failure
rollback and all ordinary/string behavior remain.

`finish_into` checks exact registered public output, full requested-prefix completion
and private ownership, polls, then rechecks under the mutex. The commit seals the
valid prefix, marks complete, publishes the output and clears the private builder
word without allocation, safepoint, callback or payload-sized scan/copy. The internal
seal helper is noexcept after validation. Failed/incomplete publication preserves
the old output. Abort clears/unlinks the private root; normal GC later reclaims the
abandoned object. It does not synchronously wipe/free a rooted payload under the
mutex. Wrong-thread/state/LIFO destruction retains Frame's fatal invariant policy.

### 26.3 Ordered physical construction and scope gates

The runtime-internal `DataCursor` walks the same admitted schema nodes and physical
field indexes, using a depth-sized explicit work stack prepared NativeSafe. Admission
caches whether a subtree has data; empty/zero-footprint subtrees are skipped. Record
and Repeat events do not expand into per-element metadata. Dense byte/u8 repetitions
coalesce into spans while preserving the ability to consume u8 scalar events one at
a time. The outer dense-u8 path advances a checked complete-element prefix per bulk
chunk. Large byte events can span chunks. Node-walk and byte work respect supplied
quotas, with polling between chunks and bounded scalar writes.

Scalars use exact native 1/2/4/8-byte encoding, checked high bits and bool 0/1;
floating encodings require native IEC559 support and preserve bit patterns. Byte
writes accept byte regions/u8 streams, not arbitrary typed scalar records or pointer
words. A complete-element prefix advances only after all events; private partial
progress is unforgeable. All-zero data is complete for this closed reference-free
subset, so zero_elements can append untouched zeroed whole elements without a
payload walk. Zero-footprint elements need no data events. These are physical values,
not execution of language field defaults, constructors or fresh-reference defaults.
Any throwing builder operation poisons the transaction; only cleanup remains legal.
A failed multi-chunk input may have changed private bytes but cannot publish them.

Native input spans must remain host-owned/valid across polls; this is the explicit
C++ lifetime/provenance precondition from Section 23.4, not an escape hatch for an
unrooted managed borrow. Every reference/string/interface node is rejected before
reservation, even inside a zero repetition. Stage 2D must separately add capacity
tracing, typed reference validation and completeness/publication proofs. No reference
writes, read leases, deep/owned copying, growable headers, generated-code ABI, LLVM
lowering, ORC/LSP certification or source native-array activation is introduced here.
The stack/ABI/encoded-CFG research and its rejection gates remain unchanged.

### 26.4 Focused proof and measurements

`gc_buffers` adds one suite, preserving all 62 existing suites. Independent byte
oracles cover 4 MiB dense input, nested records/repeats, every scalar encoding,
padding and spare zeros. Positive cases include 64 MiB NativeSafe reservations,
256-byte over-alignment with independently checked deletion family, typed empty
capacity, million zero-footprint elements, equivalent layout tokens, retained then
collected capacity metadata and 2,048-deep data shapes. Negatives cover arithmetic,
rounding, native address representation, foreign/invalid layout/schema/root,
incompatible equal-byte identity, bad quotas/width/bool/kind/order, incomplete
publication, forbidden legacy allocation and poisoned reuse.

Measured constructor-allocation ordinal sweeps exercise normal and budget-triggered
collection paths; all successful-path sites are injected, old outputs/epochs remain
valid and every reservation/metadata/node is reclaimed. Separate subprocesses require
fatal code 79 for private-root and private-edge escape attempts, retaining the existing
invariant classification instead of treating corrupt graphs as recoverable failures.

Linux test-only linker wrappers pause a production memset call after exactly half
of the poisoned raw reservation is zeroed; collection completes with no Heap identity
before zeroing resumes. Clang ASan uses the corresponding __asan_memset wrapper.
There is no runtime hook or disabled optimization. Other native platforms use the
allocation pause and concurrent NativeSafe-phase witness, without claiming an exact
half-zeroed observation. Collection after adoption, within a partial record, between
chunks, before publication and after abandonment is exercised, together with actual
cooperative parks. Native Windows/macOS execution is not claimed locally.

Local Linux x64 LLVM 19.1.7 full rebuild/CTest: **Debug 63/63 (115.11 s),
Release 63/63 (55.60 s), strict ASan/UBSan RelWithDebInfo 63/63 (140.54 s)**.
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 stayed enabled for sanitized build
steps and tests, outside the desktop ptrace sandbox. The final strengthened lifetime
fixture then passed the four focused sanitizer suites (1.12 s). Explicit 1 MiB
Debug/Release runs passed 10/10 suites (13.25 s / 4.07 s), including final buffer
regressions, generated JIT, normal/O2 linked AOT, callbacks, schemas, collection
failure recovery, strings and interfaces. Existing invariant/crash instrumentation
and research sentinels remain in the full suite. Measured allocation-site sweeps
cover all four normal constructor sites and six budget-triggered sites. The local
Debug chunk witness completed 2,046 collections and exercised cooperative parking;
this count is scheduling-dependent, not a throughput promise.

Release `void_gc_buffers --measure` uses explicit 64 KiB byte/64-node fixture work
quotas, one live buffer, a host-owned input vector outside the measured region,
prewarmed map buckets and three samples per size. These quotas are test inputs,
unrelated to the historical stack threshold. The fixture records actual global-new
requests with matching-family deletion checks; bytes include metadata and payload,
not RSS or allocator overhead. Both 1 MiB and 32 MiB buffers use four allocations
and payload + 152 requested bytes, with zero retained measured storage after GC.
Native embedding sizes are Frame=32, Slot=32 (unchanged) and BufferBuilder=264 bytes;
these are C++ object sizes, not total machine-frame or recursive-stack certificates.
No payload-sized native temporary exists; deep cursor storage is allocated NativeSafe
on the native heap. The 1 MiB execution limits provide actual configuration evidence.

Three-sample medians from the final local Release run:

| Buffer bytes | Allocate + zero MiB/s | Populate MiB/s | Finish us | Live collect us | Dead collect us |
| --- | --- | --- | --- | --- | --- |
| 1,048,576 | 2,552.73 | 29,137.5 | 0.20 | 0.33 | 0.41 |
| 33,554,432 | 6,454.79 | 5,880.50 | 1.66 | 2.48 | 426.82 |

Cold/warm allocator effects are visible: 1 MiB allocation/zeroing ranges from
1,990.77 to 49,726.5 MiB/s and dead collection from 0.17 to 81.31 us. For 32 MiB,
allocation/zeroing ranges 6,375.67-6,752.07 MiB/s, population 5,266.36-6,558.99,
finish 1.48-1.89 us, live collect 2.359-3.21 us and dead collect 345.918-476.067 us.
This is a local instrumented microbenchmark, not a worst-case time or game frame
benchmark. Sweep/deallocation cost can scale with allocator/OS work despite the
reference-free mark path and publication avoiding a payload walk.

The existing reproducible legacy audit was rerun against locked 4d25f4f using
`tools/bench/heap_legacy.py --output build/phase23-2c-legacy-final --parent 4d25f4f
--stage2b 4d25f4f --samples 9 --objects 100000 --cpu 0 --label stage2c`.
Source hashes in results.json match the final production sources. Per-object new
calls (2 without refs, 3 with refs), requested metadata and retained metadata are
unchanged for 0/1/4/16-reference ordinary fixtures. Retained requested bytes/object
remain 85.83/93.83/117.83/213.83, including buckets. Median allocation ns/object is
157.3/145.0/141.1/357.3 locked versus 162.8/161.6/153.1/384.8 now; live-GC medians
are 40.1/70.9/87.0/135.9 versus 43.0/71.1/90.1/136.8. Do not claim zero timing cost
from the new kind/privacy checks, or attribute every difference solely to them;
raw ranges/control samples are retained in the artifact. The material per-object
DAG amplification remains eliminated without adding a legacy cache or metadata
allocation. Tests and benchmark tools stay operational; no CI workflow was changed.

### 26.5 Remaining gates and stop

Stage 2C is complete locally for owner/wrench review. Stage 2D reference-bearing
construction/capacity tracing remains blocked, including required-reference live
validity and interface atomic pairs. Stage 2E leases/copies, owned fixed payloads,
source defaults/constructors, managed mutation, source array ABI, LLVM lowering,
ORC/LSP/public certification and target-native hosted acceptance remain separate.
Native Windows/macOS tests were not run locally. No commits, pushes, branches or
remote changes were made.
Allocator/OS work, metadata/map growth and stop-the-world sweep can still block;
no production latency or catchable-overcommit guarantee is inferred. Caller quotas
in tests/measurements are scheduling inputs, not approved production limits.


### 26.6 CI #50: deterministic partial-buffer collection witness

15 October 2026. Owner reports Windows 2022 Release CI #50 passed 62/63 suites,
with gc_buffers reporting "no collection/poll interaction exercised"; Windows
Debug passed 63/63. That old message combines completed collections and cooperative
parks. Neither counter was printed, so its exact hosted values cannot be recovered
from the reported assertion. Do not claim that the Windows failure identified one
particular counter or established a runtime failure.

The old collector signaled readiness before entering its collect/done loop. A
writer could finish all construction and set done before any request; alternatively
collections could complete before the writer became Managed, with no cooperative
park. Both schedules are legal. Temporary Release witnesses, using latches to
control these schedules without sleeps, reproduced the original failure with
collections=0/parks=0 and collections=1/parks=0 respectively. These were Linux
executions of the existing runtime, not native Windows reproductions.

Only tests/runtime/buffers.cpp changes: write a 4,096-byte prefix of the existing
4 MiB buffer, keep its builder/root and writer Managed, and use an unattached
observer to wait for an actual collection request. The single collector cannot
finish while that writer stays Managed. The observed snapshot must contain the
next pending epoch, unchanged completed epoch, exactly two attached threads, one
parked collector and one private Heap object. The remaining write_bytes must then
poll/park. A snapshot taken before finish_into or any Native transition must prove
that epoch completed, the private object survived and the cooperative-park count
increased exactly once. Publication and byte checks cover both sides of the prefix
boundary and the final element. Distinct assertions distinguish overlap, collection,
completion and park failures; successful logs print both counters.

The observer's five-second wait is solely a failure watchdog. A timeout cannot
satisfy the test. No sleep, retry/count inflation, scheduling yield, disabled
assertion or timing-based success condition remains. An initial local observer
prototype used milliseconds::max with the timed wait; this returned immediately on
the local standard library and led to a future-unwind wait until the existing
150-second subprocess watchdog failed in Debug/Release. It was corrected to the
finite watchdog and deferred checks before the final successful validation. No
runtime timed-wait API was changed by this test correction.

A temporary negative witness compiled a separate copy of buffer.cpp with only
write_bytes polls removed, leaving finish_into polling intact. It fails with
"chunk write did not complete the pending collection". This demonstrates that a
later publication poll or NativeSafe join cannot substitute for the required
construction poll. Neither temporary witness changes tracked production sources.

Final Linux x64 checks, existing LLVM 19.1.7 builds: nine selected runtime suites
pass in Debug (2.87 s), Release (2.03 s), strict ASan/UBSan (2.06 s), and explicit
1 MiB Debug (2.72 s)/Release (2.03 s) stacks. The selection includes gc_buffers,
gc_schemas, runtime_coordination/generated/callbacks/failures/invariants and both
normal/O2 independently linked runtime AOT suites. gc_buffers retains the fatal
private-root/private-edge subprocesses and all four/six constructor-allocation
failure ordinals. The final direct Release witness reports exactly one completed
collection and one cooperative park. This focused test-only correction did not
rerun the full 63-suite matrix. Native Windows 2022 execution is unavailable here;
owner-controlled hosted validation remains required. Runtime/compiler code,
Stage 2D gates, CI workflows and Git history remain untouched. Stop for owner/wrench
review.
