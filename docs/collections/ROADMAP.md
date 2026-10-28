> Current authorization: Boss approved the complete contract on 27 October 2026.
> Copy-returning transforms, NaNs-last ordering and both mutation refinements are
> settled. Original proposal wording below is historical. See
> [engineering status](IMPLEMENTATION.md) and the separately approved
> [generic operator amendment](GENERIC_OPERATORS.md). Local engineering is complete; independent review remains open. See [handoff](HANDOFF.md).

# Collections: one complete implementation sprint

27 October 2026. Baseline **`bb7244ea029623ddc83acfc163f888a261379051`**.
**Conditional engineering plan; research does not authorize implementation.**
[Exact proposed API](DESIGN.md) · [source/evidence and capability ownership](RESEARCH.md)

## Scope and sequence

Deliver the complete **24-function** contract, error type, private existing-IR
adapters, examples, documentation and focused verification as one sprint after
Golden Wrench review and Boss approval. Do not release a numeric-only sort demo,
omit readonly/reference cases or call a subset complete. No new Runtime/LLVM
operation, native sort, storage engine or collection framework is proposed.

1. **Resolve the actual owner decisions.** Confirm the proposed copy-returning
   transform scope versus historically favored in-place sort/reverse/remove-if,
   and NaNs-last numeric_less. If Boss requires alias-preserving mutators, revise
   their failure/mutation architecture before engineering instead of silently
   omitting them or pretending local rebinding updates aliases. Accept the final
   inventory as a whole; other ordinary language restrictions are not new choices.
2. **Establish end-to-end authority support first.** Recreate the research probes
   using freshly built compiler profiles and lower/run list/filter/find/map and
   dictionary builders. Source check alone did not prove independent IR, JIT or
   AOT acceptance. Preserve exact callable-result/const profiles. Fix only genuine
   shared inference/lowering defects through existing code, with focused regression
   tests; do not add collections-specific thawing, return qualifiers, overloads or
   a second checker. Verify explicit map type arguments and typed callbacks.
3. **Add the ordinary source facade/error/builders.** Create stdlib/collections.void,
   core.error-derived not_found/empty_collection, plain documentation and private
   correctly profiled result builders. Use empty slices for same-element lists,
   first-entry profile seeding for dictionary copy/keys/values and typed empty
   results when input is empty. Never require default construction of arbitrary t.
   Add reversed by existing slice semantics; full list copy remains builtin syntax.
4. **Implement live linear algorithms.** Find/find_index/index_of/contains,
   any/all/count_if, filter/map/fold, min/max/sum and is_sorted use ordinary for
   loops, source operators and synchronous callbacks. Preserve argument order,
   exact visit order/count, short-circuit behavior, next-access/completion checks,
   later nonstructural replacements, referenced identity and callback effects.
   No hidden input clone before an early query. Sum uses typed zero and exact
   existing arithmetic; callback min/max preserves the first equivalent item.
5. **Implement the two ordering helpers.** numeric_less uses a source numeric
   context anchor, NaN handling and primitive comparisons without subtraction;
   text_less uses exact unsigned UTF-8 lexicographic order. Keep locale/casing/
   normalization and string '<' syntax out. Record its actual conversion/allocation
   cost; do not add an unreviewed raw-string API or modify unrelated text/math.
6. **Implement stable copied sorting.** Complete one private source slice before
   the first comparison. Use iterative bottom-up merge of i64 indices and two
   reusable buffers, then materialize the output with correct child authority.
   Compare the direct-value alternative only with representative measurements;
   preserve stable ties and descending direction by swapping comparator arguments.
   Bound merge loops and arithmetic without overflow. No recursive algorithm,
   per-pass list allocation or payload-sized native stack buffers. Fixed-value
   copies remain governed by current lowering, including their allocation cost.
7. **Expose only the needed existing version operations privately.** Canonical
   /@stdlib/collections.void selection for `_collections_version` and
   `_collections_check`, with exact growable/u64/none signatures, ordinary
   argument capture and source spans. Reuse IR list_version/list_check, existing
   LLVM lowering, Runtime helpers and independent operand validation. Do not add
   Runtime symbols, public list.version, dictionary guards, content revision
   fields or a commit subsystem. Existing loop/display IR use stays legal.
   Do not expose unrelated `$collection` display operations. Preserve existing
   worker-effect rules and observable guard placement after callback calls.
8. **Implement ordered searches.** Capture selected input/version once; lower,
   upper and binary search use safe low+(high-low)/2, required comparator/direction
   and a check immediately after every comparator. Include every final equivalence
   comparison before return; do not recapture a newer version between substeps.
   No clone/sortedness prepass, implicit equality, native comparator trampoline or
   new recovery mechanism. Existing Runtime checked failure reports modification.
