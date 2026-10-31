# Native platform recipes and acceptance boundary

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
Only Linux has an executed backend acceptance record. Windows identity supports
P12 and an explicit `location\store\40-hex-thumbprint` reference; PEM remains an
explicit unsupported case. Automatic client-certificate selection is disabled.
Store lookup and P12 parsing require real Schannel verification before acceptance.

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

No hosted CI configuration or run was changed in this mission. Existing hosted
jobs need the explicit dependency and test-fixture bootstrap before configuring
the HTTP-enabled root project; Boss owns that integration and dispatch.
