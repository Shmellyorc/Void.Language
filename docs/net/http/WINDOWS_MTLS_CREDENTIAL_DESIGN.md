# Windows P12 credential ownership — proposed architecture

3 November 2026. LLVM/Clang/LLD 23.1.4; source baseline
`c9a1f47c16b8fe2410cf33dd97e33dfff72edcc8`. **Design only; requires Golden Wrench
architecture review and Boss implementation approval.** No production, tests,
providers, TLS policy or vendored source were changed. Linux/macOS stay untouched.

## Conditional feasibility authorization and corrections

Boss has conditionally approved the two disclosed tradeoffs (temporary in-memory
exportability and crash residue with conservative recovery) for isolated feasibility
work only. Production implementation remains unauthorized. The
[feasibility report](WINDOWS_MTLS_FEASIBILITY.md) records the unavailable native
test environment and source-supported corrections: store properties are not
durable markers, the live lock must be a stable sidecar separate from replaced
journal records, and explicit store deletion must be checked. Native key-marker
atomicity remains unproven. These clarifications govern the proposal below.

## Evidence and source corrections

[CI 41563174932 investigation](CI41563174932_SCHANNEL_REVIEW.md) preserves both
Windows 340/341 results, P12 password/anonymous/trust negatives and subsequent
`InitializeSecurityContext` failure `0x80090304`. Peer receipts contain no
successful identity. The non-persisted-key/LSASS boundary remains the strongly
supported explanation, not a native-verified repair.

Actual source changes two assumptions in the preceding proposal:

- `stdlib/net/http.void` already has `client_identity.windows_store(reference)`;
  `_pack_options` encodes identity 3, `client_error` validates the exact selector,
  `Driver::open` excludes it from file snapshots, and `configure` sets SSLCERT.
  The earlier review's claim that a store reference needs a new API is incorrect.
- Each `ClientState` owns its own CURLM; CURLSH shares **cookies only** within that
  client. The driver drains transfers, cleans the cookie easy/share/multi, then
  publishes `closed`. This gives a usable credential-release boundary. But
  `ClientResource::close` and `RuntimeDriver::close` currently return success
  unconditionally: observable deletion failure needs explicit integration.

Pinned curl `get_client_cert` accepts `CurrentUser\\store\\thumbprint`, opens that
specific store and finds that certificate. The certificate store survives with
its reference-counted SSPI credential until `schannel_session_free`. No public
Schannel certificate-context setter is needed for the design below.

## One recommended API and integration

Append one identity case (native numeric identity 4; retain existing numbers):

```void
client_identity.windows_user_pkcs12(path: string, password: string)
```

It is a **combined explicit authority** to reimport a caller-supplied P12 key
through protected process memory and persist a non-exportable, user-scoped
Windows key until client cleanup. Its declaration/hover must explicitly disclose
both temporary exportable decoding and persistent OS storage, including crash
residue. Choosing this case is the opt-in; ordinary `pkcs12` remains non-persistent
and never retries as this case after curl 35. If the combined authority is not
approved, this design must not be implemented under a persistence-only label.
The future Windows fixture's successful P12 case must explicitly choose this
new authority. Keep a separate non-persistent P12 check for zero durable writes
and no fallback; never count its failed handshake as mTLS success. Linux/macOS
positive P12 tests keep their existing identity. This is an API choice awaiting
approval, not a test-only substitution performed in this assignment.
No separate managed credential object, provider selector, caller-chosen key
container, filesystem path for raw keys, global default or new worker pool.

The existing value `tls_options.identity` carries the case unchanged through
copies; each `open_client` snapshots material and creates an independent native
owner. Explicitly close the client to observe cleanup. Other platforms return
`unsupported` for this new case without touching storage; their existing cases,
providers and behavior are unchanged. Existing `windows_store` continues to
borrow caller-managed credentials and must never acquire deletion ownership.

