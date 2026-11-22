# G96 — VOIDUP public installer and first-run experience

Engineering review candidate, 21 November 2026. Baseline:
`1080440953847338807d5005d162ad34841de701`.

The native first-install route passes 20 consumer checks on the local Linux host
with authenticated engineering releases and the completed permission changes.
This is not a public release or final acceptance. Production keys remain
unapproved; full native Windows/macOS first-install qualification is outstanding.
The exact 28-file G96/shared inventory and verification evidence are in
`~/Documents/Void.Language-archives/g96-2026-11-21/changed-files.json` and
`qualification.json`. The separate permission inventory remains in
`build/permission-dx-review/changed-files.json`; its 37 files are dependencies and
overlap context, not all G96-authored changes. Editor work is excluded.

The independent integration review below supersedes the original handoff status.
Its checkout starts at accepted F5 commit
`433919d84eca7de40149cdd1742ed6ec33941eb8`, with the G95 and unified filesystem
commits preserved. Original qualification identities remain historical evidence;
the review records its own corrected sources and focused checks separately.

## Implemented behavior

The existing native manager now accepts a bare invocation, `setup`, `update`, or
`install latest` and selects the highest compatible, unrevoked numeric stable
version from authenticated metadata. It verifies, installs and activates through
the existing transaction, inventory, recovery and rollback paths. Explicit release
IDs and private catalog overrides remain supported.

Selection uses an optional managed `selection` object containing required version
and channel fields. Original schema-1 catalogs and installed records without this
object remain usable by explicit ID, verify and run; no version is guessed from
filenames. Automatic selection rejects missing/ambiguous stable versions and
prevents silent downgrade below the selected known stable version.

The official GitHub Release catalog is built in. Checked-in engineering trust
cannot use it automatically: a producer overlay must supply an isolated test
source or separately approved production public roots. The signature domain,
Ed25519 threshold and native authentication remain unchanged. GitHub asset
transport permits only the exact repository's HTTPS release endpoint handing off
once to the approved HTTPS asset CDN; private channels retain same-origin/no-
redirect behavior. Final catalog signatures, hashes and full inventories remain
mandatory. Package transfers have a one-hour total budget with 30-second
connect/header/read budgets; the existing explicit timeout overrides both.

A small native platform launcher retains the manager and exposes stable `voidc`,
`voidls` and `voidup` proxies. Every compiler/server launch delegates to native
VOIDUP's existing verified `run` operation. Linux shell-profile integration and
Windows per-user registry integration are separate from toolchain authentication.
`--no-path` skips integration. Removing the original download does not remove the
retained manager. There is no second installer, verifier or toolchain store.

The completed permission work replaces installation storage's private-directory
capability with ordinary `io.directory_handle` capabilities. Existing modes and
ACLs do not need repair; native held-identity, no-follow, locking, publication and
authenticated-content checks remain. The Linux extraction wrapper uses umask 077
for newly created installer-owned data. Explicit signing-key confidentiality is
unchanged. Source hashes from the final first-install receipt match the checkout's
manager, standard-library and launcher sources.

The G96 follow-up fixes profile ACL preservation. A focused before/after test
demonstrated loss of an existing named ACL and addition of an inherited grant to
a profile originally without an ACL. Replacement now copies the held original's
access ACL, including preserving its absence, before publication. Linux uses
descriptor-based xattrs; the macOS branch uses `fcopyfile(COPYFILE_ACL)` and remains
unqualified natively. The complete installer also verifies a 0664 profile's named
ACL before and after setup/retry. No new runtime dependency is introduced.

The single existing package producer now has native Windows SDK/executable
handling. Download packaging produces a Linux self-extracting `.run` or Windows
ZIP with `Install VOID.cmd`. macOS complete-payload/dependency relocation remains
unfinished and production fails closed. Producer Python/CMake/cryptography are
not consumer installer dependencies.

