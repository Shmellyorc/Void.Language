# Windows in-memory TLS provider architecture review

3 November 2026. Baseline `51e7ff6f8c202db56fc0a317b87d73ddb10972d9`;
LLVM/Clang/LLD 23.1.4. Architecture research and isolated native evidence only.

**Subsequent adapter evidence:** the [native security integration investigation](WINDOWS_TLS_ADAPTER_FEASIBILITY.md)
proves bounded Windows verification/CNG signing but rejects the public-hook
integration after an origin identity reached a requesting HTTPS proxy. IP peer
context, AWS-LC async retry and Runtime deadline/lifetime integration also remain
unresolved. Its current recommendation is to retain Schannel for the full Windows
contract; the conditional proposal below is historical, not migration approval.

**AWS-LC is practical for in-memory Windows P12 authentication, including TLS
1.3 on this Windows 10 VM. It is not an unchanged, security-equivalent replacement
for Schannel.** Its default revocation behavior and Windows store identity support
do not meet the existing Windows contract. No provider migration is approved or
implemented. Stop for Golden Wrench review after the authorized research push.

**Preferred transport architecture: one AWS-LC backend and the existing HTTP
driver, conditional on resolving Windows verification and store-identity policy
before implementation.** Preserve Schannel in production meanwhile. If the
current native Windows semantics remain mandatory, the next design must prove
bounded Windows verification and explicit store-signing adapters; importing roots
alone is insufficient. If Boss instead approves a narrower AWS-LC capability
contract, that is an explicit security/API decision, not a transparent fix.
Do not introduce two libcurl images merely to make per-client selection appear
available. This recommendation follows working transport and negative controls,
not compilation alone.

## Current Schannel boundary

The existing [credential design](WINDOWS_MTLS_CREDENTIAL_DESIGN.md),
[native feasibility](WINDOWS_MTLS_FEASIBILITY.md) and
[hosted failure review](CI41563174932_SCHANNEL_REVIEW.md) remain the historical
evidence. Real RSA/ECDSA TLS 1.2 through an explicitly selected persistent store
credential works. NO_PERSIST P12 fails the Schannel handshake path; named
persistence was rejected because exclusive ownership could not be established.
This assignment does not rerun those collision experiments.

Schannel provides Windows chain/trust policy, ordinary revocation checking and
explicit access to pre-provisioned Windows store identities, including keys that
need not be exportable. Automatic client identity selection remains disabled.
Pinned `schannel.c` sets chain revocation flags; `schannel_verify.c` uses
`CertGetCertificateChain`, including the exclusive-root engine for explicit CAs,
and rejects revoked/unknown/offline statuses rather than silently disabling them.
Explicit origin/proxy CA snapshots replace ambient trust in the approved Runtime
configuration. The Windows 10 Schannel TLS 1.3 limitation is specific to that
provider/host combination; it does not constrain AWS-LC's TLS implementation.

## Confirmed native AWS-LC results

The [reusable isolated sources](native-awslc/README.md) and
[compact receipts](evidence/windows-awslc-provider-20261103.json) record
**38 controls per profile, 76 total**, plus **four MultiSSL selection controls**.
Debug and Release both pass their intended assertions. Some controls deliberately
record a missing security equivalence, particularly default revocation.

| Environment | Actual identity |
| --- | --- |
| OS | Windows 10 Home 22H2 x64, `10.0.19045.6456` |
| Compiler/linker/SDK/CRT | LLVM Clang/LLD 23.1.4; SDK 10.0.19041.0; independent Debug/Release, `/MT` |
| AWS-LC | Approved 5.12.0, revision `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8` |
| AWS-LC archive | SHA-256 `1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68`; 154,548,427 bytes; no source modification |
| Transport/H2 | Unchanged pinned curl 8.23.0, nghttp2 1.70.0; native WinIDN and Windows SSPI compiled |
| Build helpers | Pinned NASM 2.16.03; existing Git Perl; CMake 4.4.3/Ninja |
| Fixture/peer | Git OpenSSL 3.5.9; Python 3.14.8/OpenSSL 3.5.9; pinned h2 4.4.1/hpack 4.2.0/hyperframe 6.1.0 |

