# Stage 10 CI #1 macOS stack harness repair — 24 October 2026

**Prepared, locally verified, NOT DISPATCHED. Golden Wrench/Boss review required.
Stage 10 remains INCOMPLETE.** No compiler, Runtime, original test, main CI or
ThinLTO-experiment behavior changes. No Git mutation or hosted execution occurred.

## Preserved CI #1 observations

[Targeted run 40147114803](https://github.com/Shmellyorc/Void.Language/actions/runs/40147114803),
attempt 1, tested exact source `b490579f84a702ec5d11da51a82934a498f88afa`.
This is a separate stack-evidence run, not ThinLTO experiment #4 or main CI #98.
The UTC logs are dated 25 October; this handoff is dated 24 October in Boss's
America/Los_Angeles timezone.

| Configuration / job | Actual observation |
|---|---|
| Windows Debug / 121030709886 | Passed. At 00:34:35 UTC both `voidc.exe` and `void_tests.exe` reported `SizeOfStackReserve: 1048576`; frontend 1/1 and the unchanged semantic-depth/recovery assertions passed. |
| Windows Release / 121030709743 | Passed. At 00:35:31 UTC both actual PE reserves were 1048576; frontend 1/1 and unchanged semantic-depth/recovery assertions passed. |
| macOS ARM64 Debug / 121030709535 | Configure/build succeeded. Both Python wrappers failed at 00:44:20 UTC with `ValueError: current limit exceeds maximum limit` at the stack setter, before test execution. |
| macOS ARM64 Release / 121030709746 | Configure/build succeeded. Both wrappers failed at 00:37:34 UTC at the same setter; neither original test executed. |

[Run metadata](run.json), [job/step conclusions](jobs.json), four complete decoded
logs (compressed; [normalization/hash receipts](logs.json)) and
[four artifact receipts](artifacts.json) preserve the native build and failure
evidence. Connector text logs have CRLF normalized to LF and one trailing newline;
these are not original ZIP byte hashes. Artifact digests are GitHub metadata;
artifact ZIPs/binaries were not downloaded or independently rehashed. Hosted
artifact retention remains finite. The original CI #4, CI #98, experiment #3,
Step 41 and previous review packages remain unchanged.

## Documented restriction and precise diagnosis

[CPython's macOS stack-limit issue](https://github.com/python/cpython/issues/78783)
documents Python builds linked with a custom Mach-O stack. Apple's
[XNU resource-limit implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c#L1443-L1448)
rejects `RLIMIT_STACK` changes with `EINVAL` when `P_LCUSTOM_STACK` is set,
including attempts to lower a limit. CPython 3.12's
[resource implementation](https://github.com/python/cpython/blob/3.12/Modules/resource.c#L243-L249)
maps `EINVAL` to the observed ValueError; its message does not prove the request
actually exceeded the hard limit.

This establishes a harness/platform restriction compatible with the CI #1
failure, not a frontend/depth defect. The old logs do not contain the failing
Python executable's Mach-O headers or kernel flag, so its exact custom-stack
setting is an inference, not a newly measured fact. The repaired run captures
Python's load commands as diagnostics. It does not retry or suppress a failed
setter inside Python.

## Bounded repair

The [native Bash launcher](../../../../tools/ci/stage10_macos_stack.sh) prints
the original limits, sets soft then hard stack limits to **1024 KiB**, verifies
both shell readbacks and `exec`s the existing Python interpreter. Any failed
`ulimit`, wrong readback or failed exec fails the phase. It adds no sleeps,
fallback to larger stacks, Python rebuild, compiler/link flags or test changes.

The [existing helper](../../../../tools/ci/stage10_native_stack.py) routes only
macOS frontend/depth phases through that launcher, preserving separate raw
launcher logs, exit codes and durations. The inherited child uses
`resource.getrlimit` only: each original test is scheduled only after observing
**(1048576, 1048576)** bytes. Each independent receipt records the method and
Python path. The original frontend CTest/JUnit command, depth script, recovery
assertions, budgets and Windows PE gate remain intact. Child failures propagate;
the final collector and artifact upload still execute after failures.

Readback alone must not misrepresent a custom mapped stack. Therefore the helper
also records native `otool -l` output and hashes for both actual test binaries.
Each must contain exactly one `LC_MAIN` with `stacksize 0`; missing, duplicate,
unparseable or custom nonzero values fail before tests. Apple's
[exec implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_exec.c#L6728-L6744)
uses the inherited soft stack limit to protect the default native main stack
when no custom stack size is requested. The inherited hard limit prevents
ordinary descendants from raising it. These are main-thread stack gates, not
claims about Python's custom mapped stack or arbitrary pthread stacks.

LLVM **19.1.7**, original CMake settings, temporary empty per-job ThinLTO cache,
`cmake --build --parallel 3`, 360-minute timeout and all existing source identity,
build/test logs, binaries and failure artifacts remain preserved. Mach-O receipts
and raw shell limit logs are added to the already-uploaded evidence directory.

## Local verification and exact review package

[Validation results](validation.json) and [raw output](local-validation.log.gz)
record 24 passing local checks: Python/Bash syntax, actionlint **1.7.7**,
manual-only trigger and both matrix choices; real Linux Bash → Python → compiled
native child inheritance with both limits **1048576**; rejection when a 512 KiB
hard limit makes the requested boundary unavailable; exact exit-17 propagation;
Mach-O format fixtures accepting default and rejecting custom/missing/duplicate
headers; readback without any Python setter; unchanged test argv; rejection
before test scheduling; raw command-byte/exit preservation; incomplete-evidence
failure and scope preservation. [Validation source](validate.py) identifies
simulated Darwin logic explicitly. **These are Linux/fixture checks, not a
macOS native pass.** No original suite was rerun for this harness-only repair.

[Exact workflow/helper/launcher diff](harness.diff),
[changed-file inventory](changed-files.json) and [final preservation checks](final-validation.json)
freeze this increment against the CI #1 source. The original preparation's
workflow/helper diff and local records remain historical, not overwritten by
the correction. Main CI, experimental CI, root README and all existing test,
compiler, Runtime and earlier evidence bytes remain unchanged.

## Bounded follow-up, not executed

After independent Wrench review, Boss may commit/push the reviewed ref and
authorize a **new** manual dispatch of
[the targeted workflow](../../../../.github/workflows/stage10-native-stack.yml)
with `platforms=macos-only` (the default). This schedules only macOS ARM64
Debug/Release. `platforms=all` retains the original Windows/macOS four-job scope;
no automatic trigger or seven-job matrix is added. Do not rerun old CI #1:
its immutable source still contains the failing setter.

Required new evidence: exact committed SHA and unchanged-source receipts;
successful native builds/default Mach-O stack receipts; launcher before/after
KiB values; both child receipts showing soft/hard **1048576** bytes; actual
unchanged frontend/depth command exits; JUnit/depth output, native binaries and
complete summaries/artifacts for both configurations. If setup/enforcement fails,
retain the failed evidence and leave the gate open. CI #1 Windows results remain
valid for their recorded SHA; the follow-up must verify production/test bytes
still match that source. No macOS result is inferred from this local validation.

**Windows console/ConPTY remains UNVERIFIED:** headless PE/frontend/depth passes
do not establish real interactive console behavior. Native host limitations and
exclusions remain in the [native-final handoff](../native-final/HANDOFF.md).
No VM investigation, new architecture, stage transition or release lock occurs.
