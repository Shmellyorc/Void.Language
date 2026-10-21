# UTF-8 string foundation, Phase 22

## Stage 8 Sprint 3 — current extension

The owner-approved [text/collection contract](STAGE8_TEXT_COLLECTIONS.md) supersedes
the dated get/pop/text-inspection exclusions below. Sprint 3 is locally verified;
[acceptance evidence](stage8/SPRINT3_ACCEPTANCE.md) records exact results and limits.
Wrench review remains pending. Public policies are approved
in [DECISIONS.md](DECISIONS.md); hosted CI remains deferred to Stage 8 end.


Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

S1-S8 in STRINGS_PROPOSAL.md are owner-approved, including resource limits. This
guide describes the implemented Phase 22 contract. Phases 1–25 are now hosted
accepted at the baseline linked above; the following matrix is historical evidence. WORK_LOG.md records the actual matrix and acceptance status.
All six SDK/configuration matrices passed 59/59, with 306 stress and 42 small-stack
executions plus real editor/unit/VSIX checks. The runtime remains
an experimental precise-GC foundation, not a production-ready GC.

## Source behavior

One builtin immutable `string` supports quoted UTF-8 literals, local bindings,
parameters/returns, module constants, declared field/initializer parameter
defaults, nested value storage and managed/inherited fields. Empty is the safe
default. Assignment/copy shares immutable content. `==`/`!=` compare every byte;
`+` concatenates two strings and `+=` rebinds existing writable storage. Operands
evaluate once left to right; compound assignment snapshots its selected owner and
old value before evaluating the RHS. String identity is not exposed.

```swift
const greeting = "Hello, "

value struct label:
    text: string

struct message:
    title: string = "untitled"
    payload: label

func echo(const text: string) -> string:
    return text

func entry(value: i32) -> i32:
    var empty: string
    var m = message(payload = label(text = greeting + "世界"))
    var copy = echo(m.payload.text)
    copy += "!"
    if empty == "" and copy == "Hello, 世界!":
        return value + 1
    return 0
```

Strict scalar UTF-8 is required. Supported escapes are `\"`, `\\`, `\n`, `\r`,
`\t`, `\0` and `\u{H...}` with one through six hexadecimal digits for a valid
scalar. Unknown escapes, arbitrary byte escapes, surrogates, out-of-range values,
raw ASCII controls/DEL, raw line breaks, single/triple quotes and line continuation
are rejected. Physical LF/CRLF/CR never becomes literal content implicitly. Import
paths retain their existing separate grammar and portable filename restrictions.

There is no normalization, case folding or locale comparison. Thus `"é"` equals
`"\u{e9}"` and differs from `"e\u{301}"`. Embedded NUL is ordinary content:
`"a\0b"` equals `"a" + "\u{0}" + "b"`. C terminated-API restrictions do not
truncate or restrict ordinary string values.

`{{`/`}}` denote literal braces. Stage 8 Sprint 6 now admits single-brace
substitutions containing ordinary single-line string expressions, including
`"Hello, {name}"`. `\u{7b}` yields a literal brace; decoded escapes are never
reparsed as interpolation. Numeric/bool/custom display and formatting remain
deferred; see STAGE8_ERGONOMICS.md. Numeric/bool concatenation, ordering, casts and truthiness are not
implicitly introduced by string support.

Const bindings cannot be rebound. Payload immutability applies to all bindings;
copying a const string into var permits rebinding the var. Plain and const string
formals accept immutable text; const also protects the parameter binding. Copied
value carriers can replace their independent string slots without thawing any
managed/interface leaves. Replacing a field through a readonly managed owner
remains forbidden. Interface/virtual methods can use exact string signatures
under their existing conformance/effect rules.

Pure module constants and declared/initializer defaults hold owned byte templates,
including nested aggregates, concatenation and equality. They perform no runtime
allocation during analysis. Nonempty templates materialize fallibly at execution
use; no runtime module initializer or permanent cache is added. Stage 8 Sprint 2 also admits ordinary pure function/method defaults using the
same binder; see [the calls contract](STAGE8_CALLS_NUMERIC.md).
C1 fields are valid before init and S1 managed stages commit identity once. String
fields safely default to empty, not unavailable nonnullable managed references.

Compiler limits are 16 MiB decoded bytes per literal/folded result and 64 MiB of
unique owned byte templates in the compiled module pool, including declaration
metadata. Limits diagnose before lowering or oversized copies. Original literal
spelling remains in exact const/default/reload contracts even when decoded bytes
are equal. These are compiler limits, not a 16 MiB runtime string maximum.

