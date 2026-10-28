# OS module: source audit and platform research

27 October 2026. Baseline **`0d014e851851203e30d3b376ca99606a67d36a7a`**.
**Research proposal; no implementation or acceptance claim.**
Read with [proposed contract](DESIGN.md) and [engineering roadmap](ROADMAP.md).
Stop for independent Golden Wrench architecture review and Boss approval.

## Final completeness amendment

Boss approved the original Windows directory and resource-layout choices after
Golden Wrench review. The existing eleven-function design remains the foundation.
This bounded revision adds proposed `state_dir` and `process_id`, verifies cwd
reuse and explicitly assigns adjacent OS responsibilities. It preserves the
original platform/Runtime investigation, errors and native verification gates.
No new investigation stage or implementation is authorized. See DESIGN's final
13-function inventory and capability ownership matrix.

## Recommendation

Add one ordinary `stdlib/os.void` facade over a small private, checked native
query boundary. Discover current-user application directories, home, temp,
process executable/resources, process ID and individual environment values. Return
owned strings/scalars, never filesystem handles or borrowed native pointers. Reuse `path` for
joining/parent extraction and `io` for directory creation and file operations.

Directory getters return locations only. A returned location need not exist or
be writable. No cwd changes, directory enumeration, symlink resolution, implicit
mkdir, fallback search, process management or new scheduler is required.

Windows configuration placement and the portable resource-layout convention are
now settled by Boss; the original research's two open policy questions are
superseded. No additional owner decision has emerged from this amendment beyond
final acceptance of the amended complete scope. Native behavior, encoding checks,
GC safety and build dependencies remain engineering requirements.

## Verified foundation versus missing functionality

