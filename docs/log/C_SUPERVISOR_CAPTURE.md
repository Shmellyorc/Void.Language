# Native C supervisor capture — 12 plus 50 attempts complete

28 October 2026. Baseline `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
**Classification D: unresolved. Performance OPEN. Release HOLD.**

The first C-supervisor campaign completed **12 original-command launches, all
native exit 0**. Boss then ran the separately authorized 50-attempt campaign using
the delivered ZIP. **All 50 launches completed with native exit 0**, with every
raw wait status retained. Neither campaign produced a VOIDC SIGSEGV or new VOIDC
core. The 50-launch budget is exhausted. **STOP for independent Golden Wrench
review; do not rerun the package or reset its claim.** Passing observations do not
resolve the historical crash or justify a production repair.

## Completed user-run campaign

On 28 October 2026, the user-run campaign began at **15:10:41 PDT** and ended at
**15:26:52 PDT**, elapsed 970.84 seconds. The agent subsequently inspected saved
evidence offline without launching a target. All 50 fixed-format `wait.raw`
records independently agree with `results.json` and the supervisor's reports:

- Raw wait status 0, `WIFEXITED=1`, `WEXITSTATUS=0`, no signal/core flag.
- All statuses durable; no unknown outcome, exec error, timeout or evidence error.
- Actual pre/post-exec soft and hard stack 1,048,576 bytes, unlimited core limits,
  filter `0x37`, exact argv/environment and private executable mapping verified.
- 48 PERF rows per invocation, **2,400 preserved observations**, with recorded
  stdout/stderr hashes matching the files. These are not performance acceptance.

Supervisor, offline collector and journal reader each exited 0. The saved kernel
journal query returned `-- No entries --`. No new target core was found and no
matching supervisor or original target remained active. The user's pasted log
ended at `START attempt=50`; the complete private log includes
`DONE attempt=50 pid=1696932 raw=0 ... durable=1 io_error=0`, and its independent
raw wait record proves that final outcome. No launch is inferred from a partial
paste or silently counted as a pass.

Original executable, all 61 live inputs and preserved copies, prior DSE core and
the delivered ZIP still match their saved hashes. `F/post-user-run/verification.json`
records every attempt/PID, native status, wait-record hash, settings, times and
summary. The preceding report/handoff and evidence index are saved in that private
directory. The ZIP remains unchanged as the historical pre-launch delivery; its
statements that the campaign had zero launches describe delivery time, not the
current state. The earlier 12-attempt campaign remains separate: **62 completed
original invocations across these two C campaigns, all exit 0**, with no causal
clearance implied.

## Recovery before the user launch

The required crash, resident-text and DSE-input reports and handoff were read.
Private receipts independently distinguish these three sets of observations:

| Set | Original VOIDC launches | Native result | Core |
|---|---:|---|---|
| Completed C campaign | 12 | Twelve exit 0; raw wait status 0 each | No new VOIDC core |
| User-run 50-attempt campaign | **50** | Fifty exit 0; raw wait status 0 each | No new VOIDC core |
| Controlled supervisor fixtures | 7 per supervisor build | All expected classifications verified | Deliberately raised fixture SIGSEGV only |

Recovery at 14:33:57 PDT found no `campaign.started`, attempt directories,
supervisor output, or termination receipt in the 50-attempt campaign. Only its
prepared command metadata existed. The supervisor durably writes the campaign
marker and per-attempt intent before forking, so these missing records, the
absence of matching active processes, and the absence of a launch tool call
establish zero launches. No old process needs to be resumed or reaped. The
read-only kernel journal query covering 14:22:39–14:33:57 PDT returned status 0
with no entries. The only core in the original target working directory remains
the previously preserved DSE core. Fixture cores are in separate private fixture
directories. Recovery results are in `F/recovery-state.json`.

Here `H` is the literal private root
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028`,
`S` is `H/c-supervisor-capture`, and `F` is
`H/c-supervisor-capture-50`. These abbreviations are explanatory; exact recorded
argv use full paths. Both campaigns and all original evidence remain separate.

## Original identity and execution controls

Original executable SHA-256:
`c284712dd4958faf41547deb49a851306256a9075cf59d968671272bc17e96bb`.
All **61 live original inputs and 61 preserved copies** match the original
incident inventory. Exact argv remains:

