# G83 — Collection Equality & Hashing

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Implementation candidate on accepted G82 baseline
`ee4cade9a3752562364b1613621fd3ade3bdb3c0`, Linux x86-64, 20 November 2026.
No staging, commit, push, installation, publication or hosted acceptance is claimed.
Golden Wrench's independent review found one P1 helper-analysis stack overflow.
The focused correction below is a candidate for Golden Wrench re-review.

## Implemented contract

[Collection equality](../COLLECTION_EQUALITY.md) is the canonical behavior contract;
[the runnable example](../../examples/learn/collection_equality.void) demonstrates it.
Matching fixed arrays and growable lists compare length and ordered contents.
Dictionaries compare logical key/value contents independently of insertion order,
while iteration remains insertion ordered. Values compare with ordinary element
`==`; `!=` negates the same comparison. Nested collections, supported enums,
nullable presence wrappers, concrete generics, callable and managed identity, and
accessible custom equality retain their existing meanings. No alias fast path
bypasses NaN behavior or custom side effects. Operands evaluate once, left first.

Readonly views and ordinary fixed-array value snapshots use existing machinery.
Structural mutation of either dynamic operand during element comparison fails
through existing version checks, including a callback returning false. Existing
nonstructural replacement rules remain; comparisons do not promise transactions
or concurrent snapshots. Recursive structural collection/enum shapes diagnose;
managed object graphs stop at identity leaves. User operators remain responsible
for their own recursive calls. Unsupported plain value equality still diagnoses.

Pure built-in constant comparisons support nested collections, active enum
payloads, nullable presence and callable identity. Dictionary literal duplicate
keys canonicalize with the existing last-write-wins rule. Custom operator calls
and managed allocations remain excluded from constants. All-null nullable
identity templates remain valid without permitting managed allocation.

Key eligibility was audited against existing data-only schemas. Integers, bool,
exact UTF-8 strings, eligible enums/value structs, nullable value wrappers and
managed identity keys retain coherent equality/hash behavior. Value-key snapshots
and managed identity remain distinct. Collection, float, interface, option/result
and custom-equality key exclusions are preserved, including stored custom leaves.
Content equality does not make mutable collections hashable. No new key family,
public hash API, callback, Runtime hashing algorithm or native ABI was needed.
Bucket-collision tests exercise full-key lookup, removal/reinsertion and reversed
insertion order. Private hash values make no cross-launch stability or hostile-
input randomization promise.

## Implementation

- `collection_equality.inc` generates ordinary private comparison functions using
  the existing enum-helper approach, source foreach/count/lookup and element
  operators. Existing private collection version/check hooks lower to existing IR.
  There is no new Runtime representation, backend opcode or callback subsystem.
- Helpers are cached by concrete type, readonly capability and requesting function.
  Both collection and nested enum adapters preserve lexical ownership and generic
  specialization context. Otherwise private operators and the approved exact
  substituted-owner generic operator grant would be lost inside the helper.
- Typed IR independently verifies inherited operator/owner authority through the
  existing lexical-parent model: private readonly bool helpers, same-module parent
  identity and actual direct parent call. Helpers cannot be called by another
  parent or escape as function values. Tests corrupt provenance to verify rejection.
- `collection_constants.inc` compares the existing constant representation and
  active enum payloads. It serializes eligible data-only keys privately to recognize
  duplicate keys; this is compile-time canonicalization, not a Runtime hash API.
- Source operator completion reports built-in `==`/`!=` and readable types, hides
  derived helpers and avoids counting synthetic helper operator occurrences as
  extra source references. Unsupported collection shapes diagnose without internal
  package IDs. Dictionary custom-equality notes retain the declaration location
  without exposing an internal owner key.

No new dependencies, Runtime storage/GC subsystem, LLVM optimization changes,
security-policy changes or public callable API were introduced.

## Bounded adjacent nullable correction

The new nullable-fixed test exposed a pre-existing G79/G82 recheck defect:
`print(value == null)` failed for a nullable value wrapper after display binding
reanalyzed its already-typed `$nullable_null_equal` expression as a source operator.
`nullable-print-baseline.json` records the unchanged G82 Release compiler hash
`ca6b06c698f08811e2e7da8c9517099e5fff187f438eea9f312a5873a2bc1814`, matching retained
G82 qualification, the exact input and exit-1 `VOID2016` diagnostic. The minimal
correction recognizes already-checked null-test markers before nullable operator
checking. The same input now prints `true`; nullable-fixed tests cover it in each
profile. No null semantics or checked extraction contracts changed.

