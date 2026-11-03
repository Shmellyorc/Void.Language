# Standalone native Schannel feasibility probes

These programs are research fixtures, never production initialization or a
credential-management library. They create explicitly disclosed, disposable,
non-exportable **persistent CurrentUser test keys** and private certificate
stores. Their temporary P12 decode is exportable. Run only in an approved
disposable Windows test environment. No trust store is modified.

The different-key collision cases deliberately target a sentinel created and
owned by the same invocation. They demonstrate a safety failure of the proposed
named-import assumption on Windows 10. Do not turn this code into a production
owner or use its random names as a substitute for collision protection.

## Build

Use a native x64 Visual Studio developer environment, Windows SDK, Ninja, CMake,
Python, and the repository-pinned complete LLVM 23.1.4 SDK. The SDK archive must
match `tools/ci/llvm_sdk.py`. Set `LLVM_SDK` below to its extracted directory;
no system installation or full VOID build is required.

```powershell
python tools/verify_curl_source.py
cmake -S docs/net/http/native-schannel -B .toolchains/native-schannel/build -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_COMPILER=$env:LLVM_SDK/bin/clang-cl.exe" "-DCMAKE_CXX_COMPILER=$env:LLVM_SDK/bin/clang-cl.exe"
cmake --build .toolchains/native-schannel/build --target probe peer-client --parallel 4
```

Repeat configuration in `build-release` with `CMAKE_BUILD_TYPE=Release`. Both
builds use `/MT`. Curl comes directly from the unchanged pinned
`third_party/curl` library. Optional transport dependencies are disabled in this
isolated build; it does not certify the production HTTP dependency closure.

`run.py` uses the installed Git OpenSSL CLI only for disposable certificates and
a signed CRL. Its default location is explicit in the source. Python's SSL server
requires client authentication; Schannel requires the supplied CA, hostname and
ordinary revocation checks. An HTTP server provides the fixture's CRL over
loopback. No `insecure`, `NO_REVOKE`, auto-client-cert or TLS fallback is used.

## Run and inspect

```powershell
python docs/net/http/native-schannel/run.py --algorithm RSA
python docs/net/http/native-schannel/run.py --algorithm ECDSA
python docs/net/http/native-schannel/run.py --mode concurrent
python docs/net/http/native-schannel/run.py --mode collision
python docs/net/http/native-schannel/run.py --mode deferred-collision
python docs/net/http/native-schannel/run.py --mode deny-store-delete
python docs/net/http/native-schannel/run.py --mode stop-intent
python docs/net/http/native-schannel/run.py --mode stop-import
python docs/net/http/native-schannel/run.py --mode stop-finalize
python docs/net/http/native-schannel/run.py --mode stop-store-unmarked
python docs/net/http/native-schannel/run.py --mode stop-store-marked
python docs/net/http/native-schannel/run.py --mode stop-key-unmarked
python docs/net/http/native-schannel/run.py --mode stop-wrong-marker
```

Add `--build build-release` for Release. Run cases sequentially. The concurrency
case admits two owners, performs simultaneous verified requests, closes the first,
then verifies that the second remains usable. Each peer client performs two
requests on one easy handle; the second must report zero new connections.

Collision cases return harness success only when the probe reports the expected
architecture failure and both known test keys are absent afterward. This is a
passing reproducer of a **failed security guarantee**, not production acceptance.
The `deny-delete` diagnostic is intentionally retained as a failing control:
denying DELETE on the KSP key did not prevent deletion, including after reopen.
The private-store ACL produces a genuine error-5 deletion failure instead.

The fixture directory receives a current-user/SYSTEM DACL before private files
are generated. Passwords travel only through pipes, never argv or receipts.
The C++ bridge keeps encrypted PKCS#8 and its independent random password in
bounded, zeroing memory and wipes them before publishing the store selector.
The generated fixture keys, P12 and passwords never become evidence artifacts.
Private fixture files are removed on exit. Only public recovery metadata is
retained if cleanup cannot be confirmed; inspect any `*-public-recovery.txt`
under `.toolchains/native-schannel` before claiming no residue.

The `recover` subcommand is an intentionally small experiment-specific probe:
it acquires the stable `.lock`, accepts a bounded nonce/public-key/certificate
receipt, checks exact names and markers, and quarantines ambiguous objects.
`operator-cleanup` is called **only by the fixture controller for its own child**
after its recorded creation checkpoint, to remove intentionally unmarked test
objects. It is not automatic recovery authority and must never be used against
objects with unknown provenance. A production journal would also need the full
SID/application/schema/path-admission contract; this fixture does not implement it.

All termination points use actual child-process termination, not destructors.
The finalized-key case also rejects corrupt, oversized and wrong-public-identity
records before successfully recovering the correct record. Live journal
replacement succeeds while replacing the stable lock is denied with error 32.
This does not prove power-loss atomicity, secure erasure or arbitrary provider
behavior. Runtime cancellation, GC and shutdown integration remain unimplemented.

Receipts are written under `.toolchains/native-schannel`. After the documented
case set, `python docs/net/http/native-schannel/collect.py` writes the compact
public evidence. It excludes host paths, user names, SIDs and disposable selectors.
Use `python -m json.tool` to pretty-print the compact JSON receipt for inspection.
