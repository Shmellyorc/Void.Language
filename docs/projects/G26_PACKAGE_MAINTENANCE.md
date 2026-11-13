# G26: package add, project libraries and tidy

Baseline: `49e419b7985c5e10234d158533d18146753d3308`.
Authorization: Boss approved G26 implementation. No staging, commits, pushes,
package publication or hosted CI are part of this qualification.

## CLI and source layout

```sh
voidc add ALIAS [--project DIR] --path DIR
voidc add ALIAS [--project DIR] --git ORIGIN --rev FULL_SHA [--subdir DIR]
voidc tidy [--project DIR] [--check]
```

The project defaults to `.`; local source paths are relative to that project.
There is no registry lookup. Git origins/pins/subdirectories use existing D3
validation, acquisition, proof and whole-repository bounds.

| Source | Physical location | Compiler identity and ownership |
| --- | --- | --- |
| Ordinary local library | `libs/math/math.void` | `lib/math`; project-owned, editable, no manifest/tidy needed |
| Existing in-project path package | Its existing declared path | `pkg/<alias>`; live editable path, existing D3 lock |
| External local source installed by add | `libs/<alias>/` | Editable copy, `pkg/<alias>`; original external directory is not a live link |
| Installed Git source | `libs/<alias>/` | Immutable `pkg/<alias>`; checked materialized view of D3's verified snapshot |
| Manual Git dependency without installed view | Existing `.void/git-deps` snapshot | Existing supported `pkg/<alias>` workflow; missing view is optional advice |
| Standard library | Toolchain `stdlib/` | Existing separate standard-library domain |

Git views retain the selected package subtree and manifest. `.void-package`
contains the existing D3 proof; it grants no independent authority. The resolver
checks exact view content against that snapshot before compiler admission, and
compiler source bytes still come from verified D3 snapshots. Definitions use
normalized physical installed paths. Transitive packages outside a selected
subtree retain their verified cache locations. No extra package cache, source
index, registry, transport, native loader, Runtime or ABI contract was introduced.

Both `lib/...` and quoted-path imports are denied at an installed package's
boundary. User-directory collisions, incorrect case, aliases, links/reparse
points, nested boundaries, package ID/version conflicts and immutable-content
changes are rejected. Ordinary discovery still excludes `libs/`; unreferenced
libraries are not compiled or recursively discovered.

## Manifest editing and publication

Add infers the source's package ID, exact version and explicit entry, then appends
one dependency table. Existing TOML bytes, comments, unrelated order and LF/CRLF
style are preserved. The resulting manifest goes through the existing parser and
Resolver. Repeated additions retain editable contents; conflicting additions
fail. Existing dependencies must be consistent before add changes the project.
No package installation script is executed. A verified D3 snapshot explicitly
fetched by add may remain cached if later graph validation fails; the manifest,
lock and installed source state roll back.

The existing dependency writer serializes publication. Private directory/file
staging reuses project-creation/D3 helpers, pinned parents, regular-file checks,
exclusive publication and atomic renames. `.void/package-edit/` temporarily saves
prior manifest/lock bytes and exact before/after installed trees. Compiler and
resolver readers reject this gate while an edit is pending. Changes are validated
before the gate is removed. Replaced managed views are atomically moved aside;
they are not recursively deleted during publication.

Exceptions restore prior state. A mutating add/tidy first recovers a process-
interrupted operation. Recovery validates all removals and refuses subsequent
foreign edits, preserving the journal for inspection. The gate itself is renamed
away atomically at commit, so interrupted garbage collection cannot leave a
partly deleted recovery record under its live name. Committed cleanup debris may
remain under `.void/package-done-*` after process termination; it is not source
or compiler authority. This is process-crash recovery, not a claim of power-loss
or hostile concurrent-filesystem transactions.

## Tidy and read-only validation

Tidy uses the actual compiler Parser/SourceGraph for admitted source roots or the
explicit entry and referenced local imports. Its metadata result cannot lower IR
or authorize native code. It validates local entry spelling/boundaries and direct
`pkg/` declarations, retains unused dependencies with advice and never edits or
deletes developer `.void` files. Missing source locators are not guessed.

The existing resolver computes the graph and lock. Legitimate local-source and
manifest changes can refresh that lock; unchanged immutable Git claims cannot be
re-blessed after tampering. An intentional Git pin change requires explicit D3
fetch first. Replacement requires the previous physical view to match its old
verified snapshot. No immutable pin is upgraded automatically.