## Research and applicability

Bounded upstream research informed the contract:

- [.NET SequenceEqual](https://learn.microsoft.com/en-us/dotnet/api/system.linq.enumerable.sequenceequal)
  and [Swift Array equality](https://developer.apple.com/documentation/swift/array/%3D%3D%28_%3A_%3A%29)
  document ordered equal-length element comparison. VOID uses its own ordinary
  equality/access contracts rather than adding a foreign comparer framework.
- [Rust HashMap](https://doc.rust-lang.org/std/collections/struct.HashMap.html)
  documents equal-key/equal-hash coherence and the danger of mutable key identity.
  This supports retaining VOID's safe schema and snapshot/identity policy.
- [Swift OrderedDictionary](https://github.com/apple/swift-collections/blob/main/Documentation/OrderedDictionary.md)
  explicitly differs from ordinary Dictionary by making equality order sensitive.
  VOID keeps insertion iteration but deliberately chooses logical dictionary
  equality. No upstream order policy is assumed from storage layout alone.
- [Swift generics](https://download.swift.org/docs/assets/generics.pdf) provides
  background on element-dependent comparability. VOID reuses existing concrete
  specialization and normal operator checking, without adding a conformance system.

These are established documentation/source references, not proposed upstream
patches or proof of VOID correctness. Existing VOID display adapters and lexical
closure provenance supplied the directly applicable implementation pattern.

## Verification and evidence

Task-owned evidence: `/tmp/g83-s6ssftru`. Raw results, original inputs, failures,
source/artifact identities and commands are retained outside the checkout.
Qualification uses LLVM/Clang/LLD **23.1.4** exclusively and the existing
`build/llvm23-{debug,release,strict}` configurations.

| Qualification | Final result | Provenance |
| --- | --- | --- |
| Debug selected gates | 28/28 qualified | Initial 27/28; focused updated enum/stdlib/source rerun 3/3. Not one uninterrupted all-green run. |
| Release selected gates | 28/28 qualified | Initial 26/28; corrected CTest rerun 3/3 and exact-identity focused records 3/3. |
| Fatal ASan/UBSan/leaks | 28/28 | One clean `strict-final-*` run after build completion; 261.50 seconds. |
| New G83 source/application checks | 133/133 per profile | Final inputs, fresh JIT/AOT O0/O2, expected negative exits and original 1 MiB child stacks. |
| Extension tests | 14/14 | Existing configuration, receipt, diagnostic-version and grammar tests. |
| Real VS Code 1.141.0 | 10/10 | Isolated test profile, final Release server, provider and recovery observations. |
| Real VSCodium 1.143.1 | 10/10 | Existing Linux test copy, isolated profile, same server and observations. |
| Runnable example | Pass | Five `true` lines, exit 0, final Release compiler. |
| Whitespace/index | Pass | `git diff --check`, new-file trailing-whitespace scan; index remains empty. |

Strict environment: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`; `LSAN_OPTIONS` unset.
No suppressions were introduced. The Debug large fixed-payload stdlib stack case
passed in 413.78 seconds at its unchanged 900-second test timeout and 1 MiB child
stack. This is test duration, not a comparative performance result.

`*-qualification.json`, final corrected records, CTest XML/transcripts and
`final-source-artifact-manifest.json` record commands, exact source hashes,
compiler/server/test artifact hashes and selected build settings. Production source
hashes match all three final compiler qualifications. Debug/Release snapshots
precede the documented test-expectation corrections; focused records establish the
final test and key-fixture identities. Later documentation-only edits are captured
in the final manifest. `qualification-summary.json` preserves the initial failures
alongside the combined result rather than rewriting the first batches as passes.

Reproduce the focused G83 checks with the registered `collection_equality_source`,
`collection_equality_runtime` and `lsp_collection_equality` CTest names in a freshly
built profile. `qualify.py` and `corrected.py` in the evidence archive retain the
exact selected integrated and correction commands. The new source test additionally
covers constant structural/enum/nullable keys, all integer key widths, 40 colliding
bucket inputs, NaN aliases, UTF-8/NUL, custom effects, private access and imported
generic operator provenance.

Archive: `~/Documents/Void.Language-archives/g83-collection-equality-2026-11-20-s6ssftru.zip`,
with adjacent `.index.json` and embedded `_ARCHIVE_MANIFEST.json`. Selected original
logs/JSON/inputs, final patch and all 38 candidate text files are included; original
working evidence is retained. ZIP CRC and every selected member SHA-256 are checked.
Builds, executable copies and disposable editor profiles are excluded.

The selected integrated tests cover collection source/runtime, enum equality,
dictionaries, generics/interfaces, nullable behavior, ordinary frontend/tooling,
operator resolution/access, LSP recovery/semantic information and the original
stack regressions. They are proportionate local gates, not a full repository run.
The new source harness imposes **1,048,576 bytes** on every POSIX child, including
fresh JIT and AOT O0/O2 programs and expected failures. Original semantic-depth,
async, globals, source-CLI and stdlib stack tests keep their existing limits and
failure policies. Timeouts, assertions and sanitizer checks were not relaxed.

Runtime tests verify precise roots, custom-operator allocation under forced GC,
managed cycles as identity leaves, aliases, failure cleanup/retry and independent
Runtime owners, at O0/O2. Typed-IR corruption checks reject missing parent proof,
wrong-parent calls and public adapter forgery. LLVM validation and JIT/AOT lowering
use the unchanged backend; no backend source or native ABI changes are present.

### Preserved unfavorable and intermediate evidence

- `baseline.json`: clean accepted G82 rejects list equality with `VOID2016`.
- Numbered build logs retain compile-time implementation errors and an initial new
  target registration miss before reconfiguration; successful logs do not replace them.
- `private-context-before.log` and `generic-context-before.log` retain access failures
  before helpers inherited checked lexical/generic provenance; matching after logs
  and maintained fixtures demonstrate correction.
- `source-first` through `source-fourth` retain fixture errors, the real nullable
  recheck failure, and expectations revised to existing presence/constant contracts.
  `keys-first.log` retains invalid literal-suffix fixture syntax. `lsp-first` retains
  an assertion which omitted the existing readonly signature annotation.
- `enum-lsp-before-update.log` and `original-enum-lsp.py` preserve the obsolete
  expectation that list enum payload equality diagnoses. The maintained negative
  now uses a plain value element without equality, retaining editor range checks.
- Initial Debug/Release integrated evidence preserves obsolete enum-list equality
  negatives (`original-enum-completeness.py`) and Release preserves `collections_stdlib_frontend`
  rejecting its test expectation: primitive nested-list equality now correctly
  succeeds. `original-collections-frontend.py` is retained. Three original cases
  remain positive execution tests; their negatives now use unsupported value
  elements. No unsupported-element negative or access test was removed. Enum-list equality
  positives remain in the G83 source fixture.
- Nullable constant exploration logs, including a breakpoint that was not reached,
  remain. Actual collection `T?` uses G79's presence wrapper; no fabricated claim
  that null equals empty or nullable collection constants are unsupported remains.

- `strict-overlap-aborted.json` records a test-orchestration error: an early
  launcher was terminated before build completion, but its child CTest survived
  and overlapped another run. Both task-owned test trees were stopped. Their
  overlapping logs are unfavorable evidence, not acceptance. Only the later
  `strict-final-*` run after the completed build counts as sanitizer qualification.

## Focused correction after independent review

Golden Wrench's preserved review is `/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/retained/g83-correction-2kjprh_e/independent-review/REVIEW.md`.
Its original `depth_24_equality.void` and `depth_48_equality.void`, declaration
controls, `reproductions.json`, nine additional failures in `repeat.json`, and
fatal sanitizer traces remain unchanged. These are failures of the reviewed
candidate, not results of this correction. Correction evidence is retained in
`/tmp/g83-correction-2kjprh_e`; `review-original-sha256.json` records every original
review artifact before correction work.

The cause was synchronous `analyze_one` calls from equality-helper construction.
Each child helper retained the parent function's expression/statement analysis
frames, exhausting the required 1 MiB stack at only 24 list levels. Both collection
and enum adapters now publish their prepared functions without analyzing them
immediately. The existing `Unit::functions` list and ordinary pending-body pass
in `Semantics::analyze` analyze each body after the previous body has unwound.
Both edges must defer because enum payloads and collections can alternate.
No separate queue, analysis engine, extra stack/thread, or smaller nesting limit
was introduced. The existing structural cycle and 128-level guard remain intact.

Generated bodies still contain ordinary element `==` expressions, with their
original source tokens and inherited lexical/generic context. Helpers created
while analyzing constants, parameter/field defaults or globals remain in the
same function list and are checked before successful compilation finishes, even
if the originating constant comparison folds away. Unsupported empty constant
leaves therefore still diagnose. Custom dispatch, key eligibility, mutation
checks, readonly capabilities, typed-IR authority and GC lowering are unchanged.

The new `collection_equality_nesting` regression uses the original **1,048,576-byte
soft and hard stack limits** for every POSIX compiler, linker and executable
child. It generates byte-identical independent-review 8/24/48-level comparisons
and controls, checks adjacent 23/25/47/49 levels, and exercises alternating
enum/list structural depths 127/128 with a source-located rejection at 129.
It also checks the existing source-nesting diagnostic. Fresh JIT and AOT O0/O2
cover 8/24/48 comparisons and controls, nonempty 24-level lists, dictionaries and
fixed arrays, deep private equality through a generic static method, and
constant/global initialization. Negatives cover private access, unsupported
runtime and folded-constant leaves, constant custom equality and type mismatch.

Exact correction inventory relative to the independently reviewed 38-path
candidate: **five paths**, four edited and one added:

- `src/frontend/collection_equality.inc`: defer prepared helper body analysis.
- `src/frontend/enum_equality.inc`: defer payload helpers, including alternating
  collection/enum chains.
- `tests/collection_equality/nesting.py` (new): focused stack and diagnostic matrix.
- `CMakeLists.txt`: register the focused regression with a 1200-second test budget.
- `docs/projects/G83_COLLECTION_EQUALITY_HASHING.md`: correction and evidence.

All other candidate paths retain their independently reviewed content. The total
candidate inventory is now **39 paths: 21 modified and 18 new**, listed below.

The final nesting matrix passed **94/94 checks in each of Debug, Release and
fatal ASan/UBSan/leaks**. It was invoked directly through the registered Python
script; `nesting-{debug,release,strict}.json` and matching logs retain exact inputs,
commands, expected/actual exits, output, compiler SHA-256 and child stack limits.
All original 8/24/48 reproducers and controls match the independent review bytes.
Final CTest registration is recorded in `registration-*.json`. To repeat:

```bash
TMPDIR="$PWD/build/g83-correction-tmp" ctest --test-dir build/llvm23-debug \
  -R '^collection_equality_nesting$' --output-on-failure
```

Create that scratch directory first; use the matching Release/strict directory
and the fatal environment below for sanitizer repetition. This disk-backed
scratch choice changes no compiler or application stack limit.

Supporting correction gates are `collection_equality_source`,
`collection_equality_runtime`, `lsp_collection_equality`,
`enum_equality_diagnostics` and `lsp_enum_equality`. All five are qualified in
Debug, Release and fatal sanitizers. Release is one clean 5/5 run (15.25 seconds),
and fatal sanitizers one clean 5/5 run (67.68 seconds). Debug initially passed
4/5; its source gate passed on a separate 35.32-second retry. That is combined
qualification, not an uninterrupted 5/5 run. The source gate contains 133
source/application checks per profile, including custom equality, key exclusions,
generic/private access and mutation behavior. The rebuilt runtime target checks
precise GC roots, forced collection during custom equality, cleanup/retry and
typed-IR helper authority at O0/O2. The two editor gates check compiler providers
and diagnostics; no real-editor or unrelated full qualification suite was rerun.

Unfavorable correction evidence is retained:

- `gates-debug.log`, `source-debug-first.json` and `nesting-debug-first.*` record
  two initial Debug AOT linker bus errors under concurrent `/tmp` runs, including
  a declaration/count control. Later runs use disk-backed
  `build/g83-correction-tmp` scratch with the same stack limits and compiler.
  The linker failure's cause was not established; no linker correction is claimed.
- `nesting-release-first.*` retains an exploratory maximum-depth 128 JIT O0
  invocation that exceeded its 90-second child timeout. The accepted 127/128
  structural-boundary results are frontend checks. Full JIT/AOT O0/O2 coverage
  is focused on the original 24/48 cases and the other cases described above.
  This timeout is excluded from passing results; it is not a nesting rejection.

The fatal environment remains
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, with `LSAN_OPTIONS`
unset and no suppression options. All three profiles use the existing LLVM/Clang/
LLD 23.1.4 configurations. No stack limit, existing timeout, assertion or sanitizer
policy was relaxed. The new nesting gate has its own 1200-second total budget and
90-second child budget; the original source gate keeps its 240-second budget.

`commands.json`, build/configuration logs and `final-manifest.json` capture the
correction commands, all 39 candidate hashes, the five-path delta against the
independently reviewed candidate, compiler/server/runtime-target identities,
unchanged accepted HEAD, empty index and whitespace checks. Original review
artifact hashes are rechecked before packaging. The corrected candidate and
selected evidence, including original reproducers, failure JSON and sanitizer
traces, are retained in
`~/Documents/Void.Language-archives/g83-stack-correction-2026-11-20-2kjprh_e.zip`,
with an adjacent `.index.json` and embedded `_ARCHIVE_MANIFEST.json`.
Every archive member SHA-256 and ZIP CRC is verified. The original implementation
archive and independent evidence directory are retained. This candidate awaits
focused Golden Wrench re-review; no integration approval is inferred.

## Limits and impact

Linux x86-64 local qualification only. Native Windows/macOS are **UNQUALIFIED**.
Real editor checks exercise providers, metadata, diagnostics and recovery; no new
popup-paint latency, pixel/screenshot or performance benchmark claim is made.
No hosted CI, installation or extension publication ran. Structural graph equality,
general value-struct equality, arbitrary custom hashing, floating keys and new
collection key families are outside the implemented contract.

Comparison work is linear in sequence length or dictionary entries with existing
schema lookup costs, short-circuiting on differences. Helpers use readonly existing
storage; fixed arrays retain normal value snapshots. New helper code can increase
compiled code size by requested caller/type specialization. There is no new global
cache, hashing pass or hidden deep collection clone. No comparative performance
benchmark or production speedup claim is made.

## Exact candidate inventory

39 paths: 21 modified and 18 new. No generated binaries, editor profiles or build products are included.

- `CMakeLists.txt`
- `docs/ARRAY_OPERATIONS.md`
- `docs/COLLECTION_EQUALITY.md`
- `docs/DEVELOPMENT_STRATEGY.md`
- `docs/DICTIONARIES.md`
- `docs/ENUMS.md`
- `docs/FIXED_ARRAYS.md`
- `docs/GENERICS.md`
- `docs/GROWABLE_ARRAYS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/README.md`
- `docs/collections/API.md`
- `docs/collections/DESIGN.md`
- `docs/projects/G83_COLLECTION_EQUALITY_HASHING.md`
- `docs/stage10/OPERATORS.md`
- `editors/vscode/src/test/collectionEqualityHost.ts`
- `examples/learn/collection_equality.void`
- `src/frontend/collection_constants.inc`
- `src/frontend/collection_equality.inc`
- `src/frontend/compiler.cpp`
- `src/frontend/enum_constants.inc`
- `src/frontend/enum_equality.inc`
- `src/ir/interfaces.inc`
- `src/ir/module.cpp`
- `tests/collection_equality/editor.void`
- `tests/collection_equality/imported/api.void`
- `tests/collection_equality/imported/helper.void`
- `tests/collection_equality/imported/main.void`
- `tests/collection_equality/keys.void`
- `tests/collection_equality/lifecycle.void`
- `tests/collection_equality/lsp.py`
- `tests/collection_equality/main.void`
- `tests/collection_equality/nesting.py`
- `tests/collection_equality/private.void`
- `tests/collection_equality/runtime.cpp`
- `tests/collection_equality/source.py`
- `tests/collections_stdlib/frontend.py`
- `tests/enums/completeness.py`
- `tests/lsp/enum_equality.py`
