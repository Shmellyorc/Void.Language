# Current HTTP engineering

31 October 2026 UTC (30 October local). LLVM/Clang/LLD 23.1.4, baseline
`15f7868b0af177f8f7d49c548392c79c7bd921d2`.
**Architecture GO. Linux implementation and local verification are ready for
[Golden Wrench handoff](HANDOFF.md); full HTTP acceptance is pending.** [Earlier chronological checkpoints](ENGINEERING_HISTORY.md)
retain every original failure and intermediate limitation.

## Settled decisions implemented

- Authoritative curl 8.23.0 is directly in `third_party/curl/`, left unstaged for
  Boss. All 1,586 upstream file identities are verified with exactly four private
  corrections: checked cookie-enumeration OOM in cookie.c/h/getinfo.c, and the
  exact subsequently approved HTTP/2 window correction in http2.c. CMake builds
  these files directly. No production patch workflow, ZIP or manual application.
- Linux selects the exact approved unmodified AWS-LC 5.12.0 revision/archive.
  The original OpenSSL 3.5.9 UBSan failure is upstream incompatible callback-type
  UB, independently reproduced without curl/VOID. Strict instrumentation remains
  intact. [Cause and original receipts](DEPENDENCY_CONFLICT.md).
- The existing worker pool has Runtime lifetime, with native-only file producers
  on that same bounded queue. Scope task/group draining, generation leases and
  worker limits remain authoritative. Two-phase shutdown cancels unentered native
  producers before draining dependent work; it preserves admitted source Work.
- The four configuration records use the approved value semantics. Clients,
  requests, responses and file authority retain managed ownership. Referenced
  lists are snapshotted at the existing owner/native boundary.

## Implemented functionality

`stdlib/url.void` provides HTTP(S) parse/resolve/normalize, immutable address
fields, strict text/byte percent encoding/decoding, ordered query pairs and query
replacement. CURLU is the single structural parser; the private bridge validates
stdlib identity, typed IR shapes, effects and owner/worker/readonly authority.
Unicode hosts use explicit UTF-8 IDN handling without changing process locale.

`stdlib/net/http.void` provides the complete declared source facade: clients,
requests and all body variants, buffered get/send, streaming response/read/write/
finish/close, native file upload/download, mixed file-backed multipart, sync/async
pairs, metadata/trailers/history, strict text conversion, typed errors and native
capabilities. Native libcurl multi handles real HTTP/1.1 and ALPN HTTP/2, TLS,
asynchronous c-ares DNS, pools, backpressure and cancellation on one driver per
Runtime. No source callback runs on the driver.

TLS verifies chain/hostname and requires TLS 1.2+. Explicit CA/PEM/key/P12 inputs
are bounded regular-file snapshots, combined at most 1 MiB; trust replaces system
roots for both origin and HTTPS proxy. Linux PEM/P12 mutual TLS is exercised.
Native Windows Schannel/store and macOS SecTrust/ICU recipes/adapters are present;
**native execution is unverified**. [Platform details](NATIVE_PLATFORMS.md).

Basic/Bearer/Digest authentication, explicit/environment HTTP/HTTPS/SOCKS proxies,
opt-in bounded in-memory cookies, manual redirect policies and credential isolation
are implemented. A peer-observed HTTP/2 REFUSED_STREAM originally caused six POST
transmissions; the pre-request guard now rejects hidden replay before a second
request while permitting explicit Digest challenge exchanges. The peer now sees
one 13-byte POST. Netrc, inherited key logging, silent HTTPS downgrade and
consumed stream/file replay are disabled or rejected by the approved policies.

Bounded gzip/zlib-deflate decoding validates checksums/end markers and concatenated
gzip, independently limits encoded/decoded sizes, and rejects unsupported/stacked
encoding unless raw mode is selected. Literal-wire fixtures check framing,
interim/header/trailer limits, truncation prefixes, malformed data and redirects.

