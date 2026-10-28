# Logging API

`use log` provides checked console/file logging in ordinary VOID source.
[Approved contract](DESIGN.md), [source](../../stdlib/log.void),
[engineering history](IMPLEMENTATION.md), [acceptance](ACCEPTANCE.md) and
[review handoff](HANDOFF.md) separate behavior from verification status.

The default is info, UTC timestamps and stderr, with no file or directory creation.
A record is exactly `<UTC> <LEVEL> <quoted-name> <quoted-message><LF>`, or starts
with LEVEL when timestamps are disabled. Levels are TRACE, DEBUG, INFO, WARN and
ERROR. The fixed UTC spelling is `yyyy-MM-dd'T'HH:mm:ss.fff'Z'`; fractions truncate.
Names and messages escape quotes, backslashes, LF/CR/tab and the precise Unicode
control set in DESIGN. Other UTF-8 bytes retain their identity. Completed records
occupy one physical line. Oversized records fail before rotation/output.

## Results and configuration

Every write/convenience/lazy/async emission returns `result<bool, log.failure>`:
`Ok(true)` means every configured sink accepted the complete record; `Ok(false)`
means filtered without log-owned formatting, clock sampling or output. Errors
retain typed console/file/clock/cleanup causes and this record's per-sink progress.
Partial output is possible. Error severity does not terminate the application.
Transport/rotation failure or uncertain unwind faults admitted writes until explicit
recovery; no entire-record replay or automatic stderr fallback occurs.

`log.config` is copied. Later changes to the caller's value do not change a logger.

| Field | Default |
| --- | --- |
| minimum | `log.level.info` |
| name | `""` |
| console | `log.console_sink.stderr` |
| timestamps | `true` |
| file_path | `""` |
| rotation_bytes | `8388608` |
| backups | `3` |
| max_record_bytes | `65536` |
| existing | `log.existing_files.reject` |

Threshold order is trace, debug, info, warn, error, off. Passing off as a record
severity fails. Sizes must be positive, backups must be 0–32, and the total current
file bound must fit i64. The record budget must fit the escaped name and the largest
empty-record framing, and cannot exceed the file bound when a file is configured.
A sink is required unless the threshold is off. Invalid settings fail before
filesystem mutation. Disabling records does not defer explicit file acquisition.

## Complete function surface

`R` below abbreviates the documented spelling `result<bool, log.failure>` only.
There is no new source type alias, overload set or formatting language.

| Module function | Instance method | Return |
| --- | --- | --- |
| `open(options: config = config())` | none | `result<logger, failure>` |
| `async open_async(options: config = config())` | none | `result<logger, failure>` |
| `configure(options: config)` | `configure(options: config)` | R |
| `async configure_async(options: config)` | `async configure_async(options: config)` | R |
| `enabled(severity: level)` | `enabled(severity: level)` | bool |
| `write(severity: level, message: string)` | same | R |
| `trace(message: string)` | same | R |
| `debug(message: string)` | same | R |
| `info(message: string)` | same | R |
| `warn(message: string)` | same | R |
| `error(message: string)` | same | R |
| `write_lazy(severity: level, make_message: func() -> string)` | same | R |
| `async write_async(severity: level, message: string)` | same | R |
| `flush()` | `flush()` | R |
| `async flush_async()` | `async flush_async()` | R |
| `shutdown()` | `close()` | R |
| `async shutdown_async()` | `async close_async()` | R |
| none | `deinit()` | no result |

`logger` is managed and privately constructed. Aliases share state and lifecycle;
independent instances, even with identical names, remain independent. `enabled`
is a filter predicate, with no writable-sink guarantee. Existing deep readonly and
worker restrictions apply. The compiler-owned inventory is preserved in evidence.

Ordinary eager argument/interpolation effects happen before filtering. A lazy body
runs zero times when filtered and once when admitted execution reaches it. Callback
factories and capture construction can still allocate. The busy guard precedes the
builder. Same-instance admitted recursion returns busy; filtered valid calls can
return false. Other loggers remain usable. No user callback runs under a sink lock.

## Files and recovery

The active path P and exactly P.1 through P.N form the current managed set.
Parents must exist. Relative paths resolve once at acquisition, including recovery
of the same configured path after cwd changes. A persistent owned file remains open.
Rotation occurs before a record would cross the byte bound: flush, close, remove
the exact oldest owned slot, descending no-replace moves, and exclusive creation.
N=0 removes only the closed active file. Gaps are allowed. Failures stop at the
actual boundary, preserve completed changes and fault the logger. There is no
transaction, guessed rollback, wildcard cleanup, copy/truncate fallback or directory
sync promise. Path changes leave the previous completed logs in their old namespace.

Reject mode refuses any pre-existing current member before creation. Explicit adopt
requires regular files within the bound, empty or ending in LF. Known links/reparse
points, directories and special entries fail metadata preflight. Adoption requires
the caller to control trusted parents and exclude competing writers, hostile name
mutation, distinct-file violations and unrelated hardlink/path aliases. Metadata
checks do not prove identity or eliminate filesystem races.

Same-namespace configuration retains authority over its previously owned exact set.
Shrinking removes only previously owned excess slots; growth requires absent new
slots or adoption. Unknown higher historical names remain untouched. Reduced byte
bounds are checked against retained files before mutation. Settings publish after
successful replacement; partial replacement leaves an explicit fault, without
quietly continuing stale settings. A torn active file requires application repair
or another destination. A failed acquisition can leave an empty created active file;
cleanup closes its handle and does not guess a filename rollback.

## Lifecycle, async and examples

Explicit flush drains the borrowed console and requests file fsync/FlushFileBuffers.
Close/shutdown attempt native release even after a recoverable flush error and
retain both causes. Native close errors retire the handle; busy retains ownership.
Completed close is idempotent. Deinit is terminal, synchronous and one-shot; it
uses checked close and escalates errors through the existing Runtime path. Terminal
admission does not suppress an explicit close retry on a still-owned busy resource.
GC never runs source deinit. Runtime draining of forgotten resources does not
substitute for checked synchronization.

Async methods use the existing awaitable io operations for acquisition, validation,
rotation, writes, synchronization and close. The console portion remains synchronous.
Guards, candidate identities and buffers remain rooted across suspension; cancellation
drains entered jobs before synchronous emergency cleanup. Another admitted operation
returns busy. There is no hidden writer queue, async builder or worker privilege.
Default replacement reserves the old default before its first await and retains
an unpublished candidate under its own acquisition guard until publication.

Examples: [stderr](../../examples/log/main.void),
[development .void/logs](../../examples/log/development.void),
[released game state directory](../../examples/log/released_game.void),
[rotation](../../examples/log/rotation.void), [lazy construction](../../examples/log/lazy.void),
[checked shutdown](../../examples/log/checked_shutdown.void) and
[explicit async root](../../examples/log/async.void).
Development and released paths are recommendations configured by the application.
The compiler's `.void/cache` is separate from application log storage.
