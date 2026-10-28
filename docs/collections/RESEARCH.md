> Current authorization: Boss approved the complete contract on 27 October 2026.
> Copy-returning transforms, NaNs-last ordering and both mutation refinements are
> settled. Original proposal wording below is historical. See
> [engineering status](IMPLEMENTATION.md) and the separately approved
> [generic operator amendment](GENERIC_OPERATORS.md). Local engineering is complete; independent review remains open. See [handoff](HANDOFF.md).

# Collections: source audit, dependencies and architecture

27 October 2026. Baseline **`bb7244ea029623ddc83acfc163f888a261379051`**.
**Research only; no production implementation or acceptance claim.**
[Proposed 24-function contract](DESIGN.md) · [engineering roadmap](ROADMAP.md)

## Recommendation and approval boundary

Implement eager algorithms in ordinary VOID over the existing native collections.
Use stable copy-returning sorting/reversal, ordinary live iteration for linear
algorithms, existing ordered dictionaries and exact synchronous callbacks. Two
canonical-private adapters to existing list_version/list_check IR preserve
logarithmic ordered-search probes without cloning the whole input.

No new Runtime entry point, LLVM operation, collector, container, reflection or
query framework is needed for that proposed surface. Readonly inference must be
preserved, not bypassed by a collection-specific semantic engine.

Two genuine owner decisions remain: accepting copy-returning transforms rather
than the historically favored in-place sort convenience, and NaNs-last natural
numeric ordering. DESIGN makes both explicit. The proposed inventory is a complete
bounded algorithm module, not a claim to implement every collection facility or
silently defer a previously approved function. Stop for Golden Wrench review and
Boss's contract decision before implementation.

## Language Bible and current contracts

Read the actual [Language Bible](../LANGUAGE_BIBLE.md), especially sections 21,
26 and 32–35, with [ARRAY_OPERATIONS](../ARRAY_OPERATIONS.md),
[FIXED_ARRAYS](../FIXED_ARRAYS.md), [GROWABLE_ARRAYS](../GROWABLE_ARRAYS.md),
[DICTIONARIES](../DICTIONARIES.md), [GENERICS](../GENERICS.md),
[ADVANCED_LANGUAGE](../ADVANCED_LANGUAGE.md) and
[Stage 8 recoverable collection contract](../STAGE8_TEXT_COLLECTIONS.md).
The Bible's version/invalidation and independent-value/shared-identity directions
remain authoritative. Dated statements deferring get/pop/slices are superseded by
the implemented contracts/source; the reverse-remove-current exception and custom
iterator protocol remain unimplemented proposals.

The [stdlib handoff section 11](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md)
favors sorting without comparer classes, a Boolean less callback and useful
collection algorithms. Its method spelling and no-comparator numeric examples do
not establish working overloads or source extension methods. The current
[backlog](../VOID_STDLIB_BACKLOG_2026-10-27.md) requests bounded complete modules,
not List/Stack/Queue replacements. The baseline already contains os and other
source modules; historical index/backlog status paragraphs do not override source.

## Verified capability ownership matrix

E = existing; I = included in the proposed initial module; O = optional/future;
B = explicit Boss scope decision. Included is a proposal, not implementation.

