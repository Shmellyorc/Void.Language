# Current HTTP engineering checkpoint

30 October 2026. LLVM/Clang/LLD 23.1.4; source baseline
`15f7868b0af177f8f7d49c548392c79c7bd921d2`. The full mission is **unfinished**.
The [curl workflow correction](VENDORED_PROVIDER.md) is implemented directly in
the repository. The [full-provider OpenSSL strict conflict](DEPENDENCY_CONFLICT.md)
was isolated to upstream callback-type undefined behavior. Boss approved the
exact unmodified Linux AWS-LC 5.12.0 pin; it is now selected and the production
strict integration passes. No sanitizer or public contract was weakened. This checkpoint is not a full HTTP delivery or independent review.

## Implemented source and build integration

- `third_party/curl/` is the authoritative curl 8.23.0 library source, left
  unstaged for Boss. Its manifest verifies all 1,586 retained upstream files,
  with exactly the four recorded private corrections. Original license,
  version, signed-archive identity and review-diff checksum are retained.
- `cmake/CurlProvider.cmake` builds that tree directly. There is no production
  patch-application step or required ZIP. Regression feature profiles and the
  full production dependency profile are distinct. Root tests remain enabled.
- `third_party/http-versions.json`, `tools/setup_http_deps.py` and
  `tools/verify_http_dependencies.py` pin, build and verify the dependency closure
  in disposable storage. Explicit paths prevent ambient provider substitution;
  profile mismatches, altered binaries, escaping shared-library aliases and
  altered corresponding-source archives are rejected. The full provider has
  HTTP/HTTPS only, nghttp2, c-ares, IPv6, PSL, large offsets, AWS-LC and IDN.
  Curl's automatic content decoders are disabled for future bounded decoding.
- The current bootstrap/import recipe supports **Linux x86_64 only**. Native
  Windows/macOS recipes and acceptance remain missing; root HTTP configuration
  on those platforms currently stops explicitly. This is an unfinished platform
  integration, not a cross-platform implementation claim.
- `cmake/NativeBuild.cmake` extends the existing native manifest to v2 with the
  dependency archives/shared libraries, link order and install-relative path
  expansion. Existing v1 manifests remain readable. Staging, atomic executable
  publication and VFS paths were not rewritten. The cache identity now includes
  the actual provider/dependency binaries and policy/provenance inputs.
- Install metadata carries the runtime closure, licenses, pinned PSL and
  unmodified GNU IDN/Unicode corresponding-source archives. Those two shared
  libraries remain replaceable. The installed compiler's independently linked
  AOT program was tested after relocating the install tree.
- `src/runtime/http_provider.cpp` owns one matching libcurl global reference
  across native leases, preserves a host's independent initialization, serializes
  first/last use and checks exact version/required features. It installs no
  process allocator, locale, environment or signal policy. A real Runtime HTTP
  driver lease now lives through native driver shutdown. Owner-side resource ledger integration now exists; compiled public HTTP source acceptance is recorded below.

## Shared URL foundation

`stdlib/url.void`, its private checked ABI, frontend recognition, typed IR
validation and common LLVM lowering implement all approved URL signatures:
parse/resolve/normalize, immutable address fields/canonical serialization,
strict component text/byte decoding, encoding, ordered query pairs and replacement.
CURLU is the structural parser. Native validation rejects userinfo, invalid
ports, zone identifiers, controls/backslashes, invalid percent grammar and limits.
Encoded reserved bytes, path dots by default, duplicate query order, empty query
and fragment markers and bare-versus-empty pairs are preserved.

On Linux the explicit UTF-8 IDN API maps the parsed host before giving its ASCII
form back to CURLU. The initial curl Unix punycode path depended on process locale;
the real Unicode test caught this. No `setlocale` call or host mutation was added.
Private opcodes require the exact stdlib module, arity, input/reply geometry and
query-item identity; owner/worker and readonly authority rules remain checked.
The source facade snapshots input lists and publishes results through existing
precise Runtime roots. It adds no URL handles, GC representation or public FFI.

The native URL test also checks host initialization before/after VOID leases and
concurrent parsing/lease acquisition. URL docs, navigation, read-only stdlib rename
and public completion inventory pass through the actual LSP.

## Actual local evidence

