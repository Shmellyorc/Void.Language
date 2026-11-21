# G93 — Pure compile-time function evaluation

21 November 2026. Corrected candidate for focused independent Golden Wrench re-review.
Accepted baseline: `f54362ce55e15d298690d75060d20d512e86c553` (G92), initially clean.
No branch creation, staging, commit, push, installation or publication is authorized
or performed. Local qualification is distinct from independent acceptance.

## 1. Implemented source syntax

```void
compile func square(value: i32) -> i32:
    return value * value

value struct dimensions:
    width: i32
    height: i32

compile func (d dimensions) area() -> i32:
    return d.width * d.height

const answer = square(8)
const pixels = dimensions(width = 20, height = 10).area()

func main():
    var width = 9
    print(answer, pixels, square(width))
```

The existing visibility modifier precedes `compile func`, for example
`public compile func`. Generic functions, generic value receivers and method type
parameters use ordinary syntax and specialization. The
[runnable example](../../examples/compile_functions/main.void) prints
`64 200 5050 81`; run it with `voidc run examples/compile_functions/main.void`.
There is no alternate `comptime`, `compile const`, `compile var`, compile type,
macro, reflection, source generation or native execution facility.

G87 source selection precedes parsing and generic capture. `compile func` passes
through the selector as an ordinary declaration; `compile if`/`compile else` retain
their existing region behavior and original byte positions. VOID's predicate
spelling remains `target_os == "linux"`, not `target.os`. Inactive functions and
imports remain absent; package conditions belong to the owning manifest.

## 2. Evaluation and purity semantics

`compile func` has one ordinary typed body and one ordinary Runtime implementation.
The existing parser, declaration resolver, argument/default mapper, generic
specializer, body checker and numeric/constant kernels remain authoritative.
`compile_functions.inc` adds owned local activation storage and statement control
flow around `constant_value`; it neither generates source nor executes JIT/native
code. Calls carry their ordinary resolved declaration and source owner. Generic
bodies are reparsed by the existing specializer, not a compile-specific parser.

Every concrete marked body is checked, even when unused or containing an
unselected runtime branch. An uninstantiated generic retains the established
concrete-operation checking policy. Its instantiated body must satisfy the same
contract even when requested only by an ordinary runtime caller.

Supported body operations:

- Scalar, string, numeric/tagged enum and eligible pure value/collection templates;
  ordinary numeric, bitwise, conversion, string and equality operations already
  supported by the typed constant kernel, including numeric `abs`/`clamp`.
- Owned locals, scalar/value/fixed-array assignment, branches, conditional
  expressions, conditional loops, inclusive/exclusive/reversed ranges and fixed
  array iteration, `break`/`continue`, recursion and nested marked calls.
- Pure named-field value construction and field access; returns, named/unnamed
  multiple returns and ordinary local destructuring.
- Statically selected pure value receiver methods. Their receiver remains
  readonly. Existing ownership, visibility and readonly capability analysis also
  applies to their ordinary Runtime callers.

Rejected operations include globals (reads and writes), unmarked calls, native
bindings, observable I/O, tasks/await/spawn, closures/function-value storage,
managed/interface/native-resource identities, custom initialization/operators, nominal display adapters,
virtual/lifecycle/property effects, patterns, `with` and `defer`. Collection
mutation through reference storage and property assignment are rejected. Returning
an owned constant template does not return a pointer into a compiler frame.
Compiler collection templates use host-owned constant values and the existing
Runtime materialization rules; evaluation never acquires a Runtime managed object.
Text/collection APIs absent from the existing constant kernel remain unsupported.

All numeric operations use the existing software kernel with declared precision,
wrapping, signed division, shifts and IEEE behavior. No host arithmetic substitutes
for VOID arithmetic. Both compile-time and Runtime results participate in the
cross-mode tests, including f32, NaN comparison, narrow wrapping, negative shifts,
string contents, enum values, collection equality and fixed value copies.

The engineering references were bounded by these decisions:

