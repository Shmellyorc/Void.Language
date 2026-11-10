# Slice 7 Gate 4 verified dynamic loading checkpoint

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`.
**Stopped at the loader architecture checkpoint.** Windows 10 resolved
a provider's PE import to a different DLL and ran that DLL's initializer
before returning from the restricted load. Both approved files and their
directory were still held. The provider itself matched the verified file.
This leaves a gap in pre-initializer closure enforcement for the current
Windows launch contract. Production implementation and qualification
remain pending a reviewed launch and dependency policy.

## Existing authority and lifetime audit

- The existing terminfo wrapper searches library names and looks up symbols.
  Its search and fallback rules are outside G4 authority. Windows console
  and host-directory code query already loaded OS modules. The Windows
  host helpers provide Unicode path validation, reparse and hardlink
  checks, file information and ACL checks. They do not bind a PE import
  to a held file.
- `CPrimitiveAdmission::admit` parses a bounded descriptor and calls the
  linked target thunk's probe. Its transport contract assumes that the
  trusted host owns the linked code for the process lifetime. The IR
  validator separately checks the generated wrapper, ordinal, nominal
  provider authority and exact ABI shape. A dynamic plan needs distinct
  offline admission so source analysis and object emission can run
  without executing a provider probe or initializer.
- ORC binds the private transport and bridge on each generation tracker.
  `NativeCode::LeaseScope` pins the executing generation. A G3 resource
  holds its acquisition generation through terminal release, and the
  Runtime drains native jobs before ledger teardown. The accepted
  acquisition fix commits ownership only after transport success.
- AOT currently calls the linked descriptor probe before Runtime startup
  and eager globals. Dynamic AOT will need to complete the same
  closure admission as JIT before reaching that point. A loaded module
  lease must also cover AOT teardown and the G3 release dispatch.

[Accepted architecture](SLICE7_ARCHITECTURE_REVIEW.md) ·
[Terminfo](../../src/runtime/console_terminfo.inc) ·
[Windows filesystem helpers](../../src/runtime/host_windows.hpp) ·
[C admission](../../src/ir/c_primitive.cpp) ·
[Independent IR](../../src/ir/module.cpp) ·
[LLVM and AOT bootstrap](../../src/backend/llvm/backend.cpp) ·
[Runtime coordination](../../src/runtime/coordination.cpp) ·
[G3 teardown proof](SLICE7_G3_FINAL_CLOSEOUT.md).

## Linux x86-64 GNU checkpoint

Pinned Clang 23.1.4 built an independent ELF provider and dependency
with a real `DT_NEEDED` edge and matching dependency SONAME. Neither
fixture imports the CRT. On glibc 2.44 the probe copied both objects to
separate `memfd` files, sealed each against writes, truncation and growth,
and opened the dependency first in a fresh `dlmopen` namespace. It
then loaded the provider into that namespace via its sealed descriptor.
It had already mapped a competing SONAME in the base namespace and
replaced the dependency's original pathname. The isolated namespace
contained exactly two objects and the call returned `42`.

The executable regions for the provider export and the dependency
actually reached by the provider matched the device and inode of the
corresponding sealed descriptor. The probe also demonstrated that
`dlsym(provider, dependency_only_export)` returned a symbol owned by the
dependency. Every required export needs an owner check even when looked
up on the exact provider handle.

The candidate Linux policy would independently validate the bytes that
go into each sealed image and parse the complete bounded ELF graph
before mapping. It needs exact machine and ABI, unique admitted
SONAMEs, controlled dependency order and verification of the final
namespace and imports. It must reject path-bearing dependency names,
RPATH/RUNPATH, unsupported forwarding, IFUNC and resolver relocations,
unexpected initializer hooks and other behavior that can execute
before the verification point. A controlled launch must account for
loader environment and audit hooks. The probe proves the sealed-image
and isolated-namespace mechanism for a freestanding fixture. It leaves
system ABI and sanitizer dependency exceptions unqualified.
[Linux source](evidence/slice7-g4/checkpoint/linux-mapping.c) ·
[Reproduction script](evidence/slice7-g4/checkpoint/linux-probe.sh) ·
[ELF, digests and mapping transcript](evidence/slice7-g4/checkpoint/linux-run.txt) ·
[Linux loader](https://man7.org/linux/man-pages/man3/dlopen.3.html) ·
[File seals](https://man7.org/linux/man-pages/man2/memfd_create.2.html) ·
[Mapping identity fields](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html).

## Windows 10 x64 checkpoint

Pinned Clang 23.1.4 built independent PE DLLs. The main DLL imports
`g4_dependency` by name from `void_g4_probe_dep.dll`. The probe holds
the parent directory and opens each file with read sharing only. It
records the held volume and file index, compares the mapped NT device
path, and queries the allocation type. The side-by-side controls also
compared each loaded image to a data mapping created from the held
file via the native `NtAreMappedFilesTheSame` export. That additional
oracle was measured on build 19045 and has no approved support
contract for the production loader.

| Native control | Observed result |
| --- | --- |
| Fresh process, matching closure | Main and dependency mapped as images with the expected paths. Call returned `42`. |
| Preloaded competing dependency basename | Absolute `LoadLibraryExW` with the DLL load-directory and System32 search flags still used the competing dependency. The approved main mapped correctly and the call returned `134`. |
| POSIX-style file replacement during the held handles | NTFS refused the attempted replacement with Windows error 32. The correct dependency still mapped and returned `42`. |
| Tested top-level DotLocal and thread activation redirects | The absolute top-level provider remained at its held path in those controls. The DotLocal override registry value was absent in the guest. |
| Thread activation context redirects the dependency, no preloaded name collision | The approved provider mapped, but the import reached the alternate DLL. The alternate initializer marker was already `1`. The kernel comparison returned zero for the provider and `c00000d4` for the dependency. |
| Push default over the thread override in a clean-default executable | The import reached the held dependency, both kernel comparisons returned zero and the alternate initializer marker was absent. |
| Push default in an executable with the redirect embedded in its default manifest | The unapproved dependency mapped and initialized again. The clean-default expectation returned status 1. |

`LoadLibraryExW` accepts a pathname and requires the reserved file
handle argument to be null. Restricted flags constrain filesystem
search directories. They did not constrain the already-loaded module
list or the active side-by-side import redirection in these probes.
The clean-default result applies only to that tested launch. A
preflight basename scan plus a post-load image check can detect the
observed mismatches, but the latter occurs after initializer
execution. Detecting the mismatch or calling `FreeLibrary` at that
point cannot undo initializer effects. The stopped design does not
grant loading authority on the strength of these checks.

[PE imports and exports](evidence/slice7-g4/checkpoint/windows-build.txt) ·
[Clean map](evidence/slice7-g4/checkpoint/windows-clean.txt) ·
[Loaded-name collision](evidence/slice7-g4/checkpoint/windows-collision.txt) ·
[Pinned replacement denial](evidence/slice7-g4/checkpoint/windows-replace.txt) ·
[DotLocal control](evidence/slice7-g4/checkpoint/redirection-bare.txt) ·
[Top-level activation control](evidence/slice7-g4/checkpoint/activation-override.txt) ·
[Dependency redirection and initializer](evidence/slice7-g4/checkpoint/edge-override.txt) ·
[Clean-default control](evidence/slice7-g4/checkpoint/edge-default.txt) ·
[Redirected process default](evidence/slice7-g4/checkpoint/process-default-run.txt) ·
[Native launch scripts](evidence/slice7-g4/checkpoint/windows-probe.cmd) ·
[LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw) ·
[DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order) ·
[Activation contexts](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-activateactctx).

## Decision for Golden Wrench

The common lifecycle under review is offline host admission, whole-closure
validation, verified mapping and binding, immutable publication, then
Runtime startup. A plan would carry a version, provider and target,
descriptor and thunk identities, bounded artifact hashes and dependency
edges, required owned exports, trusted roots and an explicit OS
dependency policy. The new mode must bind closure identity into the
independent IR authority and reload compatibility. Each Runtime and
generation should carry its admitted immutable binding and image lease.
Its resources must retain that release authority until G3 teardown
finishes. Binding failures must stop before eager globals. This is a
candidate lifecycle, with no production transport, plan or schema
change at this checkpoint.

- **Controlled Windows launch profile.** Review an attested JIT and AOT
  launch contract with a protected executable and bundle root, a
  qualified activation-context and redirection policy, and control of
  the module namespace across preflight and load. Qualify the handling
  of preexisting modules and resolution of every PE import. Explicitly
  scope API sets, KnownDLLs and supported system libraries. Require a
  backed-image oracle and fail closed outside the qualified profile.
- **Separate native process.** Review a fresh process with a controlled
  default manifest and isolated authority if existing hosts cannot
  control the loader state. This changes how resource identity,
  borrowing and release cross the boundary.
- **Split platform approval.** A separately approved Linux-first
  implementation can build on the sealed-image probe while Windows
  remains deferred. This changes the platform acceptance gate.

Exact host prerequisites and any OS library allowlist need review
before adopting a Windows profile. Application-bundled imports with
unproved resolution remain outside the stopped profile.

## Evidence, inventory and qualification status

The executed tests are standalone OS-loader checkpoint probes on Linux
and Windows 10 Home x64 build 19045. They used independent dynamic
images and a real imported dependency. The G4 VOID execution,
Release/Debug/fatal sanitizer matrices, installed relocation, and
G1–G3 regression reruns remain pending after the architecture stop.
The previous G3 receipts retain their original scope. No production
source changed. The probe patch and inventory identify every added
source and evidence path.

The evidence collector's initial input list included the Linux-only
source and failed after copying the Windows logs. The exact Windows
input list then verified the existing copies and wrote their hashes.
[Collector failure](evidence/slice7-g4/checkpoint/collector-initial-error.txt) ·
[Collector replay](evidence/slice7-g4/checkpoint/collector-rerun.txt) ·
[Native evidence hashes](evidence/slice7-g4/checkpoint/preserved-evidence.json) ·
[Exact probe source patch](evidence/slice7-g4/checkpoint.patch) ·
[Layered Git inventory](evidence/slice7-g4/inventory.txt).

## End-of-task temporary workspace cleanup

After preserving the transcripts and verifying retained native binaries,
the Windows cleanup removed the duplicate replacement DLL and its empty
probe directory. The equivalent DLL and all executable probes remain in
the native test harness. Windows `C:` had 24,752,857,088 bytes free
before cleanup and 24,752,861,184 bytes afterward (23.053 GiB). The
Linux checkpoint build and source transfer inputs remain available for
reproduction. There were no disposable host-only files to remove.
Prior native binaries, source packets, build trees and toolchains
remain in place. The receipt records the exact deleted paths.
[Windows cleanup receipt](evidence/slice7-g4/checkpoint/cleanup-results.json).

Stop for Golden Wrench's independent G4 architecture review.
