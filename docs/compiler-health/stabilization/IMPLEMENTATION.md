# Compiler stabilization engineering

**STATUS: COMPILER STABILIZATION COMPLETED / STANDARD-LIBRARY DEVELOPMENT HOLD LIFTED BY BOSS.**

Boss authorized this change after local acceptance of the await/propagation
syntax. See the [syntax delivery and current authorization](../../async-await-syntax/HANDOFF.md).
Normal VOID development may resume under the existing Boss approval, Research, Wrench review and Keyboard implementation workflow. This lifts the special stabilization freeze; it does not approve unspecified modules or unrelated features. No new standard-library development was performed in this syntax sprint.

H1–H7 remain unresolved historical observations. Independent review and hosted
acceptance are not inferred from the lifted hold.

## Historical stabilization record — original hold superseded above

27 October 2026. Authorized baseline `1d4ad15f5215838d209e48bb47ac7c3f52b05371`.

**STATUS: NEW STANDARD-LIBRARY DEVELOPMENT PAUSED — COMPILER STABILIZATION IN PROGRESS.**

This is an active engineering record, not acceptance or independent review.
The complete Boss directive remains authorized. No new library development,
Git mutations, hosted CI, root README edits or ptrace debugging.

Baseline source archive, untouched research documents, original known-issues
diff and frozen Debug/Release/strict compiler binaries are retained under
`build/compiler-stabilization/baseline/`, with `SHA256SUMS`. The Release binary
matches the research SHA-256
`3342a2153fc909a98111207db82a8c7b2736983a5311e33a4f68369ec391a952`.
Original research archives and receipts remain unchanged. Their fixtures and
helpers were extracted under `build/compiler-stabilization/research/`.

Work order: shared hints and traversal; decoder factoring; cold-compile
measurement; safe CLI reachability; automatic optimized cache; remaining
measured bottlenecks; three-profile correctness/performance acceptance and
Golden Wrench handoff. Nothing is accepted merely because a retry passes.

## First integrated Release candidate

Compiler SHA-256 `5c7a99925673211f9304fa7ed5097e5d2c5d86e3d9dafb28e3723d69edd33ce6`.
Frozen binary and source archive are retained as `candidate1-voidc` and
`candidate1-sources.tar` in the build evidence directory. All source fixtures
are byte-identical to the preserved research archive where compared.

| Workload | Baseline | First candidate | Observation |
|---|---:|---:|---|
| Left arithmetic 20, source check | 1.504090 s fresh replay | 0.009543 s | Shared child hints remove the exponential revisit. |
| Balanced arithmetic 20, source check | 0.009814 s fresh replay | 0.010379 s | Process-floor control; single samples, no regression claim. |
| Tiny O2 uncached | 0.022495 s research median | 0.023533 s | Single new sample. |
| Unused JSON import, O2 uncached | 10.829944 s | 0.238247 s | Entry emission removes unrelated native bodies after full validation. |
| DTO128 O2 uncached | 309.755 s | 82.276312 s | 791.4 MiB RSS; still unacceptable, not solved. |

Every measured child used an actual 1 MiB soft/hard stack. Raw wall/user/system/
RSS, compiler/source hashes, commands, outputs and host load are in
`build/compiler-stabilization/measurements/measurements.jsonl` and `results/`.
The first baseline checks overlapped build activity and are corroborated by the
earlier frozen research; final performance measurements must be serialized.

All 27 first-candidate hint fixture checks passed. Release focused
`generics_source`, `debug_cache`, `debug_cache_processes` and
`automatic_cache_processes` passed 4/4. These do not establish sprint acceptance.
The first incremental build failed because headers were edited during the
build, producing mismatched constructor signatures. Its log is preserved as a
build/source race; the stable second build succeeded. No execution fault or
LLVM crash was involved.

## Residual root-frame experiment

Decoder factoring alone leaves the 82-second DTO128 case. The backend still
allocates, zeroes and registers every reference instruction separately despite
their identical function-duration lifetimes. The next candidate packs synchronous
instruction homes into one target-layout aggregate with one bulk zero fill and
one ordinary Slot listing every precise reference offset. It does not retire,
reuse or omit root words, alter safepoints, change the GC ABI or remove validators.
Async frame homes retain their existing layout. This experiment needs structural
measurement, GC/failure/ownership and three-profile acceptance before retention.

An additional independent dictionary-literal hint defect was found: generic
identity could see the first key's string type instead of the dictionary. Its
hint now defers to the existing collection inference path, like array literals.
A nested dictionary/array/propagation regression is added.

The coherent task repair also needs `spawn` hints to describe task handles.
Both direct generic spawn arguments and immediate `await spawn` were reproduced
against frozen candidate 2 before adding the type-only wrapper. Their exact
sources and candidate diagnostics are retained in the measurements directory.