Precise GC now retires unobservable completed results, Work and quiescent native
Task/group records during a long root, while retained handles remain repeatedly
awaitable. Native bulk borrows existing files at their current cursor, holds
exclusive operation authority through completion, preserves ownership/partial
progress, and makes no managed chunk task per block. Single-worker barriers cover
source/producer independence, queued/entered cancellation and Runtime close.

Driver polling aggregates all client multis, uses fixed descriptor storage,
clamps to active deadlines and wakes for native commands. This removes a measured
10 ms delay for active clients behind an idle first client. Completed-task reaping
short-circuits dependency scans once retention is established, repairing a measured
unchanged-IO CPU regression without changing collection or lifetime decisions.
Per-attempt header clocks distinguish Digest's first challenge from the subsequent body upload.
TLS/proxy connect timeouts preserve their actual phase; producer wait is excluded
from write idle. Early 303 redirects reset old-handle pause state. A held native
file read keeps a timed-out result unpublished until quiescence, then reports
`timeout/file` with actual progress. The original failures remain in evidence.

## Documentation, tooling and deployment

All 29 HTTP public types/records and 34 functions/constructors have declaration,
field/case, parameter and result documentation. LSP checks cover public/private
inventory, members, method hovers/definitions, active parameters, incomplete input
and read-only stdlib rename. [Five runnable examples](../../../examples/net_http/README.md)
cover JSON, authenticated POST, native files, async streaming and cancellation.

Native manifest v2 carries the checked full link closure and install-relative
paths. A separate SHA-256 resource inventory verifies actual archives/shared
libraries before native build publication. Altered still-linkable curl bytes are
rejected while preserving an existing output. Installation/Unicode relocation
includes notices, provenance, PSL, replaceable GNU libraries and corresponding
source. Older v1 manifests remain readable. CMake handles custom runtime-output
locations. No executable staging/VFS/path publisher was replaced.

Pinned native Windows/macOS bootstrap recipes preserve static CRT or SDK/ABI
boundaries, exact dependency checks and full strict requirements. The portable
unmodified PSL control passes 184 upstream cases under strict instrumentation.
This is not native Windows/macOS execution acceptance. No hosted CI was edited
or dispatched; its future dependency bootstrap is a Boss-controlled action.

## Actual evidence and open gates

