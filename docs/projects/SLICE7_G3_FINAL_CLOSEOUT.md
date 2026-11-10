# Slice 7 Gate 3 final resource teardown audit

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. This audit
adds one ownership repair and focused controls to the retained
[Gate 3 implementation and qualification](SLICE7_G3.md).

## Confirmed defect and repair

`Resource::acquire` previously passed its owning field `native_` as
the transport output. If a rejected transport left a nonzero word,
unwinding destroyed a candidate whose field appeared to own that
pointer. The direct fixture reproduced this by letting the verified C
thunk reject the acquisition's argument count, then leaving an existing
owner's pointer in the rejected output. The original Linux Release
process aborted with a double free.

The bridge now receives the output into a zero-initialized local and
assigns `native_` only when dispatch succeeds. The same test source
and CTest selection then passed with just that change. The production
descriptor, independent C fixture and target C thunk retain their
original bytes.
[Original CTest transcript](evidence/slice7-g3-final/original/release-ctest.txt) ·
[Exact original probe](evidence/slice7-g3-final/original/probe.cpp.txt) ·
[Corrective CTest replay](evidence/slice7-g3-final/original/corrected-ctest.txt) ·
[Replay source and executable hashes](evidence/slice7-g3-final/original/corrected-sha256.txt).

## Acquisition and release ownership

| Boundary | Owner and observed outcome |
| --- | --- |
| Successful nonnull C acquisition | The bridge candidate owns the pointer after accepted transport. Only successful publication installs the candidate as the Runtime ledger owner. |
| Publication rejection after shutdown starts | The in-flight job still holds the candidate. The fixture paused after successful C allocation, waited for shutdown to close admission, then resumed. Publication failed, the candidate destructor released the pointer once before the job drained, and the caller's registered output was cleared. |
| A live owner already holds that pointer | Publication finds the exclusive claim in the same Runtime and returns false. The candidate clears its claim without calling C. The first owner remains live, readable and releasable exactly once. |
| Accepted null result | The temporary wrapper never becomes a ledger owner. The bridge clears its output and the fixture reports zero allocations and releases. |
| Transport refusal on acquisition | No ownership transfers. Both an empty output and a rejected output containing a live owner's address leave the ledger and C release counter unchanged. |
| Transport refusal on close | The verified C thunk refuses before calling the release function. The resource restores its exclusive claim, retains its pointer, wrapper and generation lease, and leaves the caller's status word untouched. A borrow still works and the next accepted close releases once. A second acquisition of that same pointer remains rejected after refusal. |
| Terminal C release reports a provider error | The accepted transport has already consumed the pointer. The bridge clears its owned pointer and drops the root and lease as on successful release. Explicit close exposes status 17 in the fixture. Shutdown reports status 17 and a second shutdown causes no additional release. |

The provider must transfer fresh ownership on successful acquisition,
including across Runtimes. The duplicate check is a Runtime-local
defense while an exclusive claim is published. It does not establish
process-wide pointer uniqueness. Publication checks for a duplicate
before testing `closing_` so a candidate for a still-claimed pointer
cannot enter releasing rollback when shutdown has begun. The close
path withdraws that claim at the terminal handoff so an allocator can reuse an
address immediately, and reinstates it if transport refuses before
handoff. Refused transport must leave the C operation unexecuted.
A terminal provider status during unpublished rollback leaves the
failed-publication result in place because the destructor has already
consumed the pointer. A persistent transport refusal during final
destruction violates the trusted contract and terminates. G3 requires
a statically linked, process-lifetime thunk, no
native-to-VOID reentry or retained borrows, and a terminal release
that is safe on the shutdown thread.
[Resource bridge](../../src/runtime/c_resource.cpp) ·
[Linked transport contract](../../include/void/c_primitive.hpp) ·
[Publication and ledger teardown](../../src/runtime/native_resource.cpp) ·
[C transport](../../tests/ffi/resources_thunk.c) ·
[Direct controls](../../tests/ffi/resources.cpp).

## Locks, roots and generation retirement

1. The checked bridge validates a registered root and takes a
   `NativeJob` before entering C. Publication takes the Runtime
   mutex before the ledger mutex, and reads only atomic exclusive
   claims. Lookup releases the ledger lock before any resource lock.
   It returns a strong owner reference.
2. Borrow and terminal release hold the same per-resource mutex
   through the thunk. Explicit close requires the acquiring thread.
   The race control suspends the borrow before the C read, rejects a
   foreign-thread close, then observes shutdown waiting with the C
   allocation still live. The read returns the original value before
   shutdown releases it.
3. Shutdown sets `closing_` under the Runtime mutex, signals stop,
   drains the native jobs with its condition-variable lock released
   while waiting, and drains native workers. The ledger serializes
   shutdown, swaps its entries out under the ledger lock, then
   quiesces and releases them without that lock. Runtime global roots
   remain valid until native teardown finishes.
4. The acquisition output frame and the persistent `Runtime::Handle`
   root the wrapper before the C operation enters NativeSafe. GC
   enumerates these roots under the Runtime mutex and never takes
   the resource mutex or calls the native finalizer. The direct
   control dropped the frame reference and collected, then verified
   the ledger-held wrapper survived. After close removed the
   persistent root, clearing the alias and collecting reclaimed it.
5. Explicit close zeroes the native pointer under the resource mutex
   after accepted release, then clears the persistent wrapper root
   and erases the expected ledger entry. A retained alias has
   no second close authority. The source bridge is owner-thread-only,
   shutdown waits for its job, and the underlying `close` also checks
   the zero pointer under the same lock. The source and raw-IR stale
   alias controls exercise the rejected path.
