# Windows C FFI: architecture resolution for review

9 November 2026. Research only. No production changes, native experiments,
policy deployment, builds, Git operations or hosted CI were performed.
The local G4 records identify baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`;
findings below were checked against the present working source, including its
accepted G1–G3 and Linux G4-L layers. Upstream branch links describe source read
for this review, not pinned VOID dependencies or tests run by this investigation.

## Decision

**Recommend an explicitly authorized, OS-managed native execution profile for
ordinary applications. Preserve exact-image admission as a separate profile.**
On Windows, use the normal loader with an absolute provider path, restricted
dependency search, a trusted installed bundle, and VOID's existing checked C
transport and resource leases. Use the same runtime admission path for JIT and
AOT. Ordinary customers need no App Control policy or administrator privilege.

This is a viable conventional architecture, **not an implementation or a proof
of the existing Windows G4 pre-initializer requirement**. Boss must approve the
additional trust profile and its public wording. The old strict profile must
continue to reject unsupported Windows loading; it must never fall back silently.
If exact pre-initializer identity under adversarial loader state remains mandatory
for every application, no general unpackaged solution has been established under
the supported interfaces reviewed here. More variations of pathname loading will
not resolve that conflict.

The practical route is therefore one policy decision followed by integration,
not another attempt to discover an undocumented handle-loading guarantee.
Raylib additionally needs explicitly qualified blocking/affinity behavior; general
C callbacks are a separate capability, not something the loader can supply.

## 1. What is actually blocked

The failure is **authority over image selection**, not a demonstrated inability
to call Windows C functions or to lower the Windows x64 ABI.

| Evidence | What it establishes | What it does not establish |
|---|---|---|
| [G3 final closeout](SLICE7_G3_FINAL_CLOSEOUT.md), Windows Debug/Release 14/14 selections | Existing target-compiled C transport, normal/O2 JIT, independent AOT, resource ownership and teardown passed the recorded Windows 10 controls. | Arbitrary dynamic providers, Windows 11 qualification, callbacks or graphics-thread teardown. |
| [W1 collision](evidence/slice7-g4-windows/native/probe-collision.txt), [thread redirect](evidence/slice7-g4-windows/native/probe-thread.txt), [preloaded approved dependency](evidence/slice7-g4-windows/native/probe-preload-thread.txt) | Top-level provider matched; its IAT selected the alternate dependency; alternate initializer was already observed as `1`. Holding the approved dependency did not reserve its import name. | A generic Windows loader bug or a failed C ABI. |
| [W1 forwarder](evidence/slice7-g4-windows/native/probe-forward.txt) | `GetProcAddress` itself could load and initialize a forwarded-to image before the caller invoked the result. | That checking only the initial PE imports covers later loading. |
| [W1 startup good](evidence/slice7-g4-windows/native/startup-good.txt) and [bad](evidence/slice7-g4-windows/native/startup-bad.txt) | Matching the suspended executable path did not authenticate startup dependencies. First entry could already observe the alternate. | That a fresh process or executable import library authenticates dependency bytes. |
| [W2 pins](evidence/slice7-g4-w2/native/main/case-pin.txt), [clean](evidence/slice7-g4-w2/native/main/case-clean.txt), [separate](evidence/slice7-g4-w2/native/main/case-separate.txt) | Held files/ancestors prevented the tested writes, replacement and rename; clean absolute loading executed the approved fixture. | Protection from every loader redirect or same-user process compromise. |
| [W2 substitution](evidence/slice7-g4-w2/native/main/results.json), [guarded rejection](evidence/slice7-g4-w2/native/main/case-swap-guarded.txt) | After the verified handle was deliberately dropped, replacement initializers exited with calibrated codes; a fresh digest check rejected the changed file. | A bypass of the still-held-file control. Do not describe this as one. |
| [W2 ACL failure](evidence/slice7-g4-w2/native/main/case-acl.txt), [restricted-token follow-up](evidence/slice7-g4-w2/native/acl-followup/case-acl-full.txt) | The write-restricted case obtained DELETE access; the fully restricted case denied the tested mutation rights. | That a normal same-account private directory is a sandbox against that account. |

W1's 14 controls and W2's main 14 successful predicates/one failed ACL hypothesis
remain evidence within their Windows 10 Home 19045/NTFS fixture scope. The W2
leaf-symlink branch was not executed because creation lacked privilege. Controlled
late loads demonstrate stale observations, not a measured concurrent exploit.
Original `_fltused`, object-free forwarder, W2 compile and first ACL failures remain
in the [W1 archive](evidence/slice7-g4-windows/) and
[W2 archive](evidence/slice7-g4-w2/); no successful retry erases them.
The complete [W1 assessment](SLICE7_G4_WINDOWS_ARCHITECTURE.md) and
[W2 assessment](SLICE7_G4_WINDOWS_IMAGE_SELECTION.md) remain valid for their
**strict** requirement.

The public executable loader accepts a pathname; `LoadLibraryExW`'s file-handle
argument is reserved and must be null. Data/image-resource mappings do not become
ordinary executable bindings. `DONT_RESOLVE_DLL_REFERENCES` is explicitly discouraged
and is not a documented validate-then-promote transaction.
[Microsoft load contract](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw).

Dependency names, activation contexts, existing modules, API sets and KnownDLLs
participate separately from directory selection. Full-path loading of the parent
does not turn all dependencies into full-path loads.
[Microsoft search rules](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
Initialization happens during loading; a subsequent hash/IAT/owner check can
prevent publication to VOID, but cannot undo native initialization.
[DLL initialization contract](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).

### Additional current-source boundaries

| Source | Confirmed implementation consequence |
|---|---|
| `src/ir/c_primitive.cpp:106–126` | Only Linux x64 SysV and Windows x64 MSVC descriptor profiles; effects are exactly `direct-sync nonblocking no-callbacks`. Apple arm64 C admission is not already implemented. |
| `src/dynamic_closure.cpp:18–21,58–60`; `src/runtime/c_dynamic.cpp:462–550` | Current closure parser is Linux `system none`, canonical `.so` images; non-Linux verified loading rejects. Adding a Windows suffix alone cannot implement the required architecture. |
| `src/runtime/c_resource.cpp:53–86` | Acquisition transfers only after successful transport; release relinquishes its exclusive claim at terminal handoff, restores it on transport refusal, and retains its generation through release. Preserve the double-free repair. |
| `src/backend/llvm/backend.cpp:2404–2414,2646–2676`; `src/runtime/abi.cpp:164–176` | Existing dynamic binding ownership and AOT bootstrap are reusable integration points. |
| `include/void/execution.hpp:41–46` | Exact native contract participates in replacement compatibility. The new profile must participate too. |
| `src/frontend/project.cpp:264–265` | Nonempty `native` and `dependencies` project tables are still reserved. A project declaration is not already an authorization workflow. |

Linux G4-L is also deliberately narrower than ordinary C interoperability: it
excludes libc/system dependencies, constructors, TLS and other ordinary ELF
features. Preserve its sealed-image proof rather than treating it as proof that
arbitrary raylib builds already work on Linux.
[Accepted Linux scope](SLICE7_G4_LINUX.md#limits-and-review-boundary).

## 2. What established implementations actually do

The negative finding is scoped: **none of the loading paths reviewed below
implements VOID's exact-file-before-initializer promise**. This does not claim no
application written in those languages can add installation checks or OS policy.
These implementations separate ABI/ownership mechanisms from trusting executable
native code and the OS loader.

### Go

`src/syscall/dll_windows.go`: `LoadDLL` converts to UTF-16, selects the designated
system-library path or ordinary `loadlibrary`, and eventually calls
`LoadLibraryExW`; `FindProc` uses `GetProcAddress`, `Release` uses `FreeLibrary`.
The ordinary load has flags zero. `LazyDLL` delays/caches loading, not admission.
The file explicitly warns about relative-name preloading.
[Go syscall source](https://github.com/golang/go/blob/master/src/syscall/dll_windows.go).

`x/sys/windows/dll_windows.go`: `NewLazySystemDLL` marks `System`; lazy load reaches
`loadLibraryEx`, which selects `LOAD_LIBRARY_SEARCH_SYSTEM32` where supported.
This is a system-DLL search restriction, not an arbitrary third-party hash check.
Its compatibility fallback is not needed for VOID's Windows 10/11 target.
[x/sys implementation](https://github.com/golang/sys/blob/master/windows/dll_windows.go#L350).

`Proc.Call` reaches `SyscallN`, then runtime `syscall_syscalln` stores arguments in
thread runtime state and enters the native bridge via `cgocall(asmstdcallAddr,…)`.
That raw argument interface is not a replacement for checked C aggregate lowering.
[Runtime transition](https://github.com/golang/go/blob/master/src/runtime/syscall_windows.go#L405).
cgo instead generates C wrappers, processes headers and linker flags, and supports
external native linking. Go pointer retention requires its pinning rules; C is
not made memory-safe. C allocations and GUI thread affinity remain obligations
of the binding/application.
[cgo source contract](https://github.com/golang/go/blob/master/src/cmd/cgo/doc.go).

Adopt the distinction between designated system loads and trusted application
loads; retain VOID's stronger checked descriptors instead of importing a public
raw `uintptr` call mechanism.

### Odin

`foreign import` supplies link inputs; `foreign` declares native exports and C
calling convention. A Windows `.lib` may contain real static objects or be an
import library for a separately deployed DLL. The extension alone does not mean
the implementation is statically included.
[Odin binding contract](https://odin-lang.org/news/binding-to-c/).

The compiler's `check_foreign_import_fullpaths` resolves import paths; its Windows
linker path consumes `LibraryName.paths` and SDK/library directories. Native ABI
classification is explicit: `lbAbiAmd64Win64` handles Windows x64 arguments/results.
These are compiler/linker mechanisms, not runtime executable-image attestation.
[Checker](https://github.com/odin-lang/Odin/blob/master/src/checker.cpp#L6341),
[linker](https://github.com/odin-lang/Odin/blob/master/src/linker.cpp#L327),
[ABI lowering](https://github.com/odin-lang/Odin/blob/master/src/llvm_abi.cpp#L638).

For explicit dynamic loading, `core/dynlib/lib_windows.odin` converts the path,
calls `LoadLibraryW`, resolves with `GetProcAddress` and unloads with `FreeLibrary`.
It adds no hash/manifest admission in that path. Pointer validity, allocation
pairing and library lifetime remain programmer responsibilities.
[Dynamic loader](https://github.com/odin-lang/Odin/blob/master/core/dynlib/lib_windows.odin).

There is a concrete game-library precedent: Odin's raylib binding selects
`windows/raylibdll.lib` for `RAYLIB_SHARED`, otherwise `windows/raylib.lib`, and
declares Windows system libraries. It does not solve game-library loading by
requiring no imports or no CRT startup.
[Actual raylib binding](https://github.com/odin-lang/Odin/blob/master/vendor/raylib/raylib.odin#L99).

### Rust

`unsafe extern "C"` declarations and `repr(C)` data establish a programmer-supplied
foreign contract, not automatic verification of what an export actually does.
Native `#[link]` supports static and dynamic libraries; Windows `raw-dylib` can
generate an import library. MSVC linker code passes `.lib` inputs to the linker.
[Foreign declarations](https://doc.rust-lang.org/reference/items/external-blocks.html),
[C data representation](https://doc.rust-lang.org/reference/type-layout.html#the-c-representation),
[actual MSVC link path](https://github.com/rust-lang/rust/blob/main/compiler/rustc_codegen_ssa/src/back/linker.rs#L978).

`libloading` is a separate library. Its Windows `new` uses flags zero;
`load_with_flags` calls `LoadLibraryExW(filename,0,flags)`. Loading is unsafe
because initialization itself executes foreign code; unloading has the same
obligation for termination routines. Symbol lookup requires the caller's correct
function type. No reviewed step checks an approved digest before initialization.
[Windows implementation](https://github.com/nagisa/rust_libloading/blob/master/src/os/windows/mod.rs#L139).

The portable wrapper ties `Symbol<'lib,T>` to a borrowed library lifetime.
Dropping/explicitly closing the owner releases the module; copying an extracted
raw function pointer can escape the protection. Foreign objects need their own
RAII wrapper and must not outlive their release code.
[Lifetime wrapper](https://github.com/nagisa/rust_libloading/blob/master/src/safe.rs#L198).
VOID should retain its existing resource/module leases, without claiming they
prevent arbitrary native memory corruption.

### Zig

`std.DynLib` selects `WindowsDynLib`. `open` reaches `openEx` with no flags;
`openExW` calls the Windows `LoadLibraryExW` wrapper. `lookup` uses
`GetProcAddress` and casts to the requested type; `close` calls `FreeLibrary`.
The public entry documents that the file is trusted and can execute arbitrary
code. This is not an exact-image verifier.
[Dynamic implementation](https://github.com/ziglang/zig/blob/master/lib/std/dynamic_library.zig#L566).

C interoperability uses C calling conventions and `extern` layouts; the build
module exposes library linkage. The COFF implementation contains import directory,
lookup and address tables keyed by library names. A statically linked object or
DLL import is selected by the build; runtime DLL resolution remains the OS's job.
[Language reference](https://ziglang.org/documentation/master/#extern-struct),
[build linkage](https://github.com/ziglang/zig/blob/master/lib/std/Build/Module.zig#L363),
[COFF imports](https://github.com/ziglang/zig/blob/master/src/link/Coff.zig#L1921).
Explicit allocation/free and native resource ownership remain with the binding.

### .NET / CoreCLR

P/Invoke lookup in `nativelibrary.cpp` uses the per-assembly resolver, applicable
AssemblyLoadContext hook, caches/probing, then the resolving event. Search reaches
`LocalLoadLibraryHelper` and `CLRLoadLibraryEx` with a null file argument and
selected flags. An AssemblyLoadContext is a managed resolution hook, **not a
separate Windows native-loader namespace**.
[CoreCLR implementation](https://github.com/dotnet/runtime/blob/main/src/coreclr/vm/nativelibrary.cpp#L738),
[documented resolution order](https://learn.microsoft.com/en-us/dotnet/core/dependency-loading/loading-unmanaged).

`NativeLibrary.Load` has path and assembly-aware forms; `SetDllImportResolver`
registers per-assembly resolution. `GetExport` returns an address and `Free`
releases explicit handles. Do not infer that resolver-created handles or arbitrary
objects are safely freed merely because a managed load context disappears.
[Managed API source](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/Runtime/InteropServices/NativeLibrary.cs),
[loading documentation](https://learn.microsoft.com/en-us/dotnet/standard/native-interop/native-library-loading).

Interop signatures, marshalling and `SafeHandle` wrappers handle particular
memory/lifetime obligations; unmanaged code still executes in-process with its
privileges. Search attributes restrict probing; they do not attest an arbitrary
binary's digest. Standard P/Invoke is dynamic; NativeAOT can additionally use
direct/static native linking, which is a different build choice, not a stronger
DLL-selection guarantee.
[Interop guidance](https://learn.microsoft.com/en-us/dotnet/standard/native-interop/best-practices),
[NativeAOT interoperability](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/interop).

### Game-library implementations

SDL's Windows `SDL_LoadObject` uses `LoadLibraryW`, `SDL_LoadFunction` uses
`GetProcAddress`, and unloading uses `FreeLibrary`.
[SDL source](https://github.com/libsdl-org/SDL/blob/main/src/loadso/windows/SDL_sysloadso.c).
GLFW 3.4 loads optional system components by name, through a wrapper around
`LoadLibraryA`; these paths trust the platform and application search environment.
[Module wrapper](https://github.com/glfw/glfw/blob/3.4/src/win32_module.c),
[Windows initialization](https://github.com/glfw/glfw/blob/3.4/src/win32_init.c#L84).
This is evidence of conventional deployment, not a security recommendation to
copy unrestricted name lookup.

Raylib 5.5's desktop GLFW backend creates a context, installs **native C-to-C**
input callbacks, swaps buffers and destroys the window before GLFW termination.
Its core `EndDrawing` can wait for a target frame time and polls input. Those
operations cannot honestly be labelled nonblocking.
[Backend](https://github.com/raysan5/raylib/blob/5.5/src/platforms/rcore_desktop_glfw.c),
[frame completion](https://github.com/raysan5/raylib/blob/5.5/src/rcore.c#L887).
This version is a reviewed example, not a new dependency pin. Qualify the exact
raylib build selected for VOID, including its CRT, optional backends and drivers.

## 3. Mandatory side-by-side comparison

The VOID column distinguishes current code from the recommendation. No row means
foreign C has become memory-safe or trusted merely by being correctly typed.

| Property | Go | Odin | Rust | Zig | .NET | VOID |
|---|---|---|---|---|---|---|
| Dynamic loading | syscall/x/sys Windows wrappers | `core:dynlib` Windows wrapper | `libloading` or linked imports | `std.DynLib` Windows wrapper | P/Invoke / NativeLibrary | Current Windows dynamic G4 rejects; propose restricted OS loader |
| Static / import linkage | cgo + host linker; system syscall imports | `foreign import` static/import `.lib` | `#[link]`, import libs, raw-dylib | C objects/libraries, COFF imports | P/Invoke dynamic; NativeAOT direct/static options | Linked C thunks accepted; ordinary dynamic adapters proposed |
| Search behavior | Ordinary names risky; system-specific restricted path | OS search for dynamic names; deployment supplies import dependencies | Flags selectable; default libloading flags zero | Flags selectable; default no flags | Resolver + probing + attributes + OS | Absolute bundle path, explicit directory flags; no PATH/CWD fallback proposed |
| Pre-init exact binary identity | Not in reviewed path | Not in reviewed path | Not in reviewed path | Not in reviewed path | Not in reviewed path | Existing strict requirement exceeds these; proposed trust profile expressly does not claim it |
| Dependencies | Link graph and Windows | Link graph and Windows | Link graph and Windows | Build graph and Windows | Managed resolver chooses root; OS resolves PE dependencies | Host-approved bundle/OS surface; native loader remains trusted |
| Memory boundary | GC/pinning rules; C unsafe | Native pointers/manual ownership | Unsafe FFI; caller validates ABI and lifetimes | Explicit native types/manual ownership | Marshalling, pinning/SafeHandle; native unsafe | Checked descriptors, copied spans, precise roots; trusted C can still corrupt process |
| Module/resource ownership | DLL release and explicit C frees | Explicit unload/free | Library RAII + symbol lifetime; C-object RAII separately | Explicit close/free | Runtime P/Invoke cache or explicit handle; resource wrappers separately | Runtime ledger, generation/binding leases, terminal release |
| Security assumption | Trusted native code and OS | Trusted native code and OS | Trusted unsafe implementation and OS | Trusted native code and OS | Trusted native code and OS | Must explicitly choose OS-managed trust or strict image profile |
| Deployment | DLLs/CRT and toolchain-specific build needs | Import/static libraries and matching DLLs | Target linker/runtime and DLLs | Target libraries/runtime and DLLs | .NET or NativeAOT plus native assets | Pinned Clang 23.1.4 build, verified installed bundle; no device policy for ordinary profile |
| Games | Practical with bindings and correct thread ownership | Concrete bundled raylib binding | Practical with FFI wrappers | Practical with C bindings | Practical with P/Invoke wrappers | Loader route viable; public admission, blocking and main-thread resource policy still need engineering |

## 4. Separate the trust requirements explicitly

**Proposed `trusted-os-loader` profile (name illustrative):** an explicit developer/
embedding-host decision permits a specified native dependency bundle to execute
with the process's authority, including its initializers and trusted OS/driver
dependencies. VOID checks the binding contract and installed artifacts, constrains
its search requests, and retains release authority. It trusts the OS, launch
environment, approved native components and deployment owner. It is not isolation
from malicious C, injected code or a hostile same-account actor controlling the
application's loader state.

**Existing strict image profile:** authorize particular executable bytes and a
qualified dependency closure before provider execution, with the stated platform
restrictions. Preserve Linux G4-L as it is. Windows has no established general
implementation under its current requirement. Optional managed enforcement would
be another expressly qualified deployment profile, not automatic strict parity.

Both retain G1–G3's supported-type rules, exact descriptor checks, borrow bounds,
registered roots, ownership, release semantics and independent IR validation.
Neither makes buggy/malicious C safe once it is executing in-process. Native
permissions are **admission permissions**, not a sandbox restricting the library's
filesystem, network, subprocess or Win32 access after admission.

Authentication has several different meanings which diagnostics must distinguish:

1. A signed/trusted manifest authenticates the approved artifact inventory.
2. A file digest establishes the bytes inspected at that check.
3. A loader/OS policy governs which executable mappings may run.
4. A descriptor probe and owned-symbol checks govern what VOID subsequently calls.

No implication from (1)/(2)/(4) to strict (3) is assumed. Authenticode verifies a
publisher/trust action, not equality to an application-specific flat digest and
not safety of the implementation.
[WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust).

Migration requires an explicit new plan/profile identity and host opt-in. Existing
plans, CLI invocations and embedding APIs must not silently acquire weaker loading
authority. Source import, editor workspace trust or a project manifest alone
cannot authorize native execution. Analysis remains offline and must not load
DLLs, execute descriptor probes or run library constructors.

## 5. Ranked Windows architectures

These compare delivery mechanisms; some are complementary rather than exclusive.
“C libraries” means ordinary compatible Windows C DLLs/static objects whose exposed
signatures and lifetime effects have valid bindings, not every imaginable ABI.

| Rank / architecture | Ordinary user, admin/policy requirement | JIT / AOT and G1–G3 | Callbacks / affinity / raylib | Truthful guarantee and remaining mismatch |
|---|---|---|---|---|
| **1. Explicit OS loading from an application-owned approved bundle** | Windows 10/11 user-mode; no admin for per-user installation, no special policy | Same checked dynamic bridge in both; retain all existing supported contracts | Native loader supports ordinary libraries; VOID must separately admit callback/affinity effects | Checked artifact inventory and constrained search in a trusted process. Redirection, preexisting modules and later native loads remain OS/process trust. Recommended. |
| **2. Import library + executable import table** | Normal deployment; no special policy | Excellent ordinary AOT; JIT needs a linked host or an explicitly loaded adapter DLL | Conventional game deployment; same binding/affinity obligations | Import table names dependencies; it is not a digest lock. Startup initializers run before VOID's entry checks. Optional AOT packaging, not a strict G4 repair or the initial common path. |
| **3. Static link approved C objects into host/application/adapter** | Normal user build/deployment; licenses and CRT compatibility matter | Strong fit for AOT; JIT needs linked host or adapter | Raylib supports static builds; OS/graphics loads remain | Removes selected DLL-name edges, not native-code trust or all dynamic dependencies. Useful choice where upstream permits, not forced substitute for dynamic FFI. |
| **4. MSIX/package-owned deployment** | Requires suitable signing/install trust; per-user installation can avoid admin; enterprise restrictions vary | Runtime and AOT possible, subject to package/driver constraints | Desktop game use possible; native qualification needed | Package graph/integrity strengthens deployment. Does not expose verified-handle loading or automatically create a per-Runtime namespace. Optional packaging, not prerequisite. |
| **5. Separate native helper process** | Can run unelevated without special policy | Parent's ABI becomes an IPC protocol; G2 buffer copies and G3 remote capabilities require new implementation | Event loop can live in helper; synchronous callbacks/reentrancy and rendering IPC are costly | Address-space/crash separation only by default; same-user helper is not a permission sandbox. Startup selection remains OS-managed. Poor first raylib route. |
| **6. Enforced App Control / UMCI** | Device administrator/management deployment; not ordinary application-controlled setup | C ABI unaffected; LLVM JIT compatibility requires real policy qualification | Potentially admits games/drivers; every deployed policy matters | OS disallows images outside effective policy. It does not select the correct one among multiple allowed versions or bind a particular Runtime's digest. Optional enterprise track. |
| **Not recommended: no-entry/import-free provider plus explicit table as universal FFI** | Special rebuild needed; not general existing libraries | Synthetic table passed; image selection proof still absent | Rejects normal CRT/dependency/UI libraries and does not solve callbacks | Useful strict-profile fixture, not a route to arbitrary raylib DLLs. |
| **Reject: custom PE loader, undocumented promotion, post-load hash presented as prevention** | New unsupported/security burden | Large compatibility surface and new lifecycle failure modes | TLS, CRT, exceptions, CFG, unwind, drivers become additional obligations | No established solution here; does not satisfy KISS or current proof requirements. |

Microsoft's package loader requires package identity and permits eligible module
reuse within its graph. It is not an unpackaged alternate handle loader.
[Packaged loading](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-loadpackagedlibrary).
Package signing/install trust is a separate deployment prerequisite; a signature
alone must not be described as continuously enforced package integrity.
[MSIX signing requirements](https://learn.microsoft.com/en-us/windows/msix/package/signing-package-overview).

App Control availability must not be misstated as “Enterprise only”: Microsoft
documents policies effective on all editions, with tooling differences on Home,
and device-wide scope. Deployment is still a privileged administrative decision.
[Feature availability](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/feature-availability).
Enforced UMCI and effective policy matter, not audit-only logs. Its normal PE hash
is Authenticode-based and excludes some file fields; it is not identical to VOID's
whole-file SHA-256. Broad signer rules and independently allowed wrong DLLs also
leave a selection problem. Therefore UMCI is useful stronger deployment evidence,
not an already-proven exact-byte substitute.
[Policy and hash semantics](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/select-types-of-rules-to-create).

## 6. Recommended production architecture

### Admission and packaging

Keep host-issued `CPrimitiveAdmission` authority and the existing descriptor-driven
source module. Add a versioned OS-loader plan alongside the strict plan; do not
reinterpret `void-native-closure 1` or relax `system none`. The new plan identifies
profile, target ABI, provider/descriptor/thunk identity, canonical bundle artifacts
and hashes, required exports, declared application dependencies, approved system
surface and permitted native effects. Reject duplicate case-insensitive Windows
basenames and conflicting provider versions within one process plan.

Use an immutable versioned installation bundle with one flat application-DLL
directory initially. The installation/launch owner controls its executable,
manifest, dependencies and applicable ACLs. Protect it from unrelated writers;
retain existing path/reparse/identity checks and file holds where reusable.
Per-user installation trusts that user, not protection from that user's malicious
processes. Updates use a new bundle; do not rewrite DLLs underneath live bindings.
No new installer architecture is needed to prove the loader integration.

Hash/validate all declared artifacts before loading any of them. Inspect actual
PE imports, delay imports, machine type and required exports. Native initializers
and TLS are allowed only because this profile explicitly trusts native execution;
do not carry over the strict synthetic no-entry restriction. Inspect unexpected
forwarders before lookup; either resolve an explicitly approved forwarder owner
or reject with a precise unsupported-binding diagnostic. Never classify a filename
as trusted OS code merely because it resembles a system DLL.

The declared closure cannot prove every future `LoadLibrary` call a native library
might make. Provider-driven optional loads, OS API-set hosts and graphics drivers
belong to the explicit trusted native/OS boundary. A provider needing plugins or
private search mutation must declare a reviewed deployment requirement; do not
claim a hash list enforces all of its future behavior.

### Supported loader arrangement

The proposed top-level operation is:

```text
LoadLibraryExW(absolute_installed_adapter_path, NULL,
              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
              LOAD_LIBRARY_SEARCH_SYSTEM32)
```

No fallback to a bare name, PATH, current directory or flags zero after failure.
These flags restrict directory search; they are not a complete image-selection
policy. Microsoft's DLL security guidance supports absolute names and restricted
search as practical protections.
[DLL security guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-security).

For the **standalone VOID application host**, establish a deliberate process
default early, before third-party native loading: System32 plus a single approved
bundle directory registered through the documented user-directory mechanism.
This accommodates trusted libraries' later ordinary optional loads without using
the working directory. Avoid a growing set of provider directories with ambiguous
search order. Exact per-load flags remain in use for VOID's own loads.
Default search configuration cannot undo startup imports or neutralize a provider
that explicitly changes loader state. An embedding Runtime must not silently
change its host's process-wide policy: require the host to supply/accept that policy
or reject the profile. Scope those choices in the host admission contract.
[Process search configuration](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories).

Use the target-compiled adapter DLL as the existing C transport implementation.
It can link ordinary import libraries for the approved C DLLs. Load the adapter,
resolve its dispatch export, verify its descriptor probe and publish the immutable
binding only after all checks succeed. Compiler-generated VOID continues through
the checked bridge, not arbitrary source-provided addresses. No second runtime
FFI engine, manual PE mapping or global mutable dispatch table is needed.

Loaded-name/activation-context preflight and post-load owner diagnostics are useful
for finding misdeployment. Label them correctly: observations, not transactions.
A post-load mismatch is failed admission after potentially executed native code;
do not publish the binding or run eager VOID globals. The standalone host should
exit; an embedding host receives that failure and must not assume the process was
rolled back. Native faults and initializer side effects are not recoverable merely
because the wrapper returns an error.

### Ownership, sharing and reload

The Runtime binding owns its module reference and all separately acquired module
references. The loader retains ordinary import dependencies while needed; explicit
references and any approved forwarded-to code must remain valid through dispatch
and release. Keep the binding in generation leases and resources exactly as the
current dynamic path does. Drain calls and resource releases before unloading.
Do not promise safe unloading while a provider retains threads/callbacks: require
its shutdown contract or explicitly keep that approved binding until process exit.

Windows module identity is process-wide. Separate Runtimes do not imply separate
DLL global state. Compatible bindings may refer to the same mapped module; reject
conflicting versions rather than promise side-by-side isolation. Raylib's global
graphics session needs exclusive host ownership. Changed native plans/profile/
bundle identities require a new qualified execution session; compatible VOID code
reload may retain the existing native binding. Do not resurrect persistent C-provider
object caching while it remains unqualified.

The permission is to execute trusted native code, not to expose its raw pointers.
Retain G2 snapshots/writeback/overlap checks and G3 opaque tokens, Runtime-local
exclusive claims, owner checks and exactly-once terminal release. An actual
allocator/deallocator pair must come from the same approved provider contract;
matching C signatures do not authorize freeing memory through another CRT.

### Public developer path

The production milestone must include a narrow ordinary-developer admission path,
not end with a C++ test host. Reuse project preflight and existing compiler-owned
binding injection; explicitly version the currently reserved native declaration
schema. A dependency declaration identifies a reviewed bundle and binding artifact.
A trusted host/user action grants its native execution profile; the project cannot
self-grant it. Pin that approval to the reviewed plan identity, and invalidate it
when executable/ABI/effect inputs change.

Then ordinary VOID source imports the generated checked module, builds through
the pinned native toolchain and distributes the executable plus its approved native
bundle. AOT embeds the accepted plan before launch; JIT obtains it from host
authority. Neither requires Python, a custom loader, a developer-authored C++ host,
or customers manually configuring Windows policy. Exact CLI/schema spellings need
the existing project design review; none is falsely presented as working today.

## 7. JIT/AOT, callbacks and a real raylib milestone

**Common JIT/AOT route:** resolve the same Windows profile before eager globals;
use existing normal/O2 lowering and Runtime dispatch. AOT locates the installed
bundle from the executable, not CWD or a build-tree absolute path. Do not add
third-party provider startup imports to the main executable in the first common
route; an adapter DLL's normal imports execute only when that adapter is loaded.
This keeps offline analysis/build and runtime permission separate. It does not
retroactively authenticate the executable's trusted OS/Runtime startup imports.

**ABI coverage:** preserve Windows LLP64 (`c_long` i32, pointers/size 64-bit), exact
record layout and target-compiled by-value calls. No new universal ABI classifier
is needed. A C wrapper can adapt unsupported C++, variadic or pointer-rich APIs to
the existing supported surface, but the wrapper is an approved native artifact,
not evidence that those unsupported source types now work.

**Raylib has two additional concrete requirements:**

- `InitWindow`/render/input/`CloseWindow` must obey the chosen backend's main-thread
  and context rules. G3 currently permits shutdown on another thread only because
  its admitted release is required to be safe there. A graphics handle cannot be
  quietly placed into that policy. Add an explicit main/owner-thread session
  lifetime using existing host coordination: the host runs and drains this session
  on the main thread; foreign-thread shutdown requests notify that owner and keep
  roots/module leases until it drains. Do not synchronously wait while blocking
  the owner, or invoke graphics release from a foreign destructor. Qualify the
  failure path when owner progress is unavailable; it cannot promise successful
  native cleanup. Old G3 thread-independent releases retain their current behavior.
  This is a required new teardown policy, not an existing generic guarantee:
  reject an unsupported direct foreign-thread teardown before it starts releasing
  affine resources, and route orderly host requests through owner-thread drain.
- Some graphics calls can block, including swap and frame pacing. Extend the
  admitted effect metadata deliberately for these calls and use existing NativeSafe
  transitions; do not mislabel them nonblocking or move main-thread operations onto
  a generic worker. GC cooperation does not make a blocked owner responsive.

GLFW documents main-thread initialization, event processing, window creation and
destruction. This remains a binding contract even when a particular Windows call
appears to work on another thread.
[GLFW thread requirements](https://www.glfw.org/docs/3.4/intro_guide.html#thread_safety).

A useful first application can poll input and render without a **native-to-VOID**
callback. Raylib's internal GLFW callbacks remain C-to-C and do not violate that
boundary. Avoid exposing audio callbacks, retained string/buffer pointers or custom
VOID logging callbacks in that milestone. Use bounded call-scoped UTF-8 conversion
or a checked native adapter for window titles; reject embedded NUL. Rendering
records must pass the existing C-record verification.

For later general callbacks, reuse the existing private callback admission,
generation leases and drain machinery described in the
[Slice 7 architecture review](SLICE7_ARCHITECTURE_REVIEW.md). Generalize only with
explicit unregister/quiescence, owner-thread, reentrancy, captured-root and foreign-
thread rules. A callback-capable DLL loader does not mean revision 3 descriptors
already admit callbacks. Cross-boundary exceptions/longjmp must remain forbidden
unless separately specified. General retained callbacks are not needed to prove
dynamic raylib window/render/input feasibility.

For Linux, retain the strict loader and add any ordinary native profile separately;
do not feed raylib/libc into the `system none` loader. For macOS, normal dyld/system
framework loading is the corresponding architecture, but Apple arm64 descriptors,
calling ABI, signed bundle/library-validation behavior and main-thread teardown
require native qualification. This report does not label macOS G1–G4 complete.

## 8. Smallest discriminating proof and acceptance

After profile approval, perform **one integration proof** using the existing
Windows G1–G3 fixture and a normal target-built adapter/dependency DLL pair. Include
ordinary CRT startup and an intentional benign initializer marker. This changes
the question from “can a path load authenticate hostile loader state?” to “does
the explicitly trusted OS profile preserve VOID's checked bridge and lifetime?”

1. A trusted host selects the new profile and exact bundle. Offline analysis does
   not load it; unapproved source/manifest input cannot create native authority.
2. Load through the documented restricted arrangement. Demonstrate the expected
   initializer, primitive/float call, by-value record, bounded span and resource
   acquire/borrow/close through actual VOID source in normal and O2 JIT.
3. Independently link normal/O2 AOT, copy executable and bundle to a new Unicode/
   space-containing directory, remove the source/build bundle, and run from an
   unrelated CWD/PATH containing a conflicting dummy DLL. Verify actual behavior,
   module observations and diagnostics, not just load success.
4. Reuse G3's rejected-acquisition nonzero output, failed publication, refused
   release, terminal error, alias, shutdown race and generation-retention controls
   against the dynamically owned dispatch. No release code may unmap early.
5. Missing/replaced bundle, bad machine/descriptor/export and conflicting planned
   basenames fail appropriately. Strict-profile selection must still reject this
   ordinary Windows closure. Preserve W1/W2 as demonstrations of the stronger
   boundary; do not relabel them passing exact-image tests under the new profile.

This needs no App Control deployment and no new PE-loader campaign. A focused
Windows 10 and Windows 11 Debug/Release run is needed for the claimed support;
the existing Windows 10 receipts are not Windows 11 acceptance. Use the pinned
23.1.4 tools and existing 1 MiB stack, GC/reload and abnormal-exit evidence rules.
Preserve `_crash.void`, first failures and native dumps where generated. Boss
controls any hosted CI; no full hosted run is prescribed for each edit.

The **raylib completion proof** then exercises an exact approved shared build:
open a visible window, draw over multiple frames, receive real keyboard/mouse and
close events, close from the owner thread, release graphics before DLL retirement,
and repeat after compatible VOID reload where permitted. Cover early source error,
window creation failure and foreign-thread shutdown request. Run normal/O2 JIT
and relocated linked AOT. Capture native imports/CRT dependency receipts and actual
graphics-driver environment. Do not count a headless numeric fixture as graphics
acceptance, or require customers to install developer SDKs to run the result.

## 9. One implementation handoff, with migration dependencies

| Order | Components and change | Acceptance / stop condition |
|---|---|---|
| 1. Freeze profile and guarantees | Add the explicitly trusted OS-managed profile contract; preserve strict defaults and all old plan bytes. | Boss approves profile separation and public wording. If exact identity is still universal, stop conventional integration; no disguised downgrade. |
| 2. Common admission and Windows binding | `include/void/c_dynamic.hpp`, `src/dynamic_closure.cpp`, `include/void/c_primitive.hpp`, `src/ir/c_primitive.cpp`, platform implementation of `src/runtime/c_dynamic.cpp`; reuse parser bounds, descriptor/effect validation and binding ownership. | New profile cannot be forged through IR/source or confused with strict Linux; supported OS load, bounded preflight, no ambient fallback. |
| 3. JIT/AOT/lifetime integration | `src/backend/llvm/backend.cpp`, `src/runtime/abi.cpp`, `include/void/execution.hpp`, existing native resource/generation bridges and focused FFI fixtures. | Integrated proof above passes; no weakening of G1–G3 or accepted Linux G4-L. Do not patch away the acquisition/release guards. |
| 4. Developer packaging/admission | `src/frontend/project.cpp`, existing CLI project-preflight/build path, `src/native_build.cpp`, installed resource inventories and compiler-owned LSP analysis. Reuse controlled staging/atomic publication. | Ordinary developer can authorize the bundle, compile, relocate and run without a custom C++ host. Analysis remains inert; failed builds preserve old outputs. No root README or public promise before acceptance. |
| 5. Raylib effect/session integration | Versioned binding effects, existing NativeSafe/callback coordination and native-resource teardown; one owner-thread graphics session and checked raylib adapter. | Blocking and affinity are truthful, shutdown drains on owner, native globals not falsely per-Runtime. Fail rather than run release on the wrong thread. |
| 6. Native qualification and closeout | Existing tests extended for Windows 10/11 x64; Linux regression and explicit macOS arm64 qualification for any cross-platform claims. Record deployment/CRT/license requirements of selected artifacts. | Real source JIT/O2/AOT and graphics receipts; no broad claim of arbitrary C types/callbacks or hostile-loader identity. |

This is one coherent engineering mission after review. The small integration proof
is its early falsification step, not another open-ended research assignment. Do not
implement a helper IPC engine, package manager, general callback framework or new
security-policy installer to get ordinary Windows loading working.

## 10. Remaining decisions and final recommendation

1. **Approve a distinct ordinary native-execution profile.** Recommended: yes,
   explicit opt-in, with trusted OS/native process semantics and retained checked
   language boundaries. Keep strict Windows G4 unsupported instead of claiming it
   was solved. This is an intentional contract addition requiring Boss approval.
2. **Approve the standalone host's narrow search/deployment contract.** Recommended:
   one installed bundle plus trusted Windows system/driver surface; no CWD/PATH
   fallback, no concurrent conflicting versions. Embedders explicitly own their
   process policy. Developer-owned per-user installs are permitted, without an
   invented guarantee against their owner.
3. **Approve the raylib session/effect extension.** Recommended: main-thread
   lifecycle, explicit blocking effects, no native-to-VOID callbacks in the first
   window/render/input binding. Preserve existing G3 semantics for current providers.

**GO for the proposed conventional Windows FFI integration after these decisions;
HOLD the existing strict-image Windows claim.** Established languages demonstrate
the former architecture, not the latter guarantee. The accepted ABI and ownership
work can be reused. No supported interface found here supplies the stronger missing
transaction, and neither a fresh process nor a signed/hashed file changes that fact.

Stop for independent Golden Wrench architecture review and Boss authorization.
