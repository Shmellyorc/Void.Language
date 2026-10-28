# OS module: proposed public contract

27 October 2026. Baseline **`0d014e851851203e30d3b376ca99606a67d36a7a`**.
**Proposed contract for Wrench/Boss review; no implementation authorization.**
[Research/evidence](RESEARCH.md) · [complete sprint](ROADMAP.md)

**Final completeness amendment:** Boss approved the original Windows placement
and application-resource policies. Those choices are settled below. The original
eleven functions and their contracts are retained; `state_dir` and `process_id`
extend the proposed initial API to **thirteen functions**. Cwd discovery reuses
`path`; no native cwd duplicate or portable runtime-directory promise is added.
This amendment is research/design, pending final architecture review and approval
of the complete engineering scope, not permission to implement now.

## Public surface

One flat `use os` module, consistent with current `path`/`io`. These signatures
are proposed declarations, not source currently available to import:

```void
func data_dir(app_id: string) -> result<string, error>
func config_dir(app_id: string) -> result<string, error>
func cache_dir(app_id: string) -> result<string, error>
func state_dir(app_id: string) -> result<string, error>
func home_dir() -> result<string, error>
func temp_dir() -> result<string, error>
func executable_path() -> result<string, error>
func resource_dir() -> result<string, error>
func env(name: string) -> result<string, error>
func platform() -> string
func architecture() -> string
func process_id() -> i64
func is_bundle() -> result<bool, error>
```

No function overloads, nullable strings, nested static namespaces, environment
setters, directory creation flags or source-language syntax changes. `platform`
returns exactly `linux`, `windows` or `macos` on supported targets.
`architecture` describes the running binary: `x86_64`, `aarch64`, `x86`, `arm`,
or `unknown` for an unrecognized build target. These names deliberately normalize
the public spelling of macOS arm64 to `aarch64`. They do not expose CPU features,
physical-machine architecture, OS versions or emulation detection.

Platform/architecture/is_bundle are synchronous observations; managed allocation
can still use normal Runtime failure behavior. `is_bundle` returns Ok(true) only
for the current process in a recognized macOS APPL `.app` bundle with executable
URL; Ok(false) for confirmed plain command-line programs and other platforms.
It is a packaging observation, not signature/entitlement/access validation.
Native lookup invariant failures retain Runtime failure handling; ordinary
inability to classify the macOS host is Err(unavailable), not a false answer
authorizing fallback.

## Directory placement

`id` below is one validated application-ID component. Native roots mean current,
potentially redirected or sandbox-specific locations, not hardcoded profile
strings. Lowercase `config`/`cache`/`state` below are literal VOID-selected leaves.

| Getter | Linux | Windows | macOS |
|---|---|---|---|
| `data_dir(id)` | selected XDG_DATA_HOME / id | LocalAppData / id | Application Support / id |
| `config_dir(id)` | selected XDG_CONFIG_HOME / id | RoamingAppData / id | Application Support / id / config |
| `cache_dir(id)` | selected XDG_CACHE_HOME / id | LocalAppData / id / cache | Caches / id |
| `state_dir(id)` | selected XDG_STATE_HOME / id | LocalAppData / id / state | Application Support / id / state |
| `home_dir()` | HOME, then current real-user passwd fallback | KnownFolder Profile | Foundation current application home |
| `temp_dir()` | TMPDIR, otherwise `/tmp` | GetTempPathW | Foundation temporary directory |

Data is durable application-managed content such as local saves. Config is
settings; cache is regenerable. Windows local saves do not roam automatically;
the approved config location may roam under Windows profile policy. macOS
config uses an ordinary file directory, not Library/Preferences. The macOS config
child and Windows cache child keep distinct roles within shared native roots.
No universal guarantee of backup, synchronization, retention or available space.

