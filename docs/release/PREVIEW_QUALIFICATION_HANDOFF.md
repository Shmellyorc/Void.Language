# VOID 2.0 private preview qualification handoff

## Bounded framing implementation — 4 November 2026

**LOG-PERF OPEN / release HOLD. Golden Wrench approved the bounded candidate for commit and push.**
The authorized text.translate_scalars candidate and _quote integration are
implemented in the working tree. The current LLVM 23 check passed 48 observations
on the historical source path. Original LLVM 19 cause/classification evidence
is preserved. Golden Wrench explicitly approved existing frozen-backing
snapshots and atomic string publication.

Final Release/LLVM 23 JIT O2 medians are heavy framing **8.603 ms** (PASS ≤10),
full heavy console **18.501 ms** (PASS ≤20), and file200 **21.390 ms** (FAIL <10).
The current prepared-file control is 12.195 ms. Text JIT/AOT, 31,224 ownership
checks, 15 logging byte/UTC checks, 1,688 logging JIT checks and 827 checks per
AOT mode pass. The initial candidate and failed build diagnostics are retained.
The report includes the source diff identity, measurements, safety boundary and
remaining limitations. [Candidate report](../log/BOUNDED_FRAMING_RESULT.md) · [Evidence](../log/evidence/llvm23-framing-20261104/results.json)

The smallest next measurement is a bounded small-file/direct-I/O comparison
separating framing, writes and flush/close costs after candidate review.
Installation remains closed at d784c049. The reviewed logging candidate is authorized for commit and push with [skip ci].
Further small-file investigation remains separately scoped for Golden Wrench review.


## Retained logging baseline investigation — 4 November 2026 (review only)

**LOG-PERF remains OPEN / release HOLD. Stop for Golden Wrench review before
production implementation changes.** The installation closure alone was committed
and pushed with `[skip ci]` as `d784c0490d13b8abdca754dfecf99578ab498733`;
`origin/main` was independently checked at that SHA. It contains exactly the
installation handoff, blocker register and acceptance receipt. No CI was dispatched.
All logging investigation changes below remain uncommitted for separate review.

The [concise report](../log/PERFORMANCE_BASELINE_LLVM23.md) and
[machine-readable results](../log/evidence/llvm23-baseline-20261104/results.json)
record criteria, historical profiling, exact build/source identities, commands,
wall/CPU samples, throughput, allocation/resource observations and limitations.
Production code measured was `ffd8920befe04a67e3eaef6aadcdf0233df13563`, with
fresh Release libraries and LLVM/Clang/LLD 23.1.4; the later closure commit changes
documentation only. No production compiler/Runtime/CMake/logger/README byte changed.

The bounded original benchmark supplied 216 normal/O2 JIT and independent AOT
observations. Selected component timing/allocation runs supplied 192 observations,
including warmups; all benchmark processes exited 0 with their own byte/count
oracles satisfied. This is performance evidence, not a new correctness acceptance.
GC budget was 256, timed children had actual hard/soft 1 MiB stacks, and builds
finished before measurements. The developer desktop remained active.

JIT O2 file200 is **24.042 ms**, versus **11.662 ms** prepared direct I/O:
**2.06×** meets the relative aim but the **<10 ms absolute screen FAILS**.
The direct-I/O floor itself exceeds that screen in JIT on this host; no logger-only
causal regression is inferred. Heavy 1-KiB/100 framing is **910.628 ms** against
10 ms; full console is **947.525 ms** against 20 ms: both FAIL. Prepared output is
7.946 ms. Framing records **2,509,955 ordinary C++ allocations**, **77,100 source
managed constructions** and 1,162 collections for those 100 records.

All recorded post-GC heap/handle snapshots return to their pre-sample baseline;
disabled sink/lazy and ordinary-file metadata observations meet their bounded
criteria. Complete scaling/nonregression, current heavy AOT components, native
Windows/macOS logging performance, tail latency and continuous peak/live native
memory remain UNKNOWN. Installation 3/3 PASS does not qualify logging performance.

The current evidence locates the dominant heavy workload in quote/framing and
reproduces the large allocation/construction counts in historical profiles. It
does not newly split wall time among metadata-key allocation, locks/root checks,
polling, copying and GC, or establish why the file-I/O floor changed. The smallest
justified next action is Golden Wrench review of a separately scoped framing
repair plan against the existing scalar-substitution safety contract and unchanged
screens. The historical implementation stop condition needs explicit review;
passing current benchmarks does not prove a cause or repair of earlier crashes.

Exact raw receipts, generated measurement-only copies and preserved binary/source
hashes are private under
`/home/shmellyorc/.local/state/void-log-performance/ffd8920b-20261104-9v8echee/`.
No historical crash campaign, full CTest, sanitizer, HTTP/HTTPS, installation or VM
testing was run. There is no speculative optimization or production repair.

## Installation qualification closed — 4 November 2026

