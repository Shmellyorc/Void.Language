# Compiler and Runtime audit

**STATUS: COMPILER STABILIZATION COMPLETED / STANDARD-LIBRARY DEVELOPMENT HOLD LIFTED BY BOSS.**

Boss authorized this change after local acceptance of the await/propagation
syntax. See the [syntax delivery and current authorization](../../async-await-syntax/HANDOFF.md).
Normal VOID development may resume under the existing Boss approval, Research, Wrench review and Keyboard implementation workflow. This lifts the special stabilization freeze; it does not approve unspecified modules or unrelated features. No new standard-library development was performed in this syntax sprint.

H1–H7 remain unresolved historical observations. Independent review and hosted
acceptance are not inferred from the lifted hold.

## Historical stabilization record — original hold superseded above

The production CLI, identical-fixture comparisons, full ladder, profile matrix,
cache workflow and edit/run results are in [the measurement tables](BENCHMARKS.md).
The following attribution uses preserved IR, actual LLVM pass timings, native
objects/disassembly/stack-size sections, instrumented API boundaries and Runtime
allocation/lifetime measurements. All timed compiler/native children have a hard
and soft 1 MiB stack. No ptrace-based debugger or tracing was used.

## Frontend, typed IR and validation

The arithmetic defect revisited each child after `operator_anchor` had already
walked it. Returning the original child hints eliminates exponential traversal.
The identical 20-operation left-chain check median is 1.518449→0.008038 seconds;
the balanced control is 0.007915→0.009295 seconds, with overlapping startup-range
samples. A 64-operation regression also checks the repaired traversal.

Projection hints now describe the propagated payload, awaited payload, actual
async/spawn task wrapper and conditional value branches. Array/dictionary literals
use the established collection-inference path. Type-only hints do not execute
arguments or check conditional pattern bindings early. Exact nested JSON and
shared generic fixtures, order/error/defer/readonly checks and compiler-owned
LSP tests cover the repaired boundary.

Compact decoder/encoder source also reduces semantic and typed-IR work. The
instrumented DTO128 API measured analysis 0.266368→0.134313 seconds, typed lowering
with its validation 3.047936→0.474376, and the helper's additional explicit IR
validation 3.086904→0.430097. The extra helper validation is not removed from
production for a speed claim. All full-graph source checking and typed validation
precede CLI body pruning; later lowering/verification checks remain enabled.

The rejected deep-chain Debug failure was a separate stack defect. Outlined hints
and borrowed read-only checking contexts reduce recursive dispatch reservations;
collection handlers retain their mutable context copy. The original depth limit,
legal chains, rejected-chain diagnostic locations and later-error recovery remain
unchanged and pass at 1 MiB. Failed intermediate reductions are preserved.

## Generated code, roots and LLVM

Repeated fallible field-admission/find/descent bodies made wide DTO functions
large. The decoder now shares that work while preserving ordered comparisons,
missing nullable admission, paths and exact budgets. The encoder shares repeated
scalar/reference conversions after selecting the name/value. Fixed/value fields
retain the original path to preserve owned-array copying behavior.

Pointer-shaped homes with the same existing function-duration lifetime share
precise target-layout storage/maps. Compound values, interface views and prepared
receivers keep individual descriptors. All original words and safepoints for the
resulting typed program remain; there is no slot retirement/reuse or new liveness
checker. Immutable descriptor size/map/count data is bulk-copied, then every
storage address is installed before the existing frame-enter call. The Runtime
ABI and validators are unchanged.

| DTO128 inventory | Baseline | Final |
|---|---:|---:|
| Pre-O2 LLVM instructions | 301,776 | 84,437 |
| Post-O2 LLVM instructions | 284,226 | 57,182 |
| Largest post-O2 block | 101,915 | 1,572 |
| Largest Slot descriptor count | 9,603 | 173 |
| Largest registered reference-word count | 10,646 | 826 |
| Largest native function stack frame, bytes | 414,344 | 13,208 |
| Decoded machine instructions, including padding/traps | 221,382 | 55,060 |
| Native `.text` bytes | 1,746,250 | 302,730 |

Descriptor counts and reference-word counts measure different costs. Generated
code compaction reduces needed homes; packing preserves the words while reducing
descriptor/setup overhead. Stack sizes are individual native function frames,
not a claim about total call-stack high-water use. Actual small-stack execution
is verified independently.

| Standalone LLVM work on preserved DTO128 IR | Baseline wall seconds | Final wall seconds |
|---|---:|---:|
| Full `default<O2>` opt process | 107.308 | 2.795 |
| InstCombine pass | 53.520 | 0.428 |
| MemCpyOpt pass | 40.918 | 0.319 |
| Full Default/O2 llc process | 180.651 | 4.835 |
| Post-RA top-down latency scheduler | 126.592 | 0.140 |
| Machine instruction scheduler | 27.852 | 0.098 |

Process wall includes startup/parsing; individual pass wall comes from LLVM's
actual timing report. Every command, CPU/feature string, version, IR and output
is preserved. Standalone pass totals are compared within that setup, not summed
as a substitute for production CLI wall time. The same optimization pipeline and
native codegen level run on both sides; no LLVM fork or disabled pass is used.
The primary problem was oversized generated control flow and root scaffolding.
Current native codegen is led by register coalescing (1.157 seconds) and DAG
instruction selection (1.137 seconds), both measured on the smaller final input.

The descriptor template shifts constant data into `.data.rel.ro`: 3,272→45,064
bytes in DTO128, an increase of 41,792 bytes. `.rodata` falls 163,608→54,616 bytes,
and `.text` falls by 1,443,520 bytes. This explicit data/layout tradeoff accompanies
the large scheduler/frame reduction; it is not a free-size claim.

