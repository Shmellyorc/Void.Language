# G94 — VOIDUP and Python-free developer experience

21 November 2026. Current baseline `cc89906a2133eeead625b7167d1881562d1ef1bd`,
initially clean. Original G94 baseline `8025d23f471c65a0f4bcc00f91189afff1d06ff3`. **Candidate for independent Golden Wrench review.**
No independent acceptance, Git integration, hosted acceptance, production release
or live Wiki publication is claimed. LLVM/Clang/LLD remain **23.1.4**.

## Result and scope

Retained the existing native VOIDUP application and its release architecture.
Corrected its source compatibility with current enum diagnostics, added verified
session/editor exports, replaced consumer raylib preparation with a standalone
VOID resource hosted by the existing compiler, removed Python from the manual
header-import and raylib-example setup tutorials, and prepared the live Wiki's seven-page installation
correction. Internal Python regression/generation/production tools remain useful
and are retained.

The primary documented route is **obtain VOIDUP → install a complete toolchain →
create a project → code**. There is still **no verified public bootstrap/catalog**.
This is a working private engineering route, not a new public distribution claim.
The precise remaining production work is in
[release production](../voidup/RELEASE_PRODUCTION.md); the developer-facing
command and root contract is in [VOIDUP usage](../voidup/USAGE.md).

Root README, prior reports/evidence, dependency pins, native libraries, original
compiler/Runtime/ABI architecture and the release authentication scheme are
unchanged. No stage, commit, branch, push, CI dispatch, system installation,
binary publication or live Wiki write was performed. The original installation journeys used task-owned qualification prefixes.
This continuation creates no toolchain installation or bootstrap copy.

## Audit of the retained implementation

Read `apps/voidup/`, Phase A/Phase B and integration reports, G29 readiness,
existing host/HTTP/LLVM packaging, package preparation and the live/current Wiki.
The integration report's older provider/process incidents remain historical
qualified/unqualified evidence, not newly repaired claims. In particular, later
host-provider native results do not qualify native Windows/macOS **VOIDUP
application installation**. G29's newer-host ABI failures remain preserved.

| Responsibility | Existing implementation retained |
| --- | --- |
| CLI and native bootstrap | VOID `main`, strict global options and literal forwarding; CMake already builds it with matching native `voidc`. No runtime Python installer. |
| Authentication | Exact catalog bytes prefixed by `VOIDUP CATALOG v1\n`; bounded schema-1 envelope; two distinct valid Ed25519 signatures among three pinned public test roots. |
| Freshness | Expiry/future bounds, serial replay/equivocation checks, retained clock floor and accumulated revocations; trust persisted before artifact transfer. |
| HTTPS | Explicit HTTPS origin, no query/fragment, no redirects, same-origin metadata/artifact references, explicit TLS roots only when requested, finite transfer/deadline bounds. |
| Whole payload | Catalog pins inventory and archive extents/SHA-256; ZIP checks member size/hash/CRC, names, duplicates, collisions, expansion and executable classification; complete installed-tree checks reject extra objects. |
| Publication/recovery | Held private directories, no-follow traversal, exclusive transaction names and writer lock, file/nested-directory synchronization, no-replace immutable versions, atomic control publication with honest uncertainty flags. |
| Toolchains | Versioned full prefixes, signed build/host/LLVM identity, current/previous selection, verification before switching/rollback and explicit verified tool launch. |
| Process safety | Absolute selected executable, held/rehashed tool, literal arguments, controlled environment, stdio inheritance and preserved normal exit status; direct-child resource cleanup. |
| Dependencies | Existing libarchive/libsodium/HTTP implementations and profile/recipe/source/file receipts; compiler/tooling CMake components, native resource and notice manifests. No library or crypto rewrite. |

