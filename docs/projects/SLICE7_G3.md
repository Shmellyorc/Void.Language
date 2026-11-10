# Slice 7 Gate 3 native resource ownership and lifetime

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. Gate 3 follows
the approved [architecture](SLICE7_ARCHITECTURE_REVIEW.md), [G1](SLICE7_G1.md)
and [G2](SLICE7_G2.md). Hosted CI remains on strict hold.

## Ownership audit and contract freeze

The existing Runtime ledger registers a precise managed wrapper identity,
rejects publication once closing starts, and drains dependent operations
before release. Existing native resources are opaque and nonconstructible.
Their GC handles retain identity but do not invoke foreign finalizers. The
G3 extension uses this ledger, the Runtime's existing native-job admission
and drain, and the checked JIT generation lease. No change to Runtime
shutdown ordering is needed.
[Ledger](../../src/runtime/native_resource.cpp) ·
[Shutdown](../../src/runtime/coordination.cpp) ·
[Generation lease](../../src/runtime/reload.cpp).

An admitted acquisition returns an ordinary nullable opaque managed
wrapper with no source-visible payload. Exactly one ledger entry owns
the C pointer and release authority. VOID aliases refer to that single
owner and share its terminal close state. This revision has no move,
copy-owner, conversion from integer, ownership-transfer, or source
borrow-value operation. Only the private call bridge materializes a
borrow from the live ledger entry. An alias may be kept in a local or
application global. Keeping an alias does not extend a native borrow
or grant another release authority.

Source operations require the acquiring OS thread. Explicit close is
one-shot. A closing Runtime stops admission, waits for native jobs to
finish, quiesces its ledger and releases live resources. An initial
provider must have a release operation that is safe on the shutdown
thread. Native thread-affine shutdown release requires a later review.
The C release is terminal even when it returns a nonzero provider
status. Such a failure reports the error with ownership already
consumed. A provider contract that can fail while retaining ownership
is outside G3.

G3 adds the `3 3` descriptor/transport revision with the existing exact
provider, target, descriptor, fixture and calling-convention checks.
Resource declarations precede binding records. The initial form is:

```text
resource <VOID type> <C opaque name> pointer null <acquire binding> <release binding> owner-calls shutdown-any consume-always
bind <acquire binding> <C export> owner:<type> [<scalar> ...]
bind <borrow binding> <C export> <scalar-or-void> borrow:<type> [<scalar> ...]
bind <mutable binding> <C export> <scalar-or-void> mut:<type> [<scalar> ...]
bind <release binding> <C export> i32 close:<type>
```

The scalar argument words keep their prior meanings. The pointer word
for an acquisition or borrow is private to the linked thunk and bridge.
The bridge stores the exact plan and original generation lease until
release completes. A compatible body reload keeps the same Runtime
and can use an existing owner. A different descriptor or linked thunk
requires restart. Null acquisition is an ordinary absence. A failed
borrow can return an admitted scalar error without transferring
ownership. The bridge verifies actual Runtime, provider, resource
identity and release ordinal on each operation.

## Descriptor, transport and static admission

Revision 3 admits up to 16 opaque identities before records and bindings.
It verifies the exact acquire and close binding for each identity.
Resource operations take at most one resource, in the first operand
position. They cannot combine a resource with a record or span in the
same binding. Scalar, record and span bindings from the earlier gates
retain their existing behavior. Descriptor-only or ordinary source
analysis grants no access to the native transport.
[Descriptor template](../../tests/ffi/resources.descriptor.in) ·
[Admission parser](../../src/ir/c_primitive.cpp).

The generated class uses the same nonconstructible nominal wrapper
mechanism as the private native providers. Its private scalar seal is
storage for the managed identity and carries no C address. The address
exists in a transport word and the Runtime ledger owner only. The
checked LLVM wrapper carries the exact provider-qualified export,
ordinal, role, source identity and terminal release ordinal. JIT binds
the transport and Runtime helper on the generation tracker. Linked AOT
uses the same wrapper and rechecks the descriptor before starting
the application.
[Frontend](../../src/frontend/compiler.cpp) ·
[Independent IR](../../src/ir/module.cpp) ·
[LLVM](../../src/backend/llvm/backend.cpp).

