# Bounded resident-text capture — no new core

28 October 2026. **Disposition D: unresolved. STOP for Golden Wrench review.**
Performance OPEN; release HOLD. The resident-text comparison could not be made
within the authorized 12-launch budget. No production correctness clearance,
repair, or logging implementation follows from these passing observations.

## Outcome and protocol limitation

| Launches | Result |
|---|---|
| 1 | Original process started; parent metadata serialization failed. Native exit status and output were not retained. No new core found. Counted against the budget, never as a pass. |
| 2–12 | Eleven exit-0 observations; requested filter, actual stack limits and private executable mapping verified for every process. No core captured. |
| After 12 | No further target execution. |

The first launch's parent metadata writer attempted to serialize a Python
`PosixPath`. This was an investigation-launcher defect. When that launcher exited,
its output pipes and native wait status were lost; the launch cannot be treated
as a fully preserved control. The saved partial `/proc` files do establish its
filter and stack settings. The original launcher, traceback and partial JSON
remain intact. The corrected launcher converted the path to text and continued
with **attempt 2**, preserving the total ceiling of 12. No target binary, native
host, source input or repository test was changed.

The existing crash recorder preserves the launcher failure as
`incident-pr3wveef` with **launcher exit 1 and native status explicitly unknown**.
This is not a captured native SIGSEGV. A separate read-only journal check returned
status 1 with `-- No entries --`; the helper expected zero and preserved
`incident-e6cbekot`. Its saved result was reconciled as no matches without rerunning
the query. That diagnostic-tool status is also not a native target crash.

## Identity, mapping and execution controls

Baseline: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
Original executable SHA-256:
`c284712dd4958faf41547deb49a851306256a9075cf59d968671272bc17e96bb`.
All **61 original source identities**, including their preserved copies, were
verified before and after the run sequence. Original argv, `optimized timing`
mode, launch cwd `/home/shmellyorc/Documents/Void.Language`, and host working
directory `/tmp/VOID-hotpath-combined-kdkc1i_d` retain their spelling. Exact argv
is preserved in `source-identity.json` and `plan.json`, and matches the command
in [CRASH_CLASSIFICATION.md](CRASH_CLASSIFICATION.md).

Explicit workload settings remain:

```text
VOID_STDLIB_ROOT=/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/flat-builder/stdlib
VOID_PERF_SHAPES=0,4,9
VOID_PERF_STAGES=6,7,11,12
```

Relevant absent preload, audit, library-path, sanitizer and allocator variables
match the prior native capture plan. The child's `VOID_CRASH_EVIDENCE` retains
the preceding native run's value; the parent recorder writes into the new evidence
directory. The original complete environment was not retained, so perfect
environmental identity remains unprovable. This run's launch environment is saved
privately and excluded from the review archive.

The old fault's normalized ELF address `0x215070e` lies in a file-backed executable
mapping. Each successful launch's `/proc/PID/maps` confirms **`r-xp`** at that
relocated address: readable/executable, private, backed by the original executable.
These parent-side metadata reads did not stop or attach to the child and did not
read process memory. ELF core flags alone had not established the private/shared
property in the previous investigation.

Only the child pre-exec hook sets `coredump_filter=0x37`, adding private file-backed
pages to the preceding `0x33` filter. This bit is defined by the
[Linux core-filter documentation](https://www.kernel.org/doc/html/latest/filesystems/proc.html#proc-pid-coredump-filter-core-dump-filtering-settings).
The eleven complete observations confirm:

- `coredump_filter`: **00000037** after exec.
- Actual soft **and** hard execution stack: **1,048,576 bytes**.
- Soft and hard core limit: unlimited; umask 0077 set before exec.
- Original executable path and argv; no preload or debugger injection.

No kernel-wide setting changed. The parent filter remains `00000033`. Existing
private working/evidence directories remain 0700. Before execution, free space
was 13,831,467,008 bytes on the core destination filesystem and 530,765,455,360
bytes on the external evidence filesystem. At least 2 GiB free in each was
required again before every remaining attempt. The old core was never replaced.

## What remains unknown

The PID-specific kernel journal query spanning this sequence returned no matching
entries. No new SIGSEGV, registers, failing call stack, trap record or resident
instruction bytes were captured. Offline comparison code was prepared but **not
executed**, because there was no new core to inspect. No equality or mismatch
claim is made about resident executable text.

The previous core `incident-w415alhc` and its same-PID kernel record still establish
a general-protection fault in LLVM DSE during VOIDC's O2 compilation path. They
still omit the resident executable-text pages. The component location, original
signal/register/backtrace evidence and its limitations remain exactly those in
[CRASH_CLASSIFICATION.md](CRASH_CLASSIFICATION.md). These eleven passes do not
explain that fault or exclude a defect affecting released VOIDC correctness.

**Root cause remains unresolved; the requested resident-text evidence gap remains
open.** No production correction or cause-specific regression can be justified
from this run. Further collection requires Golden Wrench review of the exhausted
bound; no automatic extension, instrumentation variant, source reduction, optimizer
workaround or logging implementation was attempted.

## Evidence and preservation

Private root:
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/resident-text-capture`.

- `results.json` and `resident-*.result.json`: all 12 launches, statuses and PIDs.
- `observations/`: mappings, actual limits, filter and process metadata.
- `preflight.json`, `plan.json`, `source-identity.json`: exact controls and identities.
- `crashes/`, `receipts/`, `drivers/`: both diagnostic failures, original/corrected
  launchers, logs and command receipts; `drivers/usage.json` identifies unused code.
- `verification.json`: kernel reconciliation, source/executable preservation and
  per-process mapping checks; `disposition.json`: explicit unresolved disposition.
- `EVIDENCE_INDEX.md` and `review/void-log-resident-text-review.tar.gz`: compact
  handoff; raw environment, cores and native binaries are excluded from the archive.

Integrity checks verified 721 prior incident/native-capture evidence files,
783 frozen repository files, 11 frozen executables, 8,343 historical repository
evidence files, and the 1,176- and 356-file earlier external inventories. All
61 original inputs and the protected root README remain unchanged.

Only this report and pointers in `CRASH_CLASSIFICATION.md` / `HANDOFF.md` change
in the repository during this follow-up. No Git state mutation, live debugger,
ptrace, hosted CI, production/test repair, root README edit or package work.
