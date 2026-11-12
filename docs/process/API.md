# Bounded subprocess capture

`use process` provides `capture` and `capture_async` on Linux x64 GNU and
Windows x64 MSVC. Both take an absolute native executable path, literal
arguments, a complete caller-supplied environment, an absolute working
directory and a `capture_options` value. The launch rules are those of
`process.start`, with direct native execution and no PATH search or shell.
Stdin is inherited. Each output stream can be captured or
inherited independently. The capture call owns the direct child and the
pipe readers through termination and cleanup.

| Option | Meaning |
| --- | --- |
| `stdout_limit`, `stderr_limit` | Use `-1` to inherit the stream. A limit from 0 to 16 MiB enables capture. With a zero limit, the first byte exceeds the budget. |
| `total_limit` | Combined retained output budget, from 0 to 16 MiB. Enable capture on at least one stream. |
| `timeout_ms` | Monotonic time from successful native launch until completion, from 1 through 300,000 milliseconds. Queueing and native process creation precede the deadline. |

The checked return type is `result<capture_result, system_error.error>`.
An `Err` reports option, launch or setup failure. An `Ok` owns binary
`stdout` and `stderr` prefixes and an `outcome` of type `capture_end`.
`complete` means the direct child was reaped and every captured pipe reached
EOF. Per-stream `stdout_complete` and `stderr_complete` are true only when
EOF was observed on that captured pipe. Empty output can be complete.
Inherited streams have empty byte lists and false EOF flags.

An extra byte beyond any budget ends capture with `output_limit`. The
returned prefixes stay within all budgets. `deadline`, `io_failure` and
`cancelled` also end capture and preserve the prefixes already collected.
When both streams contend for the aggregate budget, their shares depend on
the bytes available to the reader. The child is terminated if still
running. `os_code` reports the primary native I/O code, when present.
`cleanup_code` reports a separate native close, termination or reap error.
The deadline bounds observation of output and exit. Cleanup may extend past
it while the operating system completes termination and reap. A termination
error does not release direct-child authority or bypass the reap.

`exited` indicates that exit was observed before abort cleanup. When true,
`status` contains the exact exit code or a separate POSIX signal. A
nonzero exit or a signal can accompany complete capture. Windows retains
the full unsigned process status in `i64`. When `exited` is false, the
status fields are zero and have no exit meaning.

The synchronous call waits in the calling activation. Use
`await process.capture_async(...)` in async code to run launch, both pipe
reads and cleanup on an existing bounded native worker. A running capture
occupies one worker. Task cancellation uses the scheduler's cancellation
outcome. The worker terminates and reaps the child before the task drains,
and a cancelled task publishes no managed reply. Runtime shutdown also
requests cancellation. The synchronous capture can return `cancelled`
with its retained prefixes when shutdown interrupts an admitted call.
The conventional CLI application entry is synchronous. Native hosts with an
async entry can use the existing `Jit::compile_async` and `emit_async_object`
adapters.

Descendants are outside direct-child ownership. Inherited pipe writers in
descendants can delay EOF after the direct child exits. Capture observes
them only within the same output and time budgets. Abort cleanup closes
the readers and targets the direct child. It never signals a process
group or a Windows process tree. Descendant writes after reader closure
have the operating system's ordinary broken-pipe behavior. Concurrent
stdout and stderr are drained in bounded increments with independent
EOF tracking.

Linux x64 capture requires glibc 2.34 or newer for explicit descriptor
closure during spawn. Windows x64 uses private named pipes and an explicit
inheritance list. Only the selected child standard handles are inherited.
Other hosts report the existing unsupported-operation error. The ordinary
`start`, `child.wait` and `child.close` APIs retain their launch contracts.
[glibc spawn descriptor closure](https://sourceware.org/pipermail/glibc-cvs/2021q3/073654.html) · [Windows named-pipe modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes)

The retained-byte budgets bound the output prefixes. Native read scratch
space is fixed, and conversion into the owned VOID byte lists can temporarily
retain both native and managed representations. A capture owns one process
and up to two parent pipe readers. Async concurrency uses the Runtime's
existing worker and admission limits.

From the repository root on Linux, run the example with an absolute native
executable and the current directory.

```sh
voidc run examples/process/capture.void --no-cache -- /usr/bin/uname "$PWD"
```

The example reports both byte counts and the direct child's exit code or
signal. It only displays stdout as text if the bytes decode as UTF-8.

[Executable example](../../examples/process/capture.void)
