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
result printing stays CLI behavior; it is not silently converted into source print.

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
caller's retry starts after the reported consumed prefix. Finite waits are useful
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

Shutdown marks the stream context closing, rejects new operations, wakes pending
read waits and drains admitted operations before destroying native buffers. Use
readiness plus a wake event/self-pipe on POSIX; Windows overlapped pipe reads or
cancelable native reads must cancel **only VOID-owned pending requests**, not all
host I/O on a standard handle. Console waits use input/wake events. A Windows
adapter whose sync-read cancellation cannot meet this ownership condition is a
blocker for that adapter, not permission to hang shutdown. No user-owned close of
borrowed process handles. Deterministic barriers establish admission/drain tests.

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
that every operation works. Unknown/dumb terminals have text capabilities only.
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
