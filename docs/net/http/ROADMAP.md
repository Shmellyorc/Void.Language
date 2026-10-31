# HTTP engineering roadmap and acceptance

## Subsequent Boss authorization, 30 October 2026

The [complete engineering mission](AUTHORIZATION.md) now authorizes this dependency
sequence as one cohesive mission. Its mandatory corrections supersede the pending
approval choices below: repair async-result ownership, integrate external native
completion and pre-wait shutdown, own shared curl process lifetime correctly,
measure actual provider memory, and exclude persistent cookie import/export.
The original proposal is retained below. No repeated owner approval is needed
for settled choices; [engineering evidence](IMPLEMENTATION.md) identifies actual
progress or a demonstrated conflict requiring a new decision. No partial delivery
is accepted as the complete module. Git, hosted CI and root README stay protected.

**Proposed implementation mission; engineering is not authorized.**
Research date 30 October 2026; source baseline
`15f7868b0af177f8f7d49c548392c79c7bd921d2`, LLVM/Clang/LLD 23.1.4.
Read [RESEARCH.md](RESEARCH.md) for evidence, including actual missing Runtime
support, and [DESIGN.md](DESIGN.md) for the proposed complete public contract.

Golden Wrench first reviews architecture and the five owner decisions in DESIGN
section 12. Boss approves the resulting contract, exclusions and dependency scope.
Keyboard then implements one cohesive HTTP mission in the substantial deliveries
below. They are dependency order, not excuses for shipping an unfinished module or
running full hosted CI between steps. No partial delivery is a completeness lock.

## Approval prerequisites and concrete technical gates

- Approve transport/TLS/resolver/deployment policy and exactly which protocols and
  advanced integration boundaries are excluded. In particular, HTTP/3 absence is
  an owner decision, not a hidden omission.
- Resolve the bounded async pull-result decision. Current task results remain
  rooted until root drain; a source async chunk loop does not meet a whole-program
  bounded-memory promise. Native bulk file operations are mandatory independently.
- Review the smallest external-completion and structured transfer-lifetime change
  against existing task/group/code-lease semantics before building a broad API on
  it. No second scheduler, task detachment or source callbacks on transport threads.
- Select maintained dependency releases and hashes, exact features/backends,
  architecture/CRT/OS minima, native trust and IDN behavior, license/notice closure,
  and verified link/install metadata. curl 8.23.0 is the researched candidate,
  not an installed or accepted pin.

If a required gate fails, preserve the evidence and revise this same contract for
Wrench/Boss. Do not quietly deliver HTTP/1.1-only, fake async, disabled verification,
unbounded buffering or a Unix-only module under the word complete.

## Delivery 1 — dependency closure, Runtime integration and URL foundation

Build the approved dependency profile using LLVM 23.1.4-compatible native objects
and current CMake/Runtime architecture. Use imported targets, source checksums and
existing install tooling. Validate HTTP/2, SSL, asynchronous resolver, IPv6,
large-file offsets, PSL, backend identities and trusted CA behavior. Reject a
dependency whose name/version looks right but whose features do not match. Avoid
building unrelated curl protocols/tools and do not add an application-time downloader.

Extend existing `void_native.txt` production/install handling to carry the checked
native link closure; use protected install-relative locations. Update direct
native-host test links too. Keep Windows static CRT, both Debug/Release paths,
Apple SDK/deployment/architecture and Linux native library resolution consistent.
Do not change native executable staging, atomic publication or VFS paths.

Implement a private HTTP transport resource using the existing ledger, per-client
multi state, one lazy driver thread per Runtime, bounded native command/completion
queues and strict driver ownership of libcurl calls. Add the narrow external
completion path to existing Work/receive/wait/cancellation/abandonment machinery.
Account for a queued completion, already-complete cancellation race, publication
failure, native ownership commit, root drain and Runtime-close notification before
the native-job wait. Associate transfers admitted inside async activations with
their structured lifetime even before their first read task exists.

