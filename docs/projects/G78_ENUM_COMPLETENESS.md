# G78 — Enum completeness and flags

Baseline: `2376e123026e5dd17b9743b6a7b6af2b0a1fd94f`.
Status: implemented and locally qualified for focused review. This is implementation
qualification, not an independent review or hosted acceptance. Nothing is staged,
committed, pushed, installed or published. Existing G77 user tools are unchanged.

## Result and architecture

The implementation extends the existing parser, semantic checker, generic
specializer, typed IR and LLVM backend. It adds no dependencies, alternate type
system, Runtime ownership protocol, native ABI or FFI authority.

- **Tagged equality:** private generated ordinary functions test the active case
  and compare only its payloads using ordinary `==`. Different cases are unequal;
  matching payload-free cases are equal. Operands evaluate once and comparisons
  short-circuit. Existing explicit enum/payload operators, managed identity,
  strings and IEEE comparisons retain their rules. Unavailable payload equality
  diagnoses at the original operator. `!=` negates the same equality; `is`/`match`
  remain available. Canonical option/result and unrelated value structs do not
  silently gain equality. Constants support built-in payload evaluation; user
  operators remain runtime calls under the existing constant-call restrictions.
- **Dictionary keys:** numeric/flags enums and ordinary tagged enums whose
  payloads satisfy existing key eligibility reuse the current schemas, hashing
  and canonical zero-inactive storage. Tags distinguish cases; strings hash by
  content and managed references by identity. Floats, collections, interfaces,
  custom-equality owners/payloads and canonical option/result remain excluded.
  Dictionary assignment still shares identity; enum/value fields keep their
  existing copying and child-reference ownership rules. No Runtime file changed.
- **Numeric conversion:** integer `as`/`as?` to ordinary enums checks mathematical
  bounds and declared discriminants. Flags require bounds but accept unnamed
  combinations. Enum `.value` extracts the exact backing type; enum `as`/`as?`
  integer performs checked conversion. Distinct enums require an explicit integer
  intermediate. No implicit integer/enum conversions or enum arithmetic are added.
- **Flags:** contextual `flags enum`, default `u32` or explicit integer backing,
  automatic power-of-two allocation, earlier-member bitwise constants and exact
  width validation. All backing bit patterns are valid, including unnamed values.
  `&`, `|`, `^`, `~` and `&=`, `|=`, `^=` use typed integer/flags IR. Ordinary enums,
  bools and floats remain ineligible. Flags matches cannot claim exhaustiveness
  from declared members alone. No shifts or general custom bitwise protocol are
  introduced. Existing `with … as …` grammar retains its postfix-only boundary.
- **Generics:** enum templates reparse and specialize through the existing generic
  structure machinery. Concrete payload construction, payload inference, matching,
  generic functions and collections retain nominal specialization identity.
  Payload-free construction requires explicit type arguments outside patterns;
  a concrete match subject supplies contextual pattern arguments. Numeric/flags
  specializations retain their correct nominal type kind.
- **Assistance:** `.name`, numeric `.value`, `is_defined()`, eligible static
  `values()`, and immutable flags `has`, `has_any`, `with`, `without`, `toggled`
  expose compiler-owned parameter/return documentation through the same editor
  documentation model as `##` comments. Declared cases/methods retain precedence.
  Generated helpers are hidden from source declarations/references and cannot
  create phantom hover/coloring. Physical examples use `##`, `## @param` and
  `## @return`; generic case documentation and concrete signatures are tested.
  Contextual grammar coloring preserves ordinary identifiers named `flags`.

[Enums and flags](../ENUMS.md) is the current language guide. The Bible, numeric,
generic, dictionary, diagnostics and editor guides are updated. G18 is explicitly
marked historical, with its original evidence retained. The runnable example is
`examples/learn/enums.void`; its verified output is `data` then `read and write`.

## Upstream research and applicability

