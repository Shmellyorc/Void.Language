# Channel router: complete implementation roadmap

**Implementation authority, 27 October 2026:** Boss approved the complete scope
and all three final design decisions. Earlier approval gates below are historical.
See [current API](API.md) and [engineering handoff](HANDOFF.md) for implementation
and verification status.

27 October 2026. Baseline **`72ee05db7e417645d434b3932bfc1e00ffd4ecb6`**.
Research proposal only. [API and semantics](DESIGN.md), [source audit](RESEARCH.md).
Stop for Golden Wrench review and Boss approval before Keyboard engineering.

## One cohesive sprint

Deliver `stdlib/channel.void`, documentation, examples and focused tests together.
The complete initial system includes typed routing, exact subscription handles,
strong ownership, safe reentrant snapshots, reusable clear, failure handling,
compiler-owned tooling and JIT/O2/AOT integration. No intermediate prototype is
accepted as the delivered module. Internal steps below are an implementation
order, not separate approvals or a new stage framework.

1. Resolve the two policy choices in DESIGN: readonly payloads and reusable
   clear instead of terminal disposal. Record Boss's answers before engineering.
2. Add ordinary generic state/topic/registration types and an opaque non-generic
   identity handle. Use existing event storage and private owner methods for all
   dispatch. Keep module imports/source identity ordinary; no native provider.
3. Implement metadata-first subscribe and direct-store publication tail. Prepare
   null-wrapper records before the final event append. Verify the emitted typed
   IR has no fallible helper, dictionary access or deferred cleanup after append.
4. Implement exact token unsubscribe: event removal before registry deletion,
   direct wrapper clearing, then active-record scan and empty-topic pruning.
   Keep scans outside structurally mutating iterations. Wrong/stale tokens return
   false. No user equality/hash/callback executes during bookkeeping.
5. Implement read-only emit with one logical topic selection through the existing
   dictionary API and the event owner's emission.
   Unknown topic/null state is a no-op, without lazy allocation/pruning/writes.
   Implement clear as a single nullable-state reference store. No callback clear
   loop, disposal subsystem, closed-state error family or implicit finalization.
6. Complete public `##`/`@param`/`@return`, examples and ordinary module/tooling
   integration. Public details must not expose internal records/event storage.
7. Run focused semantic, persistent-state fault, GC/reload/cache, actual 1 MiB and
   performance comparisons below; resolve directly relevant failures. Complete
   the API/behavior matrix and retain evidence before Wrench source review.

## Acceptance matrix

| Area | Required complete behavior and tests |
|---|---|
| Construction/sharing | Explicit router<t>; independent local routers; assignment/fields/parameters share one identity; global in an imported module initializes once. Same module aliases share it; different application Runtimes do not. |
| Topics/types | Exact case/Unicode/NUL/empty keys, unrelated same-named topics on different typed routers, unknown topic no-op. Wrong payload width/type/nullability, callback arity/result/async and unsupported inference reject with source spans. |
| Tokens | Same callback registered twice gives two distinct handles. Remove first/second independently; alias/const handle works; wrong-router and stale/repeated handles return false. A token cannot mutate private metadata or keep callbacks/router alive. Losing a token does not unsubscribe. |
| Order/reentrancy | Stable registration order; zero/one/many subscribers; subscribe/remove/self-remove inside delivery; nested emission; remove then re-register moves new connection to the end; current snapshot unaffected. No live-container iterator invalidation. |
| Clear/lifecycle | Clear empty/repeatedly; reuse after clear; clear during delivery; clear then subscribe/nested emit uses new state while old snapshot finishes. Old handles stay inactive. No source destructor/finalizer or native-resource release is implied. |
| Payload ownership | Scalars, strings, managed/nullable/interface values, nested value/fixed/growable/dictionary payloads and existing enums/results. Deep readonly paths survive wrappers/generics; mutation through payload rejects, captured state mutation works. Existing copy/identity semantics remain. |
| Runtime failure | Snapshot failure calls no handler; handler panic stops later handlers and runs reached cleanup; earlier effects persist. Callback-local recoverable errors stay local. No catch-and-continue or implicit retries. |
| Thread/async | Global owner restrictions remain; workers reject managed transfers/indirect calls. Router cannot cross callable-containing async argument/result/receiver boundaries. Owner async locals/globals can use synchronous dispatch across their ordinary lifetime. Native foreign-thread concurrency is not granted. |
| Tooling | use channel discovery; concrete router methods/callback signatures; descriptions/return docs; active parameter docs; original definitions/references; private members filtered. Standard source rename protection and unsaved/stale-result behavior remain. |

## Persistent-router allocation and GC checks

Extend/reuse `tests/managed/probe.hpp` and existing advanced/generic/global test
infrastructure. CLI smoke tests alone do not establish publication safety. Avoid a
second fault-injection framework, custom allocator or new Runtime testing API.