The producer composes both install components, materializes symlink aliases,
packages Clang/LLD plus `sdk/lib/clang/23`, and inventories all bytes. The exact
current Linux fixture includes 29 standard source modules, compiler/server,
Runtime/native/HTTP/host libraries, package sources and the VOID preparation
resource. The host label denotes the existing supported slot, not proof that a
PikaOS-linked binary runs on Ubuntu. All current execution here is on Linux x64
PikaOS; no current-SHA Ubuntu-floor release was built.

## Native workflow corrections

### VOIDUP diagnostics and discovery

Current stdlib enums require `.name`; VOIDUP still used `string(cause.kind)` and
`string(cause.phase)`. This prevented rebuilding it against the accepted language
baseline. Updated only those diagnostic conversions; original failed compiler
output is retained. No Runtime change or enum-display compatibility shim was added.

`voidup env [--format json|sh|powershell|vscode]` verifies the selected complete
prefix and returns build identity plus matching tools, a quoted session PATH
export, or the existing editor setting keys. It shares the existing manager
selection/verification path. No automatic environment/profile/registry write,
proxy executable, activation directory, editor resolver or second installation
root was introduced. Shell exports preserve existing PATH entries and reject an
unrepresentable host PATH separator. JSON/editor paths and `run` remain usable.

Exports name a specific version. After switching or rollback, regenerate them
and restart the server. A graphical editor can use the explicit paths without
inheriting terminal PATH. Ordinary direct launches retain ordinary executable
semantics; `voidup run` performs verification on every launch. No manager
self-update or `latest`/automatic release ordering is invented.

### Verified packages

The only maintained bundled verified package is raylib. Its old consumer first-add
selected Python from PATH and invoked the installed Python preparer. Replaced
that path with the compiler-hashed `raylib-prepare.void`, staged with the already
verified source/receipt before hosting it through the existing `voidc run` path.
Package-supplied scripts are still not executable authority.

The utility reuses VOID HTTP, SHA-256, ZIP, filesystem, text, OS and process APIs,
then existing `native-bind` and the configured Clang. Official Linux/Windows asset
size/hash pins, provider ABI/descriptors, target identities, source snapshot,
native locks, consent and consumer wrapper ownership are retained. It builds
reviewed physical bindings; it does not regenerate public declarations.

The ZIP module intentionally accepts ZIP only. Linux's official binary asset is
tar.gz, so the utility verifies the entire pinned asset first, then runs explicit
`/usr/bin/tar` with one exact member and bounded stdout; no archive paths are
extracted. It strips TAR_OPTIONS and caller PATH from child environments; native
Windows uses the existing ZIP reader. First Linux add therefore needs GNU tar/gzip
and the declared native development libraries. This is documented, not concealed
as a bundled capability. Python/CMake are not consumer dependencies.

The original correction created owner-only OS scratch rather than adopting or
chmodding a raced candidate. The continuation now uses project-local
`.void/acquire-<random>/` scratch through the existing project-directory publisher.
It reuses exclusive private directory creation, pinned identities and cleanup;
no second storage subsystem was added. An existing unsafe or aliased `.void`
fails closed. Normal completion and recoverable failure remove only this
invocation's leaf. A substituted leaf is preserved, and hard-killed acquisition
scratch requires manual inspection rather than automatic deletion authority.
The original group-mode failure remains in the preserved archive.

### Header import and ordinary generation

`voidc native import`, `native-bind`, declarative C package preparation, source
library creation/registration/add/tidy, standard sources and ordinary project
creation/check/JIT/build are native already. The manual native tutorial still
used inline Python to hash/write a catalog. Replaced that step with the small
VOID example `examples/native/catalog.void`, using existing crypto/IO and exclusive
output creation. It records identities, not execution consent. No unrelated
utility was added to the compiler CLI.

## Python dependency inventory

