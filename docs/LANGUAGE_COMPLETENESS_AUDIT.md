# VOID 2.0 language completeness audit

Research date: 20 October 2026. Research only; no compiler, Runtime, editor or
standard-library implementation is authorized by this document. Companion:
[implementation proposal](LANGUAGE_COMPLETENESS_ROADMAP.md).

## Baseline and evidence

The inspected checkout is `f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`.
Boss supplies the accepted Stage 7 baseline: CI #81, attempt 2, seven successful
Linux/Windows/macOS jobs, 140/140 CTest suites per job, 980/980 total. Hosted logs
were not independently re-audited in this assignment. Older pending-lock statements
in the strategy, decisions, server guide, acceptance report and known issues are
historical; they do not reopen Stage 7.

This audit concerns the C++20/LLVM **19.1.7** compiler in `src/`, not the original
C-emitting compiler. Evidence order is actual behavior/source, current approved
feature contracts and decisions, then dated plans and illustrative Bible examples.
The [Bible](LANGUAGE_BIBLE.md) describes the target language, not an executable
feature manifest. New owner requests about imports below are proposed requirements,
not claims of implementation or permission to rewrite that contract silently.

Local verification used existing Release binaries rebuilt with:

```sh
cmake --build build/ci-release --target voidc voidls -j2
ctest --test-dir build/ci-release -j2 --output-on-failure -R '^(frontend|numeric|modules|analysis|tooling|sessions|fixed_arrays_source|growable_arrays_source|arrays_source|generics_source|collections_source|dictionaries_source|properties_source|patterns_source|advanced_source|managed_source|managed_initialization_source|reference_values_source|composition_source|inheritance_source|interfaces_source|strings_source|async_source|async_parallel|async_lifecycle|native_source|stage6_integrated|lsp_stage7)$'
```

Build succeeded; **28/28 focused suites passed**, 60.17 seconds. This was not a new
full Debug/Release/sanitizer or hosted acceptance run. Additionally, 51 bounded
source probes produced 13 successful checks and 38 expected rejections. Eleven
of the positive programs ran in normal and O2 JIT modes: ten succeeded in both;
one deliberately tested signed-minimum checked failure and failed in both as
expected. Two positive probes checked imports only. Four more real-pipe LSP
scenarios examined import completion, five requests each. No new production
tests or verification framework were introduced.

Run artifacts are temporary, under `/tmp/void-completeness-audit/`;
`/tmp/void-completeness-build.log` and `/tmp/void-completeness-focused.log` record
the build/suites. Findings, reproducer forms and permanent source/test references
are retained here so these artifacts are not prerequisites for the next sprint.

## Feature inventory

Status: **I** implemented and verified within the stated contract; **P** partial
or restricted; **A** approved design missing from implementation; **D** deliberately
deferred/unsupported; **U** unclear and requiring verification/decision. A row may
separate a working core from a missing extension. These are capability assessments,
not a count of Bible paragraphs or a promise that every illustrative program runs.

