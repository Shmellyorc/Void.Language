# Structured diagnostics and compiler recovery (Phase 5)

The user reports Phase 4 commit `d78b6b8`, Actions run #6, passed Linux, Windows
and macOS. Phase 5 preserves that LLVM 19.1.7 foundation and all ten suite names.
The Language Bible's section 48 examples are illustrative, not previously reserved
IDs. The assignments below now form the implemented compiler diagnostic contract.

## Shared representation and ownership

`include/void/diagnostic.hpp` defines `Diagnostic`, `Severity`, `SourceSpan`,
`RelatedLocation`, and `CompilationError`. `compile_source`, `compile_project`,
source loading, and CLI entry validation use this representation. `CompilationError`
derives from `std::invalid_argument` for existing clients and exposes
`diagnostics()`; `what()` renders that same collection for the CLI. There is no
second editor parser or type checker, global diagnostic registry, or LSP.

A diagnostic has an assigned code, severity, description, primary span, optional
related spans/descriptions, expected/received type names, and hint. A span holds an
immutable shared source snapshot, declaring VFS identity, and half-open byte
`[begin, end)` offsets. A missing token has an empty insertion span. Diagnostics
usually identify the offending token, binding, keyword or operand rather than the
entire enclosing expression. Related declaration ranges can belong to another file.
The snapshot retains original bytes and line starts; copying SourceFile shares
immutable storage. Files are never reread to render diagnostics. Invalid UTF-8
retains the original rejected snapshot (an empty file-opening anchor cannot
replace loaded bytes) and identifies its first invalid byte.
A standalone SourceFile/read_source has no VFS owner until used in a project.
An unreadable file has an empty diagnostic anchor, not fabricated source content.

Locations are one-based lines and **Unicode-scalar columns**, not UTF-8 byte
columns or UTF-16 LSP positions. A leading UTF-8 BOM does not occupy a column.
LF, CRLF, CR, empty lines and EOF without a final newline retain original offsets.
The renderer shows a numbered gutter and a scalar-counted caret range on the
primary line, plus related snippets, expected/received types and hints. A multiline
span remains intact in the representation; its console underline stops at the
first line ending. Tabs are errors; carets do not claim tab expansion, grapheme,
wide-character or terminal-display-width accuracy. Long source lines are retained
and rendered in full. A future IDE adapter must convert byte spans to its protocol
encoding using these snapshots, not reinterpret scalar columns as UTF-16.

Phase 22's approved diagnostic amendment escapes each malformed source byte as
uppercase ASCII `\xHH` in compile-time snippets, including related locations.
Valid UTF-8 scalars remain unchanged. Caret indentation and range width account
for the four displayed characters per escaped byte, while diagnostic header
locations, original byte spans, source snapshots and LSP UTF-16 positions remain
unchanged. The underline for VOID1001 covers the escaped first offending byte;
additional malformed bytes on that line are escaped too. Source validation still
rejects the file at its original first malformed byte. No replacement characters,
normalization or rewritten source buffers are used. Runtime excerpts keep their
existing presentation because invalid source never reaches runtime lowering.

The existing frontend suite verifies retained bytes/spans, scalar locations,
expanded primary/related carets and valid UTF-8 rendering. The source_cli suite
also invokes `tests/integration/utf8_diagnostics.py`, which captures real
check/run/emit stdout and stderr as bytes and decodes both with strict UTF-8.
It covers stray continuations, overlong encodings, surrogates, values above the
Unicode range, bad continuations, truncation, BOMs, LF/CRLF/CR, missing final
newlines and non-ASCII text before/after damage. The test checks exact locations,
escapes/carets, rejection and protected output/source bytes in both execution
modes. The LSP protocol regression verifies malformed disk source retains its
UTF-16 range through an unsaved repair and close back to the rejected disk bytes.
WORK_LOG.md records actual local verification and hosted limitations.

No warnings are currently generated. Severity supports warning/information; the
lowering gate tests error severity, so warnings alone do not invalidate a program.
There is no successful-compilation warnings/result API in this slice.

