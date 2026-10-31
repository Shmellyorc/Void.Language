# Tracked curl provider integration

Boss's workflow correction authorizes direct filesystem implementation of the
three cookie enumeration corrections and the exact subsequently approved HTTP/2
window correction. The authoritative source is
`third_party/curl/`, suitable for Git tracking; Keyboard has not staged it.
`.toolchains/` contains disposable tools, dependency downloads and build outputs.
Boss need not apply patches, copy files, modify dependencies or consume another ZIP.

The retained official curl 8.23.0 source has archive SHA-256
`8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5`.
The exact three-file correction has review-diff SHA-256
`6a558dc87faec9bbba68056ee79903dff0c05b5e61ba7b08a513b2e21b817920`.
`VOID-PROVENANCE.json` records every retained original file digest and the corrected
digests. `COPYING` and source notices are preserved. Only `lib/cookie.c`,
`lib/cookie.h`, `lib/getinfo.c`, and `lib/http2.c` differ from their retained upstream originals.
The vendored subset includes the library, public headers, CMake, docs and scripts;
it intentionally omits the command-line executable, upstream tests and autotools
support directories. Its supported build is the library-only CMake configuration.

`cmake/CurlProvider.cmake` verifies that inventory before configuring the provider.
VOID's root CMake selects the full HTTP profile, with an explicit pinned transitive
prefix and no ambient libcurl fallback. Regression-only COOKIE/NO_COOKIES/NO_HTTP
profiles live in the isolated native test project. They do not qualify the full
HTTP transport. curl's `BUILD_TESTING` setting is scoped to its subdirectory;
VOID's own tests remain enabled.

Clean native Linux builds of the checked-in source passed:

| Configuration | Receipt |
| --- | --- |
| Release | [Commands/results](evidence/vendored-Release-20261030T142520417346Z/commands.json) |
| Debug | [Commands/results](evidence/vendored-Debug-20261030T142521600374Z/commands.json) |
| Strict ASan/UBSan/LSan | [Commands/results](evidence/vendored-strict-20261030T142522798761Z/commands.json) |
| Disabled cookies, strict | [Commands/results](evidence/vendored-no-cookies-20261030T142632076789Z/commands.json) |
| Disabled HTTP, strict | [Commands/results](evidence/vendored-no-http-20261030T142633300689Z/commands.json) |
| Second clean Release directory | [Commands/results](evidence/vendored-Release-20261030T142634540925Z/commands.json) |

Each cookie-enabled run checks 12 deterministic allocation failures, successful
empty and real-cookie states, partial-list cleanup and a host initialization
control. All probe processes report enforced soft/hard 1 MiB stacks. The two
independently configured Release directories produced byte-identical `libcurl.a`:
SHA-256 `58108da54e1fac0cb5a09b9c5ded1231d19359acdd94ac839a1e09d8766a151e`.
These are the isolated cookie profile, not complete HTTP/TLS/h2/PSL acceptance.

The first disabled-feature runner attempts failed their JSON assertion despite
successful native probes: that probe reports code/list/stack in one row. The
runner now checks that actual format. The failed receipts remain preserved.

Reproduce directly from the repository (no patch application):

```sh
python3 tools/verify_curl_source.py
python3 tests/net/http/verify_provider.py --profile Release
python3 tests/net/http/verify_provider.py --profile Debug
python3 tests/net/http/verify_provider.py --profile strict
python3 tests/net/http/verify_provider.py --profile no-cookies
python3 tests/net/http/verify_provider.py --profile no-http
```

`third_party/http-versions.json` pins transitive releases and official archive
hashes. `tools/setup_http_deps.py` builds them outside the authoritative curl tree.
The initial Linux recipe uses replaceable shared GNU IDN/Unicode libraries and
installs their unmodified corresponding source archives and notices. The remaining
dependencies are static. Public suffix data is separately pinned in
`third_party/publicsuffix/`. Full provider integration, native trust and install
acceptance are still being verified. Windows/macOS execution is not claimed.

The OpenSSL build disables its key-log feature. curl's private key-log hooks are
replaced by VOID's no-output build implementation, leaving the reviewed vendored
files unchanged. This prevents inherited `SSLKEYLOGFILE` from silently writing
TLS secrets; runtime tests must still exercise that policy.

The [upstream follow-up](PATCH_REVIEW.md#upstream-follow-up-record) is prepared
locally and has not been sent. No upstream acceptance or independent Wrench
source approval is claimed. The complete [authorized HTTP mission](AUTHORIZATION.md)
continues; these cookie checks are not a full-module completion claim.

The initial full configuration exposed unwanted ambient zlib/zstd linkage because
upstream tri-state cache options replaced function-local values. The helper now
sets those provider cache keys explicitly to OFF. The table above is the final
rerun after that correction; initial configuration/build logs remain preserved.

## Latest clean full-provider reproduction

After the exact approved HTTP/2 correction, fresh Release, Debug and strict full
provider builds pass. Two independent Release directories produce byte-identical
`libcurl.a`, SHA-256
`1526915123669110951425c89d1cf51e218aeb172f413149149094684b2c5837`.
Receipts: [Release 1](evidence/vendored-full-Release-20261031T032400962158Z/commands.json),
[Release 2](evidence/vendored-full-Release-20261031T032429933889Z/commands.json),
[Debug](evidence/vendored-full-Debug-20261031T032541548470Z/commands.json),
[strict](evidence/vendored-full-strict-20261031T032402162232Z/commands.json).
The current native provider probes are maintained under `tests/net/http/provider/`;
original historical evidence files remain untouched. Fresh strict disabled-cookie
and disabled-HTTP configurations also pass. [H2 exact source review](HTTP2_WINDOW_REVIEW.md)
records the second patch checksum, upstream follow-up draft and packet-credit proof.
All four approved source corrections are authoritative; no production patch is applied.