| Workflow | Consumer requirement after this candidate | Internal or remaining scope |
| --- | --- | --- |
| VOIDUP obtain/install/verify/select/rollback/run | Native bootstrap and supplied authenticated channel; no Python/CMake | Public bootstrap, production roots and hosting still unpublished. |
| `voidc` new/check/run/build/package, stdlib | Native compiler and matching prefix/resources | Host CRT/development libraries for native linking remain real requirements. |
| Local/registered source libraries | Native package commands, no Python | Git source dependencies additionally use the existing explicit Git transport. |
| First bundled raylib add | VOID preparation resource, HTTPS, native-bind, packaged Clang/LLD; Linux tar/gzip | OS development/device libraries remain; Windows execution unqualified here. |
| Subsequent raylib import/check/run/build | Locked editable source/provider, no preparation/download/Python | Native execution needs explicit matching consent; wrappers remain consumer source. |
| Header import and checked binding emission | Existing native importer/emitter and selected Clang | Standalone VOID tutorial now writes its catalog without Python. |
| CLI/editor onboarding | Native tools and prepared matching VSIX; literal exported paths | Extension/source packaging uses Node/npm internally; no Marketplace availability claim. |
| Compiler/source building | Contributor workflow still uses Python/CMake and full pinned SDK | `install.py`, SDK/HTTP/host recipe/verifier scripts are producer utilities. |
| Release engineering and bootstrap assembly | Producer may use Python offline | Existing `build_voidup.py` and engineering fixture producer retained; no Python runtime decision path. |
| Maintained binding/data generation | Not run by consumers | `tools/raylib/*`, Unicode/tzdata and research/regression harnesses retained. |
| Wiki maintenance | Not a compiler/install requirement | Structural checker/exporter remain internal Python tools. Async Wiki's Python command is explicitly a contributor host-adapter test, not ordinary onboarding. |

Search found no remaining Python invocation in `src/` for ordinary commands.
Retained `verified-packages/raylib-prepare.py` supports custom internal provider
qualification; CMake no longer ships or selects it as the consumer resource.
Historical docs and source-build instructions are not erased to reduce file count.

## Primary workflow and public readiness

The verified command sequence is documented in `USAGE.md`: explicit supplied
HTTPS catalog → list available → install ID with optional activation → list and
compiler version → project new/check/run/build → explicit selection/rollback.
Dedicated roots use OS user-data discovery or a repeated absolute `--root`.
Missing parents are privately created within the existing bound. Each complete
version keeps `bin`, `lib/void`, `share/void` and `sdk` together.

`build_voidup.py` still assembles the existing native Linux bootstrap, now with
compiler/source/stdlib/output hashes, build identity, host/libc and explicit
nonproduction status in its receipt. Native CMake/direct-voidc build instructions
and the existing producer API are documented reproducibly. This is not a claim
of byte-identical builds, release clearance or a newly invented distribution system.

Remaining public dependencies are exact:

1. Deploy a separately reviewed production trust build and independently managed
   offline Ed25519 keypairs/IDs; never the three engineering test keys. Existing
   source hardcodes test IDs and private diagnostics. No key ceremony or root
   rotation protocol was implemented or claimed.
2. Adapt the existing Linux engineering producer for production release IDs,
   host inputs, production signing and new receipt destinations. Its current
   convenience CLI writes historical evidence and is not a production signer.
3. Produce/qualify exact payloads on declared ABI floors and close the existing
   payload/source/notice obligations. Windows/macOS application/native packaging
   and the current Ubuntu-floor build remain unqualified. SDK/CRT prerequisites
   must be declared rather than disguised as bundled support.
4. Configure and verify a direct, same-origin, no-redirect HTTPS channel serving
   catalog, `.sig`, inventories and complete large archives. GitHub release asset
   downloads can redirect and cannot simply be substituted into this contract.
   The GitHub Releases API returned `[]`; no usable public bootstrap/catalog URL
   was verified. No paid signing certificate or membership is needed or used.
5. After independent review/integration, perform Boss-controlled publication and
   fresh-host journeys, including live Wiki rendering. Neither local private
   fixture publication nor source inspection establishes those outcomes.