Candidate 2 combined instruction-root packing with a measured encoder repair:
its largest remaining pre-O2 function was the repeated-field encoder (3,478 root
slots), so conversion/descent is now shared by concrete child type after name
and value selection. Balanced ordinary if-expressions select values without
linear parser/semantic nesting. This preserves path-before-field-read ordering,
readonly projections, ordinary fixed-value checking and the existing work charges.
No generated helper ABI, allocation table or optimization downgrade is introduced.

Candidate 2 DTO128 O2 uncached: 10.097976 s, 10.015475 CPU seconds, 141.3 MiB RSS.
Pre-O2: 65 functions / 90,216 instructions / largest block 2,965. Post-O2:
61 functions / 66,565 instructions / largest block 2,507. Remaining local root
descriptors justify extending the same aggregate storage to reference-bearing
locals; scalar locals stay independent to preserve ordinary LLVM promotion.

Release candidate 2 passed 6/8 focused suites, including JSON forced GC,
allocation failures, cleanup and native-object reload. The other two stopped in
the provenance harness: Python `glob('*.void')` also matches the new `.void`
directory, causing `read_bytes` to raise `IsADirectoryError`. First logs are
retained. Source hashing and crash preservation now select regular files; the
synthetic preservation regression includes a real cache directory. No source
diagnostic, Runtime fault or weakened assertion is hidden by this correction.

The original compiler produced the 2,048-line JSON metering oracle, SHA-256
`7c89496e1987b39f786e8e2cb7cee93487d82f8b701145b519ab931e5cce3462`,
from `tests/json/metering.void`. It sweeps every work budget 1–1024 for valid
mixed nullable/inherited data and competing unknown/type/missing errors. An
initial fixture omitted an explicit i32→i64 cast; its exact invalid source and
ordinary diagnostics are retained, then the fixture was corrected. This is
test setup evidence, not a product repair or a performance comparison.

## Consolidated candidate findings and retained corrections

Release candidate 3 completed 82/87 selected suites. Four independent aggregate
root proof observers aborted with their explicit descriptor-shape assertions:
reference values, composition, inheritance and interfaces. A combined descriptor
preserved the words but hid the original compound layout from these independent
observers. Their checks are unchanged. Packing is narrowed to pointer-shaped
homes; compound aggregates, interface views and prepared receivers retain their
original individual descriptors. The four exact test binaries, source archive,
first CTest log and hashes are frozen under
`build/compiler-stabilization/first-aggregate-proof-failure/`.
These SIGABRTs came from the observers throwing inside a noexcept hook; they
are not unexplained execution SIGSEGVs or a claimed historical crash repair.

The fifth failure was a test-targeting error: automatic runs populate several
objects, so the globals corruption test modified an unrelated first directory
entry and then correctly hit the unchanged target object. That corruption probe
now owns a separate project/cache and retains the original coordination-byte
mutation and required miss assertion. The ordinary cache/security tests remain.

The superseded Debug pass was stopped deliberately with SIGINT after Release
established these causes, before rebuilding. This is a controlled test stop,
not a product signal or passing acceptance. Its receipt is retained. That stop
also exercised provenance preservation and exposed a second directory-glob
case in explicit provenance inputs. Source/provenance directory entries are now
ignored while real files retain exact-byte/hash verification; nonexistent files
still fail. The synthetic preservation regression passes a real cache directory
both as source/provenance and still verifies all exact bytes.

Encoder source review found an avoidable fixed-value-copy cost: nested choice
joins snapshot fixed arrays. Fixed/value fields retain their original per-field
conversion path; pointer/scalar fields may share a conversion, and single-field
groups avoid an extra selected-value temporary. No owned-array copying contract
is changed to improve compilation. A fresh stable three-profile pass is required.

## Final-pass failures and stack repair

The next stable-input selection completed Release 93/94, Debug 92/94 and strict
sanitizer 91/93. These were failed acceptance attempts, retained in
`docs/json/evidence/implementation/acceptance-{release,debug,san}-1793116533*.json`.
All report unchanged source/build hashes during execution. The compound negative
searched for `array_capture`, but this growable capture has ordinary stores. The
validator rejected each actual removed capture store in the independent probe.
The regression now removes each actual compound-line array capture write and
requires rejection; its positive suspension fixture and validators are unchanged.
The sanitized operator integration binary was stale (old `lower()` ABI), omitted
from the initial explicit build list. Its binary, ABI symbols and first log are
frozen. The final build list is derived from all selected CTest commands and
includes this target; hash stability alone is not evidence of freshness.

