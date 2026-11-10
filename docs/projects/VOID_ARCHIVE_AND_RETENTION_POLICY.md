# VOID archive and retention policy

**Applies to every stage, slice, gate and milestone.** Golden Wrench reviewed the first cleanup and Boss authorized its exact tracked and local sets. The audited Git baseline is `a642ce746a62a7646d06d42d9f87931813aaa812`.

## Permanent workflow

| When | Required practice |
| --- | --- |
| Start | Check free space before large builds. Create task-owned disposable output directories outside the source and docs trees. Identify the maintained inputs and the unique records worth keeping. |
| Qualify | Run the maintained tests with their existing assertions and limits. Reuse suitable configurations. Keep diagnostics while a failure is actionable and put routine outputs in scratch. |
| Close | Record the accepted behavior, limitations, platforms, profiles, results, pinned dependencies and replay commands in one Markdown closeout. Keep source, tests and required inputs. Archive selected historical material, then remove disposable outputs when authorized. Record free space before and after cleanup. |
| CI | Run the same maintained coverage. Use short-lived diagnostic artifacts for failures. Keep generated transcripts, captures, packages and build outputs out of Git. |

Monitor large new directories during the work. Use ordinary ZIP with DEFLATE level 9 for approved historical records. Keep active toolchains, verified provider inputs and release notices with the development environment.

## Test development

- Give each test invocation its own temporary workspace. Use Python's `TemporaryDirectory` for fixtures and subprocesses. A Python harness that writes receipts can use `tools/test_output.py`'s `output_directory()`. A CMake recipe that passes an output path can use `void_add_output_test` and the `@VOID_TEST_OUTPUT@/` prefix. Both use the system temporary location and clean up when the test exits normally, including assertion failures.
- Keep checked-in fixtures and golden outputs read-only. Use `voidc run --no-cache` when exercising a checked-in source outside a cache test. Keep cache behavior tests pointed at copies or generated fixtures in a private directory. CTest disables Python bytecode writes. Use `python -B` for direct harness runs.
- Use `ctest --output-on-failure` for routine diagnostics. Set `VOID_TEST_DIAGNOSTICS_DIR` to a private directory outside the checkout to keep a rerun's shared-harness output. That opt-in also retains successful reruns. Inspect and remove the directory after diagnosis. If a test runner is killed, check its task-owned temporary root for leftovers.
- Keep build and install inputs, including generated manifests and prelinked regression executables, with their configured build. CTest's own result log stays with that build. Pass explicit output paths to one-off qualification and fixture-generation tools.

## Verified archives

Archives are in `/home/shmellyorc/Documents/Void.Language-archives/`. Each ZIP embeds `_ARCHIVE_MANIFEST.json` with the original path, source SHA-256 and member SHA-256. The external `archive-index.json` records the ZIP hashes and verification. Ordinary Python `zipfile` passed the CRC checks and extracted every member into a disposable directory. All extracted member SHA-256 values matched the selected originals. ZIP sizes include the embedded manifest.

| ZIP | Source files | Original bytes | ZIP bytes | ZIP/original |
| --- | ---: | ---: | ---: | ---: |
| `voidup-hosted-ci-failures-2026-11-07-to-08.zip` | 13 | 31,114,452 | 2,720,539 | 8.7% |
| `slice7-integration-and-g5-2026-11-09.zip` | 27 | 521,387 | 125,925 | 24.2% |
| `slice7-raylib-platform-diagnostics-2026-11-09.zip` | 13 | 24,974 | 13,961 | 55.9% |
| **Total** | **53** | **31,660,813** | **2,860,425** | **9.0%** |

The VOIDUP ZIP keeps four representative original failed job logs, job metadata and restoration indexes. The Slice 7 ZIP keeps the native link and G5 refusal controls, selected qualification failures, the authoritative Release JUnit from the launcher incident, and final summaries. The raylib ZIP keeps the Mesa leak and WGL failure records, native controls and Win32 software build recipe. The selection leaves out routine passing transcripts, screenshots, executable copies, objects, debug symbols and build trees.

## Exact cleanup proposal

The audited tracked manifest remains `/home/shmellyorc/Documents/Void.Language-docs-cleanup-manifest.json.gz`, SHA-256 `fe0ea6c5d74a6edc300b2a924c618abeebeaf082879eb61422d2fd376fba0042`. The external `cleanup-proposal.json.gz` gives the exact tracked disposition and the nine local trees' relative paths, leaf and directory identities, and fingerprints. Its SHA-256 is `529b9684d7394f7f91c61442eedeabb47de25d779b3d853da1bdc3763999a93d`.