**Use a unique private CurrentUser certificate store and the existing exact
store-selector path. No curl change is presently justified.** The store is not
MY, ROOT or CA, is not registered into a trust collection, and is never searched
as an ambient identity source. Only the generated private store's exact leaf is
selected. A SHA-1 thumbprint is required by curl as a locator, not as the security
identity check: compare full DER and SHA-256 internally and at the peer. Cert/key
association uses `CERT_KEY_PROV_INFO_PROP_ID` naming the controlled persisted CNG
key, not a borrowed ephemeral `CERT_KEY_CONTEXT` handle.

## Creation: why direct persistent PFX import is rejected

`PFXImportCertStore` does not take a caller-selected fresh key name or provide
transactional rollback of every possible partial persistent import. Removing
`PKCS12_NO_PERSIST_KEY` cannot meet the ownership contract. Repackaging with a
new raw ASN.1/P12 implementation is also disproportionate.

Recommended Windows-only owner implementation, using the Microsoft Software KSP:

1. Reserve the existing client slot/memory admission **before durable creation**;
   current `Driver::open` snapshots files before slot admission, so persistence
   cannot simply be added inside its file-reading lambda. Keep the combined
   1 MiB input limit. Recover eligible prior orphan records before admitting a
   new persistent owner; fail closed on unresolved same-application recovery.
2. Decode with `PFXImportCertStore` using `PKCS12_NO_PERSIST_KEY`,
   `PKCS12_ALWAYS_CNG_KSP`, `CRYPT_USER_KEYSET`, and **explicitly approved
   `CRYPT_EXPORTABLE` for this temporary decode only**. Verify actual provider
   and key type: do not assume a requested CNG flag guarantees the provider.
   Reject unprotected/unsupported password forms, ambiguous multiple-key P12,
   unsupported algorithms/provider, excessive certificate counts or oversized
   output. Proposed initial scope: one RSA or ECDSA private key, at most 16
   certificates; extra public chain certificates do not create extra keys.
3. Acquire the temporary key with `CryptAcquireCertificatePrivateKey`, verify
   its public key matches the selected certificate, and export a bounded PKCS#8
   blob with `NCryptExportKey`. This extra memory copy and temporary exportability
   are real security costs, **not** equivalent to a non-exportable import.
   Use a dedicated zeroing buffer, never log/write it, destroy it on every exit.
   Reject any provider that cannot perform this operation; no silent downgrade.
4. Prepare fixed-size owner/recovery metadata and a held process-lifetime lock.
   Generate 256 random bits with `BCryptGenRandom`; derive distinct bounded key
   and private-store names from that nonce. Persist intent before creation.
   Open only `MS_KEY_STORAGE_PROVIDER`. `NCryptImportKey` with PKCS#8,
   `NCRYPTBUFFER_PKCS_KEY_NAME` and `NCRYPT_DO_NOT_FINALIZE_FLAG` creates the named
   user key. Never set MACHINE, OVERWRITE or WRITE_KEY_TO_LEGACY_STORE flags.
   `NTE_EXISTS` means collision: do not open/delete that existing key.
5. Before finalization, set export policy zero, signing usage consistent with
   the certificate, restricted security descriptor (current SID and SYSTEM;
   no broad inherited access) and an owner marker. Finalize and read back the
   policies, provider, public-key identity and marker before publication.
   **Native gate:** establish exactly when key/properties become durable and
   whether the marker can persist with finalization. API documentation does not
   establish an atomic crash transaction here. Do not assume it does.
6. `CertRegisterSystemStore` with CURRENT_USER and CREATE_NEW creates only the
   fresh private store. Check creation success; an existing store is not owned.
   Add fresh certificate contexts made from DER (not arbitrary copied PFX
   properties), bind the leaf to the named KSP key, add only bounded chain
   certificates, and check exact contents. Set/read back restrictive store
   security and a durable registry-backed owner marker on that exact private
   store before exposing the selector; CertSetStoreProperty is not persistent.
   See the feasibility report for the documented registry/security boundary and
   unmarked-store quarantine interval. No guessed private-key paths. Do not import trust anchors into ROOT.
7. Publish `CurrentUser\\VOID-HTTP-<nonce>\\<40-hex-thumbprint>` privately to the
   driver's existing store branch. Keep the public option identity as the new
   case. Wipe temporary PKCS#8/key/password buffers and close the ephemeral PFX
   store. Confirm the **persisted** key is non-exportable by a negative export
   control in the acceptance fixture.

