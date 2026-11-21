# TestGame enum field defaults and editor recovery

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Bounded editor/compiler correctness correction on accepted G83
`cdebc9351de200feff431712f9f11b87bc9c8469`, 20 November 2026, Linux x86-64.
G84 remains the active feature milestone; no fallible-constructor implementation
or G84 qualification is claimed here. The correction was initially a review
candidate; Boss subsequently authorized the local installation and editor
verification recorded below. No staging, commit, push, hosted CI or publication.

## Incident and identity

The original Code logs record SIGSEGV at **04:02:30.329** and **04:06:34.655**,
after incomplete-expression/unknown-identifier diagnostics in
`TestGame/src/scenes/scene_manager.void`. They also record a `VOID1106` diagnostic
for the custom initializer after a complete enum-valued field default was added.
Original logs, the full source/import snapshot, compiler/server identities,
reproduction inputs and stderr are retained in `/tmp/voidls-testgame-0ja701qf`.
The trace was configured for message names rather than full edit contents;
historical keystrokes are not claimed to have been recovered from that trace.
The replay uses the preserved full source and the reported incomplete states.

Before this work, the default extension command `~/.local/bin/voidls` resolved to
`~/.local/opt/void-g72-7ef5ba6d/bin/voidls`. Both installed executables differed
from the reviewed G83 Release artifacts:

| Tool | Original installed SHA-256 | Reviewed G83 Release SHA-256 |
| --- | --- | --- |
| `voidc` | `ac029c9390a1844911870ac9ff4664c5b1fa3505380e5218e173bf886ba352d3` | `0031342ac40d81be60dfc2c3941e94bae256d4dc7fe63cdef08bfa69fb06277d` |
| `voidls` | `cc699fbd1cb117f03f1ab1d6e7e220a752a6f09a6ef43404f873896639b02d0a` | `35b0c2803152e8d5c9b35a24aa546047c1e200b3370e222b912a6a4cc4418b03` |

Installed `voidc --version` reported development version 0.1.0, LLVM 23.1.4 and
build identity `14c4a0286e21317248ea9b0713851fc968b37c4f2016065a702c7f590c96083f`.
The protocol's `voidls` version is 0.2.0; that shared label does not identify a
source revision. The directory's `g72` label is not treated as revision proof.
The actual editor log confirms its selected executable. The agent and desktop
Code environments did not independently resolve bare `voidc`/`voidls` on PATH;
the extension's installed-location fallback selected the canonical command.

The crash also reproduced with the matching G83 Release server, so the outdated
installation was not the sole cause. Fatal ASan/UBSan gave a null-expression
reference at the original `constant_value` operand dereference
(`compiler.cpp:6652`), reached through `Semantics::field_default` and declaration
default evaluation. It was a frontend recovery defect, before LLVM execution,
not an attribution to the last unknown-identifier diagnostic or editor indexing.
No original core was found in TestGame, the inspected Documents/Code locations,
or the system coredump/crash directories; full sanitizer reproduction stderr was
retained instead. New reproduction children disabled core generation.

## Confirmed defects and correction

1. **A recovery hole was evaluated as a constant.** Partial enum member recovery
   preserves the enum type in an `editor_hole` and removes its operands. Field
   defaults accepted that type and entered the ordinary constant evaluator,
   whose operand fallback dereferenced nonexistent storage. The evaluator now
   propagates the existing `Poison` boundary for such holes. It never invents an
   enum value or allows a placeholder into executable IR.
2. **A valid declared enum default was omitted before managed `init`.** The
   initializer-default predicate covered numeric enums and options but omitted
   tagged enums, results and callable defaults. Those fields require complete
   values, so the prelude and readiness analysis disagreed with the checked
   declaration template. The predicate now covers the same required pure-value
   families already supported by field defaults. Existing preparation writes
   their actual checked values before the body. `state = scene_state.none`
   followed by `init(): pass` is valid; the reported `VOID1106` was incorrect.
3. **Failed default templates must remain poisoned.** Verification exposed a
   related exit-2 path when preparation read a truncated template after a bad or
   recovered default. Initializer preparation now consults the existing default
   resolver before reading template fields. Its cached failure stops preparation
   with normal diagnostics rather than `vector::_M_range_check`.

Presentation analysis now takes its existing early-return path for repaired
trees as well as diagnosed broken bodies. Recovered defaults retain safe typed
presentation without entering ownership or IR contracts. Runtime compilation and
the ordinary initialization, visibility and publication rules remain unchanged.
Fields with no valid default still require initialization; negative tests retain
that `VOID1106` behavior.

