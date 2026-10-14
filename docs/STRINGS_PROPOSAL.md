# Phase 22: UTF-8 strings proposal

Status: **S1-S8 approved by boss and wrench clanker, 14 October 2026**, including
the recommended 16 MiB per-constant and 64 MiB constant-pool limits. Full Phase 22
implementation is complete and locally verified, pending owner review and the
seven-job hosted gate. The alternatives below retain the
investigation rationale; the recommended contracts are now the approved choices.
STRINGS.md describes the implementation and WORK_LOG.md records the completed
six-configuration matrix, stress, stack and editor verification. No hosted
verification of Phase 22 is claimed.

## Recommendation

Introduce one immutable UTF-8 `string` with content equality, concatenation,
empty defaults and ordinary local/parameter/return/field storage together. Use the
existing coordinated heap, precise roots, checked failure channel and shared
JIT/AOT lowering. Nonempty strings own flat bytes in one traced leaf allocation;
zero machine storage denotes the empty string internally. Neither representation
nor a raw pointer becomes a public ABI.

Include pure string templates for module constants and declared/initializer
defaults, with runtime materialization at use. Include native helper proofs for
copying and borrowing, but no new general source FFI. Defer interpolation,
formatting, print, multiline text, indexing, slicing, iteration, ordering and
public length APIs. Reserve interpolation syntax from the beginning rather than
accepting future expressions as ordinary text.

The foundation must ship as one coherent slice. Literals alone without roots,
aggregate storage, complete publication, failure cleanup or reload-safe ownership
would be unsafe and would not meet this proposal.

## Confirmed commitments and evidence boundaries

LANGUAGE_BIBLE.md Sections 3, 7, 8 and 9 commit to one public UTF-8 string, an
empty-string default, automatic interpolation without a prefix, invariant
formatting, eventual simple print and no user-facing cstring/pointers. Section 8
originally left Unicode indexing, normalization, malformed input, NUL and format
grammar open. S1-S8 and Section 8.1 now record the approved foundation and future
direction. Triple quotes and custom display are future language commitments,
not facilities present in the current compiler.

The owner reports phases 1 through 21 accepted, Phase 21 hosted run #36 and the
subsequent pass improvement hosted run #37 each passing seven jobs. These are
owner-reported results, not remotely inspected results in this investigation.
Local `ctest -N` confirms 55 suites. The workflow contains Ubuntu Debug, Release
and Sanitizers, Windows Debug/Release and macOS ARM64 Debug/Release.

The original Bible/Strategy opening status and implementation-language discussion
are historical snapshots. DECISIONS.md and actual CMake/source settle C++20 and
exact LLVM 19.1.7. GC_RUNTIME.md retains historical source/automatic-budget
exclusions superseded by managed-source implementation. KNOWN_ISSUES.md and
INTERFACES.md still say the Phase 21 hosted gate is pending; the owner's current
report supersedes that status. This proposal records the discrepancy without
rewriting those guides or treating proposed Bible examples as current syntax.

## Architecture investigation

Paths below refer to current repository files, not hypothetical new subsystems.