Reuse existing native snapshots, Runtime admission and owner-side rooted result
construction. A completion notification is not proof that callbacks/transport
cleanup returned. Never use a raw task/scope pointer after its ticket retires.
Treat invariant/native allocation failures through existing checked Runtime paths;
no C++ exception may cross a C ABI or library callback.

Add the shared URL bridge and proposed `stdlib/url.void` facade. Reuse CURLU for
parse, resolve and transport authority, with ordinary source percent/query helpers
where practical. Do not create a second URL structural parser. Add exact stdlib
identity/type/arity/effect/IR checks and shared ABI lowering patterned after io/os.
Native HTTP needs a small HTTP-specific bridge, not a general FFI framework or
mislabelled filesystem instructions. Preserve worker and readonly rejection.

Verify U, A and dependency/install portions of X early. Include a deterministic
one-worker test in which file work completes while a network read/producer is
paused; a worker-per-request design must fail this gate rather than pass by
increasing worker counts. No actual internet service is required.

## Delivery 2 — complete request, response and streaming engine

Implement request/header/body/metadata/error types and basic buffered send/get;
the same state machine powers start/response/read/write/finish/close. Add native
framing, header/interim/trailer/redirect limits, known/unknown upload lengths,
partial progress and clean EOF/error distinction. Required HTTP/1.1 and HTTP/2
must share the same public path. Wire version reporting to actual negotiation,
not configuration intent.

Implement upload/download backpressure, driver-thread pause/unpause with safe
reentry, half-duplex and overlapping response/upload progress, early final server
responses, synchronous calls and real async completions. No user function runs
under curl callbacks or native locks. Sync waits enter the current NativeSafe
mechanism so GC coordination does not stall; async waits suspend the owner.

Implement incremental encoded/decoded queues and bounded zlib processing. Keep
provider content decoding disabled; own content limits independently of transfer
framing and encoded Content-Length. Account for HTTP/2 windows and aggregate
headers/TLS/cookies/native metadata in admission, not just public queue lengths.
Abort safely if any checked arithmetic, provider error, budget or decoding check
fails. Preserve existing files, TLS contexts and other client transfers as required.

Add five timeout phases, driver monotonic deadlines and cancellation wakeups,
then default pooling/idle/maximum-age rules. A timer that cannot preempt a native
TLS/filesystem call must be described and measured as cancellation-to-drain
latency; do not certify an impossible hard real-time return guarantee.

Implement native bulk download/upload and file-backed multipart using existing
io resource/kernel/reservation infrastructure. Necessary internal access must be
typed and nonforgeable; no public raw-handle accessor. Bulk transfer uses bounded
native buffers and a fixed number of source/native task results, regardless of
file length. One admitted bulk job may use an existing worker while the independent
driver progresses it; it must never need another worker to escape its own wait.
Enforce finite bulk admission and measure impact on ordinary file operations.
Do not write a per-chunk source await loop and call that bounded.

If Boss requires general sustained pull streaming, deliver the approved narrow
task-result-retirement dependency in this delivery before accepting S. Prove that
unreachable completed results can retire while retained task handles/repeated
awaits and ordered failure observation remain correct. The design of that repair
must receive Wrench review; this report does not approve a new GC or a weak-handle
framework. If no small safe mechanism exists, report the blocker, not a fake pass.

Verify H, B, S, F, D and V, including large payloads, callback mutation snapshots,
protocol errors and cancellation during publication. Do not postpone those until
the final packaging run.

## Delivery 3 — complete policy and application experience

Implement redirect hops, replay classification and credential isolation; basic,
bearer and digest; custom trust/client-identity forms; explicit/environment proxy
policy; bounded optional session cookies; final/raw compression behavior and
strict text decoding. Do not rely on defaults for allowed schemes, header safety,
TLS verification, netrc, proxy credentials or downgrade behavior.

Reuse curl MIME/cookie/auth/proxy/TLS engines. Source wrappers expose plain VOID
types and existing errors. Keep distinct original io/url causes, safe bounded
native diagnostic text and actual progress. Validate provider cookie state and
temporary header-processing allocation bounds; if a public budget cannot be
enforced through supported APIs, expose that as a design blocker for review,
not permission to fork curl or introduce a second cookie engine.

