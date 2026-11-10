# D3 HTTPS Git dependency acquisition closeout

Implementation against `63dc61ba963d2f522adc242e721bea33ef40e172` on
`main`. The HTTPS lane uses the existing D3 acquisition command, resolver
and snapshot format. [D3 closeout](DEPENDENCY_D3_CLOSEOUT.md) · [Dependency design](PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md)

## Declaring and using a pin

```toml
[dependencies.geometry]
id = "org.example.geometry"
git = "https://example.org/void/geometry.git"
rev = "0123456789abcdef0123456789abcdef01234567"
version = "=1.4.0"
```

```sh
voidc deps fetch --project .
voidc resolve --project .
voidc check --project .
voidc run --project .
voidc build --project . -o my-app
```

Public repositories must support Git smart HTTPS. The origin uses a plain
lowercase hostname or IPv4 address, an optional canonical decimal port from
1 to 65535, and a portable repository path. The revision is a full lowercase
SHA-1 commit ID. The optional `subdir` selects a library within the pinned
repository, with the root as the default. Project-relative `file:` origins
still work. The HTTPS lane rejects URL credentials, queries, fragments, IPv6
literal syntax and alternate protocols. It has no private-repository
authentication.

The root manifest authorizes its direct pins. A missing transitive pin
requires `--allow-git ORIGIN REV` unless the root also declares that exact
pin. Acquisition checks the existing store first and can reuse a verified pin
with the server unavailable. Changing the URL or revision selects a distinct
pin. Run `deps fetch` to acquire it and `resolve --refresh` to replace a
stale lock. Only `deps fetch` can run Git. Resolution, compilation and editor
analysis consume the verified local store.

## Transport and verification