| Scope | Outcome and evidence |
| --- | --- |
| Corrected curl, isolated cookie profile | Debug, Release, strict, disabled cookies and disabled HTTP pass; [five configurations and receipts](VENDORED_PROVIDER.md) |
| Full provider, Debug | Cookie failure/empty/partial-list/embedding and native URL/provider checks [pass](evidence/vendored-full-Debug-20261030T142845121409Z/commands.json) |
| Full provider, Release | Same checks [pass](evidence/vendored-full-Release-20261030T142843951621Z/commands.json), with a [second clean build](evidence/vendored-full-Release-20261030T144058199486Z/commands.json) |
| Clean full Release archive identity | Both directories produce SHA-256 `59eb3a2120f5ee651e22286137e8afe7bd0d24355bfd0766f3df61326d397b8b` |
| Full provider, strict | **FAIL** during upstream OpenSSL initialization; [isolated proof and trace](DEPENDENCY_CONFLICT.md) |
| URL Release | [4/4 CTests](evidence/url-ctest-Release.log): native checks, 10 forged-IR rejections, source check/JIT/O2/AOT, LSP |
| URL Debug | [3/3 CTests](evidence/url-ctest-Debug.log): native/IR/source, including 7 source processes |
| URL strict, pure helpers | [7 source processes pass](evidence/url-helpers-strict-halt.log): percent text/bytes and ordered query helpers in JIT/O2/AOT, with halt-on-error sanitizers; this does **not** test parser/provider initialization |
| URL strict IR | [10 forged-IR rejections pass](evidence/url-ir-strict.log) |
| Runtime regression, Release | [2/2 native tests](evidence/provider-regression-Release.log) plus [8/8 source/lifecycle/activation/retention tests](evidence/provider-runtime-source-regressions.log) |
| Runtime regression, Debug/strict | [Debug 2/2](evidence/provider-regression-Debug.log), [strict 2/2](evidence/provider-regression-strict.log) |
| Native Runtime stack | [Six processes](evidence/vendored-runtime-stack.json), soft/hard 1 MiB enforced and queried before exec, no native failure |
| Install relocation and independent AOT | [os_install passes](evidence/provider-install-relocation.log), exercising the extended manifest/closure |

The earlier table records foundation checks before native transport work. New
continuation evidence below supersedes its former "no network requests" status;
it does not supersede the original OpenSSL strict failure.

## Authorized native transport continuation

`include/void/http.hpp` and `src/runtime/http.cpp` implement the native multi
kernel. A single native driver thread owns all easy/multi/share operations, with
only documented multi wakeup crossing threads. Native completion tickets contain
no managed pointers or source callbacks. Finite client/transfer slots, conservative
512 MiB reservation admission, fixed receive/upload rings and owner publication
leases are implemented; this reservation is **not yet a certified RSS ceiling**.
Curl callback entry never allocates managed data or waits for a source producer.

The loopback [first real HTTP/2 receipt](evidence/http-native-h2-initial/commands.json)
passes actual TLS ALPN, two simultaneous streams on one allowed connection and a
2 MiB flow-controlled response. The same process passes HTTP/1.1, duplicate/binary
headers/body, header-before-body streaming, known/unknown uploads, an 8 MiB pull,
gzip/deflate, decoding limits, redirects, cookies, timeout, inspectable 404,
untrusted TLS rejection and explicit CA trust. A poisoned `SSLKEYLOGFILE` creates
no file. Test-only HTTP/2 wheels are exact-pinned in
`tests/net/http/python-versions.json`; `tools/setup_http_tests.py` verifies and
extracts them into disposable storage. They are not application dependencies.

A real timeout exposed removal-time curl progress callback reentry while the
transfer mutex was held. The [debugger trace](evidence/http-native-loopback-gdb/native.stdout)
is retained. Removal/cleanup now run with that mutex released and publication
waits for detachment. Further duplex, early-error and cancellation probes are
under development; the source API and full policy matrix are still unfinished. Native bulk kernels
pass current-cursor, non-2xx no-write, short input and partial IO-cause checks;
the typed source file-borrow integration is still in progress.

Native Task and lexical-group bookkeeping now retires only after existing precise
GC proves the source task handle unreachable and all work, dependents, children,
results and diagnostics are quiescent. Retained handles preserve repeated await.
Monotonic sequence/group identities preserve ordering after list erasure. No weak
root representation, collector or scheduler was introduced. Focused Release
6/6 and strict 4/4 async/source/lifecycle tests pass. Actual retention observers
run 16 processes per profile, including O2 and source-worker variants, with at
most three settled Task records and one group at 128 iterations:
[Release](evidence/record-retirement-release-results.json),
[strict](evidence/record-retirement-strict-results.json).
These results address the concrete native metadata leak; sustained **HTTP source**
streaming still needs its own lifetime and memory proof after source integration.

Original failed harness receipts remain intact: incorrect nghttp2 CMake keys,
libidn2's initially omitted support-library build, compression cache options,
disabled-probe JSON format, native target names before regeneration, the URL
fixture's forbidden type-name shadowing, the Unicode-host test and an early
strict helper launch before the final compiler link. The latter failed native
manifest recognition with the old executable; the completed executable passes.
The status/output recorder labels some such incidents as crash evidence; these
exit-1 harness/compiler diagnostics are not reported as compiler process crashes.

## Remaining mission and review boundary

