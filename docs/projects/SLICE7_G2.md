# Slice 7 Gate 2 verified C records and call-scoped byte spans

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. Gate 2 extends
the existing [Gate 1 host admission and checked transport](SLICE7_G1.md).

## Contract freeze

The approved synthetic providers share the G1 parser, host-owned immutable
admission, virtual declaration module, direct-import proof and checked
wrapper instruction. Descriptor/transport revisions are independently
selected as the exact pair `1 1` or `2 2`. The second linked symbol has
the same C machine signature with distinct word meanings. G1 remains
byte compatible.

G2 admits natural, nonempty records with exact case-sensitive C type
and member names and earlier-record-only nesting. Each field describes
one scalar, one earlier record, or one inline fixed scalar C array.
A byte-level natural-layout pass verifies all offsets, size, alignment
and array stride, including inter-field and tail padding. The trusted
C artifact independently asserts those facts and uses ordinary C
prototypes to determine register, stack and hidden return-pointer
classification. A VOID value struct carries the source-level record.
Managed fixed arrays inside the value are explicitly marshalled.
The foreign layout and the VOID layout have separate representations.

A span is a nonnullable `[u8]` argument with separately named
zero-based offset and length operands of type `c_size`. Lengths are
in bytes. The offset belongs to VOID and is never passed to C. Every
span is bounded by its descriptor maximum and the list's live count,
using subtraction before any endpoint addition. At most two spans
and one writable span are admitted per call. Two ranges on the
same list may be disjoint. Overlap is rejected. G2 span bindings
return signed `i64`. A negative C result is a recoverable error and
leaves VOID data unchanged. For a writable span a nonnegative result
is the written prefix length. Input-only results are ordinary
signed values. The trusted C implementation may use its pointer only
during that call. Zero length passes null and a zero count.
Records and spans use separate binding signatures in this revision.

Rooted list handles are checked in Managed state. They stay registered
through NativeSafe. The bridge supplies independent aligned native
scratch and reenters Managed before validating and copying back the
reported prefix. Output-only scratch starts zeroed, and in/out scratch
starts with the input snapshot. C's reported prefix is an admitted
provider contract. The bridge prevents copying uninitialized process
memory and checks the extent. Arbitrary C writes cannot be observed or
contained by an in-process bridge. The transactional commit rechecks
the destination structure and does not publish a failed or oversized
result. No provider can keep a span or acquire a source-level pointer.

## Descriptor and transport

The G1 common header retains provider, target and profile, VFS source,
thunk and fixture SHA-256 identities, effects, calling convention and
C aliases. G2 adds the following records before any `bind` records.
Record declarations follow dependency order.

```text
void-c-primitive 2 2
...
record <VOID name> <C name> <size> <alignment> <field count>
field <VOID name> <C name> <scalar|record:name|array:scalar:extent:stride> <offset>
...
bind <VOID name> <exact C export> <result> [<argument> ...]
```

A result is a scalar, `void` or `record:name`. Arguments may also be
`in:offset-index:length-index:maximum`, `out:...` or `inout:...`.
The pointer occupies that argument's position in the C projection. Its
`c_size` length operand stays in the projection. The offset operand is
consumed by VOID and dropped from the C prototype. Each control index is
distinct and identifies an exact `c_size` operand. Empty ranges require
offset within the live extent. The parser allows up to 16 records, 24
fields per record, 256 bytes per record, 8-byte alignment, 32 inline
elements and a 65,536-byte span ceiling. The fixture declares a 64-byte
limit. G1's existing descriptor and transport remain version 1.
[Canonical fixture template](../../tests/ffi/records.descriptor.in) ·
[Parser](../../src/ir/c_primitive.cpp).

The host supplies `CPrimitiveAdmission::admit` with the versioned trusted
linked transport. `Binding::abi_parameters` and `Binding::abi_result`
describe G2 shapes, with the G1 scalar fields retained for existing hosts. The pinned thunk checks the full canonical descriptor
and the independently compiled fixture's identity. G2 uses
`vd_c_primitive_dispatch_v2` with G1's machine signature. Scalar words
retain the G1 encoding. Record words carry aligned native-scratch addresses
that only the backend can materialize. The thunk reconstructs its C values
with `memcpy` and enters each exact C prototype. The result pointer names
scratch sized and aligned for the verified return. VOID imports fields
individually and ignores C padding. It reconstructs inline arrays through
the existing checked Runtime array ABI with registered result storage.