Publish examples for synchronous CLI JSON APIs, explicit-host async requests,
authenticated POST, URL/query construction, large-file native download/upload,
multipart file upload, non-2xx inspection, partial failure and task cancellation.
Examples must actually compile against the final accepted signatures and handle
distinct `json.error`, `io.error`, `url.error` and `http.error` explicitly. Do not
add implicit cross-module error conversion to make examples shorter.

Document every public parameter and result using existing `##` tags. Verify source
module discovery, all public methods/options, private binding invisibility,
UTF-16 diagnostics, hover, signature help, references and navigation through the
one compiler-owned voidls. No VS Code/Vim/Neovim-specific networking semantics.

Verify T, N, R, P, C, E and L. Protocol errors and negative TLS cases are part of
the delivery, not optional hardening after successful examples.

## Delivery 4 — integrated acceptance, measurements and review handoff

Run focused Linux Debug/Release/strict selections across sync/async normal JIT,
verified O2 and independently linked AOT, then native Windows/macOS verification
for actual provider behavior. Full seven-job hosted CI is only a Boss decision
when the completed implementation is ready, not a routine checkpoint.

Check installation/relocation with only the declared dependency closure; real
Unicode/percent/long host paths; CMake and voidc build/external-link routes;
missing/incorrect dependency diagnostics; restricted CA stores and proxy settings;
1 MiB main/worker/network stacks; GC/allocation failure; cache/reload/Runtime close.
Keep original incidents and unsuccessful runs, with exact fixture preservation.

Provide final capability/acceptance matrix, installed dependency manifest/notices,
public API examples, measurements, original failure record and Golden Wrench
handoff. Mark pending native gates UNVERIFIED, never inferred from Linux/cross
compilation. Do not declare complete while an approved essential row is missing.

## Focused acceptance matrix

Each ID maps to RESEARCH.md's completeness inventory. Use existing CTest/native
runner/crash capture conventions; add focused HTTP fixtures, not a new validation
framework. Deterministic barriers and local endpoints take priority over sleeps
or flaky public internet services.

