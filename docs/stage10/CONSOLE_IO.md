# Stage 10 console and terminal contract

## Boss authorization — 21 October 2026

Boss approved implementation of the entire 44-step Stage 10 mission, B1–B4,
and the Wrench amendments. This supersedes the proposal-only authorization
statements below; it does not claim implementation or acceptance.

- One heterogeneous print and shared display/format kernel; exact numeric
  ties-to-even rounding, checked allocation, no arbitrary width/precision cap.
- Declaring-owner field privacy and deep const paths; direct own-initializer
  replacement grant only; existing method/property privacy remains unchanged.
- Owner-contained exact operators include binary `%` and derived `%=` in addition
  to the other arithmetic, unary, equality and ordering families. Floating builtin
  remainder remains prohibited. Independently declared `!=` remains prohibited.
- Approved verified pinned Ryu/date and bundled versioned IANA rules; recursive
  custom-equality dictionary-key exclusion remains required.
- A standalone CR completes a line immediately. Pending CRLF continuation belongs
  to logical line reads; mixed raw reads retain byte ownership.
- Cursor queries require an accessible owned response channel. Redirected stdin,
  unavailable ownership or responses return a recoverable error; no hidden tty.
- Preserve Stage 9 Windows corrections, LLVM 19.1.7, ownership and GC. No Git
  mutation, hosted CI dispatch, publication or root README edit is authorized.


**Approved for implementation; Stage 10 acceptance remains open.** See [the roadmap](../STAGE10_DESIGN.md).
API tables are design signatures; they are not claims that these calls compile
today. Names below follow VOID spelling and existing `result<T,E>` semantics.

## Evidence and design choice

VOID 2.0 has strict UTF-8 strings, recoverable results, named/default calls and
NativeSafe Runtime transitions, but no source console operations. Original VOID
has a complete local console tree: `Console.void`, `IO/StandardInput.void`,
`IO/StandardOutput.void`, `IO/StandardStreams.void` and `IO/StandardTerminal.void`.
The original input implementation shares `_pendingByte` between scalar, line and
byte reads, including CR lookahead and invalid UTF-8 continuation handling. Its
shutdown audit records why default output must not depend on managed singleton
objects after roots are torn down. Source locations/digests are in
[EVIDENCE.md](EVIDENCE.md); old audit pass counts are historical, not Stage 10 tests.