The lowering keeps native-scratch addresses outside typed VOID IR. The
private Runtime bridge receives registered root words and exact scalar
argument indices. It derives the range length from the same word that
the C thunk sees. It validates control-slot uniqueness, bounds and
same-list overlap, holds native scratch through the call, and checks
the returned prefix after Managed reentry. An output list is committed
under the Runtime lock only after its structural version, live extent and
writability are rechecked. Native errors, oversized outputs and changed
destinations leave the source buffer untouched by the bridge. The C
fixture uses no retained pointer, and the descriptor rejects other
effect profiles.
[Runtime bridge](../../src/runtime/c_span.cpp) ·
[Locked copyback](../../src/runtime/array.cpp) ·
[Checked LLVM lowering](../../src/backend/llvm/backend.cpp).

## Independent verification

The IR verifier requires the host seal, binding module and direct import,
exact provider/export and ordinal, wrapper-only opcode shape, source
record identity, field order and types, and initialized argument
placement. Layout numbers live only in the sealed descriptor. The
parser independently recalculates natural layout before the C identity
probe. A malformed IR operation therefore cannot supply its own sizes
or offsets. The IR checks readonly copyback authority, scope, workers,
async and eager global initialization. The existing definite-storage
and array completion checks now cover admitted inline arrays and
G2 carrier inputs. The bridge separately validates the registered root
and live extent at Runtime.
[IR validation](../../src/ir/module.cpp) ·
[Definite storage](../../src/ir/numeric_enum_storage.inc) ·
[Mutation effects](../../src/ir/reference_capabilities.inc).

The independently compiled C fixture and thunk cover a 4-byte color,
a 16-byte mixed integer/floating record with a padding gap, and a 32-byte
nested packet with an inline three-element array and further padding.
The fixture exercises by-value returns and mixed scalar/record arguments.
The C thunk asserts type signedness, offsets, alignments and inline
stride. The executable comparison uses the actual C calls in both JIT
and AOT.

| C shape | Size and alignment in bytes | Verified offsets |
| --- | --- | --- |
| `G2Color` | 4 and 1 | Channels at 0, 1, 2 and 3 |
| `G2Sample` | 16 and 8 | Signed tag at 0, double at 8 |
| `G2Packet` | 32 and 8 | Color at 0, channels at 4/6/8, mode at 10, nested sample at 16 |

Direct IR cases cover a missing seal, foreign identity,
wrong ordinal or record shape, forged argument placement, readonly
output, raw transport import, missing direct import, worker use,
uninitialized record/list/scalar storage and an unfinished inline
array. The separate Runtime control collects with scratch live in
NativeSafe. It verifies zeroed output scratch, partial copyback,
oversized and negative rollback, capacity, conflicting control
slots and an unregistered root.
[C fixture](../../tests/ffi/records_fixture.c) ·
[C thunk](../../tests/ffi/records_thunk.c) ·
[VOID client](../../tests/ffi/records.void) ·
[Executable controls](../../tests/ffi/records.cpp).

## Qualification

| Configuration | Final result | Evidence |
| --- | --- | --- |
| Linux x64 Release | 45/45 | [CTest](evidence/slice7-g2/linux/release-ctest.txt) · [ABI and process receipt](evidence/slice7-g2/linux/release-artifacts.json) |
| Linux x64 Debug | 45/45 | [CTest](evidence/slice7-g2/linux/debug-ctest.txt) · [ABI and process receipt](evidence/slice7-g2/linux/debug-artifacts.json) |
| Linux x64 fatal ASan/UBSan | 45/45 | [CTest](evidence/slice7-g2/linux/strict-ctest.txt) · [ABI and process receipt](evidence/slice7-g2/linux/strict-artifacts.json) |
| Windows 10 x64 build 19045 Release | 44/44 on serial replay | [CTest](evidence/slice7-g2/windows/Release-ctest.txt) · [Direct JIT and IR](evidence/slice7-g2/windows/Release-g2-direct.txt) · [C ABI](evidence/slice7-g2/windows/Release-g2-c-abi.json) |
| Windows 10 x64 build 19045 Debug | 44/44 | [CTest](evidence/slice7-g2/windows/Debug-ctest.txt) · [Direct JIT and IR](evidence/slice7-g2/windows/Debug-g2-direct.txt) · [C ABI](evidence/slice7-g2/windows/Debug-g2-c-abi.json) |

All five configurations executed normal and O2 JIT and independently
linked normal and O2 AOT. The compiled VOID fixture returned 103 for
input 7, including the by-value record and span copyback comparisons.
Both modes rejected undersized storage, overlap, oversized returned
length and endpoint overflow with the expected runtime diagnostics.
The existing G1 and native-provider cases and selected S1–S6 project,
numeric, Runtime and GC regressions were rerun. Windows omits the
Linux-only manifest authority test.