The native kernel, Runtime resource bridge, public `net/http` facade and native
bulk/file continuations are implemented and undergoing focused integration.
Complete source/negative-wire/TLS/proxy/auth coverage, native platform recipes,
consolidated performance and acceptance remain unfinished. The provisional
512 MiB admission envelope still needs provider allocation audit and measured
validation. No full memory guarantee or complete HTTP handoff is claimed.

The OpenSSL dependency conflict is resolved for the Linux profile by the approved
unmodified AWS-LC pin. The original strict failure remains preserved. The curl
correction does not require Boss to apply or copy anything. No upstream report,
Git staging/commit/branch/push, hosted CI, release or protected root README
modification was performed.

## Current production integration after explicit follow-up decisions

Boss approved the [exact Linux TLS pin](TLS_REVIEW.md) and
[existing-pool lifetime extension](NATIVE_IO_LIFETIME_REVIEW.md). Production
prefixes now live under `.toolchains/http/prefix/linux-aws-lc-5.12.0/PROFILE`;
old OpenSSL prefixes/binaries remain intact. The bootstrap builds every dependency
fresh for each profile, including fully instrumented AWS-LC. CMake invalidates
cached provider feature checks when the pin manifest changes. Original LICENSE
and NOTICE are retained and installed. No source patch or checker suppression
is used for TLS.

The source worker dispatch now uses the same Runtime-owned pool as native-only
file producers. Scopes still own/drain their Work and generation leases. Tests
caught and corrected an unused constructor bypassing wake initialization. A second
integration attempt wrongly waited for/cancelled already admitted source Work at
Runtime.close; existing parallel/lifecycle tests exposed the semantic change.
The final implementation preserves the original contract: admitted source Work
remains Scope-owned, while Runtime-owned file producers are stopped/drained.
Final pool join is deferred until that Scope leaves. Original failed logs remain.

Focused integrated production results:

- [Release 8/8](evidence/runtime-pool-scoped-drain-tests-Release.log).
- [Strict 8/8](evidence/runtime-pool-scoped-drain-tests-strict-final.log), full
  ASan/UBSan, leak detection and halt-on-error.
- [Debug native 3/3](evidence/runtime-pool-scoped-drain-tests-Debug.log).

These include actual HTTP, external completion, source IO/acquisition, source
async, parallel normal/O2, compatible reload/generation leases, allocation
rollback, structured cleanup and shutdown. The new worker probe verifies native
submission outside any async root, three roots reusing the same pool/thread,
idle worker-limit reconfiguration, busy rejection and pre-wait stop notification.

A strengthened required-h2 fixture detected an incompatible HTTP/2 preface sent
to a TLS endpoint without h2 ALPN. The native pre-request hook now checks the
selected protocol through the documented TLS session API before request bytes.
All three production HTTP fixtures now report zero incompatible requests.
The original [failed no-send proof](evidence/http2-required-no-send-initial.log)
is retained; an eventual response-version error was not sufficient enforcement.

Actual 2 GiB Runtime async pulls preserve a repeatedly awaited first task, while
unreachable results and native records retire:

- [Release receipt](evidence/http-native-2GiB-Release/commands.json): 129 samples,
  live managed payload 65,634 bytes, 3 Task / 0 explicit group / 1 Work records,
  at most 7 persistent handles, RSS 16,404 KiB throughout the sampled interval.
- [Strict receipt](evidence/tls-review-aws-lc-2GiB-strict/commands.json): 96 samples,
  the same live payload and native record counts; RSS 218,636–424,024 KiB,
  ending at 423,412 KiB under ASan. This includes sanitizer allocation overhead;
  it is not a production RSS ceiling.

These use actual transport and Runtime activations, not yet a compiled public
`net/http` source loop. Full source/JIT/O2/AOT byte-array publication still needs
its own integration proof. Multipart native file producer/cancellation work is
now under focused development, using the approved pool and existing IO kernel.
The public HTTP facade, complete source bridge, native platform recipes,
provider memory audit and full performance/acceptance matrix remain unfinished.


## Public source and native file continuation integration

`stdlib/net/http.void`, `http_ops.hpp`, `http_abi.hpp`, `http_abi.cpp` and the
frontend/IR/LLVM HTTP cases now connect actual source programs to the multi
engine. Private records have checked exact module identities, field shapes,
resource authority, async effects and worker restrictions. Source and independent
IR capability analysis both record client/transfer/file mutations. No generic
FFI, managed callback on the driver or source scheduler was added.

The source facade implements managed client/transfer factories, buffered and
streaming sync/async pairs, bulk upload/download, multipart file parts, cookies,
capability inspection, headers, strict UTF-8/BOM/charset handling and typed errors.
The four configuration records use the explicitly [approved value form](SOURCE_API_REVIEW.md).
Default call forms and all fields remain. Capability inspection is the public
`capabilities()` constructor, preserving both the type and callable spelling.

