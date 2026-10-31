# Pinned curl HTTP/2 window correction: exact review

30 October 2026. **Exact correction approved and integrated directly into
authoritative source. Production verification passes 17/17 focused checks in each profile.** Boss's exact
reply was “Approve this exact additional pinned correction (Recommended)”. The
fourth correction in `lib/http2.c` preserves the three existing cookie changes.
Approval applies to patch SHA-256
`f109b12c2c5b327fb9b2da2b9d302cb111d936097b98ace830ea886db7d4f1a2`.

## Concrete defect and preserved evidence

The admitted native reservation assumes curl's pinned 10 MiB HTTP/2 stream
window. In curl 8.23.0, `cf_h2_update_local_win` increases that window with both
`nghttp2_session_set_local_window_size` and an additional
`nghttp2_submit_window_update` of the same delta. The first API already queues
window credit. The peer records **two identical increments** for each paused
stream, permitting approximately 20 MiB in flight. Curl's pause buffer capacity
then grows to 32 MiB per stream instead of 16 MiB.

The native test reproduced the budget failure without changing the provider:
three paused 512 MiB responses, four small application pulls and fast sibling
requests on the sole HTTP/2 connection. Live requested allocations increased
from 1,227,131 to 102,577,468 bytes, exceeding the 71,303,366-byte admitted native
reservation. Only three 64 KiB application queues were full. The large fixture
payload is generated incrementally; it is not stored in the client or fixture.

- [Original failed live-allocation proof](evidence/http-h2-resume-before/native.stdout).
- [Original peer window-credit trace](evidence/http-h2-resume-before/commands.json).
- [Initial failed RSS-only probe](evidence/http-h2-paused-strict-rss-before/commands.json).

An early regression assertion conflated RSS with live provider allocations. The
strict probe now reads ASan's public live-allocation counter and checks actual
requested bytes against admission. RSS remains reported separately. Leak
detection, quarantine, ASan/UBSan and halt-on-error remain enabled. The live-byte
probe confirmed a real budget violation, not just instrumentation overhead.

## Exact candidate contents

Remove the redundant increment and its corresponding error branch. Retain the
absolute setter, its error handling, pause/resume logic and all public APIs.
No protocol, cookie, trust, queue, body or admission limit is relaxed.

```diff
--- a/lib/http2.c
+++ b/lib/http2.c
@@ -343,13 +343,6 @@
       if(rv) {
         failf(data, "[%d] nghttp2 set_local_window_size(%d) failed: "
               "%s(%d)", stream->id, dwsize, nghttp2_strerror(rv), rv);
-        return CURLE_HTTP2;
-      }
-      rv = nghttp2_submit_window_update(ctx->h2, NGHTTP2_FLAG_NONE,
-                                        stream->id, dwsize - wsize);
-      if(rv) {
-        failf(data, "[%d] nghttp2_submit_window_update() failed: "
-              "%s(%d)", stream->id, nghttp2_strerror(rv), rv);
         return CURLE_HTTP2;
       }
       stream->local_window_size = dwsize;
```

The resulting setter error branch still returns `CURLE_HTTP2`; the diff aligns
identical return/brace lines across the removed block. The only changed archive
member is independently checked as `http2.c.o`.

| Identity | SHA-256 |
| --- | --- |
| curl 8.23.0 signed source archive | `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5` |
| Original `lib/http2.c` | `6a9d36f79b772b18cea2d17707fc32fa5dd49493d4bc4e94363bc5cd8ef91fc9` |
| Corrected `lib/http2.c` | `6237bdfb17bcb6670be046520ab9170a21ef7e946e9149df17f7e56cf6313914` |
| [Exact review diff](evidence/curl-8.23.0-http2-window-candidate.patch) | `f109b12c2c5b327fb9b2da2b9d302cb111d936097b98ace830ea886db7d4f1a2` |

## Verification and reproduction

