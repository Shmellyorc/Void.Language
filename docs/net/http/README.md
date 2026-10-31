# URL and HTTP engineering

`use url` supplies HTTP(S) URL parsing/resolution and percent/query codecs.
`use net/http` supplies explicit clients, verified HTTP/1.1 and HTTP/2,
buffered responses, streaming, native file transfers, auth, cookies and proxies.
The complete public contract is in [DESIGN.md](DESIGN.md); declarations and
parameter/result documentation live in `stdlib/url.void` and `stdlib/net/http.void`.

**Architecture is approved. Full HTTP acceptance remains pending.**
[CURRENT_ENGINEERING.md](CURRENT_ENGINEERING.md) records implemented behavior,
retained failures and current tests. [ROADMAP.md](ROADMAP.md) defines acceptance;
[AUTHORIZATION.md](AUTHORIZATION.md) preserves Boss decisions. Native Windows/macOS
recipes are present; their execution acceptance, [provider-memory proof](MEMORY_REVIEW.md)
must be resolved before claiming complete portable delivery.
The [Golden Wrench handoff](HANDOFF.md) records exact local results and review gates.
See [native recipes and exact verification gaps](NATIVE_PLATFORMS.md).

## Local Linux build

Use the repository's exact LLVM/Clang/LLD 23.1.4 toolchain and existing CMake
configuration. Build dependencies explicitly; there is no system-libcurl fallback
or application-time downloader:

```sh
python3 tools/setup_http_deps.py --profile Release
cmake -S . -B build/llvm23-release \
  -DCMAKE_C_COMPILER="$PWD/.toolchains/llvm-23.1.4/bin/clang" \
  -DCMAKE_CXX_COMPILER="$PWD/.toolchains/llvm-23.1.4/bin/clang++" \
  -DLLVM_DIR="$PWD/.toolchains/llvm-23.1.4/lib/cmake/llvm" \
  -DVOID_LINKER="$PWD/.toolchains/llvm-23.1.4/bin/ld.lld" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/llvm23-release --parallel 4
```

Authoritative curl 8.23.0 source is `third_party/curl/`. Its exact inventory
contains the three approved cookie corrections and approved HTTP/2 window fix;
`tools/verify_curl_source.py` verifies all 1,586 files. `.toolchains/http/` holds
disposable archives/builds/prefixes. Production never applies a patch to build.
All dependency versions, archive hashes and Linux TLS revision are in
`third_party/http-versions.json`; CMake verifies the installed dependency prefix.
Notices and replaceable GNU-library corresponding source accompany installation.

The strict profile uses `--profile strict`, a `RelWithDebInfo` CMake build and
`-DVOID_SANITIZERS=ON`. Keep ASan/UBSan/leak checks enabled and use a 1 MiB stack:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
bash -c 'ulimit -s 1024; ctest --test-dir build/llvm23-strict -R "^(http_|url_)" --output-on-failure'
```

## Application use and limits

[Runnable examples](../../../examples/net_http/README.md) cover JSON APIs, URL
construction, authenticated POST, bulk files, async streaming and cancellation.
`open_client` snapshots configuration; `start` snapshots its request before
native registration. Ordinary list arguments retain normal sharing semantics
until that snapshot. Async functions snapshot when their bodies execute.

Clients and transfers are managed resources; use `with` or explicit close.
Transfers created in an async activation participate in its structured lifetime.
Task handles retained by the application remain repeatably awaitable; unreachable
completed results can retire after precise collection during sustained streaming.
File transfers borrow existing IO files at the current cursor, preserve ownership
and leave partial progress visible after failure. They do not create a source
task or managed byte array for every block.

TLS verifies chain and hostname with a minimum of TLS 1.2. Default Linux trust uses
distribution root locations. Explicit CA/certificate/key/P12 files are regular,
combined at most 1 MiB and snapshotted at client creation. Custom roots replace
system roots for both origin and HTTPS proxy. The pinned Linux backend has a
100 KiB peer certificate-message default. Native callbacks/parser work can delay
cancellation-to-drain; no hard real-time return guarantee or total RSS ceiling is
claimed. The 512 MiB native admission budget remains provisional pending the
complete provider audit.

DNS uses c-ares with finite cache capacity/TTL; capacity eviction can precede TTL.
Response queues use fixed bounded rings. Headers, trailers, cookies, snapshots,
decoding and response publication have separate finite limits described in the
contract. Encoded and decoded byte limits are independent. Unsupported or stacked
content encoding fails unless raw mode is explicitly selected. Text conversion is
strict UTF-8; bytes remain inspectable.

Proxy use is direct by default. Environment proxy settings are an explicit
snapshot choice. HTTP_PROXY is ignored, including its case aliases on Windows;
use ALL_PROXY or explicit routing for plain HTTP there. Windows environment
values use the existing UTF-16/UTF-8 OS kernel. Credentials require HTTPS; cross-origin redirects with configured
credentials or sensitive fields are denied. Permitted cross-origin GET/HEAD hops
drop caller fields. HTTPS downgrade and silent consumed-stream/file replay are
denied. There are no hidden request retries.

HTTP/3, h2c, WebSockets, HTTP servers, browser SSO/OAuth login flows, persistent
cookie storage, arbitrary URI schemes and public raw sockets are excluded.

## Measurements

`void_http_performance` and `tests/net/http/performance.py` are opt-in local
Release measurements for cold/warm JSON requests, one versus four concurrent
requests, observed peer connections, partial-response close, source check/JIT/AOT
and unused-import package cost. They impose no arbitrary throughput target.
[Actual receipts](evidence/http-performance-release/measurements.json) retain
versions, sample ranges and concurrent validation load. Multi-GiB source and
native bulk proofs are separate from these small-request latency samples.

The one-worker IO comparison is `void_http_io_performance` plus
`tests/net/http/io_performance.py`. It exercises the same async file loop with
HTTP absent, idle and intentionally paused. [Baseline comparison and repaired
bookkeeping cost](PERFORMANCE.md) retain all measurements and original regressions.