| ID | Required tests and independent oracle |
| --- | --- |
| U — URLs | Explicit/relative URLs, empty query/fragment, duplicates/bare query flag, form plus versus literal plus, every percent byte, malformed escapes/UTF-8/NUL, signed paths/repeated slash/%2F/dot normalization, userinfo, invalid port, bracketed IPv6, IDN provider differences, oversized inputs and unsupported schemes. Test shared HTTP and url parser results, not two independently guessed interpretations. |
| H — methods/metadata | Raw loopback server checks exact request bytes/method/query/header duplicates/empty values; responses with duplicate Set-Cookie/obs-text, interim 100/103, final status, HEAD/204/304, trailers and final URL. Reject CRLF/NUL/name injection, framing overrides, bad/conflicting length/transfer coding and unexpected upgrades. Run malformed HTTP/1 raw fixtures and a genuine HTTP/2 server; do not pretend an HTTP/1 stub verifies h2. |
| B — bodies | Empty versus zero bytes, UTF-8 text, form escaping/order/duplicates, binary NUL, multipart mixed text/bytes/files/Unicode metadata and injected filenames; known length, unknown chunked/h2, excess/short input, source EOF, server early response, 100-continue. Server hashes/counts bytes independently, including a sparse/generated >4 GiB upload. |
| S — streaming/memory | Slow producer/consumer, paused h2 plus fast sibling, one-byte splits, duplex early response, huge compressed expansion, slow sink, queue saturation, owner progress and max connection/admission bounds. Measure native queues, provider buffers, live GC payload and task/result counts separately. Native bulk memory must plateau as logical transfer grows from MiB to GiB. Sustained async pull plateau is a separate required gate if approved; a bounded transport queue alone fails it. |
| F — files | Borrowed file cursor/reservation, mutable alias contention, one-worker progress, read/write/seek errors, disk full/partial writes, flush/close ownership, file-backed MIME, cancellation during active file call, source EOF and unread error body. Destination remains partial after failure according to io contract; test actual progress and no accidental truncate/close/rename. |
| T — TLS | Local CA fixture: correct trust/hostname, expired/not-yet-valid/wrong-host/self-signed/untrusted chains; custom replacement trust; SNI/ALPN; minimum TLS; mTLS supported forms and explicit unsupported ones; Windows native store, macOS SecTrust and Linux trust locations. Verify no plaintext fallback, client cert auto-selection or insecure flags. Real revocation backend differences recorded. |
| N — DNS/pools | Controlled DNS success/failure/no-answer/delay, v4/v6/dual-stack fallback, hosts/search/VPN/split-DNS platform scenarios, finite cache/lifetime, separate client/proxy/trust pools, idle server close, repeated reuse and isolation. Test cancellation during DNS, no resolver thread/channel outliving Runtime and no effect on unrelated requests. |
| D — timers | Independently stall DNS, TCP, proxy CONNECT, TLS handshake, final headers, body reads, upload consumption and file completion. Verify timer phase, global deadline across redirects, intentional pause versus read/write idle rules, queued/start timeout and zero/negative/overflow configuration. Record requested deadline, abort request, native detachment and owner publication times separately. |
| A — async/lifetime | Deterministic barriers: before admission, queued, DNS/TLS, full queues, callback active, completion-before-cancel, cancellation-before-publication, close with active read/write, child unobserved result, result allocation failure, abandoned acquisition and Runtime shutdown. No source callback/managed allocation on driver. Prove exactly-once detachment/completion, parent failure precedence, shielded drain, repeated awaits, no stale scope pointer and no transfer surviving its async activation. |
| R — redirects/replay | Relative/absolute/loop/limit, 301/302/303/307/308 for multiple methods, downgrade/non-HTTP/userinfo target, changed ports/origins, API-key/custom/Authorization/Cookie/proxy/mTLS credential isolation, forbidden body replay and bounded intermediate bodies/history. Two distinct loopback origins verify what each actually receives. No hidden retry after partial mutating upload. |
| P — auth/proxy | Basic/bearer/digest challenge/replay, missing/invalid credentials and 401/403 body retention; HTTP/HTTPS proxy CONNECT, independent proxy verification, SOCKS5 local/remote DNS, no_proxy matching and explicit environment mode. Poison environment/netrc and verify direct default plus no credential leakage. No automatic SSO/PAC claim. |
| C — cookies | Opt-in/off, client isolation, domain/path/expiry/secure/HttpOnly/prefix handling, public-suffix rejection, redirect scope, concurrent updates, jar limits/temporary overshoot and clear/close. Verify documented native localhost behavior instead of claiming browser semantics. Persistent jar not tested as implemented unless Boss adds that scope. |
| E — decoding | gzip, concatenated gzip, zlib deflate, malformed/truncated/checksum/trailing data, extreme expansion and decoded/encoded limits, unsupported/stacked encoding, raw mode and split UTF-8/BOM/charset errors. Compare known byte/hash vectors and existing JSON parser results. Never use a tolerant string decoder to make binary tests pass. |
| V — protocol | Genuine ALPN h2 and h1.1 servers, negotiate fallback, forced h1, required h2 against h1 (fails), multiplexing, stream reset/GOAWAY, closed pooled sockets and partial framing. Report actual version per response. No HTTP/3 claim; test any requested unavailable protocol returns unsupported. |
| X — systems | Normal/O2 JIT, independent sync/async AOT via existing explicit adapters, official native manifest route, Debug/Release/strict ASan/UBSan/leaks, all relevant allocation failure points, full root cleanup, two independent Runtimes, cache identity, compatible reload with held requests, incompatible provider restart, installed/relocated modules/deps, real native Windows/macOS paths/trust/stacks. |
| L — compiler/editor | Source check valid/invalid signatures, readonly/private factory/worker/startup restrictions, malformed IR/resource forgery/opcode/ABI shapes, ownership effects, compiler diagnostics, stdlib discovery, documented active parameters, hover/definition/rename references and private helper invisibility through shared voidls. Existing editor adapters consume the same metadata; no HTTP logic duplicated there. |
| O — owner scope | Every approved exclusion named in public contract/capabilities; every essential inventory row maps to implemented APIs and retained passing evidence, or visibly blocks acceptance. No legacy CI totals presented as HTTP verification. |

