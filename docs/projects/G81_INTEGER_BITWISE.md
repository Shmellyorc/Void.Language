# G81 — General integer bitwise and shift operators

Accepted baseline: `58eac251838d6f6a63f55b92fd7818a11ed15e28` (G80).
Linux x86-64 implementation; LLVM/Clang/LLD 23.1.4 exclusively.
Integration remains subject to independent Golden Wrench source review and Boss
approval. Nothing is staged, committed, pushed, installed or published.

## Implemented contract

G78's integer AND/OR/XOR/complement and compound assignments are retained. G81
extends the same parser, semantic checker, typed IR, numeric constant kernel and
LLVM backend with `<<`, `>>`, `<<=` and `>>=`. All eight signed/unsigned widths
retain their exact result type. Counts can independently use any fixed-width
integer type; they never promote the value operand.

For an N-bit value, counts are modulo N, including negative and excessive counts.
Zero and width-multiple counts preserve the value. Left shifts discard high bits
and insert zeros; signed right shifts extend the sign bit and unsigned right
shifts insert zeros. Constant/runtime, JIT/AOT and O0/O2 share this policy.

[C#'s established masked-count and arithmetic-right-shift behavior](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/operators/bitwise-and-shift-operators) informed this
choice. VOID deliberately retains exact small widths rather than adopting C#
numeric promotions or bool operators. [Rust's overflow treatment](https://doc.rust-lang.org/reference/expressions/operator-expr.html#overflow) was considered;
profile-dependent exceptional shifts are not adopted. [LLVM shift instructions](https://llvm.org/docs/LangRef.html#shl-instruction) require masking to
prevent poison for out-of-range counts. This is bounded use of existing upstream
semantics, not a backport or dependency change. Links and applicability are in
[the research record](evidence/g81/research.json).

AND/OR/XOR continue requiring identical integer types or the same nominal flags
type. No enum shifts, bool/float shifts, implicit integer widening, custom shift
protocol, `>>>` or new literal bases are introduced. Nullable integers require
explicit extraction. These are deliberate language boundaries, not qualification
failures. See [the numeric contract](../NUMERIC_TYPES.md#integer-bitwise-and-shifts-g81).

## Implementation details

- The lexer recognizes three-character shift assignments. Expression parsing
  combines adjacent angle tokens into shifts, leaving nested generic closers
  intact. Shifts bind below addition and above AND, and associate left to right.
  The `with ... as name` cutoff moves with conversion precedence so resource
  binding retains its existing grammar.
- Shift typing uses the left value's context and independently types the count.
  Generic inference anchors the result to the left operand, never the count.
  Existing mutable-place checks and compound-assignment snapshots are reused.
  Compound lowering removes the entire assignment suffix instead of taking only
  the first glyph.
- Two typed IR operations are appended without renumbering existing operations.
  Validation requires integer operands, allowing different widths for counts,
  and rejects unsupported metadata/forged operands before LLVM lowering.
- Constant evaluation masks the raw count before constructing a value-width
  APInt. This handles both wider counts and negative two's-complement counts
  without an APInt bounds assertion.
- LLVM lowering masks in the original count type, converts the already-safe count
  to the value width, and emits plain `shl`, `ashr` or `lshr`. It never attaches
  `nsw`, `nuw` or `exact`. Existing validation and conservative ownership/use
  consumers remain applicable; no optimization pass or Runtime changes.
- Editor lexical grammar recognizes complete operator glyphs. Existing compiler
  metadata supplies exact-width hover/signature/completion information. No
  additional language-server analysis or scheduling subsystem is introduced.

## Examples and documentation

[The runnable example](../../examples/learn/integer_bitwise.void) packs/unpacks
RGB bytes, manipulates a permission mask and reads a big-endian two-byte value.
Its public helper has `## @param`/`## @return` source documentation.
Numeric/enum contracts, the Language Bible, the custom-operator boundary, the
feature-status record and documentation index describe the implemented behavior.

## Initial qualification (before the P2 correction)

The original evidence is retained unchanged. Final correction source/artifact
identities and focused gates are recorded in [corrections.json](evidence/g81/corrections.json).
The following initial results and historical source/artifact identities are recorded in
[qualification.json](evidence/g81/qualification.json). Raw commands, stdout,
stderr, generated inputs and unsuccessful runs remain in
`/tmp/g81-82_6glp5`; maintained regressions are under `tests/bitwise/`.

| Profile/check | Initial result |
| --- | --- |
| Debug selected integration gate | **27/27** |
| Release selected integration gate | **27/27** |
| Fatal ASan/UBSan + leak selected gate | **27/27** |
| G81 source/application checks | **44 per profile**, JIT/AOT O0/O2 |
| Dynamic JIT/constant-model/IR checks | **1,442 per profile**, O0/O2 |
| Extension tests | **14/14** (13 existing + operator grammar) |
| Real VS Code 1.141.0 | **8/8** |
| Real VSCodium 1.143.1 | **8/8** |
| `git diff --check` | **Pass** |

Source checks cover all eight value types against all eight count types, zero,
width boundaries, negative and excessive counts including u64 maximum, exact-width
complement/bitwise assignments, globals/fields/indexed updates, single evaluation,
generic specialization/inference and nested generic closers. Negative tests retain
readonly authority and reject bool/float/string/nullable/enum operands, invalid
generic specializations and implicit value-width conversion. Explicit constants
and independent arbitrary-precision/unsigned models agree with native execution.

Fatal checks use `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, with no leak
suppression options. Both compiler and native executables are exercised. The
selected gate includes the existing original 1 MiB stack tests without edits.
The matching source/test and executable hashes are retained in qualification JSON.

The selected integration gate covers numeric/frontend/tooling behavior, generic
inference, G78 enum/flags regressions, G79 nullability, G80 static members,
source CLI behavior and the original depth/stack tests. It is a proportionate
selected gate, not a full repository suite. Original 1 MiB requirements and
fatal sanitizer/leak settings are unchanged. No test timeout is extended.

## Preserved unsuccessful evidence

[The unsuccessful-evidence record](evidence/g81/unsuccessful-evidence.json) retains
paths, hashes and classifications. The existing pre-G81 Release artifact rejected
shift syntax as expected; this is syntax baseline evidence, not a performance
comparison. The first source fixture accidentally shadowed a function named
`count`, and the first LSP harness omitted semantic-token capabilities; both
harness failures remain recorded.

The first integrated Debug run was **24/27**, not a pass. The newly added resource
binding regression and existing `async_syntax_stack` both exposed the intermediate
conversion-precedence cutoff; the cutoff was corrected without changing the
`with` contract. The IR rejection fixture also initially omitted its module
identity and therefore reached an earlier validation error. It now supplies a
valid identity and requires the specific integer-shift validation error. The
original logs are retained separately from final results.

## Performance and qualification limits

No comparative benchmark was requested or run. Dynamic lowering adds a bounded
integer mask and, when widths differ, an integer conversion; there is no Runtime
call, allocation or new pass. Constant counts can fold through existing LLVM
optimization. Test durations are verification timings, not production-performance
measurements. No unrelated performance acceptance is claimed.

Native Windows and macOS execution remain **UNQUALIFIED**. Linux qualification
cannot establish their compiler/editor behavior. Real-editor checks use isolated
VS Code/VSCodium development profiles and the candidate server; installed tools,
user settings and other extensions are preserved. They verify provider behavior
and trigger editor popups; no pixel-paint latency or manual visual acceptance is
claimed. No hosted CI or independent-review acceptance is claimed by this report.

## P2 correction — shift-aware pattern lookahead

Golden Wrench's original review (`/tmp/g81-independent-r5o6_n4b/REVIEW.md`)
identified a concrete missing parser integration: pattern lookahead examined one
angle token, so a call followed by `<<`/`>>` looked like a completed constructor
pattern. A named `bits << 1 >> 1` expression could also look like generic brackets.
The original review, sources, failing outputs and initial 25-file candidate
fingerprints remain in `/tmp/g81-independent-r5o6_n4b`; correction evidence is in
`/tmp/g81-pattern-correction-_abtwv__`.

The correction makes the existing `binary_operator` helper accept an optional
absolute token cursor. Ordinary generic-call lookahead and both pattern lookahead
sites now use that same adjacent-token recognition. `<<` is excluded from a generic
argument-list opening, and call-tail precedence recognizes the complete shift.
The existing head scan also follows qualified generic owners/methods before
checking the final call tail. Actual type parsing and generic constructor/enum
pattern binding are unchanged.
There is no new parser, tokenization policy, lowering, Runtime or ownership system.

The [C# lexical specification](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/lexical-structure#625-grammar-ambiguities)
describes context-sensitive angle-token/generic disambiguation. Its applicable
practice is consistent token-aware lookahead across parser consumers. VOID retains
its existing type-argument and pattern grammar; no upstream implementation is
backported. Earlier integer/LLVM research remains applicable without repetition.

The maintained `tests/bitwise/patterns.void` and `.py` regression covers ordinary
and generic calls (including generic owners and method-specific type arguments)
with left/right shifts in `match`/`is`, named shift chains,
adjacent glyphs, evaluation counts, nested generic payload/whole-enum constructors,
generic numeric/flags patterns and nested generic function arguments. Negative
cases preserve rejection of separated glyphs, unsupported shift operands,
nongeneric type arguments, incorrect arity and different nominal enum types.
The test is registered as `integer_bitwise_patterns`.

| Final correction check | Debug | Release | Fatal ASan/UBSan + leaks |
| --- | --- | --- | --- |
| Focused suites on final source | **14/14** | **14/14** | **14/14** |
| Original review reproductions/controls | **11/11** | **11/11** | **11/11** |
| Additional qualified-owner reproduction | **Accepted** | **Accepted** | **Accepted** |
| New pattern regression invocations | **18/18** | **18/18** | **18/18** |
| Existing G81 source/application checks | **44/44** | **44/44** | **44/44** |
| Existing JIT/constant-model/IR checks | **1,442** | **1,442** | **1,442** |

New regression execution covers JIT and native AOT at O0/O2 in every profile.
All five originally rejected forms now compile successfully; original controls
and reproducer files remain unchanged. Both qualified generic-owner and
method-generic calls execute correctly. Current source/test/executable hashes,
individual test names/results, original-review hashes and unsuccessful attempts
are recorded in [corrections.json](evidence/g81/corrections.json).
`git diff --check` passes; nothing is staged.

The focused gate includes the new regression, existing G81 integer/model/LSP
checks, pattern source/CLI suites, generic and G78 pattern corrections, and all
four previously selected source/depth/stack gates. The original 1 MiB sources and
limits, existing timeouts and fatal sanitizer/leak policy are unchanged. This is
focused requalification, not a repeated full repository suite. The original
27/27 and editor/extension evidence above remains historical; the current parser
and server are covered by the correction gates and identities. Build paths are
reused for rebuilt executables: the initial hashes identify earlier artifacts,
while correction hashes identify the current binaries.

A boundary check after the first minimal correction exposed a qualified-owner
form, `factory<i32>.one() << 1`, whose call tail was beyond the initial head scan.
That failing source/log and the preliminary parser diff remain preserved. The
final correction follows the full qualified head using the same balanced
type-argument scan, without changing actual type parsing. Earlier Debug/Release
14/14 runs describe that preliminary state; final results above identify the
qualified-owner correction as well.

Two preliminary new-test failures are also preserved: the first fixture placed a
compound generic flags value directly in constructor-pattern position and was
corrected to an explicit parenthesized value pattern; the next harness assertion
expected generic type-mismatch wording instead of the existing precise
`different nominal type` diagnostic. Neither change relaxes an existing test or
changes compiler semantics. Raw input/output records retain both attempts.

The correction changes lookahead only. No comparative performance benchmark is
claimed. Native Windows/macOS remain UNQUALIFIED; real-editor GUI tests were not
repeated for this parser-only correction. Focused LSP checks use the final rebuilt
server. Golden Wrench focused re-review is required before integration; no staging,
commit, push, installation, publication or hosted CI has occurred.

## P1 correction — imported generic static owner selectors

Golden Wrench's focused re-review (`/tmp/g81-focused-rereview-voo1crah/REVIEW.md`)
confirmed the P2 correction and exposed a separate frontend crash. An imported
owner such as `api.factory<i32>` is a field expression whose one child is the
module qualifier. Static type resolution passed that selector to shared
constructor inference with argument offset zero; the constructor loop interpreted
the qualifier as an argument and accessed an empty argument-name vector.
Debug aborted, Release segfaulted, and fatal sanitizers diagnosed the invalid
reference. Original source, outputs and inspected backtraces remain untouched.
Both affected functions matched G80 before this correction; **baseline executable
reproduction was not performed and regression attribution remains unconfirmed**.

The minimal production repair is in `static_members.inc::static_type`: pass the
selector's child count as the existing constructor-inference argument offset.
Selectors have no constructor arguments, so this excludes qualification children
while retaining explicit owner-type validation and the existing specialization
pipeline. Local selectors still use offset zero. The shared `concrete_structure`
algorithm, actual constructor callers, member lookup, storage, GC, readonly
checks, IR and native ABI are unchanged. No assertion or sanitizer check is
suppressed.

The [C# generic-method documentation](https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/generics/generic-methods)
and [Roslyn qualified-name syntax handling](https://github.com/dotnet/roslyn/blob/main/src/Workspaces/CSharp/Portable/CodeGeneration/CSharpSyntaxGenerator.cs)
reinforce the distinction between owner qualification/type arguments and method
arguments. They are bounded architectural references; no upstream patch or new
framework is adopted. VOID uses its existing argument-offset machinery.

Maintained regressions are `tests/static_members/imported_owners.void`,
`imported_owners_library.void` and `imported_owners.py`, registered as
`static_members_imported_owners`. Positive cases cover imported/local managed and
value owners, owner and method generics, nested inference, zero-argument calls,
method references, per-specialization static state, readonly static reads,
constructor inference and the repaired G81 shift patterns. Negative cases require
normal source diagnostics for absent/extra type arguments, nongeneric owners or
methods, wrong payloads, private access, readonly mutation, instance selection and
invalid constructor arguments.

| Final P1 correction check | Debug | Release | Fatal ASan/UBSan + leaks |
| --- | --- | --- | --- |
| Focused suites on final source | **19/19** | **19/19** | **19/19** |
| Original crash/application invocations | **14/14** | **14/14** | **14/14** |
| New imported-owner regression invocations | **20/20** | **20/20** | **20/20** |
| Preserved G81 pattern regression invocations | **18/18** | **18/18** | **18/18** |
| Preserved G81 integer source invocations | **44/44** | **44/44** | **44/44** |
| Preserved JIT/constant-model/IR checks | **1,442** | **1,442** | **1,442** |
| Fresh imported-owner LSP probes | **5/5** | **5/5** | **5/5** |

The two exact original crashing sources now check, run and build successfully,
with original expected outputs and exits, under JIT and native AOT O0/O2 in each
profile. Their source files and original failure records are unchanged. New LSP
probes verify diagnostics, concrete hover, completion and invalid-to-valid edits.
The focused gates include existing G80 static lifecycle, nested generic results,
static source/LSP suites and the original depth/stack requirements. Fatal sanitizer
and leak options remain unchanged, with no suppression options. No existing
1 MiB limits or timeouts are relaxed. `git diff --check` passes; the index is empty.

The raw correction directory is `/tmp/g81-imported-owners-ln5fgt75`. Existing
qualification JSON and P2 correction JSON remain unchanged and historical; latest
source/test/executable identities and focused evidence are in
[imported_owners.json](evidence/g81/imported_owners.json). Build paths are reused.
The original crash review and all unfavorable evidence remain preserved. The
first new fixture reused a project-root-visible type name in its imported module,
violating the existing shadowing contract. Its duplicate-declaration/unknown-member
cascade, fixture text and inspection logs are retained; the local control was
renamed without changing production lookup or relaxing a test.

No full-repository suites, performance benchmarks or real-editor GUI reruns were
commissioned. Fresh LSP probes use the final servers and the original imported
reproducer. Native Windows/macOS remain UNQUALIFIED. Independent focused re-review
is pending. No staging, commit, push, installation, publication or hosted CI.

## Exact changed-file inventory

34 files: 15 modified, 19 new. The P1 correction touches 7 paths; the earlier P2 correction is preserved.

- `CMakeLists.txt`
- `docs/ENUMS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/NUMERIC_TYPES.md`
- `docs/README.md`
- `docs/projects/G81_INTEGER_BITWISE.md`
- `docs/projects/evidence/g81/corrections.json`
- `docs/projects/evidence/g81/imported_owners.json`
- `docs/projects/evidence/g81/qualification.json`
- `docs/projects/evidence/g81/research.json`
- `docs/projects/evidence/g81/unsuccessful-evidence.json`
- `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`
- `docs/stage10/OPERATORS.md`
- `editors/vscode/src/test/bitwiseGrammar.test.ts`
- `editors/vscode/src/test/bitwiseHost.ts`
- `editors/vscode/syntaxes/void.tmLanguage.json`
- `examples/learn/integer_bitwise.void`
- `include/void/ir.hpp`
- `src/backend/llvm/backend.cpp`
- `src/frontend/compiler.cpp`
- `src/frontend/pattern_parser.inc`
- `src/frontend/static_members.inc`
- `src/ir/module.cpp`
- `src/numeric.cpp`
- `tests/bitwise/completeness.py`
- `tests/bitwise/completeness.void`
- `tests/bitwise/editor.void`
- `tests/bitwise/lsp.py`
- `tests/bitwise/patterns.py`
- `tests/bitwise/patterns.void`
- `tests/bitwise/runtime.cpp`
- `tests/static_members/imported_owners.py`
- `tests/static_members/imported_owners.void`
- `tests/static_members/imported_owners_library.void`
