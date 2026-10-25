# Stage 10 focused tests and acceptance evidence

## Current acceptance checkpoint — 24 October 2026

Implementation, examples and executed coverage are reconciled in
[final acceptance](ACCEPTANCE.md). Step 43 is complete; Step 44 review and required
native evidence remain open. Stage 10 is not locked. Earlier dated research and
progress statements below retain their historical meaning.

## Boss authorization — 21 October 2026

Boss approved implementation of the entire 44-step Stage 10 mission, B1–B4,
and the Wrench amendments. This supersedes the proposal-only authorization
statements below; it does not claim implementation or acceptance.

- One heterogeneous print and shared display/format kernel; exact numeric
  ties-to-even rounding, checked allocation, no arbitrary width/precision cap.
- Declaring-owner field privacy and deep const paths; direct own-initializer
  replacement grant only; existing method/property privacy remains unchanged.
- Owner-contained exact operators include binary `%` and derived `%=` in addition
  to the other arithmetic, unary, equality and ordering families. Floating builtin
  remainder remains prohibited. Independently declared `!=` remains prohibited.
- Approved verified pinned Ryu/date and bundled versioned IANA rules; recursive
  custom-equality dictionary-key exclusion remains required.
- A standalone CR completes a line immediately. Pending CRLF continuation belongs
  to logical line reads; mixed raw reads retain byte ownership.
- Cursor queries require an accessible owned response channel. Redirected stdin,
  unavailable ownership or responses return a recoverable error; no hidden tty.
- Preserve Stage 9 Windows corrections, LLVM 19.1.7, ownership and GC. No Git
  mutation, hosted CI dispatch, publication or root README edit is authorized.


**Approved acceptance plan, not a claim of executed results.** Current focused
engineering evidence is recorded separately in IMPLEMENTATION.md.
The repository's existing CTest, C++ source harnesses, Python subprocess helpers,
LSP client and real editor runners are sufficient. Extend those facilities; do
not build a certification service, new validation framework or CI workflow.
[Main roadmap](../STAGE10_DESIGN.md).

## Test execution discipline

For each implementation slice, run the changed source/Runtime suite and relevant
existing neighboring suites on Linux. Add normal/O2 execution and linked AOT for
observable source behavior. Use the existing sanitizer configuration for memory,
concurrency handoff and lifetime changes; use deterministic barriers/fault adapters
for shutdown and partial I/O. Repeat broader checks after failures or integration
changes, not after every documentation edit or unchanged step.

After all four systems integrate, run the complete locally available consolidated
Linux suite once, then targeted native platform tests. Windows **Debug and Release**
both require console/Unicode/error-path evidence; a Release result alone does not
cover Debug. macOS needs its own terminal/time-zone/pipe evidence. Missing local
platform access is recorded explicitly and handed to Boss for a later authorized
run; it is never inferred from Linux. No full seven-job hosted run is required
between engineering steps. No hosted run is authorized by this document.

## Source behavior matrix

Every executable fixture below must pass source checking, normal JIT, O2 JIT and
an independently linked native executable unless a row explicitly exercises only
analysis/tooling or Runtime injection. Compare exact bytes, values, side-effect
logs, status and diagnostics, not just exit success. Keep legacy entry-result CLI
output distinct from application stdout.

| Area | Required positive cases | Required rejection/failure cases |
|---|---|---|
| Print | Zero/one/many heterogeneous values; Unicode/NUL; separator/end/file/flush; custom display | Unknown/duplicate controls, positional after named, wrong control type, no-result/unfinished argument |
| Evaluation | Written argument order, value snapshots, managed alias identity; holes left-to-right | Throwing/failed display stops later hooks/holes; expression effects retained; no partial print record before write |
| Numeric defaults | All signed/unsigned widths, minima/maxima; f32/f64 normal/subnormal ±0 | No widening, locale dependence, ACP conversion or loss of signed zero |
| Digit patterns | `000`, `0.00`, `0.00#`, `#,##0.00`, `+0.00`; carry/sign/grouping | Invalid grouping/scaling commas, optional-before-required fraction placeholders, unknown tokens |
| Rounding | Exact 2.5→2 and 3.5→4 at precision 0; f64 2.675→2.67 at precision 2; percent ties | No double rounding; independent of host rounding mode and locale |
| Bases/scientific | Every integer width; signed negatives; X/x/B/b; padding; exponent carry | Float D/X/B rejected; no accidental sign extension or overflow on signed minimum |
| Special floats | nan/±inf/negative zero default and patterned/aligned | Canonical spellings; NaN payloads not accidentally printed as numbers |
| Resources | Long literals/output, large precision/padding, checked extents | Unrepresentable width/precision rejected before allocation; allocation faults clean roots; existing constant limits retained |
| Composite display | Ordered arrays/dicts, enums/results, nullable, opaque types; cycles | No private-field or native address leak; mutation-invalidated iteration fails safely |
| Custom hooks | Managed/value/generic, readonly receiver, inherited/using selection, custom domain errors | Wrong signature, mutating/async hook, inaccessible/ambiguous hook, recursion, allocation failure |
| Interpolation | Calls, indexes, dict/result/enum expressions, if/match, nested strings, multiline holes/triples | Empty/unmatched holes, ambiguous colon with helpful parentheses diagnostic, malformed spec/quoting |
| Escapes | `{{`/`}}`, `\u{7b}` literal brace, quotes/backslashes/NUL, CR/LF/CRLF | Decoded braces never reparsed as holes; literal bytes/spans remain original |
| Dates/times | Valid factories, leap years (2000 yes, 1900 no), month names, 12/24h, fractions | Invalid components, 12h without AM/PM, wrong-type tokens, no leap-second invention |
| Duration/instant | Negative fractions, i64 extrema, checked arithmetic, monotonic elapsed | Overflow, calendar projection outside 1..9999, calendar months treated as durations |
| Zones | UTC/fixed offset; two geographic zones; exact historical-second offsets | Missing/corrupt/version mismatch; overlap reject/earlier/later; gap rejects; no ambient local zone |
| Private fields | Owner init/method/accessor/operator/closure reads; internal/public cross-module rules | Same-file free helper, derived access, component friendship, hidden-member fallback, unauthorized field pattern/label |
| Const fields | Pure defaults, visible creation args, direct own init assignment; alias behavior | Post-init store/compound/nested mutation/mutable escape; helper/closure/base-phase grant abuse |
| Copy authority | Reference-free independent copies; reference-bearing value snapshots | Declared const fields remain const after whole-value copy; embedded readonly references never thaw |
| Object integration | Managed/value + generics + inheritance + using + properties + interfaces | Missing setter, ambiguous promotion, staged identity escape, incomplete required references, foreign-owner methods |
| Operators | All approved unary/binary glyphs; mixed exact types; generic specialization | Duplicate tuple/result-only overload, wrong arity/result/owner, implicit conversion, unsupported glyph, async/virtual, mutable operands |
| Selection | Direct owner imports, explicit base cast, nullable exact matches | Two-owner ambiguity, hidden exact target, transitive imports, inheritance/promotion discovery, reversed operands |
| Compounds | Locals/fields/properties/indexed values; LHS/index/getter/RHS once | Failed operation does not commit; captured owner not rebound; readonly and structural-version failures retained |
| Equality/hash | `!=` invokes `==` once; old keys preserve existing equality/hash | Custom equality keys rejected recursively; no callbacks stored in dictionary descriptors; patterns do not invoke custom `==` |

