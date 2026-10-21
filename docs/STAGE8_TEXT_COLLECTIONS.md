# Stage 8 Sprint 3 — UTF-8 text and recoverable collections

20 October 2026. **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending.**
[Local acceptance](stage8/SPRINT3_ACCEPTANCE.md) records 435/435 final unique suite
outcomes, the Release assertion repair and remaining platform/client boundaries. Starting commit verified:
`483ef87c3ec62c29c2bf1000529b480f0ba5ef2d`; initial worktree was clean.
[The documentation index](README.md) and [approval ledger](LANGUAGE_COMPLETENESS_APPROVALS.md)
govern scope. Sprint 1's Windows repair and Sprint 2 contracts must survive.

## Owner-approved public contract

Boss explicitly approved all three parts of the consolidated checkpoint.
The permanent approval is recorded in [DECISIONS.md](DECISIONS.md):

1. `text.length` and `text.byte_length` return i64 scalar and byte counts.
   `text[index]` returns a one-scalar string, accepts existing integer index types
   and list-style negative indexes, and uses the existing checked failure channel
   for invalid indexes. No grapheme, normalization or UTF-16 representation.
2. `text.to_utf8()` returns independent owned `[u8]` storage.
   `string.from_utf8(bytes)` accepts readonly or mutable `[u8]`, copies/validates
   all bytes and returns `result<string, string>`. Invalid encoding returns
   `Err("invalid UTF-8")`; embedded NUL survives. Resource failures keep the
   existing checked channel rather than masquerading as malformed input.
3. List and fixed-array `get(index)` return `result<T, string>` with
   `Err("index out of range")`; list `pop()` returns `result<T, string>` with
   `Err("empty list")`; dictionary `get(key)` returns `result<V, string>` with
   `Err("key not found")`. Reads preserve deep readonly authority and existing
   negative-index/value-copy/reference-identity rules. Pop requires mutable
   authority; only success changes contents/count/structural version.

## Implementation contract

The existing immutable UTF-8 payload, Heap, typed buffers, precise Frame/Slot roots,
source checked failure ABI and LLVM 19.1.7 lowering remain authoritative. There is
no second string representation, collector, allocator or per-string index cache.

`length`, `byte_length`, indexing and conversion are runtime operations. They do
not add compile-time execution of text/collection queries, borrowed views, slicing,
normalization, graphemes, interpolation, arbitrary primitive extensions or new methods
on result values. No imports are required for these compiler-owned APIs.

### Text and owned bytes

Scalar count scans the shared strict validator; byte count reads the existing
payload extent. Every integer index width is admitted, excluding bool/floats.
Negative normalization avoids signed overflow, including i64 minimum; large
unsigned indexes never wrap into negative indexes. Indexing stages one complete
scalar before allocation, yielding owned immutable text rather than a borrowed
slice. Indexing/properties cannot be assigned.

Encoding copies source text into native scratch before metadata admission or
allocation can poll. An existing u8 BufferBuilder stages owned bytes; a private
ordinary list header is published only after completion. Decoding captures the
selected backing/count under the Runtime lock, roots that backing and uses an
existing read lease while copying into native scratch. The lease ends before
string construction. Strict validation rejects overlong encodings, incomplete or
stray continuation sequences, surrogate scalars and values above U+10FFFF. NUL is
ordinary data. Malformed input is Err; OOM/resource failures are checked source
failures with ordinary cleanup. Failed native conversion does not replace its
output slot. Readonly byte lists are accepted without granting write authority.

The typed IR adds four string facilities: count (scalar/byte selector), scalar
index, encode and decode. The independent validator checks receiver, integer index,
exact u8 list/result shapes, ownership and flags. Both JIT modes and linked AOT
use the same Runtime helpers. Decode constructs the existing result variant with
zero inactive fields; no separate error representation or public ABI is introduced.

### Recoverable collections

`get(index)` is available separately on `[T]` and `[T,n]`; pop is list-only.
Dictionary get accepts the existing exact key type. Existing shared argument
mapping admits positional or the corresponding single named argument (`index`,
`key`, `bytes`); duplicate/unknown names and explicit generic arguments reject.

One lowerer composes existing count/contains, checked lookup/removal, branches and
Ok/Err construction. There are no new collection convenience opcodes or Runtime
storage operations. Signed bounds are widened safely, negative indexes normalize
against the captured collection's count and unsigned bounds compare without
truncation. Invalid indexes/missing keys do not execute checked lookup. Empty pop
branches before removal, leaving contents/count/version untouched. Successful pop
uses existing removal, including cleared vacated roots and structural versioning.