`tidy --check` reads only, reports required consistency corrections with nonzero
status, and keeps optional missing-view/unused-package advice separate. Tests
compare complete file bytes, modification times and directory membership,
including administrative cache/journal state. An unchanged second tidy performs
no publication. Manual D3 manifests and lock formats remain supported.

## Native source and raylib

Source installation and D4 approval remain separate. Advice identifies native
providers from parsed source declarations and directs the developer to existing
package README/preparation instructions. Adding source does not add native
requirements, prepare binaries or authorize execution. Declared application
requirements and selected catalogs are still verified through D4; execution still
requires `--allow-native`. Missing native catalog selection fails without changes.
Raylib's README, `prepare.py`, catalog and physical native declarations already
provide the necessary prerequisites; no new metadata format is needed.

The raylib fixture now uses add/tidy for its existing physical-source workflow.
Canonical `verified-packages/raylib/` and its [600-entry inventory](../../verified-packages/raylib/inventory.json) are unchanged:
236 direct C ABI functions, 352 ownership/value adapters, ten callback interfaces
and two restricted typed equivalents for variadic declarations. G5 callback
invocation/replacement/retirement and shutdown fixtures remain in qualification.

The previous independent external leak findings remain unsuppressed: 256 bytes
in Gallium after window shutdown, 408 bytes in raylib's WAV music context and 64
bytes in its sound-alias converter. Clean strict headless and bounded callback
ownership checks are separate from those failing full-device paths. G26 does not
reclassify them. See [strict leak classification](G23_G24_IMPLEMENTATION.md#strict-leak-classification).

## Qualification results

All five native profiles passed. Linux used the existing PikaOS builds and
pinned LLVM/Clang/LLD 23.1.4. Windows was actual Windows 10 `10.0.19045`,
in VS 2022's x64 Native Tools environment: VS `17.14.42`, VCTools
`14.44.35207`, SDK `10.0.19041.0`, pinned Clang/LLD `23.1.4`.

| Profile | G26 workflow / five interruption phases | Focused regressions | Final generated-native compatibility | Raylib source / symbols / Git JIT-AOT-D5 | Native editor |
| --- | --- | --- | --- | --- | --- |
| Linux Debug | PASS | 34/34 | 2/2 | PASS | G26 9 events; G23 37 checks |
| Linux Release | PASS | 34/34 | 2/2 | PASS | G26 9 events; G23 37 checks |
| Linux strict ASan/UBSan, leak detection | PASS | 34/34 | 2/2 | PASS | Real LSP consumer completion/hover; GUI hosts separately qualified above |
| Windows 10 Debug | PASS | 18/18 configured native suites | 2/2 | PASS | G26 9 events; G23 37 checks |
| Windows 10 Release | PASS | 18/18 configured native suites | 2/2 | PASS | G26 9 events; G23 37 checks |

The final two targeted tests add `tidy --check` to the existing conventional and
managed native fixtures. They proved and corrected a genuine missing-virtual-
module diagnostic: import inspection now reuses the existing descriptor-generated
source. Conventional checks succeed before native binaries exist; both fixtures
assert native initializers never ran. No native execution permission is granted.
The full focused suites were preserved; these final targeted tests and Windows
maintenance reruns qualify the isolated metadata correction.

Raylib's disposable smart-HTTPS consumer first uses the established manual
fetch/resolve workflow, then `add` adopts the same declaration into `libs/raylib`
without changing its lock. `tidy --check`, source/native verification, normal/O2
JIT, native AOT, D5 verify/relocate/execute, five retained global hooks,
TraceLog/TextFormat equivalents, native guards and real LSP checks pass. This
also caught and fixed the missing-view case for an existing identical Git
addition. Source/cache/provider tampering, changed revisions and stale locks
remain rejected. Every one of the 600 native symbols is independently checked.

Linux headless callback invocation, replacement, checked failures, implicit
shutdown, relocation and compatible reload passed in Debug/Release/strict.
Windows Debug's complete callback/audio fixture also passed during G26; the
unchanged Windows Release device/callback qualification from G24/G25 is retained.
Both Windows final focused suites rerun G5 conventional/managed callbacks and
both Git consumers invoke the retained package hooks.

Real graphics/input results:

| Host / execution | Exit | Rendered frames | Input frames |
| --- | --- | --- | --- |
| Linux Debug JIT | 0 | 50 | 23 |
| Linux Release relocated D5 | 0 | 51 | 22 |
| Windows Debug native AOT | 0 | 55 | 28 |
| Windows Release relocated D5 | 0 | 58 | 28 |

Pixel motion, keyboard input, owning module paths and one-time texture/window
release were checked with the existing platform graphics fixtures. These are
functional device results, not claims that the known strict device leaks vanished.
G25 C/C++ header-import/edit/rejection tests passed in all native profiles;
pinned SQLite 3.53.4 normal/O2 JIT/AOT and failure cleanup passed additionally in
all three Linux profiles. Prior Windows SQLite/native qualification is preserved.

Exact final command logs, XML and receipts are retained at:

```text
/home/shmellyorc/Documents/Void.Language/build/g26/
/home/shmellyorc/Documents/Void.Language/build/g26/handoff/
/home/shmellyorc/VM/g26-20261113/final/
```

`qualify-linux.py` and the native Windows `qualify.py` / `generated-native.py`
coordinators in those evidence directories record the established fixture commands;
they are local qualification orchestration, not new product/test infrastructure.
Native source overlays were restored after every completed/cancelled coordinator.
`handoff/windows-windows-identities.json` retains the actual OS/shell, final
compiler digests, package rows, source/native locks and approved native catalog
file identities. `final-linux-compilers.json`, `linux-identities.json`, copied
locks and per-profile JSON retain the Linux equivalents.

Disposable raylib source/lock identities (these are **not public GitHub commits**):

| Profile | Git commit | Combined source/native lock SHA-256 |
| --- | --- | --- |
| Linux Debug | `ea72af0bdb13dd71a08e0507bb042df74b3213c8` | `617533dbe5835a25b62c5df2b5a63853b384a6bbf1dec8a6ae36d122fcb09a93` |
| Linux Release | `6815a7f3ecc0428f8ea8a03fed934dc65dc59e25` | `77f2f6c82ddbe9d1049a123721f7cc7b62dc71b2723de53a98c66b12f94dd08b` |
| Linux strict | `5ed248c40e8f537bb0a5afc6942c4ba4942a4e17` | `133fd7cc03ca5f13c04e4d964142f8a364a87a7fbef257efa21df4db08dd2973` |
| Windows Debug | `df93bbffa0b0c29edc2ea76c889ac28b463287d0` | `6ab0da9b1b0a8dd5aaa5d1de97cbe73df1d3af88a35abc307fd80f70ff6d1049` |
| Windows Release | `87b6467f8a7f8a590315929e2a9aeb46d13d2e1b` | `5084087245f6bccda454f733601bb7776a2ff7c93ada48465dcf37fd31093333` |

Earlier failures are retained rather than overwritten or represented as passes:

- Installed definition paths initially included `./`; normalizing the managed
  locator corrected the real editor failure.
- The first Windows build found a deleted handle-assignment operation; the
  existing owning handle now receives the native handle directly.
- One initial strict build raced a source rewrite and failed with a compiler
  mmap/parser fault. Final frozen-source builds passed with sanitizers enabled.
- Initial tests overlapped binary relinking, and the G4 hard-link fixture crossed
  filesystems. Frozen binaries and the existing same-filesystem diagnostics
  directory corrected those harness runs. A stale catalog test now uses the
  existing named 128-KiB descriptor limit instead of an obsolete 16-KiB literal;
  no production limit changed.
- The older Debug binary reproduced identical-add's missing installed view; the
  final Git consumer rerun passed after the smallest correction.
- Windows editor assertions reached evidence publication, then Electron refused
  the UNC output host. Writing local evidence and copying it through Python
  corrected the harness; both native editor suites passed in both profiles.
- A cold Windows Debug JIT exceeded the graphics fixture's 30-second window
  discovery interval. Its timeout remains recorded; the independently qualified
  Debug AOT and relocated Release D5 images passed actual graphics/input. A
  Windows console Unicode-print failure was corrected by UTF-8 harness output.
- Conventional and managed generated native modules reproduced a false missing
  source error in tidy. Reusing existing generated source corrected it, with
  final 2/2 native tests on all five profiles and no initializer execution.

The superseded Windows coordinator was interrupted explicitly after its Debug
native results; its `finally` restored the original source. Its incomplete
campaign is not counted as a Release pass. The final completed campaign and
isolated metadata qualification supply the matrix above. Existing VS Code host
logs also retain the archive's missing TextMate-worker warning; the requested
language-server/editor command assertions passed. No sanitizer report or actual
transport/source/package failure was suppressed.

Fixtures:

- `tests/project/maintenance.py`: ordinary local libraries and observed edits;
  local editable copy/live path; exact TOML prefix/CRLF; readonly and idempotence;
  disposable immutable Git, explain/locked, normal/O2 JIT, native AOT and relocated
  D5 verification/execution; tamper and import-bypass rejection; explicit pin
  change/view replacement; optional-view recreation; malformed/conflicting inputs,
  denied native selection, links/reparse paths and rollback.
- Existing `tests/project/creation.cpp`: real process termination before and after
  manifest/lock publication, after displacement of an old view, and before/after
  replacement publication. Read-only gate checks and exact recovery cover five
  interruption phases.
- Existing D3/D4/D5, G23 project, G25 native-import/C/C++/SQLite and G5 suites;
  physical raylib, 600 native symbols, callback and disposable HTTPS consumer
  fixtures. No actual GitHub qualification is claimed by a disposable fixture.
- `maintenanceHost.ts` through the existing native VS Code 1.141.0 runner:
  physical package definition, local/package completion, local-source edits,
  manifest updates, forged/restored lock diagnostics and Check/Package Project.
  The existing G23 editor host also remains in qualification.

## Publication and remaining limits

The standalone public raylib repository is still unpublished and unauthorized.
The old full VOID.Language monorepo exceeds D3's whole-repository limits; those
limits were not changed. Boss must authorize publication of the canonical
package's standalone Git subtree. Only its actual immutable public commit can
qualify a public GitHub add/fetch/resolve/locked/JIT/AOT/D5/editor consumer. The
existing [standalone publication proposal](G24_PHYSICAL_SOURCE_IMPLEMENTATION.md)
remains applicable.

External local sources are copied, not linked. Local transitive paths escaping
that installed tree still fail the established project enclosure. Git view edits
are never merged or repaired silently. Recovery refuses foreign changes made
after interruption. Add does not infer source origins, Git upgrades or native
approval; tidy is import/graph maintenance rather than a full type checker.
Existing D3 transitive acquisition authorization still applies; explicitly add a
required external pin as a direct source first, or use the existing manual
`deps fetch --allow-git` workflow. Physical source/journal inventories are bounded
to 64 levels, 32768 entries and 256 MiB, with D3's stricter per-snapshot limits
unchanged.

## Changed files and Git checks

Exactly 23 changed files, all unstaged:

```text
CMakeLists.txt
docs/CLI.md
docs/LOCAL_LIBRARIES.md
docs/projects/G26_PACKAGE_MAINTENANCE.md
docs/roadmap/MILESTONE_REGISTER.md
editors/vscode/src/test/maintenanceHost.ts
editors/vscode/src/test/runRaylibHost.ts
include/void/frontend.hpp
include/void/project_resolution.hpp
src/frontend/compiler.cpp
src/frontend/project.cpp
src/frontend/project_creation.cpp
src/frontend/project_files.hpp
src/frontend/project_resolution.cpp
src/frontend/project_selection.inc
src/main.cpp
tests/ffi/conventional_process.py
tests/project/creation.cpp
tests/project/maintenance.py
tests/project/managed_native.py
tests/project/manifest.cpp
tests/project/raylib_physical.py
tests/project/verified_raylib.py
```

`git diff --check` passes (exit 0). `git diff --cached` is empty.
HEAD remains the baseline SHA. `git diff HEAD -- verified-packages/raylib` is
empty. No source files were staged, committed or pushed; no repository/package
was published and no hosted CI was dispatched. Final Git status:

```text
 M CMakeLists.txt
 M docs/CLI.md
 M docs/LOCAL_LIBRARIES.md
 M docs/roadmap/MILESTONE_REGISTER.md
 M editors/vscode/src/test/runRaylibHost.ts
 M include/void/frontend.hpp
 M include/void/project_resolution.hpp
 M src/frontend/compiler.cpp
 M src/frontend/project.cpp
 M src/frontend/project_creation.cpp
 M src/frontend/project_files.hpp
 M src/frontend/project_resolution.cpp
 M src/frontend/project_selection.inc
 M src/main.cpp
 M tests/ffi/conventional_process.py
 M tests/project/creation.cpp
 M tests/project/managed_native.py
 M tests/project/manifest.cpp
 M tests/project/raylib_physical.py
 M tests/project/verified_raylib.py
?? docs/projects/G26_PACKAGE_MAINTENANCE.md
?? editors/vscode/src/test/maintenanceHost.ts
?? tests/project/maintenance.py
```

