# G27 — Complete library authoring integration

13 November 2026. Boss-authorized Keyboard implementation at baseline
`5aa51bdd1edbf1146fb64379497b2a1fb0b78561`, following Golden Wrench's review of
[G27-R research](../compiler-health/G27_FUNCTIONAL_COMPLETENESS_AUDIT.md).
Local implementation and focused qualification are distinct from independent
acceptance, hosted CI and release. No staging, commit, push or publication is
part of this work.

## Production changes and CLI contract

There are exactly two production changes:

1. `src/frontend/project_creation.cpp::plan_creation` adds
   `if(!app && starter)text+="\nentry = "+toml_string(*starter);` after the package
   name. The creation plan already owns the generated source path. New libraries
   now advertise that exact import entry, including custom `--source` paths.
   There is no source scan or entry inference. Init has no starter and still
   leaves package entry unspecified; application main discovery is unchanged.
2. `src/frontend/project_resolution.cpp::add_dependency` removes only the
   `manifest->kind()!=Kind::application` refusal. Application and library
   consumers now use the same Resolver, lock validation and
   `publish_package_edit`, including rollback and interruption recovery.

The supported commands remain:

```sh
voidc new PATH --kind library --package-name NAME [--source RELATIVE_VOID_PATH]
voidc init DIR --kind library --package-name NAME
voidc add ALIAS [--project DIR] --path DIR
voidc add ALIAS [--project DIR] --git ORIGIN --rev FULL_SHA [--subdir DIR]
voidc tidy [--project DIR] [--check]
```

Creation retains its existing name/version/project/package ID options, host
entropy, exact-case/path validation, `--nested`, exclusive publication and
existing-destination refusal. Default library entry is `src/NAME.void`; a custom
starter becomes the entry verbatim through the existing TOML serializer.
`init` preserves source and requires the author to supply a known package entry
before that library can be added. This deliberate init boundary is not a
regression. Library check does not invent an executable root; direct library
run/build remain unsupported.

Add defaults to the current project. Path arguments are selected-project
relative: in-project paths stay live, external paths become editable copies
under `libs/ALIAS`. It verifies the source's explicit package entry, ID and
version and appends the dependency table while retaining existing TOML bytes,
comments, order and newline style. It maintains the existing lock format.
Repeated matching additions preserve editable source changes. Full immutable
Git pins retain D3 acquisition/proof/snapshot rules; transitive acquisition
still needs explicit D3 authorization. No bare-name registry is added.

Tidy validates relevant imports, retains unused dependencies with advice,
refreshes legitimate local locks and reconstructs verified optional Git views.
It does not fetch or upgrade pins. `tidy --check` changes no bytes, timestamps,
directory membership or administrative state and fails for required corrections.
CLI usage/operation status conventions are unchanged; see [CLI](../CLI.md).

## Generated and transitive workflows

The required unedited journey is maintained in `tests/project/creation.py`:

```sh
voidc new app
voidc new geometry --kind library --package-name geometry
voidc add geometry --project app --path ../geometry
voidc tidy --check --project app
```

The test then imports the generated geometry API and observes return status 42.
It also checks default/custom library entries, no guessed init entry, unchanged
application metadata and a real voidls definition pointing into installed source.