Limits are proposed admission policy for this opt-in, not new restrictions on
existing P12. Bound the PKCS#8 buffer at 1 MiB and reject larger output before
allocation. Charge input + bounded decoded/export buffers + fixed owner/journal
metadata against the existing ledger. Provider allocations retain the unresolved
Schannel proof boundary in [MEMORY_REVIEW.md](MEMORY_REVIEW.md); this design does
not claim a total-process heap bound or eliminate paging/crash-dump exposure.

## Owner, failure and recovery state machine

`ClientState` holds one Windows-only `WindowsCredentialOwner` with fixed cleanup
metadata, owned handles, creation flags and first failure. No source references
or callbacks run on the driver. Creation/recovery run synchronously at client
open in the existing NativeSafe boundary; no extra queue/scheduler. Persistent
storage operations may block in Windows: do not promise cancelable OS calls.

```text
admitted -> ephemeral validated -> intent durable -> key owned -> store owned
         -> ready -> closing -> curl fully released -> deleting -> clean
any creation failure -> rollback owned objects -> clean or cleanup_pending
crash -> orphan record -> same-owner recovery -> clean or quarantined
```

Transfers, idle connections and SSPI sessions retain the client, not independent
key copies. Cancellation closes exchanges first; owner deletion waits until all
client easy handles, cookie inspector, share and CURLM have been cleaned. There
is no cross-client SSL-session sharing in current source. Disable exposure of the
private selector through public capability/debug output. Verify curl cleanup
actually frees all matching SSPI references natively; if it does not, stop for a
specific integration review rather than introducing an unproven delay.

After releasing curl/SSPI, remove owned certificate entries/store registration with checked
`CERT_STORE_DELETE_FLAG` deletion and absence verification, then delete the exact owned key with `NCryptDeleteKey`. Close/freeing a store or
key handle is not deletion. Missing already-deleted owned objects are idempotent
success; access denial or identity mismatch is not. Never delete by prefix scan.
Prepare cleanup resources before persistence; no allocation-dependent destructor
cleanup. Retain the recovery record until all durable objects are confirmed gone.
Attempt rollback after every returned creation error, using only acquired
ownership; preserve primary and cleanup errors separately.

**Crash recovery is conservative, not magical.** Use bounded internal metadata
under the current user's local application-data VOID HTTP directory, resolved by
Known Folder APIs, protected against reparse redirection and broad ACLs. No
private bytes/passwords belong there. Scope by SID plus canonical executable
path digest and schema version (path is an organizational scope, not a security
boundary). Hold an exclusive stable lock sidecar, separate from the mutable journal,
with sharing rules that prevent recovery while the live owner exists. Never
replace the lock file. Use checked replacement/flush of journal records before
creation; this is not a power-loss transaction guarantee. PID alone is not liveness. Limit the application namespace to 64 records
of at most 4 KiB; overflow or corrupt metadata blocks new persistent acquisition,
not existing ordinary HTTP. Do not enumerate all user keys/stores.

Recovery occurs only on a later explicit persistent acquisition by the same SID
and application scope. An unlocked record is eligible only after checking its
exact provider/name, persisted owner marker and public-key/DER identity. Never
follow a record into a different provider, user, namespace or machine store.
Markers distinguish a recorded intent from a key whose creation actually
succeeded. An existing name on collision is never adopted. A key or store left
without a provable marker by a crash/partial OS failure is **quarantined and
reported, not automatically deleted**. This deliberately trades automatic
reclamation for preservation of pre-existing credentials. Native experiments
must characterize these windows; if full atomic marker creation is unavailable,
that residual manual-recovery boundary is part of the approval, not hidden.

Same-user hostile processes and administrators can read/alter the process and
user-owned state; this is not isolation from them. Never claim cleanup after
power loss, inaccessible profiles, a changed application path, backups or OS
corruption. No forced secure-erasure guarantee. Namespace/profile changes can
leave a record requiring explicit operator disposition. There is no background
credential manager, startup deletion outside opt-in, or general-purpose key API.

## Observable cleanup within existing Runtime machinery

