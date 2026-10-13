# Phase 17: managed custom initialization

Implements owner-approved **S1 + MI1–MI7** from
[MANAGED_INITIALIZATION_PROPOSAL.md](MANAGED_INITIALIZATION_PROPOSAL.md).
Phase 16 is locked: the owner reports run #28 passed all seven jobs, 35 suites
per configuration. Phase 17 preserves P3 deep readonly capabilities and P4
complete-object publication. Local verification and pending hosted gates are in
[WORK_LOG.md](WORK_LOG.md).

## Source contract

```swift
struct child:
    value: i32

struct parent:
    child: child
    total: i32
    next: parent?

func add(p: parent, amount: i32):
    p.total += amount

func (p parent) init(c: child, amount: i32 = 2):
    p.child = c
    var view = p
    add(view, amount)

func entry(value: i32) -> i32:
    var c = child(value = value)
    const p = parent(amount = value, c = c)
    return p.total + p.child.value
```

One owner-module initializer has typed parameters and no result annotation. An
empty return or fallthrough completes it. Constructor arguments bind exclusively
to initializer parameters. Types without init retain named-field construction.
There is no mixed parameter/field namespace, constructor overload or fallback.
C1's positional prefix, named arguments, exact contextual typing, pure parameter
defaults, visibility and error rules apply unchanged. Required parameters precede
optional ones; no positional arguments follow named ones. Direct `.init()` and
type-qualified lifecycle calls remain invalid, even inside the owner module.

Supplied arguments run once in written order and are saved before field preparation.
Pure omitted parameter defaults add no runtime effects. Numeric/bool and nullable
fields receive their established safe/pure defaults; reference-free value fields
complete their Phase 14 nested construction in declaration order. Required value
children must remain default-constructible before a custom parent body. Direct
nonnullable managed fields instead start *unavailable* in private storage and require
explicit assignment from a completed reference. No implicit managed child allocation
is introduced. `var n: node` still needs an explicit initializer, even if `node()`
would accept no arguments; `var n: node?` still defaults to null. Compile-time
constants and pure defaults cannot allocate managed objects or execute init.

## Construction views and definite initialization

The initializer receiver denotes a private construction place, not a heap object
or ordinary reference. It has no managed identity. Numeric/value/nullable fields
are available after their defaults complete. A required reference field must be
assigned before a read, compound assignment or dereferenced/subfield mutation,
and on every successful exit. Branch joins intersect definite readiness;
conditional loops may execute zero times, while unbounded loops account for their
break paths. Failure exits do not need completed fields because they never commit.

Local aliases (`var alias = p`, including nominal annotations) retain the same
construction place and readiness. Const aliases remain readonly. Neither receiver
nor alias may be rebound. Whole construction views cannot be returned, cast,
converted to nullable references, compared for identity, retained in objects,
passed as constructor arguments or sent to opaque boundaries. This includes
self-links: establish those using the completed result after construction.
Completed child references read from fields are ordinary references under P3;
reference-free value field copies remain independent.

Before passing a whole view to any source helper/method, **all required fields
must be ready**. The existing typed body is checked in a private construction
passing context: field operations, allocation of completed children, compatible
calls/recursion, and completed child/scalar/value returns are supported. Identity,
whole-view returns, rebinding and retention make that context unsafe. Diagnostics
point at the constructor-context call and relate the prohibited helper use.
Helpers cannot perform the *first* required-field initialization in this phase.
Ordinary calls to those same helpers retain ordinary reference semantics.

Contexts are keyed by declaration and construction-parameter mask, not object or
call instances. Only reachable contexts are materialized, with a project-wide
1024-context diagnostic limit. Recursive calls reuse contexts. Several parameters
may denote the same place. No new source ownership annotations are required.

P3 is unchanged: copying a readonly reference into var or annotating/casting it
cannot recover mutation. Readonly arguments cannot be stored into mutable fields.
A fresh receiver is mutable until successful result qualification; a const result
does not freeze independent mutable aliases to completed child objects.

## Publication, roots and failure

The shared lowerer saves arguments, opens a `stage_begin` place, prepares defaults,
executes exactly one constructor-marked initializer call, then reads its finished
fields into the existing `gc_construct`. The parent acquires managed identity
**only at final successful commit**. No partially initialized parent reference,
handle or root exists. Assignment destinations are updated after success.

The same nominal Type carries a private construction capability; it cannot be
stored as a nominal field or used as an ordinary managed value. IR validation
independently proves storage/alias origins, active scopes, definite reads, helper
readiness, marked once-only initialization and final-field commit. It rejects
custom construction that omits its proven stage, reuse before commit, and active
uncommitted stages on successful exits. Legacy internal allocation proofs remain.