| Area | Status and actual behavior | Evidence and remaining boundary |
| --- | --- | --- |
| Syntax, source and names | I: indentation, comments, lowercase ASCII identifiers, UTF-8 source, LF/CRLF locations. P: multiline literal/block forms. | [source compiler](SOURCE_COMPILER.md), `lex`, `Parser` in [compiler.cpp](../src/frontend/compiler.cpp), [source.cpp](../src/frontend/source.cpp), `tests/frontend`. General parenthesized argument continuation, triple-quoted strings and Unicode identifiers are absent; visual-width carets are not established. |
| Primitive numeric types | I: exact-width signed/unsigned 8/16/32/64 integers, f32/f64, bool distinct from numeric. | [numeric contract](NUMERIC_TYPES.md), [numeric.cpp](../src/numeric.cpp), `tests/numeric`. No platform-sized aliases or arbitrary native numeric ABI. |
| Numeric expressions and conversion | I: contextual decimal literals, wrapping integer arithmetic, signed division guards, IEEE float operations and checked numeric `as`; shared constant/runtime semantics. P: literal spelling/convenience. | APInt/APFloat and shared LLVM lowering; exponent/base/separator syntax, float remainder and nullable `as?` missing. Bitwise/shift operators are unsupported but are not an established approved requirement merely because other languages have them. |
| Inference, locals and constants | I: expression/local inference, compile-time dependency checking, typed pure constants, root constants visible across project modules. | [value semantics](VALUE_SEMANTICS.md), frontend/analysis suites. Parameter inference from a default is A, not delivered by local inference. Runtime module variables/initialization remain absent. |
| Readonly | I: const bindings, deep readonly reference authority, readonly receiver/effect checks and conservative joins. | [managed structs](MANAGED_STRUCTS.md), [reference values](REFERENCE_VALUE_STRUCTS.md), capability analysis. P: callback-result authority and branch/alias joins can reject otherwise safe programs. Declared `const` fields are missing; this differs from readonly access through a const view. |
| Ordinary functions | I: typed positional parameters, no-result or one typed result, recursion and forward declarations. A: ordinary named/default parameters and default-based parameter inference. | `Parser::parameters`, `validate_parameter_structure`, source probes below. Constructor named/default support does not imply general function support. |
| Receiver methods | I: owner-module nominal receivers, const/mutating effects, bound references and generic methods within supported contracts. | [receiver/value contracts](VALUE_SEMANTICS.md), frontend `prepare_function`, `tests/properties`, `tests/generics`. Primitive receivers and type-style `T.method(value)` are absent. No free-function overload/foreign-extension system should be inferred. |
| Multiple results | A: Bible multiple return values/destructuring are absent. | `ir::Function` carries one result type; parser rejects multi-result syntax. A result enum or value struct already packages multiple fields without first-class tuples. |
| Value structs | I: independent fixed children, mixed reference fields retaining identity/authority, owner methods, constants and custom init. | [VALUE_SEMANTICS.md](VALUE_SEMANTICS.md), [REFERENCE_VALUE_STRUCTS.md](REFERENCE_VALUE_STRUCTS.md), relevant source suites. No value `deinit`, user native layout ABI or arbitrary recursive inline layout. |
| Managed structs | I: identity, nullable references, precisely traced fields/roots, deep readonly, required-reference initialization. | [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md), [MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md), runtime/managed suites. P: opaque debug pointer metadata; no general managed FFI. |
| Constructors and defaults | I: implicit/custom value and managed initialization, named/default initializer arguments, pure field templates, definite initialization and explicit no-init wrapper composition. | [INITIALIZATION.md](INITIALIZATION.md), managed initialization and construction includes. P: C1 requires complete value children before the enclosing init body; S1 restricts incomplete-self use/helper first writes. A/D: runtime field defaults, overloaded/fallible `init`. Result-returning factories already work. |
| Encapsulation and static members | I: function/method public, file-private and current project-internal visibility; accessor logic. A: private/const field modifiers, static fields and general type-style static APIs. | [MODULE_SYSTEM.md](MODULE_SYSTEM.md), [PROPERTIES.md](PROPERTIES.md), `ValueField` in [ir.hpp](../include/void/ir.hpp), negative field probes. Do not advertise private storage invariants for a public wrapper before this is settled. |
| Composition | I: stored `using` forms, one-hop field/method promotion retaining original receiver semantics. | [COMPOSITION.md](COMPOSITION.md), `tests/composition`. D: transitive promotion, nullable targets, stateless mixins; no implicit subtype conversion. |
| Inheritance | I: bounded managed inheritance, inherited fields, override/virtual dispatch, super calls and guarded type-pattern recovery. | [INHERITANCE.md](INHERITANCE.md), `tests/inheritance`. D: general `as` downcasts, protected/hiding/covariant/abstract/final systems and open-world loading. |
| Interfaces | I: structural requirements, managed-backed views, embedded value providers, receiver effects and guarded patterns. | [INTERFACES.md](INTERFACES.md), [interfaces.inc](../src/ir/interfaces.inc), `tests/interfaces`. D: standalone value/primitive boxing, interface inheritance/default bodies, overloads, Self/generic interfaces, width casts and general FFI. Families remain conservatively whole-project. |
| Generics | I: concrete specialization, inference/explicit arguments, bounded structural constraints and generic methods/function references. | [GENERICS.md](GENERICS.md), frontend generic includes, `tests/generics`. P: no generic enums or const-generic extent parameter; unused templates are not a universally validated program. Primitive structural method constraints cannot work until primitive member admission exists. |
| Fixed arrays | I: `[T,n]`, nested storage, compile-time checked extents, value copies, managed references, readonly and traversal. | [FIXED_ARRAYS.md](FIXED_ARRAYS.md), [ARRAY_OPERATIONS.md](ARRAY_OPERATIONS.md), source/backend suites. No arbitrary 64 KiB production limit. Real layout/work/representability guards remain. |
| Growable arrays/lists | I: `[T]`, shared outer identity, typed Runtime buffers, count/index, push/insert/remove_at/clear and approved iterator version checks. | [GROWABLE_ARRAYS.md](GROWABLE_ARRAYS.md), [ARRAY_OPERATIONS.md](ARRAY_OPERATIONS.md), list/array Runtime. P: `.pop()` awaits result/error contract; `.get()` and value-search/removal APIs absent. |
| Slicing | I: explicit two-endpoint array slices produce independent outer collections with ordinary child-copy/identity rules. | Array source suites and local independence probe. D/U: omitted endpoints and views. Strings are not currently sliceable. Do not convert existing copies to views for convenience/performance. |
| Dictionaries | I: literals/templates, shared identity, string/integer/bool/managed-identity and eligible value keys; checked indexing/upsert, contains/remove/clear/count and deterministic iteration. | [DICTIONARIES.md](DICTIONARIES.md), [dictionary.cpp](../src/runtime/dictionary.cpp), dictionary suites. P/D: result get, enum/float/interface/collection keys, custom equality/hash, randomized hashing, clone/views and stable cross-launch hashes. |
| Collection evaluation and mutation | I: capture receiver/index/key/version before RHS, checked indexing, fixed-child copies, approved structural iteration invalidation. | Array/dictionary contracts, source and IR validation tests. A: `has` operator; library helpers can already compose contains/index. General iterator/search protocols need a separate contract. |
| Strings | I: one immutable valid UTF-8 value, empty/literal values including NUL, concat/equality, struct/collection/enum/callable/async integration, native explicit borrowing/copy paths. | [STRINGS.md](STRINGS.md), [strings.inc](../src/frontend/strings.inc), string suites and local aggregate probe. A: scalar indexing/count, byte_length, interpolation, formatting, ordering and source-visible byte conversion. No source `string?` today. |
| Printing and parsing | A: ordinary source `print`, general string formatting and string-to-number APIs. | Bible §§8–9/30 and negative probes. Pure algorithms belong in the library, but source cannot yet inspect UTF-8 bytes/scalars and fixture native admission is not general IO. CLI printing its entry result is not language `print`. |
| Conditions and loops | I: Boolean logic, statement if/else, for/ranges/reverse, break/continue/return/pass and compound assignment. | Compiler, collection/range suites, Bible §27.1. A: if-as-expression. Prefix increment and increment-as-value are intentionally excluded. No new while syntax is required by a for-based design. |
| Patterns and match | I: shared is/statement-match/expression-match checker, value/type/range/struct/list/dictionary/enum/result/nullable patterns; success-scoped captures. | [ADVANCED_LANGUAGE.md](ADVANCED_LANGUAGE.md), `tests/patterns`. Incomplete matches warn; unmatched statement does nothing, unmatched expression fails. Module-time patterns are deferred. |
| Enums and recoverable errors | I: simple/tagged enums, `result<T,E>`, Ok/Err, exact-error propagation `?`, nullable present patterns. | Advanced contract/source suites. P: generic enums and `result<(),E>` unit syntax absent. A one-case enum already supplies an ordinary unit payload. Nullable numeric/list/string families are not universal. |
| Properties and callable objects | I: get/set accessors, getter purity, property assignment through existing places, compatible call() objects. | [PROPERTIES.md](PROPERTIES.md), properties/advanced suites. No universal runtime type object, source static storage or setter-failure magic. |
| Function values and closures | I: exact callback signatures, named/generic/method references, escaping shared captured cells, value-receiver snapshots, managed receiver identity and leased code. | Advanced contract, closure/capability includes, source suites. P: conservative readonly callback results; no arbitrary external C callback ABI. |
| Events | I: synchronous ordered strong subscriptions, snapshot emission, duplicate/latest removal, owner-only emission, stable callback identity. | Advanced contract/decisions and advanced suite. D: weak/default-async events and arbitrary native event threading. |
| Resources and cleanup | I: reached LIFO defer, captured call arguments versus block captures, with/deinit one-shot managed release, cleanup on propagation and checked runtime failure. | Advanced contract, native/advanced/integrated suites. D: value-resource destruction, GC finalizers, arbitrary exception unwinding and native-owner task transfer. Dropped native owners are retained to explicit Runtime shutdown by design. |
| Async and cancellation | I: ordinary async functions, managed receivers, await, owning root adapters, cancellation/groups, value-only parallel spawn and shutdown draining. | [ASYNC.md](ASYNC.md), runtime async, source/parallel/lifecycle/integrated suites. P: explicit host adapter, bounded transferable values, root-duration completed task/frame retention. No general async CLI or public arbitrary task type. |
| Synchronization | I: GC/thread coordination and bounded worker contract. P/A: general source-shared synchronization/locks/atomics and general shared-memory task capture are outside the delivered subset. | Async contract and Runtime; GC safe points do not serialize application writes. Do not infer general concurrency from value-only spawn. |
| Native interoperability | I: explicitly trusted static fixture with validated scalar/string/resource/callback ownership, affinity and quiescence. P: fixed fixture catalog, not a general library binding language. | [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md), [native_binding.cpp](../src/ir/native_binding.cpp), `tests/native`. Arbitrary providers, dynamic loading, aggregates, UTF-16 and async native generalization are deferred. |
| Modules and project VFS | I: quoted `/`, `./`, `../` exact `.void` imports, normalization, direct imports/aliases, cycles, ownership and immutable overlay loading. | [MODULE_SYSTEM.md](MODULE_SYSTEM.md), `SourceGraph`, `tests/modules`/`tests/sessions`. No installed stdlib namespace; bare quoted paths currently diagnose package imports. Detailed new proposal below. |
| Entrypoints and application building | I: explicit entry file/root and host-native legacy entry(i32) -> i32 JIT/object adapter, normal/O2 common lowering; main() is an ordinary callable declaration. A: owner-requested optional main/top-level execution, main() or main() -> i32 application entry, successful default exit 0 and automatic main.void discovery. U: coexistence/selection details, async entry and ordinary executable build workflow. | [SOURCE_COMPILER.md](SOURCE_COMPILER.md), [main.cpp](../src/main.cpp), detailed entry/discovery research below. Legacy adapters print their return value and exit 0 on success; they do not interpret it as process status. No linker driver, arbitrary public entry/export ABI or cross-compilation. |
| Managed memory | I: precise nonmoving coordinated tracing, persistent/frame roots, loops/calls safepoints and automatic allocation-budget collection. | [GC_RUNTIME.md](GC_RUNTIME.md), Runtime/backend and managed/integrated suites. D: production throughput certification, allocator caches, weak references/finalizers, interior pointers, LLVM statepoints/LTO audit and tighter last-use roots. |
| Reload and cache | I: owned generation leases, exact compiler dependency/constant contracts, conservative restart rules, bounded async leases, optional whole-graph Debug native cache. | [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md), [STAGE5_CACHE_PROPOSAL.md](STAGE5_CACHE_PROPOSAL.md), runtime reload/cache suites. D: auto source publication, capture migration, atomic multi-slot publication, per-function compilation/eviction; Windows persistence deliberately bypasses. |
| Diagnostics | I: owned source spans, stable codes, related declarations, deterministic bounded recovery, source-located checked failures and successful warnings. | [DIAGNOSTICS.md](DIAGNOSTICS.md), source/backend/numeric suites. P: lexical cascades, bounded traces, AOT stdout/JIT stderr discrepancy, native/foreign stacks and internal-backend failure presentation. |
| Tooling and editors | I: one voidls, UTF-16 full sync, definition/references, bounded completion/signatures/hover, symbols, guarded subset rename/formatting, refresh/save/watch handling, cancellation and parent lifecycle. | [STAGE7_IMPLEMENTATION.md](STAGE7_IMPLEMENTATION.md), [EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md), LSP/editor suites. P: entry-reachable graph, limited recovery/rename, no incremental parser/compiler. |
| Documentation and discovery | A: attached `##` declaration/parameter docs and stdlib/project import-path discovery. Existing hover is generated signatures/qualifiers, not source documentation. | Bible §§46–47; lexer discards docs as comments; analysis/candidate structs have no docs field. Four live import-completion scenarios below. General package registry/install UI remains a separate deferred ecosystem goal. |
| Platform and tooling support | I: owner-accepted seven-job baseline for the configured matrix. U: unexecuted editor/version combinations and clean-machine dependencies. | Current baseline plus [Stage 7 acceptance](stage7/ACCEPTANCE.md). VSCodium and minimum-version support are not established by API similarity; native archives are not proof of self-contained portability. No publication/licensing decision is made here. |

## Targeted behavior that changes the priorities

These minimal forms were checked against the rebuilt current compiler. Error codes
describe these probes, not a proposed allocation of codes for new functionality.

| Probe form | Observed result | Implication |
| --- | --- | --- |
| `add(b = 2, a = x)` on an ordinary function | VOID2004 | Named calls are restricted to construction. |
| `func add(a: i32, b: i32 = 2)` | VOID2020, followed by missing-argument VOID2004 | Parsed defaults are deliberately rejected outside init. |
| `func add(a = 2)` | VOID1101 | Default-based parameter inference needs parser/type work. |
| Custom `init(x: i32 = 2)` and `point(x = x)` | Accepted; normal/O2 return -50 | Reuse initializer argument mapping rather than inventing another mapper. |
| `func (value i32) abs()` / `x.abs()` | VOID2018 | Primitive receivers fail admission/member selection today. |
| `i32.max_value` | VOID1101 | Primitive type keywords are not value/type-member expressions. |
| Nominal `number.read(value)` | VOID3003 | Current type-style spelling goes through module-qualifier resolution. |
| `number(...).increment().increment().read()` | Accepted; -48 in normal/O2 | Postfix chaining itself works; primitive member resolution is the missing part. |
| `private value: i32` / `const value: i32` in a struct | VOID1102 | Neither storage modifier is implemented. |
| `items.pop()` / list or dictionary `.get(...)` | Rejected (VOID1103/VOID2018 by form) | Approved result/signature decisions are pending; these are not impossible algorithms. |
| `text.length`, `text.byte_length`, `text[0]`, `string?`, interpolation, multiline string, string ordering, source `print` | Rejected | Immutable UTF-8 storage is much further along than its usable source API. |
| `x has y`, multiple returns, if-expression, omitted slice endpoint, nullable scalar/list, `as?`, generic enum, static field | Rejected | Distinct remaining language/contract work; not delivered by Stage 4's broad label. |
| `result<(), string>` versus `result<unit, string>` with one-case enum `unit.done` | First rejected; second accepted and returns 1 | Unit syntax need not gate an initial recoverable library API. |
| Enum used as a dictionary key | Rejected | Tagged/simple enum support does not automatically imply hash-key support. |
| Bitwise `&`, exponent literal, multiline parenthesized call | Rejected | Separate unsupported/unimplemented syntax, with different approval status. |
| Explicit slice copied then modified | Accepted; original unchanged, returns 3 | Preserve copy semantics when adding methods. |
| String in a value struct, ordinary generic clamp | Accepted, normal/O2 agree | Use existing value/generic infrastructure. |