State is persistent operational history/restart context, such as logs, recent
items, window/layout or undo history; it is distinct from important user saves,
settings and disposable cache. It is not login-session IPC storage. `state_dir`
does not rotate logs, delete files on exit, clear cache, restore a session or
guarantee durability. Logging/retention policy belongs to the application or a
separately approved future `log` module. Windows/macOS `state` children are VOID
placement policy, not native special folders with special retention guarantees.
The `config`, `cache` and `state` children inside a data root are reserved by this
layout convention; applications should not use those names for unrelated files.

Getters do not create or verify directories, test access, change cwd, resolve
links, read content, request entitlements or alter permissions. Native directory
services may read system metadata; “read-only” does not mean nonblocking or free
of OS calls. A path is a snapshot, not a capability or identity handle. Its later
creation/use belongs to `io`, which reports later permission/race failures.

Returned directory/executable paths are nonempty, NUL-free, strict UTF-8 and
absolute under the host path rules, or the operation fails. Do not expand `~`,
variables or shell syntax; trim whitespace; change case; normalize Unicode;
collapse `..`; remove extended prefixes; or resolve symlinks. Append only the
validated literal component using existing `path.join`. Existing public path
separator conventions apply; equivalent separator formatting must retain the
native namespace and work through `io`. No promise of a canonical spelling or
trailing separator. Applications needing lexical cleanup explicitly use `path`.

### Linux root selection

For each XDG getter independently:

1. Nonempty absolute selected XDG variable: use it. Do not consult HOME first.
2. Missing, empty or relative selected XDG variable: ignore it and use the
   HOME-based default `.local/share`, `.config`, `.cache` or `.local/state`,
   corresponding to data, configuration, cache or state respectively.
3. For HOME, nonempty absolute value wins. Missing/empty uses
   `getpwuid_r(getuid())`; missing record is `unavailable`. A present relative
   HOME is `invalid_path`. A malformed absolute native encoding is
   `invalid_utf8`, with no alternate destination.

Classify a native XDG override's relative/absolute form before converting it:
even a malformed relative override is ignored, whereas an absolute malformed
one fails. No lossy repair or locale decoding. Missing/empty TMPDIR selects
`/tmp`; present relative TMPDIR is `invalid_path`, malformed encoding is
`invalid_utf8`. These HOME/TMPDIR rules are VOID policy, not XDG requirements.

### Application identifiers

The ID is a portable directory token, not a path or authenticated application
identity. Proposed exact rules:

- 1–255 ASCII bytes; first and last byte are lowercase `a`–`z` or `0`–`9`.
- Other bytes may additionally be `.`, `_`, `-`; dot-separated pieces cannot
  be empty. Uppercase is rejected, never converted.
- Reject Windows device basenames `con`, `prn`, `aux`, `nul`, `com1`–`com9`,
  `lpt1`–`lpt9`, including when the stem precedes the first dot.
- Thus no separators, colon, NUL, spaces, Unicode, drive/UNC/extended prefixes,
  `.`/`..`, leading/trailing dot or traversal. IDs need not contain a dot; reverse
  DNS such as `com.studio.game` is recommended, not a strict DNS grammar.

Validate before native discovery, consistently on every platform. Failure is
`invalid_argument`. The length bound protects a portable leaf, not the total
path length or every possible filesystem. IDs differing only by unaccepted case
do not acquire separate directories. OS case/Unicode behavior in parent roots
remains native. Two apps choosing the same ID intentionally collide; no random
suffix or automatic migration. The ID does not select a sandbox, bundle ID,
Windows package or another user's profile.

## Executable and application resources

`executable_path()` means the current **native process image**:

- Linux: `/proc/self/exe`, checked for readlink truncation.
- Windows: GetModuleFileNameW for the main module, checked for truncation.
- macOS: `_NSGetExecutablePath`, checked for required buffer size.