| Area | Existing facility and concrete gap |
| --- | --- |
| Source bytes | `src/frontend/source.cpp`, `include/void/source.hpp`: immutable shared snapshots, line starts, strict scalar UTF-8 validation, original byte spans, scalar diagnostic columns. Invalid encoding is rejected even in comments; BOM and LF/CRLF/CR are handled. This does not decode string escapes. |
| Lexer/parser | `src/frontend/compiler.cpp`: every double-quoted token is currently `TokenKind::path`, ending at the next quote or line ending with no escape decoding. Only use/import consumes it. No general string token/expression exists. Import paths need their own unchanged grammar; adding backslash escapes to paths would silently change module semantics. |
| Types/AST | The same compiler owns Expr, Type resolution, semantic checking, typed defaults and retained validated analysis. `string` is absent from builtins and is currently available as an ordinary name. Introducing it reserves a new builtin name and must diagnose collisions. No second string checker or parser is needed. |
| Constants/defaults | `ir::ConstantValue` currently contains numeric bits and recursive fields, not byte strings. `constant_value`, field/default plans and lowering would need owned byte templates, including nested aggregates. Current pure contexts exclude managed allocation/init execution. Allowing pure string templates with fallible materialization is an explicit extension, not already implemented support. |
| IR | `include/void/ir.hpp`, `src/ir/module.cpp` and capability/construction includes provide one type model, independent validation, typed slots, field paths, complete construction and call/return checking. No string type, byte constant pool or string operation exists. Existing reference equality is identity equality and cannot implement string equality. |
| Reference shape | `reference_paths` drives both precise-root offsets and readonly/effect facts. `reference_value` currently means managed/interface references. Simply adding string to this predicate would also impose nullable/nominal/default/mutable-authority behavior that is wrong for immutable strings. Traceability and mutable authority need explicit classification in the same shared model. |
| Lowering | `src/backend/llvm/backend.cpp`: one lowerer for JIT and object emission, module-owned DataLayout, nested physical paths, parameter/local/result roots, loop polls, checked failures and no-GC call/return handoffs. Literal lowering currently handles numeric/bool/null, not owned text. |
| Runtime ownership | `Runtime` owns the existing non-moving Heap, frame registry, handles and coordinator. `construct_into` verifies a registered result root, collects according to its allocation budget, polls, then allocates/copies/publishes under the metadata lock. Fallible work precedes publication. Reuse this transaction, not a new allocator or collector. |
| Variable sizes | `Heap::allocate(TypeLayout)` accepts a positive byte size and numeric reference offsets. A string can be a leaf with no offsets. However admitted NominalDescriptor layouts and `validate_descriptor` require exact fixed size/offset matches. A variable tail cannot be disguised as one fixed nominal descriptor. There is no intrinsic string-kind/extent validation today. |
| Initialization | C1 value fields must be complete before init; S1 managed construction uses identity-free staged storage and commits once. Defaults, selected owners, aggregate snapshots and nested roots already exist. Strings must join those plans and must not gain unavailable-child exceptions or expose unfinished self. |
| Runtime ABI | `include/void/runtime_abi.hpp`, `src/runtime/abi.cpp`: private same-build noexcept helpers and RuntimeFailure. Managed operations require attachment and Managed state. Source CLI bootstrap creates the runtime, also for separately linked AOT. Current native host entry remains i32(i32); source functions have richer private signatures. |
| Reload | `src/runtime/reload.cpp`, backend compilation and validated-analysis contracts retain whole-generation leases and the runtime owner. Nominal/witness metadata contains owned data without executable pointers. String bytes must similarly survive allocating-generation retirement without retaining that generation. |
| Native text | `include/void/native.hpp`, `src/runtime/native.cpp`: native_utf8 validates scalars, rejects embedded NUL and returns an owning std::string for a terminated borrowed call. It neither allocates a VOID object nor roots a managed string nor defines retained-pointer ownership. Current trusted IR native imports are i32(i32), not general string FFI. |
| LSP/editor | `src/lsp/positions.cpp` shares scalar validation and converts retained byte positions to UTF-16, rejecting split scalars/surrogate pairs. Sessions own exact source snapshots. The TextMate grammar highlights quoted imports only; ordinary strings/escapes and the builtin type require additions. There is no existing interpolation or string semantic-token engine. |

Relevant implemented contracts are in SOURCE_COMPILER.md, GC_RUNTIME.md,
REFERENCE_VALUE_STRUCTS.md, INITIALIZATION.md, MANAGED_INITIALIZATION.md,
INHERITANCE.md, INTERFACES.md and ANALYSIS_COMPATIBILITY.md. The existing runtime
is experimental precise GC with substantial tests, not a production-ready GC.

## S1: Representation, ownership and empty values

Alternatives:

1. A flat intrinsic leaf in the existing heap, with an internal empty sentinel.
   One allocation and one root word per nonempty string; requires explicit
   variable-size intrinsic payload validation in the heap/runtime.