**ACCEPTED — 3/3 PASS. Hosted installation qualification is complete.**
Boss accepted [run 41916674041](https://github.com/Shmellyorc/Void.Language/actions/runs/41916674041),
attempt 1, at exact commit `ffd8920befe04a67e3eaef6aadcdf0233df13563`.
Read-only GitHub metadata confirms that every platform completed dependency
provisioning, Release builds, compiler/tooling component installation, installed
executable smoke checks and evidence upload. The
[compact closure receipt](evidence/install-ci-41916674041.json) records the job
IDs, artifact IDs/digests, tested SHA and verification boundary.

| Tested hosted environment | Job | Result |
| --- | --- | --- |
| Ubuntu 24.04 x64 | `126878099207` | PASS |
| Windows Server 2022 x64 | `126878099689` | PASS |
| macOS ARM64 (`macos-15`) | `126878099627` | PASS |

This closes the hosted install hold, including the earlier macOS dependency
failure. The installation-only AWS-LC option is covered at this tested SHA;
ordinary HTTP fixture callers retain their separate tool-enabled contract.
Artifacts were uploaded with the recorded 14-day retention; closure uses Boss's
verified acceptance plus job/artifact metadata, without rerunning or rehashing
the installations. Earlier failed-run and source-review records below remain
historical snapshots; their UNKNOWN/HOLD statements are superseded only for
this bounded hosted qualification.

Acceptance covers these hosted source-build/install/use environments. It grants
no public binary distribution, license clearance, independent physical-machine
portability, broader ABI, editor-UI or relocated-application acceptance. No
installation, HTTP/HTTPS or VM testing is reopened. Logging performance and
public licensing remain separate release holds. This documentation closure is
prepared for a separately authorized commit/push; logging investigation work
must remain outside that closure commit.

## macOS installation dependency failure — 4 November 2026

**INSTALLATION-ONLY REMEDY VALIDATED — REVIEW BEFORE THE NEXT MANUAL RUN.**
Boss authorized a narrowly scoped commit/push with `[skip ci]`. Workflow dispatch
and reruns remain Boss-controlled and are not part of that authorization.
The source-review sections below are preserved pre-run snapshots; their former
uncommitted/not-dispatched statements do not describe this investigation.

[Installation run 41902140893](https://github.com/Shmellyorc/Void.Language/actions/runs/41902140893),
attempt 1, tested `ca923ffaac418c2c0b8b6a065868b57b5bf2fbf4`. This correction
started at that same SHA on a clean working tree.

| Hosted platform | Recorded run result | Status of the proposed correction |
| --- | --- | --- |
| Linux x64 | PASS; job `126827103064` succeeded | Linux recipe unchanged; no new run |
| Windows x64 | PASS; job `126827103472` succeeded | Windows branch unchanged; no new run |
| macOS ARM64 | FAIL; job `126827103377`, AWS-LC dependency build | Installation UNKNOWN; corrected recipe unexecuted |

Golden Wrench supplied the platform conclusions. This investigation independently
checked the job/step metadata and downloaded macOS artifact
`install-macos-15-ARM64-Release-1`, ID `13358678047`. Its ZIP SHA-256 is
`4e9a007468f349cd3187e1ad2fa5cb23a19902d7829320bc4db1ef73b50ed0ce`;
all 80 entries in its evidence seal verified. Linux/Windows artifact contents were
not rehashed here. Hosted results do not establish independent physical-machine
portability.

Confirmed immediate cause: the macOS arm64 linker cannot resolve
`std::__1::__hash_memory(void const*, unsigned long)` from `pkcs8.cc.o` while
linking AWS-LC's auxiliary `tool-openssl/openssl`. The dependency receipt records
`cmake --build <macOS Release dependency build>/aws-lc --parallel 3` exiting **2**.
The setup wrapper propagates failure as exit **1**. Python is reporting the
failed native command, not causing the unresolved symbol. Build stdout records
`crypto`, `ssl` and `bssl` targets completed before that failure; AWS-LC's install
command was not reached. VOID configure/build/install/smoke were all SKIP, so no
macOS installation pass is claimed. A deeper libc++ ABI diagnosis or repair is
not established by this bounded inspection.

The cached AWS-LC 5.12.0 archive rehashes to the existing approved pin
`1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68`,
revision `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8`. Its top-level CMake source
also byte-matches the official immutable revision. That
[pinned source](https://github.com/aws/aws-lc/blob/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8/CMakeLists.txt#L1510)
declares `BUILD_TOOL` and gates only the `tool` and `tool-openssl` directories with
it. Crypto remains unconditional; SSL remains under `BUILD_LIBSSL`. Headers and
the library install/export rules remain outside that gate; C++ stays enabled for
SSL. The existing static-library and SSL options therefore preserve VOID's
required `libcrypto.a`, `libssl.a`, `include/openssl` and library CMake exports.
In installation-only mode, the omitted tool-directory installs are `bssl`,
`openssl` and `c_rehash`; default provisioning keeps them. This is source
verification of the required production contract, not a newly executed macOS
install.

Golden Wrench held the initial unconditional `-DBUILD_TOOL=OFF` proposal because
the shared recipe also serves HTTP regression fixtures. The corrected
[native recipe](../../tools/setup_http_deps_native.py) adds an explicit
`--macos-install-only` option, default **false**. The Darwin AWS-LC configure
command now explicitly selects **`BUILD_TOOL=ON` by default**, selecting OFF only
when that option is supplied. It is rejected outside macOS before provisioning.
[The installation harness](../../tools/ci/install_smoke.py) supplies the option
only for macOS dependency setup; Linux and Windows installation commands retain
their original arguments. The existing setup forwarder passes macOS arguments
through unchanged. No compiler, Runtime, production CMake, optimization flags,
upstream source, LLVM version, platform SDK, dependency pin or workflow changes.
Recipe identities already include this script's hash; use a fresh matching
dependency prefix for the reviewed run.

**Compatibility boundary:** existing HTTP fixture callers omit the new option,
so they keep the default tool build and installation of `<prefix>/bin/openssl`.
[The regression fixture preflight](../../tools/ci/http_bootstrap.py) and separate
fixture provisioning are unchanged. Both command modes retain static
`libssl.a`, `libcrypto.a`, headers and library install/export rules. The opt-out
is used only by installation CI, which provisions fresh dependencies without
compiled caches and never runs HTTP fixtures. An installation-only prefix is not
a substitute for a tool-enabled HTTP fixture prefix. This correction does not
repair or claim acceptance of the auxiliary executable's libc++ linkage in the
default mode; it preserves existing behavior. No HTTP campaign was run or marked
passing here.

The new [focused command tests](../../tests/ci/install_dependencies_test.py) PASS:
**8 tests**, including default ON versus explicit OFF across Release/Debug/strict,
unchanged SSL/static-library options and install commands, argument forwarding,
macOS-only harness opt-in, non-macOS rejection, unchanged Windows provider
selection and the required archive/header consumer paths. Reproduce with
`python3 tests/ci/install_dependencies_test.py`. Downloads, archive extraction and
native subprocesses are mocked; these are command-generation checks, not native
installation acceptance. Pinned upstream source inspection confirms the crypto/
SSL/header install rules survive both modes and default tool installation retains
`openssl`. Python AST/bytecode and Python 3.12 grammar checks PASS for both changed
scripts and the new tests; Actionlint 1.7.12 and whitespace checks PASS.

Original artifact/source evidence remains under
`/tmp/void-macos-build-tool-_y743x9w/`. Option command plans, validation results
and all local test attempts are retained under
`/tmp/void-macos-install-option-k3149okd/`. Two initial test-fixture attempts lacked
host-query isolation and copied recipe-hash inputs; only the fixture was corrected
before all tests passed. The relevant change set is exactly the native recipe,
installation harness, focused command tests and this handoff. Nothing was
configured, built or installed locally; no CTest, sanitizer, benchmark, HTTP/HTTPS
campaign or historical crash reproduction was executed. macOS installation after
this remedy remains **UNKNOWN** until a reviewed manual run.

## Installation CI source review — 4 November 2026

**HARNESS CORRECTED — COMMIT HOLD FOR GOLDEN WRENCH SOURCE REVIEW.**
Boss controls the first manual run. No commit, push or dispatch was performed.
This section supersedes the earlier sprint's scope/stop wording only for the
workflow, its supporting harness and this handoff update. Historical results and
their tested SHAs remain unchanged. This work began on clean source
`d99033d297357e3239952ef47d0fd5d0142ee69e`.

The new [installation workflow](../../.github/workflows/ci-install-qualification.yml)
has only `workflow_dispatch`, three Release matrix jobs, independent outcomes and
`fail-fast: false`. Its [single supporting harness](../../tools/ci/install_smoke.py)
records commands and performs bounded installed-use checks. Existing workflows,
compiler/Runtime/CMake, dependency recipes and protected README are unchanged.

### Golden Wrench native resource comparison correction

Golden Wrench found that the layout comparison counted only `.a`/`.lib` paths,
omitting Linux's hashed `libidn2.so` and `libunistring.so`. A realistic fixture
also exposed the same suffix assumption treating the `-Xlinker` exclusion list
ending in `.a` as a file. Both valid Linux fixtures failed before correction;
the existing Windows `.lib` and macOS `.a` fixtures passed.

The harness now compares resolved file arguments without filtering by extension.
It consumes operands of the options emitted by `NativeBuild.cmake` (`-Xlinker`,
`-framework`, `-isystem`, `-isysroot`, `-arch`), keeping flags, system libraries,
framework names and SDK/rpath/exclusion-list operands out of the resource set.
The existing hash, private-prefix containment, native manifest version/driver/
linker and Windows static CRT checks remain. Missing or extra file resources
still fail exact inventory equality; a dangling paired option also fails.

The new [focused layout tests](../../tests/ci/install_smoke_test.py) call the real
`Qualification.layout()` using temporary files and realistic platform manifests,
without building or executing VOID. All **15 tests PASS**, including Linux `.so`
aliases and versioned targets, Windows `.lib`, macOS archives/frameworks, missing/
unhashed/corrupt resources, prefix/symlink escapes and preserved manifest checks.
Reproduce with `python3 tests/ci/install_smoke_test.py`. Python syntax checks for
both harness and tests and Actionlint 1.7.12 also pass. The workflow itself is
unchanged by this correction. Earlier fixture checks below did not represent the
Linux shared resources or exclusion-list operands; this focused suite supersedes
that layout coverage. Hosted installation results remain UNKNOWN.

| Proposed hosted job | ISA gate | Current installation result |
| --- | --- | --- |
| `ubuntu-24.04` / Release | Actual Linux x64 and runner X64 | UNKNOWN — not dispatched |
| `windows-2022` / Release | Actual Windows x64 and native x64 MSVC environment | UNKNOWN — not dispatched |
| `macos-15` / Release | Actual Darwin ARM64 and runner ARM64 | UNKNOWN — not dispatched |

`macos-15` is an explicitly selected Apple Silicon image in
[GitHub's runner table](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
with an actual-architecture assertion before provisioning. An invented
`macos-15-arm64` label is not used. Its
[current image inventory](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md)
lists Xcode 16.4 as default and Xcode 26.3 as available. The harness explicitly
selects `/Applications/Xcode_26.3.app/Contents/Developer` for VOID's required
vendor libc++ headers, then records Xcode and physical SDK versions. If that
version disappears or lacks required headers, provisioning/configuration fails;
there is no alternate compiler or CMake repair path.

### What a future PASS will establish

- A fresh source build on each named hosted runner, at the recorded dispatch SHA.
  No compiled VOID, SDK or HTTP cache is restored. The existing
  [SDK helper](../../tools/ci/llvm_sdk.py) downloads the complete official
  LLVM/Clang/LLD **23.1.4** archive and hashes it before extraction. All three
  existing pins matched the official release asset digest metadata during local
  review of [LLVM's release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4):
  Linux `25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`,
  Windows `854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2`,
  macOS `5d3179db39adeca5b17a34d82708fe976bc60e055b05fe557878fb3fb64d39ef`.
  Clang/Clang++ and platform LLD versions are checked and retained. Linux selects
  pinned LLD; macOS retains the approved Apple system linker/vendor libc++ setup,
  and Windows retains the approved SDK driver/native MSVC setup.
- The existing [HTTP setup](../../tools/setup_http_deps.py) and its
  [Windows/macOS recipe](../../tools/setup_http_deps_native.py) build the complete
  Release closure afresh, followed by the existing dependency verifier. Providers
  remain Linux AWS-LC/libidn2/libunistring, Windows Schannel/WinIDN/static MT and
  macOS AWS-LC/SecTrust/Apple IDN. HTTP fixtures and protocol campaigns are omitted.
  Windows also uses the SDK helper's real zlib/static zstd/LibXml2 bootstrap.
- `BUILD_TESTING=OFF`, sanitizers/research OFF, and only the `voidc`/`voidls`
  production targets and their required resources. Compiler and tooling component
  installs go to a new private prefix in runner temp, outside the fresh build
  directory and checkout. Provisioning refuses pre-existing `.toolchains` inputs
  and installation refuses an existing prefix.
- Installed PATH discovery; external `main.void` minimal check/run; installed
  `console` import check/run; native build and direct execution with exact LF
  output `VOID installed stdlib OK` and exit 0. Every VOID command names the
  installed executable. Standard-source bytes match the checkout, and the
  disposable checkout's stdlib is hidden during all smoke checks and restored in
  `finally`, so a source fallback cannot supply the successful console import.
- Installed native manifests have the expected version, pinned driver and
  installation-relative Runtime/provider paths. Each resource hash is checked,
  and link-manifest resources must equal the hashed inventory. Linux verifies
  ldd resolution and installed IDN/Unicode libraries plus ELF dynamic metadata;
  macOS verifies ARM64 Mach-O, system library imports and load commands; Windows
  verifies x64 PE imports resolve to available system DLLs/API-set contracts.
  Successful installed/native execution exercises each runner's loader.
- Installed `voidls --discover` selects the external project. A real stdio process
  receives ordered Content-Length framed initialize/initialized/shutdown/exit
  messages on one connection; both response frames, protocol 1, null shutdown,
  stderr and process exit are validated. This is a bounded pipelined handshake,
  with raw sent/received bytes retained, not editor interaction coverage.

### Receipts, failure behavior and acceptance limits

Each job always attempts to produce its own `summary.json` and Actions summary.
Required checks report PASS/FAIL; an interrupted check remains UNKNOWN, and a
check never reached is SKIP. Overall PASS requires every required check to pass
and prior Actions steps to succeed. Required failure fails the job; there is no
`continue-on-error`. Cancellation/incomplete evidence cannot produce PASS.

The `install-<runner>-<ISA>-Release-<attempt>` artifact retains compact JSON
identity/check/command receipts, exact argv/cwd and selected environment, real
exit codes (null if no exit was observed), raw stdout/stderr/stdin, exceptions,
SDK archive identity, dependency receipts/configure logs, install inventory and
native program hashes. A SHA-256 index seals the evidence files. Artifacts retain
14 days; binaries/SDK payloads are not uploaded. Abrupt runner loss or a forced
job timeout can prevent finalization/upload; missing evidence stays UNKNOWN and
must be investigated from Actions logs, never treated as acceptance.

A future hosted PASS will not establish independent physical-machine portability,
bare-host provisioning without preinstalled platform tools, SDK-free operation,
application relocation after moving its install prefix, a full runtime dependency
matrix, complete Windows transitive/dynamic DLL closure, actual editor UI,
installer packaging/uninstall, performance, licensing or public-release approval.
Absolute native SDK paths and Linux application RUNPATH limits remain. Full
CTest, sanitizer suites, logging/performance benchmarks, HTTP/HTTPS regression
campaigns and historical LLVM crash reproductions are excluded. No optimization
flags are changed to obtain a pass, and no speculative production repair is added.

### Local review validation and stop

YAML parsing and Actionlint 1.7.12 pass; Python AST/bytecode compilation passes.
Local harness checks use disposable fake projects and stub executables, never
existing VOID binaries: framing rejection, layout/resource corruption detection,
external-project command orchestration, exact output/exit failure preservation,
spawn/timeout recording, all three platform configure/provider command plans and
runtime-metadata acceptance/rejection assertions with mocked tool output,
source restoration after failure, PASS/FAIL/UNKNOWN reporting, SKIP classification
and the evidence hash seal pass. These validate the harness, not installation on
any of the three operating systems. The initial local validator reused its own
fixture prefix and was correctly rejected by the fresh-build guard; giving each
platform plan its own scratch directory corrected that validator only.

Actionlint initially rejected `runner.temp` in job-level `env`; the corrected
workflow derives the scratch path from `RUNNER_TEMP` inside Python and uses
`runner.temp` only in the artifact step. No hosted failure was reproduced or
claimed fixed. The macOS runner inventory review also supplied the explicit
Xcode selection above. No SDK download, dependency/VOID build, component install
or real VOID/LSP execution was performed locally in this sprint.

The proposal comprises four paths: the workflow, harness, focused layout tests
and this handoff. All remain unstaged and uncommitted. Golden Wrench
must review the source; Boss decides the reviewed ref and first manual dispatch.
On a future failure, preserve original receipts and classify it before proposing
any out-of-scope compiler/Runtime/CMake fix. Engineering stops here for review.

## First-install documentation sprint — 4 November 2026

**REPORT READY — STOP FOR GOLDEN WRENCH REVIEW.**

Documentation validation passed; no implementation or host test was performed.

This section records the preceding sprint. The original Linux qualification handoff below is a
preserved historical snapshot. Boss subsequently committed those 15 Linux records
at `d52e6b708539dd3026c934c98332c58c8e5e311d`; their former uncommitted status is
not the initial state of this sprint. This sprint began at that SHA on clean
`main`, tracking `origin/main`. See [start hashes](evidence/first-install-docs/start.json).
HEAD, root README, production/Runtime/CMake, licensing and historical acceptance/
crash evidence remain protected. Only documentation and review evidence are in scope.

### Accepted installation evidence and provenance

| Platform / exact tested input | Accepted scope | Still open |
| --- | --- | --- |
| Linux x64, `97aa4e4334f1b9354ba651770b5cf13e74da03d0` | Fresh Ubuntu 24.04 container source build, component install, installed stdlib/native execution and real LSP handshake. [Original report](LINUX_CLEAN_HOST_QUALIFICATION.md) and receipts unchanged. | Shared hardware/kernel; separate VM/machine, fresh-network SDK acquisition and broader ABI coverage unqualified. |
| Windows x64, `1afd881ecfa65c616261e7aeb76ecd2bf87770b6` | Golden Wrench accepted the bounded Windows 10 developer-host isolated install of existing Release artifacts. [Exact Windows-authored record](WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md). | No fresh compiler rebuild or independent clean-host provisioning; later refs including observed remote `97aa4e43` are not qualified by these hashes. |
| macOS ARM64 | Existing hosted compiler/HTTP acceptance only. | Source installation/clean-host qualification unexecuted. |

The complete Windows report was supplied by Boss in the connected chat
“Review before continuing,” then explicitly accepted by Golden Wrench there.
It was recovered from that exact Boss message and copied as supplied, retaining
host paths, hashes, command descriptions, shadow-install method and both original
harness failures. [Import provenance](evidence/first-install-docs/windows-import-source.json)
identifies the source/acceptance messages and the imported UTF-8 text hash. Only a
terminal LF is standardized; original Windows file bytes/line endings and private
raw VM receipts were not accessed. No missing commands, script or results were
reconstructed. No Windows test was repeated.

Windows host assumptions: Windows 10 Home 22H2 build 19045.6466 AMD64; full
LLVM/Clang/LLD 23.1.4; VS Build Tools 2022 17.14.37806.7; MSVC toolset 14.44.35207
(cl 19.44.35229); Windows SDK 10.0.19041.0; CMake 4.4.3, Ninja 1.13.2, Python
3.14.8. The installed VC redistributable was v14.44.35211.00. Restricted PATH and
removed explicit VS/VOID environment overrides did not remove these disk inputs.
The absolute SDK driver and Microsoft libraries remained available. The successful
EXE-only copy proves that VM's result, not a machine without those prerequisites.

| Windows artifact | Exact tested SHA-256 |
| --- | --- |
| Installed `voidc.exe` | `9e7a9d283dc9ba51bdb3bed00aeb680af687bdbfa86d1743ad63ee130d662993` |
| Installed `voidls.exe` | `2b9ee648cbc67976363c93a0413e48de9065ad943fd38c2d2795688286faf2ca` |
| Native `qualified.exe` | `68423ed09a68e2436fc6253a047ae5f07ce306734e5bc1348e2530f4ca3483df` |

Other source/Runtime/SDK identities remain in the exact imported report. Its 562
original Release-tree files were preserved by paths, sizes and modification times;
76 installed files and seven resource archives were hash checked. Those are the
Windows author's reported checks, not a new Linux-side raw-evidence verification.

### Exact modifications and verification

- Imported the Windows-authored report without changing its content.
- Updated the blocker register for accepted bounded Windows/Linux results, separate
  independent-host gates, and the first-install guide/README review state.
- Corrected `docs/SOURCE_COMPILER.md`: Linux prerequisites, pinned full SDK and
  standalone helper invocation, HTTP setup/verification, profile/source-cache/CA
  contract, production Release configuration, component installs, current
  `main.void` and native CLI workflow, installed stdlib discovery and portability
  limits. Windows developer-host requirements are distinct from fresh provisioning;
  macOS hosted compiler evidence is distinct from source-install acceptance.
- Prepared [README_LLVM23_CLI.patch](proposals/README_LLVM23_CLI.patch), **unapplied**.
  It corrects LLVM/Python requirements, points to the maintained complete recipe,
  qualifies the still-supported legacy entry example and uses current `build`.
  Feature/editor/licensing sections and the valid legacy source example are unchanged.
- Retained all historical Linux/HTTP/LLVM/crash records and existing receipts;
  no prior evidence digest/inventory is rewritten to describe this new sprint.

[Validation results](evidence/first-install-docs/validation.json) and the
[reproducible checker](evidence/first-install-docs/validate.py) record local links/
anchors, shell/heredoc syntax, Linux recipe comparison with retained accepted
argv, exact known smoke-source comparison, Windows identity/provenance, the dry-run
README patch check and scope/preservation. The
[sprint inventory](evidence/first-install-docs/inventory.json) lists every changed
path and digest. Validation is static and source/receipt based: no new compiler,
CMake configure/build/install, SDK download, Windows command or LSP exchange is
executed in this documentation sprint. Existing command results remain attached
to their original SHAs. Network link reachability is not newly verified.

Read-only retrieval note: the first chat-read request exceeded the tool's 20,000-
character per-item limit and was rejected before retrieval. The corrected request
returned the complete supplied Windows report. No source content was reconstructed.
The first static checker incorrectly treated Python code inside a fenced block as
an inline Markdown link. Its failure is retained in `validation.json`'s attempt
history, with the original full report/checker preserved privately under
`$HOME/.local/state/void-first-install-docs/d52e6b70-20261104/`. Excluding code from
link extraction corrected the validator; no documentation criterion was relaxed.

Final static results: all six validation groups PASS, 97 local links/anchors,
10 shell blocks accepted by `bash -n`, and one Python SDK heredoc AST-parsed.
Linux package/configure/build/HTTP/component-install/smoke commands match retained
accepted argv after documented path normalization. Both smoke-source texts match
accepted source hashes. The unapplied README diff passes `git apply --check`;
root README still hashes to
`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`.
The Windows import matches the exact supplied Markdown body, SHA-256
`a20fa700f27990d6748df3cb0b7b5f030bed94c3ebc0becac1c8ed66b08613f0`.
The complete changed-file inventory contains 10 paths: three modified documents
and seven new documentation/review-evidence files. All remain unstaged/uncommitted.

### Unresolved gaps and next clean-host procedure (plan only)

The source guide now includes the previously omitted HTTP/CA/install workflow.
The protected README remains unchanged pending approval of its separate proposal.
Standalone SDK download wiring is source-inspected; the accepted Linux test used
a locally cached official archive with a verified hash. Generated Linux apps
retain an absolute installed-library RUNPATH, and native manifests retain absolute
SDK paths. LLVM notice completeness, public licensing, logging performance and
current-toolchain optimizer risk remain separate register entries. No crash
campaign or logging work reopened. Windows' exact shadow-install harness/raw
receipts are private on the VM; no universal shadow generator is invented here.

After Golden Wrench reviews this documentation and Boss authorizes the next run:

1. Create a fresh Ubuntu 24.04 x64 VM with a new disk and record image digest,
   hypervisor, kernel, architecture, libc, resources and whether hardware is shared.
   A VM on the same physical machine is an intermediate test, not independent
   physical-host portability. Do not mount the development home, SDK, build tree
   or dependency prefixes, and do not reuse a prepared VM snapshot.
2. Obtain Boss's exact reviewed source revision and these guide changes in that
   VM. Record full SHA, clean status and source/archive identities. Install only
   declared prerequisites; retain exact package versions and every command's
   exit, stdout/stderr and relevant environment. Obtain the official SDK anew via
   the documented pinned helper and retain its identity/hash receipt. Build HTTP
   dependencies from their pinned sources in that VM, without compiled caches.
3. Follow the corrected Linux guide verbatim: verify the prefix/source hashes,
   configure plain Release, build `voidc`/`voidls` and Runtime dependencies, and
   install both components into a new private prefix. Preserve any failure before
   any change; classify it without speculative production/build-system repair.
4. In a fresh project outside the source, check/run minimal `main`, import/run
   installed console, build/execute native output, verify loader/manifest paths
   and installed stdlib byte identities, discover installed `voidls`, and perform
   real framed initialize/initialized/shutdown/exit. Test source-fallback isolation
   only on disposable VM copies. Repeat the bounded missing-prefix/std-root/native-
   manifest/driver controls; record expected failures separately from product faults.
5. Preserve compact non-sensitive receipts and all failed attempts. Stop immediately
   on a correctness/safety concern and stop for Golden Wrench review at completion.
   Do not use repeated LLVM19/current-optimizer crash campaigns, change optimization
   to make a check pass, or infer editor/public-release acceptance from these checks.

This sprint does not launch that VM, repeat Windows tests or authorize an installer,
CI run, Git mutation, package publication or deployment. Independent Windows
provisioning/source rebuild and macOS install qualification remain later explicit
scopes. Current optimizer risk uses LLVM23 evidence, with historical Linux LLVM19
and earlier Windows LLVM23 incidents distinct and causal repair unproven.

### Current changed-file inventory and stop

All current changes remain **unstaged and uncommitted**. Root README is unchanged;
its proposal exists only as the separate patch and a private validation copy.

- `docs/SOURCE_COMPILER.md`
- `docs/release/PREVIEW_QUALIFICATION_HANDOFF.md`
- `docs/release/PREVIEW_BLOCKERS.md`
- `docs/release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md`
- `docs/release/proposals/README_LLVM23_CLI.patch`
- `docs/release/evidence/first-install-docs/start.json`
- `docs/release/evidence/first-install-docs/windows-import-source.json`
- `docs/release/evidence/first-install-docs/validate.py`
- `docs/release/evidence/first-install-docs/validation.json`
- `docs/release/evidence/first-install-docs/inventory.json`

Validation is complete; stopped for Golden Wrench review. No staging, commits,
pushes, branch creation, hosted CI, publication, deployment, README application,
production/CMake/Runtime changes, historical crash-evidence edits or licensing
decision is authorized by this sprint.

## Original Linux qualification handoff — preserved prior snapshot

4 November 2026. **REPORT READY — STOP FOR GOLDEN WRENCH REVIEW.**

Linux x64 source installation passed the bounded fresh Ubuntu container checks.
This is private qualification, not public-preview/release acceptance, independent
machine/VM certification or permission to distribute compiler/dependency binaries.

## Identity, mission and authority

Initial and final source HEAD: `97aa4e4334f1b9354ba651770b5cf13e74da03d0`,
branch `main`, tracking `origin/main`. Initial working tree was clean. No applicable
ancestor/repository `AGENTS.md` was found. All twelve mandatory files were read
before checks; [initial hashes](evidence/qualification-start.json) and
[exact examined-file inventory](evidence/examined-files.json) preserve the inputs.
Additional large files were inspected in relevant sections as marked there.

Boss authorizes Linux source-build dependency inventory, available fresh-environment
verification, classification and durable reporting. Production, CMake and Runtime
repairs require separate Golden Wrench review and Boss authorization. Root README
is protected/read-only. No production/test/Runtime/CMake change was made in the
actual checkout. No staging, commit, push, branch creation, hosted CI dispatch,
package publication, deployment or public redistribution occurred.

HTTP/HTTPS remains [accepted](../net/http/ACCEPTANCE.md) at the recorded seven-job
LLVM23 hosted run; it was not reopened. Website modernization is outside this task.
No historical investigation or acceptance record was rewritten.

Boss further clarified that historical Linux LLVM19 DSE cause remains unproven;
LLVM23 may have eliminated an upstream defect, but this is not established.
Current optimizer risk must use LLVM23 evidence: its three passing bounded
DSE-associated checks, seven passing hosted jobs and separately tracked current-
toolchain incidents. Historical uncertainty is not itself a demonstrated current
LLVM23 defect. Exhausted LLVM19 reproduction campaigns remain closed. Earlier
Windows LLVM23 incidents remain separate. None was rerun or causally reclassified.

## Completed work and every outcome

The [qualification report](LINUX_CLEAN_HOST_QUALIFICATION.md) contains the complete
source dependency/install contract, exact successful recipe, observed runtime
closure, missing-prerequisite classifications and seven first-install guidance gaps.
[Command receipts](evidence/commands.jsonl), [all-command result index](evidence/results.json)
and [probe oracles](evidence/checks.jsonl) include passes and failed attempts.

- PASS: independent initial Git inspection and all mandatory preparation.
- PASS: new rootless Podman Ubuntu 24.04 x64 userspace, no host mounts or initial
  build tools; exact image/package versions recorded. It shares host kernel/CPU
  and network. This is not a relocated checkout test on the existing host OS.
- PASS: official full LLVM 23.1.4 archive hash checked before/after transfer,
  extracted fresh; versions/commit, Release, assertions OFF and RTTI NO confirmed.
- PASS: all seven HTTP dependency source downloads/builds, 22 child commands,
  167 prefix files verified. No development-built HTTP library was reused.
- PASS: plain Release CMake configure including genuine ORC link/run probe,
  264 production build actions, compiler and tooling component installs.
- PASS: prefix relocation; original container source stdlib moved out of reach;
  installed PATH discovery, minimal `main.void` check/run and installed `console`
  import check/run, default O2. Standard module output matched exactly.
- PASS: installed `voidc build` and direct execution of its native executable.
- PASS: installed `voidls --discover` and real framed initialize/initialized/
  shutdown/exit; protocol 1, shutdown null, exit 0. No editor UI claim.
- PASS: 86 installed files/aliases hashed; all 21 installed standard sources
  byte-match preserved source; actual ldd/readelf closure resolves successfully.
- PASS EXPECTED REJECTIONS: missing HTTP prefix, unavailable stdlib root,
  absent native manifest, missing configured Clang. Each exits 1 with useful
  diagnostics. Disposable negative-control copies were never used for positives.
- FAIL, harness preparation: first fixture-generation command had Python quoting
  syntax error before VOID execution. Original preserved; corrected fixture passed.
- FAIL, harness oracle: first minimal check exited 0 with correct current success
  text, but the harness incorrectly required empty stdout. Source inspection
  confirmed the CLI contract; corrected check passed. Original failed oracle
  remains in receipts. No production repair or required status change occurred.
- FOLLOW-UP: existing date/console/proof warnings retained without suppression;
  no crash/sanitizer marker found in completed build output. Release is not
  sanitizer coverage. CMake's test-only dwarfdump variable was unused as expected.
- FOLLOW-UP, documentation recorder: no-index new-file diff returned 1 without
  whitespace output; its recorder expected 0. Original receipt retained; the
  final direct Markdown/JSON/link/preservation validation passed.
- INVENTORY TOOL LIMITS: absent `rg`, broad find exit 1 with suppressed stderr,
  and a guessed example-path read failure are recorded separately. Python/find
  and the actual paths supplied the inventory. No test result depends on them.

SKIP: full CTest/Debug/strict reruns, HTTP/HTTPS regression campaign, logging and
`text.translate_scalars`, LLVM19 crash reproductions, preserved-artifact optimizer
investigation, native Windows/macOS, actual editor UI, archive/VSIX packaging and
all owner-controlled Git/CI/publication actions.

UNKNOWN: independent machine/VM and other distro/ABI results, complete missing-
prerequisite matrix, absent-CA diagnostics, fresh-network LLVM SDK download,
post-link application relocation, broader editor compatibility and distribution
license/notice clearance. No unavailable host result was invented.

## Blockers, next step and stop conditions

The [blocker register](PREVIEW_BLOCKERS.md) separately classifies optimizer risk,
logging performance, public licensing, Linux install scope, SDK/application
portability, notice completeness, documentation and platform/editor coverage.
Logging performance remains OPEN and public redistribution is not authorized.
Historical logging correctness is not performance acceptance.

Fresh source installation works on the recorded Ubuntu userspace with its
retained absolute SDK paths. Generated executables retain an absolute installed
library RUNPATH. CMake installed HTTP/common notices and GNU corresponding source,
but has no standalone LLVM notice rule; this needs review before distribution.
The protected README still names LLVM19, and the current build guide omits HTTP
preparation and the complete installed-use workflow. These findings were documented,
not repaired speculatively. No environmental blocker prevented this bounded run.

**Recommended next step:** Golden Wrench reviews this report, evidence and the
fresh-container acceptance boundary. Boss then decides any separately scoped
documentation/notice/portability work and a later independent Linux machine/VM
attempt. Windows/macOS installation qualification follows the accepted Linux
baseline. No implementation, CI or public release action is authorized by this
handoff; engineering stops here for review.

Stop immediately for a new correctness/safety issue outside installation scope.
No crash campaign or logging work may restart under this qualification. Do not
mark historical optimizer cause fixed from passing LLVM23 checks. Do not edit
root README, production/CMake/Runtime, stage/commit/push, create a branch, dispatch
hosted CI, distribute binaries or deploy without the stated owner authority.

## Resumption and evidence

Raw evidence is retained privately under
`$HOME/.local/state/void-preview-qualification/97aa4e43-20261104/`: per-command raw
stdout/stderr/stdin and argv, source tar identity, dependency build evidence and
CMake/configuration archive. Portable receipt variables redact actual host path
bindings; no full environment dump or credentials are included. Large logs,
compiler binaries, SDK and installed payloads remain outside repository docs,
website and public outputs. Only compact metadata/receipts are added here.

Container `void-preview-linux-97aa4e43` is stopped and retained. A later explicitly
scoped resumption can start it; do not treat its already-built state as a new
clean attempt. Valid install: `/opt/void-relocated`; original source stdlib:
`/work/source-stdlib-preserved`; negative-control install:
`/opt/void-missing-manifest`. The SDK remains under `/src/void/.toolchains/llvm-23.1.4`.
No further test or repair is needed to complete this report.

## Exact changed-file inventory

All changes are new, **uncommitted and unstaged** documentation/receipt files.
Tracked source, protected README, historical evidence and HEAD remain unchanged.
The [final preservation/inventory receipt](evidence/final-inventory.json) records
Git checks, protected/input hashes and every path below. Its own hash is omitted
from the digest table to avoid a recursive checksum.

- `docs/release/LINUX_CLEAN_HOST_QUALIFICATION.md`
- `docs/release/PREVIEW_BLOCKERS.md`
- `docs/release/PREVIEW_QUALIFICATION_HANDOFF.md`
- `docs/release/evidence/build-log-review.json`
- `docs/release/evidence/checks.jsonl`
- `docs/release/evidence/commands.jsonl`
- `docs/release/evidence/dependency-build.json`
- `docs/release/evidence/examined-files.json`
- `docs/release/evidence/final-inventory.json`
- `docs/release/evidence/installed-layout.json`
- `docs/release/evidence/inventory-tooling-notes.json`
- `docs/release/evidence/lsp-handshake.json`
- `docs/release/evidence/probe-inputs.json`
- `docs/release/evidence/qualification-start.json`
- `docs/release/evidence/results.json`

Container cleanup note: `podman stop --time 10` exited 0 after warning that PID 1
(`sleep infinity`) did not stop on SIGTERM and required SIGKILL. No VOID probe was
running then; this is container housekeeping, not a compiler crash or unknown
qualification exit. Final inspection reports `exited` with no mounts.