It is not the source entry, original launch command, VFS identity or a stable
security reference. On Linux the kernel's ` (deleted)` spelling is preserved;
the returned path may no longer be usable. On macOS symlink spelling may remain.
No procfs/relative native result/unavailable image means a recoverable failure;
no argv, PATH, cwd or source fallback. In particular macOS relative output is
`unavailable` rather than guessed against a later cwd.

Boss-approved `resource_dir()` convention:

- Recognized macOS `.app`: mainBundle.resourceURL; missing/non-file/unrepresentable
  resource location fails. Do not substitute `Contents/MacOS` after bundle lookup
  fails.
- Plain macOS, Windows and Linux executable: existing `path.parent` of the
  process executable path.

The internal macOS classification must distinguish confirmed plain executable,
recognized application bundle and failed discovery. `resource_dir` must not use
an unsuccessful bundle lookup as permission to fall back to executable parent.

This is a documented default **portable executable layout**, not universal Linux
installation discovery, a Windows package-root query or a resource search.
Resources are normally read through `io`; the getter does not promise read-only
permissions or choose writable saves. No fallback to current directory, source
directory, `/usr/share`, neighboring folders or packaging environment guesses.

Normal/O2 JIT both execute inside `voidc` or an embedding host. Executable,
bundle and resource queries therefore describe that host. Linked AOT describes
the actual application. This difference is deliberate and must appear in hover
documentation. Development/engine hosts and installations using another layout
pass their asset root as an ordinary application parameter or configuration
value. No automatic source-path leak, compiler-root state, reserved environment
override or new Runtime application-context subsystem is introduced.

## Current working directory: reuse `path`

No `os.cwd()` is proposed. Existing
`path.absolute("", "") -> result<string, io.error>` already snapshots cwd once,
qualifies the empty path against it and applies existing host lexical
normalization/output conversion. `path.absolute("")` is the same call through
the default empty base. No second native operation, wrapper or error translation
is needed merely to rename a complete portable query.

This returns an owned absolute cwd snapshot or the existing `io.error`, including
native lookup failures and strict path conversion failures. It does not consult
PWD, identify the source project, mutate cwd or guarantee that a later relative
I/O operation uses the same directory. Host threads can change the process cwd;
retain the returned base and resolve subsequent paths with explicit
`path.absolute(value, base)` when one consistent base is needed. Native cwd
spelling and the existing lexical normalization apply; no stronger physical
identity or symlink-spelling guarantee is introduced.

## Minimal process identity

`process_id() -> i64` returns the current native process identifier using
`getpid()` on Linux/macOS and `GetCurrentProcessId()` on Windows. Both facilities
document success without an error sentinel; this is an ordinary nonfallible
scalar query, not `result`, a string, or a process object. Directly widen native
pid_t/DWORD into i64; never route a Windows unsigned DWORD through signed i32.
The full 32-bit unsigned Windows range fits in the existing signed i64 type.

The ID is shared by application Runtimes/threads in the same process and can be
reused after process termination. On Linux it is observed in the caller's PID
namespace, not a globally unique host/container identifier. JIT/O2 returns the
compiler/embedding host PID; linked AOT returns the application's PID. Observe
it at execution time; do not embed a compiler/editor PID in object code or cache.

No authentication, user identity, permission, executable identity, thread ID,
parent PID, process handle, permanent application ID, unique filename guarantee
or process-management authority follows from the number. Native resource
ownership and current owner/worker restrictions remain unchanged. The shared
closed bridge gets a proper i64 output channel; PID is not stuffed into os_code
or string output and should not allocate managed strings merely to return it.

## Runtime/session directories

No portable `runtime_dir` or `session_dir` is in the initial API. Linux
XDG_RUNTIME_DIR is a per-user login-lifetime IPC location, shared across concurrent
logins, with ownership, mode 0700, local-filesystem and cleanup requirements.
Ordinary temp/state/cache locations and Windows session namespaces or macOS temp
directories do not establish that contract. No `/tmp` substitution, synthesized
`/run/user/<uid>`, fallback creation or warning-on-import is proposed.