Manual CI preparation and publication workflows are prepared. Preparation uses
ephemeral engineering roots for native tests and public inputs for unsigned
production artifacts. Offline custodians sign the exact catalog; publication
verifies threshold signatures and artifact identities before publishing immutable
version assets, then updates stable metadata last. No workflow was dispatched.

## Verified snapshot and retained evidence

Evidence is in `~/Documents/Void.Language-archives/g96-2026-11-21/`.

| Check | Actual result |
| --- | --- |
| Packaged Linux first install | 20 consumer cases passed, including automatic numeric selection, install/activation, new-terminal discovery without Python/CMake/ambient LLVM, source project JIT/native build and execution, LSP initialize/shutdown, download-folder independence, ordinary shared installation modes, idempotent setup, authenticated update and refusal cases |
| Original signed-catalog compatibility | Automatic selection refuses missing metadata; explicit install/activate, verify and compiler run pass |
| Hostile authenticated metadata/packages | 30 rejection cases plus TLS/untrusted-root refusal passed |
| Partial transfer | Actual 1 MiB progress, timeout, retained trust floor/partial data and lock reuse passed |
| Interrupted transfer | Actual 1 MiB partial download, concurrent writer refusal, SIGKILL, retained abandoned data and authenticated full retry passed |
| Focused Release and fatal sanitizers | `voidup_smoke` and `voidup_presentation`: 2/2 each, hard/soft 1 MiB stacks; fatal ASan/UBSan and leak checks enabled |
| Current combined-source policy | Numeric ordering, revocation, legacy identity, metadata equality and strict GitHub redirect-boundary vectors passed in Release and fatal-sanitizer builds at a 1 MiB stack |
| Profile ACL correction | Before/after reproduction passes; named ACL, absent ACL and existing contents preserved. Full installation also preserves a 0664 named-ACL profile |
| Offline signing tooling | Five checks passed: independent endorsements, duplicate signer, altered exact bytes, mismatched key and engineering/production separation |
| Workflow preparation | Both YAML files and eight Python sources parsed; this is not hosted CI execution |
| Producer bounds | Traversal and identifiers beyond the native 80-byte limit rejected before output creation |

The complete first-install evidence is
`post-permission-first-run/first-run-summary.json` and `first-run-cases.json`.
The installed fixture compiler build identity was
`43e02ff5d31ac59e300683045d90b0f3cd3f5163add30632e7aa0705c2e6dab5`.
The tested bootstrap manager hash was
`549c6cead3cd6539ad6cb9ddfb4f4468648c2a43e819a302078ae64091e9576f`;
launcher hash was
`8ee6be06bf0a05d8b02f744ac12d848b7675bd11705ab496858189dc88cff5cc`.

The actual compressed download was **22,082,749 bytes (21.06 MiB)**; runnable
expanded payload was **39,184,653 bytes (37.37 MiB)**. The launcher itself was
75,664 bytes. Required notices and library aliases are included. These are local
development artifacts, not Ubuntu-floor production binaries.

The final first-install receipt includes native dependency measurements for both
tested binaries. They require
the system ELF loader, libc, libstdc++, libgcc and libm, plus bundled libidn2 and
libunistring aliases. The local manager imports GLIBC through 2.43 and GLIBCXX
through 3.4.35; it cannot establish Ubuntu 24.04 compatibility. Linux extraction
also needs standard sh/mktemp/tail/tar/gzip/rm utilities. The bootstrap is not
standalone. Measure final ELF/PE dependencies again on actual production runners.

Retain original failed attempts. In particular, required top-level selection
strings initially broke old catalog decoding; the optional managed object fixes
that without relaxing strict decoding. An early legacy fixture used links outside
the test server's admitted root and returned 404; its rejection pass is discarded
as qualification. Ordinary task-owned fixture copies corrected that setup.
`post-permission-security.log` and its lossless receipts contain the current valid
reruns against the completed Runtime changes. The timeout assertion accepts both
enum display spellings
while still requiring the exact timeout identity and 1 MiB progress.