File workers now execute bounded IO steps through the same existing queue. A
pending network operation carries only a native completion ticket and native
wake state. The queue skips that pending file item while servicing ready source
Work or other native file steps. Source Work still runs once under its owning
Scope. Upload EOF releases the worker before a response is received. Download
checks final 2xx headers before writing, and both directions retain existing IO
borrows through quiescent completion. Streaming transfer adapters retain their
file borrows until finish/close. Ledger quiescence precedes file close regardless
of resource map order.

Actual new Release results:

- [Public facade, 9 process checks](evidence/http-source-public-Release/commands.json):
  source check, sync JIT and independently linked AOT in both optimization modes;
  async source normal/O2 with one worker, forced GC and actual 128 MiB pulls.
  Repeated awaits preserve a retained first task/chunk. Sampled post-GC live
  payload stays 208,655–211,727 bytes, with 10 tasks / 2 explicit groups / 1 Work
  and 11 registered Handles. Root drain leaves zero managed objects; Runtime
  shutdown retires the idle driver's null Handle too.
- [Native file/network fixture](evidence/http-file-queue-fixed-Release/commands.json):
  mixed byte/file MIME, zero/known/unknown counts, repeated source-file cursor,
  known short EOF, blocked filesystem cancellation and quiescent close pass.
  A 32 MiB producer whose peer refuses consumption until a second file request
  arrives progresses with one worker. Multipart input and native file output
  progress together on that same worker. The complete native TLS/h1/h2/proxy/
  auth/decoding fixture and 128 MiB Runtime pull checks pass.
- [Worker/external completion 2/2](evidence/http-file-queue-fixed-runtime-Release.log).
- Earlier [Debug](evidence/http-file-owner-debug/commands.json) and
  [strict](evidence/http-file-owner-strict/commands.json) file producer tests pass
  under enforced 1 MiB stacks. Those precede the yielding-queue/download change;
  new Debug/strict builds are running, not yet counted as passes.

Retained integration failures include an omitted frontend mutation summary
(`http-source-first/`), test-harness null Handle accounting, a sample cadence
that supplied only three points, and a harness shutdown call outside NativeSafe.
The queue's first readiness method accidentally matched the pre-existing source
Work completion predicate, so ordinary source Work never ran. The native worker
probe and native HTTP fixture timed out, and the launched debugger captured the
idle worker/root wait (`native-worker-yield-hang.log`). Renaming the queue method
`queue_ready` preserves the separate predicates; the focused worker and complete
native fixture pass afterward. No test was removed or sanitizer suppressed.

New helper/bulk source cases, strict source publication and broader consolidated
regression checks remain in progress. The Release source receipt above is a
focused integration result, not full protocol/platform/memory/performance acceptance.


## Consolidated local integration and further close checks

The complete focused set passed with actual 1 MiB inherited stack limits:
[Release 12/12](evidence/http-source-queue-integration-release.log),
[Debug 12/12](evidence/http-source-queue-integration-debug.log), and
[strict 12/12](evidence/http-source-queue-integration-strict.log). Strict retains
ASan/UBSan, leak detection, halt-on-error and stack traces. The executables were
rebuilt before those runs, including the existing async/IO regression harnesses.
The set covers actual HTTP, source JIT/AOT, native URL/provider inspection,
15 forged HTTP IR rejections, 5 source authority/private/worker rejections,
IO source/acquisition, worker shutdown, external completion and async source,
parallel and lifecycle/reload. These are focused local checks, not the full root
matrix or all protocol/platform/performance acceptance.

Public source receipts and samples are preserved in `evidence/http-source-queue-`
`{release,debug,strict}/`. Across normal/O2 128 MiB pull runs, each profile has 14
post-GC observations: Release 208,655–211,727 managed payload bytes; Debug
208,655–214,799; strict 211,727–214,799. All sample at most 10 native task records
and 11 Handles. The retained first task still repeats its result after later GC.
Root drain leaves zero managed objects and Runtime shutdown removes the idle
provider's null Handle.

A native inspection probe then exposed a genuine repeated-configuration trust
bug. Normal `auto` variables and cached `CURL_CA_*_SET` flags produced a curl
configuration with no default CA locations. The original configuration and failed
probe are in `evidence/system-trust/`. Curl now owns cached native distribution
discovery, its stale discovery flags are reset, and official Linux configuration
requires a usable location. Two successive configurations retain
`/etc/ssl/certs/ca-certificates.crt` and `/etc/ssl/certs`; the native inspection
probe passes in every profile. A manual public-source request to curl.se returned
HTTP 200 over actual HTTP/2 using default distribution trust, without a custom
bundle. That external witness is not a deterministic loopback CTest.

