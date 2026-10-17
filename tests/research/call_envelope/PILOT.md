> Historical research report. Order 2 replaced the envelope/protocol code and
> its certificate drivers with direct LLVM calls and behavioral tests. See
> [README.md](README.md) for the current implementation and commands. Statements
> below describe the earlier snapshot only.

> Historical pilot record. Current integration and runner: [README.md](README.md).

# Stage 3B explicit call-envelope pilot — owner/Wrench review stop

Research only, against `7abc6249a5e1828fd49232a868dcc300b13b7894`.
No production ABI, LLVM fork, auditor, public AOT/ORC gate, or Stage 3C change.

The envelope materially reduces **incoming stack-argument provenance** work on
this Linux x64 host: four unproved incoming slots become zero. It does **not**
complete a generated fixed-array calling proof. The normal boundary receives a
storage certificate; O2 still rejects a tail call and its encoded-call mismatch.
These gates are unchanged. Stop here for owner and Wrench review before any
production lowering integration.

## Concrete fixture

- `envelope.hpp`: a bounded 96-byte same-build envelope on this host. It holds a
  signature/layout identity pointer, a per-call epoch, eight snapshot-home
  addresses, the caller result-home address, and failure context. Payload extent
  never changes its size. `CalleeState` is nine pointer words (72 bytes).
- `boundary.cpp`: closed, out-of-line caller/callee definitions. Their calling
  boundary takes exactly **one pointer**. Actual Clang 19.1.7 output is used as
  LLVM input. This is not VOID frontend/lowering output.
- `runtime.cpp`: links both definitions and all hooks against the existing
  Runtime and `vd_fixed_*` implementation. A separate, synchronous, thread-local
  active-call record authorizes exact home addresses and their roles. It is
  created by the harness, not inferred from the envelope or stack positions.
  This is a closed fixture registry, not a proposed compiler-wide ownership
  system or permission to accept arbitrary incoming pointers.
- `audit.cpp`: standalone client of the unchanged `AotObjectEmitter`. It
  inventories every original alloca before freeze; the state is root storage
  and saved formal words are parameter storage. This inventory describes
  storage, not semantic authority over referenced objects.
- `run.py`: reproduces normal/O2 native witnesses, 1 MiB stack runs, storage
  observations, and the existing auditor's full fixture/negative set. It reuses
  an existing CMake Makefiles build's library closure without editing CMake.

The identity check verifies the same-build envelope size/alignment/field offsets,
revision, eight-argument shape, admitted u64 layout identity and fixed extent
against independent expected data. Exact identity pointer, epoch, envelope
address, argument order, result address and failure address must also match.
This is not a serialized, cross-target, cryptographic or open-world ABI identity.
Epochs and the active record cover only this synchronous fixture; recursion,
concurrency, cancellation and epoch exhaustion are not designed here.

## Roots, snapshots, completion and failure

The caller registers ten one-word homes in a real `Runtime::Frame` before any
fixed construction/copy. It finishes each of eight independent snapshots, then
mutates its source before taking the next snapshot. The callee sees 7 through
14 while the source reaches 15. Pairwise payload identity checks also establish
independence. A separate caller result home begins with a rooted sentinel.

Entry validates each actual caller root with `root_word`/`fixed_scalar` and exact
layout before loading handles. It validates result registration separately.
These operations do not poll (`src/runtime/buffer.cpp`). It then installs a
callee Frame for all eight handles before the first possible managed safepoint.
Root Slot/Frame bookkeeping is a bounded native heap allocation in this pilot;
it contains no array payload. Frame construction itself does not poll
(`src/runtime/coordination.cpp`). Caller registrations remain live throughout.

Immediately before `vd_gc_collect`, the fixture checks all callee registrations;
after collection it reads all eight expected contents again. Callee argument 0
is a handoff of an independent caller snapshot, not an alias of the original
source or another argument. A transactional `vd_fixed_copy` publishes a fresh
result into the independently authorized caller home. Mutating argument 0 after
return leaves the result unchanged. The source and argument 1 remain unchanged.

Every successfully entered path releases its callee Frame. A saved root token is
then deliberately used while stack storage is still live: Runtime must reject
its expired registration. All homes are finally cleared and collection must
leave zero managed objects. This checks private temporary storage/root retention
as well as visible result contents.

Fourteen negative cases check their **specific rejection message** and invariant:

1. Missing argument registration despite an authorized home address.
2. Stale identity pointer with identical copied descriptor fields.
3. Descriptor extent/layout disagreement.
4. Swapped registered argument homes.
5. Result redirected to an argument home.
6. Authorized but unregistered result address.
7. Failure after registration, before the first poll.
8. Failure after the first collection.
9. Out-of-bounds mutation (the existing bridge reports invalid fixed contract).
10. Result destination changed to a callee home at publication time.
11. Omitted cleanup/publication: exit checking detects it and repairs registration
    **before stack homes die**. This is a test-only repair, not accepted behavior.
