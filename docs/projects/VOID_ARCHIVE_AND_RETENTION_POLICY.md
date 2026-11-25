# VOID archive and retention policy

Current policy: 25 November 2026. Boss's repository cleanup direction supersedes
older artifact-retention exceptions and append-only documentation requirements.

`docs/` contains legitimate Markdown only. Keep maintained schemas, package and
license metadata, source fixtures and tooling inputs with their operational
consumers outside `docs/`. Generated receipts, logs, test results, source snapshots,
binaries, caches, archives and investigation outputs stay outside the repository.
Do not add review Markdown, evidence directories, inventories or handoff files to
the checkout. Use Git history for superseded tracked records.

## Permanent workflow

| When | Required practice |
| --- | --- |
| Start | Check free space before large builds. Create task-owned disposable output directories outside the source and docs trees. Identify the maintained inputs and the unique records worth keeping. |
| Qualify | Run the maintained tests with their existing assertions and limits. Reuse suitable configurations. Keep diagnostics while a failure is actionable and put routine outputs in scratch. |
| Close | Update an existing canonical contract when needed and return review findings in the conversation; do not create repository review files. Keep source, tests and required inputs. Archive selected historical material, then remove disposable outputs when authorized. Record free space before and after cleanup. |
| CI | Run the same maintained coverage. Use short-lived diagnostic artifacts for failures. Keep generated transcripts, captures, packages and build outputs out of Git. |

Monitor large new directories during the work. Use ordinary ZIP with DEFLATE level 9 for approved historical records. Keep active toolchains, verified provider inputs and release notices with the development environment.

## Test development

- Give each test invocation its own temporary workspace. Use Python's `TemporaryDirectory` for fixtures and subprocesses. A Python harness that writes receipts can use `tools/test_output.py`'s `output_directory()`. A CMake recipe that passes an output path can use `void_add_output_test` and the `@VOID_TEST_OUTPUT@/` prefix. Both use the system temporary location and clean up when the test exits normally, including assertion failures.
- Keep checked-in fixtures and golden outputs read-only. Use `voidc run --no-cache` when exercising a checked-in source outside a cache test. Keep cache behavior tests pointed at copies or generated fixtures in a private directory. CTest disables Python bytecode writes. Use `python -B` for direct harness runs.
- Use `ctest --output-on-failure` for routine diagnostics. Set `VOID_TEST_DIAGNOSTICS_DIR` to a private directory outside the checkout to keep a rerun's shared-harness output. That opt-in also retains successful reruns. Inspect and remove the directory after diagnosis. If a test runner is killed, check its task-owned temporary root for leftovers.
- Keep build and install inputs, including generated manifests and prelinked regression executables, with their configured build. CTest's own result log stays with that build. Pass explicit output paths to one-off qualification and fixture-generation tools.

## Canonical locations

- Main integration checkout: `~/Documents/Void.Language/`.
- Linked worktrees: `~/Documents/Void.Language-worktrees/`; preserve dirty and
  ON-HOLD work, and use Git's worktree relocation operation.
- Retained engineering records: `~/Documents/Void.Language-archives/`.
- Owned disposable scratch: outside the checkout, cleaned on exit. `/tmp` is not
  retained storage and must not accumulate abandoned test runs or evidence.

The 25 November Documents reconciliation moved 78 loose evidence/review entries
intact into `Void.Language-archives/relocated-documents-2026-11-25/`. Registered
G50 recovery moved into `Void.Language-worktrees/g50-recovery/`, preserving its
HEAD, uncommitted state and HOLD. The unrelated `Documents/Void/` repository was
retained. App-managed `.codex/worktrees/` checkouts were not moved by this cleanup.

Repository artifact moves, operational-input locations, original inventories and
verification records are outside the checkout under
`Void.Language-archives/repository-cleanup-2026-11-25/`. Configured build trees
were preserved intact there; cached absolute old paths are historical, so a future
build should be configured freshly outside the source checkout. Pinned active
LLVM/HTTP/host provider installations remain available in `.toolchains/`.

## Historical preservation

The previous exact cleanup proposals, original sizes, ZIP manifests, platform
qualification and reproduction notes remain in
[the recorded Git snapshot](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)
and the existing external archives. No history was rewritten or object database
pruned. Retained original failure records were moved intact, not relabeled as
passes. Relocation organizes data; it does not by itself reclaim its disk space.

Manual dependency/CI and historical qualification scripts that still write below
`docs/` require coordinated output-path migration before reuse. Their recipe
identity and collector contracts must remain intact. `.gitignore` prevents normal
staging of generated docs; it cannot prevent local filesystem writes.