2. A fixed nominal header referencing a separate traced byte object. This can
   reuse fixed header metadata, but adds allocation, indirection and another
   payload classification for the tail. It does not eliminate variable-size work.
3. Static/JIT byte views, reference counting or a separately owned string heap.
   Views need permanent literal ownership or generation leases and a mixed root
   model. Reference counting needs pervasive copy/drop behavior. A separate heap
   duplicates lifetime machinery. None is recommended for this foundation.

Recommend option 1. Add a distinct builtin string Type kind in the existing model,
not a user-managed struct, interface, nullable reference or new type system.
Conceptually, a nonempty payload is `{u64 byte_count; bytes[byte_count]}` with
target-correct internal header alignment and no traced children. Its root is the
whole allocation address. Byte interiors, source snapshots and LLVM globals are
never managed roots. Do not rely on a terminator or expose the header layout.

Zero storage means the valid empty string, not source null. All producers
canonicalize zero-length output to that representation. This avoids allocation
and failure for implicit empty defaults. An alternative is a runtime-owned rooted
empty singleton, which costs startup/root policy without an observable benefit.
Reject source null and `string?` for Phase 22. Nullable strings are subsequently
approved in principle as future direction in Language Bible Section 8.1; their
remaining interactions are future work. Internal zero initialization must not
trigger managed nonnull guards.

Extend existing heap allocation records/layout validation with a private intrinsic
leaf kind and checked byte extent, rather than interning one nominal descriptor
per length or accepting any untyped object as text. No code/LLVM/AST pointer or
generation lease belongs in the payload or its metadata. Typed nominal admission,
ancestry and interface discovery remain unchanged; strings do not participate in
inheritance, using composition, receiver extensions or interface conformance in
this slice. Intrinsic metadata is runtime-owned and bounded independently of the
number of distinct string lengths.

Payloads are immutable after complete publication. Copies copy the root word and
share immutable bytes; object identity is unobservable. Interning, small-string
optimization, ropes and hash caching are unnecessary initially. Flat repeated
concatenation can be quadratic; do not claim builder performance.

## S2: GC, publication and execution contracts

Strings are traced leaves, including in nested values, managed fields, inherited
payloads, interface method signatures, selected targets, arguments and results.
Extend the existing bounded logical shape traversal with leaf classification:
tracing includes strings, while mutable-authority requirements exclude immutable
string referents. Reuse the traversal and physical mapper; do not duplicate the
aggregate layout walker or omit strings from GC to bypass readonly checks.

Plain/const string formals both accept immutable text. Const still prevents
rebinding its binding. Copying a const carrier to var can permit replacing its
independent string field while preserving all pre-existing readonly managed and
interface leaves. Replacing a field through a readonly managed owner is forbidden.
String retention is still reachability even though it grants no mutation right.
Audit frontend effects, independent IR proof and interface/virtual summaries for
these distinctions. Existing numeric, managed and interface rules do not weaken.

The shared lowerer must register every string-bearing parameter, local, actual
receiver/stage field and instruction-result home before any poll/allocation. Keep
left-to-right snapshots rooted while later arguments/RHS values allocate. Reuse
fixed loop slots, zero-before-registration and existing failure exits. The callee
seeds roots before its first poll; return unlink/return/caller store remains a
no-safepoint handoff. Allocation returns through an already registered output root.
Dead homes may retain strings until function return, as with current references.

For the first implementation, recommend a runtime helper that prepares a complete
header/byte payload in bounded native scratch storage, then invokes the existing
complete-publication transaction. Scratch uses ordinary RAII bytes, contains no
managed pointers and never grows on the native stack with string length. Literal
materialization copies leased constant bytes. Concatenation operands stay rooted
in Managed state while helpers validate extents and copy their bytes; scratch may
then survive collection/poll in construct_into because it owns its complete copy.
No native callback or state transition occurs during an unprotected managed read.