```text
/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/combined-keys/void_log_host-timing
/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/flat-builder/performance.void
/tmp/VOID-hotpath-combined-kdkc1i_d
optimized
timing
```

Launch cwd is `/home/shmellyorc/Documents/Void.Language`; the host subsequently
uses `/tmp/VOID-hotpath-combined-kdkc1i_d` with that spelling. Explicit environment
retains `VOID_STDLIB_ROOT=H/flat-builder/stdlib`, `VOID_PERF_SHAPES=0,4,9`, and
`VOID_PERF_STAGES=6,7,11,12`. The full saved resident-text launch environment is
reused byte for byte, including its entry order. Its NUL-encoded SHA-256 is
`25c3e171810e3500df00257482d2c4c1c1a09a0d02eefa85c062daef18be0ccf`.
**The original incident's full environment was not retained.** Reusing this later
environment does not establish perfect environmental identity with that incident.

Each child sets and verifies actual soft **and** hard stack limits of 1,048,576
bytes, unlimited process-local core limits, filter `0x37`, and umask 0077. Parent
metadata reads after exec verify limits, filter, executable, argv and private
file-backed `r-xp` mapping. They do not read process memory or attach a debugger.
All 62 completed original runs across both C campaigns passed these checks. Their signal blocked,
ignored and caught masks were zero. Private files are 0600 and directories 0700.
The supervisor checks at least 2 GiB available on both evidence/core filesystems
before every attempt. Preparation found approximately 13.8 GB at the core
destination and 530 GB at the evidence destination. Kernel `core_pattern=core`,
`core_uses_pid=1` and `suid_dumpable=0` remain unchanged.

## Supervisor identity and validation

The private C supervisor uses `fork`/`execve`, EINTR-aware `waitpid`, native wait
macros and a close-on-exec error pipe. It preallocates `wait.raw` before fork and
writes its fixed raw status record with `pwrite`/`fdatasync` immediately after
reaping, before formatted reporting. Later report failure cannot erase that
record. Stdout/stderr go to private regular files rather than the previous Python
launcher's pipes. No target rebuild, signal handler, library injection or live
debugging is involved. Python is used for offline checks and receipt conversion;
it does not launch the original target.

Both supervisor builds use the project's pinned LLVM 19.1.7 clang with
`-std=c11 -O2 -g -Wall -Wextra -Wpedantic`; both exited 0 without diagnostics.
Exact full build commands are in each campaign's `build/identity.json`.

| Identity | SHA-256 |
|---|---|
| Initial supervisor source | `00d487929d9aecd5e830a120a88eaf4457d8671bad924dd3a0ff88e64f387a58` |
| Initial supervisor executable | `4319f90b9961568510f3cf6c1f3da0fff9810d10621a87b96a18082c985f4b93` |
| 50-attempt supervisor source | `eaf559b64d944c6cbed370b529b6ae9506d6a035b2a685d6cd6fa6cf0a0f26eb` |
| 50-attempt supervisor executable | `63c14ca772bd5c2bfdab97d64ef84f4f483db2b9eaaa30928e7ae6f92435611e` |
| Pinned compiler | `c57e2928bc629e58da157867bb9af550edfc1195e7b003edfcfecd57faf2b71b` |

The second supervisor source differs only in the maximum count and corresponding
campaign-marker text, 12 → 50. `F/sources/supervisor-bound.diff` preserves this
change. Both builds passed these controlled fixtures before any corresponding
target launch was permitted:

| Fixture | Raw wait status | Classification |
|---|---:|---|
| Exit 0 | 0 | Native exit 0 |
| Exit 7 | 1792 | Native exit 7 |
| SIGTERM | 15 | Signal 15 |
| Missing executable | 32512 | Exec stage 9, errno 2; child setup exit 127 |
| Deliberate SIGSEGV | 139 | Signal 11, core flag set, actual private fixture core |
| Timeout | 9 | Timeout recorded; child reaped with SIGKILL |
| Later report creation failure | 0 | Native exit 0 retained; supervisor reports evidence failure 70 |