`tests/project/maintenance.py` no longer repairs generated manifests. It creates
geometry, adds it to a generated consumer library, then adds that library to a
fresh application. Consumer source explicitly imports geometry and returns 49;
the application imports only consumer. The maintained onboarding example in
[creating and composing libraries](../LOCAL_LIBRARIES.md#creating-and-composing-libraries)
was executed directly from its shell/source blocks: **12 commands pass**, with
exact final stdout `49\n`.

The maintenance fixture also adds a full-pin Git source to a library and consumes
it through an application already declaring the same verified immutable origin.
That is shared D3 identity, not coalescing independent editable packages. A second
copied local geometry with the same ID is rejected with **VOID5007** and preserves
the published graph. An application's undeclared `pkg/geometry` import remains
an error; nothing implicitly re-exports transitive aliases.

## Focused qualification

All profiles reuse the existing complete LLVM/Clang/LLD **23.1.4** environments.
Linux builds are `build/llvm23-debug`, `build/llvm23-release` and
`build/llvm23-strict` (RelWithDebInfo, `VOID_SANITIZERS=ON`). Strict invocations use
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, without suppressions.

Windows executes natively in the existing VM: Windows 10 build **19045**, despite
the VM's historical name “Windows 11”, with x64 VS 2022 native environment,
pinned SDK and `/MT`. Existing Debug/Release configurations are reused.

| Profile | Focused CTest | Native-library fixture | G27-R combined replay |
| --- | --- | --- | --- |
| Linux Release | 18/18 pass | Pass | 19 commands pass |
| Linux Debug | 18/18 pass | Pass | 19 commands pass |
| Linux strict ASan/UBSan + leaks | 18/18 pass | Pass | 19 commands pass |
| Native Windows 10 Debug | 17/17 pass | Pass | Not repeated |
| Native Windows 10 Release | 17/17 pass | Pass | Not repeated |

The 18 maintained Linux CTest cases are project manifest, dependencies, dependency
explain, packages, managed native, application package, package native, managed
native resolution, Git dependencies, Git HTTPS, selection, sessions, creation,
maintenance, creation process, LSP editor recovery, LSP project-root recovery and
LSP protocol. Windows has the same selection except the Linux-only managed native
resolution case, for 17 registrations. These include local HTTPS pinned Git fixtures and package ownership,
tamper, native approval, relocation and reader/publication controls. No unrelated
full suite or hosted campaign is required or dispatched.

Build the existing targets `voidc voidls void_project_creation
void_project_manifest void_project_selection void_project_sessions
void_project_application void_project_packages_ir` with `cmake --build`.
Run `ctest --test-dir BUILD --output-on-failure -j 2` with the exact selection:

```text
^(project_(manifest|selection|sessions|creation|creation_process|maintenance|dependencies|dependency_explain|packages|managed_native|managed_native_resolution|application_package|package_native|git_dependencies|git_https)|lsp_(protocol|editor_recovery|project_root_recovery))$
```

The additional maintained invocation is
`python -B tests/project/raylib_physical.py BUILD/voidc CATALOG` (use `.exe` on
Windows). It reuses the existing G24/G26 catalog for that profile. Linux also
replays Research's archived `linux/journey.py` with each current compiler and
a unique `g27-PROFILE` label. Coordinator scripts, command receipts, XML and
compiler/server SHA-256 values are retained with the evidence archive below.
The Windows replay packet reuses the retained
`/home/shmellyorc/VM/g26-20261113/final/source-generated-native.zip` and replaces
only the two production and three test members listed below. The evidence archive
keeps those five overlays and a 727-member source hash manifest, avoiding another
duplicate complete source packet. The native coordinator restores every changed
guest source file after qualification; active build configurations remain available.

## Original failures and corrected regressions

Research's baseline findings remain unchanged in the untracked G27-R report
and verified archive `g27-functional-audit-2026-11-13.zip`, SHA-256
`f264f3802f9df165391c3b138107da833369ac2771362debe2bf011a86ad4934`.
Its `linux/creation-old-expectation.txt` retains the actual failed assertion:

```python
assert package["package"] == {"id": "org.test.toolkit", "name": "toolkit"}
```

The original unchanged production fixture passed 93/93 commands; Research's
candidate failed that assertion after 48 commands. Its deliberately amended
fixture passed 93/93. G27 updates the maintained exact map to include
`"entry": "src/toolkit.void"` and adds workflow/boundary checks: **101 process
commands**, plus the generated-package LSP query. No safety assertion is removed.
Original F1 missing-entry and F2 application-only refusals on all five baseline
profiles remain historical evidence rather than being rewritten as successes.

Two implementation qualification setup failures are retained. The first extended
maintenance run used a hyphen in a generated library package name for a recovery
phase. Existing VOID5004 correctly rejected that test input; the fixture now uses
identifier-safe recovery names. The first Windows build overlaid only the changed
files onto the restored older guest source and lacked G26 header declarations
(`ProjectFiles`). It failed during `project_creation.cpp` compilation. Reusing the
matching maintained G26 source packet plus current G27 files corrects that setup;
no platform API or production workaround is introduced. An attempted guest-control
login was unavailable, so the existing interactive native shell launches the
coordinator. These failures are not hidden by the final passing receipts.

Evidence collection also exposed a disposable ZIP-helper error after all product
tests passed: Windows path separators in member lookup keys differed from
zipfile's normalized names. The retained `initial-collection.log` records that
KeyError. The helper uses `relative_to(root).as_posix()` before writing/reading
member names, then verifies the same bytes. This follows Python's
[portable path spelling](https://docs.python.org/3.14/library/pathlib.html#pathlib.PurePath.as_posix)
and [existing zipfile implementation](https://github.com/python/cpython/blob/3.14/Lib/zipfile/__init__.py).
It changes no compiler, package ZIP implementation or cleanup subsystem.

## Security, native approval and publication

No resolver identity/origin/version check, full-pin or snapshot verification,
native ABI, provider selection, ownership receipt, case/collision/alias check or
existing application path is changed. The maintained tests still reject forged
immutable tree claims, modified/unowned Git views, boundary-bypassing lib/quoted
imports, wrong IDs/versions, conflicting aliases, occupied destinations and
failed/missing sources. Git pin updates retain explicit fetch and verified old
view replacement. Tidy check compares complete pre/post project state.

The five process-interruption phases now run for both application and library
consumers: before/after publication, displaced replacement, before replacement
commit and after replacement commit. Read-only check/tidy refuse pending edits
without mutation; mutating tidy restores the exact prior manifest, lock and
installed tree. Recovery continues through the one existing publication journal.
This does not claim power-loss or hostile-filesystem transaction safety.

The raylib fixture now generates an engine library, adds raylib to that library,
and adds engine to an application. Check refuses the application without its own
native requirement. After explicit requirement/catalog admission, check succeeds
and run still refuses without `--allow-native`. Approved normal/O2 JIT, linked
AOT and D5 relocation execute the transitive API with `LIBRARY: 41`. Existing
physical signature/body/provider guards and editable wrapper checks remain.
No native preparation or provider startup occurs during add/tidy/check/tooling.

## JIT, AOT, D5 and editor evidence

The generated transitive source fixture succeeds under normal and O2 JIT,
linked native AOT, package verification and relocated D5 execution after all
original source projects move. Research's combined replay includes conversion,
collections, generic local source, application args and Unicode; exact stdout is
`conversion passed\nresult=049 args=2 unicode=雪😀\n` in both JIT modes, native
AOT and relocated execution. Those replays establish source independence, not
SDK-free clean-machine installation or arbitrary native-provider isolation.
The new headless raylib path separately covers transitive native source and D5.

Fresh real voidls stdio checks cover generated-package definition navigation;
the existing package fixture covers transitive definitions, completion, dirty
package buffers and diagnostics recovery. The selected LSP protocol/editor/root
recovery fixtures preserve G13/G21/G22 ownership and synchronization behavior.
No editor resolver or client behavior changes. Actual VS Code UI qualification
is the prior G26 evidence, not a new G27 UI claim; Vim/Neovim parity, Windows
PDB/IDE and JIT debugging remain outside this task.

## Documentation reconciliation

Updated current strategy, source compiler overview, local-library guide, CLI,
first-program links, docs index and milestone register. They describe implemented
project sessions/metadata, native imports/packaging and integrated G23–G26 scopes,
the new library workflow, existing TCP/UDP/DNS delivery and actual platform limits.
Historical reports, original qualification limits, language history, G27-R audit
and the root README are preserved. Standalone raylib publication remains pending.

## Storage, retention and final repository state

Task scratch was `/tmp/void-g27-integration`; Windows transport was
`/home/shmellyorc/VM/g27-20261113`. Existing build configurations, active SDKs,
native catalogs, G26 evidence and Research's original archive remain retained.
Starting available space was **362,478,264,320 bytes** on `/` and
**16,513,069,056 bytes** on `/tmp`.

Evidence is retained in
`/home/shmellyorc/Documents/Void.Language-archives/g27-library-authoring-2026-11-13.zip`:
**139 selected files**, **1,107,671 original bytes**. The ZIP is **609,056 bytes**,
including its embedded restoration/checksum manifest. ZIP SHA-256:
`496684a9100b255a897b0f6732d191d4632ab0387e4e490f50d6019868ae54f4`.
DEFLATE level 9, CRC and every extracted member's SHA-256 passed. Source bytes
were rechecked before deletion. The existing external `archive-index.json` has
the verified entry; the adjacent `g27-library-authoring-2026-11-13-cleanup.json`
records exact cleanup results. Original setup failures, source hashes, XML,
command receipts, generated inputs, replay scripts and native source restoration
records are retained. Research's earlier ZIP remains unchanged.

After verification, only task-owned scratch/transport and the two private guest
scratch roots were removed. Linux scratch held **102,408,192 allocated bytes**;
transport held **2,531,328 allocated bytes**. Guest scratch held **124,585 logical
bytes**. All **92** overlaid guest source files were restored exactly before cleanup.
No active build, toolchain, catalog, G26 evidence or unrelated file was removed.

| Filesystem | Available immediately before cleanup, bytes | After cleanup, bytes |
| --- | ---: | ---: |
| Linux `/` | 362,420,449,280 | 362,420,596,736 |
| Linux `/tmp` | 16,410,640,384 | 16,513,048,576 |
| Native Windows `C:` | 8,805,818,368 | 8,805,961,728 |

These filesystem-wide measurements include other activity and retained build/VM
writes; they are not an assertion that archive compression reclaimed that amount.
No broad cleanup was performed.

## Exact changed-file inventory and Git checks

| Repository path | Change |
| --- | --- |
| `src/frontend/project_creation.cpp` | Serialize the known new library entry |
| `src/frontend/project_resolution.cpp` | Remove the application-only add guard |
| `tests/project/creation.py` | Exact manifest contract, default/custom/init boundaries, unedited workflow and generated-package LSP navigation |
| `tests/project/maintenance.py` | Remove metadata repair; library/transitive/Git, origin/import refusal and library interruption regressions |
| `tests/project/raylib_physical.py` | Native library consumer, separate application approval and transitive JIT/AOT/D5 checks |
| `docs/DEVELOPMENT_STRATEGY.md` | Current project/native/network implementation status |
| `docs/SOURCE_COMPILER.md` | Current shared compiler/session/metadata/native/package overview |
| `docs/LOCAL_LIBRARIES.md` | Executed generation/composition onboarding, explicit identities and current maintenance contract |
| `docs/CLI.md` | Generated package entry and application/library add contract |
| `docs/FIRST_PROGRAM.md` | Library onboarding link |
| `docs/README.md` | Canonical library/maintenance/report navigation |
| `docs/roadmap/MILESTONE_REGISTER.md` | G23–G26 integration identities and G27 status |
| `docs/projects/G27_LIBRARY_AUTHORING.md` | This consolidated implementation handoff |

These are **13 task-owned changed/new files**. The separate pre-existing
untracked `docs/compiler-health/G27_FUNCTIONAL_COMPLETENESS_AUDIT.md` is preserved
byte-for-byte, SHA-256
`3e26040d71a74b107e529a4daaa572faaa2ccf95208b9d4b4263a93c74d73eeb`.
No other historical report or the root README is modified. Git HEAD remains the
baseline; all tracked changes are unstaged and both reports are untracked.
`git diff --check` exits 0. The new relative documentation link scan reports
**zero newly missing targets**; it does not claim to repair historical broken links.
Exact status, diff statistics, patch and checks are retained with the receipts.

## Unqualified boundaries

No current macOS qualification, hosted CI, standalone public raylib acquisition,
GUI/device campaign, new real VS Code UI campaign or clean-machine release is
claimed. Async CLI root hosting, scheduler/Runtime/GC/native ABI changes, type
system expansion, implicit transitive re-exports, origin merging and package
security changes are excluded. Existing unresolved optimizer, Windows symlink,
executable-cleanup and external graphics leak records remain in their original
reports; these focused headless checks do not close those incidents.