The earlier `current-policy.log` failure occurred while the permission work was
replacing directory APIs. The frozen checkpoint passed separately. After that
work completed, matching compiler/Runtime builds and the current
`post-permission-policy.log` and `post-permission-strict-policy.log` pass. The full
current installer run also passes; the in-progress API mismatch is superseded.

## Native scope and remaining publication work

Full native Windows first-install and user-PATH qualification remain
**UNQUALIFIED**. An earlier G96 Windows launcher compiled and executed its help path
natively with LLVM/Clang/LLD 23.1.4, `/MT` and a 1 MiB stack; this exercises neither
complete installation nor registry integration. Isolated test files were cleaned.
No macOS end-to-end execution is claimed. Windows CI contains an
explicit disposable-user PATH test with restoration; local native work does not
modify the existing user registry, tools or profiles. Credentials and identifying
guest details are not retained in source or engineering evidence.

Production requires Boss-approved three independent public keys/IDs, actual
native support-floor CI execution and complete payload qualification, final
dependency/notice review, initial bootstrap trust announcement and separately
authorized publication. No production public-key file exists in this candidate.
No engineering key may enter a publicly distributed installer. The free offline
generation, storage, enrollment and independent 2-of-3 signing procedure is in
[PRODUCTION_TRUST.md](../voidup/PRODUCTION_TRUST.md).

There is no automatic manager self-update, key rotation, installed-version pruning
or claim to eliminate OS executable reputation prompts. Unix PATH integration is
qualified for Bash; other shells need their own checks. Native application linking
still needs declared OS development headers/CRT, independently of JIT use.

At the original implementation handoff, editor/F5 work and permission-only source
remained separate. Non-G96 dirty source
hashes were checked against the beginning of this follow-up; no differences were
introduced. Six G96 files overlap the permission inventory and are explicitly
attributed in the final inventory. This chat has staged, committed, pushed,
installed into a real prefix and published nothing. Isolated installer fixtures
were disposed normally. The index remains empty and HEAD remains the baseline.
The installed compiler changed externally during earlier concurrent work;
whole-toolchain hash preservation since the beginning of G96 is not claimed.

## Reproduction and closeout

Use a matching pinned LLVM/Clang/LLD 23.1.4 build, matching stdlib and compiler
licenses. Set `TMPDIR` to the task-owned archive scratch directory and retained
evidence outside scratch. Run `tests/voidup/first_run.py` with `--build`, `--sdk`,
`--licenses` and `--evidence`; CI defaults to in-memory ephemeral engineering keys.
Run `tests/voidup/policy.py /matching/voidc` and focused native smoke/presentation
checks with 1 MiB stacks. Legacy rejection/timeout/interruption checks require a
freshly signed catalog and ordinary files contained by the local HTTPS fixture.

The completed permission chat and its engineering report were reviewed at Boss's
signal. The installer overlap was corrected and verified with matching inputs.
The candidate is ready for independent Golden Wrench review within the recorded
Linux/native limits. No independent PASS, production readiness or integration
authorization is claimed. No extra performance campaign or publication occurred.