The intrinsic kind must be installed atomically with the existing heap record,
before final root publication. Allocation budget, collection, poll, lock order,
failed-rendezvous behavior and allocation fault hooks must remain equivalent to
construct_into. A narrow extension of that common transaction is acceptable if
the intrinsic tag cannot be supplied through today's signature. Do not create a
parallel string allocation/publication algorithm. A later direct-fill optimization
requires its own proof that copying is infallible after allocation, inputs remain
rooted and no incomplete object becomes visible.

Scratch/heap/map/collection failures leave the destination unchanged and propagate
the existing checked source failure channel. Check byte-count addition, header
addition, size_t conversion and scratch-container limits before allocation/copy.
The representable runtime limit is the minimum of u64 and target/storage limits,
including header space. Overflow gets a deterministic checked size failure, not
wrapping allocation followed by an overrun. No C++ unwind crosses generated code.
Earlier argument/initializer effects remain, with no retry or rollback promise.
Failure message storage follows the existing runtime/code-lease lifetime contract.

## S3: Unicode, literals and future interpolation

Recommend strict valid scalar UTF-8 for every string producer. Source validation
already rejects overlong encodings, surrogate scalars, out-of-range values,
truncation and stray continuations. Reuse that policy for decoded literals and
future native imports. The runtime currently has a separate native_utf8 validator;
share a small LLVM-independent validator usable by core and runtime if practical,
while preserving the terminated bridge's additional NUL rejection. Do not link
the runtime to the frontend or LLVM just to validate bytes.

Proposed literal grammar is single-line double quotes, raw valid UTF-8 text and
escapes `\"`, `\\`, `\n`, `\r`, `\t`, `\0`, and `\u{H...}` (one through six hex
digits naming a Unicode scalar). Reject unknown escapes, `\xNN`, UTF-16 surrogate
pairs, surrogate/out-of-range escapes, missing braces, unterminated quotes and raw
line breaks. Require escapes for raw ASCII control characters including NUL, tab
and DEL. Physical LF/CRLF/CR never becomes literal content implicitly. Single
quotes, triple quotes and line continuations diagnose as deferred/unsupported.
Import strings keep their current path policy and do not gain interpolation.

Recommend preserving decoded bytes exactly with no normalization, case folding
or locale dependence. Equality compares byte length and all bytes. Thus composed
and decomposed spellings can differ even when they look alike. Alternatives are
canonical-normalized storage/equality, or scalar/text equivalence as a separate
API; both require additional policy and Unicode tables. Strict UTF-8 gives each
scalar sequence one encoding, but does not equate canonical spellings or graphemes.

Reserve unescaped single braces for automatic interpolation. In this slice,
`"hello {name}"` must produce a targeted deferred-interpolation diagnostic, not
the literal characters `{name}`. Accept doubled `{{` and `}}` as literal braces,
consistent with the Bible. Reject unmatched single braces. Recognize brace syntax
in source text before escape decoding: `\u{7b}` produces a literal brace, and
decoded escape output is never reparsed as interpolation. No f/$ prefix is added.
This compatibility rule is approved for Phase 22 implementation.

Tokens/AST must retain both exact written byte spans and owned decoded content.
Diagnostics point into written source, not offsets in decoded bytes. LSP still
reports UTF-16 positions and tools still use immutable snapshots. Recovery must
stop an unterminated single-line literal at the physical line boundary so later
independent diagnostics survive. Syntax coloring must keep #/keywords inside text
from being interpreted as code and must recognize escapes/doubled braces.

## S4: Length, characters, indexing and slicing

Approve byte-based storage/extent and strict decoding now. Internal byte_count is
u64; target allocation checks remain mandatory on narrower hosts. A Unicode scalar
is not a user-perceived character: combining sequences and multi-scalar emoji make
those notions different. Never call a UTF-8 byte a character.

