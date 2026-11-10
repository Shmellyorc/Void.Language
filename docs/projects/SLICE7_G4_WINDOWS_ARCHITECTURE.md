# Slice 7 G4-W Windows loader architecture decision

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`.
**Architecture hold.** No evaluated option has yet established
pre-initializer authority for arbitrary application-bundled imports
using supported Windows contracts. The smallest next profile to
qualify is a protected launcher with independently admitted,
import-free provider images and an explicit host-owned dependency
table. That profile still needs a proof of exact executable image
selection before the first load. This checkpoint leaves the
[accepted G4-L implementation](SLICE7_G4_LINUX.md) and
[G3 teardown rules](SLICE7_G3_FINAL_CLOSEOUT.md) in place.

## Root cause and native controls

Windows binds the top-level absolute pathname and each dependent
PE import through different decisions. The PE import identifies
a DLL by name. Side-by-side redirection and loaded-module reuse
can take precedence over the directory search used for imports.
An approved provider and a held copy of the approved dependency
therefore do not establish the owner of its resolved IAT slot.
Windows runs a newly selected DLL's process attach code before
the executable load returns.
[Microsoft DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order) ·
[DLL initialization](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain) ·
[Original Windows checkpoint](SLICE7_G4.md#windows-10-x64-checkpoint).

The native replay ran on Windows 10 Home x64 build 19045 with the
retained pinned Clang 23.1.4 toolchain. It rebuilt the approved and
alternate fixture files from the exact checkpoint source hashes.
The new observer read the live import address table directly and
queried the owner of that address. It recorded file IDs from the
held files, device paths and `MEM_IMAGE` from the mapped code.
The held dependency refused a write-open with sharing error 32.
The earlier checkpoint additionally compared image and file mappings
with an undocumented native oracle. That remains observational
evidence rather than a production API dependency. The final native
runner, executables and DLLs ran from local `C:` paths. The share
carried source, the transfer bootstrap and the output receipts.
[Native runner and binary hashes](evidence/slice7-g4-windows/native/native-results.json) ·
[PE imports, exports and manifest resources](evidence/slice7-g4-windows/native/pe-facts.txt) ·
[Independent IAT reader](evidence/slice7-g4-windows/probes/loader_probe.cpp).

| Control | Observed result |
| --- | --- |
| [Clean process](evidence/slice7-g4-windows/native/probe-clean.txt) | The approved top-level image and dependency owned the expected addresses. The alternate initializer marker was absent. |
| [Loaded-name collision](evidence/slice7-g4-windows/native/probe-collision.txt) | The approved main mapped. The IAT reused the previously loaded same-name alternate even though the approved dependency was held. |
| [Late same-name load](evidence/slice7-g4-windows/native/probe-late.txt) | The snapshot saw neither a loaded dependency name nor a redirect. The probe then loaded the competitor before loading the provider, and the provider's IAT used the competitor. This was a controlled ordering change, with no claim of a discovered concurrent race. |
| [Thread activation redirect](evidence/slice7-g4-windows/native/probe-thread.txt) | No dependency name was loaded beforehand. The approved main mapped and the alternate acquired the IAT slot. Its initializer marker was already `1` at the first post-load observation. |
| [Push a clean process default](evidence/slice7-g4-windows/native/probe-thread-default.txt) | The clean process default suppressed the thread override in this control. Both mappings matched the held images. |
| [Push a redirected process default](evidence/slice7-g4-windows/native/probe-process-default.txt) | The executable had the redirect embedded in its default manifest. Pushing that default left the alternate import and initializer in effect. |
| [Preload approved, then activate the redirect](evidence/slice7-g4-windows/native/probe-preload-thread.txt) | The preloaded good image matched its held file. The later import still bound the alternate, whose marker was `1`. |
| [Thread guard](evidence/slice7-g4-windows/native/probe-thread-deny.txt) and [process-default guard](evidence/slice7-g4-windows/native/probe-process-deny.txt) | A query for the exact dependency name found the active redirect. The guard stopped before the executable load. Both provider and alternate were absent. |
| [Data-only image view](evidence/slice7-g4-windows/native/probe-data.txt) | `SEC_IMAGE_NO_EXECUTE` from the held handle exposed the initial marker value `0`. The executable module was absent. A separate ordinary load ran the initializer and changed the marker to `1`. |
| [Forwarded export](evidence/slice7-g4-windows/native/probe-forward.txt) | Loading the forwarder left the alternate absent. `GetProcAddress` resolved its export into the alternate and the marker was `1` on return. The probe never called the returned address. |
| [Explicit table](evidence/slice7-g4-windows/native/probe-explicit.txt) | Two known import-free, no-entry images loaded under the clean probe and owned the selected exports. A caller-supplied immutable table supported a C record, byte span, opaque token and one terminal release. |
| [Fresh startup with approved DLL](evidence/slice7-g4-windows/native/startup-good.txt) and [fresh startup with alternate DLL](evidence/slice7-g4-windows/native/startup-bad.txt) | Both child executable copies had the same SHA-256. The parent matched each suspended child image pathname to its held executable. After resume, first user entry reported `42` with the good dependency and `121` when the alternate had already initialized. |

The forwarder PE had zero import, delay import and TLS directories
and a zero entry-point RVA. Its export table still named
`void_g4_probe_dep.g4_dependency`. A candidate must inspect export
ownership and reject unapproved forwarders **before** calling
`GetProcAddress`. The Microsoft loader documentation and Raymond
Chen's forwarding explanation describe the relevant behavior.
A firsthand Python/Boost report describes two same-named
`python25.dll` copies loading through distinct manifest paths. That
report is related field evidence. The IAT and initializer probes above
reproduce the exact failures at issue on this Windows 10 target.
[PE export table format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#export-address-table) ·
[Microsoft forwarding explanation](https://devblogs.microsoft.com/oldnewthing/20080204-00/?p=23593) ·
[Firsthand Python DLL report](https://stackoverflow.com/questions/2147729/dll-file-loaded-twice-with-dll-redirection-through-manifest).

## Controlled launch and implicit imports

The launcher must own the executable identity, expected embedded
manifest, deployment directory and ancestor protection, input plan,
initial environment, working directory and any inherited handles.
Before launch it must verify the executable's PE imports, delay
imports and manifest inputs. Startup may import only the approved OS
surface. Application dependencies need to stay out of startup
imports so an admission guard can run first. The tested launcher,
observer and default-manifest observer declared only `KERNEL32.dll`
as a PE import. The intentionally unsafe child declared
`KERNEL32.dll` and the test dependency. Those are direct import
facts for these executables. Windows owns the loader, `ntdll`,
KnownDLLs, API-set mapping, system forwarders, application
compatibility policy and any later OS-mediated loads.

`CreateProcessW` with an absolute executable and
`CREATE_SUSPENDED` provides a launch and observation boundary. The
child still performs normal loader startup when resumed. Matching
its executable image does not authenticate all DLLs it will use.
`SetDefaultDllDirectories` restricts the process's default search
directories. It does not create a separate module namespace or an
atomic dependency reservation. `ActivateActCtx(NULL)` selects the
process's original default. Its safety depends on the manifest and
applicable OS policy selected before launch. The exact-name
`FindActCtxSectionStringW` guard prevented the two tested redirects.
It is a targeted check of the current activation context. It gives
no process-wide proof about DLL-provided manifests or later changes
to the loaded-module list.
[CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw) ·
[Search directory policy](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories) ·
[Activation context](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-activateactctx) ·
[Activation lookup](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-findactctxsectionstringw).

The proposed launcher must use a protected local staging root with
single-assignment canonical names, pinned files, checked directory
ancestry and access that excludes untrusted writers. It must prove
which executable manifest is effective, including embedded and
external manifest policy, DotLocal, package behavior and machine
redirection. It must either prevent loader-state changes between
admission and load or have an OS policy that rejects unapproved
executable images before initialization. A per-Runtime lock cannot
arbitrate loader calls from other components. Standard executable
`LoadLibraryExW` accepts a pathname and reserves its file-handle
argument. Its supported interface does not provide a held-file
dependency graph or a pre-initializer veto over resolved imports.
Post-load mapping and IAT checks still detect violations, after
initializer side effects may have occurred.
[LoadLibraryExW contract](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw) ·
[DotLocal policy](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-redirection).

Possible managed deployments can evaluate an enforced OS image
allowlist with an explicit trusted OS scope. The per-process image
load and signature mitigations have their own limited policies,
such as remote/low-label restrictions and Microsoft or Store
signing. They do not express this application's list of exact DLL
hashes. An App Control deployment would be a separate privilege,
edition and installation decision. It would also need to prove
policy activation and bind its rule identity to the host's exact
artifact approvals. Dynamic-code prohibitions require separate
JIT compatibility review. This checkpoint changed no machine
policy and qualifies none of these alternatives.
[Image-load mitigation](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_image_load_policy) ·
[Signature mitigation](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_binary_signature_policy) ·
[App Control rule types](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/select-types-of-rules-to-create) ·
[Dynamic-code mitigation](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_dynamic_code_policy).

## Dependency-free provider images and explicit dispatch

The proposed first profile combines the controlled launch contract
with no static provider imports. It also rejects delay imports,
forwarded application exports, nonzero entry points, TLS and
unsupported PE load metadata. All images, import tables and export
RVAs are inspected from held files before loading. A
`SEC_IMAGE_NO_EXECUTE` view supports that inspection and does
not provide an executable module. Each permitted image must then
pass its own pre-execution selection proof. A replacement image
can contain entry points even if the approved image has none.
`DONT_RESOLVE_DLL_REFERENCES` and manual mapping provide no
qualified route to an initialized executable binding for this
profile.
[CreateFileMappingW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappingw) ·
[PE directory and export definitions](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) ·
[LoadLibraryExW modes](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw).

The host would resolve each individually admitted module, validate
exact owned addresses and publish a complete immutable table to
one Runtime. The thunk would receive that authority from the host.
It would never search DLL names. The synthetic table is passed
explicitly and demonstrates that Windows x64 target-compiled C
can carry a by-value record and call-scoped buffer without linking
a provider import. Its opaque token and terminal release are
standalone probe operations. Production needs an internal binding
context that preserves the existing descriptor and typed IR ABI,
persists through generations and is available to terminal release
on the shutdown thread. A global mutable table or a thread-local
lookup that disappears at teardown would violate G3 ownership.
The checkpoint did not change the existing transport.
[Explicit C fixture](evidence/slice7-g4-windows/probes/explicit.c) ·
[Windows x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention).

The initial synthetic provider profile can have zero OS imports
within the provider closure. Windows and the launcher still depend
on their trusted OS loader and base system libraries. API-set
contracts are OS aliases whose host binaries can vary by system.
An eventual policy must name which OS interfaces and code owners
are trusted and how their redirects and forwarders are qualified.
Windowing, graphics and driver loads would extend that boundary.
For example, the Windows OpenGL runtime can select an installable
client driver through platform policy. No particular raylib
artifact or driver closure was audited.
[API-set mapping](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation) ·
[Windows OpenGL client driver loading](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/loading-an-opengl-installable-client-driver).

## Separate native helper process

A fresh helper gives each provider or Runtime a separate address
space and loaded-module list. Its executable, default manifest,
roots and OS-only bootstrap still require the protected launch
contract. The child startup control proves that a separate process
with an application PE import can initialize an alternate before
its first protocol message. An authenticated handshake must occur
only after closure admission. The parent must publish no binding or
start eager VOID globals until the handshake succeeds.

- **Launch and restart.** Launch the attested helper by absolute
  installed path with explicit environment and working directory.
  Grant only the intended IPC handles. Handshake over a restricted
  channel with a protocol version, exact closure and descriptor
  identities, and a fresh session identity. A restarted helper is a
  new session. Old resource and pending-call identifiers remain invalid.
- **Calling ABI.** Send bounded ordinal, size and scalar fields.
  Marshal G2 records from descriptor-defined fields into the
  helper's target-native by-value carrier. An out-of-process buffer
  borrow would copy input bytes into helper-owned storage and return
  only the validated written prefix. Commit it in the parent with the
  existing version/overlap rules. Raw addresses, managed references
  and padding bytes cannot serve as IPC authority.
- **Resources and teardown.** Hold actual pointers and release
  authority in the helper ledger. The parent owns opaque session
  and generation-scoped capabilities. The helper enforces thread
  ownership, drains calls and closes on the admitted thread before
  unmapping. A crash invalidates capabilities and pending calls.
  OS process cleanup cannot guarantee a provider's terminal action
  or undo external side effects. Reporting and retry semantics need
  separate approval.
- **Windowing and callbacks.** Put the event loop, window, graphics
  context and owner-thread calls in the helper. Input and redraw
  events need a bounded delivery contract. Reentrant callbacks and
  message pumping change ordering and failure behavior. Those remain
  deferred. IPC latency and per-call rendering cost require native
  measurement before a raylib decision.
- **AOT and installation.** Locate the approved helper and closure
  relative to the protected relocated executable. The installer and
  launcher must bind the expected helper and bundle identities before
  start. Optimized and unoptimized code can share the checked IPC
  bridge. There is no qualified installed helper, IPC ABI, or
  graphics/system-library allowlist in this checkpoint.

## Comparison and recommendation

| Alternative | Pre-initializer guarantee and supported API status | JIT, AOT and G2/G3 consequences | Graphics, deployment and remaining failures |
| --- | --- | --- | --- |
| Controlled in-process launcher with implicit imports | The tested guard blocks the exact redirects. Supported search and activation APIs leave the general dependency selection transaction unproved. | Least ABI change if proven. Normal and O2 JIT and relocated AOT could reuse the existing carrier and ledger. | Closest to ordinary DLL deployment. Must account for bundled imports, module cache, side-by-side defaults, forwarders and OS driver loading. |
| Protected launcher with import-free explicit table | Removes implicit application name binding and the tested forwarder path if those features are rejected before mapping. Each executable image still needs a proven selection boundary. | Keeps descriptor and target C shapes. Requires a private binding context, same Runtime/generation ownership and shutdown release proof. AOT needs the same protected admission after relocation. | Smallest next synthetic profile. Existing DLLs with imports or entry points require a different profile or a reviewed rebuild. No general graphics compatibility follows. |
| Separate native helper | Can constrain launch state and isolate failures. It still needs the same pre-initializer image admission. Process separation alone failed the startup control. | Shared checked IPC bridge can serve both JIT modes and AOT. G2 copies, G3 token lifetimes, affinity, session failure and teardown require new contracts. | Highest deployment and latency cost. Natural ownership for a native UI thread, with event routing, restart, and graphics driver policy unresolved. |

**Recommended next checkpoint** is the protected-launch and
explicit-table profile for the synthetic closure. It removes the
observed implicit application dependency mechanisms while keeping
the C call and resource model in-process. The outstanding gate
remains a supported pre-execution guarantee that a selected
absolute image name resolves to the held, approved artifact.
The current probes establish behavior of a known clean fixture
and show how it fails outside that condition. They leave
production Windows loading on hold.

### Required follow-up and open decisions

1. Freeze the trusted Windows host and installer identity, token
   model, approved OS build policy, protected root ACLs and relocation
   rules. Test junction, reparse, hardlink, rename, same-name
   startup, drive/path aliases, DotLocal, external and embedded
   manifest and package-state negatives. Use a sentinel that can
   distinguish prevention from detection after initialization.
2. Qualify the effective default, thread and module activation
   contexts, plus the proof of no independent loader state changes
   between validation and load. If supported APIs cannot establish
   exact pre-execution selection, decide whether managed OS image
   enforcement is an acceptable deployment requirement. Preserve
   the stop if neither route qualifies.
3. Freeze bounded offline PE validation and export ownership,
   including forwarders, normal and delay imports, TLS, entry
   points and unsupported loader metadata. Include the forwarder
   initializer and the preloaded-approved override controls in
   the negative suite.
4. Review the private immutable table and release context before
   integrating either JIT mode or independently linked AOT.
   Requalify resource close, shutdown thread, refused acquisition,
   reload and module retirement. Establish the same admission after
   relocated install with the source workspace absent.
5. Keep Windows OS libraries and graphics drivers under a
   separately approved explicit policy. If moving to a helper,
   decide the IPC framing, owner thread, restart and loss semantics
   before attempting any raylib or callback integration.

## Evidence and preserved failures

All 14 assertions matched their expected positive or negative
outcomes. This was standalone Windows 10 probing. It did not
qualify the compiler, AOT integration, production loader, graphics
or callbacks on Windows. The native inputs and logs are hashed.
The first build exposed the freestanding MSVC `_fltused` marker
needed for the record fixture. The next build's object-free
forwarder command emitted an import library without a DLL.
The final replay supplied an ordinary object and verified the
actual PE forwarder. The original compile/link diagnostics and
the corrected command receipts are retained.
[Initial link failure](evidence/slice7-g4-windows/native/original/link-explicit.txt) ·
[First replay PE diagnostic](evidence/slice7-g4-windows/native/first-replay/pe-facts.txt) ·
[Corrective native replay](evidence/slice7-g4-windows/native/native-results.json) ·
[Evidence hashes](evidence/slice7-g4-windows/native/preserved-evidence.json).

The layered inventory preserves all 1,737 previous paths, including
the original G4-W failure evidence and the complete G4-L layer.
The patch contains the standalone probes and evidence helpers.
HEAD and the staged index match the baseline.
[Exact probe patch](evidence/slice7-g4-windows/probe.patch) ·
[Layered Git inventory](evidence/slice7-g4-windows/inventory.txt).

## End-of-task temporary workspace cleanup

After archiving the native evidence, Windows cleanup verified 42
retained files in the final probe tree. It removed eight exact
scratch paths, including the two redundant local transfer scripts
and the duplicate child staging files and directories from the
failed inspection replay. The native probe harness and both failed
build trees remain available for independent reproduction.

Windows 10 `C:` free space was 24,744,894,464 bytes (23.045 GiB)
before cleanup and 24,745,115,648 bytes (23.046 GiB) afterward.
The host transfer share remains available with the raw native
evidence and reproduction inputs. There were no separate host build
scratch files to remove. Prior artifacts, personal files, installed
toolchains, reusable build trees and Git state were untouched.
[Exact cleanup receipt](evidence/slice7-g4-windows/native/cleanup-results.json).

Stop for Golden Wrench and Boss's design approval.