### Fixture and native-test strategy

Use a bounded loopback HTTP/1 raw/scripted server for malformed wire cases; TLS
fixtures use an explicit local CA and real negative chains. Use a pinned nghttp2
test server for actual HTTP/2/multiplexing/ALPN. Add a local proxy/SOCKS fixture and
controlled DNS endpoint only where existing fixtures cannot exercise the behavior;
do not grow a general internet simulation framework. Test clients against an
independent server implementation, not only a stub sharing their parser.

Choose ephemeral loopback ports safely, signal readiness explicitly, capture both
server and client logs, enforce process deadlines and close every fixture child
on failure. External public endpoints may supplement native trust observations,
but cannot be the correctness oracle or a requirement for offline tests. Keep
test keys local/explicit; never install test roots into an ordinary user trust
store without a separate owner-approved isolated native procedure.

Instrument provider boundaries using focused test hooks following existing io
barriers/allocation injection. No shipping tracing framework or packet/body logs
by default. Network loss/cancellation and `Err` must not be confused with abnormal
compiler/runner exits.

For **every abnormal compiler or runner exit**, preserve exact `_crash.void`, all
module inputs, generator/provenance hashes, command/environment/build identities,
stdout/stderr/status and available crash artifacts through the existing preservation
helper before temporary cleanup. Retain original failures and classify retries;
a passing retry is not a repair. No ptrace is planned or required by this research.

### Performance acceptance without premature optimization

Measure Release medians/ranges on identical local workloads, recording versions,
host, warm/cold pools, bytes, connection/stream counts and stack settings:

- Repeated small JSON requests: first connection versus reused connection; h1
  and h2, one/four concurrent, negotiated protocol and actual handshakes.
- Large download/upload: 1 MiB, 64 MiB and generated multi-GiB; throughput, CPU,
  native allocations/queues, post-GC live bytes, task/Work/result counts and RSS.
- Fast + intentionally paused h2 streams: starvation, actual native buffering,
  decoder expansion and maximum ready-owner latency.
- Cancellation/shutdown: median/worst observed abort-to-detach and detach-to-owner
  delivery in each phase; detect straggler sockets, native jobs, resources/leases.
- Existing async filesystem/worker throughput with HTTP idle/active; regression
  versus unchanged baseline. One bulk worker tradeoff is reported explicitly.
- Source check/lowering/JIT/O2/native package sizes for a no-HTTP program versus
  imported HTTP; verify unused APIs do not trigger network initialization or a
  new O2/compilation bottleneck. Link time/dependency size reported, not hidden.

No arbitrary throughput target is invented without host evidence. Correctness
gates are no payload-size-proportional native bulk growth, no scheduler deadlock,
bounded admission, real pool reuse, no owner network wait in async mode, and no
material unexplained regression in unchanged workloads. Wrench reviews measured
tradeoffs before any optimization beyond the necessary architecture.

## Final acceptance and stop condition

Completion requires the **whole approved contract**, not a working get demo:

1. All inventory owners/public APIs and U–O gates implemented with source/receipt
   links; no silent substitution of smaller protocol/platform behavior.
2. Required protocols/security/default policies and resource cleanup work in
   normal/O2 JIT and independently linked AOT; every supported native platform's
   gates actually executed or an explicit Boss-approved exclusion recorded.
3. Bounded native bulk transfer proven, and the async pull-result gate resolved
   according to an explicit Boss decision; no false whole-program memory claim.
4. Debug/Release/strict, GC/allocation/shutdown/cancellation, cache/reload, tooling,
   real 1 MiB and installed link-closure evidence complete; original incidents intact.
5. Working examples, public docs, dependency notices/update policy, measurements
   and limitations reconciled with real implementation for independent review.

Stop for Golden Wrench implementation/evidence review and Boss acceptance.
Git staging/history/branches/pushes, hosted CI and protected root README remain
Boss-controlled. This research performs none of those actions and implements none
of the proposed APIs.