Subsequent file close integration adds per-client weak native job observations
inside the existing owner resource adapters. They introduce no managed weak-root
framework or transport reference to Runtime. Unentered producers can retire
without a worker for explicit close, cancellation and failed publication.
Native file sinks signal quiescence to the transport. Client close waits for
entered file IO and can finalize queued sink cleanup on the owner after native
network close, without depending on an occupied worker. Delayed cleanup erases
only its own ledger resource, preserving a newer client/transfer at a reused key.

The latest [Release native receipt](evidence/http-file-sink-close-current-Release/commands.json)
passes queued upload/sink close while another file operation occupies the only
worker, native progress during a blocked sink, entered IO drain, exact file
progress and no IO after close returns. The expanded
[public source receipt](evidence/http-file-sink-source-Release/commands.json)
passes normal/O2 forced-GC async file download, client close of an active download,
released file reservations and zero root retention, alongside sync JIT/AOT.
Expanded Debug/strict checks for this later sink change remain to be run.

The original sink-barrier assertion incorrectly expected no write after the
cancel signal. Existing IO semantics permit an entered syscall to finish; close
must drain it. The failure is retained in `http-file-sink-close-Release/`.
The corrected check compares actual file progress and verifies the cursor cannot
change after close returns. No production cancellation rule was weakened.


## Cleanup controls and failure recovery

Client/transfer destruction and acquisition rollback use dedicated checked
`http_control` operations with no reply construction. Native resource closure
therefore completes during owner allocation failure without allocating a result.
The independent IR test rejects 35 forged module, authority, worker, opcode and
result-shape combinations, including both cleanup controls. The source facade
retains result-producing explicit close and its async counterpart.

The consolidated cleanup checkpoint passes **13/13 tests in each profile** at
1 MiB stacks: [Release](evidence/http-cleanup-final-tests-release.log)
(143.43 s), [Debug](evidence/http-cleanup-final-tests-debug.log) (357.07 s),
[strict](evidence/http-cleanup-final-tests-strict.log) (328.78 s).
Strict enables leak detection and halt-on-error ASan/UBSan without suppressions.
The source suite executes 15 analysis/JIT/native-link processes, including
independently linked async AOT at normal/O2 and a 128 MiB source pull.
Allocation sweeps recover 904 synchronous and 1,040 async owner allocation
failures in each optimization mode, with precise-root balance, zero remaining
managed objects/client/transfer slots, and a successful subsequent invocation.
Receipts and source identities are retained under
`evidence/cleanup-control-{release,debug,strict}/`.

The earlier fault harness incorrectly forced collection inside its allocation
failure callback and terminated the probe itself. Its failed receipt remains;
the corrected sweep follows the IO fault harness and exercises actual Runtime
allocation/collection failures without imposing that harness-only condition.
These sweeps do not claim to inject failures in dependency malloc calls or native
worker threads; the corrected curl's separate enumeration probes cover its
allocator failure paths.


## Form, URL cause, cookie census and sustained source proof

The private HTTP form operation reuses the URL percent codec with the HTTP
64 MiB graph/output budget. Native snapshot metadata is charged before reserve;
escaped output expansion is checked before output allocation. Public URL query
encoding retains its 16 KiB limit and its original LSP inventory. A private
`_form_item` value record is checked through the existing HTTP ABI rather than
adding a public URL helper. The independent IR suite now rejects 43 forged
operation/module/type/authority/control/worker combinations.

HTTP errors preserve the shared parser's URL error kind and attributable byte
offset. Source tests check malformed-escape causes. Empty-reference resolution
now retains the base path/query and clears its fragment, following RFC 3986;
explicit empty fragments remain present. The URL source fixture checks all
17 outputs in JIT/O2/AOT.

Cookie census runs immediately after each provider-processed Set-Cookie header,
on the driver thread using the inactive inspector. This prevents a multi batch
from assigning the overflow error to the wrong response. The original concurrent
H2 reproduction fails; the corrected driver fails the first overflowing response,
clears its jar before sibling headers, and independently enforces the 1024-count
and 1 MiB serialized-byte budgets. No additional cookie parser or provider patch
was introduced.

The private-bridge source checkpoint passes 8/8 Release and 9/9 Debug checks;
strict passes its eight source/IR/URL/LSP checks, while the new native H2 memory
probe exposes the dependency gate below. Public source execution includes
15 analysis/JIT/AOT processes per profile. Expanded form-inclusive allocation
sweeps recover **1,448 synchronous and 1,607 async failures per optimization mode
in all three profiles**, with zero remaining source/native resource roots and
successful reuse. Receipts: `evidence/http-private-form-faults-PROFILE/`.