## Qualification and preserved failures

Original evidence is retained in
`~/Documents/Void.Language-archives/g94-voidup-python-free-candidate-2026-11-21.zip`
under `evidence/`, with the original candidate source under `source/`. The old
`/home/shmellyorc/Documents/void-g94-evidence-20261121` path in recorded commands
is historical and is not a current reproduction destination. Generated
installations, keys and binaries are not restoration inputs from this archive.
The archive SHA-256 is
`1dcf0345b400fec60e80aba063f64055cbcec00dabda50fd3b1b91db54f89d23`;
the continuation verified all 712 member hashes plus the manifest.

Continuation receipts and the current Wiki review are retained under
`~/Documents/Void.Language-archives/g94-continuation-2026-11-21/`.
Disposable build/test artifacts used this checkout's `.void/g94/`. Manager and
consumer probes that require safe ancestors used automatically cleaned private
fixtures below the continuation evidence root, with project acquisition and
application output under each fixture project's `.void/`. No new evidence
archive, copied toolchain or retained `/tmp` output was created.

The qualification bullets below describe the **original G94 candidate**, before
the storage baseline and continuation. They are preserved evidence, not a claim
that a new signed payload of the current source has been installed.
All test compilers, generated native programs and manager invocations use 1 MiB
soft/hard main stacks where the harness records that boundary. Build orchestrators,
Node/GFM rendering and Python parents are not claimed as 1 MiB executions.
No unrelated compiler-wide suite was run. No Runtime/provider library source or
pin changed; no native Windows/macOS or full workbench GUI run was performed.
The existing Windows VM was running, but VirtualBox reported no active guest
sessions and the available GuestControl login was refused. Native desktop
control is unavailable in this session. No credentials, VM configuration or
security settings were changed; the refusal and VM observation are retained as
`native-guest-access.txt`, `native-guest-sessions.txt` and
`native-vm-availability.txt`. This is an execution-access limit, not a product
failure or a claim that the Windows machine does not exist.

- Native Release builds: matching `voidc`, `voidls`, `voidup`; current VOIDUP,
  preparer and catalog sample all pass source checking.
- Registered manager smoke/presentation/CI-wiring checks: **3/3 pass**.
- Signed fresh full-toolchain journey: real HTTPS/test threshold keys, complete
  inventory, identities, private modes, root quoting, both tools, project new/check/
  JIT/native build, LSP initialize/shutdown, A/B selection/rollback, tamper refusal,
  threshold failure and serial replay refusal. Initial journey has **33** receipts.
- Hostile metadata/package suite: **30 named rejection cases** plus TLS and
  unknown-root refusal. Existing threshold, exact-byte, path/ZIP/hash, transport,
  expansion, revocation and replay contracts remain exercised.
- Interrupted real transfer: **1,048,576 bytes** written before intentional
  SIGKILL; competing lock owner rejected, trust floor/partial bytes retained,
  retry installs and verifies the actual release. Original failed `/tmp`-quota
  retry and the successful disk-backed repeat both remain.
- Native package consumer: first add with `PATH=.`, locked import/check, O0/O2
  JIT and independent AOT print `241`; no-consent refusal and unchanged-add reuse.
  Follow-up verifies locked resolution, tampered preparation-resource rejection
  before project mutation, editable wrapper preservation and native source-library
  registration/add/uninstall. The exact signed current-prefix journey is recorded
  separately from the earlier task prefix. The complete multi-module raylib
  consumer example also passes native init/add/tidy/check and headless execution.
- Header-import tutorial: importer, native-bind, native Clang provider, VOID catalog,
  check, O0/O2 JIT, AOT and existing-output refusal pass with Python absent from PATH.
- Shared VS Code/VSCodium discovery source: exported explicit paths, PATH lookup,
  sibling compiler and real manifest preflight pass using installed tools. The
  source modules are transpiled locally with the existing TypeScript dependency;
  execution uses VS Code's embedded Node 24.22.1. This is discovery qualification,
  not a VSCodium/VS Code graphical workbench installation claim.
