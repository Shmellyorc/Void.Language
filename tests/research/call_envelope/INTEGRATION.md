> Historical research report. Order 2 replaced the envelope/protocol code and
> its certificate drivers with direct LLVM calls and behavioral tests. See
> [README.md](README.md) for the current implementation and commands. Statements
> below describe the earlier snapshot only.

> Historical integration checkpoint. Current generated authority: [README.md](README.md).

# Stage 3B private call-envelope integration — owner/Wrench review stop

Baseline: `7abc6249a5e1828fd49232a868dcc300b13b7894`. Linux x64, LLVM/Clang
19.1.7. Research-only integration; no production calling-convention replacement,
public JIT/AOT activation or Stage 3C. The [original pilot record](PILOT.md) and
its rejected O2 evidence under `build/stage3b-envelope/review` remain available.

## Outcome and provenance

Both the revised Clang boundary and the first **VOID lower()-generated private
boundary** certify in normal and O2 with the unchanged storage auditor. Neither
has incoming stack-argument slots, outgoing stack arguments or tail calls. The
existing ten-pointer regression still rejects four unproved incoming slots in
each profile, preserving both output sentinels.

The original caller ended with `pilot_caller_check(envelope); return;` in a void
function. LLVM O2 could replace that final call with a tail transfer. The revised
caller branches on checked success, reads the actual result and adds one for its
consumer. Its witness asserts 100 on success and 0 on failure. That observable
continuation prevents the tail transfer without dummy operations, tail attributes
or disabling optimization. No tail/call or encoded-CFG gate changed.

These are three distinct input provenances:

| Input | Producer | Meaning |
| --- | --- | --- |
| Historical ten-pointer fixture | Existing authored LLVM fixture | Unchanged rejection regression |
| `boundary.ll` / adapted input | Clang 19.1.7 from `boundary.cpp` | Revised C++ witness boundary, not VOID lowering |
| `generated-*.json.ll` | Existing VOID `lower()` research branch | Compiler-emitted private envelope/caller/callee from validated owned IR |

The research driver obtains canonical `[u64, 2]` types from frontend analysis,
then explicitly builds a private owned-IR function with eight identical array
parameters and the body `return argument 0`. It is **not** native compilation of
that source function. `ir::validate` runs before generation. The explicit
`emit_private_envelope_research` API derives the logical identity and exact target
profile using the existing `fixed_call_layout`, then invokes `lower()` with a
private fixture request. No source, CLI, JIT or public AOT entrypoint passes that
request. The ordinary array-type rejection remains intact.

`private_envelope_fixture.inc` is a narrowly admitted fixture within that lowerer,
not a general implementation of array instructions. Other bodies, signatures,
imports and missing ownership authority reject. Driver negatives test a valid
but unsupported return-argument-1 body and an absent ownership contract, retaining
sentinels. General instruction/control-flow lowering and selected-place commits
remain outside this path.

## Generated storage and protocol

The shared research-only POD header declares a 112-byte envelope, a 48-byte host
context and a 24-byte identity on this host. Generated DataLayout checks compare
size, alignment and every field offset to independently compiled C++ layouts.
The identity contains the compiler-derived logical signature, target/profile
identity and fixed extent. The envelope contains this identity, an epoch, eight
snapshot-home addresses, caller result home, admitted layout, borrowed quota and
failure context. No array payload is placed on the stack.

The generated caller allocates eight one-word homes, eight real Slots and a
Runtime Frame, initializes homes to null and registers them before preparation.
A fixture-only context check validates source/result registration and rejects
aliasing before source effects. Each `vd_fixed_copy` completes before a later
source write; snapshots contain 7 through 14 while source reaches 15. The C++
closed-call authority record checks actual Runtime registration, independent
payload identity, original context, signature/profile and exact ordered homes.
It is not an offset-based ownership assumption or general incoming acceptance.

The generated envelope lives in caller storage through the call. Before loading
its handles, the callee invokes the fixture's no-poll validation hook. It then
creates its own generated Slots/Frame for eight handle homes before its first
possible safepoint. Runtime assertions check each root before collection.
`vd_fixed_copy` returns argument 0 into the authorized caller result home; failure
branches skip publication/continuation. The caller checks failure, reads 7, and
returns the processed value 8. Both Frames leave on their supported exits. Hooks
use saved registration tokens while storage is still live to prove expiration.
The final collection must find zero retained managed objects.

Every original allocation is inventoried at construction with the existing
immutable inventory. The same emitter freezes it, optimizes it, observes machine
storage and checks encoded code before delivery. The runner requires certified
delivery and an exact object SHA-256 match **before linking**. It executes only
those exact accepted boundary objects; rejected candidates are never executed.

## Focused local evidence

Reproduce using an existing Makefiles LLVM 19.1.7 build; output must be fresh:

```sh
cmake --build build/ci-debug --target void_arrays_storage_audit -j 2
python3 tests/research/call_envelope/run.py \
  --build build/ci-debug --output build/stage3b-envelope/integration-final
```

