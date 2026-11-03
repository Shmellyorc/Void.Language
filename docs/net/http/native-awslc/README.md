# Isolated Windows AWS-LC provider study

Research only. These executables are never linked into VOID. The authoritative
curl source, existing providers, Runtime and public configuration are unchanged.
See [the architecture review](../WINDOWS_TLS_PROVIDER_REVIEW.md) before drawing a
production conclusion from a successful TLS handshake.

The fixed VM layout reuses the previous feasibility mission's verified
`.toolchains/native-schannel/sdk/clang+llvm-23.1.4-x86_64-pc-windows-msvc/bin`
and its read-only credential inventory executable. Use the x64 Visual Studio
developer environment with SDK 10.0.19041.0 and `/MT`. Git's OpenSSL and Perl are
explicit fixture/build tools, not production link dependencies.

From the repository root in that developer environment:

```text
python docs/net/http/native-awslc/build.py
python docs/net/http/native-awslc/run.py --profile Debug
python docs/net/http/native-awslc/run.py --profile Release
python docs/net/http/native-awslc/build.py --multi
python docs/net/http/native-awslc/select.py
python docs/net/http/native-awslc/collect.py
```

`build.py` verifies the existing dependency pins for AWS-LC 5.12.0 and nghttp2
1.70.0, plus the already selected NASM 2.16.03 archive. It extracts and builds
only under `.toolchains/native-awslc`, with source modification disabled.
No Linux worktree or dependency archive collection is transferred. Separate
Debug/Release static libraries use the same `/MT` setting. Curl is compiled from
the unchanged repository source with AWS-LC, nghttp2, WinIDN and Windows SSPI.
The probe omits c-ares, PSL and compression; it does not satisfy the complete
production feature/link/install contract. `--multi` builds separate experimental
libcurl images containing both Schannel and AWS-LC.

The controller generates disposable RSA-2048/ECDSA-P256 identities and two-day
CA/server certificates inside a freshly restricted current-user/SYSTEM directory.
Its P12 uses OpenSSL's default AES/PBES2 format. OpenSSL receives password bytes
through a binary pipe; an independent parse checks the intended password before
the native test. Fixture P12/key files are necessary test inputs, are removed on
exit and are never evidence artifacts. Native AWS-LC parses bounded process-owned
P12 bytes, checks certificate/key correspondence, and curl receives a NOCOPY blob
whose owner outlives all easy/connection cleanup. The probe never imports a key
into a Windows provider or writes a certificate store.

For each profile, 38 controls cover both TLS versions and both HTTP versions,
exact peer DER, wrong password/root/hostname, anonymous clients, reuse, native
root import, revocation and verified HTTPS CONNECT including explicit proxy P12
authentication. Two additional owners run in separate native processes; this is
not a same-process ClientState or Runtime shutdown test. The read-only SSL-context
callback observes trust-object counts without changing verification policy.
The loopback peer requires the generated client certificate and records the
actual TLS version and ALPN. No insecure or revocation-disable flag is used.

The revocation controls deliberately distinguish a **security difference** from
an unexpected test failure. Default AWS-LC accepts the now-revoked server and
does not fetch its CDP. An explicitly supplied valid CRL succeeds before
revocation and rejects afterward. Required OCSP stapling rejects the unstapled
peer. Those expected observations do not establish Schannel-equivalent policy.

Both builds also execute four total MultiSSL controls, explicitly selecting each
backend before global initialization, then trying the other backend while the
first is active. Selection is global to the libcurl image; separate easy/multi
handles cannot override it. There is no provider fallback in the TLS study.

Original build receipts/logs and prior run progress/results are preserved under
`earlier-*` before replacement. The collector validates final source/binary
hashes and retains original failure excerpts and preliminary public receipts in
`../evidence/windows-awslc-provider-20261103.json`. It excludes host paths, private
bytes, passwords and SIDs. Test Python wheels use the repository's exact hashes;
after Python rejected that download host's chain, Windows curl downloaded them
with ordinary native verification. No trust settings or verification bypass was
introduced.

Cleanup closes easy handles and their internal multi/connection cache before
global curl cleanup, frees directly parsed EVP/X509/P12 objects, and explicitly
zeros the owned input/password buffers. This is deterministic ownership evidence,
not a heap-allocation/secure-erasure proof or an ASan/UBSan run. Before/after
CurrentUser Software KSP name/algorithm/flags fingerprints and certificate-store
DER inventories match. No persistent test key/store is created; inventories do
not compare the private file bytes or every property of unrelated keys.

Native Server 2022, installed relocation, the complete Runtime, cancellation/GC,
provider memory certification, enterprise trust policy and Windows store signing
remain acceptance work. No hosted CI is dispatched by these scripts.