12. A preexisting failure must suppress work and retain its original message.
13. A replayed per-call epoch.
14. Invalid copy work quota after mutation, exercising transactional copy rejection
    and lease/temporary cleanup without publishing the caller result.

Failures retain the original caller result pointer; none publishes a partial
result. No collection occurs through intentionally dangling Frame storage. The
pilot does not inject every allocator failure ordinal; the existing `gc_buffers`
suite retains the fixed helper allocation-failure sweeps and fatal controls.

## Focused local evidence (17 October 2026)

Host: Linux x86_64, Debian Clang/LLVM **19.1.7**, existing `build/ci-debug` libraries.
Reproduce from the repository root, choosing a fresh output directory:

```sh
python3 tests/research/call_envelope/run.py \
  --build build/ci-debug --output build/stage3b-envelope/review
```

Evidence is in `build/stage3b-envelope/review`: exact commands, source hashes,
raw/adapted LLVM IR, native logs, sentinel-preserving rejection logs, complete
JSON reports and a fresh unchanged ten-pointer baseline under `baseline/`.

**Input distinction:** raw Clang IR is rejected at freeze because its
`target-cpu=x86-64` and feature attributes differ from the emitter's
`generic`/empty profile. That rejection is retained. The standalone adapter
removes only `target-cpu`, `tune-cpu`, `target-features`, `target-abi` attributes
**before** inventory/freeze, for this portable POD-only source, and saves the
adapted IR. DataLayout must already match. The emitter establishes its own target
profile; all subsequent digest, inventory and machine checks run unchanged.
The raw output must not be represented as directly certified Clang code.

| Research input / profile | P | F | R | E | A | C | Incoming slots | Outgoing bytes | Storage delivery |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Ten-pointer / normal | 8 | 40 | 8 | 8 | 16 | 48 | 4 | 32 | rejected |
| Ten-pointer / O2 | 8 | 40 | 8 | 8 | 16 | 48 | 4 | 32 | rejected |
| Envelope callee / normal | 80 | 104 | 80 | 8 | 16 | 112 | 0 | 0 | accepted |
| Envelope caller / normal | 8 | 24 | 8 | 8 | 16 | 32 | 0 | 0 | accepted |
| Envelope callee / O2 | 80 | 104 | 72 | 8 | 16 | 128 | 0 | 0 | module rejected |
| Envelope caller / O2 | 8 | 24 | 0 | 8 | 16 | 48 | 0 | 0 | tail/call mismatch |

Here F is the report's `frame`; C is its existing research charge. The research
65,536-byte threshold is unchanged. These per-function charges are not a total
stack bound for hooks or the executable. The ten-pointer fixture only forwards
homes; the envelope fixture also has explicit callee state and checks. Its larger
frame is not an apples-to-apples performance comparison. Native metadata and
heap payload costs are not counted as certified stack storage.

Normal/O2 C++ witnesses each pass 16 cases: the fourteen negatives, normal small
extent, and normal 131,072-u64 (1 MiB) payload extent. Each is also run under an
actual 1,048,576-byte stack limit: **64 case executions**. These ordinary C++
executables are built separately from the auditor candidates. **No audited
candidate, including the normally accepted one, is linked or executed.**

The unchanged storage-auditor suite passes and preserves all four incoming-slot
rejections in both ten-pointer reports. A separate focused `gc_buffers` CTest
passes (1/1). The new witness also passes all 16 cases with ASan/UBSan and leak
detection against `build/ci-sanitizers/libvoid_runtime.a`. The initial sandboxed
LeakSanitizer run could not inspect the process; the unrestricted rerun passes
with `detect_leaks=1:halt_on_error=1:exitcode=87` and
`halt_on_error=1:print_stacktrace=1:exitcode=86`. Logs are retained in
`build/stage3b-envelope/`. No sanitizer suppression or weaker flag was used.

## Remaining proof burden and decision

The single pointer removes the packed-store problem for separate incoming
arguments in this observed shape. Semantic obligations become explicit exact
home/identity checks and root lifetimes. This is a useful reduction and warrants
owner/Wrench review as an alternative; it is not a reason to relax incoming
ownership gates or approve a production ABI now.

O2 still emits a tail call to `pilot_caller_check`, so the unchanged module
delivery rejects `unsupported dynamic/tail/realigned frame` and
`final encoded calls differ from machine certificate`. That rejection and its
destination sentinel remain intact. No tail-proof exception was added.

The machine certificate covers the bounded boundary translation unit, not
the independently compiled hook implementations, caller preparation, registry
authority or complete root/failure protocol. The runtime assertions provide
closed-fixture evidence, not a final-byte semantic/root certificate. Full VOID
generated preparation, envelope construction, schema/signature derivation,
root/failure cleanup lowering, helper provenance, caller/callee identity binding
through optimization, ORC, and other targets remain unverified integration work.
No arbitrary incoming ownership acceptance is introduced. Existing public AOT/ORC
acceptance gates and the production/private ABI remain unchanged.

No commits, pushes, branches or hosted CI. The preexisting Language Bible edit
is preserved. **Owner and Wrench review required before integration; Stage 3C
remains locked.**