## Assigned codes

Codes identify stable problem categories; wording and hints can improve without
changing the code. Add new IDs rather than repurposing an assigned ID. These are
compiler IDs, not claims that the Bible's future string/package/runtime features
are implemented.

| Code | Implemented category |
|---|---|
| VOID1001 | Invalid UTF-8 source |
| VOID1002 | Tabs or inconsistent indentation |
| VOID1003 | Unsupported/unexpected source character |
| VOID1004 | Unsupported numeric literal syntax |
| VOID1005 | Unterminated quoted import path |
| VOID1101 | Missing/unexpected token or invalid statement/declaration syntax |
| VOID1102 | Invalid or reserved identifier in a declaration |
| VOID1103 | Unsupported/missing value type |
| VOID1104 | `while`; suggest the supported `for` spelling |
| VOID1105 | Unsupported collection/range iteration |
| VOID1106 | Const declaration missing its initializer |
| VOID1107 | Parser, expression, constant dependency or import depth/size limit |
| VOID2001 | Exact type mismatch (also carries expected/received types) |
| VOID2002 | Unknown value identifier |
| VOID2003 | Unknown function |
| VOID2004 | Incorrect argument count |
| VOID2005 | Duplicate declaration or forbidden shadowing |
| VOID2006 | Missing return path |
| VOID2007 | Mutation of readonly const/declaration |
| VOID2008 | Calling a noncallable value |
| VOID2009 | Invalid value use, no-result value or standalone value expression |
| VOID2010 | Break/continue outside a `for` loop |
| VOID2011 | Function call in a compile-time constant |
| VOID2012 | Structural constant dependency cycle |
| VOID2013 | Known division/remainder by zero |
| VOID2014 | Integer literal magnitude outside the supported range |
| VOID2015 | Invalid checked numeric conversion |
| VOID2016 | Illegal numeric operator |
| VOID2017 | Floating literal range or exact-representability failure |
| VOID3001 | Invalid/unsupported/nonportable VFS import path or identity |
| VOID3002 | Source/import file discovery, reading or directory access failure |
| VOID3003 | Private/inaccessible export or unresolved qualifier |
| VOID3004 | Ambiguous imported declaration/qualifier |
| VOID3005 | Conflicting import alias or alias/declaration collision |
| VOID3006 | Imports used without a filesystem compilation context |
| VOID4001 | Missing or incompatible CLI entry adapter |
| VOID9001 | Diagnostic budget exhausted; further analysis/diagnostics suppressed |

## Recovery and safety boundaries

The lexer diagnoses a bad logical line, removes its partial statement tokens,
keeps indentation boundaries and emits an invalid-line marker. It then resumes
at the next line. The parser consumes that marker without inventing another
expression error. Malformed UTF-8 rejects that entire file; discovery still visits
other imports of a readable importer.

The existing recursive-descent parser catches failures at statement and top-level
declaration boundaries. Synchronization tracks emitted indent/dedent tokens,
consumes a bad statement and its nested block, and resumes at the next sibling
statement or top-level declaration. EOF is a permanent sentinel. Every recovery
advances or exits; missing punctuation never becomes accepted alternative syntax.
Malformed local var/const declarations with readable names leave explicit invalid
bindings for diagnostic analysis. A malformed signature/constant declaration with
a readable name reserves an invalid declaration, suppressing repeated call/use
errors. Lexically unreadable lines may not preserve a declaration name.

Semantic analysis catches independent failures per statement/function/constant,
checks both operands and call arguments where safe, and continues across files.
An AST-only error type marks invalid values, distinct from i32, bool and no-result;
it never enters IR. Invalid local initializers poison dependent uses, and failed
constant states prevent reevaluation and repeated cycle reports. Invalid returns
remain returns for recovery flow analysis. Missing-return diagnostics are omitted
for the affected malformed/invalid function; unrelated well-formed functions still
receive return-path checks. Invalid conditions do not prevent checking their bodies.

