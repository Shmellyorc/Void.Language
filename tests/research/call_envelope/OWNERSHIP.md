> Historical research report. Order 2 replaced the envelope/protocol code and
> its certificate drivers with direct LLVM calls and behavioral tests. See
> [README.md](README.md) for the current implementation and commands. Statements
> below describe the earlier snapshot only.

# Stage 3B generated ownership integration — owner/Wrench review stop

Accepted baseline: `7abc6249a5e1828fd49232a868dcc300b13b7894`. Research only,
Linux x64, pinned LLVM/Clang 19.1.7. No production ABI, public AOT/ORC activation,
Stage 3C, LLVM fork, general ABI-provenance rewrite or production 64 KiB policy.

Earlier evidence is preserved: [pilot](PILOT.md), [preceding integration](INTEGRATION.md),
`build/stage3b-envelope/integration-final`, its sanitizer evidence and review ZIP.
This checkpoint's evidence is under `build/stage3b-ownership/review`.

## Review findings and corrections

The preceding generated callee delegated authorization to `research_validate` and
`research_context_validate`, both backed by a test-only active-call registry.
Those functions are no longer emitted or linked. The old registration protocol
was real, but its identity/order permission was test authority. The new callee
checks those relationships in generated code. The test registry remains only an
observer and fault injector; no hook returns permission to proceed.

Two other defects were corrected while binding type authority. Registration and
a supplied layout did not establish that the layout represented the compiler's
u64 element type and extent. Runtime now compares those compiler-derived values
to its existing admitted scalar schema, without another schema/type analyzer.
Also, an output place must not be treated as an initialized input value: separate
output validation accepts a registered null result home. The original result
pointer remains unchanged after failures.

The private callee now has internal linkage. A direct caller-to-callee edge is
included among the required operations, closing a gap where helper operations
could have remained in an unused callee after removal of the actual handoff.

## Where authority comes from

The compiler synthesizes the fixture's source-effect wrapper using existing
owned IR (`argument`, local `store`/`load`, `fixed_snapshot`, `fixed_select`,
`fixed_commit`, `call`, `return`) and runs the existing shared `ir::validate`.
There is no new ownership rule engine. Each snapshot is fresh and consumed once
at its call argument; later source mutation cannot reuse a stale borrowed view.
The snapshot IR values are 3, 9, 15, 21, 27, 33, 39 and 45 in this wrapper.

The actual callee body is validated owned IR. It may return any one of the eight
same-layout scalar-u64-array arguments; arguments 0 and 3 are exercised. A valid
but unsupported body containing an additional snapshot still rejects explicitly.
This remains a fixed research wrapper, not lowering of arbitrary caller IR or
native execution of a VOID source-array program. Frontend analysis supplies
canonical types; the driver builds the private owned IR explicitly.

| Relationship | Generated/shared authority | Independent evidence |
| --- | --- | --- |
| IR snapshot to caller home | Shared freshness/consumption validation; actual `vd_fixed_copy` destination | Original LLVM checks and distinct payload/content assertions |
| Caller home to registered root | Same SSA address in its Slot storage field; real Frame entry before copying | Runtime exact-Frame membership check |
| Caller home to envelope argument | Same SSA home stored in exact ordinal envelope and CallBinding fields | Original typed-GEP checks; generated per-field equality checks |
| Envelope to callee root | Validate identity/order/Frame/layout before handle loads; store selected handles in callee homes/Slots | Runtime validates each callee home in its own live Frame before collection |
| IR return to result | Publication source comes from the validated argument index; transactional copy to captured caller result place | Arguments 0/3 yield different snapshots; later source mutation cannot change result |
| Failure and cleanup | Explicit failure branches; Frame entry dominates effects and cleanup postdominates entered paths | Before/after-poll failures, token expiration, cleanup mutants and zero retained objects |

`CallBinding` is bounded compiler-generated **per-call stack storage**, not a
second runtime registry. It captures the exact envelope, active caller Frame,
ordered snapshot home addresses, result place, layout/quota/failure and epoch.
The compiler writes it before the direct private call. The callee compares the
mutable envelope to that binding and its own immutable compiler identity, then
revalidates before publication. There is no registration service, token lookup
or global call-authority table. Only the existing Runtime Frame list is used.

`Runtime::root_word(home, expected_frame)` checks actual membership in that live
Frame on the current managed thread. It never dereferences an unlinked expected
Frame and does not mint ownership. `vd_fixed_validate_home` additionally uses
Runtime's admitted scalar descriptor to check encoding/extent and complete source
value. `vd_fixed_validate_output` checks layout and registered place without
reading its old referent. Freshness comes from the validated IR and actual copy,
not root membership, matching offsets or one-pointer transport alone.

The private native `Context` is still a trusted embedding input/output contract;
its source is supplied by the harness. Arbitrary native callers, adversarial
native writes to the binding, or retained pointers to dead calls are not admitted
as production authority. Source code cannot access the private binding. The
fault-injection hooks deliberately violate that construction in controlled tests.

## Supplemental IR gate and machine certification

`envelope_protocol.hpp` checks original LLVM snapshot/Slot/envelope/binding and
callee-load relationships, including the correct ordinal and Slot storage field.
It records required helper calls and the private handoff, and checks their
survival and SSA operands after optimization. Dominance/postdominance checks
reject entered paths that bypass Frame cleanup or reach collection before entry.

