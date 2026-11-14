# G30-R — standard-library completeness and developer usability

13 November 2026. Audited commit: `2a14bb619d761e4c66c84ddee3c857875e0ec0a3`.
Research, source/contract review and disposable application probes only. No production
repair, optimization, package publication, Git mutation or hosted execution.

## Assessment

**VOID's current stdlib is sufficient for the common CLI, configuration, collection,
local networking and reusable-library tasks examined here. No new essential module
or correctness repair is justified by this audit.** This is a scoped finding, not
proof that every API, workload or platform is defect-free.

The smallest useful follow-up is a documentation/navigation pass: expose the
complete installed module inventory from the canonical index, link the current
socket/process/security contracts, and explain the few important composition
boundaries beside existing runnable examples. Do not add alternate collection,
string, JSON, networking or task engines. No new release hold is recommended.

Five native profiles passed the same new CLI/text/collections application. Three
additional Linux Release controls verified an intentional fixed-array boundary
and the existing recoverable builtins. G28/G29 application and installation results
are reused explicitly rather than counted as new executions.

## 1. Scope, identity and evidence precedence

The starting tree was clean and HEAD matched the assigned commit. Read the current
[Wrench instructions](../WRENCH_CLANKER.md), [Bible](../LANGUAGE_BIBLE.md),
[strategy](../DEVELOPMENT_STRATEGY.md), [milestone register](../roadmap/MILESTONE_REGISTER.md),
[G28 application audit](G28_REAL_APPLICATION_READINESS.md),
[G29 audit](../release/G29_FIRST_RELEASE_READINESS.md) and
[G29 repair](../release/G29_MINIMAL_RELEASE_PREPARATION_REPAIR.md), alongside the
module/API contracts and source cited below. Dated proposals do not override
implemented contracts or later acceptance.

All five `voidc` executable hashes match the preserved G28 identity records exactly.
The native Windows guest's 29 stdlib files match the current checkout's SHA-256
inventory. No compiler was rebuilt or substituted. Version receipts identify LLVM
23.1.4. Linux uses `build/llvm23-{release,debug,strict}`; Windows uses the existing
native `C:/Users/a/VoidG20-20261112/{Release,Debug}` builds. Binary equivalence
supports reuse of G28 evidence; it does not expand that evidence's original scope.

Source inspection includes public facades and private boundaries: frontend canonical
stdlib admission and Random arity adaptation in `src/frontend/compiler.cpp:559–619`,
collection version adapters at `:3578`; `src/frontend/host.inc`; registered/leased
host argument copying and resource admission in `src/runtime/host_abi.cpp:137–230`;
checked list reads in `src/runtime/list.cpp:44–66`; and the shared temporal bridge
in `src/runtime/temporal_abi.cpp`. Ordinary source wrappers are not a public unchecked
native interface. The compiler continues to own docs, types and editor metadata.

## 2. Actual public inventory — 29 source modules

These are physical `stdlib/**/*.void` modules, not inferred roadmap entries.
Each source link is the authoritative declaration surface. The archived inventory
records paths, hashes and declaration outlines; it is not a replacement parser or
an assertion that every declaration was executed in G30.