The current driver completes `client.closed` with success; the resource wrappers
ignore its result. Implementation must carry a stable cleanup result instead:

- Preserve ErrorKind `tls_failure`; use Phase `cleanup` and append a
  `windows_crypto` native domain for raw Win32/SECURITY_STATUS (preserve existing
  enum numbers). Preserve acquisition error plus existing cleanup-code field
  when rollback fails. No private material or container names in public messages.
- Complete `closed` once with the first cleanup failure. Repeated explicit close
  returns that same result; it never reopens the client or reports false success.
  Failed deletion leaves only bounded metadata/recovery state, not a usable key
  lease in a closed client. Recovery on a later open is separate from close.
- `ClientResource::close` and `RuntimeDriver::close` must return the recorded
  `std::error_code` rather than `{}`. Keep a bounded driver aggregate for GC-led
  cleanup. `NativeLibrary::shutdown` already aggregates resource errors; use it.
  `NetworkJob::release` must not discard a new cleanup failure when publishing
  the explicit close reply. Keep NativeSafe ownership and generation leases.
- Destructors remain noexcept/best effort; they cannot deliver a result to a
  vanished caller. A recovery record plus the Runtime shutdown error path is
  required. Explicit close is the deterministic user-observable boundary.
  Verify allocation-free cleanup controls and source `with` failure propagation.

No core GC/task scheduler redesign is required. Windows failure plumbing may
extend shared records, but behavior for all existing non-Windows cases stays
identical. Do not release the ledger charge before live secret buffers/handles
are gone. Durable quarantined metadata has a separate bounded disk cap.

## Alternatives and approval boundaries

- Blind persistent PFX import or the upstream fork: uncontrolled naming/partial
  import and ignored deletion errors; rejected.
- Existing caller-managed `windows_store`: already supported and useful, but
  cannot stand in for this owned P12 feature or authorize deleting external keys.
- Per-client private store: uses existing curl support; adds bounded public store
  metadata. Preferred over a new private curl certificate-context ABI. A narrowly
  scoped curl change is a fallback only if native evidence disproves this path.
- Custom P12 parser/new production crypto library, TLS cap, system credentials,
  provider replacement or CAPI fallback: unnecessary scope or weaker semantics.

**Conditional approvals are recorded:** Boss permits temporary exportable
in-memory decoding/reimport and crash residue with conservative ownership/recovery
for feasibility work. Do not request those decisions again. This does not approve
production persistence. Native gates below and the corrections in the feasibility
report must be resolved before implementation authorization. This document is not
independent security approval or Windows acceptance.

## Implementation inventory and order (future, not edits made now)

| Files | Planned responsibility |
| --- | --- |
| `stdlib/net/http.void`, `include/void/http.hpp`, `src/runtime/http_abi.cpp` | Append identity 4, native-domain mapping, value packing/validation and disclosure; preserve existing ABI fields/numbers where possible |
| `src/runtime/http.cpp` | Reserve before persistence, hold owner, route private store selector, delete after curl quiescence, preserve close/shutdown failures |
| new `src/runtime/http_windows_credentials.cpp`, new `include/void/detail/http_windows_credentials.hpp` | Windows-only bounded CNG/store owner and journal/recovery; no Linux/macOS implementation |
| `include/void/http_resource.hpp`, `src/runtime/http_resource.cpp`, `include/void/detail/http_hooks.hpp` | Carry cleanup results and Windows-native fault/ownership probes; no source callbacks |
| `CMakeLists.txt` | Windows-only source and system `ncrypt`, `crypt32`, `bcrypt`, security/known-folder API link closure; no new third-party dependency |
| new `tests/net/http/windows_credentials.cpp`, new `tests/net/http/windows_credentials.py` | Isolated synthetic-key native acceptance and fault/crash harness |
| `tests/net/http/native.cpp`, `tests/net/http/fixture.py`, source/IR fixtures as required | Add Windows opt-in acceptance without removing non-persistent, borrowed-store or credential negatives; existing Linux/macOS runs unchanged |
| `docs/net/http/README.md`, `NATIVE_PLATFORMS.md`, `MEMORY_REVIEW.md`, implementation receipts | Document final approved API, limits and measured evidence |