[C# enum guidance](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/builtin-types/enum),
[flag design guidance](https://learn.microsoft.com/en-us/dotnet/standard/design-guidelines/enum)
and [System.Enum](https://learn.microsoft.com/en-us/dotnet/fundamentals/runtime-libraries/system-enum)
provide established typed bitmask, zero-mask and name/value conventions. VOID
keeps checked conversions and nominal identity, including rejection of implicit
integer zero conversions. It deliberately gives unnamed flags a predictable
numeric name instead of inventing composite display strings.
[Rust equality](https://doc.rust-lang.org/core/cmp/trait.PartialEq.html) and
[derived traits](https://doc.rust-lang.org/book/appendix-03-derivable-traits.html)
support active-payload comparison and eligibility determined by payload operations.
VOID uses its existing operator checker rather than importing Rust's trait system.
These are published upstream contracts, not proposed patches or frameworks.

[LLVM APInt](https://llvm.org/doxygen/classllvm_1_1APInt.html) documents explicit
truncation behavior. The local pinned LLVM **23.1.4** header was checked directly:
its integer constructor defaults `implicitTrunc` to false. A new constant-complement
prototype supplied an oversized 64-bit mask for narrow backing types; the repair
constructs a width-fitting mask before APInt evaluation. Runtime and constant
complements now agree for all eight backings. No upstream LLVM patch is needed.

The earlier enum/flags handoff discussion is advisory; the Boss's G78 instruction
is the current authorization. Research supported concrete decisions rather than
expanding the milestone into another compiler architecture.

## Initial-candidate verification

All compiler profiles use pinned LLVM/Clang/LLD 23.1.4 on Linux x86-64.
[Machine-readable evidence](evidence/g78/qualification.json) records exact tests,
source/binary hashes, timings and final outcomes. Full local logs remain in
`/tmp/void-g78-484t4_vc`.

| Gate | Final result | Scope |
| --- | --- | --- |
| Release | **25/25** | Selected semantic, ownership, IR, cache, editor and stack gates |
| Debug | **25/25** | Same accepted sources and tests |
| Fatal ASan/UBSan/leaks | **25/25** | Same selected set; all fatal policies retained |
| Extension | **13/13** | Config, contextual grammar, receipts and diagnostic-version tests |
| Real VS Code 1.141.0 | **7/7 observations** | Isolated source-extension host, current Release server |

The 25 selected gates are enumerated in the evidence. They cover G78's maintained
process/LSP tests; existing numeric enum process/IR/LSP tests; frontend/numeric;
generic source/hints; dictionaries, patterns, advanced ownership; four Stage 10
operator gates; enum/editor recovery; raylib parity/documentation; and the original
`text_encoding_semantic_depth` and `source_cli`. This is **not** a claim that every
repository test was rerun.

`enum_completeness` executes **60** compiler/application invocations per profile:
active-case/custom/IEEE/managed equality, dictionary keys and identity, sparse
inactive enum storage, generic matching/functions/collections, flags metadata,
all eight widths, JIT/AOT O0/O2, compile-time rejection and checked runtime failures.
Expected negative results retain their diagnostic/exit classifications. The IR
gate adds malformed bitwise operands/metadata and out-of-width enum rejection;
existing storage audit reports **24 negative, 12 positive, zero mismatches**.
A new low-threshold Runtime stress checks enum dictionary string/managed payloads,
identity equality and complete heap cleanup under both normal and optimized JIT.
Existing numeric enum GC/reload/cache gates continue passing.

The 1 MiB frontend stack limits, timeouts and failure policies are unchanged.
Strict execution used:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

The source extension was exercised through real VS Code providers and editor
commands: documented hover, concrete generic payload hover, active parameter and
parameter docs, unsaved flags completion, invalid-to-valid repair, semantic tokens,
and parameter/suggestion commands. The test profile/settings were temporary and
separate from the user's installed extension. No popup-paint latency is claimed.

## Preserved failures and corrections

[Baseline evidence](evidence/g78/baseline.json) retains the five initial rejected
feature probes and compiler identity. [Unsuccessful evidence](evidence/g78/unsuccessful-evidence.json)
retains prototype failures; none have been replaced with successful summaries.

- A payload-free generic pattern lost its contextual specialization. Pattern
  resolution now selects the subject's concrete enum; explicit nominal checks
  remain. The generic pattern/collection probe passes.
- Narrow constant complement produced an all-64-bit mask. Width-fitting APInt
  operands fix this without relaxing enum storage validation.
- Numeric `.value` lacked a literal anchor, rejecting `u8` extraction compared to
  an integer literal. The field anchor now exposes its true backing type.
- Synthetic helper declarations/locals polluted source hover. Explicit helper
  classification hides their presentation while retaining compilation/contracts.
- An intermediate IR check incorrectly treated the unused Instruction type field
  as an operation result annotation. The established IR contract derives these
  results from operands; malformed-operation tests target actual operand/metadata
  invariants instead.
- Initial Debug/Release integration was **24/25**. The unchanged raylib generator's
  final physical-name assertion checked retired ABI spellings instead of the
  canonical public-name metadata established before G78. Its one-line correction
  uses `public_name` with the existing fallback. Regeneration/parity then passes;
  no binding, public API, generated output or ABI identity changed.
- Initial strict integration was **23/25**: enum process/IR rejected valid bitwise
  operations. The module object had been compiled during an edit and retained the
  intermediate validator. Refreshing it from the frozen source resolved the
  reproducer without a source or sanitizer-policy change; the full selected strict
  set then passed **25/25**. Before/after source SHA was identical
  (`926e275b1df57780f16602eef529ef4b4c453c53abe4c48f8369ae7b69b7225f`),
  while object/compiler hashes changed. Original logs, unsuccessful debugger
  inspection, hashes and before/after disassembly are retained. Leak detection was
  disabled only for ptrace inspection, never for qualification.
- Harness mistakes are also retained: reserved fixture name `event`, incorrect
  assumption of independent dictionary assignment, pattern captures used outside
  their scope, AOT given a JIT-only flag, missing semantic-token capability
  negotiation, expected custom-key diagnostic text, and C++/TypeScript build
  errors. The first extension test selected an old default build server; explicit
  `VOIDLS_TEST_SERVER` selects this candidate. Editor assertions initially
  serialized hover wrappers as `{}` and overlooked VS Code's plaintext-to-Markdown
  escaping/`&nbsp;`; inspecting the actual client/editor conversion fixed the
  assertions. No product result was fabricated to satisfy those probes.

Release/Debug acceptance combines the original unchanged passing cases with the
corrected parity check and final expanded enum-test reruns. The strict final run
is a complete rerun of the same selected 25 gates. Earlier unfavorable runs remain
visible in both raw and checked-in evidence.

## Performance and qualification limits

No LLVM optimization policy or Runtime optimization was changed.

| Local measurement | Median | p95 | Worst |
| --- | ---: | ---: | ---: |
| New flags fixture, real VS Code completion provider, 20 warm samples | 4.65 ms | 24.74 ms | 26.42 ms |
| 300-member enum, G77-equivalent warm LSP, 31 samples | 10.208 ms | 10.816 ms | 11.063 ms |
| Same input, G78 warm LSP, 31 samples | 10.207 ms | 11.150 ms | 11.303 ms |
| Same input, old installed compiler whole-process `check`, 31 paired samples | 7.723 ms | 8.085 ms | 8.252 ms |
| Same input, G78 whole-process `check`, 31 paired samples | 9.117 ms | 9.374 ms | 9.431 ms |

The unfavorable whole-process delta is retained: **+1.395 ms / +18.1% median**.
A `--version` startup control also differs by **+1.036 ms** (4.152 → 5.188 ms),
and minimal-file check by +1.205 ms (5.090 → 6.296 ms). These are informational
process measurements against installed G77 packaging/paths, not an isolated
frontend attribution. `src`/`include` are unchanged between G77 and the requested
baseline, but the installed binary has installation RUNPATH changes. Warm LSP
median is unchanged; p95 differs by +0.334 ms. Do not infer raylib popup paint or
all-project responsiveness from the new small flags fixture. No detailed timing
instrumentation was enabled. Raw samples and concurrent-qualification caveats are
recorded; no outlier was discarded after warmups.

**Windows and macOS are unqualified:** no native tests ran. Linux results do not
establish their filesystem, ABI, editor or sanitizer acceptance. Existing unrelated
G77 native graphics/audio leak limitations remain historical failures, not claimed
passes here; G78 did not modify those paths or rerun device qualification.

## Focused corrections following Golden Wrench review

The Boss authorized correction of the three confirmed P2 findings within G78.
The original review, inputs and failures remain under
`/tmp/g78-independent-3p_fu0sn`; correction logs are under
`/tmp/g78-corrections-apqyd6kg`. The original qualification above is retained as
historical initial-candidate evidence. Current correction evidence is recorded in
[evidence/g78/corrections.json](evidence/g78/corrections.json).

### Exact corrections

1. **Generic patterns:** a shared enum-pattern resolver uses ordinary name lookup
   and the existing generic specializer before deciding whether a path denotes a
   whole enum or a member. It verifies the exact subject definition, preserves
   contextual specialization and rejects type arguments on nongeneric enums.
   Numeric/flags member patterns become the same typed literals used by existing
   member comparisons. A parser bit distinguishes member values from calls, so
   `code<i32>.ok()` remains rejected. Whole-value patterns and module-qualified
   patterns now accept the correct concrete enum type. No generic system or new
   source pattern form was introduced.
2. **Callable constants:** callable identity is evaluated before ordinary
   aggregate traversal, matching the already-supported runtime function-value
   identity. Named function values and nullable callable values compare without
   invoking those functions. Custom operators and user calls remain excluded from
   compile-time evaluation.
3. **Editor metadata:** generic enum templates and their members publish enum
   symbol/token kinds with symbolic declaration signatures. Payload parameters
   have parameter records rather than exposing synthetic storage fields.
   Existing reference presentation supplies each use's concrete signature; the
   existing multi-specialization presentation remains available for generic
   bodies. Declaration records no longer depend on the first instantiation.

The focused negative checks exposed two adjacent defects in these same paths,
reproduced with the pre-correction compiler and preserved before repair:

- A generated case test for a case named after its enum could be mistaken for a
  whole-type test. Synthetic case tests now bypass source whole-type resolution;
  their active tags retain their actual semantics. Equality, name inspection and
  case identity have explicit regression checks.
- An invalid runtime function call inside an enum constant was diagnosed, but
  its enclosing expression retained a usable enum/bool type. Constant evaluation
  then indexed the invalid zero-argument call: original Debug aborted, and Release
  segfaulted. Constant analysis now respects the semantic checker's existing poison
  flag before evaluation. It returns the original source diagnostic and exit 1;
  no exception swallowing, signal handling or disabled checks were added.

### Research and applicability

[The Rust pattern reference](https://doc.rust-lang.org/reference/patterns.html#patterns.ident.precedent)
illustrates resolving a path's meaning through name lookup rather than guessing
from spelling. VOID reuses its own lookup/specialization and nominal rules.
[The constant-context reference](https://doc.rust-lang.org/reference/const_eval.html#const-eval.const-expr.path-item)
distinguishes function values from function calls. VOID's established callable
identity and compile-time-call restrictions remain authoritative; no Rust const-fn
policy, native pointer comparison policy or framework was imported. Editor roles
use the existing [LSP symbol model](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#symbolKind).

### Current qualification

| Current correction gate | Result |
| --- | --- |
| Release | **25/25** |
| Debug | **25/25** |
| Fatal ASan/UBSan/leaks | **25/25** |

All three profiles run the same accepted fixtures. Each includes the 23 new
compiler/application checks and the new LSP regression alongside the retained
semantic/ownership/IR/stack gates.

The exact preserved review source fixtures were also replayed with all three
repaired compiler profiles: **42/42 invocations reached their expected outcomes**,
including rejection of the invented nongeneric type arguments. Their original
failure records remain separate from these replay results.

The maintained `enum_review_corrections` test performs **23** compiler/application
checks per profile, including JIT/AOT O0/O2. Positives cover qualified and contextual
numeric/flags members, whole enums, captures, active tags and named/nullable
callable identity. Negatives preserve nominal mismatch, invented type arguments,
member-call rejection and compile-time user-call/custom-operator restrictions.

`lsp_enum_review_corrections` checks enum symbols/tokens and symbolic case/type
hover before any instantiation, after two concrete instantiations in both orders,
and after invalid-to-valid unsaved edits. It separately verifies concrete use
hover, active-parameter help and parameter documentation. The initial signature
probe targeted a numeric literal, where existing assistance deliberately returns
no result; the maintained assertion uses an identifier argument. The original
probe is retained, and no literal-assistance policy was changed.

The current 25-test selection retains frontend/numeric, G78's original process and
LSP tests, numeric enum IR/process/LSP, generic source/hints, dictionaries, patterns,
advanced ownership, four operator gates, editor/enum recovery and the original
1 MiB `text_encoding_semantic_depth`/`source_cli` checks, plus the two new tests.
The unchanged raylib generator/documentation checks retain their valid earlier
evidence. No complete repository suite, installation or extension publication was
performed. The prior 13/13 extension tests and seven real-editor observations are
retained; these corrections change compiler-owned metadata, and their additional
editor qualification uses the real LSP server rather than claiming a new renderer
or popup-paint run.

Fatal ASan/UBSan/leak settings, the original 1 MiB requirements, existing timeouts,
LLVM 23.1.4 and native ABI/security rules are unchanged. Production files were
frozen before the final profile builds; recorded hashes are checked afterward.

### Measurements, retained evidence and limits

A bounded repeat of the same 300-member warm LSP probe recorded corrected G78 at
**11.145 ms median / 11.692 ms p95 / 11.809 ms worst** (31 post-warmup samples).
The source-equivalent installed G77 control recorded 10.625 / 12.800 / 13.472 ms.
The unfavorable median difference (+0.520 ms, approximately 4.9%) is retained;
these samples ran alongside qualification and do not establish an isolated
correction regression or replace the earlier packaging/startup qualification.
No backend/Runtime optimization or new performance target was introduced.

One initial correction check accidentally reused the review harness's output
path, replacing its editor JSON with a corrected result. That corrected output
was saved separately. The original review Markdown and compiler result records
are unchanged; the old Debug server freshly reproduced the original editor
failures, and its result is retained with an explicit provenance note. The new
record does not claim that reproduced JSON is the original byte-for-byte file.
Subsequent harness outputs use only the correction directory. Original failures,
the new invalid-call crash/backtrace, failed literal-position probe and successful
correction results are retained in the evidence.

Windows/macOS remain **UNQUALIFIED**. No native execution, installation or
publication occurred on those platforms. No new genuine blocker requires Boss
approval; all listed current gates pass and the corrected candidate is ready for
focused independent re-review.

## Final source and Git checks

`git diff --check` passes. The index remains empty; HEAD remains the authorized
baseline. No user installation, settings, extension or unrelated worktree was
modified. Generated build products stay under ignored build/output directories;
the evidence files below are intentional documentation artifacts.

The correction touches these **12 paths** (the original 42-file implementation
is preserved):

- `CMakeLists.txt`
- `docs/EDITOR_INTEGRATION.md`
- `docs/ENUMS.md`
- `docs/projects/G78_ENUM_COMPLETENESS.md`
- `docs/projects/evidence/g78/corrections.json`
- `src/frontend/compiler.cpp`
- `src/frontend/enum_constants.inc`
- `src/frontend/generic_tooling.inc`
- `src/frontend/pattern_parser.inc`
- `src/frontend/pattern_semantics.inc`
- `tests/enums/corrections.py`
- `tests/enums/corrections_lsp.py`

Exact final G78 inventory: **46 paths** (26 modified, 20 new). The original
42-path inventory remains in the correction evidence.

- `CMakeLists.txt`
- `docs/ADVANCED_LANGUAGE.md`
- `docs/DIAGNOSTICS.md`
- `docs/DICTIONARIES.md`
- `docs/EDITOR_INTEGRATION.md`
- `docs/ENUMS.md`
- `docs/GENERICS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/NUMERIC_TYPES.md`
- `docs/README.md`
- `docs/developer-experience/G18_ENUM_EQUALITY_DIAGNOSTICS.md`
- `docs/projects/G78_ENUM_COMPLETENESS.md`
- `docs/projects/evidence/g78/baseline.json`
- `docs/projects/evidence/g78/corrections.json`
- `docs/projects/evidence/g78/qualification.json`
- `docs/projects/evidence/g78/unsuccessful-evidence.json`
- `editors/vscode/src/test/config.test.ts`
- `editors/vscode/src/test/enumsHost.ts`
- `editors/vscode/syntaxes/void.tmLanguage.json`
- `examples/learn/enums.void`
- `include/void/ir.hpp`
- `src/backend/llvm/backend.cpp`
- `src/frontend/compiler.cpp`
- `src/frontend/enum_api.inc`
- `src/frontend/enum_constants.inc`
- `src/frontend/enum_equality.inc`
- `src/frontend/enum_parser.inc`
- `src/frontend/generic_tooling.inc`
- `src/frontend/generics.inc`
- `src/frontend/pattern_parser.inc`
- `src/frontend/pattern_semantics.inc`
- `src/ir/module.cpp`
- `src/numeric.cpp`
- `tests/enums/completeness.py`
- `tests/enums/contracts.void`
- `tests/enums/corrections.py`
- `tests/enums/corrections_lsp.py`
- `tests/enums/generic.void`
- `tests/enums/lsp.py`
- `tests/enums/metadata.void`
- `tests/enums/payloads.void`
- `tests/frontend/enum_equality.py`
- `tests/lsp/enum_equality.py`
- `tests/numeric/enums.py`
- `tests/numeric/enums_ir.cpp`
- `tools/raylib/generate.py`
