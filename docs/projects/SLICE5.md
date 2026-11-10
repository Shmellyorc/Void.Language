# Slice 5 project creation and VS Code first-file workflow

Baseline HEAD `2f667ea338127b8f91c3136804377ef3fa5218d6`. Golden Wrench accepted
[S4](SLICE4.md) for local continuation. This handoff stops at independent S5
review. The integrated general C FFI and dynamic raylib milestone remains the
hosted CI gate.

## Implementation and ownership

- `voidc init [DIR]` exclusively publishes a manifest in an existing admitted
  directory. It preserves filenames and source bytes, uses the schema defaults,
  and leaves entry selection to `voidc check --project .`. Init can succeed with
  zero or multiple eligible mains. Check reports the actual selection diagnostic.
- `voidc new PATH` publishes a directory with a manifest and a starter. The app
  defaults to `src/app.void` and a library to `src/<package-name>.void`. A valid
  custom path chooses the starter name and sole discovery root. The library
  starter has a public function and no executable entry. Both commands support
  the [documented options](../CLI.md#project-creation-slice-5).
- The native frontend creates two independent 128-bit host-entropy values when
  both IDs are omitted. It validates the serialized manifest using the Slice 1
  parser and validates the starter with the VFS module identity rules. CLI
  previews the absolute manifest path and resulting identities before writing.
  Invalid data and filesystem failures exit 1. Malformed CLI usage exits 2.
- Creation admission checks physical ancestor spellings, aliases and the selected
  root identity. An enclosing manifest requires `--nested`. Malformed,
  wrong-case and aliased manifest names still occupy the namespace. The stage
  contains only the checked manifest and, for new, its starter. Recheck includes
  the stage layout, file identity/link count and the reviewed bytes. Rollback
  examines recorded objects in reverse order and retains unknown content.
- The editor requests a bounded compiler inventory and compares only the
  returned identities to the fresh event batch. Automatic review requires
  a complete detail with at most 16 source identities and 8 KiB of identity text. It stores the offer state by
  physical workspace identity. The review requires a second explicit action
  after showing destination, name, kind, version and IDs. A same-root collision
  selects the existing manifest through ordinary compiler preflight. A new
  session uses the existing VS Code overlay synchronization. Automatic
  creation requires one trusted filesystem folder, no explicit project
  selector and an active VOID document within the workspace when one is open.
- Creation subprocesses use their own versioned envelope. Schema-2 project
  preflight, compiler semantics, legacy standalone entry mode and the runtime
  application metadata ABI retain their previous implementations.

## Filesystem qualification and limits

The native writer stages inside the admitted directory. POSIX holds a directory
descriptor, uses private modes and serializes cooperating creators at the same
parent. Windows pins physical ancestors and creates a staging directory with a
protected current-user DACL. Publication uses the platform's exclusive rename
operation. The check immediately after publication diagnoses an uncertain outcome
and leaves the published object available for inspection. This is one exclusive
publication, with separate physical checks around it. Filesystem snapshots and
power-loss durability are outside that guarantee. The rollback injection
preserves foreign content left in a staging directory and verifies that known
objects were removed. On NTFS, verified child handles close before an exclusive
directory rename while the stage root stays pinned. If publication fails, the
writer reopens each closed child and checks its saved volume/file identity before
removing it by handle.

## Acceptance and compatibility

| Case | Executed coverage |
| --- | --- |
| P36 | Existing `game.void` bytes preserved, second init collision, arbitrary nested starter, app/library defaults and overrides, independent IDs. |
| P37 | Wrong case, invalid names/version/identity, nested boundaries, concurrent creators, permission failures on Linux, entropy failure, stale root, stage tampering and foreign-object retention. |
| P38 | Real workbench accept and review, decline, dismiss, close while offered and persisted suppression after reopening. |
| P39 | Batch coalescing, existing-tree suppression, cancelled review, explicit reset, concurrent external manifest, dirty source through project handoff. |
| P40 | Restricted and multi-folder workspaces, outside-root source and older server. Virtual workspaces are disabled in the extension manifest. |
| S1–S4 | Manifest provenance/headers/authority, ordinary and nested project selection, sessions, profiles, schema-2 preflight, legacy standalone and application metadata checks. |
| Installed | CMake compiler/tooling installation relocated outside the checkout. Init/new execute from a fixture outside the checkout, generated app runs with release and debug profiles, and independent native output runs after its project source directory moves. |

## Reproduction

Use the existing qualified LLVM and provider configurations. The focused Linux
build/test script and editor launcher are recorded with the evidence. The editor
runner uses the matching built `voidls`, the retained S3 legacy `voidls` and the
existing VS Code 1.141.0 executable. It drives real workbench widgets through
CDP and uses workspace files, saved buffers and native subprocesses.

The Windows recipe verifies each source-packet digest, overlays the provider-attested
source root with exact backups, uses separate Release and Debug build trees, and
verifies restoration. Windows receipts identify the guest as Windows 10 x64 build
19045.

## Original failures and corrective reruns

- The starting compiler rejected `init` as unsupported. The first S5 C++ build
  selected `std::quoted` by overload resolution. The corrected writer uses a
  distinctly named TOML escaping function. The first TypeScript build lost
  derived response fields after root narrowing. Its validator now keeps the
  supplied response type while checking root fields.
- Linux Debug initially expected status 1 for a stale-root `voidls` creation
  call. The server's existing exception boundary uses status 2. The corrected
  test asserts status 2, the stale-root error, and a replacement root left empty.
- The standard VS Code test workbench refuses modal dialogs. The review uses
  Quick Pick with a separate, explicit publication action. The initial review
  used consecutive empty separator groups, so the workbench omitted metadata.
  The final review renders each field as a row. The harness selects the precise
  row label rather than the combined separator and row text.
- An earlier editor fixture left a deleted source open while creating another
  dirty main. The received overlays correctly produced an ambiguity. Closing a
  tab alone did not immediately dispose of the cached document. Isolating the
  old-tree negative control in a separate workspace preserves S3 overlay behavior
  and exercises the intended dirty-buffer handoff.

- Windows 10 Release and Debug initially returned error 5 while publishing a new
  project. The target directory had open descendants. Both product builds were
  rebuilt with the handle-lifetime correction. The second full run passed
  process creation and installation, but exposed a test fixture that never
  changed its staged file because CRT sharing conflicted with the held DELETE
  access. The corrected fixture opens with explicit compatible sharing and
  asserts a successful write followed by the precise staged-content rejection.

The original logs and exact reruns are under [Slice 5 evidence](evidence/slice5/).

The additional VS Code configuration regression first used the older default
server path and omitted `VOIDLS_TEST_SERVER`. Supplying the built and retained
legacy server explicitly corrected both failures. The same final editor build
then passed seven configuration tests and the 14-check S3 project-session host
suite. A test-mode relaunch also lost an outstanding offer because the workbench
uses ephemeral storage under the extension test launcher. The corrected
persistence test uses two ordinary editor windows with the same isolated user
data and graceful quit. The second window suppressed the new-file event. The
initial normal launch used VS Code's default shared storage. The final replay
isolated shared storage inside the disposable editor workspace as well.

## Final local qualification

| Environment | Result |
| --- | --- |
| Linux Release | 17/17 focused CTest cases |
| Linux Debug | 17/17 focused CTest cases |
| Linux fatal ASan/UBSan | 17/17 with fatal sanitizer and leak settings |
| Linux VS Code 1.141.0 | Nine S5 extension-host scenarios, two ordinary windows for persistence, seven configuration tests and 14 S3 project-session checks |
| Windows 10 x64 build 19045 Release | 14 distinct cases passed across the corrected product run and targeted fixture rerun |
| Windows 10 x64 build 19045 Debug | 14 distinct cases passed across the corrected product run and targeted fixture rerun |

The Linux process receipts include actual unoptimized and O2 JIT, AOT,
source-independent installed execution, no-argument OS application paths
matched to the generated app ID, and negative cases. The supplemental invalid
ID test returned `VOID5004` and left an empty destination parent in all three
Linux configurations and both Windows configurations.

On Windows, the final product run passed 13 of 14 cases per profile, including
the generated application's JIT/AOT and relocated installation. Only the test
fixture changed for the remaining case. The targeted rerun rebuilt that test
against the same `void_core.lib` and passed directly and through CTest in both
profiles. The compiler, language server and other tested executable hashes
matched before and after that rerun. The [Windows fixture receipt](evidence/slice5/windows-fixture/rerun.json)
records all commands and hashes. The source packet matches 3,253 working-tree
files byte for byte. Its one corrected test file is separately hashed in the
[fixture addendum](evidence/slice5/windows-fixture/creation.sha256). The guest's
original source tree was restored and verified.

Native qualification covers Linux and Windows 10. macOS, Windows Server 2022,
other Windows versions, Windows sanitizer builds and VS Code on Windows remain
unqualified. Virtual workspace exclusion was verified in the extension declaration
and scope guard.

## Exact source and Git inventory

The [S5 patch](evidence/slice5-source.patch) applies to the accepted S1–S4
working tree captured at entry. Reconstruction applied the patch to the
captured bytes, compared every resulting source file and checked the reverse
patch. [Per-file hashes](evidence/slice5/source-hashes.json) and the
[layered Git inventory](evidence/slice5-inventory.txt) identify every amended
and added path. The index's 68,996 path/mode/blob/stage tuples match both the
starting index and HEAD. The staged diff is empty.

## End-of-task temporary workspace cleanup

After receipts and hashes were preserved, the Windows 10 guest removed its
two verified source-overlay backup directories and two empty S5 test directories.
The host removed six S5 launch screenshots and the generated bytecode for the
native fixture runner. Exact entries are listed in the
[Windows cleanup receipt](evidence/slice5/windows-cleanup-results.json) and the
[host cleanup receipt](evidence/slice5/host-cleanup-results.json).

The guest system drive had **7,140,917,248 bytes free before** and
**7,143,841,792 bytes free after** cleanup, approximately **6.65 GiB** after.
The qualified Release and Debug build trees, binaries, fixture harnesses and
both source packets remain available. The Linux Release, Debug and strict
build trees, starting-byte checkpoint and raw review logs were retained for
reproduction. Cleanup did not modify unrelated files, Git state, reusable caches
or installed toolchains.

Hosted CI remains on strict hold. This handoff stops at Golden Wrench's
independent S5 review.