LLVM uses target-aligned stack payload storage and the existing root Frame/Slot
ABI. Owned payloads are zeroed before frame publication and when a fresh loop
construction begins. Root descriptions flatten actual mutable *field storage*,
including helper parameter storage; a pointer to staging bytes is never a managed
root. Every nonnull registered leaf denotes an already completed heap object.
Aliases do not create stale field copies. Existing expression, argument, return
and selected-owner roots remain active across normal/O2 polls and allocation.
Module-owned DataLayout, debug layout and code-generation leases are preserved.

The existing Runtime::construct_into and Heap commit are unchanged. Collection,
parking and fallible work precede the final locked copy/publication interval.
No second heap, pending-object registry, GC suppression, statepoint experiment,
constructor allocator or alternate backend was introduced.

Argument/default/child/body/arithmetic/conversion/collection/commit failures
propagate through the existing source failure channel, unlink frames and skip
remaining steps. A failed rendezvous never sweeps. No parent is published or
assignment destination replaced on failure. Earlier effects on completed objects
are **not rolled back**; the body precedes final parent allocation, so its effects
may remain if that allocation fails. There is no retry or rerun of init. Completed
children may outlive the failed parent if independently reachable. No incomplete
parent allocation needs rollback. Managed reachability remains independent of JIT
code lifetime; active code leases and native-provider quiescence remain authoritative.

## Shared analysis, tooling and reload

Validated declarations expose construction parameter masks and required entry-field
names alongside existing initializer parameter/default spans and policy contracts.
The same typed AST retains proven construction aliases and call contexts for lowering.
Constructor references navigate the original init; helper variants navigate/report
the original source helper. Invalid unsaved readiness source invalidates queries,
and repair restores them through the existing session/LSP path.

Compatibility includes initializer presence, owner/access, labels/order/types/pure
default syntax, layout/default policy, readonly/effects and construction contexts.
Initializer bodies and transitive helper/constructor bodies retain C1's conservative
restart requirement. Compatible unrelated entry/body edits still reload using the
same runtime. No migration, automatic restart or reinitialization is implied.

## Real source execution and acceptance

`examples/managed_initialization/` includes independent main, GC/helper stress,
numeric, imported private-factory and four failure fixtures. With input 10:

```sh
build/ci-debug/voidc check examples/managed_initialization/main.void
build/ci-debug/voidc run examples/managed_initialization/main.void 10 --no-opt # 26
build/ci-debug/voidc run examples/managed_initialization/main.void 10          # 26
build/ci-debug/voidc emit examples/managed_initialization/main.void -o build/managed-init.o
clang++-19 build/managed-init.o build/ci-debug/libvoid_runtime.a -pthread \
  --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/managed-init
build/managed-init 10                                                      # 26
```

Windows uses the configured MSVC/SDK library environment, including the existing
legacy_stdio_definitions library. The source entry signature/bootstrap and runtime-only
AOT link contract are unchanged; no arbitrary public managed C ABI is claimed.

Four additional suites extend 35 to **39**, preserving existing suites/assertions
except the explicitly authorized obsolete managed-init deferral rejection:

- `managed_initialization_source`: normal/O2 source, readiness/escape/readonly
  diagnostics, malformed IR, bounded recursive/multiple-alias contexts, actual
  root-leaf inspection, forced/budget GC, body-before-publication observation,
  code retirement, protected-output allocation failure, deterministic safepoint
  overlap, commit-time failed rendezvous/no-sweep/recovery, reload and analysis facts.
- `managed_initialization_aot_normal`, `managed_initialization_aot_optimized`:
  actual emitted source linked with runtime-only dependencies, precise stage roots,
  GC, commit OOM, retained body effects, recovery and bootstrap failures.
- `managed_initialization_cli`: eight source fixtures, three inputs, both modes
  and JIT/linked AOT (96 executions), including source failure traces.

The existing LSP protocol suite adds imported custom constructor/helper navigation
and unsaved readiness failure/repair. Test-only ABI probes forward to the real
runtime; they do not reconstruct source IR or replace GC. Precise root assertions
inspect actual registered storage; timing tests gate real operations without sleeps.
WORK_LOG.md records full Linux matrices, stress, small-stack and editor evidence.
Hosted acceptance remains all seven configurations, including Windows and macOS
ARM64 ABI/layout/O2 execution and Debug editor runs; Linux cannot certify these.

## Deferred work

Self identity/self-links during construction, borrowed view returns/rebinding,
helper-assisted first initialization, reference-containing value structs, required
value-field definite initialization, runtime defaults, ordinary named/default calls,
fallible constructor results, resources/deinit/finalizers, native managed exports,
strings/collections, generics/inheritance/interfaces and source threading remain
future design work. These are current boundaries, not permanent language bans.
No production GC readiness or arbitrary native-provider teardown guarantee is claimed.

Recommended next phase: design reference-containing value capabilities and their
copy/readonly/root contracts before implementing them. Broader construction-view
escape or identity support requires its own approved publication/lifetime design.
