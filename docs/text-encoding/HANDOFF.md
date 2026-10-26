# Text, hex, Base64 and ASCII — Golden Wrench handoff

Baseline e24d3a95db03fd282c87a5d9b885471057e38798. Implementation authorized by Boss.
**Local implementation, final acceptance and integrity audit are complete. Stop for Golden Wrench review.**

## Source and contract

Four ordinary source modules implement the [contract](CONTRACT.md). The compiler's
[73-declaration inventory](API.md) records actual signatures/source locations and
complete descriptions, parameter documentation and returns, without public warnings.

Text uses immutable UTF-8 values, scalar boundaries/indexes, exact Unicode 16.0
White_Space, KMP literal matching and geometric byte accumulation. It provides all
requested inspection/search/extraction/split/join/replacement/repeat/padding APIs.
Empty fields and NUL survive. Recoverable ranges and construction limits use a
managed text.error extending the shared base; Runtime allocation failure stays on
the checked resource-failure channel.

Boss confirmed plain-text encode plus recoverable try_encode for both codecs.
Decode returns result with independent bytes. Try_encode reports output-limit errors;
plain encode follows the checked Runtime failure policy for an exceeded output
budget. Allocation exhaustion remains a checked Runtime failure for both variants.
Both variants are implemented through one shared encoding body. Hex is strict
pairs with explicit uppercase output.
Base64 requires explicit standard/URL-safe and required/omitted policies, rejects
whitespace/mixed alphabets/malformed padding and validates unused trailing bits.
ASCII strictly converts all 128 ASCII values and rejects high bytes/scalars; byte
classification and case conversion are exhaustively exercised for all 256 u8 values.

All UTF-8 validation/construction uses existing authoritative operations. No new
string representation, collection system, scheduler, public intrinsic syntax,
regex or OS module was introduced. Source construction uses a 64 MiB bound;
split field limits default to 1,048,576 and are caller-selectable.

## Necessary native support

One canonical-text-only helper implements Unicode 16.0 full default lower/uppercase,
including expansions and Final_Sigma. Embedded mappings and properties are pinned,
with [generator provenance/hashes](evidence/unicode.json) and a bundled Unicode
license installed with compiler/tooling. No Python/Perl/Unicode library is needed
by the resulting executable. Source arrays remain the accumulation mechanism.

The helper stages native owned input/output before ordinary Runtime string
publication, uses the shared UTF-8 scalar validator and preserves precise roots
and checked allocation cleanup. Independent IR validation rejects forged authority,
operand/result/shape/coordinated-state cases. JIT/O2/AOT share one lowering.

The helper does not normalize, fold case or apply Turkish/Azeri/Lithuanian locale
tailorings. Those are deliberately excluded from locale-independent default casing.
No global resolver ordering or initialization policy was changed by this sprint.

## Crash found, preserved and repaired

A real Debug 1 MiB compiler crash was found in the existing full semantic-depth
gate. The exact [deep-methods_crash.void](evidence/crashes/incident-splghldg/inputs/deep-methods_crash.void)
was copied before temporary cleanup; original/copy SHA-256 is
`73871bef34c030e336699c8e7ff706917428d0450dfa5e7c7b923c9419245b00`.
The [incident receipt](evidence/crashes/incident-splghldg/receipt.json) retains exact
command/cwd/SIGSEGV/output, executable hash, source set and build flags; readonly
provenance includes the pre-repair compiler and generator. The original binary is
retained privately in build/text-encoding-crash-binaries/voidc-before-stack-repair.
No executable or core is added to tracked/public evidence.

[GDB replay](evidence/stack-crash-gdb-before.txt) proved stack exhaustion at anchor's
prologue, with SP below the 1 MiB mapped stack. Cold helper boundaries isolate
name/global hint and new case-call temporaries. [Measured prologues](evidence/stack-frames.json)
show anchor stack subtraction 3840→3584 and call_expression 19904→16224 bytes.
The unchanged full semantic-depth gate then passed. Semantic limits, stack reserve,
resolver order, diagnostics and exception handling were not weakened. Permanent
text_encoding_semantic_depth registration includes exact crash preservation.

This is separate from the historical Release filesystem SIGSEGV, whose cause
remains unestablished. Existing filesystem research evidence was preserved unchanged.

## Verification and remaining limits

The [engineering history](IMPLEMENTATION.md) preserves actual failures/corrections.
The [receipt](evidence/RECEIPT.md) records final Debug 56/56, Release 56/56 and
strict ASan/UBSan/LeakSanitizer 53/53 selections with exact commands/logs. Earlier
selections and corrected harness checks remain separately archived. [Performance](evidence/PERFORMANCE.md) reports observed Debug/Release
normal/O2 scaling, including allocation/GC cost, without an invented speedup.

Native Windows/macOS execution remains pending the owner's later platform matrix.
Tests and portable implementation branches are registered; local Linux execution
is not platform acceptance. Unicode casing is pinned to 16.0, and results are
Unicode scalars rather than graphemes or normalized text. No hosted CI, branches,
commits or pushes are authorized. Root README, console and existing standard-library
implementation sources were not edited by this sprint. Boss separately reformatted
stdlib/time.void; that confirmed user change is preserved and excluded from this
implementation. Other protected source/research hashes remain unchanged. Local
acceptance and the [completeness/hash audit](evidence/completeness-final.json) are
complete. Stop for independent Golden Wrench review.