| Checkpoint | Actual outcome |
| --- | --- |
| Approved H2 production correction | [17/17 per Debug, Release, strict](HTTP2_WINDOW_REVIEW.md), real HTTP/TLS/proxy/source/JIT/AOT and strict 1 MiB execution |
| TLS snapshots plus wider Runtime integration | [Debug 18/18](evidence/http-tls-snapshot-tests-debug.log), [strict 18/18](evidence/http-tls-snapshot-tests-strict.log) |
| Public source sustained streaming | [2 GiB normal/O2 JIT and independent AOT](evidence/http-reaping-source-2GiB-release/commands.json): live managed payload 212,924–219,068 bytes, Task/group/Handle maxima 11/2/11 |
| Public native bulk | [2 GiB download + 4 GiB+1 upload](evidence/http-reaping-public-bulk-4GiB-release/commands.json), independently counted/hashed, steady Release RSS 10,580–11,200 KiB |
| Managed failure sweeps | `http-private-form-faults-PROFILE`: 1,448 sync and 1,607 async injected failures per optimization mode in each profile; cleanup/reuse/zero roots |
| Fresh full provider | [Release 1](evidence/vendored-full-Release-20261031T032400962158Z/commands.json), [Release 2](evidence/vendored-full-Release-20261031T032429933889Z/commands.json), [Debug](evidence/vendored-full-Debug-20261031T032541548470Z/commands.json), [strict](evidence/vendored-full-strict-20261031T032402162232Z/commands.json): all pass; Release archives byte-identical |
| Fresh disabled provider builds | [No cookies](evidence/vendored-no-cookies-20261031T025547365577Z/commands.json), [no HTTP](evidence/vendored-no-http-20261031T025609098921Z/commands.json), strict, pass |
| Focused timeout/held-file regression | [Release](evidence/http-timer-final-isolated-release/native.stdout), [Debug](evidence/http-timer-final-isolated-debug/native.stdout), [strict](evidence/http-timer-final-isolated-strict/native.stdout) pass; full native driver/Runtime probes pass in each profile |
| Release measurement tool | [JSON requests, cold/warm pools, four concurrent, close and unused import](evidence/http-performance-release/measurements.json); actual peer connection counts; concurrent host load recorded |
| Consolidated Release | [382/387 original pass](evidence/http-consolidated-tests-release.log), then [25/25 integration repairs](evidence/http-final-tests-release.log) and [79/79 final affected HTTP/URL/async/IO checks](evidence/http-reaping-final-tests-release.log). Legacy installed link helpers now expand v2 paths; worker assertions check Scope quiescence plus joining at Runtime close; valid raw peers advertise close. All five original failures have passing repairs |
| Expanded Release frontend/HTTP/async | [92/92](evidence/http-frontend-final-tests-release.log) after splitting builtin dispatch from ordinary argument recursion; all-target build passes |
| Final-source clean pinned build | [18 example process checks / 1 CTest](evidence/http-final-clean-examples-release.log), [2/2 LSP](evidence/http-final-clean-lsp-release.log), explicit LLD 23.1.4. [Five archives, three compiler objects and voidls are byte-identical](evidence/http-final-clean-build-identity.json); eleven native resources verify. External LLD absolute-percent-output limitation remains documented in the handoff; CLI staging/publication is unchanged |
| Earlier clean compiler/custom-output control | [18/18 example checks](evidence/http-clean-output-examples-release.log) with Unicode/space/#/% output and hashed resources; that CMake control left `VOID_LINKER` empty and is not pinned-LLD acceptance. Its curl archive is byte-identical to both clean full-provider builds |
| Baseline IO comparison | [Actual `15f7868b` comparison](PERFORMANCE.md): initial current pure-IO CPU median 1.282 s vs baseline 0.837 s; redundant reaping dependency scans repaired. Pure current median becomes 0.890 s; HTTP-capable absent/idle/paused median 0.837 s, with all ranges retained |
| Expanded final Debug | [105/105](evidence/http-final-tests-debug-repaired.log), all four original Debug failures repaired, twelve private-call probes and actual held HTTP reload pass |
| Expanded final strict | [92/92](evidence/http-frontend-final-tests-strict.log) after the frontend split and bounded DNS fixture port-pair repair; full ASan/UBSan/leak checks |
| Registered-name reconciliation | [All 389 Release, 402 Debug and 379 strict names have passing latest available results](evidence/http-final-verification.json), from original full runs plus named focused reruns; not three fresh all-tests executions |
| Consolidated Debug/strict | Original Debug 396/400 and strict 374/377; every original failure now has a passing rerun. Debug math stack pressure is repaired without changing the 1 MiB limit; the original crash bundles remain. A parallel shared-object generator race found during rebuilding has one CMake producer; original evidence remains. Strict original TLS corpus timed out after flat live-allocation samples; the explicit-workload-deadline run passes all 256 handshakes and negative peers ([receipt](evidence/http-tls-corpus-explicit-deadline-strict/commands.json)) |

The 512 MiB reservation remains provisional. [Provider memory review](MEMORY_REVIEW.md)
separates measured envelopes, bounded source/bulk behavior and the incomplete
malformed-input/system-trust allocation proof. Native Windows/macOS acceptance
and provider worst-case memory certification remain open. No independent Golden Wrench
implementation acceptance or release lock is claimed. Historical logging
optimizer/performance/release holds are unchanged.

No staging, commits, branches, pushes, hosted CI, external upstream messages or
protected root README edits were performed. Boss owns Git; independent Golden
Wrench source/evidence review remains required.
