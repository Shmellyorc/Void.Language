# Native Windows AWS-LC adapter investigation

**Rejected integration prototype.** Installing an origin store identity through
the public curl SSL-context callback disclosed that identity to a requesting
HTTPS proxy. Use only the controller's disposable synthetic identities. This is
feasibility evidence, not production code or a credential-provisioning design.
See the [decision and limitations](../WINDOWS_TLS_ADAPTER_FEASIBILITY.md).

Baseline `5bf1c514cb9c6a52d00a1ee359d5c54af762e8ec`; native Windows 10 Home
22H2 x64, LLVM/Clang/LLD 23.1.4, SDK 10.0.19041.0, `/MT`, Debug and Release.
The [prior transport study](../native-awslc/README.md) supplies the unchanged
AWS-LC 5.12.0, curl 8.23.0 and nghttp2 archives, Python dependencies and local
peer. No dependency download, provider rebuild, second backend or full VOID
compiler build is needed here. Run from the repository root on the authorized VM.

```powershell
cmd /c '"C:\BuildTools\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul && python docs/net/http/native-awslc-adapters/build.py'
python docs/net/http/native-awslc-adapters/study.py --profile Debug --track all
python docs/net/http/native-awslc-adapters/study.py --profile Release --track all
python docs/net/http/native-awslc-adapters/study.py --profile Debug --track scope
python docs/net/http/native-awslc-adapters/study.py --profile Release --track scope
python docs/net/http/native-awslc-adapters/study.py --profile Debug --track proxy-origin
python docs/net/http/native-awslc-adapters/study.py --profile Release --track proxy-origin
```

`--track trust` and `--track sign` run independent mechanism controls. `all` runs
verification first, signing against the ordinary AWS-LC verifier next, then the
combined controls. `scope` deliberately reproduces the failed identity privacy
assertion; a controller exit of zero means the flaw was reproduced and cleanup
completed. It does not mean security acceptance. The inherited proxy fixture
rejects CONNECT to `wrong.test` before origin TLS; the original `all` row
`origin_wrong_hostname_after_proxy` is inconclusive. `proxy-origin` corrects that
control with a wrong-SAN leaf at `localhost` and requires the exact native policy
sequence `0`, `0x800B010F`.

For the separate read-only public-anchor control, supply a public PEM for a
generated unrelated CA absent from Windows ROOT. It need not have a private key:

```powershell
python docs/net/http/native-awslc-adapters/roots.py --unrelated-ca C:\path\to\public-unrelated-ca.pem
python docs/net/http/native-awslc-adapters/collect.py
python tools/verify_curl_source.py
```

The published anchor run used the public `wrong.pem` retained from an original
failed fixture setup. A successful anchor check is not a public-server TLS or
enterprise-policy acceptance test. `roots.cpp` reads public ROOT certificates
and compares default Windows chain acceptance with an exclusive unrelated CA
snapshot; it never opens an identity key or writes ROOT.

Files and authority:

| File | Purpose |
| --- | --- |
| `common.hpp` | Bounded inputs, public-key metadata, handle helpers and test ACLs |
| `trust.hpp` | Windows chain/SSL policy with strict revocation; explicit exclusive roots |
| `signing.hpp` | Exact CurrentUser store selection, CNG key matching and signing |
| `probe.cpp` | One AWS-LC image, synchronous hooks, independent owners/reuse and retry/deadline negatives; rejected proxy scoping |
| `fixture.cpp` | Operator-controlled synthetic CNG key/store setup, inspect and exact owned cleanup; never called by the adapter |
| `study.py` | Disposable local certificates, CRLs/AIA, peers, cases and inventory/cleanup receipts |
| `roots.cpp`, `roots.py` | Read-only public-anchor/replacement diagnostic |
| `build.py`, `CMakeLists.txt` | Isolated native builds reusing existing archives |
| `collect.py` | Validate binary/core-source/archive hashes and compact public evidence without reclassifying failures |

The fixture controller creates sign-only non-exportable RSA-2048/ECDSA-P256 keys
and isolated `CurrentUser\VOID-ADAPTER-TEST-STORE-<nonce>` stores. A persisted
owner marker plus exact public key and certificate checks constrain cleanup to
its own objects. This serial test setup makes no concurrent CNG creation or
crash-recovery guarantee and does not reopen the rejected persistence proposal.
The adapter opens those credentials read-only, borrows signing handles, and does
not create, modify or delete keys or stores. Private export controls are size
queries only; no private-key output buffer is allocated.

Windows AIA retrieval can cache the generated public intermediate in CurrentUser
CA. The controller checks it was absent initially, then removes only that exact
DER/SHA-256 entry and refuses entries with a private-key association. Pre-existing
certificates and machine stores are preserved. OS-managed public URL caches are
not globally purged. Before/after inventories cover software-KSP key identifiers
and CurrentUser store certificate DER, not every OS provider/cache counter.

Private CA/server fixture files are restricted to the current user and SYSTEM
and removed in `finally`. Earlier failed directories retain only public material;
`*.key`/`*.p12` are excluded. A forced termination can interrupt this process;
do not substitute broad key/store deletion for the ownership checks. Never set
`SSLKEYLOGFILE` for these experiments.

Disposable builds, JSON progress and original failures stay under ignored
`.toolchains/native-awslc-adapters/`. Only
[compacted public evidence](../evidence/windows-awslc-adapters-20261103.json) is a
deliverable. Core compiled C++ and binary hashes match the final runs. Controller
and CMake changes made later are identified by their historical receipt hashes;
the collector does not claim every older run executed the final Python source.
Strict sanitizer, full Runtime, cancellation/GC/shutdown, production dependency
closure and Windows Server 2022 acceptance remain open.
