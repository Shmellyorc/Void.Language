# Compiler foundation HTTP bootstrap repair — 22 November 2026

Prepared for Sacred Wrench source review against
`05f3be9b984d4088c187ca08580eb11306cd8414`. This is a workflow repair;
the corrected hosted workflow has not been run. No commit, push or hosted
dispatch is part of this review preparation.

## Demonstrated cause

Run [45000745140](https://github.com/Shmellyorc/Void.Language/actions/runs/45000745140)
was triggered by a push of that baseline. All seven jobs installed/verified LLVM
23.1.4 successfully, then failed their configuration step before compilation or
CTest. The workflow went directly from LLVM preparation to CMake, omitting the
existing pinned HTTP preparation. Ubuntu reported `Pinned HTTP dependencies
missing`; Windows and macOS reported missing `VOID-HTTP-DEPENDENCIES.json` in
their native platform/profile prefixes. An LLVM SDK cache cannot supply these
separate HTTP prefixes.

| Failed job | Job ID | Required HTTP prefix beneath `.toolchains/http/prefix/` |
| --- | --- | --- |
| Ubuntu 24.04 Debug | 137141518141 | `linux-aws-lc-5.12.0/Debug` |
| Ubuntu 24.04 Release | 137141518078 | `linux-aws-lc-5.12.0/Release` |
| Ubuntu 24.04 Sanitizers | 137141517949 | `linux-aws-lc-5.12.0/strict` |
| Windows 2022 Debug | 137141518058 | `windows-schannel-x86_64/Debug` |
| Windows 2022 Release | 137141518071 | `windows-schannel-x86_64/Release` |
| macOS ARM64 Debug | 137141518101 | `macos-aws-lc-5.12.0-arm64/Debug` |
| macOS ARM64 Release | 137141518114 | `macos-aws-lc-5.12.0-arm64/Release` |

## Correction and scope

Only `.github/workflows/ci.yml` and this report belong to the proposed repair.
The workflow reuses the existing, unchanged HTTP/cache/host-provider step block
from `ci-link-experiment.yml`:

1. Verify the pinned LLVM SDK, then run `python tools/ci/http_bootstrap.py identity`.
2. Restore source archives, the exact native dependency profile, and pinned
   fixtures using the existing computed keys.
3. Run `python tools/ci/http_bootstrap.py bootstrap`, forwarding the dependency
   cache-hit output. Build missing dependencies with the existing native recipe;
   verify restored dependencies and fixture tools before proceeding.
4. Save verified caches, then run `python tools/ci/voidup_bootstrap.py bootstrap`.
   These host providers are also required by the current CMake configuration and
   consume the verified HTTP profile's zlib. Omitting this existing preparation
   would merely expose the next missing-manifest failure.
5. Configure, build and run the existing unfiltered tests.

`EXPERIMENT_CONFIGURATION` carries the matrix configuration into both existing
helpers; `EXPERIMENT_DIR` places their receipts under `build/foundation/`, retained
by the existing always-run artifact upload. Linux additionally installs
`build-essential` for the existing native builds. macOS configuration passes the
ARM64 architecture, selected SDK and deployment target explicitly, as required by
the native dependency imports. Sanitizers retain `RelWithDebInfo`,
`VOID_SANITIZERS=ON` and the instrumented `strict` dependency profile.

HTTP versions, source checksums, recipe identities, manifest/file verification,
platform/profile/SDK separation and sealed cache-hit checks are unchanged. Keys
are identical to the existing link workflow for matching identities, allowing
valid caches to be reused. There are no fallback restore keys. Cache saves follow
successful verification; the existing uncached host-provider policy is retained.
Windows retains Schannel and its profile-specific static zlib; macOS retains
AWS-LC, SecTrust, ICU and the selected SDK/deployment identity. No production
manifest is synthesized. No helper, compiler, CMake import, dependency pin, test
filter, trigger or permission changes are included.

## Focused verification

Local host: Linux x86_64, glibc 2.44, CMake 4.3.4, repository LLVM 23.1.4.
This is not a new Ubuntu hosted qualification.

- `python3 -B tests/net/http/ci_bootstrap_test.py`: **17/17 pass**, including
  profile/SDK separation, missing or mismatched seals, tampering, fixture policy
  and warm paths that do not build dependencies.
- `python3 -B tools/ci/http_bootstrap.py verify --configuration <configuration>
  --evidence <receipt-directory>`: **pass** for `Debug`, `Release`, `Sanitizers`.
  Each verifies the real 167-file native prefix plus pinned fixtures/certificate
  tools. These are read-only verification runs, not dependency rebuilds.
- `python3 -B tools/ci/voidup_bootstrap.py verify --configuration <configuration>
  --evidence <receipt-directory>`: **pass** for all three Linux configurations,
  including the actual host receipts and their exact HTTP zlib identity.
- Focused workflow checks: **all seven configurations pass** matrix/profile,
  preparation order, cache-hit forwarding, exact existing bootstrap step block,
  cache-save ordering, strict instrumentation and explicit Apple SDK argument
  checks. Space-containing SDK arguments are checked with a command model;
  Windows PowerShell execution is not inferred from that model.
- Minimal CMake consumers run the unchanged production HTTP import and verifier
  against actual Linux `Debug`, `Release`, `strict` prefixes: **3/3 pass**.
  Temporary check projects are removed automatically; no compiler matrix runs.
- `git diff --check`: **pass** for the proposed scope. The eight pre-existing
  editor file hashes and protected HTTP infrastructure hashes remain unchanged.

Original job metadata and timestamped configure-log excerpts, local commands,
verification receipts, check script, patch and SHA-256 inventory are retained in
`Void.Language-archives/ci-foundation-http-bootstrap-2026-11-22/` beside the
repository. The patch and inventory describe only this repair; concurrent
macOS VOIDUP workflow work and unrelated editor changes are excluded.

## Native evidence and qualification boundary

[Existing HTTP acceptance](ACCEPTANCE.md) records all seven native jobs passing
at `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`, run
[41815539463](https://github.com/Shmellyorc/Void.Language/actions/runs/41815539463),
using Compiler link experiment `thinlto-cache` and LLVM 23.1.4. Existing Windows
VOIDUP Debug/Release evidence remains in
[the Windows completion report](../../voidup/WINDOWS_COMPLETION.md).
Those are historical native results, not execution of this changed workflow.
No new Windows Server 2022 or macOS run, cold/warm hosted timing or full compiler
qualification is claimed. Current recorded platform/public-release limitations
remain in force. The independent macOS VOIDUP CI work is outside this repair.

## Integration and CI-skip boundary

Source review must precede the scoped commit/push. Proposed maintenance title:
`Repair Compiler foundation dependency bootstrap [skip ci]`.

[GitHub's supported skip mechanism](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/skip-workflow-runs)
applies to `push` and `pull_request`. The parsed trigger inventory confirms
Compiler foundation is the only current automatic workflow and uses exactly
those events, plus `workflow_dispatch`; the other workflows use manual dispatch
or reusable calls. The trigger inventory must be rechecked immediately before
integration because concurrent work can change it. No repository-wide workflow
disablement or permanent skip condition is introduced. Future ordinary commits
retain normal CI behavior. Skip markers do not block manual dispatch; none is
authorized here. Skipped required checks can remain pending under branch rules.

After acceptance: recheck HEAD/remote main, candidate hashes and exact staged
scope; run `git diff --cached --check`; verify the emitted commit actually contains
`[skip ci]`; push normally. Then independently confirm the remote SHA and query
Actions runs for that exact SHA/event to check that no automatic run was created.
Server-side skipping cannot be claimed before that authorized push. If hosted
qualification is later authorized, Boss prefers the existing Compiler link
experiment with `thinlto-cache`; this repair dispatches nothing.