| Module | Implemented capability | Important boundary / current guide |
| --- | --- | --- |
| [console](../../stdlib/console.void) | stdin/stdout/stderr, checked text/byte/line/character input, writes/flush, keys, terminal capabilities and scoped modes; simple read/readline | Simple helpers discard recoverable EOF/error distinctions; use checked reads when needed. [Console contract](../stage10/CONSOLE_IO.md), [later additions](../combined/CONTRACT.md). |
| [convert](../../stdlib/convert.void) | Exact typed numeric/bool/payload-free-enum parsing, shared display conversion, unwrap/unwrap_or | Complete invariant grammar; whitespace trimming is explicit; unwrap is checked failure, not error recovery. [Combined contract](../combined/CONTRACT.md). |
| [error](../../stdlib/error.void) | Common managed diagnostic base | Does not erase each module's typed error or automatically unify every result. |
| [text](../../stdlib/text.void) | Literal search, scalar substring, trim, split/lines, join, replacement, repetition, padding, Unicode case mapping and scalar translation | Strict UTF-8, scalar indexes, bounded construction; no grapheme segmentation/normalization/regex. [API](../text-encoding/API.md). |
| [encoding/ascii](../../stdlib/encoding/ascii.void) | Strict ASCII encode/decode and byte classification/casing | High bytes are not silently transliterated. [Contract](../text-encoding/CONTRACT.md). |
| [encoding/hex](../../stdlib/encoding/hex.void) | Encode/try_encode/decode | Explicit uppercase option; rejects odd length, whitespace and malformed digits. Same contract. |
| [encoding/base64](../../stdlib/encoding/base64.void) | Standard/URL-safe alphabets, required/omitted padding, encode/try_encode/decode | Strict canonical tail bits and chosen alphabet/padding; no silent autodetection. Same contract. |
| [encoding/binary](../../stdlib/encoding/binary.void) | Ten primitive numeric representations, explicit byte order | Exact-width decode, owned bytes, not arbitrary struct serialization. [Contract](../binary/CONTRACT.md). |
| [math](../../stdlib/math.void) | 29 generic numeric operations and six typed constants | Exact numeric families; no implicit widening/fast-math or cross-platform transcendental bit identity. [API](../math/API.md). |
| [random](../../stdlib/random.void) | Explicit seeded generators, default generator, all integer/float families, bounded forms, choose/shuffle/shuffled, checked variants | Gameplay PRNG, algorithm version 1; not cryptographic randomness. Direct-call arity adaptation is not general overloading. [API](../random/API.md). |
| [collections](../../stdlib/collections.void) | Stable sorted/reversed copies, bounds/search, predicates/find, filter/map/fold, min/max/sum, dictionary keys/values/copy | Growable inputs; fixed arrays explicitly slice. Deep readonly, snapshots and iterator invalidation remain. [API](../collections/API.md). |
| [io/memory](../../stdlib/io/memory.void) | Owned seekable byte stream, exact/partial reads, numeric/string/Pascal reads/writes | Shared cursor, independent snapshots, recoverable errors preserve state. Current source uses geometric growth/in-capacity writes; old whole-buffer-copy descriptions are historical. [Contract](../memory/CONTRACT.md). |
| [io](../../stdlib/io.void) | Checked file/resource access, text/bytes, directories, metadata, links, copying/moving, sync/async operations; private directories, locks, publication and atomic saves | Atomic-save capability has private/no-follow/local-filesystem requirements and distinct publication/durability outcome. [API](../io/API.md), [G06](../roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g06-atomic-application-saves). |
| [path](../../stdlib/path.void) | Join/normalize/components/extensions, roots, absolute/relative paths | Native lexical rules, not containment/security proof; reuses `io.error` and existing path kernels. |
| [os](../../stdlib/os.void) | Application data/config/cache/state locations, home/temp, image/resources, env, platform/arch/PID/bundle and arguments | Returns paths; creates nothing. JIT image/resource queries describe the host. [API](../os/API.md). |
| [time](../../stdlib/time.void) | Date/time/instant/duration/offset/zone values, validated construction, arithmetic, formatting, wall/monotonic clocks and async sleep | Explicit gap/overlap policy, separate monotonic domain, no general date-string parser. [Formatting contract](../stage10/DISPLAY_FORMATTING.md), [G10 timers](../ASYNC.md#g10-monotonic-timers). |
| [json](../../stdlib/json.void) | Bounded strict parser, immutable observed DOM, typed conversion, explicit field defaults and compact stringify | Duplicate decoded keys reject; ordered output; public stored-data DTO restrictions and metering. [API](../json/API.md), [G09](../json/IMPLEMENTATION.md#g09-configuration-and-typed-field-defaults). |
| [log](../../stdlib/log.void) | Checked global/instantiated logging, filtering/lazy messages, console/files, rotation, recovery, sync/async operations | Per-sink progress, synchronous completion and durable flush; no hidden batching. [API](../log/API.md). |
| [channel](../../stdlib/channel.void) | Instantiated typed synchronous string-topic publish/subscribe | Built-in event snapshots, explicit tokens, owner-thread use; not a worker queue. [API](../channel/API.md). |
| [url](../../stdlib/url.void) | HTTP(S) parse/resolve/normalize, component/query encoding/decoding and query construction | Not an arbitrary-scheme URI or filesystem path engine. [HTTP/URL guide](../net/http/README.md). |
| [net/address](../../stdlib/net/address.void) | Strict IPv4/IPv6 values, endpoints/scopes, formatting, sync/async DNS | No port parsed out of an arbitrary hostname string; family/scope/port explicit. [G07](../roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g07-native-tcp-udp-and-dns). |
| [net/error](../../stdlib/net/error.void) | Shared socket/DNS kind, phase, native domain, cleanup and progress data | Keeps failed operation/partial outcomes visible; not interchangeable with every HTTP error. |
| [net/tcp](../../stdlib/net/tcp.void) | Connect/listen/accept, read/write, half-close/close, endpoints, deadlines and sync/async paths | No public raw handle; stream framing belongs to the application. Same G07 contract. |
| [net/udp](../../stdlib/net/udp.void) | Bind, send-to, receive-from, close and sync/async paths | Bounded datagrams/truncation reporting; approved unicast scope, not multicast/raw sockets. Same G07 contract. |
| [net/http](../../stdlib/net/http.void) | Verified HTTP/1.1 and HTTP/2 clients, bounded streaming/file transfers, TLS, redirects, auth, cookies, proxies, compression, cancellation | Explicit capabilities/platform limits; no HTTP server/WebSocket/HTTP3 claim. [Accepted scope](../net/http/ACCEPTANCE.md). |
| [system_error](../../stdlib/system_error.void) | Shared host-operation failures | Supplies the narrow crypto/archive/process facades; no replacement exception hierarchy. |
| [crypto](../../stdlib/crypto.void) | Incremental SHA-256 and detached Ed25519 verification | No signing, encryption/password hashing or public secure-random byte API. Narrow implemented host scope, not a general crypto toolbox. |
| [archive](../../stdlib/archive.void) | Bounded stored/deflate ZIP/ZIP64 reader; deterministic stored ZIP writer | Reader never extracts paths automatically; encoder accepts flat ASCII names. [ZIP export](../archive/ZIP_EXPORT.md), [host scope](../voidup/REPORT.md). |
| [process](../../stdlib/process.void) | Explicit executable/argv/environment/cwd launch, wait/close, bounded sync/async capture | No shell/PATH fallback, implicit environment inheritance or process-tree supervisor. [G04](../roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g04-bounded-subprocess-capture-deadlines-and-cancellation). |

Arrays/lists/dictionaries, string length/index/UTF-8 conversion, `print`, `format`,
interpolation, results, `with`/`defer`, tasks/groups/workers and built-in events are
compiler/language facilities. Their absence as separate module files is not a gap.
See [arrays](../ARRAY_OPERATIONS.md), [dictionaries](../DICTIONARIES.md),
[display](../stage10/DISPLAY_FORMATTING.md) and [async](../ASYNC.md).

## 3. Everyday workflows and focused experiments

### New CLI / collections / text composition

One two-module disposable application consumes an actual argument and reads a
4,096-byte-bounded text file. It trims/splits decimal values, reports malformed
input separately from I/O failure, filters positive numbers, stably sorts, maps
through an imported helper using `math`, sums and performs ordered searches.
It verifies independent fixed values and copied dictionaries against shared list
identity, and round-trips `雪😀` through hex/Base64 and UTF-8. Formatting uses ordinary
interpolation. It introduces no production helper or special native host.

Input ` 3, -1, 2, 2, 1 ` gives exact stdout:

```text
4 16 1 3
3 8 3 9 99 1
雪😀 雪😀 1 2 7
```

`1, nope, 2` returns status 3 / `invalid number`; an absent file returns status 4 /
`input error`. Those are deliberate application outcomes, not compiler failures.
This demonstrates useful ordinary application composition, not a general CSV parser.

| New profile | Results | Execution evidence |
| --- | --- | --- |
| Linux Release | 11/11 expected outcomes | Version/check, normal/O2 JIT, two independently linked AOT builds/runs, JIT/AOT invalid data, native missing file |
| Linux Debug | 11/11 | Same |
| Linux fatal ASan/UBSan/LSan | 11/11 | Same, no sanitizer findings |
| Native Windows 10 Release | 11/11 | Same, physical NTFS output and native MSVC environment |
| Native Windows 10 Debug | 11/11 | Same |

These are **55 subprocess outcomes**, including five version and five check commands,
not 55 CTests. The Linux child processes had actual 1,048,576-byte soft/hard stack
limits. Strict settings were `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. This instruments
compiler/Runtime native code, not every generated JIT instruction.

Windows reports build 19045 in the existing misleadingly named “Windows 11” VM;
MSVC tools 14.44.35207, SDK 10.0.19041.0, x64 VS developer shell 17.14.42 and pinned
Clang/LLD 23.1.4. The established `void_windows_native_staging.exe --create-private-root`
created the task root. No shared-folder build, cross-compile or new Windows stack
reservation test is counted as native qualification.

Three extra Release controls isolate composition questions:

1. Passing `[i32,3]` directly to `collections.sorted` correctly rejects (VOID2001,
   generic inference diagnostic). The supported `fixed[0..<fixed.count]` composition
   executes and prints `1 3`, preserving the original fixed value. No new overload or
   container engine is necessary. The diagnostic is less explanatory than the API
   guide, but a diagnostic redesign is not justified by this one control.
2. Empty `list.pop`, absent `dictionary.get` and malformed `string.from_utf8` execute
   their existing recoverable error paths: `empty list`, `key not found`,
   `invalid UTF-8`. Older documentation saying these are deferred is not current.

That is three expected-result commands (one rejection, two successful executions),
for **58/58 recorded expected outcomes** overall. No unexpected crash, failed
expectation or sanitizer incident occurred. The preliminary source check also
passed and is not added to this count. No optimization candidate was created.

### Maintained evidence reused, not rerun

- **Configuration/data:** G28's unchanged `tests/json/configuration.py` exercised
  bounded parse, typed defaults, domain validation, exact atomic save/reload and
  five source rejection controls. All three Linux profiles and both native Windows
  profiles passed. G28 also qualified a relocated packaged settings application.
- **Reusable libraries:** G28's `tests/project/packages.py` qualified dependency
  composition, exactly-once shared initialization, stdlib `math`, edited dependencies,
  locks, normal/O2 JIT/AOT and compiler-owned LSP completion/definitions/diagnostic
  repair. Its extra local+pinned offline consumer returned 83 in all recorded modes.
  G30's helper module adds a small composition observation; it is not a new package test.
- **Networking:** G28's maintained local TCP/UDP/DNS driver passed Linux Release,
  Debug and strict, including explicitly hosted async I/O/HTTP and cancellation.
  Native Windows sync source/echo/address controls passed both profiles. G28's full
  Windows driver lacked auxiliary executables; its fresh async lane remains unqualified.
  Older G07 native Windows async acceptance remains separate evidence, not a G30 rerun.
- **Collections/ownership:** maintained `tests/collections_stdlib/semantics.void`,
  `mutation.void`, `authority.void`, `faults.void` and the host/publication/reload
  controls cover stable ties, error cases, callback mutation, readonly, allocation
  failure and leases. Their [recorded validation](../collections/VALIDATION.md)
  is historical; G30 does not claim a fresh execution of every case.
- **Installed/editor use:** G29 repair recorded Linux/Windows installed journeys,
  source-denied Windows execution, and real VSIX editor tests. It also corrected
  dependency bootstrap, VSIX license inclusion and stale delivery metadata. Those
  accepted corrections are not newly discovered G30 deficiencies.

## 4. Findings, API consistency and limits

### Priority 1 — improve discovery, not functionality (nonblocking)

The [canonical index](../README.md) has useful stdlib links, but no complete public
module table. Console/convert/time/security/process guidance is split among Stage
10, combined-sprint and VOIDUP records; TCP/UDP's engineering link initially leads
through a historical architecture proposal. A developer looking for child output
capture or deterministic ZIP export should not need to reconstruct sprint history.
`stdlib/net/address.void:75–92` also has useful public helpers without `##` descriptions;
source-defined availability and rich hover documentation are different things.

**Smallest remedy:** one complete capability table linking existing authoritative
source/API guides and executable examples, plus short composition notes in the
relevant guides. Keep compiler-owned docs; no editor-specific API list or generated
second catalog. Preserve historical receipts and explicitly link later status.
This is a documentation follow-up, not a release-blocking runtime defect.

### Priority 2 — explain intentional error and ownership distinctions (nonblocking)

| Apparent inconsistency | Verified explanation / recommendation |
| --- | --- |
| Text search returns `-1`; collections search returns typed `Err`; built-in get/pop/UTF-8 use `result<…,string>` | Deliberate existing contracts. New controls reproduce built-in outcomes. Document the distinction; do not perform breaking broad error renaming. |
| Plain encode/random/unwrap can fail through checked Runtime failure while `try_*`/parse return domain errors | Checked allocation/resource failure is distinct from recoverable malformed input. Prefer checked variants for untrusted data; `unwrap_or` fallback is eager. |
| `console.readline()` cannot distinguish blank input, EOF and recoverable failure | Deliberate convenience policy. Existing `stdin.read_line()` returns explicit variants; no replacement input API is needed. |
| Copy-returning collection operations do not make child references mutable | Fresh outer storage is not deep cloning or readonly laundering. Fixed values copy; lists/dictionaries share identity unless explicitly copied. |
| Fixed arrays need a slice; generic map needs explicit types or a typed callback | Existing API and inference boundary, reproduced above. A fixed slice is an owned copy, not a borrowed view. Do not hide that cost. |
| Random accepts bounded call forms but ordinary functions do not overload | Closed compiler adapter for canonical declarations. `src/frontend/compiler.cpp:559–619` and source-private `_arity_*` helpers implement it. Function values retain their actual signature. |
| Stream reads use different EOF/progress shapes | Memory, file, console, TCP and UDP have different EOF/datagram/timeout semantics. Preserve checked progress and each documented result rather than unify by discarding information. |
| OS discovery vs filesystem security | `os` returns locations; `path` is lexical; `io` owns filesystem access. `path.normalize` is not sandbox containment. Private atomic save may reject a permissive directory; do not “fix” this by weakening ownership. |
| `process.start/capture` demand executable, environment and cwd | Intentional explicit launch authority and bounded capture. No shell/PATH fallback. A convenience wrapper must not import ambient execution authority. |
| `with`/`defer` vs successful close | Deterministic cleanup is available, but checked flush/close and atomic-save publication/durability may still need explicit observation. GC is not source `deinit`. |

### Existing functionality needing no new implementation

`stdlib/collections.void:63–110` already uses a nonrecursive stable merge over index
buffers and a private outer snapshot; bounds queries at `:131–181` reuse structural
version guards. The text implementation at `stdlib/text.void:151–207` uses a KMP
prefix/search loop and converts a found byte position to a scalar position. It is
not justified to call that literal-search algorithm quadratic.

`io/memory.void:165–177` publishes a new cursor/state after the existing private
`_memory_write`; ordinary growth retains spare capacity. `path` delegates to IO
path primitives. `convert.to_string` delegates to `string(value)`. `channel` wraps
built-in events. JSON has one source parser/writer and generated ordinary typed
adapters; no second serializer is required to configure an application.

### Boundaries requiring honest scope, not speculative additions

- Ordinary CLI execution is not an implicitly hosted async root. Source async
  networking/timers/process capture use the existing authorized host/Task machinery.
  Do not recommend a new scheduler or claim these APIs are absent.
- `channel.router<T>` is owner-thread synchronous pub/sub. It is not equivalent to
  a concurrent Go channel, worker queue or backpressure transport. Generic type,
  callback captures, event snapshots and depth-first nesting remain explicit.
- Unicode operations use scalars/exact UTF-8, not graphemes, normalization or locale
  collation. Date parsing, general CSV/YAML/TOML/XML, regex, specialized sets/heaps,
  image/database frameworks and compression families are optional candidates or
  external packages until an actual application establishes need. Compiler manifest
  parsing is not a public application TOML library. The numeric-file probe does not
  establish quoted/multiline CSV support.
- No general public crypto toolbox, secure token generator, HTTP server, WebSocket,
  raw socket or HTTP/3 functionality is claimed. Gameplay random must not generate
  security tokens. Narrow SHA-256/Ed25519 and ZIP capabilities retain their actual
  bounds and dependency requirements. New protocol/security scope needs its own
  application evidence and owner decision, not silent expansion under this audit.
- HTTP uses platform capability checks (including Windows certificate-store identity
  rather than supplied PEM/P12), bounded snapshots/queues and explicit errors. See
  [current platform limits](../net/http/NATIVE_PLATFORMS.md). macOS current release
  qualification cannot be inferred from these Linux/Windows probes.
- Historical LLVM19/async crash incidents remain unresolved where classified that
  way. These passing probes do not prove their repair. External raylib/Gallium
  strict leak observations remain unsuppressed; G30 ran no graphics tests.

## 5. Representative future performance baseline

G30 ran no cross-language benchmark campaign and established no new latency gates.
Probe wall times include compilation/launch where applicable and must not become
stdlib throughput claims. Use the existing fixtures with current binary/source
identities; preserve unfavorable observations.

| Area | Reusable fixture / representative task | Controls needed for a fair comparison |
| --- | --- | --- |
| Strings/Unicode | `tests/text_encoding/performance.void` / `.cpp`: literal search/split/replace, case mapping and codecs, ASCII and multibyte inputs | Same valid/invalid UTF-8, scalar results, owned output, limits and checksums; separate byte search from scalar-index conversion. No repeated scalar indexing substituted for the actual KMP algorithm. |
| Numerics | `tests/math/`, `tests/random/`, accepted SIMD fixtures | Exact widths, wrap/conversion behavior, float NaN/signed zero/ties, matching seeded algorithm or clearly different algorithm tests; no fast-math; report JIT/AOT separately. |
| Collections | `tests/collections_stdlib/benchmark.py`: ordered/reverse/duplicate/mixed stable sorting, bounds, map/filter/fold, dictionary snapshots | Preserve input, stable ties, comparator work, fresh outputs, readonly/value-copy semantics. Include setup/allocation consistently. Separate large fixed children and text comparisons. |
| JSON | `tests/json/measure.py`: save/config/deep64/wide128 parse and stringify | Reject duplicate decoded keys, preserve order and numeric precision, match DTO conversion/limits/metering. Current stringify includes fresh DOM conversion; comparing only another language's writer is not equivalent. |
| Filesystem/logging | Existing `tests/log/performance_*`, `tests/net/http/io_performance.*`, memory benchmark | Same 200 actual writes/acknowledgements where FILE200 is used, exact bytes, final durable sync and timer boundaries. Report CPU vs sync/host variation separately; no hidden buffered comparison. |
| Networking | `tests/net/http/performance.*`, local socket source fixtures | Loopback/peer setup, protocol/TLS verification, cold vs reused connections, payload/chunk bounds, cancellation/drain and concurrency held equal; no public-internet latency contest. |
| Allocation/ownership | `tests/runtime/root_lookup_measure.cpp`, `tests/memory/benchmark.py`, collection host controls | Same payload graph, independent snapshots and retained references; GC enabled; report live/retained memory separately from process RSS/high water. Observation instrumentation separate from timing controls. |
| Tasks/channel | `tests/channel/performance.py` / `benchmark.cpp`, async retention and one-worker network host fixtures | Compare synchronous typed pub/sub against synchronous callbacks, not Go queue transfer. Separately measure structured async completion/cancel/drain and repeatable results. |

Official Go references reinforce the need to match semantics: stable sorting mutates
its input slice, whereas VOID `sorted` returns an independent outer list
([Go slices](https://pkg.go.dev/slices#SortStableFunc)); `File.Sync` commits file
contents to stable storage, which must be included when comparing durable writes
([Go os](https://pkg.go.dev/os#File.Sync)); string search/split contracts are not a
substitute for VOID's scalar-index and bounded-result contract
([Go strings](https://pkg.go.dev/strings#Index)). No Go execution occurred here.

The historical 65,536-byte text workload's 6.688 seconds is not a fresh baseline.
The later LLVM23 [internal repair](INTERNAL_PERFORMANCE_REPAIR_RESULT.md) recorded
1,860.165 ms for its retained O2 text/codecs case under that experiment's conditions.
Likewise [collection timings](../collections/BENCHMARKS.md) are LLVM19-era evidence,
not today's measured speed. JSON wide-field metering and fresh DOM construction,
text comparator UTF-8 copies and fixed-value copies remain legitimate measurement
subjects; their presence alone does not authorize optimization or a replacement engine.

Preserve the accepted [AOT-only SIMD scope](AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md):
seven-job acceptance at `175c08c8`, run `42231556437`; `finish` receives the AOT
TargetMachine, JIT remains unchanged, SLP remains explicitly disabled. Current
`src/backend/llvm/backend.cpp:86–97,2630,2772,2995,3015` agrees. Older strategy wording
about pending cross-platform qualification is superseded by that acceptance record.
General checked-array/loop optimization stays deferred; G30 does not reopen it.

## 6. Priorities and one minimal handoff

| Classification | Finding / action |
| --- | --- |
| New release blocker | **None demonstrated by this audit.** No new hold or speed target. |
| Existing release obligations | Keep Linux binary ABI floor, current macOS qualification, exact VSIX/npm dependency-security review, public VOIDUP trust/distribution, supported-platform declarations and Boss-controlled comprehensive CI in G29's owner-controlled work. |
| Small justified usability follow-up | Complete canonical stdlib navigation and add concise composition/ownership/error notes linking existing examples. Source hover omissions can be documented for an authorized docs-only touch; no parallel editor catalog. |
| No implementation justified | New collection/text/JSON/async engines, general overloads, broad API renames, crypto/protocol expansion or optional formats without a concrete requirement. |

**Recommended Keyboard scope, only if Boss chooses the documentation follow-up:**
update the canonical index/current API guides as one bounded documentation task.
List all 29 modules, distinguish builtins, link G04/G06/G07/G09/G10/G12 and existing
configuration/socket/collection examples, and explain fixed slicing, generic callback
types, typed-vs-string errors, checked close/publication and async hosting. Link
current acceptance rather than rewrite historical evidence. Reuse the archived
G30 CLI composition if promoting one maintained example provides value; do not add
an argument-parser framework or duplicate tests merely to increase counts.

Acceptance is accurate links/signatures and runnable touched examples through the
existing checker and supported execution paths, retaining ownership and failure
behavior. If a new product failure appears, preserve it and narrow the repair before
expanding scope. A docs pass requires no whole CI campaign or performance target.
No production implementation is required for the tasks already demonstrated.

## 7. Evidence, replay, archive and cleanup

Archive: `/home/shmellyorc/Documents/Void.Language-archives/g30-stdlib-audit-2026-11-13.zip`.
SHA-256: `cf2c3a0a31e5e5da4a01b73e6fa38cd328f1caec54c8d3b4483bd746d1119c3a`.
39 preserved source/script/JSON/text files, 79,021 uncompressed bytes; 31,158-byte
DEFLATE-9 ZIP plus its manifest member. CRC and actual extracted SHA-256 verification
passed for every inventoried file, followed by rehashing originals before cleanup.
The external archive index records baseline, identity and verification.

The archive contains `sources/main.void`, `sources/helper.void`, valid/invalid inputs,
`probe.py`, all five profile commands/stdout/stderr/status/identities, the three
composition controls, the 29-module inventory, Windows shell/toolchain/private-root
receipts, guest inventory/storage and G28 binary equivalence. No binaries, active
builds, SDKs or provider copies were archived. No unexpected failure evidence exists
to discard; the expected rejection and application failures remain in raw receipts.

Replay by verifying the ZIP/manifest, extracting to a fresh private workspace and
running `python3 -B probe.py /absolute/path/to/build linux-release` (or the matching
profile; `linux-strict` activates fatal sanitizer settings). Windows uses the same
script under the x64 developer shell with a verified private NTFS workspace and the
matching native build. The archived guest launcher records this machine's specific
paths; do not assume those paths on a different installation. The source baseline
and matching installed stdlib/dependencies remain required inputs.

Storage and final changed-file inventory are recorded below after verified cleanup.

- Native Windows task `C:/Users/a/VoidG30Research-20261113`: all 13 inventoried files rehashed before removal; **12,374,295 logical bytes removed**. Guest free space was 8,815,726,592 bytes before qualification, 8,803,135,488 after qualification, and 8,815,206,400 after cleanup. The immediate cleanup free-space delta was 12,398,592 bytes; concurrent filesystem activity explains differences from logical size.
- Linux scratch `/tmp/void-g30r-j7ypt_qn`: **92,924,646 logical bytes removed**; free bytes immediately before/after cleanup: 16,417,038,336 / 16,510,107,648. Shared transfer root `/home/shmellyorc/VM/g30r-20261113`: **29,897 logical bytes removed**. All three task roots and the temporary location pointer are absent.
- Cleanup verification is retained beside the archive as `g30-stdlib-audit-2026-11-13.cleanup.json`. Active builds, SDKs, providers, guest source, earlier evidence and retained G26–G29 work were untouched.
- Final repository inventory: only this new untracked report, `docs/compiler-health/G30_STDLIB_COMPLETENESS_AUDIT.md`. No tracked diff or staged change; HEAD remains the assigned baseline. Outside the repository: one verified ZIP, cleanup receipt and an entry in the existing archive index.

**Answer:** an ordinary developer can build the examined common applications with the current stdlib. The smallest justified improvement is clearer discovery and composition guidance. No new stdlib engineering milestone is supported by these results. Stop for Golden Wrench/Boss review.
