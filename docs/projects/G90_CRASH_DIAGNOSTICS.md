# G90 — Permanent opt-in crash diagnostics

20 November 2026, final Windows handler correction candidate for focused Golden Wrench re-review.
The original review required three corrections, subsequently verified independently. A final Windows handler-preservation correction is recorded below; earlier findings and evidence remain preserved.
Accepted starting revision: `86d23621477767ef1ac28211ea66c5ed9e49760b` (G89), initially clean.
No staging, commit, branch, push, installation or publication is part of this work.
Linux qualification below is local evidence, not independent acceptance.

## 1. Architecture and native mechanisms

The compiler and language server share a small executable-only diagnostics library.
It is **not linked into `void_runtime` or generated applications**. Normal startup
checks `VOID_CRASH_DIAGNOSTICS`; only the exact value `1` enables supervision.
The disabled path creates no process, mapping, descriptor, report directory,
signal handler, tracing thread or diagnostic file.

When enabled, an invocation has one healthy supervisor and one worker. There is
no daemon, installed debugger, watchdog, automatic retry or restart. Linux/macOS
use native wait status; opted-in Windows launches use ordinary process waits and
collect only owning-process sanitizer findings. The worker
executes the existing compiler/LSP/LLVM/Runtime paths. A 136-byte shared record
contains a 32-entry ring of closed operation identifiers and a sanitizer death
marker containing the owning process PID. It contains no pointers or arbitrary strings.
All shared accesses use lock-free atomic integers. The sanitizer callback and
breadcrumb writer check the owning PID before touching the shared record. A
forked child cannot overwrite the parent's context or death marker. Breadcrumbs are best effort;
concurrent operations can overwrite each other's recent context without causing
an unbounded read or allocation in the supervisor.

- **Linux/macOS:** anonymous shared `mmap`, startup `fork`, native `waitpid` status.
  Fork occurs before compiler/LSP threads start. The worker keeps the inherited
  stack limit, signal mask and dispositions, including `SIGCHLD` behavior. The
  supervisor prevents inherited `SIG_IGN`/`SA_NOCLDWAIT` from losing its worker's
  exit. It forwards directed INT/TERM/HUP/QUIT cancellation; terminal-generated
  group signals are not forwarded twice. Arbitrary caller-generated group
  signals can still reach both processes; applications that count such signals
  should account for the explicitly enabled supervision boundary.
- **Windows:** `CreateProcessW` relaunches the same executable with its literal
  Unicode command line. An inherited mapping handle is passed through a one-use
  environment marker, cleared from the worker's CRT/native environment before
  application work. A handle whitelist admits only the mapping and duplicated
  standard streams. Ordinary `WaitForSingleObject`/`GetExitCodeProcess` preserve
  the exit DWORD. Only an owning-process sanitizer marker authorizes a report;
  unmarked native exceptions are unsupported and never inferred from exit values.
  No debugger is attached and no application exception filter is installed or
  replaced. Application `SetUnhandledExceptionFilter` behavior remains under the
  Windows API contract. Console Ctrl+C/Break remains directed to the worker.
  The explicit AOT adapter launches the application directly, waits normally and
  preserves its exit DWORD; it cannot collect uninstrumented application crashes.
  No arbitrary descendant, register, debug string, module, source or process
  memory is observed. Native Windows compilation/execution remains unqualified.
- **Fatal sanitizer builds:** the enabled worker registers
  `__sanitizer_set_death_callback`. Its callback stores its owning PID atomically; it does
  not allocate, write files, acquire locks, unwind or recover execution. The
  supervisor uses that marker rather than guessing from exit codes 86/87.

No fatal signal/SEH handler is added to the worker. Supported termination evidence
is collected by the healthy supervising process. Stack overflow, worker heap
corruption, worker deadlock and exceptions cannot require the collector to unwind
or allocate inside the failed thread. A deadlock alone does not trigger collection;
there is no hang detector. A collector failure has no recursively registered
collector. Setup failures warn once and continue the ordinary unsupervised path.
Storage/report failures do not replace the worker's termination result.

