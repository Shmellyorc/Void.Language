# G80 — User-defined static fields and methods

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Baseline: `d9d9ff017a685abe59736fa088e5e01abf204c93` (G79).
Scope: one integrated G80 candidate; native Linux x86-64 qualification.
The current generic static-result correction and its final source evidence are
recorded at the end; the original 31-file qualification is retained below.
Toolchain: existing pinned LLVM/Clang/LLD 23.1.4 builds.

## Delivered behavior

Managed/value structs support `static name: type = expression`, inferred static
fields and `static const` runtime-readonly state. Fields are absent from instance
layouts/constructor parameters. Methods use `[visibility] static [async] func
owner.name<parameters>(arguments)` outside their owning module's type declaration,
with no instance or hidden receiver. The owner name is unqualified in declarations;
imported calls use normal module/type qualification. Named/default parameters,
returns, callable static fields, supported static function values and method-specific
generics reuse ordinary function behavior.

Private static access belongs to the exact nominal owner, including instance/static
methods, initializers and their closures. Public/internal access retains direct
imports and library domains. Static/instance lookup and collision diagnostics remain
distinct. Static lookup uses the written owner without inherited/promoted aliases.
Static properties, events, using fields, lifecycle/virtual methods, primitive/enum
extensions and a new async CLI-root convention are outside this feature.

Each concrete generic struct has independent cells, shared by all references to
that same nominal specialization. Generic-owner methods bind the owner's declared
type parameter names; method-specific parameters cannot shadow them. Declarations
keep symbolic editor metadata; concrete references carry concrete signatures.
Generic completion includes unspecialized method-specific generic declarations and
does not accumulate suggestions for every historical specialization.

[Static-member guide](../STATIC_MEMBERS.md) is the detailed syntax/lifecycle
contract. The [runnable example](../../examples/learn/static_members.void) covers
settings, a receiver-free value factory and independent generic state. The Language
Bible Section 19 and relevant generic/global/editor guides now describe implementation.

## Implementation and safety

- The ordinary parser stores static field syntax separately from instance fields.
  Concrete layouts materialize those declarations as the existing runtime-global
  initializer functions and cells. There is no second storage, GC or initialization
  subsystem. Source order merges static and module-global declarations; canonical
  concrete identity resolves equal-site generic ties.
- Static selection resolves an accessible nominal owner through the established
  import/generic resolver, then selects its own static declaration. Already checked
  calls retain their binding during repeated hint/display analysis, including
  zero-argument factories. Module-namespace completion excludes type members.
- Static methods lower as ordinary functions. A small typed-IR marker preserves and
  independently validates their nominal lexical access context. It does not add a
  receiver or change native instance layout. Generic function provenance reuses
  the existing nominal-specialization proof for static owners.
- Readonly cells carry the normal readonly field/authority checks. Compiler and IR
  validation retain private access, exact owner/module identity and worker-reachable
  global prohibitions. Generated worker wrappers retain an authorized same-type
  static context for private pure calls; this grants no shared-state access to workers.
- Initialization remains eager and synchronous, with unpublished-cell guards,
  persistent precise GC roots, failure state, owner-thread affinity and explicit
  resource cleanup. Static fields remain unavailable to compile-time expressions
  and instance-field/parameter defaults. Async static methods retain the existing explicit
  `compile_async`/scheduler-root contract.
- Cache hits and independent programs receive fresh Runtimes. Compatible body-only
  replacement retains values. Identity/type/visibility/readonly/order/initializer
  dependency changes retain the existing restart boundary. No source destructor
  is introduced into GC.
- Editor declarations/references retain canonical identities while source-facing
  signatures show readable owners and payloads. Static fields/methods receive
  semantic modifiers, navigation/references and compiler-bound completion. The
  TextMate grammar also recognizes `static` during pending analysis.

No LLVM optimization changes, Runtime ABI changes, native-approval changes, new
dependencies or separate editor parser were introduced.

## Bounded upstream research

