# Project VOIDUP — feasibility and architecture handoff

6 November 2026. Research only. Stop for Golden Wrench review and Boss approval.

**Feasible as a VOID-native application with a bounded set of new host APIs;
not safely implementable using the unchanged current public standard library.**
The missing pieces are application arguments, authenticated artifact verification,
archive decoding, installation-safe filesystem capabilities and controlled child
process execution. None requires new language syntax, compiler self-hosting or a
package dependency solver. They do require real engineering and security tests;
this is not simply a translation of `tools/install.py`.

The principal program, command parsing, manifest/schema policy, download policy,
version selection, extraction policy, transaction sequencing, activation and
rollback must be `.void`. Native code supplies bounded primitives, as it already
does for HTTP and IO. Do not hide an entire installer behind a C++ `install()`
operation or invoke Python for the installation decisions.

## Current baseline and capability audit

Inspected current sources, installed-module declarations, entry lowering, Runtime
resources, native build manifests, source installation and current qualification
records. `docs/README.md` now records AOT-only target-aware O2 acceptance at
`175c08c8`, run `42231556437`; the subsequent developer-experience record adds
source onboarding. JIT remains on its existing pipeline. These are repository
acceptance records, not a new independent hosted verification in this mission.
No tests, builds, installation, Git operations or CI were run for this report.

| Area | Verified existing capability | Missing or limiting behavior |
|---|---|---|
| Application entry | `src/frontend/compiler.cpp:693,6809`; `src/main.cpp`; `src/backend/llvm/backend.cpp:2210–2253`: synchronous `main()` with void/i32 result, top-level applications and legacy `entry(i32)->i32` | `main` requires no parameters. CLI rejects application operands; generated AOT application adapter rejects argc other than one. No public argument-list API. Compiler `wmain` already preserves Windows Unicode, but emitted application startup is separate. |
| Files | `stdlib/io.void:420,481,577,713,739`: exclusive file creation, bounded reads, complete-write loops, partial progress/errors, explicit durable file flush and close | Exclusive file creation is not exclusive private-directory creation. No public permission/DACL, directory-handle, installation lock or namespace-durability API. Ordinary paths are not a containment boundary. |
| Publication | `io.move_file(..., replace)` and `move_directory`; `src/runtime/filesystem.cpp:956`: native same-filesystem rename, exclusive no-replace operations, cross-device errors | Directory replacement is not portable replacement of an arbitrary nonempty installation. No transaction around an entire installed prefix. File flush alone does not persist a directory entry. |
| Locations | All 13 functions in `stdlib/os.void`: data/config/cache/state/home/temp, executable/resources, readonly environment, platform/architecture, PID/bundle | Discovery returns a location, not secure ownership, existence or permissions. Environment mutation and process launch are not provided. JIT executable/resource discovery describes the host. |
| HTTPS | `stdlib/net/http.void:238–313,960–1209`: verified TLS, explicit proxies, bounded raw/decoded transfer, deadlines, sync/async streaming and direct file download | Default 64 MiB transfer limits are too small for many toolchains. Streaming does not authenticate a release. Direct download neither flushes nor closes the destination. |
| Manifests | `stdlib/json.void:544,602,1107`: duplicate-name rejection, metering, typed decoding with unknown-field rejection; `time.now_utc()` | No trusted release catalog, release signing policy or anti-replay state exists. JSON parsing is not authentication. |
| Hash/signature | Compiler-private LLVM SHA-256 in `src/native_build.cpp:117`; Python SHA-256 pins in `tools/ci/llvm_sdk.py:20–67` | No public cryptographic hashing/signature module. Resource checksums are unsigned consistency checks. Gameplay `random` and FNV are not artifact authentication. |
| Archives | SDK helper verifies SHA-256 then uses Python `tarfile` with `filter="data"` | No source-language archive reader/extractor. HTTP content decompression is not archive extraction. |
| Processes | Compiler-private `llvm::sys::ExecuteAndWait` at `src/native_build.cpp:192`; argv-vector construction | No source-visible spawn/wait or child environment API. Importing a stdlib name cannot access this compiler-private function. |
| Hosting | Existing normal/O2 JIT and independent AOT; Runtime-owned resources and worker jobs | Ordinary CLI async application hosting is not claimed by current onboarding. Use a synchronous `main` and the existing synchronous HTTP/IO facade initially. Do not make a new async-main feature a dependency. |
| Distribution | CMake compiler/tooling install components, relative stdlib discovery, native resource inventories, notices and external smoke tests | Source installation is nontransactional and not a portable binary package. Native manifests retain original SDK/platform paths. There is no approved public bootstrap binary or complete relocatable toolchain artifact. |

