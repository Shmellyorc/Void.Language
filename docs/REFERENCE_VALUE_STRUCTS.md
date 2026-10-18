# Reference-containing value structs (Phase 18)

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

The owner approved RV1–RV7 in REFERENCE_VALUE_STRUCTS_PROPOSAL.md. This guide
supersedes historical exclusions of managed references from source value fields.
The existing frontend, semantic model, IR, LLVM 19.1.7 backend and coordinated
runtime implement this extension. Verification evidence belongs in WORK_LOG.md;
local Linux results do not establish Windows/macOS acceptance.

## Working source surface

A value may contain numeric/bool fields, nested values and nullable/nonnullable
managed references. Copying duplicates inline storage, including reference bits.
It preserves referenced object identity; it never clones an object.

```swift
struct node:
    value: i32

value struct packet:
    owner: node
    count: i32

func changed(p: packet) -> packet:
    p.count++
    p.owner.value++
    return p

func entry(value: i32) -> i32:
    var n = node(value = value)
    var a = packet(owner = n, count = 10)
    var b = changed(a)
    b.owner = node(value = 90)
    return a.count + b.count + n.value
```

For input 10 this returns 32: a.count remains 10, b.count becomes 11, and the
shared original node becomes 11. The real copies.void example also checks identity.
Nullable references use existing contextual null and checked same-nominal `as`.
Inline recursive layouts remain rejected; references break inline layout recursion.
The stress example makes a managed cycle through nested inline value fields.

## Readonly authority and writable storage

Reference authority is tracked at each inline reference leaf. A local const or
const value formal provides deep readonly reference views. Copying to var permits
mutation of independent inline storage, but preserves each referent's authority.
Unrelated mutable leaves remain mutable. Plain value formals require mutable
reference leaves; const formals accept either authority and treat them as readonly.

```swift
# Inside a function, with mutable node m and readonly node r:
var p = pair(left = r, right = m)
var q = p
q.count++                     # Independent inline storage.
var selected = q.right as node
selected.value++              # Mutable authority to the right referent.
# Mutation through q.left remains rejected.
```

See readonly.void for a complete executable example.

A binding's inferred leaf profile is fixed. An explicit annotation cannot thaw it.
A readonly leaf slot in a var copy may be replaced with a fresh mutable reference;
subsequent reads of that slot still provide readonly access. The independent fresh
mutable alias remains usable. Assigning a readonly carrier into existing mutable
leaf storage, a plain parameter, or recursively mutable managed field storage is
rejected. Managed fields cannot acquire instance-dependent readonly profiles.

Receiver effects distinguish inline storage writes from mutations/retention through
references and requirements to enter mutable parameter/storage contracts. Even a
currently read-only plain helper requires mutable reference leaves; use const
formals to accept readonly views. Requirements propagate through inferred receiver
effects rather than introducing permissive plain-parameter borrow contexts. Referent-only methods can operate on mutable-reference temporaries;
inline-mutating methods require writable selected storage. Mutating a reference-free
inline field inside a heap object still requires authority over the selected heap
owner. Receiver-dependent return and storage summaries preserve leaf provenance.

## Construction, defaults and publication

Nullable leaves safely default to null. Nonnullable references have no implicit
allocation or zero source default. No-init named construction must supply direct
required references and complete required nested values. `var p: packet` uses the
same construction checks. A value constructor assembles complete field operands;
it does not start from a logically invalid all-zero required-reference aggregate.

C1 remains unchanged: every value field is valid before custom value init begins.
A value with a direct required-reference field therefore uses explicit no-init
construction in this phase. Required nested values cannot be deferred into a value
initializer or an enclosing managed initializer. S1 readiness continues to cover
direct managed construction-view fields only.

Fresh value init has a fixed conservative reference profile, inferred from defaults,
assignments, const inputs and helper results, joining both sides of branches. The
whole body, including copies made before an assignment, is checked under that
profile. Permissions never depend on runtime branch outcomes. No constructor
specialization by readonly mask or new ownership annotation is introduced.

Pure, init-free all-null aggregate templates may be module constants and declared
field defaults. Their materialized literal storage is authority-neutral. A runtime
const view remains readonly even when its current reference bits are null. A const
formal still qualifies a supplied all-null template. Live objects, allocations and
custom init remain forbidden in pure compile-time expressions.

