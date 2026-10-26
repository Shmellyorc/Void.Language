# Sprint 3 engineering record

## Step 1 — audit and contract

Read the Language Bible's string/collection/current-boundary sections, Stage 8 text
contract, standard-library design handoff, current binary/memory/error modules,
frontend canonical helpers, independent IR validation and Runtime UTF-8 operations.
Source arrays support efficient geometric push; immutable scalar indexing scans and
therefore must not drive repeated traversal. Contract recorded before implementation.
The only planned native support is pinned full Unicode casing. No async Runtime work.
Existing untracked io research documents/evidence are preserved. Root README and
console source are protected. No hosted CI or Git writes are authorized.

## Initial outstanding sequential steps (historical)

2. Text implementation/focused tests; 3. hex; 4. Base64; 5. ASCII; 6. integrated
acceptance, documentation metadata inventory, performance and handoff. This record
must be updated with actual failures/results; no completion is claimed yet.

## Step 2 — text focused implementation

Source text uses existing UTF-8 byte conversion/decoding and geometric lists.
Search is KMP; extraction/trimming traverse boundaries once. Unicode casing uses
one private canonical-module operation with independent IR validation and shared
JIT/AOT lowering. Unicode 16.0 mappings/properties are embedded, with generator
and input/output hashes retained; no runtime Unicode dependency. Final_Sigma uses
two linear property passes, including overlapping Case_Ignorable/Cased properties.
Debug source check and normal/O2 text fixture passed. Initial diagnostics are
retained: enum-to-integer casts are not supported; error codes now use explicit
category selection. The initial check also preceded completion of the new compiler
build; the rebuilt compiler recognizes the private helper. No source crash occurred.

Crash preservation is implemented before further process tests. Each failure path
copies input bytes before temporary cleanup, checks hashes and preserves provenance.

## Step 3 — hex

Hex implemented entirely in source with checked sizes, explicit digit/length/limit
errors and UTF-8 byte offsets. Debug check and normal/O2 passed all-byte roundtrip,
independent 00/80/FF uppercase vector, mixed-case input, empty, malformed and owned
output checks. No crash or implementation correction was needed in this step.

## Step 4 — Base64

Source implementation uses explicit standard/URL-safe alphabets and required/omitted
padding. Checked preflight arithmetic bounds output before construction. Decoder
rejects whitespace, NUL, alphabet mixing, impossible lengths/padding and noncanonical
unused bits. RFC empty/f/fo/foo/foob/fooba/foobar vectors, URL-safe bytes and malformed
cases passed source check and both Debug JIT modes. No crash occurred.

## Step 5 — ASCII

Strict encode/decode and eight byte predicates plus byte lower/uppercase are source
functions. Debug check and normal/O2 passed all 128 valid ASCII values, every high
byte rejection, NUL preservation, ownership and exhaustive 0..255 predicate/case
boundaries. Independent integrated reference cases are now being exercised.

## Step 6 — integrated verification in progress

857 independently expected Python-oracle cases passed Debug check and normal/O2,
including all-byte codec vectors and every changed Unicode 16.0 case mapping in
chunks, plus Final_Sigma context. The Unicode mapping oracle shares the pinned UCD
with table generation; official SpecialCasing examples and explicit context tests
supply additional checks, rather than treating a roundtrip as proof.

The first ownership harness incorrectly required source-object constructor events
for immutable string/list work. Its failed receipt is retained. The corrected
harness forces GC at generated polling points and inspects precise roots/frames.
It passed 2,419 injected allocation failures per normal/O2 mode across 17 operations,
with retry, immutable-input and unreachable-object checks (37,886 checks total).
Independent IR rejected 11 forged authority/type/shape cases. Persistent VOIDLS
passed 54 hover/definition/completion/signature/malformed-constant recovery checks.

The interoperability fixture initially reused imported type name `stream` as a
local; ordinary diagnostics were retained and the local renamed `buffer_stream`.
No compiler behavior was widened. Two synthetic signal-only subprocesses verified
exact byte preservation and separate evidence incidents; these are not real VOID
source crashes. Actual crash evidence directory is still empty.

### Verification corrections and additional coverage

The opt-in subprocess.run/Popen preservation layers initially both recorded a
synthetic signal. The failing Release self-test receipt is retained. Coordination
now assigns run-managed processes to one layer; direct Popen waits/communicate
preserve separately. Intentional harness termination is distinguished from a crash.
An extended synthetic test also exposed communicate's direct os.write bypass of
stdin.write; the collector now preserves the supplied transport exactly once.
The final three-incident self-test verifies separate immutable disk copies and an
exact unsaved LSP document snapshot. Failed and corrected receipts remain separate.

The initial depth harness used a relative executable after changing cwd, then
incorrectly assumed 64 nested calls were accepted. The existing parser budget
rejects that expression with VOID1107. Corrected tests preserve that policy:
32 nested calls accept, 64/124/400 reject safely on an actual 1 MiB stack in
Debug, Release and strict sanitizer builds. No parser budget was changed.

Strict AOT generators initially hit LeakSanitizer's documented ptrace restriction
inside the sandbox. That environmental failure is retained. The rebuild and strict
runs use approved execution outside the sandbox, retaining ASan/UBSan/leak checks.
Public metadata inventories 71 declarations with all descriptions/parameters/
returns validated. Extra error/readonly and depth fixtures supplement the initial
45/43-case consolidated selections; final results must retain that distinction.

Measured normal/O2 Debug/Release accumulation scaling is recorded in PERFORMANCE.md.
No historical baseline implementation or performance speedup is invented.