The [source install guide](../install/README.md),
[developer-experience report](../developer-experience/REPORT.md) and
[HTTP acceptance](../net/http/ACCEPTANCE.md) distinguish real native results from
prepared instructions. Current Linux onboarding has fresh Ubuntu 24.04 evidence;
the new wrapper's native Windows/macOS execution remains unqualified. Historical
logging-based automatic release holds in older documents are superseded; this
proposal establishes no unrelated release blocker.

## First bounded product

Recommend a **per-user manager of prebuilt, signed, host-native toolchains**.
No elevation, source rebuild on the user's machine, package installation,
project dependency resolution, arbitrary install scripts or compiler self-hosting.
The bootstrap and release producer may use existing build tools; installed
`voidup` must not require Python, CMake or a C++ compiler to execute its own logic.

The first complete milestone installs two distinct releases side by side, verifies
them, switches the selected release, launches its tools and rolls back. It retains
installed versions. Automatic pruning/uninstallation and manager self-update are
excluded initially to avoid deleting files used by running tools or replacing a
running Windows executable. These exclusions need Boss's scope approval, not a
claim that missing cleanup is already implemented.

Proposed CLI; these commands do not exist today:

```text
voidup --help
voidup --version
voidup list [--available]
voidup install <release-id>             # install only; does not change default
voidup install <release-id> --activate
voidup default <installed-release-id>
voidup rollback                        # select recorded previous installation
voidup which [<release-id>] <voidc|voidls>
voidup run <release-id|default> <voidc|voidls> -- <literal arguments...>
voidup verify <installed-release-id>
voidup doctor
```

An optional absolute `--root` selects a dedicated admitted user-owned root.
Default root: `os.data_dir("voidup")`; never adopt a generic existing `.local`
prefix or an existing manual VOID installation. Release IDs are bounded catalog
identifiers, not arbitrary filesystem paths. Current compiler `0.1.0 (development)`
is not sufficient release identity: include immutable build identity and host.
Do not infer upgrade ordering from commit hashes or lexical version strings.