| Area | Actual baseline evidence | Consequence |
|---|---|---|
| Module | `stdlib/os.void` is absent. The handoff section 13 contains proposed `os.dirs`/`os.app` examples, not working API. | No existing public OS API to preserve; choose one flat module consistent with `path`. |
| Paths | [path.void](../../stdlib/path.void), lines 4–104; [filesystem.cpp](../../src/runtime/filesystem.cpp), lines 638–690. `join` uses native path composition; `normalize` is explicitly lexical; `absolute` can consult cwd. | Reuse `join`/`parent`, without implicit normalization or cwd qualification. `path.join` takes an array, not variadic arguments. |
| Complete cwd query | path.void lines 87–92; filesystem.cpp lines 668–693; [kernel.cpp](../../tests/io/kernel.cpp), lines 324–339. Empty base queries std::filesystem::current_path once; the existing deleted-cwd test checks recoverable not_found for implicit-base resolution. | `path.absolute("", "")` already supplies the portable owned cwd snapshot. Explicit-base resolution does not query cwd. Add focused source-level coverage, not another native operation or os wrapper. |
| Native PID | No source-level PID query exists in stdlib or current Runtime query operations. Existing [numeric types](../NUMERIC_TYPES.md) include i64. | Add a native scalar PID query to the same closed bridge; POSIX pid_t and Windows unsigned DWORD both fit i64 on supported targets. |
| Application arguments/status | [main.cpp](../../src/main.cpp), lines 55–68; [backend.cpp](../../src/backend/llvm/backend.cpp), lines 2146–2189. Source applications reject extra operands; legacy entry accepts one i32. Normal application returns/cleanup are already implemented. | Historical handoff section 16 is not evidence of working main([string])/argv. Assign general arguments to future process/args; preserve existing entry/CLI contracts. |
| Exclusive creation versus temp discovery | io.void lines 142–146; filesystem.cpp lines 577, 591 select CREATE_NEW/O_EXCL. | Existing io offers exclusive file creation, not a complete unique/private temporary-object convenience protocol. Keep discovery in os, future temp-object acquisition in io. |
| Encoding | [path.hpp](../../include/void/path.hpp), lines 6–13; [utf8.hpp](../../include/void/utf8.hpp); filesystem.cpp lines 96–113, 1318–1320. Native paths use UTF-8/native conversion, NUL checks and strict UTF-8 validation. Invalid native path bytes become `io.invalid_path`. | Preserve strict, lossless representation. The proposed separate `os.error_kind.invalid_utf8` can identify OS conversion failure without changing `io` errors. |
| Errors | [error.void](../../stdlib/error.void); [io.void](../../stdlib/io.void), lines 1–113. Managed base has `message`, `path`, `code`; `io.error` adds transfer/cleanup/resource fields. | Derive a narrow `os.error` from the base; do not construct irrelevant I/O progress arrays/fields. |
| Error propagation | [compiler.cpp](../../src/frontend/compiler.cpp), lines 2554–2562; [combined/execution.py](../../tests/combined/execution.py), lines 170–182. `?` requires exactly matching error types, even for derived errors sharing core.error. | Explicitly translate path-helper errors inside os; application functions combining os/io need ordinary branching and explicit error adaptation. No automatic base-error propagation is implied. |
| Missing values | [strings/tests.cpp](../../tests/strings/tests.cpp), line 30, rejects `string?`; Bible section 8 retains this boundary. Ordinary `result<string,error>` and `Ok`/`Err` work. | Missing environment is `Err(not_found)`, empty environment is `Ok("")`. No nullable-string or overload feature is needed. |
| Source wrappers | [filesystem_ops.hpp](../../include/void/filesystem_ops.hpp), [frontend/filesystem.inc](../../src/frontend/filesystem.inc), compiler.cpp lines 3074–3083. Canonical stdlib identity selects a closed operation table. | Follow that pattern for canonical `/@stdlib/os.void`, not arbitrary names or editor heuristics. |
| Independent IR checks | [ir/filesystem.inc](../../src/ir/filesystem.inc); [module.cpp](../../src/ir/module.cpp), lines 1420–1438. Canonical operation, geometry and worker restrictions are independently validated. | A new OS bridge needs the same small closed validation and transitive worker prohibition. Readonly queries are not automatically worker-safe. |
| Runtime boundary | [filesystem_abi.cpp](../../src/runtime/filesystem_abi.cpp), lines 146–181, 371–440; [temporal_abi.cpp](../../src/runtime/temporal_abi.cpp), lines 10–65. Copy native inputs before NativeSafe, return to managed execution before string allocation/publication. | Reuse `Runtime::Native`, precise output roots and `RuntimeFailure`. No FS job/resource ledger is needed for synchronous location queries. |
| GC/lifetimes | [runtime.hpp](../../include/void/runtime.hpp), `Frame`, `Slot`, `string_into`; [coordination.cpp](../../src/runtime/coordination.cpp), lines 115–121; [native_resource.cpp](../../src/runtime/native_resource.cpp). | Native API allocations get scoped native cleanup. Managed outputs remain precisely rooted through every allocating operation. No source-visible resource class or GC finalizer. |
| Async | [ASYNC.md](../ASYNC.md); [async.cpp](../../src/runtime/async.cpp). `io` has real native jobs; ordinary owner calls can enter NativeSafe synchronously. | Initial OS queries are synchronous, legal in owner async functions, with no `_async` variants. Some native lookup can block; NativeSafe permits GC cooperation, not source-task progress. |
| Globals/reload | [RUNTIME_GLOBALS.md](../RUNTIME_GLOBALS.md), lines 96–138. Runtime-owned globals retain values across compatible replacement. | An application may explicitly cache a path in an ordinary global. The OS module itself needs no singleton or persistent roots. |
| Cache | [current cache policy](../compiler-health/stabilization/CACHE.md); [backend.cpp](../../src/backend/llvm/backend.cpp), around line 2011; [CMakeLists.txt](../../CMakeLists.txt), lines 199–222. Runtime archive/build identity participates in object-cache identity. | Cache code/metadata, never queried home/env/resource values. Older global-cache prose describing only Debug is historical. |
| Executable identity | [ir.hpp](../../include/void/ir.hpp), lines 189–209, distinguishes physical debug `source_path` from application/root identity. Runtime has no general project/resource-root context. | The process executable is the JIT host under JIT, and the application under linked AOT. Do not reinterpret a source path or VFS root as an executable. |
| Tooling/install | [tooling.cpp](../../src/frontend/tooling.cpp), lines 8–31; CMakeLists.txt lines 179–193; [random LSP test](../../tests/random/lsp.py). Declaration/docs/import completion are compiler-owned; stdlib source installs recursively for compiler and tooling. | Adding documented source and the canonical bridge should need no editor-specific OS engine or list. Analysis must never query the editor host's environment. |

