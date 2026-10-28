# OS environment and locations

`use os` exposes exactly 13 public functions. All queries run synchronously on the
owning application thread, including owner async activations and eager globals.
Worker-reachable calls are rejected transitively. Import, source checking, editor
analysis and object emission execute no native OS query.

| Function | Return | Behavior |
|---|---|---|
| `data_dir(app_id: string)` | `result<string, error>` | Durable application-managed content, including local saves. |
| `config_dir(app_id: string)` | `result<string, error>` | Settings, with the platform placement below. |
| `cache_dir(app_id: string)` | `result<string, error>` | Regenerable application content. |
| `state_dir(app_id: string)` | `result<string, error>` | Persistent operational history and restart context. |
| `home_dir()` | `result<string, error>` | Current native application/user home. |
| `temp_dir()` | `result<string, error>` | Temporary storage base, without object creation/reservation. |
| `executable_path()` | `result<string, error>` | Executing native process image. |
| `resource_dir()` | `result<string, error>` | Actual recognized macOS application resource URL; otherwise process-image parent. |
| `env(name: string)` | `result<string, error>` | One current environment value, preserving exact valid UTF-8. |
| `platform()` | `string` | `linux`, `windows` or `macos`. |
| `architecture()` | `string` | Running binary target: `x86_64`, `aarch64`, `x86`, `arm` or `unknown`. |
| `process_id()` | `i64` | Native getpid/GetCurrentProcessId, with unsigned DWORD widened directly. |
| `is_bundle()` | `result<bool, error>` | Recognized macOS APPL `.app` with a file-backed executable; false for confirmed plain hosts/other platforms. |

Every returned path is owned, nonempty, absolute, NUL-free and strict UTF-8, or
the operation fails. Discovery creates nothing and promises no existence,
writability, permissions, backup, retention or security capability. It does not
expand shell syntax, trim whitespace, normalize Unicode/case, collapse parent
components or resolve symbolic links. Existing `path` separator conventions apply.

## Application placement

| Getter | Linux base / app ID | Windows | macOS |
|---|---|---|---|
| Data | XDG_DATA_HOME or HOME/.local/share | LocalAppData / ID | Application Support / ID |
| Config | XDG_CONFIG_HOME or HOME/.config | RoamingAppData / ID | Application Support / ID / config |
| Cache | XDG_CACHE_HOME or HOME/.cache | LocalAppData / ID / cache | Caches / ID |
| State | XDG_STATE_HOME or HOME/.local/state | LocalAppData / ID / state | Application Support / ID / state |

Each Linux XDG override is selected independently. Nonempty absolute overrides
win without consulting HOME. Missing, empty and relative overrides use that
getter's HOME default. Relative overrides are ignored before encoding validation;
malformed absolute overrides fail. Nonempty absolute HOME wins; missing/empty
HOME uses `getpwuid_r(getuid())`. Relative HOME fails. TMPDIR is used when nonempty
and absolute; missing/empty selects `/tmp`, and relative values fail. Native
launcher-provided roots are retained, including Flatpak/Snap/AppImage conventions.
Actual packaged launcher execution is a separate verification claim.

Windows uses redirected current-user Known Folders with DONT_VERIFY, no CREATE
or DEFAULT_PATH. Scoped COM initialization preserves existing STA/MTA apartments
and changed-mode ownership. Home uses Profile and temp uses GetTempPathW. Native
wide APIs and strict conversion preserve Unicode, long and UNC/extended paths.
The compiler's separate secure staging policy is unchanged.

macOS uses Foundation user-domain discovery with create:NO and native home/temp,
including the actual application/container environment. State does not imply
login-session storage, log rotation, cleanup or a logging service. The selected
config/cache/state children in shared data roots are reserved layout conventions.

IDs are 1–255 ASCII bytes, beginning/ending with lowercase a–z or 0–9. Interior
bytes may also be `.`, `_`, `-`; dot pieces cannot be empty. Windows device stems
con/prn/aux/nul/com1–9/lpt1–9, including before a first dot, are rejected on every
platform before discovery. No case conversion, Unicode, separators, NUL, spaces,
colon, traversal or automatic app-identity authentication is provided.

## Process image and resources