## ORC, linking, metadata and cache startup

A separate scratch copy of the production backend adds ordinary C++ clocks at
existing boundaries. It uses its own cache identity suffix and disposable project
so its objects cannot populate production fixture caches. The instrumentation
patch and full source/build hashes are preserved.

For DTO128's optimized cold miss, typed lowering took 0.495 seconds, raw validation
0.432, LLVM construction including its validation 0.515, and finish/verify/O2
2.158. Adding the IR module took 0.000594 seconds. Lookup then spent 4.085509
seconds before the object-transform callback received the completed machine
object; the following linking/lookup window was 0.002684 seconds. Metadata symbol
lookup plus admission took 0.000123 seconds; atomic object publication took
0.020819 seconds. The callback boundary separates lazy machine generation from
subsequent object processing; that latter window includes capture/plugin work.

On a fresh-JIT warm hit, object read/admission preparation took 0.004680 seconds,
object addition 0.000092, lookup/object transformation/linking 0.002821, and
metadata lookup/admission 0.000118. No typed-lower, LLVM-lower or finish events
occurred on the hit. This makes ORC linking/lookup a small residual cost rather
than the cause of the original minutes-long compilation.

Normal automatic optimized caching preserves stdout and full diagnostics.
Separate no-IR-O2 objects and the legacy alias are verified. Cold/warm objects,
source/import edits, real Release→Debug compiler changes, returning to the first
compiler, separate projects, fresh globals/Runtimes, corruption, unsafe modes,
symlinks, interrupted publication and concurrent writers have evidence. Cache
hits continue to pay legitimate analysis, metadata admission and application
execution costs. A tiny first cached run costs 0.034 seconds versus 0.021 uncached
in this series; warm is about 0.009. Publication overhead remains visible.

## Runtime allocation, lookup, polling and retention

The Runtime archive is bit-identical to baseline. These are measured remaining
costs and effects of smaller generated code, not claimed Runtime algorithm fixes.

A positional root probe performs 5,000 fresh-token/list-count queries. With
16,384 single-word Slots, the last root takes 0.453967 seconds versus 0.001094
for the first. Reusing the token still takes 0.310106 seconds for the last root:
source validation deliberately rechecks registration/frame ownership. The source
walks registered frames/Slots/offsets. Packing alone is not an O(1) lookup index.

Runtime micro measurements report zero allocations for frame entry/leave and
root-word lookup. At 1,024 Slots, entry/leave is about 1.506 microseconds and last
root lookup 1.071 microseconds. Scalar-layout lookup costs about 175 ns with three
C++ allocations/103 requested bytes; ordinary array read about 308 ns with one
allocation/31 bytes, versus fixed-layout read about 106 ns and zero allocations.
These distinguish metadata construction and checked access from heap-size scans.
Empty publication is about 29 ns across the measured 1–1,024 buffer heaps.
A 1 MiB copy plus collection takes about 59 microseconds, with nine allocations
and 1,048,839 requested bytes. Quota/copy checks remain enabled.

Idle poll is about 11–12 ns, with no allocations, across 1–4,096 root words. An
empty collection costs about 43 ns at one null root and 11.68 microseconds at
4,096. A collection retaining 4,096 simple objects costs 84.66 microseconds and
26 allocations/131,056 requested bytes for its root/trace work. These are specific
single-thread micro workloads; contention/complex object graphs are separate.

The final DTO128 helper's second invocation takes 0.287431 seconds, versus the
baseline helper's 0.635052. The final root-thread counter observes 391,906 C++
allocation calls and 17,208,774 requested bytes on that invocation. This is
allocation churn, not retained payload size, and is not an all-thread/malloc
counter. After two invokes, explicit collection reduces managed objects/payload
596/23,848 bytes→0/0; 50 nominal types and 11 schemas remain in the live metadata
catalog. Existing async retention measurements verify retired execution frames,
stable settled catalogs and zero objects/tasks/handles at root drain. Task/result/
Work records can remain until that drain under the established contract.

Twelve uncached create/analyze/compile/invoke/destroy cycles characterize compiler
retention. DTO8's live allocator bytes settle around 1.02 million bytes, while trimming
reduces roughly 86 MiB resident high-water memory to about 48 MiB (IR O2). Tiny's
default allocator report grows about 65 KiB across the twelve O2 cycles. A bounded
control with `GLIBC_TUNABLES=glibc.malloc.tcache_count=0` stays exactly 374,064 live
bytes and 42,148 KiB trimmed RSS on every O2 cycle; no-IR-O2 stays 374,016 bytes
and 39,560 KiB. This explains the observed tiny growth as allocator-cache retention
in this control, not compiler-owned growth. It is not a universal leak proof.

## Remaining boundaries and review status

Entry emission conservatively retains all eager global initializers, virtual
families and interface targets; full metadata remains. Embedding and raw `emit`
retain all exports by default. Full incremental editor analysis, minimal closed
world dispatch pruning, per-function caching and native-cache eviction are not
introduced. Windows persistence retains its safe unavailable fallback. No native
Windows/macOS or hosted CI acceptance is claimed by these focused Linux runs.

Root lookup and checked scalar/layout allocation costs remain measurable. JSON
keeps its ordered comparison/work-budget contract and allocates a fresh DOM where
established; runtime throughput is separate from cold compilation. More precise
root liveness or Runtime indexing would need dedicated lifetime/security proofs.
H1–H7 stay unresolved observations, with their original evidence and identities.
The [incident ledger](INCIDENTS.md) reports failed experiments and measurement
setup defects. Independent Golden Wrench review and Boss authorization still
control any standard-library resumption.