Transport decisions reference [GitHub release-asset documentation](https://docs.github.com/en/rest/releases/assets#get-a-release-asset)
and [Microsoft user-environment notification](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settingchange).
A read-only HEAD against the pinned public LLVM release asset confirmed the
specific GitHub CDN handoff shape; it was not a VOID production-channel test.
Profile ACL preservation follows the descriptor-based
[Linux xattr interfaces](https://man7.org/linux/man-pages/man2/setxattr.2.html) and
[Apple copyfile ACL interface](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/copyfile.3.html).

## Independent integration review — 21 November 2026

Review scope is the original 28-file inventory, actual source, retained successful
and failed qualification records, and compatibility with accepted commits
`ad0df4f78c593a7608b7663fa1f1921c9b3e441d` and
`433919d84eca7de40149cdd1742ed6ec33941eb8`. Both descend from the accepted G95
baseline above. No new milestone or production acceptance is introduced.

Twenty-seven of the 28 inventory entries initially matched their original SHA-256 exactly.
The remaining file, `docs/README.md`, matched after subtracting only the accepted
unified-workflow navigation link. The review's initial checkout, original source
copies, final 32-file source inventory and tested binary identities are retained
in `/home/shmellyorc/Documents/Void.Language-archives/g96-independent-review-2026-11-21/`.
The four added files are focused regressions, not new implementation scope.

**Verdict: ready for Sacred Wrench final acceptance within the recorded Linux
engineering scope.** Five concrete findings are corrected. The packaged follow-up
passes 21 consumer checks, including the accepted editor-consent harness through
the installed proxy. Focused final-source regressions pass. This is not public
production acceptance or authorization to integrate.

### Concrete findings and corrections

| Finding | Reproduction and affected behavior | Required outcome and correction |
| --- | --- | --- |
| Lost developer/consent environment | `before-environment-valid.log` records loss of CI, XDG and desktop variables. The accepted compiler checks CI before native consent and uses user data for saved approvals. | Preserve those explicit OS/session inputs through verified `run`; keep loader/compiler overrides and caller PATH excluded. |
| Editor cancellation stopped only the proxy | `before-cancellation.log` records both the manager and compiler surrogate surviving SIGTERM. | Redirected Unix launches relay cancellation to their child process group. Terminal launches retain their foreground group; SIGTERM, SIGINT and terminal Ctrl-C regressions cover the distinction. |
| Explicit manager root became a duplicate option | `before-proxy-root.log` records two injected `--root` options. The native parser rejects duplicates, breaking the documented advanced command. | Inject the proxy's root only when no explicit global root is supplied. Forwarded compiler operands remain literal. |
| ACL bytes retained with the wrong owning group | `before-profile-group.log` records group 4 becoming 1000 on both profiles, despite identical mode/ACL bytes. This changes GROUP_OBJ access. | Preserve original owner/group before restoring mode and ACL. The OS must permit preservation; failure leaves the original profile intact and `--no-path` remains available. No chmod ceremony is added. |
| Maximum catalog lifetime exceeded native bounds | `before-release-qualified.log` records a 2,592,060-second interval for the accepted `--valid-days 30` argument. Native verification permits 2,592,000 seconds. | Calculate expiry from `issued`, including the existing 60-second clock allowance, so the maximum advertised producer input remains installable. |

Correction inventory: `apps/voidup/manager.void`, `apps/voidup/launcher.cpp`,
`tools/voidup_release.py`, `tools/ci/voidup_release.py`,
`tests/voidup/first_run.py`, the new `tests/voidup/developer_environment.py`,
`launcher.py`, `profile.py`, `release.py`, and this report plus
`docs/voidup/USAGE.md` and `RELEASE_PRODUCTION.md`. Original signing, authenticated
transport, inventory, publication, recovery and revocation mechanisms are retained.

The process correction follows the established direct-child limitation documented
by [Node](https://nodejs.org/api/child_process.html) and uses existing
[process groups](https://www.man7.org/linux/man-pages/man2/setpgid.2.html) and
[group signaling](https://www.man7.org/linux/man-pages/man2/kill.2.html).
The storage correction preserves the locations defined by the
[XDG specification](https://specifications.freedesktop.org/basedir/).
Profile ownership is preserved with the existing descriptor and the OS's normal
[fchown permissions](https://www.man7.org/linux/man-pages/man2/fchown.2.html).
These are bounded compatibility repairs, not an additional security framework.

### Evidence and compatibility boundaries

Original evidence remains valid for its recorded sources: 20 Linux consumer
cases; Release and fatal ASan/UBSan/leak smoke/presentation 2/2 each; numeric and
redirect policy; 30 hostile authenticated cases plus TLS/unknown-root refusal;
exact 1 MiB timeout/interruption with trust-floor and lock recovery; five offline
signing checks; two producer identifier checks; and the earlier named/absent ACL
repair. Its failed required-version decoding, 404 fixture, missing-license fixture
and in-progress directory-API attempts remain retained and are not counted as
passing qualification. The new review also retains its discarded environment
probe drafts separately from the valid product reproduction.

The focused review exercises the environment regression in Release and with the
existing fatal-sanitizer compiler, native cancellation/root forwarding with fatal
ASan/UBSan/leak settings, supplementary-group/named/absent ACL preservation and
idempotent retry, and 1/7/30-day producer boundaries. Native launcher/profile and
environment executions use hard/soft 1 MiB stacks. The producer qualification
entry point now includes these regressions. No hosted workflow was dispatched.

The packaged first-install follow-up uses the accepted F5 compiler identity
`5390ab043d8d8958f863c7bfee8801f3bd719ba697cf576597436da3ef72922f`
and adds the existing compiler-owned editor-consent harness through the installed
proxy. It checks explicit denial, EOF/wrong identity, approval, unchanged reuse,
review-time provider changes, CI refusal and the pre-application phase seal in
disposable user storage. It does not replace the existing real VS Code/LLDB
acceptance or claim another GUI-debugging run.

The follow-up's packaged launcher source is SHA-256
`08e583102ae0a3206c6400fe7ea4639b9dce26b730b5c00e7a642ae878ee3405`.
It includes cancellation repair; the subsequent explicit-root and profile-owner
repairs are qualified by the focused native harnesses against the final launcher.
That tested source is retained in `first-run-source/`; final source hashes and
both patch boundaries are explicit. The expensive complete installer campaign is
not repeated for those two isolated launcher corrections.
The follow-up download is 22,158,893 bytes and its expanded payload is 39,788,039
bytes; these remain local-host engineering measurements.

The unified directory capabilities, executable classification, filesystem ACL
behavior, package/editor refresh and G95 cache implementation are preserved.
Compiler, Runtime, stdlib, language-server and extension source receive no review
corrections. The permanent `~/.local/bin/voidc`, `voidls`, installed extension and
debugger match the accepted F5 integration receipt. Rebuilding/replacing those
permanent tools is therefore unnecessary. All installer prefixes are disposable;
unrelated editor working changes and recovery artifacts are preserved.

### Actual public installation boundary and handoff

The demonstrated route is an authenticated **engineering** Linux installer:
automatic stable selection, installation/activation, PATH discovery, compiler/LSP
use, verified launches and retained-manager operation. Updates reuse the native
transaction and activation paths; the original suite covers verification,
rollback and failed-transfer recovery. The first-install update check refreshes
authenticated metadata for the same installed release; it is not evidence of a
new public version rollout. A new terminal is needed after PATH integration.

The public download-to-coding journey still requires approved independent
production roots, public bootstrap and catalog publication, and actual native
support-floor production builds with final payload/ABI/dependency/notice review.
Local GLIBC 2.43/GLIBCXX 3.4.35 artifacts do not establish the Ubuntu 24.04 floor.
The bootstrap requires the measured system runtime and extraction utilities.
Native application linking/F5 builds still require the declared OS development
headers/CRT/SDK; the installer does not provision those OS packages. Installation
does not grant native-code approval.

Windows complete install, registry PATH and process-tree cancellation need native
qualification. macOS complete payload/relocation and full native installation
remain unfinished/unqualified. Initial bootstrap trust announcement and separately
authorized publication remain required. No automatic manager self-update, key
rotation, pruning, other-shell qualification or OS reputation bypass is claimed.

Review handoff: `review.json`, `reviewed-sources.json`, `corrected-files.json`,
`corrections.patch`, `integration.patch`, original/focused receipts and source
snapshots are in the review archive directory. The accompanying ordinary ZIP
contains a SHA-256 restoration manifest and verified CRCs for Sacred Wrench review.
Final verdict and completed test counts are recorded in `review.json`; Boss/Sacred
Wrench final acceptance and Git integration remain separate. Nothing is staged,
committed, pushed, published or installed into a real prefix by this review.