The validator rejects missing admission, forged allocation or complete
construction of the wrapper, integer conversion, forged ownership
movement flags, release ordinal and calling convention, a foreign
provider or direct-import scope, raw helper import, escaped binding
function values, uninitialized owner storage, worker execution and
eager-global calls. The operation is valid only in the exact generated
wrapper, with its complete control-flow shape. Borrow permission never
exists as an independently storable IR value. Immutable borrows accept a
readonly reference. Mutation and close require write authority and
publish their effects through the existing reference-capability pass.

Liveness of an alias remains dynamic. The direct typed-IR test constructs
an acquisition, close and borrow of the same alias. The IR admits those
well-typed operations and execution rejects the stale alias at the
ledger. There is no legal move operation to use after. Source and
malformed-IR paths cannot forge a second owner.
[Malformed IR and lifecycle harness](../../tests/ffi/resources.cpp).

## Runtime transitions and release

A native job protects each operation from shutdown admission through
the last Managed transition. Acquisition prepares a precisely rooted
wrapper, calls C, then publishes the owner. Null acquisition clears
the wrapper slot. A publication race with shutdown releases the
unpublished acquisition before the native job ends. The Runtime checks
duplicate C pointer ownership across its live ledger entries. When a
provider returns a duplicate, the candidate drops its claim without
releasing the existing owner's pointer. Providers must transfer fresh
ownership, including across Runtimes.

Borrow and close validate the registered root, active Runtime ledger,
provider descriptor bytes, resource type, linked thunk and release
ordinal. Source calls require the acquiring OS thread. A per-owner
mutex protects the private pointer during the NativeSafe C operation.
A wrong-thread explicit close fails while the borrow remains active.
Shutdown stops admission and drains native jobs before releasing ledger
owners. The independent C fixture confirms this thread-neutral shutdown
release and checks allocation/release counts.

The release thunk reports a provider error only after consuming the
pointer. The bridge reports it through the explicit result or the
existing shutdown error channel. Aliases then fail, and shutdown does
not release that resource again. A transport refusal occurs before
the verified terminal operation and leaves the pointer owned. An
undispatchable release at final destruction is a trusted-provider
contract violation and fails closed. Ordinary C status and runtime
diagnostics use the existing checked failure path.

The owner holds its acquisition generation lease until C release
finishes. The compatibility check includes the exact descriptor and
the admitted linked thunk address. A compatible body reload preserves
the same Runtime and resource. Changed linked authority requires
restart even when its descriptor bytes match. AOT retains its
statically linked thunk for the executable lifetime.
[Resource bridge](../../src/runtime/c_resource.cpp) ·
[Exclusive ledger admission](../../src/runtime/native_resource.cpp) ·
[Reload compatibility](../../include/void/execution.hpp) ·
[Independent C fixture](../../tests/ffi/resources_fixture.c) ·
[Target C thunk](../../tests/ffi/resources_thunk.c).

## Windows Release concurrency follow-up

The original G2 Windows Release failure remains archived. The
retained, hash-verified G2 and G1 Release executables first passed
16 unchanged repeats of `analysis`, `reload` and `numeric` each under
`ctest -j 2`. Repeating with the corresponding Debug profile active
at the same time reproduced the original assertion in G2 Release.
The same overlap also reproduced the assertion in both pre-G2 G1
profiles.