The emitted `.protocol.json` is a **supplemental IR check report**, not an
independent final-byte ownership certificate. LLVM tracking handles follow SSA
replacement; this does not prove optimized store lineage. The report explicitly
sets `optimized_store_lineage_certified` and `final_byte_ownership_certified` to
false. Fifty-two required operations survive in each tested module.

The unchanged immutable storage inventory, machine observer, target/profile and
final encoded-code gates still run before delivery. Actual object SHA-256 and
certified-delivery status are checked before linking. No rejected object is run.
The original ten-pointer fixture still rejects four incoming slots per profile
and preserves both sentinels. Raw Clang target-profile rejection remains intact;
only that historical C++ fixture is adapted before freeze. Compiler-generated
LLVM requires no target-attribute repair.

| Generated function (arguments 0 and 3) | Profile | P | F | R | E | A | C | Incoming / outgoing | Delivery |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| callee | normal | 352 | 408 | 352 | 8 | 16 | 416 | 0 / 0 | accepted |
| callee | O2 | 352 | 424 | 352 | 8 | 16 | 432 | 0 / 0 | accepted |
| caller | normal and O2 | 592 | 664 | 592 | 8 | 16 | 672 | 0 / 0 | accepted |

No tested boundary has a tail call. Envelope and CallBinding are each 120 bytes
on this host; no payload-sized stack storage is introduced. F and C are existing
per-function research measurements, not whole-executable stack bounds or a
production limit. All new helper calls fit the observed register calling shape.

## Focused tests and reproduction

```sh
cmake --build build/ci-debug --target void_arrays_storage_audit -j 2
python3 tests/research/call_envelope/run.py \
  --build build/ci-debug --output build/stage3b-ownership/review
```

Choose a fresh output directory when reproducing. The runner preserves original
and optimized LLVM, type/profile/body manifests, protocol reports, machine
certificates, source hashes, commands, execution logs and fresh baseline evidence.

- Existing Clang witness: 64 case executions, retaining its fourteen negatives,
  real continuation and large-payload/small-stack case.
- Generated checked witness: 19 cases per body/profile, also at a real 1 MiB
  stack limit: **152 executions**. Covers independent snapshots, source effects,
  GC, source/result registration, exact field order, copied identity with identical
  physical layout, changed signature, wrong live Frame, replayed epoch, failure
  before/after collection, incorrect publication, quota failure, preexisting
  failure, same-sized i64-for-u64 substitution, wrong extent, and empty result.
- The same four exact certified objects are linked with `no_witness.cpp`: all
  five hooks are no-ops, with no TLS registry, identity lookup or root checker.
  Eight cases per body/profile plus small-stack repeats: **64 executions**.
  Typed/layout failures and successful empty outputs work without test authority.
- Twelve protocol mutants per body/profile: **48 rejected candidates**, with
  intact sentinels. Before freeze: remove either entry/leave, redirect a snapshot,
  remove publication or change an envelope field. After the actual selected
  pipeline: remove caller cleanup, callee entry, a snapshot or the handoff, or
  redirect an entered exit around cleanup. No rejected mutant is executed.
- Unsupported-body and missing-ownership admission controls remain (eight checks
  across the four modules). Existing storage-auditor/encoded-CFG negatives pass
  unchanged. Five affected Runtime/GC suites pass; see engineering log for timing.

Strict sanitizer reproduction, using the updated sanitized Runtime:

```sh
cmake --build build/ci-sanitizers --target void_runtime -j 2
python3 tests/research/call_envelope/sanitizers.py \
  --evidence build/stage3b-ownership/review \
  --build build/ci-sanitizers --output build/stage3b-ownership/sanitizers
```

This retains strict ASan/UBSan and leak detection without suppressions. On this
host LeakSanitizer needs process-inspection permission outside the sandbox.
Instrumentation covers C++ Runtime/helpers/witnesses, not the certified LLVM
boundary objects; the sanitizer result must not be described as complete native
boundary instrumentation. Exact results are recorded in the engineering log.

## Remaining obligations and review stop

The generated protocol now establishes the closed call-site relationships;
independent test witnesses observe or corrupt them but do not authorize them.
Machine storage certification and runtime behavior are distinct evidence.

Optimized store lineage, IR-event-to-final-call-target ownership correspondence,
linked helper semantics/relocation identity and the complete final executable's
ownership/failure protocol remain unproved. The supplemental gate deliberately
does not grant final-byte ownership credit. No-op hooks prove independence from
the active registry, not behavior after a future LLVM compilation that removes
all instrumentation calls or introduces LTO. The Context's native provenance,
exhaustive allocator-failure sweeps on the new sequence, arbitrary caller/control
flow, nested/managed arrays, ORC, other targets and general source-array lowering
remain outside this checkpoint.

The fixed eight-argument u64 shape, named fixture functions and source-effect
workload are still deliberate research restrictions. A separately approved
integration must consume real caller IR rather than silently generalize this
wrapper. Existing production ABI/gates, value/readonly/initialization decisions
remain binding. Protected documents were not edited. No branch, commit, push,
remote write, hosted CI or six-configuration matrix.

**Stop for owner and Wrench review before production integration or Stage 3C.**
