# Worktree management

Use `~/Documents/Void.Language-worktrees/` for future **linked Git worktrees**.
Keep the main integration checkout at `~/Documents/Void.Language/`. Git's
`worktree move` does not move the main checkout. Evidence, archives, recovery
copies, build directories and personal projects are separate resources; a
similar directory name does not make them worktrees.

Use `git worktree list --porcelain` as the starting inventory. The
[18 November 2026 inventory and execution record](projects/WORKTREE_HOUSEKEEPING_INVENTORY_2026-11-18.md)
documents initial adoption, including the original read-only snapshot and the
[approved execution results](projects/WORKTREE_HOUSEKEEPING_INVENTORY_2026-11-18.md#execution-closeout).
Commands below describe the permanent procedure; each assignment's current
authorization determines which operations may run.

## Names and ownership

- Name directories by milestone and purpose, such as `gNN-short-purpose`, or
  `raylib-public-api-docs` for documentation work. Avoid ambiguous names such as
  `new`, `final2` or an unexplained recovery suffix.
- Use a `codex/` topic branch, normally matching the directory's purpose. Keep
  one owner/chat and one purpose per branch. Record the owner, path, branch or
  detached HEAD, accepted remote/base commit and current disposition in the
  milestone record or inventory. Branch names and old commit authors alone do
  not establish current ownership.
- Keep `main` in the integration checkout. Do not force the same branch into
  multiple worktrees. Label detached experimental work clearly and preserve any
  new commits through an appropriate named ref or verified bundle before
  retirement; a reflog alone is not permanent preservation.

## Create

Within an authorized engineering task, choose a fresh destination and branch:

```sh
repo="$HOME/Documents/Void.Language"
worktree_root="$HOME/Documents/Void.Language-worktrees"
mkdir -p "$worktree_root"
git -C "$repo" fetch origin
git -C "$repo" worktree add -b codex/topic-short-name \
    "$worktree_root/topic-short-name" origin/main
```

For an existing unoccupied branch, use `git worktree add DESTINATION BRANCH`.
Record its owner and accepted base before starting work. Creation does not
authorize staging, committing, pushing, installation or publication beyond the
current assignment.

## Inventory before relocation or retirement

Capture the registered path, HEAD, branch/upstream, lock/prunable state and
submodules/gitlinks. Check staged and unstaged changes, untracked files and
ignored files separately:

```sh
git --no-optional-locks -C "$repo" worktree list --porcelain
git --no-optional-locks -C "$old" status --porcelain=v1 --untracked-files=all
git -C "$old" diff --binary
git -C "$old" diff --cached --binary
git -C "$old" ls-files --others --ignored --exclude-standard
```

Confirm the appropriate remote's current tip, using `ls-remote` during a
read-only pass or a fetch when authorized. Compare commits against that verified
remote, including detached HEADs. No upstream does not necessarily mean unpushed
work: a topic's commits may already be included in `origin/main`. Conversely,
an integrated HEAD says nothing about later uncommitted files.

Check the owning chat/editor and processes using the path. An idle-looking
directory or absence of a process is not proof that its owner has finished.
Record external symlink targets without following them for cleanup. Inspect
ignored build/package/evidence payloads even when ordinary Git status is clean:
normal worktree removal can delete ignored files too.

Use these classifications:

| Class | Default disposition |
| --- | --- |
| Active | Retain; coordinate the owner before an approved move. |
| ON HOLD | Retain all unfinished work and evidence; relocation does not clear the hold. Only an explicit owner decision changes that status. |
| Completed | Retire only once commits and necessary local contents are demonstrably preserved. |
| Uncertain | Retain until ownership and contents are established; an approved move can organize it without discarding its data. |

## Relocate with Git

After authorization, ensure the destination is absent, its parent exists, and
the owner has coordinated any shells, editors, builds or watchers using the old
path. Preserve the pre-move inventory, then use:

```sh
git -C "$repo" worktree move "$old" "$destination"
git --no-optional-locks -C "$destination" status --porcelain=v1 --untracked-files=all
git -C "$repo" worktree list --porcelain
```

Compare HEAD, branch, staged/unstaged state, file hashes, untracked/ignored
manifests and symlink targets with the original inventory. Update the owner's
workspace references. Configured builds may contain absolute old paths: retain
their evidence and configure a fresh build only when subsequent work needs it.
Do not rewrite historical logs or modify shared SDK/download targets.

Git cannot use this move operation for the main checkout or linked worktrees
containing submodules. If it refuses a move, inspect the actual reason; do not
add force flags or manually move/edit Git metadata. Locked worktrees remain
protected until an authorized owner coordinates the operation.

## Retire without losing work

Require a completed disposition, integrated or independently preserved commits,
and an explicit decision for every local or ignored payload that matters.
Preserve unique changes using the existing
[archive and retention policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md):
include the base identity, staged/unstaged patches, necessary untracked/ignored
contents, modes and symlinks, checksums and a verified restoration manifest.
A patch alone omits untracked/ignored data; a default stash also omits it.

Do not reset, stash, commit or discard somebody's unfinished work merely to make
removal succeed. Dirty worktrees remain retained until the owner authorizes
their specific disposition. A recovery worktree needs an explicit recovery
ownership decision. Never infer permission from its age or a similar copy.

For an approved, verified clean retirement:

```sh
git -C "$repo" merge-base --is-ancestor "$head" "$accepted_remote"
git -C "$repo" worktree remove "$old"
git -C "$repo" worktree list --porcelain
```

Keep branch refs unless branch deletion is separately authorized. Normal removal
handles its Git registration; no routine metadata pruning is needed. Do not use
`--force`, broad filesystem cleanup or manual `.git/worktrees` deletion to bypass
checks. Missing/prunable registrations require inspection and authorization,
not an automatic prune.

## After an accepted milestone is pushed

Verify the accepted remote SHA, refresh the worktree inventory and record the
owner's completion/retention decision. Preserve necessary evidence outside a
retiring worktree, then perform only cleanup covered by the task's authorization.
Keep unfinished, ON-HOLD and uncertain work; carry useful retained checkouts
forward under the standard root. Review completed leftovers periodically rather
than accumulating a new checkout for every experiment. Ordinary housekeeping
does not require compiler tests or another feature milestone.

Git behavior references: [worktree manual](https://git-scm.com/docs/git-worktree),
[status and ignored files](https://git-scm.com/docs/git-status), and the
[Git 2.53 removal check](https://github.com/git/git/blob/v2.53.0/builtin/worktree.c#L1213-L1318).