Use a small set of borrowed standard endpoints and source policy wrappers, with
low-level checked OS operations in the existing Runtime. Python provides a useful
precedent for one variadic print with separator/end/file/flush controls; VOID
retains static typing and its own failures rather than Python's dynamic object
model. [Python print](https://docs.python.org/3/library/functions.html#print).

## One print operation

```text
print(*values, sep: string = " ", end: string = "\n",
      file: console.output = console.stdout, flush: bool = false)
```

`*values` denotes a compiler-recognized heterogeneous positional pack, not new
general-purpose source varargs or a runtime object array. Controls are named-only
after the value pack. Existing named argument spelling uses `=`:

```python
print()
print("Score:", score)
print("Loading...", end = "", flush = true)
print("failed", code, file = console.stderr)
print("a", "b", sep = ", ", end = "")
```

Every value uses the [same display implementation](DISPLAY_FORMATTING.md) as
`string(value)` and interpolation. Print itself does not parse formatting codes;
`print("Score: {score:000}")` performs interpolation first. Strings are emitted
without added quotes. No value arguments means only `end`, with no separator.
No trailing separator. `sep` and `end` may be empty, Unicode or contain NUL.

Apply ordinary call evaluation: evaluate every supplied expression exactly once
in written order and capture existing value snapshots/reference identities. Then
render values left to right into one owned record, append separators/end, and
write that record. Source expression and hook side effects are not rolled back.
If rendering fails, none of that print record reaches its destination. Hold no
stream lock while evaluating expressions, invoking source hooks or allocating
managed results. Once writing begins, an OS failure can leave a visible prefix;
do not promise output rollback.

`print` has no source result. An output error uses the existing checked Runtime
failure channel, with operation, stream, portable error kind, OS code and completed
byte count. Code that needs to recover formats a string and uses `.write()`.
This avoids introducing another print family. Existing legacy `entry(i32)` CLI
result printing stays CLI behavior; it is not silently converted into source print. Its legacy
result trailer retains native CRT text newlines (Windows CRLF, POSIX LF), through
CLI std::cout and independently emitted printf. Source print records remain exact
UTF-8/LF bytes on every platform; tests must not normalize entire output streams.

## Standard streams and error values

`use console` exposes `stdin: input`, `stdout: output`, `stderr: output`.
Endpoints are unforgeable compiler-owned borrowed tokens. Copies refer to the same
standard endpoint; they do not own/close descriptors. Source cannot manufacture a
token from an integer, replace a process handle, inspect a native payload, or gain
external native-provider admission. They are not worker-transferable resources.

Use existing source structs/enums/results for public outcomes:

| Value | Required fields / cases |
|---|---|
| `text_read` | `text(value: string)`, `eof` |
| `bytes_read` | `data(value: [u8])`, `eof` |
| `io_error` | readonly `kind`, `os_code: i64`, `message: string`, `bytes_transferred: i64`, `partial: [u8]` |
| `io_error_kind` | `invalid_argument`, `invalid_encoding`, `unsupported`, `would_block`, `timed_out`, `closed`, `broken_pipe`, `interrupted`, `io_failure` |

`partial` contains valid consumed bytes before a read failure, or an empty byte
array for a write failure. A caller never has to guess whether consumed input was
lost. Error messages are documented portable strings; native codes are diagnostic
detail and never used for portable branching. Allocation failure/size overflow
continues to use the existing Runtime failure channel, not an allocated fake
`io_error` that might itself be impossible to create.

| Operation | Result / behavior |
|---|---|
| `input.read_char(timeout_ms = -1)` | `result<text_read,io_error>`; one Unicode scalar as string, not a new char type |
| `input.read_text(max_scalars: i64, timeout_ms = -1)` | Same result; up to that many complete scalars; does not wait to fill once available text has been obtained |
| `input.read_line(timeout_ms = -1)` | Same result; one complete line or final unterminated line |
| `input.read_bytes(max_bytes: i64, timeout_ms = -1)` | `result<bytes_read,io_error>`; owned byte snapshot from the same input queue |
| `output.write(text: string)` | `result<i64,io_error>`; write all UTF-8 text or return error/progress |
| `output.write_bytes(const bytes: [u8])` | Same; exact redirected bytes, terminal restriction below |
| `output.flush()` | `result<bool,io_error>`; `Ok(true)` means VOID-owned pending output drained |
| `input.is_redirected`, `output.is_redirected` | bool; independent stdin/stdout/stderr classification |
| `input.capabilities()`, `output.capabilities()` | Snapshot of actual supported terminal operations; no terminal allocation |

Counts are i64; negatives reject. Count zero returns `text("")`/`data([])` without
blocking or probing EOF. Positive-count EOF returns `eof` only after all queued
data has been consumed. Empty line is `text("")`, never EOF. EOF is not an error.
EOF at the end of a nonempty unterminated line returns that line once, then EOF.
There is no dependency on future `string?` or general file-stream classes.

`timeout_ms = -1` means wait; zero means poll; positive means one monotonic deadline
for the whole call, including decoding/lookahead, not a renewed timeout per byte.
On timeout after consuming part of a requested line, return `timed_out` and put
the partial bytes in `io_error.partial`; no hidden half-line is discarded. A
caller's retry starts after the reported consumed prefix. Deadlines govern admission,
readiness waits and retries; an already-entered uncancellable syscall may complete
after its deadline under the approved best-effort contract. A successful final
transfer remains successful; further required work stops with its exact known
prefix. Finite waits are useful
for interactive applications and shutdown tests without introducing async tasks.

## Input ordering, decoding and redirection

There is **one** serialized byte queue/UTF-8 decoder per standard input context,
shared by every console read API. Runtime owners sharing the default process
context share this queue; no competing read-ahead buffers. Explicit host test
contexts can supply isolated endpoints. Mixing VOID reads with independent host
stdio readers of the same descriptor cannot provide coherent read-ahead ownership;
the embedding host must use one input owner. This is an embedding contract, not
a second language input system.

Redirected input is strict UTF-8, without locale/ACP decoding. Preserve NUL; reject
overlong encodings, surrogate scalars and values above U+10FFFF. A leading UTF-8
BOM is ordinary U+FEFF text, consistent with byte-preserving strings; it is not
silently stripped. No UTF-16 BOM autodetection. Bytes APIs allow binary input.

On malformed UTF-8, consume the maximal invalid prefix, leave the first byte that
could start the next scalar queued, and return `invalid_encoding`. Include earlier
valid consumed bytes in `partial`; do not return an invalid VOID string. Truncated
encoding at EOF is an encoding error once, then EOF. Test every split boundary.
Line decoding uses this same mechanism; it does not validate a second, differently
buffered input stream.

`read_line` strips one LF, CRLF or CR terminator. A standalone CR completes
its line immediately, including a redirected pipe whose writer remains open.
Record pending CRLF continuation without waiting for another byte. The next
logical line read consumes its following LF if present; a non-LF stays owned by
that next line. Byte/character/text reads preserve raw terminators and clear the
continuation when they consume the following byte, so switching APIs never
silently discards that byte. Test CR/LF split across native reads. Other reads
preserve raw line terminators. Interactive canonical
terminals may perform their own configured input translations; the Runtime does
not claim it can reverse transformations already performed by the terminal.

On Windows, actual console text input uses wide console facilities and validates
surrogate pairs before producing UTF-8. Redirected handles use byte reads. Console
byte reads return the UTF-8 encoding of console text through the same queue;
they do not expose arbitrary UTF-16 halves. No `ReadConsoleA`, ANSI conversion,
`chcp`, process code-page changes or global CRT mode changes.

## Output completion and platform boundaries

Redirected text output is exact UTF-8, with no BOM and no LF-to-CRLF translation.
`write_bytes` preserves arbitrary bytes on files/pipes. On a real Windows console,
text output uses validated UTF-8-to-UTF-16 conversion and `WriteConsoleW`; arbitrary
byte writes return `unsupported` unless the bytes form valid UTF-8, in which case
they use that same text path. POSIX terminals receive bytes through `write`.
For portability, binary programs should use redirected endpoints.

Microsoft explicitly distinguishes console and redirected handles: `WriteConsole`
fails for redirected files and applications should select a file write path there.
Its completed-character count must be checked.
[WriteConsole](https://learn.microsoft.com/en-us/windows/console/writeconsole).

Loop on positive partial progress until all data is transferred or an error occurs;
retry EINTR when no cancellation/timeout applies; treat zero progress on a nonempty
write as failure. Preserve original error codes before cleanup calls overwrite
them. Chunk to native count bounds without splitting a UTF-16 surrogate pair.
Report progress in UTF-8 payload bytes, translating completed wide scalar progress
back to original byte offsets. A partial output result must not claim the whole
record succeeded. Linux's own API documentation explicitly permits short writes
and reads. [write](https://man7.org/linux/man-pages/man2/write.2.html),
[read](https://man7.org/linux/man-pages/man2/read.2.html).

For POSIX broken pipes, do not globally replace an embedding host's SIGPIPE policy.
Use a scoped per-thread signal mask around the write, preserve an already-pending
SIGPIPE, consume only a newly generated signal associated with an EPIPE result,
then restore the mask. Verify this mechanism on Linux/macOS before adopting it;
it is a platform acceptance obligation, not proof supplied by this research.
Windows reports the corresponding broken-pipe error through the ordinary result.

## Concurrency, flushing, prompts and lifecycle

Serialize completed print/write records with a process-standard-output mutex
covering both stdout and stderr. This also prevents two VOID writers whose streams
have been redirected to the same handle from splitting each other's records.
Ordering between concurrent records is mutex admission order, not scheduling order.
No guarantee extends to foreign writers or independent processes; writes larger
than an OS pipe atomicity bound can interleave with those writers.

Default output has no hidden long-lived text buffer. A completed write has handed
all bytes to the OS. `flush` drains any owned adapter buffer, and is a no-op when
none exists; it does not call fsync, force disk durability, or use Windows pipe
`FlushFileBuffers` as a generic console flush. `flush = true` still provides the
correct boundary if later measured buffering is introduced without API changes.

Prompts are explicit: `print("Name: ", end = "", flush = true)` followed by
`console.stdin.read_line()`. Reading never prints a hidden prompt, automatically
opens a terminal, or bypasses redirected stdin. An explicitly printed prompt also
appears under redirection; applications choose whether to print it by querying
capabilities. Read operations do not flush unrelated host stdio objects.

Native input/output buffers own copies of all bytes. Finish source formatting and
copying while Managed; enter the existing NativeSafe state **before acquiring a
possibly blocking native stream lock or waiting in the OS**. Release locks before
returning to Managed or allocating/publishing result objects. No GC pointer or
borrowed string payload is dereferenced while NativeSafe. Use ordinary rooted
handoffs for results. Classify console calls as blocking/I/O effects in the current
effect graph; keep them prohibited in worker-reachable graphs under today's rules.
An ordinary async source call may block its owning executor just as today's
synchronous native calls do; this stage does not promise nonblocking async input.

Shutdown marks the stream context closing, rejects new operations, cancels queued
operations and promptly wakes interruptible readiness waits before draining admitted
operations. POSIX uses readiness plus an owned wake pipe; Windows cancellation must
target only VOID-owned requests/threads, never unrelated host I/O. **Boss-approved
correction, 22 October 2026:** cancellation of an already-entered OS syscall is
best-effort. Readiness does not reserve capacity, and external host activity or a
stalled device may prevent syscall completion. Shutdown can therefore remain blocked
until the OS call actually completes. Retain buffers, adapters, leases and Runtime
lifetime throughout; never detach or fabricate progress to make shutdown return.
Do not close/duplicate borrowed handles or change process-wide descriptor flags.
No mandatory io_uring or replacement I/O framework. Deterministic barriers establish
admission, wake and retained-lifetime tests.

On normal source completion (including a nonzero application return), run reached
source cleanup, drain/flush console operations, restore owned terminal modes, then
release Runtime roots. On an existing failure, preserve that primary failure and
attach flush/restoration failures as cleanup failures. If only flushing fails,
report failure. Never dispatch source hooks or managed singleton methods after
root/code teardown. No promise of cleanup after process kill, fatal OS termination
or an unrelated host tearing down live descriptors.

## Terminal surface and unsupported behavior

Capability flags are per endpoint: `text_input`, `text_output`, `key_input`,
`input_modes`, `size`, `cursor_position`, `cursor_visibility`, `color`, `clear`,
`title`, `bell`. Detection is conservative; terminal presence alone is not proof
that every operation works. Unknown/dumb terminals have text I/O and live OS dimensions where available; no
control protocol is inferred.
An actual operation can still fail after a successful capability query.

| Operation | Required public behavior |
|---|---|
| `input.read_key(echo = false, timeout_ms = -1)` | `result<key_event,io_error>`; character string or named key, modifiers, repeat count; UTF-16 pairs joined on Windows |
| `input.key_available()` | `result<bool,io_error>`; queued event/readiness without consuming a character |
| `input.begin_mode(echo = true, line_input = true, control_c_as_input = false)` | `result<terminal_session,io_error>`; scoped `with`/one-shot cleanup, exact prior mode restoration |
| `output.size()` | `result<terminal_size,io_error>`; current columns/rows, positive integers |
| `output.cursor_position()` / `set_cursor(column,row)` | Position result / acknowledgment; zero-based, negative/out-of-range rejected |
| `output.set_cursor_visible(bool)` | Acknowledgment; change only this terminal |
| `output.set_color(foreground,background)` / `reset_color()` | Defined 16-color enum plus terminal defaults; never assume all RGB modes |
| `output.clear()` | Clear visible display and home cursor; no promise to erase terminal scrollback |
| `output.set_title(text)` | Acknowledgment; reject control characters/escape injection in title payload |
| `output.bell()` | Acknowledgment of supported request, not a guarantee that a human hears sound |

Acknowledgments are `result<bool,io_error>` with `Ok(true)` on completion. Position,
size and key values are ordinary immutable source records. A key event has a
character field (empty for nontext keys), key enum, modifier flags and repeat
count. Include enter/tab/backspace/escape, navigation and F1–F12; unknown sequences
are reported as an `unknown` event retaining their bytes, never silently dropped.
POSIX does not provide reliable key-up or every modifier combination; expose
unknown information explicitly. Use a documented 30 ms escape-sequence continuation
deadline (bounded also by the caller's deadline); preserve unconsumed bytes/events
when a sequence is incomplete. Tests use an injected monotonic clock, not sleeps.

`read_key` temporarily enters noncanonical/no-echo mode when needed and restores it
on every exit; `echo = true` explicitly writes its character through the output
record path. Reuse existing input queue state; do not discard line-reader lookahead
when switching to key input. Character bytes already buffered cannot retroactively
recover physical-key modifier information, so such events report unknown modifiers.
Ctrl-C retains OS interrupt behavior by default. A scoped control-C-as-input mode
permits reading it as data; no managed signal callback/event framework is added.

Redirected/unsupported operations return `unsupported` **before writing controls or
changing modes**. Never open `/dev/tty` or `CONIN$` behind the user's redirection.
Use existing terminal capabilities/terminfo on POSIX and native console queries
or enabled VT facilities on Windows. If the required native terminfo dependency
is unavailable, expose the operation as unsupported instead of guessing escape
support. Cursor queries require a bounded response protocol through the shared
input parser; unrelated input is retained. Do not wait indefinitely for a cursor
report. Title readback is not portable; expose setting only and never claim cached
values reflect changes by other applications.

Mode sessions cannot be copied into workers/tasks or escaped after close. Serialize
sessions for a given input, permit properly nested same-owner sessions with LIFO
restoration, and reject conflicting owner sessions. Native saved mode state lives
in the existing deterministic resource registry, not GC finalizers. A Runtime
shutdown restores any remaining owned sessions before detaching. Restoration
cannot run arbitrary source code.

Windows VT behavior requires appropriate console mode support; POSIX input mode
flags must be restored rather than globally overwritten.
[Windows VT](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences),
[termios](https://man7.org/linux/man-pages/man3/termios.3.html).

## Required integration evidence

The [test plan](TEST_PLAN.md) includes byte-for-byte redirection, shared mixed reads,
partial transfer faults, EOF/encoding errors, PTY sessions, native Windows console
probes, concurrent record boundaries, GC while blocked, shutdown drain and normal/
O2/JIT/AOT parity. Console cannot be marked complete on pipe-only tests or a
string-only print implementation. Unsupported terminal hardware is a documented
capability outcome; an unimplemented operation on a claimed supported terminal is
missing engineering work.


## Step 23 native foundation — 22 October 2026

`include/void/console.hpp` and `src/runtime/console.cpp` provide native embedding
state only. No source console API, endpoint token or input/output ABI operation is
implemented by this increment. `Runtime::console_context()` lazily publishes one
native session per Runtime. Optional host injection supplies isolated `Streams`.
Preparation occurs NativeSafe and outside the GC mutex; publication rechecks Runtime
closing. Default process `Streams` remains shared across Runtime lifetimes, with one
input gate and one output gate covering both stdout and stderr regardless of native
destination equality. Later shared input bytes/decoder state belongs in this same
Streams object, not a session-local read-ahead buffer.

The platform adapter captures borrowed standard endpoints and immutable classification
snapshots independently: POSIX uses fstat/isatty, Windows uses GetStdHandle,
GetFileType and positive GetConsoleMode detection. Unavailable/non-confirmed-terminal
endpoints report redirected; absence retains its native diagnostic code. A terminal
classification does not imply support for presentation/key/cursor operations. No
CloseHandle/close, descriptor duplication, hidden terminal opening, stdio buffering,
code-page or terminal-mode change occurs. The embedding host must establish redirects
before first default-context use and must not replace/close live borrowed endpoints
behind VOID. Isolated native adapters are a trusted host boundary, not source handle
construction or a sandbox for malicious C++ providers.

Each Runtime session has independent closing/admission state and a standard C++
stop token. Acquire rejects new work after close, counts queued/granted operations,
and serializes input or output. Queued calls wake and reject on close; granted
operations receive cancellation and retain their session/adapter/native buffers
until release. Context close drains outside the GC mutex; Runtime teardown closes
its session before native-library/heap teardown. Closing one session never stops
another Runtime's default input session. Standalone Context destruction cancels
without self-waiting; its outstanding operation leases retain native state. Explicit
self-drain rejects before Runtime admission is changed. Existing Runtime rules still
require host participants/handles to outlive their Runtime correctly; this increment
does not permit destruction beneath an attached Managed invocation.

Acquire/drain/observation waits require NativeSafe or an unattached native host.
Runtime lookup and native-state destruction temporarily enter NativeSafe when needed.
Release leases on their acquiring thread before returning to Managed. Nested gates
follow input-before-output order; recursive acquisition rejects. Cancellation
callbacks are native signal-only handlers scoped within the operation lifetime;
they must not drain their own context or invoke source/GC-managed cleanup. Step 24
will translate these per-session tokens into owned OS-request wakeups and implement
real transfers/deadlines; Step 23 does not claim cancellation of an OS read that does
not yet exist. The adapter currently exposes borrowed endpoint inspection and an
injectable monotonic clock, with no pretend read/write implementation.

Primary boundary references checked for this implementation:
[isatty](https://man7.org/linux/man-pages/man3/isatty.3.html),
[fstat](https://man7.org/linux/man-pages/man2/fstat.2.html),
[GetStdHandle](https://learn.microsoft.com/en-us/windows/console/getstdhandle),
[GetConsoleMode](https://learn.microsoft.com/en-us/windows/console/getconsolemode),
[GetFileType](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfiletype).
Native Windows/macOS behavior remains unverified. Exact focused Linux outcomes and
source hashes: engineering/step23-results.json. Steps 24–31 and composite display
remain unfinished; no console usability or complete Stage 10 claim follows.


## Historical Step 24 blocker — resolved by the approved contract below

The prescribed readiness/self-pipe approach is insufficient by itself for every
borrowed blocking output endpoint. Readiness does not reserve writable capacity;
a permitted independent writer may consume it before even a one-byte write. A
self-pipe wakes poll, but cannot interrupt a write already entered. A deterministic
Linux probe reproduces the resulting retained lease/shutdown wait. Targeted libc
POSIX AIO cancellation also returns AIO_NOTCANCELED for an entered pipe read.
Neither is an implemented production transfer path. See
[the blocker and policy choices](engineering/step24-feasibility.md).

Boss/Wrench must settle the platform request/capability policy before this adapter
is extended. No contract weakening, global signal handler, shared descriptor flag
change, detached request, host descriptor closure or unsupported placeholder was
adopted. Linux io_uring setup is available on this host outside the restricted
runner but denied by the restricted runner; actual request integration/cancellation
and portability have not been verified. Native Windows/macOS remain unverified.
Step 24 remains unimplemented; Step 23's foundation and later-stage gaps are unchanged.


## Step 24 checked native transfers — 22 October 2026

Boss approved Golden Wrench's revised borrowed-stream contract: queue and readiness
wait cancellation is prompt; entered syscall cancellation is best-effort, with all
native state retained through actual completion. This supersedes the earlier
universal-drain assumption and resolves the recorded feasibility blocker. The
original counterexamples remain evidence for this limitation, not defects claimed
fixed by readiness alone. No io_uring investigation was repeated or dependency added.

`Context.read(max_bytes, timeout_ms)` returns native-owned bytes plus `Transfer`
(completed count, explicit EOF, portable error/native code/secondary cleanup code).
All consumed input is retained even on a failure. `Context.write(stream, Bytes,
timeout_ms)` owns its record and completes short writes under one stdout/stderr
gate. No partial write is reported as complete. A positive read returns its short
result without waiting to fill. Zero requests succeed without gate acquisition,
endpoint probing or EOF discovery, including after close; invalid controls still
reject. Negative counts or timeout below -1 reject. Allocation/size exceptions
remain native checked-boundary failures, never allocated fake IO errors. These
are C++ embedding APIs, not source console types or a new source ABI.

One request deadline starts before read-buffer allocation/admission and is reused
across gate waiting, native waits, EINTR/EAGAIN retries and partial writes. Large
timeouts saturate safely at the monotonic clock maximum; internal bounded wait
slices do not shorten or renew that deadline. Zero polls return would_block when
not ready and preserve any completed write prefix; an interrupted zero poll may
report interrupted. Successful OS completion wins a stop/deadline race; if output
still needs more transfers, its completed prefix accompanies closed/timed_out.
Original native errors and secondary cleanup failures remain available.

POSIX waits poll the borrowed endpoint and an operation-scoped, nonblocking owned
wake pipe. Only wake descriptors receive nonblocking/close-on-exec flags. Borrowed
descriptors and host stdio/terminal modes are untouched. Retain and use the original
endpoint availability snapshot for every transfer, so an initially absent standard
fd cannot be confused with a wake descriptor allocated by this or another operation. Reads/writes perform real
syscalls, preserve binary bytes and EOF, retry checked interruptions/readiness races,
and report original errno. Normal pipe writes use PIPE_BUF chunks; this reduces
blocking but does not promise immunity to independent writers.

Each POSIX write temporarily blocks SIGPIPE on its thread, records already-pending
SIGPIPE, consumes a newly generated signal only on its own EPIPE when none was
already pending, then restores the prior mask. No global signal handler change.
Linux uses zero-time sigtimedwait; Darwin's corresponding pending/sigwait path
requires native verification. An original failure survives any cleanup error.
The native SIGPIPE handler/default/ignored/preexisting-pending and mask tests all
pass on Linux. External signal injection during that same critical interval has
the OS's ordinary standard-signal coalescing limitations; no signal callback system
is introduced.

The Windows byte adapter uses ReadFile/WriteFile for redirected endpoints, querying
actual synchronous/overlapped handle mode through FileModeInformation. Synchronous
calls run on a request-owned native thread, with CancelSynchronousIo targeting only
that thread; overlapped stream requests use their own OVERLAPPED/event and targeted
CancelIoEx. Waits use an owned cancellation event and the same deadline. All request
state, native buffers and handles remain alive until completion; no host I/O is
canceled. An overlapped disk handle requiring explicit positions cannot supply the
ordered borrowed stream position and returns unsupported; no host position is
invented. Confirmed console handles do not use this raw binary adapter: Step 26's
wide text path is still unimplemented. No ANSI/code-page/CRT-mode assumptions.
Native Windows compilation/execution and driver cancellation behavior are unverified;
Linux tests do not certify this branch.

Focused Debug 9/9 (13.09 s) and strict ASan/UBSan/leak 9/9 (6.53 s) pass: 178
deterministic transfer checks, 210 real POSIX driver checks, and the preserved
72 foundation/14 endpoint checks. Real probes include binary pipes/files/PTYs,
short reads while writers remain open, shared stdout/stderr records, concurrent
readers, polling/deadlines, cancellation with precise GC roots, real EINTR, all
SIGPIPE policies, closed descriptors and ENOSPC. Source console, shared decoder/
line behavior, Windows wide input/output, print and terminal operations are not
claimed. Step 25 is the next bounded increment. Full evidence: step24-results.json.


## Step 25 shared native input — 22 October 2026

`Context.read_bytes`, `read_char`, `read_text` and `read_line` now provide the
**native C++ foundation only**. The existing `read` byte alias and leased
`Operation.read` use this same path. No public `use console`, source endpoint,
source read/print, Windows-wide or terminal operation is implemented here.

Streams::State owns one native queue of 4096-byte blocks, ordered native errors,
an EOF observation and pending logical CRLF continuation. One existing input lease
covers a complete logical request; only its private Step 24 native transfer helper
can refill this queue. Blocks are allocated before an OS read. A cursor retains all
read-ahead and consumed bytes until the owned result is materialized; then commit
removes the reported prefix. Allocation/size failure rolls back logical consumption
without another allocation or discarding OS-consumed data. This is one byte queue,
not another line buffer/reader/GC path. Native buffers persist across Runtime
sessions sharing Streams; closing one session does not reset another's input.

UTF-8 width classification is factored from the existing shared utf8.hpp validator;
streaming uses that validator for complete scalar legality. It preserves NUL/BOM
and handles every scalar width, strict continuation/range/surrogate/overlong checks,
Native transfer splits and EOF truncation. Once invalidity is known, consume the
maximal immediately available prefix that cannot start another scalar, using only
nonblocking continuation probes. Never wait for a future recovery byte. Keep the
first legal scalar starter queued. An encoding error never constructs an invalid
text result. A native adapter encoding failure is an ordered event reported once,
separate from decoder invalid-prefix recovery (including the future Step 26 feed).

TextResult has valid UTF-8 `text` on success, empty `text` and owned raw `partial`
on error, plus the Step 24 transfer/error fields. `error_offset` identifies the
invalid prefix start within partial; native/control failures identify its end.
`transfer.completed` counts logically consumed input bytes, including stripped
line delimiters and a skipped CRLF-continuation LF. Read-ahead not logically
consumed remains queued, outside that result's progress. Native Adapter counts
still identify exact OS progress. Original OS/secondary cleanup errors survive.

A line strips LF or CR and CR completes immediately, with no following-byte probe.
Pending CRLF is skipped only by a later logical line read; non-LF stays that next
line's first character. Bytes/char/text consume their raw LF normally and clear
continuation only when a byte is actually consumed. Zero-count or failed empty
reads do not change continuation. Empty lines are successful empty text. A final
nonempty unterminated line is delivered before EOF. An EOF observation stays
pending behind bytes/truncation error until delivered; subsequent reads can query
the same host endpoint again, preserving Step 24 behavior for a terminal or file
that later supplies input. EOF does not close the borrowed descriptor.

`read_text` returns complete scalars from available queued/native data, up to its
count. After producing some text it never blocks merely to fill capacity or finish
a following incomplete scalar; that entire scalar prefix remains queued. With no
complete scalar yet, char/text waits for completion under the original request
deadline. Line reads wait through their terminator, EOF or error. Timeout/cancel/
failure returns all logically consumed raw bytes; a retry starts after the reported
prefix, with no undisclosed half-line or automatic reassembly of an already
reported partial scalar. Request-scoped stop/timeout/would-block events cannot
poison a different Runtime session, including after result-allocation failure.

Zero byte/scalar counts still succeed without a gate, read, EOF probe or state
change; invalid counts/timeouts reject and NativeSafe remains required. All reads
reuse one Step 24 deadline through admission, decoding/refills and error recovery.
Already-entered uncancellable syscalls retain the Boss-approved best-effort and
shutdown-lifetime limits; this increment changes no borrowed descriptor flags,
terminal modes, signal policy or ownership guarantees.

Focused Linux Debug 11/11 (13.40 s) and strict ASan/UBSan/leak 11/11 (7.20 s)
pass: 2261 deterministic input checks, 264 real POSIX driver checks in 19 native
process cases, with Step 23–24 regressions enabled. Large 35,000-byte Unicode/NUL
lines force real 4096-byte transfer boundaries; real CR with an open pipe writer,
mixed reads, partial timeout retry, invalid recovery, files/PTYs, buffered shutdown
and precise GC are covered. Native Windows/macOS and public-source/compiler/editor
console acceptance remain unverified/unimplemented as applicable. Next is Step 26;
its Windows console UTF-8 feed must use this same queue. Exact evidence and hashes:
engineering/step25-results.json.

## Step 26 Windows native wide boundary — 22 October 2026

Confirmed console handles now use ReadConsoleW/WriteConsoleW; redirection remains
raw ReadFile/WriteFile, including binary bytes. GetConsoleMode classification and
existing borrowed-handle/per-request worker ownership are reused. No ANSI API,
code page, mode, CRT translation, hidden input handle or global setting is used.
This implementation exposes no public source console API yet.

The allocation-free internal transcoder has only one UTF-16 carry unit and one
UTF-8 scalar remainder. It feeds the one Step 25 queue under its existing input
lease, preserving NUL/BOM, cross-call surrogate pairs, precise invalid-unit events,
recovery units and EOF. Input requests one UTF-16 unit at a time; it never probes
past a completed CR. Host cooked-mode transformations/waits remain host behavior.
A high surrogate survives timeout/cancellation and independent Runtime close until
its actual following unit or EOF. A byte-sized read can consume part of a converted
scalar, with its remainder retained. A native failure accompanying a valid converted
scalar is disclosed with that request's partial bytes; independent encoding/native
events retain their order.

All output UTF-8 is validated before native output. Invalid console binary returns
unsupported with ERROR_NO_UNICODE_TRANSLATION; redirected binary is unchanged.
Bounded 2048-unit native chunks and all resubmitted short-write spans contain whole
surrogate pairs. Reported native unit counts translate to complete source UTF-8
byte progress. If the OS itself reports acceptance of only a high half, output stops
with a checked failure (original native/cleanup codes preserved, or ERROR_WRITE_FAULT
for an otherwise successful half completion). Error.partial_utf16_units reports
that one additional accepted UTF-16 unit. It is non-retryable automatically; no
isolated low unit or duplicated high unit is sent. This extra native progress fact
must survive future source-level console error conversion.

Existing native storage, record gate, NativeSafe/root and request-lifetime rules
apply. Only the VOID-owned request/thread receives cancellation. Entered calls,
including cooked ReadConsoleW, can remain blocked after timeout/poll/cancellation
until the host/device completes them; buffers and leases remain alive. Queued
admission and interruptible waits retain prompt cancellation. No stronger syscall
interruption guarantee is claimed.

Production transcoder/adapter tests pass on Linux; Win32 dispatch is implemented
but native Windows compilation/execution is UNVERIFIED. The local Windows target
probe cannot find target C++ headers; no SDK/cross-toolchain was found. Actual
console input/output, active host modes, cancellation and redirected Windows
behavior require native acceptance. Exact evidence: engineering/step26-results.json.

## Step 27 public source bindings — 22 October 2026

The print operation above is implemented for currently supported shared displays:
all numeric widths, bool/string/null, nominal/custom/generic values, opaque
resources/callables and temporal values. Collection/enum/result composite display
is still absent and rejects explicitly. No fallback formatting is invented.

Every explicit positional value and named control evaluates and snapshots once
in written order, then ordinary display hooks run left to right. Existing join/
checked-builder code assembles the whole record before the native stream gate.
Native owned bytes precede NativeSafe/blocking. Hooks may themselves print without
an outer lock; a failed hook emits none of its outer record. Native partial writes
remain observable failures, with original error/progress and source frames.

`use console` now exposes borrowed stdin/stdout/stderr and input/output types.
`write`, `write_bytes`, `flush`, `is_redirected` and all four read methods in the
contract above are bound to existing Runtime operations. The tokens contain only
compiler-owned role identity, never native handles. Construction, payload mutation,
malformed IR and worker transfer reject. Stdlib identity/discovery/install paths
are unchanged; standalone copied library code cannot invoke private operations.
Existing import/name-shadow rules apply, including imported input/output type names.

The public readonly io_error also has `operation: string`, `stream: string`,
`cleanup_code: i64` and `partial_utf16_units: i64`. `bytes_transferred` counts exact
completed UTF-8 payload bytes; the additional UTF-16 field truthfully reports a
native half-scalar completion without claiming complete UTF-8 output. Read failures
return all consumed partial raw bytes. Allocation/extent failures continue through
the checked Runtime failure channel. Explicit flush/print(flush=true) acknowledge
that unbuffered writes have drained VOID-owned output, not durable disk storage.

Focused Linux JIT/O2/linked-AOT, native pipe, private-IR and strict sanitizer evidence
is in engineering/step27-results.json. The later Step 28 checkpoint supplies prompt/exit-failure acceptance; Step 29
presentation is recorded below. Modes/keys/final concurrency remain Steps 30–31. Native Windows/macOS and fresh
console editor/final Stage 10 acceptance remain open; Step 26 is implemented with
native Windows acceptance UNVERIFIED. This is not a complete console-stage claim.

## Step 28 explicit prompt and lifecycle integration — 22 October 2026

The approved prompt is print(end="", flush=true) followed by existing read_line.
No new helper/API is supplied. An explicitly printed prompt is visible under
redirection before input is requested, including without a newline. No hidden
prompt, terminal opening or host stdio flush. Existing synchronous unbuffered
completion drains VOID-owned output; no durability guarantee or late source hook.

Reached source defers finish before application entry returns, including a nonzero
result. Existing CLI/AOT ownership then closes/drains console native work before
Runtime heap/root destruction. Reusable host JIT invocation does not implicitly
close its Runtime. Concurrent host close cancels/rejects native admission and drains
entered operations; the host retains Runtime/code and joins admitted source work
before destruction. Already-entered uncancellable calls remain best-effort and
may block shutdown indefinitely if the external host prevents progress.

Admitted source cleanup observes recoverable closed via existing console replies;
print reports a checked closed failure. No closing lazy-session initialization.
A cleanup may first need intrinsic byte-array metadata for io_error; the ordinary
validated metadata path can finish for active source Managed depth. New host
attachment/entry/resource/metadata admission still rejects; no metadata check is
removed. Precise roots and code leases persist through source cleanup return.
Existing primary failures precede attached additional defer failures; cleanup-only
failure becomes primary. Native byte/partial-UTF16 and cleanup progress stay exact.

Focused Linux evidence: engineering/step28-results.json. Actual prompt/read pipes,
normal/O2 JIT/linked AOT, zero/nonzero exits, hook/native/allocation/cleanup failures,
shutdown barriers and GC are verified. Native Windows/macOS and Steps 29–31 remain
separate acceptance obligations; no final console/Stage 10 completion claim.

## Step 29 implemented presentation contract

capabilities() is a per-endpoint immutable terminal_capabilities snapshot, not an
authority token. It has the flags listed above. Text flags reflect availability/
direction; key_input/input_modes remain explicitly false until Step30. Live size
uses OS dimensions without cached fallbacks. cursor_position means a live query is
currently admissible; conservative false does not prohibit independently supported
set_cursor. Native errors may still follow a true capability snapshot.

Source records are terminal_size(columns:i64,rows:i64) and
terminal_position(column:i64,row:i64), all const. Acknowledgments are existing
result<bool,io_error>. The color enum has black,red,green,yellow,blue,magenta,cyan,
white, then bright_black through bright_white in the same order (indices 0–15).
Coordinates, palette values, Boolean controls and UTF8/C0/DEL/C1 title injection
are checked before native writes. Reset uses terminfo default colors or captured
Windows initial console attributes. Clear affects the visible display and homes;
no RGB, scrollback-erasure promise, keyboard or mode API is supplied here.

POSIX presentation uses optional system terminfo capability strings and tparm/
tputs, with serialized facility access and saved/restored cur_term. Unknown/dumb/
missing descriptions and redirected streams emit no control bytes. Titles require
XT-advertised OSC support or an explicit OSC2 title pair. The native-only facility
DSO is process resident, as with ordinary dynamic linkage: no unsafe global curses
cleanup/unload; each temporary TERMINAL is released. Installed system ncurses and
its database are not shipped with executables. CMake/voidc Linux builds link libdl.
Windows uses existing console detection plus native per-buffer geometry/cursor/
visibility/attributes/visible-clear, UTF16 SetConsoleTitle and processed-output bell;
no code-page/input/output mode mutation. Native Windows/macOS remain unverified.

POSIX cursor queries require matching accessible stdin/output terminals and existing
host noncanonical/no-echo settings. No mode change, hidden channel, redirected input
consumption or competing VOID reader. Input ownership is taken without waiting on
another reader, then output ownership, with one 200ms monotonic deadline. The existing
queue preserves pre-request/stale bytes, then removes only a validated fresh DSR.
Timeout/partial/unrelated/invalid bytes and pending CRLF state remain owned by normal
reads. Request write progress/errors stay exact; a report-boundary native error is
not swallowed. Windows direct queries consume no terminal input but obey the same
conservative input-owner requirement. Already-entered native calls retain the
approved best-effort cancellation/lifetime qualification; foreign host descriptor/
reader interference is outside the borrowed ownership contract.

Evidence: engineering/step29-results.json. PTY checks validate actual OS dimensions,
response ownership and emitted bytes, not every emulator's visual policy. Native
Windows/macOS SDK/execution, Step30 keys/modes and Step31 final lifetime acceptance
remain separate obligations. The Step28 explicit prompt/lifecycle checkpoint is
implemented; no convenience prompt helper is pending.

## Step 30 source and native contract — 23 October 2026

Implemented source input.read_key(echo=false,timeout_ms=-1), key_available() and
begin_mode(echo=true,line_input=true,control_c_as_input=false). Source key_event is
an immutable record: character, key enum, shift/alt/control, modifiers_known,
key_down_known/key_down, repeat_count and readonly bytes. The enum covers character,
enter/tab/backspace/escape, arrows, home/end/insert/delete_key/page_up/page_down,
f1–f12 and unknown. POSIX logical events have repeat_count=1 and unknown physical
modifier/key-up facts. Unknown sequences retain their exact native bytes. Native
EOF is a recoverable closed error for read_key; ordinary text/byte EOF is unchanged.

The existing InputReader owns all bytes and physical event annotations. Availability
never commits a character, physical event or CRLF continuation, and never changes
terminal modes. POSIX polls current-mode readiness: an unfinished canonical line
remains in the OS editor, so use an explicit noncanonical session for per-key polling. Native Windows
availability peeks console records; it does not steal them from a later ReadConsoleW.
Repeated/key-up/nontext queued physical events cannot silently become ordinary
bytes: an incompatible byte/text read returns unsupported and leaves the event for
read_key. A partially consumed UTF8 scalar remains raw byte-owned; a later key read
reports invalid_encoding with its owned partial bytes rather than inventing text or
physical metadata. Native key errors are delivered once; the next read can recover.

POSIX named sequences come from the actual terminfo key descriptions. The normal
ECMA navigation protocol is accepted only when the description explicitly advertises
its query/report grammar. No TERM-name/PTY guess or keypad-mode control emission.
Escape continuation uses one checked, saturating 30ms monotonic deadline from the
observed ESC, bounded by the caller's original absolute deadline. Zero-timeout reads
remain polls. Timeout yields escape or an unknown partial sequence with retained
bytes; unconsumed queue bytes survive. Native errors retain exact partial progress.

A managed terminal_session owns one native identity; ordinary aliases share it.
Its payload is binding-private and independently validated. Construction, inheritance,
worker/async transfer and payload forgery reject through existing type/IR rules.
with session uses the established reached cleanup and one-shot deinit. close()
returns result<bool,io_error>; repeat explicit close reports closed. Deinit tolerates
an already-closed identity. Same-execution-context nesting is LIFO; conflicting
Runtime/thread/async owners and non-LIFO closes fail without mode changes.

Saved native modes live in the existing console Session/Streams lifetime. No new
resource registry, GC finalizer, source callback or terminal manager is introduced.
The existing Runtime close drains admitted native operations, restores modes and
returns any native cleanup Error to embedding callers; existing callers may discard
that return value. JIT/AOT entry completion restores only modes created within that
entry, including failures before with activation, preserving outer reentrant modes.
Restoration failures append to the original failure through vd_cleanup_failure;
failed saved state remains available for shutdown retry. Abandoned native Sessions
restore after their final admitted lease completes. Persistent external native
restoration failures are reported, not disguised as successful restoration.

Temporary key reads suppress native echo (including POSIX ECHONL), use noncanonical
input and preserve the prior OS interrupt policy. Explicit echo writes character
records through the existing coordinated output gate; key-up does not double echo,
and physical repeats preserve the repeat count. Explicit control-C-as-input sessions
change ISIG/ENABLE_PROCESSED_INPUT only for their owned lifetime. Apply and restore
read back the native flags; even partially applied failures attempt exact restoration.
The begin_mode reply/session allocations precede a successful mode change. NativeSafe,
input-before-output ordering, checked allocation and entered-syscall lifetime remain.

Windows uses ReadConsoleInputW/PeekConsoleInputW, native modifier/repeat/key-up facts
and the existing strict WideConsole transcoder/carry. No ANSI/code-page conversion,
hidden input handle or process-wide signal policy. Actual SDK compilation, console/
ConPTY behavior, native mode combinations (including host API restrictions on
ECHO_INPUT without LINE_INPUT), surrogate/event ordering and failure restoration
remain native Windows acceptance obligations. macOS termios/terminfo/tty acceptance
also remains unverified. The approved borrowed-syscall best-effort shutdown caveat
is unchanged. Private compatibility is console-v2/terminal-v2; dependencies/licenses
and LLVM 19.1.7 remain unchanged. Focused results: engineering/step30-results.json.

Exceptional native unwinding keeps the original exception and restoration failure
in a native per-call carrier, translated by the existing checked console ABI before
reached source cleanup. A successful later retry cannot erase it. Another entry or
shutdown thread cannot consume that caller's diagnostic. This adds no source-level
exception mechanism or shared error registry; saved native modes still belong to
the existing Session/Streams state and remain available for restoration retry.

Mode save/apply/restore runs outside the shared metadata mutex under the existing
input lease, including shutdown cleanup admission. Entered native mode calls retain
their state while queued/new operations can observe cancellation. Key mode setup
uses the caller's original deadline; expiration/cancellation before apply changes no
mode, and an entered apply completed afterward is rolled back before returning.


## Step31 focused concurrency/lifetime verification — 23 October 2026

The existing shared record/input gates and Session/Streams ownership now have
combined multi-Runtime acceptance, including real Linux PTYs. Source generation
leases survive publication/GC/entered I/O/reached cleanup; queued CRLF ownership
survives one Runtime closing and another joining the same Streams. Native requests
retain buffers and saved modes until actual completion. Close restoration errors
remain with the close caller; source primary/partial/secondary errors retain their
existing checked failure path. No production ABI or console architecture changes.

Context::close/Runtime::close still return native Error; rebuild matching headers
and archive, inspect errors and retry restoration when appropriate. Host must join
already-admitted source execution before destroying Runtime/code. An externally
stalled uncancellable borrowed syscall may still prevent shutdown completion. No
foreign endpoint close, hidden input, global descriptor/mode/signal/code-page policy
or detached live request. Exact results: engineering/step31-results.json. Native
Windows/macOS and final-stage acceptance remain separate and unverified here.

## Console tooling integration — 23 October 2026

Public source declarations carry ## documentation through the existing compiler
records. Endpoint method hover/navigation/parameter help uses the installed
/@stdlib/console.void identity, never another API list in an editor. Private and
internal installed declarations are excluded from ordinary symbol menus; native
session payloads stay hidden while ordinary close() remains discoverable.
Compiler-owned print metadata exposes *values and the named controls, including
console.output's real public type, rather than synthetic positional placeholders.
This is source/editor integration only; native adapters, stream lifetime, modes,
shutdown and Runtime behavior are unchanged. Documentation-stripped console source
bytes match the preceding accepted snapshot exactly (engineering evidence).
