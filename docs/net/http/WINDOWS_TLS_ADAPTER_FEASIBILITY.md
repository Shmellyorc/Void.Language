# Windows AWS-LC trust and store-signing adapter feasibility

3 November 2026. Source baseline
`5bf1c514cb9c6a52d00a1ee359d5c54af762e8ec`. Native research only; no production
provider, public API, vendored dependency or platform configuration changes.

**Retain Schannel for the complete Windows security contract.** Windows chain
verification and non-exportable CNG signing both work inside the single AWS-LC
transport for the tested subset. The public SSL-context hook is insufficient for
this integration: the prototype discloses the origin's selected store identity
to a requesting HTTPS proxy, lacks authoritative peer context for IP verification,
and cannot suspend for AWS-LC verification/signing retries with pinned curl.
Blocking Windows chain retrieval also exceeds the configured curl deadline.
These are demonstrated integration limits, not reasons to weaken verification or
silently remove existing store capabilities. Production migration remains HOLD.

The [sources and reproduction](native-awslc-adapters/README.md) and
[public native receipts](evidence/windows-awslc-adapters-20261103.json) preserve
successes, failed security assertions, inconclusive controls and original fixture
failures separately. Stop for Golden Wrench review after the authorized research
commit/push. No implementation sprint is authorized by this report.

## Baseline and evidence boundaries