The repository, archive checksums and existing SDK were used locally. No Linux
worktree or large worktree archive was transferred. The isolated curl intentionally
omits PSL, c-ares and compression; it is not the complete production dependency
closure and cannot pass `require_features()` unchanged.

| Native control | Debug and Release observation |
| --- | --- |
| RSA-2048 and ECDSA-P256, TLS 1.2 and TLS 1.3, h1 and h2 | All 16 algorithm/version/protocol/profile positive combinations authenticate; peer compares the entire expected client DER and records negotiated TLS/ALPN |
| In-memory P12 | Default AES/PBES2 P12 parses from bounded memory; certificate/private-key correspondence checks; curl uses the owned memory blob |
| Wrong password | Direct parse rejects before TLS; no Windows key creation |
| Anonymous client | Required-client-certificate peer rejects |
| Wrong root / wrong hostname | Curl error 60; no verification bypass |
| Connection reuse | Every positive control makes two requests; new connections `1`, then `0` |
| Native roots plus explicit CA | SSL-context observation sees 20 objects: 19 imported native anchors plus the fixture root |
| Explicit CA replacement | Each direct P12 positive has exactly one trust object; no native/file/directory fallback |
| Native-only / native plus unrelated explicit CA | The generated untrusted fixture fails; native-root import is observed, but no positive native-system-root server acceptance is claimed |
| Explicit valid CRL before revocation | Succeeds |
| Server revoked, default AWS-LC policy | Still succeeds; zero HTTP fetches of the advertised CRL distribution point |
| Server revoked, explicit CRL supplied | Curl error 60 |
| OCSP staple required, none supplied | Curl error 91 |
| Verified HTTPS CONNECT, server-only proxy | Origin P12 mTLS works through the verified TLS proxy |
| TLS proxy demands a client certificate | Origin identity alone fails; explicitly configured proxy P12 succeeds with exact DER at both peers |
| Wrong proxy root / hostname | Curl error 60 before CONNECT; proxy identity cannot bypass trust |
| Independent owners | Two native processes authenticate concurrently; this is not a same-process Runtime/client isolation test |
| Cleanup/inventories | Fixture directories removed; one pre-existing CurrentUser Software KSP key and eight store inventories unchanged; no test-created OS credentials |

The RSA revocation/proxy/native-root controls run in both profiles; algorithm,
protocol and required origin negatives also cover ECDSA. Proxy tests use TLS 1.3
for both outer and inner TLS, as recorded by the peers. Proxy CRL/OCSP freshness
policy and HTTP/2 proxy multiplexing are not claimed tested. No forced downgrade,
insecure flag, auto-client-cert selection or provider fallback is used.

Original failures are retained: missing Perl discovery, CMake Windows archive
discovery, imported-target directory scope, a probe-only Windows/OpenSSL header
macro collision, Python's verified-download failure, and CRLF password transport
between Python text mode and Git OpenSSL. The latter generated a P12 with a
different password than intended; it was a fixture defect, not unsupported
AES/PBES2. Binary password input plus an independent OpenSSL password check repairs
that fixture. Windows curl downloaded the checksum-pinned test wheels using
normal Schannel verification; no trust configuration was changed. Public build
failure excerpts and preliminary rejection receipts are included independently of
the final successful results. Full disposable logs remain under `.toolchains/`.

## Pinned curl backend selection and ownership

[CMakeLists.txt](../../../third_party/curl/CMakeLists.txt) counts enabled TLS
backends and enables MultiSSL when both Schannel and OpenSSL/AWS-LC are selected.
AWS-LC is the **openssl** backend name, with an actual version string of
`AWS-LC/5.12.0`; the OpenSSL compatibility header version is not a different
provider dependency. Both single and dual-backend images compiled natively.