The independently compiled C declarations and definitions agree.
SysV returns `G2Sample` in integer and floating registers, while the
MSVC ABI uses a hidden return pointer. Both targets use a hidden return
pointer for `G2Packet`. The receipts include exact compiler arguments,
descriptor and object hashes, Clang IR signatures and process output.
The Windows runner verified 3,278 source packet inputs and restored the
provider-attested source root. It measured **30.88 GiB** free on C:
before building.
[Windows qualification and replay receipt](evidence/slice7-g2/windows/native-results.json) ·
[Windows preflight](evidence/slice7-g2/windows/storage.json).

All configurations used pinned Clang/LLVM 23.1.4. The strict build
instrumented the compiler, Runtime and both independent C units.
ASan and UBSan were fatal with leak detection. The LLVM SDK lacks
RTTI, so vptr instrumentation remains unavailable. The emitted VOID
programs use the normal verified LLVM lowering. Supplemental source
probes rejected native dispatch during eager startup and a readonly
list passed as an output span on all three Linux profiles.
[Linux replay](evidence/slice7-g2/qualify-linux.sh) ·
[Startup probe](evidence/slice7-g2/startup.void) ·
[Readonly probe](evidence/slice7-g2/readonly.void).

### Original failures and corrective runs

The first Linux Release suite passed both AOT paths and failed the direct
IR harness's expectation for a span argument-slot swap. The validator
rejected the IR at the more specific argument-placement guard. The
test now requires that exact diagnostic and the corrective focused run
passed all three tests. The final matrix also exercises the strengthened
Runtime request, which takes offset and length from the same transport
words as the C thunk.
[First run](evidence/slice7-g2/original/release-first-ctest.txt) ·
[Corrected focused run](evidence/slice7-g2/linux/release-pre-final-focused.txt).

The first Windows 10 Release suite passed 43 of 44 tests, including
all G2 JIT/AOT tests. The existing analysis test reported
`concurrent call/lease failure` in its compound concurrency assertion.
That diagnostic does not isolate which condition fired. The replay
verified the same source packet and guest overlay hashes, then ran
the unchanged Release selection with serial CTest scheduling. The
test's own concurrent callers and assertions were unchanged.
[Original Windows Release transcript](evidence/slice7-g2/original/windows-original-Release-ctest.txt) ·
[Original native receipt](evidence/slice7-g2/original/windows-original-native-results.json) ·
[Release replay harness](evidence/slice7-g2/windows/replay.py).

## Source and Git inventory

[Exact source patch](evidence/slice7-g2/source.patch) ·
[Before and after hashes](evidence/slice7-g2/source-hashes.json) ·
[Complete layered Git inventory](evidence/slice7-g2/inventory.txt).

The exact patch covers 20 source and test paths relative to the accepted
G1 working tree. It amends ten prior inventory paths, modifies one
previously clean tracked path and adds nine source/test paths. All 1,197
other starting paths retain their exact bytes. The index matches
HEAD's 68,996 entries and the staged diff is empty.

## Remaining scope

This gate admits the two x64 C ABI profiles and natural, pointer-free
records. General source pointer values, unions, bitfields, packed and
flexible records, inline arrays of non-scalars, mixed span/record
bindings, multiple writable spans, pointer retention, resources,
callbacks, native worker admission, dynamic loading, dependency
resolution and raylib remain outside the implemented contract. Windows
sanitizers, Windows Server 2022, macOS and other architectures remain
unqualified.

## End-of-task temporary workspace cleanup

The host removed 64 verified entries from the Gate 2 scratch
workspace and per-profile probe directories. Windows removed the
restored overlay backup, the two empty build temp roots and two
disposable regression probe directories. Windows 10 C: free space
rose from 29,926,432,768 bytes to
**29,928,173,568 bytes (27.87 GiB)**.
The receipts list the exact paths and retained hashes.

The Linux and Windows reusable build trees, current qualified binaries,
prior Windows G1 artifacts, source archive, extracted native harness,
receipts and installed toolchains remain available. Cleanup preserved
unrelated files, shared dependencies, reusable caches and Git state.
[Host receipt](evidence/slice7-g2/host-cleanup.json) ·
[Windows receipt](evidence/slice7-g2/windows/cleanup-results.json).

Stop for Golden Wrench's independent G2 acceptance. Hosted CI remains
on strict hold.
