# Phase 23 proposal: typed arrays, indexing and iteration

**Status: A1-A10 APPROVED AS REVISED; implementation authorized at P23-1 through
P23-6. Owner/wrench lock P23-1 after run #40 passed seven jobs, 60/60 suites
per configuration. Native arrays remain unimplemented at the P23-2 storage proof
gate. The current assignment is ABI investigation only, recorded in Section 13.**

The owner approved the revised architecture and recorded choices, including final
i32/i64 ranges, on 14 October 2026. The owner now approves 64 KiB (65536 bytes) as the initial target-aware per-function
native static-storage budget for array-carrying functions, subject to the storage
planning and ABI proofs below. This is not an allocation/save-file limit or an
arbitrary-recursion safety guarantee. P23-2 implementation was authorized, then
stopped at its required proof gate; production work is paused for the currently
authorized ABI investigation. Section 10 records independently verified native
probe run #3 evidence; Sections 12 and 13 record the subsequent ABI investigations.

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
subject to finite inline layout and explicit resource limits. Private construction
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
capacity is private GC storage only. The earlier 256 KiB proposal is superseded by the owner-approved 64 KiB
per-function policy, subject to target-aware accounting and implementation proofs.

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

- `[t, n]` is an inline value type whose identity includes the exact element type
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
assignment, arguments and returns copy inline elements; embedded list/managed
references retain target identity, and strings share immutable content. No implicit
deep clone, frozen object or source-visible stack/heap choice is introduced.

A const list is a deep readonly access path. Copying that reference to var cannot
thaw it. Independently held mutable aliases may still change it. A const fixed
array cannot be written through its binding; copying it to var yields independent
writable inline storage while preserving readonly authority of embedded referents,
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
selected inline bytes through aliases, and the owner remains rooted/stable. A
readonly formal alone does not prove that: another argument may mutate its owner.
Otherwise copy into an independent snapshot at that argument's ordinary evaluation
point, before evaluating later arguments, or retain a rooted owned temporary for
an rvalue. Neither later-argument writes nor callee writes through another alias
may change the inline bytes observed by the const formal. A const formal does not
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

Recommended representation, compiler-private and subject to target layout:

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

### A10. Approved compatibility policy and numerical resource approval gate

Retain existing parser/semantic, inline-depth and expanded-shape guards. Count
fixed-array multiplicity in the existing 65536 expanded-layout bound and 128 depth
bound; never materialize an enormous vector just to discover it exceeds the bound.
Check total owned literal/template work as well, not just an individual literal.

These shape bounds alone do not bound native stack size: instruction homes,
parameters, construction staging and copied arrays can multiply storage. The owner
approves **65536 bytes per array-carrying function** after native probe run #3.
Use the selected target DataLayout and actual ABI to account for parameter/result
homes, local/instruction homes, copies, roots, staging, alignment and known ABI
scratch. Do not enforce a payload-only guess. The earlier 256 KiB estimate is not
adopted. The same target configuration and shared plan must serve analysis, LSP
and lowering. Array-free functions receive no incidental new restriction.

P23-1 prepares `NativeStoragePlanner`, a common checked inventory/aggregation
contract requiring an explicit target layout provider and all storage categories,
including those proved empty. It has no host-size fallback. P23-2 must populate
that inventory from real lowering decisions, root records and target calling
conventions, feed the same configuration to shared analysis/tooling, and prove
coverage before enabling LLVM ArrayType lowering. P23-1 deliberately blocks native
arrays while that integration is incomplete. Test-only layout providers are
arithmetic fixtures, not platform measurements or ABI proofs. Stop if the approved
budget is unworkable rather than changing it, heap-boxing value arrays or increasing
platform stack reserves. This does not authorize the receiver-chain hardening task
or promise arbitrary source recursion fits 1 MiB.

Runtime lengths/counts use checked exact arithmetic, INT64_MAX and target size_t
limits plus ordinary allocation failure. No arbitrary public capacity maximum is
approved here. Review may choose a smaller first-slice allocation policy; it must
be documented, tested and distinguished from string constant limits. Compiler
constant-array pool work needs a separate reviewed budget; the string pool's 64 MiB
limit is not an automatic array-storage policy.

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
| P23-0 | Completed owner/wrench approval of A1-A10 and, after native run #3, the initial 65536-byte policy subject to real target/ABI inventory proofs |
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
with PIC. No noinline/volatile attributes, suppression of optimization, heap
boxing or stack-reserve increase are used. Independent
modules naturally preserve the call boundary; Section 13.3 also tests inlining.

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

Two measured gaps matter before production:

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
PrologEpilogCodeInserterID. The exact pipeline integration and any later target
frame changes still need verification. Reconcile complete object extents, fixed
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