## Real Debug 1 MiB stack crash — preserved and repaired

The existing `tests/integration/semantic_depth.py` gate exposed SIGSEGV in Debug
on the rejected 400-method chain, after its legal-depth cases. Release passed.
Before cleanup, the crash harness preserved exact input bytes as
`evidence/crashes/incident-splghldg/inputs/deep-methods_crash.void`, SHA-256
`73871bef34c030e336699c8e7ff706917428d0450dfa5e7c7b923c9419245b00`.
The receipt records original command/cwd/signal/stdout/stderr, executable hash,
all canonical stdlib sources, build flags and generator provenance. The actual
Debug executable is retained privately under `build/text-encoding-crash-binaries/`.
The crashing compiler.cpp and semantic_depth.py were copied into incident provenance
before repair. Original crash copies are readonly and were not edited or reduced.

GDB reproduced the fault using that executable and an independent exact replay
copy. SP 0x7ffff485bc30 was below mapped 1 MiB stack start 0x7ffff485c000; the fault
was anchor's prologue at +11, with repeated anchor frames. This establishes native
stack exhaustion, not a Unicode execution, GC, optimizer or filesystem crash.
The complete stack/mappings receipt is `stack-crash-gdb-before.txt`.

The new inline Function-construction temporaries enlarged call_expression's Debug
frame. Existing global/name hint temporaries also occupied every recursive anchor
frame. Cold noinline helpers now isolate both, preserving resolver order and the
unchanged semantic guard. Measured initial repair prologues: anchor 3840→3584 bytes;
call_expression 19904→16224 bytes. The full existing legal-depth/recovering-rejection
suite then passed at 1 MiB, including nominal, numeric and string-index chains,
recursive type grammar and the later independent diagnostic. No depth threshold,
stack reserve or exception policy was weakened. The record does not attribute every
byte of prior anchor pressure to this sprint; the directly observed fault is stack
exhaustion with the pre-repair executable.

Permanent `text_encoding_semantic_depth` registration uses the existing full gate
with crash preservation. Expanded final acceptance must be rerun after rebuilding
all selected targets, including globals and LSP regressions, in all three builds.
Earlier acceptance and final-check receipts remain historical, not the final repair
verification. The original filesystem Release SIGSEGV remains unrelated/unresolved.

### Boss-confirmed encode API adjustment pending current gates

Boss explicitly selected plain `encode -> string` plus recoverable `try_encode`
for both hex and Base64. Finish the in-flight post-stack-repair gates before
editing their source inputs, then add thin plain-text adapters, move fallible
encoding under try_encode, update all tests/docs/inventory and run final acceptance
for that approved API. The codec algorithms and strict decode policies are unchanged.
This supersedes the initial result-returning encode contract in earlier receipts.

### Final API implemented

All post-stack-repair gates completed before source changes: Debug 56/56,
Release 56/56, strict sanitizers 53/53. Exact logs, selections and source hashes
are archived under evidence/pre-api-acceptance/. Plain hex/Base64 encode now
returns string through convert.unwrap of the shared try_encode implementation;
recoverable size errors remain on try_encode. There is no duplicated conversion
algorithm. Independent oracle coverage increased from 857 to 929 cases, including
both public variants. Forced-GC/allocation-budget scenarios increased from 17 to
19. Compiler metadata now inventories 73 public declarations, with zero warnings.
Final acceptance below must use these source bytes.

A separate stdlib/time.void blank-line formatting change appeared during this
sprint (observed SHA-256 f0de623206cbcd5679764f61c9df2be9dd025c3de37704ad1e46509a72bd595b).
Boss confirmed this was their own formatting change to match neighboring files.
It was not made by this sprint and is left untouched. The original protected hash
is retained as evidence; do not describe the whole working tree as exclusively
Sprint 3 changes. All other protected sources/evidence still match.

An exploratory `focused.py --help` invocation produced IndexError because this
internal positional driver has no help mode; it did not run a compiler or fixture.
The proper positional check/JIT/O2 invocation subsequently passed. This was not an
acceptance test failure or source crash.

## Final local acceptance and completeness audit complete

Boss-confirmed final APIs passed Debug 56/56, Release 56/56 and strict
ASan/UBSan/LeakSanitizer 53/53. Exact commands, timings, selections and logs are
linked from evidence/RECEIPT.md. All eight fixtures passed check, actual normal/O2
JIT and independently linked normal/O2 AOT. Both native builds passed 1 MiB source
execution and persistent LSP; all three passed parser/full semantic-depth gates.
Each profile passed 19 forced-GC/ownership scenarios, 2,668 recovered allocation
failures per JIT mode and 41,778 ownership/root checks. Public metadata verified
73 declarations with zero warnings; the independent oracle fixture contains 929
cases. Installed/relocated imports and the Unicode license passed in every build.

Final 40-file source freeze, all executable/cache hashes, 20 protected hashes and
13 immutable crash input copies verified. Boss's independently confirmed time
formatting remains the sole protected-file exception. Original pre-repair binary
and provenance hashes verified. Git HEAD remains the authorized baseline and
`git diff --check` passes. No new crash occurred in final acceptance. Earlier failed
receipts, the one repaired Debug crash and synthetic tests remain separate.

Final performance measurements use the approved API and retain realistic scaling
limitations. Native Windows/macOS execution and independent Golden Wrench source
review remain pending. Historical filesystem Release SIGSEGV remains unresolved.
No hosted CI, Git mutation, console/root README edit or unrelated implementation
work was performed. Stop for Golden Wrench review.