- Wiki: 24 topic pages, two shared files, all 34 unchanged VOID blocks and **118**
  unique local destinations pass structure/link checks. A current live preimage
  comparison, exact seven-page diff and source-link publication boundary are retained.

Preserved unsuccessful attempts include the original enum diagnostic failures;
reserved legacy function name `entry` in the new standalone utility; group-writable
LLVM temp mode; an overlarge aggregate capture budget (without changing its native
16 MiB limit); misuse of bounded split as a partition; a trailing descriptor blank
line; two `/tmp` quota errors; a missing `--project` in a test command; a string
instead of a workspace array in the discovery test; and real writer-lock contention
between our concurrent qualification callers. The corrected cases retain their
first evidence and were rerun only within the affected scope. None was hidden by
relaxing a security check or an assertion. Intentional SIGKILL evidence remains
identified as intentional, rather than a product crash.

## Live Wiki preparation and validation

Read-only clone and repeated `ls-remote --symref` observe default branch `master`,
HEAD **`4862b9fa11d1af0e712f92c1ad71946466db06cb`**. Installation currently leads
with Python source building. All 26 local baseline page bytes matched that live
snapshot before this correction; there are no unknown files in the observed clone.

The existing exporter now accepts a clean `--live-snapshot` and emits preimage
hashes, changed-file allowlist, unknown-file preservation inventory and a unified
`review.diff`. It still exports only the 26 allowlisted public files into `pages/`;
review JSON/diff remain outside. Final export:
`~/Documents/Void.Language-archives/g94-continuation-2026-11-21/wiki-review/`.
The continuation re-cloned the live Wiki read-only and revalidated the same HEAD
and all replacement preimages. The old export remains inside the original archive.

Exactly seven live pages are proposed: Installation, Editor setup, First program,
Native compilation, Packages and raylib, Resources and status, Troubleshooting.
They lead with VOIDUP, explain availability, roots, activation/rollback,
session/editor exports and genuine host requirements, and keep source building
separate. No milestone labels or workshop conversations enter public pages.
All seven existing live Wiki routes returned rendered HTTP 200 pages; this
observes the old live content. Existing tutorial source examples are byte-identical. The stale planned
`compile func` statement is corrected to implemented bounded pure evaluation.

Before the reviewed publication step, integrate matching source/canonical docs,
recheck remote HEAD and all replacement preimages, inspect the seven-page diff,
preserve unknown/concurrent content, copy only the allowlisted changed bytes,
and use the existing normal fast-forward Wiki procedure. New `blob/main`
canonical links are local-validated but must exist on the integrated public main
before publication. No hosted rendering or live correction is falsely claimed;
the current live Wiki remains unchanged pending that step.

### Final candidate artifact identity and receipts

The original final exclusive-owner OS-scratch source was built and packaged as engineering
release `engineering-g94-final-2`:

- Compiler build identity: `3c486fc41511c8d286a0a1fa002923239647775d71b4a1c2d3abd09b02b2aacb`.
- Complete signed inventory: **678 entries**, **753,010,431 expanded bytes**, all
  29 standard modules; archive **269,221,408 bytes**.
- Archive SHA-256: `c883b9b229c1ac92ec262b6aed02eeb0cfe78a82e4e9b97f115c154364cacaff`.
- Inventory SHA-256: `da358575c233fb31da86466497228f816a7728ea300b4d2d58e03dc18716a2db`.
- `final-manager-2/final.jsonl`: **18 expected-status receipts**, exact final
  signed install/verify, Python-free first-add, O0/O2 JIT, independent native
  output, reuse, locked resolution, launch, rollback and reselection.
- `manager-qualification/g94.jsonl`: 33 original signed integration receipts.
  `final-manager/final.jsonl` and `final-followup/selection.jsonl` retain the prior
  signed candidate and the corrected lock-contention follow-up.
