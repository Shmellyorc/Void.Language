# Native platform recipes and acceptance boundary

## Hosted acceptance — 4 November 2026

**ACCEPTED.** [Final acceptance](ACCEPTANCE.md) records run
[41815539463](https://github.com/Shmellyorc/Void.Language/actions/runs/41815539463),
commit `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`, Compiler link experiment,
mode `thinlto-cache`, LLVM/Clang/LLD 23.1.4. All seven hosted jobs passed.
Golden Wrench closes HTTP/HTTPS cross-platform acceptance, including Windows
Schannel store-backed mTLS and the connection-scoped HTTP/2 ALPN retention issue.
The separate optimizer incident and documented limitations retain their status.
Earlier checkpoints below preserve their original scope and evidence.


The [3 November Windows repair](WINDOWS_SCHANNEL_ACCEPTANCE.md) adds actual
Windows 10 VM Debug/Release public-store mTLS and focused HTTP/URL/source/install
evidence. Schannel supports explicitly provisioned store identities in that tested
scope; supplied P12/PEM is unsupported without a persistence fallback. Windows
Server 2022 hosted acceptance, supported strict and full memory certification
remain separate. The earlier Linux-host checkpoint below retains its date/scope.

The Linux x86_64 implementation has actual local Debug, Release and strict
execution receipts. Native Windows and macOS recipes and adapter code are
implemented but **unexecuted on this Linux host**. This is an acceptance gap,
not a Linux result extrapolated to other operating systems. No supported-platform
exclusion or portable delivery acceptance has been approved.

| Native host | Provider/trust/IDN | Build boundary |
| --- | --- | --- |
| Linux x86_64 | Approved unmodified AWS-LC 5.12.0; distribution CA bundle/directory; explicit UTF-8 libidn2 | Separate Debug/Release/strict prefixes; replaceable GNU IDN/Unicode shared libraries plus corresponding source |
| Windows 10+, x86_64 or arm64 | Schannel; native certificate stores and WinIDN | Matching native Visual Studio SDK environment, LLVM 23.1.4 clang-cl, Ninja, `/MT` in every profile; no GNU IDN/Unicode dependency |
| macOS 14+, x86_64 or arm64 | Same immutable AWS-LC 5.12.0 source; curl SecTrust, Apple ICU UTS46 | One native architecture per prefix; physical SDK and deployment target recorded; existing vendor libc++/system-runtime pairing preserved |

The native macOS recipe reuses the exact already selected AWS-LC revision and
archive checksum. It does not introduce a different TLS release or source fix.
Windows identity uses an explicit `location\store\40-hex-thumbprint` reference;
supplied P12 and PEM return unsupported before file access or key import.
Automatic client-certificate selection is disabled. Caller-managed CurrentUser
software-CNG RSA/ECDSA store identities now have native source acceptance;
hardware, other locations and hosted acceptance retain their own proof gates.

Use the existing LLVM SDK setup and native platform prerequisites first, then:

```text
python tools/setup_http_deps.py --profile Release
python tools/setup_http_tests.py
```

Windows test-certificate generation additionally uses
`python tools/setup_http_tests.py --certificates`. This builds a separate
test-only CLI from the same pinned AWS-LC archive. Production Windows HTTP still
uses Schannel; this CLI and its crypto archive are not production link resources.

Configure separate single-configuration application directories with the same
LLVM SDK environment and existing platform compiler flags. For Debug use
`--profile Debug` and `-DCMAKE_BUILD_TYPE=Debug`; for strict use
`--profile strict` and `-DVOID_SANITIZERS=ON`. Strict requires full ASan/UBSan
compile/link/run support, with halt-on-error and leak checking. Unsupported native
instrumentation must fail configuration; no reduced strict profile is accepted.
`cmake/HttpNativeDependencies.cmake` rejects mismatched profiles, architectures,
SDK/deployment policy and missing pinned files. CMake never downloads dependencies.

The portable libpsl recipe compiles unmodified upstream source with a generated
UTF-8/ACE suffix automaton. On Linux, its independent strict control passes
upstream cookie-domain, registrable-domain and builtin-data checks (184 cases).
Those controls do not establish Windows/macOS ABI, trust, IDN or stack acceptance.

Native acceptance must run the same HTTP/URL/source/install/LSP tests, real
HTTP/1.1 and ALPN HTTP/2, native path/environment/IDN cases, trust replacement and
identity tests. Native certificate-store/revocation behavior, proxy discovery,
hosts/search/VPN/split DNS, actual worker/transport stack sizes and installed
relocation require platform evidence. No user trust store is modified by the
loopback fixtures. The AWS-LC parser-memory corpus is not a Schannel allocator
audit and is intentionally not registered as such on Windows.

The subsequent 31 October 2026 [CI preparation](CI_HANDOFF.md) integrates pinned
dependency/fixture bootstrap and verified caches into the existing seven-job fast
workflow. No hosted run was dispatched. Native Windows/macOS execution acceptance
remains open; Boss owns Git and hosted acceptance after Golden Wrench review.