Linux callers can inspect `os.env("XDG_RUNTIME_DIR")`; that only returns the
configured string and does not certify ownership, permissions, lifetime or safe
IPC placement. A validated runtime/IPC facility is explicitly platform-specific
future work requiring its own security contract. Excluding it from this portable
surface must not be described as full XDG_RUNTIME_DIR support.

## Readonly environment lookup

`env(name)` queries the process environment at call time. A present empty value
is `Ok("")`; absence is `Err(not_found)`. Failure is recoverable, never an implicit
panic or empty-string substitute. This uses existing `result`, because ordinary
`string?` is currently unsupported.

Reject empty names, embedded NUL and `=` with `invalid_argument`, including
Windows hidden drive-entry names. Otherwise accept strict UTF-8 names. Windows
lookup is case-insensitive under the OS API; POSIX lookup is case-sensitive.
Values may contain newlines, spaces or `=`; retain their exact content. Invalid
POSIX bytes or unpaired native UTF-16 become `invalid_utf8`, never replacement
characters. Do not expand references, enumerate all variables, log values or
echo secrets in error messages.

Return an owned immutable string. Native pointers must not survive a native
lookup or cross managed allocation. Host/native environment mutation must be
quiescent with calls; readonly VOID access does not make foreign concurrent
mutation safe. Windows size changes use a finite retry bound (recommend eight
attempts, then `unavailable`, retaining the final native code if one exists).
Do not claim an atomic snapshot across several independent queries.

## Errors and failure semantics

```void
use error as core

enum error_kind:
    not_found
    invalid_argument
    invalid_path
    invalid_utf8
    permission_denied
    unavailable
    unsupported
    system_failure

struct error extends core.error:
    operation: string
    kind: error_kind
    os_code: i64
    native_domain: string
```

`code` inherited from core is the stable 1-based order of `error_kind` above;
`message` explains the operation without environment contents; `path` is the
relevant representable path or empty. `operation` is a public name such as
`os.env` or `os.config_dir`, including when a private native/root/path helper
failed. This error shape does not change `io.error`.

| Kind | Meaning |
|---|---|
| `not_found` | Requested environment variable is absent. |
| `invalid_argument` | Invalid application ID or environment name. |
| `invalid_path` | Present directory root is relative or violates host path requirements; no cwd repair. Missing/empty configured override fallback follows the explicit rules above. |
| `invalid_utf8` | Required native text cannot be represented losslessly as VOID UTF-8. |
| `permission_denied` | Native lookup reports a recognized access-denied condition. Does not predict later I/O access. |
| `unavailable` | No current-user record/location/image, empty native location, non-file/relative executable or resource result, failed bundle classification, or repeatedly changing lookup. |
| `unsupported` | An operation/host facility is explicitly unsupported, without a fabricated fallback. |
| `system_failure` | Other native errors; do not guess a stronger portable classification. |

Native domain is empty and os_code zero when there is no native diagnostic;
otherwise use `errno`, `win32`, `hresult`, or the original NSError domain string.
Win32/HRESULT values preserve unsigned 32-bit bits in i64; POSIX and NSError
values retain their signed numeric meaning. Capture the proper return/error
source immediately: getpwuid_r returns its code, KnownFolder returns HRESULT,
and Win32 APIs document their last-error behavior. Do not classify arbitrary
HRESULT/NSError codes as errno or discard their domain. A known Win32-facility
HRESULT may be decoded for classification while retaining its original value.

Native/managed allocation failure and invalid internal ABI/geometry use existing
Runtime failure propagation. They are not `not_found`, false bundle success,
partial string success or ordinary environmental errors. All temporary native
allocations, COM guard obligations and Objective-C scopes unwind on failure.
No persistent OS object is opened by the public API.

