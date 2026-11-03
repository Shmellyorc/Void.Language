# LLVM 23.1.4 Ubuntu disk preflight — Golden Wrench review

Baseline `08f7190c7b411049836ba469e27ae8e715173b43`.
Infrastructure-only repair; **hosted success remains UNVERIFIED**. Windows mTLS
instrumentation and its unresolved native acceptance requirements are unchanged.

## Failure and evidence limits

The latest run is [41556790927](https://github.com/Shmellyorc/Void.Language/actions/runs/41556790927).
The job API reports Ubuntu Debug job 125657537468 failed after reaching the
complete LLVM SDK restore; development-package installation succeeded. The run
was still active when inspected. Boss supplied the tar error naming
`.toolchains/llvm-23.1.4/bin/llvm-gsymutil` and `No space left on device`, with a
roughly 2.71 GiB compressed cache. This is a restoration/infrastructure failure,
not an observed HTTP test failure.

The job-log endpoint returned HTTP 404; the connected GitHub tool reported
`BlobNotFound`. A subsequent CLI retrieval still returned 404. There were no
published artifacts at inspection. Thus **hosted pre-restore free bytes, total
filesystem capacity, inode availability, competing allocations and exact peak
usage cannot be recovered from available evidence**. The original workflow did
not measure these. We do not invent those values or infer test failures from an
unexecuted test step. The retained API records and retrieval failure are under
[evidence/llvm-disk-repair](evidence/llvm-disk-repair/); existing CI evidence is untouched.

## Measurements and budget

Local `du -sx -B1` measurements (allocated bytes; not archive compression size):

| Allocation | Bytes |
| --- | ---: |
| Complete LLVM 23.1.4 SDK | 12,432,510,976 |
| Official Linux SDK xz archive | 2,017,061,664 |
| Release build tree | 5,822,873,600 |
| Debug build tree | 13,522,292,736 |
| Strict build tree | 19,656,425,472 |
| Largest current AWS-LC provider build, including sources | 898,060,288 |
| Largest current installed provider prefix | 173,666,304 |
| HTTP source download directory | 229,257,216 |
| Largest retained hosted ThinLTO build-cache receipt | 132,422,080 |
| Local editor directory | 156,200,960 |

Local build trees contain accumulated build/test output; these are conservative
planning measurements, not fresh hosted per-configuration peaks. The ThinLTO
observation is from retained prior hosted receipts, not a guaranteed Linux upper
bound. Local `.toolchains/http` totals 9,882,714,112 bytes but includes three
profiles and old TLS review builds. That entire developer tree is **not** restored
by CI; only one verified profile/fixture set is used. Existing preinstalled Clang
versions on the runner are separate from the pinned SDK, but are left alone to
avoid disturbing package-owned compiler dependencies. No duplicate pinned SDK
installation is introduced.

The official archive SHA-256 was recomputed and matches the existing approved
`25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
The installed receipt, development headers/libraries, Clang and LLD pass the
unchanged `llvm_sdk.py` verification at 23.1.4.

The preflight uses these recorded quantities, not a compressed-cache-only
threshold. Define S=SDK, D=provider build+prefix+downloads,
B=configuration build+ThinLTO+editor, A=3 GiB (round up the reported 2.71 GiB).
Margin M=ceil((S+D+B)/4), an **explicit 25% engineering estimate**, accounts for
hosted variation, link scratch, fixture growth and artifact compression. It is
not a measured guarantee. Before restore require S+A+D+B+M free; before build
require B+M free (SDK/dependencies already occupy disk). This conservatively
keeps the archive allowance even though cache extraction later releases it.

| Configuration | Before restore, bytes | Before build, bytes |
| --- | ---: | ---: |
| Release | 28,027,464,752 | 11,072,744,496 |
| Debug | 37,651,738,672 | 20,697,018,416 |
| Sanitizers | 45,319,404,592 | 28,364,684,336 |

If workspace and temporary storage are separate filesystems, also require A in
temporary storage. If the remaining runner space fails this budget, stop with
available, required and deficit bytes before allocating; do not lower the budget
merely to get past preflight. Real receipts should inform any later refinement.
Local root capacity is 998,143,664,128 bytes, with 440,264,159,232 available at
measurement; `/tmp` is separate tmpfs. This does **not** reproduce the hosted
limit. Only arithmetic/identity simulations were used for low-space controls;
no developer disk was filled and no local system SDK was removed.

## Cache restoration and layout

The inspected [actions/cache v5 bundled restore implementation](https://github.com/actions/cache/blob/v5/dist/restore/index.js)
downloads a complete compressed archive into a unique `RUNNER_TEMP` directory,
extracts it to the workspace, then unlinks the archive in `finally`. Therefore
compressed and extracted data coexist during restoration on a shared filesystem.
Moving between directories on the same filesystem does not recover that peak.
An independent adequately sized filesystem could help, but none is established
by the failed job's available evidence; no speculative mount/layout change is made.
The same action may create a compressed archive during post-job cache save on a
miss. The planning budget retains archive and scratch headroom.

The exact cache path/key, official archive checksum, identity receipt and
compiler checks are unchanged. No cache eviction, SDK pruning or recompilation
was added. A valid cache hit still follows the existing fast path. A cache miss
still uses the existing checksum-verified complete official download.

## Cleanup and workflow changes

`tools/ci/linux_disk.py` is restricted to GitHub-hosted Ubuntu 24.04. It removes
only these explicit unrelated runner-image SDK trees, if present:

- `/usr/local/lib/android`: Android mobile SDK, not native Linux/compiler/editor work.
- `/usr/share/dotnet`: .NET SDKs, not used by VOID or the Node editor tests.
- `/opt/ghc` and `/usr/local/.ghcup`: Haskell toolchains, not used by this workflow.
- `/usr/share/swift`: Swift SDK, not the pinned LLVM SDK or Linux C++ dependencies.

The [Ubuntu runner inventory](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md)
identifies these unrelated tool families. Missing paths are harmless; redirected
path components, non-directory targets and mount-point targets are rejected
before any deletion. `rm --one-file-system` prevents crossing child filesystems.
No arbitrary path argument is accepted. `apt-get clean` removes downloaded package
archives, not installed packages. Python, Node, system C/C++ support, required
libraries, source, active build/cache paths and the entire hosted toolcache stay
untouched. No blanket `/tmp`, Docker, `/usr`, `/opt`, `/home` or `.toolchains`
cleanup is performed. Actual reclaimed bytes are measured, not promised.

The existing seven-job workflow adds only three Linux steps:

1. Run the focused safety/policy tests; record pre-cleanup `df`, bounded directory
   totals; clean the allowlist; record post-cleanup and pre-restore measurements
   and enforce the restoration/build budget, before the first large LLVM restore.
2. Record disk/SDK size after successful SDK verification.
3. Record disk and enforce remaining headroom immediately before the main build.

Receipts use the existing `build/link-experiment/` artifact collection, including
preflight failure records. There is no new cache key, workflow matrix, test filter
or disk-management dependency. ThinLTO, full CTest, editor checks, strict sanitizer
options, native platform checks, HTTP verification and artifacts remain intact.

## Verification and review boundary

- Actionlint 1.7.12: pass, no diagnostics.
- PyYAML parse: pass; seven unchanged matrix configurations.
- `python3 tests/net/http/ci_disk_test.py`: 5/5 pass. All configurations test the
  exact success boundary and one-byte deficit; separate temp filesystem failure
  and success; allowlist, symlink/parent redirect/mount rejection; developer-host
  rejection before deletion; Linux-only step order, unchanged cache key/full CTest.
- `python3 tests/net/http/ci_bootstrap_test.py`: 17/17 pass.
- `python3 tests/net/http/ci_triage_test.py`: 12/12 pass, including 1,588 reviewed
  checkout blobs and cache-acquisition/security negative controls.
- Unchanged SDK verifier: pass; official archive digest independently matched.
- Release HTTP dependency manifest verifier: pass, 167 files.
- Python byte compilation and `git diff --check`: pass.

The dependency verifier does not implement `--help`: an initial help probe
reported a missing `--help/VOID-HTTP-DEPENDENCIES.json`. The proper prefix command
then passed; this was an invocation error, not a dependency failure.
No compiler/runtime source changed, so a new expensive build/stress run was not
needed for this infrastructure-only diff. No hosted acceptance is claimed.

Golden Wrench should review the bounded cleanup and planning assumptions. Boss's
next authorized Ubuntu run must record actual cleanup recovery, pass both
preflights, restore/verify the complete SDK, then finish the existing build,
unfiltered tests/editor checks, artifacts and cache post-step. A green restore
alone is not full disk-headroom acceptance. Windows Schannel mTLS remains open as
recorded in [CI41533124522_TRIAGE.md](CI41533124522_TRIAGE.md).

Changed files: workflow, `tools/ci/linux_disk.py`, `tests/net/http/ci_disk_test.py`,
this record, CI handoff/current-engineering links and compact evidence inventory.
No staging, commits, pushes, branches, CI dispatch, dependency upgrades or root
README modifications. **Stop for Golden Wrench review.**
