# Windows Schannel credential feasibility

3 November 2026. **Source-supported investigation completed; native feasibility
NOT ESTABLISHED. Do not authorize production integration from this report.**
The architecture remains a plausible Windows-only route, with the corrections
below. No credential was created, exported, persisted or deleted in this sprint.
No Schannel handshake experiment was executed.

Boss conditionally approved temporary in-memory exportability and possible crash
residue for isolated feasibility work. Those decisions are recorded, not requested
again. They do not authorize production persistence. Linux/macOS, public API,
production code, existing tests, workflows and vendored curl are unchanged.

## Environment and work performed

Repository baseline: `c9a1f47c16b8fe2410cf33dd97e33dfff72edcc8`, with prior
uncommitted review documents retained. Local execution is Linux x86_64,
kernel `7.2.11-pikaos`; installed Clang/LLD pin is 23.1.4. VirtualBox reports no
running VM. The only registered guest is a powered-off Windows 11 VM, not an
available disposable Server 2022 test session. It was neither booted nor modified.
No native Windows compiler/SDK execution session was available; Wine/Linux were
not used to claim Schannel acceptance. No hosted run was dispatched or requested.

Read the complete [credential design](WINDOWS_MTLS_CREDENTIAL_DESIGN.md), the
[latest investigation](CI41563174932_SCHANNEL_REVIEW.md), TLS/pooling contract,
native platform and memory limits, and actual source at these boundaries:

- `stdlib/net/http.void` identities and `include/void/http.hpp` native options;
- `src/runtime/http.cpp` selector configuration, per-client multi/share ownership,
  admission and final cleanup; `http_resource.cpp` and `http_abi.cpp` result paths;
- pinned `third_party/curl/lib/vtls/schannel.c` PFX import, store selection,
  credential acquisition and reference-counted SSPI cleanup.

Read-only checks: curl provenance verifies **1,586 entries and four approved
corrections**; original Windows artifact identities/receipts remain intact;
document links and `git diff --check` pass. Evidence is in
[evidence/windows-mtls-feasibility.json](evidence/windows-mtls-feasibility.json).
No crypto experiment is described as a pass. Existing failed Windows mTLS evidence
is preserved, not replaced by another ordinary HTTPS test or by Linux results.

## What contracts support, and what they do not prove

| Transition | Source-supported conclusion | Native proof still required |
| --- | --- | --- |
| Ephemeral P12 decode | `PFXImportCertStore` supports NO_PERSIST and temporary exportability; use only test-generated input | Actual provider, key count, password rejection before any durable object, absence of durable staging keys |
| Acquire/export | CNG acquisition/export APIs exist; honor returned key-spec and caller-free flag; compare public key to certificate | Actual RSA/ECDSA PKCS#8 export succeeds under allowed temporary policy; buffer limits, zeroing and no artifacts containing secrets |
| Named reimport | Software KSP documents PKCS#8 name buffer, deferred finalization, user scope and no-overwrite `NTE_EXISTS` | Exact status at import/finalize/property operations; when each object becomes reopenable/durable |
| Non-exportable final key | Export policy zero excludes export flags | Reopen key, perform a valid signing operation, then reject both plaintext and encrypted private export; invalid-handle failure is not evidence |
| Exact private store | curl opens the explicit existing store and exact thumbprint; no need to change curl demonstrated | Store registration/ACL/association persists correctly; exact DER at peer with TLS 1.2 and TLS 1.3 |
| Lifetime/deletion | Client CURLM and curl SSPI reference release supply an apparent cleanup boundary | Real pooled/reused connections retain access until final release; exact key/store absence afterward |

