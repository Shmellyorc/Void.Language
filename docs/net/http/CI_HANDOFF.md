# Native HTTP CI preparation — Golden Wrench handoff

## Current Windows repair candidate — 3 November 2026

[Schannel store acceptance](WINDOWS_SCHANNEL_ACCEPTANCE.md) replaces the unsupported
Windows positive-P12 requirement with real caller-provisioned store mTLS, retains
P12 rejection/parser negatives and corrects capabilities. New Windows-only
`http_windows_store` proves RSA/ECDSA through public source JIT/O2/independent AOT,
exact peer DER, GC/cancellation/cleanup. `http_native` also covers active Runtime
shutdown during authenticated TLS. Effective local Windows 10 Debug/Release
focused acceptance is 34/34; strict Windows configuration fails its full-runtime
probe without a reduced substitute. Original hosted failure remains historical.

Boss should manually run the unchanged seven-job `ci-link-experiment.yml`, mode
`thinlto-cache`, after independent review. Require full Windows Server 2022 Debug/
Release HTTP/URL tests including both credential-owning tests and successful
credential inventory/deletion evidence, plus all existing Ubuntu/macOS and strict
gates. The CTest resource lock serializes user-store inventory fixtures. No staging,
commit, push or dispatch occurred. Earlier dated preparation records follow.

31 October 2026. Baseline `490df04edee92756c20c336c0f32114476167941`;
LLVM/Clang/LLD 23.1.4. **Prepared for source review; hosted native acceptance is
not claimed.** No staging, commits, branches, pushes, hosted dispatch, publication
or protected root README edits were performed.

## Windows credential feasibility — source-only result

[Feasibility report](WINDOWS_MTLS_FEASIBILITY.md): Boss conditionally approved
temporary exportability and crash residue for experiments, not production. No
disposable native Windows session was available. Source review identifies required
durable-store-marker and stable-lock refinements; native import, non-exportability,
Schannel and recovery results remain UNVERIFIED. Production and Linux/macOS are
unchanged. Stop for Golden Wrench review; no hosted run requested.

## Windows-only credential architecture proposal

[Windows mTLS credential design](WINDOWS_MTLS_CREDENTIAL_DESIGN.md) proposes one
explicit identity case, controlled user-scoped CNG ownership and the already
implemented exact Windows store selector; no curl correction is currently
justified. Temporary in-memory exportability and crash/cleanup residuals require
explicit review. Production behavior and all Linux/macOS code remain unchanged.
**Design only: stop for Golden Wrench architecture review, not a hosted run.**

## Windows Schannel security review — 3 November 2026

[CI 41563174932 review](CI41563174932_SCHANNEL_REVIEW.md) confirms Windows
Debug/Release pass 340/341 tests; the only failure is the P12 identity handshake.
Both now capture `next InitializeSecurityContext`, `SEC_E_INTERNAL_ERROR
(0x80090304)`, and peer EOF without authenticated identity. Source and Microsoft
contracts strongly implicate the non-persisted-key/LSASS boundary. No safe
contract-preserving correction is established. A concrete explicit opt-in key
persistence design is proposed for Golden Wrench/Boss architecture review;
no production/security change is implemented. Windows mTLS remains failed.
The five other hosted configurations passed, including Ubuntu disk preflight.

## Ubuntu LLVM cache disk repair review

[Disk-space repair](CI_LLVM_DISK_REPAIR.md) adds bounded unused-SDK cleanup before
LLVM restoration and measured capacity preflights, preserving all seven jobs,
pins and acceptance tests. Local workflow/policy checks pass; actual hosted disk
recovery and full-build headroom remain UNVERIFIED. Windows mTLS diagnostics and
its unresolved native acceptance requirements are unchanged.

## Latest Windows review boundary

[CI 41533124522 triage](CI41533124522_TRIAGE.md) records five successful hosted
platform configurations and two remaining Windows failures. The path test has
a focused correction; mTLS has stronger diagnostics/negative controls but no
proven transport repair. Local Release/Debug/strict focused tests pass 6/6 each.
Windows acceptance remains UNVERIFIED; Boss controls native diagnostic execution.
No workflow dispatch or dependency/policy change is part of this handoff.

## Workflow and bootstrap

Only the existing [fast workflow](../../../.github/workflows/ci-link-experiment.yml)
is integrated. Its seven entries remain unchanged:

| Runner | Configurations | HTTP profiles |
| --- | --- | --- |
| Ubuntu 24.04 x86_64 | Release, Debug, Sanitizers | Release, Debug, strict |
| Windows Server 2022 x86_64 | Release, Debug | Release, Debug |
| macOS ARM64 | Release, Debug | Release, Debug |

The dispatch default is now `thinlto-cache`; the control comparison remains
available. The complete LLVM SDK cache, initial empty ThinLTO cache assertion,
probe/build/warm-relink measurements, three-way build parallelism, unfiltered
CTest/JUnit command, small-stack frontend regression, extension/editor tests and
artifact steps remain. This does not use the compiler-foundation workflow.

After the native MSVC environment/macOS architecture guard and verified LLVM SDK,
`tools/ci/http_bootstrap.py identity` checks the authoritative curl inventory and
selects the exact native prefix/profile. Three cache restores precede bootstrap;
bootstrap precedes CMake. macOS configuration explicitly uses the same physical
SDK, ARM64 architecture and deployment minimum (14.0 by default) as its dependencies.
All existing native feature and compile/link/run checks remain authoritative.

`bootstrap` invokes the tracked setup scripts on a prefix miss and verifies every
restored prefix before configuration. A cache miss with an existing prefix fails;
a hit without the matching CI identity seal fails. The seal binds the current
host/toolchain/recipe identity to the setup manifest digest. The existing verifier
then checks profile, platform/architecture, current pins, declared recipe hashes,
all inventoried files, shared-library aliases and corresponding-source archives.
macOS SDK/deployment metadata must match as well. Old local Linux manifests remain
readable by the separate read-only `verify` phase; an unsealed prefix cannot be
adopted as a CI cache hit.

Linux retains AWS-LC and full strict instrumentation. Windows retains Schannel,
WinIDN, native stores and static `/MT`, including Debug's existing iterator ABI.
macOS retains the approved AWS-LC 5.12.0 source, SecTrust and Apple IDN. Curl still
builds directly from tracked `third_party/curl/`; all four approved corrections
verify. No production patch application, ambient curl or TLS fallback is added.

## Cache identity and fixture policy

| Cache | Identity / sharing |
| --- | --- |
| Source archives | OS and production pin-set hash; shared across configurations, never compiled objects |
| Native prefix | LLVM 23.1.4, OS, architecture, profile, official SDK receipt, relevant compiler/recipe/CMake/PSL/curl provenance hashes, CMake version and native ABI details |
| Fixtures | OS/architecture, Python major/minor, wheel pins and setup recipe; Windows also binds SDK/MSVC/Windows SDK identity and the production pins used for its separate certificate tool |

Native ABI details include Linux libc, Windows toolset/SDK/target architecture,
or macOS physical SDK/version, Xcode and deployment minimum. Prefix keys differ
for all seven jobs. Fixtures and sources share keys across Debug/Release where
their bytes are the same; link experiment mode does not duplicate dependency keys.
No fuzzy restore keys or cross-OS binary reuse is used.