9. **Complete source/tooling/install documentation.** All 24 functions, error
   fields, parameter and return docs; examples for numbers, strings, game records,
   explicit fixed slices, dictionary copies, readonly references, map's explicit
   generic spelling and typed lambdas. Import/member completion, hover, signature
   help, original generic navigation/references and protected stdlib rename use
   compiler-owned metadata. No editor-specific catalog or host execution during
   analysis. Confirm compiler/tooling install components and relocation.
10. **Run the focused acceptance matrix and bounded measurements.** Linux Debug,
    Release and strict sanitizer profiles; normal/O2 JIT, independently linked and
    executed AOT, allocation/GC/reload/cache, actual hard 1 MiB stacks and LSP.
    Record native Windows/macOS results or their limits honestly. No routine
    seven-job hosted CI. Preserve failures and complete the API/evidence handoff
    for Golden Wrench independent source review; Boss controls Git and CI.

These are an implementation order, not separate partial feature deliveries or a
new stage framework. The sprint is not complete until every approved signature
and behavior has evidence or an explicitly recorded unresolved acceptance gap.

## Focused acceptance matrix

| Area | Required cases and assertions |
|---|---|
| API/inference | Inventory all 24 functions, error fields and ownership boundaries. Mutable/const growable input, explicit empty lists, typed/named callbacks, bound methods/callable structs, explicit generic function references. Map explicit T/U or typed variable succeeds; bare generic comparator, untyped lambda and destination-only U inference diagnose honestly. Ordering flag is descending, since reverse is reserved. No overload/default-comparer magic. |
| Element/type matrix | All ten numbers, bool where relevant, strings/NUL/Unicode, managed/nullable/interface refs, value structs, fixed children, nested lists/dictionaries, enums/results and function values. Unsupported source equality/numeric types reject even on empty runtime input. Dictionary key exclusions remain unchanged. |
| Fixed/value/reference | Fixed callers slice explicitly; direct fixed argument to [t] rejects. Large fixed children remain independent after copy/filter/map/sorted/reversed/dictionary copy; nested managed/list identities stay shared. No implicit deep clone/resource duplication. Check aliasing and caller rebinding during callbacks. |
| Readonly/IR | Private result outer storage is writable where permitted; input-derived referenced children stay readonly through list/dictionary results, Ok extraction, generics and aliases. Fresh/independently mutable map outputs retain their own legitimate authority. Reject laundering an input alias through callback return. Independently validate emitted IR, not just source success. |
| Linear queries | Empty any/all/count is false/true/0; find/find_index/index_of not_found; first duplicate/index; live left-to-right ordering and exact short-circuit callback counts. Custom equality follows source effects, NaN equality remains false. No -1 sentinel. |
| Live mutation/reentrancy | Capture the current candidate before callback: replacing its input slot must not make find/filter/extrema reread a different candidate afterwards. Later indexed replacement is observed when visited. Push/remove/insert/nonempty clear, including remove+push restoring count, fail at next access/completion. Test any/find early success and all early failure after structural mutation: no future access means no mandatory failure. Filter/map/count/min/max/fold completing traversal detect final-callback mutation. Nested calls obey their own captures/guards. No rollback of captured side effects. |
| Filter/map/fold | Empty/none/all selections; stable order and one callback per visited item; no extra callback for type/empty initialization. Fold left associativity, explicit seed on empty, reference accumulator identity/mutation and value initial independence. Map U includes scalar, value/fixed, nullable, result and function outputs; callback result is data, not implicit propagation. |
| Aggregation | Same-type sums for each numeric width, wrapping integer extremes, float NaN/infinities/signed zero and left-fold rounding. No widened accumulator/reassociation. Min/max empty errors, first tie, custom record comparison, numeric_less NaN policy and signed-zero first tie. |
| Sorting | Stable tagged duplicates, empty/singleton/two-item, ascending/descending/equal/reversed input, randomized/adversarial distributions and custom comparator; reverse preserves tie order. NaN/infinity/zero cases, extreme signed/unsigned values. Comparator/callback failure leaves input outer sequence untouched except user effects; no result publication. Comparator mutations of original outer list do not redirect private snapshot. |
| Invalid comparators | Always true, inconsistent/flipping answers, state changes and duplicate-equivalence violations must not cause native unsafe access or unbounded internal loops. Do not assert correct sortedness for invalid ordering. A callback that never returns is not an algorithm termination test. |
| Ordered search | Empty/before/between/after endpoints, all duplicates, first equivalent match and lower/upper positions, ascending/reverse custom order. Numeric_less equivalence for NaNs/zeros differs from ==. Verify logarithmic comparison counts, no O(n) snapshot/sortedness scan, safe midpoint. Guard after every comparator/equivalence call catches append, remove+push and nested mutation; callback checked failure propagates. Nonstructural ordering-key mutation remains a documented precondition violation, with checked/bounded memory behavior. |
| Dictionary builders | Empty and nonempty; insertion order, update/delete/reinsert policy; readonly managed keys and values, nested fixed copies and shared reference leaves. copy_dictionary identity differs; assignment still aliases. Values/keys lists preserve order and authority without a native factory or new hashing. |
| GC/allocation/cleanup | Force collections during comparisons, callbacks, slice/builder growth, result/error construction and fixed-child copies. Reuse tests/managed probes/budgets; sweep operations requiring allocation, inspect precise roots/publication, no UAF/leaks and valid retry. Partially built result stays private; prior callback/accumulator side effects remain. Native resources are not deinitialized by collection algorithms. |
| Existing IR guards | Malformed hook arity/types/u64, attempts to access stdlib-private hooks from lookalike project modules, private-qualified misuse and forged IR operands reject. Ordinary same-name project functions retain their own semantics. Existing for/formatting version checks still pass. No extra mutation authority or worker admission. Test code generation retains checks across normal/O2 after callbacks. |
| Cache/reload | Miss/hit/--no-cache under current automatic policy; no serialized lists/callbacks/heap state. Retain typed callback/global state across compatible body reload, old generation active during nested calls, reject incompatible captures/signatures/types/instance sets under current policy. Distinct applications remain independent. |
| Tooling/install | Compiler-owned use/member/error completion, hover and contextual parameter help, generic declarations/references, aliases, incomplete/unsaved input, UTF-16 positions, stale rejection and protected stdlib rename. Installed/relocated compiler and tooling source discovery; compile/build/run examples through installed native resources. No separate editor semantics. |
| Modes/platform/stack | Linux focused Debug/Release/ASan/UBSan/leak; true normal/O2 JIT and independently linked AOT execution. Actual hard 1 MiB child stacks for compilation and algorithms, including many elements and >1 MiB fixed payloads. No enlarged stack or arbitrary payload cap. Native Windows/macOS focused execution before claiming their verification; source-only portability is not a native pass. |

