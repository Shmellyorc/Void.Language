# VOID documentation

Current operations use **LLVM/Clang/LLD 23.1.4 exclusively**. Start with the
[toolchain requirements](SOURCE_COMPILER.md#active-toolchain). Earlier-version commands and dated
holds in historical records do not govern current work.

## Canonical documentation

- [Install from source](install/README.md), [first program](FIRST_PROGRAM.md),
  [CLI](CLI.md), [compiler/build architecture](SOURCE_COMPILER.md),
  [editor setup](EDITOR_INTEGRATION.md) and [troubleshooting](TROUBLESHOOTING.md).
- [Creating and composing libraries](LOCAL_LIBRARIES.md#creating-and-composing-libraries),
  [package add/tidy](CLI.md#source-packages-add-and-tidy) and
  [G27 implementation and qualification](projects/G27_LIBRARY_AUTHORING.md).
- [Language design](LANGUAGE_BIBLE.md), [numeric types](NUMERIC_TYPES.md),
  [modules](MODULE_SYSTEM.md), [strings](STRINGS.md), [value semantics](VALUE_SEMANTICS.md),
  [value structs](VALUE_STRUCTS.md), [reference/value structs](REFERENCE_VALUE_STRUCTS.md),
  [managed structs](MANAGED_STRUCTS.md), [initialization](INITIALIZATION.md),
  [managed initialization](MANAGED_INITIALIZATION.md), [composition](COMPOSITION.md),
  [inheritance](INHERITANCE.md), [receiver methods](RECEIVER_METHODS.md), [properties](PROPERTIES.md).
- [Fixed arrays](FIXED_ARRAYS.md), [growable arrays](GROWABLE_ARRAYS.md),
  [array operations](ARRAY_OPERATIONS.md), [dictionaries](DICTIONARIES.md),
  [generics](GENERICS.md), [interfaces](INTERFACES.md), [advanced language](ADVANCED_LANGUAGE.md).
- [Async/Tasks](ASYNC.md), [native libraries](NATIVE_LIBRARIES.md),
  [GC Runtime](GC_RUNTIME.md), [runtime globals](RUNTIME_GLOBALS.md),
  [cache policy](compiler-health/stabilization/CACHE.md), [diagnostics](DIAGNOSTICS.md),
  [analysis compatibility](ANALYSIS_COMPATIBILITY.md), [language server](LANGUAGE_SERVER.md).
- [Standard-library modules](#standard-library-modules) and
  [maintained examples](#standard-library-examples).

## Standard-library modules

These are the **29 implemented source modules** installed under `share/void/stdlib`.
Use the module spelling shown below, for example `use collections as c` or
`use net/tcp`. Each module link opens its authoritative declarations and source docs;
the guides explain usage and limits without replacing those declarations.

| Module / source | Use for | Existing guide |
| --- | --- | --- |
| [console](../stdlib/console.void) | Console input, output and terminal control | [Checked and convenience input](stage10/CONSOLE_IO.md#choosing-key-or-line-input) |
| [convert](../stdlib/convert.void) | Typed parsing, display conversion and result unwrapping | [Conversion contract](combined/CONTRACT.md#convert) |
| [error](../stdlib/error.void) | Common managed diagnostic base | [Typed error identity](combined/CONTRACT.md#installed-modules-and-error-identity) |
| [text](../stdlib/text.void) | Literal search, split/join, trimming and Unicode scalar operations | [Text API and failures](text-encoding/API.md) |
| [encoding/ascii](../stdlib/encoding/ascii.void) | Strict ASCII conversion and byte classification | [ASCII contract](text-encoding/CONTRACT.md#ascii) |
| [encoding/hex](../stdlib/encoding/hex.void) | Hex encoding and strict decoding | [Codec contract](text-encoding/CONTRACT.md#hex-and-base64) |
| [encoding/base64](../stdlib/encoding/base64.void) | Standard or URL-safe Base64 with explicit padding | [Codec contract](text-encoding/CONTRACT.md#hex-and-base64) |
| [encoding/binary](../stdlib/encoding/binary.void) | Exact-width numeric bytes and byte order | [Binary contract](binary/CONTRACT.md) |
| [math](../stdlib/math.void) | Typed numeric operations and constants | [Math API](math/API.md) |
| [random](../stdlib/random.void) | Seeded gameplay PRNG, ranges, choice and shuffle | [Random API](random/API.md) |
| [collections](../stdlib/collections.void) | Sorting, search, map/filter/fold and dictionary copies | [Collections API](collections/API.md) |
| [io/memory](../stdlib/io/memory.void) | Owned seekable byte streams and wire values | [Memory contract](memory/CONTRACT.md) |
| [io](../stdlib/io.void) | Checked files, directories and private atomic saves | [I/O API](io/API.md) |
| [path](../stdlib/path.void) | Lexical host-path composition and components | [Paths and filesystem authority](io/API.md#paths-cleanup-and-publication) |
| [os](../stdlib/os.void) | Application locations, arguments and host queries | [OS API](os/API.md) |
| [time](../stdlib/time.void) | Dates, zones, clocks, durations and async sleep | [Temporal formatting](stage10/DISPLAY_FORMATTING.md), [timers](ASYNC.md#g10-monotonic-timers) |
| [json](../stdlib/json.void) | Bounded JSON, typed conversion and field defaults | [JSON API](json/API.md) |
| [log](../stdlib/log.void) | Checked console/file logging and rotation | [Logging API](log/API.md) |
| [channel](../stdlib/channel.void) | Typed synchronous topic pub/sub | [Channel API](channel/API.md) |
| [url](../stdlib/url.void) | HTTP(S) URLs, percent encoding and query construction | [URL/HTTP guide](net/http/README.md) |
| [net/address](../stdlib/net/address.void) | Numeric addresses, endpoints and bounded DNS | [Address API](net/address/API.md) |
| [net/error](../stdlib/net/error.void) | Typed socket/DNS failures and progress | [Network errors](net/error/API.md) |
| [net/tcp](../stdlib/net/tcp.void) | TCP listeners and byte streams | [TCP API](net/tcp/API.md) |
| [net/udp](../stdlib/net/udp.void) | Bounded unicast datagrams | [UDP API](net/udp/API.md) |
| [net/http](../stdlib/net/http.void) | Verified HTTP/1.1 and HTTP/2 clients and transfers | [HTTP guide and platform limits](net/http/README.md) |
| [system_error](../stdlib/system_error.void) | Shared host-operation errors | [Host scope](voidup/REPORT.md) |
| [crypto](../stdlib/crypto.void) | SHA-256 and detached Ed25519 verification | [Implemented host scope](voidup/REPORT.md) |
| [archive](../stdlib/archive.void) | Bounded ZIP reading and deterministic stored ZIP export | [ZIP contract](archive/ZIP_EXPORT.md) |
| [process](../stdlib/process.void) | Explicit child launch, wait/close and bounded capture | [Process API](process/API.md) |

Arrays/lists, dictionaries, strings and UTF-8 conversion, `print`/`format`, results,
`with`/`defer`, events and Task/group/worker syntax are **language builtins**, not
additional stdlib modules. See [array operations](ARRAY_OPERATIONS.md),
[dictionaries](DICTIONARIES.md), [display](stage10/DISPLAY_FORMATTING.md),
[recoverable builtins](STAGE8_TEXT_COLLECTIONS.md) and [explicit async hosting](ASYNC.md#explicit-root-adapter).

## Standard-library examples

Open the maintained source and its guide for inputs and execution requirements.
The ordinary CLI entry is synchronous; async examples use the explicit host above.

| Task | Existing example and guide |
| --- | --- |
| Configuration and defaults | [Configuration application](../examples/json/configuration.void), [JSON defaults](json/API.md#explicit-configuration-defaults) |
| Text and codecs | [Small text example](../website/public/docs/snippets/text-encoding.void), [text cases](../tests/text_encoding/text.void), [codec/file composition](../tests/text_encoding/interop.void) |
| Collections and generic callbacks | [Sorting, mapping, fixed slicing and dictionary copy](../examples/collections_stdlib/main.void), [collection guidance](collections/API.md#choosing-inputs-and-callbacks) |
| Local sockets | [Addresses](../examples/net_sockets/addresses.void), [TCP server](../examples/net_sockets/tcp_echo_server.void)/[client](../examples/net_sockets/tcp_echo_client.void), [UDP loopback](../examples/net_sockets/udp_loopback.void) |
| HTTP and JSON | [API client](../examples/net_http/api_json.void), [streaming async](../examples/net_http/streaming_async.void), [HTTP contract](net/http/DESIGN.md) |
| Package composition | [Library creation and transitive consumer workflow](LOCAL_LIBRARIES.md#creating-and-composing-libraries), [maintained package composition fixture](../tests/project/packages.py) |
| Files and publication | [Filesystem application](../examples/io/filesystem.void), [atomic save](../examples/atomic_save/test_game.void), [checked logger shutdown](../examples/log/checked_shutdown.void), [I/O contract](io/API.md#paths-cleanup-and-publication) |
| Child output capture | [Capture application](../examples/process/capture.void), [launch authority and budgets](process/API.md#choosing-launch-and-capture) |

## Engineering documentation

- [Archive and retention policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)
  governs qualification outputs and cleanup for every milestone.
- [Windows x64 D5 application packaging](projects/W2_WINDOWS_D5_IMPLEMENTATION.md)
  records the package contract and the
  [accepted Windows Server 2022 Debug and Release qualification](projects/W2_WINDOWS_D5_IMPLEMENTATION.md#hosted-acceptance).
- [Slice 2 project discovery and entry selection](projects/SLICE2.md): compiler-owned
  manifest/VFS admission, immutable D/E snapshots, any-name main, library checking,
  explicit overrides, actual execution tests and independent-review boundary.
- [Slice 1 project manifest implementation](projects/SLICE1.md): schema-only
  compiler API, validation/diagnostics, executed local qualification and explicit
  deferred integration/platform limits.
- [First-class projects and C interoperability roadmap](PROJECT_ARCHITECTURE_ROADMAP.md):
  Phase 1 schema/identity/discovery/metadata/creation contract proposal, source
  audit, compatibility matrix, 42 acceptance cases and six implementation slices.
  Local Slice 2 implementation/review boundary; later enum/FFI/raylib/package
  direction and the single final hosted acceptance gate are retained.
- [Current status/strategy](DEVELOPMENT_STRATEGY.md), [Wrench instructions](WRENCH_CLANKER.md),
  [Research role](RESEARCH_CLANKER.md), [known issues](KNOWN_ISSUES.md).
- VOIDUP: [Phase A](voidup/REPORT.md), [Phase B](voidup/PHASE_B_REPORT.md),
  [integration](voidup/INTEGRATION_REPORT.md), [LLVM CI repair](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair).
- [HTTP current engineering](net/http/CURRENT_ENGINEERING.md),
  [TCP/UDP architecture handoff](net/TCP_UDP_HANDOFF.md),
  [compiler-health issues](compiler-health/ISSUES.md),
  [integrated performance qualification](compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md).
- [G19 completion attribution](compiler-health/G19_COMPLETION_ATTRIBUTION.md):
  phase measurements and a rejected isolated reuse experiment; production HOLD
  pending Golden Wrench review.
- [LLVM compatibility](llvm23/COMPATIBILITY.md), [preview blockers](release/PREVIEW_BLOCKERS.md),
  [redistribution/licensing](release/REDISTRIBUTION_AUDIT.md),
  [documentation consolidation report](voidup/INTEGRATION_REPORT.md#documentation-review-revision).

## Historical records

[Work log](WORK_LOG.md), [decisions](DECISIONS.md),
[approval ledger](LANGUAGE_COMPLETENESS_APPROVALS.md), module implementation reports
and Stage 7–11 acceptance records retain original decisions, CI links and failures.
Original proposal bodies remain at their existing `*_PROPOSAL.md` paths, labeled
historical; API guides above define delivered behavior.

The earlier index, Wrench, Research and strategy checkpoints already exist at
immutable commit `82e97f541fab7eec760083383569ab9c9ce8f355`:
[chronology](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/README.md),
[Wrench/workshop](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/WRENCH_CLANKER.md),
[Research](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/RESEARCH_CLANKER.md),
[strategy](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/DEVELOPMENT_STRATEGY.md).
The same originals are available offline with `git show <commit>:docs/<file>`.
Their old toolchain recipes and pending-stage holds are historical, not current
instructions. No second set of full checkpoint copies is maintained.

## Clanker entry point and permanent documentation policy

Only implement the scope Boss authorizes. Keep engineering instructions and status
under `docs/`, use this index for navigation, and update the relevant canonical
contract or implementation report. Preserve unique approvals and evidence when
consolidating; link to history rather than copying old status into every guide.
The root README remains protected and requires separate explicit authorization.
Boss controls Git and hosted acceptance. See [Wrench](WRENCH_CLANKER.md) for the
shared workflow; old sprint gates do not create fresh authorization requirements.