- [Microsoft C# static contract](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/static):
  established type-qualified access, no instance receiver and one set of static
  fields per concrete generic type. These principles apply directly.
- [Microsoft static initialization](https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/classes-and-structs/static-constructors):
  established source ordering and initialization concerns. VOID deliberately retains
  its existing eager runtime-global protocol instead of adopting CLR first-use
  constructors/locks/default reads. There is no new static-constructor facility.
- [Roslyn binder implementation on main](https://github.com/dotnet/roslyn/blob/main/src/Compilers/CSharp/Portable/Binder/Binder_Expressions.cs):
  static context suppresses `this` while retaining a containing member/type. The
  applicable separation is reflected in VOID's receiver-free nominal access context.
  This is existing upstream source, not an unmerged proposal or proof of VOID's code.

Research supports the local design; VOID's initialization, ownership, native and
reload contracts govern execution. No CLR framework or LLVM pass was copied.

## Preserved unsuccessful evidence

Raw records are under `/tmp/g80-px1if_gm` (pointer `/tmp/g80-current`). The initial
G79 compiler rejects the Boss's static-field example. `baseline.json` preserves
that output before implementation.

Intermediate failures remain available, including the missing IR lexical-owner
model, generic static specialization provenance, imported type-root assignment,
private pure-worker context and zero-argument static calls revisited by display
analysis. Their corrections have focused regressions. Namespace leakage and
unused generic member conflicts were identified by source inspection; the final
checks cover their required behavior without claiming a pre-fix execution that
was not run.

The first integrated Debug selection passed 22/23 existing suites. Its frontend
failure showed `point.init()` incorrectly reaching static lookup before the
established lifecycle rejection. The existing check now keeps priority, and the
failed frontend suite plus static gates pass on retry. The original failing JUnit
and output remain separate.

A focused Debug reproduction also accepted invented method arguments on a
nongeneric method of a generic owner (`read<string>()`). Static selection now
distinguishes owner specialization from actual method type parameters and rejects
these arguments. The positive method-generic cases and negative regression remain
required. An older overlapping Release build also recorded that acceptance during
a source/binary freshness smoke check; it was not qualified as the corrected build.
A targeted translation-unit rebuild and fresh profile checks establish final
qualification.

The first C++ builds also retain the optional-Type assignment and local-name
shadowing errors. Initial editor probes used a string-literal cursor (the existing
format-completion context) and assumed a definition result was always an array;
the corrected probe uses a named argument value and accepts the protocol's single
Location shape. No language diagnostic or readonly rejection was weakened.

An exploratory async CLI fixture first used an unsupported async main convention,
then omitted the required scheduler root. Both failures are preserved. The final
async test uses the existing `compile_async` adapter at O0/O2; compiler entry
semantics were not changed to accommodate the test.

Original G79 review failures, the Boss's reproduction and committed qualification
records remain untouched. G80 output records are separate.

## Initial implementation qualification (retained)

| Profile | Relevant suite coverage | G80 source / lifecycle checks |
| --- | --- | --- |
| Debug | **26/26** covered, with proportional evidence reuse below | **68 + 46**, plus **7** managed-generic checks |
| Release | **26/26**, final source; 29.59 s | **68 + 46**, plus **7** managed-generic checks |
| Fatal ASan/UBSan/leaks | **26/26**, final source; 102.61 s | **68 + 46**, plus **7** managed-generic checks |

Debug retains 22 unaffected passes from the initial 23-suite integration run;
the frontend failure was corrected and its retry passed. The final three static
suites passed after the later method-type-argument correction (10.97 s). The
frontend/three-static retry after the lifecycle correction was 4/4 (11.46 s).
These are separate records, not a claim that the entire Debug selection was rerun
on every intermediate source. Final Release and strict runs cover all 26 suites
on the corrected source. No failure is hidden by evidence reuse.

The 68 source checks cover exact successful outputs and required rejections,
managed/value owners, concrete generics and method generics, imported aliases,
readonly/private access, source-order and unpublished-cell startup, function values,
side-effect ordering in interpolation, the runnable example and JIT/AOT O0/O2.
An additional identical managed-generic input passes check/JIT/AOT O0/O2 in all
three profiles (7 checks each); its exact source and commands are in the evidence.

The 46 native lifecycle checks cover explicit ABI parameter counts (no receiver),
precise roots/GC, persistence, independent/cache-hit Runtime isolation, foreign-thread
rejection, body-only reload, required restarts, readonly rejection, forged IR owner
proofs and private pure-worker calls through the existing async root at O0/O2.
Actual voidls checks cover symbolic/concrete generic hover, documentation/signatures,
active parameter 1, definition/references, property/method static tokens, private
filtering, no-history generic-method completion, namespace filtering and edit recovery.

The selected integration gates cover the existing frontend, generic inference/CLI,
inheritance, receiver methods, runtime globals, enum/nullable completeness and
ownership, tooling and async worker/syntax contracts. The unchanged actual
**1,048,576-byte** source, semantic-depth, global and async stack tests pass.
Strict settings remain `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, with no LSAN
suppression options. Original expected failures remain their expected failures.

Extension checks pass **13/13**. Real isolated **VS Code 1.141.0: 9/9** and
**VSCodium 1.143.1: 9/9** provider checks pass with the matching final Release server.
They exercise actual hover/signature/definition/reference/token providers, open
parameter hints and suggestion popups, type/generic completion, visibility filtering
and invalid-to-valid unsaved edits. This is provider/editor-host evidence, not a
manual visual or paint-latency measurement. User profiles and installations are untouched.

[Qualification JSON](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g80/qualification.json) records per-run source/artifact
hashes, raw outputs, commands, exact fixtures, evidence-reuse provenance and the
complete manifest. [Original unsuccessful evidence](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g80/unsuccessful-evidence.json)
retains unfavorable intermediate results. All 25 original G79 review/evidence files
still match their checksums. `git diff --check` passes; the index is empty.


## Qualification limits and cost

Native Windows/macOS remain **UNQUALIFIED**. Linux compiler/editor runs do not
qualify their native execution, layouts or editor hosts. No hosted CI or complete
repository-suite run is claimed. The original 1,048,576-byte stack limits and fatal
ASan/UBSan/leak policies are retained. Existing source diagnostics and expected
negative outcomes remain required failures.

There is no new isolated performance benchmark or claim of measured zero overhead.
A static access uses the established guarded global-cell path; a static call has
no receiver allocation/parameter. Focused/integrated elapsed times are test evidence,
not a compiler-performance comparison. No user-prefix tools/settings/installed
extensions were replaced, and nothing was staged, committed, pushed or published.

## Initial reviewed 31-file inventory (retained)

**31 paths: 16 modified, 15 new.** All belong to G80.

- `CMakeLists.txt`
- `docs/EDITOR_INTEGRATION.md`
- `docs/GENERICS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/README.md`
- `docs/RUNTIME_GLOBALS.md`
- `docs/STATIC_MEMBERS.md`
- `docs/projects/G80_STATIC_MEMBERS.md`
- `docs/projects/evidence/g80/baseline.json`
- `docs/projects/evidence/g80/qualification.json`
- `docs/projects/evidence/g80/unsuccessful-evidence.json`
- `editors/vscode/src/test/config.test.ts`
- `editors/vscode/src/test/staticHost.ts`
- `editors/vscode/syntaxes/void.tmLanguage.json`
- `examples/learn/static_members.void`
- `include/void/ir.hpp`
- `src/frontend/compiler.cpp`
- `src/frontend/generic_declarations.inc`
- `src/frontend/generic_tooling.inc`
- `src/frontend/generics.inc`
- `src/frontend/static_members.inc`
- `src/frontend/tooling.cpp`
- `src/ir/interfaces.inc`
- `src/ir/module.cpp`
- `tests/static_members/async.void`
- `tests/static_members/completeness.py`
- `tests/static_members/generics.void`
- `tests/static_members/lifecycle.cpp`
- `tests/static_members/lsp.py`
- `tests/static_members/ownership.void`
- `tests/static_members/values.void`


## Golden Wrench correction — concrete generic static results

The focused review at `/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/retained/g80-review-6lo8i4wl/REVIEW.md` identified a P2 defect:
`keep(utility.echo<i32>(7))`, its inferred-inner variant and a generic-owner variant
failed VOID2001 in all three profiles. Direct static calls and the equivalent
nested free-function control passed. All original review inputs, diagnostics,
checksums, the HOLD verdict and its successful controls remain unchanged.

### Root cause and minimal correction

The generic static selection's hint branch returned the source template's result
before argument binding and specialization. Its result could therefore still be
unresolved `t`, which surrounding inference correctly declined to treat as a
concrete argument type. Even explicit inner type arguments were ignored by that
premature return.

The only production change removes that early return in `static_members.inc`.
Hints now pass through the same existing argument mapping, inference and cached
concrete specialization used by checked static calls, then expose that concrete
result through the existing hint return below. This is also the established local
ordinary-call approach in `concrete_function`; the bounded upstream inference
reference from the review remains applicable. No new architecture/research survey,
Runtime storage, LLVM change, implicit conversion or private-access exception was
needed. Argument expressions are not lowered or executed by hints. The regression
checks execution counts and source order, shared reference identity and preserved
readonly rejection.

### Focused regression coverage

`static_members_generic_results` adds 15 compiler/application invocations per
profile, including check and JIT/AOT O0/O2. Its positive program covers ordinary
and generic managed/value owners, inferred/explicit inner arguments, named arguments,
multiple nested calls, free-function controls, function values, concrete list,
nullable and result returns, nominal reference identity and independent owner state.
Negative inputs retain rejection of excess/invented method arguments, wrong and
nominally distinct payloads, inaccessible private methods and readonly authority
loss. Their exact inputs/output are recorded in the correction JSON.

The existing actual-voidls probe additionally checks concrete `i32`/`string` hover
for inferred nested results and concrete parameter help for an explicitly selected
nested static generic call. Existing completion, visibility, semantic tokens,
navigation/references, namespace filtering and edited-document recovery assertions
remain intact.

### Correction qualification — current source

| Profile | Focused relevant suites | Elapsed |
| --- | --- | --- |
| Debug | **10/10** | 19.30 s |
| Release | **10/10** | 9.38 s |
| Fatal ASan/UBSan/leaks | **10/10** | 37.39 s |

All profiles pass the new **15-invocation** regression, existing **68-invocation**
static source suite, **46** native lifecycle checks, extended actual-voidls probe,
existing generic source/inference/editor tests and the unchanged semantic-depth,
source-CLI and runtime-global **1 MiB** stack tests. The seven original review
reproduction/control inputs now check successfully in each profile (**21/21**
checks total); this includes all three previously failing forms, without
reclassifying the original failures. Successful fixture output and expected
negative diagnostics/exits remain asserted in JIT/AOT O0/O2 paths.

The [current correction evidence](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g80/corrections.json) records exact
commands, inputs, outputs, final source/test and compiler/server hashes, original
review checksums and the complete inventory. All **17** original review records
remain byte-for-byte unchanged. Initial G80 qualification and its unfavorable
evidence JSON remain unchanged and historical; the current correction record
qualifies the final changed source. No unrelated complete-suite rerun was performed.
The production correction remains one removed early return plus its explanatory
comment; CMake and focused tests supply the additional qualification.

No isolated performance delta or fresh GUI/extension run is claimed for this
correction. The initial extension 13/13 and both real-editor 9/9 records remain
prior evidence; current assistance is verified through actual rebuilt voidls.
Native Windows/macOS remain UNQUALIFIED. The original 1,048,576-byte stack limits
and fatal sanitizer policies remain unchanged. Original review failures and all
prior unfavorable G80/G79 evidence are retained.

The working correction is ready for focused Golden Wrench re-review; this is not
a claim of independent integration acceptance. Nothing was staged, committed,
pushed, installed or published.

### Current exact changed-file inventory

**34 paths: 16 modified, 18 new.** The correction touches **7** paths relative to the reviewed candidate.

Correction paths:

- `CMakeLists.txt`
- `docs/projects/G80_STATIC_MEMBERS.md`
- `docs/projects/evidence/g80/corrections.json`
- `src/frontend/static_members.inc`
- `tests/static_members/lsp.py`
- `tests/static_members/nested_generics.py`
- `tests/static_members/nested_generics.void`

Complete current candidate:

- `CMakeLists.txt`
- `docs/EDITOR_INTEGRATION.md`
- `docs/GENERICS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/README.md`
- `docs/RUNTIME_GLOBALS.md`
- `docs/STATIC_MEMBERS.md`
- `docs/projects/G80_STATIC_MEMBERS.md`
- `docs/projects/evidence/g80/baseline.json`
- `docs/projects/evidence/g80/corrections.json`
- `docs/projects/evidence/g80/qualification.json`
- `docs/projects/evidence/g80/unsuccessful-evidence.json`
- `editors/vscode/src/test/config.test.ts`
- `editors/vscode/src/test/staticHost.ts`
- `editors/vscode/syntaxes/void.tmLanguage.json`
- `examples/learn/static_members.void`
- `include/void/ir.hpp`
- `src/frontend/compiler.cpp`
- `src/frontend/generic_declarations.inc`
- `src/frontend/generic_tooling.inc`
- `src/frontend/generics.inc`
- `src/frontend/static_members.inc`
- `src/frontend/tooling.cpp`
- `src/ir/interfaces.inc`
- `src/ir/module.cpp`
- `tests/static_members/async.void`
- `tests/static_members/completeness.py`
- `tests/static_members/generics.void`
- `tests/static_members/lifecycle.cpp`
- `tests/static_members/lsp.py`
- `tests/static_members/nested_generics.py`
- `tests/static_members/nested_generics.void`
- `tests/static_members/ownership.void`
- `tests/static_members/values.void`

`git diff --check` passes. The index remains empty; no Git integration or installation occurred.