Receivers evaluate once before index/key expressions. Fixed receivers retain their
ordinary value snapshot; list/dictionary receivers retain the selected shared
identity. Index/key effects complete before lookup. Existing fixed/value child
copies, shared managed/list/dictionary identities, key equality and iterator timing
remain unchanged. No new cross-thread atomic lookup guarantee is made. Resource
failures retain existing checked-operation behavior; earlier successful mutations
or user effects are not generally rolled back.

Frontend capability analysis maps collection element/value facts into the Ok field.
Independent IR analysis checks the emitted ordinary operations and variant facts.
Readonly get remains observational and cannot thaw nested references; pop requires
mutable list authority even when the runtime list happens to be empty.

### Results, tooling and reload

Source-spelled results and compiler-produced results share one canonical factory;
per-use readonly field profiles remain separate from canonical nominal identity.
Compiler-owned metadata supplies properties, completion, hover, signatures and
references. Synthetic prelude records are readonly and never writable filesystem
navigation or document/workspace symbol targets. Editor clients receive no private
catalog or parser. Source declarations continue to navigate normally.

The analysis contract includes a versioned text/collection prelude marker. Existing
binding, capability, constant/default and layout restart checks remain enabled.
Compatible function-body edits preserve retained old generation leases. Sprint 2's
four numeric members and ordinary argument/default contracts remain unchanged;
Sprint 1's CI #82 discovery source/test hashes remain preserved.

## Verification contract (completed locally)

Focused engineering tests must cover text boundaries, exact scalar/byte counts,
ASCII/multibyte/combining/NUL/empty values, malformed encodings, owned-copy
independence and forced GC. Collection tests must cover all index widths and
negative boundaries, missing/present/empty cases, fixed child copies, nested
references, alias identity, readonly rejection, iterator timing and cleanup.
Use normal/O2 JIT and linked AOT, diagnostics, reload and compiler-owned tooling.
Measure repeated text traversal before considering indexing caches.

Executed consolidated local Debug, Release and strict
ASan/UBSan/leak suites plus both complete 1 MiB gates. Preserved failures and fixes
without inflating totals. Linux execution cannot establish Windows/macOS support.
No hosted CI is authorized; its seven-job gate remains deferred to Stage 8 end.
No root README edits, Git/remote mutations, publication, website work or Sprint 4.
Stop after Sprint 3 local acceptance for Wrench review.

## Preparation evidence

The existing Release build passed **7/7 focused baseline suites**, zero failures,
43.16 seconds: collection/dictionary/pattern/string source families and Sprint 1/2
LSP/numeric regressions. This is a baseline, not a fresh Sprint 3 build or acceptance.
[Machine-readable preparation evidence](stage8/sprint3-preparation.json) records
the exact command and log hash. No production code has changed.

## Debug stack regression and repair

The first Sprint 3 Debug depth check reproduced a SIGSEGV on a rejected 400-call
nominal chain at 1 MiB. New text/result hint temporaries had expanded the recursive
anchor frame from the prior Sprint 2 baseline. Cold helper boundaries now keep
text/result construction and text-index checking off recursive ordinary frames.
[Measured prologues](stage8/sprint3-stack-frames.json) show anchor **4896 → 3456**,
method_expression **1888 → 1760** and index_expression **3872 → 2912** bytes on
local Linux x86-64 Clang Debug. Semantic/parser depth limits and stack reserves
are unchanged. Debug nominal/numeric/string chains through 124 calls pass; 400-call
chains reject with nesting diagnostics and retain the later independent error.
The complete frontend 1 MiB gates and consolidated results belong in
[acceptance evidence](stage8/SPRINT3_ACCEPTANCE.md).

## Preserved admission boundaries

Builtin text/collection method values are not introduced. Direct calls, including
single named arguments and generic source wrappers, are the admitted API shape.
The existing conservative worker-reachability checker does not admit these new
synthetic method-call targets as known private worker callees. A focused fixed-array
get/spawn probe retains VOID1103 (“Worker reachable calls must be synchronous and
have known private effects”). This sprint does not broaden native worker admission
or add an async transfer protocol. Ordinary synchronous calls, serial async use and
existing worker programs keep their existing contracts; property/index operations
are separate from method-call admission. This boundary is not Windows evidence.

## Repeated traversal measurement

[Focused Release samples](stage8/sprint3-traversal.json) used strings containing
64/1,024/4,096 two-byte scalars. Repeating scalar count 128 times took
29/385/1,506 microseconds; repeating last-scalar indexing 32 times took
30/223/845 microseconds. These local samples include ordinary validation/allocation
costs and are not portable latency guarantees. They support retaining the initial
linear scan rather than adding a per-string index cache. Byte count reads the
stored byte extent. Full local verification retains its own independent results.