Use existing tests/collections, tests/arrays, tests/dictionaries, tests/generics,
tests/advanced, tests/stage11 and stdlib module LSP/CLI harnesses. Add a bounded
module-focused suite instead of another validation framework. Existing tests
must retain their meaning; do not rewrite iteration or random behavior to fit a
new expectation. Use independent output/reference oracles, not implementation-
mirroring tests or only “the list looks sorted.”

## Representative measurements

Separate source analysis/lowering, JIT compilation, cache loading and repeated
invocation from timed algorithm work. Use deterministic generated inputs and
record compiler/hash/profile, sizes, callback counts, allocations/peak live memory,
elapsed samples and output checksums. No performance threshold invented before a
baseline; no benchmark framework or production instrumentation is required.

- Stable sort: scalar sizes 0/1/32/1,024/16,384, tagged duplicate records,
  referenced game entities and bounded large fixed-value elements; random,
  sorted and reverse distributions. Compare index merge with a simple source
  reference where meaningful. Count comparisons and fixed-copy/allocation costs.
- Repeated lower/upper/binary queries on a prebuilt sorted list: comparison count
  against size growth and linear scan baseline. Confirm the chosen guards do not
  introduce linear copies; include expensive comparator/large element cases.
- Filter/map/fold: compare with equivalent ordinary source loops and builtin
  slice copies; measure managed/nullable/value elements, small/large lists and
  callback dispatch/GC pressure. Do not conflate fixed-to-list conversion with
  algorithm body time.
- text_less: ASCII, multibyte, long common prefixes and NUL; report owned UTF-8
  conversion allocations separately. If cost is material, bring evidence for a
  separately reviewed narrow optimization; do not add raw borrows speculatively.
- Dictionary copy/keys/values: insertion order, size and nested fixed/reference
  payload costs. Distinguish retained capacity/conservative roots from leaks.

## Evidence and stopping rule

All compiler/runner subprocesses must use the existing crash-preservation helper
from tests/text_encoding/crash.py or its current shared integration. Every abnormal
exit preserves exact `_crash.void` sources/imports, generator/provenance hashes,
command/profile/stdout/stderr before temp cleanup. A passing retry does not prove
a repair. Keep historical H1–H7 separate; stop before ptrace unless Boss explicitly
authorizes that investigation.

Final handoff: complete API/behavior matrix, changed-file inventory, original
failures and corrections, per-mode/profile evidence, native limits, benchmark
receipts and genuine unresolved problems. No root README edits, unrelated modules,
Git staging/commits/branches/pushes or hosted CI. **Stop for independent Golden
Wrench architecture/source review and Boss authorization/acceptance.**
