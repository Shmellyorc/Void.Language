# Focused Linux validation

LLVM/Clang 19.1.7, Linux x86_64. Builds are `ci-official-debug` (Debug),
`ci-official-release` (Release) and `ci-official-san` (RelWithDebInfo with
`VOID_SANITIZERS=ON`). Final receipts contain full CMake configuration,
executable/source hashes, exact commands, stdout/stderr and JUnit outcomes.

| Final profile | Focused suites | Receipt |
|---|---:|---|
| Debug | 10/10 passed | [p5d5zp84](evidence/acceptance-debug-p5d5zp84/receipt.json) |
| Release | 10/10 passed | [q24zvwya](evidence/acceptance-release-q24zvwya/receipt.json) |
| Strict ASan/UBSan/LSan | 10/10 passed | [9khqxhvs](evidence/acceptance-san-9khqxhvs/receipt.json) |

Strict environment:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
No selected test is disabled or skipped. The ten suites are hard-stack execution,
native GC/allocation/reload host, expected invalidation, source negatives,
independent oracles, LSP, installation/relocation, cache, Stage 10 operators and
shared crash preservation. The registered default-stack execution suite is not
duplicated in this selection; the complete execution fixture set runs with hard
1 MiB child stacks instead. The linker driver itself uses its default stack.

Earlier [Debug](evidence/acceptance-debug-y03mjm6y/receipt.json),
[Release](evidence/acceptance-release-neywluje/receipt.json) and
[strict](evidence/acceptance-san-_yoi3hiz/receipt.json) selections also passed 30/30.
They precede the generic receiver-provenance repair and the final added owner/IR
negatives. They do not substitute for the final table above or explain away the
preserved adjacent regression.

## Behavior-to-fixture map

Paths in this table are under `tests/collections_stdlib/` unless otherwise noted.
Assertions execute actual compiler-owned source lowering and independent IR.

