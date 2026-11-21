# G79 — Nullable type completeness

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Baseline: `6ca2baf0cc667aac2b37852e0c42e96e5a0700e7`.
Linux x86-64 implementation candidate. The initial qualification below is retained
as historical evidence; the final review-correction qualification appears at the end.
No independent review, hosted acceptance, Windows/macOS acceptance or installation
is implied. No staging, commit, push or publication is authorized for this work.

## Implemented contract

[Nullable types](../NULLABLE_TYPES.md) is the canonical guide. The maintained
example is [nullable.void](../../examples/learn/nullable.void).

- `T?` covers numeric primitives, bool, strings, value structs, tagged/numeric/
  flags enums, fixed arrays, lists and dictionaries. Existing nullable managed,
  interface and callable representations remain unchanged. Supported values can
  appear in fields, globals, parameters, returns, collections and generics.
- Required types remain required. Contextual `T` to the same `T?` lifting is
  explicit in typed expressions. Null requires a nullable context; omitted
  nullable locals/fields/array elements default to null without constructing T.
- Null differs from zero, false, empty strings and empty collections. Null checks
  work for every payload. Two present values use the payload's existing equality;
  unsupported equality still diagnoses. No new general collection/value equality,
  lifted arithmetic, implicit dereference or truthiness is introduced.
- `Ok(pattern)` tests presence using the existing pattern system. `as T` checks
  same-payload extraction before reading the payload. `as? T` returns canonical
  `option<T>`; the existing option representation now admits ordinary supported
  payloads. Nullable, option and result remain different nominal families.
  Existing numeric range/discriminant casts and result error propagation remain.
- Generic `t?` specializes and infers the payload type. An already nullable
  substitution remains nullable without another presence layer. Explicit custom
  operators can take nullable secondary operands or return nullable results;
  a nullable value operand alone still cannot satisfy the declaring-owner rule.
- Copies retain existing value/fixed snapshot versus list/dictionary/reference
  identity rules. Readonly facts cross wrapping, extraction, Some and patterns
  without granting mutable authority. Native ABI/security rules are unchanged.
- Hover, completion details, signatures and generic call sites show public nullable
  names. Nullable type mismatch messages use the same readable editor spelling;
  compiler diagnostics retain canonical identities, codes, spans and recovery.

## Architecture and bounded upstream research