Numeric expected values should include checked-in bit-pattern vectors generated
from an independent high-precision reference during engineering, plus upstream
Ryu test vectors. Test formatter bytes against known vectors, not against a second
call to the same implementation. Use exact bit inputs to avoid confusing source
literal rounding with display rounding. Re-run vectors with changed fenv and
non-English locale in the native harness; never change source numeric semantics.

## Console OS and Runtime tests

| Mechanism | Minimum evidence |
|---|---|
| Redirected streams | Independent stdin/out/err pipes and files, exact UTF-8/NUL/newlines, no BOM or terminal escapes, no terminal attached |
| Shared input | Alternate byte/char/text/line/key reads; CR followed by non-LF, empty line, final line, EOF, every multibyte split; queued lookahead survives switching |
| Encoding errors | Overlong/surrogate/out-of-range/invalid/truncated input; error partial bytes and recovery position; Windows unpaired UTF-16 console events |
| Transfer faults | Inject short success, EINTR, zero write progress, would-block, timeout, error after prefix, full-disk equivalent, broken pipe; verify exact completed byte counts |
| SIGPIPE | Native process with ordinary/custom/preblocked/prepending host signal state; failed write returns result without global handler changes or consuming preexisting signal |
| Concurrent writes | Multiple host callers/Runtime contexts; shared stdout/stderr destination; no intra-record interleaving among VOID writers; external-writer limitation documented |
| Blocking + GC | Hold native read/write/stream-lock wait while another Managed participant allocates and collects; no stale managed pointer or GC rendezvous deadlock |
| Shutdown | Barrier-held admitted read; closing wakes/drains it; new calls reject; no host handle close/cancellation; native buffers and leases released after actual completion |
| Terminal session | Real PTY on Linux/macOS; native console/ConPTY on Windows; capability detection, key/modifier/repeat/unknown sequences, cursor/size/color/clear/title |
| Restoration | Normal exit, return, Runtime failure, nested mode scope, primary+cleanup failure, Runtime shutdown; prior modes restored and primary failure preserved |
| Redirected terminal APIs | All unsupported operations return explicit error before control output/mode changes; never open hidden terminal to bypass redirect |
| Prompt | Explicit no-newline record and flush observed before blocking read; no implicit prompt/read flush; redirected prompt follows explicit caller choice |

A small injectable native-operation adapter is justified for transfer faults and
clock deadlines that real pipes cannot reliably force. Keep it inside the existing
Runtime unit-test boundary; do not expose source fault controls, add a production
validation subsystem or replace real terminal tests with mock-only acceptance.
Use real pipes and a native platform probe for the OS boundary itself.

## IR, lifetime, snapshots and compatibility

Extend existing malformed-IR tests with unauthorized private reads, const writes,
forged initializer phase/owner, skipped readonly projection, incorrect operator
signature/target and custom-equality dictionary admission. Ensure the IR checker
derives necessary capability facts instead of trusting source success/effect flags.

Force GC during collection display/custom hooks, string building, temporal/error
result publication and constructor/operator returns. Sweep allocation failure
points across builders, result/enum materialization, staged construction, native
buffers and metadata admission; output roots/frames must be valid after failure.
No C++ exception may unwind through generated source frames.

Retain an old successful analysis, delete/edit its files, and prove diagnostics,
format spans and later lowering still use that snapshot. Change private/const,
operator set/signature/effect, selected hook and format constant syntax: each
appropriate compatibility change must require restart. Compatible hook/operator
body edits publish a new generation while old calls retain old code; no callback
address or code lease enters GC/dictionary/zone descriptors. Cache cold/hit/corrupt
paths must rebind Runtime operations and account for formatter/rules versions.

## Compiler-owned tooling and real editors

