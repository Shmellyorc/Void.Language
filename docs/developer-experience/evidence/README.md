# Developer-experience evidence

All results are local unless explicitly identified as the previously accepted
GitHub CI metadata. No binaries, dependency caches or compiler build trees are
added to this bundle. Paths inside receipts identify the actual local/container
commands, not portable download locations.

| Evidence | Meaning |
| --- | --- |
| `accepted-simd-ci.json` | Read-only GitHub metadata: accepted commit, seven successful jobs, run URL. |
| `local-fresh-build-install.txt` | New local source-install helper, fresh Release compiler build with verified pre-existing SDK/HTTP caches; Makefiles. |
| `local-warm-install.txt`, `local-source-install.json` | Final helper rerun into a prefix with spaces/Unicode/#/%, including installed smoke checks. |
| `path-discovery.txt`, `local-installed-loader.json` | Actual installed PATH selection; Linux IDN dependencies load from the installed prefix. |
| `ubuntu-fresh-install.txt`, `ubuntu-fresh-source-install.json` | Fresh Ubuntu userspace, downloaded SDK and HTTP inputs, source builds and full installed smoke. |
| `ubuntu-sdk-identity.json`, `ubuntu-http-dependencies.json` | Official SDK identity and existing HTTP recipe's commands/source pins/installed hashes. |
| `ubuntu-fresh-input-hashes.json`, `ubuntu-final-host.json` | Exact initial key inputs; final source hashes and actual Python/CMake/Ninja/Make/Perl/glibc versions. Final hashes were compared with the checkout. |
| `ubuntu-final-install.txt`, `ubuntu-final-source-install.json` | Final synchronized helper/verifier passed on the same fresh container's verified caches. |
| `ubuntu-cli.txt`, `ubuntu-install-regression.txt` | 74 installed CLI process checks; relocated install, all 21 stdlib sources, removed-module/Runtime controls and output preservation. |
| `ubuntu-container.txt`, `ubuntu-prerequisites-summary.json` | Container/image identity, host network mode, no directory mounts; apt log location. |
| `initial-{release,debug,strict}-ctest.txt` | Original focused runs, including the old fake-driver version expectation failure. |
| `{release,debug,strict}-workflow-rerun.txt` | Corrected existing native-workflow test passed in all three configurations. |
| `{release,debug,strict}-cli.txt` | Direct 74-process CLI contract tests in all three configurations. |
| `{release,debug,strict}-install-final.txt` | Final CTest install checks in each configuration, including installed Linux loader closure. |
| `learning-examples.txt` | Nine displayed code blocks match canonical programs; synchronous JIT and fresh async AOT execution. CTest also exercised it in each configuration. |
| `installer-errors.json`, `sdk-local.txt` | Real preflight errors without setup/build mutation; SDK helper works outside GitHub environment files. |
| `install-layout-tests.txt`, `install-dependency-tests.txt` | Existing 15 layout tests and eight dependency-plan tests. Platform simulations are not native qualification. |
| `license-validation.json` | Existing five licensing validation groups passed. Windows checks are file-rule simulation. |
| `website-live.json` | Official website only: 52 successful HTTP reads, per-page hashes and original option/version counts. Bytes differ from checkout; deployment revision is not inferred. |
| `website-urllib-failures.json` | All 52 original urllib 403 responses, preserved alongside the successful curl audit. |
| `website-check.txt`, `website-tests.txt`, `website-dist-check.txt`, `website-examples.txt` | Source/built website checks, ten tests, 310 result checks and 12 fresh async AOT builds. |
| `original-failures.json`, `initial-license-harness-usage.txt` | Original access/environment/harness failures and resolutions, including failures not represented by a complete raw log. |
| `changed-paths.txt` | Exact modified/new review paths, including this curated bundle. |
| `verification.json` | Final source preservation, input-hash, link and evidence checks. |

The retained `initial-*` CTest output intentionally shows failure. Its corresponding
rerun is separate; no failed receipt was rewritten to PASS. Logs identify the
actual platform and path; they do not establish independent-machine portability.
The report's focused totals combine unchanged original passes with the repaired
single-test reruns, rather than claiming a second full-suite run.

Full build trees, `/tmp/void-dx-*.log`, `/tmp/void-dx-license-evidence/`, installed
prefixes and the stopped qualification container remain local for review.
