# Collections — Golden Wrench source-review handoff

28 October 2026. Baseline `bb7244ea029623ddc83acfc163f888a261379051`.

**Complete local engineering and evidence delivery: 30/30 final focused suites
and 12/12 adjacent suites passed. Stop for independent Golden Wrench source
review; Boss acceptance remains open.**

Boss approved the entire DESIGN/ROADMAP sprint, copy-returning transformations,
NaNs-last natural order and both equality/sortedness mutation refinements. The
later [generic operator amendment](GENERIC_OPERATORS.md) is separately and
explicitly authorized. The original research/proposal bodies are preserved with
current authorization banners; earlier pending-decision language is historical.

## Delivered behavior

`stdlib/collections.void` implements reversed, sorted, is_sorted, numeric_less,
text_less, lower_bound, upper_bound, binary_search, find, find_index, index_of,
contains, any, all, count_if, filter, map, fold, min, max, sum, keys, values and
copy_dictionary, plus managed core.error-derived error/error_kind. All public
signatures exactly match the [approved design](DESIGN.md). The [API inventory](API.md)
documents empty/error/authority/mutation boundaries.

Stable sorting completes its private outer snapshot before the first callback,
then iteratively merges reusable i64 index buffers and materializes a fresh list.
Descending swaps comparator arguments and preserves ties. Ordered queries capture
one structural version and check it immediately after every comparator, including
final equivalence probes. Live queries retain ordinary captured-current and
next-access/completion rules, including successful early return after mutation.
Dictionary builders preserve insertion order and existing key/value authority.

No Runtime container, native/public collection API, LLVM opcode, ownership/GC
subsystem, comparator hierarchy or generic-overload framework was added. Native
backend and Runtime production files are unchanged.

## Shared compiler work requiring source review

| Files | Review focus |
|---|---|
| `src/frontend/compiler.cpp`, `src/frontend/capabilities.inc` | Only canonical /@stdlib/collections.void receives the two unqualified private guard adapters. Typed argument binding/capture emits existing list_version/list_check operations. Fresh empty return literals receive inferred return authority; existing/nonempty collections remain invariant. Unsupported builtin collection equality diagnoses during specialization. |
| `include/void/reference_capabilities.hpp`, `src/ir/reference_capabilities.inc` | Shared callable-result provenance excludes readonly formal leaves as origins of a promised mutable result. Source and IR still reject input-alias laundering; independent/fresh mutable outputs remain legal. IR capability diagnostics show expected/actual profiles. |
| `include/void/ir.hpp`, `src/frontend/generics.inc`, `src/ir/interfaces.inc`, `src/ir/module.cpp`, frontend lowering | Minimal typed generic declaration/instance records prove canonical identity, scope, arity, valid arguments and exact concrete function/nominal owner. Generic methods bind to the separately verified receiver; construction suffixes derive from formals; nested closures prove lexical creation. The existing exact operator selector adds only public exact substituted-owner access. Ordinary imports, nonpublic access, matching, ambiguity and readonly rules remain enforced independently. |
| `tests/stage10/operator_resolution.cpp` | Old generic-import negative is amended under explicit approval. Ordinary restrictions remain tested, alongside inferred/explicit, generic receiver/closure and forged-proof cases. |
| `tests/text_encoding/crash.py`, `crash_test.py` | Existing recorder now preserves unmet execution expectations, sanitizer findings, timeouts and immutable launch source bytes. No second recording framework. The synthetic control verifies thirteen deliberate incidents separately from actual sprint failures. |

The adjacent generic suite exposed a real receiver-identity regression in the
first provenance implementation. It is repaired through verified nominal binding,
with malformed receiver-proof negatives. No import-check bypass or permission bit
was substituted. All original attempts remain in the [failure record](FAILURES.md).

## Execution evidence and examples

The [validation map](VALIDATION.md) links final per-profile JUnit/config/hash
receipts and maps each acceptance area to executable fixtures. Debug, Release and
strict ASan/UBSan/LSan each passed 10/10 final focused suites (30/30).
All four existing adjacent generic/advanced/collections/reference-value suites
passed in each profile (12/12), including strict sanitizer. Earlier 30/30 focused
passes remain separate from final repaired-binary evidence.

Execution includes normal/O2 JIT and independently emitted/linked/executed AOT/O2,
actual hard 1 MiB compiler/runner stacks, 2 MiB fixed children and 16,384 scalars,
independent stable-sort/bisect oracles, 39 source negatives, 184 Stage 10 operator
checks, 40 LSP checks, forced precise GC, eight allocation sweeps, private-result
publication, compatible captured-callback/equality reload, active leases,
incompatible restart cases, cache miss/hit/bypass and installed/relocated tools.

Run [the example](../../examples/collections_stdlib/main.void) with
`build/ci-official-release/voidc run examples/collections_stdlib/main.void --no-cache`.
It demonstrates numeric/game-record/text sorting, independent dictionary copying,
readonly shared references, a typed generic map callback and explicit fixed slices.
It is executed in all four execution modes by the hard-stack suite.

The [33-workload benchmark report](BENCHMARKS.md) preserves setup/compile/execution
separation, samples, callback counts, checksums, source and host hashes. Large
fixed-value copying and per-comparison owned UTF-8 conversions remain measured
costs. Complete allocator-event counts and cross-platform timing guarantees are
not claimed. Measurements predate the bound-method proof repair; their recorded
collections algorithm/source is unchanged by that repair.

## Inventory, preservation and stopping boundary

[CHANGED_FILES.json](evidence/CHANGED_FILES.json) is the exhaustive status/path/size/
SHA-256 inventory of changed and untracked files, including all evidence; its
detached hash is [CHANGED_FILES.sha256](evidence/CHANGED_FILES.sha256). The manifest
explicitly excludes itself and its detached digest to avoid recursive hashes.
[FINAL_VALIDATION.json](evidence/FINAL_VALIDATION.json) groups final profile and
adjacent receipts, executable/config hashes and archive byte/hash audit results.

Seventeen original unexpected failures remain in unique immutable evidence
directories, with exact `_crash.void` fixtures/imports and command/output/provenance
receipts. They were ordinary diagnostics/IR/test-assertion failures, not native
compiler crashes. A passing retry does not erase an original or establish its
cause; FAILURES.md records the separate diagnosis and correction. H1–H7 remain
separate and unresolved. Ptrace was authorized by Boss but not used.

Native Windows/macOS execution, hosted seven-job CI, independent Golden Wrench
review and release acceptance remain open. Linux sanitizer builds instrument
compiler/Runtime C++; generated JIT code is not blanket sanitizer instrumentation.
No currently reproduced local defect is being accepted by suppressing a failure.

No Git staging, commits, branches, pushes, remote changes, hosted CI, unrelated
module implementation or protected root README edit occurred. Pre-existing
docs/README.md and research inputs were retained and incorporated in their
authorized scope. Local engineering stops here after final evidence completion
for independent Golden Wrench source review; Boss retains Git/CI/release control.