6. `NativeCode::invoke` installs the generation lease in
   `LeaseScope`, and `Resource` copies that lease during construction.
   Close leaves `generation_` in the owner throughout
   `dispatch(release_, …)`. Only after the thunk returns does it move
   the lease to a local. It then resets that local in NativeSafe,
   outside the resource and GC locks. The ORC generation destructor
   removes its tracker and JIT dylib only when the final shared
   reference is gone. The stored thunk address points to linked
   host code, independent of the ORC symbol registration. AOT uses
   the linked thunk for the executable lifetime.
7. Compatible replacement shares the Runtime, descriptor and exact
   linked thunk address. The old invocation owner can be dropped
   while the live resource still holds its generation. For each of
   normal and O2 JIT, the direct probe dropped the old code owner and
   observed its generation via a weak reference immediately before
   and after the real C release. It observed exactly one release and
   expiration after the bridge returned. It ran both explicit close
   and Runtime shutdown after replacement, and rejected a replacement
   with a different thunk.

[Runtime shutdown, thread state, handles and GC](../../src/runtime/coordination.cpp) ·
[Invocation and reload leases](../../src/runtime/reload.cpp) ·
[ORC tracker and replacement](../../src/backend/llvm/backend.cpp).

## Focused qualification

| Configuration | Selected CTest cases | Receipt |
| --- | ---: | --- |
| Linux x86-64 Release | 14/14 | [CTest](evidence/slice7-g3-final/linux/release-ctest.txt) · [Direct output](evidence/slice7-g3-final/linux/release-LastTest.txt) |
| Linux x86-64 Debug | 14/14 | [CTest](evidence/slice7-g3-final/linux/debug-ctest.txt) · [Direct output](evidence/slice7-g3-final/linux/debug-LastTest.txt) |
| Linux x86-64 fatal ASan/UBSan | 14/14 | [CTest](evidence/slice7-g3-final/linux/strict-ctest.txt) · [Direct output](evidence/slice7-g3-final/linux/strict-LastTest.txt) |
| Windows 10 x64 Release, build 19045 | 14/14 | [CTest](evidence/slice7-g3-final/windows/Release-ctest.txt) · [Direct output](evidence/slice7-g3-final/windows/Release-g3-direct.txt) |
| Windows 10 x64 Debug, build 19045 | 14/14 | [CTest](evidence/slice7-g3-final/windows/Debug-ctest.txt) · [Direct output](evidence/slice7-g3-final/windows/Debug-g3-direct.txt) |

The focused selection covered G1–G3, reload, GC and analysis using
`ctest -j 2`, including normal/O2 JIT and independent normal/O2 AOT
executions. ASan leak detection was enabled and both sanitizers were
fatal. The existing pinned LLVM RTTI limitation still applies.
Windows sanitizers, Windows Server 2022, macOS and other architectures
remain outside this qualification.
[Linux reproduction script](evidence/slice7-g3-final/qualify-linux.sh) ·
[Strict instrumentation receipt](evidence/slice7-g3-final/linux/strict-instrumentation.txt).

Both Windows 10 profiles passed on their first focused parallel run.
The runner used pinned Clang 23.1.4 and verified all 3,288 input hashes.
Only the two audit source paths differ from the qualified G3 packet.
The runner restored the original Windows source tree.
[Native receipt](evidence/slice7-g3-final/windows/native-results.json) ·
[Evidence hashes](evidence/slice7-g3-final/windows/preserved-evidence.json).

The retained Windows overlap comparison reproduced the composite
assertion in G1 and G2. The diagnostic G2 reproduction observed
`calls=0` before the stop flag, so the assertion did not reach the
lease check in that run. The original unsplit failures do not identify
which branch failed. G3's original failing overlap and passing
unchanged parallel retry are retained. This audit leaves the original
concurrency test and its assertions intact.
[G1 overlap](evidence/slice7-g3/windows/parallel-g1-overlap-results.json) ·
[G2 overlap](evidence/slice7-g3/windows/parallel-overlap-results.json) ·
[Diagnostic replay](evidence/slice7-g3/windows/diagnostic-release-overlap.txt) ·
[G3 overlap](evidence/slice7-g3/windows/Release-attempt-1-ctest.txt) ·
[G3 parallel rerun](evidence/slice7-g3/windows/Release-ctest.txt).

## Source and layered Git inventory

The exact source delta is two paths. The inventory verifies the
previous G3 hashes for all unaffected paths, the old and new hashes
of both amended paths and the unchanged HEAD and index.
[Source patch](evidence/slice7-g3-final/source.patch) ·
[Before and after hashes](evidence/slice7-g3-final/source-hashes.json) ·
[Layered Git inventory](evidence/slice7-g3-final/inventory.txt).

## End-of-task temporary workspace cleanup

After preserving the original failure, corrective replay and final
qualification receipts, Linux cleanup removed 17 verified scratch
paths. Windows cleanup removed seven verified scratch roots and 83
contained entries, including the restored source overlay's backup,
empty temp directories and disposable probe files. Both receipts
list the exact deleted paths.
[Linux cleanup receipt](evidence/slice7-g3-final/host-cleanup.json) ·
[Windows cleanup receipt](evidence/slice7-g3-final/windows/cleanup-results.json).

Windows `C:` had 24,733,876,224 bytes free (23.035 GiB) before
cleanup and 24,736,034,816 bytes (23.037 GiB) afterward. Cleanup
verified and retained qualified binaries, reusable build trees, the
source packet and extracted native harness. Unrelated data, installed
toolchains, reusable caches and the Git index were untouched.

Stop for Golden Wrench's final G3 acceptance.