VOID invokes the absolute `VOID_GIT_EXECUTABLE` selected at configuration
time. The command-local `void-source` remote maps to the declared URL.
Git fetches the literal revision from that remote at depth one. It verifies
incoming objects with `fetch.fsckObjects` and skips tags, submodule
recursion, maintenance and worktree checkout. Package hooks, attributes,
filters and scripts have no execution path. If the remote refuses an exact
object-ID request, the diagnostic identifies the unavailable pin or server
capability failure. Git receives one revision request and VOID requires the
exact commit object even if the remote advertises other refs.
[Git fetch](https://git-scm.com/docs/git-fetch)

The private bare repository uses an isolated environment and configuration.
HTTPS is the only transport it can launch. Redirects are disabled, including
redirects on the same host. Caller Git configuration, proxies, credential
helpers, askpass and user netrc are excluded. Terminal prompts are disabled.
Git verifies the certificate chain and hostname with its default trust store.
An absolute local `GIT_SSL_CAINFO` file can explicitly supply a CA bundle
while preserving both checks. The caller's other Git and TLS settings are
excluded. [Git transport and TLS settings](https://git-scm.com/docs/git-config)

The fetch has a 30-second deadline and Git's 10-second abort below one
byte per second. VOID
limits the private transport workspace to 96 MiB of files, 1,024 entries and
256 KiB of diagnostics. POSIX also limits individual file writes and
supervises a dedicated process group. Windows uses a job with a 512 MiB
memory limit and a 288 MiB write threshold that also counts helper I/O.
Workspace and job accounting are sampled. The size limit applies to stored
transfer data. HTTP framing and bytes in flight are governed by the
deadline. The D3 object read then uses the remaining 60-second acquisition
budget. A normal success or failure removes the
private Git workspace. A process killed externally can leave private files.
A new invocation creates a new workspace and ignores abandoned ones.

### Cross-origin fetch repair

The integration review reproduced an origin violation with Git
`2.53.0.697.g625c4fb2daa`. A TLS server inserted an unsolicited v2
`packfile-uris` section before the ordinary inline pack. Git invoked
`http-fetch` for the supplied URI and a second TLS origin received a
request. The subprocess subsequently failed when that endpoint returned
404. Disabling redirects and leaving `fetch.uriProtocols` empty allowed
the request. Requesting protocol v1 blocked the v2 section when the server
responded in v1. A server that forced a v2 advertisement and completed
the v2 exchange still reached the second origin.
[Git packfile URI design](https://git-scm.com/docs/packfile-uri) · [Git v2 client implementation](https://github.com/git/git/blob/v2.53.0/fetch-pack.c#L1659-L1705)

The repair keeps normal Git protocol negotiation. The fetch sets
`remote.void-source.url` to the declared URL and
`remote.void-source.proxy=` to permit a direct connection for that remote.
The default `http.proxy=://` is a deliberately invalid URL. The standalone
`http-fetch` helper opens its own HTTP session without the named remote
override and Git rejects that default before connecting. The policy also
refuses unsolicited packfile downloads back to the declared origin.
[Per-remote configuration](https://git-scm.com/docs/git-config) · [Standalone pack downloader](https://github.com/git/git/blob/v2.53.0/http-fetch.c)

The maintained HTTPS regression records requests at both endpoints and
requires zero requests for the unsolicited pack. A separate dumb-HTTP
fixture confirms that the depth-one requirement stops acquisition before
Git requests object alternates. Bundle sources and promisor remotes remain
unconfigured in the private repository. HTTPS requires a smart server
that can deliver the pinned commit in the ordinary fetch response.

| Loopback control on Git `2.53.0.697.g625c4fb2daa` | Requests to the second TLS origin |
| --- | ---: |
| Original fetch, protocol v2 | 1 |
| Original fetch with empty `fetch.uriProtocols`, protocol v2 | 1 |
| Protocol v1 requested and returned | 0 |
| Protocol v1 requested, server forced v2 | 1 |
| Repaired VOID fetch, protocol v2 | 0 |

### Verification and publication

The raw selected commit, its tree objects and every exported blob are
rehashed against their Git IDs by the existing D3 verifier. Path, mode,
inventory and content limits apply before package admission. The resolver
independently computes the D1 SHA-256 package digest from the exported bytes.
Identical Git objects from local and HTTPS repositories produce identical
proof, exported files and D1 package digests. The D3 store key and snapshot
digest still bind the declared origin and revision. Git graphs keep lock
format and resolver revision 2. Source ownership and lock-derived cache
identity use the existing D2 contracts.

Acquisition validates the graph before publication. D3 stages and verifies
each snapshot before an exclusive rename into `.void/git-deps`. A concurrent
winner is reused after verification. Failed transport, missing objects,
invalid packages and incomplete snapshots are rejected. A corrupt published
entry must be moved aside before an explicit reacquisition. Power-loss
durability remains untested.
[Git snapshot implementation](../../src/frontend/project_git.cpp) · [Resolver](../../src/frontend/project_resolution.cpp)

## Qualification

The maintained HTTPS fixture uses a disposable real Git HTTP backend and
an ephemeral CA. It verifies exact pins after branch movement, missing
commits and restricted servers, origin changes and transitive authorization,
TLS trust and hostname rejection, cross-origin redirect refusal and no
credentials on requests. It interrupts and corrupts pack responses, retries
past an abandoned private store, and exercises concurrent publication.
It compares local and HTTPS snapshot data and package digests, verifies
deterministic lock bytes and cache hits and misses, and tests missing and
tampered snapshots. Check, emit, debug and release JIT, and debug and release
AOT run after the server stops. The linked executables print the value from
the updated pinned package. [HTTPS fixture](../../tests/project/git_https.py)

| Native platform and profile | Original focused qualification | After origin repair |
| --- | --- | --- |
| Linux x86-64 Release | 11/11 | 11/11 |
| Linux x86-64 Debug | 11/11 | 11/11 |
| Linux x86-64 RelWithDebInfo, fatal ASan/UBSan with leak detection | 11/11 | 11/11 |
| Windows Release and Debug | UNQUALIFIED | UNQUALIFIED. VirtualBox Guest Control again rejected login with `VBOX_E_IPRT_ERROR (0x80bb0005)`. |
| macOS ARM64 Release and Debug | UNQUALIFIED | UNQUALIFIED. The focused manual workflow passed static validation. |

The public-host probe used GitHub's `octocat/Hello-World` at
`7fd1a60b01f91b314f59955a4e4d4e80d8edf11d` with default TLS trust. The
transport and Git snapshot verifier completed. Package admission reported
the missing VOID manifest and left the store absent. The end-to-end package
workflow used the loopback HTTPS Git server. The same public pin reached
package admission after the repair with default TLS trust.
[Public commit](https://github.com/octocat/Hello-World/commit/7fd1a60b01f91b314f59955a4e4d4e80d8edf11d)

In a separate Linux Debug run, the compiler resolved, checked, emitted and
ran both JIT and AOT profiles inside a new network namespace. The client
could not connect to the live fixture server, and the server saw no new
requests. A `voidls` probe navigated to the acquired source and then
reported a missing snapshot on refresh after removal, with no Git requests.
Release transport controls refused a pack containing 99 MiB of
incompressible source and stopped a trickling server at 30.02 seconds.
Both controls left the dependency store absent and cleaned the private
Git workspace.

The repaired Debug compiler repeated the isolated-network and editor
checks. Snapshot reuse, resolution, check, emit, JIT and AOT completed in
the network namespace. Definition navigation and missing-snapshot refresh
left the live HTTPS server's request count unchanged.

Early qualification found a missing include for the shared pin validator
and a Release object compiled while the port parser edit was landing.
The first strict link also used a generated build graph from before the
transport source was registered. The include was added, the affected
Release object rebuilt and the strict build graph reconfigured. The initial
fixture expected a missing snapshot before the compiler's missing-lock
diagnostic. It now checks both states. The HTTPS fixture also preserves Git
error packets when the backend exits nonzero, so missing and unadvertised
commits exercise the actual client diagnostic.

The first repair replay's dumb-HTTP fixture used a close-delimited response.
Adding its content length let Git reach the intended shallow-transport
refusal with the Linux GnuTLS backend.

## Reproduction and macOS handoff

Configure disposable Release and Debug builds against LLVM/Clang/LLD 23.1.4
and the profile-matched host and HTTP provider prefixes. Use
`RelWithDebInfo` and `VOID_SANITIZERS=ON` for strict. The HTTPS fixture needs
Python 3.12 or newer, Git with HTTPS support, and the OpenSSL command to
create temporary certificates. [Linux configure commands](DEPENDENCY_D2_CLOSEOUT.md#linux-reproduction)

```sh
mkdir -p "$scratch/tmp"
export TMPDIR="$scratch/tmp"
export PYTHONDONTWRITEBYTECODE=1 VOID_TEST_STDLIB="$PWD/stdlib"
# Set these for the strict build.
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake --build "$build" --parallel 3 --target voidc voidls void_project_packages_ir \
  void_project_header void_project_manifest void_runtime_callbacks void_g5_reload
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_manifest_header|project_manifest|project_dependencies|project_packages|project_git_dependencies|project_git_https|local_libraries|project_creation_process|runtime_callbacks|ffi_g4_conventional|ffi_g5_callbacks)$'
```

The opt-in reproduction uses the original Git argument list against two
disposable loopback TLS origins before running the repaired compiler
fixture. Use the Git version recorded above for the positive controls.

```sh
python3 -B tests/project/git_https.py "$build/voidc" /usr/bin/git --reproduce-packfile-uris
```

`ci-macos-git-https.yml` is manually dispatched with the full reviewed SHA.
It builds `voidc` and selects exactly `^project_git_https$` on native
`macos-15` ARM64 Debug and Release. The shared workflow reuses the pinned
LLVM/Clang/LLD 23.1.4 and native provider bootstrap, Xcode Mach-O linker,
essential compiler check/JIT/AOT smoke, and bounded failure logs. Actionlint,
Bash syntax checks and selector inspection passed. The D1, D2 and D3
`file:` workflow selectors were verified byte-for-byte. No dispatch occurred.
[Manual workflow](../../.github/workflows/ci-macos-git-https.yml)

## Changed files and Git status

| Git status | Path |
| --- | --- |
| Modified | `CMakeLists.txt` |
| Modified | `src/frontend/project.cpp` |
| Modified | `src/frontend/project_git.cpp` |
| Modified | `tests/project/git_dependencies.py` |
| Untracked | `.github/workflows/ci-macos-git-https.yml` |
| Untracked | `src/frontend/project_git_process.cpp` |
| Untracked | `src/frontend/project_git_process.hpp` |
| Untracked | `tests/project/git_https.py` |
| Untracked | `docs/projects/DEPENDENCY_D3_HTTPS_CLOSEOUT.md` |

`main` and `origin/main` remain at the baseline with an empty index diff.
The task-owned build trees, object stores, fixtures and diagnostic logs
were removed after qualification.
The implementation is ready for Golden Wrench's independent review before
integration. [Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md)
