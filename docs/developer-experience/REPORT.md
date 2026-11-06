# GOLDEN WRENCH — developer experience implementation and review

6 November 2026. **Implementation complete for review; no publication authorized.**

Baseline: accepted AOT-only SIMD commit `175c08c8a0cab0b701ba71365ffcc398a0c9bcf3`,
then `.gitignore`-only HEAD `d517459d`. The checkout was clean at the start.
Read-only GitHub inspection independently confirmed [run 42231556437](https://github.com/Shmellyorc/Void.Language/actions/runs/42231556437)
concluded success at `175c08c8`, with all seven expected Linux, Windows and macOS
ARM64 jobs green; [retained metadata](evidence/accepted-simd-ci.json).
This milestone has no new hosted CI result.

## Implemented and tested

### Source build and installation

`python3 tools/install.py --prefix <directory>` coordinates existing workflows;
it does not introduce another dependency manager. It reuses the pinned official
LLVM 23.1.4 archive installer/verifier, native HTTP setup and digest verifier, and
CMake's existing `compiler`/`tooling` install components. `--configuration Debug`,
`--build-dir` and `--jobs` are optional. Ninja is selected when available;
Linux/macOS can use Unix Makefiles, and Windows requires Ninja. Python 3.12+ is
now enforced by ordinary dependency configuration as well as tests.

The helper builds only `voidc`, `voidls` and their required resources, installs
both components, and calls `tools/verify_install.py` from an external temporary
project. It checks installed stdlib use, O2 and Debug JIT, native linking/execution,
installed resource hashes and an LSP initialize/shutdown handshake with tooling
protocol 1. It writes exact child commands, exit codes, timings and logs beside
the build in `source-install.json`. Failed steps stop subsequent work. Existing
HTTP caches must pass the existing verifier; mismatches do not silently fall back
to ambient libraries. No OS package installation or shell profile edits occur.

Actual layout includes both tools, native manifests/inventory, Runtime/native
support/HTTP libraries, 21 matching stdlib sources, notices/corresponding source
inputs, and Vim/Neovim adapters. No VSIX installation is invented; existing editor
packaging remains documented separately. The SDK helper now exposes its existing
preparation as a reusable function and can report local setup without requiring
GitHub environment files. CI's environment-file path remains supported.

The source guides cover Linux prerequisites, Visual Studio x64/Windows/DIA SDKs,
Xcode 26+ and Apple libc++/SDK requirements, PATH discovery, verification, logs and
recovery. Native manifests intentionally retain the original SDK paths. Keeping
that SDK and host development resources is part of this source-install contract.
Installation is CMake's existing nontransactional copy operation: a failed install
may leave a partial prefix. Reinstall only after resolving its first failure.

### Public CLI

The [reviewable current contract](../CLI.md) retains check/run/build/emit and adds
`--help`, `-h`, `help [command]`, command-specific help/examples, `--version` and the
conventional `--` operand delimiter. Version output identifies the CMake project
version as **development**, LLVM, the SDK default target and compiler build identity;
it is not a stable release/versioning promise. Errors use the same usage function
as help and point to command-specific examples. Invalid invocations remain 2,
compiler/toolchain failures 1, and successful help/version 0. Application run
statuses continue to propagate; legacy numeric entries still print results and
exit zero. `--app` remains rejected, even alongside help.

No frontend, Runtime, stdlib, backend, ORC, target selection, cache policy, native
manifest format, output publication or source semantics changed. Existing
application discovery/legacy precedence and native linking remain in force.
Only help/error parsing and diagnostic text changed in production driver code.
Rebuilding that driver naturally changes the existing whole-compiler fingerprint;
normal cache invalidation remains active.
Native resource errors now explain reinstall/layout requirements; linker failures
identify the driver and point to host prerequisites. Existing output preservation
and resource/version validation remain tested.

New CTest entries are `cli_contract`, `cli_install` and `onboarding_examples`.
Actual process tests cover valid help/version/commands, option errors/conflicts,
signed i32 boundaries, default/directory discovery, application versus numeric
status/output, source errors, spaces/Unicode/#/% and dash-leading paths, native
objects/executables, collisions, absent native resources and old-output preservation.
Native-platform branches are present; running them on Linux does not count as
Windows/macOS execution.

### README and canonical learning material

Boss explicitly authorized the protected root README rewrite. It now introduces
the language, implemented features and limits, LLVM 23.1.4/Python 3.12+, source
installation, real console Hello World, check/run/native build, platform guides,
editor/docs/Wiki/official-site links and accurate MIT/dependency licensing scope.
Detailed engineering stays in `docs/`.

Canonical onboarding lives in `docs/install/`, `FIRST_PROGRAM.md`, `CLI.md` and
`TROUBLESHOOTING.md`. Existing build/editor/application documentation links there;
the obsolete Clang version in the native workflow was corrected. Seven small
canonical programs live under `examples/hello` and `examples/learn`. Nine displayed
README/first-program/Wiki VOID blocks are matched to those files. Synchronous
learning cases execute in both JIT modes; async is freshly emitted, linked and
executed in both modes using the existing explicit test adapter. Ordinary CLI
async hosting is not claimed.

SIMD documentation now records accepted run 42231556437 and preserves the old
pre-CI instructions/local measurements as historical evidence. No optimization
work or new SIMD experiment was performed.

## Qualification: local versus native platform

| Environment / check | Actual result and scope |
| --- | --- |
| PikaOS 4 x86-64 developer host | Fresh Release compiler/tooling build through the new helper, using verified existing SDK/HTTP caches; complete install and external smoke passed. Unix Makefiles was exercised because Ninja is absent. |
| Warm source installation on that host | Passed again into `/tmp/VOID DX installed é # %`; PATH resolved both installed tools. Final verifier passed, including prefix-local native hashes and LSP protocol. |
| Fresh Ubuntu 24.04 x64 container | Complete source onboarding passed: freshly downloaded/hashed official LLVM SDK, all seven freshly downloaded/verified HTTP source archives built, Ninja compiler/tooling build, CMake install, external check/O2+Debug run/native build+run/LSP smoke. See the fresh receipt below. |
| Final Ubuntu helper and install regressions | The final helper/verifier/test sources were synchronized after the fresh run; warm installation, 74 installed CLI process checks and relocated-install/missing-module/Runtime controls passed. The final input hashes and receipts distinguish those two runs. |
| Linux Release focused CTest | 12 selected tests exercised; the single initial workflow expectation failure was repaired and its rerun passed. Other 11 passed originally. |
| Linux Debug focused CTest | 11 selected tests exercised; the same initial expectation failure was repaired and rerun passed. Other 10 passed originally. |
| Linux strict RelWithDebInfo | Same 11 selected tests, with ASan/UBSan/leak detection and explicit sanitizer failure exit codes. The same expectation repair passed; other 10 passed originally. No sanitizer failure was accepted as an expected CLI failure. |
| Final relocated-install checks | Repeated in all three configurations after strengthening prefix-local resource/LSP and Linux loader checks; all passed. 21 installed stdlib files match source bytes. Removing installed console or Runtime proves installed lookup rather than a build-tree fallback. |
| SDK / existing installation checks | Local SDK helper succeeded without GitHub environment files. Existing 15 layout tests and 8 native dependency-plan tests passed. Those Windows/macOS representations are command/file-rule tests, not native execution. |
| Licensing | All five existing validation groups passed: source inventory, real Linux compiler/tooling installs and simulated Windows notice rules. 47 inventory entries checked; no new legal clearance asserted. |
| Website source/build | 52 pages validate, ten checker tests pass, built output validates; all 310 example result checks and 12 fresh async AOT builds pass. |
| New native Windows/macOS wrapper execution | **Not performed.** Instructions explicitly label the new wrapper paths prepared/unqualified. Prior Windows developer-host install and accepted seven-job compiler CI are separate dated evidence. |

Focused CTest selection was `cli_contract`, `cli_install`, `onboarding_examples`,
`source_cli`, `application_workflow_stage9`, `native_output_paths_ci88`,
`debug_cache_processes`, `automatic_cache_processes`, `lsp_protocol`, `numeric`,
`frontend`, plus Release `http_ci_policy`. The full compiler/Runtime suite and
real editor matrix were not rerun or relabeled as passing this milestone.

Strict runs used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

### Fresh Linux boundary and receipts

The new container is `void-dx-onboarding-20261106-host`, from the locally cached
Ubuntu 24.04 image ID `d9b8c34d61a9b9a3c44cab883d7d2f2619d674966ab520186f799cf32e3e0377`.
It has fresh Ubuntu userspace, no host directory mounts and no copied SDK,
dependency prefix or compiler binary. A source-only working-tree archive was
copied into `/src/void`. The SDK and dependency archives were fetched inside the
container. It shares the host kernel, CPU and networking; this is not an independent
machine, VM, older-kernel, lower-ABI or different-CPU qualification.

[Fresh install commands/status/timings](evidence/ubuntu-fresh-source-install.json),
[LLVM identity](evidence/ubuntu-sdk-identity.json),
[HTTP build commands and hashes](evidence/ubuntu-http-dependencies.json),
[fresh input hashes](evidence/ubuntu-fresh-input-hashes.json),
[final inputs/host](evidence/ubuntu-final-host.json) and
[final install receipt](evidence/ubuntu-final-source-install.json) are retained.
Text logs and the [evidence index](evidence/README.md) record both runs, focused
failures/reruns, loader resolution, PATH, website checks and license validation.
The first snapshot predates final verifier safeguards and test registration;
production `src/main.cpp`/`src/native_build.cpp` hashes match final sources. Final
helper/verifier behavior was then exercised with the synchronized sources.

## Proposed-only work: Wiki and official website

### Wiki approval bundle

`docs/wiki/` contains 13 task-oriented pages (Home plus twelve topics), `_Sidebar`,
`_Footer` and a repository-only maintenance README: **16 files**. Topics cover
installation, first program, basics, types, functions, collections, managed memory,
async, native compilation, CLI, editor setup and troubleshooting. Walkthroughs
include annotated results and small native-flow/managed-identity diagrams. They
link to canonical repository contracts rather than duplicate a specification.

Every topic is explicitly a draft. No Wiki repository was modified or published;
existing remote Wiki content/migration has not been assumed. Boss approval is
needed for any publication or replacement. Maintenance notes identify the exact
publication set, draft-banner removal, conversion of local `.md` topic links to
Wiki routes, and checking new `blob/main` links after an approved repository
change exists. These links do not claim that uncommitted files are already on main.

### voidlang.com audit and local correction proposal

Source is present in this checkout under `website/`; no external source access
was needed to prepare edits. Read-only curl fetched all **52** official pages with
HTTP 200. The original live pages contained **zero** `--app` occurrences and zero
LLVM 19.1.7 occurrences. Thus no live flag removal is claimed. The workflow page
already used LLVM 23.1.4 and automatic entry selection. Live bytes did not match
checkout files exactly, so the checkout is not asserted to be a verified deployment
revision. [Per-URL audit](evidence/website-live.json).

Exact local proposal:

| Source path under `website/public/` | Correction |
| --- | --- |
| `getting-started/index.html` | Add source-installation/platform anchors, 23.1.4/Python 3.12+ prerequisites, Linux helper/PATH/verification steps, native Windows/macOS qualification limits and links to the compiler/editor workflows. Retain the no-public-binary statement and repository-availability gate. |
| `docs/compiler-workflow/index.html` | Link installation, discoverable help/version, explicit automatic selection and rejected `--app`, native default paths/resources, cache options and status/output distinctions. |
| `docs/modules/index.html` | Include build in directory discovery; link the source-installation guide. |
| `roadmap/index.html` | Replace the superseded Windows HTTP failure/old release-hold description with the accepted HTTP/integrated functional scope; distinguish source installation from portable distribution. |

The four corresponding tracked `website/dist/` HTML files were regenerated by
the existing build script. Checker/tests now retain source prerequisites,
verification/platform anchors and the public binary boundary. Existing samples
were preserved and executed through the existing verifier. The only newly added
`--app` text explains rejection. No domain, navigation framework or sample system
was replaced.

These are **local proposed website edits**, not live publication. No website was
deployed or represented as updated. Publication needs Boss approval and the actual
site deployment authority. The repository-availability gate remains untouched;
when Boss separately approves public repository access, source-guide links can
point to the approved repository's `docs/install/{linux,windows,macos}.md` routes.
No new download/binary/package-manager URL was invented.

## Original failures and what changed

[Failure inventory](evidence/original-failures.json) and initial CTest logs preserve
what failed instead of presenting only retries:

- Baseline source audit found stale root README LLVM/Python/entry/license guidance,
  no public CLI help/version, manual disconnected setup steps and pending SIMD CI
  text. These are source-audit findings, not a claimed baseline binary rerun.
- The initial helper prototype rejected the developer host's missing Ninja. It now
  selects supported Unix Makefiles when Ninja is absent on Linux/macOS.
- First rootless container startup exited 125 because `pasta` is not installed.
  That created container was preserved; a separate host-network container succeeded.
- The web opener could not access the site; urllib returned 403. Curl retrieved all
  52 pages successfully. Failed access evidence is retained separately.
- The first focused CTest run failed the old fake-driver expectation in each
  configuration because `voidc --version` now succeeds. The test now distinguishes
  a non-Clang version identity from a failed probe, retaining both negative controls.
  Native version validation was not weakened. All reruns passed.
- Two harness mistakes were corrected: missing license-check arguments, and a
  preflight test that expected a later guard before an earlier valid overlap guard.
  Neither required changing compiler/install admission policy.

No unresolved implementation/test failure is hidden by the retry record.

## Remaining distribution/licensing questions and approval boundary

This is an experimental source-build/install entry point and a reviewed command
contract, not a stable installer product or portable binary package. Native
Windows/macOS wrapper execution, independent clean-machine packages, ABI/OS floors,
SDK relocation/redistribution and downstream native dependency closure remain
separate qualification work. The new tests are ready for future Boss-authorized
native-platform execution; no new hosted run was dispatched.

VOID-original compiler, Runtime, stdlib and tooling use the existing MIT grant.
Dependency notices, corresponding-source/replacement obligations, MPL/GNU terms,
platform runtime conditions, borrowed/generated material provenance and the stated
limits around website/artwork/historical documentation remain governed by the
existing [scope](../release/LICENSING_SCOPE.md) and
[redistribution audit](../release/REDISTRIBUTION_AUDIT.md). No release or legal
clearance is inferred from successful installation or license-file checks.

[Exact changed paths](evidence/changed-paths.txt) separate source, canonical docs,
Wiki drafts, proposed website source/generated output and curated text evidence.
Runtime/frontend/backend/stdlib/headers/workflows and `.gitignore` are unchanged.
No unrelated work was removed. Git HEAD/branch/index were preserved: no stage,
commit, branch, push, CI dispatch, Wiki publication or website deployment occurred.
Boss review is the next external-action boundary; implementation and local
qualification are complete.
