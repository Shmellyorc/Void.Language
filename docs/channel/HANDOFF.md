# Channel implementation handoff for Golden Wrench

27 October 2026. Baseline `72ee05db7e417645d434b3932bfc1e00ffd4ecb6`.

`stdlib/channel.void` implements the complete approved initial API through ordinary
managed generics, identity-key dictionaries, closures and existing built-in events.
Boss settled readonly payloads, reusable state-detaching clear and one exact payload
type per router. [API and lifecycle documentation](API.md) is authoritative for
users; [design](DESIGN.md) and [research](RESEARCH.md) retain their original history.
Independent Golden Wrench source/evidence review and native Windows/macOS acceptance
are unclaimed. No Git mutation, hosted CI, root README edit or unrelated compiler
change is included.

## Implementation and failure audit

The public surface is `router<t>()`, opaque `subscription`, `subscribe`,
`unsubscribe`, `emit`, and `clear`. The module has no singleton, close, deinit,
native provider, type erasure or separate event dispatcher. Private state owns
topic and registration indexes; a handle has only an inaccessible Boolean field
and private initializer, with no back-reference. Each wrapper captures the
original handler directly, so removing a registration cannot suppress its already
captured event snapshot.

Subscribe prepares its token, wrapper, state/bucket and inert null-wrapper record,
then indexes metadata before event append. The concrete typed IR audit finds one
`list_push` publication. Its remaining instructions are local loads/stores and
one direct `gc_write` into the held record's wrapper field, followed by the handle
return. No lookup, helper, allocation, defer or cleanup follows append. The shared
backend lowers this callable/reference store directly; event insertion retains
the existing transactional Runtime behavior. Clear lowers to one nullable state
field store, with no dictionary operation or construction. The host test asserts
these facts from actual compiler-generated IR and saves the full channel IR.

Unsubscribe removes the exact event wrapper, nulls its held record, then deletes
metadata and scans active registrations before pruning. Dictionary layout/cursor
preparation and cleanup remain fallible. Sustained and one-shot allocation sweeps
exercise those native allocations as well as managed construction. Inert residual
metadata is permitted; no failed subscription may retain its marker or deliver a
new unreturned callback. Clear succeeds under a whole-invocation zero-allocation
budget. Successful unique-topic churn releases historical topic/record graphs.

One draft cleanup defect was identified and independently reproduced: comparing
bucket identity while pruning can delete a replacement topic after an earlier
removal failed during cleanup. The corrected scan compares topic bytes across
all active records. A control using the old comparison fails the new regression
with `inactive cleanup pruned a replacement topic`; the corrected source passes.
This is a channel draft defect, unrelated to historical compiler incidents.

## Acceptance coverage

| Area | Evidence |
|---|---|
| Types and topics | All numeric widths, bool, strings, managed/nullable/interface, list/dictionary, enums/results and a value containing a 2 MiB fixed array. Exact empty/NUL/case/slash/Unicode keys; composed/decomposed Unicode remain distinct. Topic/payload expressions execute once. |
| Tokens and sharing | Duplicate identity and exact earlier removal, copied/const/foreign/stale handles, different payload routers, discarded handles, local/global routers, fields, parameters and two aliases of the same state module. |
| Snapshots | Ordered `AACB`, clear/reuse `ACB`, self-removal `SBB`, 128 subscribers, depth-16 nesting, captured application mutation and captured payload reference identity/value copies. |
| Fault safety | Four subscribe paths (fresh state, existing capacity, event/index growth, new topic with existing registrations), sustained/one-shot budgets to first success, exact unsubscribe before/after removal and pruning, new-topic recreation after failed cleanup, snapshot/partial delivery failures and handler cleanup. |
| GC and ownership | Runtime threshold 1, forced construction collections, precise frame/root/commit validation, failed callback marker reclamation after frames exit, retained tokens without retained callback/router cycles, and source deinit never run by GC. A separate bound receiver/callback cycle survives allocation pressure and readonly emission, then disconnects without deinit. |
| Native ownership | Captured trusted native resource remains open after unsubscribe/clear/GC and closes at Runtime shutdown; channel owns no native resource. |
| Async and workers | Owner async local/global use across await, normal/O2 explicit scheduler adapters and linked AOT; forbidden callable graph transfers, async handlers and worker/global access reject. No new sharing permissions. |
| Reload and cache | Compatible stored callback body replacement, old-generation invocation, active callback paused through replacement, obsolete generation retirement, incompatible signature/global initializer rejection, fresh independent cache-hit Runtimes, automatic optimized/no-IR-O2 cache miss/hit/bypass and fresh CLI launches. |
| Tooling | 55 real stdio LSP checks for import/type/member discovery, concrete readonly i32 callback hover, parameter/return docs, signatures, definitions/references, private filtering, installed source rename protection and unsaved malformed-source recovery. |
| Diagnostics | 28 source-located rejection fixtures for types/widths/nullability, callback arity/results/authority, arguments, private construction/fields, const mutation, scope disposal and async/worker misuse. |
| Stack and AOT | Both IR modes execute independently linked native objects; real 1,048,576-byte soft/hard stacks cover many subscribers, bounded nesting and 2 MiB value payloads. Owner async native objects also execute on hard 1 MiB stacks. |