Source discovery publishes a unit before following imports, preserving function-only
cycles. Each failed import is diagnosed at its own source location; unrelated
reachable imports continue. A failed qualifier suppresses dependent lookups.
For a missing or syntactically damaged direct dependency, unresolved unqualified
names in that importer are conservatively suppressed because ownership is unknown.
This can hide a separate spelling mistake until the dependency is repaired; exact
type, readonly, known-declaration and unrelated-module checks still run.

The per-compilation collector deduplicates equal physical-file/span/code/message
records and shares owning snapshots. It retains at most 100 diagnostics plus one
VOID9001 notice, stops further lexical/parser/semantic work at that budget, and
bounds the dedup registry too. Within the retained collection, ordering is VFS
identity, primary byte offset, then numeric code, with stable insertion order for
ties. Discovery is deterministic (written import order, ordered VFS maps); host
filesystem enumeration never selects output ordering. When truncated, the first
100 discovered diagnostics are retained before this final sort. The notice follows
them and borrows the final source anchor; it is not another fault at that location.

Resource guards are explicit implementation limits, not new language grammar:
128 combined parser recursion frames, 64 block/conditional frames, 512 primary
expressions between statement starts, 256 shared semantic expression frames
(including recursion through constants), 128 constant dependencies and 128 newly
loaded import frames. Function execution recursion remains an application/runtime
concern. A limit diagnostic asks for smaller expressions/functions/dependency
chains; it never silently accepts or lowers truncated input.

All reachable units finish validation **before any AST-to-IR lowering**. Any error
throws the structured collection, so check/run/emit return 1, no JIT invocation
occurs, and emission neither creates a partial object nor overwrites an existing
output. Valid CLI behavior and exit codes remain unchanged. Internal IR/backend
failures remain developer errors; recovery does not catch and relabel them as
ordinary source mistakes.

## Runtime and reload audit

SourceFile's line/column and snapshot utilities are shared. Runtime excerpts retain
their established presentation, and generated failure storage, checked entry ABI,
per-invocation isolation, original source snapshots, 32-caller trace bound and
native CRT printing remain unchanged. Compile-time diagnostics do not become
exceptions inside generated code. See VALUE_SEMANTICS.md.

The exact conservative length-delimited const restart token is unchanged and is
constructed only after successful full validation. A failed graph cannot produce
a candidate generation. Tests retain an older native lease and slot across an
invalid edit, then reject a valid but changed const generation while continuing
to invoke the old generation. A runtime division failure from that old generation
still renders its original source after the invalid edit. Diagnostic snapshots
survive edits/deletion. This
is a host/API audit, not a newly implemented file watcher or automatic reload.
Precise source/dependency compatibility deserves a separate design phase.

## Real-source examples

```sh
build/ci-debug/voidc check examples/diagnostics/errors.void
# exits 1; reports all three primary problems in the same invocation
build/ci-debug/voidc check tests/fixtures/diagnostics/multi/main.void
# exits 1; /a.void shadowing + unknown value, /b.void return mismatch,
# /main.void missing import, with a related root declaration snapshot
build/ci-debug/voidc run examples/diagnostics/corrected.void 10
# 52
build/ci-debug/voidc emit examples/diagnostics/corrected.void -o build/ci-debug/corrected.o
clang-19 --ld-path=/usr/lib/llvm-19/bin/ld.lld build/ci-debug/corrected.o -o build/ci-debug/corrected
build/ci-debug/corrected 10
# 52
```

The first invocation reports these actual source locations (physical checkout
prefix omitted here only for readability):

```text
examples/diagnostics/errors.void:2:23: error: Type mismatch: expected bool, received i32.
    code: VOID2001

 2 |     var total: bool = value
   |                       ^^^^^
    Module: /errors.void
    Expected: bool
    Received: i32

examples/diagnostics/errors.void:4:5: error: Cannot mutate readonly const or declaration 'bonus'.
    code: VOID2007

 4 |     bonus += 1
   |     ^^^^^
    Module: /errors.void
    Hint: Use var for a mutable local binding.

examples/diagnostics/errors.void:5:12: error: Unknown function 'missing'.
    code: VOID2003

 5 |     return missing(value)
   |            ^^^^^^^
    Module: /errors.void
    Hint: Declare it here or import its declaring file; imports are not transitive exports.
```