Deferred Boss VS Code observations and future editor-quality acceptance are recorded
in [the permanent Step31 backlog](IMPLEMENTATION.md#deferred-editor-quality-observations--boss-report-step-31).
These are observed F12/F2/hover/doc presentation issues, without an established
cause. They are recorded, not authorized for implementation in Step31. Preserve
existing guarded rename and compiler-owned documentation/symbol infrastructure.

Use `tools/lsp_client.py`, existing LSP CTest suites and `tests/editors/run.py`;
extend current VS Code tests and the Neovim/Vim adapters without a second semantic
engine. Cover:

1. `use console` / `use time` discovery from installed source metadata; real API
   docs and parameter help; no hardcoded editor completion list.
2. Completion/hover/signature help and navigation in typed interpolation holes,
   including numeric/temporal spec suggestions and user format/operator docs.
3. Private access contexts inside/outside owners and const-field readonly hints;
   owner-aware patterns/construction labels; operator glyph references.
4. Invalid unsaved format/operator/field source, diagnostic repair, disk refresh,
   close fallback and late old-revision result rejection. No source execution.
5. Supplementary Unicode before a hole/spec, CRLF and multiline literals; exact
   UTF-16 diagnostics, references and versioned edits from the same snapshot.
6. Guarded field/parameter rename and deliberate operator-glyph rename rejection;
   installed standard source remains readonly; preserve Vim's current rename boundary.
7. Source formatting is idempotent and token/literal-preserving for new grammar;
   no whitespace rewrite changes indentation inside expression bodies or literal text.
8. Real VS Code/VSCodium/Neovim/Vim integration on installed versions where available;
   record each actual product/version and startup/repair/shutdown result separately.

## Performance and completion record

Measure representative large print/interpolation output, many small scalar reads,
large redirected lines, cyclic/nested collection display, repeated temporal format
and changed-document editor queries. Record workload, build/optimization, machine,
elapsed time and peak/live memory separately. Compare behavior/output first; no
performance claim without measurements. Check linear output construction and
bounded-by-input parsing, not a speculative microbenchmark threshold.

The engineering acceptance document must list exact commands, pass/fail counts,
locally unavailable checks, dependency versions/digests and each completeness-row
status. A skipped platform probe is not a pass. All four systems and required
cross-system cases must be complete before the Stage 10 completion claim. Wrench
reviews evidence, and Boss controls Git, hosted CI and release decisions.

## Step 34 focused integration fixtures

`tests/stage10/operator_integration.cpp` reuses the managed/native ABI probes for
forced collection, precise roots, allocation-failure retry, retained cleanup
receivers, native side-effect traces and real worker safepoints. The existing
operator JIT/AOT Python runner links emitted objects independently with Runtime.
Fixtures cover managed/generic value snapshots, RHS await suspension, successful
worker calls, checked/dynamically owned failures, failure inside cleanup and
prohibited structured-cleanup enqueue. Negative analysis and malformed-IR cases
check worker native/clock/task/indirect effects and const capability propagation.
Exact executed results belong to engineering/step34-results.json; this addition
does not mark unexecuted platform or final Stage 10 rows as accepted.

## Step 35 captured-place compound fixtures

`operator_compounds.cpp` and its source fixtures cover every arithmetic compound,
private/readonly paths, properties, fixed/list/dictionary elements, generics and
mixed exact operands. Native ABI probe traces count getter/index/RHS/operator/
setter/cleanup calls, including failed operator and invalidated-place noncommit.
RHS index/owner rebinding, retained managed identity, value-copy snapshots,
ordinary result assignment conversions, GC, RHS await and allocation-failure retry
use the existing managed probes. Negative source and malformed IR cover selected
operator proof/targets, returned readonly authority, bad collection writes, worker
effects and missing async array-capture initialization. The shared Python runner
executes normal/O2 JIT and independently links the same native fixtures. These
are focused Linux tests; exact outcomes and unexecuted platforms are in the dated
engineering evidence, not implied by this plan.

## Step 36 custom-equality dictionary protection

`operator_keys.cpp` checks declarations of any visibility/signature on direct value,
managed, nullable and concrete generic owners; recursively nested/private fields;
fixed-element shapes including zero extent; foreign declaration source spans; and
generic function instantiation. Fixed arrays remain unsupported keys independently
of equality. Existing scalar/string/managed identity/value key behavior is retained.
Forged IR checks key admission and equality marker/declaration agreement separately.
Runtime tests cover transactional key-schema rejection, ordinary value schemas,
managed leaves, native dictionary metadata, ABI flags and policy downgrade rejection.
The ordinary JIT catalog is checked against transitive owner policy.

`operator_keys.void` also runs through the existing normal/O2 JIT and independently
linked AOT runner: custom-equality values, nested private value payloads, managed
payload/ancestor identity boundaries, strings with NUL, value-copy keys and patterns
that compare data without invoking custom ==. Existing dictionary, array, generic,
pattern, interface and operator suites provide neighboring regressions. Exact
Linux outcomes and unavailable native platforms belong in the engineering evidence.

## Step 37 operator/format reload and cache fixtures

`operator_reload.cpp` checks normal/O2 body-compatible publication and retained old
leases; operator addition/removal/signature/visibility/effect changes; unused and
concrete generic owners; owner visibility/kind and exact generic target bindings;
imported builtin/custom selection switches and caller invalidation. Formatting cases
cover primitive/nominal/direct hooks, helper body edits, interpolation, named specs,
constant specs and concatenated specifications. Existing tooling/session tests retain
stale/invalid revision and canonical graph protections.

The same harness uses actual compiler build identity with the existing Debug cache:
cold/hit, body/specification/build invalidation, corruption fallback, forced-Runtime
reload hits and stale revision rejection. Operator, compound, formatting and temporal
Runtime calls execute on cold and cached objects. `operator_format_cache.void` also
uses the existing normal/O2 JIT and independently linked AOT runner. Existing cache
root/allocation-failure/multiprocess tests remain enabled. Strict compiler/Runtime
ASan/UBSan/leak verification remains distinct from generated-code instrumentation
and unexecuted native-platform/editor/final-stage acceptance.


## Step 38 compiler/editor intelligence fixtures

`tooling.cpp` tests concrete generic operator presentation with unified source
identities, docs/effects, derived/compound glyph ranges, immutable snapshots and
unchanged compatibility/invalidation. `lsp_intelligence.py` exercises every approved
glyph family, private/direct-import policy, generic-body multi-target/signature
aggregation and completion intersection, generic const-field metadata, operator
completion and rename rejection, ordinary rename, custom-hook navigation/references,
typed numeric/temporal patterns, CRLF/triple/Unicode holes, signatures, malformed
source stale-index rejection and repair. Existing protocol/discovery/session suites
remain enabled. Shipped VS Code host and native Neovim/Vim tests query the actual
server for operator and format intelligence; Vim also verifies multiline syntax
synchronization. Exact native host versions, strict instrumentation and unavailable
platforms are recorded separately in engineering/step38-results.json.


## Step 39 source-formatting and diagnostic fixtures

`source_formatting.cpp` covers every owner operator glyph and arithmetic compound,
concrete generic owners, private/const fields, private methods/properties, exact
bindings/contracts before/after, escaped/nested/triple/Unicode interpolation,
comments/blank lines, LF/CRLF, BOM and missing final newline. It checks deterministic
idempotence, exact literal bytes, immutable diagnostic sources and offsets, and
normal/O2 native execution. An originally valid multiline lambda inside a hole
must receive no unsafe indentation rewrite. Malformed declarations/holes return
no edit; syntactically valid semantic failures remain failures.

`lsp_source_formatting.py` exercises real stdio dirty overlays, exact UTF-16 error
positions, malformed syntax/specifications, diagnostic repair, stale didChange,
refresh, close/disk restoration and reopen with fresh source. Existing intelligence,
protocol, session, discovery and depth regressions remain enabled. Native VS Code,
Neovim and Vim fixtures apply real compiler edits and reject stale application after
a newer buffer revision. Response/request timing is controlled only in the test
harness, without substituting compiler results. Exact counts, failed development
fixtures and platform/instrumentation limits belong in engineering/step39-results.json.


## Step 40 bounded implemented-system integration

`operator_systems.void` combines private generic const storage/direct initialization,
base-private initialization and derived virtual hooks, const promoted components,
property-captured custom compounds, RHS owner rebinding, separate mutable aliases,
value-copy independence, growable/fixed arrays and dictionary values. Custom equality
and derived inequality, result-returning formatting hooks, numeric/layout/interpolation,
UTC-independent zoned overlap resolution and concrete i64 duration arithmetic execute
together. `operator_systems_async.void` keeps selected places alive across await and
passes formatted generic/temporal results through a worker task awaited repeatedly.

`operator_native_systems.void` traces the actual promoted property getter, value getter,
RHS, operator, setter, derived equality and custom hook/cleanup calls. A failed hook
inside the operator must leave retained managed storage unchanged, skip its setter,
preserve primary/cleanup reasons and keep original compound/operator source frames.
The existing GC probe forces collection and checks root/frame/commit integrity;
allocation faults sweep the combined source execution and must recover. Negative
cases preserve private access, const paths, readonly generic return authority and
worker effect rejection through a custom hook that reaches a clock operation.

The existing reload harness also runs its cold/hit/body/spec/build/corruption/stale
revision and old-generation lease checks against this combined source fixture.
The existing normal/O2 JIT/independently-linked AOT runner retains every prior case
and adds these fixtures. Its bounded process budget is 600 s for the expanded matrix;
this is an execution budget, not a performance acceptance threshold.

`operator_systems_workload.void` repeatedly interpolates 10000 exact 27-scalar records
with a private generic custom hook and temporal formatting. A second workload renders
one million fraction places, copies UTF-8 bytes and checks the complete 1000003-byte
output. The existing integration command records LLVM compile and invocation time,
post-collection object count and host process peak RSS where available; process wall
time is recorded for JIT and linked AOT. Peak RSS includes the compiler, not only the
workload. No arbitrary speed/memory threshold or new renderer/ABI/GC path is introduced.
Exact configurations, results and limits belong in engineering/step40-results.json.

Console/print/terminal Steps 23–31 and composite display remain unimplemented and
must join a later Step 40 integration pass before complete four-system acceptance.


## Step 23 native stream foundation fixtures

`tests/runtime/console.cpp` exercises lazy/default and isolated contexts, independent
endpoint roles/snapshots and fake clock, one input owner across Runtime lifetimes,
stdout/stderr gate exclusion, operation moves/thread affinity, queued cancellation,
concurrent closes and lazy publication/close races. Semaphores and admitted-count
predicates hold real native state across Runtime destruction and prove no adapter or
buffer is freed early. Signal-only stop callbacks wake the injected operation; no
keyboard, sleeps or external terminal hardware is needed. The existing callback
provider admission/self-drain/teardown path is exercised without a new callback
framework. Actual frozen GC roots remain valid while two participants are NativeSafe
holding/waiting on the shared input gate. Native-only resource destruction is checked
when a host drops its final context during Managed code.

`console_endpoints.py` launches the native probe with independent pipe/file/PTY
standards, stdin unavailable, and stdout/stderr sharing one file. It checks roles,
classification, native codes, unchanged host descriptor flags and exact untouched
Unicode/NUL input subsequently read by host test code after Runtime teardown.
Only the host probe reads those bytes; no public console read is implemented.
Neighboring coordination, buffer/schema, allocation-failure and fatal-invariant
regressions remain enabled. Exact Debug/strict outcomes are recorded separately in
engineering/step23-results.json; Windows/macOS and later real-transfer/shutdown
integration remain open.


## Historical Step 24 feasibility blocker — superseded by the approved contract revision

engineering/step24-posix-feasibility.cpp is a standalone Linux architecture probe,
not a production adapter or a new passing console-transfer CTest. Seventeen checks
per Debug/strict run establish a readiness-to-blocking-write counterexample, actual
Context/Runtime token wake and retained-lease shutdown behavior, native byte/flag
preservation after explicit release, and targeted libc AIO cancellation failure.
Barriers and /proc syscall inspection establish the entered wait; no sleeps. All
probe requests drain and join. Existing seven Runtime CTests remain enabled and
pass Debug/strict. io_uring setup is probed separately inside/outside restriction;
no async transfer capability is inferred from setup success.

Step 24's requested transfer/EOF/error/deadline/cancellation/GC/concurrency/SIGPIPE
acceptance remains OPEN. No corresponding source API, JIT/O2/AOT, new native
Windows/macOS or complete console result is claimed. Resolve the policy recorded
in engineering/step24-feasibility.md, then run the original full focused Step 24
cases against the real adapter. Existing Step 23/32–40 evidence remains historical
and does not substitute for these unimplemented transfer tests.


## Step 24 focused native transfer acceptance — 22 October 2026

Boss's revised contract permits best-effort cancellation of already-entered
uncancellable calls; their buffers/leases stay alive until completion, and an
external host/device can keep shutdown blocked. Prompt queued/readiness cancellation
and one monotonic deadline across retries remain required. This resolves the old
blocker; its probes/results remain historical, without repeating that investigation.

| Focus | Current evidence |
|---|---|
| Checked native results | 178 deterministic transfer checks: short writes/reads, explicit EOF, no-progress failure, partial input/output errors, native/secondary codes, invalid controls, allocation cleanup, malformed adapter results and token/Managed rejection |
| Deadlines/admission | Injected clock proves one deadline through EINTR/EAGAIN and partial progress; large timeout saturation; zero polling; real timed gate wait; queued close and independent-session authority |
| Records/lifetime | Full-record stdout/stderr gate across short writes; concurrent readers; entered-transfer barriers prove close retains storage/lease through completion and accepts real successful progress after stop |
| Real endpoints | 210 Python driver checks plus per-child assertions; exact binary NUL/invalid UTF-8 bytes through pipes/files/raw host-configured PTYs, short input while writer stays open, empty EOF, inherited O_NONBLOCK and unchanged flags/modes |
| Real concurrent operations | 128 records of 8193 bytes from stdout/stderr into one redirected file without interleaving; 200 native byte reads across two Runtime sessions with exact aggregate ownership |
| Readiness/GC/shutdown | Real blocked input and full-pipe output, native syscall observation on Linux, precise frozen roots collected while waiting, stop wakes/drains with exact output prefix |
| Native faults | Real EINTR via host test SIGUSR1 handler, closed stdin/stdout (including fd reuse by a concurrent output wait), ENOSPC and broken pipes; default/custom/ignored/preexisting-pending SIGPIPE, unchanged dispositions/masks |
| Baseline | All previous console foundation, endpoint, coordination, buffers/schemas, allocation-failure and fatal-invariant suites enabled |

Nine focused Debug suites pass (13.09 s); nine strict ASan/UBSan/leak suites pass
(6.53 s). Strict leak checks actually execute outside ptrace. Raw successful driver
output includes per-case progress, errors and child counters; manifests identify
exact final source. Initial closed-input hang and its descriptor-reuse fix remain
recorded in the development log. A second deterministic counterexample proves
snapshot availability must survive another operation reusing an absent standard
fd for its wake pipe. The original snapshot now governs every native transfer. No assertion, safety rule or sanitizer check was
disabled. The existing RTTI-off LLVM ABI still excludes vptr checking.

Native Windows/macOS are pending, not passing. Windows must exercise synchronous
redirected files/pipes, overlapped pipe requests and exact-target cancellation
without affecting host requests, native progress/errors, byte/NUL preservation,
shutdown retention and explicit-offset-disk rejection. Darwin must exercise poll
wakeups and its scoped pending/sigwait SIGPIPE path under host signal policies.
Confirmed Windows console text/UTF-16 conversion belongs to Step 26, not this raw
byte adapter. No new compiler/JIT/O2/AOT/editor acceptance or source console claim
is implied. Step 25 queue/UTF-8/line policy is next and remains unimplemented.


## Step 25 shared native input acceptance — 22 October 2026

| Focus | Focused evidence |
|---|---|
| Single input ownership | Mixed bytes/char/text/line and leased raw reads; shared Runtime sessions, independent close, concurrent typed readers and no repeated native read of buffered bytes |
| Unicode | ASCII/NUL, 2/3/4-byte bounds, BOM, supplementary scalars, all transfer partitions for representative scalars and line forms; existing validator reused |
| Invalid input | Invalid lead families/continuations, overlong/surrogate/out-of-range/truncated sequences, maximal immediately available invalid prefix, retained next starter and precise partial offset |
| Lines and mixed transitions | LF/CR/CRLF, repeated/empty/final lines; immediate CR without another native request; line/byte/char/text transitions, zero/failed empty reads and persistent continuation across Runtime sessions |
| Progress/events | Partial native/control errors, byte/scalar splits, one injected deadline, no undisclosed half-line, successful entered completion, ordered native encoding error reported once |
| Allocation | Faults after OS bytes enter the queue: result string/partial/raw allocation and next-block allocation; bytes and continuation stay recoverable, leases release, request-control errors do not poison retries |
| Real native input | Pipes/files/raw host-configured PTYs, binary NUL/UTF-8/BOM, 35,000-byte Unicode line across real 4096-byte native boundaries, invalid boundary and bytewise pipe delivery |
| Native stop/GC | Buffered CRLF/tail before actual blocked input, precise frozen-root collection, stop partial progress and fresh Runtime polling without stale session failure |
| Preserved regressions | All Step 23–24 console/fault/endpoint, coordination, buffer/schema, allocation-failure and fatal-invariant tests remain enabled |

Debug 11/11 (13.40 s) and strict ASan/UBSan/leak 11/11 (7.20 s) pass; 2261 new
deterministic input checks, 264 real POSIX driver checks across 19 native process
cases. Strict leak checks run outside ptrace with unchanged halt/abort settings;
existing RTTI-off vptr exclusion remains. Raw logs include per-child counters.
The failed native-encoding-event regression and its provenance fix are recorded
in engineering/step25-results.json. No assertions or compiler warnings disabled.

This is native input acceptance, not public source-console or final-stage
acceptance. Native Windows/macOS remain pending; Step 26 must route Windows wide
input through this same queue. Existing source/JIT/O2/AOT/editor facts remain
historical and are not claimed to test unimplemented source console APIs.

## Step 26 bounded Windows-wide integration acceptance

`runtime_console_wide` runs the production boundary helper through an injected
existing Adapter/Streams/Runtime, independently checking conversion against the
standard codecvt facet. It covers all BMP scalars, supplementary high/low families
and byte-sized remainder splits, every lone surrogate, truncation/recovery, NUL/BOM,
CR/CRLF mixed readers, native/cleanup errors and exact byte/error offsets. Output
cases include every tested UTF-16 completion position, pair-safe native chunking,
zero progress, full-record invalid-UTF8 rejection, malicious native counts and
non-retryable native half-scalar progress. Allocation failures preserve native
input; post-I/O transcoding needs no allocation. Deadlines, shared stdout/stderr
records, Runtime carry handoff and cancellation/precise-root GC are exercised.
Run alongside all Step 23–25 focused suites in Debug and strict ASan/UBSan/leak.
No simulation result counts as native Windows acceptance.

Native Windows gate (UNVERIFIED here): build with real Windows SDK; run normal
console and Windows Terminal/ConPTY, active cooked/raw modes set by the test host,
NUL/BOM/non-BMP input/output, ReadConsoleW unit splits and pending cooked input,
CR/LF transformations, redirected binary/file/pipe EOF/errors/short writes, shared
stdout/stderr, borrowed synchronous/asynchronous handles, original LastError/modes/
code pages unchanged, queued cancellation and entered best-effort shutdown. Verify
VOID cancellation cannot cancel unrelated host requests; retain buffers until
actual completion and inspect actual UTF-16 completion counts. Test native failure
and half-scalar progress where injectable OS-call fault controls permit. Native
throughput of unit-sized console reads requires measurement, without speculative
thresholds. macOS and final source/JIT/O2/AOT/editor console integration remain open.

## Step 27 focused source-console acceptance

stage10_console_source executes the real compiler/LLVM path against existing
Adapter/Streams/Runtime injection. It checks invalid controls/shadowing, endpoint
construction/field access/worker and malformed-IR rejection, the intrinsic const-field
capability repair with a separately mutable-path negative, exact records, all
portable io_error kinds and original native/cleanup/partial-UTF16 progress. Forced
GC (allocation budget one), native-wait GC, allocation-failure sweeps and reached
cleanup preserve roots/frames and emit no partial pre-write records. Retain the
synthetic array-count regression and const-field selected-place rejection.

stage10_console_aot compares binary stdout/stderr for 24 runs: six fixtures,
normal/O2 JIT and independently linked AOT. It covers numeric widths, Unicode/NUL,
custom/generic/temporal display, every named control's side effects, old-value versus
managed-reference snapshots, failed hooks/cleanup, mixed source reads/CRLF/EOF,
invalid UTF-8 recovery and partial errors. Actual closed-pipe output in O2 JIT/AOT
must return a checked broken_pipe with completed=0 rather than SIGPIPE termination.
Also verify installed stdlib discovery/identity, standalone private-op rejection,
legacy CLI result output and cache cold/hit/corrupt recovery/source changes.

Run both suites alongside preserved field/display/interpolation/temporal/operator,
array-codec and native Steps 23–26 regressions in Debug and strict ASan/UBSan/leaks.
Use a verified isolated stdlib snapshot for the preserved Step 37-sensitive suites.
Native Windows/macOS, fresh Release/console editor, prompt/complete lifecycle,
terminal and composite-display acceptance remain separately open. Do not equate
this bounded source integration with full Stage 10 acceptance.

Endpoint copying/defaults, repeated templates, managed/value initializer readiness
and generic storage run in all four modes. A one-shot allocation sweep must prove
that bare null display allocation occurs only after the written RHS side effect.

## Step 28 focused prompt/lifecycle acceptance

Extend the existing console source/driver harnesses, retaining all Step 27 checks.
A child must expose exact no-newline Unicode prompt bytes before the parent sends
input. Exercise normal/O2 JIT and separately linked AOT, Unicode, empty line, EOF,
invalid encoding and standalone CR while the parent's input pipe remains open.
Assert exact stdout/stderr/exit status; failure diagnostics and source frames must
match byte-for-byte across all four execution modes. Test zero/nonzero/top-level
completion, nested reached/unreached defers, primary plus secondary hook failure,
cleanup-only failure and real broken pipe with both failed and successful defers.

Inject source NativeSafe output barriers, host close/cancellation and GC; prove
entered storage is retained and shutdown cannot return before completion. Verify
recoverable closed in source cleanup (including lazy io_error byte metadata), no
closed native output, first I/O after close, and rejected new host metadata. Run
persistent and one-shot cleanup allocation sweeps with precise roots/frame checks.
Include native console/GC/callback, async lifecycle and application/native-provider
regressions in Debug and strict ASan/UBSan/leak acceptance. No terminal-mode or
native Windows/macOS acceptance is inferred from these Linux tests.

## Step 29 focused terminal acceptance

Existing source driver adds redirected terminal APIs and normal/O2 JIT/linked-AOT
actual PTYs, all 16 colors, exact control/stderr bytes, geometry/position, retained
old reports and unrelated user bytes, title injection and unchanged host modes.
A production voidc build/native executable smoke separately verifies packaging.
Source harness adds readonly snapshots, enum types, worker rejection, precise GC
while presentation blocks and checked one-shot allocation failures.

Native Adapter/Context tests cover all 256 palette pairs, invalid coordinates/
colors/visibility/title controls, exact native/cleanup/partial-UTF16 errors, stale
and split reports, report-boundary errors, CRLF/raw ownership, invalid UTF8, query
ownership, timeout, concurrent records and entered-call shutdown. Real POSIX driver
uses PTYs/files/pipes, actual ioctl dimensions, xterm/dumb/unknown and a compiled
custom non-ANSI terminfo profile. Verify no controls reach redirects; no changed
termios/flags, hidden tty, stdin theft or cached cursor guess. Keep strict leaks on:
per-call descriptors must release while the process-owned system facility remains
loaded; no global curses cleanup may destroy embedding host state.

Native Windows gate: actual SDK build, independent standard-handle/buffer cases,
GetConsoleScreenBufferInfo live coordinates/viewport resize, all 16 attributes and
original-default reset, visibility, visible-only clear/home, Unicode/empty title,
processed-output bell versus unsupported modes, redirected purity, borrowed handles,
GetLastError, untouched modes/code pages, and cancellation/GC/source JIT/O2/AOT.
macOS gate: real SDK/system ncurses dynamic bindings (including legacy tparm ABI),
term database/profile behavior, live ioctl, owned raw host response channel and
strict no-mode-change/cleanup tests. No Linux simulation counts for either gate.

### Step 30 focused acceptance

Use tests/runtime/console_keyboard.cpp and console_keyboard.py with the existing
console suites, plus tests/stage10/console.cpp, console.py and console_keys.void.
The key fixture must execute in normal/O2 JIT and independently linked normal/O2
AOT, including real PTY input and exact output/restore comparison. CMake's actual
Python 3.11 interpreter must run the harness (the shell's newer Python alone is
insufficient). Keep the preliminary syntax-error evidence; repair the test without
weakening any assertion.

Deterministic native cases cover every key ownership transition, unknown and partial
escape bytes, 30ms/original/polling deadlines and clock-limit saturation, EOF/error/
invalid UTF8 recovery, partial physical scalar tails, genuine modifier/repeat/key-up
metadata and no invented physical facts from ordinary bytes. Availability preserves
CRLF/character/event ownership. Repeated/key-up events cannot vanish into byte reads.
Mode tests cover same-owner nesting, conflicting owners, LIFO/closed checks, partial
apply failure, exact restore, primary plus cleanup failures, shutdown retries,
abandoned sessions, allocation exceptions and entered-read cancellation barriers.

Real Linux PTYs cover Unicode/supplementary scalars, Enter/Tab/Backspace, all arrows/
navigation/F1–F12, unknown bytes, Escape, explicit Ctrl-C input, nested and abandoned
mode sessions. Compare every original termios attribute after close/shutdown. Test
all ECHONL/ISIG prior-state combinations during temporary reads: no native/double echo,
prior interrupt policy preserved, exact restoration. Source cases additionally
exercise reached with/deinit, closed aliases, failure before cleanup activation,
allocation sweeps, precise GC while key input blocks and primary source failures
with failed restoration. Preserve normal output, old queue/wide/terminal/callback/
async/field/hook/interpolation/reload regressions.

Native Windows acceptance is still separate: compile with the actual supported SDK;
exercise traditional console, Windows Terminal and ConPTY with ReadConsoleInputW/
PeekConsoleInputW, surrogate pairs/replay and raw-byte carry transitions, physical
key-up/repeat/modifiers, unknown keys, busy owners and native failure/partial state.
Test every public echo/line_input/control_c combination against actual host API
restrictions; verify exact prior native modes/code pages and redirected unsupported
purity. macOS must verify native termios attributes/cc/speeds, actual terminfo key
profiles and PTY restoration. No Linux fake adapter is native acceptance for either.
Step 31 final concurrency/lifetime integration and Stage 10 final acceptance remain
open even after this focused increment passes.

Keep the real canonical-editor regression: with line input enabled, availability
must leave unfinished 'abc' in the OS editor; erase+Enter must still read 'ab'.
Compare exact original flags as well as text (restoring flags alone is insufficient).
Native mode save/apply/restore must not hold the shared metadata mutex over OS calls:
barrier-controlled entered queries/restores must leave cancellation and rejected
admission observable before the native operation completes. Original key deadlines
also cover mode preparation; an injected slow save must not apply mode/read data
or restart the caller's deadline after expiration.


### Step31 focused console concurrency and lifetime acceptance

Extend the existing native console harness with byte-at-a-time output shared by
multiple Runtime contexts: stdout/stderr and presentation must retain whole records.
Use barriers and native cancellation callbacks to hold entered key input while
foreign line/key-availability/cursor/mode operations reject, independent Runtime
shutdown leaves the owner unchanged, and precise collection retains frozen roots.
Drain the owning Runtime only after native completion, restore all nested modes,
and read queued CRLF bytes from a later Runtime over the same Streams.

Real Linux PTYs must exercise three process-stream Runtime contexts, nested mode
ownership, foreign-reader rejection, forced GC, concurrent exact Unicode output,
mixed key/line reads and exact original termios restoration. Explicit host input
translation settings are part of the captured original PTY state. They must not
be silently attributed to VOID mode changes.

Source normal/O2 execution must retain an entered generation through compatible
publication, GC, native record completion and reached cleanup. Verify old/new return
values, exact output and eventual generation retirement. Combine partial native
write failure, original primary diagnostics, reached failing cleanup, Runtime close
restoration failure and successful retry; return native restoration errors to the
originating close caller. Preserve allocation-failure sweeps, checked roots, source
frames, neighboring callback/async/coordination tests and independently linked
AOT/production native builds against matching headers/archive. Linux simulation is
not native Windows/macOS acceptance; external uncancellable syscalls may still
block shutdown and embedding hosts must join admitted source work before teardown.


## Collection composite display focused acceptance

Use existing stage10 conversion/interpolation/hook, console source/driver, display
reload, typed-IR, Runtime/GC and numeric/temporal neighboring suites. Extend the
existing harnesses rather than creating another runner architecture. Normal/O2 JIT
and independent normal/O2 linked AOT must compare the same exact stdout/stderr
bytes for console_collections, console_collection_invalidated and
console_collection_failed_hook, retaining all prior console cases.

Cover empty fixed/list/dictionary values, all numeric widths, bool, raw UTF8/NUL,
fixed value copies, shared list identity, nested/mixed containers, insertion order,
nullable/opaque nominal children, const generic hooks and eligible custom-display
keys. Keep current dictionary-key admission unchanged: fixed-array keys remain
prohibited by the existing key contract. Enum/result children remain a separately
pending display family, not an unsupported OS capability.

Verify the managed continuation/work stack and typed owner/cursor/version stacks,
active-path <cycle> edges versus repeated shared references, key/value snapshots,
ordinary leaf effects/worker restrictions and current version invalidation after
hooks mutate an independently captured alias. Check no outer print record prefix
on failed conversion, reached cleanup/provenance, successful retries, managed/native
allocation sweeps and forced collections with valid roots/frames. Compare exact
1 MiB generated result extent and boundary samples without a performance threshold.
Universal Unicode-scalar alignment must work; literal numeric/temporal collection
specs diagnose and dynamic unsupported specs return the existing Err.

Generated traversal must retain type-directed child conversion and the established
prelude lookup rules (string is a reserved source type name; ordinary format can
be shadowed). Verify generated hook dependencies, hidden helper declarations,
body-compatible reload, old-generation leases and compiler/formatter policy cache
identity (void-format-v3;collections-v1). Native Windows/macOS execution, real editor
collection cross-products and final Stage10 acceptance remain separate obligations.

Collection increment final focused acceptance: Debug 20/20 (840.04s), strict
ASan/UBSan/leak 20/20 (993.45s), 156 conversion checks and 84 exact execution
records per configuration; matching production native builds pass. Detailed
commands, failures/repairs, deep test cost, source hashes and outstanding enum/result/
platform/editor/final-stage obligations: engineering/collections-results.json.
Actual collection native cache cold/hit/corrupt/hook-body/specification probes also
pass 5/5 in each configuration; records: engineering/collections-{debug,san}-cache.json.

## Enum/result display focused acceptance — 23 October 2026

Retain all collection acceptance and the 72-level regression. Extend existing
conversion, source console and display-reload harnesses for empty/payload enums,
qualified cases, Ok/Err, Ok(null), nested results/containers, fixed value copies,
managed payloads, generic const hooks, raw Unicode/NUL, scalar alignment, literal
bad specs and dynamic errors. Verify only active payload hooks run, declared-order
exactly-once callbacks, recursive enum/list cycles, repeated shared references and
independent mutable aliases. Readonly pattern mutation and argument/returned-alias
escapes must reject without changing legal inferred readonly returns.

Use existing IR validation to reject invalid sum tags/layouts, out-of-range/tag-zero
payload access and unchecked aggregate extraction. A well-typed wrong-variant
sum_get must fail through its checked Runtime path, leaving roots/frames clean and
publishing no outer record. Exercise forced GC with real managed payloads and
managed/native allocation sweeps, retries, nested failed hooks/reached cleanup and
structural invalidation while a sum continuation is active.

Run normal/O2 JIT, independently linked normal/O2 AOT and production native builds.
Compare exact stdout/stderr and failure provenance; retain existing prompt, PTY,
shutdown, install/source-identity and cache tests. Add actual sum native-cache cold,
hit, corrupt, hook-body, layout-spec and variant-name probes and ordinary reload
old-generation leases/visibility rejection. Run focused Debug and strict ASan/UBSan/
leak acceptance, including pattern/collection/dictionary/generic neighbors because
the recursive readonly repair reuses shared authority admission. No full-stage or
hosted matrix. Native Windows/macOS and fresh editor/Release acceptance remain open.

Final enum/result focused acceptance: Debug 24/24 (994.99s), strict
ASan/UBSan/leak 24/24 (1137.49s), 203 conversion checks, 92 exact execution
records and six cache probes per configuration; production native builds pass.
Exact commands/source hashes/failures/repairs and outstanding obligations are in
engineering/sum-display-results.json. Final-stage/native/editor acceptance remains open.

## Console/composite compiler-owned tooling acceptance — 23 October 2026

The existing stage10_tooling and lsp_stage10_display suites now combine public
console endpoints/modes/presentation, ordinary source docs/parameter help and
installed readonly/private boundaries with collection/enum/result conversion
metadata. Cross-file qualified enum payload constructors and patterns must share
a source declaration and named parameters; concrete result constructors/patterns
must preserve nullability/readonly capabilities without fabricating a filesystem
source for builtins. Contextual Ok/Err completion uses the existing expected type;
lexically invalid uppercase fragments retain conservative no-result behavior.

Cover typed fixed/list/dictionary members and readonly payload menus, generic
pattern captures, nullable managed composites, string/format/hole/print analysis,
universal layout-only suggestions, invalid numeric patterns, UTF-8/UTF-16 and
CRLF/triple/nested-hole spans, incomplete enum and print calls, dirty diagnostic
repair, stale notifications, refresh, close/reopen and formatting idempotence.
Hooks and clocks remain source metadata only; answering editor requests must never
execute them or native I/O. Hide generated display helpers/native payloads and
installed private/internal declarations from ordinary symbol/completion menus.

Run current Debug and strict ASan/UBSan/leak compiler/session/LSP suites plus
neighboring field/hook/interpolation/pattern/generic/reload and source-console
JIT/O2/linked-AOT regressions. Keep the full 72-level collection test unchanged;
its already-recorded cost does not require rerunning it for each tooling edit.
Use real shipped VS Code, Neovim and Vim clients where available, in isolated
profiles with real console source metadata, automatic and explicit project entry.
Record exact client versions, requests/responses, source hashes and failures in
engineering/console-tooling-results.json. Final Stage10/Release/native Windows/
macOS acceptance and the separately deferred editor-quality backlog stay open.

## Step40 completed-system integration — 23 October 2026

Extend operator_integration.cpp and console.cpp/console.py, retaining all prior
fixtures. The shared operator_console_model combines private/const generic value
and managed owners, inherited/promoted hooks, captured property arithmetic and
remainder, derived inequality, mutable aliases versus readonly/value snapshots,
array/dictionary/enum/result display and numeric/date/duration/explicit-zone text.
Exact traces distinguish capture/getter/RHS/operator/setter/display and cleanup.

Force GC at construction/rendering and a blocked native write; inject allocation
and partial write failures, check roots/frames/retry, failed commit, no partial
outer record, active cycles, recursive hooks and structural invalidation. Validate
source private/readonly/key/format/worker/construction negatives and forged operator
IR. A blocked scoped-mode call retains its old generation through replacement;
subsequent calls use the new hook. Reject changed layout/visibility/operator sets,
format specifications and a newly clock-effectful hook using existing contracts.
Shutdown must retain admitted buffers/code and restore modes after actual completion.

The existing binary-output process driver compares normal/O2 JIT and independent
normal/O2 AOT, real prompt-before-read (Unicode/CRLF/raw LF/NUL/EOF), nonzero application
status versus legacy result printing, normal/failing real PTY restoration, native
partial progress and production native build/startup. Linux pipe progress is observed
with FIONREAD at actual capacity before closing the reader; source-reported accepted
bytes must match exactly, independently of bytes later discarded with the pipe.
The completed combined program exercises explicit cold/hit/corrupt/body/spec cache
paths without modifying caching. Large composite stdout and earlier numeric/temporal
workloads retain correctness, elapsed times and available process memory observations.

Focused selected Debug/strict tests and existing1MiB proof probes are Step40 evidence,
not Step41's consolidated matrix. The unchanged72-level regression and earlier cost
are preserved; it is not rerun in this increment. The demonstrated cold-frame
regressions in compiler metadata/name probing must pass the original1MiB legal
receiver/type cases and bounded diagnostic recovery without changing limits. Final
focused metadata/LSP/composite checks must use the repaired frontend. Native Windows/macOS
and final-stage acceptance remain separate. Full records are in
engineering/step40-final-integration-results.json; earlier step40-results.json remains
the historical partial-system checkpoint. No deferred compiler/editor/debugger
implementation or Git/hosted-CI/publication action belongs to this increment.

## Step41 consolidated Linux acceptance — 23 October 2026

Run all current registered Linux CTest suites without a selection filter in Debug,
a freshly configured Release directory, and existing strict ASan/UBSan/leak builds.
The current official configuration registers199 suites; the older Release175-test
inventory is stale and must not be treated as complete. Coverage/inventory and
unregistered historical/platform-only controls are listed explicitly in
engineering/step41-acceptance-inventory.json. No registered difficult/expensive suite
is omitted. Build every target before final runs; serialize configurations to avoid
build/acceptance resource contention without inflating any existing timeout.

Retain full203 conversion/deep72 and existing JIT/AOT/native/retention workloads.
Report deep-suite elapsed time as aggregate regression cost, not an isolated72-level
compile benchmark. Existing workload compile/invoke/process-wall and available RSS
measurements keep their proper scopes. Normalize only the raw process harness's
historical nonsanitizer Debug configuration label when reporting actual Release;
retain its raw output and CMake compile flags.

Re-run genuine1MiB semantic-depth and full frontend regressions in Debug/Release;
source_cli also invokes the unchanged semantic-depth/UTF-8 diagnostic probes in
all configurations. Preserve124-call acceptance and400-call error recovery. Real
VS Code/Neovim/Vim checks use the final Debug server and isolated profiles. Re-run
six default-root temporal/operator-reload controls and verify isolated/default
library hashes; passing probes do not explain historicalStep37 anomalies.

The original modules_cli builtin compound failure location at3:5 is a regression
contract, preserved unchanged. Repairs must not replace it with a glyph location.
Preserve initial failures/interrupted runs separately and certify only final-source
results. Native Windows/macOS and final-stage review remain separate obligations.


## Step41 historical acceptance stop —23 October2026

Final Debug199/199 plus native workload and both genuine1MiB probes pass.
Fresh Release198/199: source_cli and both standalone1MiB gates reproduce SIGSEGV;
Release native workload passes. Final strict199-suite run has not started.
All six editor phases and six default-root controls pass;766 frozen inputs and
historical artifact hashes match. Earlier196/199 Debug and corrected legacy
harness evidence are retained. No partial attempt certifies full acceptance.

The parent-owned Release GDB probe returned `ptrace: Operation not permitted`;
no stack was obtained. Engineering stopped at Boss's required security boundary,
without escalation, substitute program or policy change. Owner recovery decision
and Release failure diagnosis/repair are pending. No limits/guards/timeouts were
changed. See [step41-results.json](engineering/step41-results.json) and the latest implementation ledger checkpoint.
Step41 and Stage10 remain INCOMPLETE; Steps42–44 remain separate.


## Step41 final Linux acceptance after Release repair —23 October2026

All199 registered suites pass unfiltered in each final-source configuration:
Debug199/199, Release199/199, strict ASan/UBSan/leaks199/199. Zero final skips or
failures. Original semantic_depth.py and full frontend genuine1MiB gates pass
Debug/Release; source_cli includes its semantic-depth checks in all three.
Release focused source_cli/frontend/stage10_tooling and both124/400 controls pass.
The existing result_expression noinline boundary repair shrinks Release dispatcher
stack reservation4152 to1088 bytes without changing guards or optimization flags.
Full203 conversion cases including72-level graph, normal/O2 JIT, independent AOT,
production/native packaging, installed/relocated libraries, console PTYs/GC/failure/
cache/reload and existing workloads are retained. Actual VS Code/Neovim/Vim six
phases and six default-root controls pass against final matching artifacts.

Exact commands, timings, source/artifact hashes, memory scopes, earlier failures
and repairs: [Step41 results](engineering/step41-results.json). The traced strict
build launch failure is preserved separately; final instrumented build helpers
and tests execute through the established untraced profile with leak detection
unchanged. Boss supplied the native GDB backtrace; worker debugger access was not
retried. Step37 observations and the transient Clang crash remain unexplained. A separate Release LLD GVN stack
trace was emitted despite original complete build exit0, without retry; its signal/
cause remains unestablished and preserved.
Stage10 INCOMPLETE; native Windows/macOS and Steps42–44 remain separate.


## Step42 native acceptance gate — 23 October 2026

Boss reports Step41 Wrench acceptance. Linux597/597 remains historical evidence.
[Native execution commands](engineering/step42-native-commands.md) and
[complete source-derived inventory](engineering/step42-inventory.json) prepare
Windows x64/macOS ARM64 Debug/Release. Execution is **UNVERIFIED**: no native host
is available here and hosted execution requires Boss authorization. The existing
workflow dispatch runs all seven jobs; there is no native-only dispatch input.

Capture actual native inventory/JUnit, full logs, source-before/after receipts,
compiler/SDK/binary identities, original1MiB frontend/semantic-depth gates, native
workloads, installed/relocated stdlib/AOT and real editor evidence. Keep existing
limits, failures, guards and checks. Predicted counts194 Windows/199 macOS are not
observed passes. Five POSIX native-console registrations do not run on Windows;
wide injected-adapter acceptance does not certify actual console APIs or ConPTY.
Traditional console/Windows Terminal/ConPTY state, UTF-16 partial progress, physical
keys, cancellation/borrowed handles and restoration need additional genuine host
measurements. Retain UNVERIFIED rows even if the general native suite passes.
Native macOS must establish actual ARM64, original1MiB RLIMIT, Darwin signals,
terminfo/PTY restoration and canonical /var identities. No tests are skipped to
convert missing coverage into acceptance. Step42 results distinguish local
preparation checks from still-unexecuted platform gates; Steps43–44 remain separate.


## Step42 CI96 repair verification

Preserve original native Windows Debug/Release190/194 and macOS build failures;
engineering/ci96/ contains complete decoded job logs and committed source identities.
Focused Linux repairs do not certify repaired native binaries. Exact pending Apple
SDK/compiler/libc++ macro/availability compile-link-run commands are in
engineering/ci96/REPAIR_HANDOFF.md. CMake must use real std stop-token callbacks;
failed standard and pinned opt-in link probes are fatal, never no-op cancellation.

Re-run operator_reload, format_temporal, console_aot and LSP source formatting in
both native configurations, then complete remaining native gates. Verify Windows
unavailable cache/no storage plus behavior/edits/reload/old leases, all original
calendar round-trips and explicit year1/9999 boundaries, separate exact JIT/AOT
legacy CRT trailers with source print bytes unchanged, and UTF-8 fixtures retaining
Unicode/UTF-16/revision assertions. Native classic/Terminal/ConPTY and actual macOS
PTY/SIGPIPE/restoration remain explicit gates. No blanket newline normalization,
Windows persistence implementation, larger stacks or disabled tests is permitted.
Accepted Step41 Linux597/597 remains historical; fresh focused repair builds use
separate directories and preserve all originals. No hosted rerun is authorized here.