- Keep an already usable router alive in a global across an injected failed
  subscription, then invoke the same application again without injection.
  Its earlier subscriptions must still dispatch and unsubscribe normally.
- Sweep the actually reached allocation sites in handle/closure/record/state,
  map preparation/growth and final event append, with a finite bound and first
  successful completion. No failed call may leave a newly delivering handler
  without a returned handle. Null-wrapper pending records may remain inert.
- After execution frames exit, collect and establish that a failed subscription's
  otherwise unreferenced captured marker is not retained by pending metadata.
  Conservative roots inside an active frame must not be mistaken for a leak.
- Exercise failed unsubscribe before event removal and after successful removal
  but during metadata cleanup/pruning. The live callback must retain its lookup
  record until removal; an inactive leftover record must not dispatch. Retry and
  clear recover under the documented partial-effects contract.
- Force failure during ordinary dictionary operations/cleanup preparation, not
  only managed heap allocation. Their BufferLayout/cursor allocations matter.
  Do not assert dictionary removal or a source defer is intrinsically infallible.
- Clear's body must contain no allocation or dictionary/event removal. Verify
  state detachment, old-snapshot rooting and later reclamation. Retained external
  handles must not retain router/callback graphs after unsubscribe/clear.
- Include capture cycles, receiver callbacks, nullable fields and failed entry
  cleanup. Do not deinitialize a subscriber-owned native resource automatically.

A stronger zero-metadata-rollback guarantee under sustained OOM is not part of
this design. Do not add copy-on-write registry cloning or a new transaction
subsystem to obtain an unrequested guarantee. Failure safety here is precise
roots, valid state, no newly live unreturnable handler, no lost active registration
and a safe state-detaching clear.

## Reload, cache, AOT and stack

Use current application-global/callback reload tests as the base:
`tests/stage11/globals.cpp` and `tests/advanced/tests.cpp`. Persist a router and
tokens, replace a handler body compatibly, retire original code and emit again.
Verify replacement behavior without re-subscribe and old active calls finishing
under their lease. Reject incompatible capture/signature/layout/instance/global
changes without disturbing previous state. No lease or native pointer in tokens.

Exercise fresh applications, repeated invocations and automatic native-object
miss/hit in optimized and no-IR-O2 modes; cache must never restore old heap state
or subscribers. Custom native binding/cache and Windows unavailable paths retain
current policy. JIT, O2 JIT and independently linked native executables must agree.
Native AOT output must be executed, not merely emitted or linked.

Use actual 1 MiB child stacks for normal/O2 JIT and linked AOT: many subscribers
with bounded loop depth; nested delivery at representative bounded depths;
large fixed/value payloads using existing heap-backed copy semantics. Reuse the
existing large-payload fixtures where practical. Do not implement recursive
subscriber traversal, arbitrary stack enlargement or a hidden async trampoline.
Unbounded user recursion is not an acceptance promise.

## Focused performance comparison

Measure direct built-in events against the completed router with identical
handlers and payloads: 0/1/16/128 subscribers per topic, one versus several
unrelated topics, repeated subscribe/unsubscribe churn, clear and bounded nested
emission. Use an unused-topic control. Record execution separately from compile
and cache state, wall/CPU/RSS, output/checksum and variation; serialize runs.

Acceptance is structural plus measured: emit touches only selected-topic
subscribers, takes one ordinary event snapshot, and adds no registry scan or
per-listener recursive depth. Empty emit must not create topic/state. Unsubscribe
includes event search, a linear registration scan and capacity/collision-dependent
dictionary deletion; successful churn must prune empty topic indexes instead of
retaining every historical name. Clear detaches once and avoids allocation in
its body. Report wrapper/snapshot/metadata costs relative to direct events rather
than invent an unsupported throughput target or new benchmark service. Optimize
only a demonstrated consequential bottleneck, preserving dispatch semantics.

## Delivery and verification boundaries

Linux focused Debug, Release and relevant strict ASan/UBSan/leak checks are the
engineering route. Run affected channel/advanced/generics/globals/readonly/cache/
LSP selections, plus normal/O2/linked AOT and actual stack gates. Use existing
runners/helpers where possible. Add a small channel-specific selection, not a
full compiler test matrix after each internal edit.

Preserve the exact `_crash.void`, imports, binary/source/tool hashes, command,
status and streams for every abnormal compiler/runner exit using the existing
incident system. Keep original failures even when corrected fixtures or retries
pass. Stop before ptrace for Boss authorization; no routine debugger is required.

Final deliverables: implemented source and examples, exact public API inventory,
completed acceptance matrix, measured performance/stack evidence, local test
receipt and an honest handoff under docs/channel/. Update the internal index,
not root README. No Git mutations, hosted CI, independent review claim or native
Windows/macOS acceptance without Boss-controlled evidence. Stop for Wrench's
implementation review and Boss's next decision.