[NCryptImportKey](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptimportkey)
documents named Software KSP PKCS#8 import and deferred finalization, with collision
failure when overwrite is absent. This supports the selected mechanism, not a
transactional import guarantee. [Key storage properties](https://learn.microsoft.com/en-us/windows/win32/seccng/key-storage-property-identifiers)
distinguish export permission from plaintext export permission. Do not assume
`CRYPT_EXPORTABLE` alone establishes the exact temporary PKCS#8 export operation;
inspect actual policy and status in the native probe, without broadening the final
key's policy or adding an unreviewed fallback.

[CryptAcquireCertificatePrivateKey](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-cryptacquirecertificateprivatekey)
returns both the key type and whether the caller must release the handle. A cached
borrowed handle must not be independently freed. Use CNG-only acquisition and
verify Software KSP; preserve the certificate/store while a borrowed key is used.
A certificate/private-key match must be checked explicitly, not inferred from
successful handle acquisition or certificate enumeration order.

## Required design corrections

### 1. A store property is not a durable owner marker

[CertSetStoreProperty](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certsetstoreproperty)
explicitly documents that store properties are not persisted. A custom property
can therefore disappear on reopen/restart. The original design's unspecified
store marker cannot be implemented as a store property and then treated as crash
ownership evidence. This is a concrete documentation finding, not a failed native
experiment.

Minimum candidate correction: use a versioned public ownership record on the
**exact newly created private store's registry key**, with name/nonce, SID,
application scope and DER digest also bound to the journal. CurrentUser registry
location is documented in [System Store Locations](https://learn.microsoft.com/en-us/windows/win32/seccrypto/system-store-locations).
Use checked registry value/flush and
[registry security APIs](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetkeysecurity)
for that private key only. This is public metadata, not a raw private-key path.
Do not modify the parent store namespace, MY/ROOT/CA or any unrelated ACL/value.
Inspect inherited restrictions before creation, then validate the private store's
owner/DACL before adding credentials or exposing a selector; fail closed if the
required isolation cannot be established. Native tests must prove how this
metadata interacts with CertRegister/Unregister and actual store layout.

Registration followed by marker write is not atomic. A crash before the durable
marker leaves ambiguous ownership: quarantine and report. Journal intent plus a
matching random name alone never permits deletion. Live rollback may use its
successful create-new result, but that transient fact cannot be invented after
restart. Do not make journal state claim marker durability before flush/reopen
verification.

### 2. Separate the stable liveness lock from mutable journal records

[CreateFile sharing](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
and [ReplaceFile access requirements](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
make the original wording insufficient: an exclusively opened record cannot also
be replaced as though the lock transparently follows the new record. Use a
per-owner **stable lock sidecar** that is never replaced while alive, separate
from the bounded replaceable journal. Recovery must acquire the same lock before
reading/checking/deleting anything and hold it through completion. Deny deletion
sharing on that lock. No PID/time heuristic and no unlink/recreate while ownership
is active. Record replacement/flush failure aborts creation; do not assume
ReplaceFile provides a power-loss transaction (its write-through flag is unsupported).
The existing 64-owner cap must include sidecars and interrupted record files.

### 3. Treat key-marker durability as a native gate, not an atomic property

[NCryptSetProperty](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptsetproperty)
restricts `NCRYPT_PERSIST_FLAG` to persisted key handles.
[NCryptFinalizeKey](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptfinalizekey)
does not promise that a custom marker and all key state commit atomically.
Probe the marker/property calls before and after finalization with exact return
codes, reopen checks and child termination at each boundary. If only post-finalize
marker persistence works, retain the unmarked-key quarantine interval. Never
infer ownership from provider name, prefix, certificate match or a prepared journal
alone. Quarantine is already within the conditionally approved residual-risk
policy; no new owner decision is requested here.

### 4. Make store removal and all cleanup failures explicit

[CertUnregisterSystemStore](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certunregistersystemstore)
requires `CERT_STORE_DELETE_FLAG` to request deletion after unregistering.
Closing handles or unregistering without checking removal is not complete cleanup.
Keep key deletion separate (`NCryptDeleteKey`), check every status, and retain the
journal until each owned object is proven absent. A mismatched marker, unexpected
certificate/entry or access denial must preserve evidence and report cleanup
failure; never remove an entire store on its name alone. Failed deletion does not
justify weakening ACLs automatically. Fault tests may restore only their own
synthetic object's test ACL before an explicit cleanup attempt.

The existing VOID wrappers discard cleanup results and would need the previously
specified error propagation during a future approved implementation. No such
production change was made in this assignment.

## Exact remaining native experiment set

All entries below are **NOT EXECUTED**. Run in an isolated disposable Server 2022
user profile, using generated credentials only, pinned LLVM 23.1.4 native SDK,
MSVC static CRT and pinned curl 8.23.0. Do not put this in production initialization
or modify CI to run it automatically. Bound runs to two concurrent owners, two
normal create/cleanup cycles, and one probe per fault boundary per build profile.
A synthetic sentinel key/store is created by the harness solely to prove it is
untouched by the experiment and is cleaned separately by that sentinel's owner.

| Probe | Receipt required; rejection/control |
| --- | --- |
| P12 staging | API status, provider/key-spec, public digest, caller-free ownership; bad password and ambiguous/mismatched cert/key create no persistent key/store |
| Provider/algorithm | Software KSP RSA and ECDSA positives; reject unsupported provider/algorithm without fallback or publication |
| Export/reimport | Temporary policy and bounded export status only; new name and user scope, key public digest and reopened final export policy; no secret bytes/passwords in receipt |
| Collision | CREATE_NEW store and no-overwrite key statuses; pre-existing synthetic objects' public properties unchanged |
| Marker/finalize | Reopen after each property/finalize step; fail import/finalize/marker calls individually; terminate child before/after each; unmarked object quarantined |
| Store/ACL | Registration, exact registry marker, current SID/restricted DACL, cert association, reopened private selector; wrong SID/marker and registration failure never publish |
| Schannel | TLS 1.2 and TLS 1.3 separately; exact peer DER/SHA-256 with verified explicit CA and hostname; missing/wrong identity, wrong root/hostname fail |
| Reuse/concurrency | Two independent names for identical P12; concurrent and reused connections authenticate; closing one leaves the other usable |
| Cleanup | Close curl sessions/pool, delete only owned store and key; reopen proves absence; two repetitions leave zero experiment-owned objects |
| Denied deletion | Deliberately deny on one owned synthetic object; observable error and retained journal; no unrelated modification |
| Recovery/liveness | Stable live lock blocks another process; child termination releases it; marked orphan recovered, missing/incorrect marker quarantined; corrupt record never broadens search |
| Rollback | Fault each creation/publication edge; owned objects removed or explicit unresolved receipt; primary and cleanup errors preserved separately |

Record JSON with OS/build, compiler/provider/curl identities, operation, expected
and actual status, public hashes, policy flags and before/after owned-object counts.
Keep stderr/status from failed attempts. No P12, private blob, dump, password,
packet key log or personal-store inventory in artifacts. Certificate public DER
is sufficient for peer identity. Disposable P12 fixture storage, if needed by the
fixture generator, stays access-restricted and is removed by its test owner; it
is never a diagnostic artifact or a persisted raw-key workaround.

## Outcome and recommendation

The architecture is **not established feasible unchanged**: durable store ownership
and stable journal locking need the above refinements, and actual named import,
non-exportability, SSPI usability and interrupted-cleanup behavior need native
receipts. These corrections preserve the approved direction and existing
quarantine policy; they do not justify a curl change, new credential subsystem,
TLS downgrade or Linux/macOS modification.

Recommendation: keep production implementation on hold; review this source-only
result and execute the bounded native gates when a disposable native environment
is available. The two security tradeoffs are conditionally approved for feasibility
and are not being reopened. There is no new claim of Windows passing acceptance.

Files changed in this sprint: this report, design clarification, current-engineering
and CI-handoff links, and its compact evidence receipt. Previous uncommitted
investigation/design files remain preserved. No staging, commits, push, branches,
hosted dispatch, production changes, existing-test edits or root README edits.
**Stop for Golden Wrench review.**