Existing `?` propagation requires exactly matching error types. Sharing the
core.error base does not permit `os.error` and `io.error` to propagate
interchangeably or through `result<...,core.error>` automatically. Mixed-operation
application functions use explicit branching/error adaptation. The wrappers
likewise translate path-helper errors once, rather than adding language coercions.

## Ownership, execution and tooling

All native discovery is owner-side synchronous execution. Calls from source
worker graphs are rejected transitively in frontend and typed IR, including
through helpers. Readonly arguments confer no worker authority. The owner can
evaluate strings and transfer permitted snapshots under existing worker rules.
No automatic async dispatch, task creation, cancellation protocol or new native
resource type. Blocking platform calls can stall the owner; applications should
normally discover startup locations once and retain them explicitly.

Calls in ordinary runtime-global initialization are allowed and use existing
eager exactly-once/failure rules. Compatible reload preserves an explicitly
stored global path; a fresh call re-queries native state. Independent applications
and cache hits never inherit another application's observed environment or heap
state. JIT/O2/AOT share the same guarded ABI. No result is constant-folded from
the compiler/editor machine. Source import/VFS identity and security are unchanged.

Every public symbol/field needs plain `##` documentation, documented parameters
and returns under DOCUMENTATION_HOVER. Completion, hover, signature help,
definition and references use normal source metadata; installed stdlib rename
remains protected. No host queries during checking or editor operations. Preserve
UTF-16 source positions, unsaved overlays and stale-result rejection.

## Final capability ownership matrix

**I** = included in the amended initial `os` contract, pending complete-scope
approval; **E** = implemented elsewhere at this baseline; **F** = assigned to a
future module only if separately approved; **P** = platform-specific/unsupported
by this initial portable contract. Future module names are responsibility
assignments, not new files, approved designs or implementation promises.

| Significant capability | Class | Owner and exact boundary |
|---|---|---|
| User data/config/cache discovery | I | `os.data_dir`, `config_dir`, `cache_dir`; approved native/current-user placement; paths only. |
| Persistent application state | I | `os.state_dir`; all three platforms, distinct state placement/role, no log rotation or cleanup. |
| Home and temporary-directory discovery | I | `os.home_dir`, `temp_dir`; current application environment/container; no secure-object reservation. |
| Process executable and default resources | I | `os.executable_path`, `resource_dir`; approved process-host/bundle convention, no source-root or package scan. |
| Individual environment reads | I | `os.env`; absent/empty distinguished, no inventory of all variables. |
| Platform/process architecture | I | `os.platform`, `architecture`; no OS-version, physical CPU or emulation inference. |
| Current process ID | I | `os.process_id`; runtime scalar only, no security or process handle. |
| macOS application-bundle observation | I | `os.is_bundle`; narrow fallible packaging predicate, not sandbox/authentication. |
| Current cwd discovery | E | `path.absolute("", "")`; one existing cwd snapshot and existing io.error. No os alias. |
| Path composition/normalization | E | `path`; lexical host operations; source-import VFS remains a separate compiler contract. |
| Filesystem content/metadata/mutations | E | `io`; read/write/copy/move/delete, directory creation/listing, links and explicit async operations. No os duplicates. |
| Exclusive file creation | E | `io.open(..., io.open_mode.create_new)`; atomic name collision rejection, not a complete private-temp protocol. |
| Secure temporary file/directory convenience | F | Future `io` work: naming, exclusive acquisition, permissions, lifetime/cleanup and race protection need a separate contract. `os.temp_dir` supplies only a base. |
| Environment mutation/enumeration | F | Future `process` ambient-state design; process-wide synchronization, snapshot and embedding semantics. Readonly os does not grant it. |
| Cwd mutation | F | Future `process` policy, if approved; affects all threads/relative I/O, never a path-manipulation side effect. |
| Process launch/wait/signals/forced termination | F | Future `process` with lifecycle/error/security contracts. No shell, child manager or os.exit is added. |
| Normal return/exit status | E | Existing entry/main/Runtime unwind and CLI/AOT adapter; no arbitrary forced-exit API is implied. |
| Application command-line arguments | F | Future `process` for raw argument access and future `args` for parsing; current application entries accept no operands. Legacy entry(i32) input is implemented, not a string argv API. |
| OS release/version and detailed system information | F | Future `system`; platform-specific feature/version/memory/CPU semantics need explicit design. `platform` remains a stable family identifier. |
| User/account/security identity | F | Future `system`; real/effective user, Windows tokens and sandbox principals differ. Home lookup's private account fallback is not a public identity/authentication API. |
| Native host/hardware architecture or emulation | F | Future `system`; deliberately separate from the running binary's architecture. |
| General packaging/sandbox/entitlement information | F | Future `system` with platform-specific adapters; os preserves native redirected roots but does not certify sandbox status/access or enumerate packages. |
| XDG runtime/session/IPC directory | P | Linux-specific future facility; security/lifetime requirements have no approved portable equivalent. Env inspection is not validation. |
| System-wide resource/config search lists | P | No initial API for XDG_DATA_DIRS/CONFIG_DIRS, Windows package roots or arbitrary installation search; future packaging/system contract if required. |
| Clocks, entropy and standard streams | E | Existing `time`, `random`, `console`; no clock, entropy, print, newline or stream aliases in os. |

