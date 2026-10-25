# Stage 10 Windows console host checklist — 24 October 2026

## Executed Windows 10 VM checkpoint — 25 October 2026

The [Golden Wrench VM review](windows10-vm-2026-10-25/WRENCH_REVIEW.md) now records genuine attached **classic Console Host** results: native Debug/Release builds, focused CTests **4/4**, ten scripted native console flows **10/10** (explicit output CP 65001), and four Boss-attested physical Up-arrow tests **4/4** (Debug/Release × closed/unclosed). Original native reports, machine receipts, successes and prior failures are preserved **privately by Boss**, not published with machine-identifying paths. The raw-archive digest and public derived results are [recorded here](windows10-vm-2026-10-25/evidence-manifest.json). The corrected read-only observer's one-line native-exit-status fix is prepared in `observe.ps1`, based on the successful evidence-only Windows copy.

**Remaining:** Windows Terminal/ConPTY, actual modifier/down/repeat values, in-process restoration/error snapshots, UTF-16 API/partial-progress traces, borrowed sync/overlapped cancellation/lifetime and complete rendered presentation remain **UNVERIFIED**. The following 24 October "NOT EXECUTED" host-access statement is retained as a historical checkpoint, **not** a claim about the later VM work. Stage 10 remains INCOMPLETE and no additional CI or release lock is implied.

**NOT EXECUTED. No interactive Windows host is available in this Linux session.**
No console/ConPTY acceptance or Stage 10 lock is issued. Golden Wrench/Boss must
review this bounded handoff before host execution. No VM investigation occurred.

## Evidence preserved

