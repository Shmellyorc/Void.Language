# Step 42: final macOS Release console fixture investigation

24 October 2026. Baseline `9a4985f89f40c56b8eaa984775faf18c52b69f19`.
**Stage 10 and Step 42 remain INCOMPLETE. Golden Wrench review pending.**

## Independent hosted observations

[Main CI #98](https://github.com/Shmellyorc/Void.Language/actions/runs/40091828693)
and [ThinLTO experiment #3](https://github.com/Shmellyorc/Void.Language/actions/runs/40091853317)
each passed six of seven jobs on the same commit. All configurations except macOS
Release passed, including macOS Debug. Job API snapshots for both runs and both
macOS Release failure logs are preserved under this directory. The two failures
are investigated separately; no causal connection to ThinLTO is asserted.

- Main macOS Release: 198/199; only `runtime_console_native_terminal` failed.
  Exact observed report for `redirected-input`:
  `['1 2002 2 ', '0 80 24', '3 80 24', '']`.
- Experiment macOS Release: 198/199; only `runtime_console_native_transfers` failed.
  `--closed-input-while-output` reported `cancelled write lost native prefix`.
  Its old diagnostic did not include a numeric completed count. Zero progress is
  consistent with the demonstrated scheduling hole; it is not an observed native
  numeric value recovered from that log.

## 1. Redirected input: the test raced its own zero-timeout poll

**Classification: fixture delivery synchronization, not demonstrated production
input loss or misclassification.**

The native terminal probe calls cursor_position, then `read_bytes(4096,0)` once.
For pipe stdin, output cursor-position capability is absent and the query returns
unsupported. `Context::present` returns on that adapter error before before_report,
any response read or any queue erasure. Acquiring/releasing the input lease does
not consume native bytes. A zero-timeout read on a still-empty pipe correctly
returns would_block with an empty byte vector.

The old Python fixture launched the child and then wrote its 12-byte payload.
There was no ordering guarantee between the child's nonblocking read and that
parent write. The original byte assertion wrongly assumed the write had already
happened. Neither Popen nor process admission establishes that ordering.

A deterministic Linux reproduction withholds delivery until the old probe exits;
no sleep is used. It reproduces the **exact four rows** above. A second same-process
Runtime probe provides an explicit READY barrier after the initial empty poll.
Only then does the parent supply the original payload. Actual Debug and strict
records report:

```text
input_kind=3 query_error=3 first_error=4 first_bytes=0 later=7265646972656374656400ff
```

Kind 3 is the existing pipe enum, error 3 is unsupported, and error 4 is
would_block. All 12 bytes are returned intact by the later read on the same
Context, including NUL and FF. Thus the empty poll is not evidence that those
bytes were consumed or lost from the shared queue. Production code is unchanged.

**Repair:** the fixture creates an ordinary pipe, writes the complete original
payload before launching the child, closes the writer, passes the reader as stdin,
and closes only its parent-owned reader afterward. Input remains a pipe, not a
file or another hidden input channel. The original unsupported-query, no-terminal
output, exact payload and termios checks remain unchanged. Native endpoint and
read semantics are unchanged. Both local final configurations record the exact
same first three rows and final payload `7265646972656374656400ff`.

## 2. Cancelled output: admission did not prove native progress

**Classification: fixture progress synchronization, exposed by the weaker macOS
barrier, not demonstrated production loss of an accepted prefix.**

The old fixture waited for a started flag and one admitted operation. With missing
stdin it also waited for the output operation's owned wake pipe to occupy fd 0.
On Linux it additionally inspected the worker's poll syscall. On macOS that last
barrier was absent. Admission and wake-pipe creation both precede the first native
write. Even the first native readiness poll alone does not prove any bytes were
written. Calling close at that point can legally cancel with completed=0.

A deterministic existing-Adapter/Runtime reproduction holds an admitted write
before progress, requests cancellation, then releases the operation. Debug and
strict both produce:

```text
admitted=1 completed=0 error=6
```

This demonstrates the invalid implication in the old fixture. It is an adapter
schedule model, not a macOS syscall trace or a reproduction of the hosted numeric
counter. Existing production rules correctly retain any actual entered progress;
no counter, cancellation or cleanup code is changed.

**Repair and actual native boundary:** the Python parent creates a fresh stdout
pipe, retains its read end and does not drain it until the child finishes. The C++
fixture first verifies that this pipe is writable **before** starting the worker.
After admission, it waits using the existing bounded yield helper until a
zero-timeout native poll reports that stdout is no longer writable, without any
POLLERR/HUP/NVAL. The only writer is the Runtime operation, and no reader drained
the pipe. Therefore that transition proves that native writes have used the
pipe's writable capacity. It cannot be reached merely by allocating bytes,
admitting an operation or creating wake descriptors.

The observation uses only poll on fd 1: it does not open an fd, consume bytes,
change flags, or accidentally reopen the deliberately missing stdin. EINTR is
retried. The original Linux syscall barrier is retained. Only after the native
progress boundary does the test perform its existing absent-stdin/EBADF check,
precise GC, close and join. The positive-but-partial completed count, closed/error
kind, frozen GC root, admission drain, borrowed flags and exact parent-observed
`W` prefix assertions are all retained.

Raw `.progress.json` data are incorporated into the existing driver report and
failure diagnostics. Actual Linux Debug and strict observations, for both ordinary
and missing-stdin write cancellation, were:

| Observation | Value |
| --- | ---: |
| Initial poll result / revents | 1 / 4 (POLLOUT) |
| Progress-boundary poll result / revents | 0 / 0 |
| Cancelled write completed bytes | 65,536 |
| Error kind | 6 (closed) |

The parent verified exactly 65,536 `W` bytes, and all existing GC/ownership/cleanup
assertions passed. This is an actual Linux native observation, not a promised
macOS pipe capacity; native macOS counts may differ and must satisfy the unchanged
`0 < completed < 1 MiB` and exact-output checks.

## Focused validation

- Debug **11/11**, strict ASan/UBSan/leaks **11/11**: six console Runtime drivers
  plus five native endpoint/transfer/input/keyboard/terminal suites.
- Each changed native fixture repeated **20/20** in each configuration: 40 focused
  repetitions per configuration. Per-iteration stdout/stderr/status/timing is in
  `debug-repeat-records.json` and `san-repeat-records.json`.
- Both deterministic scheduling reproductions pass in Debug and strict. The old
  terminal executable, unchanged accepted Runtime archives and virtual Adapter
  contract were used; raw records distinguish native observations from models.
- Only the C++ transfer **test executable** was rebuilt with the accepted LLVM
  19.1.7 configuration flags. The Runtime archives and other five test executables
  are the preserved CI97 artifacts; no production rebuild or full CMake/JIT/AOT
  acceptance is claimed. This mission changes only three test files and records.
- Strict uses the established untraced profile from the start, with
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. No suppression,
  timeout increase, arbitrary sleep, removed assertion or Runtime repair was used.
- Exact diff/reverse-apply check, Python compilation, source freeze, production
  hashes, both workflows, original CI97 binaries, Step41 evidence and original
  1 MiB protections are verified. Prior CI97 investigation/evidence is untouched.

## Remaining native verification, controlled by Boss

Golden Wrench reviews this diagnosis, `repairs.diff`, raw records and updated
`step42-results.json` before Boss commits or launches another hosted run.
This session has no native macOS host and has dispatched nothing.

On a matching repaired macOS **Release** build, rebuild the changed transfer test
and rerun the two original native suites, including repeated attempts. Record the
terminal report and the new progress metadata with exact observed output counts.
The full Release acceptance and the other six previously green configurations
remain Boss-controlled hosted gates, not inferred local passes.

```sh
cmake --build build/Release --target void_runtime_console_transfers --parallel 3
ctest --test-dir build/Release --output-on-failure --repeat until-fail:20 \
  -R '^(runtime_console_native_terminal|runtime_console_native_transfers)$'
cat build/Release/step29-native-terminal-results.json
```

The transfer driver prints its raw progress/flags records into the existing CTest
log; the terminal driver retains its existing native JSON report. Preserve both
and the full native failure output if another boundary fails. No workflow change
is needed. This is not a Step42/Stage10 lock or authorization to start Steps43–44.