[Restore/save actions](https://github.com/actions/cache/blob/v5/README.md)
save each cache only after successful bootstrap verification, before compiler
configuration. A later native compiler/test failure can therefore reuse the
verified dependencies. A first parallel cold run can still build the common
Windows certificate tool twice before either job publishes its cache; preventing
that would require changing job dependencies. No additional job was added.

Python fixture imports are recreated from checksum-verified h2/hpack/hyperframe
wheels, removing obsolete cached imports. Windows's Release-built AWS-LC
certificate CLI is test-only, shared across its two configurations and separate
from the Schannel link closure. Its cache checks recipe/pin/LLVM/MSVC/SDK/CRT
identity and every inventoried file before skipping the build.

The pinned AWS-LC source requires NASM for native Windows x64 builds
(`BUILDING.md` and `crypto/CMakeLists.txt` in the verified archive). The
[current Windows 2022 inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)
does not list NASM. The test setup therefore downloads the official **NASM
2.16.03 win64 ZIP**, selects both `NASM_EXECUTABLE` and `CMAKE_ASM_NASM_COMPILER`
explicitly, checks its runtime version, retains its license and caches it with
the fixtures. No assembly is disabled and no production dependency pin changes.

- [Official archive](https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip)
- Exact measured archive SHA-256:
  `3ee4782247bcb874378d02f7eab4e294a84d3d15f3f6ee2de2f47a46aa7226e6`
- [Acquisition record](evidence/ci-preparation/nasm-acquisition.json): downloaded,
  hashed and inspected on Linux; the Windows executable was not run here.

## Coverage and diagnostics

After the normal build, `test-plan` requires all 17 common HTTP/URL CTests plus
the AWS-LC corpus on non-Windows hosts. It records their commands, timeouts and
registration count. It does not mark registration as execution. The existing
CTest command remains completely unfiltered, covering the native real h1/h2,
TLS negatives, DNS/proxy/auth/cookies, flow control, cancellation/shutdown,
streaming/files/multipart, source JIT/O2/independent AOT, install/relocation and
compiler-owned LSP tests. All other compiler tests still run.

Two separately named existing CTests are expensive: `http_tls_memory` has a
1,300-second timeout and performs 256 verified handshakes against 983 distinct
roots plus negative peers; `http_source_faults` sweeps source allocation failures.
They are retained in final acceptance, not excluded from the fast workflow. The
AWS-LC corpus is intentionally not registered for Schannel by the existing CMake
contract; that is not a Windows allocator-audit pass. Earlier local corpus times
were 48.07 s Release, 347.41 s Debug and 757.63 s strict. Those are observations,
not hosted predictions. No corpus, multi-GiB campaign or fault sweep was rerun
as part of this CI preparation.

Bootstrap identity, commands, wall times, verified manifests and fresh setup
receipts go into `build/link-experiment/http/`. Failed dependency/certificate
configuration also preserves CMake caches/configuration logs, autotools logs and
incremental certificate commands. Existing `always()` uploads retain that directory,
JUnit/Testing records and the added HTTP/URL evidence directories. The existing
link diagnostics and editor artifacts remain intact. No failed or unexecuted
native check is converted to success.

## Local verification and measured cost

[Verification summary](evidence/ci-preparation/verification.json) and
[workflow checks](evidence/ci-preparation/workflow-validation.json) record:

- Actionlint **1.7.12** passes; YAML/matrix/order/cache-pair validation passes.
  All seven matrix entries, SDK cache, original build/test/relink commands,
  job environment, timeout and editor steps are preserved.
- **17/17 cache policy tests** pass, including serialized identity, profile/ABI
  separation, wrong/missing seals, changed manifests, certificate corruption,
  obsolete imports and warm paths that do not compile dependencies.
  Windows cache branches are simulated policy tests, not native execution.
- **9/9 focused Linux Release CTests** pass in 17.34 s: native HTTP, wire,
  DNS, deadlines, HTTP/URL IR, URL kernel and HTTP/URL LSP.
  [JUnit](evidence/ci-preparation/linux-focused.xml).
- All **167 files in each Linux Release/Debug/strict prefix** verify, and the
  pinned fixture modules and certificate executables are available.
- A new disposable Linux source/build/prefix replica performs a complete Release
  dependency build in **184.97 s**, three workers, with verified SDK, source
  archives and wheel downloads already present. A real sealed hit takes
  **0.51 s**, recompiles no dependency and refreshes/verifies fixtures.
  Deliberate header corruption fails before fixture setup, then the original
  bytes are restored. [Timing and negative control](evidence/ci-preparation/bootstrap-timing.json).
- The existing configure helper passes locally with Ninja 1.13.2,
  `thinlto-cache`, the newly built prefix and pinned LLVM/Clang/LLD. Only the
  local build/prefix paths were adapted for isolation. Configure takes 15.86 s;
  the ORC probe returns successfully and all 18 HTTP/URL tests are registered
  among 389 total. [Configure metrics](evidence/ci-preparation/linux-configure/configure.json).

The timing host is Linux x86_64/glibc 2.44 on the local Ryzen 7 1700X, not the
Ubuntu 24.04 hosted image. Cache transfer, uncached downloads and Windows/macOS
bootstrap times are unmeasured. Budget minutes for a cold native build; warm
verification is much smaller locally. Required TLS/failure tests can dominate
end-to-end time. There is no promised hosted wall-clock target.

## Native risks and review boundary

Actual Windows/macOS bootstrap, compilation and execution remain unverified.
Expected first-run failure categories to inspect are native archive names and
CRT/link closure, Schannel custom-root/store/P12 behavior, ALPN/proxy/DNS/IPv6,
Apple SecTrust/IDN/SDK compatibility and the existing stop-token runtime probe.
An older Xcode without the required libc++ support must fail; verification and
sanitizers must not be relaxed. The pinned Windows NASM/CLI build is prepared,
not source-approved by independent review or proven executable on this host.
There is no known remaining missing bootstrap step identified locally, and no
native success is inferred from Linux or the policy models.

Seven green jobs would establish their executed native coverage. They would not
by themselves close the separate provider worst-case memory certification or
constitute independent Golden Wrench source approval. Those boundaries remain
in [the HTTP handoff](HANDOFF.md) and [native platform record](NATIVE_PLATFORMS.md).

## Changed-file inventory and hosted acceptance

| File | Change |
| --- | --- |
| `.github/workflows/ci-link-experiment.yml` | Same seven jobs; default cache mode, ordered bootstrap/verified caches, explicit Apple SDK policy, test registration and HTTP evidence uploads |
| `tools/ci/http_bootstrap.py` | Profile/cache identity, sealed restore verification, setup orchestration, fixture preflight and diagnostic/test-plan records |
| `tools/setup_http_tests.py` | Recreate checked Python imports; verify/reuse Windows certificate tool; pinned test-only NASM; incremental failed-build records |
| `tests/net/http/ci_bootstrap_test.py` | Focused cache and fixture policy regression tests |
| `docs/net/http/NATIVE_PLATFORMS.md` | Link this subsequent CI preparation; retain native execution boundary |
| `docs/net/http/CI_HANDOFF.md` | Scope, verification, risks and acceptance instructions |
| `docs/net/http/evidence/ci-preparation/` | Local receipts, JUnit, source/hash inventory and validation results |

[Exact changed-file/hash inventory](evidence/ci-preparation/changed-files.json).
Golden Wrench reviews these changes before Boss commits/pushes. After approval,
Boss selects an approved branch/tag resolving to the **new reviewed commit**
and dispatches the existing workflow ([CLI ref semantics](https://cli.github.com/manual/gh_workflow_run)):

```sh
gh workflow run ci-link-experiment.yml --ref <reviewed-branch-or-tag> -f mode=thinlto-cache
```

Check the resulting run's `head_sha` against the reviewed commit; the old baseline
does not contain this integration. Verify seven completed successful jobs, all CTests/JUnit outcomes,
HTTP registration/feature reports, dependency/fixture identities and editor
artifacts. Cold/warm cache status and bootstrap/link/test measurements are
available for the first actual hosted runtime assessment. No dispatch was made
by Keyboard. **Stop here for Golden Wrench source review.**

## Subsequent native run repairs

[CI 41352959359 consolidated triage](CI41352959359_TRIAGE.md) records the
Windows static-only c-ares import correction, macOS deployment propagation,
streamed-producer deadline repair and cancellation-fixture race. The existing
seven-job workflow and approved pins remain authoritative. The native rerun
must include `target_triple`, the macOS-only `macos_deployment`, both producer
pause paths in `http_timeouts`, and the delayed-barrier cancellation example,
along with the complete existing acceptance suite. Native acceptance remains
pending Golden Wrench review and Boss's hosted rerun.

[CI 41378401662 follow-up](CI41378401662_TRIAGE.md) corrects static dependency
definitions on curl's actual object target, samples current provider upload
state before deadlines, and replaces the cancellation fixture's timed barrier
with observable readiness/close checks. Local Debug/Release/strict checks pass;
Windows linking and macOS behavior still require the reviewed native seven-job
rerun. The original Debug cancellation artifact lacked its HTTP timeout phase;
that evidence limitation and the new diagnostic are recorded explicitly.


[CI 41443618378 full repair handoff](CI41443618378_TRIAGE.md) records the failed
cancellation fixture's connection-wait dependency, the controlled producer-total
reproduction, Schannel explicit-CA option correction, and Windows generator/text
fixture fixes. It supersedes the preceding local-confidence claim for cancellation.
The unchanged seven-job native acceptance must include the new `http_dns`
connection/producer gates, original `http_timeouts` with barrier timing, both
cancellation/timeout example outcomes, and Windows TLS/auth/native/URL checks.
Corrected native Windows/macOS results remain **UNVERIFIED** until Boss's reviewed
hosted run; no dispatch is authorized or performed by this handoff.
