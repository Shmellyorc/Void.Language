# Proposed VOID HTTP client contract

## Subsequent Boss approval, 30 October 2026

The [engineering authorization](AUTHORIZATION.md) approves this complete initial
contract and its stated exclusions, supersedes section 12's five pending owner
decisions and requires the narrow async-result ownership repair. Sustained async
pull-result lifetime is a required implementation dependency, not an accepted
limitation. Native bulk IO is independently required. The 512 MiB admission
budget is provisional, never a demonstrated RSS ceiling. In-memory session
cookies are included; persistent cookie storage/import/export are excluded.
[Actual engineering progress and blocking evidence](IMPLEMENTATION.md) remain
separate from approval. Original proposal wording below is preserved as history.

The subsequent [approved source-default adjustment](SOURCE_API_REVIEW.md) makes
`tls_options`, `proxy_options`, `client_options` and `request_options` value
records, preserving all defaults/call forms and existing shared-list semantics
until native snapshot. The historical managed-configuration wording below is
superseded for those four records.

**Research proposal, 30 October 2026. Not an accepted language or library contract.**
Baseline `15f7868b0af177f8f7d49c548392c79c7bd921d2`, LLVM 23.1.4.
Boss authorizes research; Golden Wrench reviews this design before Boss approves
engineering. [Evidence and source audit](RESEARCH.md), [acceptance roadmap](ROADMAP.md).

## 1. Public ownership and boundaries

`use net/http` imports `http`. An explicit `http.client` owns a connection pool,
TLS configuration and optional cookie session. Independent clients have independent
security/session state. Aliases share managed identity; there is no public global
client singleton. A game can put a client in its ordinary runtime globals or pass
it through fields/arguments using existing owner-thread rules.

`use url` owns URL parsing, relative resolution, percent encoding and ordered query
parameters. HTTP and redirects use that same native parser and validation profile.
`path` remains filesystem path manipulation, `io` remains filesystem operations,
`json` remains JSON, and native HTTP uses none of the source-import VFS rules for
remote paths. No public TCP, UDP, socket, TLS or general process module is a
prerequisite. Those can later wrap their own proven providers under separate
approved contracts; HTTP must not expose raw sockets to pre-empt them.

All signatures below are **proposed** and use current type forms: managed/value
structs, enums with payloads, arrays, default arguments and `result<T,error>`.
No overload sets, named ordinary-call arguments, public task spelling, generic
stream interface or async callback-value parameters are assumed. Tables describe
signatures without proposing new declaration syntax.

## 2. Reusable URL contract

Initial `url` parsing intentionally covers absolute **HTTP and HTTPS URLs** and
relative references resolved against such a URL. Other schemes are not guessed.
This limited parser scope needs Boss approval; encoding/query utilities are
scheme-independent. CURLU parses structure; source code does not split authority
with regular expressions. URL operations do no DNS/network/filesystem I/O.

| Public declaration | Meaning |
| --- | --- |
| `url.address` | Managed immutable parsed record, private construction/storage. Exposes readonly `scheme`, `host` (ASCII network form), `port:i32` (effective), `path`, `query`, `fragment`, `has_query`, `has_fragment`. Paths/queries/fragments retain encoded representation. |
| `parse(value:string) -> result<address,error>` | Require explicit scheme, authority and nonempty host; owned validated output. |
| `resolve(base:string, reference:string) -> result<address,error>` | Shared parser resolves a relative Location/reference; resulting URL must satisfy the same allowed-scheme/security profile. |
| `(self address) to_string() -> string` | Canonical scheme/host/effective-default-port representation, encoded path/query/fragment preserved as below. |
| `normalize(value:string, remove_dot_segments:bool=false) -> result<string,error>` | Conservative normalization by default; explicit dot-segment removal uses provider behavior, never filesystem normalization. |
| `encode_component(value:string) -> string` | UTF-8 bytes; preserve only ASCII unreserved letters/digits/`-._~`; uppercase percent hex. Space becomes `%20`, plus becomes `%2B`. |
| `decode_component(value:string) -> result<string,error>` | Strict `%HH`, plus stays plus, strict UTF-8 output; decoded NUL is allowed as string data, never a URL/native path. |
| `decode_bytes(value:string) -> result<[u8],error>` | Same percent grammar, arbitrary owned decoded bytes. |
| `query_item` | Value record `name:string`, `value:string`, `has_value:bool=true`; duplicates/order and bare `flag` versus `flag=` survive. |
| `encode_query(const items:[query_item], form:bool=false) -> result<string,error>` | Ordered pairs; form mode maps spaces to plus, escapes literal plus. |
| `parse_query(value:string, form:bool=false, max_items:i64=1024) -> result<[query_item],error>` | Split encoded separators before decoding; reject malformed escapes/UTF-8/limits. Empty query yields no items. |
| `with_query(value:string, const items:[query_item]) -> result<string,error>` | Replace query, encode exactly once. No implicit merge with existing query. |

Limits: a full URL is at most 16 KiB encoded bytes; component/query construction
checks this limit before native submission. Reject embedded NUL, ASCII controls,
unescaped whitespace, backslashes, invalid percent triplets, missing scheme,
userinfo/password and invalid/nonrepresentable ports (valid explicit ports 1–65535).
Bracketed IPv6 is supported. Scoped/link-local zone identifiers are proposed
excluded initially because portable scope selection needs a separate contract.

Lowercase scheme/ASCII host; omit an explicit default port when serializing;
empty authority path becomes `/`. Do not decode `%2F` into a path separator,
collapse repeated slashes, reorder queries, rewrite Unicode normalization or
unescape reserved bytes. Parsing preserves path dot components using the
provider's path-as-is mode; relative resolution and explicit dot removal have
separate tested behavior. This matters for signed URLs. Fragments are inspectable
but never sent as an HTTP request target.

