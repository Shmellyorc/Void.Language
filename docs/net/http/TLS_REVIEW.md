# TLS dependency decision for Golden Wrench

30 October 2026. Architecture remains libcurl 8.23.0 multi, nghttp2, c-ares,
verified TLS and native platform trust. Boss approved this exact Linux TLS pin through the in-chat review response.
Production integration is now underway. This is not complete HTTP acceptance or
a release handoff.

## Established cause

The OpenSSL 3.5.9 failure is upstream function-type undefined behavior. A typed
`pd_free(OSSL_PROPERTY_DEFINITION *)` is called as `void (*)(void *)` by the
generic stack implementation. The unchanged crypto-only reproducer fails without
curl or VOID. Sixteen C11/C23, O0/O2 and strict-aliasing controls distinguish the
incompatible call from a correctly typed adapter. This is not evidence of an
LLVM 23 bug. Original failure evidence and upstream reports remain in
[DEPENDENCY_CONFLICT.md](DEPENDENCY_CONFLICT.md).

An individual cast or `pd_free` repair does not correct the broader stack/hash
callback convention. Upstream's proposed broad repair is closed and unmerged.
No sanitizer suppression, recovery, ignorelist, reduced instrumentation or local
OpenSSL fork has been introduced.

## Concrete candidate

Use unmodified **AWS-LC 5.12.0** for the Linux OpenSSL-compatible TLS dependency:

| Identity | Exact value |
| --- | --- |
| Revision | `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8` |
| Archive | `https://codeload.github.com/aws/aws-lc/tar.gz/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8` |
| SHA-256 | `1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68` |
| Version observed through curl | `AWS-LC/5.12.0` |
| Source changes | None; clean extraction, `ENABLE_SOURCE_MODIFICATION=OFF` |
| License/notice | Original combined LICENSE and NOTICE copied into every review receipt |

Curl 8.23.0 already recognizes this backend and its static C++ link obligation.
The candidate uses the same curl source, exact cookie correction, c-ares,
nghttp2, PSL, IDN, compression and Runtime implementation as production. Link
receipts verify that original OpenSSL headers/libraries are not mixed into the
candidate. `third_party/curl/` remains authoritative. Production `third_party/http-versions.json` now selects the approved AWS-LC pin.
Fresh version-qualified prefixes preserve the original OpenSSL builds and failure
binaries. The [prior pin manifest](evidence/http-versions-before-aws-lc.json) is
retained. Fresh production dependency builds pass Debug, Release and strict;
whole-provider/Runtime integration is being verified.

Reproduction is implemented in [the review harness](../../../tools/review_http_tls.py)
and [isolated CMake project](../../../tests/net/http/tls-review/CMakeLists.txt).
Candidate sources, prefixes and build outputs are disposable. This is not a
production patch-application workflow and requires no manual copying or ZIP.

## Actual clean-build results

| Profile | Result |
| --- | --- |
| Release | [PASS](evidence/tls-review-Release-20261030T174355197157Z/commands.json) |
| Debug | [PASS](evidence/tls-review-Debug-20261030T174355207403Z/commands.json) |
| Strict | [PASS](evidence/tls-review-strict-20261030T174355198528Z/commands.json) |

All three enforce soft/hard 1 MiB stacks for cookie, URL and actual network
probes. Strict uses full `-fsanitize=address,undefined` on TLS, curl, Runtime and
the native integration probe, plus frame pointers, leak detection and halt on
error. No checker was disabled. All twelve injected cookie census failures return
checked OOM; empty, partial cleanup and host initialization controls pass.

Each clean profile also passes real HTTP/1.1/TLS, real HTTP/2 ALPN, two streams on
one allowed connection, DATA flow control, required-h2 rejection, binary/duplicate
headers, gzip/deflate, streaming and duplex, redirects, session cookies, timeouts,
Basic/Digest/Bearer, CA replacement, PEM and PKCS12 mutual TLS, HTTP/CONNECT/HTTPS
proxies, proxy Basic/Digest rejection/isolation, SOCKS local/remote DNS, native
bulk IO and actual Runtime async completion. A poisoned `SSLKEYLOGFILE` creates
no file. Runtime tests include 128 MiB pulls, a retained repeatedly awaited task,
a single-worker file producer and activation-exit transfer cancellation.

## Decision and limits

Decision: Boss approved this exact unmodified maintained backend pin for Linux.
Integrate its production recipe, license/install closure and feature verification. Preserve the original OpenSSL failure and review record. This does
not authorize weakening any HTTP or sanitizer requirement.

Native Windows/macOS implementations and execution remain outstanding. Their
approved native trust requirements are unchanged; these Linux receipts do not
establish Schannel/SecTrust or Windows CRT compatibility. The complete source
HTTP API, multipart file production, provider memory audit, malformed-wire matrix
and full JIT/O2/AOT/install/LSP acceptance are unfinished. The 512 MiB reservation
is not a certified RSS ceiling. Candidate success is not complete HTTP acceptance.