Three ordinary helper feasibility probes succeeded without production changes:

```void
enum lookup_error:
    missing

func safe_get<t>(const items: [t], index: i64) -> result<t, lookup_error>:
    var position = index
    if position < 0:
        position += items.count
    if position < 0 or position >= items.count:
        return Err(lookup_error.missing)
    return Ok(items[position])
```

A dictionary helper can use `contains(key)` then `items[key]`; a list pop helper
can return Err when count is zero, otherwise `Ok(items.remove_at(-1))`. Empty/missing
cases returned 99 through the probes' match handlers in both modes. This proves
feasibility, not a chosen public error type, atomic concurrent lookup guarantee,
optimized single-hash implementation or permission to add a standard library now.
Fixed arrays of arbitrary extent cannot use precisely the same `[T]` helper
signature without additional compiler-owned family admission; no const-generics
proposal is implied.

Checked signed abs can also be expressed using existing operations, for example
for i64:

```void
func checked_abs(value: i64) -> i64:
    if value < 0:
        return (((-(value + 1)) as u64) + 1) as i64
    return value
```

The probe returned 50 for -50; signed minimum failed at the final checked cast in
both modes. This avoids overflowing the intermediate signed negation, while
ordinary unary negation retains the approved wrapping contract. It demonstrates
that a convenience method need not add an IR opcode.

## Built-in type properties and two call forms

### What can be reused and what must change

`Parser::postfix` already chains member selection, calls and indices. Existing
receiver methods already carry identity, owner, receiver type/effects, arguments,
generic specialization and callable references. `select_member` and structural
conformance in [interfaces.inc](../src/ir/interfaces.inc), however, admit nominal
struct/interface families; frontend `member` rejects primitive receivers.
`prepare_function` also requires a nonnullable nominal receiver and owner module.
Extending syntax alone will not make primitive methods work.

Recommend an explicit semantic distinction between a type qualifier and a runtime
expression. `i32.abs(health)` selects a callable with an explicit first receiver;
`health.abs()` selects that same callable with a bound receiver. Both pass through
one argument checker and lower to the same existing call representation. Evaluate
the receiver once, then arguments in existing left-to-right order. Do not implement
this through textual rewriting, runtime type objects, editor transformations or
an alternate numeric engine.

Canonical member lookup must include `ir::Type::kind` as well as structural/nominal
identity. **`ir::Type::key()` is empty for primitive types**; using that alone would
conflate i32, f32, bool and string. This is a design hazard in the proposal, not a
reported current collision defect. Do not add argument-type overload resolution,
arbitrary user extensions or runtime primitive interface boxing incidentally.

Use one compiler-recognized core provider in the proposed stdlib domain. Admit
ordinary source bodies through the existing receiver/function machinery where
possible. A small provider declaration/admission mechanism may be necessary;
the exact authoring spelling needs review. Merely writing today's
`func (value i32) abs()` is not sufficient. A shared generic helper plus thin
declarative bindings/forwarders is acceptable if the algorithm has one body.
Avoid a new registry/framework for each primitive or method. All compiler entry
paths and tools must agree on the selected provider and its dependency identity.

Primitive members must follow visibility, generic inference, receiver constness
and normal diagnostics. Existing nominal owner rules stay in force. A top-level
user function `abs` should not hijack `x.abs()`. Duplicate canonical providers or
type/member names diagnose deterministically; import order must not pick a winner.
Whether core members require an explicit import or a tiny compiler-selected prelude
is an owner decision, not an excuse for an implicit project-wide library scan.

