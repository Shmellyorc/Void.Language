# Slice 6 final typed-IR storage audit

Golden Wrench requested a focused review on baseline
`2f667ea338127b8f91c3136804377ef3fa5218d6`. This report supplements the
[S6 implementation and qualification](SLICE6.md). All earlier evidence stays
in place.

## Findings and correction

The original Release audit constructed descriptors, locals, instructions and
terminators directly and called `vd::ir::validate`. It found nine cases
admitted incorrectly. The extra unwind fixture demonstrated a tenth.
[Original execution](evidence/slice6-final-review/linux-original.txt) and
[unwind execution](evidence/slice6-final-review/linux-original-unwind.txt).

| Storage or operation | Controlling proof |
| --- | --- |
| Direct enum local and inline aggregate field | The original enum slot pass already required a whole typed write before a load or receiver use. Each aggregate construction takes a typed operand for every field. |
| Fixed enum array, array-valued field, array of aggregates, nested arrays and aggregates | The original containment walk stopped at the array handle. Fixed arrays whose elements have no reference leaves also lack a required reference path in the reference initialization pass. The corrected walk follows aggregate field edges and fixed-array element edges with a visited descriptor set. A local load or projected receiver needs an initialized handle or value. |
| Alternate CFG predecessors | The slot proof intersects incoming writes, so both branch orders reject a missing write. `array_capture` initializes a typed slot like the corresponding backend store. Ordinary array captures used by iteration remain valid. |
| Allocated but partly populated fixed buffer | An allocation or a load from its private capture slot can only feed initialization or the checked completion operation. Ordinary use, returns, aggregate or array embedding, managed publication and ordinary stores reject the pending operand. Completed arrays use the existing fixed-array ownership rules. |
| Managed object containing these shapes | Managed handles keep their reference initialization proof. Nominal `gc_allocate` remains prohibited. Legacy zero-backed allocations now reject enum-bearing fixed-array fields through the corrected containment walk. `gc_construct` requires all typed fields. A staged initializer proves field writes before reads or exit and a commit copies those exact checked fields. |
| `option<enum>` | The option slot needs a whole initialized value. Explicit `None` is an admitted tag with zero arguments. Its inactive bytes can be zero even if zero is undeclared. Ordinary extract/insert cannot reach payload storage. Checked sum projection tests the active tag before an enum value can be observed. |
| Runtime failure into cleanup | A checkpoint can enter cleanup before any body write. A second traversal checks those reads from the unwind entry state. The compiler's terminal return-slot load stays subject to the normal-path proof and the backend places it after the existing cleanup success check. |

See the [containment and slot proof](../../src/ir/numeric_enum_storage.inc),
[independent operand checks](../../src/ir/module.cpp),
[managed construction proof](../../src/ir/construction.inc),
[reference proof](../../src/ir/reference_capabilities.inc) and
[checked sum lowering](../../src/backend/llvm/backend.cpp).

The private fixed-ownership pass returns immediately for ordinary functions.
It admits only scalar fixed arrays in its own ABI, which excludes numeric enum
elements and aggregate carriers. The added checks cover the ordinary typed
value path while leaving that separate proof in place.
[Fixed-ownership proof](../../src/ir/fixed_ownership.inc).

The incremental default recipe still evaluates each element initializer in
order. It now produces an ordinary enum-bearing array only after
`array_finish` checks the Runtime buffer's completion, exact layout and
live prefix against its capacity. The completion returns the existing
handle. The check allocates no replacement buffer and preserves existing
array copies, GC rooting and selected writes. It reuses the fixed-buffer
`Runtime::array_count` contract. The runtime checks a dynamic prefix, while
typed IR independently prevents bypassing that boundary. A deliberately early
completion is a valid checked operation that fails before publication or
element access. [Frontend recipe](../../src/frontend/compiler.cpp),
[Runtime bridge](../../src/runtime/fixed_abi.cpp),
[live-prefix check](../../src/runtime/array.cpp).

## Qualification and preserved failures

The final direct audit exercises 24 malformed cases and 12 valid controls.
It includes both predecessor orders, a missing managed initializer write,
legacy allocation, array capture, attempted escape before completion and
an initialized managed value with nested arrays. Direct normal/O2 JIT probes
reject early completion and accept the equivalent fully initialized buffer.
The source process suite executes nested effectful array defaults, managed
publication, ordinary array iteration, explicit `None` and a deferred
enum-valued return in both JIT and independently linked AOT modes.

