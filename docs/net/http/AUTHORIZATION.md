# 👑🔧⌨️ SACRED GOLDEN WRENCH: VOID NET/HTTP ENGINEERING AUTHORIZED

**BOSS-AUTHORIZED COMPLETE STANDARD-LIBRARY MISSION**

**Baseline:** `15f7868b0af177f8f7d49c548392c79c7bd921d2`

**Toolchain:** LLVM/Clang/LLD 23.1.4

**Golden Wrench decision:** The Research architecture is approved with the mandatory corrections below.

## 1. Read and reconcile the approved research

Read these complete documents before engineering:

- `docs/net/http/RESEARCH.md`
- `docs/net/http/DESIGN.md`
- `docs/net/http/ROADMAP.md`
- `docs/WRENCH_CLANKER.md`
- `docs/README.md`
- Relevant current compiler, Runtime, async, native-resource, IO and Language Bible contracts.

The original research is the technical foundation. This authorization supersedes its five pending owner decisions.

Use one cohesive engineering mission, with substantial internal deliveries where dependencies require them. Do not create artificial micro-sprints or repeatedly stop for settled decisions.

**KISS. DRY. COMPLETE FEATURES.**

## 2. Update Golden Wrench continuity

Synchronize the current-status sections of:

- `docs/WRENCH_CLANKER.md`
- `docs/README.md`
- `docs/VOID_STDLIB_BACKLOG_2026-10-27.md`

Record the actual implementation status:

- LLVM 23.1.4: hosted accepted at commit `15f7868b`; CI #40982216303, seven successful jobs, 2,454 passing CTests, zero failures, five documented cross-volume skips.
- `os`: all 13 public functions implemented; registered native tests passed across Linux, Windows and macOS.
- `collections`: all 24 approved public helpers implemented. Feature-complete for its accepted API scope, with cross-platform CI passing.
- `log`: substantial approved logging API implemented, cross-platform tests passed. Preserve the historical unexplained optimizer incident, unresolved performance gate and associated release-hold records. Never imply these were causally repaired.
- `net/http`: next Boss-authorized engineering mission.

Keep historical research and failed-test records intact. Do not claim an independent source review or formal release lock that has not occurred.

The public root README is protected.

## 3. Approved HTTP architecture and scope

Implement the researched public `url` and `net/http` modules, including their full approved API inventories.

**Required transport architecture:**

- Maintained, pinned, feature-verified libcurl multi.
- Genuine HTTP/1.1 and HTTP/2 through nghttp2.
- HTTPS with certificate-chain and hostname verification.
- Native platform trust integration.
- Real asynchronous DNS and cancellable transport.
- One Runtime-owned native transport driver using existing VOID task and resource infrastructure.
- No second source-language scheduler, homemade HTTP parser, custom TLS implementation or arbitrary public FFI.

`url` owns URL parsing, resolution and encoding. Its initial parsing scope is HTTP(S) URLs and references resolving against them, not an unqualified promise of every URI scheme.

Reuse existing IO, JSON, collections, error handling, compiler metadata, typed IR, managed roots and native link infrastructure.

## 4. Mandatory Golden Wrench corrections

### A. Async result lifetime: REQUIRED

Research confirmed that VOID currently retains completed task results until root drain.

This makes indefinitely repeated async chunk operations accumulate retained results.

Repair this using the **smallest correct extension of existing task-result ownership**.

Requirements:

- Preserve repeated-await semantics for retained handles.
- Preserve structured failure observation and cancellation.
- Never retire a result while an observable handle still owns it.
- Permit unreachable completed results to retire safely.
- Preserve precise GC roots, generation leases and cleanup.
- Avoid a new GC subsystem, weak-handle framework or task scheduler.

Verify sustained async pull streaming with repeated large transfers and stable retained-result memory behavior.

Native bulk file transfer remains mandatory independently.

Do not call the module memory-bounded if this dependency remains unresolved.

### B. Native completion and shutdown: REQUIRED

Integrate externally completed operations with existing native Work, task completion, owner publication and structured cancellation.

The existing worker-per-request mechanism must not be used for operations that wait on their own source producers.

Preserve:

- Exactly-once native completion.
- Safe cancellation races.
- Rooted managed publication.
- Abandonment and allocation-failure recovery.
- Parent/group failure ordering.
- Transfer ownership inside async activations.
- Runtime-close notification before waiting for native jobs.
- No source callbacks or managed allocation from transport callbacks.

Do not hold the transport driver hostage to a blocked managed executor.