`default` affects `voidup run default`; it does **not** silently change an unrelated
`voidc` already on PATH. First release prints the selected absolute tool path and
manual PATH instructions. No shell/profile/registry rewriting or executable
proxies are required for this milestone. Transparent `voidc`/`voidls` proxies can
later reuse the same `.void` selection logic; rustup demonstrates that separation
between a manager, versioned toolchains and proxies in its
[official design](https://rust-lang.github.io/rustup/concepts/).

Manager commands return 0 on success, 2 for invalid usage and 1 for operational
failure, with structured details on stderr. `run` inherits stdio and preserves
normal child exit codes; signal termination needs an explicitly documented host
mapping. It must not contaminate language-server stdout. Launch failures remain
distinguishable from a child's reported failure.

## Minimum missing interfaces and ownership

Names here are proposals, not approved or implemented declarations.

**1. Arguments without a new entry signature.** Add `os.args()` returning a checked,
owned `[string]` snapshot of application arguments, excluding the executable.
Keep `main()`/`main()->i32`, top-level code and legacy numeric entry behavior.
Capture arguments into per-Runtime application context before module globals can
read them; never expose process-global mutable argument storage. Preserve empty
arguments and Unicode. Bound count/total bytes and reject NUL/invalid encoding
explicitly; POSIX arbitrary non-UTF-8 byte paths are outside VOID's current string
contract. Windows application startup must consume wide arguments, not ACP.

JIT/embedding must supply the same context explicitly. Proposed CLI spelling:
`voidc run app.void -- app-arguments...`, only after an explicit source operand.
Preserve the existing `--` before a dash-leading source path. Compiler option
recognition, including the current early `--app` rejection, must stop before
forwarded application arguments. Legacy `entry(i32)` keeps its existing numeric
interface. Changes belong in `src/main.cpp`, entry adapter/backend, Runtime
context and the existing os frontend/ABI path; no new parser or main overload.

**2. Narrow cryptography.** An incremental SHA-256 state and detached Ed25519
verification suffice. Native state follows `NativeLibrary`/`NativeResource`
ownership, explicit close/finalization, checked snapshots and bounded chunks.
Signature checks consume a bounded complete catalog; do not accidentally use
Ed25519ph in place of Ed25519. Invalid sizes are typed errors; invalid signatures
must never be interpreted as success.

Prefer one pinned, maintained libsodium dependency for this surface, subject to
Boss approval and platform qualification. It provides
[streaming SHA-256](https://doc.libsodium.org/advanced/sha-2_hash_function) and
[detached verification](https://doc.libsodium.org/public-key_cryptography/public-key_signatures);
the [actual verifier](https://github.com/jedisct1/libsodium/blob/master/src/libsodium/crypto_sign/ed25519/ref10/open.c)
checks canonical/valid encodings before the signature equation. Do not implement
cryptography in VOID or link all LLVM into the application for SHA-256.
Reusing AWS-LC for Linux/macOS hashes is possible, but Windows uses Schannel and
does not thereby gain the same public Ed25519 API. Multiple provider paths add
qualification work; no TLS-provider migration is proposed. Pin an audited release
and its source hash during engineering; the inspected upstream branch is not a pin.

**3. Restricted archive reader, not a native installer.** Wrap a pinned libarchive
reader with owned metadata, `next`/EOF and bounded byte reads. Select only the
approved ZIP/ZIP64 stored/deflate profile and existing zlib support; reject
encryption and other compression/format methods. Its
[public API](https://raw.githubusercontent.com/libarchive/libarchive/master/libarchive/archive.h)
and [ZIP implementation](https://raw.githubusercontent.com/libarchive/libarchive/master/libarchive/archive_read_support_format_zip.c)
already support sequential decoding and ZIP64. `.void` matches each entry against
the signed inventory, chooses its destination and drives existing file writes.
Do not delegate pathname policy or activation to `archive_write_disk`, change
process cwd, or invoke system `tar`, `unzip` or PowerShell.

**4. Safe private filesystem operations.** Extend `io` narrowly with an admitted
directory capability: exclusive private child creation, relative no-follow file
creation, mode/permission application, a held interprocess writer lock, same-volume
publication and namespace synchronization/error reporting. Reuse file resources,
errors, leases and cleanup; do not expose raw native handles. Underlying POSIX
operations use directory-relative handles and permissions at creation; Windows
requires wide paths, controlled DACLs and reparse/identity checks. Existing
`src/native_windows.hpp:151` supplies valuable tested private-directory/handle
patterns, but is compiler-private and uses LLVM naming; factor applicable OS
mechanics without dragging LLVM or compiler policy into Runtime.

This is the most consequential missing safety surface. `io.create_directory`
currently succeeds on an already existing real directory and uses default host
creation policy; `io.open` follows ordinary paths. Lexical `path.normalize` and
`metadata(follow_links=false)` before another path operation cannot close a race.
Do not relabel them as secure extraction. Permission changes must only touch
newly created owned objects, never repair/adopt somebody else's directory.

**5. Explicit executable/argv/environment launch and wait.** Add a narrow process
resource using OS APIs, an absolute executable, literal argument array, explicit
environment/cwd and inherited stdio. No shell, PATH search or `.cmd`/`.bat` fallback.
On Windows specify `lpApplicationName` and encode arguments correctly for the
known tool's native argv convention; [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
documents why an ambiguous command string is unsafe. Use native POSIX argv APIs
on Unix, not a shell string. Capture launch errors separately from exit status.

Process lifetime, interrupt forwarding, deadlines if exposed, wait/drain and
cleanup must be explicit. No managed pointer may escape to a worker without the
existing snapshot/root/lease rules. Start with synchronous public operations;
reuse native coordination instead of creating a scheduler. In-flight validation
children must not race transaction cleanup. Abrupt process termination must leave
recoverable inactive staging, not trigger deletion based solely on a PID.

These additions follow the existing stdlib declaration, frontend capability, typed
IR/native-call, backend ABI and Runtime resource path. They need no new LLVM
operations. Extend native-link resource inventories, installed-module discovery,
compiler build/cache fingerprints and LSP metadata together; a JIT-only binding
is not a completed API. Provider state must not retain pointers into managed
arrays across safepoints, and cleanup must drain entered work before release.

## Trust and package contract

HTTPS authenticates a connection; an unsigned hash downloaded alongside an
artifact does not independently authorize a release. Existing `void-native-
resources.txt` checks link-resource consistency, not publisher identity, and does
not cover the entire executable/SDK/stdlib/loader closure.

Proposed bounded initial policy: built-in offline Ed25519 release verification
keys, signing a **bounded exact byte string** containing the catalog. Verify the
detached signature envelope before strict UTF-8/JSON decoding; never reserialize
JSON to guess the signed bytes. Domain-separate the signed message as VOIDUP
metadata. Enforce schema/version, unique names, exact integer sizes, target tuple,
minimum manager version, expiry and monotonically increasing catalog serial.
Duplicate keys, integer overflow, invalid times and unknown mandatory fields fail.

Recommended operational decision: offline signing with a 2-of-3 distinct-key
threshold embedded in the bootstrap. A simpler single offline key is possible
but lacks that compromise tolerance and must be an explicit owner choice. Do not
learn replacement root keys from the same untrusted endpoint. Initial key changes
require a separately authenticated manager release; there is no automatic
self-update/root-rotation protocol in this milestone.

Persist highest accepted catalog serial and time with protected control state;
same-serial different-content metadata fails. A clock error or rollback requires
an explicit diagnostic, not disabling freshness checks. The
[TUF specification](https://theupdateframework.github.io/specification/latest/)
shows why signatures alone do not solve rollback/freeze and why trust must start
locally. This restricted design is **not TUF**, has no delegated online roles,
and does not claim compromise recovery or protection from local administrator/
same-user account compromise. Root/OS trust and an honest clock remain assumptions.
Expiry bounds stale metadata; a first installation still cannot prove that an
unexpired signed catalog is the globally newest catalog. Previously accepted
installed versions remain usable offline, including explicit rollback, subject
to locally known revocations and verification. Offline use cannot discover a new
revocation. Fresh metadata is required for new network installation/update
decisions; local recovery never lowers the remembered trust floor.

Catalog entries bind release ID, host/ABI/deployment requirements, LLVM 23.1.4,
compiler build identity, exact archive byte count/SHA-256, and bounded inventory
byte count/hash. The inventory binds every payload file's relative name, size,
hash and executable classification. Metadata sizes, archive bytes, expanded
bytes, file count, path depth and per-file sizes have independent local ceilings;
signed metadata cannot remove them. Size these ceilings from the first real
artifact inventory before accepting that release, rather than allowing arbitrary
server-supplied allocations.

Choose one curated release ZIP containing regular files/directories only.
Publisher-side packaging materializes required SDK/shared-library link aliases
as inventoried regular files. Raw upstream tar.xz SDKs are not accepted by this
first reader; never silently discard their links and call the SDK complete.
No install hooks, devices, FIFOs, symlinks, hardlinks, reparse entries, setuid bits,
ownership restoration, ACL imports or alternate data streams.

Reject absolute/drive/UNC names, `.`/`..`, backslashes in archive-relative names,
NUL, reserved Windows names, colons, trailing-dot/space aliases, case collisions,
duplicate members and normalization aliases. For the initial publisher-controlled
payload, portable ASCII relative components simplify this policy; installation
roots must still support spaces, Unicode, `#`, `%` and long supported paths.
Do not repair hostile paths by deleting characters. ZIP metadata names and the
inventory must agree; native exclusive creation also catches filesystem aliases.
ZIP integrity checks are additional to, not replacements for, SHA-256.

No newly downloaded executable runs before archive and extracted-file verification.
Use a private, non-executable staging mode until executable classifications are
applied to verified files. Verification and subsequent use must retain authority
over the same file/root identities; hashing one pathname then reopening a
replaceable pathname is insufficient.

## Transaction, download and activation

Suggested dedicated layout:

```text
<root>/control.json                    # current, previous, metadata serial/digest
<root>/writer.lock                     # OS-held lock, not mere lock-file existence
<root>/metadata/<authenticated-id>/    # catalog, signatures, inventory receipts
<root>/toolchains/<release-and-host>/   # immutable complete prefix
<root>/transactions/<exclusive-id>/     # package, payload, validation scratch
```

1. Admit root ownership/ancestry and acquire the writer lock. Random transaction
   names reduce collisions; only exclusive creation establishes ownership. Never
   use PID/FNV-derived names as authority. Keep staging on the target filesystem;
   cross-volume copying is not an atomic publication fallback.
2. Authenticate bounded metadata; persist accepted anti-replay state while
   retaining the current/previous selection. A failed later download must not
   silently discard a newer trust floor.
3. Download into a newly owned file using existing HTTP/file resources. Require
   HTTPS, raw content mode, signed exact length and bounded deadlines. Default
   proxy discovery remains off; explicit proxy use never disables TLS/signatures.
   Allow only configured release/CDN origins. `same_origin` or explicit manual
   redirect validation avoids treating all safe cross-origin redirects as an
   installer allowlist. Require the intended final status (normally 200), clean
   completion and exact counters. No resume in the first milestone.
4. Either hash independent streaming chunks as `.void` writes them or use existing
   native `download` then hash the completed file in bounded chunks. Recommend the
   latter first: it reuses accepted bulk transfer, costs a second disk read and
   requires no HTTP hashing callback API. Explicitly flush/check close; HTTP does
   neither. A truncated or failed transfer never becomes a cached verified package.
5. Decode only after whole-archive authentication. `.void` validates entry policy,
   writes below the private directory capability, meters output and verifies each
   file. Enforce completeness: no missing, extra or duplicate payload entries.
   Allocation/disk failures preserve active installations and exact error details.
6. Apply signed executable modes, derive only approved local native-toolchain
   configuration, verify resources and perform bounded smoke validation using
   absolute verified binaries and a separate owned test project. Never use
   executable `--version` output as the initial trust check.
7. Flush completed content/metadata and publish the new version with **no-replace
   same-volume directory rename**. Existing identical versions are verified and
   reused; conflicting content is rejected, never overwritten. Only after this
   succeeds may an activation record reference it.
8. Write/flush a new control file and atomically replace the single control record,
   preserving previous selection and trust floor. Synchronize namespace metadata
   as supported and report failures. A failure after replacement but before
   durability confirmation is an **uncertain commit**, not a claim that activation
   stayed old. Recovery verifies old/new retained versions and control records.

On Linux, [fsync documentation](https://www.man7.org/linux/man-pages/man2/fsync.2.html)
explicitly distinguishes file durability from directory-entry durability. Existing
`io.flush` calls `fsync`/`FlushFileBuffers`; ordinary `io.move_*` does not provide
an installation-wide crash contract. Windows and macOS require their own native
publication/recovery tests. Do not promise universal power-loss atomicity on all
filesystems from rename alone. Unsupported storage semantics fail clearly;
network shares and externally mutable roots are not initially qualified.

Rollback changes selection to an already verified retained version; it never
rewinds the metadata trust floor. If a fresh catalog explicitly revokes a release,
normal activation/rollback rejects it. Concurrent installs serialize; normal
tool runs need not hold the writer lock because published versions are not
modified or removed. Inactive abandoned transactions are reported and retained
unless cleanup can prove ownership and absence of ongoing work. Never recursively
delete a path merely because its name resembles a transaction.

## Native toolchain, bootstrap and platform boundaries

`cmake/NativeBuild.cmake` and `src/native_build.cpp:161` already support relative
driver/linker paths and `@VOID_NATIVE_DIR@` resource references, but generated
installed manifests retain configured SDK locations. **Packaging must fix the
layout, not make voidup search PATH for substitutes.**

The release producer must provide a complete inventoried compiler/server/stdlib/
Runtime/native-dependency prefix and pinned Clang/LLD/link resource closure.
Building VOIDC from source needs the full LLVM development SDK; running an AOT
manager does not. The minimum SDK subset needed by a prebuilt compiler's native
linking must be measured and qualified before trimming the package. Retaining the
complete approved SDK in the first curated package is simpler if size permits.

Any machine-local platform SDK binding is derived from an approved template and
explicitly selected/verified platform installation, before version publication.
Store derivation inputs/hashes separately from signed payload identities. Never
accept arbitrary flags from an unsigned downloaded manifest or rewrite a signed
file and falsely report its original hash. Reject paths containing newline/NUL
where the current line-oriented native manifest cannot represent them. Clear or
control toolchain overrides such as `VOID_STDLIB_ROOT`, loader search injection
and inherited compiler search variables; an explicit validated system SDK is
different from silently selecting whichever executable/library PATH supplies.

| Platform | First qualification obligation |
|---|---|
| Linux x64 | Start with the established Ubuntu 24.04/glibc userspace floor, not “all Linux.” Test on a clean machine without the source checkout/build paths. Inventory loader, libstdc++/libgcc and IDN shared dependencies, matching CA trust, RPATH and executable mount permissions. System development libraries required for native links must be explicit. |
| Windows x64 | Unicode/wide argv and paths, DACLs/reparse resistance, case/device/ADS rules, long paths, locked executables and antivirus interference. Current native building needs MSVC/Windows SDK/UCRT; select them explicitly, not a guessed PATH environment. Static `/MT` compiler use does not eliminate SDK or redistribution questions. |
| macOS ARM64 | Existing recipes use Xcode 26+ and physical SDK/vendor libc++ headers; advertised deployment target 14.0 is not proof of execution there. Qualify package runtime dependencies and native links on the declared OS floor. Test APFS case behavior, execute permissions and platform distribution signing/quarantine. |

Bootstrap is a prebuilt, independently linked AOT `voidup` distribution for each
host, carrying its required libraries/notices and embedded trust keys. A user
must authenticate that **first** executable through an already trusted channel;
an unverified program checking its own download cannot establish trust. Linux
needs a trusted external verifier/key or authenticated distribution mechanism;
Windows can use a verified publisher signature, macOS a signed/notarized delivery.
Do not instruct users to disable OS protection or execute a remote shell script.
[Apple's distribution guidance](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
is the relevant packaging reference, not a claim of current signing/notarization.
Publisher credentials and bootstrap hosting are owner decisions.
The distributed compiler must also qualify ORC/JIT under the chosen macOS
hardened-runtime and entitlement configuration. AOT manager acceptance alone
does not prove that a signed/notarized compiler can allocate executable JIT code.

JIT is a development/test mode, never the no-VOID-installed bootstrap. Tests must
inject root/arguments; `os.executable_path()` in JIT identifies voidc/the embedding
host, not the future installed manager. Independent AOT must run with the compiler
and build tree unavailable. Qualification must separately prove that launching
the installed compiler still has its explicit native-link prerequisites.

License coverage comes from [scope](../release/LICENSING_SCOPE.md),
[notice inventory](../release/NOTICE_INVENTORY.json) and actual linked contents.
VOID-original Runtime/application source can use the existing MIT grant; that
does not relicense LLVM, HTTP libraries, system SDKs or new dependencies.
Carry libsodium's [license](https://github.com/jedisct1/libsodium/blob/master/LICENSE)
and libarchive's [component terms](https://github.com/libarchive/libarchive/blob/master/COPYING),
then qualify the exact pinned source closure. Existing GNU source/replacement,
Microsoft/Apple redistribution and complete SDK-bundle questions are not resolved
by adding a ZIP. Preserve notices/source/rebuild materials and legitimate library
replacement options; do not advertise public distribution clearance here.

## One engineering milestone and acceptance

After owner decisions, implement one cohesive **VOID-native two-release install,
activation and rollback milestone**, in dependency order:

1. Application argument context plus narrow process launch; keep old entry tests
   passing. Add the private-directory/lock/publication primitives using existing
   Runtime resource and checked IO patterns.
2. Pin/qualify the narrow cryptographic and ZIP reader dependencies; add ordinary
   source facades, compiler-owned metadata/docs and JIT/AOT ABI coverage. No general
   crypto suite, archive writer or process-manager framework.
3. Build the `.void` application and private source modules for CLI, catalog policy,
   downloads, inventory validation and transactions. Establish the signed artifact
   producer/layout using existing CMake install components and native inventories.
4. Exercise two controlled signed releases end to end on each native platform;
   package the AOT manager privately for clean-host tests. Stop before publication.

Acceptance must include:

- Normal/O2 JIT and independently linked AOT parity, Debug/Release/Linux strict
  sanitizers, real 1 MiB stacks, allocation failures, GC/reload/native-resource
  cleanup and compiler-owned LSP/documentation for every new source API.
- Exact argv preservation: empty strings, spaces, quotes, trailing backslashes,
  Unicode, `#/%`, option-looking arguments and malicious shell metacharacters;
  no shell execution or inherited untrusted executable search.
- Known crypto vectors and malformed signature/keys; altered catalog/archive/file,
  wrong host/version/length, duplicate metadata, replay, expiry, clock failure,
  revoked versions and invalid cached bytes. Do not run a rejected binary.
- Local real HTTPS tests: broken trust/hostname, redirects, truncation, encoding,
  excess bytes, stalls, download errors and partial IO; metadata/archive memory
  bounded independently. No dependence on a live public release service.
- Malicious ZIP paths, aliases, links, devices, duplicates, bad CRC, ZIP64 limits,
  bombs/oversized metadata, permission attacks and concurrently changed ancestors;
  no writes outside the admitted root. Verify hashes after extraction, not only ZIP.
- Failure injection around every publication boundary, disk full, flush/close
  failure, held files, cancellation/abrupt exit and two concurrent managers.
  Previous installed contents and unrelated sentinel files remain unchanged.
  Recovery accepts only a complete verified old/new selection, or diagnoses an
  unavailable/uncertain state; it never executes a partial installation.
- Install A and B; select A; activate B; launch correct compiler and server; roll
  back to A; keep B intact. Verify native build/run, stdlib discovery and LSP from
  the installed prefix with source/build directories absent and hostile PATH/cwd.
- Real native Linux x64, Windows x64 and macOS ARM64 clean-host/bootstrap tests,
  plus signing/notice/loader-closure receipts. A Linux mock is not native acceptance.

Reuse `tests/cli/`, `tests/stage9/workflow.py`, `tests/io/`, `tests/os/`, HTTP local
servers and `tools/verify_install.py` assertions as test designs. Python may drive
test assertions; it must not implement the installed application's workflow.
Preserve exact `_crash.void` and incident evidence on abnormal compiler/runner exits.
Do not introduce routine hosted runs or new performance/release gates.

**Decisions requiring Boss:** approve the five narrow host-capability additions
and two pinned dependencies; accept the initial per-user/prebuilt/explicit-run
scope with retained versions and no self-update; select signing threshold/key
custody/bootstrap channel; approve the artifact/platform prerequisite and
redistribution scope. Golden Wrench should review especially root containment,
post-rename failure reporting and signature freshness before Keyboard starts.

**Recommendation: GO for this bounded engineering proposal after those decisions;
NO-GO for advertising an installer built solely from today's unchanged stdlib.**
No implementation or public release authorization is inferred.