The [Go specification](https://go.dev/ref/spec#Method_expressions) illustrates
receiver-first method expressions sharing method identity. Swift's
[extension design](https://raw.githubusercontent.com/swiftlang/swift-book/main/TSPL.docc/LanguageGuide/Extensions.md)
shows that built-in-type methods need not imply stored fields or overriding.
These are comparative examples, not proposed adoption of either language's rules.

### Properties and numeric contracts

| Type/family | Useful initial properties | Contract needed before implementation |
| --- | --- | --- |
| Signed integers | typed `min_value`, `max_value`; possibly numeric `bit_width` | Exact two's-complement extrema. Bit width describes numeric representation, not public native ABI layout. |
| Unsigned integers | typed zero `min_value`, `max_value`; possibly `bit_width` | No implicit bool/numeric conversion; all widths independently typed. |
| f32/f64 | finite `min_value`/`max_value`, `min_positive_normal`, optionally `min_positive_subnormal` and `machine_epsilon` | Recommend min_value = most negative finite value, not smallest positive value. Epsilon must mean spacing at 1, not a universal comparison tolerance. NaN/infinity constants require explicit naming and bit-pattern policy. |
| bool | Existing true/false suffice initially | Do not manufacture numeric min/max/abs or storage-size guarantees. |
| string | Existing empty literal suffices; future scalar/byte inspection | Separate Unicode scalar count, UTF-8 byte count and grapheme/UI width. |
| Collections | Existing instance count; future documented safe lookup | A shared mutable `empty` constant would expose identity; fresh construction is different. Avoid static extent/capacity promises across unrelated families. |

Create numeric constants from the existing exact APInt/APFloat machinery. They
must work in supported constant contexts/defaults/extents, carry their declared
type, and participate in source compatibility/cache dependencies. Host floating
rounding, locale parsing and mutable global storage are unnecessary.
The distinctions between finite MIN, MIN_POSITIVE and EPSILON are also explicit
in the [Rust f32 reference](https://doc.rust-lang.org/std/primitive.f32.html);
VOID still needs its own approved names and semantics.

Recommend same-type integer `abs` with a checked failure for signed minimum,
documented independently of optimization level; a recoverable try_abs can follow
if useful. Unsigned abs can simply preserve the value if exposed. Floating abs
should produce +0 from -0 and preserve NaN classification, without promising payload
bits. For clamp, specify inverted bounds, NaN bounds/input and signed zero before
shipping: recommend checked invalid bounds, NaN input propagation and a documented
endpoint rule. These recommendations require approval; ordinary arithmetic wrapping
is not being changed. `min`/`max` should wait for an explicit floating policy.

LLVM 19.1.7's [pinned LangRef](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-19.1.7/llvm/docs/LangRef.rst)
distinguishes integer abs poison policy and floating minimum versus minnum behavior.
Lowering must not use poison on a language-defined checked-failure path, and must
select NaN/signed-zero behavior intentionally. An LLVM intrinsic is an optional
implementation choice after semantics, not the language definition.

### Chaining, generic code and collection ownership

Begin with nonmutating numeric methods. Scalar chaining should stay unboxed through
ordinary typed calls/optimization; no managed wrapper is required. Nominal temporary
chaining already works. Strings remain immutable handles; operations that construct
new bytes may allocate. Lists/dictionaries retain shared identity; fixed children
retain copies; mutation checks and readonly authority must survive every call form.
Mutating type-style calls need an explicit place/identity policy before extension
to value receivers. Do not promise zero-copy chains or silently mutate temporaries.

Generic ordinary clamp already specializes successfully. Generic member constraints
can reuse compiler structural checking only after primitive members become legal.
Compile-time primitive conformance need not introduce runtime interface boxing.
Do not gate a useful numeric API on a numeric-traits hierarchy or generic interfaces.

| Layer | Required responsibilities | Reuse rather than duplicate |
| --- | --- | --- |
| Compiler semantics/IR | Collection types/extents, index/slice places, readonly/effects, equality admission, evaluation order, iteration version checks, fixed-child copying and root schema. | Existing array/dictionary member paths, capability analysis and validated typed IR. A public method binding may point to an existing privileged operation. |
| Shared Runtime | Allocation/growth, buffers, rooting, version counters, hashing and checked access, UTF-8 storage. | `src/runtime/array.cpp`, `list.cpp`, `dictionary.cpp`, `coordination.cpp`; no alternate collection implementation for editor/library convenience. |
| Ordinary library | Proven safe_get/pop wrappers, numeric algorithms, search/sort/combinators where current equality/callback types allow. | Existing functions, generics, results, loops and callbacks. A repeated dictionary hash is a measurable tradeoff, not automatic justification for a new intrinsic. |
| Narrow new privileged text API | Scalar indexing/count, byte count and owned byte conversion/validation where exposed. | Existing rooted immutable UTF-8 Runtime storage. Pure VOID cannot inspect inaccessible bytes today. Never expose a dangling borrowed pointer or bypass UTF-8 validation. |

Value-based remove/search requires an equality contract for the element family;
it is not simply another spelling of remove_at. Keep approved negative indices,
explicit copying slices and next-access structural invalidation. String indexing
is approved to return a one-scalar string; its range/error policy still needs a
precise API contract. An initial linear UTF-8 scan is reasonable; caches and
grapheme indexing require use cases and measurement.

## Nested standard-library imports and project-file imports

### Verified compatibility gap

All three `use io`, `use io/files`, `use random/hash` probes fail today with
VOID3001, “Expected a quoted import path.” A real existing file imported by
`use "systems/player.void" as player` fails with VOID3001, “Package imports are
not implemented.” `use "./systems/player.void"` and
`use "/systems/player.void"` both check successfully. Thus the requested bare
quoted project spelling also needs a deliberate resolver change.

The user requires **unquoted paths for stdlib**, **quoted paths for the project
VFS**, nested folders in both domains, and discoverability through voidls. Names
io/files/random/hash are illustrative only. No modules, APIs, registry, native
capabilities or public distribution have been selected by these examples.

### Recommended small resolver extension

Represent import kind explicitly in the existing parser/module graph. For a
configured stdlib source root, map `use io` to `io.void` and `use io/files` to
`io/files.void`. A file `io.void` and folder `io/` may coexist. Do not add implicit
`index.void`/`mod.void` lookup, parent import, folder-wide import, reexport or
filesystem-search fallback. Each actual module still uses the same parser,
signature-first cycles, constants, binder, IR and LLVM pipeline.

Recommend lowercase ASCII module segments separated by `/`, no unquoted extension,
empty segment, trailing slash in a completed import, leading slash, dot traversal,
drive/UNC/backslash or URL. Incomplete trailing `/` is valid editor query context,
not a valid completed declaration. Decide how keyword-named segments work before
reserving useful module names: a path-token context may admit names such as
`string`, while an otherwise invalid implicit qualifier requires `as text`.

Preserve quoted `/`, `./`, `../` behavior. For new quoted paths without a prefix,
recommend **importer-relative**, equivalent to `./`, because existing relative
normalization already starts at the importer's parent. Boss must settle this
versus project-root-relative before implementation: the new examples in the entry
file cannot distinguish them. The completion table below uses this recommendation.
`use "io"` stays a project-file request and diagnoses missing `.void`; it must
never silently select stdlib. General third-party packages remain out of scope.

Keep existing project identities `/folder/file.void` stable. Add a distinct source
origin/library domain for standard modules; conceptually `(stdlib, /io/files.void)`
versus `(project, /io/files.void)`. The concrete encoding may remain an injective
string if that is the smallest implementation. A pseudo-project folder that users
can shadow or override is insufficient. Current `require_module_identity`,
SourceOverrides, session keys and LSP URI conversion assume project-only keys;
review each caller instead of loosening path validation everywhere.

Select one explicit, matching stdlib root for compiler and server. Resolve it once
from configured/toolchain context, not cwd or a recursive filesystem hunt. Preserve
physical diagnostic/debug source paths separately from stable semantic identities.
Cache fingerprints must include selected source bytes, domain and relevant provider
contracts; relocation must not change semantic identities, while changing stdlib
content must invalidate affected results/native artifacts. This is source-root
configuration, not a package manager or a distribution policy.

### Names, imports, visibility and diagnostics

Default qualifier is the leaf stem: `use io/files` exposes `files`, not a new
`io.files` namespace hierarchy. Explicit `as disk` remains qualified-only.
Two nested modules with the same leaf require aliases when the qualifier is
ambiguous. Keep local declaration precedence, direct-unaliased simple lookup,
ambiguity diagnostics, idempotent repeats, explicit alias conflict errors and no
transitive reexports. Stdlib must not silently win against a project declaration.
`io.void` and `io/files.void` are independent imports.

Current internal visibility is “this compiled project”; adding stdlib requires a
real library boundary. Recommend the stdlib be one logical library for internal
access, unrelated to folder depth; private stays file-private, public is importable.
Project root constants and root-owned method privileges must **not leak into
stdlib analysis** through today's project-wide lookup. Conversely, importing one
standard module must not expose its private/internal dependencies to the project.
Recommend stdlib dependencies use unquoted imports; reject stdlib-to-project
dependencies rather than reinterpret quoted paths as a different domain.

Diagnostics must distinguish invalid syntax/path, unavailable configured stdlib,
missing module, denied/escaped path, exact-case collision, ambiguous qualifier,
unavailable overlay and visibility failure. Retain owned source spans covering the
import, show the requested domain and candidate modules, and preserve existing
code meanings. Newly legal bare quoted paths must stop reporting “Package imports
are not implemented.” No source import authorizes native bindings or filesystem
access outside the configured source roots.

## Import IntelliSense and API documentation

### Current behavior, including the new project-VFS requirement

Stage 7 completion only advertises `.` as a trigger and returns compiler-probed
identifier/member candidates. `editor_assistance` rejects cursors inside string/path
tokens and returns empty after lexical failure, including an unterminated quote.
It therefore cannot discover imports by reusing its current identifier replacement
unchanged. The existing ValidatedAnalysis contains reachable modules; it is **not
an inventory of all importable files**. Unreferenced project sources must be found
without pretending they were imported/validated.

Four live stdio scenarios confirmed:

| Current typed prefix | Current labels | Required future behavior |
| --- | --- | --- |
| `use ` | An unrelated `func` keyword item, replacing columns 0–4 | Available top-level stdlib module/folder suggestions; preserve `use `. |
| `use io/` | Empty | Child modules/folders under stdlib `io/`. |
| `use "` | Empty, despite project source files | Project-VFS files/folders at the selected starting directory. |
| `use "app/` | Empty, despite `app/player.void` | `.void` files and child folders in project `app/`; continue nested discovery. |

Each scenario made five warm requests on the small temporary project: medians
0.233, 0.279, 0.244 and 0.243 ms respectively. These are **missing/incorrect-result
baseline timings**, not working discovery benchmarks or a future latency guarantee.
The first result deserves a regression test, but does not establish a runtime or
source-safety defect.

### One compiler-owned discovery query

Factor reusable directory/overlay inspection and path admissibility from the
existing `SourceGraph` loader into a small frontend query. The query should accept
the current snapshot, importer identity, import kind, prefix and cursor span, then
return typed candidate records: module/file/folder kind, display label, valid
insertion/edit range, canonical target if known, source domain and availability.
These are proposed fields, not a new mandatory framework or public API signature.
The loader remains the final authority on complete imports. Avoid separate editor
globbers, hardcoded module/function lists, generated website catalogs or a second
LSP resolver. VS Code/VSCodium, Neovim and Vim consume identical results.

The compiler must recognize an incomplete import context **before** demanding a
fully lexable/valid graph. Extend its own lexer/parser recovery for `use `,
unquoted path prefixes and unfinished quoted paths; retain raw byte spans and
normal diagnostics from the original bytes. Do not treat arbitrary strings,
comments, division expressions or later statements as import paths. Recovery output
cannot become ValidatedAnalysis or executable IR. An unrelated broken function or
missing import should not block safe filesystem-name discovery.

The metadata query must enumerate immediate eligible children, including files
not yet in the reachable graph. Merge SourceOverrides with disk children using
the loader's exact-case and source-versus-directory rules. Include a named new
unsaved `.void` overlay under the existing root; suppress explicit unavailable
entries, retain dirty renamed-path ownership until close, and return to disk on
close. Overlay-only parents can exist, as supported by today's loader. Untitled
documents without VFS identity and external roots remain unsupported. Do not parse
every discovered file to offer its name or accidentally analyze/diagnose the whole
directory tree.

| Query under the recommended relativity policy | Discovery scope and proposed insertion |
| --- | --- |
| `use ` | Stdlib root: module `io` and folder `io/` can both appear with distinct kinds. Selecting a module completes `use io`; selecting a folder leaves `use io/`. |
| `use io/fi` | Immediate standard children filtered by `fi`; module inserts `files` without `.void`; folder inserts `filters/`. |
| `use "` in the entry | Project root children. Selecting `app/` keeps the quote open for further completion. Selecting `main.void` produces a syntactically valid quoted project path. |
| `use "app/` | Only project `app/` children; selecting `player.void` yields `use "app/player.void"`. |
| `use "app/nested/pl` | Continue recursively by immediate-directory queries; preserve `app/nested/` and replace the current path suffix. |
| Same bare quoted prefix in `/tools/main.void` | Importer `/tools/`, not project root, if importer-relative is approved. Explicit `"/app/` always selects root; `"../app/` obeys root bounds. |
| Cursor in an already closed quote or before existing suffix/alias | Preserve the closing quote, `as alias`, comments and surrounding text; replace the appropriate path range, never duplicate a suffix/quote. |

Use plain text edits as the baseline: selecting a project file adds a closing quote
only if missing; selecting a folder keeps the path open. No snippets are necessary.
If an existing suffix cannot be safely reconciled, withhold that edit rather than
emit a malformed path. Both byte offsets and UTF-16 LSP ranges must derive from
the same immutable document. Include file extension in project imports and omit
it in stdlib module names. Existing project filenames can contain portable spaces;
preserve their spelling inside quotes. Invalid/unsafe names should not become
suggestions merely because the OS can enumerate them.

voidls should negotiate space, slash and quote triggers in addition to existing
member completion and honor explicit invocation. A space trigger can be filtered
by compiler context; `/` must not produce imports during arithmetic/comments.
Adapter setup may need to permit suggestions inside import strings, but must not
contain discovery semantics. Test automatic appearance after each requested prefix
in real editors; a working manual Ctrl+Space request alone does not fulfill it.
Folder selection should continue discovery through standard completion behavior;
if a client requires a retrigger command, keep that small client interaction
separate from the shared candidate query.

LSP supports typed file/folder/module items, explicit text edits, incomplete lists
and optional lazy documentation resolution. Capability negotiation and safe
fallbacks are necessary across the clients; no custom import protocol is required.
See the official [LSP 3.17 completion contract](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/completion.md).

### Imported APIs, docs and navigation

Once an import is resolved, reuse the same compiler declaration/member/call index
for API completion, hover, signature help, definition and available references.
No hardcoded stdlib API list is needed. Carry contiguous `##` source docs and
parameter descriptions into **tooling-only owned metadata** associated with the
real declaration, including generic source identity and resolved receiver form.
Comment-only changes should refresh docs without changing native compatibility.
Document attachment across blank lines/attributes needs a small explicit rule.
Retain source docs in both project and stdlib modules; missing docs yield the
existing signature fallback, never fabricated descriptions or website dependence.

Module discovery can start with filename/domain labels and optional module docs
loaded on demand; function documentation need only follow actual imports.
Do not eagerly compile all standard modules to populate the first menu. Initial
scope is explicitly imported APIs, not an auto-import symbol search over every
unreferenced file. Expose native wrapper declarations where source spans exist;
do not promise navigation into raw C implementations or runtime intrinsics.

Standard definitions may lie outside the project root. Add explicit known-stdlib
source-to-file-URI mapping for navigation/documentation, preserving physical path,
UTF-16 spans and source identity. Today's project-only URI validator cannot simply
be bypassed. Standard source views must not silently become project overlays.
Exclude stdlib destinations from project rename/format writes; make the server's
guard authoritative even when a client opens an installed file as a normal editor.
Ordinary local imported functions retain Stage 7's guarded rename subset. No promise
of type/member-family rename is added by better import discovery.

### Path safety, freshness and performance

Reuse portable component validation, exact-case collision detection, no drive/UNC/
network paths, root-escape checks and symlink/canonical-redirect restrictions for
both discovery and loading. Do not follow symlinked child folders while listing.
No host path or project file may impersonate the stdlib domain. Preserve hard-link
alias rejection at graph admission; a name candidate is not a certificate that a
later multi-file graph has no hard-link conflict. Source resolution is not a
security sandbox against concurrent filesystem mutation.

Refresh candidate metadata on relevant file/folder create/delete/rename, project
refresh, selected stdlib-root change and overlay transitions. Existing `.void`
watch handling is reusable but folder/empty-folder changes may require adapter
coverage; Vim/manual refresh remains a fallback, not proof of automatic discovery.
Tag results with document selection plus directory/root generation; reject stale
edits after document change, close/reopen, root replacement or observed invalidation.
Revalidate at import load because disk enumeration is not an atomic filesystem
snapshot. Deletion after a response must yield a normal missing-file diagnostic.

SourceGraph already enumerates each requested parent to enforce exact spelling,
merging virtual children; it does not recursively crawl the project. Reuse that
work in a directory query. Initial discovery cost is proportional to requested
parent children plus relevant overlay keys, not the number of source bodies or
all descendants. That is a proposed cost model, not an implemented benchmark.
Deduplicate and sort deterministically. Cap payloads/results with honest incomplete
responses; filter the typed prefix before truncation so a specific match is not
permanently hidden. Do not claim a truncated list is complete. Avoid scanning
non-source content/large files for labels; do not add recursive watches, a database
index, a background compiler or persistent catalog without measured need.

Existing [Stage 7 measurements](stage7/ACCEPTANCE.md#beforeafter-measurements) show large-graph
completion/signature medians around 192/187 ms because semantic probes reanalyze
the graph, versus hover around 0.11 ms. Import discovery should use metadata and
current import-context recognition rather than invoke that full path. Existing
queued-edit coalescing already reduced its measured burst from 4800.57 to 277.77
ms; do not reopen a solved backlog or infer that an incremental compiler is now
required. Measure the new feature before choosing further optimization.

Required measurements for engineering: cold/warm first-level and nested completion
at 10/1,000/10,000 sibling entries, many irrelevant descendants, overlay-only
children, stdlib-root failure, repeated typing and folder rename/delete. Separate
enumeration/filtering from semantic analysis and transport, report median/sample
tail, bytes/items and peak memory, and verify returned edits. No throughput or
latency result for the unimplemented feature is claimed in this audit.

## Optional main, executable top-level code and project discovery

### Requirement versus the inspected implementation

The additional owner requirement is: a developer may run top-level code without
main, write `func main():`, or write `func main() -> i32:` for a process exit code.
Successful completion without an explicit return means exit 0. Find `main.void`
automatically, preserve explicit entry-file overrides, and keep the existing
`entry(i32) -> i32` examples working. Apply one discovery policy through voidls
for VS Code, VSCodium, Neovim and Vim. These are requested capabilities; detailed
selection/coexistence rules below remain research recommendations.

[Bible Section 10](LANGUAGE_BIBLE.md#10-project-entry-point-and-vfs-imports-agreed)
already discusses main.void, program.void, explicit files and a unique root file
containing main, but explicitly leaves discovery priority unsettled. Its later
[entry discussion](LANGUAGE_BIBLE.md#entry-point-and-top-level-execution-discussion-12-october-2026-proposed-not-locked)
prefers main.void and optional top-level execution, proposes rejecting competing
entry styles, forbids implicit execution on import and leaves signatures/status/
migration open. The new request establishes the desired simple forms and discovery
direction; it does not retroactively implement that proposal or approve every
older alternative. No Bible or production semantics were changed by this audit.

Actual implementation has several separate entry boundaries:

| Boundary | Verified current behavior | Consequence |
| --- | --- | --- |
| CLI file selection | `src/main.cpp` requires a file argument for check/run/emit. `voidc run` prints usage and exits 2 even with main.void in cwd; `voidc run .` fails rather than discovering it. | Automatic project discovery is missing. |
| Parser/module body | `Parser::parse` accepts declarations/imports, not executable file-level var/call/return statements. | Top-level execution needs compiler parsing/binding/lowering, not just CLI dispatch. |
| Ordinary main declaration | No-result main and i32-returning main with complete returns pass check; main can be called by another function. | The spelling is already legal but has no application-entry privilege. |
| Source entry gate | Both `require_source_entry` overloads require the selected file's non-generic `entry(i32) -> i32` (VOID4001 otherwise). | A valid main-only program cannot currently run/emit through the source CLI. Imported main/entry is not searched as a substitute. |
| IR and native invocation | `lower_graph` sets `entry_symbol` to `<entry module>::entry`; Jit::compile requires i32(i32). Cache load/identity paths also assume `::entry`. | A main wrapper must update selection metadata/dependencies, not rename source text or only relax a diagnostic. |
| Current output/status | Successful legacy JIT CLI invocation prints the function result then returns process status 0. The generated AOT source adapter likewise prints `%d` then returns 0. | Returning 7 currently produces stdout `7`, **not exit status 7**. Preserve this compatibility behavior separately from new application semantics. |
| Return checking | `Semantics::analyze_one` rejects reachable fallthrough in value-returning functions (VOID2006); a no-result function cannot `return 0` (VOID2001). | Default entry exit 0 must be explicit in the application contract; do not disable return checks globally. |
| Libraries and lifecycle | check accepts libraries without entry; init/deinit are construction/resource methods, not implicit module startup/shutdown. | Keep library analysis independent of runnable application selection. |
| Server bootstrap | `voidls --entry <file>` is mandatory; Server constructs its fixed root/session before initialize. workspace/root fields do not discover a project. | Using initialize.rootUri alone needs lifecycle changes; the present server cannot simply infer a new entry after construction. |

Existing adapters also select entries before starting the server: VS Code's
[config.ts](../editors/vscode/src/config.ts) uses configured entry or a fixed
standalone active file; [Neovim](../editors/neovim/lua/void.lua) similarly pins an
explicit or first standalone file; [Vim](../editors/vim/autoload/void.vim) requires
an entry argument. VSCodium uses the VS Code extension path, not a separate
implementation. None currently searches for main.void. A common search implemented
four times in those clients would violate the shared policy requirement.

Follow-up evidence: 15 disposable source scenarios each ran check and normal/O2
run (45 invocations), plus three startup/discovery invocations. All matched the
current behavior described above. main-only/check-library forms passed checking
but failed running with VOID4001; typed main fallthrough failed VOID2006;
top-level call/var/return and mixed top-level/main failed VOID1101. Legacy return
and legacy calling main worked. A file containing main returning 99 plus legacy
entry returning 7 printed 7 and exited 0 in both modes. Therefore treating any
existing main declaration as a new competing entry would break a working case.
An explicitly selected alternate file containing only main still failed to run.
Artifacts: `/tmp/void-entry-audit/probe.py` and `results.json`. These results extend,
rather than replace, the earlier 51 language probes.

The follow-up also passed **8/8** existing Release suites in 15.56 seconds:
`frontend`, `modules`, `modules_cli`, `source_cli`, `analysis`, `sessions`,
`lsp_stage7`, `lsp_protocol` (CTest selection by those exact names). These include
existing normal/O2 JIT/AOT compatibility checks; no new application-entry behavior
is implemented or claimed by their success.

### C# comparison and recommended coexistence rule

C# permits one source file with top-level statements; the compiler synthesizes
its entry method. An explicitly declared Main is not the selected entry when
global code exists, and `/main` cannot override that selection. See Microsoft's
[top-level statements guide](https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/program-structure/top-level-statements).
The compiler reports ignored explicit Main with warning CS7022; top-level locals
are not ordinary globals accessible from other types/files. See the official
[entry diagnostics](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-messages/entry-point-errors).
VOID need not copy C#'s ordering rules, string-array arguments or async signatures
to provide similarly simple programs.

| VOID coexistence choice | Tradeoff | Recommendation |
| --- | --- | --- |
| Top-level code takes precedence; warn that main is ignored | Familiar C# behavior, but a developer can edit main and unknowingly run other code. | Viable alternative; not the preferred VOID rule. |
| main takes precedence; warn or silently ignore top-level code | Leaves written executable statements unexpectedly dormant. | Reject as the default. |
| Execute top-level code and then call main automatically | Two implicit execution phases, order/cleanup questions, possible duplicate explicit calls. | Do not introduce this rule. |
| Diagnose top-level executable statements plus an entry-eligible main in the selected file | One clear style per program; declarations remain usable; no silent selection. | **Recommended for Boss approval**, consistent with the older VOID proposal. Provide spans/hints for moving code into main or removing the competing declaration. |

For new top-level code plus a legacy-shaped entry in the same selected file,
recommend the same competing-style diagnostic. This combination is not supported
today. For an existing declaration-only file containing both legacy entry and main,
recommend preserving legacy entry selection, with its current input/output/status
behavior; main remains an ordinary helper. Report compatibility selection in
project/run information rather than silently changing execution. A strict
main-versus-legacy conflict rule would need a separately approved migration policy.
Do not search imported files for competing entry names; their main/entry functions
remain ordinary symbols and importing declarations never runs a program body.

### Proposed application body, exit status and shared lowering

For a selected new application file without legacy compatibility selection:

| Source form | Proposed behavior |
| --- | --- |
| Top-level executable statements, no main | Execute in textual order through one private compiler-owned entry body. `return n` returns an i32 process status; successful fallthrough or bare return means 0. |
| `func main():` | Invoke once with no arguments; normal fallthrough/bare return maps to status 0. An integer return is still a type error. |
| `func main() -> i32:` | Use an explicit return value as process status. To honor the requested default-success rule literally, recommend implicit 0 on reachable fallthrough **only for the selected application entry**; this is a scoped semantic addition requiring approval. Ordinary typed functions and unselected/imported main functions retain current missing-return checks. |
| Only declarations/constants, no executable body or recognized entry | check remains valid; run/emit-application reports no runnable entry. Do not call arbitrary public functions or treat imports as startup effects. |
| Checked runtime failure/cancellation/cleanup failure | Nonzero failure status via existing failure/cleanup machinery; normal implicit zero must not swallow failure. Preserve the primary failure. |

C#'s explicit integer Main also reports a process status, while a no-result entry
does not return one; see [Main return values](https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/program-structure/main-command-line).
C# does **not** permit arbitrary fallthrough in an integer-returning Main, as its
[CS0161 example](https://learn.microsoft.com/en-us/dotnet/csharp/misc/cs0161) shows.
The suggested VOID entry-only implicit zero is therefore a recommendation grounded
in this owner's default-success request, not a claim about C# or existing VOID.
Approve whether that includes typed main fallthrough before engineering; do not
quietly substitute a global missing-return relaxation.

New application mode should return status without automatically printing it. Keep
legacy mode printing its computed result and exiting 0 on success. New no-argument
main/top-level programs should not accidentally consume the old optional i32
operand. General string argv, async main/top-level await and process APIs need
their own contract and are not prerequisites for the requested synchronous forms.
Preserve i32 at the compiler/host boundary; document OS-visible status encoding
separately (tests can use portable 0 and 7), rather than promising identical shell
representation of every signed i32 on all platforms.

Reuse the existing native i32(i32) invocation through a compiler-owned adapter:
ignore its private input for new no-argument applications, invoke main or the
synthetic top-level body, and normalize a no-result success to 0. Do not expose the
synthetic symbol as a source-callable public function or conflate it with a user's
entry. Preserve direct legacy symbols/contracts. Store selected file, entry kind,
target/signature and status policy in compiler-owned metadata so JIT, AOT, cache,
reload, analysis and tools agree. Native application adapters must return the
normalized status instead of the legacy print-and-zero epilogue.

For top-level code, recommend imports first, ordinary declaration registration as
today, and executable statements collected in their original relative order.
Top-level var bindings belong to the synthetic body's local scope; declared
functions do not gain implicit access to them as runtime globals. Existing pure
root constants keep their project contract. This avoids hidden cyclic module
initialization while allowing ordinary functions/types alongside the program.
Recommend diagnosing imported executable bodies rather than ignoring them or
running them on import; this imported-script policy needs approval. Keep source
locations, precise roots, defer/with, failure propagation and generation leases in
the same pipeline. Entry-kind/body changes must obey existing restart/publication
checks, not an editor-triggered execution path.

### One automatic main.void discovery policy

Discovery chooses a **file/root**, not whether it can run; libraries remain
checkable and editor analysis must never launch source. Recommend a small shared
frontend project-selection function used directly by voidc and exposed through
voidls. Suggested deterministic priority:

1. Explicit entry-file override wins. Preserve its parent as VFS root, validate it
   normally, and report an invalid override instead of silently discovering another
   project. The file may have any valid name; it can use the new simple forms or
   legacy entry. File selection and function selection are different operations.
2. With a supplied trusted workspace/search boundary and active named file, inspect
   that file's parent and successive ancestors **within that boundary** for an
   exact-case main.void; choose the nearest. Without an active file, inspect the
   supplied project directory itself. CLI no-file run uses the supplied directory
   or cwd as that boundary, not arbitrary enclosing home directories.
3. If no candidate exists, editor analysis may explicitly label a named local
   document as standalone; CLI project run reports no main.void and offers an
   explicit-file hint. A standalone file becomes runnable only through an explicit
   file selection and supported program body. Do not silently choose an arbitrary
   `.void` file as an application.

Do not recursively scan for main function declarations or add program.void as an
unrequested fallback. Section 10's alternatives are historical proposals, not a
reason to make discovery unpredictable. Nearest nested main selects a nested
project deliberately; for equal inputs every editor gets equal results. Client
workspace/cwd boundaries may differ, so show the selected root/entry and allow
explicit override. Multi-root orchestration remains outside the current one-graph
server; ambiguity must not pick whichever file was enumerated first.

Use existing exact-case, portable path, alias/symlink and root-escape checks.
Inspect ancestor candidate names without parsing all sibling files; cost is bounded
by search depth and existing directory case validation, not source-body count.
For editor overlay-only named main files, define an explicit active-document hint
or resolve after synchronization; a disk-only preflight cannot know unsaved files.
Do not equate untitled buffers with canonical VFS sources. Keep the selected project
fixed for the session: an invalid main must report diagnostics rather than falling
back, and main creation/removal/root changes require deliberate reselection/restart
with fresh session tokens. Never silently rebase dirty overlays onto a new root.

**Recommended voidls integration:** add one read-only preflight project-selection
mode returning small structured entry/root/selection-kind data, backed by that
same frontend function. Adapters pass literal explicit entry or workspace/boundary
and active-document inputs, consume the result, then start the existing stdio
server with its resolved entry. This avoids four search implementations and fits
clients that need root_dir/document selectors before starting LSP, while preserving
the existing `--entry` lifecycle. Preflight is a separate invocation, never extra
JSON/log output injected into LSP stdout. Exact flag/schema spelling is not fixed
by this research; keep it versioned/minimal and validate subprocess failure.

An alternative is deferring Server/session construction until initialize and
negotiating selection there. That requires more lifecycle/attachment changes and
does not by itself solve clients needing a root before initialize; it is not the
recommended first implementation. No separate discovery daemon or new project
manifest is required. CLI discovery must call the shared helper, not depend on
starting the language server. VS Code/VSCodium, Neovim and Vim retain editor-specific
process/trust/buffer lifecycle handling, with no editor-specific main.void search.

### Required verification and remaining decisions

Test top-level/no-result main/integer main (0/7/fallthrough/early return), competing
styles, legacy printing/status and optional numeric input, callable main helpers,
alternate explicit files, invalid signatures and declaration-only libraries.
Exercise local scope/root constants, imported scripts, cross-file cycles, cleanup
and forced GC through normal/O2 JIT/AOT. Assert stdout **and process status**;
printed `7` with status 0 cannot pass a new integer-main exit-code test.

For discovery, compare CLI and voidls results on root/nested/no main, explicit
override, invalid override, wrong case/case collision, symlink/retargeted root,
spaces/Unicode physical roots, unopened workspace, active named unsaved main,
dirty overlays and main creation/deletion. Test supported real VS Code/VSCodium,
Neovim and Vim clients with the same fixture/inputs, explicit-entry configuration
and restart/attach behavior; API compatibility alone is not execution evidence.
Keep entry-reachable analysis and current one-project/UTF-16/stale-edit guards.

Boss/Wrench decisions still needed: the recommended competing-style error; legacy
precedence when both declarations exist; typed-main implicit-zero scope; imported
script rejection and top-level scope/order; bounded nearest-main search/reselection
and overlay-only discovery. The optional-main/default-zero/automatic-main.void
goals are already requested and should not be reopened as permission questions.

## Known-issues reconciliation

This ledger covers the substantive items in the 492-line inspected
[KNOWN_ISSUES.md](KNOWN_ISSUES.md), consolidating duplicates while preserving
their originating sections. Status here is evidence-based; the historical file
was not rewritten. Categories: **S** correctness/safety, **L** language completeness,
**Dg** diagnostics, **P** performance/resources, **T** tooling, **E** library/ecosystem.
“Unsupported” means an explicit boundary, not a newly discovered defect. Fixed
does not imply every related future capability exists.

| Origin / issue | Status; category | Evidence and consequence |
| --- | --- | --- |
| Stage 7 boundaries: bounded recovery, callee replacement and readonly suppression | Open/restricted; T | Current compiler recovery and Stage 7 tests. Import prefixes add a separate required recovery context; do not claim arbitrary error-tolerant compilation. |
| Stage 7 rename excludes types/families/aliases/generics/captures; tokens/actions absent | Unsupported in current subset; T | Server guarded rename and contract. Do not expand it as a side effect of docs/imports. |
| Stage 7 research: unsafe Vim versioned rename | Partially resolved; S/T | Production safely disables Vim rename; Neovim/VS Code guards exist. Vim parity is still missing, not a remaining unsafe advertised path. |
| Stage 7 planning/Delivery 5 pending authorization/lock/platform gates | Superseded; T | Owner CI #81 baseline replaces pending gates; retains source history. |
| Stage 7/Phase 11 client minimums, VSCodium and unexecuted combinations | Unverified; T | Local/hosted configured tests cannot establish other clients/versions. New Windows/macOS Vim/Neovim combinations still need actual execution. |
| Native archives and clean-machine dependencies | Open boundary; E/T | Stage 7 acceptance dependency audits. Private packaging success does not certify distribution; no licensing/publication decision requested. |
| Delivery 5 native callable ownership/task transfer, native worker effects, dynamic loading and general parallel sharing | Unsupported; S/L | Async/native contracts and validators. These require bounded future design, not loosening existing effect gates. |
| Delivery 5 dropped native owners survive to Runtime shutdown | Intentional; S/P | Explicit native ownership, never GC finalization. Evaluate application resource use, not a presumed collector defect. |
| Delivery 5/current async completed tasks/frames retained to root drain; Runtime data metadata retained | Open limitation; P | `Scope::tasks` owns task records until adapter teardown in runtime async. Real long-lived-root risk; no completed-task reclamation proof currently. |
| Current dictionary unsupported keys/custom hash/equality/randomization/stable hashes | Unsupported/deferred; L/E | `dict_key_supported` and dictionary contract; include enum keys, not just historical float/interface/collection list. |
| Dictionary lookup/count not compile-time, no clone/view | Unsupported; L/E | Pure literal templates work; runtime operations and shared identity are deliberate. |
| Result get/pop missing | Open decision; L/E | Compiler diagnostic and successful ordinary helper probes; exact result/error APIs are the dependency. |
| Generic enums/unit syntax/module-time patterns/bound receiver allocations | Unsupported/deferred; L | Advanced/generic contracts. One-case unit enum already suffices for ordinary result APIs. |
| Readonly callback-result authority | Open restriction; S/L | Conservative capability propagation; no evidence of an authority escape in this audit. |
| Omitted slice endpoints/views, string inspection/interpolation/formatting | Open/deferred; L/E | Negative probes plus array/string contracts; prioritize text access before formatting conveniences. |
| Current root lifetimes conservative to function return | Open limitation; P | Backend root homes; precise coordinated collection works but dead locals may stay live. |
| Stage 4-era “LSP completion/hover/rename/format absent” | Fixed within Stage 7 subset; T | Stage 7 source and passing real-pipe suite. Package/import discovery remains absent. |
| Stage 5 Windows/unidentified shared-LLVM cache persistence bypass | Intentional supported fallback; T/P | Native cache contract; do not label Windows tests as disk-cache reuse. |
| Cache corrupt entries, no eviction/shared/remote/per-function cache | Open/deferred; P/T | Corrupt artifacts miss safely; cleanup is manual. No demonstrated need to block source completeness on a cache service. |
| Historical large-array activation/64 KiB production block | Superseded/fixed; S/L | Orders 1–5/Phases 23–25, current array source suites, [ARRAY_OPERATIONS.md](ARRAY_OPERATIONS.md). `native_storage.cpp`'s 65536 research-proof bound is not called by production lowering. |
| Historical array-copy/literal/local/traversal activation | Fixed within current contracts; L | Fixed/growable/mixed array and managed storage suites pass; large positive fixtures remain. Work limits still need shape-aware reasoning. |
| Layout/semantic/generic/interface/construction work bounds | Intentional; P/Dg | Existing 128-level layout, 256 semantic recursion and 65536 expanded-field guards, generic identity-work and finite construction/interface budgets protect the compiler. They are not a byte ceiling; change only with reproducer evidence. |
| Deep valid receiver-chain stack failure | Fixed; S | Stage 7 noinline case-frame/cold-probe repair; 16/64/96/124 positives, 1 MiB regression evidence in [semantic-stack.json](stage7/semantic-stack.json), frontend suite. Earlier “still open” is obsolete. |
| UTF-8 diagnostic byte/scalar mismatch amendment | Fixed; Dg | Source renderer/snapshot tests and later acceptance; malformed bytes remain escaped rather than treated as valid scalars. |
| Phase 22 string extension boundaries and compile-time limits | Partial; L/P | UTF-8 storage/concat/equality implemented; 16 MiB literal and 64 MiB pool limits are compile-time budgets, not a universal runtime string limit. Rich API still absent. |
| Historical value/reference/managed/collection exclusions | Superseded in large part; L | Phases 18/23–25 and Stages 3–6 contracts/source suites. Keep the narrower current exclusions, not old blanket “no collections/closures/async”. |
| Historical OOP/downcast boundary | Partial; L | Inheritance and managed/interface guarded type patterns work. Arbitrary casts, protected/hiding/covariance/abstract/final remain unsupported. |
| Interface standalone value erasure, inheritance/defaults/overloads/Self/generics/FFI | Unsupported; L/S | Structural managed-backed view validator; primitive extension must not accidentally introduce boxing. |
| Interface “dynamic discovery/type tests absent” | Partial/superseded; L | Closed-graph guarded type patterns implemented; open-world discovery is not. |
| Initialization C1/S1 and pure defaults | Intentional restriction; S/L | Construction contracts/checker. Incomplete self escape/helper first-write and non-default value children cannot be made legal incidentally. |
| Runtime field defaults, ordinary default/named/inferred params | Open; L | First is still deferred; ordinary calls confirmed missing despite generic summaries saying named/defaults implemented. |
| Constructor overloads/fallible init/value deinit/foreign extensions/aggregate ABI | Unsupported; L/S/E | Explicit contracts; use result-returning factories and managed resource wrappers today. |
| Declared private/readonly fields | Open; L | Field parser/IR lacks modifiers. Readonly reference views and function privacy do not solve storage encapsulation. |
| Conservative effects/joins and project-wide interface families | Open restriction; S/P | Unrelated mutating provider can conservatively reject a reader; empty family is unknown. No path-sensitive alias engine is recommended. |
| Managed opaque debug information | Open; T | Pointer metadata works but does not expose rich source fields to a debugger. |
| Phase 14 macOS LLJIT DataLayout lifetime abort | Fixed; S | Module-owned layout correction and later owner-accepted cross-platform baseline supersede historical failed run. |
| Platform mixed RTTI/Windows AOT SDK/thread ABI incidents | Fixed in supported setup; S/T | [CI_SANITIZER_ABI.md](CI_SANITIZER_ABI.md), CMake and later CI acceptance; preserve matching ABI checks. |
| LLVM archives lack independent checksum/signature | Open; E/S | `tools/ci/llvm_sdk.py` verifies version/layout, not independent archive integrity. Supply-chain hardening is distinct from a reproduced corrupted SDK. |
| Compiler scope says no LSP/watching/cache/closures/async | Superseded in bounded current forms; T/L | Existing production features/contracts. Runtime auto-reload, partial compilation and package management are still absent. |
| External roots/package resolution/persistent cross-library identity | Open/deferred; E/L | New stdlib domain is necessary now; a full external package manager/permission system can wait. |
| Entry-parent root/no root override/automatic entry discovery | Current boundary; requested discovery remains open; T/E | Explicit entry-file overrides already work and retain entry-parent VFS root. New shared main.void discovery is proposed above; it is separate from import-path completion. |
| Portable ASCII paths/case/symlink/hard-link rules | Intentional; S/T | Shared loader; Unicode module naming remains U. Accepted hosted roots do not certify every Windows Unicode/alias situation. |
| Direct imports/aliases/no transitive reexports/current internal project boundary | Implemented, incomplete for libraries; L | Module contract and tests. Stdlib needs domain-aware internal/root lookup, not folder-based namespaces. |
| Runtime globals/module initialization | Open/deferred; L/S | Constant dependency walk is implemented; mutable top-level execution policy remains separate and partly proposed. |
| Multiline expressions/Unicode identifiers/visual carets | Partial/unverified; L/Dg | Literal/closure/match block forms work; multiline parenthesized call probe fails. No blanket “all multiline works”. |
| Block-local IR results and temporary slots/no general SSA phi builder | Intentional; P | Existing IR/LLVM optimizer; no evidence this requires a new IR architecture. |
| Numeric extra literals/remainder/as?/platform aliases/native numeric ABI | Open/deferred; L/E | Numeric contract and probes; separate missing spelling from core arithmetic correctness. |
| Foreign rounding/flush-to-zero environment | Open design boundary; S | Current hosts assume IEEE environment. Future providers changing it require save/restore or explicit contract tests. |
| Diagnostic lexical cascades/budgets | Open restriction plus intentional bounds; Dg | Lexically unreadable declarations may cause follow-on names; 100 diagnostics/32 callers are documented limits with truncation. |
| “Successful warning results absent” | Fixed; Dg | ValidatedAnalysis warnings and incomplete match warning VOID2021. |
| Rich local debug/native stacks/backend failure translation | Open; Dg/T | Source runtime call locations exist; not a complete debugger/foreign exception stack system. |
| AOT stdout versus JIT stderr diagnostics | Open; Dg | Frontend/Runtime ABI paths use different presentation; small UX repair candidate, not new language semantics. |
| Object-only AOT/simple entry/no general linker or cross-compile | Open/deferred; E/L | Actual CLI/backend. Decide a bounded application host separately; preserve host object API. |
| GC section says async roots/automatic collection absent | Fixed/superseded; S/P | Async rooted frames/groups plus Runtime allocation budget (256 in production host construction). Do not require source manual-GC controls to repair an obsolete claim. |
| Allocator caches/weak refs/finalizers/statepoints/last-use/LTO/production GC certification | Deferred; P/S | Coordinated nonmoving precise collector is working; no production throughput certification is claimed. |
| Fatal corrupt roots/uncooperative Managed timeout/no forced sweep | Intentional safety behavior; S | Runtime invariants and cooperative collector contract. Do not “fix” by sweeping unacknowledged roots. |
| Native exceptions may not unwind generated code | Intentional; S | Bridges contain host exceptions; not a general language exception feature. |
| Whole-object/max-align pointers/no interior or over-aligned types | Unsupported; S/L | GC descriptor/allocation model; a source stdlib does not need raw pointer expansion. |
| Sanitizers do not instrument LLVM/JIT application instructions | Explicit coverage limit; S | C++ compiler/Runtime/fixture instrumentation plus observable generated-root tests; never claim end-to-end sanitizer instrumentation. |
| Reload metadata host/frontend division; ordering conservative | Partial; S/T | Exact frontend constant/declaration/binding contract now exists; some layout/capture/host ABI checks remain host responsibilities. |
| “No suspended-task tracking” | Fixed within bounded async leases; S | Async activation retains generation ownership through suspension; migration is still unsupported. |
| No multi-slot transaction/capture migration/automatic native source watching | Deferred; S/T | Single publication generation and leases. Editor file refresh is not native code publication. |
| Dispatch/lease overhead unbenchmarked; retirement errors stderr | Unverified/open; P/Dg | No new dispatch benchmark here; do not prescribe lock-free replacement without evidence. |
| Arbitrary native providers/dynamic ownership/async bridges | Unsupported; S/E | Fixed-catalog binding validation and trusted quiescence; generalization must preserve ownership/affinity/admission, not accept arbitrary addresses. |
| UTF-8 temporary borrow/retained/returned buffers versus UTF-16 | Partial; L/E | Bounded fixture supports owned paths; general UTF-16/metadata syntax absent. NUL rejection is bridge-specific, not a VOID string ban. |
| Official LLVM no-RTTI prevents hosted vptr coverage | Intentional coverage limit; S/T | Supported ABI matches SDK; other sanitizers/leaks remain enabled. Do not reenable mismatched RTTI just for a check. |
| GCC16 ciso646 warning/optional CURL missing | Known external observation, nonblocking; T | Third-party header warning and unused optional dependency; required SDK dependencies retained. |
| LLD scheduler backtrace despite zero exit | Unverified; S/T | Historical serial rebuild passed, but no causal fix proved. Retain observation, capture a reproducible trace if it recurs; do not assert current deterministic failure. |
| Sandboxed LeakSanitizer process inspection restriction | Environment issue, resolved by appropriate execution context; T | Prior outside-restriction run retained leaks. No rationale to disable leak detection. |
| Phase 8 mutable-local/member/hover/body-extent records absent | Fixed in Stage 7 subset; T | Tooling-only declaration/reference/extent metadata and compiler probes; native compatibility metadata remains separate. |
| Phase 8/9 pending hosted phases/no session/LSP/cache | Superseded; T | Owner baseline and actual sessions/server/cache. Historical success cannot authorize a fresh changed graph. |
| Sessions/disk reads not atomic; stale results cannot publish | Intentional guarantee/boundary; S/T | Immutable snapshot token select/analyze/accept, refresh invalidation; disk consistency checks do not provide filesystem transactions. |
| Entry-reachable analysis/unreachable overlays | Intentional current boundary; T | Opening a file does not semantically import it. New path discovery must list it without expanding the analysis graph. |
| Local UTF-16 URI scope/no UNC/multi-root/total quota | Unsupported plus open resource boundary; T/P | Strict local source mapping, per-message/depth/doc limits, document history grows with distinct identities. Stdlib navigation needs an explicit second known root. |
| “No save/watch/refresh/cancellation/worker” | Partial: first four fixed; compiler worker still absent; T | Stage 7 synchronization/coalescing and cancellation of queued requests; running compiler work is not interrupted. |
| TextDocument lifetime exceeds tab lifetime | Intentional client behavior; T | Keep sync on model lifecycle; tab-close tests alone are insufficient. |
| setTrace logged unsupported | Fixed; T | Server explicitly consumes `$/setTrace`. Verbose source logging and wrong-executable configuration remain real operator concerns. |
| Phase 19 composition restrictions and pending hosted gate | Restrictions retained, hosted gate superseded; L/T | One-hop composition contract plus accepted current CI. |
| Interface witness/root lifetime details | Intentional; S/P | Witnesses are immutable data, not code leases/permissions; dead root homes retained until return. Preserve separation during future API work. |

### Defects, risks and missing conveniences are different

This investigation reproduced **no new production memory-safety or numeric
correctness defect**. That statement is bounded by the focused tests, not a proof
that no defects remain. Previously reproduced deep-chain and UTF-8 diagnostics
issues have specific repair evidence. The import-context keyword suggestion is a
confirmed tooling defect; source docs/path discovery, ordinary argument semantics,
text access and general native admission are confirmed missing capabilities.

The most consequential existing resource risk is completed async work retained by
a long-lived root. Its source ownership is clear, but representative memory-growth
and safe early-reclamation experiments are still needed. Native ownership limits,
readonly conservatism, GC root retention and unsupported key types are documented
boundaries rather than evidence of unsoundness. New stdlib identity, visibility and
overlay isolation are genuine architectural hazards to address before rollout.

## What must precede substantial standard-library work

1. Define and implement the proposed stdlib/project import domains, nested paths,
   domain-aware visibility, stable identities and discoverability. Include project
   file completion, API docs/navigation and all editor/server adapters in acceptance.
2. Complete ordinary argument mapping and a small canonical built-in member/type
   property facility. Resolve encapsulated wrapper needs before relying on private
   fields; do not require an entire new object model.
3. Add source-usable UTF-8 inspection and owned conversion, then approve minimal
   recoverable collection APIs using existing results/generics. Rich interpolation,
   format grammar, generic enums and unit sugar need not block first modules.
4. Before IO/resources/process modules, generalize the trusted static native
   provider boundary and implement the requested optional-main/top-level application
   host after entry/coexistence approval. Shared main.void discovery belongs with
   the first project's tooling foundation; execution lowering is a separate compiler
   responsibility. The fixture-only catalog is not a general stdlib dependency mechanism.
5. Before long-running async-heavy application claims, measure retained tasks/frames
   and prioritize demonstrated growth. Large new compiler/cache/GC optimizations
   should follow evidence, not precede basic API usability.

## Executive summary for Boss and Wrench

1. **Already solid:** shared LLVM 19.1.7 JIT/AOT, precise roots, values/references,
   collections, bounded interfaces/generics, advanced errors/closures/cleanup,
   async/native ownership gates and the shared Stage 7 editor service.
2. **Biggest gaps:** usable text APIs, ordinary named/default arguments, built-in
   members/properties, storage encapsulation, stdlib import/discovery/docs, and a
   native/application boundary beyond the fixture.
3. **Issues that matter:** root-duration async retention, carefully preserved
   native/readonly guarantees, domain identity/visibility and the verified tooling
   gaps. The 64 KiB production blocker, deep-chain failure and blanket “no async/
   LSP/closures” statements are historical.
4. **First implementation sprint:** one cohesive nested-module, shared main.void
   project discovery and import IntelliSense foundation for stdlib plus project
   VFS, using the existing loader, immutable snapshots, symbol metadata and voidls.
   Optional-main execution follows the explicit host contract. Owner decisions are explicit
   in the companion roadmap; research has not authorized implementation.
5. **Can wait:** package registry/install UI, general dynamic/async FFI, runtime
   boxing, advanced object features, generic enums/unit sugar, views, weak refs,
   incremental compilation and speculative optimization. Begin a small reviewed
   library once the actual prerequisites land; do not wait for every Bible idea.
