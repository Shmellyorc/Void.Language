# Slice 7 general C FFI architecture review

Golden Wrench accepted the S6 final closeout. This review proposes a contract for
Boss and Golden Wrench to approve before implementation. Baseline HEAD is
`2f667ea338127b8f91c3136804377ef3fa5218d6`. Hosted CI remains on strict hold.
All future syntax, artifacts and gates below are proposals.

## Existing implementation

| Boundary | Implemented contract and gap |
| --- | --- |
| Provider admission | A C++ host passes one `NativeProject` to `analyze_project`. The compiled fixture and sink catalogs fix names, signatures, provider, affinity, blocking and callback flags. The frontend rejects collisions with the injected VFS module, parses its generated declarations using the ordinary frontend and skips their bodies in lowering. `Op::native_call` independently checks the coordinated module, provider, signature and direct callback provenance. Ordinary CLI/LSP analysis has no corresponding admission API for project authors. A public declaration, multiple-provider graph and admitted arbitrary export table remain open. [Catalog](../../src/ir/native_binding.cpp#L44) · [Frontend admission](../../src/frontend/compiler.cpp#L6701) · [Independent IR](../../src/ir/module.cpp#L1026) |
| C calls and layouts | The private fixture/sink bridge transports scalar bits, rooted objects and known callback dispatch data to compiled C prototypes. The native compiler supplies the actual C call ABI. VOID numeric scalars and S6 enums have verified representations. The existing LLVM `DataLayout`, aggregate and managed layouts describe VOID storage. They do not establish foreign struct packing, field offsets or target-specific argument and return classification. There is no public arbitrary C pointer, C aggregate or variadic ABI. [Native transport](../../src/backend/llvm/backend.cpp#L1300) · [Native bridge](../../src/runtime/native_binding.cpp#L106) · [Numeric contract](../NUMERIC_TYPES.md) · [S6 IR closeout](SLICE6_FINAL_CLOSEOUT.md) |
| GC and native data | The current Heap validates precise roots and marks and sweeps allocated objects. Runtime frame slots, handles, frozen NativeSafe frames and rooted publication protect live data. Current address stability grants no C borrow rights. The compiled bridge copies terminated UTF-8 and rejects embedded NUL, supports length-aware bytes, and pairs owned returns with provider frees. Existing fixed arrays, growable arrays, lists and strings have Runtime-specific storage, live-prefix or schema rules. They are not foreign memory layouts. [Heap](../../src/runtime/heap.cpp#L228) · [GC Runtime](../GC_RUNTIME.md#roots-and-shared-llvm-lowering) · [Strings](../STRINGS.md#trusted-native-and-tooling-boundaries) · [S6 storage proof](SLICE6_FINAL_CLOSEOUT.md) |
| Native lifetime | `NativeResource` and the Runtime ledger bind opaque wrapper identity, provider, owner thread and close state. The fixture retains a Handle and the exact generation for captures. Registration reserves provider dispatch before trampoline admission, then drains both reservations and admitted callbacks. Direct named or newly formed callbacks are checked. Arbitrary forwarded function values carry no usable generation token. No general dynamic library lease, lifetime proof for third-party unregister, or arbitrary worker/async native permission is implemented. [Ledger](../../src/runtime/native_resource.cpp) · [Fixture dispatch](../../src/runtime/native_binding.cpp#L40) · [Callback admission](../../include/void/callback.hpp) |
| Loader and link | ORC binds a finite host symbol map with `absoluteSymbols` and owns generation trackers. Normal/O2 JIT and host-native AOT share the verified LLVM lowerer. `build_executable` selects the pinned Clang/LLD 23.1.4 resources and links the matching Runtime and fixture, with literal argv, inventory checks for v2 resources, source/output collision checks and controlled staging. Windows uses held paths, a private DACL, a short link workspace and verified PE publication. There are internal platform calls and an optional Linux terminfo lookup, but neither grants project loading authority. No user DLL/DSO search, import-library or export resolution plan exists. [ORC](../../src/backend/llvm/backend.cpp#L2047) · [Native build](../../src/native_build.cpp#L114) · [Windows staging](../../src/native_windows.hpp) · [Private terminfo](../../src/runtime/console_terminfo.inc) |
| Startup and reload | The S4 application record is immutable Runtime identity data. JIT and the AOT bootstrap admit it before eager globals. Exact compatibility data and generation leases protect replacement and diagnostic ownership. Debug native cache already keys on compiler and target inputs and rebuilds Runtime bindings on a hit. The Windows persistent cache remains unavailable. General native plan compatibility and loader handles are new responsibilities. [Application metadata](SLICE4.md) · [Runtime globals](../RUNTIME_GLOBALS.md) · [Reload and invocation](../../src/runtime/reload.cpp) · [Cache](../../src/backend/llvm/backend.cpp#L2161) |
| Project, package and tools | Schema 1 recognizes empty reserved `native` and `dependencies` tables and rejects nonempty ones. Local source libraries are VFS imports. VOIDUP has its own verified compiler catalog and install capabilities. S1–S3 own bounded manifests, project snapshots, diagnostics and editor revisions. S5 owns explicit creation, without granting native permissions. `voidls` uses compiler-owned analysis and VS Code gates native analysis on workspace trust. Neither dependency resolution nor third-party admission follows from these capabilities. [Manifest parser](../../src/frontend/project.cpp#L222) · [Local libraries](../LOCAL_LIBRARIES.md) · [VOIDUP trust](../../apps/voidup/trust.void) · [Project preflight](../../src/lsp/main.cpp#L739) · [Editor trust](../../editors/vscode/src/extension.ts#L65) |

The accepted closeouts establish the baseline independently of this proposal.
S1 covers schema and provenance, S2 project admission and recapture, S3 sessions,
profiles and preflight, S4 Runtime metadata and installed AOT, S5 project creation
and the editor, and S6 nominal integer enums and storage verification.
[S1](SLICE1.md) · [S1 closeout](SLICE1_CLOSEOUT.md) · [S2](SLICE2.md) ·
[S2 closeout](SLICE2_FINAL_CLOSEOUT.md) · [S3](SLICE3.md) ·
[S3 closeout](SLICE3_FINAL_CLOSEOUT.md) · [S4](SLICE4.md) · [S5](SLICE5.md) ·
[S6](SLICE6.md) · [S6 closeout](SLICE6_FINAL_CLOSEOUT.md).

## Public declaration proposal

Use one compiler-owned binding declaration form in admitted binding modules.
Normal applications call ordinary checked VOID wrappers. A block names a
package identity, while the import and lock select the exact native artifact
and ABI profile. Neither the package spelling nor an `unsafe` marker grants
admission. The following fictional C API shows the proposed spelling.

```text
unsafe native image_api from "org.example.image":
    opaque image = "struct img_handle"

    c struct color = "ImgColor":
        red: u8
        green: u8
        blue: u8
        alpha: u8

    c enum image_mode(c_int) = "ImgMode"
    const fast: image_mode = c_constant("IMG_FAST")

    extern "c" func blend(a: color, b: color) -> color = "img_blend"
    extern "c" func open_file(name: utf8_z) -> owned<image>? = "img_open"
        with (release=close_file, thread=owner, blocking=true, callbacks=none)
    extern "c" func close_file(file: closing<image>) = "img_close"
        with (thread=owner, callbacks=none)
    extern "c" func read(file: borrow<image>, bytes: out_span<u8, count>, count: c_size)
        -> c_int = "img_read" with (thread=owner, blocking=true, callbacks=none)
    extern "c" func write(file: borrow<mut image>, bytes: in_span<u8, count>, count: c_size)
        -> c_int = "img_write" with (thread=owner, blocking=true, callbacks=none)

    c callback visit_fn(context: opaque_context, index: c_int) -> c_int
    extern "c" func visit(file: borrow<image>, cb: scoped_callback<visit_fn>, ctx: context<cb>)
        -> c_int = "img_visit" with (thread=owner, callbacks=scoped)
```

Proposal rules for the example:

- The block supplies a local namespace. Block entries are private raw bindings
  by default. `extern "c"` is a declaration without a VOID body. The quoted
  export is exact and case-sensitive. A library identifier is a logical package
  key, never a path, filename, URL, search glob or linker option. A C type name
  and constant key must match admitted metadata. The wrapper may expose a
  deliberate public API through ordinary import and visibility rules.
- `c struct` admits finite, recursively plain C data with fixed-width fields,
  other admitted records and `c_array<T, N>` inline members. A C array in a
  parameter is spelled as a pointer/span under its actual adjusted prototype.
  Unknown packing, unions, bitfields, flexible members, C++ types, explicit
  vector types, by-value opaque values and unsupported ABI attributes diagnose.
  The ABI artifact provides size, alignment, field offsets and call
  classification. A record with pointer fields starts deferred pending an
  explicit per-field provenance and lifetime contract.
- `c_int`, `c_uint`, `c_long`, `c_ulong`, `c_size`, `c_ptrdiff` and
  `c_char` are binding-only target C names. `c_char` uses the verified
  target/header signedness. `c_bool` is the header's actual C Boolean type.
  Fixed-width `i8` through `u64` map only to matching C types, and
  `f32`/`f64` require verified `float`/`double` representations.
  No implicit widening, platform-sized defaults, `wchar_t`, long-double or
  `long`-equals-pointer shortcut. Target profile fixes pointer width,
  endianness, object format, compiler ABI options and default C convention.
  Linux x64 SysV, Windows x64 MSVC and Apple arm64 require distinct probes.
  Apple x64 and additional targets qualify independently. [SysV psABI](https://gitlab.com/x86-psABIs/x86-64-ABI/blob/master/x86-64-ABI/low-level-sys-info.tex) ·
  [Microsoft x64 convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) ·
  [Apple arm64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms) ·
  [AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst).
- `c enum image_mode(c_int)` asserts the actual selected C integer
  representation for this target. It preserves any returned raw value,
  including future and reserved values. It does not create a closed VOID
  enum. A checked raw integer to a S6 nominal enum uses existing `as?` at
  the wrapper boundary. Conversion from VOID enum to raw C integer needs
  explicit wrapper mapping under the current S6 contract. Constants are
  extracted as typed values from the approved header and configuration.
  Unsupported function-like macros require a fixed-prototype wrapper.
- Proposed binding-only `ptr<const T>?` and `ptr<mut T>?` express exact
  nullable data pointer declarations. Omit `?` for a nonnull contract that
  must be checked at the boundary. Initial IR can materialize raw pointers
  only from an admitted borrow, buffer, string, callback context or ownership
  conversion. A raw pointer cannot appear as an ordinary source local,
  result, struct field, integer cast or arithmetic operand. Unconstrained
  `void*` is admitted only for reviewed opaque-context/typed-buffer uses.
  Function pointers are a distinct exact-prototype capability. The first
  callback surface uses typed call slots and rejects arbitrary casts,
  returned or global function pointers and variadic functions/callbacks.
  A fixed-prototype audited C shim is the route for a necessary variadic API.
- `owned<T>?` yields either a registry-backed opaque owner or absence. A
  successful acquire binds the exact release operation and provider
  generation. Managed aliases share one checked close state. `closing<T>`
  is the generated one-time close path, and `borrow<T>` or
  `borrow<mut T>` borrows through a live owner for the call. The first
  delivery rejects arbitrary ownership transfer between libraries and
  retained borrows without an independently approved owner/release rule.
  `context<cb>` derives the C context paired with the exact named callback
  parameter. The generated wrapper hides both that context and span counts
  from ordinary callers. Additional clause keys, ownership relations and
  effect combinations require an explicit schema decision.

Exact surface alternative for the first implementation is an audited descriptor
plus generated declarations. It is smaller to ship and avoids freezing syntax
while target layouts and ownership are being qualified. For example, a checked
wrapper imports `use "./image_bindings.void" as image_c`; a sidecar record names
`{ "binding": "image_c.open_file", "symbol": "img_open", "abi": "c",
"parameters": ["utf8_z"], "result": "owned<image>?", "release": "img_close" }`
inside a separately admitted package. That abbreviated record would also need
the same effects, target evidence and exact ownership edges as the proposed
block. The other alternative is Bible-style `native "image":` with an implicit
library name. Recommend an explicit logical package key and the same canonical
descriptor for both forms, deliver the generated form first, then approve or
freeze authored grammar independently. A general raw-pointer `unsafe` block
would require a substantially broader escape and ABI contract.
[Language Bible](../LANGUAGE_BIBLE.md#41-native-c-interoperability-agreed) ·
[Project roadmap](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-general-purpose-c-interoperability).

### Lowering and ownership

Recommend target C thunks produced offline from approved headers, compiler
configuration and a canonical binding descriptor. Use the target C compiler
for actual calling convention lowering. For example, a thunk accepts checked
scalar words or addresses of aligned native input/output storage and calls
through an exact C function-pointer type. The outer bridge uses a fixed,
reviewed transport ABI with an explicit failure slot. A native struct return
is written into validated native output storage, then imported as a VOID value.
An LLVM struct with matching byte size does not alone prove the C call ABI.
Confirm layouts and calls against separately compiled C translation units,
including small, large, mixed-class and return-by-hidden-pointer records.
An offline header extractor can use matching Clang tooling and exact
compile definitions. Project analysis reads bounded descriptor data without
running it or compiling arbitrary headers. Direct LLVM ABI classification
could avoid thunks later if a separately reviewed implementation proves all
target attributes and classifications. [Clang LibTooling](https://clang.llvm.org/docs/LibTooling.html).

- Before entering NativeSafe, root any reachable managed values and materialize
  validated native scratch with exact alignment, overflow-checked length and
  initialized storage. A per-call permit holds the plan, provider, resource and
  generation leases through call completion and conversion. No C pointer
  references managed heap objects, movable temporaries, frozen slots or Runtime
  buffer internals. Copy back only after successful status, validated reported
  length and Managed reentry. Zero-length and null rules are explicit per
  binding. Distinct output spans reject forbidden overlap. A trusted provider
  can still overrun a buffer, so the policy is not an in-process sandbox.
- `utf8_z` copies a valid VOID string, rejects embedded NUL and appends one
  terminator. A length-aware UTF-8 or byte span keeps embedded NUL. Returned
  strings use a reviewed maximum or reported length, copy before the borrow
  expires, then validate UTF-8. An owned return is released even if allocation
  or validation fails. Retained strings or buffers use a binding-owned native
  copy, with deregistration and quiescence before freeing it. Unbounded scans,
  ambient locale conversion and UTF-16 admission are deferred.
- Declare nullability, length and error conventions independently. Ordinary
  wrappers return existing `result<T, E>` for recoverable API failure and
  nullable managed wrappers where absence is normal. C null, raw error codes
  and Windows last-error/errno are not interchangeable. If a binding requests
  errno or last-error capture, copy it on the calling thread immediately
  after the C call. Keep checked FFI infrastructure failures source-located
  through the existing Runtime failure channel. Safe wrappers use explicit
  close, `deinit` or `with`; GC remains a reachability mechanism.
- Resource publication is transactional. Stage roots and the wrapper first.
  On acquisition failure, release any provisional native output and preserve
  the primary error. Destruction runs on its admitted thread. Shutdown on
  another thread must schedule an approved owner-executor drain or report
  that shutdown cannot complete safely. It may not call an arbitrary
  thread-affine destructor or discard the library lease. Transitive worker
  restrictions stay in force.

## Plan, loader and linker

Separate **declarations**, **resolved inputs** and **execution permission**.
A future, versioned project schema may request a package, ABI revision and
linkage. A lock records exact resolved artifact hashes and target closure.
A host-admitted immutable plan, made from independently authenticated receipts,
adds permitted roots, target/export policy and verified artifacts. Existing
schema 1 keeps its behavior. Source, editor trust, manifests, app metadata and
lockfiles provide no native loading, install or toolchain override authority.
Recommend a small explicit, local, preverified plan for first fixtures.
Package discovery, fetch, conflict resolution and installer integration are
a separate later gate. Reuse relevant VOIDUP verification primitives without
inheriting toolchain install authority. Do not add native package flags to
`void-native.txt` or permit unrestricted linker arguments or build scripts.

The plan binds logical package identity, version, ABI revision, target and ABI
profile, exact header/configuration and thunk digests, actual artifact format,
native identity, export signatures, file hashes, transitive dependency edges and
license/source receipts. Record target system-library exceptions separately.
Do not infer binary or ABI equality from a SemVer, a SONAME or a successful
symbol lookup. Check authentic provenance, architecture, export kind, allowed
forwarding/reexports and declared imports without executing the artifact. Use
bounded object-format parsing with explicit limits on files, names, nesting
and records. Reject unexpected TLS/initializer/dynamic-import behavior where
the selected loader profile cannot enforce the declared closure. A known library
with trusted constructors still counts as native code execution.

### Common admission and publication

1. Resolve only under exact roots explicitly granted by the host, such as a
   preverified package root or installed application private bundle. Revalidate
   spelling, path containment, hardlinks, alias/reparse policy, file identity and
   hashes through held files and ancestors. Stage approved bytes into a private,
   controlled root when the platform loader requires a pathname. Match the
   whole transitive closure and reject collisions before mapping anything.
2. For execute/build, verify the plan against the selected target and graph.
   `check`, discovery, hover, completion and dependency indexing consume
   snapshots and descriptor data without mapping libraries or initiating
   downloads. A pure build validates and stages artifacts without executing
   their constructors. Require a compatible immutable plan before JIT
   materialization or AOT startup and before eager VOID globals.
3. At execution, create the loader/plan lease, load the approved closure, resolve
   the exact required exports, verify loaded image owners and publish the
   generation's call table only when complete. On a failure, publish no
   partially initialized table and release provisional handles in reverse
   dependency order when safe. If a constructor has already run, its effects
   cannot in general be rolled back. Fail the process/session and leave any
   unsafe-to-close handle retained for safe termination. Keep a previous usable
   output bundle if validation, link or publication fails.
4. Preflight is only as strong as the allowed loader and host namespace. An
   absolute top-level path, pre-load hashes, or a post-load mismatch check
   alone cannot prevent pre-entry substitution in transitive dependencies.
   Require a controlled, read-only-for-the-project artifact root and a trusted
   launcher/process environment. Record protections against replacement and
   same-name reuse separately for each platform. Existing hostile native code,
   preloaded process code and a hostile actor with equivalent host privileges
   are outside the protection this plan can claim. A profile whose closure
   cannot be constrained fails admission.

| Platform | Recommended dynamic profile and required negative tests |
| --- | --- |
| Linux | Inspect ELF class, architecture, SONAME, versioned exports, `DT_NEEDED`, RPATH and RUNPATH. Use absolute admitted loads with `RTLD_NOW | RTLD_LOCAL` and exact-handle `dlsym`/version resolution, including error and owner checks. Relocated private layouts can use deliberately verified `$ORIGIN` entries per object; `DT_RUNPATH` does not supply the whole transitive search. Specify the supported OS/loader and system ABI floor. A controlled launch must address `LD_LIBRARY_PATH`, `LD_PRELOAD`, `LD_AUDIT`, preexisting module conflicts and interposition. Reject closure designs that rely on ambient resolution. Include substituted transitive SONAME, dependency export and symlink/race cases. [Linux loader](https://man7.org/linux/man-pages/man8/ld.so.8.html) · [dlopen](https://man7.org/linux/man-pages/man3/dlopen.3.html) · [dlsym](https://man7.org/linux/man-pages/man3/dlsym.3.html) |
| Windows | Inspect PE machine, imports/delay imports, exact export names, forwarding and target CRT. Use an absolute verified DLL path with a qualified `LoadLibraryExW` restricted dependency policy, starting with a private closure directory and trusted System32 cases. Hold and verify ordinary files and DACLs using existing Windows staging foundations. Qualify KnownDLL/API-set rules, already-loaded collisions, forwarded exports and how the actual mapped file is identified. Preserve the short-path native linker staging and test Unicode, reparse, hardlink, path alias, cwd/PATH and transitive DLL substitution. The matching import `.lib` describes a DLL and is distinct from a static archive. [LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw) · [Windows DLL search](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order) |
| macOS | Inspect Mach-O architecture slices, deployment target, load commands, install names and signatures. Use verified absolute top-level opens with an explicit private `@loader_path`/`@rpath` closure and controlled system paths. Qualify actual dyld mappings, disallow unexpected reexports and insertion paths, and verify signing and Hardened Runtime/Library Validation policies. Qualify JIT entitlements separately from installed AOT; a development launch cannot stand in for signed distribution. Native host and display evidence are required before claiming support. [Apple runpaths](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/RunpathDependentLibraries.html) · [Hardened Runtime](https://developer.apple.com/documentation/xcode/configuring-the-hardened-runtime) |

**JIT.** Reuse ORC generation trackers. Resolve and lease a bounded call table
from the admitted plan. Bind only reviewed private transport symbols/trampolines,
with exact linkage identity and no generic process symbol search. A cache entry
contains relocatable code and provenance, never raw pointers, loader handles,
resources or registrations. Include target, plan/closure, descriptor and thunk
digests and ABI revision in eligibility and re-admit/rebind on a hit. Leave
persistent-cache support disabled for a new mode until cold/warm, corruption
and change-invalidation cases qualify. [LLVM ORC](https://llvm.org/docs/ORCv2.html).

**AOT.** Recommend independently linking the emitted VOID object, pinned
Runtime and approved target thunk objects with the existing driver. The
standalone executable embeds an expected plan identity and opens its bundled
DSOs at startup through the same checked loader. It needs neither JIT nor
the source checkout. This is an independently linked AOT executable with
runtime dynamic resolution of the C provider. It makes no claim that the
provider is an eager image import. If the milestone also requires a direct
ELF `DT_NEEDED`, Mach-O load-command or PE import-table dependency on the
provider, that is a distinct packaging mode and acceptance choice. Eager imports
can execute before the VOID bootstrap. They require a verified launch/installation
boundary that controls the actual native loader before startup and independent
negative proof. A first manual `emit` workflow needs a verified linker receipt
and equivalent launch contract or an explicit unsupported diagnostic.

**Static.** An explicit future selection may link verified PIC/CRT-compatible
archives in approved link order with their native dependency closure and
redistribution materials. Never substitute an archive for a failed dynamic
test. Initial static JIT is unsupported unless a separately verified
archive-materialization design is approved. Static AOT and direct-import
dynamic AOT need their own result rows.

**Redistribution.** Ship the installed executable and matching native closure
using a verified relocatable directory or platform bundle, with assets and
notices separate from trust policy. Record exact source revisions, patches,
build options, artifact hashes, third-party license texts and applicable
corresponding source. Reconcile per-platform SDK/CRT, signing and bundled
dependencies using the existing release review before distribution. The upstream
raylib license uses zlib/libpng terms. Inventory its actual selected embedded
and linked dependencies separately. No version or package pin is chosen here.
[Release inventory](../release/NOTICE_INVENTORY.json) ·
[Release blockers](../release/PREVIEW_BLOCKERS.md) ·
[raylib license](https://github.com/raysan5/raylib/blob/master/LICENSE) ·
[raylib build](https://github.com/raysan5/raylib/blob/master/src/CMakeLists.txt).

## Callback and Runtime lifetime

- **Scoped callback.** Begin with same-thread callbacks during an active C
  invocation under an explicit no-retain contract. Materialize the exact C
  trampoline and context together with the capture Handle, owner Runtime,
  checked invocation state and generation/library leases. Enter NativeSafe
  for C. The trampoline acquires admission, reenters the same Runtime in
  Managed state, roots decoded arguments and checks results. A scoped contract
  must prove no dispatch can still begin once the call returns. A joined
  foreign-thread variant can follow only with explicit join, affinity and
  cancellation evidence. Reject the callback on a wrong Runtime or thread.
- **Retained callback.** Give a binding-specific registration record an opaque
  native context and exact provenance. It owns the capture root and code/library
  leases across calls. Require an actual provider unregister-and-quiesce
  guarantee covering reserved dispatches before admission as well as admitted
  calls. A foreign thread may attach to the owning Runtime only through this
  admission. Affinity to an owner or graphics thread is checked separately.
  A callback without a user context or provider drain contract stays unsupported
  until a separate design proves a bounded dispatch key and lifetime.
- **Failures.** A trampoline is no-throw at the C boundary. Copy checked VOID
  failure details while their generation is leased and return a declared C
  sentinel/status, or record an owned fault for a later explicit drain when C
  returns void. Keep this separate from recoverable provider status. Callback
  failure can stop later admissions without pretending to abort arbitrary C.
  Preserve primary errors through cleanup. No C++ or VOID exception crosses
  a C frame, and no language runtime failure attempts to recover arbitrary
  native faults or `longjmp` across the bridge.
- **Retirement.** Close admission, stop native jobs, unregister at the provider
  without holding Runtime/ledger locks, wait for reservations and admitted
  calls, drain dependent resources on their authorized threads, and only then
  release captured roots, thunk/code leases and loader handles. Self-drain
  diagnoses before state changes. Concurrent close has one terminal result.
  If the provider cannot establish quiescence, retain the affected context and
  library until safe termination. Compatible body-only replacement keeps an
  old callback on its original generation. Any plan, layout, ABI, ownership,
  symbol or closure change requires restart. Initially reject hot replacement
  of a loaded native binary even when its public signatures match.
  [Current callback proof](../GC_RUNTIME.md#native-callbacks-and-shutdown) ·
  [Generation publication](../ANALYSIS_COMPATIBILITY.md#native-generation-publication).

## Implementation gates and falsification tests

Apply the following harness to **each gate** with a target C fixture built
independently of the generated binding and a real compiled VOID client.
Each gate pairs executed positive and negative cases, direct malformed typed
IR, normal/O2 JIT, and normal/O2 independently linked AOT. Run Linux Release,
Debug and fatal ASan/UBSan with leak detection and instrument the C fixture
where available. Run native Windows 10 Release and Debug against the same
scope and record any Windows sanitizer or macOS gaps. Record exact argv,
exit status, diagnostics and spans, toolchain/target, image/receipt hashes,
positive output and required absence of unauthorized side effects. Platform
gaps remain explicit. Preserve negative provider, S1–S6, GC, cache and
editor regressions. Golden Wrench reviews at every architectural boundary.

| Gate | Independent proof and deliberate failures |
| --- | --- |
| G1 primitives | Host supplies an admitted synthetic fixture through an explicit harness. Validate exact prototype, integer sign/extension, C Boolean and float transport, C-sized aliases, argument/result arity and calling convention. Use mixed register/stack arguments and signed boundary tests. Forge a provider key, descriptor, alias width, foreign result, flag, callable and wrong calling convention in typed IR. The fixture may use a linked test harness at this gate. It supplies no qualification for production dynamic linkage. |
| G2 structs and borrowed buffers | Validate layouts, exact type identity, plain by-value inputs and returns, scoped pointer origin, alignment, checked bounds and initialized outputs. Test small, large and nested aggregates against compiled C, output copyback, empty input and target-specific padding. Reject offset/packing changes, forged aggregate carriers, raw pointer escape, overlap and undersized or uninitialized output. Exercise GC while native scratch is live. |
| G3 ownership | Prove opaque acquire/publication/release and checked borrow ownership with C allocation counters and fault injection. Test null acquisition, allocation/conversion failures, duplicate close, use after close, wrong provider/Runtime/thread, failed release policy and cleanup order. Forge nonnull owner tokens, pointer provenance and descriptor generation changes. Test retained UTF-8 copy, NUL rejection in terminated mode, invalid returned UTF-8 and leak checks. |
| G4 dynamic closure and installation | Authenticate a local fixture plan, run both JIT modes and build/relocate both AOT modes. Record actual loaded image and closure with a native mapping oracle and a constructor sentinel. Test missing/wrong library, export or version, wrong architecture/CRT, invalid hash, altered transitive artifact, aliases, owner/permission/race, search and environment injection, and partial publication. Verify ordinary check/LSP never starts a constructor. IR rejects forged plan/ordinal/ABI revision; runtime rejects loading before admission. Test cache invalidation or explicit unavailability. |
| G5 callbacks | First qualify scoped same-thread callbacks, then independently admit a retained fixture with a proven pre-admission provider barrier. Cover GC pressure, synchronous reentry, admitted foreign dispatch, cancellation, failure sentinel, self-drain, wrong owner/thread/Runtime, duplicate unregister, old-code invocation after reload, and release only after pre-admission and in-flight drains. Forge arbitrary function pointer, context mismatch, stale generation or invalid retained provenance in IR. Test during both JIT modes and AOT, with scheduling/race stress. |
| G6 raylib application | Execute the interactive dynamic plan below. Gate ordinary and malformed generated raylib descriptors through the same independent validator. Repeat missing/wrong native artifact, lifetime, installer and hostile search probes for its actual closure. Run the instrumented compiler/Runtime and separate synthetic instrumented fixture in strict configurations, and distinguish instrumentation of raylib itself and display support. Capture Linux and Windows native evidence and macOS evidence when available. |

G1–G3 may use an explicitly supplied synthetic fixture to isolate ABI and
ownership proofs. G4 must establish a real loader lease before G5 exercises
callback retention and reload with a dynamically loaded library. Freeze the
loader threat model and plan identity **before** implementation. Do not infer
dynamic support from G1–G3 or defer all loader risk until the raylib example.

## Raylib acceptance

Use a real manifest-selected VOID application with an arbitrary permitted
entry filename. Its ordinary wrapper package uses the same general FFI and
native admission as the synthetic fixtures. Pin a reviewed raylib release,
header, target build options and license/source closure at that gate. The
upstream API currently provides the representative functions below. Exact
prototypes and constants must be regenerated from the selected header.
[Upstream header](https://github.com/raysan5/raylib/blob/master/src/raylib.h) ·
[Mouse input example](https://github.com/raysan5/raylib/blob/master/examples/core/core_input_mouse.c).

1. On the owning application thread, call `InitWindow`, check `IsWindowReady`
   and initialize a bounded texture resource. Exercise by-value `Color`,
   `Vector2`, `Image` and `Texture2D` with verified layouts and C call
   classification, including both results and parameters. For example,
   `GenImageColor`, `LoadTextureFromImage`, `UnloadImage` and
   `DrawTextureV` exercise image and GPU ownership without a second engine.
   `Image.data` requires a separate ownership proof before this gate.
   Use a generated, handle-backed record adapter that keeps the pointer in
   native storage and calls the real by-value prototypes. Check native layout
   and the acquire/release pair, and reject raw record construction, copying
   and pointer access in the independent IR validator. Apply the same handle
   rules to `Texture2D` ownership even though its fields are scalar.
   Arm the matching cleanup only for successfully acquired state. Audit
   partial-window failure cleanup against the selected library.
2. Run multiple actual `BeginDrawing`/drawing/`EndDrawing` frames. Move a
   visible object with `IsKeyDown` and a verified key constant, and exercise
   mouse movement via `GetMousePosition`. Use a controlled input event and
   assert visible or measured frame changes. Capture render/input evidence,
   frame counts and exit on an explicit bounded test path as well as
   interactive close via `WindowShouldClose`. Headless/no-input smoke is
   a separate result.
3. Release texture/image state exactly once on the owner thread and close the
   window after the last frame. Exercise induced failure and repeated launch
   without leaking native state. G5 qualifies real C callback lifetimes with
   the generic fixture. If a raylib callback is exercised, it needs its own
   exact signature, lifetime and unregister proof. Do not assume that a
   raylib callback lacking a context or accepting `va_list` qualifies.
4. Run normal and O2 JIT and independently linked normal and O2 AOT. Verify
   real dynamic mapping and the admitted shared artifact, never a static
   substitute. Relocate the installed executable and private closure away
   from the checkout and compiler, run from an unrelated working directory,
   and remove or replace the provider and a transitive dependency in isolated
   copies to prove rejection. Verify manifest identity in the application
   and preserve Linux/Windows platform startup, render, input and shutdown
   evidence. macOS is unqualified until it runs on a native host.
5. In an actual VS Code host, verify project discovery, build/run with the
   selected profile, binding definitions/hover, exact missing dependency and
   layout diagnostics, dirty revision recovery and read-only analysis of an
   untrusted workspace. Compiler-owned diagnostics carry owning source spans,
   target, expected identity and failure category. Missing, denied, unverified,
   incompatible, unsupported and runtime call failures are distinct. Expose
   an explicit resolution action, never an automatic fetch or load. Existing
   protocol and stale-result guarantees remain unless a reviewed version
   explicitly extends them.

The gate requires the descriptor and thunk toolchain, checked ownership,
a verified local native plan, target loaders, package/installation closure,
editor integration and a working display/input host. The broader resolver and
publication gate still require separate approval. An installed run, live
rendering and dynamic provenance must all be observed before the integrated
hosted acceptance gate is requested.

## Decisions requiring Boss and Golden Wrench approval

| Decision | Recommendation and alternative |
| --- | --- |
| Binding format | Approve canonical generated descriptors and matching C thunks first. Separately approve authored block grammar once diagnostics and target proofs exist. Direct LLVM classification and inline raw pointers need their own proof. |
| Target scope and C types | Begin Linux x64 SysV and Windows x64 MSVC. Treat each macOS architecture, packed/union layouts, alternate conventions, and pointer-valued records as independent admission gates. Fix the exact C-sized types, raw enum surface, pointer nullability and export/version syntax before freezing schema. |
| Ownership and effects | Approve registry ownership, call-scoped copies/borrows, explicit cleanup and structured failure. Review handle-backed C records for raylib `Image`, ownership transfer, retained buffers and thread-affine shutdown separately. |
| Loader authority | Approve plan producer, authentication trust root, installer split, exact OS system-library allowlist, how each protected closure remains immutable, and the trusted launch environment. If the platform cannot enforce closure isolation in-process, defer the target or review a separate broker/process model. |
| AOT definition | Approve standalone AOT with linked thunks and verified runtime DSO resolution as the dynamic acceptance mode, or require additional direct-import AOT under a pre-entry admission contract. Keep optional static separately qualified. |
| Callback scope | Approve scoped first, retained only for demonstrable provider quiescence, and no arbitrary forwarded/stored values. Decide whether a particular raylib callback must be exercised. Treat no-context, variadic, and unrelated-thread retained callbacks as separate designs. |
| Package and release | Approve the future manifest/lock schema and who owns resolution, choose the raylib pin and actual dependency closure after ABI/loader review, and approve packaging/signing policy before any publication. |

## Audit evidence and Git inventory

The audit used the retained Linux Release compiler for four read-only
`voidc check` probes. The ordinary manifest succeeded. Nonempty reserved
native/dependency tables returned VOID5008, and the proposed declaration
returned a syntax error. The existing default-profile debug-info warning is
present where applicable. These probes do not qualify an FFI implementation.
[Probe and storage receipt](evidence/slice7-audit.json).

The Windows 10 Home guest is build 19045. The initial read-only audit observed
3.602 GiB free on C:. The separately authorized resize expanded its existing
NTFS volume by exactly 30 GiB. The verified return from Windows Recovery
observed **109.37 GiB total and 33.81 GiB free** on C:. This is a measured
post-resize planning baseline. Recheck before native builds and budget for
both outputs, staging and logs without removing earlier qualification assets.
No S7 implementation build, raylib launch, native S7 qualification or hosted
run took place in this audit.

The [layered Git inventory](evidence/slice7-inventory.txt) verifies the 1,112
starting paths and exact index tuples. The review adds its report, receipt and
inventory only. All S1–S6 source and evidence bytes are preserved.

## End-of-task temporary workspace cleanup

Removed the assignment-owned host workspace `/home/shmellyorc/VM/void-s7-audit.Wbw08GjA`
after preserving the exact Linux probe inputs and output, storage records and
starting path hashes. The [audit receipt](evidence/slice7-audit.json) lists
every removed path. No guest cleanup was required. Windows Explorer showed
**33.8 GiB free** on C: at handoff, consistent with the byte-precise
post-resize measurement. Earlier qualification binaries, test harnesses,
source packets, build trees and toolchains were retained. Unrelated files
and Git state were preserved.

Stop for Boss and Golden Wrench's architecture approval. Hosted CI remains on
strict hold.