Final focused Linux acceptance passed **59/59 distinct configuration/suite
outcomes**: Debug **20/20**, Release **20/20**, strict ASan/UBSan/leaks **19/19**.
These comprise the original consolidated **19/19 + 19/19 + 18/18** selection plus
the completed bound-receiver suite **1/1 in each profile**, added during the final
coverage audit. That suite checks source/JIT/build/native execution in both IR
modes on hard 1 MiB stacks, under the same strict sanitizer settings.
After the final Unicode/evaluation-order fixture additions, Debug's affected host
and hard-stack suites passed again **2/2**; Release and strict acceptance used the
final fixture. Each final host run passed **49,708 C++ checks** in normal/O2 modes.
The extra strict sanitizer hard-stack execution run passed **56 command checks**,
including source checks, normal/O2 JIT, independent O0/O2 AOT, fresh automatic
cache launches and relocated standard-source discovery. It set both child stack
limits to **1,048,576 bytes**. These additional executions are recorded separately
and are not added to the 59 distinct CTest outcomes.

The focused adjacent selection includes native events/closures/cleanup, generics,
readonly managed values, dictionaries/collections/properties, precise GC buffers
and schemas, globals, caching and compiler-owned documentation/LSP. Host sanitizer
instrumentation covers C++ compiler/Runtime paths; it does not imply that every
generated LLVM instruction is sanitizer-instrumented. Native Windows/macOS and
fresh interactive editor UI execution are not claimed by these Linux/stdIO tests.

## Performance

`tests/channel/performance.py` runs 26 serialized native JIT workloads, three
samples each, with equivalent handlers/payloads and exact checksums. It measures
analysis/JIT compilation separately, initializes registrations and warms execution
before timing, and records execution wall/CPU time and process peak RSS. Caching
is disabled for these dispatch measurements. Peak RSS includes the compiler/JIT
process and is not a dispatch-only allocation measurement.

| Selected topic subscribers | Topics | Direct event median ms | Router median ms | Emissions/sample |
|---:|---:|---:|---:|---:|
| 0 | 1 | 4.911 | 0.854 | 1500 |
| 1 | 1 | 9.837 | 16.783 | 1500 |
| 16 | 1 | 43.455 | 61.245 | 1500 |
| 128 | 1 | 57.994 | 75.525 | 300 |
| 128 | 8 | 60.666 | 78.521 | 300 |

Unknown-topic/empty-event controls measured 5.256/5.522 ms per 1500 emissions;
duplicate 16-handler registration measured 43.645/59.706 ms per 1500. Churn
measured 1.375/10.741 ms per 300 operations; clear/reuse 3.261/12.600 ms per 300;
depth-8 nested emission 17.651/30.786 ms per 300 roots (2700 total emissions).
Clear's event control replaces its owner because built-in events expose no clear.
Empty routers have no state, whereas an empty event still follows its snapshot
path. Churn's router side includes mandatory exact-handle/index/pruning work.

Dispatch touches only the selected topic, never scans the registration index and
uses one existing event snapshot. Measured nonempty dispatch overhead is consistent
with dictionary selection and the extra wrapper/capture, with no demonstrated
need for a new dispatch or Runtime mechanism. No universal throughput target is
claimed. An initial unrolled benchmark made 1024 source registrations expensive
to compile (about 71 seconds for its direct-event case). The final benchmark uses
source loops with the same registration/execution counts and checksums; both
measurements and actual source inputs are retained. This fixture correction is
not a compiler defect claim.

## Reproduction and provenance

Build `voidc`, `voidls`, `void_channel_host` in the existing official local profiles,
then run `python3 tests/channel/acceptance.py debug|release|sanitizers`. Run measured
performance separately with `python3 tests/channel/performance.py
build/ci-official-release/void_channel_benchmark`; build that target first.
Individual `channel_host`, `channel_frontend`, `channel_execution`, `channel_stack`,
`channel_receiver` and `lsp_channel` CTests remain available. Source examples are
[local](../../examples/channel/local.void) and
[global](../../examples/channel/global.void).

The [implementation receipt](evidence/IMPLEMENTATION_RECEIPT.json) records exact
source/build/test hashes, final suite results, commands and scope. The
[changed-file inventory](evidence/INVENTORY.json) distinguishes initial user-provided
research from implementation changes. The [archive receipt](evidence/IMPLEMENTATION_ARCHIVE.json)
authenticates [all raw engineering evidence](evidence/implementation-evidence.tar.gz):
original stdout/stderr, source snapshots, build logs, failed/corrected attempts,
complete acceptance XML/JSON, generated typed IR and both benchmark runs. Every
archived file was reread and hash-verified. Raw originals are also retained locally
under `build/channel/evidence-raw`; no failed attempt was discarded.

[Failure/correction records](FAILURES.md), [annotated actual typed IR](evidence/typed-ir.txt),
[complete performance table](PERFORMANCE.md) and [measured samples](evidence/performance.json)
support direct review without rerunning research probes. H1–H7 remain distinct
historical unresolved incidents. This delivery establishes no root cause or repair
for them. No unimplemented channel API, failing final local check or unresolved
channel blocker remains. Engineering is complete and stopped for review.

**Next gate: stop for independent Golden Wrench source and evidence review.**
Boss retains commits, branches, staging, pushes, hosted CI and release control.