The prior freeze is lifted in current docs/README. Older handoff statements that
globals, formatting, `io` or `path` are still proposals do not describe this
baseline. The original `os.name`, newline and nested namespace examples are
design history. This sprint does not expand into newline conversion or process
arguments. Historical H1–H7 incidents remain unresolved; no OS research proves
their repair.

## Linux: discovery with launcher conventions intact

Use nonempty absolute `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` and
`XDG_STATE_HOME` roots;
ignore relative values as the specification requires. Defaults are HOME-relative
`.local/share`, `.config`, `.cache`, `.local/state`. Query HOME only when the selected override
cannot be used. Append the validated application ID as a literal component;
do not implement system search lists or `XDG_RUNTIME_DIR` in this API.
[XDG Base Directory Specification 0.8, sections 2–3](https://specifications.freedesktop.org/basedir/latest/).

For `home_dir`, honor nonempty absolute HOME, then use
`getpwuid_r(getuid(), ...)` when HOME is missing/empty. Grow owned native storage
on `ERANGE`; distinguish no record from a returned error code. A present relative
HOME is an error, not a cwd-relative storage decision. NSS can involve network
services, so even this read-only operation can block.
[Linux user lookup](https://man7.org/linux/man-pages/man3/getpwnam.3.html),
[glibc lookup](https://sourceware.org/glibc/manual/latest/html_node/Lookup-User.html),
[NSS](https://man7.org/linux/man-pages/man5/nsswitch.conf.5.html).

Preserving HOME/XDG is necessary for launcher-controlled locations. Flatpak
redirects XDG roots; Snap changes HOME in confined applications; AppImage portable
mode redirects HOME/config. Do not bypass those choices with physical passwd
home, `HOST_XDG_*`, or synthesized container paths. App IDs still append one
subdirectory inside the supplied base. This can add a deliberate application
subdirectory inside an already app-specific container; do not attempt to detect
and remove it.
[Flatpak conventions](https://docs.flatpak.org/en/latest/conventions.html#xdg-base-directories),
[Snap environment](https://snapcraft.io/docs/reference/development/environment-variables/),
[AppImage portable mode](https://docs.appimage.org/user-guide/portable-mode.html).

For temp, use nonempty absolute TMPDIR, otherwise `/tmp`. This is a location
policy, not a reservation or a writability check. Relative TMPDIR fails under the
proposed absolute-path contract; XDG's ignore-relative rule does not govern
TMPDIR. HOME/XDG/temp strings are not shell expressions: no expansion or trimming.
[glibc temporary-file facilities](https://sourceware.org/glibc/manual/latest/html_node/Temporary-Files.html).

Read `/proc/self/exe` into growing heap storage. `readlink` does not terminate
the string and a full buffer can mean truncation. The result follows the executed
image rather than preserving launch-symlink spelling. After unlink, Linux may
append ` (deleted)`. Preserve that descriptive snapshot; never strip the suffix
because a real filename can end that way. Missing procfs or an unavailable link
is a recoverable discovery failure. No argv/PATH/source fallback establishes the
same identity.
[readlink](https://man7.org/linux/man-pages/man2/readlink.2.html),
[proc executable link](https://man7.org/linux/man-pages/man5/proc_pid_exe.5.html).

There is no universal Linux resource layout across `/usr/bin` plus
`/usr/share`, portable games, AppImage, Flatpak and Snap. Consequently the
Boss-approved executable-parent resource root is an **explicit VOID packaging
convention**, not an inferred universal OS fact. Other layouts must supply their
asset root through ordinary application configuration/parameters. No filesystem
search or packaging detector is proposed.

## Windows: native user folders and Unicode

Use `SHGetKnownFolderPath` with current-user token and `KF_FLAG_DONT_VERIFY` for
Profile, LocalAppData and RoamingAppData. Omit CREATE and DEFAULT_PATH, preserving
configured/redirected locations without checking existence or requesting
creation. Windows defines neither a general configuration KnownFolder nor a
general desktop cache KnownFolder; InternetCache is not an application cache.
The LocalAppData/id data, RoamingAppData/id config and LocalAppData/id/cache
mapping is library policy, now approved by Boss. State reuses current
LocalAppData/id/state; there is no separate generic state KnownFolder.
[KnownFolder IDs](https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid),
[flags](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ne-shlobj_core-known_folder_flag),
[SHGetKnownFolderPath](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath).

Current Microsoft guidance explicitly requires COM on the calling thread. Use a
small per-call guard: successful `CoInitializeEx` calls, including `S_FALSE`, own
one matching `CoUninitialize`; `RPC_E_CHANGED_MODE` indicates an already
initialized incompatible apartment, not ownership of an initialization to undo.
On `RPC_E_CHANGED_MODE`, continue the lookup in the already initialized apartment
without calling CoUninitialize for the failed initialization attempt. Other
initialization failures retain their original HRESULT. Preserve the host's
existing apartment and verify STA/MTA cases natively. Always
free returned KnownFolder storage with `CoTaskMemFree`, including failure cases
where an output was allocated. Do not add a process-wide COM owner or worker.
[Known Folders overview](https://learn.microsoft.com/en-us/windows/win32/shell/known-folders),
[COM initialization](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).

Use W APIs and strict UTF-16/UTF-8 conversion. `GetEnvironmentVariableW` needs
zero-length success distinguished from ERROR_ENVVAR_NOT_FOUND: clear last error
before the call, as Microsoft's WIL does. Resize/retry after size changes, with
checked arithmetic and a bounded changed-value retry failure, not an infinite
loop. Existing-empty and absent are observably different. GetModuleFileNameW
requires growing storage when the return equals capacity; its output may retain
short or extended spelling. Do not canonicalize it.
[environment API](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablew),
[Microsoft WIL implementation](https://github.com/microsoft/wil/blob/master/include/wil/win32_helpers.h),
[module filename](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew),
[strict conversion](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte).

Recommend `GetTempPathW` initially, matching existing native-build discovery and
avoiding a new OS-version floor or optional API loader. It returns a configured,
unverified location. `GetTempPath2W` improves SYSTEM-account behavior but has
version/update requirements; no claim of secure private temp creation or SYSTEM
service support is made by this gameplay discovery API. This choice does not
modify VOID's separately secured compiler staging machinery.
Both temp APIs document their own output-size ceiling; a native limitation must
remain a checked failure, not a silently truncated path or a guessed short alias.
[GetTempPathW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw),
[GetTempPath2W](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppath2w).

Do not truncate to MAX_PATH, strip extended/UNC prefixes, case-fold, Unicode
normalize or resolve links. Keep output consumable by existing `path`/`io`
conversion and test that round trip with long extended paths. Existing generic
separator formatting is not a license to damage an extended namespace.
[Windows path representations](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation).

Packaged desktop redirection, AppContainer and UWP are different support claims.
Honor the returned OS location; do not substitute a guessed package data path.
No WinRT/Windows App SDK dependency is needed for ordinary desktop Win32.
Discovery is not a permission grant. If AppContainer-specific stores are later
required, they need an explicit contract and native evidence, not an assertion
based on desktop tests.
[Packaged desktop behavior](https://learn.microsoft.com/en-us/windows/uwp/porting/desktop-to-uwp-behind-the-scenes).

## macOS: user-domain discovery and actual bundles

A tiny Apple-only Objective-C++ adapter using Foundation is the clearest route.
Use NSFileManager user-domain Application Support and Caches discovery with
`create:NO`; config/data both use Application Support, with distinct appended
leaf directories in the proposed contract. Prefer Foundation home/temp APIs so
the application container is respected. Do not compose `HOME/Library`, inspect
container environment markers, or use physical-user lookup to escape a sandbox.
Do not use NSItemReplacementDirectory: Apple documents creation even with
`create:NO`.
[directory lookup](https://developer.apple.com/documentation/foundation/filemanager/url%28for%3Ain%3Aappropriatefor%3Acreate%3A%29),
[standard directory discovery](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/LowLevelFileMgmt/Articles/StandardDirectories.html),
[sandbox directories](https://developer.apple.com/documentation/security/protecting-user-data-with-app-sandbox).

Application Support is appropriate for application-managed data/configuration;
Library/Preferences belongs to native preferences APIs, not arbitrary config
files. Cache files must be regenerable. The native directory root is selected by
Foundation; the portable app ID selects a child, not a bundle entitlement.
[Apple filesystem directory roles](https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/FileSystemProgrammingGuide/FileSystemOverview/FileSystemOverview.html).

`NSBundle.mainBundle` describes the current executable, including the compiler
host under JIT. A non-nil bundle alone does not establish a packaged `.app`.
For `is_bundle`, require main-bundle package type APPL, `.app` bundle URL and a
file-backed executable URL; verify normal, renamed and nonstandard bundles.
For a recognized app, use its `resourceURL`, not executable-parent `Contents/MacOS`.
Do not manufacture `Contents/Resources` if resourceURL is unavailable. A plain
command-line process follows the explicit portable executable-parent convention.
[main bundle](https://developer.apple.com/documentation/foundation/bundle/main),
[resourceURL](https://developer.apple.com/documentation/foundation/bundle/resourceurl?language=objc),
[bundle layout](https://developer.apple.com/documentation/bundleresources/placing-content-in-a-bundle).

Use `_NSGetExecutablePath` with growing storage. The manual promises a path, not
canonical physical identity or an unconditional absolute spelling. Under the
proposed absolute-location contract, a relative result is `unavailable`, with no
qualification against a potentially changed cwd. Native relative-launch testing
must establish ordinary usability. Preserve symlink spelling; no `realpath`.
[dyld manual](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dyld.3.html).

Use file URL filesystem representation, not percent-escaped URL serialization.
Do not use `standardizingPath`: it can consult the filesystem and change symlink
or `/private` spelling. Contain Objective-C lifetimes in an autorelease scope;
copy outputs into owned C++ storage before leaving it. Preserve NSError domain
and code; Cocoa codes cannot be classified as errno.
[filesystem representation](https://developer.apple.com/documentation/foundation/nsurl/filesystemrepresentation?language=objc),
[standardization](https://developer.apple.com/documentation/foundation/nsurl/standardizingpath),
[NSError](https://developer.apple.com/documentation/foundation/nserror?language=objc).

## Amendment findings: state, cwd, PID and capability boundaries

### Persistent state is a missing everyday directory

Include `state_dir(id)`: Linux selects XDG_STATE_HOME, defaulting to
HOME/.local/state; Windows appends id/state to LocalAppData; macOS appends
id/state to user-domain Application Support. Reuse existing root lookup, ID
validation, strict encoding, error construction and path joining. No new native
folder provider, directory creation or automatic cleanup is necessary.
[XDG state location](https://specifications.freedesktop.org/basedir/latest/),
[Windows native bases](https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid),
[Apple Application Support](https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/FileSystemProgrammingGuide/FileSystemOverview/FileSystemOverview.html).

Data, config, state and cache are separate application roles. State includes
operational history/restart context; cache can be regenerated. The Windows/macOS
state leaf is VOID policy, not an OS retention service. Native container-aware
bases remain authoritative. Do not recreate physical-user paths or select a
different store because a launcher already supplied an app-specific base.

### Cwd already has the necessary portable implementation

At filesystem.cpp lines 668–693, an empty base requests current_path once and
preserves its error. An empty value resolves against that base. Output undergoes
existing lexical normalization and UTF-8 conversion, then io._path_absolute
returns the exact existing error type/operation. Thus `path.absolute("", "")`
already is a complete portable cwd query under current path semantics.
The standard specifies current_path as an absolute cwd result, with the
error-code overload returning empty on failure; VOID checks the error first.
[C++ filesystem current_path](https://eel.is/c++draft/fs.op.current.path).

Recommend no os.cwd alias: it adds a second name/error boundary without new
capability. Document the existing expression prominently and add focused empty-
input source coverage. Keep deleted-cwd errors and explicit-base success already
covered by tests/io/kernel.cpp lines 324–339. Cwd is process-wide mutable state,
not PWD or the project root; no query makes subsequent relative I/O atomic with
that snapshot. Mutation remains outside this readonly module.
[Windows cwd sharing](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory).

### PID requires only one scalar native query

Use getpid on Linux/macOS and GetCurrentProcessId on Windows, each documenting
successful direct ID return. `process_id() -> i64` fits the supported pid_t and
unsigned DWORD representations. Widen DWORD directly, without signed i32
truncation, GetLastError checks or string formatting. Existing libc/Kernel32
linkage suffices; this adds no process handles, provider or library dependency.
[Linux getpid](https://man7.org/linux/man-pages/man2/getpid.2.html),
[Apple getpid](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/getpid.2.html),
[Windows PID](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid).

The result identifies the executing native process, including the JIT host,
not a Runtime/application or permanent security identity. It is shared across
threads and can be reused after termination. It is not a safe filename
reservation. The native reply plan needs a distinct i64 payload, not os_code;
use the same closed operation table/ABI, with no managed string allocation just
for PID. Observe at execution time, including cache hits.
[Process identifiers and lifetime](https://learn.microsoft.com/en-us/windows/win32/procthread/process-handles-and-identifiers).
Linux IDs are scoped to the caller's process PID namespace, not globally unique
across containers.
[Linux PID namespaces](https://man7.org/linux/man-pages/man7/pid_namespaces.7.html).

### Runtime/session directories do not have a portable substitute

XDG_RUNTIME_DIR requires a user-owned 0700 local IPC location with login-bound
lifetime and suitable filesystem behavior. A path-only getter cannot certify
these properties. Exclude a portable runtime/session getter; do not manufacture
an alternative from temp, cache, state or a guessed UID path.
[XDG runtime contract](https://specifications.freedesktop.org/basedir/latest/).

Simultaneous logins share the per-user directory; it is not session-private.
Systemd's login integration describes removal after the final concurrent
session, reinforcing the difference from persistent state and ordinary temp.
[pam_systemd](https://www.freedesktop.org/software/systemd/man/latest/pam_systemd.html).
Windows session IDs/object namespaces and Darwin temporary locations establish
different contracts; neither is an equivalent portable filesystem service.
[Windows session namespaces](https://learn.microsoft.com/en-us/windows/win32/termserv/kernel-object-namespaces).

Readonly env inspection of XDG_RUNTIME_DIR remains possible, without validation
or fallback promises. A future Linux runtime/IPC API needs explicit security and
lifecycle design. This exclusion preserves the boundary instead of weakening
XDG guarantees to fit a string-returning portable API.

### Adjacent responsibilities are explicit, not silently implemented

DESIGN's capability matrix classifies included os queries, existing path/io/
console/time/random facilities, future process/system/args/log responsibilities
and platform-specific exclusions. No public setters, child management, argv,
user/security identity or detailed system inventory exist in this initial
module. General application argv is also absent from actual current entry
adapters, despite older illustrative handoff discussion. Normal return/status
and legacy entry(i32) remain implemented.

Environment/cwd mutation and forced process control would change shared process
state/lifecycle; they need a separate process contract. Detailed version queries
also have platform caveats: Windows version results can depend on the application
manifest, so platform-family identification is not a version/feature probe.
[Windows version behavior](https://learn.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version).
Existing exclusive io.create_new is a useful primitive but not proof of a secure
temp-directory/file helper. No future module is created by this ownership audit.

## Integration dependencies and genuine risks

1. **One native bridge, not a second filesystem engine.** OS-native APIs cannot
   be called from ordinary source today. A closed `os` query operation is justified;
   expanding arbitrary FFI, the file-resource state machine or the async executor
   is not. A small reply needs status, native code/domain, text, a flag and an
   i64 payload for PID; reuse
   the temporal/string-output pattern or precise small aggregate layout. Do not
   reuse `io._reply`'s ten fields/arrays simply to avoid a narrow bridge.
2. **Foreign environment mutation.** Copy native environment bytes immediately,
   before managed allocation. A readonly VOID API cannot synchronize arbitrary
   native `setenv`/`unsetenv` by an embedding host. Require such mutation to be
   quiescent with OS queries; no false thread-safety claim or ineffective private
   mutex. Privileged/setuid execution is not a supported environment-trust model.
   [glibc environment access](https://sourceware.org/glibc/manual/latest/html_node/Environment-Access.html).
3. **Allocations and GC.** Copy managed inputs before NativeSafe, then make no
   managed borrows there. Reenter managed execution before `string_into`; root
   text/domain and error construction through all allocations. Native allocation
   failures remain `RuntimeFailure`, not fabricated `Err(not_found)` or partial
   successful strings. A failed query has no persistent resource to drain.
4. **Two link paths.** CMake Runtime consumers and `voidc build` must both receive
   Windows shell/COM dependencies and Apple Foundation. CMakeLists.txt line 90
   defines Runtime; [NativeBuild.cmake](../../cmake/NativeBuild.cmake), lines 5–47,
   records direct native flags consumed by [native_build.cpp](../../src/native_build.cpp),
   lines 178–194. Updating target linkage alone misses installed/relocated AOT.
   Apple-only OBJCXX setup must match C++20, deployment target, arch, SDK, RTTI and
   sanitizer requirements; preserve LLVM/Clang/LLD 19.1.7. Existing Runtime archive
   digest already covers its new object; do not add a parallel cache identity.
5. **No hidden environment cache.** Query at execution time, not source analysis,
   import time, object emission or editor completion. An explicitly initialized
   application global is an application-owned snapshot and intentionally persists
   across compatible reload. Environment values themselves are not code-cache
   keys, serialized data or automatic restart triggers.
6. **Process architecture.** Return the architecture of this executing binary,
   not physical hardware. Rosetta x86_64 remains x86_64; Windows emulation remains
   the process target. No OS-release parsing, CPU-feature manager or native-host
   architecture query is necessary.
   [Apple translation semantics](https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment),
   [Windows process/native distinction](https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-iswow64process2).
7. **1 MiB stacks/performance.** Use growing heap buffers, bounded native-size
   arithmetic/retries and iterative byte validation. No large stack PATH_MAX
   arrays, recursive directory search, environment enumeration or repeated
   existence checks. No measured OS-module bottleneck exists because the module
   is absent; do not add memoization or a performance framework without evidence.

## Research evidence and limits

Source inspection and independent Linux/Windows/macOS documentation audits were
reconciled into the linked contract. No production code, stdlib modules or build
configuration were changed; no compiler/native tests, tracing, builds or hosted
CI ran for this research. Documentation/link/scope checks are recorded separately
from engineering acceptance. Official APIs establish design feasibility, not
verified behavior of a nonexistent VOID implementation.

Original research validation, preserved as historical evidence: all **177 local
links** across these three documents and the internal index resolved; Markdown
fences/whitespace and the **11-query API inventory** passed. Scope checks found
exactly these three new files plus the
docs/README index edit, with HEAD unchanged at the baseline. No production test
result is claimed by those documentation checks.

Final amendment validation: **181 local links**, **13 public functions** and
**25 capability ownership rows** pass the documentation/inventory checks;
Markdown fences/whitespace and allowed-file/unchanged-HEAD checks also pass.
Independent Linux/Windows review found one stale active count, corrected from
eleven to thirteen; the original 11-query receipt above remains historical.
No production tests, builds, native executions, tracing or hosted CI were run.

The remaining native uncertainties are COM import libraries/apartment behavior,
strict long Unicode path round trips, macOS Foundation/OBJCXX integration and
relative-launch/bundle/sandbox behavior. Linux environment fixtures cannot prove
Flatpak/Snap packaging, Windows containers or macOS App Sandbox. ROADMAP makes
each native claim conditional on real platform execution. Unavailable native
access must be reported as an acceptance limit, never converted to a pass.
