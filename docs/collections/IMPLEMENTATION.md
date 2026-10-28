# Collections engineering record

Baseline: `bb7244ea029623ddc83acfc163f888a261379051`.
Boss authorized the complete 24-function DESIGN contract and ROADMAP scope on
27 October 2026, including copy-returning transforms, NaNs-last numeric ordering
and both custom-equality/sortedness mutation refinements. The research-only and
pending-decision paragraphs in the original documents are historical.

**All 24 functions are implemented. Final local validation is recorded in
[VALIDATION.md](VALIDATION.md); independent review and platform locks remain open.**
Original approved documents and pre-existing documentation edits are preserved.
No Git staging, commits, branches, pushes, hosted CI or root README edits occurred.
Boss subsequently authorized ptrace if needed; none has been needed so far.

## Implementation

`stdlib/collections.void` contains all 24 public functions, the core.error-derived
managed error and error_kind. Algorithms are ordinary source over existing lists,
slices, live iteration and ordered dictionaries. Sorting snapshots the entire
outer input before comparisons, stably merges two reusable i64 index lists and
materializes correctly profiled output. Descending swaps comparator arguments.
Width and midpoint arithmetic use remaining-length bounds. Ordered searching
captures one structural version and checks it after each comparator, including
both final equivalence calls. The canonical-only `_collections_version` and
`_collections_check` adapters emit existing independently checked list_version /
list_check operations. No Runtime entry point, LLVM opcode or collection store
was added.

Linear functions keep captured current candidates, live next-element reads and
existing next-access/completion invalidation. There is no extra final check on
an early return. Dictionary builders seed authority from the first entry; fresh
empty literals receive their inferred return profile. Input-derived referenced
children remain readonly; independently held mutable references and freshly
constructed map results retain their legitimate authority.

## Shared compiler corrections and authorized language amendment

1. Exact callback result authority now excludes provenance from readonly formal
   leaves when the declared result is mutable. Previously this invented a mutable
   input requirement for a map callback constructing a fresh object. Both source
   analysis and independent IR use the shared helper; callback definitions still
   must prove their promised result authority.
2. Fresh empty collection return literals now receive the inferred return profile.
   Previously mixed empty/nonempty dictionary branches passed source checks but
   failed independent IR invariance. This does not convert existing collections,
   aliases or nonempty literals across invariant profiles.
3. Unsupported builtin list/dictionary/fixed-array equality now diagnoses during
   specialization instead of reaching a later IR comparison mismatch.
4. The intentional Stage 10 definition-import rule conflicted with the mandatory
   custom-equality API. Boss explicitly approved the narrow generic extension on
   28 October; see [the precise amendment](GENERIC_OPERATORS.md). Ordinary code
   retains the direct-import rule. No private/internal visibility bypass exists.

## Evidence preservation

Every collections compiler/runner invocation uses tests/text_encoding/crash.py
through tests/collections_stdlib/support.py. The shared recorder now also accepts
expected exit/output assertions, classifies ordinary unexpected exits separately
from abnormal exits, detects sanitizer output and saves execution context. Timeout
termination is recorded before cleanup. Source directories retain imported .void
companions recursively. Receipts include source/generator/executable/build/tool
hashes and exact stdout/stderr. Every incident has a unique directory under
`evidence/crashes/`; retries never replace originals. Expected diagnostics are
successful negative tests, not compiler crashes. Synthetic preservation tests use
disposable storage and verify thirteen intentional incidents, expected rejection,
source collisions, unsaved editor snapshots and a passing retry retaining history.

Original failed attempts include an enum equality spelling, imported-name
shadowing, the callback provenance defect, an invalid test using collection
identity equality, the empty-return IR discrepancy, an unsupported test escape,
the intentional generic operator visibility conflict and an i32/i64 test mismatch.
These were ordinary diagnostics/assertion failures, **not native compiler crashes**.
Exact original sources and receipts remain preserved separately from corrections.
H1–H7 and all earlier compiler/toolchain incidents remain separate and unresolved.

## Verification and review

The [validation record](VALIDATION.md) maps each required behavior to executable
fixtures and immutable profile receipts. It covers Debug/Release/strict sanitizer,
normal/O2 JIT and independently emitted/linked AOT, actual hard 1 MiB stacks,
independent oracles, forced GC and allocation publication/cleanup, automatic
cache miss/hit/bypass, retained callbacks and generic equality across compatible
reload, incompatible restart cases, active generation leases, compiler-owned
LSP and installed/relocated tools. Four existing adjacent source suites exercise
shared generics, advanced language, collections and reference values.

The provenance gate initially rejected generic methods on generic receiver types
in those existing suites. Its canonical identity now derives from the declared
method and separately verified nominal specialization. Construction variants
derive their suffix from formal construction parameters. Missing/mismatched or
foreign receiver proofs reject independently; ordinary generic methods and
nested generic closures execute in both JIT modes. The expanded Stage 10 suite
checks 184 operator resolution/call outcomes.

See [original failures and corrections](FAILURES.md), [measurements](BENCHMARKS.md),
[public inventory](API.md) and [Golden Wrench handoff](HANDOFF.md). No native
Windows/macOS execution, independent source review or hosted acceptance is claimed.
Generated JIT code is not blanket instrumented by the C++ sanitizer configuration;
strict builds instrument the compiler/Runtime and retain their existing JIT/ABI
checks. No production Runtime, LLVM backend or collection storage implementation
was modified. Performance boundaries for UTF-8 conversion and large fixed-value
copies are measured rather than concealed by a speculative Runtime redesign.