Each fixture core contains 873 initialized executable-text bytes matching its
fixture binary. The second fixture core SHA-256 is
`35ff91bfd0c652182ad58417ed910a752ecdac4d4e84d81b1a8adc3039d6ac11`.
It validates collection mechanics, **not VOIDC's resident text or crash cause**.
Per-fixture PIDs, raw records, source/binary hashes and comparisons are retained
in each `fixtures/results.json`. No fixture core is included in the user ZIP.

## Completed original executions and causal limits

The initial campaign ran 14:18:44–14:22:39 PDT. Attempt/PID pairs are
1/1691138, 2/1691153, 3/1691164, 4/1691206, 5/1691226, 6/1691238,
7/1691252, 8/1691264, 9/1691283, 10/1691306, 11/1691322 and 12/1691342.
All twelve raw wait statuses are 0: no signal, timeout, launch failure, unknown
native outcome or evidence error. Each produced 48 PERF rows, 576 retained
observations total. These are not performance acceptance or evidence of repair.
`S/results.json`, `S/verification.json` and per-attempt raw records retain details.

No new VOIDC fault registers, resident instruction bytes, kernel trap or failing
backtrace are available from these campaigns. The historical core remains
SHA-256 `2c313fd4d8fba185efdceb35f519a5f6d8bc3d80e47e37b3ed367569405aad24`:
SIGSEGV/SI_KERNEL and kernel general-protection error 0 in LLVM 19.1.7 DSE during
VOIDC O2 compilation. Its executable-text pages were omitted. The on-disk shift
instruction at its RIP still cannot establish the resident faulting operation.
The Stage 11 DSE observation has a different fault location and input; a shared
cause remains unproven. See [CRASH_CLASSIFICATION.md](CRASH_CLASSIFICATION.md)
and [DSE_INPUT_REVIEW.md](DSE_INPUT_REVIEW.md).

No concrete invalid operation or broken invariant justifies a repair. Shared
production correctness/reliability cannot be excluded from impact. Attribution
to LLVM, VOIDC, Runtime or hardware remains unsupported. **Classification D
remains unresolved.**

## ZIP handoff and preservation

`F/review/void-native-capture-50.zip` contains the exact validated supervisor and
fixture binaries/sources, build identity, seven fixture results, original binary
and 61 input backups, saved private environment, command, offline collector,
instructions and a payload hash inventory. It is a **same-host handoff**: the
launcher verifies and uses the existing original absolute paths and prepared
campaign. It does not relocate or restore original inputs automatically. Keep
the archive private because it contains the launch environment and native binary.

The user runs the extracted `run.sh`. Its `--check` mode performs checks without
claiming or launching the campaign. Actual execution first verifies identities,
space, kernel configuration, zero existing attempts and no matching processes,
then atomically claims this one campaign. Every extracted copy shares that claim.
The unchanged C supervisor enforces the total ceiling and stops after any signal,
nonzero exit, timeout or evidence failure. The shell invokes the existing
`drivers/run-original.sh` once, then imports already-reaped results through the
existing `tests/text_encoding/crash.py` preservation workflow and saves a kernel
journal receipt. An interruption requires recovery; the launcher never silently
retries or resets the budget. No target launch was used to test the package.

The first package-only preflight exited 1 because its process guard treated an
unreadable executable link for the unrelated user `systemd` service as an unknown
target. Its name/status identified that service. The guard now checks inaccessible
links for matching campaign process names and retains the marker/count checks.
The corrected preflight exited 0 without claiming or launching the campaign.
Both receipts and the initial checker are preserved; this was a packaging guard
error, not a lost native exit or a VOIDC failure.

Preservation checks passed for 4,539 pre-existing external evidence/report files,
783 frozen repository files, 11 frozen executables, 8,343 historical evidence
files, all 61 original live inputs and copies, original binary/core, and both
private diagnostic build identities. These checks precede the intentional update
to this report and the handoff; the preceding handoff is preserved privately.
Root README and production/test sources remain unchanged. No Git mutations,
hosted CI, package work, optimizer workaround or logging implementation occurred.
`F/EVIDENCE_INDEX.md` and `F/archive-identity.json` identify the delivery and
verification receipts. The completed user-run results are recorded above;
**the campaign is closed with Classification D unchanged.**