### C. libcurl process lifetime: REQUIRED

Define and implement correct process-wide libcurl initialization and shutdown ownership.

Multiple VOID Runtimes and embedding hosts must coexist without prematurely destroying shared libcurl state.

Respect actual selected-provider thread-safety rules and initialize before use.

All easy/multi handles, resolver activity, callbacks and worker threads must be quiescent before their dependent state is destroyed.

Do not assume global cleanup joins resolver threads or that a Runtime owns an embedding application's unrelated libcurl state.

No Windows DLL-loader-lock initialization.

### D. Real memory accounting: REQUIRED

The proposed 512 MiB Runtime admission budget is provisional, **not a proven RSS ceiling**.

Measure and account for:

- HTTP/2 flow-control windows and provider buffering.
- TLS and DNS allocations.
- Headers, cookies and decompression state.
- Encoded and decoded receive queues.
- Request snapshots and pending results.
- Concurrent transfers and connections.

Select finite, justified defaults using actual dependency behavior.

Do not confuse a small VOID buffer with bounded total memory.

Avoid an unnecessary global accounting framework. Extend current Runtime admission where practical.

### E. Correct the research inventory

`RESEARCH.md` mentions checked cookie import/export while `DESIGN.md` explicitly excludes persistent cookie storage.

Reconcile this contradiction before implementation.

In-memory cookie sessions are approved. Persistent cookie import/export is not part of the initial contract.

Do not silently invent an API or mark an excluded capability implemented.

## 5. Required feature-complete HTTP client

Implement the complete approved scope:

1. URL parsing, relative resolution, component encoding/decoding, ordered query parameters and strict validation.
2. Request methods, duplicate/binary headers, text/byte/form/multipart/stream request bodies.
3. Responses with actual status, headers, trailers, redirect history, final URL and strict text conversion.
4. Buffered send/get and genuine streaming read/write operations.
5. Bounded native bulk file uploads/downloads using existing IO reservations.
6. Full lifecycle: open, start, finish, flush-equivalent completion where applicable, close and deterministic cleanup.
7. TLS verification, explicit custom trust, supported client certificates and secure defaults.
8. DNS, IPv4/IPv6, connection pooling, keep-alive and isolation.
9. Connect/header/read/write/total timeouts with monotonic deadline handling.
10. Redirect security, request replay rules and cross-origin credential isolation.
11. Bearer, Basic and Digest authentication, supported proxies and optional in-memory cookie sessions.
12. Bounded gzip/deflate decoding, independent encoded/decoded limits and raw mode.
13. Actual HTTP/1.1 and HTTP/2 negotiation, multiplexing, protocol enforcement and honest version reporting.
14. Real synchronous and asynchronous APIs, structured cancellation and clean shutdown.
15. Complete source documentation, runnable examples, compiler-owned LSP discovery and install support.

**Approved exclusions:** HTTP/3, h2c, HTTP servers, WebSockets, browser SSO, OAuth login flows, persistent cookie storage, arbitrary URI schemes, raw public sockets and other separately owned networking functionality.

These are explicit product boundaries, not omissions disguised as implemented features.

Do not broaden scope to unrelated protocol systems.

## 6. Dependency and security policy

Choose exact maintained dependency releases and immutable checksums. Verify the selected feature set and full transitive link closure.

Check licensing, notices, static/dynamic deployment obligations, Windows CRT compatibility, Linux trust configuration and macOS SDK/framework requirements.

No uncontrolled system-libcurl fallback or shelling out to curl executables.

HTTP/2 must be actually available and enforceable. `http2_required` must reject an HTTP/1.1 fallback before an incompatible request is sent.

Do not disable TLS verification, hostname checking or certificate validation to make tests pass.

Use strict origin/redirect security, protected credential handling and finite resource limits.

Preserve the existing native executable staging, atomic publication, cache identity and reload semantics.

## 7. Engineering sequence

Work through the complete mission in dependency order:

**Foundation:** dependency selection, build/link integration, native resource ownership, external completion, URL facade and async lifetime correctness.

**HTTP engine:** complete request/response types, genuine h1/h2 operations, streaming, bounded memory, bulk IO, timeouts and cancellation.

**Client policy:** TLS, authentication, proxies, redirects, cookies, decoding, errors and complete application-facing APIs.

**Integration:** documentation, examples, tooling, installation, real tests, performance measurements and final evidence.

These are internal engineering steps, not separate release milestones.