| Tracked disposition | Exact scope |
| --- | --- |
| Three originals represented by curated ZIP contents | `docs/voidup/evidence/ci-{42377496702,42427327049,42446206797}/original-ci.tar.gz`. The new ZIP contains selected original members. The audited commit retains the full bundles. |
| 16,007 files with no new archive | 42 generated benchmark ELF executables, 15,963 routine passing outputs and receipts, one byte-matched duplicate HTTP ZIP and one generated compiler IR/object archive. |
| Required inputs retained | Maintained implementation, tests, sample sources, benchmark scripts, pinned vendor sources, both configure-time curl patches, Unicode provenance, test fixtures, notices and licenses. The three frozen logging baseline executables remain outside this proposal. |

All **16,010** paths matched the audited Git modes, blobs and sizes. The proposal removes **1,420,280,079 bytes**, taking the unpacked baseline from **1.94 GiB to 628 MiB** and from **69,112 to 53,102 files** before documentation edits. Including this policy, the projected checkout has **53,103 files** and rounds to the same size. Git history retains the old objects.

The nine local Slice 7 build roots match the previous inventory across **26,240 leaves and 18,148 directories**. Their measured data-block recovery is **43.41–88.66 GiB** on XFS, depending on surviving reflink references. With the tracked proposal, the initial local range is **44–90 GiB**. The manifest lists six older build roots and five toolchain investigation roots for follow-up after their manual consumers and unique inputs are checked. It separates the nine build roots from sibling source packets, installed packages and diagnostic archives. The accepted Windows recovery remains **9.87 GiB**.

Keep `build/llvm23-{release,debug,strict}`, the active LLVM/Clang/LLD **23.1.4** SDK, verified host and HTTP prefixes, required provider archives and licensing inputs, and the verified S1–S7 integration backup. Disposable build trees are cleanup candidates. The older LLVM 19 and SDK inspection copies need a separate ownership and dependency check.

The authorized cleanup removed all **16,010 tracked files** and **nine inventoried Linux build trees**. Available filesystem space increased by **89.14 GiB** after the disposable validation workspace was removed. The remaining workspace measured **230.56 GiB** by `du`. Release retained all **443** accepted registrations and passed **15/15** focused checks. The Markdown scan found zero newly missing relative targets. The 703 pre-existing unresolved targets include 293 historical Slice 7 evidence targets.

## Reproduction and documentation

The VOIDUP report now points to its retained restoration indexes. The HTTP triage points to the tracked extracted logs. The logging benchmark notes distinguish rebuildable copies from the frozen baseline. Slice 7 now gives the exact Windows test selector, Linux profile settings and checkout-relative commands. The raylib closeout includes the verified Windows software DLL build flags and points to the retained sample workflow. [VOIDUP integration](../voidup/INTEGRATION_REPORT.md) · [HTTP triage](../net/http/CI41307437931_TRIAGE.md) · [Logging benchmarks](../log/BENCHMARKS.md) · [Slice 7 replay](SLICE7_FINAL_CLOSEOUT.md#reproduce-from-source) · [Raylib replay](SLICE7_RAYLIB_INTEGRATION.md#build-and-run-the-sample)

The accepted Slice 7 results remain **443/443 Release**, **456/456 Debug** and **433/433 fatal ASan/UBSan with leak detection** on Linux, and **150 selected cases per profile** on Windows 10 Release and Debug with the recorded retries. Historical receipt links in the closeouts describe the original audit. Current build, test and installation procedures use the retained source and pinned dependencies. Run the affected registered suites from a clean candidate checkout before an approved deletion. This archive preparation verified documentation and manifests. It used the recorded qualification results.

## Restoring historical records

Verify the ZIP hash against `archive-index.json`, then extract into a fresh private directory with Python. Compare the restored members with the SHA-256 values in `_ARCHIVE_MANIFEST.json`.

```sh
archive="$HOME/Documents/Void.Language-archives/voidup-hosted-ci-failures-2026-11-07-to-08.zip"
restore=$(mktemp -d)
python3 -m zipfile -t "$archive"
python3 -m zipfile -e "$archive" "$restore"
```

To recover the complete original VOIDUP bundle from Git history, write it outside the checkout and compare its member hashes with the retained restoration index. Substitute the corresponding run ID for either earlier bundle.

```sh
git show a642ce746a62a7646d06d42d9f87931813aaa812:docs/voidup/evidence/ci-42446206797/original-ci.tar.gz \
  > "$restore/original-ci.tar.gz"
```