| Contract area | Concrete verification |
|---|---|
| Complete API/errors | `semantics.void`: all 24 functions; empty/singleton outcomes; error operation/kind/inherited fields; first matches; predicate ordering/counts; stable filtering/mapping/folding; extrema ties; dictionary insertion/update/delete/reinsert and independent outer storage. A separate source audit matches all 24 signatures exactly against DESIGN.md. |
| Element types/inference | `types.void`, `authority.void`: managed/nullable/interface refs, strings, bool, value structs, fixed children, nested lists/dictionaries, enums/results, bound methods/callable structs, named and captured function outputs, explicit/typed generic callbacks, mutable fold reference seed. `frontend.py` rejects destination-only map inference, bare generic comparators and untyped lambdas. |
| Readonly authority | `authority.void`, `types.void`: result outer storage stays writable; referenced input children remain readonly and share identity; independent/fresh mutable callback results retain their authority. `frontend.py` rejects laundering, nested mutation through results and key/value/dictionary-result mutation. Fresh empty dictionary/key/value outputs execute with the same valid profiles. |
| Numeric order/sum | `numeric.void`: all ten widths, signed/unsigned endpoints and wrapping, NaNs/infinities/zeros, stable NaN/zero equivalence, NaN ordinary equality false, first-equivalent bounds, same-type IEEE left-fold rounding and typed empty zero. `frontend.py` rejects bool/text numeric helpers and unsupported equality/key types. |
| Stable sorting | `semantics.void`, `numeric.void`, `oracles.py`: tagged stable ties in both directions, empty/singleton/two-item, extreme numbers, UTF-8/NUL text and custom records. Independent Python `sorted`/`bisect` expectations cover 48 numeric distributions, 240 bound questions and 64 tagged records. Always-true/flipping comparators must terminate and preserve the multiset; no sortedness claim is made for invalid comparators. |
| Mandatory custom equality mutation | `mutation.void` + `failures.py`: index_of/contains append and same-count remove/push effects, immediate return, subsequent new iterator and already-active outer iterator; genuine next-access/completion failures remain checked Runtime diagnostics. No final check was added to early-return linear queries. |
| Mandatory is_sorted mutation | `mutation.void`: first inversion with mutation returns immediately; continuing iteration and final completion detect invalidation. Previous/current values are captured normally. |
| Live iteration/reentrancy | `mutation.void`: any/all/find short circuits, captured-current find/filter/min/max candidates, later indexed replacement, caller variable rebinding, final-callback invalidation for count/filter/map/fold/min/max, independent nested calls and sort's completed private outer snapshot. |
| Ordered searches | `semantics.void`, `numeric.void`, `mutation.void`, `large.void`: boundaries/duplicates/first equivalence/direction, one captured structural version, append and same-count replacement, mutation in the first/second/third binary-search comparison (including both final equivalence probes), final lower/upper comparator mutation and logarithmic comparison bound. No full input copy/sortedness scan. Ordering-key mutation remains a documented precondition. |
| Large fixed values/hard stack | `large.void`: two 2 MiB fixed children through sort/reverse/filter/map/dictionary copy remain independent after input mutation; then 16,384 scalars plus 100 bound searches. `execution.py --small-stack` sets both soft and hard RLIMIT_STACK to 1,048,576 before compiler/runner execution. All seven fixtures, including the runnable example, execute in normal/O2 JIT and separately emitted, linked and executed AOT/O2. |
| GC/allocations/publication | `host.cpp`, `faults.void`, `publication.void`: existing managed probes force collection during semantics/type/numeric/large-fixed workloads and inspect precise roots/publication/frame balance. Eight allocating operations sweep budgets through failure and success in both modes; failure cleanup and retry remain asserted. Map publication stays private on partial failure while callback effects persist. Sort comparator failure preserves its completed append effect and prior destination. The forced-GC large entry intentionally omits the separate 16,384-scalar loop; the hard-stack execution suite runs it fully. |
| Private guards/IR | `frontend.py`: 39 source-located negatives, including private/internal operator and owner access, canonical hook type/arity and project lookalikes; an ordinary same-name project function still executes. `host.cpp::guard_ir` corrupts each guard operand independently and requires IR rejection. The final native host includes these checks; targeted `--ir-only` runs additionally passed all three profiles. |
| Approved generic access | `tests/stage10/operator_resolution.cpp`: 184 checks, inferred/explicit public-owner positives, ordinary transitive-import and private/internal/unrelated-owner negatives, forged declaration/argument/arity/receiver records, exact/nullability/ambiguity/readonly rules and generic method closures. `mutation.void`, LSP, cache and reload also exercise application equality selected inside collections. |
| Cache/reload/lifetime | `cache.py`, `host.cpp`, `reload.void`: automatic miss/hit/no-cache with fresh Runtime state; captured comparator and generic equality retain old/new generation behavior across compatible body changes; stored callback survives old generation retirement; incompatible initialization/layout/capture/instance sets reject; an in-flight algorithm keeps its old generation lease while subsequent calls use replacement code. |
| Tooling/install/examples | `lsp.py`: 40 compiler-owned completion/hover/signature/navigation/reference/protected-rename/unsaved/UTF-16 checks, including application equality navigation from specialized stdlib source. `install.py` installs compiler/tooling, relocates to a Unicode/special-character path, removes VOID_STDLIB_ROOT, checks/runs/builds/independently links the example in both modes on hard stacks and exercises the installed LSP. |
| Failure preservation | Existing `tests/text_encoding/crash_test.py`: thirteen intentional incidents in disposable storage; run/Popen crashes, sanitizer statuses/output, timeout, assertion, colliding imported names, unsaved editor bytes, launch snapshots surviving source changes and passing retry preserving history. Windows status handling is a simulation on this Linux host. Actual sprint originals remain under evidence/crashes, classified in FAILURES.md. |

## Adjacent regressions and acceptance limits

`regressions.py` runs freshly linked existing `void_generics_tests`,
`void_advanced_tests`, `void_collections_tests` and `void_reference_values_tests`
through the same recorder. All three profiles passed all four (12/12). Receipts record their exact executable hashes and
suite names; the final evidence index groups them by profile. Existing allocation,
GC, readonly, initialization, tooling, iteration and reload assertions remain.

The shared compiler/Runtime C++ is sanitizer-instrumented. Arbitrary generated JIT
machine code is not blanket ASan/UBSan instrumentation; its existing precise GC,
checked Runtime and compiler-generated ABI/root checks remain in force. AOT is
emitted separately and linked through each profile's native resource manifest.

Native Windows/macOS execution, full seven-job hosted CI, independent Golden
Wrench review and a release lock are **not** claimed. No H1–H7 repair is claimed.
No ptrace was used. The bounded [benchmark record](BENCHMARKS.md) states its own
timing/allocation limits and does not establish a performance guarantee.
