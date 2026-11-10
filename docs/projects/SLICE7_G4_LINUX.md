# Slice 7 Gate 4-L verified Linux dynamic loading

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`.
This slice implements the Linux x86-64 GNU **system none** profile.
Windows remains at the separate architecture hold. The prior
[architecture checkpoint](SLICE7_G4.md) and
[G3 final teardown evidence](SLICE7_G3_FINAL_CLOSEOUT.md) are retained.

## Closure and authority

A trusted embedding host calls `CPrimitiveAdmission::admit_dynamic`
with the canonical closure and an absolute host-selected root. The
existing analysis API receives that admission object. Its construction
parses the closure and the existing revision 3 C descriptor without
opening the provider files or executing a thunk. Source, manifests
and editor analysis have no route to create that authority. An AOT
host authorizes the same immutable bytes by embedding them in the
compiler-emitted executable. The loader resolves that executable's
sibling `native` directory through `/proc/self/exe`.

```text
void-native-closure 1
provider <logical provider>
target x86_64-unknown-linux-gnu sysv-x64
system none
layout executable-sibling-native
thunk <target thunk SHA-256 identity> vd_c_primitive_dispatch_v3
image <dependency SONAME> <SHA-256>
image <main SONAME> <SHA-256> <dependency SONAME>
export <dependency SONAME> <owned function>
export <main SONAME> vd_c_primitive_dispatch_v3
export <main SONAME> <owned descriptor function>
descriptor
<exact canonical revision 3 descriptor bytes>
```

The ASCII/LF plan is bounded at 32 KiB, including a descriptor of
at most 16 KiB. Up to eight images appear in dependency-first order,
with the main image last. Every image must be reachable from the
main image. Image names are canonical `lib*.so` components and also
the expected SONAMEs. Each of at most 128 required exports has one
named owner. Provider, target and thunk identity agree with the
embedded descriptor. Every descriptor binding must name a required
export owned by the main image. The main image SHA-256 binds the
actual target-compiled thunk bytes and the thunk probe must accept
the exact descriptor after mapping.

The plan carries portable identities. The Runtime claims the loaded
binding once, and its generation receives a strong reference. A
binding cannot be adopted by another Runtime. Reload compares the
exact plan bytes, host root and existing Runtime identity.
[Closure parser](../../src/dynamic_closure.cpp) ·
[Offline C admission](../../src/ir/c_primitive.cpp) ·
[Reload compatibility](../../include/void/execution.hpp).

## Pre-initializer ELF and mapping proof

1. Admission requires NativeSafe or unattached host state. It checks
   the loader environment at startup and at admission, the trusted
   procfs process view, normal execution mode, page size and absence
   of a system preload configuration. The host executable, its base
   namespace and platform loader form part of the trusted launch.
2. The loader walks the allowed root with held directory descriptors,
   opens regular single-link files without following symlinks, reads
   each into a bounded snapshot and checks its SHA-256. It validates
   all files and all edges before calling `dlmopen`. The ELF reader
   bounds the program headers, 16 MiB image extent, dynamic and
   string tables, 512-symbol SysV hash geometry, and relocation
   tables. It checks the ELF64 little-endian x86-64 ABI, file-backed
   tables, load permissions, SONAMEs and exact ordered `DT_NEEDED`.
3. Only ordinary global function symbols and bounded
   `R_X86_64_JUMP_SLOT` and `R_X86_64_GLOB_DAT` relocations are
   admitted. Relocations target nonexecutable writable storage
   outside loader metadata. Every undefined symbol must have one
   exact definition from its declared direct dependency. The
   allowlists reject initializer and finalizer tags, IFUNC and
   IRELATIVE resolvers, TLS, versioned or weak symbols, interpreter
   segments, RPATH/RUNPATH, audit and filter tags, executable
   stacks and writable executable segments. No provider code needs
   to run to decide these checks.
4. All authenticated bytes go into separate memfds with write,
   grow, shrink and seal seals applied and verified. Each
   `/proc/self/fd` loader pathname must identify that held device and
   inode. The first dependency opens in a new `dlmopen` namespace,
   and subsequent artifacts load by those sealed descriptor paths
   in dependency order with `RTLD_NOW | RTLD_LOCAL`. The link map
   must contain exactly the admitted prefix after each load. No
   ambient directory or base-namespace handle supplies an import.
5. Every executable segment must match its sealed descriptor's
   device, inode, offset and permissions in `/proc/self/maps`.
   Each relocated slot must contain the exact expected owning
   symbol address. Exact-handle `dlsym`, ELF address calculation
   and the returned `dladdr1` link map must all agree for every
   required export. Only then does the host call the target thunk's
   descriptor probe and publish the immutable Runtime binding.

Failures carry their stage in the owned diagnostic. The stages
distinguish closure and authority, namespace, path and artifact,
digest, ELF and dependency, seal and load, mapping, symbol owner,
and transport attestation.

The production JIT probe independently enumerates the actual
namespace, rehashes the descriptors named by its link maps, and
records their sealed file identities. A same-SONAME library mapped
in the base namespace did not supply the provider's import. Replacing
the approved dependency on disk after admission left the running
binding unchanged. A fresh Runtime rejected those replacement bytes.
The earlier checkpoint separately exercised pathname replacement
between sealing and loading.
[Loader and binding code](../../src/runtime/c_dynamic.cpp) ·
[Native probes](../../tests/ffi/dynamic.cpp) ·
[Sealed-image checkpoint](evidence/slice7-g4/checkpoint/linux-run.txt) ·
[ELF dynamic linking specification](https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.dynamic.html) ·
[glibc namespace interface](https://man7.org/linux/man-pages/man3/dlopen.3.html) ·
[memfd seals](https://man7.org/linux/man-pages/man2/memfd_create.2.html) ·
[Mapping identity fields](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html).

A separately linked provider variant compiled its dependency call
with `-fno-plt`. It supplied a real `GLOB_DAT` relocation for
`g4_bias` and kept the thunk's `JUMP_SLOT` release relocation.
The normal and O2 JIT lifecycle controls ran against that exact
closure as well.
[Relocation replay script](evidence/slice7-g4-linux/qualify-got.sh) ·
[Release replay](evidence/slice7-g4-linux/linux/release-got.txt) ·
[Debug replay](evidence/slice7-g4-linux/linux/debug-got.txt) ·
[Strict replay](evidence/slice7-g4-linux/linux/strict-got.txt).

## Compiler, AOT and lifetime

The descriptor still produces the same compiler-owned binding module.
The independent IR validator retains the exact generated wrapper,
import scope, ordinal, carrier, resource and call-signature checks.
Dynamic dispatch, resolution and AOT startup helpers are reserved
against raw IR imports and arbitrary JIT symbol binding. The execution
tests exercise forged provider, ordinal, erased admission and raw
bridge imports. Both JIT modes go through the verified LLVM lowering
and retain the binding in their ORC generation. Persistent object
caching remains unavailable for C-provider programs.

The shared fixture is independently compiled C with a real
`DT_NEEDED` edge. The target thunk calls independently compiled
primitive functions, a SysV by-value record function, a bounded
byte-span operation and opaque resource operations. The AOT executable
contains compiled VOID and the linked Runtime/dispatch bridge.
It opens the provider at startup from the embedded closure before
attaching or reaching eager globals. It has no G4 shared-object
`DT_NEEDED`, provider transport definition or build-tree RUNPATH.

Each AOT object was emitted from a temporary standalone source root
with an absent provider directory. That source root was deleted
before execution. The process tests copied the executable and its
two-image bundle to a new installation and ran from a different
working directory containing a conflicting dependency. Both normal
and O2 returned value `104`. A separately linked startup-control
program reached an eager division-by-zero with a valid bundle.
Missing, replaced and wrongly relocated bundles reported loader
errors before that initializer. Loader-related environment variables
also caused controlled rejection.

Dynamic scalar and span calls take native jobs and retain the
binding through the call. Resource calls resolve the exact bound
thunk and use the G3 bridge. Its resource retains the acquisition
generation through terminal release. The Runtime owns the same
binding through shutdown, including for independently linked AOT.
The native probes race shutdown against a live call with an owned
resource. They observe the allocation before drain, one terminal
release while the module remains mapped, and unmapping after the
Runtime and generation owners retire. Compatible reload keeps the
original binding. Changed roots and artifact digests are rejected
while the old resource remains usable.

The dynamic acquisition refusal deliberately leaves the first
owner's pointer in the output word. The release count stays unchanged
and that owner remains readable. The unchanged G3 direct test retains
the original double-free reproducer and passes in the same matrices.
[LLVM bridge](../../src/backend/llvm/backend.cpp) ·
[AOT bootstrap](../../src/runtime/abi.cpp) ·
[Process controls](../../tests/ffi/dynamic_process.py) ·
[G3 resource bridge](../../src/runtime/c_resource.cpp) ·
[G3 original double-free evidence](evidence/slice7-g3-final/original/release-ctest.txt).

## Local qualification

| Linux x86-64 GNU configuration | Selected tests | Evidence |
| --- | ---: | --- |
| Release | 52/52 | [CTest](evidence/slice7-g4-linux/linux/release-ctest.txt) · [Direct output](evidence/slice7-g4-linux/linux/release-LastTest.txt) · [Native linkage](evidence/slice7-g4-linux/linux/release-native-linkage.txt) |
| Debug | 52/52 | [CTest](evidence/slice7-g4-linux/linux/debug-ctest.txt) · [Direct output](evidence/slice7-g4-linux/linux/debug-LastTest.txt) · [Native linkage](evidence/slice7-g4-linux/linux/debug-native-linkage.txt) |
| Fatal ASan/UBSan with leak detection | 52/52 | [CTest](evidence/slice7-g4-linux/linux/strict-ctest.txt) · [Direct output](evidence/slice7-g4-linux/linux/strict-LastTest.txt) · [Native linkage](evidence/slice7-g4-linux/linux/strict-native-linkage.txt) |

All configurations used the pinned Clang 23.1.4 toolchain on
glibc 2.44 and ran with CTest parallelism 2.
[Platform receipt](evidence/slice7-g4-linux/linux/platform.txt).

The focused selection includes G1–G4, Runtime source/reload/GC,
native JIT/AOT regressions and accepted project tests. Strict uses
fatal ASan and UBSan with leak detection for the executable,
compiler and Runtime. The freestanding mapped C fixture uses
UBSan traps in the strict configuration. Its opaque handles come
from a bounded native pool. Full ASan inside the
mapped fixture would introduce a sanitizer runtime and load-time
initialization, which this closure profile does not admit.
[Reproduction script](evidence/slice7-g4-linux/qualify-linux.sh) ·
[Strict compile flags](evidence/slice7-g4-linux/linux/strict-compile-flags.json) ·
[Fatal sanitizer environment](evidence/slice7-g4-linux/linux/strict-sanitizers.txt).

The negative controls include a genuine C initializer that would
exit the process with status 121 if executed. Its exact hashed
dependency was rejected during ELF validation. The matrix also
checks bad target and symbol owner, competing symbols, missing and
aliased files, altered hash, undeclared system dependency, unsupported
loader tags, TLS and interpreter segments, IFUNC and IRELATIVE,
malformed closures, forged IR, Runtime binding transfer and wrong
installed location.

### Preserved failures and corrections

The first Release run caught a forged ordinal at the earlier wrapper
range guard. The expectation now checks that diagnostic. The AOT
controls initially found a RUNPATH inherited from the pinned Runtime
link setup. The G4 AOT targets now omit it. The next run revealed
that the existing executable adapter prints its returned value and
exits successfully. The process checks now verify both the exact
stdout and the exit status. That run also showed that the concurrent
test had attempted to enter a program's eager globals from a foreign
thread. The corrected probe invokes on the application owner thread
and has other threads observe the native call and request shutdown.
The corrected four-test Release selection passed.
[Initial execution](evidence/slice7-g4-linux/original/release-first-ctest.txt) ·
[Initial AOT dynamic section](evidence/slice7-g4-linux/original/release-first-aot-dynamic.txt) ·
[Second execution](evidence/slice7-g4-linux/linux/release-replay-ctest.txt) ·
[Corrected focused run](evidence/slice7-g4-linux/linux/release-focus-corrected-ctest.txt).

The first 52-test Release and Debug runs reached the existing
independent-global-program guard in a direct wrong-closure probe.
The corrected probe uses global-free IR against the same Runtime
with a live resource. It reaches the closure binding guard directly.
The independent-global-program rule and parallel test selection were
preserved.
[Initial full Release run](evidence/slice7-g4-linux/original/full-first-release-ctest.txt) ·
[Initial full Debug run](evidence/slice7-g4-linux/original/full-first-debug-ctest.txt).

## Limits and review boundary

The closure admits no system ABI libraries. The host executable
uses its ordinary trusted OS runtime dependencies outside the
provider namespace. General glibc dependencies, libc TLS, sanitizer
runtime imports, constructors, versioned exports, unsupported
relocation kinds and arbitrary third-party closures remain outside
this qualification. The next architectural decision for libraries
that require them is the exact system ABI allowlist and its
pre-initializer dependency and resolver policy. The loader assumes
a trusted host and OS startup, including absence of injected native
code and preexisting audit instrumentation. A check in the executable
cannot undo effects of code run by the operating-system loader
before its entry point. [glibc loader environment](https://man7.org/linux/man-pages/man8/ld.so.8.html).

Windows G4 remains blocked on a separate pre-initializer admission
proof. Its dependency-redirection and initializer evidence retain
the same scope. macOS and other targets are unqualified.
[Windows checkpoint](SLICE7_G4.md#windows-10-x64-checkpoint).

## Source and layered Git inventory

The patch covers 23 source and test paths. Seven amend paths from the
accepted inventory and 16 are new. The 1,637 other previous paths
retain their bytes, including the entire G4 checkpoint. HEAD and the
staged index match the baseline.
[Exact source patch](evidence/slice7-g4-linux/source.patch) ·
[Before and after source hashes](evidence/slice7-g4-linux/source-hashes.json) ·
[Layered Git inventory](evidence/slice7-g4-linux/inventory.txt).

## End-of-task temporary workspace cleanup

After collecting and checking the qualification receipts, Linux
cleanup removed 70 verified scratch paths, including the temporary
G1, G3 and G4-L source probes and the G4-L negative and replacement
copies. It verified and retained 72 built artifacts. The three
reusable Linux build trees, both qualified fixture layouts, the
checkpoint probes and prior source packets remain available.
The host also removed the four staging copies used for the Windows
storage query and their empty transfer directory after preserving
the scripts and receipts.

The Windows 10 guest storage checks were read-only. Its `C:`
drive had 24,749,342,720 bytes free before cleanup and
24,749,277,184 bytes after cleanup, both 23.050 GiB rounded.
No Windows guest files were cleaned for this Linux-only slice.
Unrelated files, installed toolchains, reusable caches and the Git
index were untouched by cleanup.
[Linux cleanup receipt](evidence/slice7-g4-linux/host-cleanup.json) ·
[Storage before](evidence/slice7-g4-linux/storage/storage-before.json) ·
[Storage after](evidence/slice7-g4-linux/storage/storage-after.json) ·
[Transfer cleanup](evidence/slice7-g4-linux/storage/transfer-cleanup.json).

Stop for Golden Wrench's independent G4-L review.
