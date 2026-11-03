# Windows Schannel P12 — security architecture review

3 November 2026 (America/Vancouver). Baseline
`c9a1f47c16b8fe2410cf33dd97e33dfff72edcc8`; LLVM/Clang/LLD 23.1.4.
**Investigation complete; mTLS repair remains blocked on a consequential credential
policy decision. No production, fixture, vendored source or security policy change
is implemented in this handoff. Windows mTLS acceptance remains failed.**

## Subsequent architecture correction — 3 November 2026

The [source-grounded credential design](WINDOWS_MTLS_CREDENTIAL_DESIGN.md)
corrects this review's store-API statement: `client_identity.windows_store` is
already implemented end-to-end. It also identifies the explicit temporary
exportability needed for controlled named-key reimport and the existing cleanup
wrappers that currently discard errors. Historical investigation and receipts
below remain unchanged; the design is not implementation approval.

## Actual hosted evidence

[CI 41563174932](https://github.com/Shmellyorc/Void.Language/actions/runs/41563174932)
passed all five Ubuntu/macOS jobs. Windows Release job 125678330370 and Debug
job 125678330377 each passed 340/341 CTests; only `http_native` failed. The path
identity correction now passes natively. Both runners report Windows Server
2022, 10.0.20348.

Complete Windows logs and all four diagnostic/test ZIPs were downloaded and
inspected. Artifact SHA-256 matched GitHub metadata for Debug 13202226262 /
13201792497 and Release 13198789399 / 13198649628. Unmodified ZIPs and full logs
remain in `.toolchains/ci41563174932/`. Compact public receipts, original native
streams, API metadata and hashes are preserved in
[evidence/ci41563174932](evidence/ci41563174932/). No private key or P12 was
retained in the original artifacts or added here.

Both native stderr streams report, after successful earlier TLS/authentication:

```text
curl provider error 58: schannel: Failed to import cert file (memory blob), password is bad
passed: bad-password client close
curl provider error 35: schannel: next InitializeSecurityContext failed: SEC_E_INTERNAL_ERROR (0x80090304) - The Local Security Authority cannot be contacted
explicit mutual TLS identity: kind=12 code=35
```

The certificate tool successfully exported and parsed P12. Wrong-root verification
produced error 60 (`SEC_E_UNTRUSTED_ROOT`). Anonymous mTLS produced an unsuccessful
request and peer `PEER_DID_NOT_RETURN_A_CERTIFICATE`. Each peer receipt contains
that event followed by two `UNEXPECTED_EOF_WHILE_READING` events, consistent with
the sequential bad-password and positive-identity attempts. There is **no**
`authenticated` event. Peer verification was `CERT_REQUIRED`, Python server SSL
OpenSSL 3.0.16. Different freshly generated public certificate fingerprints in
Debug and Release have the same failure. This is not successful ordinary HTTPS
being counted as mTLS.

## Exact failure boundary and root-cause confidence

The pinned code trace is:

1. `src/runtime/http.cpp:726`: owned certificate snapshot becomes
   `CURLOPT_SSLCERT_BLOB`, with P12 type and explicit password. Windows PEM is
   rejected separately. Source storage remains client-owned through transfer
   cleanup; there is no evidence of borrowed-file or byte-lifetime failure.
2. `third_party/curl/lib/vtls/schannel.c:504`: `get_client_cert` calls
   `PFXImportCertStore(..., PKCS12_NO_PERSIST_KEY)`, clears its temporary password,
   finds a certificate with a private key, and keeps the returned memory store.
   Import/certificate-selection failures return error 58, unlike the observed 35.
3. `schannel.c:603`: `acquire_sspi_handle` uses `SCH_CREDENTIALS` on build 20348
   without a custom legacy cipher list. The explicit certificate is supplied to
   `AcquireCredentialsHandle`. The observed **next** handshake error proves this
   acquisition and the initial handshake stage were passed; acquisition did not
   produce the captured failure.
4. `schannel.c:1098`: the error comes from the subsequent SSPI context processing,
   through `schannel_error`, before a complete authenticated handshake. It maps
   the native internal error to curl 35. Trust and hostname flags remain enabled;
   automatic default client credentials remain disabled.
5. `schannel.c:835` retains the client store on the reference-counted credential.
   Final `schannel_session_free` releases SSPI credentials, then closes the store.
   Keeping the store alive addresses local lifetime, not cross-process access.

[Microsoft's PFX import contract](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-pfximportcertstore)
explains that non-persisted keys are associated through `CERT_KEY_CONTEXT_PROP_ID`;
this context does not marshal to external LSASS handshakes. Persistent import
instead supplies provider information. Omitting the flag writes keys to disk;
closing the temporary certificate store alone is not key deletion.
[Microsoft's SSPI documentation](https://learn.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--schannel)
distinguishes credential acquisition from successive context-establishment calls;
acquisition success does not prove the later handshake can use a private key.

[Upstream curl issue 17626](https://github.com/curl/curl/issues/17626) reports the
same Server 2022 error at the same stage. Its experiment removing the flag
succeeded but left private-key files. The issue was closed as a **known bug**, not
as a merged repair. The cited [.NET implementation](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Net.Security/src/System/Net/Security/SslStreamPal.Windows.cs)
also explicitly diagnoses unsupported ephemeral Windows credentials. These are
strong corroboration, not substitute native testing of a VOID correction.

**Evidence-supported explanation:** the non-persisted Schannel credential cannot
be used by this Windows handshake path; the cross-process key-access limitation
is the leading, strongly corroborated mechanism. The precise failing SSPI stage
and status are now confirmed. The artifacts do not include LSASS traces, runtime
key-provider/property inspection or a controlled alternate-import run, so they
do not independently prove the internal Windows call chain. They also do not
record a negotiated TLS version for the failed handshake. Curl enables TLS 1.3
on this OS; describing the failed connection as confirmed TLS 1.3 would overstate
the receipts. Do not infer a specific CSP/KSP merely from the P12 algorithm.

## Supported strategies and consequences

| Strategy | Ownership, cleanup and failures | Assessment |
| --- | --- | --- |
| Keep current non-persisted P12 | Key stays associated with the owned certificate store; current failure remains explicit | Preserves current security contract; no established fix for this Windows path |
| Force CNG while retaining non-persistence | Changes provider selection, not the documented cross-process property limitation; provider availability may vary | No native evidence this repairs the failure; not a supported universal workaround |
| Delete `PKCS12_NO_PERSIST_KEY` | Introduces disk keys without an owner or deletion contract; store closure does not erase them | Reject as a production correction |
| Explicitly opted-in, managed user-scoped persistent P12 identity | Application owns named keys until all connections/credential references drain; needs rollback, deletion errors and crash-residue handling | Narrowest candidate that retains Schannel and P12, but changes security policy and requires approval plus native proof |
| Explicit pre-provisioned Windows store identity | Caller/admin owns provisioned key lifecycle; VOID borrows a specifically selected identity and must not delete it | Supported curl mechanism; useful separate capability, **not** fulfillment of existing file/P12 acceptance |
| In-process Windows TLS provider (e.g. AWS-LC) | Could retain byte-owned P12 keys without Schannel key marshaling; requires Windows provider/build/trust/revocation review | Broader architecture change; importing OS roots alone does not establish identical native trust policy |
| Force TLS 1.2/legacy credentials or silently use system credentials | Alters negotiation or identity semantics; does not establish general supported P12 behavior | Not an acceptable test fix or silent fallback |

Curl documents explicit store paths in
[CURLOPT_SSLCERT](https://curl.se/libcurl/c/CURLOPT_SSLCERT.html), and pinned
`get_client_cert` implements that branch. VOID currently interprets certificate
input as owned file material; passing a store path there is not an existing
approved source API. Neither global MY-store import nor implicit auto-selection
is proposed. A provider change cannot be hidden behind a retry.

The fork [873bca690dd3832f25becc5e3d6b5efd0d2e8771](https://github.com/kevinAlbs/curl/commit/873bca690dd3832f25becc5e3d6b5efd0d2e8771)
was inspected, not applied. It switches import flags to zero and adds deferred
CAPI/CNG key deletion. Its helper explicitly ignores cleanup errors and skips
cleanup on allocation failure; it does not establish crash recovery or VOID's
ownership requirements. It is not one of the four approved vendored corrections.
Its existence is not evidence that an unchanged non-persisting fix is available.

## Concrete recommendation for Golden Wrench/Boss

**If Schannel plus P12 must be retained, approve a design for explicit opt-in
managed Windows key persistence, with default non-persistence unchanged.** This
is authorization to develop/review that design, not a claim that a tested patch
already exists. There is no safe one-line correction to approve here. If no disk
persistence is acceptable even with opt-in, reject this route and commission an
explicit Windows in-process TLS-provider/trust design instead; the current
Schannel path cannot be declared accepted from existing evidence.

The persistence design would belong at VOID's explicit credential-authority
boundary, not as an unconditional vendored curl import change. Reviewable rules:

- A distinct, explicit capability selects user-scoped OS-managed persistence;
  existing P12 requests never auto-upgrade after error 35. No machine-wide key,
  default identity, certificate trust-root change, raw key file or exportable-key
  fallback. Validate the password before any durable publication.
- A native owner tracks every key/container created by this operation, not just
  the selected leaf certificate. Use isolated unique names and narrowly scoped
  permissions; never overwrite or delete pre-existing/caller-owned containers.
  Imported metadata must not choose arbitrary providers/containers to mutate.
- Prepare ownership/rollback metadata before persistent creation. Fail closed
  on partial import, store registration or credential acquisition failure; prove
  rollback covers every created key. A scratch journal may contain identifiers
  only, never key bytes or passwords, and requires its own access control.
- Keep the owner alive until transfers, pooled connections and cached SSPI
  credentials are actually released. Request completion alone is insufficient.
  A store-selector integration must use a private identity store and deterministic
  certificate selection; curl reuse must never cross credential owners.
- Use provider APIs for deletion, not filesystem guesses. CNG
  [NCryptDeleteKey](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptdeletekey)
  returns status and frees a successful deletion's handle; legacy CAPI uses
  [CryptAcquireContext with CRYPT_DELETEKEYSET](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-cryptacquirecontextw).
  Avoid allocations on the critical cleanup path. Cleanup failure must be
  observable through an approved close/shutdown result and retained recovery
  record; do not silently report successful erasure.
- Process termination, power loss, OS/provider failure and backups mean
  temporary persistence is **not** equivalent to never writing a key. A bounded,
  same-owner recovery design and this residual risk need explicit acceptance.
  Do not promise destructor cleanup after a crash or forensic secure erasure.

This is a proposed security contract, not an implementation recipe proven safe on
Windows. The exact private-store/owner integration and any narrowly required
curl lifetime hook need source review before being applied. An opaque automatic
curl patch cannot decide VOID's consent, recovery or error-reporting policy.

## Verification and native acceptance boundary

User scope clarification: any future repair must stay Windows-only. Linux/macOS
source, providers, dependency pins, fixtures and workflow behavior remain unchanged;
the five successful hosted configurations are not being reopened.

Focused local results: **6/6 pass, no skips, in each of Debug (177.91 s), Release
(78.61 s), and strict (177.07 s)**. Source verification passes all 1,586 vendored
entries and four approved corrections; `git diff --check` passes. Results are recorded in `evidence/ci41563174932/local-results.json`
and the profile JUnit receipts. Existing LLVM 23.1.4 builds were exercised at a
1 MiB stack, with strict ASan leak detection/halt and UBSan halt/stack traces.
The selection is `http_native`, `http_timeouts`, `http_dns`, `http_examples`,
`http_ci_policy`, `http_curl_objects`. The native fixture retains genuine client
fingerprint verification, anonymous rejection, incorrect-password rejection,
wrong-root/hostname checks and adjacent authentication/proxy/streaming behavior.
Source/GC/Runtime code and all tests are unchanged. Local Linux AWS-LC success
cannot verify Windows key storage, SSPI or the proposed strategy.

Before implementing a policy-changing correction, obtain the architecture
ruling above. A subsequent approved native experiment must inspect key context
versus provider properties without private material, prove the exact expected
client certificate at the peer, and test both supported TLS versions explicitly.
It must cover wrong password/root/hostname, absent/wrong client identity,
concurrent owners, reconnect/session reuse, cancellation/Runtime shutdown,
partial-import/allocation failure, deletion denial and crash/recovery. Compare
owned container inventories before/after without dumping secrets; separately
prove pre-existing keys remain untouched. Full native Debug/Release and all
existing platform acceptance remain required. No native Windows execution was
available locally and no hosted dispatch was performed.

Changed files are documentation and compact evidence only; exact inventory is
[evidence/ci41563174932/changed-files.json](evidence/ci41563174932/changed-files.json).
No source/test/dependency/security relaxation, staging, commit, push, branch,
CI dispatch or root README modification. **Stop for Golden Wrench review.**