The final directory records commands, source hashes, raw/adapted Clang input,
compiler-generated original/optimized IR, identity manifests, full certificates,
execution logs and a fresh baseline auditor run. The raw Clang target-profile
rejection is retained: its x86-64/features attributes do not match the emitter's
generic/empty profile. Only the portable Clang fixture is explicitly adapted
before inventory/freeze. Compiler-generated input needs **no** attribute repair.

| Boundary / profile | P | F | R | E | A | C | Incoming | Outgoing | Delivery |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Ten-pointer / normal and O2 | 8 | 40 | 8 | 8 | 16 | 48 | 4 | 32 | rejected |
| Clang callee / normal | 80 | 104 | 80 | 8 | 16 | 112 | 0 | 0 | accepted |
| Clang caller / normal | 16 | 24 | 16 | 8 | 16 | 32 | 0 | 0 | accepted |
| Clang callee / O2 | 80 | 104 | 72 | 8 | 16 | 128 | 0 | 0 | accepted |
| Clang caller / O2 | 16 | 24 | 0 | 8 | 16 | 48 | 0 | 0 | accepted |
| VOID callee / normal and O2 | 352 | 392 | 352 | 8 | 16 | 400 | 0 | 0 | accepted |
| VOID caller / normal | 464 | 552 | 464 | 8 | 16 | 560 | 0 | 0 | accepted |
| VOID caller / O2 | 464 | 536 | 464 | 8 | 16 | 544 | 0 | 0 | accepted |

F is the existing report's frame size; C is its unchanged **research** charge.
These are per-function observations, not complete stack bounds for linked hooks
or performance comparisons to the simpler forwarding fixture. No 64 KiB
production policy is introduced.

Focused runs:

- Clang boundary: 16 cases per profile, repeated at an actual 1 MiB stack limit:
  **64 case executions**. Retains all fourteen pilot negatives, including stale
  identity/epoch, missing registration, swapped homes, invalid publication,
  first-failure preservation, copy failure and omitted-cleanup detection/repair
  before homes die. Includes the 1 MiB payload case.
- VOID-generated boundary: 12 cases per profile, also repeated with a 1 MiB stack:
  **48 case executions**. Checks registered/unregistered source and result homes,
  aliasing, missing snapshot registration, stale/changed identity, swapped homes,
  result redirection, post-poll failure, preexisting failure, quota rejection,
  independent snapshots/result, and callee/caller root expiration. Each negative
  checks its expected message and original result pointer.
- Existing storage-auditor fixture/negative suite passes unchanged. Five rebuilt
  affected suites (`gc_buffers`, `arrays_foundation`, `runtime_generated`,
  `runtime_failures`, `runtime_invariants`) pass, 5/5 in 11.27 seconds locally.
- Strict ASan/UBSan/leaks: 32 Clang-boundary plus 24 generated-boundary cases,
  **56 total**, using `sanitizers.py` and existing sanitized Runtime. Only exact
  certified boundary objects are linked. C++ hooks and Runtime are instrumented;
  those LLVM boundary objects are not ASan/UBSan-instrumented. This is not full
  generated-code sanitizer coverage. Logs: `build/stage3b-envelope/integration-san`.
  Sandbox LeakSanitizer process inspection failed; the unrestricted rerun uses
  strict leak detection without suppressions or weaker flags.

Sanitizer reproduction (requires process inspection permission on this host):

```sh
python3 tests/research/call_envelope/sanitizers.py \
  --evidence build/stage3b-envelope/integration-final \
  --build build/ci-sanitizers --output build/stage3b-envelope/integration-san
```

## Proof boundary and review stop

The transport and encoded machine-storage gates agree for these four delivered
modules. The incoming packed-store blocker and the realistic caller's O2 tail
problem are resolved for this closed Linux x64 fixture.

Semantic authority still comes from the exact admitted owned IR, generated
protocol and closed C++ test registry/assertions. There is no independent
final-byte certificate for the entire root/ownership/failure protocol, linked
hook implementations or complete linked executable. No arbitrary incoming
pointer is granted ownership. Missing-cleanup fault injection remains in the
Clang pilot; generated cleanup is checked by token expiration, not a separate
emitted missing-cleanup mutant. New-path allocation failures are not exhaustively
swept; the existing fixed helper failure sweeps remain enabled.

General source-array execution, arbitrary owned IR bodies, nested/managed element
carriers, generic constraints, selected-place commits, concurrent/recursive
registry authority, ORC and non-Linux targets remain unimplemented or unverified.
The registry is test support, not a proposed compiler-wide ownership system.
Production/private ABI selection and public JIT/AOT gates are unchanged.

`LANGUAGE_BIBLE.md` and `WRENCH_CLANKER.md` were not edited by this sprint. No
branches, commits, pushes, remote writes, hosted CI or six-configuration matrix.
Stop for owner and Wrench review before production integration or Stage 3C.