[vtls.c](../../../third_party/curl/lib/vtls/vtls.c) owns one global `Curl_ssl`
pointer per libcurl image. `curl_global_sslset` chooses it before global
initialization; subsequent selection of the other backend returns
`CURLSSLSET_TOO_LATE`. Native controls confirm `first_selection=0`,
`global_init=0`, `other_selection_after_init=2` in both directions and profiles.
The [public selection contract](https://curl.se/libcurl/c/curl_global_sslset.html)
also requires selection before initialization. This is neither per-driver,
per-CURLM, per-connection nor per-request selection. Final global cleanup can
reset the image's backend state; that requires all leases/connections to drain
and cannot support simultaneously different clients.

[http_provider.cpp](../../../src/runtime/http_provider.cpp) maintains a mutex and
process-wide ProviderLease reference count. The first lease initializes curl and
checks provider/features; the final lease cleans it up. Different VOID Runtimes
still share that provider state. A second Driver or Runtime does not create an
independent TLS backend. MultiSSL also has environment/default-backend selection;
any approved use would need an explicit pinned selection before the first lease,
without an environment-driven or error-triggered fallback.

The [existing driver](../../../src/runtime/http.cpp) has a separate CURLM,
connection pool and owned CA/certificate/key snapshots per ClientState. Transfers
retain their client owner. Shutdown drains transfers, cleans easy/share/multi
objects, then releases owned snapshots and admission. Curl's
[TLS configuration comparison](../../../third_party/curl/lib/vtls/vtls_config.c)
includes identity blobs, password, trust and relevant options when matching
connections. A single AWS-LC backend fits those existing ownership boundaries;
it does not require another driver, CURLM per client, Runtime, worker pool or GC
authority. Native easy-handle reuse demonstrates the transport mechanism, not
full native Runtime cancellation, GC or shutdown acceptance.

## Trust, revocation and identity compatibility

The pinned [OpenSSL backend](../../../third_party/curl/lib/vtls/openssl.c) reads a
P12 memory BIO, parses EVP/X509 objects, installs them on SSL_CTX, transfers chain
ownership, and frees parser references. Client-owned blob storage remains alive
through native cleanup. Supplied P12 needs no CNG import, store registration,
named key, ownership journal or key deletion transaction.

With `CURLSSLOPT_NATIVE_CA`, `ossl_windows_load_anchors` imports Windows **ROOT**
and **CA** certificates into the AWS-LC X509 store. It filters validity, signing
usage and server-auth EKU, then AWS-LC builds/verifies the chain and curl checks
the hostname. It does **not** call Windows' chain policy verifier. This does not
establish equivalent enterprise/disallowed-store policy, automatic root updates,
AIA/intermediate retrieval or online revocation. Native import is additive to
explicit roots when its flag is enabled, as the
[curl option contract](https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html) states.
It must be off for approved explicit-CA replacement semantics.

This probe clears CAINFO/CAPATH and proxy equivalents, disables CA fallback and
OpenSSL configuration loading, and uses a bounded CA blob. The one-object
measurement confirms replacement for the generated fixture. A migration must
select native roots for default trust, clear them for explicit trust, and remove
the Runtime's OS-only assumption that CAPATH clearing is unnecessary on Windows.
The current Schannel-only dependency settings do not automatically configure
AWS-LC default trust. Native-root counts do not certify Windows policy parity.

Default AWS-LC/curl does not fetch CRL distribution points or make Schannel's
online chain-revocation checks. `CURLOPT_CRLFILE` enables CRL checks, and
`CURLOPT_SSL_VERIFYSTATUS` requires a valid stapled OCSP response. Those mechanisms
are different policies with distribution/freshness/failure requirements, not a
substitute silently enabled by importing roots. Current public TLS options expose
neither a CRL snapshot policy nor a revocation-mode decision. HTTPS proxy trust
has the same provider difference; source supports a separate proxy CRL file, but
this study does not establish a complete proxy revocation design.

`client_identity.windows_store(...)` currently reaches Schannel's explicit store
branch. AWS-LC's curl certificate loader has no equivalent Windows private-key
selector. A non-exportable CNG/CAPI/smart-card key cannot be turned into an
in-memory P12 by assuming export authority. Passing a store reference into the
AWS-LC PEM/file path is unacceptable; capability rejection must precede fallback
file access if a narrower provider profile is approved. Native Windows store
support therefore needs Schannel or an explicit Windows signing adapter.

Pinned AWS-LC exposes `SSL_CTX_set_custom_verify`, peer-chain access and
`SSL_CTX_set_private_key_method` in
[its exact SSL header](https://github.com/aws/aws-lc/blob/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8/include/openssl/ssl.h).
Curl exposes SSL_CTX_FUNCTION, exercised here only for read-only observation.
These are source-supported hooks for a future Windows policy/signing adapter,
not a demonstrated adapter. A design must prove exclusive explicit-root versus
system trust, fail-closed Windows revocation/hostname policy, key/certificate
matching, RSA-PSS/ECDSA signature formatting and non-exportable key lifetime.
Device UI, cancellation and blocking policy calls also need native proof.
AWS-LC's asynchronous custom verification/private-key retry errors do not have
corresponding cases in the pinned curl handshake wait handling; asynchronous
integration cannot be assumed available without further reviewed work. No such
adapter, curl modification or verification bypass is implemented here.

Origin P12 and HTTPS-proxy P12 are separate curl options. Native tests explicitly
configure both only for the proxy-identity control. VOID currently applies its
identity to the origin and shares explicit CA trust with origin/proxy; its proxy
authentication record is HTTP Basic/Digest, not a proxy TLS identity record.
This research demonstrates upstream proxy capability, not a newly exposed API.

## Practical architecture comparison

| Arrangement | Resources/ownership | Security and compatibility | Assessment |
| --- | --- | --- | --- |
| A. One AWS-LC backend | One libcurl image, existing driver and per-client CURLM/pool; SSL/crypto static archives; no supplied-P12 OS persistence | Native results support P12, TLS 1.2/1.3, h1/h2, verification and proxy transport. Windows policy/store signing require adapters or explicit contract changes | **Preferred transport direction, conditional; migration HOLD** |
| B1. Schannel and AWS-LC in one MultiSSL image | Both backend code/closures; one global selection | Cannot serve differently selected simultaneous clients. Switching around requests would violate lease/pool ownership | Reject as a per-client solution |
| B2. Separate private provider DLLs | Two libcurl images with independent globals; provider-tagged handles and dispatch; separate backend closures, TLS caches and installer resources | Can in principle preserve both capability sets while reusing one HTTP scheduler. Every call/allocation/free needs the owning image; not natively demonstrated here | Reserve only if simultaneous legacy-store and P12 capability is mandatory and adapters are rejected |
| C. Separate explicit Windows build profiles | One provider per executable/process; common HTTP implementation; two supported artifact/test matrices if both shipped | Avoids duplicated live globals, but capabilities differ by profile; does not solve unchanged AWS-LC revocation or same-process mixed clients | Narrow delivery alternative after explicit capability/policy approval |
| Schannel plus pre-provisioned store only | Existing driver/provider; caller/admin owns credentials | Retains native behavior; supplied in-memory P12 remains unsupported | Smallest immediate supported scope if policy changes/adapters are declined |

Two statically linked unmodified libcurl copies collide on symbols. Two ordinary
Runtime instances using the same DLL still share one image. Private DLL dispatch
would need complete handle/allocator provenance, feature reporting, error and
URL/MIME/list routing, installation and cleanup checks; it is not just two calls
to `curl_global_sslset`. A helper-process HTTP service would additionally introduce
credential IPC, framing and cancellation machinery without a demonstrated need.
Neither is the preferred fix. No second HTTP implementation is proposed.

## Dependency, Runtime and memory impacts

An approved A implementation would extend the existing immutable AWS-LC pin's
platform policy and native setup/verification/manifests for Windows, with SSL and
crypto archives in the install/link closure and their exact LICENSE/NOTICE/source.
It would preserve nghttp2, c-ares, zlib, PSL, WinIDN, matching architecture and
CRT/profile checks. CMake must explicitly select Windows configuration-specific
AWS-LC archives and the native C++ runtime. NASM/Perl are build prerequisites,
not production TLS runtimes. No new provider version/fork is needed for this
transport proof; production Windows authorization is still separate from the
existing Linux/macOS approval and Windows test-only certificate tool.

`require_features()` currently insists on Schannel on Windows. Provider capability
checks, native trust defaults, explicit-root handling and identity admission would
need reviewed changes; merely replacing an archive would fail that guard or alter
policy. A needs no new source configuration record if it becomes the single
approved Windows provider. B2 or a runtime-selectable provider policy would need
additional configuration/ownership design and Boss approval. The four approved
configuration records are not silently expanded by this assignment.

Current ClientState/CURLM limits, cancellation, connection pooling, ProviderLease,
Runtime drain/join and GC authority should remain structurally unchanged. The
implementation must retain client snapshots until easy and pooled SSL objects
are gone, then release credentials before final provider cleanup. This probe
explicitly frees direct EVP/X509/P12 references and zeroes transport/password
buffers; it does not certify every provider allocation or secure erasure of all
internal copies. Full native Runtime integration has not been exercised.

The [provider memory boundary](MEMORY_REVIEW.md) remains open. P12 parsing occurs
inside process-owned SSL contexts, with per-connection allocations and native root
growth to account for. Existing 1 MiB snapshot bounds and provisional parser
charges are not a Windows worst-case heap proof. No new 512 MiB RSS claim,
allocator replacement or reduced sanitizer profile is made.

Measured artifacts, not performance estimates: the Release standalone probe is
3,032,576 bytes; its AWS-LC crypto/SSL archives are 5,287,964 / 1,233,970 bytes.
They are not a production executable size delta. Build timings/hashes are receipts;
no throughput, latency, memory-peak or Schannel performance advantage is claimed.

## Proposed implementation scope and decisions

The exact next proposal for A is limited to Windows dependency setup/link/install
verification, ProviderLease provider checks, provider-aware trust/identity
admission, and narrowly scoped native verification/store-signing adapters if the
existing semantics are retained. Reuse the driver and public P12/store identity
records. Do not change Linux/macOS, vendor curl, GC authority, worker-pool lifetime
or introduce credential persistence. An adapter needing curl changes, additional
threads or asynchronous retry support is a new scope decision, not implicitly
authorized by this report.

Boss/Golden Wrench must rule on:

1. Whether full Windows chain/distrust/online-revocation parity is required. The
   existing requirement remains in force; the unchanged AWS-LC path fails that
   equivalence gate. Authorize a native verifier design/proof or explicitly amend
   the contract before migration.
2. Whether Windows store identities must work alongside supplied P12 in one
   process. Retention needs a proven native signing adapter or a separately
   reviewed B2 design; dropping support requires an explicit capability decision.
3. Whether to authorize the single-provider implementation proposal, with its
   dependency/install and native security acceptance work, or keep the narrower
   pre-provisioned Schannel scope. No migration is recommended for immediate merge.

Server 2022 still needs full Debug/Release and supported strict acceptance through
the real source API/Runtime, actual CA replacement/default trust/revocation,
enterprise/distrust/intermediate policy, proxy auth/trust, store/hardware keys if
retained, independent owners, pooled reconnect/session reuse, cancellation/GC/
shutdown, malformed/OOM bounds and installed relocation/link closure. Strict
ASan/UBSan/leak requirements remain unchanged; this Debug/Release transport study
does not supply that certification. No hosted CI is dispatched.

## Deliverable inventory

- This `WINDOWS_TLS_PROVIDER_REVIEW.md`.
- `native-awslc/CMakeLists.txt`, `build.py`, `probe.cpp`, `peer.py`, `run.py`,
  `select.py`, `collect.py`, `README.md`.
- `evidence/windows-awslc-provider-20261103.json`.
- `docs/README.md` index and `docs/net/http/HANDOFF.md` engineering handoff.

Production source, public APIs, existing TLS policy, curl, Ubuntu/macOS, workflows,
root README and original Schannel/CNG evidence remain unchanged. Pre-existing
case-colliding JSON working-tree changes are preserved and excluded. Private
fixtures, certificates with private material, P12, keys, toolchains and build
outputs are not deliverables. **Stop for Golden Wrench review.**