The actual compiled public-source 2 GiB run was repeated after the private ABI
change: normal/O2 JIT and independently linked normal/O2 async AOT pass. Across
262 forced-GC samples, live managed payload remains 212,924–215,996 bytes;
Task/group/Handle maxima remain 10/2/11. Retained tasks remain repeatedly
awaitable; all source roots retire, and Runtime shutdown retires the idle driver
Handle. AOT uses the ordinary Runtime's 256-byte collection budget, rather than
the forced-GC observer. Exact per-process RSS samples and source/compiler hashes
are in [the receipt](evidence/http-private-bridge-2GiB-release/commands.json) and
[measurements](evidence/http-private-bridge-2GiB-release/measurements.json).
This is sustained application proof for that loop, not a universal provider RSS
ceiling or completion of the remaining memory audit.

All 34 public functions/constructors now carry source documentation, including
parameter and result semantics. Complete type/field/enum documentation remains
unfinished. HTTP import/docs/definition/read-only rename/public completion checks
now exercise the real LSP. An interim public underscore URL helper was caught
by the inventory test and removed; it is not part of the implemented API.

## New exact-review gate: HTTP/2 duplicate window credit

The larger native pressure test uses three paused 512 MiB responses, four small
application resume cycles and fast sibling requests on a single H2 connection.
It measures both resident memory and, under ASan, actual live requested bytes.
The original pinned provider grants two identical window increases per stream;
its pause buffers exceed the existing native reservation. A seven-line deletion
in `lib/http2.c` removes the redundant delta update after the absolute setter.

The isolated candidate passes native Debug/Release/strict, the packet-credit
invariant and 15 source execution checks per profile. Strict live requested
allocations remain at 52,270,438 bytes through the pressure/resume interval,
within the unchanged 71,303,366-byte reservation. No queue, admission, body,
protocol or sanitizer limit was relaxed. The original failed RSS-only assumption,
real live-allocation failure and peer traces remain preserved.

[HTTP2_WINDOW_REVIEW.md](HTTP2_WINDOW_REVIEW.md) contains the actual exact diff,
all checksums, reproduction commands, original failures and upstream follow-up
record. This is a **fourth curl source file**, beyond the approved cookie fix,
so Golden Wrench exact approval is pending. Authoritative curl source and
production pins remain unchanged; the full production HTTP acceptance gate
therefore remains failing. Candidate tests replace one archive member only in
disposable storage. After approval Keyboard can integrate the exact source change
directly; Boss has no manual patch/dependency-copy task.

The full mission still requires DNS/cache and TLS-material allocation audit,
native Windows/macOS build recipes and acceptance, additional wire cases,
complete declaration docs, install/tooling/performance and consolidated acceptance.
No Git staging, commit, branch, push, hosted CI or protected root README edit was
performed.


## Approved HTTP/2 correction: production integration

Boss approved the exact fourth-file correction, patch SHA-256
`f109b12c2c5b327fb9b2da2b9d302cb111d936097b98ace830ea886db7d4f1a2`.
It is now applied directly to `third_party/curl/lib/http2.c`, whose corrected
SHA-256 is `6237bdfb17bcb6670be046520ab9170a21ef7e946e9149df17f7e56cf6313914`.
The 1,586-file inventory verifies four corrected files and both exact review
diffs. There is no production patch step or dependency copying for Boss.

Production verification passes **17/17 tests per profile** at 1 MiB stacks:
[Release](evidence/http2-approved-tests-release.log) (296.38 s),
[Debug](evidence/http2-approved-tests-debug.log) (561.63 s),
[strict](evidence/http2-approved-tests-strict.log) (517.92 s).
These include the previously failing native H2 reservation/credit case, source
JIT/O2/AOT, allocation failures, LSP, URL, IO acquisition and async lifecycle.
Strict retains leak detection and halt-on-error ASan/UBSan throughout. The
HTTP/2 window gate above is therefore resolved; full HTTP acceptance still
requires the remaining work and memory audit.

The isolated review tool now reconstructs its old control member from the
checksum-verified signed release, so the old/new comparison remains reproducible
after authoritative integration. It does not revert or patch production source.


## DNS cache and precise unsent failures

The pinned c-ares adapter retains at most 256 addresses per family, but curl's
persistent cache permits 29,999 entries. TTL alone therefore cannot bound a
sequence of distinct target/proxy names. Driver configuration now clears only
the multi-owned DNS cache after eight configured hops, using the supported
`CURLMOPT_NETWORK_CHANGED` / `CURLMNWC_CLEAR_DNS` trigger. Connections and live
resolutions remain valid; ordinary cache reuse, negative caching and configured
TTL remain within each capacity epoch. No resolver, parser, global environment
setting or second worker pool was introduced.

A native-only fixture authority supplies per-easy loopback DNS servers; it is not
part of the source API. Actual UDP/TCP peers verify cache reuse/finite eviction,
negative caching, UDP truncation/TCP fallback, IPv4/IPv6, canonical IDN queries,
cancellation and connect timeout. Distinct 300-address replies exercise the
provider's retained-address cap. Strict retained live allocations stay at
270,685–386,234 bytes across the steady pressure interval (228,101 baseline),
with unchanged strict instrumentation.

