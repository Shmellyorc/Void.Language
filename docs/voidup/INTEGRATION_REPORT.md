**Active follow-up: [CI #42377496702 cross-platform repair](#ci-42377496702-cross-platform-repair), baseline `799a29e3c0b0e204abecd4dc1f14356bf1df3443`.**
The earlier integration and cleanup inventories below are retained historical
records: they were already present in this baseline. They are not the pending
file list for this follow-up. Native Windows Release/Debug verification is complete
for the stated local environment; hosted seven-configuration acceptance remains
unqualified. Use the [current exact allowlist](evidence/ci-42377496702/commit-paths.txt)
for this repair. Original failures and corrective reruns are separated below.

**Historical commit inventory after the earlier source-control cleanup:**
[REPAIR_COMMIT_PATHS.txt](REPAIR_COMMIT_PATHS.txt) and
[REPAIR_COMMIT_INVENTORY.json](REPAIR_COMMIT_INVENTORY.json).
Earlier `COMMIT_PATHS.*` and `PENDING_PATHS.*` lists describe their historical
review scopes and must not be used for this commit. Generated raw evidence is
preserved in the [verified archive](evidence/repair-raw-evidence-2026-11-07.tar.xz)
with a [per-file restoration index](evidence/repair-raw-evidence-2026-11-07.json).
A link fragment on that archive names the original repository-relative member;
extract into a fresh directory to inspect those records. Directly linked retained
summaries, source diffs and original CI ZIPs remain individual files.

**Earlier scope: controlled libsodium backport integration — 7 November 2026.**
Golden Wrench/Boss authorized permanent integration engineering of the exact
reviewed three-file candidate. The unchanged base pin, mandatory patch-aware
receipts and corresponding-source packaging are integrated. See the
[current source diff and qualification](#controlled-libsodium-backport-integration).
Windows/macOS native execution and hosted acceptance remain UNQUALIFIED.
The dated REVISE/proposal-only records below describe their original review
scope and are superseded for integration authorization, without changing their
failures, measurements or historical CI identities.

**VOIDUP local integration for Boss review — 2026-11-06/07**

The complete Phase A/B implementation is prepared for the existing seven-job **Compiler link experiment** workflow. No staging, commit, branch, push, hosted dispatch or binary publication was performed. The obsolete/default workflow, root README, TCP/UDP handoff and existing website changes are preserved. This is local integration evidence; Windows/macOS and hosted seven-job acceptance are not claimed.

The exact per-file commit inventory is [COMMIT_PATHS.txt](COMMIT_PATHS.txt), with an equivalent literal NUL-delimited [COMMIT_PATHS.nul](COMMIT_PATHS.nul). [COMMIT_INVENTORY.json](COMMIT_INVENTORY.json) records intended paths and hashes, exclusions, ignored-but-intended evidence, baseline checks and the suggested title/command. It includes every retained Phase A/B failure and source snapshot, including the OS assertion incident explicitly referenced by `REPORT.md`. The inventory rejects unclassified changed paths, compiled outputs, key/archive candidates and symlinks. Directory shorthand below describes scope; staging uses individual file names only.

| Included scope | Exact inventory coverage |
| --- | --- |
| Compiler/Runtime/ABI | Changed `include/void/{application,filesystem,filesystem_abi,host_abi,host_ops,ir,runtime,runtime_abi}.hpp`; frontend/IR/backend host `.inc` bridges and their changed compiler files; `src/main.cpp`; filesystem/managed ABI/application and all new native host providers/internal platform headers. |
| CMake/dependencies/notices | `CMakeLists.txt`, `cmake/{CurlProvider,HostDependencies,InstallLicenses,NativeBuild}.cmake`, `cmake/host-sodium.targets`, `third_party/host-versions.json`, exact host license/source-notice files and changed release notices/inventory. |
| Standard library/application | `stdlib/{archive,crypto,io,os,process,system_error}.void`; ten `apps/voidup/*.void` modules, with public engineering trust roots only. |
| Tests/tooling | Exact `tests/host_capabilities/` and `tests/voidup/` source files, host setup/verification/native recipes, application build/offline fixture producer, `tools/install.py`, integration inventory generator and new CI helper. |
| Records/evidence | `docs/voidup/` reports, feasibility, all original and integration evidence; `docs/os/evidence/crashes/incident-2cmxfrao/` is intended Phase A regression evidence, not an unrelated experiment. |
| CI integration | Only `.github/workflows/ci-link-experiment.yml`, shared selected-workflow disk helper `tools/ci/linux_disk.py`, and `tools/ci/voidup_bootstrap.py`. |

The exact unrelated Git-visible paths excluded are:

```text
docs/net/TCP_UDP_HANDOFF.md
website/dist/assets/style.css
website/dist/getting-started/index.html
website/public/assets/style.css
website/public/getting-started/index.html
website/scripts/check.py
website/scripts/test_check.py
```

All `build*/`, `.toolchains/`, Python caches and generated native packages/binaries remain excluded. Specifically, `build/voidup-fixtures/keys/offline-{1,2,3}.pem` is outside the allowlist; no private signing credentials enter Git or CI. Build roots include the retained Phase A/B outputs and the new isolated integration builds, not production source inputs. No cleanup/reset was used to produce this inventory.

The existing `.log` ignore rule hides legitimate report-linked evidence from ordinary `git status`. Those **individual** evidence files are deliberately included in the allowlist. `.gitignore` is unchanged. The recommended force flag applies only to the reviewed file list, not to a directory sweep:

```bash
git --literal-pathspecs add -f --pathspec-from-file=docs/voidup/COMMIT_PATHS.nul --pathspec-file-nul
```

This command is a recommendation for Boss, **not executed here**. Suggested single commit title:

```text
Integrate VOIDUP host capabilities, manager and native CI bootstrap [skip ci]
```

The selected workflow retains its manual trigger, seven jobs, LLVM/HTTP setup and caches, control/thinlto-cache modes, native MSVC environment, ARM64 macOS check, unfiltered CTest and existing artifact steps. Its integration changes are small:

1. Ubuntu explicitly installs `build-essential`, which supplies Make and the declared native C/C++ development environment.
2. After HTTP preparation and before compiler configuration, `voidup_bootstrap.py bootstrap` builds the pinned libsodium/libarchive providers. It reuses the existing HTTP planner's exact profile/SDK/platform mapping: Debug → Debug, Release → Release, Linux Sanitizers/RelWithDebInfo + `VOID_SANITIZERS` → strict. Windows prefixes are `windows-x86_64/<profile>`, macOS `macos-arm64/<profile>`, and Linux retains `<profile>`.
3. The helper verifies existing HTTP hashes/corresponding sources, the host pin/profile/platform/architecture/file/recipe receipt, and the exact selected HTTP zlib path/hash. macOS additionally binds physical SDK and deployment target; Windows requires the existing x64 developer environment and MSBuild. Host prefixes are freshly built; no new dependency cache or ambient provider fallback was introduced. An existing prefix is refused by bootstrap; the separate read-only `verify` phase handles local qualification.
4. Disk checks retain the existing bounded hosted-Ubuntu cleanup allowlist and developer-machine refusal. There is no new cleanup path. The pre-restore estimate adds a **2 GiB host-source/build/prefix reservation** and **512 MiB app/build reservation**, with the existing 25% margin. Before-build checks account for remaining app/build headroom. The new cross-platform preflight records available workspace/temp bytes and requires an additional 2.5 GiB/2 GiB respectively. These are conservative reservations, not measured Windows/macOS or hosted peaks. `disk-reservations.json` records exact per-profile requirements.
5. Before the existing unfiltered CTest step, `test-plan` checks eleven required tests on Linux and five on Windows/macOS, with nonempty commands and no disabled entries. Signed private installation tests are forbidden in that registration set. Host/smoke evidence is collected by the existing artifact step, including failures during native provider setup.

CMake now has a `voidup` **ALL** target. It invokes the matching `voidc build` on the real `.void` application and depends on app/stdlib inputs, Runtime and native resource manifests. The output is `build/<configuration>/voidup/voidup[.exe]`. It is a local build target, not a new public distribution/install component. Warm `voidc`-only relinking still avoids rebuilding downstream AOT outputs.

`voidup_smoke` uses only Python's standard library. It checks the real native application entry, help/version, Unicode invalid-argument diagnostics and forwarding-separator rejection. Unix additionally exercises a dedicated private root, initial/reopened control JSON, native locking through doctor/list and missing-default failure on a real 1 MiB hard child stack. Raw stack receipts remain in evidence and are validated separately from application JSON. Windows smoke covers CLI/Unicode entry only; it **does not** claim private-filesystem or installation qualification. The separately registered portable provider/source tests still exercise actual Windows capabilities and publication uncertainty. No unsupported result is converted into native installation PASS.

The existing common Phase A tests remain registered on native platforms: dependency admission/rejection, portable native provider and normal/O2 JIT plus independent AOT source tests. Linux retains its full host, hard-stack, frontend, LSP and native GC/allocation/publication tests. The existing installed/relocated compiler-provider test is now registered on Linux with `RUN_SERIAL`, because its checkout-denial fixture uses Linux Landlock. `voidup_ci_integration` exercises profile layouts, workflow order/matrix/credential exclusions, disk rejection and missing/disabled/private test-plan rejection. Private signed A/B packages and fixture signing stay local and separate.

Host tests now write new results under each build's `host-capabilities-evidence/`, while smoke uses `voidup-smoke-evidence/`. Old Phase A/B reports and indexed failures are not overwritten. The portable provider harness honors the same existing evidence-directory override as the other host harnesses. Source application policy, compiler semantics, HTTP transport and Runtime ownership behavior are unchanged in this integration assignment.

Actual local qualification and retained failures:

| Check | Result / evidence under `evidence/integration/` |
| --- | --- |
| Selected workflow YAML and seven-job matrix | Parsed locally; `workflow-yaml.json`. Default workflow hash is retained in the baseline. No hosted job was run. |
| Integration policy / existing disk tests | Four policy tests and five existing disk tests passed; `policy-tests.log`, `disk-tests.log`; also exercised through registered `voidup_ci_integration` and existing `http_ci_policy`. |
| Existing Linux providers | Release, Debug and strict prefixes passed read-only verification, including selected HTTP zlib identity; `*-provider-verification/`. |
| Fresh provider bootstrap | Actual Ubuntu 24.04 userspace build in a separate source root passed; **87/87 libsodium upstream tests**, static libarchive build/install and both HTTP/host verification passed. `fresh-ubuntu-provider-bootstrap/` captures commands, logs, receipts and cache diagnostics. Existing prefixes/notices were not rebuilt in place. |
| PikaOS native builds | Release, Debug and strict configurations and explicit application/host/LSP targets built successfully; `*-configure.log`, `*-build.log`. The focused HTTP/URL IR/native binaries were relinked against the current frontend/Runtime before their corrective checks. |
| Release focused CTest | Original run: 14 pass, two failures of 16. The corrected smoke and rebuilt HTTP IR tests passed **2/2**. This is effective coverage of all 16 selected tests, not a fabricated clean original run. |
| Debug focused CTest | Original run: 10 pass, one smoke-fixture failure of 11. Corrected smoke passed **1/1**. Effective coverage **11/11**, including installed/relocated providers and normal/hard-stack source tests; original run retained. |
| Strict focused CTest | Original run: 14 pass, the same two setup/fixture failures of 16. Corrected smoke and rebuilt HTTP IR passed **2/2**, with ASan/UBSan/leak checking and dedicated fatal codes. Effective coverage **16/16**; original failures retained. |
| Fresh Ubuntu Ninja project integration | Actual current-source configure and **276-step** selected target build passed against the freshly bootstrapped host prefix and verified Ubuntu HTTP closure. App smoke, CI policy, dependency receipts, portable native provider and normal/O2 JIT/independent AOT source suite passed **5/5**. This is Ubuntu 24.04 container userspace on the PikaOS kernel, not a separately booted Ubuntu kernel. |

Two setup/fixture errors were preserved and repaired. The first isolated dependency root omitted the existing HTTP fixture pin JSON, then the PikaOS preflight correctly refused absent Ninja. Qualification moved to the existing Ubuntu environment with real Ninja rather than weakening preflight. The first smoke assertion attempted to decode the launcher's stack receipt as application JSON; it now retains/validates that receipt and decodes the actual app diagnostic separately. Initial HTTP IR failures came from an old executable that had not been included in the first explicit target build; relinking it against the already current frontend repairs the stale-test-binary failure. Original logs/JUnit/raw smoke records remain. These are not claimed as compiler/Runtime repairs or discarded as passing results.

Windows x64 and macOS ARM64 native host-provider builds, application AOT execution and filesystem/CRT/durability acceptance remain **UNQUALIFIED until Boss's selected native jobs actually execute and their evidence is reviewed**. The existing Phase A/B records retain their precise local/cross-only limits. Hosted storage headroom and new native prerequisite recipes are checked by the workflow, but no future seven-job success or native total disk peak is promised. Signed A/B installation, replay/revocation, hostile archive and recovery qualification remains the retained private Linux Phase B work, not a hosted CI credential deployment.

The commands actually exercised include:

```text
python3 tests/voidup/ci_integration.py
python3 tests/net/http/ci_disk_test.py
python3 tools/ci/voidup_bootstrap.py verify --configuration Release --evidence docs/voidup/evidence/integration/release-provider-verification
cmake -S . -B build/llvm23-release
cmake --build build/llvm23-release --target voidup void_host_portable void_host_capabilities void_host_child voidls -j3
python3 tools/ci/voidup_bootstrap.py test-plan --configuration Release --build build/llvm23-release --evidence docs/voidup/evidence/integration/release-test-plan
ctest --test-dir build/llvm23-release -R '^(host_capabilities.*|voidup_.*|http_ci_policy|http_native|http_ir|url_native|cli_contract)$' --output-on-failure
ctest --test-dir build/llvm23-release -R '^(voidup_smoke|http_ir)$' --output-on-failure
podman exec voidup-portability-ubuntu python3 build/voidup-integration-ubuntu-bootstrap/tools/ci/voidup_bootstrap.py bootstrap --configuration Release --evidence /home/shmellyorc/Documents/Void.Language/docs/voidup/evidence/integration/fresh-ubuntu-provider-bootstrap
podman exec voidup-portability-ubuntu cmake --build build/voidup-integration-ubuntu --target voidup void_host_portable voidls -j3
podman exec voidup-portability-ubuntu ctest --test-dir build/voidup-integration-ubuntu -R '^(voidup_.*|host_capabilities_dependencies|host_capabilities_portable|host_capabilities_portable_source)$' --output-on-failure
python3 tools/voidup_integration_inventory.py
```

Debug and strict used their matching build directories/provider profiles; strict tests explicitly set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The captured configure/build logs, provider command JSON, CTest JUnit and [original/repair result mapping](evidence/integration/test-results-original-and-repairs.json) define exact scope and failures. This was a focused local run, not the entire existing CTest suite or a hosted seven-job execution.

The [integration evidence index](evidence/integration/INDEX.json) binds the report and literal inventory. Its audit rechecks the original Phase A and Phase B evidence indexes, the initial dirty/untracked-file hashes and the default workflow hash. Only `CMakeLists.txt` and the portable harness evidence-output adjustment changed among preexisting dirty/untracked files; the selected workflow/disk helper and new integration files are additional intended changes. The six website files retain their exact initial hashes. `git diff --check` passes and the Git staging area remains unchanged.

**Recommendation: RETAIN this local integration for Boss-controlled staging/commit and the selected native CI run.** The exact intended count and every path are recorded in `COMMIT_INVENTORY.json`/`COMMIT_PATHS.nul`; all unlisted paths remain excluded. Do not promote Windows/macOS or hosted acceptance before their actual results. No binary publication or private-key deployment is authorized or performed. Stop here for Golden Wrench/Boss review.


## 6 November 2026 CI repair addendum

The [LLVM 23.1.4 CI repair](INTEGRATION_REPORT.md#llvm-2314-ci-repair) preserves run 42334902548 and
separately reproduces fatal upstream libsodium sanitizer failures. The earlier
local upstream 87/87 result did not establish fatal UBSan acceptance; recover-mode
checks can exit successfully after reports. The current strict bootstrap enforces
fatal settings. Original measurements and qualification records above are retained
unchanged; focused VOIDUP success does not qualify all upstream provider tests.
Windows resource selection is corrected in source; native Windows/macOS evidence
for this repair remains unqualified. No hosted acceptance is claimed.


## LLVM 23.1.4 CI repair

6 November 2026. Local source repair and investigation for Golden Wrench/Boss
review. **REVISE: retain the selector correction for native review; strict upstream
libsodium remains blocked. No hosted or native Windows/macOS acceptance claimed.**

### Original Windows failure and cause

[Run 42334902548](https://github.com/Shmellyorc/Void.Language/actions/runs/42334902548),
commit `82e97f541fab7eec760083383569ab9c9ce8f355`, failed before compiling
libsodium in Windows Debug and Release. The preserved Release artifact records
MSBuild `17.14.60+43b635718`, `PlatformToolset=ClangCL`, `LLVMInstallDir` and
`CLToolPath` under `.toolchains/llvm-23.1.4`, but **no `LLVMToolsVersion` override**.
The compiler identity log already reports Clang **23.1.4**. Neither the pinned
libsodium MSVC project nor its MSVC support files define `LLVMToolsVersion` or
`LLVMInstallDir`; the omitted selector was left to imported native toolset state. The failure is in
`Microsoft.Cpp.ClangCl.Common.targets(77,5)`, target
`ClangClFixupCompileOptions`: effective selector **19** asks for
`llvm-23.1.4\\lib\\clang\\19`. Thus installation/compiler selection and MSBuild's
resource-version selection diverged. Installing LLVM 19 would violate the
required toolchain and would not repair this integration.

The retained artifact contains no evaluated property dump or binary log from the
original run. It proves the effective value and failing target, but not the exact
imported `.props` assignment that supplied 19. The explanation that it was the
Visual Studio ClangCL default is consistent with the missing override and
[Microsoft's documented independent installation/toolset properties](https://learn.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=msvc-170#custom_llvm_location).
The precise defining import remains unverified without that native installation.
No fabricated evaluated-property or compiler-command evidence is substituted.

Original artifacts are retained byte-for-byte:

| Artifact | Scope | ZIP SHA-256 |
| --- | --- | --- |
| [13558653075.zip](evidence/llvm23-repair/original-ci/13558653075.zip) | Windows Debug diagnostics | `8a4dd5d9e49ccd8240d6b18bf1e85af914054c34487cc7938579cc7f9b853d13` |
| [13558638061.zip](evidence/llvm23-repair/original-ci/13558638061.zip) | Windows Release diagnostics | `9ee4f916532e9e3763270c421162feca809aed33ff07b33bcd3b9e2226d878a0` |
| [13558802832.zip](evidence/llvm23-repair/original-ci/13558802832.zip) | Linux Sanitizers diagnostics | `65a37d6dd16cc349da545638afa465ef33a64bf8c6ba661a06f69d066e7c3ad4` |

Selected `.log`, `.json` and `.trs` members are also extracted under their artifact
IDs. Original failed results remain unchanged; neither artifact is a repaired run.

### Permanent source correction

| Changed source | Purpose |
| --- | --- |
| [tools/ci/llvm_sdk.py](../../tools/ci/llvm_sdk.py) | Reuse the existing official archive identity; validate SDK headers/static library, exact tool versions, compiler resource headers/libraries and tools inside the SDK. Query the pinned `clang-cl.exe /clang:-print-resource-dir`; derive `LLVMToolsVersion` from that actual directory leaf. Export installation/selector on Windows. Add verified pinned C++ selection for standalone engineering scripts. |
| [tools/setup_host_deps_native.py](../../tools/setup_host_deps_native.py) | Pass explicit SDK-derived `LLVMToolsVersion`, `LLVMInstallDir`, compiler/linker/librarian paths and executables as MSBuild global properties. Reject differing evaluated properties and incomplete native SDK/VC headers/static libraries before building. Capture property JSON, preprocessed imports and binary log; force a real rebuild and inspect COFF static CRT directives before installation. |
| [cmake/host-sodium.targets](../../cmake/host-sodium.targets) | Fail before ClangCL fixup/compile/archive on a selector mismatch or missing explicit tools/resources. Emit the selected paths. Preserve existing `RuntimeLibrary=MultiThreaded` (`/MT`), including Debug, and sanitizer options. |
| [tools/verify_host_deps.py](../../tools/verify_host_deps.py) | Include the shared LLVM recipe in native provider recipe verification. Old native receipts must be rebuilt rather than silently reused. |
| [tools/setup_host_deps.py](../../tools/setup_host_deps.py) | Enforce fatal ASan/UBSan/leak settings for local strict bootstrap, matching CI instead of allowing recover-mode UBSan to report successful tests. |
| [tools/ci/voidup_bootstrap.py](../../tools/ci/voidup_bootstrap.py) | Preserve upstream per-test `.log`/`.trs` and `test-suite.log` on failed bootstrap. The original helper retained the summary/configuration but lost the individual test diagnostics. |
| [tests/voidup/ci_integration.py](../../tests/voidup/ci_integration.py) | Add SDK selector, wrong-version/identity/resource and missing-component rejection tests to the already registered CI integration test. |
| [tools/bench/heap_legacy.py](../../tools/bench/heap_legacy.py), [buffer_preparation.py](../../tools/bench/buffer_preparation.py) | Replace active Clang 19 defaults with the verified pinned SDK and reject an alternate compiler path before executing it. Historical benchmark results are unchanged. |
| [tools/verify_discovery_engines.py](../../tools/verify_discovery_engines.py), [verify_website_examples.py](../../tools/verify_website_examples.py) | Apply the same pinned compiler selection to active acceptance helpers. No website files or compiler behavior changed. |

The correct resource selector is **not assumed to be `23.1.4`**. The actual Linux
23.1.4 SDK reports a resource leaf `23`; Windows derives its own value on the
native host. Unit fixtures cover both `23` and `23.1.4`. Accepted resource names
must be version prefixes of the exact pinned release and resolve directly under
that SDK's `lib/clang`. Missing compiler-rt libraries or headers fail closed.
`llvm-lib` has no version switch; its path/hash is bound to the existing verified
SDK archive identity rather than treating an ignored version argument as proof.

A Linux cross-target C object compiled by the actual pinned 23.1.4 Clang with
`/MT` emits `/DEFAULTLIB:libcmt.lib /DEFAULTLIB:oldnames.lib`. C objects need not
carry C++'s `MT_StaticRelease` mismatch tag. The provider check therefore requires
static CRT directives and rejects dynamic/debug CRT directives. The same real cross-target probe accepts `/MT` and rejects `/MD` and `/MTd`
using the production ABI validator ([commands/results](evidence/llvm23-repair/coff-abi.json)).
This object probe is not a native Windows build or a libsodium ABI acceptance result.

The source archives, hashes, upstream trees, licensing materials and `/MT` policy
are unchanged. HTTP still owns the shared zlib dependency. Native receipts include
the shared LLVM recipe hash; cache invalidation is intentional, not a fallback.
No workflow, language architecture, Runtime or public API change is required.

### Active toolchain audit

All six workflow files, CMake configuration/dependency files, SDK/HTTP/VOIDUP
bootstrap scripts, install helper and tracked non-Markdown source paths were
scanned. Active configuration already pins 23.1.4; the four standalone script
defaults above were the remaining explicit 19 selections. The final
[scan](evidence/llvm23-repair/active-old-toolchain-scan.txt) has no matches in
tracked active non-Markdown paths (excluding docs, third-party material and
website). CMake still requires exact LLVM 23.1.4 and matching Clang/LLD.
The selected seven-job workflow and obsolete/default workflow are unchanged.
The [documentation consolidation](INTEGRATION_REPORT.md#documentation-review-revision) separately corrects
current instructions while preserving historical identities and failures.

### Independent Linux Sanitizers investigation

The original hosted summary is **74 PASS / 13 FAIL / 87 TOTAL**, zero skips.
Failed names, in retained CI order:

`aead_aes256gcm`, `aead_aegis128l`, `aead_aegis256`,
`aead_chacha20poly1305`, `aead_xchacha20poly1305`, `box_seal`, `codecs`,
`generichash`, `generichash3`, `misuse`, `secretbox_easy`, `sodium_utils`,
`pwhash_argon2id`.

The original uploaded summary did not include individual upstream `.log` files.
New local diagnostics are separately labeled; they are not attributed to a
missing original CI stack trace.

Using the unchanged pinned **libsodium 1.0.22** archive, exact source-tree
verification and actual Clang **23.1.4**:

| Environment | Outcome | Difference from original hosted names |
| --- | --- | --- |
| PikaOS x64, existing instrumented build, fatal rerun | 73 PASS / **14 FAIL** / 87, zero skips | Adds `box_easy2`, `secretbox_easy2`; `misuse` passes |
| Ubuntu **24.04.5** x64 container, fresh configure/build | 74 PASS / **13 FAIL** / 87, zero skips | `box_easy2` fails instead of `box_seal` |

The [failure manifest](evidence/llvm23-repair/sanitizer-failures.json) records all
three exact sets and the actual local diagnostics. Per-test logs, summaries and
status files are retained in [PikaOS evidence](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/llvm23-repair/local-original-strict)
and [Ubuntu evidence](evidence/llvm23-repair/ubuntu-strict/).

Observed causes:

- The upstream negative tests pass null arguments to functions whose public
  headers declare those arguments `nonnull`. Fatal UBSan stops at those calls:
  `aead_aegis128l.c:587`, `aead_aegis256.c:668`, `aead_aes256gcm.c:3198`,
  `aead_chacha20poly1305.c:118`, `aead_xchacha20poly1305.c:125`, `codecs.c:175`,
  `generichash.c:1370`, `generichash3.c:133`, `pwhash_argon2id.c:497`, and
  Ubuntu's `misuse.c:177`.
- Actual provider code in `crypto_stream/salsa20/xmm6int/u1.h:187` performs a
  typed `uint32_t` load at a misaligned byte address. The failing box/secretbox
  tests expose this. The [pinned upstream source](https://github.com/jedisct1/libsodium/blob/77e1ce5d6dee871c49ef211222ba18ef0c486bda/src/libsodium/crypto_stream/salsa20/xmm6int/u1.h)
  contains those casts; the full local UBSan reports are preserved.
- `sodium/utils.c:132` calls glibc `explicit_bzero(NULL, 0)` in a null/zero-length
  test; the libc declaration has a nonnull contract, producing another report.

This is independent of the Windows MSBuild selector. An earlier local
instrumented `make check` could report 87/87 with recover-mode UBSan; that is not
fatal strict acceptance. Original reports remain intact with an addendum pointing
here. Existing VOIDUP tests against an installed provider do not validate all 87
upstream cases.

**First-review repair boundary (superseded by the proposal below):** a genuine correction requires changes to upstream
annotations/test contracts and provider alignment/zero-length code, or a different
reviewed source pin. The assignment also requires preserving upstream source and
pinned dependencies. No evidence supports a build-selection-only fix for all
three causes. No source patch, pin change, disabled backend, sanitizer suppression,
skipped test or weakened flag was applied. Strict bootstrap correctly remains
failing. Golden Wrench/Boss must review this source/pin constraint before a full
strict-provider pass can be claimed.

### Reproduction and focused validation

Run from the checkout root. The actual Ubuntu commands and timings are retained
in [ubuntu-strict/commands.json](evidence/llvm23-repair/ubuntu-strict/commands.json).
The container supplies Ubuntu userspace, not a separate kernel or physical host.

```sh
python3 tests/voidup/ci_integration.py
# 10 tests pass, including six SDK/selector rejection tests.
python3 -m py_compile tools/ci/llvm_sdk.py tools/setup_host_deps_native.py \
  tools/setup_host_deps.py tools/verify_host_deps.py tools/ci/voidup_bootstrap.py
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  make -C .toolchains/host/build/strict/sodium check -j3
# Fails: 14/87 on the local PikaOS host; all original diagnostics retained.
```

The fresh Ubuntu build uses `--disable-shared --enable-static --enable-minimal
--disable-dependency-tracking`, `CFLAGS=-g -O2 -fsanitize=address,undefined
-fno-omit-frame-pointer`, `LDFLAGS=-fsanitize=address,undefined`, the same fatal
options and `make check -j2`. Build succeeds; check exits 2 with 13/87 failures.
No provider installation is performed by this isolated reproduction.

Additional checks and their exact commands/statuses are recorded in
[evidence](evidence/llvm23-repair/). Linux .NET MSBuild guard fixtures pass a matching
selector and reject four mismatches before any compiler execution. They exercise
the XML guard only, without Visual C++ imports or native Windows tools.
The final build/CTest table below distinguishes rebuilt configurations from the
original strict run and the independently failing upstream suite.

### 6 November local outcomes — first review pass

| Check | Actual outcome / retained evidence |
| --- | --- |
| Release all-target build, focused host/VOIDUP CTest | PASS; **11/11**, 130.37 s; `release-build.log`, `release-ctest.log` |
| Debug all-target build, focused host/VOIDUP CTest | PASS; **11/11**, 453.40 s; `debug-build.log`, `debug-ctest.log` |
| Strict targeted rebuild and fatal CTest | PASS; **11/11**, 448.88 s; `strict-build.log`, `strict-rebuilt-ctest.log` |
| Earlier existing-binary fatal strict run | PASS; **11/11**, 535.27 s; `strict-ctest.log`; retained separately |
| Integration/SDK unit suite | **10/10 PASS**; `toolchain-unit-final.log` |
| Actual Linux .NET MSBuild XML guard / property JSON | Matching fixture passes; four invalid fixtures reject; `msbuild-guard.json`, `msbuild-evaluated-fixture.json`; no Windows imports or compiler execution |
| Actual Clang 23.1.4 cross-target C COFF | `/MT` accepted, `/MD` and `/MTd` rejected; `coff-abi.json`; not a Windows link/libsodium build |
| Existing LLVM/HTTP/host/zlib closure, Release/Debug/strict | PASS; `closure-*/verified.json`; current source/config verification, not an upstream strict-test pass |
| Four active helper CLIs | Help succeeds; an unpinned path rejects before output creation/compiler execution; `helper-smoke.json`, `helper-rejections.json` |
| Python syntax and Git whitespace | PASS; ten changed Python files compile; `git diff --check` clean |
| Source/archive verification | PASS; unchanged libsodium **678** and libarchive **1,796** files, exact source archive hashes; `source-verification.json` |
| Documentation | 18/18 archived bodies preserved except relative URLs; protected Markdown unchanged; zero new broken links; `documentation-links.json` |
| Full pinned upstream strict suite | **FAIL**: PikaOS 14/87, Ubuntu 13/87; no tests skipped or disabled |
| Native Windows/macOS repaired build | **UNQUALIFIED**; no accessible native execution host |

Actual Linux build/test commands:

```sh
cmake --build build/llvm23-release --parallel 3
ctest --test-dir build/llvm23-release -R '^(host_capabilities.*|voidup_.*)$' --output-on-failure
cmake --build build/llvm23-debug --parallel 3
ctest --test-dir build/llvm23-debug -R '^(host_capabilities.*|voidup_.*)$' --output-on-failure
cmake --build build/llvm23-strict --target voidc voidls void_host_portable \
  void_host_capabilities void_host_child voidup --parallel 3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/llvm23-strict -R '^(host_capabilities.*|voidup_.*)$' --output-on-failure
python3 tools/ci/voidup_bootstrap.py verify --configuration Release --evidence <evidence>/closure-release
# Same verification with Debug and Sanitizers; all three pass.
```

The focused suites cover ordinary/O2 source JIT, independently linked AOT,
Unicode arguments, crypto/archive rejection, private ownership, publication
failure ordering, process cleanup, GC, install closure and a real hard 1 MiB
stack. The new raw receipts and CTest transcripts are retained in `host-release`,
`host-debug`, `host-strict-rebuilt` and `*-LastTest.log`. Original evidence paths
are preserved. Timings are observations, not performance claims or thresholds.
Fresh replay drivers are retained alongside the logs (`documentation_audit.py`,
`msbuild_guard.py`, `coff_abi.py`, `ubuntu_sodium.py`).

The [first-review changed-file/evidence inventory](evidence/llvm23-repair/changed-files.json)
lists exact paths, status and content hashes, including ignored diagnostics that
an ordinary staging command could omit. Nothing is staged. No private signing
keys, generated native objects/binaries, build directories or unrelated files
belong to that inventory.

### Native evidence still required

No accessible native Windows or macOS host was found in this session. Wine and
Linux .NET MSBuild are not native acceptance. The corrected Windows recipe has
not yet completed an actual libsodium build, COFF library verification or VOIDUP
host-provider verification. Actual compiler invocation and the original default's
precise defining import remain unverified.

On Boss's next authorized native run, the recipe retains `sodium-properties.log`,
`sodium-evaluated.xml`, `sodium-build.binlog`, the normal compiler command log,
`sodium-static-abi.log` and the provider receipt. Review those for pinned
`clang-cl.exe`, derived resource leaf, `lld-link.exe`/`llvm-lib.exe`, matching native
Windows SDK/VC headers/libraries, `/MT` and successful host verification. Wrong
versions/missing components must fail before compilation. macOS source integration
is unchanged except shared recipe hashing; fresh native qualification is unclaimed.

**Recommendation: REVISE.** Retain the Windows selector/guard and evidence-retention
repair for independent review, retain the documentation cleanup, and keep strict
upstream-provider acceptance blocked until a source-preserving correction is
possible or Boss authorizes a narrowly reviewed upstream fix/pin. No commit,
branch, push, hosted dispatch, binary publication or root README edit occurred.


## Libsodium source correction proposal

7 November 2026, Golden Wrench **REVISE** follow-up. The
[actual three-file source diff](evidence/llvm23-repair/review/libsodium-candidate.patch)
is **PROPOSED / NOT APPLIED**. No active recipe reads this patch. The installed
providers, pristine source cache, archive hash and `third_party/host-versions.json`
remain unchanged. The only experimental source edit is to a test harness in a
disposable build copy; no cryptographic implementation was edited or installed.

| Demonstrated class | Smallest proposed correction | Evidence and limits |
| --- | --- | --- |
| Negative tests violate nonnull contracts | In `test/default/cmptest.h`, remove `__SANITIZE_ADDRESS__` from the branch that substitutes NULL for `guard_page`. ASan then uses the existing non-ASan guarded-allocation path. | Keep every public nonnull attribute, all 87 targets, test assertions and expected outputs. The protected nonnull pointer still faults if a rejected call incorrectly reads it. |
| Misaligned provider accesses | Backport both exact hunks of upstream [11cd77c5cd35fafa2317c36cd7f61de0b9d03cd2](https://github.com/jedisct1/libsodium/commit/11cd77c5cd35fafa2317c36cd7f61de0b9d03cd2), in `xmm6int/u1.h` and `u4.h`. | Both upstream preimages match the pin byte-for-byte. Replace typed loads/stores with existing `LOAD32_LE`/`STORE32_LE` helpers; no rounds, constants, counters or cipher redesign. Neither provider hunk has been applied. Include `u4.h` to retain the complete upstream fix for the same unsafe-access pattern in longer blocks. |
| Invalid zero-length clearing call | The same harness change makes the existing `sodium_memzero(guard_page, 0)` test pass a nonnull guard pointer rather than NULL. | No `utils.c` or secure-clearing implementation change is needed for this demonstrated failure. This is a call-site correction, not a new guarantee that `sodium_memzero(NULL, 0)` is valid. |

The existing endian helpers use `memcpy` on native little-endian targets. An
independent probe against the **unmodified** provider checked load/store values,
zero-length no-op and nonzero clearing at 16 byte offsets under fatal ASan/UBSan:
PASS. Its original-invalid-call control, `sodium_memzero(NULL, 0)`, still fails
with UBSan exit **86**. This confirms that checks were not suppressed. The first
probe compile used an overly broad header and failed to find generated
`export.h`; the diagnostic is retained, and the corrected probe includes the
utility header it actually uses. This was a probe setup error, not a provider fix.

### Upstream comparison

The latest observed release remains **1.0.22-RELEASE**. Inspected immutable refs:
master `c5717543ad05c70dcc97df7ea08b497d5afc2c86` and stable
`e85990784cacc03a5909f73114e4ae378a342772`. The exact alignment fix is present on
master; stable retains the pinned unsafe accesses. The inspected harness still
substitutes NULL under ASan on both refs, and neither inspected `sodium_memzero`
body adds a zero-length early return. No complete released three-class fix was
found. These findings apply to those refs, not to all possible future upstream work.

[Compared-file URLs/hashes](evidence/llvm23-repair/review/upstream/compared-files.json)
and [the exact upstream commit](evidence/llvm23-repair/review/upstream/alignment-fix.json)
record provenance. The upstream alignment commit is dated 8 April 2026; its
preimages match this pinned source despite being on another branch. Do not infer
inclusion from its date or release number alone. Moving to floating master would
pull unrelated cryptographic changes and is not the proposed repair. Keep the
current archive SHA-256 `adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`.
If approved, integrate this explicit backport through the existing source-copy,
recipe-hash and provider-receipt verification; do not replace archive identity
with a post-patch hash or silently reuse an old installed prefix.

### Proven experiment outcomes

Both experiments changed **only `test/default/cmptest.h`** in a disposable copy.
All other source files remain byte-identical to the pristine pin. Both use Clang
23.1.4, `-g -O2 -fsanitize=address,undefined -fno-omit-frame-pointer`, fatal
ASan/leak exit 87 and fatal UBSan/stacktrace exit 86. No test is skipped or disabled.

| Environment | Harness-only result | Remaining failed names |
| --- | --- | --- |
| PikaOS x64 | **83 PASS / 4 FAIL / 87**, zero skips, `make check` exit 2 | `box_easy2`, `box_seal`, `secretbox_easy`, `secretbox_easy2` |
| Ubuntu 24.04 x64 userspace | **85 PASS / 2 FAIL / 87**, zero skips, `make check` exit 2 | `secretbox_easy`, `secretbox_easy2` |

Every remaining diagnostic is the unchanged `u1.h` misaligned typed load. Different
failure counts remain recorded; neither run is a full candidate pass. Original
hosted **13/87**, PikaOS **14/87** and Ubuntu **13/87** failures remain separate
in the preceding first-review record. Detailed new test logs, `.trs` files,
commands, environments and exact sets are in
[guard-results.json](evidence/llvm23-repair/review/guard-results.json),
[guard-only](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/llvm23-repair/review/guard-only), and
[ubuntu-guard-only](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/llvm23-repair/review/ubuntu-guard-only).

`patch --dry-run --batch --fuzz=0 -p1` accepts every candidate hunk against the
pristine pin. [Provider integrity](evidence/llvm23-repair/review/pristine-providers.json)
verifies the unchanged archives and all 678 libsodium / 1,796 libarchive source
files. The retained toolchain/rejection suite passes **10/10**. The existing
Windows selector and rejection implementation is byte-identical to the candidate
from the first review.

**Not proven:** the combined three-file provider build or 87/87 pass, native
Windows `/MT` build/selection, and macOS execution. Applying the two cryptographic
provider hunks requires the requested Boss/Golden Wrench approval; this review does
not grant it. After approval, run all 87 tests with unchanged fatal settings and
reverify the VOIDUP provider closure. The next separately authorized Windows run
must show the actual pinned compiler/linker/resource paths, derived selector,
static `/MT` archive and successful libsodium/provider verification. Unit fixtures
and Linux cross-target objects cannot establish that native acceptance.

**Recommendation: approve the narrowly scoped three-file candidate for a later
implementation/qualification step; retain REVISE until actual results exist.**
No dependency pin/hash change, crypto implementation patch, suppression, staging,
commit, branch, push or hosted dispatch was performed.

## Documentation review revision

The previous cleanup added **22 Markdown files**. This revision removes those
22 additions, with **zero original Markdown files removed and zero net Markdown
additions** relative to the original 1,573-file baseline. This is a structural
reduction, not renaming the copies to another documentation extension:

- Fourteen proposal bodies return to their existing stable paths with explicit
  historical banners. The extra archive copies and active redirect-only stubs are
  eliminated. Original technical decisions, approvals, results and anchors remain.
- Four duplicate full checkpoint copies are eliminated. Their exact original
  bodies already exist at immutable commit `82e97f541fab7eec760083383569ab9c9ce8f355`,
  verified against the baseline and accessible through the main index online or
  offline using `git show`. Existing work/decision/approval and module evidence
  records remain in place. No Git history is changed.
- Toolchain requirements are consolidated into the existing
  [build guide](../SOURCE_COMPILER.md#active-toolchain). This integration report
  holds current repair/review evidence. The main documentation index holds history
  navigation. The separate toolchain, cleanup, repair and history-index files are
  removed and every maintained relative reference is redirected.
- The concise [Wrench handoff](../WRENCH_CLANKER.md#workshop-continuity) explicitly
  retains Golden Wrench as architecture partner, independent source/evidence
  reviewer and keeper of the engineering thread. Boss controls final architecture
  and Git decisions; Research investigates; Keyboard implements. The workshop motto
  and known-incident continuity are retained.

Actual review artifacts:

- [Documentation revision diff](evidence/llvm23-repair/review/documentation-revision.patch):
  exact delta from the previously reviewed local tree, including all 22 removals.
- [Final documentation diff against HEAD](evidence/llvm23-repair/review/documentation-final.patch):
  complete cumulative proposed Markdown changes at retained paths.
- [Retained build-source diff](evidence/llvm23-repair/review/windows-and-build-source.patch)
  and the separate [unapplied libsodium source proposal](evidence/llvm23-repair/review/libsodium-candidate.patch).
- [Move/provenance ledger](evidence/llvm23-repair/review/documentation-moves.json),
  [link/preservation checks](evidence/llvm23-repair/review/documentation-validation.json),
  and [exact current file inventory](evidence/llvm23-repair/review/changed-files.json).

The prior audit results and failure evidence are retained as first-review
snapshots, rather than overwritten as if they described this revision. The root
README, website, original raw evidence and LLVM 23.1.4 operational requirement
remain intact. This report separates first-review results, current partial
experiments and unverified native behavior; it does not claim hosted acceptance.

Validation for this revision: **1,573 → 1,573 Markdown files**, 14/14 proposal
bodies preserved apart from corrected URLs, and 4/4 committed checkpoint bodies
verified byte-for-byte against both local Git and fetched immutable originals.
All maintained local documentation paths/anchors and eleven fixed-commit
references validate; **zero broken links remain in that checked scope**. Two
pre-existing stale anchors were repaired. Original captured evidence and the
protected root README/website/vendor Markdown remain unchanged. The revision
patch was checked and applied only to a disposable document snapshot, reproducing
the exact revised tree. No Git index, object database or history was modified.

## Controlled libsodium backport integration

7 November 2026. Golden Wrench/Boss authorized permanent integration engineering
of the accepted isolated candidate. This supersedes the earlier proposal-only
restriction for this exact patch. **Recommendation: RETAIN the local integration
for independent source/evidence review; native Windows/macOS and hosted acceptance
remain UNQUALIFIED.** No compiler/Runtime architecture, public host API, HTTP
transport, dependency pin or workflow change is part of this delta.

### Integrated source and identity

The [actual integration source diff](evidence/llvm23-repair/integration-2026-11-07/integrated-source.patch),
[documentation delta](evidence/llvm23-repair/integration-2026-11-07/documentation-integration.patch),
[exact approved dependency patch](evidence/llvm23-repair/integration-2026-11-07/libsodium-approved.patch)
and [per-file inventory](evidence/llvm23-repair/integration-2026-11-07/changed-files.json)
are review artifacts, not summaries in place of source. The two integration diffs
replay against disposable copies of the preceding reviewed working tree and
reproduce every resulting byte. Prior Windows/source and documentation review
diffs remain retained in their original evidence directories.

The original libsodium **1.0.22** archive, recorded upstream revision
`77e1ce5d6dee871c49ef211222ba18ef0c486bda`, and archive SHA-256
`adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`
are unchanged. The exact accepted patch SHA-256 is
`f0de9004b8129626e14750437f51eb9524f030149607b50bb75ce0b6c998af74`.
Its proposal-era preamble is retained byte-for-byte; the new provenance manifest
records the subsequent controlled-integration approval. Libarchive's original
3.8.11 archive/hash and all HTTP pins are unchanged.

Only three dependency files change in each disposable source copy:

- `test/default/cmptest.h`: ASan uses the existing guarded, nonnull fixture instead
  of the NULL substitution. Negative assertions and expected outputs remain.
  The zero-length clearing test now supplies that nonnull fixture; there is no
  `sodium_memzero` implementation or public-contract change.
- Salsa20 `xmm6int/u1.h` and `u4.h`: the exact upstream alignment backport from
  commit `11cd77c5cd35fafa2317c36cd7f61de0b9d03cd2`, using the existing
  `LOAD32_LE`/`STORE32_LE` helpers. Cipher constants, rounds and counters are unchanged.

The verified pristine source cache is copied, never patched in place. Application
uses `git apply --no-index`, with a dry-run check, no index/history mutation and
complete post-application tree verification. No patch fuzz or partial application
is accepted. The three before/postimage hashes and the complete applied-tree hash
are required by the existing provider verification module. The applied-tree
SHA-256 is `e545c52f26e44f16d99db575a382af024100f46708f23f8051042964e0d698dd`.
Windows alone permits upstream's generated, version-checked `version.h` addition.

| Changed paths | Purpose |
| --- | --- |
| `third_party/host-patches/libsodium-1.0.22/{asan-alignment.patch,PROVENANCE.json}` | Exact approved patch, origins, pre/postimages, full 87-test inventory and two known oracle exceptions. |
| `tools/verify_host_deps.py` | Shared pristine/applied source verification, mandatory schema-3 admission, exact recipe/patch inventory, existing SDK identity, build identity and source-bundle replay. |
| `tools/setup_host_deps.py` | Fresh Linux disposable patched source, explicit pinned compiler/linker/archiver, all upstream tests, fatal strict checks and schema-3 receipts. |
| `tools/setup_host_deps_native.py` | Same approved source preparation and receipt identity for Windows/macOS; retained Windows selector, evaluated MSBuild paths and static `/MT` checks. |
| `CMakeLists.txt` | Include patch and provenance among existing host build/cache identity inputs. |
| `cmake/InstallLicenses.cmake` | Include patch/provenance and reproducible relative recipe/helper layout in existing compiler source/notice installation. |
| `tests/host_capabilities/{dependencies,install}.py` | Self-consistent forged-receipt rejection and installed/relocated source reconstruction/tamper checks. |
| `docs/release/{REDISTRIBUTION_NOTICES.md,NOTICE_INVENTORY.json}` | Describe compiled patched source accurately and bind patch/provenance/updated notice hashes. |
| `docs/ADVANCED_LANGUAGE.md` | Label Stage 4's LLVM 19.1.7 as historical; current operations require LLVM/Clang/LLD 23.1.4. Original commit and CI identity remain. |
| `docs/WRENCH_CLANKER.md`, this report | Record controlled-integration authorization, evidence and native limits while preserving Wrench's architecture-review/workshop role and earlier dated records. |

Schema 3 rejects previous unpatched receipts before compiler configuration or
installation reuse. It requires the exact base and applied source identities,
all eight recipe/policy/patch hashes, existing verified LLVM 23.1.4 SDK metadata,
actual selected tool paths/hashes, profile/platform/architecture, static provider
file hashes, archive feature inventory and HTTP zlib identity. The build-identity
hash binds those receipt fields, including compilation/link flags and platform
SDK/CRT details where applicable. Command logs/timings are evidence rather than
build identity. Non-Windows receipts require all 87 named target outcomes and
explicit oracle exceptions. Windows explicitly records upstream Unix tests as
not run; it does not manufacture those test outcomes.

The existing bootstrap still refuses implicit prefix reuse. CMake and installer
admission reuse the stronger existing verifier; there is no ambient provider
fallback or second dependency framework. The receipt, static archives, recipes,
patch and policy feed the existing compiler cache identity. The SDK selector's
recipe hash is bound through the receipt. Native resource manifests continue to
bind the actual link archives through existing rules.

Old Release/Debug/strict prefixes and build trees were retained under
`.toolchains/host/retained/pre-approved-backport-2026-11-07/`, with original receipt
hashes in [the retention mapping](evidence/llvm23-repair/integration-2026-11-07/retained-original-prefixes.json).
Fresh default prefixes were built from source. All three old receipts reject;
a separate CMake probe rejects an old receipt before configuring link dependencies.
No old archive or evidence was overwritten to obtain a passing result.

### Corresponding-source and installation verification

The existing compiler component now installs the unchanged upstream archives,
receipt, license notices, exact approved patch/provenance and a `repro/` tree with
both builders, verifier, shared LLVM SDK helper, MSBuild targets and host/HTTP pins.
The eight receipt-bound recipe files retain their expected relative paths and
exact bytes. The original per-file license snapshots describe upstream inputs;
the applied patch and provenance are separately and explicitly identified.

Installed/relocated tests reconstruct the actual sodium source from the packaged
base archive and patch outside the checkout, without an SDK or network, and
check all postimages against the receipt. A corrupted packaged patch must reject.
The same installed compiler then runs normal/O2 JIT and independent AOT with the
checkout and installed compiler inaccessible during AOT execution, using real
1 MiB stacks. This is corresponding-source reconstruction and component
qualification, not a claim that the package alone supplies the external native
SDK or complete HTTP build prerequisites. Public redistribution clearance is not
established by these checks.

The final notice/package audit passes **5/5 groups**: 53 source inventory entries,
Linux compiler (51 notices/25 stdlib sources), Linux tooling (17/25), Windows
compiler file rules (17) and Windows tooling file rules (13). The Windows groups
exercise file installation rules on Linux, **not native Windows execution**.
See [the exact package audit](evidence/llvm23-repair/integration-2026-11-07/license-package-fixed/validation.json).

### Actual local results

Fresh provider builds and fresh compiler build directories were used for each
PikaOS profile. LLVM/Clang/LLD **23.1.4 exclusively**; no LLVM 19 installation,
selection, download or fallback occurred. The existing SDK archive metadata is
reused; no replacement SDK dependency mechanism was introduced.

| Check | Actual result |
| --- | --- |
| PikaOS Release provider bootstrap | PASS; 87/87 upstream targets, 0 failures/0 skips; static libarchive and HTTP/host closure verification. |
| PikaOS Debug provider bootstrap | PASS; 87/87 upstream targets, 0 failures/0 skips; same closure checks. |
| PikaOS fatal strict provider bootstrap | PASS; 87/87 upstream targets, 0 failures/0 skips; ASan/UBSan/leak settings retained. |
| Fresh Release compiler/VOIDUP build and focused CTest | PASS, 11/11. |
| Fresh Debug compiler/VOIDUP build and focused CTest | PASS, 11/11. |
| Fresh strict compiler/VOIDUP build and focused CTest | PASS, 11/11; fatal ASan/UBSan/leak settings. |
| Receipt admission/rejection | PASS, 18 checks per PikaOS profile, including recomputed-build-identity forgeries, missing/wrong patch/postimages/recipes/SDK and old schema. |
| Old receipt at actual CMake dependency boundary | Rejected as required before link configuration. |
| Retained Windows selection/rejection unit suite | PASS, 10/10 fixture tests; not native build evidence. |
| Integrated Ubuntu 24.04.5 Release provider | PASS; 87/87 upstream targets, 0 failures/0 skips, static archive build/install, closure verification and 18 receipt checks. |
| Installed strict archive supplemental controls | Valid protected guard/nonnull zero-length clear: exit 0; overflow, underflow and guard read: fatal ASan diagnostics, actual exit 86 each. |

The eleven focused tests per compiler profile are `voidup_smoke`,
`voidup_ci_integration`, `host_capabilities_dependencies`,
`host_capabilities_portable`, `host_capabilities_portable_source`,
`host_capabilities`, `host_capabilities_stack`, `host_capabilities_native`,
`host_capabilities_frontend`, `host_capabilities_lsp`, and
`host_capabilities_install`. They cover actual crypto/archive/process/filesystem
host behavior, rejection paths, Unicode entry, resource/GC/failure lifetime,
normal/O2 JIT, independently linked AOT and hard-stack/relocated installation.
These are focused checks, not the complete unrelated compiler CTest suite or
private signed Phase B installation suite.

Ubuntu evidence comes from genuine Ubuntu **24.04.5 userspace / glibc 2.39** in
the existing rootless container on this PikaOS kernel, not a separately booted
Ubuntu kernel. The integrated recipes were copied byte-for-byte into a fresh
source root and built with the verified SDK, original source archives and the
existing verified Ubuntu HTTP Release closure. The earlier isolated strict
candidate result remains 87/87 in both userspaces. The present integration adds
Ubuntu Release provider qualification; it does not claim a new Ubuntu Debug or
strict whole-compiler run.

**Two upstream oracle limitations remain visible.** In every new provider suite,
85 `.res` files match `.exp` byte-for-byte. `sodium_utils2` and `sodium_utils3`
produce empty `.res` files despite 46/43-byte expected files; their unchanged
handlers call `_exit(0)` and bypass common output comparison/finalization. Under
ASan they also exclude intentional fault paths. Thus 87 target passes do not mean
87 output-oracle passes, complete intentional-fault coverage or normal-exit leak
finalization for those two tests. No test/expected-output list was changed and no
suppression was added. Existing stream/box/secretbox vectors still match, including
the affected Salsa20 paths. Supplemental controls against the newly installed
strict archive separately demonstrate guard mapping, valid zero-length clearing
and fatal invalid-access rejection; they do not repair the two upstream oracles.
The combined-runtime fatal exit remains **86**, not an invented distinct ASan 87
result. All diagnostics are retained.

Strict build/tests preserve:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
ASAN_SYMBOLIZER_PATH=<verified LLVM 23.1.4 SDK>/bin/llvm-symbolizer
-fsanitize=address,undefined -fno-omit-frame-pointer
```

The actual running strict CTest environment was captured in
[fatal-environment.json](evidence/llvm23-repair/integration-2026-11-07/strict/fatal-environment.json).
Raw source/vector results, receipt IDs, exact compiler commands/statuses and
installed reconstruction records are indexed by
[qualification-summary.json](evidence/llvm23-repair/integration-2026-11-07/qualification-summary.json).
[Ubuntu commands and raw evidence](evidence/llvm23-repair/integration-2026-11-07/ubuntu-integrated/)
and [strict fault-control diagnostics](evidence/llvm23-repair/integration-2026-11-07/strict-controls/results.json)
are separate from the focused CTest counts.

Representative actual commands (the linked command JSON retains full absolute
SDK paths, flags, output roots, statuses and timing observations):

```text
PATH="$PWD/.toolchains/ninja-1.13.2:$PATH" python3 tools/ci/voidup_bootstrap.py bootstrap --configuration Release --evidence docs/voidup/evidence/llvm23-repair/integration-2026-11-07/bootstrap-release
# Matching fresh Debug and Sanitizers bootstraps used their respective evidence roots.
cmake --build build/voidup-backport-integration/Release --target voidc voidls void_host_portable void_host_capabilities void_host_child voidup --parallel 2
ctest --test-dir build/voidup-backport-integration/Release -R '^(host_capabilities.*|voidup_.*)$' --output-on-failure
# Matching fresh Debug and strict builds/tests used explicit pinned SDK paths.
python3 tests/host_capabilities/dependencies.py .toolchains/host/prefix/strict strict Linux x86_64
python3 tests/voidup/ci_integration.py
python3 tests/release/check_licenses.py --build-dir build/voidup-backport-integration/Release --evidence-dir docs/voidup/evidence/llvm23-repair/integration-2026-11-07/license-package-fixed
podman exec voidup-portability-ubuntu python3 build/voidup-backport-integration/ubuntu-source/tools/setup_host_deps.py --profile Release --jobs 3
```

Integration setup failures are retained with their repairs. Initial
bootstrap preflight for each profile rejected Ninja missing from PATH; retry used
the already installed pinned Ninja, without installing a replacement or bypassing
preflight. The first notice audit found the stale inventory hash for the changed
redistribution notice; its exact entry was corrected and all five groups passed
on retry. The first Ubuntu negative-test invocation imported the checkout's
verifier instead of the copied source root and correctly rejected the different
SDK path identity; running the byte-identical test from the copied root passed
18 checks. See the `*-attempt1-preflight.log`, `license-package/`, and
`ubuntu-integrated-attempt1-test-root/` records. These failures are not rewritten
as clean first attempts. The initial generated review diff also lacked new-file
mode headers; its disposable replay rejected, and the generator was corrected
before the final replay. A subsequent link audit ran before writing its own
index files; output generation was reordered and the final audit passes. Both
audit-attempt diagnostics are retained. The final Markdown check initially mixed
a tracked-only count with the earlier broader inventory; it was corrected to
compare the same baseline scope (1,308 tracked Markdown paths, unchanged).
Original CI 13/87 failures, local 14/87 and 13/87 failures,
harness-only results and the earlier negative-probe oracle failure remain intact.

### Native limits, preservation and review recommendation

The Windows LLVM 23.1.4 resource-selector repair is retained. The helper and
MSBuild targets are unchanged by this integration; native source preparation now
uses the approved backport. No actual native Windows x64 build has run here.
Successful libsodium compilation, actual MSBuild compiler/linker/resource paths,
COFF `/MT` directives, provider verification and host behavior remain required
native evidence, not implied by Linux unit fixtures. macOS ARM64 recipe/source
integration is present but native compile, SDK/link behavior, tests and filesystem
semantics remain unverified. Both platforms remain **UNQUALIFIED** until actual
native execution is reviewed. No hosted seven-job acceptance is declared.

[Preservation validation](evidence/llvm23-repair/integration-2026-11-07/preservation-final.json)
compares the 81,290 protected inputs from the accepted experiment, accounting for
378 old-prefix files retained at their new paths. Only the twelve authorized
tracked integration paths changed; two permanent patch/policy files were added.
No unexpected original source, evidence, archive, source cache, root README,
website or workflow change occurred. The current documentation revision is
retained except these scoped report/toolchain/notice updates; no Markdown files
were added or removed. The historical Stage 4 CI commit/run identity in
`ADVANCED_LANGUAGE.md` remains unchanged. The
[review validation](evidence/llvm23-repair/integration-2026-11-07/review-validation.json)
records exact diff replay and local link checks.

**RETAIN** this controlled local source/receipt/package integration for Golden
Wrench/Boss review, with the two upstream oracle limitations and native-platform
qualification gaps explicit. No staging, commit, branch, push, history change,
hosted CI dispatch, binary publication, LLVM 19 use, test suppression or test
removal occurred. Stop here for independent review; Boss retains Git and hosted
integration controls.

## CI 42369685905 review and macOS host-test repair

7 November 2026. Reviewed the exact hosted commit
`9521329e7e67f4c8bb5aec38d684fde3f3c887de` in
[run 42369685905](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905).
The accepted selector and integrated backport were still pending locally and are
absent from that commit. Their production integration was **not** redone,
overwritten or duplicated. This follow-up changes one test call site and appends
this record; all accepted dependency/toolchain source, pins, recipes, receipts,
packaging rules and prior evidence remain intact.

### Exact hosted outcomes and retained evidence

All seven jobs are now completed. The run conclusion is **cancelled**, comprising
four failed jobs and three cancelled jobs:

| Job | Final outcome / demonstrated failure |
| --- | --- |
| [Windows Release, 128395046546](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046546) | Failed host bootstrap: MSB8073 requests resource selector `19` under installed `llvm-23.1.4/lib/clang/19`. |
| [Windows Debug, 128395046571](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046571) | Same demonstrated MSB8073 selector mismatch. |
| [Ubuntu Sanitizers, 128395046448](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046448) | Failed unpatched libsodium `make check`: 72/87 passed, 15 failed, zero skips. |
| [macOS Release, 128395046593](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046593) | Build failed at `tests/host_capabilities/portable.cpp:102`: ambiguous `wait`; diagnostics identify both POSIX `::wait` and VOID's operation enum. |
| [Ubuntu Release, 128395046619](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046619) | Cancelled during build. This is the job URL supplied in the follow-up; it is not a macOS job. No completed test acceptance follows from it. |
| [Ubuntu Debug, 128395046667](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046667) | Cancelled during build; no completed test acceptance. |
| [macOS Debug, 128395046673](https://github.com/Shmellyorc/Void.Language/actions/runs/42369685905/job/128395046673) | Cancelled during build. No additional compiler error appears in the captured log, and no completed host/application qualification is claimed. |

The fifteen actual unpatched sanitizer failures are `aead_aegis256`,
`aead_aegis128l`, `aead_aes256gcm`, `aead_chacha20poly1305`,
`aead_xchacha20poly1305`, `box_easy2`, `box_seal`, `codecs`, `generichash`,
`generichash3`, `misuse`, `secretbox_easy`, `secretbox_easy2`, `sodium_utils`, and
`pwhash_argon2id`. This set/count is retained as observed; it does not replace the
previous runs' different failure sets. This artifact lacks the individual
upstream `.log`/`.trs` diagnostics, so no per-test sanitizer diagnostic is invented.
The complete captured bootstrap log records the names, totals and exit-2 failure.

Seven raw job logs and eleven original diagnostic ZIP artifacts were downloaded
read-only. Every ZIP SHA-256 matches GitHub's published artifact digest. Original
bytes and selected text diagnostics are retained under
[evidence/ci-42369685905/](evidence/ci-42369685905/), with the
[final status/failure summary](evidence/ci-42369685905/ci-summary.json),
[download hashes](evidence/ci-42369685905/downloads.json), and
[exact artifact metadata](evidence/ci-42369685905/artifacts.json).
The connector log-save request exceeded its 64,000-byte code limit; full logs were
then saved with read-only `gh api` downloads. No hosted action was dispatched.

Both hosted macOS configurations completed their **old unpatched** provider
bootstrap (87/87 upstream targets). That is evidence about the hosted commit,
not native acceptance of the pending approved backport. No additional defect
class was demonstrated in the seven completed job logs and eleven artifacts.
Cancelled jobs retain their actual incomplete scope.

### Minimal source correction and applicable checks

The [actual new source diff](evidence/ci-42369685905/portable-wait.patch) is exactly:

```diff
- q=request(wait);
+ q=request(vd::host::wait);
```

The surrounding status/repeated-wait/cleanup assertions remain unchanged. There
is no production API, Runtime, native operation, compiler or dependency change in
this follow-up. Namespace qualification selects VOID's existing operation;
removing or renaming POSIX declarations is unnecessary.

The actual compile-command entries from the existing clean Release/Debug/strict
builds were reused for a focused syntax check with `<sys/wait.h>` explicitly
visible. The original source fails with the same ambiguous-name diagnostic in
all three profiles; the qualified source compiles in all three. This reproduces
the declaration collision on Linux; it is **not** presented as a macOS cross-build
or native execution result.

`void_host_portable` rebuilt successfully in all three local profiles. Current
focused CTest results are **Release 4/4, Debug 3/3, strict 3/3**: portable provider,
receipt rejection and retained toolchain/CI policy tests in every profile, plus
installed/relocated source-package and normal/O2 JIT/AOT checks in Release.
Portable execution retains its real **1 MiB hard stack**. The toolchain fixture
suite remains 10/10 and dependency admission/rejection remains 18 checks per
profile. Strict retains fatal ASan/UBSan/leak options and the pinned symbolizer.
The original ambiguous-source failures are expected controls, not hidden passing
compilations. Full commands, statuses and outputs are in
[local-results.json](evidence/ci-42369685905/local-results.json) and the per-profile
logs/receipts.

All three schema-3 provider prefixes were reverified against the same accepted
source/patch/postimages, eight recipe hashes, SDK/tool identities, installed
static libraries and zlib closure. The pristine source caches and original
libsodium/libarchive archive hashes still match their pins. The accepted repair
source/package files and all 3,123 indexed prior integration evidence files
matched their recorded hashes before this append-only report update. See
[accepted-repairs-present.json](evidence/ci-42369685905/accepted-repairs-present.json).
The earlier Release/Debug/fatal-strict **87/87 upstream target** results remain
retained and bound to the unchanged receipts; those full provider builds/tests
were not redundantly repeated for this test-only call-site edit. The two upstream
utility output-oracle limitations remain unchanged and explicit in the preceding
integration record.

The package audit was rerun and passed **5/5 groups**, including source inventory,
Linux compiler/tooling components and Windows notice file rules. Release's
installed test again reconstructed the packaged applied source outside the
checkout, rejected a corrupted patch, and exercised relocated normal/O2 JIT and
independent AOT. Windows file-rule checks do not constitute native acceptance.
The [current package results](evidence/ci-42369685905/license-package/validation.json)
retain their exact scope. No sanitizer weakening, suppression, test removal,
pin/hash change or ambient LLVM selection was introduced.

### Complete pending inventory for Boss

The old `docs/voidup/COMMIT_PATHS.*` predates the current backport and omits its new
patch/provenance files. Its historical contents and evidence indexes are preserved.
For this pending integration, use the new exact
[PENDING_PATHS.txt](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/ci-42369685905/PENDING_PATHS.txt),
[literal NUL-delimited allowlist](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/ci-42369685905/PENDING_PATHS.nul), and
[PENDING_INVENTORY.json](evidence/repair-raw-evidence-2026-11-07.tar.xz#docs/voidup/evidence/ci-42369685905/PENDING_INVENTORY.json).
The inventory covers every current intended modified/new file: accepted LLVM
selection/rejection helpers and tests, both provider recipes/verifier, backport
and provenance, CMake identity/package rules, source-package tests, accepted
operational/historical documentation revision, this test correction, notices,
and all original/new repair evidence including ignored logs. The small existing
benchmark/verification-tool edits are retained LLVM 19 default-removal changes;
no new benchmark/performance work was done.

Already committed Phase A/B implementation files at `9521329e7e67f4` do not need
to be restaged wholesale. The new inventory is relative to the actual current
HEAD and includes hashes and explicit required integration paths. It rejects
unclassified changed files, symlinks, native outputs and private keys. Only the
fourteen specifically identified, hash-verified CI diagnostic ZIPs are permitted
archive exceptions (three earlier incident artifacts and eleven from this run).
No source dependency archives or private fixture-signing keys enter the allowlist.

Build/toolchain/cache roots remain excluded. The TCP/UDP handoff and six unrelated
website paths remain explicitly protected/excluded; none is currently a pending
Git-visible change. Root README, workflows, compiler/Runtime production source,
Phase A/B application source and all prior evidence are unchanged in this follow-up.
Exact counts and every intended/excluded path are in the machine-readable inventory.
The current cumulative
[pending source diff](evidence/ci-42369685905/integration-repair-pending-source.patch)
and [documentation diff](evidence/ci-42369685905/documentation-pending.patch) include
the previously accepted pending work as well as this correction; the separate
one-line patch identifies this assignment's production-source delta (a test only).

Historical staging command from this review, superseded by the cleanup inventory
below; **not executed here**:

```sh
git --literal-pathspecs add -f --pathspec-from-file=docs/voidup/evidence/ci-42369685905/PENDING_PATHS.nul --pathspec-file-nul
```

Suggested title:
`Repair VOIDUP provider provenance, LLVM 23.1.4 selection and host test portability [skip ci]`.
Do not use the old incomplete path list or `git add -A` for this pending repair.

**Recommendation: RETAIN for Golden Wrench/Boss review before a replacement run.**
Windows x64 actual selector/compiler/linker/resource/`/MT` success and macOS ARM64
build/execution of the integrated candidate remain **UNQUALIFIED**. The observed
old-commit native bootstrap is not substituted for that missing evidence. The
three cancelled jobs also provide no completed acceptance. No new demonstrated
defect remains unrepaired within the available evidence, but no future hosted
success is promised. HEAD and the staging area are unchanged; no commit, push,
branch, hosted dispatch, LLVM 19 use or binary publication occurred.


## Source-control cleanup and exact repair inventory

7 November 2026. Boss requested a reviewable pending tree, without losing accepted
repairs or historical results. **The current exact commit list is
[REPAIR_COMMIT_PATHS.txt](REPAIR_COMMIT_PATHS.txt), with a
[literal NUL-delimited copy](REPAIR_COMMIT_PATHS.nul) and
[per-file classes/hashes](REPAIR_COMMIT_INVENTORY.json).** It supersedes all earlier
staging lists. No staging, commit, push, branch or hosted dispatch occurred.

| Git file count | Before | After |
| --- | ---: | ---: |
| Modified tracked | 88 | 89 |
| Untracked visible | 3,568 | 70 |
| Total visible pending | 3,656 | 159 |
| Staged / added / deleted / renamed | 0 | 0 |
| Ignored untracked files, including existing build/toolchain caches | 769,617 | 773,136 |

These are actual per-file Git queries, rather than collapsed directory entries
or UI estimates. The ignored count includes the local audit's generated files;
those build/toolchain/cache roots remain outside the commit. The final tree is
intentionally dirty with the required repairs, with **zero unclassified pending
paths**. The eighty-eight original modified paths remain; the sole additional
modified tracked file is `.gitignore`.

The prior **5,879-path** force-add list reconciles exactly to **90 source/build/
patch/documentation paths, 62 retained loose evidence records, and 5,727 archived
raw evidence paths**. Its 2,223 already ignored evidence files were mistakenly
included as individual commit candidates. That list is now historical evidence,
not a current staging instruction.

The bounded ignore additions cover only the two generated repair-evidence trees
and the three exact provider qualification runs. Explicit exceptions retain the
62 chosen records. All accepted source, patch/provenance and required package
inputs remain visible; no source directory or unrelated work was ignored.

Permanent evidence retains the fourteen original CI diagnostic ZIPs, their
published hashes/metadata, exact failure sets, actual source review diffs,
qualification/vector outcomes, negative controls, receipt identities and native
limits. The other loose records are generated per-test `.trs`/`.res` output,
compile logs, extracted copies of those ZIPs, repeated receipt/source snapshots,
replay scratch records and obsolete inventory outputs. They are excluded as
individual Git paths. No original evidence file was deleted or moved.

To preserve complete access without thousands of loose commit paths, those
5,727 records are stored losslessly in the
[raw-evidence archive](evidence/repair-raw-evidence-2026-11-07.tar.xz), with an
[exact restoration index](evidence/repair-raw-evidence-2026-11-07.json).
It reduces 100,968,237 raw bytes to 5,990,576
archive bytes. Identical contents/modes share earlier-member hardlinks; timestamps
are normalized to zero. The archive contains only validated repository-relative
evidence paths and safe hardlinks, no native build output, private signing keys,
provider source tarballs or symlinks. **Every member was extracted into a fresh
temporary directory and its SHA-256 matched**; the temporary verification copy
was then cleaned. All original loose files still match their hashes locally.

For example, from the repository root, inspect the preserved records in a fresh
local directory (this command is documentation, not a requested Git operation):

```sh
mkdir -p /tmp/voidup-repair-records
tar -xJf docs/voidup/evidence/repair-raw-evidence-2026-11-07.tar.xz -C /tmp/voidup-repair-records
```

Archive link fragments name the original member path. Required live documentation
links still resolve to retained files/directories; six links to archived raw
records or obsolete allowlists now identify their exact archive member. No
historical test outcome, CI identity or approval was changed. The restoration
index also keeps the exact 5,879-list reconciliation and all per-member hashes.

The exact inventory classifies production/build/CI files, approved dependency
patch/provenance/receipt verification, permanent documentation/evidence, and the
ignore housekeeping change. Existing `.toolchains/`, `build*/`, caches, native
binaries and private fixture keys are excluded. Root README, the TCP/UDP handoff
and all six protected website paths remain unchanged. There is no unrelated
pending change to discard or stage.

Verification: all three provider receipts/source trees/SDK identities still
verify; all **18 receipt checks per profile** and **10 retained toolchain tests**
pass. The qualified `vd::host::wait` test compiles with POSIX `::wait` visible in
Release, Debug and strict. Python source syntax and `git diff --check` pass. The
accepted eight recipe inputs, exact reviewed backport and original pins are
unchanged. Earlier full 87-test suites, fatal sanitizer settings, installed-source
reconstruction, package audits and two upstream oracle limitations are preserved;
no unnecessary rebuild or full-suite rerun was performed for ignore/evidence
maintenance. Native Windows/macOS candidate execution remains **UNQUALIFIED**.

The [cleanup verification record](evidence/source-control-cleanup-2026-11-07.json)
contains actual before/after counts, source-preservation checks, reference checks,
archive results and the original-to-final classification. The old loose allowlist
and original results remain accessible through the archive and local ignored
copies, rather than being erased or relabeled as passing results.

Recommended command for Boss's reviewed current inventory, **not executed**:

```sh
git --literal-pathspecs add --pathspec-from-file=docs/voidup/REPAIR_COMMIT_PATHS.nul --pathspec-file-nul
```

No force-add is required: the current allowlist contains only deliberate visible
changes. Suggested title remains:
`Repair VOIDUP provider provenance, LLVM 23.1.4 selection and host test portability [skip ci]`.
**RETAIN for independent Golden Wrench/Boss review.** No prior result was discarded
and no additional compiler, Runtime, dependency or feature change was introduced.


## CI #42377496702 cross-platform repair

7 November 2026, follow-up to [run 42377496702](https://github.com/Shmellyorc/Void.Language/actions/runs/42377496702)
at `799a29e3c0b0e204abecd4dc1f14356bf1df3443`.
**Recommendation: RETAIN the demonstrated repairs for Golden Wrench review; hosted acceptance remains UNQUALIFIED.** No staging, commit, branch, push or hosted dispatch occurred.

Native Windows Release and Debug completed the provider, compiler and host/VOIDUP
execution chain. All 350 registered test names were exercised in each profile;
the original failures, interruptions and explicit corrective reruns remain
separate records. This is not a claim that either original unfiltered command
passed, or that the replacement hosted run has passed.
The indexed `native-effective-test-results.json` reconciles names against the
original records and passing rerun XML: each profile has 350 effective passes,
zero remaining failed names and zero remaining skipped names.

### Review artifacts and exact changes

The [source diff](evidence/ci-42377496702/source.patch) contains the following
eleven source/build/test changes. The [documentation diff](evidence/ci-42377496702/documentation.patch)
contains this report revision. The [original CI archive](evidence/ci-42377496702/original-ci.tar.gz)
and [index](evidence/ci-42377496702/original-ci-index.json) preserve all seven job
logs, twelve diagnostic ZIPs and metadata: 27 members, each read back and
SHA-256 verified. The [local verification archive](evidence/ci-42377496702/local-verification.tar.gz)
and [index](evidence/ci-42377496702/local-verification-index.json) preserve actual
commands, original failures, native probes, receipts, XML results and reruns.

| File | Demonstrated defect and correction |
| --- | --- |
| `.gitattributes` | Windows checkout converted the approved patch and provenance JSON to CRLF. Protect `third_party/host-patches/**` with `-text`; preserve existing approved hashes. |
| `tools/setup_host_deps_native.py` | Query resolved MSBuild `TargetPlatformVersion`, not the `WindowsTargetPlatformVersion=10.0` alias. Pass selected zlib paths to CMake with forward slashes, avoiding the actual `\\U` parsing failure. |
| `src/runtime/host_directory_windows.cpp` | Add metadata-read rights for ancestor handles; retain directory flush/query/ACL rights without persistent `DELETE`; use a temporary identity-checked source handle and native handle-relative rename; reject stale source aliases before mutation and preserve native metadata-query errors. |
| `src/runtime/host_windows.hpp` | Recognize `OWNER_RIGHTS` only on ancestors whose owner already passed the existing trust check. Keep exact-current-user-only private-leaf DACL enforcement. |
| `tests/host_capabilities/portable.cpp` | Exercise OWNER_RIGHTS admission and hostile ACL rejection, controlled process rejection before launch, same/cross-parent publication, collisions, replacement/held data, stale/fresh aliases and native error details. Bound the write-count conversion. Existing `vd::host::wait` qualification remains. |
| `tests/host_capabilities/portable_source.py` | Compare exact UTF-8/LF output, matching Runtime `WriteFile`; remove the incorrect CRT CRLF expectation without normalizing captured bytes. |
| `tests/stage8/sprint4.py` | Native applications accept arguments under Phase A. Check the existing program's actual expected status, rather than obsolete argument-rejection exit 2. |
| `tests/stage9/workflow.py` | Correct the same stale native-argument oracle. Bare CLI and legacy numeric-entry rejection checks remain unchanged. |
| `tests/voidup/ci_integration.py` | Add forced `core.autocrlf=true` checkout-filter byte/hash regression for the approved host patch and provenance. All 11 integration checks pass. |
| `tests/net/http/reload.py` | A deliberately held request timed out before Debug replacement compilation finished. Disable read/total deadlines only for that held request and release the peer explicitly. Retain generation/GC/results, cleanup, 180-second subprocess and 240-second CTest bounds. |
| `CMakeLists.txt` | Set only the aggregate `http_examples` watchdog to 600 seconds after all 19 unchanged checks took over 300 seconds. Keep every individual 120-second bound and all assertions/negative controls. |

The checkout reproduction matched both hosted failures. The approved patch is
10,830 bytes with SHA-256
`f0de9004b8129626e14750437f51eb9524f030149607b50bb75ce0b6c998af74`;
CRLF conversion produced 11,061 bytes and
`5296d2bd7d55cacc9435b426c824bb0f716c9a4730bb9c8b7678fdf40effdad9`.
The provenance JSON changed from
`4841bfe7f283b250bb2b6fa1872b602416bb91a0e67c5f6179742d8c4bc0c0c8`
to `98a71c6819a75a752f39646708d44e5f95e28592012ac8d8f6170846a8f0373a`.
Expected hashes were not changed to accommodate converted files.

### LLVM, providers and ownership evidence

The accepted LLVM selector repair was already present at the baseline and was
retained. Actual installed MSBuild imports show
`Microsoft.Cpp.Default.props:250` defaulting absent `LLVMToolsVersion` to
`19.1.5`; ClangCL common properties then derive resource selector `19`.
VOID explicitly uses the resource selector returned by the pinned compiler:
**`23`**, under `lib/clang/23`, for release **23.1.4**. Native builds invoked the
verified SDK's `clang-cl`, `lld-link` and `llvm-lib`. No LLVM 19 compiler was
installed or selected. Tool hashes and the evaluated property evidence are
retained in the native records.

Native Release and Debug provider builds passed in 358.38 and 300.46 seconds,
respectively, using final recipe SHA-256
`42b30dc4d52bd66d55ad779ed3c2162c0ac0043654b5326011a4eee3a6f67bca`.
Each passed receipt verification, nine pinned-receipt checks and 17 approved-
backport admission/rejection checks. Actual
COFF directives accepted `/MT` and rejected `/MD`, `/MTd`, `/MDd`, and `/MT`
without default-library directives. Both profiles' libsodium and libarchive
archives passed the static ABI guard. Actual MSBuild rejected full-release
resource selector `23.1.4` instead of SDK-derived `23`, and missing resources.
The earlier nonnumeric selector rejection is preserved separately. The Windows
MSBuild recipe does not execute or claim the Unix 87-test suite.

Both current receipts bind eight exact recipe inputs, approved source policy,
tool identities and the physical Windows SDK. Old receipts correctly rejected
the changed recipe; none were rewritten to claim a rebuild. The original
libsodium 1.0.22 archive SHA-256 remains
`adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`.
The approved patch, all three postimages, applied tree
`e545c52f26e44f16d99db575a382af024100f46708f23f8051042964e0d698dd`,
pin/revision, schema 3, cryptographic vectors, verification policy, corresponding
source rules and LLVM 23.1.4 pins remain unchanged. Production HTTP and its
dedicated timeout tests remain unchanged.

The initial ancestor failure was native error 5 after a successful
`NtCreateFile`; adding `FILE_READ_ATTRIBUTES` supplied the documented
[metadata right](https://learn.microsoft.com/en-us/windows/win32/fileio/file-access-rights-constants).
Python 3.14's protected temporary-directory ACL uses
[OWNER_RIGHTS](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/81d92bba-d22b-4a8c-908a-554ab29148ab).
Its narrow ancestor admission passed, while Everyone/BuiltinUsers mutation and
OWNER_RIGHTS private-leaf admission still failed before unsafe work.

Publication probes retained Win32 rename error 87, native target-sharing error
32, and rejected alternatives that lost flush rights or failed `ReOpenFile`
with error 5. The final native rename uses the held target and a temporary
source-only delete handle; it verifies volume/file identity and strict DACL,
closes that temporary handle before parent flush, and preserves published versus
durable outcomes. No absolute-path reconstruction or release of the held target
was introduced. The observed Win32 failure is not a claim that its public
structure universally forbids `RootDirectory`; see the
[native rename contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information).
VOIDUP continues to hold `writer.lock` across transaction dispatch. Sequential
stale aliases are rejected; no broader arbitrary uncoordinated-writer guarantee
or new internal scheduler was added.

### Native Windows results

| Check | Actual result |
| --- | --- |
| HTTP dependency bootstrap and closure | Release and Debug rebuilt and verified natively, 429.5 and 468.0 seconds. |
| Compiler configuration/build | Real ORC compile/link/run passed in both profiles. Full compiler, host fixtures and VOIDUP built in both profiles. |
| Release original unfiltered suite | **342 PASS, 7 FAIL, 1 SKIP**, 3,388.94 seconds. Original XML retained. |
| Release corrective execution | **14/14 PASS**, 189.75 seconds, including all seven original failures and conditional symlink branches. Separate unchanged cross-volume test PASS, 3.14 seconds. |
| Debug complete name coverage | Attempt 13 completed 316 names: **314 PASS, 2 FAIL**, then interrupted during 317. Attempt 14 ran exactly 317–350: **34/34 PASS**, 1,358.95 seconds. No skipped names. |
| Corrected HTTP reload / unchanged timeouts | Release **PASS 34.64 / 6.84 seconds**; Debug **PASS 103.36 / 6.86 seconds**. |
| Corrected examples aggregate budget | All 19 checks PASS under CTest: **Release 90.82 seconds; Debug 303.35 seconds**. |
| Final portable fixture and source execution | Final fixture bytes passed in both profiles. Source tests exercised 30 normal/O2 JIT and independently linked AOT invocations per profile. |
| Installed execution | Native `cli_install` passed Unicode relocation, installed compiler/LSP, independent AOT and missing stdlib/Runtime rejection preserving old output. |
| Stack headers | Actual Release/Debug compiler, portable-host fixture and VOIDUP PE headers reserve **1,048,576 bytes** each. Host fixture commits that amount; compiler/VOIDUP commit 4,096 bytes initially. |
| Corresponding-source package | Native compiler-component install, eight recipe hashes, original archives, licenses, three reconstructed postimages and tampered-patch rejection PASS. Installed compiler version/help PASS. VOIDUP is a separate built application, not part of that compiler install component. |
| Editor and control relink | Pinned task-local Node24.19.0/npm11.17.0: `npm ci` PASS65.31s; four unit tests PASS11.07s; actual VSCode1.141.0 trusted/untrusted/multiroot host checks PASS145.46s; local VSIX package PASS23.67s. Control-mode voidc-only relinks PASS: Release6.42s, Debug4.19s, then version/help/PE checks. Prior EXE/PDB retained; no publication. |

Editor dependency inputs remained unchanged. npm reported six existing development-dependency vulnerabilities (four moderate, two high) and install-script notices; diagnostics are retained without audit-fix or pin changes. The relinks used local control-mode flags, not the workflow's ThinLTO-cache configuration; these timings do not establish hosted cache behavior or performance.

The seven original Release failures were two obsolete argument oracles and five
symlink cases: `log_files`, `io_execution`, `io_async_execution`,
`io_wire_normal`, `io_wire_optimized`. The existing limited user token returned
native error 1314. An unchanged-API probe recorded zero progress/cleanup, then
created the link under the existing same-user elevated token with identical
source bytes and SID. Standard UAC consent supplied that token; no account,
password, ACL or security policy was weakened. GitHub documents
[administrative Windows runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners#administrative-privileges).
The separate cross-volume check used NTFS to VBoxSharedFolderFS; it does not
establish NTFS-to-NTFS behavior.

The original Debug reload failed after 105.59 seconds. The probe proved a
fixture race: `timeout/download` arrived after
**30.013 seconds**, before replacement compilation completed after
**35.426 seconds**. The old activation had legitimately completed; the peer's
60-second hold had not expired. All generation/GC/repeated-await/return-value
assertions remain. The first diagnostic probe's invalid enum `==` comparison
failed before HTTP; its failure and the corrected `is` probe are preserved.
Both restored exact original fixture bytes and rebuilt before final correction.

Original Debug `http_examples` timed out at 300.01 seconds after 18 recorded
successful checks totaling 268.364 seconds. Unchanged direct execution then passed **19/19 in 302.5353
seconds**, with command durations alone totaling **300.3381 seconds**. Streaming,
normal cancellation and header-timeout negative control remained below their
individual 120-second limits. Peer events and the expected connection-abort
diagnostic remain. This justified the aggregate 300 → 600 second adjustment.
No examples command deadline, assertion or test was removed. The separate
reload fixture intentionally disables only its held request's read/total deadlines.
Neither HTTP fixture
change affects provider recipe hashes or compiler cache-identity inputs.

### Linux and seven-configuration acceptance

Fresh final-recipe PikaOS 4 / glibc 2.44 providers passed in all three profiles.
Each used a new disposable build tree; prior prefixes and receipts were retained
under `.toolchains/host/retained/ci-42377496702/`. Existing compiler trees were
reconfigured and the relevant targets rebuilt/relinked.

| Local profile | Upstream targets | Receipt checks | Focused CTest results |
| --- | --- | --- | --- |
| Release | **87/87 PASS**, zero fail/skip; provider build106.57s | Nine pinned-receipt +18 approved-backport checks PASS | Host/VOIDUP11, application2, HTTP3, frontend1: **17 PASS** after the separately retained frontend harness correction. |
| Debug | **87/87 PASS**, zero fail/skip; provider build94.19s | Same checks PASS | Host/VOIDUP11, application2, HTTP3, frontend1: **17 PASS**. |
| Fatal strict | **87/87 PASS**, zero fail/skip; provider build168.80s | Same checks PASS | Host/VOIDUP11, application2, HTTP3: **16 PASS**. |

Every listed CTest group ran with both soft and hard stack limits of1,048,576
bytes. HTTP and VOIDUP dependency-closure and registered-test-plan checks
passed in each profile. Strict retained
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86` and the pinned
LLVM23.1.4 symbolizer. Normal/O2 JIT, independent AOT, install/resource and
negative checks remained enabled. The initial Release frontend was **Not Run**
because this task's focused build list omitted `void_tests`; its original XML
and harness failure remain, followed by an explicit target build and a passing
3.88-second rerun. No production correction was needed for that harness error.

Executed local harness commands were
`python3 build/ci-42377496702/run_linux_fresh.py`, followed by the bounded
`python3 build/ci-42377496702/run_linux_fresh_continue.py` after that missing-
target correction. The archive includes both exact scripts, per-command argv,
durations, exit codes, original failure and separate continuation record.
Fresh Release compiler-component packaging also passed all eight recipe hashes,
original archive/license checks, offline three-postimage reconstruction and
rejection of a tampered patch before source-output creation.

Ubuntu24.04.5 / glibc2.39 container userspace passed fresh Release providers
(**87/87 upstream**, zero fail/skip), nine pinned-receipt plus18 backport
checks, real LLVM ORC configure, Runtime build and **2/2 native host tests**
under a hard1MiB stack. Provider build took108.92s; aligned configure19.92s,
Runtime build100.40s and CTest5.08s. This is Ubuntu userspace on the PikaOS
kernel, not a full Ubuntu compiler/JIT/AOT/Debug/strict or hosted-job pass.

The first Ubuntu CMake configuration correctly rejected five differing logical
SDK tool paths: providers were built under the copied source tree, while the
harness configured the original checkout. SDK identity and all five tool hashes
matched. A separate source-context continuation completed the exact disposable
source copy and configured it in `build-aligned`; the receipt hash remained
unchanged. The original failed configure is retained. No receipt rewrite,
verification bypass, dependency or production-source change was needed.
Actual commands were `python3 build/ci-42377496702/run_ubuntu_fresh.py` and its
`run_ubuntu_fresh_continue.py` continuation. The existing container was restored
to its original exited state; Podman's retained stop log reports its10-second
SIGTERM timeout and container SIGKILL. This did not operate the Windows VM.

The two previously documented strict ASan output-oracle limitations remain
explicit: `sodium_utils2` and `sodium_utils3` execute under fatal instrumentation,
but their output comparisons are limited by the existing upstream sanitizer
path. **87 passing test targets are not 87 output comparisons.** The remaining
85 comparisons and all original failure/control evidence are retained.

| Configuration | Original hosted result / last established point | Subsequent verification and outstanding hosted acceptance |
| --- | --- | --- |
| Ubuntu Release (`128417991849`) | **UNQUALIFIED**: canceled during HTTP libunistring build. | Ubuntu24.04.5 container Release providers87/87, ORC configure and native host2/2 passed as scoped above. Complete hosted dependency/compiler/application/test chain remains outstanding. |
| Ubuntu Debug (`128417991924`) | **UNQUALIFIED**: HTTP bootstrap and 167-file closure passed; canceled at cache save. | PikaOS Debug evidence is separate. Full Ubuntu Debug host-provider/compiler/test chain remains outstanding. |
| Ubuntu Sanitizers (`128417991963`) | **UNQUALIFIED**: canceled during HTTP libunistring build. | PikaOS fatal strict evidence is separate. Full hosted Ubuntu HTTP/host 87-test/compiler/application chain remains outstanding. |
| Windows Release (`128417992061`) | **FAIL**: CRLF source-policy mismatch before MSBuild. | Native final providers, compiler, all test names with explicit corrective reruns, package and ancillary checks above. Server 2022 / SDK26100 / Python3.12 workflow execution remains outstanding. |
| Windows Debug (`128417992069`) | **FAIL**: same independently recorded source-policy mismatch. | Native final providers, compiler, all 350 names and corrected HTTP reruns above. Same hosted environment/workflow differences remain. |
| macOS ARM64 Debug (`128417991992`) | **UNQUALIFIED**: patched provider **87/87 PASS**, zero fail/skip, schema-3 receipt verified; canceled during ORC configure. | Historical receipt predates final recipe changes. Final-recipe providers, compiler configure/build and native tests remain unverified. |
| macOS ARM64 Release (`128417992071`) | **UNQUALIFIED**: HTTP closure/preflight passed; canceled during host bootstrap without completed receipt. | Final-recipe providers and all 87 tests, compiler/application build and native tests remain unverified. |

Canceled jobs are not passing jobs. No additional Ubuntu/macOS production defect
was demonstrated by cancellation. The macOS jobs did not reach the existing
explicitly qualified `vd::host::wait` call site. No native macOS host was available. Mac Debug
used Xcode 26.6, SDK 26.5 and deployment target 14.0; sodium configure selected
Apple `/usr/bin/ld`. Verified `ld64.lld` receipt identity is not proof that every
macOS link invoked LLD, and no unrelated linker-policy change was made.

### Retained failures and environment limits

The local guest is Windows 10 build 19045 despite VM name “Windows 11”, VS Build
Tools 17.14 / VC14.44.35207 / SDK10.0.19041.0 / Python3.14.8. Hosted Windows was
Server2022 / SDK10.0.26100.0 / Python3.12 with the same VC tools. The disposable
checkout is `C:\Users\a\VoidCI42377496702-Native`; the original developer
checkout and verified physical SDK were preserved. The SDK companion libraries
were built only in the disposable checkout. Native AOT `-###` selected that
SDK's `lld-link`, Clang23 resources and `libcmt`; actual AOT build/run results
are separate evidence. No AOT linker-policy change was justified.

| Original local obstacle | Preserved diagnosis / handling |
| --- | --- |
| Missing ClangCL MSBuild support | Installed Microsoft's support component, whose closure excludes the separate compiler payload. No LLVM19 compiler installed. |
| Missing Perl / omitted snapshot curl patches | Used existing Git Perl through task PATH; restored two exact tracked patch files to the disposable snapshot. No dependency changes. |
| SDK junction rejected at final AOT step | Selected the same verified physical SDK executable; retained the reparse guard. |
| Existing normal Temp ACL had foreign-SID Modify grant | Left ACL unchanged; existing staging helper created protected task TMP/TEMP. |
| Early 20GB/7CPU guest overload | Retained overload/interruption evidence. Boss restarted/configured the VM; resumed measured settings were 8,000MB/4CPU. One outer build/test chain ran at a time. |
| Debug disk interruptions | Preserved attempts stopped at 1,160/1,548, then 285/389; final 105 links passed in 213.63 seconds. Attempt12 stopped during test7; attempt13 stopped during317; exact remaining34 subsequently passed. No interruption was counted as PASS. |
| Diagnostic/bookkeeping failures | Retained invalid diagnostic enum comparison, original CTest status parser failure, rejected rename/reopen probes and wrong-selector controls. The local Linux focused harness initially omitted the frontend executable: its Not Run result remains, followed by an explicit target build and separate corrective rerun. |

Only exact task-generated EXE/PDB files received classic NTFS compression, with
before/after logical SHA-256 and reparse checks. No original source, SDK or
archive was deleted. The first 251-file Debug pass reduced allocation from
8,602,612,224 to 4,302,893,056 bytes and raised free space from 7,856,480,256 to
12,152,918,016. A Release 512-file pass raised free space from 1,030,545,408 to
2,112,487,424. A forced 1,024-file Debug pass raised free space from 2,210,074,624
to 7,508,295,680 while reported summed allocation changed only 8,192 bytes;
the discrepancy's mechanism is unproven. A final bounded 756-file pass raised
free space from 2,260,697,088 to 5,449,908,224 before the remaining34 tests.
Windows displayed its own Storage Sense notice; this task did not change that
setting. After the 285/389 interruption, free space recovered from about 2.54GB
to 7.55GB without another cleanup/compression action; cause is unproven. A later
disk hold measured 993,304,576 free bytes in five readings over 60 seconds.
Original hosted preflight had over150GB/228GB workspace free and over
84GB Temp free, so no hosted disk-policy defect was inferred. Local compressed
outputs and control-mode relink timings are not release-performance claims.

No Wine, VM power operation, saved-state operation, authentication bypass or
security weakening was part of resumed native verification. Boss controlled
the VM. Earlier emulated probes are not native acceptance evidence.

### Final inventory and review boundary

The final allowlist contains **21 paths: 12 tracked modifications and nine new
review artifacts**, with zero staged paths. See the exact
[readable allowlist](evidence/ci-42377496702/commit-paths.txt),
[NUL-delimited allowlist](evidence/ci-42377496702/commit-paths.nul) and
[per-file inventory](evidence/ci-42377496702/commit-inventory.json).
The eleven source paths are listed above; the twelfth tracked path is this
report. The nine new artifacts are the original/local archive-index pairs,
source/documentation patches, both allowlists and the inventory JSON.

At the final curation boundary, qualification had produced **540 Git-visible
pending paths** (12 modified,528 untracked). After preserving the three fresh
provider evidence directories under ignored
`build/ci-42377496702/linux-fresh/retained-provider-evidence/` and retaining their
named-test aggregate logs and both ASan oracle records in the indexed archive,
the final pending count is21. Every moved file's hash was checked; no original
failure, source, archive or user file was deleted. Redundant passing per-test
output remains locally available. This count describes this follow-up's
qualification output, not the earlier historical3,000/5,879-path cleanup.

Recommended exact staging command for Boss, **not executed**:

```bash
git --literal-pathspecs add --pathspec-from-file=docs/voidup/evidence/ci-42377496702/commit-paths.nul --pathspec-file-nul
```

Suggested title: `Repair native Windows VOIDUP and preserve LLVM 23 provider provenance [skip ci]`.
Local link/reference validation, source parsing, source-diff identity,
`git diff --check`, archive member readback and the final inventory checks pass.
No unexpected pending or staged paths remain.

All original failures remain accessible in the indexed archives and local raw
records. Generated builds, extracted sources, caches, binaries, screenshots and
temporary experiment output are excluded from the commit inventory. The
original pinned archives, approved patch, receipt checks and corresponding
source packaging remain intact. No unrelated website/user work was changed.

RETAIN these source corrections and evidence for independent Golden Wrench
review and Boss-controlled integration. Native Windows core and ancillary
verification is complete for the stated local environment. Both macOS native
configurations, the complete hosted Ubuntu jobs, Windows hosted environment
and ThinLTO-cache workflow conditions still require the replacement hosted run.
No hosted acceptance or release-performance claim is made. Work stops here for
Golden Wrench/Boss review.
