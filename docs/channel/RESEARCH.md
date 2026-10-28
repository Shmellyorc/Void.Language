# Channel router: source and architecture research

27 October 2026. Baseline **`72ee05db7e417645d434b3932bfc1e00ffd4ecb6`**.
Boss lifted the stdlib hold and authorized research, not implementation.
[Proposed contract](DESIGN.md), [roadmap](ROADMAP.md), [evidence](evidence/README.md).

**Recommendation:** ordinary managed `channel.router<t>` objects with string
topics, opaque subscription handles and synchronous dispatch through existing
built-in `event`. No singleton, worker queue, new LLVM operation, native callback
service or second dispatch loop is needed. `stdlib/channel.void` does not exist
at this baseline; the public API remains proposed.

## Verified foundation

| Requirement | Actual source and limits |
|---|---|
| Generic owner | Managed generic structs/methods use existing specialization. Empty construction requires explicit `router<damage>()`; later emits/expected return types do not infer it. [generic declarations:21–24](../../src/frontend/generic_declarations.inc), [specialization:67–106](../../src/frontend/generics.inc), [generic contract](../GENERICS.md). |
| Generic event | A private `_topic<t>` can own `event delivered: func(const t)` and emit through its own method. Concrete events become growable callable storage. [compiler.cpp:1784–1788,5515–5519](../../src/frontend/compiler.cpp). Scratch composition passed both JIT modes and linked AOT. |
| Dispatch | Strong ordered subscriptions, duplicates, latest matching callback removal, once-evaluated arguments, subscriber snapshots and ordinary failure cleanup. [Bible:124–129,1119–1151](../LANGUAGE_BIBLE.md), [event contract](../ADVANCED_LANGUAGE.md#events), [emit:4623–4626 / subscribe:4640–4643](../../src/frontend/compiler.cpp). |
| Event opacity | Events cannot be read, assigned, counted, indexed, cleared or passed as ordinary values. Use a managed owner wrapper, not fictional first-class `event<t>`. [compiler.cpp:2871,2899,3550](../../src/frontend/compiler.cpp). |
| Indexes | Existing `{string: _topic<t>}` and managed-identity token dictionaries; string keys compare exact bytes. Readonly identity keys work. [dictionary contract](../DICTIONARIES.md), [module.cpp:97–110](../../src/ir/module.cpp), [compiler.cpp:1743,1795](../../src/frontend/compiler.cpp). |
| Callback identity | Stable discriminator plus managed environment; bound receiver identity has its own comparison. A fresh wrapper capturing each subscribe invocation's callback distinguishes duplicate registrations. [backend.cpp:350–384](../../src/backend/llvm/backend.cpp). Scratch equality/removal probes pass. |
| Sharing | Any reachable file's outermost `var` is existing shared Runtime storage. Managed assignment, fields and parameters preserve identity; globals initialize once before entry. [RUNTIME_GLOBALS](../RUNTIME_GLOBALS.md). Global generic-event construction/dispatch is probed. |
| Privacy | Private stored fields are struct-lexical; private types/methods/initializers use existing file/module access. Direct commit fields can belong to a private internal type without being inaccessible private fields of another struct. [compiler.cpp:629–630,1590–1593](../../src/frontend/compiler.cpp). |
| Installation | Recursive stdlib source installation and compiler/VFS module discovery already exist. [CMakeLists.txt:190](../../CMakeLists.txt), [source.cpp:137–149](../../src/frontend/source.cpp). No hardcoded module registration/provider. |

Section 22 of the [stdlib/global handoff](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md)
requests an instantiated pub/sub router. Its `router()` and callback-unsubscribe
examples are illustrative. Its historical claim that globals do not exist is
superseded by the locked global contract. Current source and the Bible event
contract govern this design.

## Architecture and alternatives

Recommend a nullable private state holder containing two indexes: topic → event
owner and handle → registration. Public handles contain no callback/router/topic;
registrations hold the unique wrapper. Actual delivery calls the owner's event.
The state holder makes `clear` a single null-reference store; it is ordinary
library data, justified by allocation-failure cleanup rather than a new framework.
[DESIGN](DESIGN.md) specifies the publication and removal ordering.

| Alternative | Assessment |
|---|---|
| Callback unsubscribe | Matches built-in event but selects the latest duplicate; reconstructed closures are unreliable keys. |
| Numeric ID + owner marker | Works, but adds counters, exhaustion policy and ownership metadata unnecessarily. |
| Direct registration token | Simpler lookup, but a retained token can retain a bucket's other callbacks; clearing its fields conflicts with const token views. |
| Managed identity-only handle | Recommended: exact duplicate removal, harmless foreign/stale handles, no counter and no retained callback through an external token. Existing dictionary membership supplies ownership. |
| Flat callback-array router | Fewer indexes, but scans unrelated topics and reproduces event snapshot/dispatch code. No current dependency justifies a second delivery implementation. |
| Typed topics in a heterogeneous router | Requires a heterogeneous storage/dispatch contract that VOID does not currently expose safely. Separate `router<t>` instances are simpler; an application may explicitly use one closed payload enum. |

The wrapper must capture the original handler directly. A wrapper reading a
mutable registration's `active`/handler field would suppress already snapshotted
callbacks after unsubscribe/clear, changing accepted event semantics.

## Allocation and failure findings

Dictionary cleanup is **not allocation-free**. Layout preparation creates a
`BufferLayout` with `new` ([buffer.cpp:83–85](../../src/runtime/buffer.cpp));
cursors reserve storage at lines 29–44. Dictionary read/remove/clear and event
removal enter these paths. A defer rollback can fail under sustained OOM, and
its successful-path cleanup still runs. Do not promise a transaction merely
because rollback code was registered before mutation.

Use metadata-first preparation with null-wrapper pending records, then event
`+=` as the last fallible publication. Existing list insertion is transactional
([array.cpp:148–165](../../src/runtime/array.cpp)). A direct record-wrapper store
and managed-handle return follow, with no lookup/helper/defer/fixed-copy work.
Direct nullable callable/reference writes are LLVM stores
([backend.cpp:1127–1142](../../src/backend/llvm/backend.cpp)). Inspect the concrete
IR during engineering rather than assuming a refactored helper has the same tail.

If preparation fails, inert metadata may survive, but it must retain no failed
handler or newly dispatchable callback. Remove event first on unsubscribe, then
clear its stored wrapper before fallible metadata deletion/pruning. Allocation
failure is ordinary checked Runtime failure, not a channel `Err` or a promise to
roll back earlier side effects. The single-reference `clear` body can discard
inert state without dictionary cleanup. Persistent-router fault injection remains
required; success probes do not prove those failure invariants.

## GC, reload, cache and native boundaries

Callables/captures are ordinary precisely traced objects. Strong subscriptions
retain their captures; unreachable cycles are collectible. GC never calls source
`deinit`. Clear/unsubscribe drop router ownership, not subscribers' native resource
ownership. Active frames and conservative roots can retain old objects until exit.

Source callbacks dispatch under the caller generation's lease. Heap metadata has
no native code pointer/generation lease. Existing tests retain a global callback,
retire its original code and execute the replacement body against shared state
([globals.cpp:129–139](../../tests/stage11/globals.cpp)). Do not pin registration-time
code per token. Signature/capture/layout/specialization/global-initializer changes
still obey existing conservative restart rules.

Current [automatic object caching](../compiler-health/stabilization/CACHE.md)
supports optimized and no-IR-O2 modes separately, superseding old O0-only claims.
Artifacts store code/metadata, never live routers/subscriptions. New application
Runtimes get fresh state; compatible replacement preserves existing globals.
JIT/O2/AOT use the same source/IR/backend path.

Native `CallbackRegistration` has different C trampoline, quiescence and exact
code-lease requirements. It is unnecessary for source pub/sub; forwarding stored
source callables into native registration remains unsupported
([NATIVE_LIBRARIES:117–135](../NATIVE_LIBRARIES.md)). Channel adds no FFI promises.

## Thread, async and readonly boundaries

Use an instance synchronously on its application/owning execution thread.
GC locks do not make its indexes or subscriber state thread-safe. No source
`ownerthread` annotation or per-router thread stamp exists. Globals enforce
application ownership ([abi.cpp:58–70](../../src/runtime/abi.cpp)); arbitrary
local instances gain no additional runtime check. Native embedding must obey
this same serialized-use precondition.

Workers reject managed transfers and worker-reachable event/indirect calls.
Callable-containing graphs cannot cross async arguments/results, including router
receivers, even when an instance is empty ([module.cpp:199–213,652,1433](../../src/ir/module.cpp),
[capabilities.inc:210–222,608–616](../../src/frontend/capabilities.inc)). An owner
async activation may create/use a router locally across await or access a global
and call synchronous methods. This is not a blanket async ban. An identity-only
handle has no callable graph; transferring it does not transfer a router or grant
worker/global access. No automatic asynchronous delivery or async handlers.

Recommend `func(const t)` and `const payload: t`: probes accept const managed
payloads and readonly event owners; mutation-capable handlers reject. This is a
proposed channel policy, not a change to public events. Captured game state remains
mutable. Readonly is an authority view, not deep cloning/freezing. Other aliases
may still mutate a referenced payload. Readonly routers may emit but cannot mutate
subscriptions or clear.

## Mature designs and tooling

Node EventEmitter documents ordered synchronous listeners, duplicates and an
existing emission finishing despite listener removal. These agree with VOID's
existing events; its dynamic payloads, special error event and broad API are not
proposed. [Official Node events](https://nodejs.org/api/events.html#emitterremovelistenereventname-listener).
Qt connection handles demonstrate identity independent of a callback expression;
destroying a plain handle does not disconnect. We borrow that distinction, not
Qt threading/ownership. [Official Qt handle](https://doc.qt.io/qt-6/qmetaobject-connection.html).
.NET observables return IDisposable for explicit unsubscription; that alternative
would require an owner/cleanup-bearing token. [Official Subscribe](https://learn.microsoft.com/en-us/dotnet/api/system.iobservable-1.subscribe).

Existing compiler documentation/generic/visibility/VFS metadata supplies normal
completion, hover, parameter help and navigation. LSP already carries those data:
[3.17 completion](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/completion.md),
[hover](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/hover.md),
[signature help](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/signatureHelp.md).
Use [ordinary VOID documentation](../DOCUMENTATION_HOVER.md), with no client parser,
protocol extension or hardcoded module/topic list. Dynamic runtime topic strings
do not gain automatic literal completion or compile-time typo diagnostics.

## Performance and proof limits

Emit selects one topic through the ordinary dictionary API, snapshots K callbacks
and performs K synchronous
wrapper/handler calls. Snapshot storage is O(K), not allocation-free. Exact tokens
cost one wrapper/capture plus a small handle/record per subscription. Unsubscribe
requires reverse event search and a linear active-registration scan, plus
capacity/collision-dependent dictionary deletion. Removal rebuilds the existing
index across retained capacity ([dictionary.cpp:57,78–79](../../src/runtime/dictionary.cpp));
it is not bounded only by the current live count after heavy churn. Clear's body
is one reference store; reclamation is later GC
work. There is no recursive call per subscriber, but nested dispatch consumes
O(depth) native stack. No unbounded recursion or cross-platform timing guarantee.

One logical selection is not a promised single hash probe: source `get` lowers
to contains then get ([compiler.cpp:4190–4201](../../src/frontend/compiler.cpp)).
Dictionary reads also prepare allocating Runtime layouts. Even an unknown-topic
emit can fail with ordinary allocation failure; it must not create routing state.

Measure direct event versus router at representative topic/subscriber counts and
fixed/value payload sizes. Do not optimize Runtime/LLVM before that comparison.
The [evidence](evidence/README.md) establishes capabilities, not finished-router
fault safety, persistent-router reload acceptance, strict sanitizers or native
Windows/macOS execution. No production edit, Git mutation, ptrace or hosted CI.
Stop for Golden Wrench architecture review and Boss approval.