| Capability | Owner/status | Source-grounded boundary |
|---|---|---|
| Fixed/growable storage, defaults and identity | E: language/Runtime | Fixed assignment/arguments/returns copy; lists share identity. No replacement container. |
| Count/index/get/push/pop/insert/remove_at/clear | E: compiler-owned APIs | get/pop use existing result/string errors; invalid direct indexing remains checked failure. No collections aliases. |
| Full list copy and slicing | E: language | Inclusive/exclusive, both endpoints, negative offsets, fresh growable result; fixed callers explicitly slice. No new list-copy API. |
| Forward/reverse iteration | E: language | Original i64 indexes; structural invalidation on next access/completion, early break/return allowed. Reverse iteration allocates no reversed list. |
| Dictionary lookup/upsert/contains/remove/clear/order | E: language/Runtime | Existing supported key families, insertion order, equality/hash exclusions and shared identity. |
| Predicate queries/find/index/equality membership | I: collections | Existing source comparisons and live loops; no universal structural equality. |
| Eager filter/map and seeded fold | I: collections | Exact callbacks and authority, no lazy pipeline or implicit result propagation. |
| Stable sorted copy and reversed copy | I: collections | Existing private slice/value-copy machinery; no input mutation or alias-preserving commit. |
| Ordering helpers | I: collections | Boolean numeric and exact UTF-8 byte order; no locale/case normalization. Numeric NaN policy needs approval. |
| Sortedness and lower/upper/binary search | I: collections | Required comparator, explicit duplicate boundaries; two private existing-IR guards for ordered queries. |
| Collection min/max/sum | I: collections | Reduction semantics, not aliases of scalar math.min/max; fold handles custom aggregates. |
| Dictionary keys/values/outer copy | I: collections | Source builders preserve insertion order, authority and existing key policy. |
| Alias-preserving in-place sort/reverse/remove-if | B | Historical preference; stronger failure/mutation guarantees need an explicit scope/architecture choice, not a hidden partial implementation. |
| Random choose/shuffle | E: random | Reuse [random.void](../../stdlib/random.void); preserve its distinct RNG/state/error contract. |
| Text search/split/join, scalar math, formatting | E: text/math/language | Keep their current owners; text_less is an ordering callback needed by collection sorting, not a second text engine. |
| General deep clone, hashing/equality expansion | O | Object identity, resources and dictionary-key restrictions need separate contracts. |
| sort_by/key caching, partition/zip/group/joins/sets | O | Useful possible additions, not required for the chosen complete initial inventory. No LINQ clone. |
| Average/product/reduce without initial | O | Fold already supplies reduction; overflow/precision/empty policy must not be guessed for new aliases. |
| User iterators/new queues/heaps/parallel algorithms | O | Separate design; no new language primitive, scheduler or concurrent sharing. |

## Source evidence

| Area | Actual source and implication |
|---|---|
| List layout/version | [runtime.hpp](../../include/void/runtime.hpp), ListHeader; [array.cpp](../../src/runtime/array.cpp), lines 137–165. Indexed replacement does not increment structural version; insertion/growth does. Count equality cannot detect remove/push restoring count. |
| Iterator behavior | [compiler.cpp](../../src/frontend/compiler.cpp), lines 4787–4820. Collection/version captured once; check precedes each next access and completion; return/break can bypass the next check. |
| Slices/publication | array.cpp around line 272; [buffer.cpp](../../src/runtime/buffer.cpp), around line 261. Rooted private backing is prepared and revalidated before publication. |
| Fixed child copies | [backend.cpp](../../src/backend/llvm/backend.cpp), around lines 678, 896–903 and 1014–1039. Reads/arguments/writes may allocate independent fixed children. A multi-assignment swap is not atomic on allocation failure. |
| Existing guard IR | [module.cpp](../../src/ir/module.cpp), lines 946–952 validates list_check on growable owner/u64 token and list_version returning u64; backend.cpp lines 986–997 already lowers/binds both through existing Runtime queries. |
| Guard adapter precedent | compiler.cpp around lines 3176 and 4492 has compiler-generated collection-display adapters using the same IR. Reuse just version/check mapping for canonical /@stdlib/collections.void, not the entire formatting adapter interface. |
| Generics | [generic_inference.inc](../../src/frontend/generic_inference.inc), lines 31–78. `[t]` requires a growable actual; there are no numeric extent generics or element-type projection for arbitrary containers. |
| Readonly flows | [capabilities.inc](../../src/frontend/capabilities.inc), lines 238–246, 271–273 and 324–330. Exact callback result authority is enforced; slices preserve child facts; pushes cannot thaw readonly references. |
| Existing source precedent | random.void lines 521–560 uses growable-only generic functions, explicit fixed caller slices and local const-projected copies. [random design](../random/DESIGN.md) explains why forwarding that copy through mutable generic shuffle can reject. |
| Dictionary engine | [dictionary.cpp](../../src/runtime/dictionary.cpp); [DICTIONARIES](../DICTIONARIES.md). Source algorithms must not alter key admissibility or reimplement hashing/order storage. |
| Numeric behavior | [NUMERIC_TYPES](../NUMERIC_TYPES.md), lines 67–85: integer wrapping, IEEE float behavior, NaN comparisons and signed-zero equality. Collection ordering does not change these operators. |

