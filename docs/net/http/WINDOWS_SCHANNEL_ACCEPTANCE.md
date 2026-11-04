# Windows Schannel store-identity acceptance repair

## Hosted acceptance — 4 November 2026

**ACCEPTED.** [Final acceptance](ACCEPTANCE.md) records run
[41815539463](https://github.com/Shmellyorc/Void.Language/actions/runs/41815539463),
commit `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`, Compiler link experiment,
mode `thinlto-cache`, LLVM/Clang/LLD 23.1.4. All seven hosted jobs passed.
Golden Wrench closes HTTP/HTTPS cross-platform acceptance, including Windows
Schannel store-backed mTLS and the connection-scoped HTTP/2 ALPN retention issue.
The separate optimizer incident and documented limitations retain their status.
Earlier checkpoints below preserve their original scope and evidence.


3 November 2026. Baseline `eb1b396a3e0199a5d0c804e96bd8564d6053e0b8`;
LLVM/Clang/LLD 23.1.4. **Scoped local Windows acceptance passes; stop for
independent Golden Wrench source review.** No staging, commit, push, branch,
remote change or hosted CI dispatch. Windows Server 2022 acceptance is separate.

Boss/Golden Wrench selected Schannel with explicitly provisioned store identities
for this delivery. Windows supplied P12 remains unsupported under the required
non-persistent policy. This repairs the Windows acceptance requirement and its
capability reporting; it does not repair in-memory P12. The original
[hosted failure](CI41563174932_SCHANNEL_REVIEW.md),
[Schannel/CNG experiments](WINDOWS_MTLS_FEASIBILITY.md) and
[rejected AWS-LC integration](WINDOWS_TLS_ADAPTER_FEASIBILITY.md) remain intact.

## Production behavior and public API

The existing `client_identity.windows_store(reference)` path already carries the
selector through the source ABI into owned ClientState configuration and curl's
Schannel `CURLOPT_SSLCERT` lookup. The existing file-snapshot exclusion for this
form is correct. Native public-source execution now proves that path; a standalone
curl success is not the acceptance basis.

The production correction is small:

- Windows rejects supplied `pkcs12`, alongside the already unsupported PEM form,
  with `unsupported` / `prepare` before reading identity or CA files, importing
  a private key, or attempting a request. There is no persistence or fallback.
- Windows capabilities advertise `windows_store` only. Linux/macOS retain the
  identical PEM/P12 capability bits and admission behavior.
- The store-selector implementation, TLS/provider ownership, minimum TLS 1.2,
  verification, pooling, driver, worker pool, precise GC and public API are
  unchanged. No production credential manager or new dependency is introduced.

The reference is `location\store\40-hex-thumbprint`. Locations accepted by the
existing validator are `CurrentUser`, `LocalMachine`, `CurrentService`, `Services`,
`Users`, `CurrentUserGroupPolicy`, `LocalMachineGroupPolicy` and
`LocalMachineEnterprise`. Use the exact location spelling and one nonempty store
segment, without `/` or `:`; the certificate selector is exactly 40 hexadecimal
characters, either case, without separators. This is the Windows certificate
SHA-1 thumbprint used for explicit lookup, not a relaxation of TLS signature or
chain policy. Peer acceptance below compares full DER/SHA-256 independently.

The caller provisions the certificate, its private-key association and provider
access before opening the client. Keep that authority available and stable until
client/pool/Runtime cleanup drains. VOID borrows it and never deletes its key or
store. A store reference is not an imported private-key snapshot. Invalid selector
syntax returns `invalid_argument` during preparation; missing stores/thumbprints
and a selected certificate without a usable private-key association produce
`tls_failure` when the connection is attempted. There is no automatic identity
selection. The existing `SSLCERTTYPE=PEM` guard on the store path still prevents a
failed selector parse from falling through to a same-named P12 file import.

Native acceptance here covers CurrentUser Microsoft Software KSP RSA-2048 and
ECDSA-P256 identities, including non-exportable keys. Other accepted locations,
legacy CAPI, hardware/smartcards, PIN/UI and enterprise permissions remain governed
by Schannel/provider behavior and need their own native acceptance. They are not
silently replaced by exported keys or an alternate backend.

## Exact native environment

| Component | Executed identity |
| --- | --- |
| Host | Windows 10 Home 22H2 x64, `10.0.19045.6456` |
| Compiler/linker | Clang/LLD 23.1.4, revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5` |
| SDK/ABI | Windows SDK 10.0.19041.0, MSVC headers 14.44.35207, `/MT` in independent Debug/Release builds; Debug retains `/Od /Zi`, `_ITERATOR_DEBUG_LEVEL=0` |
| Provider | Production Schannel, pinned curl 8.23.0 with only the four approved corrections |
| Native dependencies | Verified static Windows nghttp2/c-ares/zlib/libpsl closure from the unchanged native recipe; WinIDN/SSPI |
| Test certificates | Separate pinned AWS-LC 5.12.0 CLI; no AWS-LC crypto/TLS archive enters the Windows production provider |
| Peer/controller | Python 3.14.8, OpenSSL 3.5.9; pinned real h2 fixture |

The official existing complete LLVM SDK archive was reverified, reused locally,
and its ordinary static development dependencies prepared. No compiler/toolchain
pin or vendored source was changed. The [public evidence](evidence/windows-schannel-acceptance-20261103.json)
contains executable/source hashes, dependency-manifest identities, per-case
observations, original failures and final reruns.

## Actual acceptance results

**Effective focused acceptance is 17/17 CTests in each profile, 34/34 combined.**
This combines original passing cases with focused repair reruns; it is not a
claim of one clean full-suite or hosted run. Debug's dedicated store case also
ran separately. All original failed runs remain in the evidence.

| Gate | Debug and Release result |
| --- | --- |
| `http_windows_store` | 16 command checks per profile: source analysis; normal/O2 JIT; independently built normal/O2 synchronous AOT; forced-GC single-worker synchronous/async JIT; independent normal/O2 async object emission, native linking and execution |
| Real public RSA/ECDSA mTLS | Both identities complete authenticated HTTPS with required client certificates and exact full DER at their respective peers; TLS 1.2 negotiated on this VM |
| Independent owners | Different RSA/ECDSA clients remain live together; peer identities stay distinct; closing RSA leaves ECDSA usable |
| Reuse | `/first` and `/reuse` use the same peer-observed connection for each owner and each executed suite |
| Negative source controls | Missing store, missing thumbprint, no private-key association, malformed selector, anonymous client, wrong selected DER, unrelated CA and wrong hostname all reject with the asserted public error behavior; no negative route reaches HTTP handling |
| P12 capability regression | Correct/wrong supplied passwords both return unsupported/prepare; native fixture separately authenticates/parses its generated P12 and rejects the wrong password using the test-only parser |
| Cleanup and confidentiality | Non-exportable raw-private/PKCS#8 size queries reject; public key, certificate DER, export policy, usage and DACL digest match before/after use; exact owned key/store deletion and absence verified |
| `http_native` | Genuine store RSA positive replaces the unsupported Windows P12 positive; two completed HTTPS requests reuse a connection; anonymous rejection and all earlier transport/H2/proxy/auth/snapshot/streaming controls remain |
| Active Runtime shutdown | Actual Runtime owns a source-style client resource; peer authenticates the selected store identity, returns headers and a held TLS body prefix; Runtime close cancels/drains the transfer, releases native observer references and leaves zero managed handles |
| Public cancellation/GC | Synchronous streaming close/idempotence; async pending request cancelled by client close; one-worker forced GC, root retirement and Runtime shutdown pass |
| Adjacent tests | `http_timeouts`, `http_dns`, `http_wire`, `http_examples`, `http_install`, `http_ci_policy`, `http_curl_objects`, `http_ir`, `http_source`, `http_source_negative`, `http_lsp`, `url_native`, `url_ir`, `url_source`, `url_lsp` pass |

The dedicated source test has **32 command checks across both profiles**, including
compile/link steps; that number is not 32 separate authentication executions.
The generic native peer observes two authenticated connections: one reused for
the completed positive requests, one for the intentionally cancelled Runtime
shutdown case. A failed P12 open is never counted as successful authentication.

Original consolidated runs were Debug 14/16 and Release 14/17. Final focused
reruns of `http_native`, `http_timeouts`, `http_curl_objects` pass 3/3 in each
profile (23.50 s / 19.24 s). Debug dedicated public-store acceptance takes
283.87 s. Original URL/source/example/install successes are retained rather than
repeated after unrelated fixture-only corrections.

## Fixture authority and cleanup

`windows_identity.cpp` is a test executable linked only to supported Windows
crypto/security libraries. It creates serial, isolated test-owned CNG keys and
`CurrentUser\VOID-HTTP-TEST-STORE-<256-bit nonce>` stores. Keys are sign-only,
non-exportable and restricted to the current user/SYSTEM. A persisted owner
marker plus exact public key, certificate DER and provider-binding checks guard
cleanup. An intentionally public-only certificate supplies the unusable-key
negative. No machine store or real identity is used.

The CSR is signed inside CNG and the test CLI signs its public certificate. The
key is never extracted. Export controls allocate no private-key output buffer.
The helper reports collisions and deletion failures as failures. Only the fixture
controller deletes its generated objects after the child Runtime exits. This is
serial test provisioning, not an atomic CNG reservation or crash-recovery design.
The earlier rejected persistence experiments are not revived.

Each final test compares software-KSP key enumeration and all enumerated
CurrentUser logical-store certificate DER inventories before/after. Baselines are
one existing software key and eleven logical stores; these differ from the eight
registry-backed stores enumerated by the older research controller. All final
comparisons match. Own-credential snapshots cover public metadata and association,
not private key-file bytes or every OS/provider counter.

CA/server private files and P12 remain in restricted disposable directories and
are removed. Exact private directory DACLs also satisfy VOID's unchanged native
link staging guard. Neither the ambient VM temp ACL nor security software is
changed. Native key logging remains disabled; its inherited-variable negative
still confirms that no keylog file is created. Public evidence contains no
private keys, P12 content, account SIDs or production credential selectors.

The two credential-owning CTests share a CTest resource lock so parallel tests
cannot invalidate each other's complete inventory comparisons. This is test
serialization; production Runtime/driver ownership is unchanged.

## Original failures and bounded repairs

All failures are retained rather than relabelled as successful final executions:

1. The first CSR fixture used `req -inform`, unsupported by the pinned test CLI.
   Passing DER directly to its `x509 -req -inform DER` path fixes public CSR
   handling. The generated key was removed and the original inventory restored.
2. The VM's ambient temporary root failed VOID's existing native-link ACL guard.
   Adding grants also left explicit access entries in place. The fixture now
   replaces its own directory DACL exactly and supplies that directory only to
   its children. A missing Windows PowerShell security-module attempt is retained;
   direct .NET directory ACL application succeeds. No native staging guard changed.
3. Initial local build setup lacked Git Perl for the test CLI and supplied an
   inappropriate `_DEBUG` define with the release static CRT. Correct environment
   and matching Debug `/Od /Zi` + `/MT` flags fix setup; no SDK source change.
4. Compiler-object policy checks initially ran outside the Visual Studio developer
   environment. Both cases pass in that environment; their assertions are unchanged.
5. The new held-Runtime test first counted its own retained native observer leases
   as leaked handles. Releasing those references after successful Runtime close
   leaves zero handles. Production lifetime code did not need a change.
6. The Digest fixture depended on a second request on the same driver to release
   server TCP backpressure. Pinned Schannel's complete-record send waits
   synchronously (`schannel.c:2046–2087`), so that dependency stalled until the
   peer's three-second guard failed. Native diagnostics record a 2.95 s release
   request and curl 55. A rate-limiter experiment removed that cycle but delayed
   response processing and caused legitimate header expiry; it was removed.
   The final Windows peer releases its own gate after 60 ms, preserving the real
   eight-MiB upload, **20 ms header budget**, five-second total budget and all
   other timeout settings. Complete payload validation, byte count and hold-time
   assertions strengthen the control. Linux/macOS retain their original gate.

The last repair isolates header-deadline semantics from a fixture dependency
cycle. It does not make Schannel's record send nonblocking. Native provider calls
can delay other work and cancellation-to-drain, as the existing contract states;
no hard real-time guarantee or Schannel transport correction is claimed.

## Remaining gates and recommended hosted verification

The complete Windows ASan/UBSan configuration was attempted with leak/halt options
intact. Its native probe fails to link ASan/UBSan runtime symbols; no strict pass
or reduced-instrumentation substitute is claimed. Run the existing supported
Linux strict job. No Linux/macOS execution was available on this Windows VM;
their source/provider/P12 behavior is preserved, with hosted regression still
required. Provider-memory accounting remains [provisional](MEMORY_REVIEW.md), not
a fixed total heap/RSS proof.

After independent source review, Boss should manually run the unchanged speedy
seven-job `ci-link-experiment.yml` workflow, mode `thinlto-cache`, on one reviewed
commit: Ubuntu Debug/Release/Sanitizers, Windows Server 2022 Debug/Release and
macOS ARM64 Debug/Release. Future Boss Git titles must include `[skip ci]`.
Do not substitute this VM's results for those jobs.

Specifically require `http_native`, new `http_windows_store`, every existing
HTTP/URL/source/LSP/example/install/DNS/timeout/policy check, full dependency and
native-link verification, and the existing stack/strict/other regression gates.
Inspect `windows-credentials.json`, exact `mutual-peer.json`, public-store command
and peer receipts, and timeout `digest-peer.json`. Require both key/store cleanup
and before/after inventory equality; upload diagnostics alone are insufficient.
No workflow modification or hosted dispatch is part of this delivery.

For local reproduction, use the existing verified LLVM/Visual Studio environment,
run the unchanged native dependency and test-certificate setup recipes, and build
`voidc`, `voidls`, `void_http_native`, `void_http_source_runner` and the existing
focused HTTP/URL targets in separate Debug/Release directories. The native target
builds its Windows identity helper. Use `/MT`, Debug `/Od /Zi` and
`_ITERATOR_DEBUG_LEVEL=0`; do not define a debug CRT against the official SDK.
Run the named CTests with `--output-on-failure --output-junit`. On this VM, source
native-link regressions additionally use a freshly created private TMP/TEMP root
protected with the same test helper's `protect_directory`; no ambient ACL change
is necessary. Raw build/test failures remain under ignored
`.toolchains/schannel-acceptance/`, with safe observations copied into the public
receipt.

## Exact modified-file inventory

Twenty-three intended paths; all remain unstaged:

- `CMakeLists.txt`.
- `src/runtime/http.cpp`, `src/runtime/http_abi.cpp`.
- `stdlib/net/http.void` (availability/ownership documentation only).
- `tests/net/http/fixture.py`, `native.cpp`, `runtime.cpp`, `source_runner.cpp`,
  `timeouts.cpp`, `timeouts.py`.
- `tests/net/http/windows_identity.cpp`, `windows_identity.py`, `windows_mtls.py`,
  `windows_store.void`.
- `docs/README.md`.
- `docs/net/http/README.md`, `NATIVE_PLATFORMS.md`, `DESIGN.md`, `HANDOFF.md`,
  `CURRENT_ENGINEERING.md`, `CI_HANDOFF.md`, `WINDOWS_SCHANNEL_ACCEPTANCE.md`.
- `docs/net/http/evidence/windows-schannel-acceptance-20261103.json`.

The two pre-existing `docs/json/evidence/implementation/readonly-{Debug,Release}.json`
changes are preserved and excluded from this inventory. Prior evidence, root
README, dependencies, LLVM, workflows, frontend/IR/backend, scheduler and GC are
unchanged. **Stop for independent Golden Wrench review; Boss owns Git and CI.**