## Shared representation and memory

The existing Type adds an intrinsic string kind. ConstantValue adds owned text;
the IR module owns unique byte templates and validates indexes, UTF-8 and limits.
Explicit string literal/concatenation/content-equality operations use the existing
independent validator and shared LLVM lowering. Numeric/reference identity
operators do not silently become string operators.

An empty string is a zero root word internally, while source string is non-nullable.
Nonempty strings occupy one allocation in the existing Heap with a private
intrinsic leaf kind, u64 byte extent and copied bytes. They have no traced children,
terminator requirement, nominal descriptor per length or code pointer. This is
not a public string/aggregate ABI. Source null/string? and receiver extensions,
using composition or standalone interface erasure of strings remain rejected.

The existing bounded reference-shape walker classifies immutable string leaves
for tracing separately from mutable-reference authority. LLVM maps those leaves
through module-owned target layout offsets, including nested values, inherited
payloads, actual stage/receiver storage, local/result homes and parameter roots.
The existing no-GC argument/return handoffs and fixed loop roots remain intact.
Root homes may retain dead text until function return; destruction timing and
minimal liveness are not promised.

Runtime helpers prepare complete header/byte scratch buffers containing no managed
pointers, then reuse Runtime::construct_into. Size/header addition is checked before
copy/allocation. Intrinsic layout/UTF-8 validation precedes heap allocation. The
common allocation budget, collection/poll, lock order and final publication are
preserved. All fallible work precedes output replacement. Failure follows the
existing checked source channel with balanced frames and no native unwind; earlier
argument/init effects are not rolled back. Scratch transiently duplicates payload
memory. Repeated flat concatenation may be quadratic; no builder claim is made.

String-only modules participate in coordination and the existing runtime bootstrap.
Normal/O2 JIT and linked AOT use the same helpers. CLI entry remains i32(i32).
Literal globals are copied during a leased call, never retained by heap objects.
Runtime-owned payloads and tracing metadata survive allocating-generation retirement
without pinning executable code. Const/default/init dependency changes still
require restart; otherwise-compatible function-body literal edits can reload.

## Trusted native and tooling boundaries

Native UTF-8 validation shares the LLVM-independent scalar utility with source/IR.
The existing native_utf8 bridge still owns a terminated temporary and rejects NUL.
Copy managed text while Managed, then pass only the owned native copy across a
NativeSafe call. Pointer-plus-length copies preserve NUL. Managed-only byte views
must not escape their root/Managed lifetime. Retained APIs need binding-owned storage
and provider release/quiescence; no public retained-pointer/managed-borrow API is
added. Future native returns must copy and validate while provider storage is valid.

Tokens retain written spans and decoded bytes independently. Shared analysis and
LSP keep original snapshots/UTF-16 positions, imported constant navigation and
invalid-revision suppression/repair. Unterminated single-line text recovers at its
physical line boundary. The existing editor grammar colors string/type/escape
regions while keeping # and keywords inside strings as text. The accepted VOID1001
malformed-byte diagnostic correction remains documented in DIAGNOSTICS.md.

## Deferred approved direction

LANGUAGE_BIBLE.md Section 8.1 records approved future scalar indexing returning
one-scalar strings, scalar `.length`, UTF-8 `.byte_length`, nullable strings in
principle and automatic interpolation. None is implemented in Phase 22. Index
error behavior, slicing and ordering semantics remain open. Printing, formatting,
multiline trimming, grapheme APIs, collections/builders, native binding syntax,
interning and public string ABI remain deferred.

## Verification

The strings family adds source, normal/optimized runtime-only AOT and CLI suites,
preserving all 55 existing suites for 59 total. It exercises actual root slots,
budget-one collection, source/IR negatives and resource bounds, managed/value/init/
interface/virtual uses, checked OOM and recovery, exact Unicode/NUL behavior,
compound selected owners, active reload leases and old-holder/new-AOT-code access.
The process-level LSP and real editor regressions include unsaved string failure
and repair. See WORK_LOG.md for completed commands, results and remaining gates.


## Stage 8 Sprint 6 — 21 October 2026

[The bounded ergonomics contract](STAGE8_ERGONOMICS.md) supersedes historical
blanket exclusions above for compile-time type `has`, typed if-expressions,
multiline calls and string-only interpolation. Existing nominal storage,
structural conformance, generic specialization, capabilities and checked failure
rules remain authoritative. Both conditional branches type-check. Formatting and
numeric/bool/custom string conversion remain deferred. Approval and implementation
are separate from local acceptance, Wrench review and the final hosted lock.