- `native-import/results.json`, `consumer-followup.json`,
  `complete-raylib-example.json`, `editor-discovery-2.json`, `ctest-final.log`,
  `rejection-regression/`, `interruption-regression-2/`, `bootstrap/build.json`,
  `bootstrap-smoke/`, `wiki-baseline-comparison.json`, `live-wiki-routes.json`
  and `wiki-export-final/live-review.json` retain their bounded evidence.

Original reproduction recipes (use current project-local scratch and archived receipts):

```sh
cmake --build build/llvm23-release --target voidc voidls voidup -j3
ctest --test-dir build/llvm23-release -R '^voidup_(smoke|presentation|ci_integration)$' --output-on-failure
python3 -B tools/wiki/check.py
python3 -B tools/wiki/prepare.py --output /absolute/fresh/wiki-export \
  --live-snapshot /absolute/clean/wiki-clone
```

The exact private-fixture construction and signed/native journeys are retained
as `qualify-voidup.py`, `qualify-final-toolchain-2.py`,
`run-legacy-regressions-2.py`, `check-native-import.py`, `consumer-followup.py`,
`check-consumer-example.py` and `editor-discovery-2.cjs` inside the original archive's `evidence/` directory.
Use fresh output names and separately held matching engineering keys for a replay;
do not overwrite original evidence or install these test credentials publicly.

## Continuation on the storage baseline

The initial worktree and index were clean at
`cc89906a2133eeead625b7167d1881562d1ef1bd`. Stash
`c72b9cd7d736d15ee91e4bf8c4378be1dfe20eed` has original G93 parent
`8025d23f471c65a0f4bcc00f91189afff1d06ff3`, a tracked tree containing both
storage and G94 work, and six untracked G94 files. Comparing the stash tree to
current HEAD identified 35 remaining tracked G94 deltas. Those deltas passed
`git apply --check` before restoration; the six new files were exclusively
created after collision checks. The stash was neither applied wholesale nor
popped/dropped. The index stayed unchanged. Storage-baseline native approval,
Runtime loader and engineering-storage source bytes remain untouched.

The continuation moves raylib preparation into project `.void/` using the
existing directory staging code; focused tests cover privacy, symlink refusal,
success/failure cleanup, user-file preservation and substituted-stage retention.
The Git ignore list now covers abandoned acquisition leaves without hiding
other user-owned `.void` content. No new installer or release infrastructure
was introduced.

Current local qualification uses the existing `build/source-release` environment
with pinned LLVM/Clang/LLD 23.1.4. The final `voidc`, `voidls` and native `voidup`
built successfully. The build has `BUILD_TESTING=OFF`; an initial combined build
request therefore could not find `void_project_creation`. Its original log is
retained. The focused creation test was compiled with the matching Clang and
linked against the freshly built `libvoid_core.a`, using the already generated
LLVM test link dependencies instead of rebuilding another environment.

Current checks, distinct from the historical signed-install receipts above:

- VOIDUP, native preparer and catalog example source checks pass with 1 MiB
  hard/soft stacks (`source-checks.json`).
- Native VOIDUP smoke, presentation and CI wiring pass (`manager-tests.json`),
  with no installs, signing keys or catalog required. Initial private-storage
  rejection beneath the group-writable checkout is retained in
  `original-manager-tests-unsafe-ancestry.json` and `smoke/`; the same tests pass
  under disposable private archive-root scratch. No ownership guard was relaxed.
- The focused project creation/storage tests pass with a 1 MiB stack
  (`creation-tests.json`). The existing creation/publication cases also run.
- A real Python-free first `add raylib` with `PATH=.` succeeds, verifies the
  pinned upstream asset and leaves no acquisition scratch. Source checking,
  explicit no-consent refusal, O0/O2 JIT, native build/execution, locked resolution
  and unchanged-provider reuse pass with 1 MiB stacks (`consumer-verified.json`).
  Both JIT modes and the native executable print `241`. The disposable project
  and provider are removed after verification. No compiler installation is made.