Cancellation during DNS previously reported an unknown remote outcome despite
sending no HTTP bytes. The driver now records origin request permission and
actual request/upload progress across hops. Proxy CONNECT traffic alone does
not count as an origin request. DNS failures/cancellation remain known-unsent;
post-send framing and timeout failures retain unknown outcome. The failed
[DNS receipt](evidence/http-dns-before/commands.json) is preserved.

Focused production DNS/native/source checks pass
[Release](evidence/http-dns-fixed-tests-release.log),
[Debug 3/3](evidence/http-dns-tests-debug.log) and
[strict 3/3](evidence/http-dns-tests-strict.log). This completes the current DNS
pressure regression, not the remaining TLS-material allocation audit.


## Independent HTTP/1 wire and redirect probes

A literal TCP peer, independent of the production HTTP parser, now tests
chunk/trailer framing, HTTP/1.0 metadata, conflicting length/TE-CL framing,
101/interim overflow, header/trailer count/byte limits, truncation prefixes,
gzip checksums/end/trailing data, raw-deflate rejection, unknown/stacked encodings,
raw representation preservation and read-deadline progress.

The probes reproduced and repaired three redirect issues: max_redirects=0 with
a follow policy incorrectly behaved like no-follow; redirect-policy errors lost
known response metadata; a 303 rewrite forgot that its original stream producer
had stopped. Explicit no-follow remains an inspectable response. Sensitive
cross-origin redirects retain their rejection metadata. The abandoned producer
continues to return upload_stopped after the final GET response, and ordinary
cancelled file reads cannot overwrite that response with a new IO failure.

The first large header/trailer fixture used one 8 KiB trailer and exposed the
provider's separate 4 KiB per-field bound. Its failed receipt remains. The valid
independent-budget fixture uses four 2 KiB fields after a 60 KiB header block;
a separate oversized field now verifies typed limit_exceeded. No provider limit
or protocol policy was weakened. The original zero-hop failure is also retained.
Focused [Release wire and full native probes](evidence/http-wire-fixed-tests-release.log)
pass; further profile receipts are recorded as they complete.


## Explicit TLS file snapshots and declaration documentation

Explicit CA/PEM/P12/key paths are now read as bounded regular files at client
creation and passed through the supported curl blob APIs. The combined input
budget is 1 MiB. Existing filesystem open/read/close code is reused with a
native-only regular-file policy: POSIX uses nonblocking open plus descriptor
validation, and length is checked before allocation growth. A subsequent path
replacement cannot redirect the already-open descriptor; growth remains bounded
by the read limit. Source IO open/read semantics retain their original path.

Owned TLS bytes survive caller file mutation and are released after provider
cleanup at client close, even if a closed native handle is retained. Custom trust
remains replacement for both origin and proxy; verify-chain/hostname and PEM/P12
capabilities remain intact. The new tests reject oversized material before
parsing and reject a FIFO without trapping the network driver. Local file input
snapshotting is synchronous configuration work; it is not network activity.

An additional conservative provisional parsing reservation is charged per
connection, alongside input bytes. Its coefficient is not a certified malformed
ASN.1 allocation proof. System-root/native-store and parser worst-case audit
remain unfinished; no total RSS bound is claimed. This material-input repair does
not by itself close the complete TLS provider-memory acceptance item.

Release passes [4/4 focused TLS/native/wire/DNS/source tests](evidence/http-tls-snapshot-stable-tests-release.log),
including mutation ownership, input limits and nonregular file rejection. The
first build was invalidated by an in-place formatting edit concurrent with Clang
reading its source; it crashed in Lexer::SkipWhitespace. The original log and
preprocessor reproducer are retained under `evidence/tls-material-build-input-race/`.
The same flags with fixed inputs build and pass. No compiler option or sanitizer
was changed to obtain this result.

Source documentation now covers 29 public HTTP types/records and their
fields/cases, plus 34 public functions/constructors and parameter/result semantics.
The module still has no additional public URL helper. Remaining examples,
installation/platform and performance/consolidated acceptance work is separate.


## Consolidated Debug: semantic stack and shared object generation

The original full Debug run completed 396/400 tests. The two math frontend
failures retain their SIGSEGV bundles at
`docs/math/evidence/crashes/incident-i7u8evtx` and `incident-xi4h0rw7`. Their
32-deep accepted ordinary-call case exposed semantic-handler stack pressure at
the enforced 1 MiB limit. Matching Debug `-fstack-usage` measurements give
22,024 bytes for the baseline ordinary call handler and 22,312 for the HTTP
candidate. The builtin adapters now have their own existing-style noinline
handler that returns before ordinary argument recursion. The ordinary frame
falls to 2,728 bytes; the separate builtin frame is 22,056 bytes. No nesting
budget, stack limit, optimization or sanitizer option changed. Both original
math frontend tests pass in the focused Debug run; the expanded profile reruns
are recorded in the final handoff.