| Independent observation | Exact source / result |
|---|---|
| [Full ThinLTO CI #4](https://github.com/Shmellyorc/Void.Language/actions/runs/40118666209) | `90c0cf64b9aba60d7c642096d2aaac45aae70d56`; 7/7 jobs, 1,383/1,383 main CTest executions. [Original records](../ci4/summary.json) remain unchanged. |
| [Targeted stack CI #1](https://github.com/Shmellyorc/Void.Language/actions/runs/40147114803) | `b490579f84a702ec5d11da51a82934a498f88afa`; Windows Debug/Release passed both actual 1,048,576-byte PE reserves and original frontend/depth tests. [Original records](../native-stack-ci1/REPAIR.md) retain both macOS setter failures. |
| [Targeted stack CI #2](https://github.com/Shmellyorc/Void.Language/actions/runs/40152738145) | `383d2ce0a404a4bbf3df01d43097de100f5829a1`; 2/2 macOS ARM64 Debug/Release jobs passed native Mach-O gates, both 1 MiB inherited limits and original frontend/depth tests. [Run](ci2/run.json), [jobs](ci2/jobs.json), [complete log receipts](ci2/summary.json), [artifact metadata](ci2/artifacts.json). Boss reports Golden Wrench independently reviewed the logs. |

CI #2 job IDs: Release **121047503258**, Debug **121047503305**. Each log has two
`stack inherited (KiB): soft=1024 hard=1024` records, frontend 1/1 and both deep
receiver success messages. The child byte-limit and Mach-O gates are required by
the successful recorded helper; individual JSON receipts remain in hosted artifacts.
The two complete connector-decoded logs are retained as gzip with hashes. Artifact
ZIPs were not downloaded/rehashed; metadata digests and finite retention are explicit.
These stack results close their exact stack gates, not Windows console behavior.
CI #98/experiment #3 failures and original Step 41 safety evidence are unchanged.

## What the existing sources actually exercise

| Existing entry point | Genuine native observation available / limitation |
|---|---|
| `void_runtime_console_keyboard.exe 2 closed` or `2 unclosed` | Uses `Runtime::console_context()` and real standard handles; nested mode entry, rejected non-LIFO close, explicit outer close or shutdown restoration path. Prints key enum, known flags, repeat and bytes. Does **not** print actual modifier mask or key-down value, native mode snapshots or close error. A successful exit alone cannot prove exact restoration. |
| `void_runtime_console_keyboard.exe shared` | Real shared process Streams, foreign-owner rejection, GC roots, concurrent Unicode stdout/stderr, mixed key/line reads, explicit nested restoration and independent Runtime shutdown. Input deadline 5 s; ordinary key probes 2 s. Does not cancel an entered native borrowed-handle operation. |
| `voidc.exe run tests/stage10/console_prompt.void` | Real Unicode prompt/output and `read_line` on attached handles, through the production Windows wide boundary. Input/output API calls and UTF-16 unit counts require native tracing; visible text is insufficient to certify hidden NULs or partial units. |
| `console_keys.void` | Existing source mode/session cleanup, key echo and ten **events**, not ten key presses. Errors are printed and do not necessarily cause nonzero exit. Physical releases/modifier-only events can consume slots; do not reuse the POSIX byte-sequence expectation as a Windows assertion. |
| `console_terminal.void` / `void_runtime_console_terminal.exe REPORT present` | Native capabilities, size/cursor/palette/clear/title/bell; C++ report file avoids redirecting standard handles. Source intentionally tests invalid title and may print `missing queued input` without deliberately supplied input. Neither is a blanket assertion of all successful operations. Presentation intentionally changes title/colors/cursor; use a disposable console. |
| Six normal `runtime_console*` CTests | Existing injected lifecycle/input/transfer/mode/wide/presentation checks remain valid regression evidence. They do not certify a physical console or native cancellation. |
| `--cancel-read`, `--cancel-write`, `--copy`, native input subprocess modes | Guarded by `#ifndef _WIN32`; do **not** invoke on Windows and label a zero exit native acceptance. Windows may fall through to the injected suite. CMake's five POSIX native registrations are UNIX-only. |

Inspection: `tests/runtime/console_keyboard.cpp:29–56, main`,
`console_transfers.cpp:315–338`, `console_input.cpp:185–237`, `console_wide.cpp`
(`Script:Adapter`), `console_terminal.cpp:52–67`, `CMakeLists.txt:374–414`,
and the source fixtures above. [Inspected source hashes](source-inputs.json)
freeze actual bytes; line numbers are navigation aids, not identity substitutes.

Production inspection found no newly demonstrated defect: real console endpoints
select `ReadConsoleW`/`WriteConsoleW`; keys select `ReadConsoleInputW` and
`PeekConsoleInputW`. `console_keyboard.inc` saves/restores input mode and verifies
applied/read-back masks; SHIFT/ALT/CTRL map to Runtime bits **1/2/4**. No console
code-page setter was found. `console.cpp` borrows standard handles, queries real
file mode, uses a dedicated native worker, targets its worker for synchronous
cancellation and its exact OVERLAPPED for asynchronous cancellation, and waits
for actual completion before releasing storage. These are source findings, not
native measurements. [Microsoft cancellation guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)
requires distinguishing cancellation requests from actual completion.

## 1. Native host and source gate

Use a Boss-supplied native Windows x64 developer PowerShell, first in classic
Console Host and then Windows Terminal. Record OS build, terminal version,
keyboard layout, input method and exactly how the session was launched. Windows
Terminal is a real ConPTY-backed host; an explicit embedding/ConPTY arrangement
needs its own owner-supplied host identity. `WT_SESSION` alone proves neither
physical input nor handle kind. No new ConPTY driver is proposed.

Use Boss-identified matching Debug and Release builds. Stack CI artifacts contain
`voidc`/`void_tests`, not necessarily the console probe targets. If missing, reuse
the existing Step 42 LLVM **19.1.7**/SDK/CMake configuration and build only:

```powershell
$b = (Resolve-Path 'build/native-stack-Debug').Path # repeat with the actual Release directory
cmake --build $b --parallel 3 --target voidc void_runtime_console_keyboard void_runtime_console_terminal
if ($LASTEXITCODE -ne 0) { throw 'Native target build failed' }
```

Do not invoke stack-workflow `identity` again against a used build/cache. Do not
rerun frontend/depth or the full matrix just to repeat accepted evidence. Retain
the existing source/binary receipts, CMakeCache/configuration/build logs and SDK
identity. A directory name alone does not identify a binary's configuration.
Verify inspected source before **and after** the focused session, from repo root:

```powershell
python -c "import hashlib,json,pathlib,sys; p=pathlib.Path; m=json.loads(p(sys.argv[1]).read_text())['sha256']; bad=[f for f,h in m.items() if not p(f).is_file() or hashlib.sha256(p(f).read_bytes()).hexdigest()!=h]; print(json.dumps({'files':len(m),'mismatches':bad})); sys.exit(bool(bad))" docs/stage10/engineering/windows-console-final/source-inputs.json
if ($LASTEXITCODE -ne 0) { throw 'Inspected source mismatch; stop and reconcile identity' }
```

This is an inspection receipt, not the entire compiler's build provenance. Pair
it with the matching build/source receipt; do not infer tested SHA from hashes of
a few files. `$source` below is the exact **Boss-verified** build commit, never a
guess. This handoff performs no Git operation to choose or switch a ref.

## 2. Direct-handle execution (both configurations, both host kinds)

The small [observer](observe.ps1) only queries before/after parent console modes
and code pages and saves JSON, actual executable SHA-256, argv and child exit.
It never reads input, changes modes/code pages, supplies keys or captures child
stdout/stderr. It rejects redirected standard handles, preserves failed receipts
and refuses to overwrite evidence. It has **not** been parsed/run by PowerShell
or exercised on Windows here. Native invocation is still a required gate.

```powershell
$ErrorActionPreference = 'Stop'
$source = Read-Host 'Exact 40-hex source commit from the Boss-verified native build receipt'
$hostDescription = Read-Host 'Actual Windows build, terminal version, keyboard layout/IME and launch arrangement'
$observer = (Resolve-Path 'docs/stage10/engineering/windows-console-final/observe.ps1').Path
$out = 'build/windows-console-evidence/Debug-ClassicConsole' # unique directory per host/configuration
New-Item -ItemType Directory -Force $out | Out-Null
$common = @{ Configuration='Debug'; SourceCommit=$source; HostKind='ClassicConsole'; HostDescription=$hostDescription }
& $observer @common -Evidence "$out/key-closed.json" -Executable "$b/void_runtime_console_keyboard.exe" -Arguments @('2','closed')
if ($LASTEXITCODE -ne 0) { throw 'Native key-closed failed' }
& $observer @common -Evidence "$out/key-unclosed.json" -Executable "$b/void_runtime_console_keyboard.exe" -Arguments @('2','unclosed')
if ($LASTEXITCODE -ne 0) { throw 'Native key-unclosed failed' }
& $observer @common -Evidence "$out/shared.json" -Executable "$b/void_runtime_console_keyboard.exe" -Arguments @('shared')
if ($LASTEXITCODE -ne 0) { throw 'Native shared probe failed' }
& $observer @common -Evidence "$out/prompt.json" -Executable "$b/voidc.exe" -Arguments @('run','tests/stage10/console_prompt.void')
if ($LASTEXITCODE -ne 0) { throw 'Native prompt failed' }
& $observer @common -Evidence "$out/presentation.json" -Executable "$b/void_runtime_console_terminal.exe" -Arguments @("$out/presentation.txt",'present')
if ($LASTEXITCODE -ne 0) { throw 'Native presentation failed' }
```

Run each command separately after reading its instructions. Never pipe the
observer/child, use `>`, `2>&1`, `Tee-Object`, a capture variable, or substitute
`Start-Process -Redirect*` when claiming real console I/O. The source fixtures and
probe report file are allowed to write their own evidence files. Use the host's
screen recording/capture for visible input/output; retain actual child handles
and host arrangement. [Microsoft console handle documentation](https://learn.microsoft.com/en-us/windows/console/console-handles)
explains why piping changes the path exercised. A ConPTY host's transport pipes
are distinct from a console client's own standard handles.

For each `2 closed/unclosed`: start with no pending keys; after READY press and
release **unmodified Up** once. On a host delivering down/up separately, expect
two Up records (enum 5), known flags true and bytes empty; retain actual repeat
counts. Do not synthesize KEY_EVENTs. If that host does not deliver this pair,
record its actual behavior without changing the count/assertions. An initial
availability failure is a real failed attempt: use a fresh disposable session
for any retry; never flush away unknown queued input. Do not type before READY.

For `shared`, after READY supply `é`, Enter, `Z`, Enter within the original 5 s.
Require `DONE:é`, complete `O0..O31` and `E0..E31` Unicode records, no Runtime
error and zero exit. Clipboard/IME input is labeled as such and proves text/queue
behavior only. `shared` is a shared-input test, not a physical-key metadata test.

For `prompt`, type/paste `é🎮` then Enter. Observe `Name é🎮: `, `Hello, é🎮`
and reached `done`; label physical/IME/paste provenance. For presentation inspect
every reported operation/error and actual terminal title/glyphs/palette/cursor;
zero exit alone is insufficient. Its intentionally rejected control-bearing title
must remain rejected. Presentation may leave the cursor hidden; disposable host
teardown is distinct from Runtime input-mode restoration.

Existing source fixtures can additionally run directly with the same observer
and original `voidc run` arguments: `console_keys.void` for source cleanup/ten
events, `console_terminal.void` for its full 16-color/Unicode presentation. Treat
their printed errors as results requiring inspection, not automatic passes. Do
not add sleeps, extend 2 s/5 s deadlines or promise exact key rows across hosts.
The already-green JIT/O2/AOT tests remain regression evidence; no full cross-product
rerun is needed before determining whether these native observations are obtainable.

## 3. What this observer cannot close

Equal before/after masks/code pages certify only the parent-visible final console
state. PowerShell/host cleanup can mask a child's failure. Parent handle validity
cannot prove the child's borrowed handles survived; each process has its own
handle table. The observer does not record active nested modes or actual Runtime
close errors, API calls/unit counts, physical modifier/down values, entered I/O,
GC retention or cancellation isolation. Its JSON always labels console acceptance
UNVERIFIED. Do not promote it to an independent in-process restoration receipt.

The smallest justified additional **test-side** observation, if a real host is
provided, is an opt-in sidecar in the existing native keyboard/transfer fixtures:

| Required fact | Exact observation boundary; acceptance |
|---|---|
| Physical metadata | Log the existing returned `KeyResult`'s `modifiers`, `key_down`, both known flags, repeat, key/character/bytes. A host-side API trace can additionally record the consumed native KEY_EVENT (VK/scancode, Unicode unit, `dwControlKeyState`, `bKeyDown`, `wRepeatCount`); the public Runtime does not expose that original record. No extra native reader, production hook or synthetic events. Physically press/release unmodified, Shift, Ctrl, Alt combinations and hold/release a key for repeat; compare only metadata the host actually supplies. Retain host translation/IME origin. A repeat value >1 is not guaranteed by hardware/ConPTY coalescing. |
| Exact restoration | In the test process read actual standard handles, all modes and both code pages before Runtime creation; after outer/inner apply, rejected non-LIFO close, inner restore, outer restore, Runtime close and shutdown of an unclosed session. Record returned native/error/cleanup codes; compare complete saved masks/code pages. Do not let shell teardown substitute for this boundary. |
| Wide I/O | Observe actual ReadConsoleW/WriteConsoleW returns and requested/completed UTF-16 units with existing Unicode prompt/print fixtures. Compare Runtime UTF-8 progress, error and partial-unit fields. Visible NUL/BOM or malformed surrogate behavior needs API/byte observations; clipboard display and injected codec tests are insufficient. |
| Borrowed sync/overlapped handles | Existing Windows standalone native cancellation mode is absent. A small Windows-only test fixture must supply real synchronous and overlapped borrowed pipe handles in separate fresh processes **before** process Streams capture. This is genuine redirected native I/O, separate from interactive console evidence. Retain original host handles and an unrelated outstanding request. Verify actual native pending/entered boundary, cancel only the Runtime request, observe original return/count/error and actual completion, retained buffer/GC roots until drain, no early close/reuse, post-close original-handle usability and unaffected foreign completion. |

Do not implement that observer without an available host merely to produce a
Linux simulation. It must keep existing tests/protocol/assertions unchanged,
write diagnostics separately and reuse existing Runtime/Streams and barrier/GC
infrastructure. Admission, a worker-start signal or an arbitrary delay does not
prove a native operation entered or progressed. Use a genuine pending native
completion/OS trace boundary; completion can legitimately win cancellation.
Unsupported/failure/partial cases that cannot be induced safely remain explicitly
unverified. Forced native failure or malformed UTF-16 injection is separate fault
evidence and never physical-key acceptance. No new resolver/runner/ConPTY framework
is authorized or required here.

## Handoff gates

Return per-host/configuration source/build identities, all successful **and failed**
observer JSON, raw terminal/probe reports, physical input provenance, native error/
progress observations and unresolved rows above. Debug and Release both matter.
No real console, ConPTY, physical metadata, exact in-process restoration or native
borrowed-handle cancellation observation was executed in this session. Stage 10
remains INCOMPLETE until Wrench reviews the actual receipts and Boss decides release.

[Local inspection/validation and exclusions](results.json),
[exact changed-file hashes](changed-files.json) and the preserved
[native-final handoff](../native-final/HANDOFF.md) distinguish this preparation
from executed CI. No next stage, hosted dispatch, production/test/workflow edit,
commit/push/branch or virtualization action belongs to this increment.