Options for future indexing include explicit byte access, scalar indexing or
grapheme indexing. Bytes are simple but can split valid text; scalar random access
is not constant time without an index; graphemes need versioned segmentation rules.
Recommend deferring all indexing, slicing, iteration, public byte_count/count/length
APIs and a public char type. Reject attempted uses rather than selecting a unit
implicitly. A later byte slice returning string must validate scalar boundaries;
an arbitrary byte slice would instead need a future byte-container contract.
No Unicode version, grapheme algorithm or external length ABI is selected here.
Subsequent owner approval selects future scalar indexing and the length/byte_length
spellings in Language Bible Section 8.1, without expanding this implementation.

## S5: Operations, constants and initialization

Proposed operations are exact string assignment/copy, `==`, `!=` and binary `+`
for two strings. Equality is ordinal byte content, not address identity. Empty
strings compare equal. Concatenation evaluates operands once left to right and
returns their complete UTF-8 byte sequences in order. Valid UTF-8 remains valid at
the concatenation boundary. Include `+=` as existing select-once assignment of
concatenation; do not add mutation of shared bytes. No mixed numeric/bool addition,
implicit conversion, truthiness, ordering, repetition or string numeric cast.

Defaults are empty for locals and string fields, including recursively nested
values and managed stages. A string field is available before custom init; it is
not a required nonnullable managed child. C1 all-fields-valid and S1/P4 complete
publication stay intact. Explicit declared string defaults and initializer
parameter defaults use pure compile-time templates. Ordinary function defaults
remain deferred under the existing rule; string support does not enable them.

Recommend extending ConstantValue with a typed owned byte-template alternative,
and recursive aggregate support, rather than packing bytes into numeric bits or
host pointers. Permit literals, constant references, string concatenation and
string equality in existing pure contexts. Calls/runtime formatting/init execution
remain forbidden there. Compilation evaluates bytes without a Runtime; execution
materializes nonempty templates through S2 when needed. No eager runtime module
initialization, permanent string cache or hidden module root table is required.

This distinguishes semantic purity of a template from fallible runtime storage
materialization. It deliberately extends today's all-null reference-template
restriction for immutable text only. A conservative alternative is empty-only
pure defaults/module constants initially, but it makes ordinary named text and
field defaults inconsistent with the useful first slice. Owner approval must
include this extension. Nested value/default construction materializes and roots
text before parent publication; nonempty field defaults finish before init bodies.

Local const is a readonly runtime binding and can hold runtime concatenation.
Var copied from it may be rebound since bytes are always immutable. Existing
const/default token contracts remain exact, including spelling and dependency
ownership, not merely decoded equality. Runtime literal/concatenation allocation
counts and identity are not language observables. Initially keep runtime `+`
unfolded; only the specified pure contexts need string constant evaluation.

Proposed compiler resource limits for approval: 16 MiB of decoded bytes per
literal or folded string result, and 64 MiB of owned unique decoded constant-pool
bytes per lowered module. Check before large intermediate allocation; use the
existing diagnosed resource-limit style and bounded constant dependency walk.
These are compilation limits, not a 16 MiB runtime string maximum. Their numbers
are proposed engineering limits, not Bible commitments or measured performance.

## S6: Embedded NUL and native boundaries

Recommend permitting U+0000 as ordinary content through `\0` or `\u{0}`.
Length-aware equality, concatenation, storage and copies preserve every byte after
it. Rejecting all NUL would unnecessarily impose a C terminated-API restriction
on language strings. Ordinary payloads need no trailing NUL.

Keep native_utf8's existing NUL rejection for terminated C APIs. A trusted wrapper
obtains a validated length-aware copy while Managed and then invokes native_utf8
to own a terminated temporary. Only afterward may it enter NativeSafe and pass
that stable temporary to the synchronous C call. Reject embedded NUL before the
provider is invoked. Allocation/validation errors use the boundary's checked
failure policy. No silent truncation, replacement or returning a borrowed c_str.

For pointer-plus-length APIs, recommend a binding-owned byte copy initially,
which may include NUL. A future zero-copy borrow needs a whole-object root/handle,
explicit byte extent, frozen lifetime and no writes, and must obey the current
ban on dereferencing managed objects from NativeSafe code. Non-moving GC alone
does not prove a legal borrow. Do not silently add such a borrow to this phase.