“Complete” here means complete implementation of the approved environment/location
surface with the adjacent responsibilities explicitly owned. It does not mean
all operating-system facilities or proposed future modules exist.

## Working-style examples

Proposed API usage, not executable acceptance evidence:

```void
use os
use path
use io
func save_score(folder: string, score: i32) -> result<string, io.error>:
    var made = io.create_directories(folder)?
    var file = path.join([folder, "score.txt"])?
    var written = io.write_text(file, "{score}")?
    return Ok(file)

func main():
    var location = os.data_dir("com.studio.game")
    if location is Ok(var folder):
        if save_score(folder, 42) is Err(var reason):
            print(reason.message)
    else if location is Err(var reason):
        print(reason.message)
    if os.env("VOID_GAME_ASSETS") is Ok(var chosen):
        print("Explicit application asset root:", chosen)
    else:
        if os.resource_dir() is Ok(var packaged):
            print("Host package resource root:", packaged)
    if os.is_bundle() is Ok(var bundled):
        print(os.platform(), os.architecture(), bundled)
    if os.state_dir("com.studio.game") is Ok(var state):
        print("Persistent restart/log location:", state)
    if path.absolute("") is Ok(var cwd):
        print("Existing cwd snapshot query:", cwd)
    print("Native process ID:", os.process_id())
```

The environment name above is application policy, not a built-in VOID override.
Directory creation is visible and fallible in `io`; separate OS discovery avoids
pretending that the distinct result error types support cross-module `?`.

For development or an engine embedding JIT, keep an explicit root in the ordinary
application layer:

```void
func asset_path(asset_root: string, name: string) -> result<string, io.error>:
    return path.join([asset_root, name])
```

This composition is not path containment: untrusted names still require the
application's own policy. `os` does not make arbitrary child paths safe.

## Settled decisions and final review gate

Boss approved LocalAppData/id data, RoamingAppData/id config and
LocalAppData/id/cache cache; recognized macOS bundle resourceURL and otherwise
process-executable parent, including the stated JIT-host meaning. These decisions
supersede the original research's two open policy questions and are not reopened.

Recommend accepting the amended 13-function inventory, including the state
mapping and scalar PID contract, with cwd reused and portable runtime-directory
discovery explicitly excluded. No additional semantic question currently needs
an owner answer; platform implementation uncertainties are verification work.
Golden Wrench's final architecture review and Boss's approval of this amended
complete scope remain the engineering gate. No implementation is authorized by
this research document itself.