A diagnostic-only G2 Release binary reused the retained G2 objects and
libraries. Its sole test edit split the original compound assertion
after the caller threads joined. The reproduced failure reported
`optimize=0 calls=0 wrong=0 old_checked=0`. In that run the caller
threads executed no calls before the replacement loop set the stop
flag. The assertion short-circuited before testing the original
lease. That result demonstrates a test scheduling gap present before
G2. The original unsplit failure does not identify which condition
fired. The replays supplied no observed wrong result or generation
lifetime defect. The existing test and its assertions stay intact.
The G3 Windows matrix uses `ctest -j 2` and records any retry of the
unchanged selection.
[Original G2 Release transcript](evidence/slice7-g2/original/windows-original-Release-ctest.txt) ·
[Parallel comparisons](evidence/slice7-g3/windows/parallel-results.json) ·
[G2 overlap](evidence/slice7-g3/windows/parallel-overlap-results.json) ·
[G1 overlap](evidence/slice7-g3/windows/parallel-g1-overlap-results.json) ·
[Diagnostic transcript](evidence/slice7-g3/windows/diagnostic-release-overlap.txt) ·
[Diagnostic build receipt](evidence/slice7-g3/windows/diagnostic-results.json).

## Qualification

| Native configuration | Selected CTest cases | Evidence |
| --- | ---: | --- |
| Linux x86-64 Release | 48/48 | [CTest](evidence/slice7-g3/linux/release-ctest.txt) · [Executable and ABI receipts](evidence/slice7-g3/linux/release-artifacts.json) |
| Linux x86-64 Debug | 48/48 | [CTest](evidence/slice7-g3/linux/debug-ctest.txt) · [Executable and ABI receipts](evidence/slice7-g3/linux/debug-artifacts.json) |
| Linux x86-64 fatal ASan/UBSan | 48/48 | [CTest](evidence/slice7-g3/linux/strict-ctest.txt) · [Executable and ABI receipts](evidence/slice7-g3/linux/strict-artifacts.json) |
| Windows 10 x64 Release, build 19045 | 47/47 on retry | [CTest](evidence/slice7-g3/windows/Release-ctest.txt) · [Direct probes](evidence/slice7-g3/windows/Release-g3-direct.txt) · [Run receipt](evidence/slice7-g3/windows/Release-result.json) |
| Windows 10 x64 Debug, build 19045 | 47/47 | [CTest](evidence/slice7-g3/windows/Debug-ctest.txt) · [Direct probes](evidence/slice7-g3/windows/Debug-g3-direct.txt) · [Run receipt](evidence/slice7-g3/windows/Debug-result.json) |

The strict build used fatal ASan and UBSan with leak detection enabled.
The pinned LLVM toolchain lacks RTTI, which limits vptr
instrumentation. The direct probes exercised the instrumented compiler,
Runtime and native fixture alongside the normal verified LLVM
emission pipeline.

Both Windows builds used the pinned clang-cl toolchain and the same
3,288-file input packet. The runner verified all 21 G3 source/test
hashes and restored the trusted Windows source root after the runs.
The one-case platform difference is the Linux-only manifest authority
test. Each Windows profile also ran the C fixture and independent AOT
probes outside CTest.
[Windows native and source-packet receipt](evidence/slice7-g3/windows/native-results.json) ·
[Windows evidence hashes](evidence/slice7-g3/windows/preserved-evidence.json).

The separately compiled C fixture and target thunk exercised acquisition,
inspection, mutation and release. Normal and O2 JIT and the independently
linked normal and O2 AOT executables returned `104` for input `7`.
The AOT probes rejected stale aliases, double close and null unwrap.
The live-owner probe returned `22` and its C exit audit required zero
remaining allocations. The direct lifecycle probes also covered
duplicate native pointers, wrong Runtime/provider/thread, an active
borrow racing shutdown, a lost publication race and a terminal C
release error. Compatible JIT reload retained the acquisition
generation through release, and a changed thunk required restart.

The direct malformed-IR probes reached the independent validator and
the stale-alias execution probe reached the Runtime guard. Separate
source probes reported VOID1103 for forged construction and VOID1106
for startup acquisition. The matrix retained the earlier primitive,
record, buffer, private native, provider, reload and GC tests.

### Original failures and corrections