Retained native text requires binding-owned copied storage lasting until the
provider's release/quiescence point. A call-local copy is insufficient. Existing
callback handles/provider barriers remain authoritative and do not automatically
cover arbitrary retained text APIs. Recommend deferring public retained-string
bindings; document and prove the trusted copying lifecycle with a fixture.

Future native-return conversion must copy while the provider buffer is valid,
validate strict UTF-8, then materialize a rooted VOID value. Malformed bytes fail
without replacement; null pointer versus empty result must be defined per binding,
never by treating an arbitrary pointer as a string. No source-level cstring,
unsafe annotation, string pointer export or public aggregate ABI is introduced.

## S7: JIT/AOT, reload and future features

Use the same owned IR byte pool and string operations for JIT and AOT. LLVM globals
may supply bytes only during a leased materialization call; helpers must copy
before returning. Persistent string objects contain none of those addresses.
Runtime metadata must not retain pool/global pointers. Retiring an allocating
generation therefore cannot invalidate stored strings or pin old code.

Mark modules coordinated whenever strings occur in relevant signatures, locals,
constants, fields or operations, even with no nominal managed struct declaration.
Today's inference scans nominal declarations, so a string-only program is a
specific missing case. Route it through the current runtime bootstrap for CLI
and AOT. Preserve i32 entry adaptation and private source-call conventions; do
not mistake a pointer-width value for permission to add an external string ABI.

Compatibility records include builtin type/layout policy, exact const/default
tokens, effects and construction dependencies. Ordinary body var literal changes
can reload where existing body rules permit; const/default/init/helper-policy
changes still require restart. Runtime identity and whole-generation leases stay
authoritative. Exercise an old rooted string-bearing holder after allocating-code
retirement from newly compiled code in both JIT and independent AOT generation
proofs. Native diagnostics must still copy generation strings before lease release.

Later interpolation can elaborate typed text/expression segments through the
same checker and concat/materialization primitives, ideally a checked multi-part
builder to avoid quadratic chains. Invariant numeric formatting should reuse
established numeric semantics rather than locale-dependent host defaults. Print
can consume length-aware UTF-8, including a separately approved NUL/output policy.
Collections can hold the same immutable traced leaves; content hashing can follow
the chosen equality. No builder, formatter, Unicode library, print ABI or collection
implementation is authorized by this proposal.

## S8: Scope and approval checklist

The owner explicitly approved every recommended selection below, including S5's
resource limits. The checklist remains a review reference for the implementation:

| Decision | Recommended selection |
| --- | --- |
| S1 | Immutable flat intrinsic leaf on the existing heap; internal zero means empty; no public null/string? or ABI. |
| S2 | Existing complete-publication transaction, precise roots and checked failures; explicit immutable-leaf classification; native scratch first. |
| S3 | Strict scalar UTF-8, listed escapes, exact bytes/no normalization; doubled braces accepted, interpolation syntax reserved with targeted rejection. |
| S4 | No indexing/slicing/iteration/char/public length yet; no implicit character unit. |
| S5 | Content equality, +/+=, empty defaults, pure owned byte templates and fallible materialization; proposed compile-time resource bounds. |
| S6 | Embedded NUL allowed in strings, rejected at terminated C bridge; trusted owned-copy proofs only, broader FFI deferred. |
| S7 | Shared JIT/AOT/runtime path; copied literal bytes, exact compatibility and generation-independent heap lifetime. |
| S8 | Ship this foundation together, defer broader string APIs and retain all acceptance gates. |

Deferred features remain future work, not permanent bans: interpolation evaluation,
format specifiers/alignment/custom display, print, triple-quote trimming, indexing,
slicing, iteration, public length/char APIs, nullable strings, normalization APIs,
ordering/search, builders, interning, collections, native binding syntax, retained
managed borrows, open-world loading, new threading semantics and public string ABI.

## Example programs under the recommended contract

These are proposed future programs, not programs accepted by the current compiler.