On POSIX the supervisor re-raises the worker's signal, preserving `WIFSIGNALED`
and shell signal exits. Only the supervisor's duplicate core is suppressed; the
worker's native core policy is unchanged. On Windows the supervisor exits with
the original DWORD. These mechanisms cannot promise evidence if the supervisor
itself is killed, the process tree is forcibly destroyed, the OS/power fails, or
failure occurs before the opt-in entry boundary. Killing a supervisor with an
uncatchable termination can leave its worker alive; use process-tree cancellation
for that case. No permanent job containment changes application child ownership.

### Research decisions

Research preceded implementation. These are established upstream designs/APIs,
not proposed VOID fixes or evidence of native acceptance:

- [LLVM CrashRecoveryContext](https://llvm.org/doxygen/classllvm_1_1CrashRecoveryContext.html)
  and the pinned LLVM 23.1.4 `Support/Program.h`: recovery contexts change execution
  and signal behavior; G90 instead observes termination. LLVM `ExecuteAndWait`
  distinguishes ordinary results, launch failure (`-1`) and crash/timeout (`-2`).
- [Clang diagnostics/crash documentation](https://clang.llvm.org/docs/UsersManual.html):
  compiler crash reproducers can contain input material. G90 does not automatically
  construct source reproducers.
- [Crashpad overview](https://chromium.googlesource.com/crashpad/crashpad/+/main/doc/overview_design.md):
  a separate collector avoids relying on a damaged process. Full dump capture adds
  platform-specific exception transport and sensitive memory. That dependency and
  dump database are unnecessary for this bounded metadata collector.
- [Microsoft MiniDumpWriteDump](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump):
  native dump production belongs outside the failing thread where possible. G90
  does not invoke DbgHelp or configure WER in crash-time code.
- [Apple crash reports](https://developer.apple.com/documentation/xcode/acquiring-crash-reports-and-diagnostic-logs)
  and [LLDB symbolication](https://lldb.llvm.org/use/symbolication.html): native crash
  reports/dumps can be reviewed separately with matching binaries and symbols.
  G90 does not replace Mach exception ports or scrape unrelated OS reports.
- [Native wait status](https://man7.org/linux/man-pages/man2/waitpid.2.html) and
  [coredumpctl](https://www.freedesktop.org/software/systemd/man/255/coredumpctl.html)
  support keeping termination evidence separate from optional native dump analysis.

## 2. Configuration and usage

### Compiler and directly launched language server

```sh
# Existing parent; the collector creates a private leaf only after a crash.
mkdir -p "$HOME/.local/state"
export VOID_CRASH_DIAGNOSTICS=1
export VOID_CRASH_DIRECTORY="$HOME/.local/state/VOID-Crashes" # optional
voidc check example.void
voidc run example.void --no-cache
voidls --entry /absolute/project/main.void

# Disable without rebuilding/reinstalling; both forms disable capture.
export VOID_CRASH_DIAGNOSTICS=0
unset VOID_CRASH_DIAGNOSTICS
```

PowerShell uses `$env:VOID_CRASH_DIAGNOSTICS = '1'` and an optional absolute
`$env:VOID_CRASH_DIRECTORY`. Disable with `Remove-Item Env:VOID_CRASH_DIAGNOSTICS`
or set it to `'0'`.

Default leaf locations (parents must already exist):

| Platform | Default |
| --- | --- |
| Linux | `$XDG_STATE_HOME/VOID-Crashes`, otherwise `$HOME/.local/state/VOID-Crashes` |
| macOS | `$HOME/Library/Logs/VOID-Crashes` |
| Windows | `%LOCALAPPDATA%\VOID-Crashes` |

An explicitly configured report leaf must be absolute. An unavailable parent or
unsafe leaf causes a warning at incident collection, not a compiler failure.
Unsetting the directory override restores platform defaults. Invalid/non-`1`
enablement values are disabled.

### VS Code and VSCodium

Set these in **user/machine settings**, not repository workspace settings:

```json
{
  "void.crashDiagnostics.enabled": true,
  "void.crashDiagnostics.directory": "/absolute/existing-parent/void-crashes"
}
```

The directory setting is optional. The enable setting defaults to `false` and
explicitly overrides inherited environment enablement for every extension-owned
server, preflight, creation, maintenance, capability and compiler-task launch.
The extension supplies the same launch environment policy to all those paths.
Changing settings restarts the server once. Set enablement back to `false` to
stop collection. Neither setting changes protocol tracing; verbose LSP trace is
separate and can contain source text.

After unexpected server closure, Output → VOID retains diagnostic context, native
process exit information and the saved JSON path when collection succeeds. The
server's stderr carries the report path; LSP stdout remains framed protocol only.
The existing one-notification, `DoNotRestart` policy remains. Recovery uses
**VOID: Restart Language Server**. A transport error alone is not proof of a
native crash; ordinary source problems remain normal Problems diagnostics.

### Explicit native/AOT boundary

```sh
voidc build app.void -o /absolute/output/app
voidc crashes run /absolute/output/app "literal argument" --app
```

`crashes run` itself is an explicit opt-in for that invocation, even when the
environment switch is absent/disabled. It preserves literal arguments, working
directory, standard streams and exit results. The executable must be an absolute
path. A normal return such as 17 remains 17 and produces no report. On Windows
this adapter preserves execution and exit status but declines native exception
collection; use separately collected native OS evidence for those failures. Directly
launching the generated binary does not enable G90, even if the environment
variable is set. No handler is silently installed into developer applications.

## 3. Classifications and evidence format

Reports are inspectable schema-1 JSON, named
`crash-<Unix-milliseconds>-<collector-pid>-<serial>.json`.

| Classification | Meaning |
| --- | --- |
| `fatal_signal` | OS-observed SEGV/ABRT/BUS/ILL/FPE/TRAP/SYS termination; not a root-cause diagnosis. |
| `sanitizer_finding` | The sanitizer death callback marked the worker; preserve its actual exit/signal. |
| `child_crash` | Compiler-owned native tool returned LLVM `-2` without a configured timeout. |
| `child_crash_or_timeout` | Header-import tool returned `-2`; its 120-second deadline makes crash versus timeout unavailable. |

TERM/INT/HUP/QUIT cancellation, SIGPIPE, SIGKILL, ordinary nonzero exits (including
86, 87 and 139), launch failures, compilation diagnostics, source-level failures
and typed `Err` results do not become native crash reports merely from their
status. A program can intentionally request a fatal signal or an exception-like
Windows exit value. A normal Windows return or `ExitProcess` with such a value
creates no crash report. Unmarked Windows exception termination also creates no
report; only a matching owning-process sanitizer marker supports collection. Reports state observed termination and do not infer intent.

The `process` identifies `voidc`, `voidls`, `aot-program` or `native-tool`.
The current CLI runs JIT code **in process**, with no separate production JIT
runner. `jit_compile` belongs to the compiler; a termination during `jit_execute`
has boundary `jit-program-or-runtime`. This is an attributed execution boundary,
not proof whether user code, a native provider, Runtime or compiler caused it.
Compiler-owned Runtime teardown outside that invocation remains a host failure.
Native-tool reports are separate child incidents; a tool crash does not fabricate
a `voidc` host crash. Ordinary tool errors retain their existing diagnostics.

Fields include the incident ID, worker/child PID where available, epoch time,
subject and collector versions, LLVM version, compiled target triple (OS/arch),
executable SHA-256, compiler build identity where available, termination namespace
and exact observed code, recent operation identifiers, and identity observation
point. Compiler cache/build identity now includes the diagnostics library itself.
The server version has one shared constant for LSP and report attribution.
External/unknown subject versions and unavailable child PIDs are `null`.

For supervised processes the executable is opened at startup and hashed by the
healthy supervisor only after an incident. Hashing streams 64 KiB at a time with
a finite 1 GiB binary-size limit; unreadable/oversized identity is `unavailable`.
This is not a transactional snapshot of a file modified in place. Native-tool
hashes are explicitly marked **observed after child termination**, because LLVM's
existing launch API does not pin the original executable or expose child PID and
signal in its result. No error-message parsing invents that missing information.

`stack`, `source_location` and `explanation` are explicitly `null`. There is no
safe stack collector attached before worker death, and native stack memory is
not read afterward. Breadcrumbs do not identify a current source span. The report
states the stack limitation instead of inventing frames or a causal explanation.
Keep the matching binary, symbols and source revision when retaining a report.

## 4. Privacy, retention, review, export and deletion

The collector has no network API, upload endpoint, external submission, telemetry
service or automatic sharing. Reports contain no source text, input/source copies,
LSP payloads, environment contents, command arguments, credentials, working/home
paths, arbitrary diagnostic messages, memory pages or unrelated directory scans.
Existing compiler stderr, editor output and sanitizer/native OS reporting retain
their own behavior; G90 does not copy them into its JSON.

Storage is local and bounded to **16 collector reports, each at most 16 KiB**
(256 KiB total report payload). Oldest filename order rotates on the next incident;
there is no periodic retention task or age timer. Directory enumeration is capped
at 256 entries. Unrelated files are not deleted. Unexpected report entries or an
oversized report cause refusal rather than unbounded inspection. Linux/macOS use
an owner-only leaf (`0700`) and exclusive `0600` files, reject symlink leaves and
unsafe existing reports, and enumerate/create/delete relative to the admitted
handle. Windows reuses native staging's protected user DACL and pinned ancestor/
reparse checks, with exclusive file creation.

A nonblocking local storage lock bounds concurrent writers. A collector that
finds the directory busy may omit its report with a warning; it never waits on a
crashed writer. Storage errors, unwritable directories, entry-limit refusal,
unsafe permissions, missing parents and short writes preserve the original exit.
Optional persistence is best effort, not power-loss-durable archival storage.

Use the exact path printed on stderr/Output → VOID. For example:

```sh
ls -l "$HOME/.local/state/VOID-Crashes"
python3 -m json.tool /absolute/report/crash-EXACT-INCIDENT.json
# Deliberate local export of one inspected file; no upload:
tar -czf /absolute/export/void-incident.tgz -C /absolute/report crash-EXACT-INCIDENT.json
# Delete only the report selected after review:
rm -- /absolute/report/crash-EXACT-INCIDENT.json
```

On Windows, Explorer/PowerShell can inspect, copy and remove the selected JSON.
Disabling collection leaves previous reports for deliberate review/deletion.

Additional evidence requires developer action. On a Linux system already
configured for native cores, use the report's **worker PID** with `coredumpctl`
and inspect the selected core with matching symbols. On macOS explicitly select
the matching Apple DiagnosticReports crash log for LLDB symbolication. On Windows
explicitly collect a WER/WinDbg dump using Microsoft's native guidance. Dumps,
sanitizer stderr, verbose LSP traces and source reproducers can contain secrets;
G90 neither enables nor collects those automatically. Existing engineering
`tests/text_encoding/crash.py` preservation remains a separate deliberate test
workflow; its source/environment snapshots are not the production report policy.

## 5. Performance and bounded resources

Release, LLVM/Clang/LLD 23.1.4, Linux x86-64. `tests/crash/performance.py` uses 41
interleaved samples per combination after one warmup, seed 90. The LSP measurement
includes startup, initialize, 100 hover requests and shutdown. The entry control
compiles the accepted G89 `main.cpp`/LSP entry against the current libraries using
current build flags; it isolates entry/breadcrumb additions and is **not a full
independent G89 rebuild**. Raw samples and the control build recipe are retained.

| Median milliseconds | G89 entry control | Variable unset | Explicit `0` | Enabled `1` |
| --- | ---: | ---: | ---: | ---: |
| `voidc --version` | 4.992 | 4.842 | 4.899 | 6.408 |
| small source check | 5.996 | 5.856 | 5.899 | 7.641 |
| JIT O0, no cache | 14.218 | 13.490 | 13.402 | 15.784 |
| LSP 100-hover session | 6.446 | 6.115 | 6.173 | 7.341 |

There is no measurable disabled-path regression in this local sample; negative
median deltas are noise, not a claimed speedup. Enabled launch adds approximately
1.2–2.3 ms here. These are short-launch measurements, not a throughput guarantee.
No disk writes occur without a report, and executable hashing is deferred until
an incident. No optimization, Runtime/GC or scheduler rewrite was introduced.

Seven synchronized blocked-JIT samples per mode measured `/proc/*/smaps_rollup`.
Median summed PSS was 47,414 KiB disabled and 47,657 KiB enabled (+243 KiB).
Summed RSS was 51,060 versus 60,496 KiB; RSS double-counts pages shared across the
supervisor/worker. The measurements in this original qualification table precede
the focused corrections. Shared diagnostics state is now 136 bytes rounded to an OS
mapping page. A healthy invocation retains one supervisor, a pinned executable
stream, that mapping and constant-size breadcrumbs. Report construction/retention
is bounded separately; incident hashing uses a 64 KiB buffer and the read limit
above. Actual process memory includes the existing compiler/LLVM/Runtime.

## 6. Focused qualification

Host: Linux 7.2.11-pikaos, x86-64; pinned LLVM/Clang/LLD 23.1.4 exclusively.
Build trees reused: `build/llvm23-release`, `build/llvm23-debug`,
`build/llvm23-strict` (RelWithDebInfo + fatal ASan/UBSan). No hosted CI was run.

| Check | Result |
| --- | --- |
| Maintained native crash matrix, all three profiles | PASS |
| Enabled/disabled success and exits 1/2/42/86/87/139 | PASS; ordinary exits create no storage |
| Native abort, main-thread and secondary-thread stack exhaustion | PASS; original status survives collection |
| Exact compiler binary/build and server binary attribution | PASS |
| Generated JIT and AOT O0/O2, normal returns and controlled native crashes | PASS |
| Direct generated AOT crash with environment enabled | PASS; no implicit collector/report |
| Real `voidls` worker termination and unmodified protocol framing | PASS |
| Native-tool child crash, no compiler-host misclassification | PASS |
| Repeated incidents, concurrent collectors, count/byte/entry limits | PASS; busy writers may explicitly refuse |
| Missing parent, unsafe/public leaf, symlink leaf, unwritable leaf | PASS; original failure preserved |
| Inherited `SIGCHLD` ignore and directed supervisor TERM forwarding | PASS |
| Original 1 MiB stack checks, including generated execution | PASS on Linux in all three profiles |
| Explicit fatal ASan and fatal UBSan injections | PASS; genuine death markers, actual selected-runtime exits preserved |
| Release focused integrated CTest | 8/8 PASS |
| Debug focused integrated CTest | 7/7 PASS |
| Strict focused integrated CTest | 6/6 PASS; that tree does not register `lsp_const_recovery_stack`, covered by the native matrix |
| Editor build/typecheck and unit tests with matching server | 14/14 PASS |
| Actual VS Code 1.144.0 / VSCodium 1.143.17232 on Linux | PASS in both products |
| Local JSON privacy/resource assertions and static implementation inspection | PASS; no network/upload implementation |

The real editor test exercises default-off launch, ordinary source errors, enabling,
actual native worker ABRT, report path and native signal in Output → VOID, one
notification, no automatic restart, explicit restart and disabling again. It uses
development-extension loading and isolated profiles; it installs/publishes no VSIX.

Representative reproduction commands from the checkout:

```sh
cmake --build build/llvm23-release --target voidc voidls void_crash_probe -j 5
python3 tests/crash/diagnostics.py build/llvm23-release/void_crash_probe \
  build/llvm23-release/voidc build/llvm23-release/voidls

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:exitcode=86:print_stacktrace=1 VOID_G90_STRICT=1 \
python3 tests/crash/diagnostics.py build/llvm23-strict/void_crash_probe \
  build/llvm23-strict/voidc build/llvm23-strict/voidls

VOID_CRASH_DIAGNOSTICS=1 ctest --test-dir build/llvm23-release --output-on-failure \
  -R '^(crash_diagnostics|cli_contract|cli_presentation|cli_report_path|lsp_editor_recovery|lsp_const_recovery_stack|runtime_failures|runtime_invariants)$'

# Use the existing pinned Node 24.19.0 environment:
VOIDLS_TEST_SERVER="$PWD/build/llvm23-release/voidls" npm --prefix editors/vscode test
VOIDLS_PATH="$PWD/build/llvm23-release/voidls" \
VSCODE_EXECUTABLE_PATH=/absolute/existing/editor/executable \
VOID_G90_EDITOR_EVIDENCE=/absolute/task-evidence/editor \
node editors/vscode/out/test/runCrashHost.js
```

All crash injections are maintained test fixtures or explicit native signals to
controlled workers. There is no production crash-injection switch, LSP crash
method or test-only behavior in the compiler/Runtime. A deliberately signalled
worker is evidence of collection behavior, not an underlying compiler defect.

## 7. Platform support and limits

Linux x86-64 has the execution evidence above. macOS POSIX support is
**IMPLEMENTED, NATIVE EXECUTION UNQUALIFIED**. Windows ordinary launch/wait and
owning-process sanitizer collection are **IMPLEMENTED, NATIVE COMPILATION AND
EXECUTION UNQUALIFIED**. Windows native exception collection is **UNSUPPORTED**,
including explicit AOT launches and compiler-owned native tools. No Unix test,
source review or cross-compilation qualifies native Windows/macOS behavior.
Other architectures are unqualified. The real-editor crash injection test targets
Linux `/proc`; Windows has a separate focused native behavior/marker matrix.

Windows diagnostics attaches no debugger and occupies no debug port. The earlier
launch-scoped debugger was removed because Windows bypasses registered top-level
exception filters while a process is being debugged. Passing first-chance events
back to Windows did not preserve those filters. Normal waits preserve program
handling; a numeric DWORD, including `-2` or `0xC0000005`, is never crash evidence.
Unmarked exceptions, fail-fast and uninstrumented stack failures produce no G90
report. A genuine fatal sanitizer callback in the owned worker remains supported;
its native Windows behavior still requires qualification. AOT binaries are not
instrumented by G90. Use separately collected OS diagnostics where needed.

Windows compiler-owned tool launches retain their original diagnostics/status
but cannot produce a proven exception report. POSIX native-tool crash/timeout
receipts remain supported. This deliberate capability limit preserves application
behavior and does not reinstate numeric-exit guessing.

macOS records POSIX termination, not Mach exception codes. Linux does not infer
fault address, signal sender, OOM cause or crash frames from a wait status.
SIGKILL/cancellation are intentionally outside native-crash classification.
Native dumps/OS retention remain separate policy.

## 8. Preserved evidence and corrections

Task-owned evidence is outside the checkout at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-G90-evidence/`.
It retains raw status/stdout/stderr records, inspectable reports, editor profiles/
logs, compiler entry controls, raw timing/memory samples and build/test logs.
`manifest.json` records SHA-256 identities of changed source files, qualification
binaries and retained evidence. It is a local engineering record, not an upload.

Original unfavorable records are retained rather than overwritten:

- Initial C++ initializer/fixture include compilation failures and corrected builds.
- Initial LSP test failures: its hand-built initialization/shutdown messages did
  not follow the existing protocol (`capabilities`, initialized object, absent
  shutdown parameters). The fixture was corrected; production LSP semantics were
  preserved.
- Initial editor unit runs used stale `build/debug/voidls`; using the existing
  `VOIDLS_TEST_SERVER` override selected the current pinned-toolchain server.
- Initial README link test exposed a pre-existing relative G87 documentation link
  that could not resolve from an unpacked extension. It now uses its canonical
  HTTPS target alongside the new crash guide.
- Initial VSCodium host observation assumed VS Code's profile log location. Both
  product tests now use an explicit isolated `--logsPath`; the original failed
  run had already produced its crash report and remains preserved.
- Initial CLI contract run expected 15 menu entries. The maintained contract now
  includes `crashes` and verifies all 16 command/help rows.
- Pilot timings, intermediate successful qualification and final measurements all
  remain separately labeled. The selected combined sanitizer runtime actually
  returned **86 for both injected ASan and UBSan failures**, despite the distinct
  requested exitcode options; the raw stderr/status remains intact. Classification
  uses the death callback and therefore does not rely on that convention.

No pre-existing Runtime/compiler defect is claimed repaired by these controlled
crash tests. Existing historical incidents and native acceptance limits remain.

## Focused Golden Wrench correction record

The original review and reproductions remain unchanged at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g90-wrench-x6nmwwxw/`.
Correction evidence is separately retained at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-G90-corrections-evidence/`.
The original G90 evidence manifest above describes the pre-correction candidate;
`manifest.json` in the corrections directory identifies the corrected source,
profile binaries and follow-up evidence. No earlier failure record was replaced.

### P1 — Process ownership across fork

The inherited callback no longer writes an unqualified shared flag. `arm` records
its worker PID; the callback and context writer require that PID to equal the
current process. The supervisor accepts a sanitizer marker only for the PID it
actually waited for. A child report through the supported POSIX native-tool
boundary remains a separate incident; child sanitizer stderr is preserved by the
explicit engineering harness without fabricating a parent termination.

Maintained regressions cover child breadcrumb writes, a child ASan failure with
parent exit 0, a nested grandchild ASan failure with parent exit 0, a child ASan
failure followed by a real parent ABRT, and a genuine host sanitizer failure.
The child findings remain visible in raw stderr; successful parents produce no
host report. A subsequent parent ABRT remains `fatal_signal`, not the child's
`sanitizer_finding`.

The exact original Golden Wrench `probe.cpp` and `reproduce.py` were copied into
`wrench-replay/` and rebuilt/relinked using its recorded commands against the
corrected strict library. Replay succeeded: **parent exit 0, genuine child ASan
finding retained, no `fork-reports` directory and no fabricated host report**.
The original fixture, binary, report and reproduction files were left untouched.

### P2 — Equivalent directory spellings

Directory normalization removes the terminal empty component left by lexical
normalization of `leaf/`, `leaf/.` and repeated separators. It then applies the
same absolute/private-leaf, ownership, permission, handle-relative and no-symlink
checks. Both new and existing Unicode/space-containing directory leaves are
covered. Relative paths, root-only selection, missing parents, regular-file leaves,
public leaves and symlink leaves remain refused, including symlink `/.` spellings.
No ancestor is newly authorized or created by normalization.

### P2 — Windows normal completion versus exception termination

[GetExitCodeProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess)
can return ordinary `main`/`ExitProcess` values or exception values. Neither a
recognized status nor LLVM's numeric `-2` is used to infer a Windows crash.
The first correction used launch-scoped debug events and passed its Linux policy
tests; those historical results remain in the original correction evidence.
Golden Wrench's subsequent source/API review found that debugger attachment
bypasses application top-level exception filters. That implementation and its
policy-only tests have now been removed. The final correction below supersedes
its handler-preservation and native exception-observation claims.

### Earlier correction results (before the final handler correction)

| Corrected-candidate check | Local result |
| --- | --- |
| LLVM 23.1.4 Release/Debug/strict builds | PASS |
| `corrections.py` on Release and Debug | PASS, native POSIX tests with 1 MiB stack limits |
| `corrections.py` on fatal ASan/UBSan strict | PASS, including nested child sanitizer isolation and real host failure |
| Original Golden Wrench fork reproducer relinked to corrected library | PASS; successful parent emits no crash report |
| Focused CTest: exception policy, corrections, CLI report path | 3/3 PASS in each profile |
| Original G90 Release crash matrix | PASS; recorded in `release-regression.log` |
| Native Windows/macOS execution | UNQUALIFIED; no native run or compile acceptance is inferred from Linux |

Correction-only Release timings used the same 41-sample interleaved script and
retained G89 entry controls (not an independent full baseline rebuild):

| Median milliseconds | Entry control | Unset | Explicit `0` | Enabled `1` |
| --- | ---: | ---: | ---: | ---: |
| version | 4.889 | 4.670 | 4.839 | 6.006 |
| source check | 5.894 | 5.617 | 5.631 | 7.116 |
| JIT O0/no cache | 13.200 | 12.747 | 12.783 | 14.460 |
| LSP 100-hover session | 6.047 | 5.996 | 6.123 | 7.026 |

Raw samples are in correction `performance.json`. There is no measurable disabled-
path regression in this local sample. The disabled context check short-circuits
before any PID query; the enabled owner check adds only a native PID query and
scalar comparison per breadcrumb. No source tracing, new file writes or storage
retention changes were introduced. Those timings precede removal of the Windows event observer and do not qualify
Windows performance. The original 1 MiB stack limit is not enlarged by these corrections.

The initial attempt to request a newly registered CMake target during automatic
regeneration saw the old generated make target inventory. Subsequent target
builds completed; all original build logs remain in correction evidence. No
source/compiler failure or new architectural gate is inferred from that build
invocation issue. The Daybreak UI interruptions are not compiler test results.

## Final focused Windows handler correction

Independent review:
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g90-rereview-cbj85rf8/review.json`.
The original Windows fixture and all earlier review/source/evidence records are
unchanged. Final engineering evidence is separately retained at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-G90-windows-handler-evidence/manifest.json`.
This is a corrected candidate awaiting independent review, not integration approval.

### API decision and resulting behavior

Microsoft documents that
[SetUnhandledExceptionFilter](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter)
invokes the application callback only when the process is not being debugged.
[UnhandledExceptionFilter](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-unhandledexceptionfilter)
passes the exception to the debugger otherwise. Therefore returning
`DBG_EXCEPTION_NOT_HANDLED` was insufficient. The smallest correction removes
all G90 debugger launch flags and event handling, retaining ordinary waits and
PID-owned sanitizer markers. Unsupported unmarked Windows observation is declined.
No replacement exception filter, debugger service or new architecture was added.

`tests/crash/windows_filter.cpp` is a byte-for-byte copy of Golden Wrench's
original fixture. Its required native result is exit 17 without a report both
directly and through `crashes run`, with diagnostics disabled and enabled.
The maintained worker probe additionally exercises the same top-level recovery
inside an opted-in worker. The focused native Windows matrix retains numeric
exception-shaped returns, handled SEH, `ExitProcess`, normal JIT O0/O2 returns,
unsupported unmarked exceptions, supported owning-worker sanitizer reports and
native-tool non-attribution. It is registered only for native Windows; no Linux
simulation is counted as execution evidence. The POSIX matrix remains separate.

### Final qualification

Results and source/binary/evidence hashes are recorded in the final manifest.

| Final-candidate check | Actual local result |
| --- | --- |
| LLVM/Clang/LLD 23.1.4 Release, Debug and fatal ASan/UBSan strict rebuilds | PASS |
| Release CTest: crash matrix, corrections, CLI report path | 3/3 PASS |
| Debug CTest: corrections and CLI report path | 2/2 PASS |
| Strict CTest: corrections and CLI report path | 2/2 PASS; child/nested-child isolation and genuine host sanitizer findings retained |
| Original fork fixture rebuilt/relinked against final strict library | PASS; parent 0, child ASan evidence retained, no parent report |
| Source ownership/path/breadcrumb comparison against reviewed candidate | Unchanged |
| Golden Wrench Windows handler fixture | Byte-identical maintained copy; native execution UNQUALIFIED |
| Final source inspection and whitespace check | No debugger APIs; `git diff --check` PASS |

The POSIX regression harness retains its exact **1,048,576-byte stack limit**;
no stack requirement was relaxed. Windows assertions now exercise the deliberately
supported behavior/marker scope rather than claiming unsupported exception reports.
The disabled path and context ownership checks are unchanged; no new performance
measurement is claimed. Earlier timing results remain historical evidence.
Native Windows compilation/execution and macOS execution remain **UNQUALIFIED**:
this Linux session exposes no native Windows command host, mounted Windows host
or configured Windows SSH target. The Windows fixture is maintained and pending
native execution. Earlier independent fork/path fixes remain preserved.

## 9. Exact changed-file inventory

Inventory includes tracked edits and new maintained files, excludes generated
build/editor outputs and task-owned external evidence. No other file is intended
for the G90 candidate.

<!-- G90 inventory generated from the final working-tree status below. -->

- `CMakeLists.txt`
- `docs/CLI.md`
- `docs/EDITOR_INTEGRATION.md`
- `docs/LANGUAGE_SERVER.md`
- `docs/README.md`
- `docs/projects/G90_CRASH_DIAGNOSTICS.md`
- `editors/vscode/README.md`
- `editors/vscode/package.json`
- `editors/vscode/src/config.ts`
- `editors/vscode/src/crashDiagnostics.ts`
- `editors/vscode/src/creation.ts`
- `editors/vscode/src/extension.ts`
- `editors/vscode/src/libraryManagement.ts`
- `editors/vscode/src/newProject.ts`
- `editors/vscode/src/projectTask.ts`
- `editors/vscode/src/test/crashDiagnostics.test.ts`
- `editors/vscode/src/test/crashHost.ts`
- `editors/vscode/src/test/runCrashHost.ts`
- `include/void/crash_diagnostics.hpp`
- `include/void/lsp.hpp`
- `src/cli_help.hpp`
- `src/diagnostics/crash_diagnostics.cpp`
- `src/lsp/main.cpp`
- `src/main.cpp`
- `src/native_build.cpp`
- `src/native_import.cpp`
- `tests/cli/contract.py`
- `tests/crash/corrections.py`
- `tests/crash/diagnostics.py`
- `tests/crash/performance.py`
- `tests/crash/probe.cpp`
- `tests/crash/windows_filter.cpp`