Frontend acceptance uses real checked-in and generated `.void` files: eight primary
errors in one file, later-function recovery, cross-file related declarations,
deterministic repeats, malformed signatures/control flow/imports/indentation,
UTF-8/BOM/newlines, source paths with spaces, constant/alias failures, diagnostic
limits, combined expression/dependency depth and retained snapshots/generations.
CLI integration checks invalid check/run/emit in normal and O2 modes, verifies
existing output bytes are preserved, and executes corrected source in JIT and
linked AOT. All existing architecture/runtime/numeric tests remain intact.
Linux sanitizer verification and CI use halt-on-error and distinct ASan/UBSan
exit codes 87/86; expected source-error status 1 cannot mask a sanitizer failure.
See WORK_LOG.md for measured build and test evidence.

Next: design precise validated source/dependency compatibility and a successful
analysis-result interface before incremental IDE integration. Do not build a second
parser/type checker. Contextual literals/conversion rules still need approval
before numeric expansion; managed types still require the GC/root/thread gate.

## Phase 6 numeric extension

The approved contract in NUMERIC_TYPES.md extends the same collector/recovery path.
VOID2014 retains integer literal range errors. Added VOID2015 for checked numeric
conversion failures, VOID2016 for illegal numeric operators (including floating
remainder), and VOID2017 for floating literal overflow/exact-representability
failures. VOID2001 still reports mismatched existing types and suffix/context
conflicts. Numeric snapshots/ranges, deterministic ordering and multiple independent
errors are tested; context probing does not hide errors in sibling operands.
Checked numeric runtime failures use the existing source/caller failure boundary.
No LLVM poison-producing conversion is reached before its guards.

## Phase 7 analysis and compatibility update

The earlier whole-file constant-token and analysis-API limitations above describe
the preceding phases. They are superseded by [ANALYSIS_COMPATIBILITY.md](ANALYSIS_COMPATIBILITY.md):
one validated, owning analysis result now supplies declarations, snapshots and
resolved dependencies without IR/machine-code generation. Exact declaration tokens
and compiler-derived bindings replace whole const-containing source files in
reload compatibility. Ordinary body/trivia edits beside unchanged constants can
reload; actual constant changes require manual restart. Language rules, numeric
semantics and structured recovery remain unchanged.


## Phase 9 revision integration

[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) adds immutable unsaved project overlays
through the shared loader and an explicit revision-safe session acceptance boundary.
Its diagnostic/query wrappers retain the analyzed project revision. Existing disk
APIs, complete-validation gates, byte spans and exact native contracts remain.

## Phase 13 receiver diagnostics

New assigned categories: VOID2018 invalid receiver declaration/storage; VOID2019
unknown nominal receiver method; VOID2020 deferred custom lifecycle initialization.
Method calls reuse VOID2001/2004 for type/arity, VOID2005 for duplicates/collisions,
VOID2007 for readonly mutation and VOID3003 for visibility/import access. Pure
constants still reject method execution with VOID2011. Receiver parsing, body
checking and deferred effect permission checks use the existing owned collector,
source spans, budget and successful-analysis gate. A 400-link method-expression
chain must diagnose VOID1107 and recover to an independent later-function error.


## Phase 14 custom value initialization

Phase 14 retains VOID2020 for invalid/direct lifecycle execution and deferred
deinit/default ordinary parameters, replacing only the old valid-init-declaration
rejection. Constructor binding uses existing VOID2004/2005 and type errors, pure
initialization uses VOID2011, and access uses VOID3003. Unavailable nested defaults
identify the field and required initializer parameter. Existing source snapshots,
bounded recovery and backend-before-publication gates remain.