```swift
const greeting = "Hello, "

value struct label:
    text: string

struct message:
    title: string = "untitled"
    payload: label

func echo(text: string) -> string:
    return text

func entry(value: i32) -> i32:
    var empty: string
    var m = message(payload = label(text = greeting + "世界"))
    const snapshot = echo(m.payload.text)
    var changed = snapshot
    changed += "!"
    if empty == "" and snapshot == "Hello, 世界" and changed != snapshot:
        return 1
    return 0
```

This exercises default/nested storage and runtime calls while retaining the
existing i32 CLI adapter. It does not require print, interpolation or collections.

```swift
value struct text_box:
    text: string

func (b text_box) init(text: string = "ready"):
    b.text = text

func nul_and_unicode() -> bool:
    var nul = "a\0b"
    var composed = "\u{e9}"
    var decomposed = "e\u{301}"
    return nul == "a" + "\u{0}" + "b" and composed != decomposed

func braces() -> string:
    return "{{name}}" # The bytes {name}; no expression evaluation yet.
```

Proposed rejected fragments, each considered independently:

```swift
var bad: string = null      # No public null string.
var optional: string?       # Nullable strings deferred.
var sum = "score " + 1      # No implicit numeric conversion.
var text = "hello {name}"   # Automatic interpolation is reserved, still deferred.
var broken = "\u{d800}"     # Not a Unicode scalar.
var bytes = "\xFF"          # Arbitrary byte escapes forbidden.
var unknown = "\q"          # Unknown escape.
var order = "a" < "b"       # Ordering deferred.
var first = text[0]         # Indexing deferred.
var length = text.length    # No public length unit selected.
```

```swift
struct box:
    text: string

func invalid():
    const name = "one"
    name = "two"           # Readonly binding cannot be rebound.
    const b = box()
    b.text = "two"         # Readonly owner cannot have its field replaced.
```

Passing `"a\0b"` to an eventual terminated native API is a checked boundary
failure, not an invalid VOID literal. Passing the same content with an explicit
length to an authorized binary-safe native binding can preserve it.

## Proposed implementation sequence after approval

1. Record approved S1-S8 choices and update current semantic commitments only
   with the owner's authorization. Preserve LLVM 19.1.7, owner-only Git and the
   current architecture. Identify obsolete string-deferral assertions narrowly;
   obtain authorization to replace them with positive coverage, never skip them.
2. Add owned byte constants and one builtin Type/leaf classification through
   shared analysis, defaults, capability/construction facts and independent IR
   validation. Add typed literal/concat/equality operations and validated pool
   references. Keep original spelling/spans for contracts and diagnostics.
3. Extend common heap publication for intrinsic validated leaf payloads and
   checked size/scratch handling. Prove empty, NUL, OOM, budget collection and
   exact output protection before enabling source execution.
4. Extend the existing lexer/parser/checker and pure evaluator, keeping imports
   separate. Add literals, +/+= and equality/default plans without enabling other
   types' operators. Enforce syntax/resource diagnostics before lowering.
5. Extend shared LLVM type/layout/root/call/failure lowering and coordinated
   inference. Bind private helpers in ORC and runtime-only AOT linking. Prove
   stage/aggregate/selected-target and parameter/return storage under O2.
6. Extend exact analysis/reload contracts and cross-generation proofs. Add
   trusted native copying/termination/retention-fixture tests without general FFI.
7. Update LSP source-span regressions and TextMate type/text/escape rules, run
   real unsaved-edit/failure/repair integration and package the VSIX. Document
   implemented versus deferred scope and actual acceptance evidence.

## Positive and negative test strategy

- Source/Unicode: empty/ASCII/multibyte/astral/combining text, every supported
  escape, braces and escaped braces, embedded NUL with a nonempty suffix, source
  BOM/line endings and no final newline. Compare exact bytes and composition
  inequality. Reject malformed UTF-8, invalid escapes/control bytes, raw newlines,
  lone braces, deferred interpolation/multiline syntax and resource overflows.