- [Rust compiler constant interpretation](https://rustc-dev-guide.rust-lang.org/const-eval/interpret.html)
  informed owned local frames and reuse of typed operations. VOID extends its
  existing expression evaluator rather than adopting MIR or a second VM.
- [Clang implementation limits](https://clang.llvm.org/docs/UsersManual.html#controlling-implementation-limits)
  informed independent call/work limits. VOID's limits account for its supported
  1 MiB stack and do not copy Clang's larger default call-depth allowance.
- [Zig's evaluation quota](https://ziglang.org/documentation/0.14.0/#setEvalBranchQuota)
  supports deterministic exhaustion diagnostics; unrestricted comptime features
  and quota-changing source builtins are not adopted.
- [Swift compiler control statements](https://docs.swift.org/latest/documentation/the-swift-programming-language/statements/)
  reinforce keeping conditional source selection separate from function execution.

These are upstream design references, not proposed patches or evidence that VOID
has passed qualification. The pinned compiler/backend remains LLVM 23.1.4.

## 3. Supported constant contexts

Ordinary module `const`, field defaults, parameter defaults, fixed-array extents,
numeric/flags discriminants and existing pure value/string/enum/collection
expressions admit eligible calls. Contextual literal rules and exact argument and
result types remain unchanged. There is no `compile const` declaration.

Numeric discriminants now retain expression ASTs until the shared checker and
constant evaluator can resolve declarations. Duplicate values, backing width and
automatic flag assignment retain their original rules. Bare earlier flag names
bind to earlier backing values, with navigation retained. An unresolved enum
reading its own discriminant set diagnoses a cycle; use independent integer
constants or the established earlier-flag vocabulary. Forward enum conversions
resolve the target discriminants before membership checks. Enum specializations
created during body analysis are drained before Runtime lowering, including
cast-only uses that never spell a member. Generic method identities are published
before layout expressions request specializations; the existing full owner/type/
constraint validation remains authoritative.

Local `const` remains readonly Runtime initialization. Module variables and static
fields retain Runtime initialization. Unmarked functions never acquire guaranteed
constant eligibility from an optimizer or a literal argument. Managed allocation,
custom initialization and unsupported constant APIs remain excluded.

## 4. Runtime-call behavior

Ordinary calls to a marked function use its same typed body, local storage,
argument ordering, default mapping, generic identity and ordinary LLVM lowering.
The frontend does not eagerly evaluate them. Existing O0/O2 optimization policy is
unchanged. Marking a function does not create a second ABI, scheduler or Runtime.

Marked bodies and their dependency contracts enter the existing initialization/
compatibility serialization: a body change that can affect constants/defaults
requires a restart. Module constants retain the existing once-per-analysis
resolved state; repeated function calls consume graph budgets, rather than
creating unbounded memoization state. Source references, including calls in enum
discriminants, use the ordinary declaration identity.

## 5. Resource limits and diagnostics

| Resource | Bound |
| --- | --- |
| Simultaneous evaluated function calls | 64 |
| Combined evaluated expression nesting | 128 |
| Combined evaluated expression / statement-sequence / loop-handler nesting | 256 |
| Work per required expression, including nested calls | 1,048,576 steps |
| Work across one graph analysis | 8,388,608 steps |
| Cumulative temporary value/local-storage accounting per expression | 64 MiB |
| Cumulative temporary value/local-storage accounting per analysis | 256 MiB |
| Existing module-constant dependency depth | 128 |
| Numeric discriminant dependency / on-demand body preparation depth | 64, also subject to the shared semantic-depth guard |
| Existing generic specialization depth / identity work | 128 / 8 MiB |
| Existing string value / module string pool | 16 MiB / 64 MiB |

A work step covers an expression, statement or loop advance. Linear enum
membership/name scans charge their member count. Accounting includes owned value
nodes, text bytes, copies and local slots; it bounds materialization work, not just
live values. Compact repeated fixed arrays stay compact until mutation; expansion
checks the complete prototype size before allocating copies. These are evaluator
budgets, not a claim that compiler RSS (including source/AST/LLVM) is capped at
64 MiB. Existing layout/source/type budgets still apply.

VOID2011 rejects unmarked calls/unsupported effects; VOID2012 reports dependency
cycles; existing numeric conversion, zero-divisor and index codes identify the
failing operation. VOID9001 identifies evaluation work, value, nesting or call
limits. Diagnostics retain the operation's source and up to twelve related
call-site locations, including imported and specialized sources. Poisoned or
incomplete bodies never enter evaluation/lowering.

The combined nesting budget spans function calls because their calling expression
remains active during body evaluation. Every nonempty statement sequence and loop
handler shares that budget with expressions; nested blocks cannot multiply the
independent call/expression allowances into unbounded native stack use. Scope guards
restore counters on normal return, limit failures and cancellation. The original
64-call and 128-expression limits remain, alongside this additional bound.

Debug expression cases use explicit noinline handler boundaries, matching existing
frontend/lowering practice. A large native dispatcher frame must not be multiplied
by the entire source/evaluation nesting allowance. The original 1 MiB requirement
was retained while repairing the recursion crash described below.

## 6. Editor integration

`voidls` invokes the same frontend evaluator and concrete body checker. It exposes
marked-function hover, computed numeric constant values, ordinary completion and
documented-parameter signature help, concrete generic information, definitions,
references and semantic identifier tokens. Existing TextMate syntax recognizes
`compile`; G87 inactive/directive tokens retain their existing classification.
Incomplete edits follow the existing recovery pipeline.

Interactive background analysis and assistance each receive an invocation-scoped,
thread-local borrowed stop token. A cancellation, superseding revision or shutdown
requests stop; the next evaluator work step diagnoses cancellation into that
obsolete analysis. Existing snapshot/revision/input checks prevent its publication.
No token or evaluation state is shared between independent compiler invocations.
All requests, including synchronous paths, also retain finite work/value budgets.
Opening an editor executes no ordinary user program, startup, native code or I/O.

The maintained real-editor regression is `tests/compile_functions/editor.cjs`.
It uses the existing extension development host, a fresh isolated profile and
explicit compiler/server paths. It checks hover and computed values, generic
information, definitions, documented signature help, automatic rendered `area`
completion, division diagnostics, semantic tokens, incomplete declaration repair
and superseded evaluation recovery. It restores the original buffer and verifies
unchanged source bytes. No extension installation or saved user-project edit is
performed.

## 7. JIT/AOT verification

`tests/compile_functions/source.py` runs real `voidc check/run/build` and native
executables with a 1 MiB compiler/executable stack on POSIX. Constant and Runtime
versions exercise the same functions at JIT O0/O2 and AOT O0/O2. Additional tests
cover f32/NaN, collection equality, tagged generic enum results, forward enum
membership, late Runtime enum specialization, method-dependent extents, imported
visibility and a locked local package with independent owning-manifest conditions.
Negative cases cover unused/unreached effects, invalid arguments, conversion and
index errors, constant cycles, recursion, specialization, work and temporary
budgets, repeated graph work and excluded spellings. Existing G87 and enum tests
remain separate regression oracles.

## 8. Qualification evidence and limitations

Evidence root: `/home/shmellyorc/Documents/Void.Language-G93-evidence/`.
`identities-and-results.json` records final source/artifact hashes, exact tool
versions, flags, process checks, editor evidence and the candidate patch hash.
`candidate.patch` and `candidate-source/` provide the complete unstaged review
candidate, including the deleted parser's removal.

The following table records the original, pre-review candidate. Its logs, patch,
source snapshot and editor captures are preserved; those binary hashes describe
that earlier build. Current correction evidence and results are recorded in
section 11 and a separate evidence directory. Original real-editor checks are not
presented as checks of the corrected binaries.

| Final profile / host | Result | Evidence |
| --- | --- | --- |
| Debug, pinned Clang/LLVM/LLD 23.1.4 | 23/23 focused CTests | `review-debug-tests.log` |
| Release, same pinned toolchain | 13/13 focused CTests | `review-release-tests.log` |
| Fatal ASan/UBSan, optimized RelWithDebInfo | 13/13 focused CTests | `review-strict-tests.log` |
| VS Code 1.144.0, isolated real extension host | PASS, original disk bytes unchanged | `review-vscode/results.json`, screenshot and trace |
| VSCodium 1.143.1, isolated real extension host | PASS, original disk bytes unchanged | `review-vscodium/results.json`, screenshot and trace |

Each compiler profile passed all **71** CLI/native process checks in the G93 source
harness. The rebuilt frontend suite also passed its 1,216 constant/normal/O2 numeric
matrix comparisons, recovery/resource cases, value structs, receiver methods and
initialization. Focused G87, generic, enum, LSP, readonly/member completion,
analysis and tooling checks passed. This is targeted qualification, not a full
repository-suite or hosted acceptance claim.

Build commands used the existing `build/llvm23-debug`, `build/llvm23-release` and
`build/llvm23-strict` environments, targeting `voidc voidls
void_compile_cancellation void_tests`. Tests used task-owned disk-backed temporary
storage via `TMPDIR` and retained JSON diagnostics via
`VOID_TEST_DIAGNOSTICS_DIR`. The source and LSP harnesses set POSIX stacks to
1,048,576 bytes; native AOT probes use that same stack. No original limit was
raised. The fatal sanitizer environment was:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1
```

The exact CTest selections and commands are retained in `qualification-commands.json`.
Editor launches used existing pinned Node 24.19.0 and the existing editor binaries
with `extensionDevelopmentPath`, explicit compiler/server paths and isolated
profiles. Final editor reports include the tested compiler/server SHA-256s, which
match the final Debug artifacts. `git diff --check` passed and the index is empty.

Linux x86-64 is the available native platform. Actual Windows/macOS execution,
hosted CI and independent Golden Wrench acceptance remain UNQUALIFIED. Pattern
execution, custom operators/initializers, managed/interface receivers, callable
values and Runtime-only text/collection APIs remain explicit subset exclusions.
This candidate does not claim unrestricted compile-time execution.

## 9. Preserved original failures

All original logs and reproductions remain outside the tracked checkout:

- `custom-display-original.log` retains the nominal interpolation mismatch
  (`value  value shown`). `dictionary-lookup-original.log` retains the integer-key
  lookup mismatch (`1 9`). Nominal adapters and dictionary lookup now fail the
  compile-function purity gate, and nested custom equality reuses the existing
  structural constant-type rejection. `pre-purity-audit-candidate.patch` and
  `pre-purity-audit-identities-and-results.json` preserve the earlier candidate.
- `baseline-smoke.log` records the accepted frontend rejecting `compile func`.
  `baseline/` contains the accepted frontend sources rebuilt with the pinned
  toolchain and a separate compiler for baseline checks.
- `debug-source-third.json` retains the SIGSEGV on recursive evaluation at 1 MiB,
  including the exact source. `compiler-before-stack-repair.cpp` and
  `compile-functions-before-stack-repair.inc` retain the failing implementation.
  The original dispatcher reserved `0x2f80` bytes per native invocation. Separated
  handler frames repair the crash without lowering the evaluation allowance.
- `forward-enum-original.log` retains the false checked-conversion rejection from
  unresolved discriminants. `generic-enum-runtime-original.log` retains the
  cast-only generic enum Runtime failure; late specialization now resolves before
  lowering. `method-extent-original.log` retains the early missing-method failure;
  the existing lazy method-header preparation now runs before that selection.
- `qualified-debug-tests.log` retains the stale frontend numeric-iteration
  expectation, also reproduced by `baseline/iteration.log`. Its expected message
  was updated without changing the language behavior. `review-frontend.log` then
  retains the Runtime constructor-default regression caused by the overly broad
  constant-call guard. The existing constructor-context flag now keeps those
  Runtime recipes eligible; `constructor-frontend.log` records the complete
  frontend pass after repair. Superseded build attempts/logs remain preserved.
- `generic-method-extent.log` retains the missing early generic-method identity;
  the maintained `extents.void` now covers ordinary and generic method extents.
- `debug-focused-first.log` retains two diagnostic-test failures. The enum type-name
  expectation was stale at G92 too (`baseline/enum_equality-test.log`). Only its
  expected display names changed. The unmarked-call test now expects the G93
  diagnostic; `baseline/unmarked.log` preserves its previous wording/code.
- `debug-final-tests.log` retains native-link failures during concurrent profile
  tests: a linker bus error and `/tmp` disk-quota exhaustion. Qualification was
  rerun with task-owned disk-backed temporary storage, without removing unrelated
  temporary files or treating these records as source successes.
- Earlier fixture/harness failures are retained: array/dictionary/destructuring
  syntax and negative-index assumptions, undocumented signature-help expectations,
  token-end hover positions and Markdown nonbreaking-space conversion. A later
  popup wait also retained an unchanged buffer: the background editor had dropped
  its type command. The harness now observes window/input focus and the inserted
  dot before checking automatic suggestions. The editor
  traces show correct server hover content during the harness failures. These
  are not assigned fabricated compiler defects.

Existing earlier sanitizer, 1 MiB and platform failure records are unchanged.

## 10. Exact changed-file inventory

The candidate changes 30 files; the removed flags-only parser is replaced
by ordinary typed constant evaluation. The root README is unchanged.

- `CMakeLists.txt`
- `docs/DEVELOPMENT_STRATEGY.md`
- `docs/DIAGNOSTICS.md`
- `docs/ENUMS.md`
- `docs/GENERICS.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/LANGUAGE_SERVER.md`
- `docs/README.md`
- `docs/RECEIVER_METHODS.md`
- `docs/STAGE8_CALLS_NUMERIC.md`
- `docs/VALUE_SEMANTICS.md`
- `docs/projects/G93_COMPILE_FUNCTIONS.md`
- `examples/compile_functions/main.void`
- `include/void/compile_evaluation.hpp`
- `src/frontend/compile_functions.inc`
- `src/frontend/compiler.cpp`
- `src/frontend/conditional.inc`
- `src/frontend/enum_constants.inc`
- `src/frontend/enum_parser.inc` (deleted)
- `src/lsp/main.cpp`
- `tests/compile_functions/calculations.void`
- `tests/compile_functions/cancellation.cpp`
- `tests/compile_functions/corrections.void`
- `tests/compile_functions/editor.cjs`
- `tests/compile_functions/extents.void`
- `tests/compile_functions/lsp.py`
- `tests/compile_functions/source.py`
- `tests/enums/corrections.py`
- `tests/frontend/enum_equality.py`
- `tests/frontend/tests.cpp`

## 11. Focused corrections after independent review, 21 November 2026

Golden Wrench's independent review returned **CORRECTION REQUIRED** for two P1
defects. The original record is
`/home/shmellyorc/Documents/Void.Language-G93-review-20261120/review.json`.
Both original reproductions, their outputs and the reviewed candidate remain
preserved. This correction is implementation evidence for re-review, not an
independent acceptance claim.

**Combined nesting:** twelve nested conditionals with 61 calls exhausted the
Debug compiler's 1 MiB stack despite staying inside the separate call and
expression bounds. The evaluator now charges expressions, nonempty statement
sequences and loop handlers to one 256-frame nesting budget. The unchanged
`statement_depth_12.void` now returns a source-located VOID9001 with call-site
information. The regression composes 12/24 nested conditionals or conditional
loops with recursion, checks shallow successful versions, preserves 64-call
successful recursion and checks the existing call-limit diagnostic on call 65.
The earlier dispatcher-frame repair in section 9 remains historical evidence;
it did not cover this composition of statement nesting with recursion.

**Fixed-array capture:** the evaluator previously evaluated a function-result
iterable initially and again on every iteration. The unchanged `loop_capture.void`
printed `13 3`, contradicting Runtime's `3 3`. The evaluator now selects its
iterable once: computed roots are owned snapshots, indexes are evaluated and
checked once, and local fixed storage retains a stable path that is reread as
Runtime does. Capturing a growable parent severs its former local ancestry,
matching Runtime handle capture. It retains no pointer into replaceable local
children. Values copied into iteration binders retain temporary-budget charging.

`tests/compile_functions/corrections.void` compares constant and Runtime results
for temporary/reversed iteration, local and field replacement, stable positive
and negative selected indexes, captured growable ancestry, compact repeated arrays
and returned value snapshots. The source harness runs these at JIT/AOT O0/O2 and
checks selected-iterable bounds diagnostics. The LSP harness now checks the
combined-depth diagnostic and successful recovery alongside its existing
cancellation and incomplete-source checks.

Correction evidence is under
`/home/shmellyorc/Documents/Void.Language-G93-corrections-20261121/`.
`identities-and-results.json` records the exact source inventory, compiler/server
hashes, flags, commands and diagnostic reports; `candidate.patch` and
`candidate-source/` contain the complete corrected G93 candidate. `corrections.patch`
isolates the five files changed by this correction from the reviewed candidate.
`original-evidence-hashes.json` verifies preservation of the original review files.

All corrections use the existing LLVM/Clang/LLD 23.1.4 build environments, with the
original 1,048,576-byte compiler/server/native-test stack unchanged. Fatal sanitizer
checks retain `detect_leaks=1:halt_on_error=1:abort_on_error=1` and
`halt_on_error=1:abort_on_error=1:print_stacktrace=1`. The first Debug correction
attempt rejected a new test fixture's unsupported array cast; its log and exact
source remain preserved in `tests-debug-first.log` and `debug/`. The fixture now
uses an ordinary value-returning function. The corrected Debug source rerun is
preserved separately in `tests-debug-second.log` and `debug-second/`.

| Corrected profile | Focused CTests | Source CLI/native checks | Unchanged original reproduction checks |
| --- | --- | --- | --- |
| Debug | 3/3 | 88/88 | 7/7 |
| Release | 3/3 | 88/88 | 7/7 |
| Fatal ASan/UBSan, optimized RelWithDebInfo | 3/3 | 88/88 | 7/7 |

The CTest selection is exactly
`^compile_functions_(source|lsp|cancellation)$`; logs are `tests-debug.log`,
`tests-release.log` and `tests-strict.log`. Each source harness retains the original
71 checks and adds 17 focused checks. The seven original-reproduction processes
per profile comprise the nesting diagnostic plus JIT O0/O2 and AOT build/execution
O0/O2 for the unchanged loop fixture; all constant/Runtime outputs are now `3 3`.
Exact arguments, output and stack settings are in `originals-{profile}.json`.
`small-stack-cancellation.json` also records successful direct cancellation and
concurrent compiler-isolation checks at 1 MiB for all three profiles. The source
and LSP harnesses retain their explicit 1 MiB limits. These are focused correction
results; the earlier 23/13/13 selections were not repeated.

The existing official [Clang interpreter documentation](https://clang.llvm.org/docs/ConstantInterpreter.html)
and [Rust constant-evaluation reference](https://doc.rust-lang.org/reference/const_eval.html)
were revisited before the corrections. These are published engineering references,
not an upstream patch adopted into VOID. VOID's existing Runtime selection
lowering remains the semantic oracle; bounded scope guards extend the existing
evaluator machinery without a second execution engine.

Native Windows/macOS execution remains UNQUALIFIED. The original real VS Code and
VSCodium evidence remains historical; the focused correction checks use the real
`voidls` protocol harness. No unrelated full suites or real-editor reruns are
claimed. Git integration, hosted execution, installation and publication remain
outside this correction authorization.