Linux reads `/proc/self/exe`, retaining the kernel's ` (deleted)` spelling.
Windows uses GetModuleFileNameW; macOS uses `_NSGetExecutablePath`, rejecting
relative native image results. Growing heap buffers check truncation and size.
There is no argv, PATH, cwd, source or guessed-location fallback.

Normal/O2 JIT runs within `voidc` or an embedding host: image, resource, bundle and
PID observations describe that host. Linked AOT describes the application process.
A macOS application is recognized through APPL package metadata, `.app` bundle
URL and a file executable URL. Recognized apps use the actual file resource URL;
lookup/classification failure never authorizes an executable-parent substitute.
Other confirmed hosts use existing `path.parent`. Other installation layouts
pass an explicit asset root through ordinary application configuration/parameters.

PIDs are shared by Runtimes/threads in one process, can be reused after exit and
are namespace-scoped on Linux. They convey no security or process-control
capability and are unsuitable as a unique filename reservation. The bridge uses
an actual i64 channel and allocates no managed string for PID.

## Environment, errors and ownership

`env` rejects empty names, NUL and equals. Other strict UTF-8 names are accepted;
POSIX lookup is case-sensitive and Windows lookup is native case-insensitive.
A present empty value is `Ok("")`; absence is `Err(not_found)`. Valid contents,
including newlines, whitespace and equals, are copied exactly. Malformed native
UTF-8/UTF-16 fails without replacement characters. Diagnostics never include values.
Native environment mutation by the embedding host must be quiescent with queries;
independent calls do not form an atomic multi-variable snapshot. Windows resizing
has an eight-attempt limit.

`os.error extends core.error` adds `operation`, `kind`, `os_code` and
`native_domain`. Inherited `code` is the one-based order below; inherited `message`
is value-free and `path` is a relevant representable path or empty.

| Code | Kind | Meaning |
|---|---|---|
| 1 | not_found | Requested environment variable is absent. |
| 2 | invalid_argument | Invalid ID or environment name. |
| 3 | invalid_path | Configured root is relative or violates native path rules. |
| 4 | invalid_utf8 | Native text cannot be represented losslessly. |
| 5 | permission_denied | Recognized native access-denied discovery error. |
| 6 | unavailable | Missing record/location/image, invalid bundle classification, or unstable repeated lookup. |
| 7 | unsupported | Native facility is explicitly unsupported. |
| 8 | system_failure | Other native failure. |

Domains retain `errno`, `win32`, `hresult` or the original NSError domain. Native
Win32/HRESULT bits widen unsigned to i64; errno/NSError preserve signed values.
Non-native errors have empty domain and zero native code. Allocation, extent and
internal ABI failures follow checked Runtime failure rather than a fabricated
ordinary result. Native resources, COM and autorelease scopes unwind safely.

`os.error` and `io.error` remain separate. `?` requires exactly matching error
types. The facade explicitly adapts the existing path helpers; applications
combining os/io handle or adapt those results explicitly. Managed inputs are
copied before NativeSafe and every output stays rooted through publication.

An explicitly stored application global is an application-owned snapshot.
Compatible reload retains it; a fresh call re-queries native state. New Runtimes,
fresh processes and native object-cache hits receive fresh application state.
No hidden OS cache, watcher, background polling or serialized host state exists.

## Existing facilities and boundaries

Use `path.absolute("")` or `path.absolute("", "")` for the existing cwd snapshot,
with its existing lexical/trailing-separator behavior and `io.error`. Retain an
explicit base for later path resolution when needed. `os` adds no cwd wrapper.

Use `io` for file/directory creation, reads, writes, copying, deletion and listing.
No setters, environment enumeration, subprocesses, argv parsing, detailed system
inspection, process manager or general FFI is included. There is no portable
runtime/session directory or secure temporary-object helper. Reading
XDG_RUNTIME_DIR through env certifies no ownership, mode, filesystem or lifetime.
See the complete [capability ownership matrix](DESIGN.md#final-capability-ownership-matrix).

[Startup example](../../examples/os/startup.void) discovers and reports locations.
[Save example](../../examples/os/save.void) explicitly creates storage through io.
[Resource example](../../examples/os/resources.void) reads through the documented
host/application layout and preserves each module's error type.
