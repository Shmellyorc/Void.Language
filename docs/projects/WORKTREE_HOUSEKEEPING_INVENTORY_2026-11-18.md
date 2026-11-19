# Worktree housekeeping — inventory and approved execution

18 November 2026 (America/Vancouver). Snapshot UTC: `2026-11-19T04:41:53.033972+00:00`.

The original first-pass inventory follows unchanged. Boss subsequently approved
its 28 moves and 14 retirements; see the [execution closeout](#execution-closeout)
for the resulting locations and verification. Statements about pending approval
and untouched worktrees below describe the original inventory snapshot.

**Proposal only. No existing worktree has been moved, removed, pruned, locked,
unlocked or deleted. No branch/ref, index, compiler, installed prefix, recovery
material or existing archive was changed. No staging, commit or push occurred.**

## Snapshot and classifications

- 44 registered Git worktrees: one main checkout and 43 linked checkouts.
- Active **2**; ON HOLD **1** (G69); Completed **26**; Uncertain **15**.
- Live `origin/main` was verified as `e96b827287cfafaf7c6ff7a399c5e15899284b39`.
  Every registered HEAD is an ancestor of that exact remote tip. There are
  **zero commits outside remote-main history** at these tips, including all
  eight detached HEADs. The 35 topic branches have no configured upstream;
  their commits are nevertheless already reachable from remote main.
- All registrations exist; none is locked/prunable, and no gitlinks/submodules
  were found. The proposed destination root does not yet exist. Every proposed
  source is on the same filesystem as its proposed destination parent.
- An open `bash` was observed in G70 (PID 3706031); the main checkout also has
  active app/server processes. No other linked checkout was referenced by the
  inspected process working directories/absolute path arguments. Recent app
  summaries did not establish ownership of the other linked checkouts. This
  snapshot does not prove that their owners will remain inactive.

The proposal is **28 retained-worktree moves**, **14 clean retirements**, and
**2 in-place retentions**. Twelve Completed trees are in the move group because
they still contain dirty or ignored payload needing preservation/disposition.
They are not proposed for deletion in this batch. Uncertain means retain and
establish ownership; it does not create a new milestone HOLD.

The canonical main checkout remains at `~/Documents/Void.Language/`; the standard
root applies to linked worktrees. Git cannot move the main checkout with
`git worktree move`. The registered G50 recovery tree remains in place.

## All registered worktrees

Source names below are exact immediate children of `/home/shmellyorc/Documents/`.
`T/U/I` means tracked-change paths, untracked files, and ignored file/symlink
entries. Ignored empty directories are inspected separately and are not counted
in I. Main counts precede the four authorized housekeeping documentation edits.
All unpublished-commit counts are zero. Branches shown without `refs/heads/`.

| Worktree | Branch / detached | HEAD | T/U/I | Class | Proposal |
| --- | --- | --- | ---: | --- | --- |
| `Void.Language` | `main` | `e96b827287cf` | 0/0/824188 | Active | Retain here |
| `Void.Language-G37-G38-repair` | `codex/g37-g38-git-repair` | `d1fc214f1771` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G38` | `detached` | `1e606f8e5067` | 4/8/0 | Completed | Move; retain all contents |
| `Void.Language-G39` | `codex/g39-managed-lifetimes` | `793de66cb8cb` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G40` | `codex/g40-root-state` | `9f00377236a1` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G41` | `codex/g41-root-store-precision` | `f46d53746221` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G42` | `codex/g42-escape-analysis-scaling` | `602289067033` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G43` | `codex/g43-source-ir-validation-scaling` | `895ae7b29193` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G44` | `codex/g44-llvm-jit-scaling` | `894e2e8d9930` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G45` | `codex/g45-native-codegen-scaling` | `894e2e8d9930` | 1/1/0 | Uncertain | Move; retain all contents |
| `Void.Language-G45U` | `codex/g45u-machinesinking-proof` | `894e2e8d9930` | 1/2/0 | Uncertain | Move; retain all contents |
| `Void.Language-G45V` | `codex/g45v-host-code` | `894e2e8d9930` | 1/3/0 | Uncertain | Move; retain all contents |
| `Void.Language-G46` | `codex/g46-text-unicode-json` | `894e2e8d9930` | 5/4/0 | Uncertain | Move; retain all contents |
| `Void.Language-G46F` | `codex/g46f-utf8-decode` | `894e2e8d9930` | 5/6/0 | Uncertain | Move; retain all contents |
| `Void.Language-G47` | `codex/g47-sorting-bounds` | `894e2e8d9930` | 3/7/0 | Uncertain | Move; retain all contents |
| `Void.Language-G47AOT` | `codex/g47-aot-poll` | `1831364f0ca4` | 0/0/971 | Completed | Move; retain all contents |
| `Void.Language-G48` | `codex/g48-dictionary-collections` | `4790a7a52ecf` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G49` | `codex/g49-dictionary-probe-reuse` | `4790a7a52ecf` | 3/1/0 | Uncertain | Move; retain all contents |
| `Void.Language-G50` | `detached` | `4790a7a52ecf` | 4/2/0 | Uncertain | Move; retain all contents |
| `Void.Language-G50-Recovery` | `detached` | `4790a7a52ecf` | 4/2/0 | Uncertain | Retain here |
| `Void.Language-G51` | `codex/g51-text-construction` | `a32a5ff3b7ce` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G52` | `codex/g52-private-list-indexed-reads` | `771fbfc56b5d` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G53` | `detached` | `771fbfc56b5d` | 5/1/1 | Uncertain | Move; retain all contents |
| `Void.Language-G54` | `codex/g54-file200-runtime-redundancy` | `808d4dce8564` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G55` | `detached` | `808d4dce8564` | 4/1/1 | Uncertain | Move; retain all contents |
| `Void.Language-G56` | `detached` | `808d4dce8564` | 1/2/0 | Uncertain | Move; retain all contents |
| `Void.Language-G57` | `detached` | `808d4dce8564` | 3/1/0 | Uncertain | Move; retain all contents |
| `Void.Language-G58` | `detached` | `808d4dce8564` | 1/1/0 | Uncertain | Move; retain all contents |
| `Void.Language-G59` | `codex/g59-native-package-workflow` | `1c3cfc4e9bd0` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G60` | `codex/g60-warm-project-startup` | `dc93b675ed44` | 0/23/3 | Uncertain | Move; retain all contents |
| `Void.Language-G61` | `codex/g61-simple-package-workflow` | `7a18607ae68f` | 0/0/1 | Completed | Move; retain all contents |
| `Void.Language-G62` | `codex/g62-custom-project-templates` | `9b93d0d839d7` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G63` | `codex/g63-durable-native-approval` | `27b8ae4e9368` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G64` | `codex/g64-vscode-launch-workflow` | `2e10af44aec6` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G65` | `codex/g65-editor-native-approval-reuse` | `a1dd73d8cbab` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G66` | `codex/g66-vscode-project-creation` | `890494b032f2` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G67` | `codex/g67-vscode-library-management` | `6e1035d06e10` | 0/0/76 | Completed | Move; retain all contents |
| `Void.Language-G68` | `codex/g68-raylib-community-package` | `92432510ebfe` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G69` | `codex/g69-void-file-icons` | `92432510ebfe` | 3/3/9353 | ON HOLD | Move; retain all contents |
| `Void.Language-G70` | `codex/g70-local-launch-readiness` | `0ef6baeb130f` | 0/0/9353 | Active | Move; retain all contents |
| `Void.Language-G71` | `codex/g71-linux-release-abi` | `01019ced83c3` | 0/0/0 | Completed | Retire after approval |
| `Void.Language-G72` | `codex/g72-editor-reliability` | `7ef5ba6d8354` | 0/0/9483 | Completed | Move; retain all contents |
| `Void.Language-G73` | `codex/g73-vscode-extension-icon` | `3f875c14b420` | 0/0/9356 | Completed | Move; retain all contents |
| `Void.Language-RaylibDocs` | `codex/raylib-public-api-docs` | `fdcbeadbdd8a` | 0/0/7 | Completed | Move; retain all contents |

## Exact proposed moves — approval required

Create only the approved parent directory during the operation phase, then use
`git worktree move OLD NEW`. Destinations are distinct and currently absent.
Each move preserves its current branch/detached HEAD, dirty state, untracked
files and ignored payload; compare the pre/post manifests. Do not clear G69's
HOLD. Coordinate the active G70 shell before its move; do not kill it.

| Existing absolute path | Proposed absolute destination | Class |
| --- | --- | --- |
| `/home/shmellyorc/Documents/Void.Language-G38` | `/home/shmellyorc/Documents/Void.Language-worktrees/g38-automatic-loop-optimization` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G39` | `/home/shmellyorc/Documents/Void.Language-worktrees/g39-managed-lifetimes` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G45` | `/home/shmellyorc/Documents/Void.Language-worktrees/g45-native-codegen-scaling` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G45U` | `/home/shmellyorc/Documents/Void.Language-worktrees/g45u-machinesinking-proof` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G45V` | `/home/shmellyorc/Documents/Void.Language-worktrees/g45v-host-code` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G46` | `/home/shmellyorc/Documents/Void.Language-worktrees/g46-text-unicode-json` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G46F` | `/home/shmellyorc/Documents/Void.Language-worktrees/g46f-utf8-decode` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G47` | `/home/shmellyorc/Documents/Void.Language-worktrees/g47-sorting-bounds` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G47AOT` | `/home/shmellyorc/Documents/Void.Language-worktrees/g47-aot-poll` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G48` | `/home/shmellyorc/Documents/Void.Language-worktrees/g48-dictionary-collections` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G49` | `/home/shmellyorc/Documents/Void.Language-worktrees/g49-dictionary-probe-reuse` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G50` | `/home/shmellyorc/Documents/Void.Language-worktrees/g50-json-text-construction` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G52` | `/home/shmellyorc/Documents/Void.Language-worktrees/g52-private-list-indexed-reads` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G53` | `/home/shmellyorc/Documents/Void.Language-worktrees/g53-list-write-performance` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G54` | `/home/shmellyorc/Documents/Void.Language-worktrees/g54-file200-runtime-redundancy` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G55` | `/home/shmellyorc/Documents/Void.Language-worktrees/g55-buffer-validation-reuse` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G56` | `/home/shmellyorc/Documents/Void.Language-worktrees/g56-native-code-layout-stability` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G57` | `/home/shmellyorc/Documents/Void.Language-worktrees/g57-text-join-performance` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G58` | `/home/shmellyorc/Documents/Void.Language-worktrees/g58-jit-code-layout-stability` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G59` | `/home/shmellyorc/Documents/Void.Language-worktrees/g59-native-package-workflow` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G60` | `/home/shmellyorc/Documents/Void.Language-worktrees/g60-warm-project-startup` | Uncertain |
| `/home/shmellyorc/Documents/Void.Language-G61` | `/home/shmellyorc/Documents/Void.Language-worktrees/g61-simple-package-workflow` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G67` | `/home/shmellyorc/Documents/Void.Language-worktrees/g67-vscode-library-management` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G69` | `/home/shmellyorc/Documents/Void.Language-worktrees/g69-void-file-icons` | ON HOLD |
| `/home/shmellyorc/Documents/Void.Language-G70` | `/home/shmellyorc/Documents/Void.Language-worktrees/g70-local-launch-readiness` | Active |
| `/home/shmellyorc/Documents/Void.Language-G72` | `/home/shmellyorc/Documents/Void.Language-worktrees/g72-editor-reliability` | Completed |
| `/home/shmellyorc/Documents/Void.Language-G73` | `/home/shmellyorc/Documents/Void.Language-worktrees/g73-vscode-extension-icon` | Completed |
| `/home/shmellyorc/Documents/Void.Language-RaylibDocs` | `/home/shmellyorc/Documents/Void.Language-worktrees/raylib-public-api-docs` | Completed |

## Exact proposed retirements — approval required

These 14 worktrees have clean tracked/untracked status, zero ignored file/link
entries, integrated HEADs, no gitlinks, and no observed owning process. Any
listed `build/` directory is empty. Recheck immediately before an approved
normal `git worktree remove PATH`; preserve local branch refs. Do not use force.

| Absolute path | Retained branch ref | HEAD |
| --- | --- | --- |
| `/home/shmellyorc/Documents/Void.Language-G37-G38-repair` | `refs/heads/codex/g37-g38-git-repair` | `d1fc214f17715d464af5fc2d6cb4177258f9eb8f` |
| `/home/shmellyorc/Documents/Void.Language-G40` | `refs/heads/codex/g40-root-state` | `9f00377236a1080ec4ebdc6252f3721843dd2f94` |
| `/home/shmellyorc/Documents/Void.Language-G41` | `refs/heads/codex/g41-root-store-precision` | `f46d53746221ca58da428e5b73188314d3a5b11a` |
| `/home/shmellyorc/Documents/Void.Language-G42` | `refs/heads/codex/g42-escape-analysis-scaling` | `6022890670333cdd5e38a20c84b8c77d0108f8cd` |
| `/home/shmellyorc/Documents/Void.Language-G43` | `refs/heads/codex/g43-source-ir-validation-scaling` | `895ae7b291935f2658fc40731ba89a2aed4ebb55` |
| `/home/shmellyorc/Documents/Void.Language-G44` | `refs/heads/codex/g44-llvm-jit-scaling` | `894e2e8d9930028f86737092d3ac74010676f389` |
| `/home/shmellyorc/Documents/Void.Language-G51` | `refs/heads/codex/g51-text-construction` | `a32a5ff3b7ce6c6ea7d56ad26aba578806fa1cac` |
| `/home/shmellyorc/Documents/Void.Language-G62` | `refs/heads/codex/g62-custom-project-templates` | `9b93d0d839d7b362ff840731896142472138f39e` |
| `/home/shmellyorc/Documents/Void.Language-G63` | `refs/heads/codex/g63-durable-native-approval` | `27b8ae4e9368c8a7c046fd05c07ff0815503a2f3` |
| `/home/shmellyorc/Documents/Void.Language-G64` | `refs/heads/codex/g64-vscode-launch-workflow` | `2e10af44aec61c6b9ff5608c8e406593398840cb` |
| `/home/shmellyorc/Documents/Void.Language-G65` | `refs/heads/codex/g65-editor-native-approval-reuse` | `a1dd73d8cbabf92522c74ec933aec2c975667a25` |
| `/home/shmellyorc/Documents/Void.Language-G66` | `refs/heads/codex/g66-vscode-project-creation` | `890494b032f23da1b451ba98db5f739384480315` |
| `/home/shmellyorc/Documents/Void.Language-G68` | `refs/heads/codex/g68-raylib-community-package` | `92432510ebfe0844aae4e6285b8c4eff579ad137` |
| `/home/shmellyorc/Documents/Void.Language-G71` | `refs/heads/codex/g71-linux-release-abi` | `01019ced83c334233b4a593a4d05b198ebf8ec4d` |

## Retain in place

| Absolute path | Reason |
| --- | --- |
| `/home/shmellyorc/Documents/Void.Language` | Main integration checkout; active app/compiler-server processes. Git worktree move cannot move the main checkout. |
| `/home/shmellyorc/Documents/Void.Language-G50-Recovery` | Registered Git worktree also serving as recovery material; ownership/disposition not established. Leave it in place. |

## Local contents that must survive

**16 worktrees contain 57 changed/untracked path-version observations (50
distinct raw Git blob contents) not found in the verified remote history.**
Repeated versions across worktrees are counted separately in the 57. Raw
whole-file mismatch does not prove that a semantic change is absent from later
work; it establishes that exact bytes have not been shown preserved in Git.
These trees are retained, not retired. The recovery checkout is not relocated.

| Worktree | Local versions not matched in remote history |
| --- | --- |
| `Void.Language-G45` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md` |
| `Void.Language-G45U` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md` |
| `Void.Language-G45V` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md` |
| `Void.Language-G46` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `include/void/runtime.hpp`, `src/runtime/array.cpp`, `src/runtime/coordination.cpp`, `tests/stage8/sprint3.hpp` |
| `Void.Language-G46F` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `include/void/runtime.hpp`, `src/runtime/array.cpp`, `src/runtime/coordination.cpp`, `tests/stage8/sprint3.hpp` |
| `Void.Language-G47` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md` |
| `Void.Language-G49` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `src/runtime/dictionary.cpp`, `tests/dictionaries/tests.cpp` |
| `Void.Language-G50` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `stdlib/json.void`, `tests/json/metering.stdout`, `tests/json/metering.void`, `docs/compiler-health/G50_JSON_TEXT_CONSTRUCTION.md` |
| `Void.Language-G50-Recovery` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `stdlib/json.void`, `tests/json/metering.stdout`, `tests/json/metering.void` |
| `Void.Language-G53` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `include/void/runtime.hpp`, `src/runtime/array.cpp`, `src/runtime/list.cpp`, `tests/growable_arrays/tests.cpp`, `docs/compiler-health/G53_LIST_WRITE_PERFORMANCE.md` |
| `Void.Language-G55` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `include/void/runtime.hpp`, `src/runtime/buffer.cpp`, `tests/runtime/buffers.cpp`, `docs/compiler-health/G55_BUFFER_VALIDATION_REUSE.md` |
| `Void.Language-G56` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `docs/compiler-health/G56_NATIVE_CODE_LAYOUT_STABILITY.md`, `tools/bench/bolt_layout_experiment.py` |
| `Void.Language-G57` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `stdlib/text.void`, `tests/text_encoding/text.void`, `docs/compiler-health/G57_TEXT_JOIN_PERFORMANCE.md` |
| `Void.Language-G58` | `docs/compiler-health/G33_PERFORMANCE_IMPLEMENTATION_LEDGER.md`, `docs/compiler-health/G58_JIT_CODE_LAYOUT_STABILITY.md` |
| `Void.Language-G60` | `libs/raylib/.void/native/raylib-linux.descriptor`, `libs/raylib/.void/native/void.native.toml`, `libs/raylib/SOURCE_ORIGIN.txt`, `void.lock`, `void.project.toml` |
| `Void.Language-G69` | `editors/vscode/.vscodeignore`, `editors/vscode/README.md`, `editors/vscode/package.json`, `docs/projects/G69_VOID_FILE_ICONS.md`, `editors/vscode/icons/void-dark.png`, `editors/vscode/icons/void-light.png` |

G38 has four tracked modifications and eight untracked files. All twelve exact
content blobs are reachable from remote-main history, but its working patch,
paths and index context are still retained; it is moved rather than cleaned
or retired. G50 and G50-Recovery are not assumed interchangeable.

Ignored files are not covered by Git commit reachability. In particular:

- G47AOT retains 971 ignored build entries across poll-release, poll-strict and
  poll-tsan. Their evidence value was not disposed of by this inventory.
- G60 retains its untracked root manifest/lock, private Raylib descriptors/origin
  metadata and two ignored native libraries. These are local fixtures, not
  evidence that a new published package exists.
- G69 retains all six unmatched edits/assets and 9,353 ignored extension entries.
- G67/G70/G72/G73 retain generated extension outputs/packages; G72/G73 also have
  phase-11 verification builds. A clean status is not permission to discard them.
- G52/G53/G54/G55/G59/G60/G61 contain an ignored HTTP-download symlink pointing
  into the main checkout's `.toolchains/http/downloads`. Preserve it as a link;
  never traverse or delete the shared target. Moving these links keeps their
  absolute targets unchanged.
- RaylibDocs is a real registered worktree on `codex/raylib-public-api-docs`,
  clean at G74, with seven ignored bytecode cache files and an empty toolchain
  directory. It is retained/moved pending payload/ownership disposition.
- The main checkout contains 824,188 ignored file/link entries. Its builds,
  SDKs and caches are outside any retirement proposal.

Inventory was limited to Git-registered paths. Unregistered evidence directories,
archives, standalone recovery copies, builds outside these worktrees and personal
projects were not treated as cleanup candidates or modified.

## Method, evidence and limits

Read-only Git commands used `GIT_OPTIONAL_LOCKS=0`: `worktree list --porcelain -z`,
`status --porcelain=v1 -z --untracked-files=all`, `ls-files --others --ignored`
with and without `--directory`, branch/upstream inspection, `merge-base`,
`log REMOTE..HEAD`, and remote-reachable object enumeration. Live refs were read
with `ls-remote`; no fetch updated shared remote-tracking refs. Every observed
dirty/untracked file was hashed without writing Git objects. Symlinks were
recorded as links. Complete ignored path lists, directory details, file hashes,
raw status, full HEADs and the machine-readable proposal are retained under
`/tmp/void-worktree-inventory-yfkzk_6b`.

Ignored payload was enumerated and its roots inspected; it was not universally
content-hashed, backed up or declared disposable. The comparison does not prove
semantic equivalence, intent, historical chat ownership or absence of external
references. Recheck registrations, statuses, ownership, remote state and manifests
at operation time. No compiler tests were needed or run.

The permanent procedure is [worktree management](../WORKTREE_MANAGEMENT.md),
based on the [official Git manual](https://git-scm.com/docs/git-worktree).
Git's [normal removal implementation](https://github.com/git/git/blob/v2.53.0/builtin/worktree.c#L1213-L1318)
checks ordinary status before deleting the directory, so ignored contents need
their own disposition even without a force flag.

## Approval boundary

Boss approval is required for the 28 exact moves and 14 exact retirements above.
Approval of those operations does not authorize deleting any retained local or
ignored payload, moving G50-Recovery, relocating the main checkout, removing
branch refs, changing shared SDKs, pruning metadata, staging, committing or
pushing these documentation changes. If a pre-operation check differs, retain
that affected worktree and report the difference; continue other approved safe
operations. G69 stays ON HOLD; G76 remains queued.

## Execution closeout

Verified UTC: `2026-11-19T04:57:43.445117+00:00` (18 November 2026, America/Vancouver).

Boss explicitly approved the original 28 moves, 14 clean retirements and two
in-place retentions. All approved operations completed using ordinary
`git worktree move` / `git worktree remove`, without force or metadata pruning.
**28 moved; 14 retired; 2 retained in place; zero worktree exceptions.**

- Every destination in **Exact proposed moves** above is now registered at
  that exact path; its old source is absent. The destination root contains
  exactly 28 linked worktrees and no extra housekeeping payload.
- Every source in **Exact proposed retirements** is absent and unregistered.
  All 14 named branch refs still resolve to their original full HEADs.
- Main and G50-Recovery remain at their original locations. The recovery
  tree passed the same filesystem/state comparison without relocation.
- Boss confirmed G70 terminal coordination. The original bash PID 3706031
  had exited; no process cwd/absolute-argument references to G70 were observed
  at its operation. No shell was killed or injected into by housekeeping.
- G69 remains ON HOLD at its approved new location with all six local
  edits/assets and ignored extension contents retained. G76 was not started.

### Preservation and Git checks

- Preflight registration, HEADs, branches, statuses, ignored-file lists and
  all 116 changed/untracked file-version hashes matched the approved snapshot.
  The live remote remained `e96b827287cfafaf7c6ff7a399c5e15899284b39`.
- All **57 unmatched local path-versions across 16 worktrees** survived,
  including those in G50-Recovery. SHA-256 checks also covered the other
  changed/untracked files: **116 local file versions verified in total**.
- All **1,895,041 relocated filesystem entries** matched
  before/after device/inode, mode, size, modification time, owner/group and
  symlink-target records. Traversal did not follow symlinks. Full-file hashing
  was limited to changed/untracked versions; this is not a claim that every
  tracked or ignored file was independently content-hashed.
- All 29 retained linked checkouts preserve HEAD, branch/detached state,
  index checksum, binary staged/unstaged patches, status and ignored manifests.
  Their `.git` pointers resolve to the main repository; Git updated the moved
  registrations and pointer files normally. No linked status changed.
- The 28 moved trees retain 44 tracked-change paths, 66 untracked files and
  38,610 ignored file/link entries. Recovery retains its four tracked changes
  and two untracked files. All branch/tag refs are unchanged, including retired
  branches. The final registration has no locked or prunable entries.
- No source, compiler, installed tool, shared-download target, pre-existing
  archive, recovery contents or unrelated project was changed. No compiler
  tests, hosted CI, installations, staging, commits or pushes were performed.

Final classification: Active 2, ON HOLD 1,
Completed 12, Uncertain 15: **30 registered worktrees**.
Completed/Uncertain payloads remain preserved pending their own disposition;
relocation does not certify that unfinished changes are merged or obsolete.

### Final registered inventory

| Absolute path | HEAD | Class | T/U/I |
| --- | --- | --- | ---: |
| `/home/shmellyorc/Documents/Void.Language` | `e96b827287cf` | Active | 2/2/824188* |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g38-automatic-loop-optimization` | `1e606f8e5067` | Completed | 4/8/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g39-managed-lifetimes` | `793de66cb8cb` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g45-native-codegen-scaling` | `894e2e8d9930` | Uncertain | 1/1/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g45u-machinesinking-proof` | `894e2e8d9930` | Uncertain | 1/2/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g45v-host-code` | `894e2e8d9930` | Uncertain | 1/3/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g46-text-unicode-json` | `894e2e8d9930` | Uncertain | 5/4/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g46f-utf8-decode` | `894e2e8d9930` | Uncertain | 5/6/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g47-sorting-bounds` | `894e2e8d9930` | Uncertain | 3/7/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g47-aot-poll` | `1831364f0ca4` | Completed | 0/0/971 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g48-dictionary-collections` | `4790a7a52ecf` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g49-dictionary-probe-reuse` | `4790a7a52ecf` | Uncertain | 3/1/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g50-json-text-construction` | `4790a7a52ecf` | Uncertain | 4/2/0 |
| `/home/shmellyorc/Documents/Void.Language-G50-Recovery` | `4790a7a52ecf` | Uncertain | 4/2/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g52-private-list-indexed-reads` | `771fbfc56b5d` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g53-list-write-performance` | `771fbfc56b5d` | Uncertain | 5/1/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g54-file200-runtime-redundancy` | `808d4dce8564` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g55-buffer-validation-reuse` | `808d4dce8564` | Uncertain | 4/1/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g56-native-code-layout-stability` | `808d4dce8564` | Uncertain | 1/2/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g57-text-join-performance` | `808d4dce8564` | Uncertain | 3/1/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g58-jit-code-layout-stability` | `808d4dce8564` | Uncertain | 1/1/0 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g59-native-package-workflow` | `1c3cfc4e9bd0` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g60-warm-project-startup` | `dc93b675ed44` | Uncertain | 0/23/3 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g61-simple-package-workflow` | `7a18607ae68f` | Completed | 0/0/1 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g67-vscode-library-management` | `6e1035d06e10` | Completed | 0/0/76 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g69-void-file-icons` | `92432510ebfe` | ON HOLD | 3/3/9353 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g70-local-launch-readiness` | `0ef6baeb130f` | Active | 0/0/9353 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g72-editor-reliability` | `7ef5ba6d8354` | Completed | 0/0/9483 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/g73-vscode-extension-icon` | `3f875c14b420` | Completed | 0/0/9356 |
| `/home/shmellyorc/Documents/Void.Language-worktrees/raylib-public-api-docs` | `fdcbeadbdd8a` | Completed | 0/0/7 |

*Main has the two modified and two new authorized housekeeping Markdown files.
Its ignored count is the original snapshot; main SDK/build contents were not
re-enumerated or cleaned during execution.

### Records, limitations and review boundary

Exact documentation inventory (all uncommitted):

- `docs/WORKTREE_MANAGEMENT.md` — permanent procedure and adoption link.
- `docs/projects/WORKTREE_HOUSEKEEPING_INVENTORY_2026-11-18.md` — original
  approval inventory plus this execution result and final registration.
- `docs/README.md` — policy navigation link.
- `docs/WRENCH_CLANKER.md` — concise policy reference; workflow reform retained.

Raw pre/post manifests, binary patches, ref/registration snapshots, per-operation
results and final verification remain in `/tmp/void-worktree-housekeeping-8bzstu30`.
The earlier inventory evidence remains at its original `/tmp` location.
These task-owned temporary records are verification evidence, not backups of
worktree contents or a permanent archive. The durable result is the preserved
worktrees, retained Git refs and this documentation.

An initial scratch-helper preflight failed to create its own parent evidence
directories. No worktree operations had run. The helper was corrected and the
full preflight passed; that initial error is retained as
`preflight-initial-helper-error.json` in the execution evidence.

Reopen retained workspaces from the new locations. Historical logs and
configured build paths were intentionally preserved; an old configured build
may need a fresh configuration before future use. No build qualification is
claimed from filesystem checks. This execution was on Linux only.

`git diff --check` and whitespace/local-link checks for both new Markdown
documents passed. Final staging/commit/push remain unauthorized pending Boss
review of this inventory and closeout. No further cleanup is implied.