The first Release configure was triggered before the G3 test source
had been written. The completed harness corrected the missing input.
The first direct IR trial attempted a forged allocation and reached
the existing complete-construction guard. The next trial supplies a
complete forged wrapper to reach the native-resource guard itself.
The same run found that the host fixture attempted Runtime type
admission in Managed state. The corrected fixture admits metadata
while NativeSafe. Both original AOT paths executed the resource
program and shutdown case successfully. A corrected focused run
passed all three G3 tests.
[Original Linux configure](evidence/slice7-g3/original/linux-release-first-configure.txt) ·
[Original Linux build](evidence/slice7-g3/original/linux-release-first-build.txt) ·
[Original direct IR trial](evidence/slice7-g3/original/linux-release-first-ctest.txt) ·
[NativeSafe admission failure](evidence/slice7-g3/original/linux-release-second-ctest.txt) ·
[First corrected focused run](evidence/slice7-g3/linux/release-corrective-focused.txt) ·
[Final focused run](evidence/slice7-g3/linux/release-focused.txt).

The first G1 comparator launch failed when a local script named
`inspect.py` shadowed Python's standard library module. Renaming the
script to `capture-commands.py` restored the unchanged comparator.
[Original launcher failure](evidence/slice7-g3/original/windows-comparator-python-shadow.txt) ·
[Completed G1 overlap comparison](evidence/slice7-g3/windows/parallel-g1-overlap-results.json).

The first G3 Windows Release matrix ran alongside Debug and failed
only the same `analysis` compound assertion. Debug passed its first
matrix. The Release runner preserved its failed transcript and JUnit
file and reran the exact full command with `-j 2`. It passed all
47 tests. The build, selection and assertions were identical. CTest
scheduled `analysis` first in the retry, while Debug was still active.
[Original G3 Release matrix](evidence/slice7-g3/windows/Release-attempt-1-ctest.txt) ·
[Unchanged parallel rerun](evidence/slice7-g3/windows/Release-ctest.txt).

## Source and Git inventory

[Exact source patch](evidence/slice7-g3/source.patch) ·
[Before and after hashes](evidence/slice7-g3/source-hashes.json) ·
[Layered Git inventory](evidence/slice7-g3/inventory.txt).

The patch covers 21 source and test paths relative to the accepted G2
working tree. It amends nine starting inventory paths, modifies two
previously clean tracked paths and adds ten source/test paths. The
other 1,310 starting paths retain their prior bytes. The staged index
still matches all 68,996 HEAD entries.

## Remaining scope

G3 supports one ledger owner per acquired C resource, checked aliases
and synchronous calls. Borrow retention, ownership transfer, generic
C function pointers, native thread-affine shutdown release, dynamic
library loading, dependency resolution, callbacks and raylib remain
later work. General C string binding remains outside the G3 descriptor.
The existing private native tests retain their UTF-8, embedded NUL
and retained-copy coverage. Windows sanitizers, Windows Server 2022,
macOS and other architectures remain unqualified.

Clang reports a misleading-indentation warning at the independent
release-result assignment in the compact validator switch. The
assignment runs after the guard.
[Strict build transcript](evidence/slice7-g3/linux/strict-build.txt).

## End-of-task temporary workspace cleanup

After qualification and evidence preservation, the Linux cleanup
removed 83 entries from 11 verified G3 scratch roots. The Windows
cleanup removed eight verified G3 scratch roots and 103 contained
entries, including the temporary source-overlay backup, the
diagnostic-only replay tree, and the disposable source probes.
The exact deleted paths are recorded in the receipts.
[Linux cleanup](evidence/slice7-g3/host-cleanup.json) ·
[Windows cleanup](evidence/slice7-g3/windows/cleanup-results.json).

The Windows 10 system drive had 26,198,446,080 bytes free (24.40 GiB)
before cleanup and 26,252,644,352 bytes free (24.45 GiB) afterward.
Qualified Linux and Windows binaries, reusable build trees, the
Windows source packet and extracted test harness were retained and
verified. Personal files, installed toolchains, dependency caches
and the Git index were untouched by cleanup.

Stop for Golden Wrench's independent Gate 3 review.
