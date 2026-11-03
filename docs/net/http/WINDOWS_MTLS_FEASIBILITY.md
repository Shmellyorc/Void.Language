# Windows Schannel credential feasibility

## Native Windows VM verdict, 3 November 2026

**The credential mechanism works for genuine TLS 1.2 mTLS, but the proposed
architecture is NOT production-feasible unchanged.** On this VM, named PKCS#8
import replaced a different synthetic sentinel key without an overwrite flag.
Immediate import and deferred import/finalization both reproduced this failure.
Do not implement production persistence from the positive handshake alone.
TLS 1.3 is explicitly unsupported on this Windows 10 host, not verified passing.

The standalone [sources and reproduction instructions](native-schannel/README.md)
and [compact native receipts](evidence/windows-native-schannel-20261103.json)
are the deliverables. Production implementation, public API, existing tests,
vendored curl, Linux/macOS, workflows and root README are unchanged. This is local
native feasibility evidence, not independent review or full HTTP acceptance.

### Baseline and actual tools

| Item | Observed identity |
| --- | --- |
| Repository / branch / upstream | `Shmellyorc/Void.Language`, `main`, `origin/main` |
| Experiment baseline | `440427c5f86b8921b6208a5ff62bab93c0eaddad`, fast-forwarded from `c9a1f47c16b8fe2410cf33dd97e33dfff72edcc8` after Boss published the required documents |
| Native OS | Windows 10 Home 22H2, x64, `10.0.19045.6456` |
| Final compiler / linker | LLVM Clang/LLD `23.1.4`, revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5` |
| SDK / C++ headers / CRT | Windows SDK `10.0.19041.0`, MSVC `14.44.35207`, static `/MT`, separate Debug and Release builds |
| SDK archive SHA-256 | `854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2`, matches the repository pin |
| Other available tools | CMake `4.4.3`, Ninja, Python `3.14.8`, Git `2.56.0.windows.3`, MinGW GCC `16.2.0`, MSVC compiler `19.44.35229` |
| Test transport | Authoritative vendored curl `8.23.0`, Schannel, built with pinned Clang; all 1,586 source entries and four approved corrections verify |
| Certificate generation / peer | Installed OpenSSL CLI `3.5.9`; Python SSL/OpenSSL `3.5.9`; generated RSA-2048 and ECDSA P-256 clients, disposable two-day CA/server certificates and signed CRL |

Only the current source was cloned, with shallow Git history. No Linux workspace,
project archive or compiler build was transferred. The complete verified LLVM
SDK and all generated outputs remain disposable under `.toolchains/`.
The initial sandbox proxy was unavailable; command-local direct GitHub access
worked. SDK/compiler execution and test fixture DACL setup required the native
execution context outside the restricted sandbox. The exploratory MSVC binary
was quarantined by Defender; Boss resolved that external block. The harness did
not change antivirus settings. Final evidence uses pinned Clang/LLD.

Fresh Windows checkout exposed two existing case-only JSON evidence collisions
(`readonly-Debug.json`/`readonly-debug.json`, and Release equivalents). Those
unrelated working-tree differences were preserved and excluded from staging.
Required documents were read, as were the actual Runtime selector/configuration,
close-result paths and curl Schannel credential/store lifetime code.

### A. Temporary P12 decode and controlled reimport

Both algorithms passed `PFXImportCertStore` with NO_PERSIST, ALWAYS_CNG,
USER_KEYSET and the explicitly approved temporary EXPORTABLE flag. The observed
provider was Microsoft Software KSP; key spec was `CERT_NCRYPT_KEY_SPEC`, export
policy was **1**, caller-free was **false**, and the fixture contained two
certificates with exactly one private-key context. Bad-password decode returned
`0x56` before any durable creation. Input, PKCS#8 and property buffers have explicit
bounds; private transport bytes and passwords are never receipts or files.

Three native corrections are necessary:

1. Acquiring the NO_PERSIST context without `CRYPT_ACQUIRE_CACHE_FLAG` returned
   `CRYPT_E_NO_KEY_PROPERTY` (`0x8009200B`). Cached CNG-only acquisition succeeds;
   the borrowed handle belongs to the certificate context. Because a cached
   acquire does not compare public keys, the probe separately exports public SPKI
   and uses `CertComparePublicKeyInfo` against the certificate before key export.
2. Policy 1 permits encrypted export, not plaintext. Plain PKCS#8 and a secret-only
   export returned `NTE_NOT_SUPPORTED` (`0x80090029`). The working bounded bridge
   uses `NCRYPTBUFFER_PKCS_SECRET`, explicit PKCS#12 SHA1/3DES PBE OID, a fresh
   eight-byte salt and 10,000 iterations through `NCRYPTBUFFER_PKCS_ALG_PARAM`.
   The independent random transport password and encrypted PKCS#8 remain in
   zeroing process buffers and are wiped before selector publication. No
   plaintext-export permission was added to the temporary key. This is an
   experiment-specific encrypted handoff, not approval of a production envelope.
3. `CRYPT_KEY_PROV_INFO.dwKeySpec` must be **0** for this native CNG key open.
   Using `CERT_NCRYPT_KEY_SPEC` there produced `SEC_E_UNKNOWN_CREDENTIALS`
   (`0x8009030D`) at `AcquireCredentialsHandle`. That acquisition-result key-spec
   constant is not the `NCryptOpenKey` legacy-key-spec parameter.

Deferred named import, export-policy zero, signing-only usage, restricted DACL,
finalization and reopen succeeded. The reopened key was user-scoped; a real
signature verified, its public blob matched the staged key, and both plaintext
and correctly parameterized encrypted private-export controls failed with
`0x80090029`. These failures use a live, signing-capable handle.

Immediate named import separately defaulted to export policy 0; attempting to
set that immutable policy afterward returned `NTE_BAD_KEY_STATE` (`0x8009000B`).
It also failed the real different-key collision test below. It is not a repair.

### B. Genuine Schannel authentication

Debug and Release each passed RSA and ECDSA TLS **1.2** authentication through
the existing `CurrentUser\\private-store\\SHA1-thumbprint` curl selector.
The peer required a client certificate and compared its entire DER byte-for-byte
with the expected generated certificate; SHA-256 receipts are retained. Each
client made two successful HTTP requests with new-connection counts **1 then 0**.
Two simultaneous independent owners of the same P12 also authenticated; after
the first closed and its key/store were deleted, the second still authenticated.

CA verification, hostname verification and Schannel revocation checks stayed
enabled. The first positive attempt authenticated at the peer but curl rejected
unknown revocation status; it was **not** counted as successful verified HTTPS.
Serving the disposable CA's signed CRL through its certificate distribution point
resolved this without a verification exemption. Anonymous clients failed with no
client certificate at the peer; wrong roots and wrong hostnames returned curl 60.
Those server-verification failures can occur after the peer has seen the client
certificate, so a peer authentication event alone is not counted as request success.

TLS **1.3** was explicitly requested separately. Pinned curl returned 35 with
`TLS 1.3 not supported on Windows 10 or older`, before a successful handshake.
No TLS downgrade, registry enablement or OS-support inference was used. Windows
11/Server 2022 TLS 1.3 and the newer `SCH_CREDENTIALS` branch remain native gates.

The ordinary non-persistent P12 control remained unsuccessful (curl 35,
`SEC_E_ALGORITHM_MISMATCH`, `0x80090331`, on this fixture/OS); the peer recorded
no authenticated identity. This differs from the preserved Server 2022
`0x80090304` incident and does not independently establish LSASS internals.
The harness made no persistent fallback. Production NO_PERSIST behavior is intact.

### C. Ownership, durable markers and explicit deletion

`CertRegisterSystemStore(CURRENT_USER | CREATE_NEW)` created only the generated
private store; a repeated registration returned `ERROR_FILE_EXISTS` (`0x50`).
`CertOpenStore(OPEN_EXISTING)` reopened it. `CertEnumCertificatesInStore` enumerated
only that private store and found exactly the expected one certificate. No global
user-key/store inventory or MY, ROOT or CA mutation was performed.

The probe sets a protected current-user/SYSTEM key DACL. The private store's
registry DACL has inheritable current-user/SYSTEM entries; root and certificate
leaf ACLs were read back and checked. The store's public owner marker is written
and flushed on the exact newly created registry key, then verified by a separate
recovery process. Certificate contexts are rebuilt from DER and bound only to the
controlled provider/name, with no borrowed ephemeral handle property copied.

Custom key properties with `NCRYPT_PERSIST_FLAG` alone returned `0x80090029`.
Adding `NCRYPT_PERSIST_ONLY_FLAG` succeeded before finalization, survived finalize,
reopen and actual process termination, and was verified in a new process.
The key was not reopenable before finalize, including after the marker setter
(`NTE_BAD_KEYSET`, `0x80090016`). This supports a usable marker sequence on this
KSP/build; it is **not** proof of an atomic power-loss transaction.

`NCryptFreeObject` and `CertCloseStore` left the named key/private store reopenable.
After curl cleanup/process completion, explicit store deletion through
`CertUnregisterSystemStore(CURRENT_USER | CERT_STORE_DELETE_FLAG)` and separate
`NCryptDeleteKey` succeeded. Exact reopen returned error **2** for the store and
`0x80090016` for the key. The successful key deletion consumes its handle.
No private-key filesystem paths or prefix-scan deletion were used.

### D. Collision failure and conservative recovery

**Blocking native finding:** the probe first created a different RSA sentinel
with `NCryptCreatePersistedKey`, finalized it, and stored its public identity and
owner marker. Importing the P12 key with that exact sentinel name and **without**
`NCRYPT_OVERWRITE_KEY_FLAG` returned success. The imported handle's name matched
the sentinel. A fresh open found a **different public key** and the sentinel's
marker was missing (`NTE_NOT_FOUND`, `0x80090011`). The deferred-import variant
also imported/finalized successfully. Both Debug and Release reproduced the
different-key failure; both fixture-owned keys were explicitly deleted afterward.

This contradicts the expected no-overwrite behavior in the
[Microsoft import contract](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptimportkey).
The native receipt establishes behavior on this VM, not every Windows version.
A same-key retry alone was insufficient evidence; the different-key sentinel
and fresh public/marker readback establish actual replacement. A random nonce,
pre-open absence check or journal lock cannot supply atomic provider collision
protection against another creator. **Remove the proposal's assumption that this
import path can safely acquire CREATE_NEW ownership.** No large workaround or
production persistence implementation was added.

The remaining bounded failure controls ran in both builds:

- Actual child termination after flushed intent, deferred import, finalized key,
  private-store registration before its marker, and flushed store marker.
  A finalized key's marker survived process death; marked objects were recovered.
- Missing key marker, incorrect key marker and unmarked private store caused
  quarantine with no automatic deletion. The fixture controller then performed
  explicit cleanup of its own recorded child-created objects; that provenance
  exception is not a production recovery rule.
- Recovery of a live owner failed on the stable sidecar with sharing violation
  **32**. Replacing the separate journal while live succeeded; replacing the lock
  failed with 32. Terminating the owner released the lock for the next process.
- Corrupt and over-4-KiB records, and a wrong expected public-key digest, were
  rejected before deletion; the correct record subsequently recovered the key.
- Denying DELETE on the exact private-store registry key produced actual
  `ERROR_ACCESS_DENIED` **5**. The public journal remained present. Only the test
  object's ACL was restored, followed by checked explicit deletion and absence.
- The attempted key-only DELETE-denial control was **ineffective**: deletion
  still succeeded, including after closing/reopening the handle. This failed
  fault-injection method is preserved and is not claimed as a KSP-denial pass.

The fixture recovery parser is deliberately small, bounded and tied to its exact
generated receipt. It does not implement production SID/application scoping,
Known Folder/reparse admission, record caps, general recovery, or an allocation-
free destructor. Native process termination is not a power failure. Key-marker
durability on another KSP/build and denial of KSP deletion need further evidence.

### Final verification, residue and recommendation

There are **26 final profile/case receipts**: 13 cases in each of Debug and
Release. These include two deliberate collision-security failure cases per build, not 26 passing
architecture guarantees. Every final case ends with a separate exact-name absence
check. RSA/ECDSA, connection reuse, concurrent-owner isolation, store denial,
liveness and conservative recovery checks passed their stated fixture assertions.
Original password-framing, acquisition, export, marker, association and revocation
failures are retained separately. Initial error-86 P12 attempts had a fixture
text-pipe password framing defect; they do not establish AES-P12 incompatibility.

**Retained test credentials: none.** Generated private keys/P12 files were removed;
all final known named keys/stores were absent, including explicitly cleaned
quarantined fixtures and overwritten synthetic sentinels. No unresolved public
recovery receipt remains. Only local disposable build/toolchain caches remain.

Recommendation: **keep production integration on HOLD for Golden Wrench review**.
The non-exportable Software KSP key/private-store Schannel mechanism is real, but
the exact named-import ownership design fails its collision safety requirement.
A replacement creation transaction needs a genuinely verified no-overwrite
primitive and must preserve signing/export/ACL/marker guarantees. Do not silently
accept this failure because names have high entropy. Keep the existing
non-persistent option and borrowed-store ownership unchanged.

Other remaining acceptance gates include native Windows 11/Server 2022 TLS 1.3,
ambiguous/malformed P12 and unsupported-provider cases, comprehensive allocation/
OS-failure coverage, and eventual Runtime cancellation/GC/close/shutdown error
propagation. No full compiler, strict sanitizer suite, hosted CI, production
integration or cross-platform acceptance was run or claimed. Only these native
feasibility deliverables are authorized for the specified `[skip ci]` commit/push.
**Stop after the authorized push for Golden Wrench review.**

---

## Earlier Linux source-only investigation, preserved

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
