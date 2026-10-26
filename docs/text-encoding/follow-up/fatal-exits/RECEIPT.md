# Final fatal-exit preservation correction

26 October 2026. Authorized baseline `cc8d47625795db861db459de58f707b8cbd238bb`.
Correction complete; stop for Golden Wrench review.

## Demonstrated defects and narrow repairs

[Before-change proof](before.json) confirms the previous classifier missed ASan's
reserved exit 87 and UBSan's reserved exit 86. It also shows signed Windows status
-1073741819 raised `ValueError` during POSIX signal-name conversion, interrupting
receipt creation. The proof used disposable simulated inputs, not a compiler crash.

`tests/text_encoding/crash.py` now recognizes exactly those two reserved positive
sanitizer statuses in addition to existing native abnormal exits. It does not
classify arbitrary positive statuses as crashes. Ordinary diagnostic exit 1 stays
excluded. Signal names are assigned only on POSIX for valid signal numbers;
Windows-specific or unknown negative statuses retain their exact return code and
use null for the signal field. This narrowly handles metadata interpretation;
no compiler/Runtime exception behavior changed.

`tests/text_encoding/crash_test.py` now uses
`ctypes.windll.kernel32.ExitProcess(0xC0000005)` on Windows, matching the existing IO
test. It no longer relies on Windows `os.abort()` returning a recognizable native
fault status.

The shared implementation, exact source hashes, readonly source copies, explicit
generator provenance, executable hashes, binary streams, editor snapshots and
collision-safe `_crash.void` naming remain intact. No second framework was added.

## Permanent focused cases

The shared self-test now verifies eight intentional disposable incidents:

- Three existing native-abnormal process cases, including checked-run failure,
  colliding filenames, relative executable hashing and a direct Popen editor input.
- Exit 87 through both run and direct Popen, with exact source/provenance and
  NUL/CRLF stdout/stderr checks.
- Exit 86 through both paths, with the same evidence checks.
- One signed Windows status receipt simulation, verifying exact returncode,
  null signal and complete source/provenance/stream evidence.

Classifier negatives include 0, 1, 2, 3, 42, 85 and 88. An actual exit-one subprocess
still produces no incident. Exactly eight receipts are required, preventing silent
positive-exit overclassification or duplicate capture. All copies are verified
before TemporaryDirectory cleanup; repository artifact inventory stays unchanged.
The unchanged IO preservation test additionally verifies its three disposable
compiler/runner/source-free AOT paths.

These are controlled status tests. They do not claim an actual ASan/UBSan defect,
or that native Windows execution occurred on this Linux host.

## Available-profile verification

Only registered preservation tests ran:

```text
^(text_encoding_crash_preservation|io_crash_preservation)$
```

| Profile | Result | CTest seconds | Verbose log | JUnit |
| --- | --- | ---: | --- | --- |
| Debug | 2/2 passed | 1.15 | [Log](debug.log) | [XML](debug-junit.xml) |
| Release | 2/2 passed | 1.12 | [Log](release.log) | [XML](release-junit.xml) |
| Strict sanitizers | 2/2 passed | 1.03 | [Log](sanitizers.log) | [XML](sanitizers-junit.xml) |

[Exact commands, cwd, environment, final source hashes and log hashes](results.json)
record each execution. The strict profile retained:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Approved execution outside the ptrace-restricted sandbox preserved the strict
configuration. These Python synthetic tests validate preservation behavior; no
claim is made that Python status simulations exercise sanitizer-instrumented VOID.
No full IO workload, compiler matrix, rebuild or hosted CI ran.

## Integrity and handoff

All 1,971 files in [the protected evidence manifest](protected-evidence-sha256.json)
match their original hashes, including actual Debug crash evidence and historical
filesystem/optimizer research. No new genuine incident occurred; the genuine root
still contains only `incident-splghldg`. No synthetic snapshot directory remains.

Production compiler, Runtime, stdlib, root README and CMake are unchanged. Existing
historical crash classifications remain unchanged. Native Windows execution remains
pending; its signed exception-status receipt path was simulated on Linux, while
POSIX native-signal preservation and both fatal positive codes ran as subprocesses.
No staging, commits, branches, pushes or hosted CI occurred. Stop for Golden Wrench.