[Microsoft's nullable value contract](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/builtin-types/nullable-value-types),
[the current .NET Nullable implementation](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/Nullable.cs)
and [Roslyn nullable conversion lowering](https://github.com/dotnet/roslyn/blob/main/src/Compilers/CSharp/Portable/Lowering/LocalRewriter/LocalRewriter_Conversion.cs)
were inspected before implementation. These are established published contracts
and upstream source, not proposed patches. Presence plus payload, checked
extraction and wrapping after checking the payload fit VOID. C#'s boxing and
nullable-reference warning system do not replace VOID's existing strict types,
precise GC, readonly authority or native layouts. No upstream patch/framework is
imported. The later inline-layout check also consulted [C# CS0523](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-messages/cs0523): value containment must be finite, while reference indirection permits recursive graphs. That invariant applies to VOID; its existing growable-value representation remains its own contract.

Non-reference nullable storage is a private canonical two-case sum (`$null`,
`Ok`) with i32 tag and one payload. It reuses existing `sum`, `sum_get`, branches,
`panic`, aggregate copying and zero-inactive storage. References/interfaces/
callables continue to use their existing nullable representation. No production
LLVM backend or Runtime implementation file changes, new IR opcode, allocation
registry or dependency are needed. Descriptor validation rejects forged private
nullable identities and incompatible layouts. Ordinary option payload validation
still goes through full type/storage validation.

Inline dependency preparation follows private nullable/option/result storage and
fixed arrays through the existing source layout checker. It stops at managed and
growable indirection. This rejects infinite nullable inline layouts without
rejecting recursive value elements behind growable lists. The GC stress includes
a cyclic list of nullable value structs.

Source annotations are resolved during validation and retained in the Unit for
immutable tooling queries after validation callbacks are retired. Generic shapes
include written nullability when comparing operator parameter tuples. Nullable
value storage remains separate from source enum metadata and cannot expose private
fields/cases as member completions. Specialized generic function references now
carry concrete presentation signatures while source declarations remain symbolic.

## Focused verification

Three registered tests provide the new coverage:

- `nullable_completeness`: **81 compiler/application checks per profile**, with
  all numeric widths, bool, NaN/signed zero, string/empty values, enums, nullable
  keys, collections, globals/defaults, nested option/result, generic fields/enums,
  operator operands/results, finite/recursive inline layouts and single operand evaluation. Positive and checked
  runtime-failure programs run through JIT/AOT at O0/O2. Negative tests retain
  required-type rejection, nominal identity and readonly constraints.
- `nullable_ownership`: O0/O2 with Runtime collection budget 1, three invocations
  each, live cyclic graphs containing nullable aggregates/strings/lists/dictionary
  values and fixed snapshots. Checks live payloads and zero heap objects after
  return/collection, plus rejection of a forged nullable descriptor.
- `lsp_nullable_completeness`: actual voidls hover, signature/active parameter,
  generic declaration versus concrete use, semantic tokens, private-symbol
  exclusion, unwrapped member completion and unsaved invalid-to-valid recovery.
  Nullable mismatch diagnostics are checked for readable names and intact errors.

The relevant integrated selection covers frontend/numeric behavior, G78 enum
features/corrections, generics, arrays, collections, dictionaries, patterns,
closures/cleanup, managed initialization/reference values/interfaces/strings,
properties, allocation elimination, explicit operators, tooling/analysis and editor
recovery. The existing **1,048,576-byte** stack requirements and nesting tests are
unchanged. Existing test limits and failure policies are unchanged.

Final-source results (all **38/38**, zero failures/skips):

| Profile | Result | Integrated elapsed |
| --- | --- | --- |
| Debug | 38/38 | 149.22 s |
| Release | 38/38 | 40.62 s |
| Fatal ASan/UBSan/leaks | 38/38 | 155.43 s |

Each profile ran the same accepted fixtures, including all 81 new compiler/application
checks. [Qualification JSON](evidence/g79/qualification.json) contains commands,
per-case output, source/test and binary hashes, actual negative exit codes and
editor observations. Earlier successful and unsuccessful runs remain separately
identified. The documentation example returns 0 and prints `100`, `0`, `Boss`, `0`.
Three additional absence-only string/list/dictionary smoke programs also return 0.

Fatal sanitizer environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Extension unit checks: **13/13** using the explicitly selected current server.
Real VS Code **1.141.0** and the existing VSCodium **1.143.17232** Linux test copy
(product API `1.143.1`) each perform **9 nullable editor checks** in isolated
profiles using the source extension and repository-built server. These exercise
real editor providers, parameter-hint/suggestion commands and unsaved edits;
they are not popup paint-latency or human visual-usability measurements. User
settings, active extensions and installed compiler/server are not replaced.

## Unsuccessful evidence and corrections

[Unsuccessful evidence](evidence/g79/unsuccessful-evidence.json) retains the initial
unsupported `i32?` baseline, intermediate compiler/build/test failures and harness
errors. Raw logs remain under `/tmp/g79-gaw4u4as` with recorded hashes.

1. The first factory implementation attempted to resolve nullable annotation
   metadata through callbacks after validation had cleared them (`bad_function_call`).
   Persisted resolved annotation types remove that lifetime dependency. Reference
   annotations retain their existing direct representation.
2. Generic declaration checks initially retained old nullable deferrals. Those
   deferrals were removed; concrete specialization performs ordinary checks.
3. A typed unary operand could receive the outer nullable hint and be wrapped
   before unary checking. Payload context now checks ordinary expressions before
   lifting; nullable branches retain nullable context. The signed-zero regression
   covers this independently of literal initialization.
4. The initial LSP probe exposed symbolic generic call hover. Concrete reference
   presentation now uses the already-specialized function signature. The probe
   also initially omitted semantic-token capabilities and assumed only one LSP
   completion result shape; those harness mistakes were corrected without
   changing the server protocol.
5. Initial integrated Release: **29/30**, with an unused generic operator whose
   only operand was a nullable value owner incorrectly admitted. The explicit
   owner check now preserves the existing restriction. The former rejection of
   a nullable value return is obsolete under G79 and is now a positive regression.
   Nullable versus non-nullable generic collection operator operands retain
   distinct tuples and exact dispatch. The old string-nullability rejection is
   likewise replaced with a positive null-versus-empty execution check; required
   string null rejection remains in the same test.
6. An extension run without `VOIDLS_TEST_SERVER` selected the old default
   `build/debug/voidls`: **12/13**. Explicit selection of the qualified current
   server gives **13/13**. No installed server was changed to mask this mismatch.
7. Fixture corrections retain the distinction between optional callable and
   callable-with-optional-return syntax, and the established `not (... is None)`
   pattern grammar. An intermediate expanded check also used a pre-refresh
   compiler and recorded the old operator behavior. Final builds explicitly
   refresh the frontend after edits, before final qualification.

8. A final direct source check accepted an illegal recursive `value struct` through
   a nullable field (expected rejection, actual exit 0). Inline dependency
   preparation now follows private sum storage and fixed-array elements into the
   existing nominal layout guard. Four negative regressions cover direct, mutual,
   option and fixed-array recursion; positive execution/GC tests retain valid
   growable indirection. The earlier 38/38 Debug/Release runs are retained as
   pre-correction evidence, separate from the final-source results.

No crash or negative result is reclassified as a pass. Expected extraction
failures remain exit 1; successful fixtures retain their specified behavior.
Historical G75/G78 evidence and native graphics/audio limitations are untouched.

## Code generation, cost and limits

The existing LLVM 23.1.4 sum lowering and optimizer are reused. Nullable value
storage adds a tag and inline payload; it does not box ordinary values. Absence
checks precede extraction, operands evaluate once and inactive payloads stay
zeroed for tracing. Precise-GC stress, dictionary null-versus-zero keys, fixed
snapshots and JIT/AOT O0/O2 checks qualify these properties on native Linux.

No new latency target or unrelated benchmark campaign was introduced. There is
no isolated before/after performance benchmark for G79, and no claim of measured
zero overhead. Existing supported non-nullable layouts and nullable reference
layouts are retained; normal value storage cost remains visible in the type's
representation. The focused checks and integrated timings are recorded rather
than presented as a compiler-performance benchmark.

Native **Windows and macOS remain UNQUALIFIED**; no native execution occurred.
Linux evidence does not qualify their ABI/layout/editor paths. The complete
repository suite, hosted CI, foreign nullable ABI and renderer paint latency are
outside this local qualification. No user-prefix tools are installed.

## Initial candidate inventory (retained)

**38 paths: 24 modified, 14 new.** Initial working tree was clean;
all paths belong to G79. No index entries, accidental generated artifacts, installed
prefix changes or unrelated worktree changes are included. `git diff --check`
passes. Nothing is staged, committed, pushed, installed or published.

- `CMakeLists.txt`
- `docs/ADVANCED_LANGUAGE.md`
- `docs/DIAGNOSTICS.md`
- `docs/EDITOR_INTEGRATION.md`
- `docs/GENERICS.md`
- `docs/INITIALIZATION.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/MANAGED_STRUCTS.md`
- `docs/NULLABLE_TYPES.md`
- `docs/NUMERIC_TYPES.md`
- `docs/README.md`
- `docs/STRINGS.md`
- `docs/projects/G79_NULLABLE_COMPLETENESS.md`
- `docs/projects/evidence/g79/baseline.json`
- `docs/projects/evidence/g79/qualification.json`
- `docs/projects/evidence/g79/unsuccessful-evidence.json`
- `editors/vscode/src/test/nullableHost.ts`
- `examples/learn/nullable.void`
- `include/void/ir.hpp`
- `src/frontend/capabilities.inc`
- `src/frontend/compiler.cpp`
- `src/frontend/enum_api.inc`
- `src/frontend/generic_declarations.inc`
- `src/frontend/generic_inference.inc`
- `src/frontend/generics.inc`
- `src/frontend/pattern_lowering.inc`
- `src/frontend/pattern_semantics.inc`
- `src/ir/module.cpp`
- `src/lsp/main.cpp`
- `tests/nullable/completeness.py`
- `tests/nullable/conversions.void`
- `tests/nullable/lsp.py`
- `tests/nullable/ownership.cpp`
- `tests/nullable/primitives.void`
- `tests/nullable/references.void`
- `tests/nullable/values.void`
- `tests/stage10/operator_declarations.cpp`
- `tests/strings/tests.cpp`

## Golden Wrench corrections — current candidate

The independent review returned **CORRECTION REQUIRED** for three concrete
findings. This section supersedes the initial candidate's readiness statement;
its original qualification and unfavorable evidence above remain intact.

1. **Constant nullable `as?` ownership:** the evaluator assigned an owning value
   from a subobject in its own vector. That destroyed the source before its
   remaining members were copied. It now retains an immutable owning input and
   copies a present payload directly into independent option output. Absent
   values do not read the inactive payload. `Some`/`None`, checked conversions,
   constant-call restrictions and Runtime lowering are unchanged.
2. **Nullable display:** the existing iterative collection/sum display adapter
   now recognizes private nullable storage. Absence prints `null`; presence uses
   the payload's normal display without private type/case names or added wrapper
   punctuation. This covers `print`, `string`, interpolation and nested collection
   elements. Custom payload string hooks, ordinary enum/result/option behavior,
   collection cycle handling and existing alignment behavior are preserved.
3. **Readonly diagnostics:** logical reference-path rendering omits private
   presence fields and names the source nullable type (for example,
   `carrier?.child`). LSP readonly primary and related messages use the existing
   readable nominal formatter. Canonical type identities, authority checks,
   VOID2007 rejection, related source locations and document recovery are unchanged.

The [C++ lifetime specification](https://eel.is/c++draft/basic.life) supports
keeping the input subobject alive through copying. The established
[.NET Nullable display implementation](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/Nullable.cs)
checks presence and delegates present values to their payload. VOID retains its
own `null` absence spelling, rather than adopting C#'s empty nullable display.
These references support the bounded corrections; no framework or new language
operator is introduced.

### Reproductions and focused coverage

The Boss independently ran the preserved minimal command and reported
`minimal strict check -6 True`. That transcript, the independent review, original
input/diagnostics and checksums are retained in the correction evidence. None of
those original files was overwritten by correction verification.

The exact original minimal, long-string and two-string aggregate inputs now pass
all **18/18** check/run invocations across Debug, Release and fatal sanitizers.
Their original sanitizer failures and Release string-pool failure remain evidence.

`nullable_review_corrections` adds **17 checks per profile**, exercising heap-backed
string/aggregate/fixed-array constants, absence, zero/false/empty values, agreement
with runtime extraction, printing/interpolation/collection elements, custom hooks,
normal enum/result controls and three readonly rejection paths. Positive programs
run in JIT/AOT at O0/O2 with exact output assertions. The existing nullable LSP test
now verifies ordinary/generic VOID2007 messages and related information, original
primary/related source spans, and invalid-to-valid recovery.

Two initial test-fixture mistakes are also retained: a dictionary expectation
needed escaped literal braces, and a temporary variable used reserved `event`.
Only those fixture spellings were corrected; no parser rule or negative test was
weakened. Focused Debug and Release runs then passed both relevant tests.

### Final correction qualification

| Profile | Final relevant gates | Elapsed |
| --- | --- | --- |
| Debug | **18/18** | 68.88 s |
| Release | **18/18** | 20.40 s |
| Fatal ASan/UBSan/leaks | **18/18** | 74.46 s |

All profiles report zero failures/skips. The [correction evidence](evidence/g79/corrections.json)
records exact commands, actual output/exit codes, source/test and compiler/server
hashes, extended LSP records, the independent review and the Boss's reproduction.
All 25 original review/evidence files still match their pre-correction checksums.
The original minimal command was not rerun or altered; correction tests read the
original inputs and write their results separately. `as?` itself retains its
existing public behavior. This is local implementation qualification, ready for
focused independent re-review rather than a claim of independent acceptance.

The selected gates cover the original 81 nullable checks, new 17 correction checks,
precise-GC ownership, frontend/strings/closure and pattern behavior, formatting and
reload/tooling, readonly reference/interface behavior, direct LSP presentation and
recovery, and the unchanged original **1,048,576-byte** stack requirements.
Original test limits and fatal ASan/UBSan/leak options remain in force.

The initial 38/38 suites, extension 13/13 and real VS Code/VSCodium 9/9 records are
retained as earlier evidence. They are not mislabeled as fresh correction reruns.
The source extension is unchanged; current correction presentation is checked
through actual voidls, including related diagnostics. No new GUI paint-latency,
full-repository, native Windows/macOS or hosted qualification is claimed.

No production LLVM backend/Runtime, native ABI, FFI authorization, dependency or
installation changes were needed. No new benchmark target was introduced; no
isolated performance delta is claimed for these correctness/presentation fixes.

### Final changed-file inventory

**43 paths: 25 modified, 18 new.** The correction touches
13 paths relative to the reviewed candidate. The complete final manifest is
also recorded in `corrections.json`. `git diff --check` passes and the index is
empty. No staging, commit, push, installation or publication occurred.

- `CMakeLists.txt`
- `docs/ADVANCED_LANGUAGE.md`
- `docs/DIAGNOSTICS.md`
- `docs/EDITOR_INTEGRATION.md`
- `docs/GENERICS.md`
- `docs/INITIALIZATION.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/MANAGED_STRUCTS.md`
- `docs/NULLABLE_TYPES.md`
- `docs/NUMERIC_TYPES.md`
- `docs/README.md`
- `docs/STRINGS.md`
- `docs/projects/G79_NULLABLE_COMPLETENESS.md`
- `docs/projects/evidence/g79/baseline.json`
- `docs/projects/evidence/g79/corrections.json`
- `docs/projects/evidence/g79/qualification.json`
- `docs/projects/evidence/g79/unsuccessful-evidence.json`
- `editors/vscode/src/test/nullableHost.ts`
- `examples/learn/nullable.void`
- `include/void/ir.hpp`
- `src/frontend/capabilities.inc`
- `src/frontend/collection_display.inc`
- `src/frontend/compiler.cpp`
- `src/frontend/enum_api.inc`
- `src/frontend/generic_declarations.inc`
- `src/frontend/generic_inference.inc`
- `src/frontend/generics.inc`
- `src/frontend/pattern_lowering.inc`
- `src/frontend/pattern_semantics.inc`
- `src/ir/module.cpp`
- `src/lsp/main.cpp`
- `tests/nullable/completeness.py`
- `tests/nullable/conversions.void`
- `tests/nullable/corrections.py`
- `tests/nullable/lsp.py`
- `tests/nullable/ownership.cpp`
- `tests/nullable/primitives.void`
- `tests/nullable/references.void`
- `tests/nullable/review_constants.void`
- `tests/nullable/review_display.void`
- `tests/nullable/values.void`
- `tests/stage10/operator_declarations.cpp`
- `tests/strings/tests.cpp`