The crash reduces to a local enum, a struct field default and a no-op entry:

```void
private enum scene_state:
    none
    add_or_remove
struct scene_manager:
    state: scene_state = scene_state.none
func main():
    pass
```

Changing the default to `scene_state.` reproduces the installed SIGSEGV even
without imports or a custom initializer. Private visibility is not the cause.
The complete declaration without custom init checks successfully on the baseline;
adding the no-op custom init reproduces the separate false-readiness diagnostic.

Bounded research consulted Clang's documented
[Recovery AST](https://clang.llvm.org/docs/InternalsManual.html#recovery-ast)
model for retaining invalid source while keeping recovery nodes out of ordinary
evaluation. This is an established compiler design reference, not an LLVM patch
applied to VOID or evidence that the pinned backend caused this failure. VOID's
existing hole, poison, default resolver and presentation pass supplied the repair.

## Verification

All final builds use the existing LLVM/Clang/LLD **23.1.4** configurations. The
production source was frozen before the final builds and remained identical
through qualification; source/artifact hashes and commands are retained.

| Profile | Focused CTest result | Duration |
| --- | --- | --- |
| Debug | 7/7 | 58.64 seconds |
| Release | 7/7 | 17.62 seconds |
| Fatal ASan/UBSan/leaks | 6/6 registered gates | 59.22 seconds |

The selected gates are `lsp_enum_field_defaults`, `lsp_enum_recovery`,
`lsp_editor_recovery`, `managed_initialization_source`, `enum_review_corrections`,
`nullable_review_corrections`, and `lsp_const_recovery_stack`. The last is already
excluded from sanitizer registration by CMake; that policy was not changed.
Every qualification child nevertheless inherited **1,048,576-byte soft and hard
stack limits**, and the new regression sets those limits explicitly for compiler,
server and executable children. No stack requirement or existing timeout was
raised. The new focused gate has a 180-second budget.

The new regression records **17 CLI expectations and 60 protocol responses per
profile**. It checks the imported TestGame-shaped declaration, tagged/payload and
generic enum defaults, result/option/callable defaults, managed inheritance and
generic owners through JIT/AOT O0/O2. Missing, mistyped and unknown defaults still
diagnose. A single live server handles empty expressions, bare/partial/invalid
enum names, repeated repair and the observed unmatched-list transition. Cold-open
prefix recovery is also checked, without earlier semantic tokens to reuse.

Interactive semantic highlighting preserves unrelated typed bindings, and enum
completion remains available for recoverable prefixes. After valid repair, exact
tokens and normal hover return. An invalid declaration may still lack a fully
validated hover/navigation index: those providers can return no result while the
presentation tree or retained coloring remains available. No stale validated
ownership information is presented as successful analysis.

The exact preserved TestGame snapshot/edit sequence also survived with clean exit
and empty stderr in final Debug/Release protocol replays, and in Release/fatal
sanitizer interactive-protocol replays. These are actual `--project TestGame`
server sessions with open-document overlays; the project's files were not edited
by the reproducer. Existing unrelated project diagnostics are not claimed fixed.
No new real-editor pixel/latency, native Windows/macOS, or full G84 qualification
claim is made.

Fatal settings:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`; `LSAN_OPTIONS` unset.
No suppressions were introduced. The managed initializer source gate exercises
the existing precise roots, construction contexts and readiness/IR proofs.

Reproduce the maintained regression with:

```bash
ctest --test-dir build/llvm23-release -R '^lsp_enum_field_defaults$' --output-on-failure
```

Original logs, signal exits, full sanitizer traces, the smallest reproducer,
pre-correction `VOID1106`, intermediate range-check/provider test failures and
final records are retained. Intermediate failures are not counted as passing
qualification. No unrelated full suite or speculative performance work ran.

## User-authorized canonical command relocation

During investigation the user requested removal of the versioned `bin` dependency.
The existing installation was relocated to the stable `~/.local` prefix:

- `~/.local/bin/voidc` and `~/.local/bin/voidls` are real regular executable files.
- Their original binary hashes and compiler build identity are unchanged.
- Matching runtime/native-resource inventory, stdlib and verified-package files
  were copied coherently. The relocation changed exactly 25 canonical payload
  paths; the full list is in `canonical-relocation.json`.
- `~/.local/opt/void-g72-7ef5ba6d/bin` was moved out of that path after successful
  checks. A verified original installation and previous canonical files remain at
  `~/Documents/Void.Language-archives/voidls-testgame-original-install-0ja701qf`,
  with a complete restoration manifest.
- Version, imported-stdlib check, JIT, native AOT/execution and LSP diagnostics,
  semantic tokens and clean shutdown passed using the canonical commands at 1 MiB.

That first operation was the separately requested layout relocation, **not an
installation of the corrected candidate**. At that point the default commands
still contained the original code. The subsequently authorized installation is
recorded below. No global editor setting was changed by the relocation.

The final PATH check found a pre-existing unmatched `fi` in `~/.profile` that
prevented login setup from reaching its canonical PATH export. That syntax error
was removed, and obsolete VOID build/versioned-prefix PATH additions were removed
from `~/.bashrc`. Both retain the canonical `~/.local/bin` export. `bash -n` passes
for both files; fresh interactive and login shells resolve both commands exactly
to `~/.local/bin`. Earlier processes keep their inherited environments until
restarted or refreshed. Original configuration files and their hashes are retained
in the installation backup's `shell-before-canonical/` directory and
`canonical-shell-path.json`. These two external shell files are additional to the
25 relocated payload paths and are outside the repository inventory.

## Exact repository inventory

Six paths, relative to the accepted G83 baseline:

- `src/frontend/compiler.cpp`
- `CMakeLists.txt`
- `tests/lsp/enum_field_defaults.py`
- `tests/lsp/fixtures/enum_field_defaults/main.void`
- `tests/lsp/fixtures/enum_field_defaults/scene.void`
- `docs/compiler-health/ENUM_FIELD_DEFAULT_RECOVERY.md`

The external installation relocation is recorded separately above and is not a
source-candidate change. The original G83 crash/re-review evidence and the G84
feature-start evidence remain retained. Windows/macOS remain **UNQUALIFIED**.

The selected evidence and these six candidate files are archived in
`~/Documents/Void.Language-archives/voidls-testgame-enum-recovery-2026-11-20-0ja701qf.zip`.
The ZIP embeds original/member SHA-256 values in `_ARCHIVE_MANIFEST.json`; its
adjacent `.index.json` records archive identity and extraction verification.
Original installed binaries remain in the separate restoration backup above.

## Boss-authorized local installation and editor verification

20 November 2026. Boss explicitly authorized installing the corrected tools and
updating/verifying the existing VS Code and portable VSCodium installations.
The six-file candidate and both Release build artifacts matched the frozen
qualification identities before installation. The accepted G83 baseline remains
`cdebc9351de200feff431712f9f11b87bc9c8469`; implementation/test sources were not
changed during this installation. Only this report was extended.

The preserved full TestGame incident source and edit sequence survived another
Release replay before installation and an installed-server replay afterward,
with clean exits and empty stderr at 1 MiB. The maintained regression against the
**installed** compiler/server passed 17 CLI and 60 protocol expectations, including
JIT/AOT O0/O2, invalid inputs, presentation recovery and repair. This checks the
relocated installation's actual resources as well as the build artifacts.

G83 installation smoke passed **9/9** commands at 1 MiB: collection equality and
dictionary-key contracts through uncached JIT and native AOT, 24- and 48-level
empty-list comparisons, and a rejected collection-valued dictionary key. The
existing main fixture exercises custom element equality. The prior Debug 7/7,
Release 7/7 and fatal sanitizer 6/6 results remain applicable to the unchanged
candidate; those qualification suites were not repeated.

### Installed identities and exact payload changes

The two existing regular executables were replaced atomically, then the existing
CMake `compiler` and `tooling` components were installed into **`~/.local`**.
The compiler's install RPATH adjustment is intentional: its installed SHA differs
from the raw build artifact while its build identity remains the same. The
server's installed bytes exactly match the qualified Release artifact.

| Installed tool | Identity |
| --- | --- |
| `~/.local/bin/voidc` | SHA-256 `b96fb7767a88a27be59443199cabcae68ddb4fc436415ad75326f6b26019206b` |
| `voidc` build | `e717ff723f5fa880f3e917c8f3ae828e4d016a459db66202db7bb8babc3ff870` |
| `~/.local/bin/voidls` | SHA-256 `4f3ecaaa1275d3a25028114c34a7a7cfb9971a2faf6c13d742fb3cc7d37554bc` |

`voidc --version` reports development 0.1.0, LLVM **23.1.4**, target
`x86_64-unknown-linux-gnu`. `voidls`'s shared protocol version is not used as source
identity; its exact artifact hash is recorded instead.

Exactly five canonical payload files changed, relative to the relocated original
installation:

- `~/.local/bin/voidc`
- `~/.local/bin/voidls`
- `~/.local/share/void/verified-packages/raylib/README.md`
- `~/.local/share/void/verified-packages/raylib/SOURCE_ORIGIN.txt`
- `~/.local/share/void/verified-packages/raylib/src/raylib.void`

The last three were refreshed by the matching compiler component. Runtime,
stdlib, native dependencies and resource inventories already matched this build;
they were verified/up to date. No versioned toolchain directory or new binary
backup was created. The original crash binaries and restoration evidence remain
in the existing backup.

### Installed extensions and actual editor results

`npm run package` used existing Node **24.19.0** dependencies, passed TypeScript
checks and rebuilt `editors/vscode/dist/void-language-0.1.2.vsix`. The CLI installed
that local VSIX into both normal extension stores:

- VS Code: `~/.vscode/extensions/void-language.void-language-0.1.2`
- Portable VSCodium: `~/.vscode-oss/extensions/void-language.void-language-0.1.2`

The existing portable application is
`~/Documents/Void.Language-G69-evidence/codium/codium`, previously used for G80–G83
qualification; no second VSCodium application was installed. Earlier qualification
used development-extension loading. The current check exercised the installed
extension from its normal store, not the repository development extension.

Both installed bundles and grammars exactly match the newly packaged repository
bytes: `dist/extension.js` SHA-256
`1be8b2a139f0fafd2f868fe839cdc2867df163f126e79e9b4647985274c72b5f`, grammar
SHA-256 `6534694d79c16b6425a4ac58d8dfab7125c714fc92f84d4f2671a27fff7feb30`.
There is no bundled server overriding PATH discovery.

| Actual application | Installed-extension result |
| --- | --- |
| System VS Code **1.144.0** | PASS |
| Existing portable VSCodium **1.143.1** (CLI build 1.143.17232) | PASS |

Each application opened the actual TestGame project in a separate verification
profile using the normal installed extension store. Auto-save was disabled in
those disposable profiles; original project settings were not changed. The
recorded checks confirm:

- Startup discovers `~/.local/bin/voidls --project …/TestGame`; the live process
  bytes match the corrected Release server, and the adjacent/default compiler
  reports the build identity above.
- The original full enum-default declaration has semantic enum roles and no
  erroneous `state` readiness diagnostic. Empty expressions, bare enum names,
  dot/partial member prefixes and repeated repair retain the **same server PID**.
- The actual suggestion widget displays enum members, partial `n` completion
  selects `none`, and unrelated `dt` bindings retain parameter semantic roles.
  Valid repairs clear diagnostics and restore the enum-member role.
- The installed TextMate grammar renders `static` in the same keyword color as
  `func` (`rgb(197, 134, 192)` under the controlled Dark+ theme). This verifies
  rendered G80 syntax support as well as the packaged grammar bytes.
- An actual integrated Bash login terminal in each editor resolves both bare
  commands to `~/.local/bin` and reports the installed hashes/build identity.

The first concurrent GUI attempt failed on window focus and literal-space
matching in rendered text; it is retained as an unsuccessful harness attempt.
Running the windows sequentially and recognizing rendered nonbreaking spaces
resolved those harness issues without product changes.

The original running VS Code window was also refreshed through its built-in
`workbench.action.restartExtensionHost` command. A temporary loopback-only Node
inspector connection invoked the editor-owned restart, preserving renderer buffers
and layout. Read-back confirms the current installed extension, running corrected
server, completion and semantic tokens in that original window. Open-document
content hashes and dirty state matched before and after. The temporary inspector
listeners were closed. The old deleted-inode server process is no longer active.
This follows the established [Node inspector](https://nodejs.org/api/cli.html#--inspect-porthostport)
mechanism and VS Code's normal extension restart; no permanent debugging setting
or helper extension was installed.

All **24 captured TestGame files** match their pre-installation hashes. The user
had already edited the project since the original incident snapshot; both that
older evidence and the current project state were preserved. No source edits,
staging, commits, pushes or publication occurred during installation. Windows/macOS
remain UNQUALIFIED. No work remains for this authorized Linux local update;
G84 feature implementation is outside this correction.

Detailed commands, hashes, install inventory, editor logs/provider observations
and unsuccessful harness records are in `/tmp/void-toolchain-update-uxqb3rji`.
Selected unique records are preserved in
`~/Documents/Void.Language-archives/void-toolchain-update-2026-11-20-uxqb3rji.zip`,
with an embedded SHA-256 manifest and adjacent verified archive index. The prior
crash-evidence ZIP and original installation backup are retained unchanged.
