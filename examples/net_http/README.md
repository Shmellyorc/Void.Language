# HTTP examples

Build with the verified dependency closure described in
[the HTTP engineering guide](../../docs/net/http/README.md). No HTTP operation
uses a curl subprocess. `VOID_HTTP_URL` supplies the endpoint for these examples.

| File | Demonstrates |
| --- | --- |
| `api_json.void` | Ordered URL/query construction, synchronous JSON API, distinct URL/HTTP/JSON errors, non-2xx inspection and retained body. The endpoint returns `{"message":"hello"}`. |
| `authenticated_post.void` | JSON encoding and verified HTTPS bearer POST. Set `VOID_HTTP_TOKEN`; optionally set `VOID_HTTP_CA` to a replacement PEM root bundle. Tokens are never printed. |
| `files.void` | Native download, upload and file multipart, separate IO causes and partial progress. Uses a service exposing `/large?1048576`, `/echo` and `/multipart`. Creates/truncates `http-example-payload.bin` in the current directory. |
| `streaming_async.void` | Concurrent requests, repeated await, bounded pull reads and deterministic transfer/client closure. Uses `/plain` and `/large?8388608`. |
| `cancellation.void` | Cancels a pending `/slow` request while a sibling `/plain` request progresses. Structured task cancellation propagates to the explicit host. |
| `host.cpp` | Existing compiler/JIT/Runtime async-root adapter, with one IO worker. No application scheduler or source callback is introduced. |

For synchronous examples:

```sh
VOID_HTTP_URL=https://your-service.example/document build/llvm23-release/voidc run examples/net_http/api_json.void
build/llvm23-release/voidc build examples/net_http/api_json.void -o /tmp/http-json
VOID_HTTP_URL=https://your-service.example/document /tmp/http-json
```

For explicit async hosting:

```sh
cmake --build build/llvm23-release --target void_http_example_host
VOID_HTTP_URL=http://127.0.0.1:8787 build/llvm23-release/void_http_example_host examples/net_http/streaming_async.void
```

The cancellation example exits 1 with `Async task cancelled` after structured
cleanup. Cancellation is a task outcome, not an automatically fabricated
`http.error`. A cancelled mutating request can have an unknown remote outcome;
HTTP close/cancel never promises server-side rollback.

The local test creates its own loopback peers and test certificate, runs all five
sources, independently links the three synchronous examples, checks malformed
JSON separately and verifies the cancellation outcome:

```sh
ctest --test-dir build/llvm23-release -R '^http_examples$' --output-on-failure
```

Async entrypoints use the explicit host above or the established emitted async
AOT adapter. An ordinary synchronous `voidc run` does not activate an async root.