- Semantics: inference, annotated empty locals, imported/root/local consts,
  nested pure templates, field/init defaults, argument/return calls, recursion,
  compound selected targets and left-to-right side effects. Test mutable var
  copies of const strings/carriers and readonly owner rejection. Keep mutable
  managed/interface leaf restrictions intact in mixed carriers.
- Independent IR: forged type/pool indexes, invalid UTF-8 constants, invalid
  intrinsic/null/nominal casts, wrong concat/equality operands, missing
  coordination, illegal mutable stores, malformed aggregate/default facts and
  incorrect capability/initialization summaries must fail before LLVM execution.
- GC/runtime: force collection at budgets 1/8, loops/backedges, nested calls,
  returned temporaries, selected owners, staged/inherited fields, copied values,
  interface/virtual string signatures and simultaneous mutators. Inspect actual
  registered roots. Verify empty/null root distinction, leaf non-scanning,
  cross-runtime misuse rejection, intrinsic tag/length checks and balanced frames.
- Failure: inject scratch allocation, heap allocation/map and collection failures;
  test length/header overflow without enormous allocations. Verify unchanged
  outputs, no partial object, retained earlier effects, no sweep after failed
  rendezvous, subsequent recovery, no leaks and no exceptions across generated code.
- Reload: rooted old strings and holder fields survive old-code retirement; new
  code compares/concatenates them correctly. Body literals may change when allowed;
  const/default/type/layout/init dependency changes reject publication. Confirm
  eventual object reclamation and generation retirement are independent.
- Native: preserve all existing malformed/NUL rejection assertions; test exact
  pointer+length copies, termination, rejection before provider side effects,
  retained-copy release after provider quiescence and malformed native returns.
  No managed pointer survives merely because the native heap is non-moving.
- Tools: real non-ASCII source before/inside/after escape diagnostics, UTF-16
  astral positions, independent recovery, immutable old snapshots, unsaved repair
  and restored navigation. Grammar checks text containing #/keywords, quotes,
  escapes and braces; actual editor integration and VSIX contents are required.

## Acceptance and risks

Preserve all 55 existing suites and add focused source, independent IR/runtime,
normal/O2 JIT, separately linked normal/O2 AOT, native and tooling regressions.
Run both complete LLVM 19.1.7 SDKs in Debug, Release and strict ASan/UBSan with
leak detection, repeated GC/allocation-failure/reload stress, 1 MiB stack checks,
real editor integration and VSIX packaging. Generated application code currently
does not acquire full ASan/UBSan instrumentation merely by instrumenting the C++
runtime; retain explicit IR/runtime bounds and publication tests. All seven hosted
jobs remain the acceptance gate, with actual Windows/macOS execution. Local Linux
execution cannot certify their ABI, linker, UTF-8 console or scheduling behavior.

Principal risks are shared shape/authority coupling, incorrect empty/default
guards, a string-only program missing coordination, variable-sized payloads being
mistaken for nominal fixed layouts, unrooted temporaries under O2, decoded/source
span confusion, hidden JIT literal borrows and accidental NUL truncation. Mitigate
with the independent proofs above, not optimizer disabling or assertion weakening.
Scratch duplicates payload memory transiently; long concatenation chains remain
expensive. Compiler byte quotas and existing nesting/layout limits do not imply
a total runtime memory quota or hard real-time collection bound.

The investigation reproduced raw malformed bytes in VOID1001 CLI excerpts. The
owner subsequently approved a narrow Phase 22 diagnostic amendment, superseding
the original document-only boundary for this defect. The existing renderer now
escapes malformed bytes as `\xHH` with matching caret widths, preserving source
bytes, spans, rejection and editor positions. Strict CLI and snapshot/LSP regressions
verify the correction; DIAGNOSTICS.md and WORK_LOG.md record the details. This
amendment is additional to the separately approved S1-S8 string implementation.

S1-S8 implementation is authorized. Completion requires the implementation and
verification above, followed by owner review before hosted CI. Separate approved
future scalar indexing, length/byte_length and nullable-string direction is recorded
in LANGUAGE_BIBLE.md and does not expand Phase 22 implementation scope.