- The initial consumer follow-up used unsupported consent flags, and the second
  assumed a run-staging directory exists for explicit invocation consent. These
  were harness errors; original receipts and `consumer-harness-failures.txt`
  identify them. The retained second fixture was reused for the correction.
- Wiki structure checks pass: 24 pages, two shared files, 34 unchanged VOID
  examples and 118 links. The live Git HEAD is still
  `4862b9fa11d1af0e712f92c1ad71946466db06cb`; the current export has exactly
  seven replacements, reviewed preimages and no unknown-file changes. The
  read-only GitHub Releases query again returns `[]`. No production bootstrap
  or catalog is verified, and no live page is written.

No new signed toolchain artifact or full install is produced in this continuation.
Original threshold/tamper/replay/interruption/activation/rollback and editor
qualification remain preserved in the verified archive; they are not relabeled
as tests of a freshly packaged current build. Windows/macOS native application
installation, current ABI-floor release packaging, production trust/hosting and
live Wiki publication remain unqualified or pending as described above.
Independent Golden Wrench review is the next acceptance boundary.

## Focused Windows catalog readback correction

Golden Wrench's independent integration review returned **CORRECTION REQUIRED**
for one P1: acquisition retained a Windows DELETE-open private stage, while
`read_package_directory(catalog)` discarded its active reservation and tried to
repin that ancestor without compatible delete sharing. The original verdict,
source identities and independent Linux checks remain unchanged in
`~/Documents/Void.Language-archives/g94-independent-review-2026-11-21/`.
This implementation does not declare independent acceptance of the correction.

Catalog readback now receives the existing `ProjectDirectoryReader` from
`with_project_scratch`. Its pinned ownership and reserved ancestry therefore
remain authoritative through inspection and file reads. The reservation is not
released, reopened with weaker sharing, or replaced. Native staging/loader,
private-access, reparse/symlink, hash, transaction and cleanup machinery are
unchanged. The nearby input-snapshot read already uses its supplied reserved
reader; the installed-package/journal reads occur outside the live acquisition
callback. No other acquisition catalog read dropped that callback's reader.

Two focused regressions reuse existing harnesses:

- `tests/project/creation.cpp` now writes and recaptures a catalog within the
  still-live acquisition reservation. Its Windows branch explicitly requires
  the default-reader attempt to report sharing violation 32, requires reserved
  readback to match every byte, and verifies that a competing stage rename is
  still refused. Existing success/failure cleanup, privacy, alias and replacement
  preservation checks remain in the same small executable.
- `tests/project/raylib_workflow.py --first-add-only` runs the actual compiler's
  fresh bundled raylib add on native Windows or Linux without the broader
  registration, execution and graphics suite. Consumer PATH is `.`. It verifies
  descriptor/runtime/link/notice sizes and SHA-256 values, locked resolution,
  user-file preservation and absence of acquisition leftovers. It lets `voidc`
  create `.void` with the native private ACL; Python does not create that ACL.
  This is an internal Python regression harness, not a consumer dependency.

Use a matching native compiler and an existing safe private scratch parent:

```sh
python3 -B tests/project/raylib_workflow.py /absolute/matching/voidc \
  --first-add-only --scratch /absolute/private/scratch
```

On Windows supply the native `voidc.exe` and an equivalent absolute private
scratch path. The POSIX focused mode enforces a 1 MiB hard/soft stack before
launching children. Windows requires the existing native compiler's 1 MiB PE
stack contract; no native Windows execution is claimed here.

Focused correction evidence is retained under
`~/Documents/Void.Language-archives/g94-windows-catalog-readback-correction-2026-11-21/`:

- Rebuilt only affected `voidc`/core inputs in the existing `build/source-release`
  environment with LLVM/Clang/LLD 23.1.4; no second compiler build environment,
  toolchain installation, bootstrap, signed payload or evidence archive was made.
- The updated creation/catalog-storage executable passes on Linux with a 1 MiB
  stack (`creation-tests.json`). It links the current core library using the
  existing generated test link dependencies; `creation-build.json` records both
  commands. Its Windows-specific sharing checks remain unexecuted.
- The real focused first-add regression passes on Linux with a 1 MiB stack:
  fresh project creation, Python-free asset acquisition/preparation, catalog
  identity checks and locked resolution (`first-add-linux.json`). Disposable
  fixtures, the test executable and its object were removed after verification.
  The earlier successful O0/O2 JIT, native execution, VOIDUP, authenticated-install
  and Wiki checks remain preserved; they were not needlessly repeated.
- The existing Windows 11 VM is running, but reports no active guest sessions.
  A bounded GuestControl execution attempt was refused at login. The command,
  refusal and qualification limit are in `native-windows-access.json`. No
  credentials, VM settings, permissions or guardrails were changed. Native
  Windows first-add and the Windows-specific regression branch remain
  **UNQUALIFIED**, pending executable guest access.
- `pre-correction-identities.json` binds the original candidate inventory and
  independent review hashes. The correction's `candidate-inventory.json` and
  `verification-summary.json` identify the corrected sources and compiler/core
  artifacts. Earlier candidate inventories and binary identities are preserved,
  not overwritten or relabeled as corrected-build results.

The correction changes exactly four paths relative to the reviewed candidate:
`src/frontend/project_resolution.cpp`, `tests/project/creation.cpp`,
`tests/project/raylib_workflow.py` and this report. The complete G94 candidate
still contains exactly 46 changed files against the accepted baseline, listed
below. The seven-page Wiki export is byte-identical to the reviewed export.
Return this corrected candidate for focused independent Golden Wrench re-review.
Nothing staged, committed, pushed, installed or published by this correction.

## Exact changed-file inventory

<!-- G94_CHANGED_FILES -->

Exactly **46 files** differ from the current baseline. This
supersedes the original 41-file inventory. The integration set contains source,
tests and documentation only; generated artifacts, keys and evidence stay out.

```text
.gitignore
CMakeLists.txt
apps/voidup/cli.void
apps/voidup/environment.void
apps/voidup/main.void
apps/voidup/manager.void
apps/voidup/storage.void
apps/voidup/support.void
docs/DEVELOPMENT_STRATEGY.md
docs/EDITOR_INTEGRATION.md
docs/FIRST_PROGRAM.md
docs/LOCAL_LIBRARIES.md
docs/NATIVE_LIBRARIES.md
docs/PROJECT_STORAGE.md
docs/README.md
docs/SOURCE_COMPILER.md
docs/install/README.md
docs/projects/G94_VOIDUP_PYTHON_FREE_DEVELOPER_EXPERIENCE.md
docs/voidup/RELEASE_PRODUCTION.md
docs/voidup/USAGE.md
docs/wiki/Editor-setup.md
docs/wiki/First-program.md
docs/wiki/Installation.md
docs/wiki/Native-compilation.md
docs/wiki/Packages-and-raylib.md
docs/wiki/README.md
docs/wiki/Resources-and-status.md
docs/wiki/Troubleshooting.md
docs/wiki/publication.json
examples/native/catalog.void
examples/raylib-consumer/README.md
src/frontend/project_creation.cpp
src/frontend/project_files.hpp
src/frontend/project_resolution.cpp
src/main.cpp
tests/project/creation.cpp
tests/project/raylib_workflow.py
tests/voidup/acceptance.py
tests/voidup/interruption.py
tests/voidup/rejection.py
tests/voidup/smoke.py
tools/build_voidup.py
tools/raylib/README.md
tools/wiki/prepare.py
verified-packages/raylib-prepare.void
verified-packages/raylib/README.md
```