No vendored curl, provider setup scripts, `.github` workflow, core GC or backend
change is planned. `src/frontend/http.inc`, `src/ir/http.inc` and
`src/backend/llvm/http.inc` must be checked against the unchanged packed-layout
contract; do not edit them unless the accepted ABI addition actually needs it.

Sequence: with the conditional feasibility approvals recorded, run the standalone native gates
using disposable generated credentials; review receipts and exact Windows owner
code; integrate opt-in/API and error propagation; run focused native Debug/Release
and source normal/O2 JIT/independent AOT; review before Boss's normal acceptance.
Do not request a hosted run for this design document.

## Native acceptance plan: prove each guarantee

| Guarantee | Required observation / negative control |
| --- | --- |
| Correct persistence and key policy | Inspect actual KSP, SID/DACL, export policy and persisted marker after reopening; deny export of final key; no machine/legacy key creation |
| Safe transfer | Ephemeral import/export -> named import on Server 2022/Windows 10+; inspect NO_PERSIST staging; verify bounds/zeroing and no private file/log output |
| Authentication and trust | Exact peer SHA-256/full DER for successful P12, separately TLS 1.2 and 1.3; bad password, anonymous/wrong identity, wrong CA/hostname fail |
| Password before publication | Snapshot owned namespace before bad-password attempt; no new key/store/intent requiring rollback afterward |
| Owner separation and reuse | Two clients using identical P12 get distinct names; simultaneous requests/reconnects/reuse succeed; closing one never damages the other |
| Last-reference lifetime | Hold response/session/idle connection while closing; credential stays until final curl/SSPI release; then key/store absent, not just handle closed |
| Cancellation, GC, shutdown | Cancel at each creation/handshake/stream stage, force GC and shutdown; repeatable error results, zero live leases; no deadlock with worker/driver |
| Partial rollback | Inject each allocation/API/registration failure; inspect exact inventory; owned objects removed or explicit cleanup_pending record, never silent pass |
| Deletion denial | Deny deletion using test-controlled ACL, verify close/with/shutdown errors and retained record; restore ACL and recover explicitly |
| Existing credentials protected | Pre-create colliding foreign key/store and mismatched marker/public key; import must fail without alteration/deletion; unrelated sentinel keys unchanged |
| Crash recovery | Kill child at every intent/import/marker/finalize/store/publication/deletion boundary; never reclaim live owner or PID-reused process; marked orphan recovered, ambiguity quarantined |
| Bounded recovery | Corrupt/oversized journal, reparse directory, wrong SID/app scope, 65th record, provider substitution all fail closed; no broad store scans |
| Platform compatibility | Windows Debug/Release + relevant source/LSP/installation closure; unchanged Linux/macOS code and existing regression selection; no claim from Linux tests about Schannel |

[Design validation and exact changed-file inventory](evidence/windows-mtls-design-review.json).

**This assignment's validation:** source/API contract review and documentation
link/inventory checks only. No native Windows execution was available; no credential
creation experiment was run, no real key persisted, no tests modified or rerun to
imply implementation acceptance. Prior local 6/6 per profile remains historical.

## Primary contracts consulted

- [PFXImportCertStore](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-pfximportcertstore): ephemeral/persistent representation and import flags.
- [NCryptImportKey](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptimportkey): named PKCS#8 import, no-overwrite and deferred finalization.
- [NCryptExportKey](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptexportkey): memory export; not an implicit authority to export.
- [Key properties](https://learn.microsoft.com/en-us/windows/win32/seccng/key-storage-property-identifiers) and [NCryptSetProperty](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptsetproperty): policy/security/marker mechanisms; no transaction guarantee inferred.
- [CertRegisterSystemStore](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certregistersystemstore), [CertOpenStore](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certopenstore), [certificate properties](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certsetcertificatecontextproperty): exact private store and provider association.
- [curl issue 17626](https://github.com/curl/curl/issues/17626): failure evidence, not approval of its persistent-import fork.

**Stop for Golden Wrench architecture review. No implementation approval is inferred.**
