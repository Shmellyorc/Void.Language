# OpenSSL strict-sanitizer conflict and approved resolution

Boss subsequently approved unmodified AWS-LC 5.12.0 for the Linux TLS dependency.
It is now production-selected and the focused strict integration passes 8/8.
[TLS_REVIEW.md](TLS_REVIEW.md) records the exact pin and clean-build evidence.
The original OpenSSL failure below remains intact; no LLVM/checker workaround
was introduced.

## Original OpenSSL investigation (historical; Linux production pin replaced)

The checked-in curl cookie correction passes its Debug, Release, strict and
disabled-feature regressions. The original different dependency failure was:
the full Linux HTTP profile with OpenSSL 3.5.9 stops during initialization under
the required Clang 23.1.4 ASan/UBSan/LSan configuration.

[Original full-provider failure](evidence/vendored-full-strict-20261030T142846283644Z/commands.json)
and [full diagnostic](evidence/vendored-full-strict-20261030T142846283644Z/probe.stderr)
show `crypto/stack/stack.c:440` calling `pd_free` through an incompatible function
pointer type. `property_parse.c:301` defines the typed callback; its typed stack
wrapper casts it to the generic stack free callback. The diagnostic occurs during
`RAND_status` invoked by curl initialization, before cookie tests or networking.
It is not a failure of the reviewed curl enumeration correction.

The [minimal C reproducer](evidence/openssl_strict_init_probe.c) links only the
pinned `libcrypto.a`, libc/platform dependencies and sanitizers. It links neither
curl nor VOID Runtime. It calls `RAND_status`, then cleanup. The
[isolated receipt](evidence/openssl-strict-isolation.json) records binary/library
hashes, exit 1, settings and actual enforced soft/hard 1 MiB stacks. The
[isolated trace](evidence/openssl-strict-isolation.stderr) reproduces the same
diagnostic. The original official archive SHA-256 is
`603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`;
its pin, commands and original source are independently identified in
`evidence/http-versions-before-aws-lc.json` and the original dependency acquisition/build receipts.

Exact instrumentation is `-fsanitize=address,undefined -fno-omit-frame-pointer`,
with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No checker was disabled, no
ignorelist or recovery policy was added, and this run is not reported as passing.
No OpenSSL source modification, backend substitution or reduced sanitizer contract
has been selected. Fixing only the first reported callback is not evidence that
all typed stack/copy/comparator paths are correct.

This remains an acceptance gate. Boss has authorized continued investigation and unrelated HTTP implementation. The authorization's stop rule applies only when a genuinely unresolved decision prevents correct completion:

> Stop only if an unresolved safety/semantic conflict, incompatible dependency obligation or genuinely necessary owner decision prevents correct completion.

Boss's direct-vendoring instruction for the three curl files remains implemented.
No manual patching or ZIP is requested. Independent source review must resolve a
compatible fully checked TLS dependency or an auditable correction before the
complete HTTP mission can meet strict acceptance. No upstream message was sent.

Independent URL/compiler/Runtime/install work continued after discovery. Current
native evidence and remaining scope are in [CURRENT_ENGINEERING.md](CURRENT_ENGINEERING.md).
No complete HTTP handoff, native Windows/macOS acceptance, release approval or
whole-provider memory/performance certification is implied.

## Cause isolation during authorized continuation

The defect originates in upstream OpenSSL's generic callback convention. The
function definition is `pd_free(OSSL_PROPERTY_DEFINITION *)`, but
`OPENSSL_sk_pop_free` invokes it as `void (*)(void *)`. Casting a function pointer
is allowed; calling through the incompatible type is undefined behavior. The
failure is independent of curl, VOID, stack size and native scheduling. There is
no evidence here of an LLVM 23 instrumentation defect.

The [callback control source](evidence/callback_type_probe.c) and
[16-run receipt](evidence/callback-type-controls/results.json) exercise C11/C23,
O0/O2 and default/disabled strict aliasing under the unchanged full sanitizer set.
All eight incompatible calls fail; all eight correctly typed adapter calls pass.
Each process enforces and records soft/hard 1 MiB stack limits. Disabling strict
aliasing does not fix an incompatible function call. A distro-Clang comparison
could not link because that installation lacks sanitizer runtimes; its retained
link diagnostic is not a pass or evidence against either compiler.

This matches upstream [issue 22896](https://github.com/openssl/openssl/issues/22896)
and the exact `pd_free` report in
[issue 26210](https://github.com/openssl/openssl/issues/26210). The proposed
[callback repair PR 22917](https://github.com/openssl/openssl/pull/22917) is closed
and unmerged. The pattern also occurs in typed stack/hash comparator, copy and
free callbacks. Repairing only `pd_free` would leave the contract unverified.
Captured upstream responses are retained beside the reproducer. No broad local
OpenSSL fork, checker suppression, recovery or reduced instrumentation was added.

## Isolated TLS candidate review

An unmodified AWS-LC v5.12.0 candidate is being evaluated in disposable review
storage. This is an OpenSSL-compatible backend already recognized by the pinned
curl CMake configuration, not a replacement transport. The authoritative
`third_party/http-versions.json` OpenSSL pin and production dependency manifest
remain unchanged. Candidate provenance, immutable source commit and archive
checksum are in [the acquisition receipt](evidence/tls-review-aws-lc-source.json).
The same crypto-only RAND/cleanup reproducer passes with full instrumentation,
halt-on-error, leak detection and a 1 MiB stack
([receipt](evidence/tls-review-aws-lc-probe.json)).

The candidate is not accepted on that initialization result alone. Actual curl
TLS, ALPN/HTTP2, authentication, trust, PEM/PKCS12 identity, cancellation, ABI,
licensing, memory behavior and platform integration require review. The isolated
curl build initially selected original OpenSSL headers from another dependency's
common include directory while linking candidate libraries. The retained failed
link is a review-harness configuration error. The corrected isolated target
puts candidate headers first; no production source or dependency pin was changed.

Golden Wrench must review a complete candidate record before any production
backend change. Original failing binaries, traces and pins remain reproducible.