Initializer argument snapshots retain existing source order. Omitted nested
construction follows existing C1 defaults. Initializers run once. S1 views cannot
be hidden in aggregate constructor operands, stored references or returns. Managed
identity still begins only at successful complete-object commit; embedded references
always target completed objects. Checked publication validates nested nonnullable
leaves before commit. Failure does not publish the destination or retain root frames;
effects already performed on completed objects are not rolled back.

## Shared representation and roots

Type carries a compile-time readonly path set, separate from nominal equality and
native representation. Paths cross inline fields and stop at a managed reference.
The shared logical shape drives capability projection, summaries and LLVM target
root offsets. Private construction-view readonly places remain distinct from heap
reference leaves; staging pointers never become managed roots.

The source capability/effect fixed point consumes the nominally checked AST.
Independent IR validation derives aggregate, extraction, insertion, call, receiver,
store and return capabilities without trusting source summaries. Provenance/profile facts stabilize before mutable-boundary constraints;
inferred alias capacities preserve their original formal-leaf origins, while
independent mutable storage cannot accept readonly inputs. A CFG must-proof
rejects reads of required-reference storage before initialization. Existing S1 scope,
readiness, once-only init and commit proofs remain authoritative.

LLVM uses the existing fixed Frame/Slot homes for locals, parameters, instruction
temporaries and actual selected receiver storage. Parameters are rooted before the
first poll. Argument snapshots, aggregate return handoffs, loop polls and checked
failure cleanup use the shared lowering. A selected heap owner remains separately
rooted; an interior payload address does not retain it. Whole-value stores complete
without a safepoint between constituent writes. Fixed homes can retain dead values
until frame exit. There are no conservative scans, runtime capability tags, new
allocator, alternative root registry, volatile roots or optimizer exemptions.

Heap descriptors contain copied numeric offsets, independent of LLVM context and
code-generation leases. A separate retired-generation test retains a cycle with an
explicit host handle to prove that independence; the aggregate-only root test uses
no host handles. This distinction is intentional.

## Tooling, reload and boundaries

Shared analyzed declarations carry qualified result/receiver facts and exact effect
contracts. Metadata comparison and conservative source compatibility include leaf
profiles. Existing type/field/method ownership and imported navigation are reused.
The LSP regression covers imported aggregate references, precise readonly diagnostics,
invalid-snapshot query rejection and unsaved repair. Diagnostics name failing reference paths and link field/formal declarations.
No second tooling type model,
new completion/hover feature or editor syntax is needed.

Normal/O2 JIT and independently linked AOT exercise the same parsed/validated source.
Aggregate calling conventions remain private compiler contracts, not an external C
ABI. The existing i32 entry adapter, native provider/quiescence rules, thread model
and generation leases are unchanged. Legacy experimental allocation IR proofs are
preserved; source construction continues to use complete publication.

Deferred: richer unavailable-value initialization, readonly nominal heap fields,
flow-sensitive permission upgrades, public aggregate/reference FFI, strings,
collections, generics, inheritance, finalizers and broader entry-point policy.
Existing inline layout/resource bounds remain. Fixed root homes may increase stack
pressure; the 1 MiB semantic regression is not a guarantee for arbitrary huge native
value frames.

## Reproduce source execution

```sh
build/ci-debug/voidc run examples/reference_values/copies.void 10 --no-opt
build/ci-debug/voidc run examples/reference_values/copies.void 10
build/ci-debug/void_reference_values_cli_copies_normal 10
build/ci-debug/void_reference_values_cli_copies_optimized 10
ctest --test-dir build/ci-debug -R '^(reference_values_.*|core|lsp_protocol)$' --output-on-failure
```

All four execution commands print 32. The native executables link source-emitted
objects against void_runtime, without linking LLVM. The new family adds source,
normal AOT, optimized AOT and CLI suites while retaining the preceding 39 suites.
See WORK_LOG.md for the final SDK/configuration matrix and hosted verification gates.


## Phase 19 composition

Owner-approved stored `using name: type` and `using type` composition, one-hop
member promotion and original embedded receiver mutation are implemented through
the existing paths. See [COMPOSITION.md](COMPOSITION.md) for exact lookup, access,
readonly, initialization, tooling and reload behavior. This adds neither subtyping
nor stateless reuse and does not change C1/S1/P3/P4/RV1–RV7. Inheritance, interfaces,
virtual dispatch, transitive promotion and has remain deferred.