| Configuration | Focused results and direct IR transcript |
| --- | --- |
| Linux x86-64 Release | [20/20 CTest](evidence/slice6-final-review/linux/release-ctest.txt), [direct IR](evidence/slice6-final-review/linux/release-direct-ir.txt) |
| Linux x86-64 Debug | [20/20 CTest](evidence/slice6-final-review/linux/debug-ctest.txt), [direct IR](evidence/slice6-final-review/linux/debug-direct-ir.txt) |
| Linux x86-64 fatal ASan/UBSan | [20/20 CTest](evidence/slice6-final-review/linux/strict-ctest.txt), [direct IR](evidence/slice6-final-review/linux/strict-direct-ir.txt) |
| Windows 10 x64 build 19045 Release | [26/26 native CTest](evidence/slice6-final-review/windows/Release-ctest.txt), [direct IR](evidence/slice6-final-review/windows/Release-direct-ir.txt) |
| Windows 10 x64 build 19045 Debug | [26/26 native CTest](evidence/slice6-final-review/windows/Debug-ctest.txt), [direct IR](evidence/slice6-final-review/windows/Debug-direct-ir.txt) |

The strict build instruments the compiler and Runtime, with fatal ASan and
UBSan and leak detection enabled. All five profiles use LLVM 23.1.4.
[Linux build configuration](evidence/slice6-final-review/linux/build-configurations.txt),
[Linux replay](evidence/slice6-final-review/run-linux.sh).

The Windows run used separate native build trees and verified all 3,259 packet
inputs against this working tree. Its runner verified restoration of the
provider-attested source. The direct replays matched the qualified executable
hashes. [Windows qualification receipt](evidence/slice6-final-review/windows/native-results.json),
[direct replay receipt](evidence/slice6-final-review/windows/direct-results.json).

Windows sanitizer and editor-host runs, Windows Server 2022, macOS and
other architectures remain outside this audit's matrix. The existing
Windows persistent native cache limitation remains. The S6 editor evidence
is retained, and the focused native LSP regression was rerun.

The audit also exercised the failing cleanup path with an enum-valued return.
A deferred handler prints its marker and the invocation retains the original
division-by-zero diagnostic in normal and O2 JIT and separately linked AOT on
all three Linux profiles. [Source fixture](evidence/slice6-final-review/cleanup-failure.void).

The original IR failures and fixtures are preserved. A first source test used
the wrong defer spelling. The corrected fixture uses the existing call form.
The direct Runtime probe initially lacked its required owner and then
expected the inner exception spelling instead of the fixed-value ABI's
diagnostic. The passing probe supplies a Runtime and compares the failed
prefix against a completed buffer with the same layout. Release also
reused a backend object compiled during the diagnostic correction. The
corrective run forces that object to rebuild. The first Windows process
was interrupted during packet extraction, and the guest process check
verified that it had created no overlay backup or native build process
before the minimized relaunch. Exact transcripts and binary hashes are
preserved with this report.

## Layered inventory

The [exact review patch](evidence/slice6-final-review-source.patch) covers
11 source and test paths relative to the qualified S6 working tree.
[Before and after hashes](evidence/slice6-final-review-source-hashes.json)
and the [complete layered inventory](evidence/slice6-final-review-inventory.txt)
identify the amended source, new regression and retained earlier evidence.
The inventory verifies the earlier evidence byte-for-byte and checks the
index tuples against both the starting index and HEAD.

## End-of-task temporary workspace cleanup

The [host cleanup receipt](evidence/slice6-final-review/host-cleanup-results.json)
lists 7 disposable screenshots and the duplicate source fixture removed
from the review workspace. The [Windows cleanup receipt](evidence/slice6-final-review/windows-cleanup-results.json)
lists the restored-overlay backup and both empty test temp directories removed.
Windows `C:` had 3,867,406,336 bytes free before cleanup and 3,868,983,296 bytes
after cleanup (3.602 to 3.603 GiB).

The receipt verifies 14 current and 14 prior S6 executable hashes. Qualified
binaries, reusable builds, the extracted test harness, the verified source
packet, the Linux checkpoint and all review evidence remain available.
Unrelated files, shared caches, toolchains, VM settings and Git state were
preserved.

HEAD remains `2f667ea338127b8f91c3136804377ef3fa5218d6` and the staged index
is empty. Hosted CI remains on hold. Work stops at Golden Wrench's final
S6 acceptance boundary.