[Frame comparison](evidence/frontend-stack/summary.json) retains both compiler
inputs and all stack-usage data. The other two original Debug failures are the
obsolete Scope worker-zero assertions already corrected for the approved Runtime
pool lifetime.

The subsequent all-target Debug rebuild exposed a separate Make graph race.
Observed/unobserved private-call tests shared a generated object, but both
consumer targets could execute its custom command concurrently after generator
relinking. That overwrote the generator's invalid-input sentinel, correctly
failing its existing check. [Original build log](evidence/http-final-build-debug.log)
is preserved. Each shared object now has one custom target producer, and both
executables depend on that producer. The generator's validation remains intact;
parallel rebuilding and the twelve private-call execution tests verify the
repair. This is unrelated to the earlier compiler-input edit/build race.

## Held public HTTP generation replacement

The new `http_reload` probe holds an actual source streaming read while precise
GC and development-host replacement run, for both cold and hit cache paths.
Both activations stay on their application owner; the host thread only replaces
code and controls the peer. Old/new calls produce 8/9, generation leases retire
from two to one, an incompatible unused layout requires restart, repeated await
remains valid, and close retires source roots and Runtime workers. The first
fixture moved acquisition to the host thread and correctly hit the affinity
invariant. That original failure is retained under
`evidence/http-reload-owner-affinity-original/`; no production affinity change
was made. The Release repair passes, followed by the consolidated profile checks.


## Consolidated strict: paired DNS fixture sockets

The first final strict affected run passed 80/81 checks, including the complete
TLS corpus and the original Stage6 repairs. `http_dns` failed before invoking
its native binary: the OS-assigned UDP ephemeral port was already used by a TCP
socket. UDP and TCP allocate their port spaces independently. The unedited
[run log](evidence/http-final-tests-strict.log) records `EADDRINUSE` from the
fixture's second bind.

The fixture now acquires both sockets before starting either server and releases
its UDP socket on a failed pair. It retries only address-in-use, at most 32
times; other startup errors propagate. Production DNS/cache/TTL code and all
peer assertions are unchanged. A deterministic
[probe](evidence/dns-port-pair-probe.py) checks one collision then success,
32-collision exhaustion, non-collision error propagation and closed socket
handles; all [three controls pass](evidence/dns-port-pair-probe.json). The final
profile runs verify the actual c-ares UDP/TCP, IDN, IPv6 and cache behavior.


## Final-source clean compiler and external percent-output control

The final-source clean build independently reproduces the Release curl, Runtime,
frontend-core and backend archives byte for byte. Its initial configure omitted
`VOID_LINKER`; that default-system-linker control is retained under
`evidence/http-clean-default-linker-control-*-release.log`. Its Make `voidls`
target tried linking without producing its main object when the CMake output
path contained `%`. This control is not pinned-LLD acceptance. The earlier
custom-output 18-check control also left `VOID_LINKER` empty; current summaries
now state that explicitly.

Selecting the exact same pinned LLD 23.1.4 settings as the accepted Release
build reproduces the documented external absolute-percent-output failure,
although the directory exists. The source/object archives remain identical.
`evidence/http-clean-pinned-linker-percent-control-*-release.log` preserves this
failure. It is the same class of external output-path constraint documented in
[Stage9 research](../../STAGE9_RESEARCH.md#reproduced-lld-output-path-issue);
no historical optimizer/scheduler incident is thereby diagnosed or repaired.

The final CMake tool output uses Unicode, spaces and `#` without `%`, with the
pinned linker explicit. VOID's existing compiler-owned same-parent private
staging, relative output and atomic publication remain unchanged. The actual
HTTP install/relocation probes in all three main profiles pass with `%` in
application locations using the pinned linker. This build-system limitation
does not redefine source VFS, host file paths or application output policy.
The local build instructions now include `-DVOID_LINKER` explicitly.


The final explicit-LLD build passes 18 example process checks (one CTest) and
two HTTP/URL LSP CTests at the 1 MiB limit. Five archives and all three compiler
objects match the main Release build byte for byte; `voidls` also matches.
All eleven adjacent native resources verify. The raw `voidc` includes a different
cache-build identity because the existing identity algorithm hashes absolute
input paths as well as bytes. A read-only byte-zeroing comparison did not
reconstruct the binary; its result is preserved. An isolated actual relink that
substitutes only the main cache-identity archive reproduces the main `voidc`
exactly. The candidate's own binary and identity stay unchanged.
[Identity and receipts](evidence/http-final-clean-build-identity.json),
[exact relink control](evidence/http-final-clean-identity-link.json).
