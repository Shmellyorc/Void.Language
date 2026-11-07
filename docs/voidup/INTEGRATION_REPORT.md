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