Debug `source_cli` exposed a real 1 MiB stack overflow on the existing 400-receiver
negative. The exact sources, original executable and receipt are frozen under
`build/compiler-stabilization/hint-stack-failure/`; the frozen baseline Debug
compiler also signals on that source. New inline hints enlarged `anchor`'s
unoptimized frame from 3,584 to 5,600 bytes. Outlining projections alone restored
3,584 but still failed; splitting hint handlers reduced dispatch to 224 but
ordinary checking still failed. Both failed source/binary attempts are preserved.
Static assembly, without ptrace, identified ordinary dispatch's 2,272-byte frame
and per-branch optional-type copies. Context now travels by const reference through
read-only handlers; array/dictionary handlers retain their own mutable copy.
The unused method context argument is removed. The final ordinary dispatch frame
is 1,040 bytes, expression 400, method 1,744, and hint method 1,088.
The unchanged semantic-depth suite now passes Debug at an actual 1 MiB, including
legal chains, both 400-call negatives, exact diagnostic locations, later recovery,
numeric/index chains and recursive type grammar. No nesting limit, optimization
setting or test expectation was relaxed. The first borrow build's unused-variable
compile error and superseded builds' deliberate SIGTERM stops are retained.

## Descriptor bulk initialization

The final LLVM root descriptor data separates immutable size/map/count fields from
native addresses. A private constant template is copied to stack storage before
installing every address and calling the existing frame-enter API. Physical homes,
individual compound descriptors, exact offset maps, safepoints and ABI are
unchanged. The isolated prototype passed all four original aggregate root proof
suites on 1 MiB stacks. Its preserved DTO128 IR reduced pre-O2 instructions
88,296→84,437 and post-O2 64,981→57,182; largest post block 2,609→1,572.
The codegen probe took 7.391→5.039 seconds with the same pinned LLVM default
codegen/O2/PIC/CPU/features. These probe runs overlapped proof linking and are
supporting evidence, not the final end-to-end benchmark. Full fresh three-profile
validation and controlled serial timings remain required.

The automatic CLI process regression additionally exercises optimized source and
import edits, a symlink candidate, interrupted temporary file and unavailable
cache directory, with real 1 MiB Linux children. The API regression explicitly
checks optimized compiler identity invalidation and entry/export policy separation.
These extend the existing security and invalidation assertions, including the
legacy no-IR-O2 cases; ordinary run remains silent.

## Final local result

Final production CLI hashes: Release
`fc2fa48b34678e4082456cddad78e0fd62d60934673a409d23a301ad4525f1f2`,
Debug `05ebd7496bfeea13e06dd31ec3082d921003cd93dfbaf1fb655774a3bf604479`,
strict sanitizer
`b853f4441cca06f7907ebefd8de115d29e5ab1b54ae55f4b7dd4c540feb12a52`.
Fresh selected-target builds passed. Frozen-input acceptance passed Release
94/94, Debug 94/94 and strict sanitizer 93/93, with unchanged before/after source
and build hashes. The existing sanitizer configuration excludes one LSP stack
probe; both other profiles pass it. No timeout/stack budget or LLVM optimization
pipeline was relaxed.

Serial replay of the frozen baseline took 297.021 seconds optimized DTO128;
final uncached runs took 8.149–8.609 seconds (an earlier independent serial set
8.161–8.361). The full 8/16/32/64/128 ladder passes JIT/AOT and both IR policies
in all three profiles at 1 MiB. Cold automatic cache publication, warm reuse,
uncached runs, real applications, edit/run, actual compiler changes and separate
projects have independent measurements. Standalone DTO128 opt fell
107.308→2.795 seconds; llc 180.651→4.835. Machine stack maximum fell
414,344→13,208 bytes. See the complete tables and audit for interpretation and
measured costs/tradeoffs rather than treating these headline numbers as universal
CI thresholds.

The allocation-counter helper's initial POSIX alignment defect and extra async
helper's initial RTTI flag mismatch are preserved in the incident ledger. Their
corrected helpers pass. Additional explicit entry checks cover virtual/interface
calls, callbacks, closures, cleanup and async activation/lifecycle/workers across
profiles and IR policies. Runtime allocation/lookup/poll/GC probes, root-drain
retention and uncached compiler lifetime cycles are complete. The tiny allocator
growth control is flat with glibc tcache disabled; no production memory repair
is inferred or required for that allocator-cache effect.

Runtime code/header/archive and protected root README remain unchanged. H1–H7
remain unresolved historical observations. Engineering is ready for independent
Golden Wrench source/evidence review; the standard-library hold remains in force
until that review and Boss authorization. No Git mutation, hosted CI or ptrace
based debugger/tracing was used.