International hostnames use the selected native/libidn2 provider. Expose its
actual ASCII result; native mapping differences are documented, not claimed
identical. Validate the final serialized URL by the **same** parser used for
transport. Policy checks are on normalized scheme/host/effective port after each
redirect. No claim of WHATWG/browser parsing or complete RFC3986 generic-URI
support is made. [CURLU](https://curl.se/libcurl/c/libcurl-url.html),
[set](https://curl.se/libcurl/c/curl_url_set.html),
[get](https://curl.se/libcurl/c/curl_url_get.html).

`url.error` extends `error.error`, with `kind` (invalid_url, invalid_encoding,
invalid_utf8, unsupported_scheme, limit_exceeded), `operation` and byte offset
when reliably attributable, otherwise `-1`. Diagnostics do not echo credentials
or full query strings. This module adds no generic URL ownership handles to GC.

## 3. Requests, headers and buffered responses

| Type | Proposed fields / contract |
| --- | --- |
| `header` | Value record `name:string`, `value:[u8]`, `sensitive:bool=false`. Names are ASCII HTTP tokens; values are bytes, preserving response obs-text. Helpers copy and validate. |
| `field(name:string,value:string,sensitive:bool=false) -> result<header,error>` | Ordinary request helper: UTF-8 encodes text, validates no forbidden control/injection. An empty value is an actual empty header, not libcurl's remove-header syntax. |
| `header.text() -> result<string,error>` | Strict UTF-8 conversion; no lossy replacement or locale conversion. Header parameter grammars remain field-specific. |
| `request` | Managed input record with `method:string="GET"`, `target:string`, `headers:[header]`, `body:body=body.empty`, `auth:authentication=authentication.none`. |
| `body` | Enum cases `empty`, `bytes([u8])`, `text(string)`, `form([url.query_item])`, `multipart([part])`, `stream(i64)`; stream payload is declared length, `-1` unknown. |
| `part` | Managed record `name:string`, `filename:string`, `content_type:string`, `headers:[header]`, `data:part_data`. |
| `part_data` | `bytes([u8])`, `text(string)`, `file(io.file,i64)`; file payload includes byte count, `-1` until EOF. File is borrowed exclusively for the operation, not closed. |
| `response_info` | Owned managed metadata: `status:i32`, `version:http_version`, `url:string`, `headers:[header]`, `history:[redirect_info]`, `content_length:i64` (`-1` unknown), `content_decoded:bool`. Readonly public fields. |
| `response` | Fully received, bounded managed value: `info:response_info`, `bytes:[u8]`, `trailers:[header]`; no live network resource. |
| `response.text() -> result<string,error>` | Strict UTF-8; remove a single UTF-8 BOM, no platform codepage guessing. Absent charset assumes UTF-8; an explicit unsupported charset errors. Bytes remain available. |
| `response.require_success() -> result<bool,error>` | True for 200–299; otherwise status error with metadata, without duplicating the entire body into the error. |
| `header_values(const fields:[header],name:string) -> [header]` | ASCII case-insensitive name match, ordered independent returned header/value snapshots; never comma-fold Set-Cookie. |

Collections keep existing semantics. Submission makes an **owned snapshot** of
request graphs/byte lists/headers/config on the owner, before native use; later
mutation by another alias cannot change the request. Fixed/value arrays copy
independently; ordinary input lists remain shared until that snapshot is taken.
Source argument evaluation remains left-to-right; async bodies start according to
the existing queue, so an async snapshot happens at body execution, not magically
when the outer task handle is created. Document this distinction and test it.

Methods use their exact validated token, not a case-normalized guessed enum.
GET, HEAD, POST, PUT, PATCH, DELETE and OPTIONS are supported. Extension methods
are supported with ordinary body rules; CONNECT/tunnelling and TRACE are excluded
as application operations. A HEAD request has no request body and returns metadata
without a response body. GET body is rejected to avoid ambiguous API/proxy behavior.
Other allowed methods may have empty or explicit bodies.

Reject header names/values containing injection, NUL, CR/LF, controls except
allowed HTAB, and DEL. A native validation repeats safety checks after source
validation. Reject caller-controlled Host, Content-Length, Transfer-Encoding,
Connection, Proxy-Connection, Upgrade, Trailer, TE and HTTP/2 pseudoheaders;
framing/authority belong to transport. Authentication/Cookie/Proxy-Authorization
use explicit options, not a second conflicting raw header path. Do not depend on
libcurl sanitizing these strings: its [header API](https://curl.se/libcurl/c/CURLOPT_HTTPHEADER.html)
explicitly forwards them verbatim. Bound every response header block and total
interim/redirect/trailer metadata. The pinned provider limits each HTTP/1 trailer
field to less than 4 KiB including its terminator; VOID also enforces a
16 KiB/32-field total trailer budget. Provider extent failures return
limit_exceeded. Preserve duplicates and arrival order. Ignore
unsupported informational hints; do not publish 100/103 as the final response.
Unexpected 101 and malformed/conflicting framing are protocol failures.

`http_version` describes actual wire versions: http1_0, http1_1, http2. Separate
`protocol` is a request policy: negotiate, http1, http2_required. A configured
negotiate value is never reported as an actual response version. `redirect_info`
contains readonly `status:i32`, redacted `from_origin:string`, redacted
`to_origin:string`, and `method_changed:bool`; query strings/secrets are not
silently copied into histories or error logs. Final response URL is deliberately
available to the application as full owned data, and must not be logged by default.

Default request Content-Type is text/plain; charset=utf-8 for body.text,
application/octet-stream for body.bytes, and absent for empty/stream bodies.
An explicit single Content-Type can replace the text/byte default (for example
application/json); no transcoding is inferred. Form and multipart own their media
type/boundary and reject conflicting Content-Type overrides.

## 4. API surface and resource lifetime

Let `R<T>` in this table mean ordinary `result<T,http.error>`, **not new syntax**.
Every blocking method marked `paired` has the exact `_async` counterpart returning
an inferred task containing that same result. No async counterpart is needed for
pure inspection or immediate registration.

| Signature | Behavior |
| --- | --- |
| `open_client(config:client_options=client_options()) -> R<client>` | Nonblocking local acquisition; no DNS/network request. Private factory and rollback. |
| `client.send(req:request,options:request_options=request_options()) -> R<response>`; paired | Receive a complete bounded response, release transfer before publication; retain client pool. Reject stream body because there is no producer. |
| `client.get(target:string,options:request_options=request_options()) -> R<response>`; paired | Forward through send with GET. |
| Module `get(target:string,options:request_options=request_options()) -> R<response>`; paired | Open an ephemeral client, perform one bounded request, close; no cross-call pooling promise. |
| `client.start(req:request,options:request_options=request_options()) -> R<transfer>` | Register a streaming transfer and return **before waiting for headers or upload consumption**. Caller can immediately put it in `with`. |
| `transfer.response() -> R<response_info>`; paired | Wait for final response headers; repeated inspection returns the same metadata. |
| `transfer.read_bytes(max_count:i64=65536) -> R<chunk>`; paired | Read up to a positive max_count <=65536. `chunk` has owned `bytes:[u8]`, `eof:bool`; empty is allowed only at clean EOF. No read operation returns an ambiguous empty pending state. |
| `transfer.write_bytes(const bytes:[u8]) -> R<i64>`; paired | Stream request bodies only; input <=65536, copied to bounded native queue before acknowledgement. Empty write is a no-op, never EOF. |
| `transfer.finish_upload() -> R<bool>` | Mark producer EOF without blocking. Known-length short body errors and aborts; excess was rejected before admission. Idempotent after successful finish. |
| `transfer.finish() -> R<transfer_info>`; paired | Await complete request/response framing and trailers; unread response data returns busy rather than deadlocking behind a full queue. |
| `transfer.close() -> R<bool>`; paired | Abort/discard unread work, wait for native detachment, invalidate aliases; no unbounded drain-to-pool. Repeated close succeeds. |
| `client.close() -> R<bool>`; paired | Stop client admissions, abort/drain its transfers, close idle connections; aliases become closed. |
| `client.download(req:request,destination:io.file,options:request_options=request_options()) -> R<transfer_info>`; paired | Bounded **native bulk** response to already-open file; no source tasks or managed response chunks per block. |
| `client.upload(req:request,source:io.file,count:i64=-1,options:request_options=request_options()) -> R<response>`; paired | Native bulk upload from existing file/cursor; req body must be empty. Response remains buffered with the normal limit. |
| `client.clear_cookies() -> R<bool>` | Clear this client's in-memory session on driver; no file I/O. Same-client concurrent cookie mutation returns busy. |
| `capabilities() -> capabilities` | Immutable provider/version/features/trust/DNS/IDN identity; never claims compiled-out features. |

`transfer_info`: final `info:response_info`, `trailers:[header]`, `uploaded:i64`,
`downloaded_encoded:i64`, `downloaded:i64` and `elapsed_ms:i64`. Counts describe
actual progress, not acknowledgement of remote application processing.

Configuration records are managed ordinary source records, snapshotted at
acquisition/submission. Their complete proposed configurable fields are:

| Record | Fields and defaults |
| --- | --- |
| `client_options` | `tls:tls_options=tls_options()`, `proxy:proxy_options=proxy_options()`, `cookies:bool=false`, `max_active:i64=4`, `max_connections:i64=4`, `max_per_origin:i64=2`, `max_http2_streams:i64=4`, `max_idle:i64=4`, `idle_timeout_ms:i64=60000`, `connection_lifetime_ms:i64=300000`, `dns_cache_ms:i64=60000`, `user_agent:string="VOID"`. |
| `request_options` | `protocol:protocol=protocol.negotiate`, `ip_family:ip_family=ip_family.any`, `redirects:redirect_policy=redirect_policy.safe`, `max_redirects:i64=10`, `decoding:content_decoding=content_decoding.decoded`, `max_encoded_bytes:i64=67108864`, `max_decoded_bytes:i64=67108864`, `max_buffer_bytes:i64=67108864`, plus the five exact timeout fields/defaults in section 6. |
| `tls_options` | `trust:trust_source=trust_source.system`, `identity:client_identity=client_identity.none`. Case payloads are specified in section 6. |
| `proxy_options` | `mode:proxy_mode=proxy_mode.direct`, `endpoint:string=""`, `auth:authentication=authentication.none`, `no_proxy:[string]`; default empty list. |

`ip_family` cases are any/v4/v6; `content_decoding` cases decoded/raw.
Runtime-wide admission/network reservation, queue/chunk, header/trailer/interim,
cookie and part bounds are fixed initial implementation caps described in sections
5/7/10, not a second source configuration framework. Existing embedding Runtime
configuration can later expose changes only under a separately reviewed contract.
Validate client options against those caps before admission. Buffer budget applies
to buffered send/upload responses; streaming/bulk helpers use the encoded/decoded
transfer budgets without imposing the buffered result limit.

`capabilities` exposes readonly `transport_version:string`, `tls_backend:string`,
`resolver:string`, `idn_provider:string`, `http1:bool`, `http2:bool`, `http3:bool`,
`ipv6:bool`, `native_trust:bool`, `gzip:bool`, `deflate:bool`, `psl:bool`, and
`client_identity_forms:[string]`. This is factual provider inspection, not arbitrary
reflection or an option to turn missing required features into silent fallback.

Response and upload can overlap. Exactly one reader and one writer may be active
per transfer; other competing reads/writes return busy. A response wait may coexist
with its writer. User-level async full-duplex requires existing owner activations,
not worker source calls. An early final error response can stop an upload; a later
write reports upload_stopped with response metadata. It cannot wait forever for a
producer after the peer has finished. finish/close do not race-free another alias
by pretending that managed identity is thread safety.

`deinit` closes the same resource once; explicit `close_async` is preferred before
leaving async `with`. Existing shielded cleanup may synchronously wait for native
quiescence. A transfer retains its client/resource state until detached, so client
close cannot release transport under callbacks. Ledger shutdown must be order
independent: a client drains its own children; subsequent child close is harmless.
No finalizer-driven network I/O. Forgotten resources remain ledger-owned until
close/Runtime shutdown, matching existing file ownership.

**Acquisition safety:** returning a live resource from an async child has a
cancellation window before its parent observes the result (`io.void:451–468`).
Therefore open_client/start are immediate owner factories; all awaitable convenience
methods return fully owned buffered data/counts, not a new live client/transfer.
Native allocation, result construction and ledger publication need rollback before
last-step commit. No source factory may acquire a file/connection and then leave
it unguarded across an await.

## 5. Streaming and bounded memory

Body bytes/text/form are replayable owned snapshots. Form uses UTF-8
application/x-www-form-urlencoded. MIME uses the proven curl MIME encoder, ordered
parts, generated boundary and explicit names/filenames/content types. Reject header
injection in multipart metadata; filenames are wire metadata, never destination
paths. File parts use existing io reservations and off-driver native file service;
no reopen-by-untrusted-path or raw FILE* on the source surface. They are nonreplayable
unless the entire helper proves/restores its initial seekable cursor under its
exclusive lease. Never silently retry them.

Known lengths use checked signed 64-bit counts mapped to a proven 64-bit
`curl_off_t`. Unknown-length HTTP/1.1 uploads use transport chunk framing;
HTTP/2 uses DATA frames/END_STREAM, not chunked encoding. Short known uploads,
declared Content-Length truncation and incomplete chunks/decoders fail. HEAD/204/304
have their protocol no-body rules. Transfer chunk boundaries are not message
boundaries and do not promise UTF-8 character boundaries.

Use 64 KiB encoded receive, decoded receive and upload queues per active transfer,
with heap-owned scratch. At most four active network transfers per client and
sixteen per Runtime initially; defaults are finite admission limits, not hidden
unbounded queues. Saturation returns busy/limit_exceeded before another request
is sent. Pool limits: four active connections per client, two per origin, four
HTTP/2 streams per connection, four idle connections per client. A Runtime cap
also bounds clients (eight) and idle connections (sixteen). Validate positive
limits and internally consistent combinations. Caps can be lowered; raising
requires the configured native-memory budget to accommodate transport windows.

These queue sizes do **not** describe total memory. Admission accounts for pinned
libcurl HTTP/2 receive windows/spare pools, TLS/DNS/header/cookie/decoder state,
snapshots and pending publications. Start with a conservative 512 MiB Runtime
network admission budget and inspect the selected dependency's actual bounds;
this is a reservation budget, **not a proven RSS ceiling**. Reject configurations
whose worst-case reservation cannot fit. Do not silently disable HTTP/2 to pass a
memory test. Publish measured provider envelope and host overhead separately.

Disable curl automatic content decompression; retain transfer-framing decoding.
Decode gzip/deflate through a bounded zlib stream into the decoded queue. Stop
decoding when full, pause network input when its encoded queue fills, and resume
only from the driver thread. No callback accepts part of a buffer and then returns
PAUSE. Include decoder checksums/end markers, concatenated gzip members, trailing
garbage and truncation tests. Do not guess raw-deflate when a zlib stream fails.
Document that interoperability exclusion before approval.

Decoded mode advertises gzip/deflate only. Unknown/stacked unsupported content
encodings error; raw mode disables content decoding and preserves the encoded
body/headers. Report original Content-Encoding/Content-Length in metadata even
when decoded, with `content_decoded` preventing confusion. Default max response
encoded and decoded bytes: **64 MiB each**, independent of Content-Length. Streaming
and file callers may explicitly request larger positive i64 budgets, allowing
multi-GiB files without allocating their full contents. Buffered send remains
bounded by the requested positive buffer limit; it is never an unlimited read.

Bulk helpers reserve the existing file for their entire operation, preserve
owner/cursor/readonly rules, keep only fixed native buffers and return one small
result. Download writes at current cursor, never closes/flushes/seeks/truncates or
renames the borrowed file. Caller chooses `io.open` mode and durability. For non-2xx,
download returns status error before writing response bytes; general transfer
allows inspecting/reading an error body. File/transport failures preserve actual
partial progress and original `io.error`, and may leave a partial file. No atomic
replacement or rollback guarantee. Resume uses explicit Range/If-Range headers
and validated 206/Content-Range in application policy; no automatic resume/retry.

**Unresolved memory gate:** current async task records retain completed chunk
results until root drain. Arbitrarily many `read_bytes_async` calls are therefore
not a bounded-memory source loop today, even when each task handle is dropped.
The same issue affects source loops around io async reads. Bulk helpers must be
native operations, not wrappers around such loops. General sustained async pull
streaming cannot be called bounded/complete until Boss approves either the narrow
result-lifetime dependency or this explicit limitation. The recommendation is to
resolve the dependency before claiming general bounded streaming; do not change
repeated-await semantics or introduce unsafe reusable result aliases.

## 6. TLS, DNS, pooling and timers

TLS verifies both chain and hostname on every newly validated connection; minimum
TLS 1.2, highest available secure version negotiated. No verify=false, auto-trust,
implicit client certificate selection, early-data replay, interactive password
dialog or fallback to plaintext. `https` requires working trust; failure is Err.
Default platform trust is Schannel on Windows, SecTrust with a supported TLS
backend on macOS, and configured distribution CA locations on Linux.

`tls_options` has `trust:trust_source` (system, pem_bundle(path)),
`identity:client_identity` (none, pem(cert_path,key_path,password),
pkcs12(path,password), windows_store(reference)). Explicit custom trust means
**replacement**, not silently widening system trust. A selected provider must
implement that meaning or return unsupported. Client identity is opt-in; PEM
separate keys are unsupported under Schannel, which uses its P12/store forms.
Require a real P12 native acceptance on the selected OpenSSL builds, otherwise
report that form unavailable instead of claiming parity. The common guaranteed
client-auth route can use PEM on Linux/macOS and P12/store on Windows.
Paths are UTF-8 host paths with existing safe conversion, unrelated to source VFS.
Loading certificates must not block the managed executor during an async request.

CA/identity options are immutable per client; changing trust requires a new
client and pool. Explicit PEM/P12/key input paths must resolve to regular files;
client creation reads an owned snapshot with a combined 1 MiB input budget.
Source pathname or file-content mutation afterward cannot change this snapshot.
A FIFO/device cannot block the native network driver. System trust retains the
native/distribution loading policy. Parsed-provider/system-store accounting is
separate from this input-byte cap and remains subject to the provider audit. Pool reuse honors origin, TLS identity/trust, proxy and protocol
isolation. Disable cross-origin HTTP/2 connection coalescing unless the provider
can prove equivalent security policy. Connections expire after at most five
minutes lifetime and sixty seconds idle; DNS cache uses a finite sixty-second
TTL candidate. OS trust/revocation changes may take effect only on revalidation;
never claim continuous certificate revalidation on an already-open socket.
Retain backend revocation policy; do not promise identical OCSP/CRL behavior on
three OSes. No certificate pinning/revocation bypass surface initially.

Prefer dual-stack DNS and libcurl Happy Eyeballs; `ip_family` option is any/v4/v6.
No public general DNS resolver is needed. c-ares compatibility needs explicit
native acceptance. Bound socket/connect counts, idle caches and DNS entries. HTTP
keep-alive/pooling and optional TCP keepalive are different mechanisms; neither
prevents read/operation timeouts. No HTTP/1.1 pipelining.

`request_options` includes the following milliseconds, measured by a monotonic
clock. Negative values error; connect_timeout_ms must be positive. Zero disables
the other individual timers. All millisecond fields are at most 2147483647 for a
portable checked mapping to the provider's Windows long arguments; larger values
error before admission. All defaults are finite. The total deadline is never
reset by a redirect/authentication hop. Do not interpret libcurl's zero connect
timeout as disabled: it actually selects its built-in default.

| Option / default | Observable interval |
| --- | --- |
| `connect_timeout_ms=10000` | Per new connection: DNS, proxy negotiation, TCP and TLS handshake, also bounded by remaining total. |
| `header_timeout_ms=30000` | Waiting for each final header block after that hop's request body is sent (or request headers for an empty body); informational blocks do not reset the deadline. An early final response is handled immediately during upload. |
| `read_timeout_ms=30000` | No incoming response-body progress while network input is demanded. Excludes intentional consumer-backpressure pause. |
| `write_timeout_ms=30000` | No outgoing body progress while upload bytes are available. Excludes waiting for a source producer. |
| `total_timeout_ms=120000` | Admission through complete transfer, including queueing, redirects, consumer/producer pauses and file work. Streaming users explicitly choose another finite deadline or zero. |

Do not implement these five timers with just curl's LOW_SPEED settings. Use
connect timeout plus driver deadlines/progress; clamp poll to the nearest deadline
and a 50 ms cancellation-service quantum. Cancellation wakeup is immediate where
possible. Millisecond options do not promise real-time scheduling or forcibly
interrupt every OS certificate/filesystem call. Report drain latency separately.
References: [connect timer](https://curl.se/libcurl/c/CURLOPT_CONNECTTIMEOUT_MS.html),
[total timer](https://curl.se/libcurl/c/CURLOPT_TIMEOUT_MS.html),
[progress callback](https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html).

## 7. Redirects, authentication, proxies and cookies

`redirect_policy`: none, same_origin, safe; default safe, max_redirects=10. Zero permits no hops and returns
redirect_limit if a follow policy encounters a redirect; explicit none returns
the redirect response for inspection. Keep status/from/to/method-change history within the header/URL limits.
One native hop controller uses CURLU and curl response metadata; it does not parse
HTTP wire framing. It must make these policies independent of evolving driver
defaults. `FOLLOWLOCATION` is disabled when explicit policy inspection is needed.

- Follow only HTTP(S); block HTTPS-to-HTTP downgrade, userinfo and invalid targets.
- 303 becomes GET except HEAD; discard body and content headers. 301/302 change
  POST to GET; other methods retain their method/body. 307/308 preserve both.
- Replaying a stream/file body requires a proven rewind contract; otherwise return
  body_not_replayable before another body attempt. No hidden buffering to enable it.
- Same-origin means normalized scheme, ASCII host and effective port. Cross-origin
  safe follow is limited to unauthenticated GET/HEAD after any permitted method
  change. Drop **all caller headers**, not just Authorization, before following;
  reconstruct only transport-owned headers and correctly scoped cookie-jar data.
  If explicit auth or any sensitive input credential is present, return redirect_denied.
- Authorization, proxy credentials and client identities never migrate to a new
  origin implicitly. A client with mTLS credentials uses same-origin redirects
  only. No downgrade or credential-forwarding override in the initial API.
- Bound/discard intermediate bodies without exposing them as the final body and
  without unbounded draining. Loop/limit errors retain safe history.

`authentication` cases: none, bearer(token), basic(user,password), digest(user,password).
Basic/bearer/digest credentials require HTTPS to the origin; no implicit netrc or
logged secrets. Basic/bearer can be sent preemptively to their exact configured
origin. Digest is a challenge exchange and inherits replay restrictions. 401/403
remain inspectable HTTP responses, not automatic exceptions or login UI. OAuth
token acquisition/refresh/sign-in is application policy; sending tokens is covered.

`proxy_options`: `mode` direct/environment/explicit, default **direct**;
`endpoint:string`, `auth:authentication` (none/basic/digest), `no_proxy:[string]`.
Support HTTP forward proxy, HTTP CONNECT, HTTPS proxy with independently verified
proxy TLS, and SOCKS5 local/remote DNS selection. Proxy credentials are isolated
from origin fields. Validate endpoint separately against the permitted proxy
schemes with the same parser infrastructure. Environment mode snapshots documented
proxy/no_proxy variables without mutating global environment. Native PAC/WPAD,
desktop auto-proxy settings, SSPI/Kerberos/NTLM automatic login are explicit
exclusions, not capabilities implied by libcurl build flags.
Proxy credentials also need a protected proxy channel: do not send basic/digest
proxy credentials to an unencrypted HTTP/SOCKS hop in the default contract.
Anonymous HTTP/SOCKS proxies remain usable. Any insecure authenticated-proxy
exception is an additional explicit owner decision, not a hidden fallback.

`client_options.cookies` defaults false. When true, libcurl owns an in-memory
session jar with PSL checks, normal domain/path/expiry/Secure/HttpOnly processing,
and no browser SameSite context claim. Jar state is per client, survives same-client
requests and clears at close. Default jar budget: 1024 cookies / 1 MiB serialized
state; per-response header limits also bound a temporary overshoot. Check after
bounded header processing, before another hop/admission; on limit clear the jar
and fail the request with limit_exceeded instead of allowing permanent growth.
The pinned provider's allocation/ordering must establish this temporary envelope.
Malformed cookies rejected by the provider are ignored per cookie semantics;
oversized/overbudget headers fail the request. Do not write a second cookie parser.
In particular, libcurl treats plain-http localhost as a secure cookie context;
retain and document that provider compatibility exception rather than claiming
Secure cookies can never be sent over local HTTP.

Do not use raw Set-Cookie import without an origin, which can over-broaden scope.
Persistent cookie files and import/export are proposed exclusions initially: the
native `COOKIEJAR` API cannot reliably report file write failure, and even cookie
list enumeration must distinguish allocation failure from an empty jar. Persistence
needs a separate checked io-based contract before it is advertised. This is a
genuine scope approval, not a promise of unnoticed future work.
[Cookie rules/PSL](https://curl.se/docs/http-cookies.html),
[COOKIEJAR failure reporting](https://curl.se/libcurl/c/CURLOPT_COOKIEJAR.html),
[unsafe domainless import](https://curl.se/libcurl/c/CURLOPT_COOKIELIST.html).

## 8. Errors, cancellation and native state machine

`http.error` extends shared `error.error`. Readonly fields: `kind:error_kind`,
`operation:string`, `phase:failure_phase`, `native_domain:string`,
`native_code:i64`, `status:i32` (0 before response), `info:response_info?`,
`uploaded:i64`, `downloaded_encoded:i64`, `downloaded:i64`,
`io_cause:io.error?`, `url_cause:url.error?`, `cleanup_code:i64` and
`remote_outcome_unknown:bool`. Inherited code is the stable public category,
message is bounded safe text and path is a redacted origin/location.

Kinds: invalid_argument, invalid_url, invalid_header, invalid_encoding, unsupported,
closed, busy, limit_exceeded, resolve_failed, connect_failed, tls_verification,
tls_failure, proxy_failed, timeout, protocol_error, redirect_denied, redirect_limit,
body_not_replayable, upload_stopped, unexpected_eof, io_failure, http_status,
transport_failure. Phase: prepare/queue/dns/connect/proxy/tls/headers/upload/download/
decode/file/cleanup. Do not invent a stable mapping from arbitrary native message
text; preserve CURLcode/OS domain separately.

Transport completion with 404/500 is `Ok(response)`. A timeout/protocol error
after headers is `Err` with metadata and actual progress. A stream can deliver a
prefix successfully and fail a subsequent read/finish. Do not report EOF before
decoder checksum/framing succeeds. Errors do not copy a potentially huge or secret
body; caller already owns previously delivered chunks. Runtime allocation failure
remains the existing checked Runtime failure, including after external effects.
Publication failure must still detach transport/release reservations without
another allocation. HTTP side effects are not rolled back by Err/cancellation.

Structured task cancellation remains **cancellation**, not an `Err(cancelled)`.
Cancel an HTTP convenience task through its ordinary task handle. Cancelling a
pending streaming read/write aborts that transfer conservatively; other transfers
continue. A read operation cannot declare cancellation while its buffer callback
still runs. Explicit transfer.close is a recoverable API to abort without asking
the language to catch cancellation. No automatic retries of application requests;
provider internal reconnect behavior must not replay a nonreplayable body or claim
at-most-once remote effects. All interrupted mutating operations can have unknown
remote outcomes.

Native transitions:

`prepared -> admitted -> resolving/connecting -> sending/receiving -> complete`
with independently tracked upload/response state, plus `abort_requested -> detached`
and `failed -> detached`. One driver owns CURLM/easy operations; owner commands use
an owned bounded queue and `curl_multi_wakeup`. Pause/resume/remove happen on that
driver. Client CURLM caches are separate; no cross-thread CURLSH connection-cache
sharing. Only cancellation/completion atomics and safe queue data cross threads.
Completion tickets publish once with release/acquire, keep native state alive
through removal/cleanup and wake the current executor. Do not hold transport mutexes
while attaching to Runtime, allocating managed values or publishing failures.

Extend existing native admission, task Work/wait/receive/abandon logic, not typed
source scheduling. Cancellation must reach external tickets from every parent/
group/Runtime-close path. A Runtime-close network stop notification occurs **before**
waiting for native_jobs==0; this ordering is mandatory. Driver threads and bulk
file jobs must quiesce before roots, file reservations and generation leases
release. A completed async root leaves no active network transfer secretly running;
resources explicitly held outside it can retain only idle pools/configuration.

Streaming start is synchronous admission but, when called within an async root,
its transfer must register with the current activation's native-resource scope.
That scope aborts/drains it at exit even if the programmer forgot `with`. Otherwise
a started transfer with no pending read task could evade structured cancellation.
This native-resource scope association is a required small lifetime integration,
not implemented by today's NativeLibrary ledger. Sync callers use explicit with/
close or existing Runtime shutdown. Preserve this distinction in tests.

Bulk file work reuses filesystem kernel operations and NativeJob off-owner safety.
At most one bulk file job runs per Runtime initially, with finite admission;
it needs only that worker and the independent network driver to make progress.
It must not enqueue another worker job and synchronously wait on it. This avoids
deadlock at worker_limit=1. Slow bulk I/O can still delay unrelated queued file
jobs on a one-worker Runtime; measure and document that throughput tradeoff.
Do not add an unbounded private file thread pool to hide it.

## 9. Protocols and explicit exclusions requiring approval

| Area | Proposed initial boundary and justification |
| --- | --- |
| HTTP/1.1 | Required, plain HTTP and verified HTTPS; persistent connections, chunking, duplicate headers, trailers and proper truncation errors. Valid 1.0 responses may be accepted by the provider without claiming a separate 1.0 client mode. |
| HTTP/2 | Required for HTTPS with nghttp2/ALPN, multiplexing with bounded admission. Default negotiate h2 then h1.1; explicit `protocol.http1` and `protocol.http2_required` modes must report/require actual version, never silently pretend success. Disable unsolicited push. |
| HTTP/3 | Proposed excluded: requires QUIC/nghttp3/ngtcp2 or another QUIC stack and compatible TLS across all platforms; adds UDP, migration, deployment and acceptance obligations. Ordinary HTTP/1.1/2 APIs/files/auth remain complete within their approved protocol scope. Boss must approve exclusion. |
| h2c upgrade/prior knowledge | Proposed excluded; HTTPS h2 is the initial secure negotiation surface. |
| Other protocols | Server/listen APIs, WebSocket, CONNECT tunnels, raw TCP/UDP/TLS, HTTP caches, gRPC framing and SSE event/reconnect parsing excluded. Generic response streaming still reads SSE bytes; no event decoder is claimed. |
| Browser/enterprise integration | PAC/WPAD, browser cookie store, persistent cookies, automatic SSO, OAuth flows and general legacy charset transcoding excluded; explicit proxy, cookie sessions, token auth and UTF-8/bytes are included. |
| Rare URL/codec behavior | Non-HTTP URI schemes, IPv6 zones, brotli/zstd and heuristic raw-deflate decoding excluded. Only advertised codecs are decoded; raw bytes remain available. |
| TLS extensions | Pinning, arbitrary TLS callbacks, custom crypto, TLS1.3 0-RTT and portable identical revocation behavior excluded; verified TLS, custom CA and explicit mTLS forms included. |
| Async CLI | Existing explicit JIT/AOT host adapters supported. Automatic CLI async main remains outside this module, unless Boss expressly expands entry semantics. Sync CLI HTTP is supported. |
| Bounded pull loops | **Unresolved dependency, not an approved exclusion.** General indefinitely repeated async chunks cannot be claimed memory bounded on today's Runtime. Native bulk paths must be bounded independently. |

No unsupported protocol can be enabled by a string option. `capabilities()`
reports actual versions/backends; missing required HTTP/2/TLS/async-DNS/PSL support
fails the official build/package gate. Do not ship an official build silently
reduced to HTTP/1.1.
The required-h2 policy accepts HTTPS only, advertises h2-only ALPN using the
selected version's supported option and cannot reuse an h1 connection. Reject a
plain-http required-h2 request before sending it. Check the actual result too;
a post-response version check alone is insufficient for a request already sent
using a forbidden protocol. Partition/select compatible pool state accordingly.

## 10. Security and compatibility invariants

Allow only HTTP(S) at the native boundary and every redirect; no file://, custom
binding or source-import escape. Enforce header/framing validation, byte budgets,
bounded histories/metadata/cookies and finite connection lifetimes. Default
max header block is 64 KiB / 128 fields; aggregate interim/redirect/trailer metadata
is 256 KiB / 512 fields; interim count is 16, trailer block 16 KiB / 32 fields,
multipart parts 128. Counts/sizes are checked before addition/multiplication and
before managed allocations. Verify upstream pre-callback header/HPACK limits;
these caller limits alone cannot prove native allocation bounds.

No secrets in default logs: redact userinfo (already rejected), query strings,
Authorization, cookies, proxy passwords, key passwords and provider error-buffer
URLs. Explicit body/header data returned to applications is not a secret boundary.
No promise that GC strings or copied native snapshots can be securely zeroized.
Keep verbose wire logs off; ensure dependency TLS key logging/debug-environment
features are explicitly disabled by the release build or documented developer
opt-in, not silently controlled by inherited application environment. Do not change
global environment or signal handlers to make networking work. Audit NOSIGNAL/
SIGPIPE behavior and selected native backend paths on each supported platform.

Plain HTTP and private/LAN addresses are available for games, so this client is
**not an SSRF sandbox**. Apps accepting attacker-supplied URLs need an origin and
network-egress policy, including redirect/DNS rebinding/proxy handling. TLS verifies
the requested hostname; it does not authorize the destination. Do not add a vague
private-address heuristic and claim complete protection.

Compatibility: dependency/build feature identity participates in existing Runtime
ABI/object-cache identity. Cache source/object artifacts, never connections,
response bytes, cookies or DNS answers. Compatible source-body reload keeps current
active native tickets safe under their originating generation lease; no JIT code
pointers in transport state. A native provider/ABI/layout/configuration change
requires restart. Metadata-only documentation edits keep their existing reload
rules. Deep readonly rejects send/start/close/read/write/cookie mutation through
readonly clients/transfers while permitting immutable capability/response inspection.

## 11. Working-style examples

Illustrative proposed APIs, **not compiled examples**. Each demonstrates the
surface without claiming the module or automatic async CLI already exists.

```void
use net/http
use json

func fetch_profile() -> result<http.response, http.error>:
    var response = http.get("https://api.example.com/profile")?
    response.require_success()?
    var text = response.text()?
    # json.error is a different error type: handle/map it explicitly.
    if json.parse(text) is Ok(var value):
        print("Profile received")
    else:
        print("Invalid profile JSON")
    return Ok(response)
```

This example handles parse failure locally; an application wanting propagation
should preserve the actual JSON cause in its own error type. No implicit
conversion between module error types is proposed. A generic `http.json<T>`
shortcut is unnecessary to make JSON services practical.

```void
use net/http
use io

# Use inside an application/game function that returns result<bool,http.error>.
with http.open_client()? as client:
    var req = http.request(target = "https://cdn.example.com/assets.pack")
    # The caller opens/guards the destination and handles its distinct io.error.
    # destination is an existing mutable io.file in the surrounding scope.
    var saved = client.download(req, destination)?
    print("Downloaded:", saved.downloaded)
```

Native bulk `_async` counterparts use the existing explicit async host adapter:

```void
async func load_asset(client: http.client, destination: io.file) -> result<bool, http.error>:
    var req = http.request(target = "https://cdn.example.com/assets.pack")
    var saved = await client.download_async(req, destination)?
    return Ok(saved.downloaded > 0)
```

Streaming upload, including producer EOF, without awaiting headers first:

```void
async func send_packet(client: http.client, const packet: [u8]) -> result<bool, http.error>:
    var req = http.request(method = "POST", target = "https://api.example.com/upload",
                           body = http.body.stream(packet.count))
    with client.start(req)? as transfer:
        # This illustration assumes packet.count <= 65536.
        await transfer.write_bytes_async(packet)?
        transfer.finish_upload()?
        var info = await transfer.response_async()?
        # Read/discard its bounded body before finish, or close explicitly.
        await transfer.close_async()?
        return Ok(info.status >= 200 and info.status < 300)
```

Authentication and recovery use ordinary results and task cancellation:

```void
var req = http.request(target = "https://api.example.com/save",
                       method = "POST", body = http.body.text(payload),
                       auth = http.authentication.bearer(token))
var pending = client.send_async(req)
# Other ready owner tasks can run while the network driver works.
# pending.cancel() requests structured cancellation; it is not catchable Err.
if (await pending) is Err(var failure):
    print("Request failed:", failure.kind, failure.remote_outcome_unknown)
    # Decide whether retrying this application operation is safe.
```

## 12. Genuine owner decisions

1. **Provider/deployment profile:** approve libcurl multi, nghttp2, selected native
   trust backends, c-ares compatibility boundary and platform IDN differences;
   choose bundled/static versus distribution libraries after exact licensing and
   native feature probes. Recommend the shared architecture, native trust and
   explicit dependency inventory; no uncontrolled system-curl fallback.
2. **Required Runtime integration:** approve the narrow external native completion,
   pre-wait shutdown notification and activation-associated streaming lifetime
   work. Recommend it; existing worker-only execution cannot safely meet the
   combined streaming/concurrency contract on its own.
3. **Bounded async pull semantics:** approve a scoped result-lifetime investigation
   and repair before claiming general bounded pull loops, or explicitly accept a
   contract limited to bounded native bulk transfers plus retained-result pull
   calls. Recommend resolving it if sustained chunk processing is required; never
   erase results merely because the first await completed.
4. **Protocol/advanced exclusions:** approve the explicit table above as the initial
   supported product boundary, especially HTTP/3 and cookie persistence. If any is
   essential, enlarge this same mission before engineering; do not label partial
   behavior complete.
5. **Policy defaults:** approve safe redirects/credential isolation, explicit proxy
   opt-in, cookies off, finite time/size/admission budgets, UTF-8 decoding and
   non-2xx-as-response behavior. Recommendations are specified above for review;
   these are observable library choices, not already settled language semantics.

Existing readonly, error propagation, lexical scope, worker ownership and GC
rules already decide the rest. Do not ask Boss to re-approve those contracts.