| Configuration | Result | Evidence |
| --- | --- | --- |
| Release native | PASS, including cookie count/byte bounds, double TLS required-h2, three paused 512 MiB responses, four small resumes and fast siblings on one connection | [receipt](evidence/http2-window-review-release-20261030T220108883803Z/commands.json) |
| Debug native | PASS, including cookie count/byte bounds, double TLS required-h2, three paused 512 MiB responses, four small resumes and fast siblings on one connection | [receipt](evidence/http2-window-review-debug-20261030T220108881738Z/commands.json) |
| Strict native | PASS, including cookie count/byte bounds, double TLS required-h2, three paused 512 MiB responses, four small resumes and fast siblings on one connection | [receipt](evidence/http2-window-review-strict-20261030T220108891008Z/commands.json) |
| Release source | 15/15 analysis, normal/O2 JIT, sync AOT and independently linked async AOT process checks | [receipt](evidence/http2-window-review-release-20261030T220108883803Z/source/commands.json) |
| Debug source | 15/15 analysis, normal/O2 JIT, sync AOT and independently linked async AOT process checks | [receipt](evidence/http2-window-review-debug-20261030T220108881738Z/source/commands.json) |
| Strict source | 15/15 analysis, normal/O2 JIT, sync AOT and independently linked async AOT process checks | [receipt](evidence/http2-window-review-strict-20261030T215721483292Z/source/commands.json) |

All native/execution checks run with 1 MiB stacks. Strict uses
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, with the full strict dependency
closure. No sanitizer exclusion, quarantine adjustment, allocator replacement or
provider feature disabling is used.

The candidate strict live-byte range is 1,227,173–52,270,438 bytes; its increase
fits the same 71,303,366-byte reservation. It remains unchanged over the four
small resume cycles. The peer now receives one initial increment per stream.
Release RSS is 15,176–48,180 KiB and Debug 15,348–48,272 KiB across the probe;
strict RSS is 561,092–669,072 KiB, reported as instrumentation/host overhead rather
than a production RSS guarantee.

`tools/review_http2_window.py --profile release|debug|strict` recompiles the
single candidate source with the matching production compile command, verifies
that only `http2.c.o` differs in a disposable archive, independently links the
actual native test/compiler/source runner, and substitutes that archive in its
local AOT manifest. It never applies a patch or changes production source/pins.
`--native-only` is explicitly a focused retest, not a source-acceptance claim.
All three unchanged dependency prefixes have separately passed verification.

## Integration after approval and upstream record

Following approval, Keyboard edited `third_party/curl/lib/http2.c` directly and extended
the authoritative correction inventory/checksums to this exact fourth change.
Focused production integration precedes consolidated acceptance. No Boss
patch application, dependency copying or separate production patch workflow is
required. The curl license, original signed archive and existing cookie patch
remain preserved. No Git staging, commit, branch, push or hosted CI is authorized.

Proposed upstream subject: `http2: avoid duplicate window increment after setting
local window size`. The local source analysis, exact diff, peer trace and
reproducer are prepared; no upstream issue/message has been sent. This is the
upstream follow-up record, not a claim of upstream acceptance or a future release.

Primary API references: [absolute window setter](https://nghttp2.org/documentation/nghttp2_session_set_local_window_size.html),
[delta window update](https://nghttp2.org/documentation/nghttp2_submit_window_update.html),
and [sanitizer live-allocation interface](https://github.com/llvm/llvm-project/blob/main/compiler-rt/include/sanitizer/allocator_interface.h).

This fixes the isolated HTTP/2 window gate only. DNS/cache and TLS-material
allocation audit, native Windows/macOS recipes/acceptance, remaining wire cases,
complete declaration docs and consolidated HTTP acceptance are still pending.
The full engineering mission is not complete.


## Approved production results

The exact correction is integrated directly in the authoritative tree. Focused
production integration passes [Release 17/17](evidence/http2-approved-tests-release.log),
[Debug 17/17](evidence/http2-approved-tests-debug.log) and
[strict 17/17](evidence/http2-approved-tests-strict.log), including the previously
failing live-reservation and peer-credit test. These supersede the review-only
status; the original failed and isolated-candidate receipts remain intact.