## Bounded source-check findings

The permanent [probe receipt](evidence/FRONTEND_PROBES.json) contains **22 exact
source inputs**, original commands/diagnostics/statuses, hashes and the identified
existing Release compiler SHA-256. These are source checks only. No fresh build,
JIT/O2 execution, independent IR validation or AOT execution was performed.
The existing binary is identified, not asserted rebuilt from this commit; actual
source audit independently targets the baseline. Engineering must rerun the
contract against freshly built profiles.

Twenty-one original status expectations matched. One exploratory expectation
was disproved: a bare generic comparator reference did not infer its concrete
arguments. Its expected-success/actual-error record is retained unchanged; the
proposal now requires `numeric_less<i32>`, not bare numeric_less. This is a
research finding, not a repaired production failure.

Confirmed practical consequences:

- `[t]` formals reject fixed arrays. Whole-container generic readers can specialize
  on both kinds but cannot derive callback element types consistently; explicit
  type arguments/typed callback variables become awkward. Follow existing random
  API precedent rather than invent overloads, associated types or extent generics.
- Typed predicate/comparator lambdas work; untyped `func(a,b)` does not. Generic
  function values need explicit concrete arguments. Map's u needs explicit type
  arguments or an already typed callback; a destination annotation is insufficient.
- Empty slices carry readonly child profiles. `var output=items[0..<0]` can accept
  const-input elements and preserve their authority. Starting from mutable
  `[t]=[]` can instead reject managed reference inputs.
- Source find returning result<t,error-base> preserves readonly managed payloads
  in Ok. Dictionary copy/keys/values can seed profiles from the first entry and
  return typed empty storage for empty input; read positives and mutation
  negatives pass source checking. No native empty-profile factory is necessary.
- Map accepts nullable, fixed, result and function-valued outputs where ordinary
  callable authority is valid. A callback returning a readonly input alias cannot
  satisfy a mutable reference result signature. Do not weaken that diagnostic or
  invent `-> const t` syntax to hide the limitation.
- Source numeric_less with a typed numeric-zero anchor and NaN branches accepts
  all ten numeric types and rejects bool/string. Explicit comparator type spelling
  and lambda parameter annotations must be shown in examples/LSP expectations.
- `reverse` is a reserved keyword in compiler.cpp lines 157–158. The historical
  illustrative parameter spelling cannot be copied into a source declaration;
  the proposed legal Boolean parameter is `descending`. No parser exception or
  language syntax change is needed.

Source acceptance does not prove source-to-IR authority agreement. First engineering
gates must lower/run these cases and repair any genuine shared inference/IR issue
through existing infrastructure; do not special-case collections to thaw values.

## Mature designs and algorithm choice