Work continuously through normal implementation problems. Fix actual defects and preserve the failures.

Stop only if an unresolved safety/semantic conflict, incompatible dependency obligation or genuinely necessary owner decision prevents correct completion. Do not silently reduce the approved contract.

## 8. Verification requirements

During development run focused tests, not the entire project matrix after every change.

At integration verify:

- Debug, Release and strict ASan/UBSan/LSan.
- Normal JIT, verified O2 and independent linked AOT.
- Real 1 MiB stack constraints.
- Actual HTTP/1.1 and HTTP/2 loopback servers.
- TLS success and negative certificate/hostname cases.
- Redirects, proxy isolation, auth and cookie boundaries.
- Multipart, streaming and multi-GiB native bulk file transfers.
- Bounded async pull-result memory over sustained transfers.
- GC, allocation failures, cancellation, task groups and Runtime shutdown.
- Source-worker limit of one without producer/network deadlock.
- Cache, compatible reload and generation lease preservation.
- Installed/relocated tools, dependency/link closure and real LSP.
- Source negatives, independent malformed-wire and security tests.
- Native Windows/macOS acceptance where available.

Do not substitute a mocked HTTP/2 server for genuine HTTP/2.

Preserve original unsuccessful attempts and classify actual failure causes.

No Linux result may be represented as native Windows/macOS acceptance.

Measure the relevant CPU, latency, transfer throughput, memory, compilation and dependency size costs. Optimize demonstrated bottlenecks, not speculative ones.

## 9. Final engineering handoff

Update permanent documentation under `docs/net/http/` with:

- Final API and capability inventory.
- Exact dependencies and licenses.
- Implemented versus excluded behavior.
- Native ownership and safety contracts.
- Correctness/negative-test evidence.
- Performance measurements.
- Original failure and repair history.
- Explicit unresolved issues and native verification limits.
- Modified-file inventory and reproducible validation receipts.

Update `docs/README.md` and `docs/WRENCH_CLANKER.md` with truthful engineering status.

Stop for independent Sacred Golden Wrench source and evidence review.

**DO NOT stage, commit, push, create branches, dispatch hosted CI, publish releases or modify the protected root README. Boss alone controls those actions.**

Keep conversation output minimal. At completion report the implementation, actual tests, remaining blockers and handoff locations.

**ONE VOID. ONE COMPILER. ONE RUNTIME. COMPLETE FEATURES.**

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
## Subsequent explicit decisions, 30 October 2026

Boss approved the exact Linux AWS-LC 5.12.0 dependency pin, immutable revision
`cd481c207e38f06aeb5766f4a1cdfce10a4c0af8`, source archive SHA-256
`1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68`.
The [TLS review](TLS_REVIEW.md) records source identity, clean Debug/Release/strict
receipts and remaining platform/whole-mission obligations. This authorizes direct
production integration without another pin question or manual owner file work.

Boss also approved promoting the existing IO worker pool from per-root Scope
lifetime to Runtime lifetime, retaining the worker limit, Scope task/group draining,
generation leases and a single pool/source scheduler. The
[ownership design](NATIVE_IO_LIFETIME_REVIEW.md) records the synchronous multipart
file producer requirement and verification obligations. These decisions do not
weaken the full source HTTP API, sanitizer contract or Git restrictions.


## Source configuration defaults, 30 October 2026

Boss explicitly approved the four value configuration records (`tls_options`,
`proxy_options`, `client_options`, `request_options`) after the exact pure-default
restriction and value/list copy semantics were presented. All approved field
names, defaults and ordinary call forms remain. Managed clients, requests,
responses and file authority retain their existing lifecycle. Exact decision:
“Approve the four value configuration records (Recommended)”.
See [SOURCE_API_REVIEW.md](SOURCE_API_REVIEW.md).


## Additional pinned HTTP/2 correction, 30 October 2026

Boss explicitly approved direct integration of the exact additional libcurl
8.23.0 `lib/http2.c` correction, patch SHA-256
`f109b12c2c5b327fb9b2da2b9d302cb111d936097b98ace830ea886db7d4f1a2`.
Exact reply: “Approve this exact additional pinned correction (Recommended)”.
The [review](HTTP2_WINDOW_REVIEW.md) includes actual patch contents, original
budget/peer-credit failures, unchanged limits and Debug/Release/strict receipts.
The authoritative inventory now permits these four corrected files, preserving
the three cookie corrections. This authorizes direct source/provenance integration;
no manual Boss patch action, new Git action or upstream release is required.
