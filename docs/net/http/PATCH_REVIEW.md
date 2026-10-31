# Pinned curl cookie-enumeration correction: exact review packet

30 October 2026. Golden Wrench selected Option 1. Boss subsequently authorized
direct implementation in the Git-trackable `third_party/curl/` tree and CMake
integration. Those three corrections are now applied directly there. Independent
Wrench review remains separate from implementation; it is not a repeated request
for Boss to apply a patch, move files or approve another ZIP.

The exact [three-file patch](evidence/curl-8.23.0-cookie-oom-candidate.patch) was
also pasted in full in chat, so review does not require access to a local path.

| Input | Immutable identity |
| --- | --- |
| Official source | curl **8.23.0**, `https://curl.se/download/curl-8.23.0.tar.xz` |
| Source SHA-256 | `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5` |
| Patch SHA-256 | `6a558dc87faec9bbba68056ee79903dff0c05b5e61ba7b08a513b2e21b817920` |
| Signed archive | [GPG verification](evidence/curl-archive-signature.log), fingerprint `27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2`; original trust warning preserved |
| License | Original [curl COPYING](evidence/curl-8.23.0-COPYING.txt), included with the review packet |

Production builds consume `third_party/curl/` directly through
`cmake/CurlProvider.cmake`. `tools/verify_curl_source.py` verifies all 1,586 retained
release files against the manifest, with exactly the three recorded corrections.
No production patch-application workflow exists. The earlier reproduction driver
and its patch command remain historical defect/review evidence only. Current
clean-build checks are `tests/net/http/verify_provider.py`.

The private enumeration functions now return a CURLcode and write their list
through an output parameter. Output is NULL before work starts. Both allocation
failure paths free the partial list and return `CURLE_OUT_OF_MEMORY`; an empty
or expired jar still returns `CURLE_OK` with NULL. The existing share lock/unlock
continues to surround enumeration and error returns. `getinfo_slist` propagates
the code directly. Disabled-cookie/disabled-HTTP builds use a no-allocation macro
that sets NULL and returns success. No public function signature, structure,
header, symbol name, calling convention or cookie grammar changes.

Actual native evidence:

- [Release](evidence/cookie-Release-20261030T122547143010Z/commands.json),
  [Debug](evidence/cookie-Debug-20261030T122727912022Z/commands.json) and
  [strict ASan/UBSan/LSan](evidence/cookie-strict-20261030T122727909757Z/commands.json):
  36 checked allocation-failure outcomes for the candidate, 36 original ambiguous
  outcomes retained, 48 valid-state controls and six embedding controls.
- [Strict disabled-cookie and disabled-HTTP builds](evidence/cookie-disabled-20261030T123553943957Z/commands.json):
  both build and return `CURLE_OK`/NULL, including replacing a non-NULL output
  sentinel. Both native probe stacks report actual soft/hard 1,048,576-byte limits.
- [Original source identities](evidence/cookie-provider-source-identity.json),
  signature/key, original failed harness attempts and corrected receipts remain
  preserved. The error-path test uses a deliberately small provider feature set;
  it is not full HTTP/TLS/h2/DNS/PSL acceptance or native Windows/macOS acceptance.
- [Public header/symbol comparison](evidence/cookie-patch-abi-comparison.json):
  only the three named private source files differ; all 12 public headers and
  the 100 `curl_` public symbol names are identical in the same-feature Linux
  Release static builds. This is not native Windows/macOS ABI acceptance.

The corrected source is now selected by VOID CMake. The full transitive
link/license/trust/native acceptance is still engineering work in progress.
The [vendoring record](VENDORED_PROVIDER.md) distinguishes the clean vendored-source
regressions from those separate transport gates.

## Upstream follow-up record

Status: **prepared locally; no upstream issue, PR, email or other message sent**.
No upstream acceptance, issue number or release inclusion is claimed. Sending
an upstream report requires explicit owner instructions under the session's
external-message rules.

The report should contain the exact version/source hash, the distinction between
valid empty and allocation-failed enumeration, the public getinfo return behavior,
the small patch and the deterministic allocator probe. Request an upstream
error-propagation fix and regression tests for both allocations/partial-list
cleanup, successful emptiness and disabled builds. After upstream inclusion,
record its immutable commit/release and compare behavior before replacing the
downstream correction. Keep the original source/patch identities and failed
observations. No provider update or patch removal is automatic.

This record is separate from the full [HTTP engineering mission](AUTHORIZATION.md)
and [current implementation status](IMPLEMENTATION.md). No HTTP contract weakening
or independent source approval is inferred from the Option 1 ruling.