Go's sort contract uses Boolean Less with coherent strict weak order and warns
about raw floating comparison with NaN. Preserve Boss's Boolean direction rather
than adding a comparer class or three-way integer convention. Adopt stable
equivalent ordering and explicitly define numeric NaN placement.
[Go sort contract](https://pkg.go.dev/sort).

Stable merging is established; Java documents an iterative stable implementation.
Recommend a simple bottom-up merge rather than recursive quicksort or a TimSort
framework. An index permutation avoids repeatedly storing deep T payloads during
merge; comparison itself may still copy T under VOID semantics. It is an
implementation recommendation to measure, not a claim that all payload copying
becomes O(n) or that native sorting is required.
[Java Arrays sorting](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/Arrays.html).

Python's bisection API separates before/after equivalent insertion points and
documents that insertion still costs linear time. Use those boundary meanings;
make VOID binary_search return the first equivalent index intentionally, rather
than leaving duplicate selection unspecified. Ordered queries perform no hidden
O(n) copy or sortedness pass.
[Python bisect](https://docs.python.org/3/library/bisect.html).

## Architecture tradeoffs and real risks

**Linear algorithms:** ordinary live loops preserve the Bible's existing behavior
without an O(n) snapshot before a short-circuit query. Structural callback effects
fail on the next access/completion; nonstructural replacements are observable
later; early returns can succeed after mutation. This difference must appear in
docs and tests, not be mistaken for universal callback mutation rejection.

**Ordered algorithms:** source-only count comparisons are insufficient. Expose
two strictly private canonical-module hooks mapping to existing list_version and
list_check. Check after every comparator, including both final equivalence calls.
No new opcode, native symbol, list-layout field or content revision. Existing IR
operations remain legal for ordinary for/display; do not restrict their general
IR use to collections. Nonstructural ordering-key changes are a precondition
violation, not a mutation the existing structural version can detect.

**In-place alternatives:** rebinding a local sorted list does not update aliases.
Indexed copy-back/swap can fail between writes for fixed children, leaving valid
elements but not the original permutation. Existing random.shuffle has a related
documented nonrollback boundary; this audit does not modify or claim repair of it.
A backing read lease does not freeze the list header against growth. Strict
mutation rejection plus atomic replacement would need content-change tracking
and prepared-buffer publication, auditing all native/host mutation paths. Do not
introduce that machinery speculatively merely to claim an in-place convenience.
An explicitly weak/nontransactional source mutator is also possible using current
writes, as shuffle does. It needs no such new tracking, but must disclose that
failure between assignments can lose the original permutation. The additional
machinery is required only for the stronger guarantees, not every in-place API.

**Performance:** merging references is cheap relative to large independent fixed
payload reads/copies. Conservative roots and retained capacity remain existing
costs. Reuse two index buffers, avoid per-pass temporary lists and calculate merge
width/midpoints without overflow. text_less can allocate two UTF-8 lists for every
comparison; measure it separately. A performance problem does not authorize new
raw string borrows or weakening value copies. No timing improvement is claimed
for an unimplemented module.

**Lifetimes/tooling:** callbacks and captured cells use existing GC/code leases;
do not store a Runtime global registry or native comparator pointer. Compatible
reload/cache follow current contracts; [automatic cache policy](../compiler-health/stabilization/CACHE.md)
is authoritative. Source docs and compiler-owned tooling supply imports,
signatures and navigation for all editors. Generic map limitations must diagnose
honestly. No callback executes during analysis or completion.

**Verification limits:** no algorithms have been executed by this research.
Normal/O2 JIT, independently linked AOT, readonly IR, allocation failures, GC,
cache/reload, real 1 MiB stacks and actual Windows/macOS execution remain required
engineering evidence. The baseline's historical H1–H7 incidents remain open.
No production code, unrelated module, root README, Git state or hosted CI changed.

## Research validation and handoff

Documentation checks pass for **179 local links**, the **24-function** inventory,
legal source identifiers, Markdown fences/whitespace and all **22 archived source
hashes**. Original source-check outcomes and the one disproved expectation remain
unchanged. Scope checks permit only these three research documents, their single
JSON evidence receipt and the internal docs index; HEAD remains the baseline.

Independent frontend, Runtime and algorithm review found and corrected the
reserved parameter spelling and made current-candidate capture, early-return
invalidation and weak-versus-strong in-place alternatives explicit. These reviews
are research checks, not Golden Wrench approval or module execution acceptance.
The two owner decisions in DESIGN remain open. Return this complete proposal for
Golden Wrench architecture review before Keyboard engineering.