| Component | Actual identity |
| --- | --- |
| Repository | `Shmellyorc/Void.Language`, existing `main`; baseline above |
| Native host | Windows 10 Home 22H2 x64, `10.0.19045.6456` |
| Compiler/linker | LLVM Clang/LLD 23.1.4, revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5` |
| Build environment | Windows SDK 10.0.19041.0, MSVC headers 14.44.35207, static CRT `/MT` in Debug and Release, CMake 4.4.3/Ninja |
| TLS | Unmodified AWS-LC 5.12.0, revision `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8` |
| AWS-LC archive | SHA-256 `1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68` |
| Transport | Pinned curl 8.23.0, existing four approved cookie/H2 corrections only; nghttp2 1.70.0; one AWS-LC backend/image |
| Windows crypto | Microsoft Software Key Storage Provider; `ncrypt.dll`/`ncryptprov.dll` 10.0.19041.1 |
| Fixtures | Git OpenSSL 3.5.9; Python 3.14.8 with OpenSSL 3.5.9; prior local peer reused |

The dependency archives are reused from the [prior provider study](WINDOWS_TLS_PROVIDER_REVIEW.md).
The new receipt records their actual sizes/hashes and executable/core-source
hashes. These isolated curl builds include WinIDN/SSPI/H2 but omit production
PSL, c-ares and compression; they do not satisfy the unchanged production
`require_features()` gate. HTTP/1.1 is sufficient for these adapter controls;
settled TLS 1.3, h2, in-memory P12, MultiSSL and CNG collision research is not
repeated. TLS 1.3 works here on Windows 10 because AWS-LC implements TLS; the
earlier Windows 10 Schannel limitation remains provider-specific.

The review read [Wrench rules](../../WRENCH_CLANKER.md), the
[documentation index](../../README.md), [credential design](WINDOWS_MTLS_CREDENTIAL_DESIGN.md),
[Schannel feasibility](WINDOWS_MTLS_FEASIBILITY.md),
[hosted Schannel investigation](CI41563174932_SCHANNEL_REVIEW.md),
[native platform contract](NATIVE_PLATFORMS.md), [memory boundary](MEMORY_REVIEW.md),
the existing Runtime/provider ownership and the exact pinned TLS sources.
No full compiler build or hosted CI was required or dispatched.

Current receipts contain **82 TLS observations plus two read-only public-anchor
controls**, across Debug and Release. Of the TLS rows, 78 are valid mechanism or
limitation controls, two original proxy-origin controls are inconclusive, and
**two identity privacy assertions fail**. The 78 include expected rejections and
demonstrated unsupported operations; they are not 78 production security passes.
Earlier independent Debug verification/signing runs and their failures are
preserved separately and are not added to these totals. `complete: true` means
the experiment and owned cleanup completed, not that its security contract passed.

## Track A: Windows verification

`SSL_CTX_set_custom_verify` supplies the actual peer DER chain through
`SSL_get0_peer_certificates`. The prototype limits it to 16 certificates/1 MiB,
creates a private memory additional store, and invokes Windows chain building
with server-auth usage and chain revocation enabled. It evaluates both chain
trust bits and `CERT_CHAIN_POLICY_SSL` with `AUTHTYPE_SERVER`, the expected name
and zero ignore flags. A successful chain API call alone is not acceptance.
Only the pinned Schannel compatibility exclusion for `NOT_TIME_NESTED` is
retained; actual not-yet-valid/expired certificates are rejected.

With no explicit CA, the adapter delegates to the default Windows chain engine.
With an owned explicit CA snapshot it creates a memory store and exclusive-root
chain engine, matching the pinned Schannel explicit-CA approach. It neither
copies ROOT into AWS-LC as a policy substitute nor adds the generated CA to OS
ROOT. Windows controls chain construction/retrieval; the test uses
`CERT_CHAIN_REVOCATION_CHECK_CHAIN | CERT_CHAIN_REVOCATION_ACCUMULATIVE_TIMEOUT`
and a 1,000 ms retrieval setting. Cache-only retrieval is a separate negative
diagnostic, never an offline acceptance fallback. The Windows API documents
chain-engine and retrieval behavior in
[CertGetCertificateChain](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetcertificatechain);
the expected-name and ignore-flag fields are defined by
[HTTPSPolicyCallbackData](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/ns-wincrypt-httpspolicycallbackdata).

The standalone Windows SSL-policy call, rather than AWS-LC's default verifier,
makes the trust decision. Exact native results below are the same in Debug and
Release unless timing is stated. Rejections occur with verification enabled.

| Native control | Actual observation |
| --- | --- |
| Generated trusted explicit CA | Chain trust/policy `0`; TLS succeeds and connection is reused |
| Unrelated explicit CA; default engine with generated untrusted root | Policy `0x800B0109` (`CERT_E_UNTRUSTEDROOT`), chain `0x01000060`, curl 60 |
| Existing public system anchor, separate read-only diagnostic | Default chain trust `0`; unrelated exclusive CA produces `0x00000020` untrusted-root bit; no identity key access |
| Wrong DNS name | Chain `0`, policy `0x800B010F` (`CERT_E_CN_NO_MATCH`), curl 60 |
| Revoked server | Chain `0x4`, policy `0x80092010` (`CRYPT_E_REVOKED`), curl 60 |
| No revocation endpoint | Chain `0x40`, policy `0x80092012` (`CRYPT_E_NO_REVOCATION_CHECK`), curl 60 |
| Unavailable endpoint; fresh cache-only miss | Chain `0x01000040`, policy `0x80092013` (`CRYPT_E_REVOCATION_OFFLINE`), curl 60 |
| Expired; not yet valid | Chain `0x1`, policy `0x800B0101` (`CERT_E_EXPIRED`), curl 60 |
| Supplied intermediate | Windows builds and verifies successfully |
| Missing intermediate, reachable AIA | One peer certificate; Windows retrieves intermediate and succeeds |
| Missing intermediate, no AIA | Chain `0x01010040`, policy `0x800B0109`, curl 60 |
| HTTPS proxy plus origin | Separate successful Windows policy calls for `proxy.test` and `localhost`; both remain verified |
| Wrong proxy name; revoked proxy | Policy `0x800B010F` / `0x80092010`; curl 60 before origin use |
| Corrected wrong-name origin behind valid proxy | Proxy policy `0`, then origin policy `0x800B010F`; curl 60 |
| Valid IP SAN, ordinary AWS-LC verifier | TLS to `127.0.0.1` succeeds |
| Same valid IP SAN, custom verifier | Missing peer name `0x0000007B` (`ERROR_INVALID_NAME`); fails closed with curl 60 |
| Verification callback returns `ssl_verify_retry` | Curl 35; handshake never resumes |
| 100 ms connect/total deadline during slow CRL fetch | Windows callback 522.509 ms Debug / 512.994 ms Release; child 0.617 / 0.600 s; curl 60 rather than timeout 28 |

Fresh generated issuers and CRL URLs prevent earlier good CRLs or intermediates
from masking negatives. The revoked serial is placed in its CRL before first
validation. Full-chain/AIA/no-AIA controls use distinct intermediates. The public
anchor diagnostic establishes anchor lookup/replacement, not a positive public
TLS handshake, Windows enterprise/distrust acceptance or universal Schannel
equivalence. No generated ROOT/Disallowed entries were installed for those tests.
Enterprise CTLs, explicit distrust updates, Windows policy changes, OCSP-specific
behavior, malformed-chain exhaustion, IDN and real system-trusted TLS remain
native acceptance work. Default policy delegation is confirmed by source and
these bounded controls; all combinations of OS policy have not been certified.

The IP rejection exposes a context gap. Curl's public SSL-context callback gives
the easy handle, SSL_CTX and application data. The prototype reads locally
configured DNS SNI, which suffices for its ASCII DNS controls. SNI is absent for
IP literals and does not authorize an identity for a proxy/origin role. Reading
the URL alone cannot determine the active TLS layer through HTTPS CONNECT,
redirects and reuse. Bypassing name checking or guessing from callback order is
not an acceptable correction.

Windows retrieval is synchronous. The measured overrun demonstrates that curl's
timer does not preempt this callback. The native retrieval timeout is not a
complete AIA/root/policy/cancellation bound. Existing Schannel also makes native
chain calls; no comparative performance regression or advantage was measured.
Moving work off the driver needs a supported suspend/resume path and bounded
existing Runtime work, not a second scheduler. A cancelled request cannot destroy
the chain input/engine while a native job is still using it.

## Track B: explicit store-backed signing

The isolated identity path accepts only an explicit
`CurrentUser\store\SHA1-thumbprint` selector. It opens the existing store read-only,
finds that exact certificate, and requests its CNG handle with
`CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG | CRYPT_ACQUIRE_COMPARE_KEY_FLAG |
CRYPT_ACQUIRE_SILENT_FLAG`. It independently compares exported **public** SPKI
against the selected certificate before TLS. No default certificate lookup,
private-key import/export, P12 persistence or backend fallback is used.

AWS-LC's `SSL_CTX_set_private_key_method` delegates TLS signatures to
`NCryptSignHash`. AWS-LC supplies the message and TLS signature scheme; the
adapter hashes once with its specified SHA-2 digest. RSA-PSS sets the digest
identifier and salt length equal to the digest length. RSA PKCS#1 uses its
distinct padding mode. CNG ECDSA returns fixed-width `r || s`; the adapter encodes
the ASN.1 DER ECDSA signature expected by TLS. Private keys remain in the KSP.
Pinned interfaces and retry contracts are in
[AWS-LC's SSL header](https://github.com/aws/aws-lc/blob/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8/include/openssl/ssl.h#L1419).

| Native control | Debug and Release observation |
| --- | --- |
| Non-exportable software RSA-2048 / ECDSA-P256, independent signing track | Both TLS 1.2 and 1.3 authenticate; exact complete client DER observed by peer |
| Combined Windows verification plus CNG signing | Both algorithms and both TLS versions authenticate; native trust/SSL policy `0` |
| Private export controls | Export policy `0`; raw-private and PKCS#8 size queries both reject with `0x80090029` (`NTE_NOT_SUPPORTED`); no private output buffer |
| RSA-PSS SHA-256/384/512 | Successful schemes `0x0804`, `0x0805`, `0x0806`; PSS salt length equals digest length |
| TLS 1.2 RSA PKCS#1 SHA-256 | Successful forced scheme `0x0401` |
| ECDSA-P256 SHA-256 | Successful scheme `0x0403` after raw-to-DER conversion |
| Missing selector/certificate | Anonymous required-mTLS rejected; missing thumbprint `0x80092004`; missing store `0x2` |
| Wrong selected identity | TLS peer observes a different complete DER and rejects before HTTP; request fails |
| Certificate associated with wrong key | `CryptAcquireCertificatePrivateKey` rejects `0x80090015` (`NTE_BAD_PUBLIC_KEY`) before TLS |
| Two independent clients selecting the same explicit fixture | Distinct acquired handles, concurrent first requests; closing A leaves B functional; B reuses its own connection |
| Ordinary successful request reuse | Two requests: new connection counts `1`, then `0` |
| Signing callback returns `ssl_private_key_retry` | One sign call, zero completion calls, curl 35; handshake never resumes |
| Borrowed credential invariants | Public key, export policy, usage, DACL hash and certificate DER match before/after use; owned handle/store/context releases recorded |

The native keys are operator-provisioned disposable fixtures in isolated stores,
not caller production credentials. Their CSR is signed through the CNG handle;
private key material is never extracted to build the certificate. Only the
fixture controller creates/removes those test objects. The adapter acquires
signing authority without provisioning or changing credential properties.
These software-key snapshots do not audit private key-file bytes, every provider
usage counter or every third-party provider side effect.

The current public API validates eight Windows store locations and case-insensitive
hex thumbprints ([Runtime validation](../../../src/runtime/http.cpp)). This
prototype tests CurrentUser and uppercase selectors only. Legacy CAPI, all other
store locations, ECDSA curves beyond P256, RSA-PSS-key OIDs, intermediate client
chain selection, smartcards/HSMs, PIN/UI, key disappearance and provider failures
remain unproved. Concurrent clients with different selected identities also need
native isolation acceptance. Restricting production to this subset would reduce
the existing contract and requires an explicit Boss decision; it is not an
implementation detail.

For CAPI, `CryptSignHash` is a distinct provider path with key-spec, digest and
RSA signature byte-order requirements; it does not establish generic RSA-PSS
support. `NCryptTranslateHandle` is not a universal bridge: a corresponding KSP
name/alias must be registered for the CSP.
[Microsoft's translation contract](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncrypttranslatehandle)
documents that restriction. No legacy key was silently exported or converted.
The synchronous software CNG signatures are feasible; hardware/UI calls may
block or fail under SILENT and need their own authorization, lifetime and
cancellation acceptance. No interactive or hardware credential was accessed.

## Demonstrated privacy failure and exact curl gaps

In both profiles, an HTTPS proxy requested an optional client certificate while
the caller configured only an origin store identity. Both server chains passed
Windows policy. The proxy received **the exact origin client certificate DER**,
and the adapter signed on that proxy TLS connection before signing for the
origin. The scope assertion expected no proxy client identity and recorded
`passed: false`. Verification success does not authorize credential disclosure.
This rejects the current callback installation design. No production user
credential was involved, and no variation of that design is proposed as a fix.

The exact source explains the result:

| Integration point | Evidence and required correction |
| --- | --- |
| `Curl_ossl_ctx_init`, `openssl.c:3967–3989` | Calls the same public `ssl_fsslctx` hook for proxy/origin before `ossl_init_ssl`. The prototype installs the origin certificate/key method on both contexts. A reviewed peer-role/name metadata bridge is needed before selecting identity or verification policy. |
| `ossl_init_ssl` / `ssl_peer` | Curl already knows the authoritative connection peer and filter role internally. Expose a supported, bounded per-TLS-connection context for expected DNS/IP name, proxy/origin authorization, owned trust/identity policy and lifetime; do not depend on private ABI layouts. |
| `ossl_connect_step2`, `openssl.c:4191–4215` | Handles read/write and conditional OpenSSL async/retry names, but not AWS-LC `SSL_ERROR_WANT_PRIVATE_KEY_OPERATION` (13) or `SSL_ERROR_WANT_CERTIFICATE_VERIFY` (16). Native retries both terminate with curl 35. |
| Retry completion and wakeup | Recognizing the numbers alone is insufficient. A completed native job must wake the existing driver and resume the correct live TLS operation without waiting for unrelated socket input; cancellation must suppress publication and safely drain work. |
| TLS cache/session provenance | Hook-installed identity is absent from normal curl SSLCERT configuration matching. Any implementation must bind identity/trust policy and peer role to pooling/session provenance and test reconnects, resumption and redirects. |

See the exact retained [curl OpenSSL backend](../../../third_party/curl/lib/vtls/openssl.c)
and [AWS-LC verification API](https://github.com/aws/aws-lc/blob/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8/include/openssl/ssl.h#L2972).
No vendored curl change was made. Hook call count, SNI equality or port comparison
cannot reliably identify the TLS layer: proxy and origin can share names,
contexts can be cached, and retries/redirects change sequencing. Curl error 35's
generic invalid-library text in the retry controls is preserved, not replaced
with an invented native success result.

## Runtime ownership, errors and dependencies

The existing [process-wide ProviderLease](../../../src/runtime/http_provider.cpp)
can continue to own one libcurl image and one AWS-LC backend. Its Windows feature
gate currently requires Schannel and would need an explicitly reviewed future
change, together with the full native dependency/install closure. Backend
selection is global; per-client or per-Runtime fallback does not solve this.
Retaining Schannel means retaining the Windows production provider, not silently
switching a request between two TLS implementations.

The existing [HTTP driver and ClientState](../../../src/runtime/http.cpp) own the
CURLM, immutable TLS inputs, pools and transfer lifetimes. Credential ownership
can fit this structure, but callback state must follow pooled TLS connection
lifetime, not a request's raw `TransferState*`. The isolated easy handles use
curl's internal multi/cache; the independent-owner control is not a full VOID
Runtime/GC execution proof. Shared multi access is not made concurrently here.

A future owner must retain the selected certificate, store, chain engine and key
for every live TLS connection and entered native job. Respect the
`pfCallerFreeProvOrNCryptKey` result; use `NCryptFreeObject` for owned CNG handles,
`CryptReleaseContext` for any deliberately supported CAPI path, then release
certificate/store references. Never delete a borrowed key. Native tests return
caller-free CNG handles. Cleanup destroys easy/cached SSL objects before their
credential owner and calls global cleanup last. Avoid a strong
ClientState → CURLM → SSL context → ClientState cycle; a separate credential/TLS
state owner should retain only the authority needed by active objects/jobs.

If async work is approved, reuse bounded existing Runtime workers, wakeup and
generation/provider leases. Cancel publication, retain entered operation inputs,
drain completion, retire TLS contexts and credentials, then release the last
provider lease. Client close, cancellation and shutdown must not free a key or
chain engine underneath a native call. Single-worker progress, a slow native
verification beside an unrelated fast client, held cancellation, GC and shutdown
must be demonstrated through the real Runtime. None is claimed by these probes.
No new Runtime, GC authority, scheduling framework or detached credential worker
is needed or authorized.

The prototype preserves precise Windows DWORDs in public diagnostic receipts;
curl normally exposes verification failure 60 or handshake failure 35. Existing
`NativeDomain` has no Windows-crypto domain ([public record](../../../include/void/http.hpp)).
A future implementation must preserve existing error kind/phase mapping and
retain sanitized native diagnostics per connection as appropriate. It must not
silently change the public error API, leak key identifiers/private bytes, report
a verifier rejection as timeout success or share error state between clients.

Adapter dependencies are supported Windows Crypt32/Ncrypt/Bcrypt APIs plus
Advapi32 for isolated fixture ACLs; system socket/SSPI/WinIDN dependencies are
already in the native link closure. AWS-LC does not receive a Windows private-key
blob. No new third-party crypto provider or credential persistence is proposed.

## Cleanup, original failures and cost limits

Test-only key/store names use random 256-bit nonces, restricted current-user and
SYSTEM DACLs, persisted owner markers, exact public-key checks and exact one-DER
store checks before cleanup. Setup is serial and operator-controlled. This
does not establish atomic CNG reservation, adversarial collision safety or crash
recovery and does not revive either rejected persistence design.

All final experiment inventories match before/after: one pre-existing software
KSP key and eight CurrentUser stores. The operator removed only its generated
identities. The adapter never deletes credentials. No machine store or existing
user identity was modified; no root/distrust-store writes were used. Temporary
CA/server private files were ACL-restricted and removed. `SSLKEYLOGFILE` was unset.

Windows AIA retrieval cached one generated public intermediate in CurrentUser
CA during the first independent trust study. The original inventory assertion
failed and remains recorded. After checking it was absent before the study,
cleanup removed only the exact DER/SHA-256 certificate
`F5DC3B92E456A15DF832C50B4E1FE23F9F22D8C602160A720D9219A5B9B31BCF`,
with no private-key association. A first cleanup command used a wrong fixture
path and failed `input bound` before store access; the corrected-path successful
receipt is preserved. Later runs perform the same narrow cleanup automatically.
OS-managed public CryptNet URL caches are not globally purged; unchanged
credential inventories are not a forensic claim of zero cache activity.

Other originals remain visible: OpenSSL rejected the initial `-days -1` expired
fixture before any native cases; explicit past validity fixed the setup. The
first signing setup supplied lowercase nonce hex to an uppercase-only parser
and failed before key creation. Finally, the inherited CONNECT fixture rejected
`wrong.test` before origin TLS (curl 56); those two final rows are inconclusive,
with separate corrected wrong-SAN controls proving the intended policy decision.
No failed observation was reclassified as a successful security gate.

Debug/Release native builds pass. `verify_curl_source.py` verifies all 1,586
retained upstream identities plus exactly four approved corrections. No Windows
strict ASan/UBSan/leak certification or full compiler test result is supplied.
The new sources bound CA/peer inputs at 1 MiB, explicit CA count at 128 with
64 KiB individual certificates, public properties at 64 KiB and signatures at
16 KiB. Windows chain/network/cache allocations remain OS-dependent and unproven
under the [provisional provider memory accounting](MEMORY_REVIEW.md).
No worst-case heap/RSS ceiling or throughput benefit is claimed. Native blocking,
hardware latency, malformed input/OOM, repeated lifecycle load, full link/install
relocation and Windows Server 2022 acceptance remain open.

## Production recommendation and remaining decisions

**Keep the existing Schannel Windows provider for verification and explicit store
identities.** Preserve its current Windows 10 TLS 1.3 and in-memory P12 limitations
as documented; do not disguise an adapter capability reduction as their repair.
The mechanisms are practical for silent software CNG/DNS peers, but a complete
replacement now entails reviewed curl peer-context and retry integration plus
Runtime lifecycle and broader store/provider acceptance. A public-hook-only
migration does not satisfy the mission's security contract.

Golden Wrench/Boss should decide whether that additional integration scope is
worth pursuing. Retaining the current contract requires no relaxation decision.
Dropping legacy/hardware/store locations, native distrust/revocation or proxy
privacy would be an explicit contract change, not the recommended path.

If Boss authorizes a new implementation proposal after review, its smallest
complete scope is:

1. Review a narrowly defined curl peer-context and AWS-LC suspend/resume bridge,
   including wakeup, cancellation and pooling/session provenance. Require native
   IP/DNS and requesting-proxy privacy gates before adopting its design.
2. Implement Windows trust and explicit store signing behind that bridge, using
   existing ClientState/TLS ownership and bounded Runtime workers where needed;
   define the retained CAPI/hardware/UI behavior without exporting keys.
3. Complete single-provider Windows dependency/feature/install checks and real
   Runtime Debug/Release/strict acceptance: all verification negatives, independent
   origin/proxy policy and identity, all retained store capabilities, resumption,
   redirects, cancellation/GC/shutdown, failure/memory bounds and Server 2022.

That is a conditional proposal requiring scope approval, not an immediate GO.
If the bridge/provider obligations are disproportionate, retain Schannel; do not
add another libcurl, per-request backend switches, a second HTTP implementation
or persistence fallback. Linux/macOS and the approved public API remain intact.

## Exact deliverable inventory

Seventeen feasibility-only paths:

- `docs/net/http/WINDOWS_TLS_ADAPTER_FEASIBILITY.md` (this report).
- `docs/net/http/evidence/windows-awslc-adapters-20261103.json`.
- `docs/net/http/native-awslc-adapters/CMakeLists.txt`.
- `docs/net/http/native-awslc-adapters/common.hpp`.
- `docs/net/http/native-awslc-adapters/trust.hpp`.
- `docs/net/http/native-awslc-adapters/signing.hpp`.
- `docs/net/http/native-awslc-adapters/fixture.cpp`.
- `docs/net/http/native-awslc-adapters/probe.cpp`.
- `docs/net/http/native-awslc-adapters/roots.cpp`.
- `docs/net/http/native-awslc-adapters/build.py`.
- `docs/net/http/native-awslc-adapters/study.py`.
- `docs/net/http/native-awslc-adapters/roots.py`.
- `docs/net/http/native-awslc-adapters/collect.py`.
- `docs/net/http/native-awslc-adapters/README.md`.
- `docs/net/http/WINDOWS_TLS_PROVIDER_REVIEW.md` (subsequent evidence link).
- `docs/net/http/HANDOFF.md` (engineering handoff).
- `docs/README.md` (index).

Production source, curl/dependencies, workflows, root README and prior evidence
are unchanged. The two pre-existing case-colliding JSON modifications under
`docs/json/evidence/implementation/readonly-{Debug,Release}.json` are preserved
and excluded. No private fixtures, credentials, binary/toolchain outputs or
temporary paths are committed. **Stop for Golden Wrench review.**
